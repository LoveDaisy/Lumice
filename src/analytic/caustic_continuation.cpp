#include "analytic/caustic_continuation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "analytic/jet.hpp"
#include "analytic/jet2.hpp"
#include "analytic/path_chain.hpp"
#include "analytic/path_fiber.hpp"
#include "analytic/so3.hpp"
#include "core/math.hpp"

namespace lumice::analytic {
namespace {

bool CheckStop(const DiscoveryStop* stop) {
  if (stop == nullptr) {
    return false;
  }
  if (stop->token != nullptr) {
    stop->token->NotePoll(kPollFamilyCurve);
  }
  return stop->Stopped();
}

double WrapAxis(double v, const PoseAxis& ax) {
  if (!ax.periodic) {
    return v;
  }
  const double span = ax.Span();
  double r = std::fmod(v - ax.min_val, span);
  if (r < 0.0) {
    r += span;
  }
  return ax.min_val + r;
}

double DeltaAxis(double a, double b, const PoseAxis& ax) {
  double d = a - b;
  if (ax.periodic) {
    const double span = ax.Span();
    d = std::fmod(d + 0.5 * span, span);
    if (d < 0.0) {
      d += span;
    }
    d -= 0.5 * span;
  }
  return d;
}

bool InAxisBounds(double v, const PoseAxis& ax, double eps = 0.0) {
  if (ax.periodic) {
    return true;
  }
  return v >= ax.min_val - eps && v <= ax.max_val + eps;
}

double Dist2D(const double a[2], const double b[2], const PoseAxis& ax0, const PoseAxis& ax1) {
  return std::hypot(DeltaAxis(a[0], b[0], ax0), DeltaAxis(a[1], b[1], ax1));
}

struct OpticalMarginInfo {
  bool valid = false;
  double min_eff_margin = -1.0;
  int failing_index = -1;
  int margin_count = 0;
  double margins[kMaxDomainMargins]{};
};

// Classifies optical validity with kSnellEventTolerance on entry/exit Snell discriminants so
// second-order Jet2 sqrt derivatives stay well-conditioned up to the optical boundary.
OpticalMarginInfo ClassifyMargins(bool chain_ok, const ChainDomain& domain, int slot_count) {
  OpticalMarginInfo info;
  info.margin_count = domain.margin_count;
  for (int i = 0; i < domain.margin_count; i++) {
    info.margins[i] = domain.margins[i];
  }
  if (!chain_ok) {
    info.valid = false;
    info.failing_index = domain.margin_count > 0 ? domain.margin_count - 1 : -1;
    info.min_eff_margin = domain.failure_margin;
    for (int i = 0; i < domain.margin_count; i++) {
      if (!(domain.margins[i] > 0.0)) {
        info.failing_index = i;
        info.min_eff_margin = domain.margins[i];
        break;
      }
    }
    return info;
  }

  double min_eff = std::numeric_limits<double>::infinity();
  int min_idx = -1;
  for (int i = 0; i < domain.margin_count; i++) {
    double eff = domain.margins[i];
    if (slot_count >= 2 && (i == 1 || i == domain.margin_count - 1)) {
      eff -= kSnellEventTolerance;
    }
    if (!std::isfinite(eff)) {
      info.valid = false;
      info.failing_index = i;
      info.min_eff_margin = eff;
      return info;
    }
    if (eff < min_eff) {
      min_eff = eff;
      min_idx = i;
    }
  }
  info.min_eff_margin = min_eff;
  if (min_eff <= 0.0) {
    info.valid = false;
    info.failing_index = min_idx;
  } else {
    info.valid = true;
    info.failing_index = -1;
  }
  return info;
}

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

// Computes ||omega_{ax0} x omega_{ax1}|| for the two right-trivialized body angular velocity
// generators of the Euler chart R(az, zenith, roll) = Rz(az - pi) Ry(-zenith) Rz(roll).
double GeneratorCrossNorm2D(const double param[3], int ax0, int ax1) {
  const double st = std::sin(param[1]);
  const double ct = std::cos(param[1]);
  const double sr = std::sin(param[2]);
  const double cr = std::cos(param[2]);
  const double w[3][3] = {
    { st * cr, -st * sr, ct },
    { -sr, -cr, 0.0 },
    { 0.0, 0.0, 1.0 },
  };
  double cross[3];
  so3::Cross3(w[ax0], w[ax1], cross);
  return so3::Norm3(cross);
}

// Unit dominant left singular vector of the 2x3 matrix with rows (x1, x2), stable at rank 1.
bool DominantLeftSingularVector(const double x1[3], const double x2[3], double e_sky[3]) {
  const double g00 = so3::Dot3(x1, x1);
  const double g11 = so3::Dot3(x2, x2);
  const double g01 = so3::Dot3(x1, x2);
  const double lambda1 = 0.5 * (g00 + g11) + std::hypot(0.5 * (g00 - g11), g01);
  if (!(lambda1 > 0.0)) {
    e_sky[0] = e_sky[1] = e_sky[2] = 0.0;
    return false;
  }
  double c = 0.0;
  double s = 0.0;
  if (g00 >= g11) {
    c = lambda1 - g11;
    s = g01;
  } else {
    c = g01;
    s = lambda1 - g00;
  }
  const double vn = std::hypot(c, s);
  if (vn > 0.0) {
    c /= vn;
    s /= vn;
  } else {
    c = 1.0;
    s = 0.0;
  }
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

void CopyNodeToEvent(const PoseCurveNode& node, CurveEventKind kind, int detail_index, CurveEvent* ev) {
  ev->kind = kind;
  ev->arclength = node.arclength;
  for (int k = 0; k < 3; k++) {
    ev->param[k] = node.param[k];
    ev->sky[k] = node.sky[k];
  }
  for (int k = 0; k < 9; k++) {
    ev->pose[k] = node.pose[k];
  }
  ev->detail_index = detail_index;
}

// =================================================================================================
// 1D Family Tracing
// =================================================================================================

struct Eval1D {
  bool valid = false;
  OpticalMarginInfo margin;
  double param[3]{};
  double pose[9]{};
  double u[3]{};
  double sky[3]{};
  double dx[3]{};
  double sky_speed = 0.0;
  double internal_disc[kMaxFaceCount]{};
  bool corridor_open = true;
};

Eval1D Evaluate1DPoint(const FaceNormalTable& table, const int* slots, int slot_count, double refractive_index,
                       const double sun_dir[3], const PoseManifold& manifold, int free_ax, double t_val,
                       Corridor* corridor, bool eval_corridor) {
  Eval1D out;
  for (int k = 0; k < 3; k++) {
    out.param[k] = manifold.axes[k].value;
  }
  out.param[free_ax] = WrapAxis(t_val, manifold.axes[free_ax]);

  Jet<1> ang[3];
  for (int k = 0; k < 3; k++) {
    ang[k] = (k == free_ax) ? Jet<1>::Variable(out.param[k], 0) : Jet<1>(out.param[k]);
  }
  Jet<1> pose_j[9];
  EulerPoseRad(ang[0], ang[1], ang[2], pose_j);
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
  out.margin = ClassifyMargins(chain_ok, domain, slot_count);
  if (!out.margin.valid) {
    out.valid = false;
    return out;
  }
  for (int i = 0; i < 3; i++) {
    if (!std::isfinite(outgoing[i].a) || !std::isfinite(outgoing[i].v[0])) {
      out.valid = false;
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
    out.corridor_open = EvaluateCorridorOpen(corridor, out.pose, sun_dir, refractive_index);
  }
  return out;
}

PoseCurveNode NodeFromEval1D(const Eval1D& e, int free_ax, double arclength) {
  PoseCurveNode node;
  for (int k = 0; k < 3; k++) {
    node.param[k] = e.param[k];
    node.u[k] = e.u[k];
    node.sky[k] = e.sky[k];
    node.tangent[k] = (k == free_ax) ? 1.0 : 0.0;
  }
  for (int k = 0; k < 9; k++) {
    node.pose[k] = e.pose[k];
  }
  node.sky_speed = e.sky_speed;
  node.sigma1 = e.sky_speed;
  node.sigma2 = 0.0;
  node.arclength = arclength;
  return node;
}

// Bisects the optical boundary on [t_valid, t_invalid] (either direction), returning the last
// valid Eval1D and the failing margin index from the invalid side.
Eval1D BisectOpticalBoundary1D(const FaceNormalTable& table, const int* slots, int slot_count, double refractive_index,
                               const double sun_dir[3], const PoseManifold& manifold, int free_ax, double t_valid,
                               double t_invalid, Corridor* corridor, int iters, double* t_out, int* failing_index_out) {
  double tv = t_valid;
  double ti = t_invalid;
  Eval1D best =
      Evaluate1DPoint(table, slots, slot_count, refractive_index, sun_dir, manifold, free_ax, tv, corridor, true);
  int fail_idx = -1;
  {
    const Eval1D ei =
        Evaluate1DPoint(table, slots, slot_count, refractive_index, sun_dir, manifold, free_ax, ti, nullptr, false);
    fail_idx = ei.margin.failing_index;
  }
  for (int it = 0; it < iters; it++) {
    const double tm = 0.5 * (tv + ti);
    const Eval1D em =
        Evaluate1DPoint(table, slots, slot_count, refractive_index, sun_dir, manifold, free_ax, tm, nullptr, false);
    if (em.valid) {
      tv = tm;
      best = em;
    } else {
      ti = tm;
      if (em.margin.failing_index >= 0) {
        fail_idx = em.margin.failing_index;
      }
    }
  }
  best.corridor_open = EvaluateCorridorOpen(corridor, best.pose, sun_dir, refractive_index);
  *t_out = tv;
  *failing_index_out = fail_idx;
  return best;
}

void BuildSegments(const std::vector<PoseCurveNode>& nodes, const std::vector<bool>& node_stationary,
                   const std::vector<bool>& edge_corridor_open, double speed_tol, std::vector<CurveSegment>* segments) {
  segments->clear();
  const int n = static_cast<int>(nodes.size());
  if (n == 0) {
    return;
  }
  if (n == 1) {
    CurveSegment seg;
    seg.start_idx = 0;
    seg.end_idx = 0;
    seg.kind = node_stationary[0] ? CurveSegmentKind::kPointImage : CurveSegmentKind::kArc;
    seg.corridor_open = edge_corridor_open.empty() ? true : edge_corridor_open[0];
    segments->push_back(seg);
    return;
  }

  // Classify each edge (i, i+1) as kPointImage only when both endpoints are stationary and the
  // sky move across the edge is consistent with a constant image.
  const int edge_cnt = n - 1;
  std::vector<CurveSegmentKind> edge_kind(static_cast<size_t>(edge_cnt), CurveSegmentKind::kArc);
  for (int i = 0; i < edge_cnt; i++) {
    const size_t idx = static_cast<size_t>(i);
    if (node_stationary[idx] && node_stationary[idx + 1]) {
      const double ds = nodes[idx + 1].arclength - nodes[idx].arclength;
      const double sky_step = so3::AngleBetween(nodes[idx].sky, nodes[idx + 1].sky);
      const double max_s1 = std::max(nodes[idx].sigma1, nodes[idx + 1].sigma1);
      const double sky_bar = std::max(1e-5, 10.0 * speed_tol * std::max(ds, 1e-4) * std::max(max_s1, 1.0));
      if (sky_step <= sky_bar) {
        edge_kind[idx] = CurveSegmentKind::kPointImage;
      }
    }
  }

  int seg_start = 0;
  for (int i = 1; i <= edge_cnt; i++) {
    const bool split =
        (i == edge_cnt) || (edge_kind[static_cast<size_t>(i)] != edge_kind[static_cast<size_t>(seg_start)]) ||
        (edge_corridor_open[static_cast<size_t>(i)] != edge_corridor_open[static_cast<size_t>(seg_start)]);
    if (split) {
      CurveSegment seg;
      seg.start_idx = seg_start;
      seg.end_idx = i;
      seg.kind = edge_kind[static_cast<size_t>(seg_start)];
      seg.corridor_open = edge_corridor_open[static_cast<size_t>(seg_start)];
      segments->push_back(seg);
      seg_start = i;
    }
  }
}

void Trace1DInterval(const FaceNormalTable& table, const int* slots, int slot_count, double refractive_index,
                     const double sun_dir[3], const PoseManifold& manifold, int free_ax, double t_start, double t_end,
                     bool is_closed, bool left_is_domain, bool right_is_domain, int left_fail_idx, int right_fail_idx,
                     Corridor* corridor, const Family1DParams& params, const DiscoveryStop* stop, PoseCurve* curve,
                     bool* stop_requested) {
  curve->kind = PoseCurveKind::kFamily1D;
  curve->coverage.seed_grid_n = params.scan_samples;
  curve->coverage.closed = is_closed;

  const Eval1D e_start =
      Evaluate1DPoint(table, slots, slot_count, refractive_index, sun_dir, manifold, free_ax, t_start, corridor, true);
  if (!e_start.valid) {
    return;
  }

  std::vector<bool> node_stationary;
  std::vector<bool> edge_corridor_open;

  auto push_node = [&](const Eval1D& e, double arclen, bool edge_open) {
    if (!curve->nodes.empty()) {
      edge_corridor_open.push_back(edge_open);
    }
    curve->nodes.push_back(NodeFromEval1D(e, free_ax, arclen));
    node_stationary.push_back(e.sky_speed <= params.speed_tol);
  };

  push_node(e_start, 0.0, e_start.corridor_open);

  if (!is_closed) {
    CurveEvent ev;
    CopyNodeToEvent(curve->nodes.front(),
                    left_is_domain ? CurveEventKind::kDomainBoundary : CurveEventKind::kOpticalBoundary,
                    left_is_domain ? free_ax : left_fail_idx, &ev);
    curve->events.push_back(ev);
  }

  // Adaptive stepping from t_start to t_end with online event bisection.
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
    Eval1D e_next =
        Evaluate1DPoint(table, slots, slot_count, refractive_index, sun_dir, manifold, free_ax, t_next, corridor, true);

    // Subdivide if the step hit an unexpected invalid sliver or turned too sharply (unless
    // bracketing a stationary point where dx reverses sign).
    bool shrunk = false;
    while (step > params.min_step * 2.0) {
      if (!e_next.valid) {
        step *= 0.5;
        t_next = t_curr + step;
        e_next = Evaluate1DPoint(table, slots, slot_count, refractive_index, sun_dir, manifold, free_ax, t_next,
                                 corridor, true);
        shrunk = true;
        continue;
      }
      if (e_curr.sky_speed > params.speed_tol && e_next.sky_speed > params.speed_tol) {
        const double d_angle = so3::AngleBetween(e_curr.dx, e_next.dx);
        const double sky_step = so3::AngleBetween(e_curr.sky, e_next.sky);
        // Only shrink for turning angle when not a 180-deg sign flip near a stationary point.
        if (sky_step > params.max_turning_angle || (d_angle > params.max_turning_angle && d_angle < 2.5)) {
          step *= 0.5;
          t_next = t_curr + step;
          e_next = Evaluate1DPoint(table, slots, slot_count, refractive_index, sun_dir, manifold, free_ax, t_next,
                                   corridor, true);
          shrunk = true;
          continue;
        }
      }
      break;
    }

    if (!e_next.valid) {
      // Unexpected interior optical boundary (narrow invalid gap missed by coarse scan).
      double t_bnd = t_curr;
      int fail_idx = -1;
      const Eval1D e_bnd =
          BisectOpticalBoundary1D(table, slots, slot_count, refractive_index, sun_dir, manifold, free_ax, t_curr,
                                  t_next, corridor, params.bisection_iters, &t_bnd, &fail_idx);
      const double ds = t_bnd - t_curr;
      if (ds > 1e-13) {
        arclen += ds;
        push_node(e_bnd, arclen, e_curr.corridor_open);
      }
      CurveEvent ev;
      CopyNodeToEvent(curve->nodes.back(), CurveEventKind::kOpticalBoundary, fail_idx, &ev);
      curve->events.push_back(ev);
      curve->coverage.closed = false;
      break;
    }

    // 1. Check stationary point (||dx/dt|| = 0): dx reverses direction across [t_curr, t_next].
    if (e_curr.sky_speed > params.speed_tol && e_next.sky_speed > params.speed_tol &&
        so3::Dot3(e_curr.dx, e_next.dx) < 0.0) {
      double ref[3] = { e_next.dx[0] - e_curr.dx[0], e_next.dx[1] - e_curr.dx[1], e_next.dx[2] - e_curr.dx[2] };
      const double rn = so3::Norm3(ref);
      if (rn > 0.0) {
        ref[0] /= rn;
        ref[1] /= rn;
        ref[2] /= rn;
        double ta = t_curr;
        double tb = t_next;
        double fa = so3::Dot3(e_curr.dx, ref);
        Eval1D e_mid = e_curr;
        for (int it = 0; it < params.bisection_iters; it++) {
          const double tm = 0.5 * (ta + tb);
          e_mid = Evaluate1DPoint(table, slots, slot_count, refractive_index, sun_dir, manifold, free_ax, tm, nullptr,
                                  false);
          if (!e_mid.valid) {
            break;
          }
          const double fm = so3::Dot3(e_mid.dx, ref);
          if ((fa <= 0.0 && fm <= 0.0) || (fa > 0.0 && fm > 0.0)) {
            ta = tm;
            fa = fm;
          } else {
            tb = tm;
          }
        }
        const double t_stat = 0.5 * (ta + tb);
        const Eval1D e_stat = Evaluate1DPoint(table, slots, slot_count, refractive_index, sun_dir, manifold, free_ax,
                                              t_stat, nullptr, false);
        if (e_stat.valid && e_stat.sky_speed <= params.stationary_tol) {
          const PoseCurveNode n_stat = NodeFromEval1D(e_stat, free_ax, arclen + (t_stat - t_curr));
          CurveEvent ev;
          CopyNodeToEvent(n_stat, CurveEventKind::kStationaryPoint, -1, &ev);
          curve->events.push_back(ev);
        }
      }
    }

    // 2. Check internal TIR onset for each internal reflection step k in [1, slot_count - 2].
    for (int k = 1; k < slot_count - 1; k++) {
      const double da = e_curr.internal_disc[k];
      const double db = e_next.internal_disc[k];
      if ((da > 0.0) != (db > 0.0)) {
        double ta = t_curr;
        double tb = t_next;
        bool pos_a = da > 0.0;
        for (int it = 0; it < params.bisection_iters; it++) {
          const double tm = 0.5 * (ta + tb);
          const Eval1D em = Evaluate1DPoint(table, slots, slot_count, refractive_index, sun_dir, manifold, free_ax, tm,
                                            nullptr, false);
          if (!em.valid) {
            break;
          }
          if ((em.internal_disc[k] > 0.0) == pos_a) {
            ta = tm;
          } else {
            tb = tm;
          }
        }
        const double t_tir = 0.5 * (ta + tb);
        const Eval1D e_tir = Evaluate1DPoint(table, slots, slot_count, refractive_index, sun_dir, manifold, free_ax,
                                             t_tir, nullptr, false);
        if (e_tir.valid) {
          const PoseCurveNode n_tir = NodeFromEval1D(e_tir, free_ax, arclen + (t_tir - t_curr));
          CurveEvent ev;
          CopyNodeToEvent(n_tir, CurveEventKind::kTirOnset, k, &ev);
          curve->events.push_back(ev);
        }
      }
    }

    // 3. Check corridor cutoff (A_P crossing 0), inserting the exact cutoff node so segments split cleanly.
    if (corridor != nullptr && e_curr.corridor_open != e_next.corridor_open) {
      double ta = t_curr;
      double tb = t_next;
      const bool open_a = e_curr.corridor_open;
      for (int it = 0; it < params.bisection_iters; it++) {
        const double tm = 0.5 * (ta + tb);
        const Eval1D em =
            Evaluate1DPoint(table, slots, slot_count, refractive_index, sun_dir, manifold, free_ax, tm, corridor, true);
        if (!em.valid) {
          break;
        }
        if (em.corridor_open == open_a) {
          ta = tm;
        } else {
          tb = tm;
        }
      }
      const double t_cut = 0.5 * (ta + tb);
      const Eval1D e_cut = Evaluate1DPoint(table, slots, slot_count, refractive_index, sun_dir, manifold, free_ax,
                                           t_cut, corridor, true);
      if (e_cut.valid) {
        const double ds1 = t_cut - t_curr;
        if (ds1 > 1e-13) {
          arclen += ds1;
          push_node(e_cut, arclen, open_a);
        }
        CurveEvent ev;
        CopyNodeToEvent(curve->nodes.back(), CurveEventKind::kCorridorCutoff, -1, &ev);
        curve->events.push_back(ev);
        const double ds2 = t_next - t_cut;
        if (ds2 > 1e-13) {
          arclen += ds2;
          push_node(e_next, arclen, e_next.corridor_open);
        }
        t_curr = t_next;
        e_curr = e_next;
        if (!shrunk) {
          h = std::min(params.max_step, step * 1.25);
        } else {
          h = std::max(params.min_step, step);
        }
        continue;
      }
    }

    arclen += (t_next - t_curr);
    push_node(e_next, arclen, e_curr.corridor_open);
    t_curr = t_next;
    e_curr = e_next;
    if (!shrunk) {
      h = std::min(params.max_step, step * 1.25);
    } else {
      h = std::max(params.min_step, step);
    }
  }

  if (!is_closed && !curve->coverage.budget_truncated && !*stop_requested && std::fabs(t_curr - t_end) <= 1e-9) {
    CurveEvent ev;
    CopyNodeToEvent(curve->nodes.back(),
                    right_is_domain ? CurveEventKind::kDomainBoundary : CurveEventKind::kOpticalBoundary,
                    right_is_domain ? free_ax : right_fail_idx, &ev);
    curve->events.push_back(ev);
  }

  std::sort(curve->events.begin(), curve->events.end(),
            [](const CurveEvent& a, const CurveEvent& b) { return a.arclength < b.arclength; });
  BuildSegments(curve->nodes, node_stationary, edge_corridor_open, params.speed_tol, &curve->segments);
}

// =================================================================================================
// 2D Caustic Continuation (J = 0 via Jet<2> pre-scan + Jet2<2> predictor-corrector)
// =================================================================================================

struct Eval2DJet1 {
  bool valid = false;
  OpticalMarginInfo margin;
  double param[3]{};
  double pose[9]{};
  double sky[3]{};
  double x1[3]{};
  double x2[3]{};
  double sigma1 = 0.0;
  double sigma2 = 0.0;
  double j_val = 0.0;
  double generator_cross = 0.0;
};

Eval2DJet1 Evaluate2DJet1(const FaceNormalTable& table, const int* slots, int slot_count, double refractive_index,
                          const double sun_dir[3], const PoseManifold& manifold, int ax0, int ax1,
                          const double q_in[2]) {
  Eval2DJet1 out;
  for (int k = 0; k < 3; k++) {
    out.param[k] = manifold.axes[k].value;
  }
  out.param[ax0] = WrapAxis(q_in[0], manifold.axes[ax0]);
  out.param[ax1] = WrapAxis(q_in[1], manifold.axes[ax1]);
  out.generator_cross = GeneratorCrossNorm2D(out.param, ax0, ax1);

  Jet<2> ang[3];
  for (int k = 0; k < 3; k++) {
    if (k == ax0) {
      ang[k] = Jet<2>::Variable(out.param[k], 0);
    } else if (k == ax1) {
      ang[k] = Jet<2>::Variable(out.param[k], 1);
    } else {
      ang[k] = Jet<2>(out.param[k]);
    }
  }
  Jet<2> pose_j[9];
  EulerPoseRad(ang[0], ang[1], ang[2], pose_j);
  for (int i = 0; i < 9; i++) {
    out.pose[i] = pose_j[i].a;
  }

  const Jet<2> incident[3] = { Jet<2>(-sun_dir[0]), Jet<2>(-sun_dir[1]), Jet<2>(-sun_dir[2]) };
  Jet<2> outgoing[3];
  ChainDomain domain;
  double margin_buf[kMaxDomainMargins]{};
  domain.margins = margin_buf;
  const bool chain_ok = TracePathChain<Jet<2>, ChainEvaluation::kFirstFailure>(
      table, slots, slot_count, Jet<2>(refractive_index), incident, pose_j, outgoing, nullptr, &domain);
  out.margin = ClassifyMargins(chain_ok, domain, slot_count);
  if (!out.margin.valid) {
    out.valid = false;
    return out;
  }
  for (int i = 0; i < 3; i++) {
    if (!std::isfinite(outgoing[i].a) || !std::isfinite(outgoing[i].v[0]) || !std::isfinite(outgoing[i].v[1])) {
      out.valid = false;
      return out;
    }
    out.sky[i] = -outgoing[i].a;
    out.x1[i] = -outgoing[i].v[0];
    out.x2[i] = -outgoing[i].v[1];
  }
  const so3::TwoByThreeSvd svd = so3::SvdTwoByThree(out.x1, out.x2);
  out.sigma1 = svd.sigma1;
  out.sigma2 = svd.sigma2;
  double cross[3];
  so3::Cross3(out.x1, out.x2, cross);
  out.j_val = so3::Dot3(out.sky, cross);
  out.valid = std::isfinite(out.j_val) && std::isfinite(out.sigma1) && std::isfinite(out.sigma2);
  return out;
}

struct Eval2DJet2 {
  bool valid = false;
  OpticalMarginInfo margin;
  double param[3]{};
  double pose[9]{};
  double u[3]{};
  double sky[3]{};
  double x1[3]{};
  double x2[3]{};
  double sigma1 = 0.0;
  double sigma2 = 0.0;
  double j_val = 0.0;
  double grad_j[2]{};
  double grad_norm = 0.0;
  double generator_cross = 0.0;
  double internal_disc[kMaxFaceCount]{};
  bool corridor_open = true;
};

// Exact closed-form J and grad J via a single Jet2<2> TracePathChain call (zero finite differences).
// Because x . x_1 = x . x_2 = 0 on S^2, x_1 x x_2 is parallel to x, so x_k . (x_1 x x_2) = 0
// identically and dJ/dq_k = x . (x_{1k} x x_2 + x_1 x x_{2k}).
Eval2DJet2 Evaluate2DJet2(const FaceNormalTable& table, const int* slots, int slot_count, double refractive_index,
                          const double sun_dir[3], const PoseManifold& manifold, int ax0, int ax1, const double q_in[2],
                          Corridor* corridor, bool eval_corridor) {
  Eval2DJet2 out;
  for (int k = 0; k < 3; k++) {
    out.param[k] = manifold.axes[k].value;
  }
  out.param[ax0] = WrapAxis(q_in[0], manifold.axes[ax0]);
  out.param[ax1] = WrapAxis(q_in[1], manifold.axes[ax1]);
  out.generator_cross = GeneratorCrossNorm2D(out.param, ax0, ax1);

  Jet2<2> ang[3];
  for (int k = 0; k < 3; k++) {
    if (k == ax0) {
      ang[k] = Jet2<2>::Variable(out.param[k], 0);
    } else if (k == ax1) {
      ang[k] = Jet2<2>::Variable(out.param[k], 1);
    } else {
      ang[k] = Jet2<2>(out.param[k]);
    }
  }
  Jet2<2> pose_j[9];
  EulerPoseRad(ang[0], ang[1], ang[2], pose_j);
  for (int i = 0; i < 9; i++) {
    out.pose[i] = pose_j[i].a;
  }
  WorldToBody(out.pose, sun_dir, out.u);

  const Jet2<2> incident[3] = { Jet2<2>(-sun_dir[0]), Jet2<2>(-sun_dir[1]), Jet2<2>(-sun_dir[2]) };
  Jet2<2> outgoing[3];
  ChainDomain domain;
  double margin_buf[kMaxDomainMargins]{};
  domain.margins = margin_buf;
  ChainInterfaceDiagnostics<Jet2<2>> diag;
  const bool chain_ok = TracePathChain<Jet2<2>, ChainEvaluation::kFirstFailure>(
      table, slots, slot_count, Jet2<2>(refractive_index), incident, pose_j, outgoing, nullptr, &domain, &diag);
  out.margin = ClassifyMargins(chain_ok, domain, slot_count);
  if (!out.margin.valid) {
    out.valid = false;
    return out;
  }

  double x11[3];
  double x12[3];
  double x22[3];
  for (int i = 0; i < 3; i++) {
    if (!std::isfinite(outgoing[i].a) || !std::isfinite(outgoing[i].v[0]) || !std::isfinite(outgoing[i].v[1]) ||
        !std::isfinite(outgoing[i].h[0][0]) || !std::isfinite(outgoing[i].h[0][1]) ||
        !std::isfinite(outgoing[i].h[1][1])) {
      out.valid = false;
      return out;
    }
    out.sky[i] = -outgoing[i].a;
    out.x1[i] = -outgoing[i].v[0];
    out.x2[i] = -outgoing[i].v[1];
    x11[i] = -outgoing[i].h[0][0];
    x12[i] = -outgoing[i].h[0][1];
    x22[i] = -outgoing[i].h[1][1];
  }
  const so3::TwoByThreeSvd svd = so3::SvdTwoByThree(out.x1, out.x2);
  out.sigma1 = svd.sigma1;
  out.sigma2 = svd.sigma2;

  double c12[3];
  so3::Cross3(out.x1, out.x2, c12);
  out.j_val = so3::Dot3(out.sky, c12);

  double t0a[3];
  double t0b[3];
  so3::Cross3(x11, out.x2, t0a);
  so3::Cross3(out.x1, x12, t0b);
  const double sum0[3] = { t0a[0] + t0b[0], t0a[1] + t0b[1], t0a[2] + t0b[2] };
  out.grad_j[0] = so3::Dot3(out.sky, sum0);

  double t1a[3];
  double t1b[3];
  so3::Cross3(x12, out.x2, t1a);
  so3::Cross3(out.x1, x22, t1b);
  const double sum1[3] = { t1a[0] + t1b[0], t1a[1] + t1b[1], t1a[2] + t1b[2] };
  out.grad_j[1] = so3::Dot3(out.sky, sum1);

  out.grad_norm = std::hypot(out.grad_j[0], out.grad_j[1]);
  if (!std::isfinite(out.j_val) || !std::isfinite(out.grad_norm)) {
    out.valid = false;
    return out;
  }
  for (int k = 1; k < slot_count - 1; k++) {
    out.internal_disc[k] = diag.discriminant[k].a;
  }
  out.valid = true;
  if (eval_corridor) {
    out.corridor_open = EvaluateCorridorOpen(corridor, out.pose, sun_dir, refractive_index);
  }
  return out;
}

void TangentFromGrad(const double grad_j[2], double grad_norm, int dir_sign, double tau[2]) {
  tau[0] = static_cast<double>(dir_sign) * (-grad_j[1] / grad_norm);
  tau[1] = static_cast<double>(dir_sign) * (grad_j[0] / grad_norm);
}

void SkyVelocity2D(const Eval2DJet2& e, const double tau[2], double v_sky[3]) {
  for (int i = 0; i < 3; i++) {
    v_sky[i] = tau[0] * e.x1[i] + tau[1] * e.x2[i];
  }
}

PoseCurveNode NodeFromEval2D(const Eval2DJet2& e, int ax0, int ax1, const double tau[2], double arclength) {
  PoseCurveNode node;
  for (int k = 0; k < 3; k++) {
    node.param[k] = e.param[k];
    node.u[k] = e.u[k];
    node.sky[k] = e.sky[k];
    node.tangent[k] = 0.0;
  }
  node.tangent[ax0] = tau[0];
  node.tangent[ax1] = tau[1];
  for (int k = 0; k < 9; k++) {
    node.pose[k] = e.pose[k];
  }
  double v_sky[3];
  SkyVelocity2D(e, tau, v_sky);
  node.sky_speed = so3::Norm3(v_sky);
  node.sigma1 = e.sigma1;
  node.sigma2 = e.sigma2;
  node.arclength = arclength;
  return node;
}

enum class ProjectOutcome {
  kConverged,
  kDomainCrossed,
  kOpticalInvalid,
  kPoleHit,
  kSingularity,
  kDiverged,
};

struct ProjectResult {
  ProjectOutcome outcome = ProjectOutcome::kDiverged;
  double q[2]{};  // unwrapped coordinates relative to q_start
  Eval2DJet2 eval;
  int iterations = 0;
  int crossed_axis = -1;
  int failing_margin = -1;
};

// Orthogonal 1D Newton projection onto J(q) = 0 from q_start (unwrapped).
ProjectResult ProjectOntoCaustic(const FaceNormalTable& table, const int* slots, int slot_count,
                                 double refractive_index, const double sun_dir[3], const PoseManifold& manifold,
                                 int ax0, int ax1, const double q_start[2], double max_total_drift,
                                 const CausticContinuationParams& params) {
  ProjectResult res;
  res.q[0] = q_start[0];
  res.q[1] = q_start[1];
  const PoseAxis& pax0 = manifold.axes[ax0];
  const PoseAxis& pax1 = manifold.axes[ax1];

  for (int iter = 0; iter <= params.max_newton_iters; iter++) {
    if (!InAxisBounds(res.q[0], pax0)) {
      res.outcome = ProjectOutcome::kDomainCrossed;
      res.crossed_axis = ax0;
      res.iterations = iter;
      return res;
    }
    if (!InAxisBounds(res.q[1], pax1)) {
      res.outcome = ProjectOutcome::kDomainCrossed;
      res.crossed_axis = ax1;
      res.iterations = iter;
      return res;
    }
    res.eval =
        Evaluate2DJet2(table, slots, slot_count, refractive_index, sun_dir, manifold, ax0, ax1, res.q, nullptr, false);
    if (!res.eval.valid) {
      res.outcome = ProjectOutcome::kOpticalInvalid;
      res.failing_margin = res.eval.margin.failing_index;
      res.iterations = iter;
      return res;
    }
    if (res.eval.generator_cross < params.pole_tol) {
      res.outcome = ProjectOutcome::kPoleHit;
      res.crossed_axis = PoseManifold::kAxisZenith;
      res.iterations = iter;
      return res;
    }
    if (std::fabs(res.eval.j_val) <= params.residual_tol) {
      res.outcome = ProjectOutcome::kConverged;
      res.iterations = iter;
      return res;
    }
    if (iter == params.max_newton_iters) {
      break;
    }
    if (res.eval.grad_norm < params.grad_tol) {
      res.outcome = ProjectOutcome::kSingularity;
      res.iterations = iter;
      return res;
    }
    const double scale = -res.eval.j_val / (res.eval.grad_norm * res.eval.grad_norm);
    const double dq0 = scale * res.eval.grad_j[0];
    const double dq1 = scale * res.eval.grad_j[1];
    const double step_norm = std::hypot(dq0, dq1);
    if (!std::isfinite(step_norm) || step_norm > std::max(max_total_drift, 0.25)) {
      res.outcome = ProjectOutcome::kDiverged;
      res.iterations = iter;
      return res;
    }
    res.q[0] += dq0;
    res.q[1] += dq1;
    if (std::hypot(res.q[0] - q_start[0], res.q[1] - q_start[1]) > max_total_drift) {
      res.outcome = ProjectOutcome::kDiverged;
      res.iterations = iter + 1;
      return res;
    }
  }
  res.outcome = ProjectOutcome::kDiverged;
  res.iterations = params.max_newton_iters;
  return res;
}

// Projects onto J = 0 with one free coordinate pinned to `bound_val` (for exact domain boundary).
bool SolveOnDomainBoundary2D(const FaceNormalTable& table, const int* slots, int slot_count, double refractive_index,
                             const double sun_dir[3], const PoseManifold& manifold, int ax0, int ax1, int pinned_slot,
                             double bound_val, double other_start, double max_drift,
                             const CausticContinuationParams& params, double q_out[2], Eval2DJet2* eval_out) {
  double q[2];
  q[pinned_slot] = bound_val;
  q[1 - pinned_slot] = other_start;
  const PoseAxis& other_ax = (pinned_slot == 0) ? manifold.axes[ax1] : manifold.axes[ax0];
  for (int iter = 0; iter <= params.max_newton_iters; iter++) {
    if (!InAxisBounds(q[1 - pinned_slot], other_ax)) {
      return false;
    }
    const Eval2DJet2 e =
        Evaluate2DJet2(table, slots, slot_count, refractive_index, sun_dir, manifold, ax0, ax1, q, nullptr, false);
    if (!e.valid) {
      return false;
    }
    if (std::fabs(e.j_val) <= params.residual_tol) {
      q_out[0] = q[0];
      q_out[1] = q[1];
      *eval_out = e;
      return true;
    }
    if (iter == params.max_newton_iters) {
      break;
    }
    const double deriv = e.grad_j[1 - pinned_slot];
    if (std::fabs(deriv) < params.grad_tol) {
      return false;
    }
    const double step = -e.j_val / deriv;
    if (!std::isfinite(step) || std::fabs(step) > max_drift) {
      return false;
    }
    q[1 - pinned_slot] += step;
    if (std::fabs(q[1 - pinned_slot] - other_start) > max_drift) {
      return false;
    }
  }
  return false;
}

// Bisects along step [q_curr, q_curr + h * tau] while staying on J = 0 via orthogonal projection,
// finding the last valid on-locus point before an optical/domain/pole boundary.
bool BisectBoundaryOnLocus(const FaceNormalTable& table, const int* slots, int slot_count, double refractive_index,
                           const double sun_dir[3], const PoseManifold& manifold, int ax0, int ax1,
                           const double q_curr[2], const double tau_curr[2], double h,
                           const CausticContinuationParams& params, double q_bnd[2], Eval2DJet2* eval_bnd,
                           int* failing_margin_out) {
  double t_lo = 0.0;
  double t_hi = 1.0;
  q_bnd[0] = q_curr[0];
  q_bnd[1] = q_curr[1];
  bool found_advance = false;
  int fail_margin = *failing_margin_out;

  const double drift_cap = std::max(4.0 * h, 1e-4);
  for (int it = 0; it < params.bisection_iters; it++) {
    const double t_mid = 0.5 * (t_lo + t_hi);
    const double q_guess[2] = { q_curr[0] + t_mid * h * tau_curr[0], q_curr[1] + t_mid * h * tau_curr[1] };
    const ProjectResult pr = ProjectOntoCaustic(table, slots, slot_count, refractive_index, sun_dir, manifold, ax0, ax1,
                                                q_guess, drift_cap, params);
    if (pr.outcome == ProjectOutcome::kConverged) {
      t_lo = t_mid;
      q_bnd[0] = pr.q[0];
      q_bnd[1] = pr.q[1];
      *eval_bnd = pr.eval;
      found_advance = true;
    } else {
      t_hi = t_mid;
      if (pr.outcome == ProjectOutcome::kOpticalInvalid && pr.failing_margin >= 0) {
        fail_margin = pr.failing_margin;
      }
    }
  }
  *failing_margin_out = fail_margin;
  return found_advance;
}

// Evaluates a scalar event function along [q_a, q_b] while projecting each bisection midpoint onto J = 0.
template <class Pred>
bool BisectOnLocusEvent(const FaceNormalTable& table, const int* slots, int slot_count, double refractive_index,
                        const double sun_dir[3], const PoseManifold& manifold, int ax0, int ax1, const double q_a[2],
                        const double q_b[2], const double tau_a[2], int dir_sign,
                        const CausticContinuationParams& params, const Pred& side_a_pred, double q_ev[2],
                        Eval2DJet2* eval_ev, double tau_ev[2]) {
  double qa[2] = { q_a[0], q_a[1] };
  double qb[2] = { q_b[0], q_b[1] };
  const double step_len = std::hypot(q_b[0] - q_a[0], q_b[1] - q_a[1]);
  const double drift_cap = std::max(2.0 * step_len, 1e-4);
  bool have_mid = false;
  Eval2DJet2 best_eval;
  double best_q[2] = { q_a[0], q_a[1] };
  double best_tau[2] = { tau_a[0], tau_a[1] };

  for (int it = 0; it < params.bisection_iters; it++) {
    const double q_mid_guess[2] = { 0.5 * (qa[0] + qb[0]), 0.5 * (qa[1] + qb[1]) };
    const ProjectResult pr = ProjectOntoCaustic(table, slots, slot_count, refractive_index, sun_dir, manifold, ax0, ax1,
                                                q_mid_guess, drift_cap, params);
    if (pr.outcome != ProjectOutcome::kConverged || pr.eval.grad_norm < params.grad_tol) {
      break;
    }
    double tau_m[2];
    TangentFromGrad(pr.eval.grad_j, pr.eval.grad_norm, dir_sign, tau_m);
    if (tau_m[0] * tau_a[0] + tau_m[1] * tau_a[1] < 0.0) {
      tau_m[0] = -tau_m[0];
      tau_m[1] = -tau_m[1];
    }
    best_q[0] = pr.q[0];
    best_q[1] = pr.q[1];
    best_eval = pr.eval;
    best_tau[0] = tau_m[0];
    best_tau[1] = tau_m[1];
    have_mid = true;

    if (side_a_pred(pr.eval, tau_m)) {
      qa[0] = pr.q[0];
      qa[1] = pr.q[1];
    } else {
      qb[0] = pr.q[0];
      qb[1] = pr.q[1];
    }
  }
  if (!have_mid) {
    return false;
  }
  q_ev[0] = best_q[0];
  q_ev[1] = best_q[1];
  *eval_ev = best_eval;
  tau_ev[0] = best_tau[0];
  tau_ev[1] = best_tau[1];
  return true;
}

struct Seed2D {
  double q[2]{};
  Eval2DJet2 eval;
  double priority = 0.0;
};

// Checks if a candidate seed already lies on any traced curve using point-to-segment distance
// with normal-offset discrimination so nearby parallel branches are never conflated.
bool IsSeedCoveredByCurves(const Seed2D& seed, int ax0, int ax1, const PoseManifold& manifold,
                           const std::vector<PoseCurve>& curves, double dedup_tol) {
  const PoseAxis& pax0 = manifold.axes[ax0];
  const PoseAxis& pax1 = manifold.axes[ax1];
  double seed_tau[2] = { 0.0, 0.0 };
  double seed_nrm[2] = { 0.0, 0.0 };
  if (seed.eval.grad_norm > 0.0) {
    seed_nrm[0] = seed.eval.grad_j[0] / seed.eval.grad_norm;
    seed_nrm[1] = seed.eval.grad_j[1] / seed.eval.grad_norm;
    seed_tau[0] = -seed_nrm[1];
    seed_tau[1] = seed_nrm[0];
  }
  const double normal_tol = std::min(0.005, 0.2 * dedup_tol);

  for (const PoseCurve& curve : curves) {
    const size_t n = curve.nodes.size();
    if (n == 0) {
      continue;
    }
    if (n == 1) {
      const double p[2] = { curve.nodes[0].param[ax0], curve.nodes[0].param[ax1] };
      if (Dist2D(seed.q, p, pax0, pax1) < 1e-3) {
        return true;
      }
      continue;
    }
    for (size_t i = 0; i + 1 < n; i++) {
      const PoseCurveNode& na = curve.nodes[i];
      const PoseCurveNode& nb = curve.nodes[i + 1];
      const double ax = DeltaAxis(seed.q[0], na.param[ax0], pax0);
      const double ay = DeltaAxis(seed.q[1], na.param[ax1], pax1);
      const double sx = DeltaAxis(nb.param[ax0], na.param[ax0], pax0);
      const double sy = DeltaAxis(nb.param[ax1], na.param[ax1], pax1);
      const double s2 = sx * sx + sy * sy;
      double t = 0.0;
      if (s2 > 0.0) {
        t = std::clamp((ax * sx + ay * sy) / s2, 0.0, 1.0);
      }
      const double rx = ax - t * sx;
      const double ry = ay - t * sy;
      const double dist = std::hypot(rx, ry);
      if (dist < 1e-3) {
        return true;
      }
      if (dist < dedup_tol && seed.eval.grad_norm > 0.0) {
        const double seg_t0 = (1.0 - t) * na.tangent[ax0] + t * nb.tangent[ax0];
        const double seg_t1 = (1.0 - t) * na.tangent[ax1] + t * nb.tangent[ax1];
        const double stn = std::hypot(seg_t0, seg_t1);
        if (stn > 0.0) {
          const double align = std::fabs((seg_t0 * seed_tau[0] + seg_t1 * seed_tau[1]) / stn);
          const double normal_off = std::fabs(rx * seed_nrm[0] + ry * seed_nrm[1]);
          if (align > 0.85 && normal_off < normal_tol) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

struct HalfBranch2D {
  std::vector<PoseCurveNode> nodes;
  std::vector<bool> node_stationary;
  std::vector<bool> edge_corridor_open;
  std::vector<CurveEvent> events;
  bool closed = false;
  bool budget_truncated = false;
  bool stop_requested = false;
  int unresolved_singularities = 0;
};

HalfBranch2D TraceHalfBranch2D(const FaceNormalTable& table, const int* slots, int slot_count, double refractive_index,
                               const double sun_dir[3], const PoseManifold& manifold, int ax0, int ax1,
                               const double q_seed[2], const Eval2DJet2& seed_eval, int dir_sign, bool allow_closure,
                               Corridor* corridor, const CausticContinuationParams& params, const DiscoveryStop* stop) {
  HalfBranch2D out;
  const PoseAxis& pax0 = manifold.axes[ax0];
  const PoseAxis& pax1 = manifold.axes[ax1];

  double tau_seed[2];
  TangentFromGrad(seed_eval.grad_j, seed_eval.grad_norm, dir_sign, tau_seed);

  double q_curr[2] = { q_seed[0], q_seed[1] };
  Eval2DJet2 e_curr = seed_eval;
  e_curr.corridor_open = EvaluateCorridorOpen(corridor, e_curr.pose, sun_dir, refractive_index);
  double tau_curr[2] = { tau_seed[0], tau_seed[1] };
  double arclen = 0.0;

  double e_sky_curr[3]{};
  bool have_e_sky_curr = DominantLeftSingularVector(e_curr.x1, e_curr.x2, e_sky_curr);
  double s_sky_curr = 0.0;
  if (have_e_sky_curr) {
    double v_sky[3];
    SkyVelocity2D(e_curr, tau_curr, v_sky);
    s_sky_curr = so3::Dot3(e_sky_curr, v_sky);
  }

  // Track last moving node for cusp detection even if an intermediate node lands near speed 0.
  bool have_last_moving = have_e_sky_curr && std::fabs(s_sky_curr) > params.speed_tol * e_curr.sigma1;
  double last_moving_q[2] = { q_curr[0], q_curr[1] };
  double last_moving_tau[2] = { tau_curr[0], tau_curr[1] };
  double last_moving_e_sky[3] = { e_sky_curr[0], e_sky_curr[1], e_sky_curr[2] };
  double last_moving_s_sky = s_sky_curr;
  double last_moving_arclen = 0.0;
  int stationary_gap_steps = 0;

  auto push_node = [&](const Eval2DJet2& e, const double tau[2], double s_val, bool edge_open) {
    if (!out.nodes.empty()) {
      out.edge_corridor_open.push_back(edge_open);
    }
    const PoseCurveNode node = NodeFromEval2D(e, ax0, ax1, tau, s_val);
    out.nodes.push_back(node);
    const bool stat = (node.sigma1 <= params.rank_tol) || (node.sky_speed <= params.speed_tol * node.sigma1);
    out.node_stationary.push_back(stat);
  };

  push_node(e_curr, tau_curr, 0.0, e_curr.corridor_open);

  double h = params.initial_step;
  double max_dist_from_seed = 0.0;
  bool moved_away = false;
  const double cos_max_turn = std::cos(params.max_turning_angle);

  // Processes interior events (cusp, internal TIR onset, corridor cutoff) on an accepted edge
  // [q_curr, q_next], appending any inserted cutoff node and q_next itself.
  auto commit_step = [&](const double q_next_in[2], Eval2DJet2 e_next, const double tau_next[2], double adv) {
    e_next.corridor_open = EvaluateCorridorOpen(corridor, e_next.pose, sun_dir, refractive_index);

    // Compute continuously oriented e_sky and signed sky velocity at q_next.
    double e_sky_next[3]{};
    bool have_e_sky_next = DominantLeftSingularVector(e_next.x1, e_next.x2, e_sky_next);
    if (have_e_sky_next && have_e_sky_curr && so3::Dot3(e_sky_curr, e_sky_next) < 0.0) {
      e_sky_next[0] = -e_sky_next[0];
      e_sky_next[1] = -e_sky_next[1];
      e_sky_next[2] = -e_sky_next[2];
    }
    double s_sky_next = 0.0;
    if (have_e_sky_next) {
      double v_sky[3];
      SkyVelocity2D(e_next, tau_next, v_sky);
      s_sky_next = so3::Dot3(e_sky_next, v_sky);
    }

    // 1. Cusp detection: sign change of tangential sky speed s_sky = e_sky . (dx * tau) along J = 0.
    const bool next_moving = have_e_sky_next && std::fabs(s_sky_next) > params.speed_tol * e_next.sigma1;
    if (next_moving) {
      if (have_last_moving && stationary_gap_steps <= 2 && so3::Dot3(last_moving_e_sky, e_sky_next) > 0.5 &&
          last_moving_s_sky * s_sky_next < 0.0) {
        const bool sign_a_pos = last_moving_s_sky > 0.0;
        double q_cusp[2];
        Eval2DJet2 e_cusp;
        double tau_cusp[2];
        auto pred = [&](const Eval2DJet2& em, const double tm[2]) {
          double em_sky[3];
          if (!DominantLeftSingularVector(em.x1, em.x2, em_sky)) {
            return sign_a_pos;
          }
          if (so3::Dot3(last_moving_e_sky, em_sky) < 0.0) {
            em_sky[0] = -em_sky[0];
            em_sky[1] = -em_sky[1];
            em_sky[2] = -em_sky[2];
          }
          double vm[3];
          SkyVelocity2D(em, tm, vm);
          const double sm = so3::Dot3(em_sky, vm);
          return (sm > 0.0) == sign_a_pos;
        };
        if (BisectOnLocusEvent(table, slots, slot_count, refractive_index, sun_dir, manifold, ax0, ax1, last_moving_q,
                               q_next_in, last_moving_tau, dir_sign, params, pred, q_cusp, &e_cusp, tau_cusp)) {
          const double ds_c = std::hypot(q_cusp[0] - last_moving_q[0], q_cusp[1] - last_moving_q[1]);
          const PoseCurveNode n_cusp = NodeFromEval2D(e_cusp, ax0, ax1, tau_cusp, last_moving_arclen + ds_c);
          CurveEvent ev;
          CopyNodeToEvent(n_cusp, CurveEventKind::kCusp, -1, &ev);
          out.events.push_back(ev);
        }
      }
      have_last_moving = true;
      last_moving_q[0] = q_next_in[0];
      last_moving_q[1] = q_next_in[1];
      last_moving_tau[0] = tau_next[0];
      last_moving_tau[1] = tau_next[1];
      so3::Copy3(e_sky_next, last_moving_e_sky);
      last_moving_s_sky = s_sky_next;
      last_moving_arclen = arclen + adv;
      stationary_gap_steps = 0;
    } else {
      stationary_gap_steps++;
      if (stationary_gap_steps > 2) {
        have_last_moving = false;
      }
    }

    // 2. Internal TIR onset detection on [q_curr, q_next].
    for (int k = 1; k < slot_count - 1; k++) {
      const double da = e_curr.internal_disc[k];
      const double db = e_next.internal_disc[k];
      if ((da > 0.0) != (db > 0.0)) {
        const bool pos_a = da > 0.0;
        double q_tir[2];
        Eval2DJet2 e_tir;
        double tau_tir[2];
        auto pred = [k, pos_a](const Eval2DJet2& em, const double* /*tm*/) {
          return (em.internal_disc[k] > 0.0) == pos_a;
        };
        if (BisectOnLocusEvent(table, slots, slot_count, refractive_index, sun_dir, manifold, ax0, ax1, q_curr,
                               q_next_in, tau_curr, dir_sign, params, pred, q_tir, &e_tir, tau_tir)) {
          const double ds_t = std::hypot(q_tir[0] - q_curr[0], q_tir[1] - q_curr[1]);
          const PoseCurveNode n_tir = NodeFromEval2D(e_tir, ax0, ax1, tau_tir, arclen + ds_t);
          CurveEvent ev;
          CopyNodeToEvent(n_tir, CurveEventKind::kTirOnset, k, &ev);
          out.events.push_back(ev);
        }
      }
    }

    // 3. Corridor cutoff detection on [q_curr, q_next], inserting exact cutoff node without stopping.
    if (corridor != nullptr && e_curr.corridor_open != e_next.corridor_open) {
      const bool open_a = e_curr.corridor_open;
      double q_cut[2];
      Eval2DJet2 e_cut;
      double tau_cut[2];
      auto pred = [corridor, sun_dir, refractive_index, open_a](const Eval2DJet2& em, const double* /*tm*/) {
        return EvaluateCorridorOpen(corridor, em.pose, sun_dir, refractive_index) == open_a;
      };
      if (BisectOnLocusEvent(table, slots, slot_count, refractive_index, sun_dir, manifold, ax0, ax1, q_curr, q_next_in,
                             tau_curr, dir_sign, params, pred, q_cut, &e_cut, tau_cut)) {
        const double ds1 = std::hypot(q_cut[0] - q_curr[0], q_cut[1] - q_curr[1]);
        const double ds2 = std::hypot(q_next_in[0] - q_cut[0], q_next_in[1] - q_cut[1]);
        if (ds1 > 1e-13) {
          arclen += ds1;
          push_node(e_cut, tau_cut, arclen, open_a);
        }
        CurveEvent ev;
        CopyNodeToEvent(out.nodes.back(), CurveEventKind::kCorridorCutoff, -1, &ev);
        out.events.push_back(ev);
        if (ds2 > 1e-13) {
          arclen += ds2;
          push_node(e_next, tau_next, arclen, e_next.corridor_open);
        }
        q_curr[0] = q_next_in[0];
        q_curr[1] = q_next_in[1];
        e_curr = e_next;
        tau_curr[0] = tau_next[0];
        tau_curr[1] = tau_next[1];
        have_e_sky_curr = have_e_sky_next;
        so3::Copy3(e_sky_next, e_sky_curr);
        s_sky_curr = s_sky_next;
        return;
      }
    }

    arclen += adv;
    push_node(e_next, tau_next, arclen, e_curr.corridor_open);
    q_curr[0] = q_next_in[0];
    q_curr[1] = q_next_in[1];
    e_curr = e_next;
    tau_curr[0] = tau_next[0];
    tau_curr[1] = tau_next[1];
    have_e_sky_curr = have_e_sky_next;
    so3::Copy3(e_sky_next, e_sky_curr);
    s_sky_curr = s_sky_next;
  };

  for (int step_idx = 0; step_idx < params.max_steps_per_branch; step_idx++) {
    if ((step_idx & 15) == 0 && CheckStop(stop)) {
      out.stop_requested = true;
      out.budget_truncated = true;
      return out;
    }

    // Retry loop with step reduction for curvature, trust region, and boundary approach.
    bool accepted = false;
    ProjectResult pr;
    double tau_next[2]{};
    double adv = 0.0;
    double turn_dot = 1.0;
    double sky_turn_angle = 0.0;

    double v_sky_curr[3];
    SkyVelocity2D(e_curr, tau_curr, v_sky_curr);
    const double v_sky_curr_norm = so3::Norm3(v_sky_curr);

    while (true) {
      const double q_pred[2] = { q_curr[0] + h * tau_curr[0], q_curr[1] + h * tau_curr[1] };
      pr = ProjectOntoCaustic(table, slots, slot_count, refractive_index, sun_dir, manifold, ax0, ax1, q_pred,
                              std::max(2.0 * h, 1e-4), params);
      if (pr.outcome == ProjectOutcome::kConverged) {
        if (pr.eval.grad_norm < params.grad_tol) {
          pr.outcome = ProjectOutcome::kSingularity;
        } else {
          TangentFromGrad(pr.eval.grad_j, pr.eval.grad_norm, dir_sign, tau_next);
          turn_dot = tau_next[0] * tau_curr[0] + tau_next[1] * tau_curr[1];
          const double dq0 = pr.q[0] - q_curr[0];
          const double dq1 = pr.q[1] - q_curr[1];
          adv = std::hypot(dq0, dq1);
          const double proj = dq0 * tau_curr[0] + dq1 * tau_curr[1];

          sky_turn_angle = 0.0;
          bool sky_turn_ok = true;
          if (v_sky_curr_norm > params.speed_tol * e_curr.sigma1) {
            double v_sky_next[3];
            SkyVelocity2D(pr.eval, tau_next, v_sky_next);
            const double v_sky_next_norm = so3::Norm3(v_sky_next);
            if (v_sky_next_norm > params.speed_tol * pr.eval.sigma1) {
              sky_turn_angle = so3::AngleBetween(v_sky_curr, v_sky_next);
              // Allow 180-deg flip across a genuine cusp, but shrink for sharp non-cusp sky bends.
              if (sky_turn_angle > params.max_turning_angle && sky_turn_angle < 2.5) {
                sky_turn_ok = false;
              }
            }
          }

          if (adv > 1e-13 && proj > 0.25 * adv && turn_dot >= cos_max_turn && sky_turn_ok) {
            accepted = true;
            break;
          }
          // Step curved too sharply or jumped sideways: shrink step.
          pr.outcome = ProjectOutcome::kDiverged;
        }
      }

      if (h > params.min_step * 2.0) {
        h = std::max(params.min_step, 0.5 * h);
        continue;
      }

      // Reached minimum step: classify and bisect boundary/singularity cleanly.
      if (pr.outcome == ProjectOutcome::kDomainCrossed || pr.outcome == ProjectOutcome::kPoleHit) {
        int bnd_axis = pr.crossed_axis;
        double q_bnd[2] = { q_curr[0], q_curr[1] };
        Eval2DJet2 e_bnd = e_curr;
        bool got_bnd = false;

        if (pr.outcome == ProjectOutcome::kDomainCrossed) {
          const int pinned_slot = (bnd_axis == ax0) ? 0 : 1;
          const PoseAxis& b_ax = manifold.axes[bnd_axis];
          const double q_pred_val = q_curr[pinned_slot] + h * tau_curr[pinned_slot];
          const double bound_val = (q_pred_val < b_ax.min_val) ? b_ax.min_val : b_ax.max_val;
          double other_guess = q_curr[1 - pinned_slot];
          if (std::fabs(tau_curr[pinned_slot]) > 1e-9) {
            const double alpha = (bound_val - q_curr[pinned_slot]) / tau_curr[pinned_slot];
            other_guess = q_curr[1 - pinned_slot] + alpha * tau_curr[1 - pinned_slot];
          }
          got_bnd = SolveOnDomainBoundary2D(table, slots, slot_count, refractive_index, sun_dir, manifold, ax0, ax1,
                                            pinned_slot, bound_val, other_guess, std::max(4.0 * h, 1e-3), params, q_bnd,
                                            &e_bnd);
        }
        if (!got_bnd) {
          int dummy_fail = -1;
          got_bnd = BisectBoundaryOnLocus(table, slots, slot_count, refractive_index, sun_dir, manifold, ax0, ax1,
                                          q_curr, tau_curr, h, params, q_bnd, &e_bnd, &dummy_fail);
        }
        if (got_bnd && e_bnd.grad_norm >= params.grad_tol) {
          const double ds = std::hypot(q_bnd[0] - q_curr[0], q_bnd[1] - q_curr[1]);
          if (ds > 1e-13) {
            double tau_b[2];
            TangentFromGrad(e_bnd.grad_j, e_bnd.grad_norm, dir_sign, tau_b);
            commit_step(q_bnd, e_bnd, tau_b, ds);
          }
        }
        CurveEvent ev;
        CopyNodeToEvent(out.nodes.back(), CurveEventKind::kDomainBoundary, bnd_axis, &ev);
        out.events.push_back(ev);
        return out;
      }

      if (pr.outcome == ProjectOutcome::kOpticalInvalid) {
        double q_bnd[2] = { q_curr[0], q_curr[1] };
        Eval2DJet2 e_bnd = e_curr;
        int fail_margin = pr.failing_margin;
        if (BisectBoundaryOnLocus(table, slots, slot_count, refractive_index, sun_dir, manifold, ax0, ax1, q_curr,
                                  tau_curr, h, params, q_bnd, &e_bnd, &fail_margin) &&
            e_bnd.grad_norm >= params.grad_tol) {
          const double ds = std::hypot(q_bnd[0] - q_curr[0], q_bnd[1] - q_curr[1]);
          if (ds > 1e-13) {
            double tau_b[2];
            TangentFromGrad(e_bnd.grad_j, e_bnd.grad_norm, dir_sign, tau_b);
            commit_step(q_bnd, e_bnd, tau_b, ds);
          }
        }
        CurveEvent ev;
        CopyNodeToEvent(out.nodes.back(), CurveEventKind::kOpticalBoundary, fail_margin, &ev);
        out.events.push_back(ev);
        return out;
      }

      // Singularity or unresolved divergence at minimum step.
      // Check if a tiny step along the tangent actually left optical validity.
      const double q_probe[2] = { q_curr[0] + h * tau_curr[0], q_curr[1] + h * tau_curr[1] };
      const Eval2DJet2 e_probe = Evaluate2DJet2(table, slots, slot_count, refractive_index, sun_dir, manifold, ax0, ax1,
                                                q_probe, nullptr, false);
      if (!e_probe.valid) {
        CurveEvent ev;
        CopyNodeToEvent(out.nodes.back(), CurveEventKind::kOpticalBoundary, e_probe.margin.failing_index, &ev);
        out.events.push_back(ev);
      } else {
        out.unresolved_singularities++;
        CurveEvent ev;
        CopyNodeToEvent(out.nodes.back(), CurveEventKind::kLocusSingularity, -1, &ev);
        out.events.push_back(ev);
      }
      return out;
    }

    if (!accepted) {
      return out;
    }

    // Track excursion away from seed so tiny loops can still close accurately.
    const double d_prev_0 = DeltaAxis(q_curr[0], q_seed[0], pax0);
    const double d_prev_1 = DeltaAxis(q_curr[1], q_seed[1], pax1);
    const double d_next_0 = DeltaAxis(pr.q[0], q_seed[0], pax0);
    const double d_next_1 = DeltaAxis(pr.q[1], q_seed[1], pax1);
    const double dist_prev = std::hypot(d_prev_0, d_prev_1);
    const double dist_next = std::hypot(d_next_0, d_next_1);
    max_dist_from_seed = std::max(max_dist_from_seed, dist_next);
    if (out.nodes.size() >= 3 && max_dist_from_seed > std::max(1.5 * params.initial_step, 3.0 * params.min_step)) {
      moved_away = true;
    }

    // Check loop closure across the seed's normal hyperplane.
    if (allow_closure && moved_away && out.nodes.size() >= 4) {
      const double s_prev = d_prev_0 * tau_seed[0] + d_prev_1 * tau_seed[1];
      const double s_next = d_next_0 * tau_seed[0] + d_next_1 * tau_seed[1];
      const double tang_align = tau_next[0] * tau_seed[0] + tau_next[1] * tau_seed[1];
      if ((dist_prev <= params.closure_tol || dist_next <= params.closure_tol) && tang_align > 0.7 && s_prev <= 0.0 &&
          s_next >= -1e-9) {
        // Close cleanly at q_seed (shifted by periodic wraps to sit adjacent to q_curr).
        const double q_close[2] = { q_curr[0] - d_prev_0, q_curr[1] - d_prev_1 };
        if (dist_prev > 1e-12) {
          commit_step(q_close, seed_eval, tau_seed, dist_prev);
        }
        out.closed = true;
        return out;
      }
    }

    const double prev_margin = e_curr.margin.min_eff_margin;
    const double next_margin = pr.eval.margin.min_eff_margin;
    commit_step(pr.q, pr.eval, tau_next, adv);

    // Adapt step size for next iteration: Newton work, parameter & sky turning angles, optical margin, domain bounds.
    double h_next = h;
    const double eff_turn =
        std::max(std::acos(std::clamp(turn_dot, -1.0, 1.0)), sky_turn_angle < 2.5 ? sky_turn_angle : 0.0);
    if (pr.iterations <= 2 && eff_turn < 0.25 * params.max_turning_angle) {
      h_next = std::min(params.max_step, h * 1.25);
    } else if (pr.iterations >= 6 || eff_turn > 0.6 * params.max_turning_angle) {
      h_next = std::max(params.min_step, h * 0.7);
    }

    if (next_margin < params.event_slowdown_margin && next_margin < prev_margin && adv > 0.0) {
      const double rate = (prev_margin - next_margin) / adv;
      if (rate > 0.0) {
        const double d_est = next_margin / rate;
        h_next = std::min(h_next, std::max(params.min_step, 0.5 * d_est));
      }
    }

    for (int slot = 0; slot < 2; slot++) {
      const PoseAxis& ax = (slot == 0) ? pax0 : pax1;
      if (ax.periodic) {
        continue;
      }
      if (tau_curr[slot] < -1e-9) {
        const double d_lo = q_curr[slot] - ax.min_val;
        if (d_lo > 2.0 * params.min_step && d_lo < h_next) {
          h_next = std::min(h_next, std::max(params.min_step, 0.5 * d_lo / std::fabs(tau_curr[slot])));
        }
      } else if (tau_curr[slot] > 1e-9) {
        const double d_hi = ax.max_val - q_curr[slot];
        if (d_hi > 2.0 * params.min_step && d_hi < h_next) {
          h_next = std::min(h_next, std::max(params.min_step, 0.5 * d_hi / std::fabs(tau_curr[slot])));
        }
      }
    }

    h = std::clamp(h_next, params.min_step, params.max_step);
  }

  out.budget_truncated = true;
  return out;
}

// Stitches backward and forward half-branches into one monotonically parameterized PoseCurve.
PoseCurve StitchCausticCurve(const HalfBranch2D& forward, const HalfBranch2D& backward, int seed_grid_n,
                             double speed_tol) {
  PoseCurve curve;
  curve.kind = PoseCurveKind::kCaustic2D;
  curve.coverage.seed_grid_n = seed_grid_n;
  curve.coverage.closed = forward.closed;
  curve.coverage.budget_truncated = forward.budget_truncated || backward.budget_truncated;
  curve.coverage.unresolved_singularities = forward.unresolved_singularities + backward.unresolved_singularities;

  std::vector<bool> node_stationary;
  std::vector<bool> edge_corridor_open;

  if (forward.closed || backward.nodes.size() <= 1) {
    curve.nodes = forward.nodes;
    node_stationary = forward.node_stationary;
    edge_corridor_open = forward.edge_corridor_open;
    curve.events = forward.events;
  } else {
    const double back_len = backward.nodes.back().arclength;
    const size_t b_cnt = backward.nodes.size();
    const size_t f_cnt = forward.nodes.size();
    curve.nodes.reserve(b_cnt - 1 + f_cnt);
    node_stationary.reserve(b_cnt - 1 + f_cnt);
    edge_corridor_open.reserve(b_cnt - 1 + f_cnt);

    // Reverse backward[b_cnt - 1 .. 1], negating tangents to point in the forward curve direction.
    for (size_t idx = b_cnt - 1; idx >= 1; idx--) {
      PoseCurveNode node = backward.nodes[idx];
      node.arclength = back_len - node.arclength;
      node.tangent[0] = -node.tangent[0];
      node.tangent[1] = -node.tangent[1];
      node.tangent[2] = -node.tangent[2];
      curve.nodes.push_back(node);
      node_stationary.push_back(backward.node_stationary[idx]);
      edge_corridor_open.push_back(backward.edge_corridor_open[idx - 1]);
    }
    // Append forward[0 .. f_cnt - 1] shifted by back_len.
    for (size_t idx = 0; idx < f_cnt; idx++) {
      PoseCurveNode node = forward.nodes[idx];
      node.arclength += back_len;
      curve.nodes.push_back(node);
      node_stationary.push_back(forward.node_stationary[idx]);
      if (idx + 1 < f_cnt) {
        edge_corridor_open.push_back(forward.edge_corridor_open[idx]);
      }
    }

    // Shift and merge events from both halves.
    for (CurveEvent ev : backward.events) {
      ev.arclength = back_len - ev.arclength;
      curve.events.push_back(ev);
    }
    for (CurveEvent ev : forward.events) {
      ev.arclength += back_len;
      curve.events.push_back(ev);
    }
  }

  std::sort(curve.events.begin(), curve.events.end(),
            [](const CurveEvent& a, const CurveEvent& b) { return a.arclength < b.arclength; });
  BuildSegments(curve.nodes, node_stationary, edge_corridor_open, speed_tol, &curve.segments);
  return curve;
}

}  // namespace

// =================================================================================================
// Public API: TraceFamily1D
// =================================================================================================

CausticTraceResult TraceFamily1D(const FaceNormalTable& table, const int* slots, int slot_count,
                                 double refractive_index, const double sun_dir[3], const PoseManifold& manifold,
                                 Corridor* corridor, const Family1DParams& params, const DiscoveryStop* stop) {
  CausticTraceResult result;
  result.coverage.seed_grid_n = params.scan_samples;
  if (!manifold.IsValid() || manifold.Dimension() != 1 || slots == nullptr || slot_count < 1 ||
      slot_count > kMaxFaceCount || !std::isfinite(refractive_index) || refractive_index <= 0.0 ||
      !ValidateUnitVector(sun_dir) || params.scan_samples < 4) {
    return result;
  }

  int free_axes[3]{};
  manifold.FreeAxes(free_axes);
  const int ax = free_axes[0];
  const PoseAxis& pax = manifold.axes[ax];
  const int n = params.scan_samples;
  const int sample_cnt = pax.periodic ? n : (n + 1);
  const double span = pax.Span();

  std::vector<double> ts(static_cast<size_t>(sample_cnt));
  std::vector<bool> valid(static_cast<size_t>(sample_cnt), false);
  double max_speed = 0.0;
  int valid_cnt = 0;

  for (int i = 0; i < sample_cnt; i++) {
    if ((i & 31) == 0 && CheckStop(stop)) {
      result.stop_requested = true;
      result.coverage.budget_truncated = true;
      return result;
    }
    const double t = pax.min_val + span * static_cast<double>(i) / static_cast<double>(n);
    ts[static_cast<size_t>(i)] = t;
    const Eval1D e =
        Evaluate1DPoint(table, slots, slot_count, refractive_index, sun_dir, manifold, ax, t, nullptr, false);
    if (e.valid) {
      valid[static_cast<size_t>(i)] = true;
      valid_cnt++;
      max_speed = std::max(max_speed, e.sky_speed);
    }
  }

  if (valid_cnt == 0) {
    result.generic_rank = -1;
    return result;
  }
  result.generic_rank = (max_speed > params.speed_tol) ? 1 : 0;

  // Periodic full loop: every scan sample is valid.
  if (pax.periodic && valid_cnt == sample_cnt) {
    PoseCurve curve;
    bool stopped = false;
    Trace1DInterval(table, slots, slot_count, refractive_index, sun_dir, manifold, ax, pax.min_val, pax.max_val, true,
                    false, false, -1, -1, corridor, params, stop, &curve, &stopped);
    result.stop_requested = result.stop_requested || stopped;
    result.coverage.closed = curve.coverage.closed;
    result.coverage.budget_truncated = result.coverage.budget_truncated || curve.coverage.budget_truncated;
    if (!curve.nodes.empty()) {
      result.curves.push_back(std::move(curve));
    }
    return result;
  }

  // Find maximal valid runs. On a periodic axis with at least one invalid sample, start scanning
  // from an invalid index so no valid run is split across the periodic seam.
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
    const int idx = (start_offset + i) % sample_cnt;
    if (!valid[static_cast<size_t>(idx)]) {
      i++;
      continue;
    }
    const int run_start_step = i;
    while (i < sample_cnt && valid[static_cast<size_t>((start_offset + i) % sample_cnt)]) {
      i++;
    }
    const int run_end_step = i - 1;

    // Unwrapped parameter coordinates for this valid run.
    auto t_at_step = [&](int s_step) {
      return pax.min_val + span * static_cast<double>(start_offset + s_step) / static_cast<double>(n);
    };

    double t_left = t_at_step(run_start_step);
    bool left_is_domain = false;
    int left_fail_idx = -1;
    if (!pax.periodic && run_start_step == 0) {
      left_is_domain = true;
    } else {
      const double t_inv = t_at_step(run_start_step - 1);
      BisectOpticalBoundary1D(table, slots, slot_count, refractive_index, sun_dir, manifold, ax, t_left, t_inv,
                              corridor, params.bisection_iters, &t_left, &left_fail_idx);
    }

    double t_right = t_at_step(run_end_step);
    bool right_is_domain = false;
    int right_fail_idx = -1;
    if (!pax.periodic && run_end_step == sample_cnt - 1) {
      right_is_domain = true;
    } else {
      const double t_inv = t_at_step(run_end_step + 1);
      BisectOpticalBoundary1D(table, slots, slot_count, refractive_index, sun_dir, manifold, ax, t_right, t_inv,
                              corridor, params.bisection_iters, &t_right, &right_fail_idx);
    }

    PoseCurve curve;
    bool stopped = false;
    Trace1DInterval(table, slots, slot_count, refractive_index, sun_dir, manifold, ax, t_left, t_right, false,
                    left_is_domain, right_is_domain, left_fail_idx, right_fail_idx, corridor, params, stop, &curve,
                    &stopped);
    result.stop_requested = result.stop_requested || stopped;
    result.coverage.budget_truncated = result.coverage.budget_truncated || curve.coverage.budget_truncated;
    if (!curve.nodes.empty()) {
      result.curves.push_back(std::move(curve));
    }
  }

  return result;
}

// =================================================================================================
// Public API: TraceCausticLocus
// =================================================================================================

CausticTraceResult TraceCausticLocus(const FaceNormalTable& table, const int* slots, int slot_count,
                                     double refractive_index, const double sun_dir[3], const PoseManifold& manifold,
                                     Corridor* corridor, const CausticContinuationParams& params,
                                     const DiscoveryStop* stop) {
  CausticTraceResult result;
  result.coverage.seed_grid_n = params.seed_grid_n;
  if (!manifold.IsValid() || manifold.Dimension() != 2 || slots == nullptr || slot_count < 1 ||
      slot_count > kMaxFaceCount || !std::isfinite(refractive_index) || refractive_index <= 0.0 ||
      !ValidateUnitVector(sun_dir) || params.seed_grid_n < 4) {
    return result;
  }

  int free_axes[3]{};
  manifold.FreeAxes(free_axes);
  const int ax0 = free_axes[0];
  const int ax1 = free_axes[1];
  const PoseAxis& pax0 = manifold.axes[ax0];
  const PoseAxis& pax1 = manifold.axes[ax1];
  const int n = params.seed_grid_n;

  auto grid_coord = [n](int idx, const PoseAxis& ax) {
    if (ax.periodic) {
      return ax.min_val + ax.Span() * (static_cast<double>(idx) + 0.5) / static_cast<double>(n);
    }
    return ax.min_val + ax.Span() * static_cast<double>(idx) / static_cast<double>(n - 1);
  };
  const double step0 = pax0.periodic ? pax0.Span() / static_cast<double>(n) : pax0.Span() / static_cast<double>(n - 1);
  const double step1 = pax1.periodic ? pax1.Span() / static_cast<double>(n) : pax1.Span() / static_cast<double>(n - 1);
  const double cell_diag = std::hypot(step0, step1);

  // Stage 1: Coarse Jet<2> pre-scan.
  std::vector<Eval2DJet1> grid(static_cast<size_t>(n * n));
  int valid_count = 0;
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
    const double q0 = grid_coord(i, pax0);
    for (int j = 0; j < n; j++) {
      const double q1 = grid_coord(j, pax1);
      const double q[2] = { q0, q1 };
      Eval2DJet1& cell = grid[static_cast<size_t>(i * n + j)];
      cell = Evaluate2DJet1(table, slots, slot_count, refractive_index, sun_dir, manifold, ax0, ax1, q);
      if (!cell.valid) {
        continue;
      }
      valid_count++;
      if (cell.margin.min_eff_margin > best_valid_margin) {
        best_valid_margin = cell.margin.min_eff_margin;
        best_valid_i = i;
        best_valid_j = j;
      }
      max_x1_norm = std::max(max_x1_norm, so3::Norm3(cell.x1));
      max_x2_norm = std::max(max_x2_norm, so3::Norm3(cell.x2));
      if (cell.sigma1 > params.rank_tol) {
        any_rank1 = true;
        if (cell.generator_cross >= params.pole_tol && cell.sigma2 > params.rank_tol * cell.sigma1) {
          any_rank2 = true;
        }
      }
    }
  }

  if (valid_count == 0) {
    result.generic_rank = -1;
    return result;
  }

  // Rank-deficient 2D family (rank 0 or rank 1): return kCollapsed2D without J = 0 continuation.
  if (!any_rank2) {
    if (!any_rank1) {
      result.generic_rank = 0;
      const Eval2DJet1& rep = grid[static_cast<size_t>(best_valid_i * n + best_valid_j)];
      PoseCurve curve;
      curve.kind = PoseCurveKind::kCollapsed2D;
      curve.coverage.seed_grid_n = n;
      PoseCurveNode node;
      for (int k = 0; k < 3; k++) {
        node.param[k] = rep.param[k];
        node.sky[k] = rep.sky[k];
        node.tangent[k] = 0.0;
      }
      for (int k = 0; k < 9; k++) {
        node.pose[k] = rep.pose[k];
      }
      WorldToBody(node.pose, sun_dir, node.u);
      node.sky_speed = 0.0;
      node.sigma1 = rep.sigma1;
      node.sigma2 = rep.sigma2;
      node.arclength = 0.0;
      curve.nodes.push_back(node);
      CurveSegment seg;
      seg.start_idx = 0;
      seg.end_idx = 0;
      seg.kind = CurveSegmentKind::kPointImage;
      seg.corridor_open = EvaluateCorridorOpen(corridor, node.pose, sun_dir, refractive_index);
      curve.segments.push_back(seg);
      result.curves.push_back(std::move(curve));
      return result;
    }

    // Rank-1 collapsed 2D family: slice along the faster-moving free axis at the best valid coordinate.
    result.generic_rank = 1;
    PoseManifold slice = manifold;
    const bool keep_ax0 = (max_x1_norm >= max_x2_norm);
    const double pin_val = keep_ax0 ? grid_coord(best_valid_j, pax1) : grid_coord(best_valid_i, pax0);
    slice.axes[keep_ax0 ? ax1 : ax0] = PoseAxis::PinnedRad(pin_val);
    Family1DParams f1d;
    f1d.speed_tol = params.speed_tol;
    f1d.bisection_iters = params.bisection_iters;
    CausticTraceResult collapsed =
        TraceFamily1D(table, slots, slot_count, refractive_index, sun_dir, slice, corridor, f1d, stop);
    for (PoseCurve& c : collapsed.curves) {
      c.kind = PoseCurveKind::kCollapsed2D;
      c.coverage.seed_grid_n = n;
      result.coverage.closed = result.coverage.closed || c.coverage.closed;
      result.coverage.budget_truncated = result.coverage.budget_truncated || c.coverage.budget_truncated;
      result.curves.push_back(std::move(c));
    }
    result.stop_requested = collapsed.stop_requested;
    return result;
  }

  result.generic_rank = 2;

  // Stage 2: Harvest J = 0 seeds from sign-change edges, optical cliffs, and near-zero local minima.
  std::vector<Seed2D> seeds;
  int failed_seeds = 0;

  auto try_add_seed = [&](const double q_guess[2], double drift_cap) {
    const ProjectResult pr = ProjectOntoCaustic(table, slots, slot_count, refractive_index, sun_dir, manifold, ax0, ax1,
                                                q_guess, drift_cap, params);
    if (pr.outcome == ProjectOutcome::kConverged && pr.eval.grad_norm >= params.grad_tol) {
      Seed2D s;
      s.q[0] = WrapAxis(pr.q[0], pax0);
      s.q[1] = WrapAxis(pr.q[1], pax1);
      s.eval = pr.eval;
      s.priority = std::min(pr.eval.margin.min_eff_margin, 0.1) * std::min(pr.eval.grad_norm, 10.0);
      seeds.push_back(s);
    } else if (pr.outcome == ProjectOutcome::kSingularity || pr.outcome == ProjectOutcome::kDiverged) {
      failed_seeds++;
    }
  };

  auto bisect_edge_sign_change = [&](const double qa_in[2], double ja, const double qb_in[2]) {
    double qa[2] = { qa_in[0], qa_in[1] };
    double qb[2] = { qb_in[0], qb_in[1] };
    double fa = ja;
    bool hit_invalid = false;
    for (int it = 0; it < params.bisection_iters; it++) {
      const double qm[2] = { 0.5 * (qa[0] + qb[0]), 0.5 * (qa[1] + qb[1]) };
      const Eval2DJet1 em = Evaluate2DJet1(table, slots, slot_count, refractive_index, sun_dir, manifold, ax0, ax1, qm);
      if (!em.valid) {
        hit_invalid = true;
        break;
      }
      if (std::fabs(em.j_val) <= params.residual_tol) {
        qa[0] = qb[0] = qm[0];
        qa[1] = qb[1] = qm[1];
        break;
      }
      if ((fa <= 0.0 && em.j_val <= 0.0) || (fa > 0.0 && em.j_val > 0.0)) {
        qa[0] = qm[0];
        qa[1] = qm[1];
        fa = em.j_val;
      } else {
        qb[0] = qm[0];
        qb[1] = qm[1];
      }
    }
    if (!hit_invalid) {
      const double q_mid[2] = { 0.5 * (qa[0] + qb[0]), 0.5 * (qa[1] + qb[1]) };
      try_add_seed(q_mid, 0.5 * cell_diag);
    }
  };

  auto probe_cliff_edge = [&](const double q_valid[2], const Eval2DJet1& ev_valid, const double q_invalid[2]) {
    double qv[2] = { q_valid[0], q_valid[1] };
    double qi[2] = { q_invalid[0], q_invalid[1] };
    Eval2DJet1 e_cliff = ev_valid;
    for (int it = 0; it < 28; it++) {
      const double qm[2] = { 0.5 * (qv[0] + qi[0]), 0.5 * (qv[1] + qi[1]) };
      const Eval2DJet1 em = Evaluate2DJet1(table, slots, slot_count, refractive_index, sun_dir, manifold, ax0, ax1, qm);
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

  auto process_edge = [&](int i0, int j0, int i1, int j1, double dq0, double dq1) {
    const Eval2DJet1& a = grid[static_cast<size_t>(i0 * n + j0)];
    const Eval2DJet1& b = grid[static_cast<size_t>(i1 * n + j1)];
    const double qa[2] = { grid_coord(i0, pax0), grid_coord(j0, pax1) };
    const double qb[2] = { qa[0] + dq0, qa[1] + dq1 };
    if (a.valid && b.valid) {
      if (a.j_val * b.j_val <= 0.0) {
        bisect_edge_sign_change(qa, a.j_val, qb);
      }
    } else if (a.valid && !b.valid) {
      probe_cliff_edge(qa, a, qb);
    } else if (!a.valid && b.valid) {
      probe_cliff_edge(qb, b, qa);
    }
  };

  const int edges0 = pax0.periodic ? n : (n - 1);
  const int edges1 = pax1.periodic ? n : (n - 1);
  for (int i = 0; i < n; i++) {
    if (CheckStop(stop)) {
      result.stop_requested = true;
      result.coverage.budget_truncated = true;
      return result;
    }
    for (int j = 0; j < n; j++) {
      if (i < edges0) {
        process_edge(i, j, (i + 1) % n, j, step0, 0.0);
      }
      if (j < edges1) {
        process_edge(i, j, i, (j + 1) % n, 0.0, step1);
      }
    }
  }

  // Also probe near-zero valid local minima to catch small loops / tangent touches between grid lines.
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      const Eval2DJet1& c = grid[static_cast<size_t>(i * n + j)];
      if (!c.valid || c.sigma1 <= params.rank_tol || c.sigma2 >= 0.15 * c.sigma1) {
        continue;
      }
      const double abs_j = std::fabs(c.j_val);
      bool is_min = true;
      const int di[4] = { -1, 1, 0, 0 };
      const int dj[4] = { 0, 0, -1, 1 };
      for (int dir = 0; dir < 4; dir++) {
        int ni = i + di[dir];
        int nj = j + dj[dir];
        if (ni < 0 || ni >= n) {
          if (!pax0.periodic) {
            continue;
          }
          ni = (ni + n) % n;
        }
        if (nj < 0 || nj >= n) {
          if (!pax1.periodic) {
            continue;
          }
          nj = (nj + n) % n;
        }
        const Eval2DJet1& nb = grid[static_cast<size_t>(ni * n + nj)];
        if (nb.valid && std::fabs(nb.j_val) < abs_j) {
          is_min = false;
          break;
        }
      }
      if (is_min) {
        const double q[2] = { grid_coord(i, pax0), grid_coord(j, pax1) };
        try_add_seed(q, 0.75 * cell_diag);
      }
    }
  }

  // Prefer well-conditioned interior seeds first so branches trace smoothly outward to both ends.
  std::sort(seeds.begin(), seeds.end(), [](const Seed2D& a, const Seed2D& b) { return a.priority > b.priority; });
  result.coverage.failed_seeds = failed_seeds;

  // Stage 3: Bidirectional predictor-corrector continuation from each uncovered seed.
  for (const Seed2D& seed : seeds) {
    if (CheckStop(stop)) {
      result.stop_requested = true;
      result.coverage.budget_truncated = true;
      break;
    }
    if (static_cast<int>(result.curves.size()) >= params.max_curves) {
      result.coverage.budget_truncated = true;
      break;
    }
    if (IsSeedCoveredByCurves(seed, ax0, ax1, manifold, result.curves, params.dedup_tol)) {
      continue;
    }

    const HalfBranch2D forward = TraceHalfBranch2D(table, slots, slot_count, refractive_index, sun_dir, manifold, ax0,
                                                   ax1, seed.q, seed.eval, +1, true, corridor, params, stop);
    HalfBranch2D backward;
    if (!forward.closed && !forward.stop_requested) {
      backward = TraceHalfBranch2D(table, slots, slot_count, refractive_index, sun_dir, manifold, ax0, ax1, seed.q,
                                   seed.eval, -1, false, corridor, params, stop);
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
