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

// Condition-aware floor separating an identically constant (rank-0) map (~1e-15 rounding noise,
// amplified by 1/sqrt(margin) near grazing Snell cliffs) from a small well-conditioned rank-1/2 map.
inline double NumericalZeroSigmaFloor(double min_effective_margin) {
  return 1e-13 / std::sqrt(std::max(min_effective_margin, 1e-8));
}

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
// Shared Segment Builder
// =================================================================================================

// Builds contiguous CurveSegments over [0, nodes.size() - 1] from per-node stationarity and
// per-edge corridor flags. Invariant: when nodes.size() >= 2, edge_corridor_open.size() ==
// nodes.size() - 1, where edge_corridor_open[i] is the corridor state of edge (nodes[i], nodes[i+1]).
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

// =================================================================================================
// 1D Family Tracing (shared by TraceFamily1D and projected rank-1 collapsed 2D manifolds)
// =================================================================================================

struct Eval1D {
  bool valid = false;
  OpticalMarginVerdict margin;
  double param[3]{};
  double tangent[3]{};
  double pose[9]{};
  double u[3]{};
  double sky[3]{};
  double dx[3]{};
  double sky_speed = 0.0;
  double internal_disc[kMaxFaceCount]{};
  bool corridor_open = true;
};

Eval1D Evaluate1DAlongTangent(const FaceNormalTable& table, const int* slots, int slot_count, double refractive_index,
                              const double sun_dir[3], const double param_rad[3], const double tangent_dir[3],
                              Corridor* corridor, bool eval_corridor) {
  Eval1D out;
  for (int k = 0; k < 3; k++) {
    out.param[k] = param_rad[k];
    out.tangent[k] = tangent_dir[k];
  }
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
  for (int k = 0; k < 3; k++) {
    node.param[k] = e.param[k];
    node.u[k] = e.u[k];
    node.sky[k] = e.sky[k];
    node.tangent[k] = e.tangent[k];
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

struct Fragment1D {
  PoseCurve curve;
  std::vector<bool> node_stationary;
  std::vector<bool> edge_corridor_open;
  bool starts_at_seam = false;
  bool ends_at_seam = false;
};

template <class EvalFn>
Eval1D BisectOpticalBoundary1DFn(const EvalFn& eval_fn, double t_valid, double t_invalid, int iters, double* t_out,
                                 int* failing_index_out) {
  double tv = t_valid;
  double ti = t_invalid;
  Eval1D best = eval_fn(tv, true);
  int fail_idx = -1;
  {
    const Eval1D ei = eval_fn(ti, false);
    fail_idx = ei.margin.failing_index;
  }
  for (int it = 0; it < iters; it++) {
    const double tm = 0.5 * (tv + ti);
    const Eval1D em = eval_fn(tm, false);
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
  best = eval_fn(tv, true);
  *t_out = tv;
  *failing_index_out = fail_idx;
  return best;
}

// Probes a near-grazing valid 1D step [t_a, t_b] for an interior invalid optical gap (e.g. a
// narrow negative dip of incidence cosine or Snell discriminant missed by both endpoints).
// Uses quartile bracketing + golden-section minimization of min_effective_margin, exiting early
// as soon as any invalid interior sample is found.
template <class EvalFn>
bool FindInteriorOpticalGap1D(const EvalFn& eval_fn, double t_a, double m_a, double t_b, double m_b, double margin_lip,
                              double* t_invalid_out) {
  const double step = t_b - t_a;
  if (!(step > 1e-13) || m_a + m_b > margin_lip * step) {
    return false;
  }

  double ts[5] = { t_a, t_a + 0.25 * step, t_a + 0.5 * step, t_a + 0.75 * step, t_b };
  double ms[5] = { m_a, 0.0, 0.0, 0.0, m_b };
  int best_idx = (m_a <= m_b) ? 0 : 4;
  double best_m = std::min(m_a, m_b);
  for (int k = 1; k <= 3; k++) {
    const Eval1D ek = eval_fn(ts[k], false);
    if (!ek.valid) {
      *t_invalid_out = ts[k];
      return true;
    }
    ms[k] = ek.margin.min_effective_margin;
    if (ms[k] < best_m) {
      best_m = ms[k];
      best_idx = k;
    }
  }

  // Only run golden-section refinement when an interior point dips below the linear chord or the
  // minimum margin is close enough to zero that a local trough could cross zero.
  double lo = (best_idx > 0) ? ts[best_idx - 1] : ts[0];
  double hi = (best_idx < 4) ? ts[best_idx + 1] : ts[4];
  if (best_m > 0.5 * margin_lip * (hi - lo)) {
    return false;
  }

  constexpr double kInvPhi = 0.6180339887498949;
  double c = hi - kInvPhi * (hi - lo);
  double d = lo + kInvPhi * (hi - lo);
  Eval1D ec = eval_fn(c, false);
  if (!ec.valid) {
    *t_invalid_out = c;
    return true;
  }
  Eval1D ed = eval_fn(d, false);
  if (!ed.valid) {
    *t_invalid_out = d;
    return true;
  }
  for (int it = 0; it < 24; it++) {
    if (ec.margin.min_effective_margin < ed.margin.min_effective_margin) {
      hi = d;
      d = c;
      ed = ec;
      c = hi - kInvPhi * (hi - lo);
      ec = eval_fn(c, false);
      if (!ec.valid) {
        *t_invalid_out = c;
        return true;
      }
    } else {
      lo = c;
      c = d;
      ec = ed;
      d = lo + kInvPhi * (hi - lo);
      ed = eval_fn(d, false);
      if (!ed.valid) {
        *t_invalid_out = d;
        return true;
      }
    }
  }
  const double t_mid = 0.5 * (lo + hi);
  const Eval1D em = eval_fn(t_mid, false);
  if (!em.valid) {
    *t_invalid_out = t_mid;
    return true;
  }
  return false;
}

// Traces a single contiguous valid sub-interval [t_start, t_end]. If a narrow interior optical gap
// is discovered during adaptive stepping (either via an invalid trial endpoint or an interior
// grazing-margin minimum), finishes this fragment at the left edge of the gap and returns
// `gap_hit = true` so the caller can locate the far edge and resume.
template <class EvalFn>
void Trace1DSingleFragment(const EvalFn& eval_fn, int slot_count, int free_ax, double t_start, double t_end,
                           bool is_periodic_full, bool left_is_domain, bool right_is_domain, int left_fail_idx,
                           int right_fail_idx, double margin_lip, bool track_corridor, const Family1DParams& params,
                           const DiscoveryStop* stop, Fragment1D* frag, bool* stop_requested, bool* gap_hit,
                           double* gap_invalid_t, double* gap_step_hint, bool* has_gap_resume_valid,
                           double* gap_resume_valid_t) {
  *gap_hit = false;
  *gap_invalid_t = t_end;
  *gap_step_hint = params.min_step;
  *has_gap_resume_valid = false;
  *gap_resume_valid_t = t_end;

  PoseCurve* curve = &frag->curve;
  curve->kind = PoseCurveKind::kFamily1D;
  curve->coverage.seed_grid_n = params.scan_samples;
  // Never mark closed prematurely: closed is set true ONLY after reaching t_end without truncation,
  // cancellation, or an interior optical gap.
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
  frag->starts_at_seam = is_periodic_full;

  if (!is_periodic_full) {
    CurveEvent ev;
    CopyNodeToEvent(curve->nodes.front(),
                    left_is_domain ? CurveEventKind::kDomainBoundary : CurveEventKind::kOpticalBoundary,
                    left_is_domain ? free_ax : left_fail_idx, &ev);
    curve->events.push_back(ev);
  }

  // Retain the last moving sample across any number of intervening near-zero nodes so fine step
  // sizes resolving an isolated stationary point (e.g. parhelion minimum) never suppress the event.
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
      if (!e_next.valid) {
        const double next_step = std::max(params.min_step, step * 0.5);
        if (!(next_step < step)) {
          break;
        }
        step = next_step;
        t_next = t_curr + step;
        e_next = eval_fn(t_next, track_corridor);
        shrunk = true;
        continue;
      }
      if (e_curr.sky_speed > params.speed_tol && e_next.sky_speed > params.speed_tol) {
        const double d_angle = so3::AngleBetween(e_curr.dx, e_next.dx);
        const double sky_step = so3::AngleBetween(e_curr.sky, e_next.sky);
        if (sky_step > params.max_turning_angle || (d_angle > params.max_turning_angle && d_angle < 2.5)) {
          const double next_step = std::max(params.min_step, step * 0.5);
          if (!(next_step < step)) {
            break;
          }
          step = next_step;
          t_next = t_curr + step;
          e_next = eval_fn(t_next, track_corridor);
          shrunk = true;
          continue;
        }
      }
      break;
    }

    if (*stop_requested) {
      break;
    }

    if (!e_next.valid) {
      // Discovered a narrow interior optical gap missed by the coarse scan: finish this fragment
      // cleanly at the bisected left edge and signal caller to locate the far edge and resume.
      double t_bnd = t_curr;
      int fail_idx = -1;
      const Eval1D e_bnd =
          BisectOpticalBoundary1DFn(eval_fn, t_curr, t_next, params.bisection_iters, &t_bnd, &fail_idx);
      const double ds = t_bnd - t_curr;
      if (ds > 1e-13) {
        arclen += ds;
        push_node(e_bnd, arclen, e_curr.corridor_open);
      }
      CurveEvent ev;
      CopyNodeToEvent(curve->nodes.back(), CurveEventKind::kOpticalBoundary, fail_idx, &ev);
      curve->events.push_back(ev);
      *gap_hit = true;
      *gap_invalid_t = t_next;
      *gap_step_hint = step;
      break;
    }

    // Even when both step endpoints are valid, a near-grazing step can bridge a narrow negative
    // dip in incidence cosine or Snell discriminant. Probe for an interior invalid minimum before
    // accepting the step so periodic traces never close across an invalid interval.
    double t_gap_inv = t_next;
    if (FindInteriorOpticalGap1D(eval_fn, t_curr, e_curr.margin.min_effective_margin, t_next,
                                 e_next.margin.min_effective_margin, margin_lip, &t_gap_inv)) {
      double t_bnd = t_curr;
      int fail_idx = -1;
      const Eval1D e_bnd =
          BisectOpticalBoundary1DFn(eval_fn, t_curr, t_gap_inv, params.bisection_iters, &t_bnd, &fail_idx);
      const double ds = t_bnd - t_curr;
      if (ds > 1e-13) {
        arclen += ds;
        push_node(e_bnd, arclen, e_curr.corridor_open);
      }
      CurveEvent ev;
      CopyNodeToEvent(curve->nodes.back(), CurveEventKind::kOpticalBoundary, fail_idx, &ev);
      curve->events.push_back(ev);
      *gap_hit = true;
      *gap_invalid_t = t_gap_inv;
      *gap_step_hint = std::min(step, t_next - t_gap_inv);
      *has_gap_resume_valid = true;
      *gap_resume_valid_t = t_next;
      break;
    }

    // 1. Stationary point detection (||dx/dt|| = 0): works independently of how many fine steps
    //    resolve the near-zero neighborhood, while rejecting flat constant-image plateaus via a
    //    scale-invariant two-sided speed check around the bisected minimum.
    if (e_next.sky_speed > params.speed_tol) {
      if (have_last_moving && so3::Dot3(last_moving_e.dx, e_next.dx) < 0.0) {
        double ref[3] = { e_next.dx[0] - last_moving_e.dx[0], e_next.dx[1] - last_moving_e.dx[1],
                          e_next.dx[2] - last_moving_e.dx[2] };
        const double rn = so3::Norm3(ref);
        if (rn > 0.0) {
          ref[0] /= rn;
          ref[1] /= rn;
          ref[2] /= rn;
          double ta = last_moving_t;
          double tb = t_next;
          double fa = so3::Dot3(last_moving_e.dx, ref);
          for (int it = 0; it < params.bisection_iters; it++) {
            const double tm = 0.5 * (ta + tb);
            const Eval1D em = eval_fn(tm, false);
            if (!em.valid) {
              break;
            }
            const double fm = so3::Dot3(em.dx, ref);
            if ((fa <= 0.0 && fm <= 0.0) || (fa > 0.0 && fm > 0.0)) {
              ta = tm;
              fa = fm;
            } else {
              tb = tm;
            }
          }
          const double t_stat = 0.5 * (ta + tb);
          const Eval1D e_stat = eval_fn(t_stat, false);
          if (e_stat.valid && e_stat.sky_speed <= params.stationary_tol) {
            const Eval1D e_left = eval_fn(0.5 * (last_moving_t + t_stat), false);
            const Eval1D e_right = eval_fn(0.5 * (t_stat + t_next), false);
            const double min_end_speed = std::min(last_moving_e.sky_speed, e_next.sky_speed);
            const bool isolated_min = e_left.valid && e_right.valid && e_left.sky_speed > 0.1 * min_end_speed &&
                                      e_right.sky_speed > 0.1 * min_end_speed &&
                                      so3::Dot3(e_left.dx, ref) * so3::Dot3(e_right.dx, ref) < 0.0;
            if (isolated_min) {
              const PoseCurveNode n_stat = NodeFromEval1D(e_stat, last_moving_arclen + (t_stat - last_moving_t));
              CurveEvent ev;
              CopyNodeToEvent(n_stat, CurveEventKind::kStationaryPoint, -1, &ev);
              curve->events.push_back(ev);
            }
          }
        }
      }
      have_last_moving = true;
      last_moving_t = t_next;
      last_moving_e = e_next;
      last_moving_arclen = arclen + (t_next - t_curr);
    }

    // 2. Internal TIR onset for each internal reflection step k in [1, slot_count - 2].
    for (int k = 1; k < slot_count - 1; k++) {
      const double da = e_curr.internal_disc[k];
      const double db = e_next.internal_disc[k];
      if ((da > 0.0) != (db > 0.0)) {
        double ta = t_curr;
        double tb = t_next;
        const bool pos_a = da > 0.0;
        for (int it = 0; it < params.bisection_iters; it++) {
          const double tm = 0.5 * (ta + tb);
          const Eval1D em = eval_fn(tm, false);
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
        const Eval1D e_tir = eval_fn(t_tir, false);
        if (e_tir.valid) {
          const PoseCurveNode n_tir = NodeFromEval1D(e_tir, arclen + (t_tir - t_curr));
          CurveEvent ev;
          CopyNodeToEvent(n_tir, CurveEventKind::kTirOnset, k, &ev);
          curve->events.push_back(ev);
        }
      }
    }

    // 3. Corridor cutoff (A_P crossing 0), inserting the exact cutoff node without stopping.
    if (track_corridor && e_curr.corridor_open != e_next.corridor_open) {
      double ta = t_curr;
      double tb = t_next;
      const bool open_a = e_curr.corridor_open;
      for (int it = 0; it < params.bisection_iters; it++) {
        const double tm = 0.5 * (ta + tb);
        const Eval1D em = eval_fn(tm, true);
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
      const Eval1D e_cut = eval_fn(t_cut, true);
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
        h = shrunk ? std::max(params.min_step, step) : std::min(params.max_step, step * 1.25);
        continue;
      }
    }

    arclen += (t_next - t_curr);
    push_node(e_next, arclen, e_curr.corridor_open);
    t_curr = t_next;
    e_curr = e_next;
    h = shrunk ? std::max(params.min_step, step) : std::min(params.max_step, step * 1.25);
  }

  const bool reached_end =
      !(*gap_hit) && !curve->coverage.budget_truncated && !(*stop_requested) && std::fabs(t_curr - t_end) <= 1e-9;
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
      // Reached periodic seam at t_end after recovering from an interior gap on a full-circle scan.
      frag->ends_at_seam = true;
    }
  }

  std::sort(curve->events.begin(), curve->events.end(),
            [](const CurveEvent& a, const CurveEvent& b) { return a.arclength < b.arclength; });
  BuildSegments(curve->nodes, frag->node_stationary, frag->edge_corridor_open, params.speed_tol, &curve->segments);
}

// Stitches two 1D fragments across a periodic seam (back_frag ending at max_val, front_frag
// starting at min_val == max_val) into one continuous open PoseCurve.
PoseCurve StitchPeriodicSeam1D(const Fragment1D& back_frag, const Fragment1D& front_frag, double speed_tol) {
  PoseCurve stitched;
  stitched.kind = back_frag.curve.kind;
  stitched.coverage.seed_grid_n = back_frag.curve.coverage.seed_grid_n;
  stitched.coverage.closed = false;
  stitched.coverage.budget_truncated =
      back_frag.curve.coverage.budget_truncated || front_frag.curve.coverage.budget_truncated;

  const double back_len = back_frag.curve.nodes.back().arclength;
  stitched.nodes = back_frag.curve.nodes;
  std::vector<bool> node_stationary = back_frag.node_stationary;
  std::vector<bool> edge_corridor_open = back_frag.edge_corridor_open;
  stitched.events = back_frag.curve.events;

  // Skip front_frag.nodes[0] which coincides with back_frag.nodes.back() at the periodic seam.
  for (size_t i = 1; i < front_frag.curve.nodes.size(); i++) {
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
  std::sort(stitched.events.begin(), stitched.events.end(),
            [](const CurveEvent& a, const CurveEvent& b) { return a.arclength < b.arclength; });
  BuildSegments(stitched.nodes, node_stationary, edge_corridor_open, speed_tol, &stitched.segments);
  return stitched;
}

// Traces an interval [t_start, t_end], automatically locating the far side of any narrow interior
// optical gaps missed by the coarse scan and resuming until t_end.
template <class EvalFn>
void Trace1DIntervalResumingGaps(const EvalFn& eval_fn, int slot_count, int free_ax, double t_start, double t_end,
                                 bool is_periodic_full, bool left_is_domain, bool right_is_domain, int left_fail_idx,
                                 int right_fail_idx, double margin_lip, double max_probe_step, bool track_corridor,
                                 const Family1DParams& params, const DiscoveryStop* stop,
                                 std::vector<Fragment1D>* fragments, bool* stop_requested) {
  double cur_start = t_start;
  bool cur_periodic_full = is_periodic_full;
  bool cur_left_domain = left_is_domain;
  int cur_left_fail = left_fail_idx;
  // On a full-circle scan split by an interior gap, the final fragment reaching t_end meets the
  // periodic seam (not a domain or optical boundary); right_fail_idx == -1 && !right_is_domain
  // signals that seam arrival.
  const bool final_right_domain = is_periodic_full ? false : right_is_domain;
  const int final_right_fail = is_periodic_full ? -1 : right_fail_idx;

  while (cur_start < t_end - 1e-13) {
    if (CheckStop(stop)) {
      *stop_requested = true;
      break;
    }
    Fragment1D frag;
    bool gap_hit = false;
    double gap_inv_t = t_end;
    double gap_step_hint = params.min_step;
    bool has_resume_valid = false;
    double gap_resume_valid_t = t_end;
    Trace1DSingleFragment(eval_fn, slot_count, free_ax, cur_start, t_end, cur_periodic_full, cur_left_domain,
                          final_right_domain, cur_left_fail, final_right_fail, margin_lip, track_corridor, params, stop,
                          &frag, stop_requested, &gap_hit, &gap_inv_t, &gap_step_hint, &has_resume_valid,
                          &gap_resume_valid_t);
    if (!frag.curve.nodes.empty()) {
      fragments->push_back(std::move(frag));
    }
    if (!gap_hit || *stop_requested) {
      break;
    }

    // Locate the far (right) boundary of the narrow gap by probing forward up to t_end, or reuse
    // the already-valid right endpoint when the gap was caught inside a valid step.
    double t_inv = gap_inv_t;
    double t_probe = gap_resume_valid_t;
    bool found_resume = has_resume_valid;
    if (!found_resume) {
      double p_step = std::max(params.min_step, gap_step_hint);
      t_probe = t_inv;
      while (t_probe < t_end - 1e-13) {
        if (CheckStop(stop)) {
          *stop_requested = true;
          break;
        }
        t_probe = std::min(t_end, t_probe + p_step);
        const Eval1D ep = eval_fn(t_probe, false);
        if (ep.valid) {
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

    double t_resume = t_probe;
    int resume_fail_idx = -1;
    BisectOpticalBoundary1DFn(eval_fn, t_probe, t_inv, params.bisection_iters, &t_resume, &resume_fail_idx);
    cur_start = t_resume;
    cur_periodic_full = false;
    cur_left_domain = false;
    cur_left_fail = resume_fail_idx;
  }
}

// Exact dual of FindInteriorOpticalGap1D: probes a near-grazing invalid 1D interval [t_a, t_b]
// (m_a <= 0, m_b <= 0) for a narrow interior valid branch missed by both scan endpoints.
template <class EvalFn>
bool FindInteriorValidPoint1D(const EvalFn& eval_fn, double t_a, double m_a, double t_b, double m_b, double margin_lip,
                              double* t_valid_out) {
  const double step = t_b - t_a;
  if (!(step > 1e-13) || m_a + m_b + margin_lip * step <= 0.0) {
    return false;
  }

  double ts[5] = { t_a, t_a + 0.25 * step, t_a + 0.5 * step, t_a + 0.75 * step, t_b };
  double ms[5] = { m_a, 0.0, 0.0, 0.0, m_b };
  int best_idx = (m_a >= m_b) ? 0 : 4;
  double best_m = std::max(m_a, m_b);
  for (int k = 1; k <= 3; k++) {
    const Eval1D ek = eval_fn(ts[k], false);
    if (ek.valid) {
      *t_valid_out = ts[k];
      return true;
    }
    ms[k] = ek.margin.min_effective_margin;
    if (ms[k] > best_m) {
      best_m = ms[k];
      best_idx = k;
    }
  }

  double lo = (best_idx > 0) ? ts[best_idx - 1] : ts[0];
  double hi = (best_idx < 4) ? ts[best_idx + 1] : ts[4];
  if (best_m + 0.5 * margin_lip * (hi - lo) <= 0.0) {
    return false;
  }

  constexpr double kInvPhi = 0.6180339887498949;
  double c = hi - kInvPhi * (hi - lo);
  double d = lo + kInvPhi * (hi - lo);
  Eval1D ec = eval_fn(c, false);
  if (ec.valid) {
    *t_valid_out = c;
    return true;
  }
  Eval1D ed = eval_fn(d, false);
  if (ed.valid) {
    *t_valid_out = d;
    return true;
  }
  for (int it = 0; it < 24; it++) {
    if (ec.margin.min_effective_margin > ed.margin.min_effective_margin) {
      hi = d;
      d = c;
      ed = ec;
      c = hi - kInvPhi * (hi - lo);
      ec = eval_fn(c, false);
      if (ec.valid) {
        *t_valid_out = c;
        return true;
      }
    } else {
      lo = c;
      c = d;
      ec = ed;
      d = lo + kInvPhi * (hi - lo);
      ed = eval_fn(d, false);
      if (ed.valid) {
        *t_valid_out = d;
        return true;
      }
    }
  }
  const double t_mid = 0.5 * (lo + hi);
  const Eval1D em = eval_fn(t_mid, false);
  if (em.valid) {
    *t_valid_out = t_mid;
    return true;
  }
  return false;
}

// Shared 1D scan + interval tracing driver for TraceFamily1D and Euler-pole 1D manifolds.
template <class EvalFn>
CausticTraceResult Trace1DWithEvaluator(const PoseAxis& pax, int free_ax, int slot_count, double margin_lip,
                                        const EvalFn& eval_fn, bool track_corridor, const Family1DParams& params,
                                        const DiscoveryStop* stop) {
  CausticTraceResult result;
  result.coverage.seed_grid_n = params.scan_samples;

  const int n = params.scan_samples;
  const int sample_cnt = pax.periodic ? n : (n + 1);
  const double span = pax.Span();
  const double scan_spacing = span / static_cast<double>(n);
  const double max_probe_step = std::max(params.min_step * 4.0, 0.25 * scan_spacing);

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
    const double t = pax.min_val + span * static_cast<double>(i) / static_cast<double>(n);
    const Eval1D e = eval_fn(t, false);
    scan_margin[static_cast<size_t>(i)] = e.margin.min_effective_margin;
    if (e.valid) {
      valid[static_cast<size_t>(i)] = true;
      valid_cnt++;
      if (e.sky_speed > NumericalZeroSigmaFloor(e.margin.min_effective_margin)) {
        any_nonzero_speed = true;
      }
    }
  }

  // Probe near-grazing invalid scan intervals for narrow valid islands missed by coarse samples.
  struct NarrowIsland1D {
    double t_left = 0.0;
    double t_right = 0.0;
    int left_fail_idx = -1;
    int right_fail_idx = -1;
  };
  std::vector<NarrowIsland1D> narrow_islands;
  const int interval_cnt = pax.periodic ? n : n;
  for (int i = 0; i < interval_cnt; i++) {
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
    if (FindInteriorValidPoint1D(eval_fn, t_a, scan_margin[static_cast<size_t>(i)], t_b,
                                 scan_margin[static_cast<size_t>(i_next)], margin_lip, &t_val)) {
      const Eval1D ev_v = eval_fn(t_val, false);
      if (ev_v.valid && ev_v.sky_speed > NumericalZeroSigmaFloor(ev_v.margin.min_effective_margin)) {
        any_nonzero_speed = true;
      }
      NarrowIsland1D isl;
      BisectOpticalBoundary1DFn(eval_fn, t_val, t_a, params.bisection_iters, &isl.t_left, &isl.left_fail_idx);
      BisectOpticalBoundary1DFn(eval_fn, t_val, t_b, params.bisection_iters, &isl.t_right, &isl.right_fail_idx);
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

  // Periodic full scan: every coarse sample is valid. Trace [min_val, max_val]; if an interior
  // narrow optical gap splits the circle, stitch the seam-adjacent end fragments together.
  if (pax.periodic && valid_cnt == sample_cnt) {
    std::vector<Fragment1D> fragments;
    bool stopped = false;
    Trace1DIntervalResumingGaps(eval_fn, slot_count, free_ax, pax.min_val, pax.max_val, true, false, false, -1, -1,
                                margin_lip, max_probe_step, track_corridor, params, stop, &fragments, &stopped);
    result.stop_requested = result.stop_requested || stopped;

    if (fragments.size() >= 2 && fragments.front().starts_at_seam && fragments.back().ends_at_seam) {
      PoseCurve stitched = StitchPeriodicSeam1D(fragments.back(), fragments.front(), params.speed_tol);
      result.coverage.budget_truncated = result.coverage.budget_truncated || stitched.coverage.budget_truncated;
      result.curves.push_back(std::move(stitched));
      for (size_t k = 1; k + 1 < fragments.size(); k++) {
        result.coverage.budget_truncated =
            result.coverage.budget_truncated || fragments[k].curve.coverage.budget_truncated;
        result.curves.push_back(std::move(fragments[k].curve));
      }
    } else {
      for (Fragment1D& f : fragments) {
        result.coverage.closed = result.coverage.closed || f.curve.coverage.closed;
        result.coverage.budget_truncated = result.coverage.budget_truncated || f.curve.coverage.budget_truncated;
        result.curves.push_back(std::move(f.curve));
      }
    }
    return result;
  }

  // Trace any narrow valid islands discovered between adjacent invalid coarse samples.
  for (const NarrowIsland1D& isl : narrow_islands) {
    if (CheckStop(stop)) {
      result.stop_requested = true;
      result.coverage.budget_truncated = true;
      return result;
    }
    std::vector<Fragment1D> fragments;
    bool stopped = false;
    Trace1DIntervalResumingGaps(eval_fn, slot_count, free_ax, isl.t_left, isl.t_right, false, false, false,
                                isl.left_fail_idx, isl.right_fail_idx, margin_lip, max_probe_step, track_corridor,
                                params, stop, &fragments, &stopped);
    result.stop_requested = result.stop_requested || stopped;
    for (Fragment1D& f : fragments) {
      result.coverage.budget_truncated = result.coverage.budget_truncated || f.curve.coverage.budget_truncated;
      result.curves.push_back(std::move(f.curve));
    }
  }
  if (valid_cnt == 0) {
    return result;
  }

  // Open runs (or periodic with at least one invalid coarse sample: start from an invalid index so
  // no valid run is split across the periodic seam).
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
      BisectOpticalBoundary1DFn(eval_fn, t_left, t_inv, params.bisection_iters, &t_left, &left_fail_idx);
    }

    double t_right = t_at_step(run_end_step);
    bool right_is_domain = false;
    int right_fail_idx = -1;
    if (!pax.periodic && run_end_step == sample_cnt - 1) {
      right_is_domain = true;
    } else {
      const double t_inv = t_at_step(run_end_step + 1);
      BisectOpticalBoundary1DFn(eval_fn, t_right, t_inv, params.bisection_iters, &t_right, &right_fail_idx);
    }

    std::vector<Fragment1D> fragments;
    bool stopped = false;
    Trace1DIntervalResumingGaps(eval_fn, slot_count, free_ax, t_left, t_right, false, left_is_domain, right_is_domain,
                                left_fail_idx, right_fail_idx, margin_lip, max_probe_step, track_corridor, params, stop,
                                &fragments, &stopped);
    result.stop_requested = result.stop_requested || stopped;
    for (Fragment1D& f : fragments) {
      result.coverage.budget_truncated = result.coverage.budget_truncated || f.curve.coverage.budget_truncated;
      result.curves.push_back(std::move(f.curve));
    }
  }

  return result;
}

// =================================================================================================
// 2D Caustic Continuation (J = 0 via Jet<2> pre-scan + Jet2<2> predictor-corrector)
// =================================================================================================

struct Eval2DJet1 {
  bool valid = false;
  OpticalMarginVerdict margin;
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
  EulerPose(ang[0], ang[1], ang[2], pose_j);
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
  out.margin = ClassifyChainMargins(chain_ok, domain, slot_count);
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
  OpticalMarginVerdict margin;
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
  // Snell-desingularized J_reg = entry_disc * exit_disc * J (1 * J for 1-face reflection) and its
  // exact gradient grad J_reg = w * grad J + J * grad w. Shares the exact zero set of J on the
  // valid domain while keeping ||grad J_reg|| = O(1) as exit_disc -> 0 (where ||grad J|| = O(1/d)).
  double j_reg = 0.0;
  double grad_j_reg[2]{};
  double grad_reg_norm = 0.0;
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
  EulerPose(ang[0], ang[1], ang[2], pose_j);
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
  out.margin = ClassifyChainMargins(chain_ok, domain, slot_count);
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

  // Desingularize by w = entry_disc * exit_disc for transmission paths (1 for 1-face reflection),
  // using the exact Jet2<2> discriminants already computed by TracePathChain. On J = 0 near a
  // grazing exit cliff d -> 0, ||grad J|| scales as O(1 / d) while ||grad(w * J)|| stays O(1).
  double w_val = 1.0;
  double grad_w[2] = { 0.0, 0.0 };
  if (slot_count >= 2) {
    const Jet2<2> w_jet = diag.discriminant[0] * diag.discriminant[slot_count - 1];
    w_val = w_jet.a;
    grad_w[0] = w_jet.v[0];
    grad_w[1] = w_jet.v[1];
  }
  out.j_reg = w_val * out.j_val;
  out.grad_j_reg[0] = w_val * out.grad_j[0] + out.j_val * grad_w[0];
  out.grad_j_reg[1] = w_val * out.grad_j[1] + out.j_val * grad_w[1];
  out.grad_reg_norm = std::hypot(out.grad_j_reg[0], out.grad_j_reg[1]);
  if (!std::isfinite(out.j_reg) || !std::isfinite(out.grad_reg_norm)) {
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
  kCliffUlpLimit,
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

// Safeguarded orthogonal 1D Newton projection onto J(q) = 0 from q_start (unwrapped). Uses the
// Snell-desingularized residual J_reg = entry_disc * exit_disc * J for step direction and
// backtracking, while enforcing raw |J| <= max(residual_tol, 1e-11) whenever allow_ulp_cliff is
// false (returning kCliffUlpLimit if Newton reaches float64 parameter-space ULP precision near a
// grazing cliff where raw |J| cannot be represented below 1e-11).
ProjectResult ProjectOntoCaustic(const FaceNormalTable& table, const int* slots, int slot_count,
                                 double refractive_index, const double sun_dir[3], const PoseManifold& manifold,
                                 int ax0, int ax1, const double q_start[2], double max_total_drift,
                                 const CausticContinuationParams& params, bool allow_ulp_cliff = false) {
  ProjectResult res;
  res.q[0] = q_start[0];
  res.q[1] = q_start[1];
  const PoseAxis& pax0 = manifold.axes[ax0];
  const PoseAxis& pax1 = manifold.axes[ax1];

  if (!InAxisBounds(res.q[0], pax0)) {
    res.outcome = ProjectOutcome::kDomainCrossed;
    res.crossed_axis = ax0;
    return res;
  }
  if (!InAxisBounds(res.q[1], pax1)) {
    res.outcome = ProjectOutcome::kDomainCrossed;
    res.crossed_axis = ax1;
    return res;
  }
  res.eval =
      Evaluate2DJet2(table, slots, slot_count, refractive_index, sun_dir, manifold, ax0, ax1, res.q, nullptr, false);
  if (!res.eval.valid) {
    res.outcome = ProjectOutcome::kOpticalInvalid;
    res.failing_margin = res.eval.margin.failing_index;
    return res;
  }
  if (res.eval.generator_cross < params.pole_tol) {
    res.outcome = ProjectOutcome::kPoleHit;
    res.crossed_axis = PoseManifold::kAxisZenith;
    return res;
  }

  const double q_scale = std::max(1.0, std::hypot(res.q[0], res.q[1]));
  const double raw_ulp_cap = std::max(params.residual_tol, kRawInteriorJCap);
  auto ulp_outcome = [&](double abs_j) {
    return (abs_j <= raw_ulp_cap || allow_ulp_cliff) ? ProjectOutcome::kConverged : ProjectOutcome::kCliffUlpLimit;
  };

  for (int iter = 0; iter <= params.max_newton_iters; iter++) {
    const double abs_j = std::fabs(res.eval.j_val);
    const double abs_j_reg = std::fabs(res.eval.j_reg);
    const double param_dist =
        (res.eval.grad_reg_norm > 0.0) ? (abs_j_reg / res.eval.grad_reg_norm) : std::numeric_limits<double>::infinity();
    const bool converged_ulp = (param_dist <= 5e-15 * q_scale);

    if (abs_j <= params.residual_tol) {
      res.outcome = ProjectOutcome::kConverged;
      res.iterations = iter;
      return res;
    }
    if (iter == params.max_newton_iters) {
      if (converged_ulp) {
        res.outcome = ulp_outcome(abs_j);
        res.iterations = iter;
        return res;
      }
      break;
    }
    if (res.eval.grad_reg_norm < params.grad_tol && res.eval.grad_norm < params.grad_tol) {
      res.outcome = ProjectOutcome::kSingularity;
      res.iterations = iter;
      return res;
    }
    const double scale = -res.eval.j_reg / (res.eval.grad_reg_norm * res.eval.grad_reg_norm);
    const double dq0 = scale * res.eval.grad_j_reg[0];
    const double dq1 = scale * res.eval.grad_j_reg[1];
    const double step_norm = std::hypot(dq0, dq1);
    if (!std::isfinite(step_norm)) {
      res.outcome = ProjectOutcome::kDiverged;
      res.iterations = iter;
      return res;
    }
    if (step_norm <= 2.5e-16 * q_scale && converged_ulp) {
      res.outcome = ulp_outcome(abs_j);
      res.iterations = iter;
      return res;
    }
    if (step_norm > std::max(4.0 * max_total_drift, 0.25)) {
      res.outcome = ProjectOutcome::kDiverged;
      res.iterations = iter;
      return res;
    }

    // Backtracking line search on the Snell-desingularized residual |J_reg|.
    bool step_accepted = false;
    ProjectOutcome last_fail = ProjectOutcome::kDiverged;
    int last_axis = -1;
    int last_margin = -1;
    double alpha = 1.0;
    for (int bt = 0; bt < 8; bt++) {
      const double q_try[2] = { res.q[0] + alpha * dq0, res.q[1] + alpha * dq1 };
      if (std::hypot(q_try[0] - q_start[0], q_try[1] - q_start[1]) > max_total_drift) {
        last_fail = ProjectOutcome::kDiverged;
        alpha *= 0.5;
        continue;
      }
      if (!InAxisBounds(q_try[0], pax0)) {
        last_fail = ProjectOutcome::kDomainCrossed;
        last_axis = ax0;
        alpha *= 0.5;
        continue;
      }
      if (!InAxisBounds(q_try[1], pax1)) {
        last_fail = ProjectOutcome::kDomainCrossed;
        last_axis = ax1;
        alpha *= 0.5;
        continue;
      }
      const Eval2DJet2 e_try = Evaluate2DJet2(table, slots, slot_count, refractive_index, sun_dir, manifold, ax0, ax1,
                                              q_try, nullptr, false);
      if (!e_try.valid) {
        last_fail = ProjectOutcome::kOpticalInvalid;
        last_margin = e_try.margin.failing_index;
        alpha *= 0.5;
        continue;
      }
      if (e_try.generator_cross < params.pole_tol) {
        last_fail = ProjectOutcome::kPoleHit;
        last_axis = PoseManifold::kAxisZenith;
        alpha *= 0.5;
        continue;
      }
      if (std::fabs(e_try.j_reg) < abs_j_reg || std::fabs(e_try.j_val) < abs_j ||
          std::fabs(e_try.j_val) <= params.residual_tol) {
        res.q[0] = q_try[0];
        res.q[1] = q_try[1];
        res.eval = e_try;
        step_accepted = true;
        break;
      }
      last_fail = ProjectOutcome::kDiverged;
      alpha *= 0.5;
    }

    if (!step_accepted) {
      if (last_fail == ProjectOutcome::kDiverged && converged_ulp) {
        res.outcome = ulp_outcome(abs_j);
        res.iterations = iter;
        return res;
      }
      res.outcome = last_fail;
      res.crossed_axis = last_axis;
      res.failing_margin = last_margin;
      res.iterations = iter;
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
                             const CausticContinuationParams& params, double q_out[2], Eval2DJet2* eval_out,
                             bool allow_ulp_cliff = false) {
  double q[2];
  q[pinned_slot] = bound_val;
  q[1 - pinned_slot] = other_start;
  const PoseAxis& other_ax = (pinned_slot == 0) ? manifold.axes[ax1] : manifold.axes[ax0];
  const double q_scale = std::max(1.0, std::hypot(bound_val, other_start));
  const double raw_ulp_cap = std::max(params.residual_tol, kRawInteriorJCap);
  for (int iter = 0; iter <= params.max_newton_iters; iter++) {
    if (!InAxisBounds(q[1 - pinned_slot], other_ax)) {
      return false;
    }
    const Eval2DJet2 e =
        Evaluate2DJet2(table, slots, slot_count, refractive_index, sun_dir, manifold, ax0, ax1, q, nullptr, false);
    if (!e.valid) {
      return false;
    }
    const double abs_j = std::fabs(e.j_val);
    const double abs_j_reg = std::fabs(e.j_reg);
    const double param_dist =
        (e.grad_reg_norm > 0.0) ? (abs_j_reg / e.grad_reg_norm) : std::numeric_limits<double>::infinity();
    const bool ulp_ok = (param_dist <= 5e-15 * q_scale) && (abs_j <= raw_ulp_cap || allow_ulp_cliff);
    if (abs_j <= params.residual_tol || (iter == params.max_newton_iters && ulp_ok)) {
      q_out[0] = q[0];
      q_out[1] = q[1];
      *eval_out = e;
      return true;
    }
    if (iter == params.max_newton_iters) {
      break;
    }
    const double deriv = e.grad_j_reg[1 - pinned_slot];
    if (std::fabs(deriv) < params.grad_tol) {
      return false;
    }
    const double step = -e.j_reg / deriv;
    if (!std::isfinite(step) || std::fabs(step) > max_drift) {
      return false;
    }
    if (std::fabs(step) <= 2.5e-16 * q_scale && ulp_ok) {
      q_out[0] = q[0];
      q_out[1] = q[1];
      *eval_out = e;
      return true;
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
                           int* failing_margin_out, bool allow_ulp_cliff = false) {
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
                                                q_guess, drift_cap, params, allow_ulp_cliff);
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

// Invariant for HalfBranch2D:
//   - node_stationary.size() == nodes.size()
//   - edge_corridor_open.size() == (nodes.empty() ? 0 : nodes.size() - 1)
//   - edge_corridor_open[i] is the corridor open/closed state of the interior of edge (nodes[i], nodes[i+1]).
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

  bool have_last_moving = have_e_sky_curr && std::fabs(s_sky_curr) > params.speed_tol * e_curr.sigma1 &&
                          e_curr.grad_norm >= 10.0 * params.grad_tol;
  double last_moving_q[2] = { q_curr[0], q_curr[1] };
  double last_moving_tau[2] = { tau_curr[0], tau_curr[1] };
  double last_moving_e_sky[3] = { e_sky_curr[0], e_sky_curr[1], e_sky_curr[2] };
  double last_moving_s_sky = s_sky_curr;
  double last_moving_arclen = 0.0;

  // Appends one node and (for every node after the first) the corridor state of the edge ending at
  // this node, preserving edge_corridor_open.size() == nodes.size() - 1.
  auto push_node = [&](const Eval2DJet2& e, const double tau[2], double s_val, bool incoming_edge_open) {
    if (!out.nodes.empty()) {
      out.edge_corridor_open.push_back(incoming_edge_open);
    }
    const PoseCurveNode node = NodeFromEval2D(e, ax0, ax1, tau, s_val);
    out.nodes.push_back(node);
    const double zero_floor = NumericalZeroSigmaFloor(e.margin.min_effective_margin);
    const bool stat = (node.sigma1 <= zero_floor) || (node.sky_speed <= params.speed_tol * node.sigma1);
    out.node_stationary.push_back(stat);
  };

  push_node(e_curr, tau_curr, 0.0, e_curr.corridor_open);

  double h = params.initial_step;
  double max_dist_from_seed = 0.0;
  bool moved_away = false;
  const double cos_max_turn = std::cos(params.max_turning_angle);

  auto commit_step = [&](const double q_next_in[2], Eval2DJet2 e_next, const double tau_next[2], double adv) {
    e_next.corridor_open = EvaluateCorridorOpen(corridor, e_next.pose, sun_dir, refractive_index);

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

    // 1. Cusp detection: sign change of tangential sky speed s_sky = e_sky . (dx * tau) along
    //    well-conditioned non-stationary segments, gated against residual-induced tangent jitter
    //    near singularities. Final scale-free geometric verification runs in StitchCausticCurve.
    const double pose_err_next =
        std::max(std::fabs(e_next.j_val), params.residual_tol) / std::max(e_next.grad_norm, params.grad_tol);
    const double cusp_speed_floor =
        std::max(params.speed_tol * e_next.sigma1, 1e4 * pose_err_next * std::max(e_next.sigma1, 1.0));
    const bool next_moving =
        have_e_sky_next && std::fabs(s_sky_next) > cusp_speed_floor && e_next.grad_norm >= 100.0 * params.grad_tol;
    if (next_moving) {
      if (have_last_moving && so3::Dot3(last_moving_e_sky, e_sky_next) > 0.5 && last_moving_s_sky * s_sky_next < 0.0) {
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
                               q_next_in, last_moving_tau, dir_sign, params, pred, q_cusp, &e_cusp, tau_cusp) &&
            e_cusp.grad_norm >= 100.0 * params.grad_tol) {
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

    // 3. Corridor cutoff detection on [q_curr, q_next]: split into edge (q_curr -> q_cut) with
    //    state open_a and edge (q_cut -> q_next) with state e_next.corridor_open.
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

    // Near an optical cliff, allow step size to refine below nominal min_step so steep grazing
    // caustics glide smoothly to kSnellEventTolerance instead of halting early.
    const bool near_cliff = e_curr.margin.min_effective_margin < params.event_slowdown_margin;
    const double eff_min_step = near_cliff ? std::min(params.min_step, 1e-9) : params.min_step;

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
      if (CheckStop(stop)) {
        out.stop_requested = true;
        out.budget_truncated = true;
        return out;
      }
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
              if (sky_turn_angle > params.max_turning_angle && sky_turn_angle < 2.5) {
                sky_turn_ok = false;
              }
            }
          }

          const bool at_min_step = !(h > eff_min_step * 2.0);
          if (adv > 1e-15 && proj > 0.25 * adv && turn_dot >= cos_max_turn && sky_turn_ok) {
            accepted = true;
            break;
          }
          // At the minimum step near a grazing cliff, accept a converged regular on-locus step
          // moving forward rather than misclassifying high sky curvature as a locus singularity.
          if (at_min_step && adv > 1e-15 && proj > 0.0 && turn_dot > 0.0) {
            accepted = true;
            break;
          }
          pr.outcome = ProjectOutcome::kDiverged;
        }
      }

      if (h > eff_min_step * 2.0) {
        const double next_h = std::max(eff_min_step, 0.5 * h);
        if (next_h < h) {
          h = next_h;
          continue;
        }
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

      // When Newton reaches float64 parameter-space ULP precision near a steep grazing Snell cliff
      // (where ||grad J|| = O(1/d) prevents raw |J| <= kRawInteriorJCap), march silently along the
      // desingularized J_reg = 0 locus without emitting ULP-noisy interior nodes, while tracking
      // corridor state transitions across internal cliff steps and at the terminal endpoint.
      if (pr.outcome == ProjectOutcome::kCliffUlpLimit) {
        double q_c[2] = { q_curr[0], q_curr[1] };
        Eval2DJet2 e_c = e_curr;
        double tau_c[2] = { tau_curr[0], tau_curr[1] };
        bool corr_c = e_curr.corridor_open;
        double s_cliff = 0.0;
        double h_c = std::clamp(std::max(h, 1e-6), 1e-9, 1e-3);
        const double raw_ulp_cap = std::max(params.residual_tol, kRawInteriorJCap);
        bool resumed_raw = false;
        bool saw_corridor_flip = false;

        // Bisects a corridor_open transition on [q_a, q_b] along J_reg = 0, emitting
        // kCorridorCutoff at the crossing (and splitting at an exact raw-accurate node if
        // |J(e_cut)| <= raw_ulp_cap).
        auto record_cliff_corridor_flip = [&](const double q_a[2], const Eval2DJet2& e_a, const double tau_a[2],
                                              bool open_a, double s_a, const double q_b[2], const Eval2DJet2& e_b,
                                              const double tau_b[2], bool open_b, double s_b) {
          if (corridor == nullptr || open_a == open_b) {
            return;
          }
          saw_corridor_flip = true;
          double qa[2] = { q_a[0], q_a[1] };
          double qb[2] = { q_b[0], q_b[1] };
          const double seg_len = std::hypot(q_b[0] - q_a[0], q_b[1] - q_a[1]);
          const double drift_cap = std::max(2.0 * seg_len, 1e-4);
          Eval2DJet2 e_cut = e_b;
          double tau_cut[2] = { tau_b[0], tau_b[1] };
          double s_cut = 0.5 * (s_a + s_b);
          bool have_cut = false;
          for (int it = 0; it < params.bisection_iters; it++) {
            const double qm_guess[2] = { 0.5 * (qa[0] + qb[0]), 0.5 * (qa[1] + qb[1]) };
            const ProjectResult pm = ProjectOntoCaustic(table, slots, slot_count, refractive_index, sun_dir, manifold,
                                                        ax0, ax1, qm_guess, drift_cap, params,
                                                        /*allow_ulp_cliff=*/true);
            if (pm.outcome != ProjectOutcome::kConverged || pm.eval.grad_norm < params.grad_tol) {
              break;
            }
            double tm[2];
            TangentFromGrad(pm.eval.grad_j, pm.eval.grad_norm, dir_sign, tm);
            if (tm[0] * tau_a[0] + tm[1] * tau_a[1] < 0.0) {
              tm[0] = -tm[0];
              tm[1] = -tm[1];
            }
            e_cut = pm.eval;
            tau_cut[0] = tm[0];
            tau_cut[1] = tm[1];
            s_cut = s_a + std::hypot(pm.q[0] - q_a[0], pm.q[1] - q_a[1]);
            have_cut = true;
            if (EvaluateCorridorOpen(corridor, pm.eval.pose, sun_dir, refractive_index) == open_a) {
              qa[0] = pm.q[0];
              qa[1] = pm.q[1];
            } else {
              qb[0] = pm.q[0];
              qb[1] = pm.q[1];
            }
          }
          if (!have_cut) {
            e_cut = e_a;
            tau_cut[0] = tau_a[0];
            tau_cut[1] = tau_a[1];
            s_cut = s_a;
          }
          const PoseCurveNode n_cut = NodeFromEval2D(e_cut, ax0, ax1, tau_cut, arclen + s_cut);
          CurveEvent ev;
          CopyNodeToEvent(n_cut, CurveEventKind::kCorridorCutoff, -1, &ev);
          out.events.push_back(ev);
        };

        auto finish_cliff_endpoint = [&](const double q_end[2], Eval2DJet2 e_end, const double tau_end[2], double s_end,
                                         CurveEventKind kind, int detail_idx) {
          e_end.corridor_open = EvaluateCorridorOpen(corridor, e_end.pose, sun_dir, refractive_index);
          if (corridor != nullptr && corr_c != e_end.corridor_open) {
            record_cliff_corridor_flip(q_c, e_c, tau_c, corr_c, s_cliff, q_end, e_end, tau_end, e_end.corridor_open,
                                       s_end);
          }
          if (s_end > 1e-13 && e_end.grad_norm >= params.grad_tol) {
            // Use the evaluated endpoint corridor state on the terminal edge so any corridor flip
            // inside the cliff tail cleanly splits the terminal segment (while never inserting an
            // interior node with raw |J| > kRawInteriorJCap).
            const bool edge_open = saw_corridor_flip ? e_end.corridor_open : e_curr.corridor_open;
            arclen += s_end;
            push_node(e_end, tau_end, arclen, edge_open);
          }
          CurveEvent ev;
          CopyNodeToEvent(out.nodes.back(), kind, detail_idx, &ev);
          out.events.push_back(ev);
        };

        for (int cstep = 0; cstep < 512 && s_cliff < 0.05; cstep++) {
          if (CheckStop(stop)) {
            out.stop_requested = true;
            out.budget_truncated = true;
            return out;
          }
          ProjectResult pr_c;
          double tau_c_next[2]{};
          double adv_c = 0.0;
          bool c_accepted = false;
          while (true) {
            const double q_cp[2] = { q_c[0] + h_c * tau_c[0], q_c[1] + h_c * tau_c[1] };
            pr_c = ProjectOntoCaustic(table, slots, slot_count, refractive_index, sun_dir, manifold, ax0, ax1, q_cp,
                                      std::max(2.0 * h_c, 1e-4), params, /*allow_ulp_cliff=*/true);
            if (pr_c.outcome == ProjectOutcome::kConverged && pr_c.eval.grad_norm >= params.grad_tol) {
              TangentFromGrad(pr_c.eval.grad_j, pr_c.eval.grad_norm, dir_sign, tau_c_next);
              const double d0 = pr_c.q[0] - q_c[0];
              const double d1 = pr_c.q[1] - q_c[1];
              adv_c = std::hypot(d0, d1);
              const double p_c = d0 * tau_c[0] + d1 * tau_c[1];
              const double td = tau_c_next[0] * tau_c[0] + tau_c_next[1] * tau_c[1];
              if (adv_c > 1e-15 && p_c > 0.0 && td > 0.0) {
                c_accepted = true;
                break;
              }
              pr_c.outcome = ProjectOutcome::kDiverged;
            }
            if (h_c > 2e-9) {
              const double next_hc = std::max(1e-9, 0.5 * h_c);
              if (next_hc < h_c) {
                h_c = next_hc;
                continue;
              }
            }
            break;
          }

          if (c_accepted) {
            const bool corr_next = EvaluateCorridorOpen(corridor, pr_c.eval.pose, sun_dir, refractive_index);
            if (corridor != nullptr && corr_c != corr_next) {
              record_cliff_corridor_flip(q_c, e_c, tau_c, corr_c, s_cliff, pr_c.q, pr_c.eval, tau_c_next, corr_next,
                                         s_cliff + adv_c);
            }
            s_cliff += adv_c;
            q_c[0] = pr_c.q[0];
            q_c[1] = pr_c.q[1];
            e_c = pr_c.eval;
            e_c.corridor_open = corr_next;
            tau_c[0] = tau_c_next[0];
            tau_c[1] = tau_c_next[1];
            corr_c = corr_next;
            // If the branch emerges back into a well-conditioned regime with raw |J| <= kRawInteriorJCap,
            // resume normal continuation from this valid point.
            if (std::fabs(e_c.j_val) <= raw_ulp_cap && e_c.margin.min_effective_margin > 3e-5) {
              pr.q[0] = q_c[0];
              pr.q[1] = q_c[1];
              pr.eval = e_c;
              pr.outcome = ProjectOutcome::kConverged;
              tau_next[0] = tau_c[0];
              tau_next[1] = tau_c[1];
              adv = s_cliff;
              turn_dot = tau_next[0] * tau_curr[0] + tau_next[1] * tau_curr[1];
              sky_turn_angle = 0.0;
              resumed_raw = true;
              break;
            }
            h_c = std::clamp(1.5 * h_c, 1e-9, 1e-3);
            continue;
          }

          if (pr_c.outcome == ProjectOutcome::kOpticalInvalid) {
            double q_bnd[2] = { q_c[0], q_c[1] };
            Eval2DJet2 e_bnd = e_c;
            int fail_margin = pr_c.failing_margin;
            BisectBoundaryOnLocus(table, slots, slot_count, refractive_index, sun_dir, manifold, ax0, ax1, q_c, tau_c,
                                  h_c, params, q_bnd, &e_bnd, &fail_margin, /*allow_ulp_cliff=*/true);
            double tau_b[2] = { tau_c[0], tau_c[1] };
            if (e_bnd.grad_norm >= params.grad_tol) {
              TangentFromGrad(e_bnd.grad_j, e_bnd.grad_norm, dir_sign, tau_b);
            }
            const double ds_end = s_cliff + std::hypot(q_bnd[0] - q_c[0], q_bnd[1] - q_c[1]);
            finish_cliff_endpoint(q_bnd, e_bnd, tau_b, ds_end, CurveEventKind::kOpticalBoundary, fail_margin);
            return out;
          }

          if (pr_c.outcome == ProjectOutcome::kDomainCrossed || pr_c.outcome == ProjectOutcome::kPoleHit) {
            int bnd_axis = pr_c.crossed_axis;
            double q_bnd[2] = { q_c[0], q_c[1] };
            Eval2DJet2 e_bnd = e_c;
            int dummy_fail = -1;
            BisectBoundaryOnLocus(table, slots, slot_count, refractive_index, sun_dir, manifold, ax0, ax1, q_c, tau_c,
                                  h_c, params, q_bnd, &e_bnd, &dummy_fail, /*allow_ulp_cliff=*/true);
            double tau_b[2] = { tau_c[0], tau_c[1] };
            if (e_bnd.grad_norm >= params.grad_tol) {
              TangentFromGrad(e_bnd.grad_j, e_bnd.grad_norm, dir_sign, tau_b);
            }
            const double ds_end = s_cliff + std::hypot(q_bnd[0] - q_c[0], q_bnd[1] - q_c[1]);
            finish_cliff_endpoint(q_bnd, e_bnd, tau_b, ds_end, CurveEventKind::kDomainBoundary, bnd_axis);
            return out;
          }

          const double q_cp[2] = { q_c[0] + h_c * tau_c[0], q_c[1] + h_c * tau_c[1] };
          const Eval2DJet2 e_cp = Evaluate2DJet2(table, slots, slot_count, refractive_index, sun_dir, manifold, ax0,
                                                 ax1, q_cp, nullptr, false);
          if (!e_cp.valid) {
            finish_cliff_endpoint(q_c, e_c, tau_c, s_cliff, CurveEventKind::kOpticalBoundary,
                                  e_cp.margin.failing_index);
            return out;
          }
          break;
        }

        if (resumed_raw) {
          accepted = true;
          break;
        }
        // Fallback if cliff march exhausted without reaching an invalid probe.
        out.unresolved_singularities++;
        finish_cliff_endpoint(q_c, e_c, tau_c, s_cliff, CurveEventKind::kLocusSingularity, -1);
        return out;
      }

      // Check whether an optical cliff lies within [q_curr, q_curr + h * tau] before reporting
      // kLocusSingularity.
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
        const double q_close[2] = { q_curr[0] - d_prev_0, q_curr[1] - d_prev_1 };
        if (dist_prev > 1e-12) {
          commit_step(q_close, seed_eval, tau_seed, dist_prev);
        }
        out.closed = true;
        return out;
      }
    }

    const double prev_margin = e_curr.margin.min_effective_margin;
    const double next_margin = pr.eval.margin.min_effective_margin;
    commit_step(pr.q, pr.eval, tau_next, adv);

    // Adapt step size for next iteration: Newton work, parameter & sky turning angles, optical
    // margin slowdown, and non-periodic domain bounds.
    const double next_eff_min =
        (next_margin < params.event_slowdown_margin) ? std::min(params.min_step, 1e-9) : params.min_step;
    double h_next = h;
    const double eff_turn =
        std::max(std::acos(std::clamp(turn_dot, -1.0, 1.0)), sky_turn_angle < 2.5 ? sky_turn_angle : 0.0);
    if (pr.iterations <= 2 && eff_turn < 0.25 * params.max_turning_angle) {
      h_next = std::min(params.max_step, h * 1.25);
    } else if (pr.iterations >= 6 || eff_turn > 0.6 * params.max_turning_angle) {
      h_next = std::max(next_eff_min, h * 0.7);
    }

    if (next_margin < params.event_slowdown_margin && next_margin < prev_margin && adv > 0.0) {
      const double rate = (prev_margin - next_margin) / adv;
      if (rate > 0.0) {
        const double d_est = next_margin / rate;
        h_next = std::min(h_next, std::max(next_eff_min, 0.5 * d_est));
      }
    }

    for (int slot = 0; slot < 2; slot++) {
      const PoseAxis& ax = (slot == 0) ? pax0 : pax1;
      if (ax.periodic) {
        continue;
      }
      if (tau_curr[slot] < -1e-9) {
        const double d_lo = q_curr[slot] - ax.min_val;
        if (d_lo > 2.0 * next_eff_min && d_lo < h_next) {
          h_next = std::min(h_next, std::max(next_eff_min, 0.5 * d_lo / std::fabs(tau_curr[slot])));
        }
      } else if (tau_curr[slot] > 1e-9) {
        const double d_hi = ax.max_val - q_curr[slot];
        if (d_hi > 2.0 * next_eff_min && d_hi < h_next) {
          h_next = std::min(h_next, std::max(next_eff_min, 0.5 * d_hi / std::fabs(tau_curr[slot])));
        }
      }
    }

    h = std::clamp(h_next, next_eff_min, params.max_step);
  }

  out.budget_truncated = true;
  return out;
}

// Single-source half-branch stitch helper shared by 2D J = 0 continuation (StitchCausticCurve)
// and continuous rank-1 collapsed tracing (TraceContinuousRank1AxisAligned).
// Relies on the edge-indexed invariant: edge_corridor_open[i] is the corridor state of edge
// (nodes[i], nodes[i+1]). Reversing backward maps edge (backward[idx], backward[idx - 1]) directly
// to backward.edge_corridor_open[idx - 1].
template <class HalfBranch>
PoseCurve StitchHalfBranches(const HalfBranch& forward, const HalfBranch& backward, PoseCurveKind kind, int seed_grid_n,
                             double speed_tol, int unresolved_singularities) {
  PoseCurve curve;
  curve.kind = kind;
  curve.coverage.seed_grid_n = seed_grid_n;
  curve.coverage.closed = forward.closed;
  curve.coverage.budget_truncated = forward.budget_truncated || backward.budget_truncated;
  curve.coverage.unresolved_singularities = unresolved_singularities;

  std::vector<bool> node_stationary;
  std::vector<bool> edge_corridor_open;

  if (forward.closed || backward.nodes.size() <= 1) {
    curve.nodes = forward.nodes;
    node_stationary = forward.node_stationary;
    edge_corridor_open = forward.edge_corridor_open;
    curve.events = forward.events;
    if (!forward.closed) {
      for (CurveEvent ev : backward.events) {
        ev.arclength = 0.0;
        bool duplicate = false;
        for (const CurveEvent& existing : curve.events) {
          if (existing.kind == ev.kind && existing.detail_index == ev.detail_index &&
              std::fabs(existing.arclength - ev.arclength) <= 1e-12) {
            duplicate = true;
            break;
          }
        }
        if (!duplicate) {
          curve.events.push_back(ev);
        }
      }
    }
  } else {
    const double back_len = backward.nodes.back().arclength;
    const size_t b_cnt = backward.nodes.size();
    const size_t f_cnt = forward.nodes.size();
    curve.nodes.reserve(b_cnt - 1 + f_cnt);
    node_stationary.reserve(b_cnt - 1 + f_cnt);
    edge_corridor_open.reserve(b_cnt - 1 + f_cnt);

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
    for (size_t idx = 0; idx < f_cnt; idx++) {
      PoseCurveNode node = forward.nodes[idx];
      node.arclength += back_len;
      curve.nodes.push_back(node);
      node_stationary.push_back(forward.node_stationary[idx]);
      if (idx + 1 < f_cnt) {
        edge_corridor_open.push_back(forward.edge_corridor_open[idx]);
      }
    }

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

PoseCurve StitchCausticCurve(const HalfBranch2D& forward, const HalfBranch2D& backward, int seed_grid_n,
                             double speed_tol) {
  PoseCurve curve = StitchHalfBranches(forward, backward, PoseCurveKind::kCaustic2D, seed_grid_n, speed_tol,
                                       forward.unresolved_singularities + backward.unresolved_singularities);

  // Scale-free geometric verification for every candidate kCusp against the stitched curve:
  // a genuine cusp lies outside kPointImage segments and shows resolved opposing sky motion
  // consistent with the local tangential sky velocity on BOTH sides (passing compact +-0.02 deg
  // boxes while still suppressing residual-jitter cusps on constant-image mirrors).
  if (!curve.events.empty() && curve.nodes.size() >= 3) {
    auto node_in_point_image = [&](int node_idx) {
      for (const CurveSegment& seg : curve.segments) {
        if (seg.kind == CurveSegmentKind::kPointImage && node_idx >= seg.start_idx && node_idx <= seg.end_idx) {
          return true;
        }
      }
      return false;
    };
    auto cusp_resolved = [&](const CurveEvent& ev) {
      for (const CurveSegment& seg : curve.segments) {
        const double s0 = curve.nodes[static_cast<size_t>(seg.start_idx)].arclength;
        const double s1 = curve.nodes[static_cast<size_t>(seg.end_idx)].arclength;
        if (seg.kind == CurveSegmentKind::kPointImage && ev.arclength >= s0 - 1e-12 && ev.arclength <= s1 + 1e-12) {
          return false;
        }
      }
      size_t split_idx = 0;
      while (split_idx < curve.nodes.size() && curve.nodes[split_idx].arclength < ev.arclength) {
        split_idx++;
      }
      auto node_resolves_motion = [&](int idx) {
        const PoseCurveNode& nd = curve.nodes[static_cast<size_t>(idx)];
        if (nd.sky_speed <= speed_tol * nd.sigma1) {
          return false;
        }
        const double ds = std::fabs(ev.arclength - nd.arclength);
        const double ang = so3::AngleBetween(nd.sky, ev.sky);
        return ang > 1e-11 && ang >= 0.05 * nd.sky_speed * ds;
      };
      int left_idx = -1;
      for (int i = static_cast<int>(split_idx) - 1; i >= 0; i--) {
        if (node_in_point_image(i)) {
          break;
        }
        if (node_resolves_motion(i)) {
          left_idx = i;
          break;
        }
      }
      int right_idx = -1;
      for (int i = static_cast<int>(split_idx); i < static_cast<int>(curve.nodes.size()); i++) {
        if (node_in_point_image(i)) {
          break;
        }
        if (node_resolves_motion(i)) {
          right_idx = i;
          break;
        }
      }
      if (left_idx < 0 || right_idx < 0) {
        return false;
      }
      const double* s_left = curve.nodes[static_cast<size_t>(left_idx)].sky;
      const double* s_right = curve.nodes[static_cast<size_t>(right_idx)].sky;
      const double d_in[3] = { ev.sky[0] - s_left[0], ev.sky[1] - s_left[1], ev.sky[2] - s_left[2] };
      const double d_out[3] = { s_right[0] - ev.sky[0], s_right[1] - ev.sky[1], s_right[2] - ev.sky[2] };
      return so3::Dot3(d_in, d_out) < 0.0;
    };

    std::vector<CurveEvent> filtered;
    filtered.reserve(curve.events.size());
    for (const CurveEvent& ev : curve.events) {
      if (ev.kind == CurveEventKind::kCusp && !cusp_resolved(ev)) {
        continue;
      }
      filtered.push_back(ev);
    }
    curve.events = std::move(filtered);
  }
  return curve;
}

// =================================================================================================
// Rank-1 Collapsed 2D Continuous Fiber-Following Tracer
// =================================================================================================

// Checks whether `sky` lies within `tol_rad` of any represented polyline segment on `curves`.
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
        closest[0] /= cn;
        closest[1] /= cn;
        closest[2] /= cn;
        if (so3::AngleBetween(sky, closest) <= tol_rad) {
          return true;
        }
      }
    }
  }
  return false;
}

// Checks whether a varying-axis coordinate `t_val` is already covered by any segment of `curves`
// (for axis-aligned rank-1 manifolds where sky depends solely on `vary_ax`).
bool IsVaryCoordCoveredByCurves(double t_val, int vary_ax, const PoseAxis& vax, const std::vector<PoseCurve>& curves) {
  const double t_wrap = WrapAxis(t_val, vax);
  for (const PoseCurve& c : curves) {
    if (c.coverage.closed && vax.periodic) {
      return true;
    }
    const size_t n = c.nodes.size();
    if (n == 0) {
      continue;
    }
    if (n == 1) {
      if (std::fabs(DeltaAxis(t_wrap, c.nodes[0].param[vary_ax], vax)) <= 1e-6) {
        return true;
      }
      continue;
    }
    for (size_t i = 0; i + 1 < n; i++) {
      const double ta = c.nodes[i].param[vary_ax];
      const double tb = c.nodes[i + 1].param[vary_ax];
      const double seg = DeltaAxis(tb, ta, vax);
      const double off = DeltaAxis(t_wrap, ta, vax);
      const double eps = 1e-9;
      if (seg >= 0.0) {
        if (off >= -eps && off <= seg + eps) {
          return true;
        }
      } else {
        if (off <= eps && off >= seg - eps) {
          return true;
        }
      }
    }
  }
  return false;
}

// Traces continuous pose curves for an axis-aligned rank-1 collapsed 2D family (sky depends on
// `vary_ax`, invariant along `fiber_ax`). Within each curve, `fiber_ax` slides continuously inside
// the locally connected feasible component (never jumping across an infeasible fiber gap), and
// every node's tangent, arclength, and sky_speed reflect the true 2D step (dt, df). Disjoint
// feasible sheets are split into separate curves, and every valid coarse grid cell's sky direction
// is covered up to `params.max_curves`.
struct Rank1ContinuousHalf {
  std::vector<PoseCurveNode> nodes;
  std::vector<bool> node_stationary;
  std::vector<bool> edge_corridor_open;
  std::vector<CurveEvent> events;
  bool closed = false;
  bool budget_truncated = false;
  bool stop_requested = false;
};

CausticTraceResult TraceContinuousRank1AxisAligned(const FaceNormalTable& table, const int* slots, int slot_count,
                                                   double refractive_index, const double sun_dir[3],
                                                   const PoseManifold& manifold, int ax0, int ax1, int vary_ax,
                                                   int fiber_ax, const std::vector<Eval2DJet1>& grid, int grid_n,
                                                   Corridor* corridor, const CausticContinuationParams& params,
                                                   const DiscoveryStop* stop) {
  CausticTraceResult out;
  out.generic_rank = 1;
  out.coverage.seed_grid_n = grid_n;

  const PoseAxis& vax = manifold.axes[vary_ax];
  const PoseAxis& fax = manifold.axes[fiber_ax];
  const bool track_corridor = (corridor != nullptr);
  const double margin_lip = std::max(6.0, 3.0 * refractive_index * refractive_index);
  const double v_span = vax.Span();
  const double f_span = fax.Span();
  const double step_min = params.min_step;
  const double step_max = params.max_step;
  const double init_step = params.initial_step;
  const double max_turn = params.max_turning_angle;
  const int max_steps = params.max_steps_per_branch;

  auto eval_at = [&](double t_vary, double f_val, bool eval_corridor) {
    double param[3] = { manifold.axes[0].value, manifold.axes[1].value, manifold.axes[2].value };
    param[vary_ax] = WrapAxis(t_vary, vax);
    param[fiber_ax] = WrapAxis(f_val, fax);
    return Evaluate1DAtPose(table, slots, slot_count, refractive_index, sun_dir, param, vary_ax, corridor,
                            eval_corridor);
  };

  // Finds a locally connected feasible fiber coordinate near `f_curr` at `t_next`, without ever
  // crossing an infeasible fiber gap.
  auto find_local_fiber = [&](double t_curr, double f_curr, double t_next, double* f_next_out, Eval1D* e_next_out,
                              int* fail_idx_out) -> bool {
    const Eval1D e0 = eval_at(t_next, f_curr, track_corridor);
    *fail_idx_out = e0.margin.failing_index;
    if (e0.valid && e0.margin.min_effective_margin >= 0.02 && (!track_corridor || e0.corridor_open)) {
      *f_next_out = f_curr;
      *e_next_out = e0;
      return true;
    }

    const double dt_abs = std::fabs(t_next - t_curr);
    const double max_slide = std::min(0.2 * f_span, std::max(0.03, 3.0 * dt_abs));
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
        double f_cand = f_curr + static_cast<double>(side * k) * df_probe;
        if (!fax.periodic) {
          if (f_cand < fax.min_val || f_cand > fax.max_val) {
            break;
          }
        }
        const Eval1D ec = eval_at(t_next, f_cand, track_corridor);
        if (!ec.valid) {
          if (found_valid_on_side) {
            break;  // Never jump across an infeasible fiber gap.
          }
          continue;
        }
        // When sliding off an invalid f_curr, verify the diagonal midpoint in (t, f) is valid so
        // the step stays strictly inside the connected feasible region.
        if (!e0.valid) {
          const Eval1D emid = eval_at(0.5 * (t_curr + t_next), 0.5 * (f_curr + f_cand), false);
          if (!emid.valid) {
            continue;
          }
        }
        found_valid_on_side = true;
        const bool c_open = ec.corridor_open;
        bool better = false;
        if (!have_best) {
          better = true;
        } else if (track_corridor && c_open != best_open) {
          better = c_open;
        } else if (ec.margin.min_effective_margin > best_score) {
          better = true;
        }
        if (better) {
          have_best = true;
          best_f = f_cand;
          best_e = ec;
          best_score = ec.margin.min_effective_margin;
          best_open = c_open;
        }
      }
    }

    if (!have_best) {
      return false;
    }

    // Local golden-section refinement around best_f within +-df_probe to keep margin smooth.
    if (best_score < 0.05) {
      constexpr double kInvPhi = 0.6180339887498949;
      double lo = best_f - df_probe;
      double hi = best_f + df_probe;
      if (!fax.periodic) {
        lo = std::max(fax.min_val, lo);
        hi = std::min(fax.max_val, hi);
      }
      if (hi - lo > 1e-12) {
        double c = hi - kInvPhi * (hi - lo);
        double d = lo + kInvPhi * (hi - lo);
        Eval1D ec = eval_at(t_next, c, false);
        Eval1D ed = eval_at(t_next, d, false);
        for (int it = 0; it < 10; it++) {
          if (ec.margin.min_effective_margin > ed.margin.min_effective_margin) {
            hi = d;
            d = c;
            ed = ec;
            c = hi - kInvPhi * (hi - lo);
            ec = eval_at(t_next, c, false);
          } else {
            lo = c;
            c = d;
            ec = ed;
            d = lo + kInvPhi * (hi - lo);
            ed = eval_at(t_next, d, false);
          }
        }
        const double f_ref = 0.5 * (lo + hi);
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
    nd.tangent[0] = 0.0;
    nd.tangent[1] = 0.0;
    nd.tangent[2] = 0.0;
    // Preserve the 2D manifold Jacobian's singular value (||d(sky)/d(vary_ax)|| = e.sky_speed;
    // d(sky)/d(fiber_ax) == 0) in sigma1 regardless of representative fiber motion; apply the
    // tangent projection |dt/ds| only to directional sky_speed.
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

  auto trace_half = [&](double t_seed, double f_seed, int dir_sign, bool allow_closure) -> Rank1ContinuousHalf {
    Rank1ContinuousHalf half;
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
      PoseCurveNode nd = make_node(e, dt_s, df_s, s_val);
      half.nodes.push_back(nd);
      half.node_stationary.push_back(nd.sky_speed <= params.speed_tol);
    };

    push_nd(e_curr, sgn, 0.0, 0.0, e_curr.corridor_open);

    // If the seed itself sits on a non-periodic domain boundary in the traced direction, emit it.
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

    double h = std::clamp(init_step, step_min, step_max);
    int steps_taken = 0;

    while (sgn * (t_limit - t_curr) > 1e-13) {
      if (CheckStop(stop)) {
        half.stop_requested = true;
        half.budget_truncated = true;
        return half;
      }
      if (steps_taken >= max_steps) {
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

      while (step > step_min * 2.0) {
        if (CheckStop(stop)) {
          half.stop_requested = true;
          half.budget_truncated = true;
          return half;
        }
        bool need_shrink = !ok;
        if (ok) {
          const double df_abs = std::fabs(f_next - f_curr);
          const double sky_step = so3::AngleBetween(e_curr.sky, e_next.sky);
          if (df_abs > 0.12 || sky_step > max_turn) {
            need_shrink = true;
          } else if (e_curr.sky_speed > params.speed_tol && e_next.sky_speed > params.speed_tol) {
            const double d_angle = so3::AngleBetween(e_curr.dx, e_next.dx);
            if (d_angle > max_turn && d_angle < 2.5) {
              need_shrink = true;
            }
          }
        }
        if (!need_shrink) {
          break;
        }
        const double next_step = std::max(step_min, 0.5 * step);
        if (!(next_step < step)) {
          break;
        }
        step = next_step;
        t_next = t_curr + sgn * step;
        ok = find_local_fiber(t_curr, f_curr, t_next, &f_next, &e_next, &fail_idx);
        shrunk = true;
      }

      if (!ok) {
        // Connected fiber sheet hit its optical boundary: bisect along the valid segment.
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
        const double dt_b = t_v - t_curr;
        const double df_b = f_v - f_curr;
        const double ds_b = std::hypot(dt_b, df_b);
        if (ds_b > 1e-13) {
          arclen += ds_b;
          push_nd(e_bnd, dt_b, df_b, arclen, e_curr.corridor_open);
        }
        CurveEvent ev;
        CopyNodeToEvent(half.nodes.back(), CurveEventKind::kOpticalBoundary, bnd_fail, &ev);
        half.events.push_back(ev);
        return half;
      }

      // Check for an interior grazing optical gap along the linear chord (t_curr, f_curr) -> (t_next, f_next).
      auto chord_eval = [&](double u_par, bool eval_corr) {
        return eval_at(t_curr + u_par * (t_next - t_curr), f_curr + u_par * (f_next - f_curr), eval_corr);
      };
      const double ds_chord = std::hypot(t_next - t_curr, f_next - f_curr);
      double u_inv = 1.0;
      if (FindInteriorOpticalGap1D(chord_eval, 0.0, e_curr.margin.min_effective_margin, 1.0,
                                   e_next.margin.min_effective_margin, margin_lip * ds_chord, &u_inv)) {
        double u_lo = 0.0;
        double u_hi = u_inv;
        Eval1D e_bnd = e_curr;
        int bnd_fail = chord_eval(u_inv, false).margin.failing_index;
        for (int it = 0; it < params.bisection_iters; it++) {
          const double u_m = 0.5 * (u_lo + u_hi);
          const Eval1D em = chord_eval(u_m, false);
          if (em.valid) {
            u_lo = u_m;
            e_bnd = em;
          } else {
            u_hi = u_m;
            if (em.margin.failing_index >= 0) {
              bnd_fail = em.margin.failing_index;
            }
          }
        }
        const double dt_b = u_lo * (t_next - t_curr);
        const double df_b = u_lo * (f_next - f_curr);
        const double ds_b = std::hypot(dt_b, df_b);
        if (ds_b > 1e-13) {
          arclen += ds_b;
          push_nd(e_bnd, dt_b, df_b, arclen, e_curr.corridor_open);
        }
        CurveEvent ev;
        CopyNodeToEvent(half.nodes.back(), CurveEventKind::kOpticalBoundary, bnd_fail, &ev);
        half.events.push_back(ev);
        return half;
      }

      const double dt_s = t_next - t_curr;
      const double df_s = f_next - f_curr;
      const double ds = std::hypot(dt_s, df_s);

      // Update node 0's tangent and directional sky_speed to match the actual first step when f
      // slides immediately (keep node 0's manifold sigma1 = e_curr.sky_speed intact).
      if (half.nodes.size() == 1 && ds > 0.0) {
        half.nodes[0].tangent[vary_ax] = dt_s / ds;
        half.nodes[0].tangent[fiber_ax] = df_s / ds;
        half.nodes[0].sky_speed = std::fabs(dt_s / ds) * e_curr.sky_speed;
      }

      // Stationary point detection along the vary_ax derivative (sky depends only on vary_ax).
      if (e_next.sky_speed > params.speed_tol) {
        if (have_last_moving && so3::Dot3(last_moving_e.dx, e_next.dx) < 0.0) {
          double ref[3] = { e_next.dx[0] - last_moving_e.dx[0], e_next.dx[1] - last_moving_e.dx[1],
                            e_next.dx[2] - last_moving_e.dx[2] };
          const double rn = so3::Norm3(ref);
          if (rn > 0.0) {
            ref[0] /= rn;
            ref[1] /= rn;
            ref[2] /= rn;
            double ta = last_moving_t;
            double tb = t_next;
            double fa = so3::Dot3(last_moving_e.dx, ref);
            for (int it = 0; it < params.bisection_iters; it++) {
              const double tm = 0.5 * (ta + tb);
              const double frac = (tm - last_moving_t) / (t_next - last_moving_t);
              const Eval1D em = eval_at(tm, last_moving_f + frac * (f_next - last_moving_f), false);
              if (!em.valid) {
                break;
              }
              const double fm = so3::Dot3(em.dx, ref);
              if ((fa <= 0.0 && fm <= 0.0) || (fa > 0.0 && fm > 0.0)) {
                ta = tm;
                fa = fm;
              } else {
                tb = tm;
              }
            }
            const double t_stat = 0.5 * (ta + tb);
            const double frac = (t_stat - last_moving_t) / (t_next - last_moving_t);
            const double f_stat = last_moving_f + frac * (f_next - last_moving_f);
            const Eval1D e_stat = eval_at(t_stat, f_stat, false);
            if (e_stat.valid && e_stat.sky_speed <= 1e-4) {
              const double s_stat = last_moving_arclen + std::hypot(t_stat - last_moving_t, f_stat - last_moving_f);
              const PoseCurveNode n_stat = make_node(e_stat, dt_s, df_s, s_stat);
              CurveEvent ev;
              CopyNodeToEvent(n_stat, CurveEventKind::kStationaryPoint, -1, &ev);
              half.events.push_back(ev);
            }
          }
        }
        have_last_moving = true;
        last_moving_t = t_next;
        last_moving_f = f_next;
        last_moving_e = e_next;
        last_moving_arclen = arclen + ds;
      }

      // Internal TIR onset along chord.
      for (int k = 1; k < slot_count - 1; k++) {
        const double da = e_curr.internal_disc[k];
        const double db = e_next.internal_disc[k];
        if ((da > 0.0) != (db > 0.0)) {
          double ua = 0.0;
          double ub = 1.0;
          const bool pos_a = da > 0.0;
          for (int it = 0; it < params.bisection_iters; it++) {
            const double um = 0.5 * (ua + ub);
            const Eval1D em = chord_eval(um, false);
            if (!em.valid) {
              break;
            }
            if ((em.internal_disc[k] > 0.0) == pos_a) {
              ua = um;
            } else {
              ub = um;
            }
          }
          const double u_tir = 0.5 * (ua + ub);
          const Eval1D e_tir = chord_eval(u_tir, false);
          if (e_tir.valid) {
            const PoseCurveNode n_tir = make_node(e_tir, dt_s, df_s, arclen + u_tir * ds);
            CurveEvent ev;
            CopyNodeToEvent(n_tir, CurveEventKind::kTirOnset, k, &ev);
            half.events.push_back(ev);
          }
        }
      }

      // Corridor cutoff along chord.
      if (track_corridor && e_curr.corridor_open != e_next.corridor_open) {
        double ua = 0.0;
        double ub = 1.0;
        const bool open_a = e_curr.corridor_open;
        for (int it = 0; it < params.bisection_iters; it++) {
          const double um = 0.5 * (ua + ub);
          const Eval1D em = chord_eval(um, true);
          if (!em.valid) {
            break;
          }
          if (em.corridor_open == open_a) {
            ua = um;
          } else {
            ub = um;
          }
        }
        const double u_cut = 0.5 * (ua + ub);
        const Eval1D e_cut = chord_eval(u_cut, true);
        if (e_cut.valid) {
          const double ds1 = u_cut * ds;
          if (ds1 > 1e-13) {
            arclen += ds1;
            push_nd(e_cut, dt_s, df_s, arclen, open_a);
          }
          CurveEvent ev;
          CopyNodeToEvent(half.nodes.back(), CurveEventKind::kCorridorCutoff, -1, &ev);
          half.events.push_back(ev);
          const double ds2 = (1.0 - u_cut) * ds;
          if (ds2 > 1e-13) {
            arclen += ds2;
            push_nd(e_next, dt_s, df_s, arclen, e_next.corridor_open);
          }
          t_curr = t_next;
          f_curr = f_next;
          e_curr = e_next;
          steps_taken++;
          h = shrunk ? std::max(step_min, step) : std::min(step_max, step * 1.25);
          continue;
        }
      }

      arclen += ds;
      push_nd(e_next, dt_s, df_s, arclen, e_curr.corridor_open);
      t_curr = t_next;
      f_curr = f_next;
      e_curr = e_next;
      steps_taken++;
      h = shrunk ? std::max(step_min, step) : std::min(step_max, step * 1.25);
    }

    // Reached t_limit cleanly.
    if (!vax.periodic) {
      CurveEvent ev;
      CopyNodeToEvent(half.nodes.back(), CurveEventKind::kDomainBoundary, vary_ax, &ev);
      half.events.push_back(ev);
    } else if (allow_closure) {
      const double df_close = DeltaAxis(f_seed, f_curr, fax);
      if (std::fabs(df_close) <= 1e-9) {
        half.closed = true;
      } else {
        // Attempt a smooth continuous fiber closure back to f_seed at t_seed if connected.
        const int n_close = std::max(1, static_cast<int>(std::ceil(std::fabs(df_close) / 0.05)));
        bool path_ok = true;
        std::vector<Eval1D> close_evals;
        close_evals.reserve(static_cast<size_t>(n_close));
        for (int k = 1; k <= n_close; k++) {
          const double f_k = f_curr + df_close * static_cast<double>(k) / static_cast<double>(n_close);
          const Eval1D ek = eval_at(t_seed, f_k, track_corridor);
          if (!ek.valid) {
            path_ok = false;
            break;
          }
          close_evals.push_back(ek);
        }
        if (path_ok) {
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

  // Rank all valid coarse 2D grid cells by margin descending so we seed from interior high-margin
  // poses first, and trace additional continuous sheets until all valid sky directions are covered.
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

  auto grid_val = [grid_n](int idx, const PoseAxis& ax) {
    if (ax.periodic) {
      return ax.min_val + ax.Span() * (static_cast<double>(idx) + 0.5) / static_cast<double>(grid_n);
    }
    return ax.min_val + ax.Span() * static_cast<double>(idx) / static_cast<double>(grid_n - 1);
  };

  for (const CellSeed& cs : valid_cells) {
    if (CheckStop(stop)) {
      out.stop_requested = true;
      out.coverage.budget_truncated = true;
      break;
    }
    const double q0 = grid_val(cs.i, manifold.axes[ax0]);
    const double q1 = grid_val(cs.j, manifold.axes[ax1]);
    const double t_seed = (vary_ax == ax0) ? q0 : q1;
    const double f_seed = (fiber_ax == ax0) ? q0 : q1;
    const Eval2DJet1& cell = grid[static_cast<size_t>(cs.i * grid_n + cs.j)];
    if (IsVaryCoordCoveredByCurves(t_seed, vary_ax, vax, out.curves) ||
        IsSkyCoveredByCurves(cell.sky, out.curves, 0.015)) {
      continue;
    }
    if (static_cast<int>(out.curves.size()) >= params.max_curves) {
      out.coverage.budget_truncated = true;
      break;
    }

    const Rank1ContinuousHalf forward = trace_half(t_seed, f_seed, +1, true);
    Rank1ContinuousHalf backward;
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

// =================================================================================================
// Public API: Parameter Validation
// =================================================================================================

bool ValidateParams(const Family1DParams& p) {
  if (p.scan_samples < 4 || p.bisection_iters < 1 || p.max_nodes_per_branch < 1) {
    return false;
  }
  if (!std::isfinite(p.min_step) || !std::isfinite(p.max_step) || !std::isfinite(p.max_turning_angle) ||
      !std::isfinite(p.speed_tol) || !std::isfinite(p.stationary_tol)) {
    return false;
  }
  if (!(p.min_step > 0.0) || !(p.max_step >= p.min_step) || !(p.max_turning_angle > 0.0) || p.speed_tol < 0.0 ||
      p.stationary_tol < 0.0) {
    return false;
  }
  return true;
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
  if (p.rank_tol < 0.0 || !(p.residual_tol > 0.0) || !(p.grad_tol > 0.0) || p.speed_tol < 0.0 || p.pole_tol < 0.0 ||
      !(p.min_step > 0.0) || !(p.initial_step >= p.min_step) || !(p.max_step >= p.initial_step) ||
      !(p.max_turning_angle > 0.0) || p.event_slowdown_margin < 0.0 || p.dedup_tol < 0.0 || p.closure_tol < 0.0) {
    return false;
  }
  return true;
}

// =================================================================================================
// Public API: TraceFamily1D
// =================================================================================================

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
    param[ax] = WrapAxis(t_val, pax);
    return Evaluate1DAtPose(table, slots, slot_count, refractive_index, sun_dir, param, ax, corridor, eval_corridor);
  };

  const double margin_lip = std::max(6.0, 3.0 * refractive_index * refractive_index);
  return Trace1DWithEvaluator(pax, ax, slot_count, margin_lip, eval_fn, corridor != nullptr, params, stop);
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
  if (!ValidateParams(params) || !manifold.IsValid() || manifold.Dimension() != 2 || slots == nullptr ||
      slot_count < 1 || slot_count > kMaxFaceCount || !std::isfinite(refractive_index) || refractive_index <= 0.0 ||
      !ValidateUnitVector(sun_dir)) {
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
      if (cell.margin.min_effective_margin > best_valid_margin) {
        best_valid_margin = cell.margin.min_effective_margin;
        best_valid_i = i;
        best_valid_j = j;
      }
      max_x1_norm = std::max(max_x1_norm, so3::Norm3(cell.x1));
      max_x2_norm = std::max(max_x2_norm, so3::Norm3(cell.x2));
      const bool full_gen = cell.generator_cross >= params.pole_tol;
      if (full_gen) {
        any_full_generator_rank = true;
      }
      const double zero_floor = NumericalZeroSigmaFloor(cell.margin.min_effective_margin);
      if (cell.sigma1 > zero_floor) {
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
    if (add_pole_event) {
      CurveEvent ev;
      CopyNodeToEvent(node, CurveEventKind::kDomainBoundary, PoseManifold::kAxisZenith, &ev);
      curve.events.push_back(ev);
    }
    return curve;
  };

  // Shared relabel/merge helper for all collapsed 2D return paths: enforces params.max_curves,
  // records budget_truncated when any curve is omitted, and optionally emits the zenith pole event.
  auto append_collapsed_curves = [&](CausticTraceResult& sub, bool add_pole_event) {
    result.stop_requested = result.stop_requested || sub.stop_requested;
    result.coverage.budget_truncated = result.coverage.budget_truncated || sub.coverage.budget_truncated;
    for (PoseCurve& c : sub.curves) {
      if (static_cast<int>(result.curves.size()) >= params.max_curves) {
        result.coverage.budget_truncated = true;
        break;
      }
      c.kind = PoseCurveKind::kCollapsed2D;
      c.coverage.seed_grid_n = n;
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

  // Explicit Euler-pole generator rank loss (e.g. TiltedSpin(0 deg) or TiltedSpin(180 deg) where
  // azimuth and roll collapse onto the same spin generator everywhere): trace the full attainable
  // spin interval (az + roll at zenith = 0, az - roll at zenith = pi) mapped continuously into
  // both axis bounds, and emit kDomainBoundary on kAxisZenith.
  if (!any_full_generator_rank) {
    result.generic_rank = any_rank1 ? 1 : 0;
    if (!any_rank1) {
      result.curves.push_back(build_rank0_curve(true));
      return result;
    }
    Family1DParams f1d;
    f1d.min_step = params.min_step;
    f1d.max_step = params.max_step;
    f1d.max_turning_angle = params.max_turning_angle;
    f1d.speed_tol = params.speed_tol;
    f1d.bisection_iters = params.bisection_iters;
    f1d.max_nodes_per_branch = params.max_steps_per_branch + 1;
    const double margin_lip = std::max(6.0, 3.0 * refractive_index * refractive_index);

    CausticTraceResult collapsed;
    const bool is_az_roll_pole = (ax0 == PoseManifold::kAxisAzimuth && ax1 == PoseManifold::kAxisRoll);
    if (is_az_roll_pole && !pax0.periodic && !pax1.periodic) {
      const double z_val = grid[static_cast<size_t>(best_valid_i * n + best_valid_j)].param[PoseManifold::kAxisZenith];
      const double pole_sign = (std::cos(z_val) >= 0.0) ? 1.0 : -1.0;
      const double span_az = pax0.Span();
      const double span_roll = pax1.Span();
      const double total_spin = span_az + span_roll;
      const double d_az_dphi = span_az / total_spin;
      const double d_roll_dphi = pole_sign * (span_roll / total_spin);
      const double ds_dphi = std::hypot(d_az_dphi, d_roll_dphi);
      const double t_vec[3] = { d_az_dphi / ds_dphi, 0.0, d_roll_dphi / ds_dphi };

      const double az_start = pax0.min_val;
      const double roll_start = (pole_sign > 0.0) ? pax1.min_val : pax1.max_val;
      const double phi_span = std::min(total_spin, 2.0 * math::kPiD);
      const double arc_span = phi_span * ds_dphi;
      const PoseAxis arc_axis = PoseAxis::FreeRad(0.0, arc_span, false);

      auto pole_eval = [&](double s_arc, bool eval_corridor) {
        double param[3] = { manifold.axes[0].value, manifold.axes[1].value, manifold.axes[2].value };
        param[PoseManifold::kAxisAzimuth] = std::clamp(az_start + s_arc * t_vec[0], pax0.min_val, pax0.max_val);
        param[PoseManifold::kAxisZenith] = z_val;
        param[PoseManifold::kAxisRoll] = std::clamp(roll_start + s_arc * t_vec[2], pax1.min_val, pax1.max_val);
        return Evaluate1DAlongTangent(table, slots, slot_count, refractive_index, sun_dir, param, t_vec, corridor,
                                      eval_corridor);
      };
      collapsed = Trace1DWithEvaluator(arc_axis, PoseManifold::kAxisAzimuth, slot_count, margin_lip, pole_eval,
                                       corridor != nullptr, f1d, stop);
    } else if (is_az_roll_pole && (pax0.periodic || pax1.periodic)) {
      PoseManifold slice = manifold;
      const bool keep_ax0 = pax0.periodic;
      const double pin_val = keep_ax0 ? grid_coord(best_valid_j, pax1) : grid_coord(best_valid_i, pax0);
      slice.axes[keep_ax0 ? ax1 : ax0] = PoseAxis::PinnedRad(pin_val);
      collapsed = TraceFamily1D(table, slots, slot_count, refractive_index, sun_dir, slice, corridor, f1d, stop);
    } else {
      PoseManifold slice = manifold;
      const bool keep_ax0 = (max_x1_norm >= max_x2_norm);
      const double pin_val = keep_ax0 ? grid_coord(best_valid_j, pax1) : grid_coord(best_valid_i, pax0);
      slice.axes[keep_ax0 ? ax1 : ax0] = PoseAxis::PinnedRad(pin_val);
      collapsed = TraceFamily1D(table, slots, slot_count, refractive_index, sun_dir, slice, corridor, f1d, stop);
    }
    append_collapsed_curves(collapsed, /*add_pole_event=*/true);
    return result;
  }

  // Physical rank-deficient 2D family (rank 0 or rank 1): return kCollapsed2D without J = 0 continuation.
  if (!any_rank2) {
    if (!any_rank1) {
      result.generic_rank = 0;
      result.curves.push_back(build_rank0_curve(false));
      return result;
    }

    result.generic_rank = 1;
    Family1DParams f1d;
    f1d.min_step = params.min_step;
    f1d.max_step = params.max_step;
    f1d.max_turning_angle = params.max_turning_angle;
    f1d.speed_tol = params.speed_tol;
    f1d.bisection_iters = params.bisection_iters;
    f1d.max_nodes_per_branch = params.max_steps_per_branch + 1;

    // When sky is invariant along one free axis (e.g. basal mirror Column {3, 1, 6} or parallel
    // path {3, 5, 6, 8}), follow continuous locally-connected fiber sheets across all uncovered
    // coarse cells (splitting disjoint sheets cleanly and enforcing max_curves).
    const bool sky_only_ax0 = (max_x2_norm <= params.rank_tol * max_x1_norm);
    const bool sky_only_ax1 = (max_x1_norm <= params.rank_tol * max_x2_norm);
    if (sky_only_ax0 || sky_only_ax1) {
      const int vary_ax = sky_only_ax0 ? ax0 : ax1;
      const int fiber_ax = sky_only_ax0 ? ax1 : ax0;
      CausticTraceResult projected =
          TraceContinuousRank1AxisAligned(table, slots, slot_count, refractive_index, sun_dir, manifold, ax0, ax1,
                                          vary_ax, fiber_ax, grid, n, corridor, params, stop);
      append_collapsed_curves(projected, /*add_pole_event=*/false);
      return result;
    }

    // General non-axis-aligned rank-1 family: trace slices along the faster-moving axis until
    // every valid coarse grid cell's sky direction is covered.
    const bool keep_ax0 = (max_x1_norm >= max_x2_norm);
    const int pin_ax = keep_ax0 ? ax1 : ax0;
    auto trace_slice_at = [&](double pin_val) {
      PoseManifold slice = manifold;
      slice.axes[pin_ax] = PoseAxis::PinnedRad(pin_val);
      CausticTraceResult sub =
          TraceFamily1D(table, slots, slot_count, refractive_index, sun_dir, slice, corridor, f1d, stop);
      append_collapsed_curves(sub, /*add_pole_event=*/false);
    };
    trace_slice_at(keep_ax0 ? grid_coord(best_valid_j, pax1) : grid_coord(best_valid_i, pax0));
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
        trace_slice_at(keep_ax0 ? grid_coord(j, pax1) : grid_coord(i, pax0));
      }
    }
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
      const double qa[2] = { grid_coord(i, pax0), grid_coord(j, pax1) };
      const Eval2DJet1& a = grid[static_cast<size_t>(i * n + j)];
      if (i < edges0) {
        const int i1 = (i + 1) % n;
        const double qb[2] = { qa[0] + step0, qa[1] };
        if (process_edge_evals(qa, a, qb, grid[static_cast<size_t>(i1 * n + j)])) {
          cell_seeded[static_cast<size_t>(i * n + j)] = true;
          if (j > 0) {
            cell_seeded[static_cast<size_t>(i * n + (j - 1))] = true;
          } else if (pax1.periodic) {
            cell_seeded[static_cast<size_t>(i * n + (n - 1))] = true;
          }
        }
      }
      if (j < edges1) {
        const int j1 = (j + 1) % n;
        const double qb[2] = { qa[0], qa[1] + step1 };
        if (process_edge_evals(qa, a, qb, grid[static_cast<size_t>(i * n + j1)])) {
          cell_seeded[static_cast<size_t>(i * n + j)] = true;
          if (i > 0) {
            cell_seeded[static_cast<size_t>((i - 1) * n + j)] = true;
          } else if (pax0.periodic) {
            cell_seeded[static_cast<size_t>((n - 1) * n + j)] = true;
          }
        }
      }
    }
  }

  // Refine cliff-adjacent (mixed valid/invalid) coarse cells whose coarse edges produced no seed
  // so narrow cliff-hugging branches (e.g. C16 lower tails under arbitrary sun azimuth phase) are
  // reliably seeded without raising the global grid cost.
  auto refine_mixed_cell = [&](auto& self, double q0, double q1, double d0, double d1, const Eval2DJet1& c00,
                               const Eval2DJet1& c10, const Eval2DJet1& c01, const Eval2DJet1& c11, int depth) -> void {
    const double h0 = 0.5 * d0;
    const double h1 = 0.5 * d1;
    const double q_m0[2] = { q0 + h0, q1 };
    const double q_m1[2] = { q0 + h0, q1 + d1 };
    const double q_0m[2] = { q0, q1 + h1 };
    const double q_1m[2] = { q0 + d0, q1 + h1 };
    const double q_mm[2] = { q0 + h0, q1 + h1 };

    Eval2DJet1 v[3][3];
    v[0][0] = c00;
    v[2][0] = c10;
    v[0][2] = c01;
    v[2][2] = c11;
    v[1][0] = Evaluate2DJet1(table, slots, slot_count, refractive_index, sun_dir, manifold, ax0, ax1, q_m0);
    v[1][2] = Evaluate2DJet1(table, slots, slot_count, refractive_index, sun_dir, manifold, ax0, ax1, q_m1);
    v[0][1] = Evaluate2DJet1(table, slots, slot_count, refractive_index, sun_dir, manifold, ax0, ax1, q_0m);
    v[2][1] = Evaluate2DJet1(table, slots, slot_count, refractive_index, sun_dir, manifold, ax0, ax1, q_1m);
    v[1][1] = Evaluate2DJet1(table, slots, slot_count, refractive_index, sun_dir, manifold, ax0, ax1, q_mm);

    bool found = false;
    for (int a = 0; a < 3; a++) {
      for (int b = 0; b < 3; b++) {
        const double qa[2] = { q0 + static_cast<double>(a) * h0, q1 + static_cast<double>(b) * h1 };
        if (a < 2) {
          const double qb[2] = { qa[0] + h0, qa[1] };
          if (process_edge_evals(qa, v[a][b], qb, v[a + 1][b])) {
            found = true;
          }
        }
        if (b < 2) {
          const double qb[2] = { qa[0], qa[1] + h1 };
          if (process_edge_evals(qa, v[a][b], qb, v[a][b + 1])) {
            found = true;
          }
        }
      }
    }
    if (found || depth + 1 >= 2) {
      return;
    }
    for (int a = 0; a < 2; a++) {
      for (int b = 0; b < 2; b++) {
        const int vc = (v[a][b].valid ? 1 : 0) + (v[a + 1][b].valid ? 1 : 0) + (v[a][b + 1].valid ? 1 : 0) +
                       (v[a + 1][b + 1].valid ? 1 : 0);
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
    const double q0 = grid_coord(i, pax0);
    for (int j = 0; j < edges1; j++) {
      if (cell_seeded[static_cast<size_t>(i * n + j)]) {
        continue;
      }
      const int j1 = (j + 1) % n;
      const Eval2DJet1& c00 = grid[static_cast<size_t>(i * n + j)];
      const Eval2DJet1& c10 = grid[static_cast<size_t>(i1 * n + j)];
      const Eval2DJet1& c01 = grid[static_cast<size_t>(i * n + j1)];
      const Eval2DJet1& c11 = grid[static_cast<size_t>(i1 * n + j1)];
      const int vc = (c00.valid ? 1 : 0) + (c10.valid ? 1 : 0) + (c01.valid ? 1 : 0) + (c11.valid ? 1 : 0);
      if (vc > 0 && vc < 4) {
        refine_mixed_cell(refine_mixed_cell, q0, grid_coord(j, pax1), step0, step1, c00, c10, c01, c11, 0);
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

  std::sort(seeds.begin(), seeds.end(), [](const Seed2D& a, const Seed2D& b) { return a.priority > b.priority; });
  result.coverage.failed_seeds = failed_seeds;

  // Stage 3: Bidirectional predictor-corrector continuation from each uncovered seed.
  for (const Seed2D& seed : seeds) {
    if (CheckStop(stop)) {
      result.stop_requested = true;
      result.coverage.budget_truncated = true;
      break;
    }
    if (IsSeedCoveredByCurves(seed, ax0, ax1, manifold, result.curves, params.dedup_tol)) {
      continue;
    }
    // Check max_curves ONLY when an uncovered seed would genuinely start another curve.
    if (static_cast<int>(result.curves.size()) >= params.max_curves) {
      result.coverage.budget_truncated = true;
      break;
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
