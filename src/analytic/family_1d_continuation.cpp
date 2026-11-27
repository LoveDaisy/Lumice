#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "analytic/caustic_internal.hpp"
#include "analytic/jet.hpp"
#include "analytic/path_chain.hpp"
#include "analytic/so3.hpp"

namespace lumice::analytic {
namespace caustic_detail {
namespace {

struct Fragment1D {
  PoseCurve curve;
  std::vector<bool> node_stationary;
  std::vector<bool> edge_corridor_open;
  bool first_node_corridor_open = true;
  bool starts_at_seam = false;
  bool ends_at_seam = false;
};

struct GapReport {
  bool hit = false;
  double invalid_t = 0.0;
  double step_hint = 0.0;
  bool has_resume_valid = false;
  double resume_valid_t = 0.0;
};

void Trace1DSingleFragment(FunctionRef<Eval1D(double, bool)> eval_fn, int slot_count, int free_ax, double t_start,
                           double t_end, bool is_periodic_full, bool left_is_domain, bool right_is_domain,
                           int left_fail_idx, int right_fail_idx, double margin_lip, bool track_corridor,
                           const Family1DParams& params, const DiscoveryStop* stop, Fragment1D* frag,
                           bool* stop_requested, GapReport* gap) {
  *gap = {};
  gap->invalid_t = t_end;
  gap->step_hint = params.min_step;
  gap->resume_valid_t = t_end;

  PoseCurve* curve = &frag->curve;
  curve->kind = PoseCurveKind::kFamily1D;
  curve->coverage.seed_grid_n = params.scan_samples;
  curve->coverage.closed = false;

  const Eval1D e_start = eval_fn(t_start, track_corridor);
  if (!e_start.valid) {
    return;
  }

  auto push_node = [&](const Eval1D& e, double arclen, bool edge_open) {
    if (!curve->nodes.empty()) {
      frag->edge_corridor_open.push_back(edge_open);
    }
    curve->nodes.push_back(NodeFromEval1D(e, arclen));
    frag->node_stationary.push_back(e.sky_speed <= params.speed_tol);
  };

  push_node(e_start, 0.0, e_start.corridor_open);
  frag->first_node_corridor_open = e_start.corridor_open;
  frag->starts_at_seam = is_periodic_full;
  if (!is_periodic_full) {
    CurveEvent ev;
    CopyNodeToEvent(curve->nodes.front(),
                    left_is_domain ? CurveEventKind::kDomainBoundary : CurveEventKind::kOpticalBoundary,
                    left_is_domain ? free_ax : left_fail_idx, &ev);
    curve->events.push_back(ev);
  }

  bool have_last_moving = e_start.sky_speed > params.speed_tol;
  double last_moving_t = t_start;
  Eval1D last_moving_e = e_start;
  double last_moving_arclen = 0.0;

  double t_curr = t_start;
  Eval1D e_curr = e_start;
  double arclen = 0.0;
  double h = std::min(params.max_step, std::max(params.min_step, (t_end - t_start) / 8.0));

  while (t_curr < t_end - 1e-13) {
    if (CheckStop(stop)) {
      *stop_requested = true;
      curve->coverage.budget_truncated = true;
      break;
    }
    if (static_cast<int>(curve->nodes.size()) >= params.max_nodes_per_branch) {
      curve->coverage.budget_truncated = true;
      break;
    }

    double step = std::min(h, t_end - t_curr);
    double t_next = t_curr + step;
    Eval1D e_next = eval_fn(t_next, track_corridor);

    bool shrunk = false;
    while (step > params.min_step * 2.0) {
      if (CheckStop(stop)) {
        *stop_requested = true;
        curve->coverage.budget_truncated = true;
        break;
      }
      bool need_shrink = !e_next.valid;
      if (!need_shrink && e_curr.sky_speed > params.speed_tol && e_next.sky_speed > params.speed_tol) {
        const double d_angle = so3::AngleBetween(e_curr.dx, e_next.dx);
        const double sky_step = so3::AngleBetween(e_curr.sky, e_next.sky);
        need_shrink = (sky_step > params.max_turning_angle) || (d_angle > params.max_turning_angle && d_angle < 2.5);
      }
      if (!need_shrink) {
        break;
      }
      step = std::max(params.min_step, step * 0.5);
      t_next = t_curr + step;
      e_next = eval_fn(t_next, track_corridor);
      shrunk = true;
    }
    if (*stop_requested) {
      break;
    }
    // Guard against float64 ULP stagnation when |t_curr| is large enough that t_curr + step == t_curr.
    if (!(t_next > t_curr) || (t_next - t_curr) <= 1e-13) {
      curve->coverage.budget_truncated = true;
      break;
    }

    auto commit_valid_step = [&](double t_target, const Eval1D& e_target) -> bool {
      const double seg_step = t_target - t_curr;
      if (seg_step <= 1e-13) {
        return true;
      }
      std::vector<CurveEvent> step_events;

      if (e_target.sky_speed > params.speed_tol) {
        if (have_last_moving) {
          double u_stat = 0.0;
          Eval1D e_stat;
          auto eval_stat_u = [&](double u) { return eval_fn(last_moving_t + u * (t_target - last_moving_t), false); };
          if (DetectStationaryPoint1D(last_moving_e, e_target, params.stationary_tol, params.bisection_iters,
                                      eval_stat_u, &u_stat, &e_stat)) {
            CurveEvent ev;
            CopyNodeToEvent(NodeFromEval1D(e_stat, last_moving_arclen + u_stat * (t_target - last_moving_t)),
                            CurveEventKind::kStationaryPoint, -1, &ev);
            step_events.push_back(ev);
          }
        }
        have_last_moving = true;
        last_moving_t = t_target;
        last_moving_e = e_target;
        last_moving_arclen = arclen + seg_step;
      }

      auto eval_tir_u = [&](double u, int k, double* disc_out) {
        const Eval1D em = eval_fn(t_curr + u * seg_step, false);
        if (!em.valid) {
          return false;
        }
        *disc_out = em.internal_disc[k];
        return true;
      };
      auto emit_tir_u = [&](double u_tir, int k) {
        const Eval1D e_tir = eval_fn(t_curr + u_tir * seg_step, false);
        if (e_tir.valid) {
          CurveEvent ev;
          CopyNodeToEvent(NodeFromEval1D(e_tir, arclen + u_tir * seg_step), CurveEventKind::kTirOnset, k, &ev);
          step_events.push_back(ev);
        }
      };
      DetectStepTirOnsets(slot_count, e_curr.internal_disc, e_target.internal_disc, seg_step, margin_lip,
                          params.bisection_iters, eval_tir_u, emit_tir_u);

      int cut_cnt = 0;
      double u_cuts[2] = { 0.0, 0.0 };
      if (track_corridor) {
        auto eval_corr_u = [&](double u, bool /*need_margin*/, bool* open_out, double* margin_out) {
          const Eval1D em = eval_fn(t_curr + u * seg_step, true);
          if (!em.valid) {
            return false;
          }
          *open_out = em.corridor_open;
          *margin_out = em.corridor_margin;
          return true;
        };
        cut_cnt =
            FindStepCorridorCrossings(e_curr.corridor_open, e_curr.corridor_margin, e_target.corridor_open,
                                      e_target.corridor_margin, seg_step, params.bisection_iters, eval_corr_u, u_cuts);
      }

      auto try_push_budgeted = [&](const Eval1D& e, double ds_add, bool open_flag) -> bool {
        if (static_cast<int>(curve->nodes.size()) >= params.max_nodes_per_branch) {
          curve->coverage.budget_truncated = true;
          AppendEventsWithinArclength(step_events, curve->nodes.back().arclength, &curve->events);
          return false;
        }
        arclen += ds_add;
        push_node(e, arclen, open_flag);
        return true;
      };

      double t_prev_p = t_curr;
      bool cur_open = e_curr.corridor_open;
      for (int c = 0; c < cut_cnt; c++) {
        const double t_cut = t_curr + u_cuts[c] * seg_step;
        const Eval1D e_cut = eval_fn(t_cut, true);
        if (!e_cut.valid) {
          break;
        }
        const double ds_c = t_cut - t_prev_p;
        if (ds_c > 1e-13) {
          if (!try_push_budgeted(e_cut, ds_c, cur_open)) {
            return false;
          }
          t_prev_p = t_cut;
        }
        CurveEvent ev;
        CopyNodeToEvent(curve->nodes.back(), CurveEventKind::kCorridorCutoff, -1, &ev);
        curve->events.push_back(ev);
        cur_open = !cur_open;
      }

      const double ds_rem = t_target - t_prev_p;
      if (ds_rem > 1e-13 && !try_push_budgeted(e_target, ds_rem, cur_open)) {
        return false;
      }
      AppendEventsWithinArclength(step_events, curve->nodes.back().arclength, &curve->events);
      t_curr = t_target;
      e_curr = e_target;
      return true;
    };

    auto finish_at_gap = [&](double t_inv, double step_hint, bool has_resume, double resume_t) {
      double t_bnd = t_curr;
      int fail_idx = -1;
      Eval1D e_bnd = BisectOpticalBoundary1D(eval_fn, t_curr, t_inv, params.bisection_iters, &t_bnd, &fail_idx);
      if (track_corridor) {
        const Eval1D e_bnd_c = eval_fn(t_bnd, true);
        if (e_bnd_c.valid) {
          e_bnd = e_bnd_c;
        }
      }
      if (commit_valid_step(t_bnd, e_bnd)) {
        CurveEvent ev;
        CopyNodeToEvent(curve->nodes.back(), CurveEventKind::kOpticalBoundary, fail_idx, &ev);
        curve->events.push_back(ev);
      }
      *gap = { true, t_inv, step_hint, has_resume, resume_t };
    };

    if (!e_next.valid) {
      finish_at_gap(t_next, step, false, t_end);
      break;
    }

    double t_gap_inv = t_next;
    if (FindInteriorOpticalTransition1D(eval_fn, t_curr, e_curr.margin.min_effective_margin, t_next,
                                        e_next.margin.min_effective_margin, margin_lip, /*seek_valid=*/false,
                                        &t_gap_inv)) {
      finish_at_gap(t_gap_inv, std::min(step, t_next - t_gap_inv), true, t_next);
      break;
    }

    if (!commit_valid_step(t_next, e_next)) {
      break;
    }
    h = shrunk ? std::max(params.min_step, step) : std::min(params.max_step, step * 1.25);
  }

  const bool reached_end =
      !gap->hit && !curve->coverage.budget_truncated && !(*stop_requested) && std::fabs(t_curr - t_end) <= 1e-9;
  if (reached_end) {
    if (is_periodic_full) {
      curve->coverage.closed = true;
      frag->ends_at_seam = true;
    } else if (right_is_domain || right_fail_idx >= 0) {
      CurveEvent ev;
      CopyNodeToEvent(curve->nodes.back(),
                      right_is_domain ? CurveEventKind::kDomainBoundary : CurveEventKind::kOpticalBoundary,
                      right_is_domain ? free_ax : right_fail_idx, &ev);
      curve->events.push_back(ev);
    } else {
      frag->ends_at_seam = true;
    }
  }

  std::sort(curve->events.begin(), curve->events.end(),
            [](const CurveEvent& a, const CurveEvent& b) { return a.arclength < b.arclength; });
  BuildSegments(curve->nodes, frag->node_stationary, frag->edge_corridor_open, frag->first_node_corridor_open,
                params.speed_tol, &curve->segments);
}

PoseCurve StitchPeriodicSeam1D(const Fragment1D& back_frag, const Fragment1D& front_frag, double speed_tol,
                               int max_nodes_per_branch) {
  PoseCurve stitched;
  stitched.kind = back_frag.curve.kind;
  stitched.coverage.seed_grid_n = back_frag.curve.coverage.seed_grid_n;
  stitched.coverage.budget_truncated =
      back_frag.curve.coverage.budget_truncated || front_frag.curve.coverage.budget_truncated;

  const double back_len = back_frag.curve.nodes.back().arclength;
  stitched.nodes = back_frag.curve.nodes;
  std::vector<bool> node_stationary = back_frag.node_stationary;
  std::vector<bool> edge_corridor_open = back_frag.edge_corridor_open;
  stitched.events = back_frag.curve.events;

  if (max_nodes_per_branch > 0 && static_cast<int>(stitched.nodes.size()) > max_nodes_per_branch) {
    stitched.nodes.resize(static_cast<size_t>(max_nodes_per_branch));
    node_stationary.resize(static_cast<size_t>(max_nodes_per_branch));
    edge_corridor_open.resize(static_cast<size_t>(max_nodes_per_branch - 1));
    stitched.coverage.budget_truncated = true;
  }

  for (size_t i = 1; i < front_frag.curve.nodes.size(); i++) {
    if (max_nodes_per_branch > 0 && static_cast<int>(stitched.nodes.size()) >= max_nodes_per_branch) {
      stitched.coverage.budget_truncated = true;
      break;
    }
    PoseCurveNode nd = front_frag.curve.nodes[i];
    nd.arclength += back_len;
    stitched.nodes.push_back(nd);
    node_stationary.push_back(front_frag.node_stationary[i]);
    edge_corridor_open.push_back(front_frag.edge_corridor_open[i - 1]);
  }
  for (CurveEvent ev : front_frag.curve.events) {
    ev.arclength += back_len;
    stitched.events.push_back(ev);
  }
  if (!stitched.nodes.empty()) {
    ClipEventsToArclength(stitched.nodes.back().arclength, &stitched.events);
  }
  std::sort(stitched.events.begin(), stitched.events.end(),
            [](const CurveEvent& a, const CurveEvent& b) { return a.arclength < b.arclength; });
  BuildSegments(stitched.nodes, node_stationary, edge_corridor_open, back_frag.first_node_corridor_open, speed_tol,
                &stitched.segments);
  return stitched;
}

void Trace1DIntervalResumingGaps(FunctionRef<Eval1D(double, bool)> eval_fn, int slot_count, int free_ax, double t_start,
                                 double t_end, bool is_periodic_full, bool left_is_domain, bool right_is_domain,
                                 int left_fail_idx, int right_fail_idx, double margin_lip, double max_probe_step,
                                 bool track_corridor, const Family1DParams& params, const DiscoveryStop* stop,
                                 std::vector<Fragment1D>* fragments, bool* stop_requested) {
  double cur_start = t_start;
  bool cur_periodic_full = is_periodic_full;
  bool cur_left_domain = left_is_domain;
  int cur_left_fail = left_fail_idx;
  const bool final_right_domain = is_periodic_full ? false : right_is_domain;
  const int final_right_fail = is_periodic_full ? -1 : right_fail_idx;

  while (cur_start < t_end - 1e-13) {
    if (CheckStop(stop)) {
      *stop_requested = true;
      break;
    }
    Fragment1D frag;
    GapReport gap;
    Trace1DSingleFragment(eval_fn, slot_count, free_ax, cur_start, t_end, cur_periodic_full, cur_left_domain,
                          final_right_domain, cur_left_fail, final_right_fail, margin_lip, track_corridor, params, stop,
                          &frag, stop_requested, &gap);
    if (!frag.curve.nodes.empty()) {
      fragments->push_back(std::move(frag));
    }
    if (!gap.hit || *stop_requested) {
      break;
    }

    double t_inv = gap.invalid_t;
    double t_probe = gap.resume_valid_t;
    bool found_resume = gap.has_resume_valid;
    if (!found_resume) {
      double p_step = std::max(params.min_step, gap.step_hint);
      t_probe = t_inv;
      while (t_probe < t_end - 1e-13) {
        if (CheckStop(stop)) {
          *stop_requested = true;
          break;
        }
        const double next_probe = std::min(t_end, t_probe + p_step);
        if (!(next_probe > t_probe)) {
          if (p_step >= max_probe_step) {
            break;
          }
          p_step = std::min(p_step * 2.0, max_probe_step);
          continue;
        }
        t_probe = next_probe;
        if (eval_fn(t_probe, false).valid) {
          found_resume = true;
          break;
        }
        t_inv = t_probe;
        p_step = std::min(p_step * 2.0, max_probe_step);
      }
    }
    if (!found_resume || *stop_requested) {
      break;
    }

    double t_resume = cur_start;
    int resume_fail_idx = -1;
    BisectOpticalBoundary1D(eval_fn, t_probe, t_inv, params.bisection_iters, &t_resume, &resume_fail_idx);
    if (!(t_resume > cur_start)) {
      break;
    }
    cur_start = t_resume;
    cur_periodic_full = false;
    cur_left_domain = false;
    cur_left_fail = resume_fail_idx;
  }
}

}  // namespace

Eval1D Evaluate1DAlongTangent(const FaceNormalTable& table, const int* slots, int slot_count, double refractive_index,
                              const double sun_dir[3], const double param_rad[3], const double tangent_dir[3],
                              Corridor* corridor, bool eval_corridor) {
  Eval1D out;
  so3::Copy3(param_rad, out.param);
  so3::Copy3(tangent_dir, out.tangent);
  Jet<1> ang[3];
  for (int k = 0; k < 3; k++) {
    ang[k] = Jet<1>(out.param[k]);
    ang[k].v[0] = tangent_dir[k];
  }
  Jet<1> pose_j[9];
  EulerPose(ang[0], ang[1], ang[2], pose_j);
  for (int i = 0; i < 9; i++) {
    out.pose[i] = pose_j[i].a;
  }
  WorldToBody(out.pose, sun_dir, out.u);

  const Jet<1> incident[3] = { Jet<1>(-sun_dir[0]), Jet<1>(-sun_dir[1]), Jet<1>(-sun_dir[2]) };
  Jet<1> outgoing[3];
  ChainDomain domain;
  double margin_buf[kMaxDomainMargins]{};
  domain.margins = margin_buf;
  ChainInterfaceDiagnostics<Jet<1>> diag;
  const bool chain_ok = TracePathChain<Jet<1>, ChainEvaluation::kFirstFailure>(
      table, slots, slot_count, Jet<1>(refractive_index), incident, pose_j, outgoing, nullptr, &domain, &diag);
  out.margin = ClassifyChainMargins(chain_ok, domain, slot_count);
  if (!out.margin.valid) {
    return out;
  }
  for (int i = 0; i < 3; i++) {
    if (!std::isfinite(outgoing[i].a) || !std::isfinite(outgoing[i].v[0])) {
      return out;
    }
    out.sky[i] = -outgoing[i].a;
    out.dx[i] = -outgoing[i].v[0];
  }
  out.sky_speed = so3::Norm3(out.dx);
  for (int k = 1; k < slot_count - 1; k++) {
    out.internal_disc[k] = diag.discriminant[k].a;
  }
  out.valid = true;
  if (eval_corridor) {
    EvaluateCorridorStateAndMargin(corridor, out.pose, sun_dir, refractive_index, true, &out.corridor_open,
                                   &out.corridor_margin);
  }
  return out;
}

Eval1D Evaluate1DAtPose(const FaceNormalTable& table, const int* slots, int slot_count, double refractive_index,
                        const double sun_dir[3], const double param_rad[3], int vary_ax, Corridor* corridor,
                        bool eval_corridor) {
  double t_dir[3] = { 0.0, 0.0, 0.0 };
  t_dir[vary_ax] = 1.0;
  return Evaluate1DAlongTangent(table, slots, slot_count, refractive_index, sun_dir, param_rad, t_dir, corridor,
                                eval_corridor);
}

PoseCurveNode NodeFromEval1D(const Eval1D& e, double arclength) {
  PoseCurveNode node;
  so3::Copy3(e.param, node.param);
  so3::Copy3(e.u, node.u);
  so3::Copy3(e.sky, node.sky);
  so3::Copy3(e.tangent, node.tangent);
  std::copy_n(e.pose, 9, node.pose);
  node.sky_speed = e.sky_speed;
  node.sigma1 = e.sky_speed;
  node.arclength = arclength;
  return node;
}

Eval1D BisectOpticalBoundary1D(FunctionRef<Eval1D(double, bool)> eval_fn, double t_valid, double t_invalid, int iters,
                               double* t_out, int* failing_index_out) {
  double tv = t_valid;
  double ti = t_invalid;
  int fail_idx = eval_fn(ti, false).margin.failing_index;
  for (int it = 0; it < iters; it++) {
    const double tm = 0.5 * (tv + ti);
    const Eval1D em = eval_fn(tm, false);
    if (em.valid) {
      tv = tm;
    } else {
      ti = tm;
      if (em.margin.failing_index >= 0) {
        fail_idx = em.margin.failing_index;
      }
    }
  }
  *t_out = tv;
  *failing_index_out = fail_idx;
  return eval_fn(tv, true);
}

// Unified dual probe on [t_a, t_b]: seek_valid=false finds an interior invalid optical gap inside
// a valid step; seek_valid=true finds a narrow interior valid island inside an invalid scan cell.
bool FindInteriorOpticalTransition1D(FunctionRef<Eval1D(double, bool)> eval_fn, double t_a, double m_a, double t_b,
                                     double m_b, double margin_lip, bool seek_valid, double* t_out) {
  const double step = t_b - t_a;
  if (!(step > 1e-13) || (seek_valid ? (m_a + m_b + margin_lip * step <= 0.0) : (m_a + m_b > margin_lip * step))) {
    return false;
  }
  const double ts[5] = { t_a, t_a + 0.25 * step, t_a + 0.5 * step, t_a + 0.75 * step, t_b };
  int best_idx = (seek_valid ? (m_a >= m_b) : (m_a <= m_b)) ? 0 : 4;
  double best_m = seek_valid ? std::max(m_a, m_b) : std::min(m_a, m_b);
  for (int k = 1; k <= 3; k++) {
    const Eval1D ek = eval_fn(ts[k], false);
    if (ek.valid == seek_valid) {
      *t_out = ts[k];
      return true;
    }
    if (seek_valid ? (ek.margin.min_effective_margin > best_m) : (ek.margin.min_effective_margin < best_m)) {
      best_m = ek.margin.min_effective_margin;
      best_idx = k;
    }
  }
  const double lo = ts[std::max(0, best_idx - 1)];
  const double hi = ts[std::min(4, best_idx + 1)];
  if (seek_valid ? (best_m + 0.5 * margin_lip * (hi - lo) <= 0.0) : (best_m > 0.5 * margin_lip * (hi - lo))) {
    return false;
  }
  auto probe = [&](double tx, double* score_out) -> ProbeStatus {
    const Eval1D ex = eval_fn(tx, false);
    if (ex.valid == seek_valid) {
      return ProbeStatus::kFound;
    }
    *score_out = ex.margin.min_effective_margin;
    return ProbeStatus::kValue;
  };
  double t_cand = 0.5 * (lo + hi);
  if (GoldenSectionProbe(lo, hi, seek_valid, 24, probe, &t_cand) || eval_fn(t_cand, false).valid == seek_valid) {
    *t_out = t_cand;
    return true;
  }
  return false;
}

CausticTraceResult Trace1DWithEvaluator(const PoseAxis& pax, int free_ax, int slot_count, double margin_lip,
                                        FunctionRef<Eval1D(double, bool)> eval_fn, bool track_corridor,
                                        const Family1DParams& params, const DiscoveryStop* stop) {
  CausticTraceResult result;
  result.coverage.seed_grid_n = params.scan_samples;

  const int n = params.scan_samples;
  const int sample_cnt = pax.periodic ? n : (n + 1);
  const double span = pax.Span();
  const double max_probe_step = std::max(params.min_step * 4.0, 0.25 * span / static_cast<double>(n));

  std::vector<bool> valid(static_cast<size_t>(sample_cnt), false);
  std::vector<double> scan_margin(static_cast<size_t>(sample_cnt), -1.0);
  bool any_nonzero_speed = false;
  int valid_cnt = 0;

  for (int i = 0; i < sample_cnt; i++) {
    if ((i & 31) == 0 && CheckStop(stop)) {
      result.stop_requested = true;
      result.coverage.budget_truncated = true;
      return result;
    }
    const Eval1D e = eval_fn(pax.min_val + span * static_cast<double>(i) / static_cast<double>(n), false);
    scan_margin[static_cast<size_t>(i)] = e.margin.min_effective_margin;
    if (e.valid) {
      valid[static_cast<size_t>(i)] = true;
      valid_cnt++;
      if (e.sky_speed > NumericalZeroSigmaFloor(e.margin.min_effective_margin)) {
        any_nonzero_speed = true;
      }
    }
  }

  struct NarrowIsland1D {
    double t_left = 0.0;
    double t_right = 0.0;
    int left_fail_idx = -1;
    int right_fail_idx = -1;
  };
  std::vector<NarrowIsland1D> narrow_islands;
  for (int i = 0; i < n; i++) {
    if ((i & 31) == 0 && CheckStop(stop)) {
      result.stop_requested = true;
      result.coverage.budget_truncated = true;
      return result;
    }
    const int i_next = pax.periodic ? ((i + 1) % n) : (i + 1);
    if (valid[static_cast<size_t>(i)] || valid[static_cast<size_t>(i_next)]) {
      continue;
    }
    const double t_a = pax.min_val + span * static_cast<double>(i) / static_cast<double>(n);
    const double t_b = pax.min_val + span * static_cast<double>(i + 1) / static_cast<double>(n);
    double t_val = 0.0;
    if (FindInteriorOpticalTransition1D(eval_fn, t_a, scan_margin[static_cast<size_t>(i)], t_b,
                                        scan_margin[static_cast<size_t>(i_next)], margin_lip, /*seek_valid=*/true,
                                        &t_val)) {
      const Eval1D ev_v = eval_fn(t_val, false);
      if (ev_v.valid && ev_v.sky_speed > NumericalZeroSigmaFloor(ev_v.margin.min_effective_margin)) {
        any_nonzero_speed = true;
      }
      NarrowIsland1D isl;
      BisectOpticalBoundary1D(eval_fn, t_val, t_a, params.bisection_iters, &isl.t_left, &isl.left_fail_idx);
      BisectOpticalBoundary1D(eval_fn, t_val, t_b, params.bisection_iters, &isl.t_right, &isl.right_fail_idx);
      if (isl.t_right >= isl.t_left) {
        narrow_islands.push_back(isl);
      }
    }
  }

  if (valid_cnt == 0 && narrow_islands.empty()) {
    result.generic_rank = -1;
    return result;
  }
  result.generic_rank = any_nonzero_speed ? 1 : 0;

  auto trace_interval = [&](double t_lo, double t_hi, bool periodic_full, bool left_dom, bool right_dom, int left_fail,
                            int right_fail) {
    std::vector<Fragment1D> fragments;
    bool stopped = false;
    Trace1DIntervalResumingGaps(eval_fn, slot_count, free_ax, t_lo, t_hi, periodic_full, left_dom, right_dom, left_fail,
                                right_fail, margin_lip, max_probe_step, track_corridor, params, stop, &fragments,
                                &stopped);
    result.stop_requested = result.stop_requested || stopped;
    if (periodic_full && fragments.size() >= 2 && fragments.front().starts_at_seam && fragments.back().ends_at_seam) {
      PoseCurve stitched =
          StitchPeriodicSeam1D(fragments.back(), fragments.front(), params.speed_tol, params.max_nodes_per_branch);
      result.coverage.budget_truncated = result.coverage.budget_truncated || stitched.coverage.budget_truncated;
      result.curves.push_back(std::move(stitched));
      for (size_t k = 1; k + 1 < fragments.size(); k++) {
        result.coverage.budget_truncated =
            result.coverage.budget_truncated || fragments[k].curve.coverage.budget_truncated;
        result.curves.push_back(std::move(fragments[k].curve));
      }
      return;
    }
    for (Fragment1D& f : fragments) {
      result.coverage.closed = result.coverage.closed || f.curve.coverage.closed;
      result.coverage.budget_truncated = result.coverage.budget_truncated || f.curve.coverage.budget_truncated;
      result.curves.push_back(std::move(f.curve));
    }
  };

  if (pax.periodic && valid_cnt == sample_cnt) {
    trace_interval(pax.min_val, pax.max_val, true, false, false, -1, -1);
    return result;
  }

  for (const NarrowIsland1D& isl : narrow_islands) {
    if (CheckStop(stop)) {
      result.stop_requested = true;
      result.coverage.budget_truncated = true;
      return result;
    }
    trace_interval(isl.t_left, isl.t_right, false, false, false, isl.left_fail_idx, isl.right_fail_idx);
  }
  if (valid_cnt == 0) {
    return result;
  }

  int start_offset = 0;
  if (pax.periodic) {
    for (int i = 0; i < sample_cnt; i++) {
      if (!valid[static_cast<size_t>(i)]) {
        start_offset = i;
        break;
      }
    }
  }

  int i = 0;
  while (i < sample_cnt) {
    if (CheckStop(stop)) {
      result.stop_requested = true;
      result.coverage.budget_truncated = true;
      break;
    }
    if (!valid[static_cast<size_t>((start_offset + i) % sample_cnt)]) {
      i++;
      continue;
    }
    const int run_start = i;
    while (i < sample_cnt && valid[static_cast<size_t>((start_offset + i) % sample_cnt)]) {
      i++;
    }
    const int run_end = i - 1;
    auto t_at = [&](int s_step) {
      return pax.min_val + span * static_cast<double>(start_offset + s_step) / static_cast<double>(n);
    };

    double t_left = t_at(run_start);
    bool left_dom = (!pax.periodic && run_start == 0);
    int left_fail = -1;
    if (!left_dom) {
      BisectOpticalBoundary1D(eval_fn, t_left, t_at(run_start - 1), params.bisection_iters, &t_left, &left_fail);
    }

    double t_right = t_at(run_end);
    bool right_dom = (!pax.periodic && run_end == sample_cnt - 1);
    int right_fail = -1;
    if (!right_dom) {
      BisectOpticalBoundary1D(eval_fn, t_right, t_at(run_end + 1), params.bisection_iters, &t_right, &right_fail);
    }

    trace_interval(t_left, t_right, false, left_dom, right_dom, left_fail, right_fail);
  }
  return result;
}

}  // namespace caustic_detail

CausticTraceResult TraceFamily1D(const FaceNormalTable& table, const int* slots, int slot_count,
                                 double refractive_index, const double sun_dir[3], const PoseManifold& manifold,
                                 Corridor* corridor, const Family1DParams& params, const DiscoveryStop* stop) {
  CausticTraceResult result;
  result.coverage.seed_grid_n = params.scan_samples;
  if (!ValidateParams(params) || !manifold.IsValid() || manifold.Dimension() != 1 || slots == nullptr ||
      slot_count < 1 || slot_count > kMaxFaceCount || !std::isfinite(refractive_index) || refractive_index <= 0.0 ||
      !ValidateUnitVector(sun_dir)) {
    return result;
  }

  int free_axes[3]{};
  manifold.FreeAxes(free_axes);
  const int ax = free_axes[0];
  const PoseAxis& pax = manifold.axes[ax];

  auto eval_fn = [&](double t_val, bool eval_corridor) {
    double param[3] = { manifold.axes[0].value, manifold.axes[1].value, manifold.axes[2].value };
    param[ax] = caustic_detail::WrapAxis(t_val, pax);
    return caustic_detail::Evaluate1DAtPose(table, slots, slot_count, refractive_index, sun_dir, param, ax, corridor,
                                            eval_corridor);
  };
  return caustic_detail::Trace1DWithEvaluator(pax, ax, slot_count, caustic_detail::MarginLip(refractive_index), eval_fn,
                                              corridor != nullptr, params, stop);
}

}  // namespace lumice::analytic
