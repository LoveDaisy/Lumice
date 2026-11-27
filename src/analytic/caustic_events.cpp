#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "analytic/caustic_internal.hpp"
#include "analytic/so3.hpp"

namespace lumice::analytic {
namespace caustic_detail {

bool EvaluateCorridorOpen(Corridor* corridor, const double pose[9], const double sun_dir[3], double refractive_index) {
  if (corridor == nullptr) {
    return true;
  }
  const double incident[3] = { -sun_dir[0], -sun_dir[1], -sun_dir[2] };
  double s_body[3];
  WorldToBody(pose, incident, s_body);
  const EntryMeasure em = corridor->Evaluate(s_body, refractive_index);
  return em.status == EntryMeasureStatus::kOk && em.value > 0.0;
}

double EvaluateCorridorOverlapMargin(const Corridor* corridor, const double pose[9], const double sun_dir[3],
                                     double refractive_index) {
  if (corridor == nullptr) {
    return 1.0;
  }
  const double incident[3] = { -sun_dir[0], -sun_dir[1], -sun_dir[2] };
  double s_body[3];
  WorldToBody(pose, incident, s_body);
  return corridor->OverlapMargin(s_body, refractive_index);
}

void EvaluateCorridorStateAndMargin(Corridor* corridor, const double pose[9], const double sun_dir[3],
                                    double refractive_index, bool need_margin, bool* open_out, double* margin_out) {
  if (!need_margin) {
    *open_out = EvaluateCorridorOpen(corridor, pose, sun_dir, refractive_index);
    return;
  }
  const double m = EvaluateCorridorOverlapMargin(corridor, pose, sun_dir, refractive_index);
  *margin_out = m;
  *open_out =
      (std::isfinite(m) && m >= -1e-9) ? EvaluateCorridorOpen(corridor, pose, sun_dir, refractive_index) : false;
}

bool GoldenSectionProbe(double lo, double hi, bool maximize, int iters, FunctionRef<ProbeStatus(double, double*)> probe,
                        double* x_out) {
  constexpr double kInvPhi = 0.6180339887498949;
  double c = hi - kInvPhi * (hi - lo);
  double d = lo + kInvPhi * (hi - lo);
  double vc = 0.0;
  double vd = 0.0;
  const ProbeStatus sc = probe(c, &vc);
  if (sc == ProbeStatus::kFound) {
    *x_out = c;
    return true;
  }
  const ProbeStatus sd = probe(d, &vd);
  if (sd == ProbeStatus::kFound) {
    *x_out = d;
    return true;
  }
  if (sc != ProbeStatus::kValue || sd != ProbeStatus::kValue) {
    *x_out = 0.5 * (lo + hi);
    return false;
  }
  for (int it = 0; it < iters; it++) {
    const bool go_left = maximize ? (vc > vd) : (vc < vd);
    double next_x = 0.0;
    double* next_v = nullptr;
    if (go_left) {
      hi = d;
      d = c;
      vd = vc;
      c = hi - kInvPhi * (hi - lo);
      next_x = c;
      next_v = &vc;
    } else {
      lo = c;
      c = d;
      vc = vd;
      d = lo + kInvPhi * (hi - lo);
      next_x = d;
      next_v = &vd;
    }
    const ProbeStatus s = probe(next_x, next_v);
    if (s == ProbeStatus::kFound) {
      *x_out = next_x;
      return true;
    }
    if (s != ProbeStatus::kValue) {
      break;
    }
  }
  *x_out = 0.5 * (lo + hi);
  return false;
}

// Shared step-internal TIR onset detector over u in [0, 1] for all tracers. Handles both single
// endpoint sign changes and same-sign interior double crossings via quartile + golden-section.
void DetectStepTirOnsets(int slot_count, const double disc_a[kMaxFaceCount], const double disc_b[kMaxFaceCount],
                         double step_len, double margin_lip, int bisection_iters,
                         FunctionRef<bool(double, int, double*)> eval_disc, FunctionRef<void(double, int)> emit_tir) {
  auto bisect_tir = [&](int k, double ua, bool pos_a, double ub) {
    for (int it = 0; it < bisection_iters; it++) {
      const double um = 0.5 * (ua + ub);
      double dm = 0.0;
      if (!eval_disc(um, k, &dm)) {
        break;
      }
      ((dm > 0.0) == pos_a ? ua : ub) = um;
    }
    emit_tir(0.5 * (ua + ub), k);
  };

  for (int k = 1; k < slot_count - 1; k++) {
    const double da = disc_a[k];
    const double db = disc_b[k];
    const bool pos_a = da > 0.0;
    if (pos_a != (db > 0.0)) {
      bisect_tir(k, 0.0, pos_a, 1.0);
      continue;
    }
    if (std::fabs(da) + std::fabs(db) > margin_lip * step_len) {
      continue;
    }
    const double us[5] = { 0.0, 0.25, 0.5, 0.75, 1.0 };
    int best_idx = (pos_a ? (da <= db) : (da >= db)) ? 0 : 4;
    double best_d = pos_a ? std::min(da, db) : std::max(da, db);
    bool found_opp = false;
    double u_opp = 0.0;
    for (int q = 1; q <= 3; q++) {
      double dq = 0.0;
      if (!eval_disc(us[q], k, &dq)) {
        continue;
      }
      if ((dq > 0.0) != pos_a) {
        found_opp = true;
        u_opp = us[q];
        break;
      }
      if (pos_a ? (dq < best_d) : (dq > best_d)) {
        best_d = dq;
        best_idx = q;
      }
    }
    if (!found_opp) {
      const double lo = us[std::max(0, best_idx - 1)];
      const double hi = us[std::min(4, best_idx + 1)];
      if (std::fabs(best_d) <= 0.5 * margin_lip * step_len * (hi - lo)) {
        auto probe = [&](double ux, double* score_out) -> ProbeStatus {
          double dx = 0.0;
          if (!eval_disc(ux, k, &dx)) {
            return ProbeStatus::kAbort;
          }
          if ((dx > 0.0) != pos_a) {
            return ProbeStatus::kFound;
          }
          *score_out = dx;
          return ProbeStatus::kValue;
        };
        found_opp = GoldenSectionProbe(lo, hi, !pos_a, 24, probe, &u_opp);
      }
    }
    if (found_opp) {
      bisect_tir(k, 0.0, pos_a, u_opp);
      bisect_tir(k, u_opp, !pos_a, 1.0);
    }
  }
}

// Shared step-internal corridor crossing detector over u in [0, 1] returning 0, 1, or 2 crossings.
int FindStepCorridorCrossings(bool open_a, double margin_a, bool open_b, double margin_b, double step_len,
                              int bisection_iters, FunctionRef<bool(double, bool, bool*, double*)> eval_corr,
                              double u_cuts_out[2]) {
  auto bisect_corridor = [&](double ua, bool side_a_open, double ub) {
    for (int it = 0; it < bisection_iters; it++) {
      const double um = 0.5 * (ua + ub);
      bool open_m = side_a_open;
      double dummy_m = 0.0;
      if (!eval_corr(um, false, &open_m, &dummy_m)) {
        break;
      }
      (open_m == side_a_open ? ua : ub) = um;
    }
    return 0.5 * (ua + ub);
  };

  if (open_a != open_b) {
    u_cuts_out[0] = bisect_corridor(0.0, open_a, 1.0);
    return 1;
  }

  const bool search_max = !open_a;
  const double us[5] = { 0.0, 0.25, 0.5, 0.75, 1.0 };
  double ms[5] = { margin_a, 0.0, 0.0, 0.0, margin_b };
  bool m_valid[5] = { std::isfinite(margin_a), false, false, false, std::isfinite(margin_b) };
  int best_idx = -1;
  double best_m = 0.0;
  auto consider_m = [&](int idx, double m_val) {
    if (std::isfinite(m_val) && (best_idx < 0 || (search_max ? (m_val > best_m) : (m_val < best_m)))) {
      best_idx = idx;
      best_m = m_val;
    }
  };
  consider_m(0, margin_a);
  consider_m(4, margin_b);

  bool found_opp = false;
  double u_opp = 0.0;
  for (int q = 1; q <= 3; q++) {
    bool open_q = open_a;
    double mq = std::numeric_limits<double>::quiet_NaN();
    if (!eval_corr(us[q], true, &open_q, &mq)) {
      continue;
    }
    if (open_q != open_a) {
      found_opp = true;
      u_opp = us[q];
      break;
    }
    if (std::isfinite(mq)) {
      ms[q] = mq;
      m_valid[q] = true;
      consider_m(q, mq);
    }
  }

  if (!found_opp && best_idx >= 0) {
    double max_slope_est = step_len;
    if (m_valid[0] && m_valid[1]) {
      max_slope_est = std::max(max_slope_est, std::fabs(ms[1] - ms[0]));
    }
    if (m_valid[3] && m_valid[4]) {
      max_slope_est = std::max(max_slope_est, std::fabs(ms[4] - ms[3]));
    }
    if (m_valid[0] && m_valid[4]) {
      max_slope_est = std::max(max_slope_est, std::fabs(ms[4] - ms[0]));
    }
    const bool worth_search = (best_idx >= 1 && best_idx <= 3) || (search_max && best_m >= 0.0) ||
                              (search_max ? (best_m > -4.0 * max_slope_est) : (best_m < 4.0 * max_slope_est));
    if (worth_search) {
      auto probe = [&](double ux, double* score_out) -> ProbeStatus {
        bool open_x = open_a;
        double mx = std::numeric_limits<double>::quiet_NaN();
        if (!eval_corr(ux, true, &open_x, &mx)) {
          return ProbeStatus::kAbort;
        }
        if (open_x != open_a) {
          return ProbeStatus::kFound;
        }
        if (!std::isfinite(mx)) {
          return ProbeStatus::kAbort;
        }
        *score_out = mx;
        return ProbeStatus::kValue;
      };
      found_opp = GoldenSectionProbe(us[std::max(0, best_idx - 1)], us[std::min(4, best_idx + 1)], search_max, 24,
                                     probe, &u_opp);
    }
  }

  if (found_opp) {
    u_cuts_out[0] = bisect_corridor(0.0, open_a, u_opp);
    u_cuts_out[1] = bisect_corridor(u_opp, !open_a, 1.0);
    return 2;
  }
  return 0;
}

// Shared 1D stationary-point detector between moving samples e_a (u=0) and e_b (u=1).
bool DetectStationaryPoint1D(const Eval1D& e_a, const Eval1D& e_b, double stationary_tol, int bisection_iters,
                             FunctionRef<Eval1D(double)> eval_u, double* u_stat_out, Eval1D* e_stat_out) {
  if (!(so3::Dot3(e_a.dx, e_b.dx) < 0.0)) {
    return false;
  }
  double ref[3] = { e_b.dx[0] - e_a.dx[0], e_b.dx[1] - e_a.dx[1], e_b.dx[2] - e_a.dx[2] };
  const double rn = so3::Norm3(ref);
  if (!(rn > 0.0)) {
    return false;
  }
  for (double& c : ref) {
    c /= rn;
  }
  double ua = 0.0;
  double ub = 1.0;
  double fa = so3::Dot3(e_a.dx, ref);
  for (int it = 0; it < bisection_iters; it++) {
    const double um = 0.5 * (ua + ub);
    const Eval1D em = eval_u(um);
    if (!em.valid) {
      break;
    }
    const double fm = so3::Dot3(em.dx, ref);
    if ((fa <= 0.0) == (fm <= 0.0)) {
      ua = um;
      fa = fm;
    } else {
      ub = um;
    }
  }
  const double u_stat = 0.5 * (ua + ub);
  const Eval1D e_stat = eval_u(u_stat);
  if (!e_stat.valid || e_stat.sky_speed > stationary_tol) {
    return false;
  }
  const Eval1D e_left = eval_u(0.5 * u_stat);
  const Eval1D e_right = eval_u(0.5 * (u_stat + 1.0));
  const double min_end_speed = std::min(e_a.sky_speed, e_b.sky_speed);
  if (!e_left.valid || !e_right.valid || e_left.sky_speed <= 0.1 * min_end_speed ||
      e_right.sky_speed <= 0.1 * min_end_speed || so3::Dot3(e_left.dx, ref) * so3::Dot3(e_right.dx, ref) >= 0.0) {
    return false;
  }
  *u_stat_out = u_stat;
  *e_stat_out = e_stat;
  return true;
}

double GeneratorCrossNorm2D(const double param[3], int ax0, int ax1) {
  const double st = std::sin(param[1]);
  const double ct = std::cos(param[1]);
  const double sr = std::sin(param[2]);
  const double cr = std::cos(param[2]);
  const double w[3][3] = { { st * cr, -st * sr, ct }, { -sr, -cr, 0.0 }, { 0.0, 0.0, 1.0 } };
  double cross[3];
  so3::Cross3(w[ax0], w[ax1], cross);
  return so3::Norm3(cross);
}

bool DominantLeftSingularVector(const double x1[3], const double x2[3], double e_sky[3]) {
  const double g00 = so3::Dot3(x1, x1);
  const double g11 = so3::Dot3(x2, x2);
  const double g01 = so3::Dot3(x1, x2);
  const double lambda1 = 0.5 * (g00 + g11) + std::hypot(0.5 * (g00 - g11), g01);
  if (!(lambda1 > 0.0)) {
    e_sky[0] = e_sky[1] = e_sky[2] = 0.0;
    return false;
  }
  double c = (g00 >= g11) ? (lambda1 - g11) : g01;
  double s = (g00 >= g11) ? g01 : (lambda1 - g00);
  const double vn = std::hypot(c, s);
  c = (vn > 0.0) ? (c / vn) : 1.0;
  s = (vn > 0.0) ? (s / vn) : 0.0;
  for (int i = 0; i < 3; i++) {
    e_sky[i] = c * x1[i] + s * x2[i];
  }
  const double en = so3::Norm3(e_sky);
  if (!(en > 0.0)) {
    e_sky[0] = e_sky[1] = e_sky[2] = 0.0;
    return false;
  }
  for (int i = 0; i < 3; i++) {
    e_sky[i] /= en;
  }
  return true;
}

void AppendEventsWithinArclength(const std::vector<CurveEvent>& src, double max_arclength,
                                 std::vector<CurveEvent>* dst) {
  const double limit = max_arclength + 1e-12;
  for (const CurveEvent& ev : src) {
    if (ev.arclength <= limit) {
      dst->push_back(ev);
    }
  }
}

void ClipEventsToArclength(double max_arclength, std::vector<CurveEvent>* events) {
  const double limit = max_arclength + 1e-12;
  events->erase(
      std::remove_if(events->begin(), events->end(), [limit](const CurveEvent& ev) { return ev.arclength > limit; }),
      events->end());
}

Family1DParams MakeFamily1DParams(const CausticContinuationParams& params) {
  Family1DParams f1d;
  f1d.min_step = params.min_step;
  f1d.max_step = params.max_step;
  f1d.max_turning_angle = params.max_turning_angle;
  f1d.speed_tol = params.speed_tol;
  f1d.bisection_iters = params.bisection_iters;
  f1d.max_nodes_per_branch = params.max_steps_per_branch + 1;
  return f1d;
}

void MergeEdgeSegments(const std::vector<CurveSegmentKind>& edge_kind, const std::vector<bool>& edge_corridor_open,
                       std::vector<CurveSegment>* segments) {
  segments->clear();
  const size_t edge_cnt = edge_kind.size();
  int seg_start = 0;
  for (size_t i = 1; i <= edge_cnt; i++) {
    if (i == edge_cnt || edge_kind[i] != edge_kind[static_cast<size_t>(seg_start)] ||
        edge_corridor_open[i] != edge_corridor_open[static_cast<size_t>(seg_start)]) {
      CurveSegment seg;
      seg.start_idx = seg_start;
      seg.end_idx = static_cast<int>(i);
      seg.kind = edge_kind[static_cast<size_t>(seg_start)];
      seg.corridor_open = edge_corridor_open[static_cast<size_t>(seg_start)];
      segments->push_back(seg);
      seg_start = static_cast<int>(i);
    }
  }
}

void BuildSegments(const std::vector<PoseCurveNode>& nodes, const std::vector<bool>& node_stationary,
                   const std::vector<bool>& edge_corridor_open, bool first_node_corridor_open, double speed_tol,
                   std::vector<CurveSegment>* segments) {
  segments->clear();
  if (nodes.empty()) {
    return;
  }
  if (nodes.size() == 1) {
    CurveSegment seg;
    seg.kind = node_stationary[0] ? CurveSegmentKind::kPointImage : CurveSegmentKind::kArc;
    seg.corridor_open = first_node_corridor_open;
    segments->push_back(seg);
    return;
  }
  const size_t edge_cnt = nodes.size() - 1;
  std::vector<CurveSegmentKind> edge_kind(edge_cnt, CurveSegmentKind::kArc);
  for (size_t i = 0; i < edge_cnt; i++) {
    if (node_stationary[i] && node_stationary[i + 1]) {
      const double ds = nodes[i + 1].arclength - nodes[i].arclength;
      const double max_s1 = std::max(nodes[i].sigma1, nodes[i + 1].sigma1);
      const double sky_bar = std::max(1e-5, 10.0 * speed_tol * std::max(ds, 1e-4) * std::max(max_s1, 1.0));
      if (so3::AngleBetween(nodes[i].sky, nodes[i + 1].sky) <= sky_bar) {
        edge_kind[i] = CurveSegmentKind::kPointImage;
      }
    }
  }
  MergeEdgeSegments(edge_kind, edge_corridor_open, segments);
}

}  // namespace caustic_detail

bool ValidateParams(const Family1DParams& p) {
  if (p.scan_samples < 4 || p.bisection_iters < 1 || p.max_nodes_per_branch < 1) {
    return false;
  }
  if (!std::isfinite(p.min_step) || !std::isfinite(p.max_step) || !std::isfinite(p.max_turning_angle) ||
      !std::isfinite(p.speed_tol) || !std::isfinite(p.stationary_tol)) {
    return false;
  }
  return (p.min_step >= caustic_detail::kMinAllowedStepRad) && (p.max_step >= p.min_step) &&
         (p.max_turning_angle > 0.0) && p.speed_tol >= 0.0 && p.stationary_tol >= 0.0;
}

bool ValidateParams(const CausticContinuationParams& p) {
  if (p.seed_grid_n < 4 || p.max_newton_iters < 1 || p.max_steps_per_branch < 1 || p.max_curves < 1 ||
      p.bisection_iters < 1) {
    return false;
  }
  if (!std::isfinite(p.rank_tol) || !std::isfinite(p.residual_tol) || !std::isfinite(p.grad_tol) ||
      !std::isfinite(p.speed_tol) || !std::isfinite(p.pole_tol) || !std::isfinite(p.initial_step) ||
      !std::isfinite(p.min_step) || !std::isfinite(p.max_step) || !std::isfinite(p.max_turning_angle) ||
      !std::isfinite(p.event_slowdown_margin) || !std::isfinite(p.dedup_tol) || !std::isfinite(p.closure_tol)) {
    return false;
  }
  return p.rank_tol >= 0.0 && (p.residual_tol > 0.0) && (p.grad_tol > 0.0) && p.speed_tol >= 0.0 && p.pole_tol >= 0.0 &&
         (p.min_step >= caustic_detail::kMinAllowedStepRad) && (p.initial_step >= p.min_step) &&
         (p.max_step >= p.initial_step) && (p.max_turning_angle > 0.0) && p.event_slowdown_margin >= 0.0 &&
         p.dedup_tol >= 0.0 && p.closure_tol >= 0.0;
}

}  // namespace lumice::analytic
