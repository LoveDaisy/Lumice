#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "analytic/caustic_internal.hpp"
#include "analytic/so3.hpp"

namespace lumice::analytic {
namespace {

using caustic_detail::AppendEventsWithinArclength;
using caustic_detail::BisectOpticalBoundary1D;
using caustic_detail::CheckStop;
using caustic_detail::Context2D;
using caustic_detail::CopyNodeToEvent;
using caustic_detail::DeltaAxis;
using caustic_detail::DetectStationaryPoint1D;
using caustic_detail::DetectStepTirOnsets;
using caustic_detail::Eval1D;
using caustic_detail::Eval2DJet1;
using caustic_detail::Evaluate1DAlongTangent;
using caustic_detail::Evaluate1DAtPose;
using caustic_detail::Evaluate2DJet1;
using caustic_detail::EvaluateCorridorOpen;
using caustic_detail::FindInteriorOpticalTransition1D;
using caustic_detail::FindStepCorridorCrossings;
using caustic_detail::GoldenSectionProbe;
using caustic_detail::GridCoord;
using caustic_detail::HalfBranch2D;
using caustic_detail::IsSeedCoveredByCurves;
using caustic_detail::MakeFamily1DParams;
using caustic_detail::MarginLip;
using caustic_detail::NodeFromEval1D;
using caustic_detail::NumericalZeroSigmaFloor;
using caustic_detail::ProbeStatus;
using caustic_detail::ProjectOntoCaustic;
using caustic_detail::ProjectOutcome;
using caustic_detail::ProjectResult;
using caustic_detail::Seed2D;
using caustic_detail::StitchCausticCurve;
using caustic_detail::StitchHalfBranches;
using caustic_detail::Trace1DWithEvaluator;
using caustic_detail::TraceHalfBranch2D;
using caustic_detail::WrapAxis;

bool IsSkyCoveredByCurves(const double sky[3], const std::vector<PoseCurve>& curves, double tol_rad) {
  for (const PoseCurve& c : curves) {
    const size_t n = c.nodes.size();
    if (n == 0) {
      continue;
    }
    if (n == 1) {
      if (so3::AngleBetween(sky, c.nodes[0].sky) <= tol_rad) {
        return true;
      }
      continue;
    }
    for (size_t i = 0; i + 1 < n; i++) {
      const double* a = c.nodes[i].sky;
      const double* b = c.nodes[i + 1].sky;
      const double ab[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] };
      const double ap[3] = { sky[0] - a[0], sky[1] - a[1], sky[2] - a[2] };
      const double ab2 = so3::Dot3(ab, ab);
      const double t = (ab2 > 0.0) ? std::clamp(so3::Dot3(ap, ab) / ab2, 0.0, 1.0) : 0.0;
      double closest[3] = { a[0] + t * ab[0], a[1] + t * ab[1], a[2] + t * ab[2] };
      const double cn = so3::Norm3(closest);
      if (cn > 0.0) {
        for (double& v : closest) {
          v /= cn;
        }
        if (so3::AngleBetween(sky, closest) <= tol_rad) {
          return true;
        }
      }
    }
  }
  return false;
}

bool IsVaryCoordCoveredByCurves(double t_val, int vary_ax, const PoseAxis& vax, const std::vector<PoseCurve>& curves) {
  const double t_wrap = WrapAxis(t_val, vax);
  for (const PoseCurve& c : curves) {
    if (c.coverage.closed && vax.periodic) {
      return true;
    }
    const size_t n = c.nodes.size();
    if (n == 1 && std::fabs(DeltaAxis(t_wrap, c.nodes[0].param[vary_ax], vax)) <= 1e-6) {
      return true;
    }
    for (size_t i = 0; i + 1 < n; i++) {
      const double ta = c.nodes[i].param[vary_ax];
      const double seg = DeltaAxis(c.nodes[i + 1].param[vary_ax], ta, vax);
      const double off = DeltaAxis(t_wrap, ta, vax);
      if ((seg >= 0.0) ? (off >= -1e-9 && off <= seg + 1e-9) : (off <= 1e-9 && off >= seg - 1e-9)) {
        return true;
      }
    }
  }
  return false;
}

// Traces continuous pose curves for an axis-aligned rank-1 collapsed 2D family (sky depends on
// `vary_ax`, invariant along `fiber_ax`), sliding `fiber_ax` continuously inside the locally
// connected feasible component without jumping across infeasible fiber gaps.
CausticTraceResult TraceContinuousRank1AxisAligned(const Context2D& ctx, int vary_ax, int fiber_ax,
                                                   const std::vector<Eval2DJet1>& grid, int grid_n, Corridor* corridor,
                                                   const DiscoveryStop* stop) {
  CausticTraceResult out;
  out.generic_rank = 1;
  out.coverage.seed_grid_n = grid_n;

  const CausticContinuationParams& params = ctx.params;
  const PoseAxis& vax = ctx.manifold.axes[vary_ax];
  const PoseAxis& fax = ctx.manifold.axes[fiber_ax];
  const bool track_corridor = (corridor != nullptr);
  const double margin_lip = MarginLip(ctx.refractive_index);
  const double v_span = vax.Span();
  const double f_span = fax.Span();
  const double rank1_stationary_tol = MakeFamily1DParams(params).stationary_tol;

  auto eval_at = [&](double t_vary, double f_val, bool eval_corridor) {
    double param[3] = { ctx.manifold.axes[0].value, ctx.manifold.axes[1].value, ctx.manifold.axes[2].value };
    param[vary_ax] = WrapAxis(t_vary, vax);
    param[fiber_ax] = WrapAxis(f_val, fax);
    return Evaluate1DAtPose(ctx.table, ctx.slots, ctx.slot_count, ctx.refractive_index, ctx.sun_dir, param, vary_ax,
                            corridor, eval_corridor);
  };

  auto find_local_fiber = [&](double t_curr, double f_curr, double t_next, double* f_next_out, Eval1D* e_next_out,
                              int* fail_idx_out) -> bool {
    const Eval1D e0 = eval_at(t_next, f_curr, track_corridor);
    *fail_idx_out = e0.margin.failing_index;
    if (e0.valid && e0.margin.min_effective_margin >= 0.02 && (!track_corridor || e0.corridor_open)) {
      *f_next_out = f_curr;
      *e_next_out = e0;
      return true;
    }

    const double max_slide = std::min(0.2 * f_span, std::max(0.03, 3.0 * std::fabs(t_next - t_curr)));
    constexpr int kProbesPerSide = 10;
    const double df_probe = max_slide / static_cast<double>(kProbesPerSide);

    bool have_best = e0.valid;
    double best_f = f_curr;
    Eval1D best_e = e0;
    double best_score = e0.valid ? e0.margin.min_effective_margin : -1e9;
    bool best_open = e0.valid && e0.corridor_open;

    for (int side : { -1, +1 }) {
      bool found_valid_on_side = e0.valid;
      for (int k = 1; k <= kProbesPerSide; k++) {
        const double f_cand = f_curr + static_cast<double>(side * k) * df_probe;
        if (!fax.periodic && (f_cand < fax.min_val || f_cand > fax.max_val)) {
          break;
        }
        const Eval1D ec = eval_at(t_next, f_cand, track_corridor);
        if (!ec.valid) {
          if (found_valid_on_side) {
            break;
          }
          continue;
        }
        if (!e0.valid && !eval_at(0.5 * (t_curr + t_next), 0.5 * (f_curr + f_cand), false).valid) {
          continue;
        }
        found_valid_on_side = true;
        const bool better = !have_best || (track_corridor && ec.corridor_open != best_open ?
                                               ec.corridor_open :
                                               ec.margin.min_effective_margin > best_score);
        if (better) {
          have_best = true;
          best_f = f_cand;
          best_e = ec;
          best_score = ec.margin.min_effective_margin;
          best_open = ec.corridor_open;
        }
      }
    }
    if (!have_best) {
      return false;
    }

    if (best_score < 0.05) {
      double lo = fax.periodic ? (best_f - df_probe) : std::max(fax.min_val, best_f - df_probe);
      double hi = fax.periodic ? (best_f + df_probe) : std::min(fax.max_val, best_f + df_probe);
      if (hi - lo > 1e-12) {
        auto probe = [&](double fx, double* score_out) -> ProbeStatus {
          *score_out = eval_at(t_next, fx, false).margin.min_effective_margin;
          return ProbeStatus::kValue;
        };
        double f_ref = best_f;
        GoldenSectionProbe(lo, hi, /*maximize=*/true, 10, probe, &f_ref);
        const Eval1D er = eval_at(t_next, f_ref, track_corridor);
        const bool mid_ok = e0.valid || eval_at(0.5 * (t_curr + t_next), 0.5 * (f_curr + f_ref), false).valid;
        if (er.valid && mid_ok && (!track_corridor || er.corridor_open || !best_open) &&
            er.margin.min_effective_margin > best_score) {
          best_f = f_ref;
          best_e = er;
        }
      }
    }
    *f_next_out = best_f;
    *e_next_out = best_e;
    return true;
  };

  auto make_node = [&](const Eval1D& e, double dt_signed, double df_signed, double arclen) {
    PoseCurveNode nd = NodeFromEval1D(e, arclen);
    const double ds = std::hypot(dt_signed, df_signed);
    nd.tangent[0] = nd.tangent[1] = nd.tangent[2] = 0.0;
    nd.sigma1 = e.sky_speed;
    nd.sigma2 = 0.0;
    if (ds > 0.0) {
      nd.tangent[vary_ax] = dt_signed / ds;
      nd.tangent[fiber_ax] = df_signed / ds;
      nd.sky_speed = std::fabs(dt_signed / ds) * e.sky_speed;
    } else {
      nd.tangent[vary_ax] = (dt_signed < 0.0) ? -1.0 : 1.0;
    }
    return nd;
  };

  auto trace_half = [&](double t_seed, double f_seed, int dir_sign, bool allow_closure) -> HalfBranch2D {
    HalfBranch2D half;
    const double sgn = static_cast<double>(dir_sign);
    const double t_limit = vax.periodic ? (t_seed + sgn * v_span) : ((dir_sign > 0) ? vax.max_val : vax.min_val);

    Eval1D e_curr = eval_at(t_seed, f_seed, track_corridor);
    if (!e_curr.valid) {
      return half;
    }
    double t_curr = t_seed;
    double f_curr = f_seed;
    double arclen = 0.0;

    auto push_nd = [&](const Eval1D& e, double dt_s, double df_s, double s_val, bool edge_open) {
      if (!half.nodes.empty()) {
        half.edge_corridor_open.push_back(edge_open);
      }
      const PoseCurveNode nd = make_node(e, dt_s, df_s, s_val);
      half.nodes.push_back(nd);
      half.node_stationary.push_back(nd.sky_speed <= params.speed_tol);
    };

    push_nd(e_curr, sgn, 0.0, 0.0, e_curr.corridor_open);
    half.first_node_corridor_open = e_curr.corridor_open;
    if (!vax.periodic && std::fabs(t_limit - t_curr) <= 1e-13) {
      CurveEvent ev;
      CopyNodeToEvent(half.nodes.back(), CurveEventKind::kDomainBoundary, vary_ax, &ev);
      half.events.push_back(ev);
      return half;
    }

    bool have_last_moving = e_curr.sky_speed > params.speed_tol;
    double last_moving_t = t_curr;
    double last_moving_f = f_curr;
    Eval1D last_moving_e = e_curr;
    double last_moving_arclen = 0.0;

    double h = std::clamp(params.initial_step, params.min_step, params.max_step);
    int steps_taken = 0;

    while (sgn * (t_limit - t_curr) > 1e-13) {
      if (CheckStop(stop)) {
        half.stop_requested = true;
        half.budget_truncated = true;
        return half;
      }
      if (steps_taken >= params.max_steps_per_branch) {
        half.budget_truncated = true;
        return half;
      }

      double step = std::min(h, sgn * (t_limit - t_curr));
      double t_next = t_curr + sgn * step;
      double f_next = f_curr;
      Eval1D e_next;
      int fail_idx = -1;
      bool ok = find_local_fiber(t_curr, f_curr, t_next, &f_next, &e_next, &fail_idx);
      bool shrunk = false;

      while (step > params.min_step * 2.0) {
        if (CheckStop(stop)) {
          half.stop_requested = true;
          half.budget_truncated = true;
          return half;
        }
        bool need_shrink = !ok;
        if (ok) {
          const double sky_step = so3::AngleBetween(e_curr.sky, e_next.sky);
          if (std::fabs(f_next - f_curr) > 0.12 || sky_step > params.max_turning_angle) {
            need_shrink = true;
          } else if (e_curr.sky_speed > params.speed_tol && e_next.sky_speed > params.speed_tol) {
            const double d_angle = so3::AngleBetween(e_curr.dx, e_next.dx);
            need_shrink = (d_angle > params.max_turning_angle && d_angle < 2.5);
          }
        }
        if (!need_shrink) {
          break;
        }
        step = std::max(params.min_step, 0.5 * step);
        t_next = t_curr + sgn * step;
        ok = find_local_fiber(t_curr, f_curr, t_next, &f_next, &e_next, &fail_idx);
        shrunk = true;
      }

      auto commit_chord_step = [&](double t_tgt, double f_tgt, const Eval1D& e_tgt) {
        const double dt_s = t_tgt - t_curr;
        const double df_s = f_tgt - f_curr;
        const double ds = std::hypot(dt_s, df_s);
        if (ds <= 1e-13) {
          return;
        }
        auto seg_eval = [&](double u_par, bool eval_corr) {
          return eval_at(t_curr + u_par * dt_s, f_curr + u_par * df_s, eval_corr);
        };
        if (half.nodes.size() == 1) {
          half.nodes[0].tangent[vary_ax] = dt_s / ds;
          half.nodes[0].tangent[fiber_ax] = df_s / ds;
          half.nodes[0].sky_speed = std::fabs(dt_s / ds) * e_curr.sky_speed;
        }

        std::vector<CurveEvent> step_events;
        if (e_tgt.sky_speed > params.speed_tol) {
          if (have_last_moving) {
            double u_stat = 0.0;
            Eval1D e_stat;
            auto eval_stat_u = [&](double u) {
              return eval_at(last_moving_t + u * (t_tgt - last_moving_t), last_moving_f + u * (f_tgt - last_moving_f),
                             false);
            };
            if (DetectStationaryPoint1D(last_moving_e, e_tgt, rank1_stationary_tol, params.bisection_iters, eval_stat_u,
                                        &u_stat, &e_stat)) {
              const double s_stat =
                  last_moving_arclen + u_stat * std::hypot(t_tgt - last_moving_t, f_tgt - last_moving_f);
              CurveEvent ev;
              CopyNodeToEvent(make_node(e_stat, dt_s, df_s, s_stat), CurveEventKind::kStationaryPoint, -1, &ev);
              step_events.push_back(ev);
            }
          }
          have_last_moving = true;
          last_moving_t = t_tgt;
          last_moving_f = f_tgt;
          last_moving_e = e_tgt;
          last_moving_arclen = arclen + ds;
        }

        auto eval_tir_u = [&](double u, int k, double* disc_out) {
          const Eval1D em = seg_eval(u, false);
          if (!em.valid) {
            return false;
          }
          *disc_out = em.internal_disc[k];
          return true;
        };
        auto emit_tir_u = [&](double u_tir, int k) {
          const Eval1D e_tir = seg_eval(u_tir, false);
          if (e_tir.valid) {
            CurveEvent ev;
            CopyNodeToEvent(make_node(e_tir, dt_s, df_s, arclen + u_tir * ds), CurveEventKind::kTirOnset, k, &ev);
            step_events.push_back(ev);
          }
        };
        DetectStepTirOnsets(ctx.slot_count, e_curr.internal_disc, e_tgt.internal_disc, ds, margin_lip,
                            params.bisection_iters, eval_tir_u, emit_tir_u);

        int cut_cnt = 0;
        double u_cuts[2] = { 0.0, 0.0 };
        if (track_corridor) {
          auto eval_corr_u = [&](double u, bool /*need_margin*/, bool* open_out, double* margin_out) {
            const Eval1D em = seg_eval(u, true);
            if (!em.valid) {
              return false;
            }
            *open_out = em.corridor_open;
            *margin_out = em.corridor_margin;
            return true;
          };
          cut_cnt = FindStepCorridorCrossings(e_curr.corridor_open, e_curr.corridor_margin, e_tgt.corridor_open,
                                              e_tgt.corridor_margin, ds, params.bisection_iters, eval_corr_u, u_cuts);
        }

        double u_prev_p = 0.0;
        bool cur_open = e_curr.corridor_open;
        for (int c = 0; c < cut_cnt; c++) {
          const Eval1D e_cut = seg_eval(u_cuts[c], true);
          if (!e_cut.valid) {
            break;
          }
          const double ds_c = (u_cuts[c] - u_prev_p) * ds;
          if (ds_c > 1e-13) {
            arclen += ds_c;
            push_nd(e_cut, dt_s, df_s, arclen, cur_open);
            u_prev_p = u_cuts[c];
          }
          CurveEvent ev;
          CopyNodeToEvent(half.nodes.back(), CurveEventKind::kCorridorCutoff, -1, &ev);
          half.events.push_back(ev);
          cur_open = !cur_open;
        }
        const double ds_rem = (1.0 - u_prev_p) * ds;
        if (ds_rem > 1e-13) {
          arclen += ds_rem;
          push_nd(e_tgt, dt_s, df_s, arclen, cur_open);
        }
        AppendEventsWithinArclength(step_events, half.nodes.back().arclength, &half.events);
        t_curr = t_tgt;
        f_curr = f_tgt;
        e_curr = e_tgt;
      };

      auto finish_optical_boundary = [&](double t_v, double f_v, Eval1D e_bnd, int bnd_fail) {
        if (track_corridor) {
          const Eval1D e_bnd_c = eval_at(t_v, f_v, true);
          if (e_bnd_c.valid) {
            e_bnd = e_bnd_c;
          }
        }
        commit_chord_step(t_v, f_v, e_bnd);
        CurveEvent ev;
        CopyNodeToEvent(half.nodes.back(), CurveEventKind::kOpticalBoundary, bnd_fail, &ev);
        half.events.push_back(ev);
      };

      if (!ok) {
        double t_v = t_curr;
        double f_v = f_curr;
        double t_i = t_next;
        Eval1D e_bnd = e_curr;
        int bnd_fail = fail_idx;
        for (int it = 0; it < params.bisection_iters; it++) {
          const double t_m = 0.5 * (t_v + t_i);
          double f_m = f_v;
          Eval1D e_m;
          int f_fail = -1;
          if (find_local_fiber(t_v, f_v, t_m, &f_m, &e_m, &f_fail)) {
            t_v = t_m;
            f_v = f_m;
            e_bnd = e_m;
          } else {
            t_i = t_m;
            if (f_fail >= 0) {
              bnd_fail = f_fail;
            }
          }
        }
        finish_optical_boundary(t_v, f_v, e_bnd, bnd_fail);
        return half;
      }

      auto chord_eval = [&](double u_par, bool eval_corr) {
        return eval_at(t_curr + u_par * (t_next - t_curr), f_curr + u_par * (f_next - f_curr), eval_corr);
      };
      const double ds_chord = std::hypot(t_next - t_curr, f_next - f_curr);
      double u_inv = 1.0;
      if (FindInteriorOpticalTransition1D(chord_eval, 0.0, e_curr.margin.min_effective_margin, 1.0,
                                          e_next.margin.min_effective_margin, margin_lip * ds_chord,
                                          /*seek_valid=*/false, &u_inv)) {
        double u_bnd = 0.0;
        int bnd_fail = -1;
        const Eval1D e_bnd = BisectOpticalBoundary1D(chord_eval, 0.0, u_inv, params.bisection_iters, &u_bnd, &bnd_fail);
        finish_optical_boundary(t_curr + u_bnd * (t_next - t_curr), f_curr + u_bnd * (f_next - f_curr), e_bnd,
                                bnd_fail);
        return half;
      }

      commit_chord_step(t_next, f_next, e_next);
      steps_taken++;
      h = shrunk ? std::max(params.min_step, step) : std::min(params.max_step, step * 1.25);
    }

    if (!vax.periodic) {
      CurveEvent ev;
      CopyNodeToEvent(half.nodes.back(), CurveEventKind::kDomainBoundary, vary_ax, &ev);
      half.events.push_back(ev);
    } else if (allow_closure) {
      const double df_close = DeltaAxis(f_seed, f_curr, fax);
      if (std::fabs(df_close) <= 1e-9) {
        half.closed = true;
      } else {
        const int n_close = std::max(1, static_cast<int>(std::ceil(std::fabs(df_close) / 0.05)));
        std::vector<Eval1D> close_evals;
        close_evals.reserve(static_cast<size_t>(n_close));
        for (int k = 1; k <= n_close; k++) {
          const Eval1D ek = eval_at(t_seed, f_curr + df_close * static_cast<double>(k) / static_cast<double>(n_close),
                                    track_corridor);
          if (!ek.valid) {
            close_evals.clear();
            break;
          }
          close_evals.push_back(ek);
        }
        if (!close_evals.empty()) {
          const double df_step = df_close / static_cast<double>(n_close);
          for (const Eval1D& ek : close_evals) {
            arclen += std::fabs(df_step);
            push_nd(ek, 0.0, df_step, arclen, e_curr.corridor_open);
            e_curr = ek;
          }
          half.closed = true;
        }
      }
    }
    return half;
  };

  struct CellSeed {
    int i = 0;
    int j = 0;
    double margin = 0.0;
  };
  std::vector<CellSeed> valid_cells;
  valid_cells.reserve(grid.size());
  for (int i = 0; i < grid_n; i++) {
    for (int j = 0; j < grid_n; j++) {
      const Eval2DJet1& cell = grid[static_cast<size_t>(i * grid_n + j)];
      if (cell.valid) {
        valid_cells.push_back({ i, j, cell.margin.min_effective_margin });
      }
    }
  }
  std::sort(valid_cells.begin(), valid_cells.end(),
            [](const CellSeed& a, const CellSeed& b) { return a.margin > b.margin; });

  for (const CellSeed& cs : valid_cells) {
    if (CheckStop(stop)) {
      out.stop_requested = true;
      out.coverage.budget_truncated = true;
      break;
    }
    const double q0 = GridCoord(cs.i, grid_n, ctx.manifold.axes[ctx.ax0]);
    const double q1 = GridCoord(cs.j, grid_n, ctx.manifold.axes[ctx.ax1]);
    const double t_seed = (vary_ax == ctx.ax0) ? q0 : q1;
    const double f_seed = (fiber_ax == ctx.ax0) ? q0 : q1;
    const Eval2DJet1& cell = grid[static_cast<size_t>(cs.i * grid_n + cs.j)];
    if (IsVaryCoordCoveredByCurves(t_seed, vary_ax, vax, out.curves) ||
        IsSkyCoveredByCurves(cell.sky, out.curves, 0.015)) {
      continue;
    }
    if (static_cast<int>(out.curves.size()) >= params.max_curves) {
      out.coverage.budget_truncated = true;
      break;
    }

    const HalfBranch2D forward = trace_half(t_seed, f_seed, +1, true);
    HalfBranch2D backward;
    if (!forward.closed && !forward.stop_requested) {
      backward = trace_half(t_seed, f_seed, -1, false);
    }
    out.stop_requested = out.stop_requested || forward.stop_requested || backward.stop_requested;
    PoseCurve c = StitchHalfBranches(forward, backward, PoseCurveKind::kCollapsed2D, grid_n, params.speed_tol, 0);
    if (!c.nodes.empty()) {
      out.coverage.closed = out.coverage.closed || c.coverage.closed;
      out.coverage.budget_truncated = out.coverage.budget_truncated || c.coverage.budget_truncated;
      out.curves.push_back(std::move(c));
    }
  }
  return out;
}

}  // namespace

CausticTraceResult TraceCausticLocus(const FaceNormalTable& table, const int* slots, int slot_count,
                                     double refractive_index, const double sun_dir[3], const PoseManifold& manifold,
                                     Corridor* corridor, const CausticContinuationParams& params,
                                     const DiscoveryStop* stop) {
  CausticTraceResult result;
  result.coverage.seed_grid_n = params.seed_grid_n;
  if (!ValidateParams(params) || !manifold.IsValid() || manifold.Dimension() != 2 || slots == nullptr ||
      slot_count < 1 || slot_count > kMaxFaceCount || !std::isfinite(refractive_index) || refractive_index <= 0.0 ||
      !ValidateUnitVector(sun_dir)) {
    return result;
  }

  int free_axes[3]{};
  manifold.FreeAxes(free_axes);
  const int ax0 = free_axes[0];
  const int ax1 = free_axes[1];
  const Context2D ctx{ table, slots, slot_count, refractive_index, sun_dir, manifold, ax0, ax1, params };
  const PoseAxis& pax0 = manifold.axes[ax0];
  const PoseAxis& pax1 = manifold.axes[ax1];
  const int n = params.seed_grid_n;
  const double step0 = pax0.periodic ? pax0.Span() / static_cast<double>(n) : pax0.Span() / static_cast<double>(n - 1);
  const double step1 = pax1.periodic ? pax1.Span() / static_cast<double>(n) : pax1.Span() / static_cast<double>(n - 1);
  const double cell_diag = std::hypot(step0, step1);

  // Stage 1: Coarse Jet<2> pre-scan.
  std::vector<Eval2DJet1> grid(static_cast<size_t>(n * n));
  int valid_count = 0;
  bool any_full_generator_rank = false;
  bool any_rank2 = false;
  bool any_rank1 = false;
  double max_x1_norm = 0.0;
  double max_x2_norm = 0.0;
  int best_valid_i = -1;
  int best_valid_j = -1;
  double best_valid_margin = -1.0;

  for (int i = 0; i < n; i++) {
    if (CheckStop(stop)) {
      result.stop_requested = true;
      result.coverage.budget_truncated = true;
      return result;
    }
    const double q0 = GridCoord(i, n, pax0);
    for (int j = 0; j < n; j++) {
      const double q[2] = { q0, GridCoord(j, n, pax1) };
      Eval2DJet1& cell = grid[static_cast<size_t>(i * n + j)];
      cell = Evaluate2DJet1(ctx, q);
      if (!cell.valid) {
        continue;
      }
      valid_count++;
      if (cell.margin.min_effective_margin > best_valid_margin) {
        best_valid_margin = cell.margin.min_effective_margin;
        best_valid_i = i;
        best_valid_j = j;
      }
      max_x1_norm = std::max(max_x1_norm, so3::Norm3(cell.x1));
      max_x2_norm = std::max(max_x2_norm, so3::Norm3(cell.x2));
      const bool full_gen = cell.generator_cross >= params.pole_tol;
      any_full_generator_rank = any_full_generator_rank || full_gen;
      if (cell.sigma1 > NumericalZeroSigmaFloor(cell.margin.min_effective_margin)) {
        any_rank1 = true;
        if (full_gen && cell.sigma2 > params.rank_tol * cell.sigma1) {
          any_rank2 = true;
        }
      }
    }
  }

  if (valid_count == 0) {
    result.generic_rank = -1;
    return result;
  }

  auto build_rank0_curve = [&](bool add_pole_event) {
    const Eval2DJet1& rep = grid[static_cast<size_t>(best_valid_i * n + best_valid_j)];
    PoseCurve curve;
    curve.kind = PoseCurveKind::kCollapsed2D;
    curve.coverage.seed_grid_n = n;
    PoseCurveNode node;
    so3::Copy3(rep.param, node.param);
    so3::Copy3(rep.sky, node.sky);
    std::copy_n(rep.pose, 9, node.pose);
    WorldToBody(node.pose, sun_dir, node.u);
    node.sigma1 = rep.sigma1;
    node.sigma2 = rep.sigma2;
    curve.nodes.push_back(node);
    CurveSegment seg;
    seg.kind = CurveSegmentKind::kPointImage;
    seg.corridor_open = EvaluateCorridorOpen(corridor, node.pose, sun_dir, refractive_index);
    curve.segments.push_back(seg);
    if (add_pole_event) {
      CurveEvent ev;
      CopyNodeToEvent(node, CurveEventKind::kDomainBoundary, PoseManifold::kAxisZenith, &ev);
      curve.events.push_back(ev);
    }
    return curve;
  };

  auto append_collapsed_curves = [&](CausticTraceResult& sub, bool add_pole_event, bool recompute_2d_svd) {
    result.stop_requested = result.stop_requested || sub.stop_requested;
    result.coverage.budget_truncated = result.coverage.budget_truncated || sub.coverage.budget_truncated;
    for (PoseCurve& c : sub.curves) {
      if (static_cast<int>(result.curves.size()) >= params.max_curves) {
        result.coverage.budget_truncated = true;
        break;
      }
      c.kind = PoseCurveKind::kCollapsed2D;
      c.coverage.seed_grid_n = n;
      if (recompute_2d_svd) {
        for (PoseCurveNode& nd : c.nodes) {
          const double q_nd[2] = { nd.param[ax0], nd.param[ax1] };
          const Eval2DJet1 e2 = Evaluate2DJet1(ctx, q_nd);
          if (e2.valid) {
            nd.sigma1 = e2.sigma1;
            nd.sigma2 = e2.sigma2;
          }
        }
      }
      if (add_pole_event && !c.nodes.empty()) {
        CurveEvent pole_ev;
        CopyNodeToEvent(c.nodes.front(), CurveEventKind::kDomainBoundary, PoseManifold::kAxisZenith, &pole_ev);
        c.events.insert(c.events.begin(), pole_ev);
      }
      result.coverage.closed = result.coverage.closed || c.coverage.closed;
      result.coverage.budget_truncated = result.coverage.budget_truncated || c.coverage.budget_truncated;
      result.curves.push_back(std::move(c));
    }
  };

  if (!any_full_generator_rank) {
    result.generic_rank = any_rank1 ? 1 : 0;
    if (!any_rank1) {
      result.curves.push_back(build_rank0_curve(true));
      return result;
    }
    const Family1DParams f1d = MakeFamily1DParams(params);
    CausticTraceResult collapsed;
    const bool is_az_roll_pole = (ax0 == PoseManifold::kAxisAzimuth && ax1 == PoseManifold::kAxisRoll);
    if (is_az_roll_pole && !pax0.periodic && !pax1.periodic) {
      const double z_val = grid[static_cast<size_t>(best_valid_i * n + best_valid_j)].param[PoseManifold::kAxisZenith];
      const double pole_sign = (std::cos(z_val) >= 0.0) ? 1.0 : -1.0;
      const double total_spin = pax0.Span() + pax1.Span();
      const double d_az = pax0.Span() / total_spin;
      const double d_roll = pole_sign * (pax1.Span() / total_spin);
      const double ds_dphi = std::hypot(d_az, d_roll);
      const double t_vec[3] = { d_az / ds_dphi, 0.0, d_roll / ds_dphi };
      const double roll_start = (pole_sign > 0.0) ? pax1.min_val : pax1.max_val;
      auto pole_eval = [&](double s_arc, bool eval_corridor) {
        double param[3] = { manifold.axes[0].value, z_val, manifold.axes[2].value };
        param[PoseManifold::kAxisAzimuth] = std::clamp(pax0.min_val + s_arc * t_vec[0], pax0.min_val, pax0.max_val);
        param[PoseManifold::kAxisRoll] = std::clamp(roll_start + s_arc * t_vec[2], pax1.min_val, pax1.max_val);
        return Evaluate1DAlongTangent(table, slots, slot_count, refractive_index, sun_dir, param, t_vec, corridor,
                                      eval_corridor);
      };
      collapsed =
          Trace1DWithEvaluator(PoseAxis::FreeRad(0.0, total_spin * ds_dphi, false), PoseManifold::kAxisAzimuth,
                               slot_count, MarginLip(refractive_index), pole_eval, corridor != nullptr, f1d, stop);
    } else {
      PoseManifold slice = manifold;
      const bool keep_ax0 =
          (is_az_roll_pole && (pax0.periodic || pax1.periodic)) ? pax0.periodic : (max_x1_norm >= max_x2_norm);
      slice.axes[keep_ax0 ? ax1 : ax0] =
          PoseAxis::PinnedRad(keep_ax0 ? GridCoord(best_valid_j, n, pax1) : GridCoord(best_valid_i, n, pax0));
      collapsed = TraceFamily1D(table, slots, slot_count, refractive_index, sun_dir, slice, corridor, f1d, stop);
    }
    append_collapsed_curves(collapsed, /*add_pole_event=*/true, /*recompute_2d_svd=*/true);
    return result;
  }

  if (!any_rank2) {
    if (!any_rank1) {
      result.generic_rank = 0;
      result.curves.push_back(build_rank0_curve(false));
      return result;
    }
    result.generic_rank = 1;
    const bool sky_only_ax0 = (max_x2_norm <= params.rank_tol * max_x1_norm);
    const bool sky_only_ax1 = (max_x1_norm <= params.rank_tol * max_x2_norm);
    if (sky_only_ax0 || sky_only_ax1) {
      CausticTraceResult projected = TraceContinuousRank1AxisAligned(ctx, sky_only_ax0 ? ax0 : ax1,
                                                                     sky_only_ax0 ? ax1 : ax0, grid, n, corridor, stop);
      append_collapsed_curves(projected, /*add_pole_event=*/false, /*recompute_2d_svd=*/false);
      return result;
    }

    const Family1DParams f1d = MakeFamily1DParams(params);
    const bool keep_ax0 = (max_x1_norm >= max_x2_norm);
    const int pin_ax = keep_ax0 ? ax1 : ax0;
    auto trace_slice_at = [&](double pin_val) {
      PoseManifold slice = manifold;
      slice.axes[pin_ax] = PoseAxis::PinnedRad(pin_val);
      CausticTraceResult sub =
          TraceFamily1D(table, slots, slot_count, refractive_index, sun_dir, slice, corridor, f1d, stop);
      append_collapsed_curves(sub, /*add_pole_event=*/false, /*recompute_2d_svd=*/true);
    };
    trace_slice_at(keep_ax0 ? GridCoord(best_valid_j, n, pax1) : GridCoord(best_valid_i, n, pax0));
    for (int i = 0; i < n && !result.stop_requested; i++) {
      for (int j = 0; j < n && !result.stop_requested; j++) {
        const Eval2DJet1& cell = grid[static_cast<size_t>(i * n + j)];
        if (!cell.valid || IsSkyCoveredByCurves(cell.sky, result.curves, 0.02)) {
          continue;
        }
        if (static_cast<int>(result.curves.size()) >= params.max_curves) {
          result.coverage.budget_truncated = true;
          return result;
        }
        trace_slice_at(keep_ax0 ? GridCoord(j, n, pax1) : GridCoord(i, n, pax0));
      }
    }
    return result;
  }

  result.generic_rank = 2;

  // Stage 2: Harvest J_reg = 0 seeds from sign-change edges, optical cliffs, and near-zero minima.
  std::vector<Seed2D> seeds;
  int failed_seeds = 0;

  auto try_add_seed = [&](const double q_guess[2], double drift_cap) {
    const ProjectResult pr = ProjectOntoCaustic(ctx, q_guess, drift_cap);
    if (pr.outcome == ProjectOutcome::kConverged && pr.eval.grad_reg_norm >= params.grad_tol) {
      Seed2D s;
      s.q[0] = WrapAxis(pr.q[0], pax0);
      s.q[1] = WrapAxis(pr.q[1], pax1);
      s.eval = pr.eval;
      s.priority = std::min(pr.eval.margin.min_effective_margin, 0.1) * std::min(pr.eval.grad_norm, 10.0);
      seeds.push_back(s);
    } else if (pr.outcome == ProjectOutcome::kSingularity || pr.outcome == ProjectOutcome::kDiverged) {
      failed_seeds++;
    }
  };

  auto bisect_edge_sign_change = [&](const double qa_in[2], double ja, const double qb_in[2]) {
    double qa[2] = { qa_in[0], qa_in[1] };
    double qb[2] = { qb_in[0], qb_in[1] };
    double fa = ja;
    for (int it = 0; it < params.bisection_iters; it++) {
      const double qm[2] = { 0.5 * (qa[0] + qb[0]), 0.5 * (qa[1] + qb[1]) };
      const Eval2DJet1 em = Evaluate2DJet1(ctx, qm);
      if (!em.valid) {
        return;
      }
      if (std::fabs(em.j_val) <= params.residual_tol) {
        qa[0] = qb[0] = qm[0];
        qa[1] = qb[1] = qm[1];
        break;
      }
      if ((fa <= 0.0) == (em.j_val <= 0.0)) {
        qa[0] = qm[0];
        qa[1] = qm[1];
        fa = em.j_val;
      } else {
        qb[0] = qm[0];
        qb[1] = qm[1];
      }
    }
    const double q_mid[2] = { 0.5 * (qa[0] + qb[0]), 0.5 * (qa[1] + qb[1]) };
    try_add_seed(q_mid, 0.5 * cell_diag);
  };

  auto probe_cliff_edge = [&](const double q_valid[2], const Eval2DJet1& ev_valid, const double q_invalid[2]) {
    double qv[2] = { q_valid[0], q_valid[1] };
    double qi[2] = { q_invalid[0], q_invalid[1] };
    Eval2DJet1 e_cliff = ev_valid;
    for (int it = 0; it < 28; it++) {
      const double qm[2] = { 0.5 * (qv[0] + qi[0]), 0.5 * (qv[1] + qi[1]) };
      const Eval2DJet1 em = Evaluate2DJet1(ctx, qm);
      if (em.valid) {
        qv[0] = qm[0];
        qv[1] = qm[1];
        e_cliff = em;
      } else {
        qi[0] = qm[0];
        qi[1] = qm[1];
      }
    }
    if (ev_valid.j_val * e_cliff.j_val <= 0.0) {
      bisect_edge_sign_change(q_valid, ev_valid.j_val, qv);
    } else if (std::fabs(e_cliff.j_val) < std::fabs(ev_valid.j_val) &&
               std::fabs(e_cliff.j_val) < 0.05 * e_cliff.sigma1 * e_cliff.sigma1) {
      try_add_seed(qv, 0.5 * cell_diag);
    }
  };

  auto process_edge_evals = [&](const double qa[2], const Eval2DJet1& a, const double qb[2], const Eval2DJet1& b) {
    const size_t before = seeds.size();
    if (a.valid && b.valid) {
      if (a.j_val * b.j_val <= 0.0) {
        bisect_edge_sign_change(qa, a.j_val, qb);
      }
    } else if (a.valid && !b.valid) {
      probe_cliff_edge(qa, a, qb);
    } else if (!a.valid && b.valid) {
      probe_cliff_edge(qb, b, qa);
    }
    return seeds.size() > before;
  };

  const int edges0 = pax0.periodic ? n : (n - 1);
  const int edges1 = pax1.periodic ? n : (n - 1);
  std::vector<bool> cell_seeded(static_cast<size_t>(n * n), false);
  for (int i = 0; i < n; i++) {
    if (CheckStop(stop)) {
      result.stop_requested = true;
      result.coverage.budget_truncated = true;
      return result;
    }
    for (int j = 0; j < n; j++) {
      const double qa[2] = { GridCoord(i, n, pax0), GridCoord(j, n, pax1) };
      const Eval2DJet1& a = grid[static_cast<size_t>(i * n + j)];
      if (i < edges0) {
        const double qb[2] = { qa[0] + step0, qa[1] };
        if (process_edge_evals(qa, a, qb, grid[static_cast<size_t>(((i + 1) % n) * n + j)])) {
          cell_seeded[static_cast<size_t>(i * n + j)] = true;
          if (j > 0 || pax1.periodic) {
            cell_seeded[static_cast<size_t>(i * n + ((j + n - 1) % n))] = true;
          }
        }
      }
      if (j < edges1) {
        const double qb[2] = { qa[0], qa[1] + step1 };
        if (process_edge_evals(qa, a, qb, grid[static_cast<size_t>(i * n + ((j + 1) % n))])) {
          cell_seeded[static_cast<size_t>(i * n + j)] = true;
          if (i > 0 || pax0.periodic) {
            cell_seeded[static_cast<size_t>(((i + n - 1) % n) * n + j)] = true;
          }
        }
      }
    }
  }

  auto refine_mixed_cell = [&](auto& self, double q0, double q1, double d0, double d1, const Eval2DJet1& c00,
                               const Eval2DJet1& c10, const Eval2DJet1& c01, const Eval2DJet1& c11, int depth) -> void {
    const double h0 = 0.5 * d0;
    const double h1 = 0.5 * d1;
    Eval2DJet1 v[3][3];
    v[0][0] = c00;
    v[2][0] = c10;
    v[0][2] = c01;
    v[2][2] = c11;
    for (int a = 0; a < 3; a++) {
      for (int b = 0; b < 3; b++) {
        if ((a == 0 || a == 2) && (b == 0 || b == 2)) {
          continue;
        }
        const double qm[2] = { q0 + static_cast<double>(a) * h0, q1 + static_cast<double>(b) * h1 };
        v[a][b] = Evaluate2DJet1(ctx, qm);
      }
    }
    bool found = false;
    for (int a = 0; a < 3; a++) {
      for (int b = 0; b < 3; b++) {
        const double qa[2] = { q0 + static_cast<double>(a) * h0, q1 + static_cast<double>(b) * h1 };
        if (a < 2) {
          const double qb[2] = { qa[0] + h0, qa[1] };
          found = process_edge_evals(qa, v[a][b], qb, v[a + 1][b]) || found;
        }
        if (b < 2) {
          const double qb[2] = { qa[0], qa[1] + h1 };
          found = process_edge_evals(qa, v[a][b], qb, v[a][b + 1]) || found;
        }
      }
    }
    if (found || depth + 1 >= 2) {
      return;
    }
    for (int a = 0; a < 2; a++) {
      for (int b = 0; b < 2; b++) {
        const int vc = v[a][b].valid + v[a + 1][b].valid + v[a][b + 1].valid + v[a + 1][b + 1].valid;
        if (vc > 0 && vc < 4) {
          self(self, q0 + static_cast<double>(a) * h0, q1 + static_cast<double>(b) * h1, h0, h1, v[a][b], v[a + 1][b],
               v[a][b + 1], v[a + 1][b + 1], depth + 1);
        }
      }
    }
  };

  for (int i = 0; i < edges0; i++) {
    if (CheckStop(stop)) {
      result.stop_requested = true;
      result.coverage.budget_truncated = true;
      return result;
    }
    const int i1 = (i + 1) % n;
    const double q0 = GridCoord(i, n, pax0);
    for (int j = 0; j < edges1; j++) {
      if (cell_seeded[static_cast<size_t>(i * n + j)]) {
        continue;
      }
      const int j1 = (j + 1) % n;
      const Eval2DJet1& c00 = grid[static_cast<size_t>(i * n + j)];
      const Eval2DJet1& c10 = grid[static_cast<size_t>(i1 * n + j)];
      const Eval2DJet1& c01 = grid[static_cast<size_t>(i * n + j1)];
      const Eval2DJet1& c11 = grid[static_cast<size_t>(i1 * n + j1)];
      const int vc = c00.valid + c10.valid + c01.valid + c11.valid;
      if (vc > 0 && vc < 4) {
        refine_mixed_cell(refine_mixed_cell, q0, GridCoord(j, n, pax1), step0, step1, c00, c10, c01, c11, 0);
      }
    }
  }

  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      const Eval2DJet1& c = grid[static_cast<size_t>(i * n + j)];
      if (!c.valid || c.sigma1 <= NumericalZeroSigmaFloor(c.margin.min_effective_margin) ||
          c.sigma2 >= 0.15 * c.sigma1) {
        continue;
      }
      const double abs_j = std::fabs(c.j_val);
      bool is_min = true;
      const int di[4] = { -1, 1, 0, 0 };
      const int dj[4] = { 0, 0, -1, 1 };
      for (int dir = 0; dir < 4; dir++) {
        int ni = i + di[dir];
        int nj = j + dj[dir];
        if ((ni < 0 || ni >= n) && !pax0.periodic) {
          continue;
        }
        if ((nj < 0 || nj >= n) && !pax1.periodic) {
          continue;
        }
        ni = (ni + n) % n;
        nj = (nj + n) % n;
        const Eval2DJet1& nb = grid[static_cast<size_t>(ni * n + nj)];
        if (nb.valid && std::fabs(nb.j_val) < abs_j) {
          is_min = false;
          break;
        }
      }
      if (is_min) {
        const double q[2] = { GridCoord(i, n, pax0), GridCoord(j, n, pax1) };
        try_add_seed(q, 0.75 * cell_diag);
      }
    }
  }

  std::sort(seeds.begin(), seeds.end(), [](const Seed2D& a, const Seed2D& b) { return a.priority > b.priority; });
  result.coverage.failed_seeds = failed_seeds;

  // Stage 3: Bidirectional predictor-corrector continuation from each uncovered seed.
  for (const Seed2D& seed : seeds) {
    if (CheckStop(stop)) {
      result.stop_requested = true;
      result.coverage.budget_truncated = true;
      break;
    }
    if (IsSeedCoveredByCurves(ctx, seed, result.curves)) {
      continue;
    }
    if (static_cast<int>(result.curves.size()) >= params.max_curves) {
      result.coverage.budget_truncated = true;
      break;
    }

    const HalfBranch2D forward = TraceHalfBranch2D(ctx, seed.q, seed.eval, +1, true, corridor, stop);
    HalfBranch2D backward;
    if (!forward.closed && !forward.stop_requested) {
      backward = TraceHalfBranch2D(ctx, seed.q, seed.eval, -1, false, corridor, stop);
    }

    PoseCurve curve = StitchCausticCurve(forward, backward, n, params.speed_tol);
    result.stop_requested = result.stop_requested || forward.stop_requested || backward.stop_requested;
    result.coverage.closed = result.coverage.closed || curve.coverage.closed;
    result.coverage.budget_truncated = result.coverage.budget_truncated || curve.coverage.budget_truncated;
    result.coverage.unresolved_singularities += curve.coverage.unresolved_singularities;
    if (!curve.nodes.empty()) {
      result.curves.push_back(std::move(curve));
    }
  }
  return result;
}

}  // namespace lumice::analytic
