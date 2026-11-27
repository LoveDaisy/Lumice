#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "analytic/caustic_internal.hpp"
#include "analytic/jet.hpp"
#include "analytic/jet2.hpp"
#include "analytic/path_chain.hpp"
#include "analytic/so3.hpp"

namespace lumice::analytic::caustic_detail {
namespace {

template <typename JetT>
bool TraceChain2D(const Context2D& ctx, const double q_in[2], double param_out[3], double pose_out[9],
                  double* gen_cross_out, OpticalMarginVerdict* margin_out, JetT outgoing[3],
                  ChainInterfaceDiagnostics<JetT>* diag_out, bool require_margin = true) {
  for (int k = 0; k < 3; k++) {
    param_out[k] = ctx.manifold.axes[k].value;
  }
  param_out[ctx.ax0] = WrapAxis(q_in[0], ctx.manifold.axes[ctx.ax0]);
  param_out[ctx.ax1] = WrapAxis(q_in[1], ctx.manifold.axes[ctx.ax1]);
  *gen_cross_out = GeneratorCrossNorm2D(param_out, ctx.ax0, ctx.ax1);

  JetT ang[3];
  for (int k = 0; k < 3; k++) {
    if (k == ctx.ax0) {
      ang[k] = JetT::Variable(param_out[k], 0);
    } else if (k == ctx.ax1) {
      ang[k] = JetT::Variable(param_out[k], 1);
    } else {
      ang[k] = JetT(param_out[k]);
    }
  }
  JetT pose_j[9];
  EulerPose(ang[0], ang[1], ang[2], pose_j);
  for (int i = 0; i < 9; i++) {
    pose_out[i] = pose_j[i].a;
  }

  const JetT incident[3] = { JetT(-ctx.sun_dir[0]), JetT(-ctx.sun_dir[1]), JetT(-ctx.sun_dir[2]) };
  ChainDomain domain;
  double margin_buf[kMaxDomainMargins]{};
  domain.margins = margin_buf;
  const bool chain_ok = TracePathChain<JetT, ChainEvaluation::kFirstFailure>(
      ctx.table, ctx.slots, ctx.slot_count, JetT(ctx.refractive_index), incident, pose_j, outgoing, nullptr, &domain,
      diag_out);
  *margin_out = ClassifyChainMargins(chain_ok, domain, ctx.slot_count);
  return require_margin ? margin_out->valid : chain_ok;
}

// Single-authority O(1) normal parameter-space convergence predicate shared by ProjectOntoCaustic
// and SolveOnDomainBoundary2D. Takes at least one Newton polish step when iter == 0 and param_dist
// exceeds the float64 ULP floor so small-step linear predictors never accumulate unprojected drift.
struct ConvergenceState {
  bool tol_ok = false;
  bool ulp_ok = false;
  bool converged_now = false;
};

ConvergenceState CheckO1Convergence(double abs_j_reg, double grad_reg_norm, double q_scale, double residual_tol,
                                    int iter, int max_newton_iters) {
  const double param_dist =
      (grad_reg_norm > 0.0) ? (abs_j_reg / grad_reg_norm) : std::numeric_limits<double>::infinity();
  const bool tol_ok = (param_dist <= residual_tol);
  const bool ulp_ok = (param_dist <= 5e-15 * q_scale);
  const bool converged_now = (tol_ok && (iter > 0 || ulp_ok)) || (iter == max_newton_iters && (tol_ok || ulp_ok));
  return { tol_ok, ulp_ok, converged_now };
}

bool IsStepAtFloat64Ulp(double step_norm, double q_scale, const ConvergenceState& cv) {
  return step_norm <= 2.5e-16 * q_scale && (cv.tol_ok || cv.ulp_ok);
}

// O(1)-conditioned normal parameter-space uncertainty at a converged J_reg = 0 node.
// Since J_reg = w * J, the float64 rounding floor of J_reg scales by w_val:
//   - Away from cliffs: w_val cancels grad_reg_norm = w_val * grad_norm, avoiding 1/w inflation.
//   - At grazing cliffs (d -> 0): w_val * sigma1^2 = O(d) * O(1/d) = O(1), keeping the floor O(eps).
// The outer 16 * eps radian floor keeps constant-image mirrors cusp-free even at residual_tol = 1e-16,
// while using actual |J_reg| (rather than inflating to residual_tol) preserves micro-scale cusps.
double ComputeRegularizedPoseErr(const Eval2DJet2& e, double grad_tol) {
  const double eps = std::numeric_limits<double>::epsilon();
  const double j_reg_floor = 16.0 * eps * e.w_val * std::max(e.sigma1 * e.sigma1, 1.0);
  return std::max(16.0 * eps, std::max(std::fabs(e.j_reg), j_reg_floor) / std::max(e.grad_reg_norm, grad_tol));
}

void TangentFromGrad(const double grad_j_reg[2], double grad_reg_norm, int dir_sign, double tau[2]) {
  tau[0] = static_cast<double>(dir_sign) * (-grad_j_reg[1] / grad_reg_norm);
  tau[1] = static_cast<double>(dir_sign) * (grad_j_reg[0] / grad_reg_norm);
}

void SkyVelocity2D(const Eval2DJet2& e, const double tau[2], double v_sky[3]) {
  for (int i = 0; i < 3; i++) {
    v_sky[i] = tau[0] * e.x1[i] + tau[1] * e.x2[i];
  }
}

PoseCurveNode NodeFromEval2D(const Eval2DJet2& e, int ax0, int ax1, const double tau[2], double arclength) {
  PoseCurveNode node;
  so3::Copy3(e.param, node.param);
  so3::Copy3(e.u, node.u);
  so3::Copy3(e.sky, node.sky);
  node.tangent[ax0] = tau[0];
  node.tangent[ax1] = tau[1];
  std::copy_n(e.pose, 9, node.pose);
  double v_sky[3];
  SkyVelocity2D(e, tau, v_sky);
  node.sky_speed = so3::Norm3(v_sky);
  node.sigma1 = e.sigma1;
  node.sigma2 = e.sigma2;
  node.arclength = arclength;
  return node;
}

// Projects onto J_reg = 0 with one free coordinate pinned to `bound_val` (exact domain boundary).
bool SolveOnDomainBoundary2D(const Context2D& ctx, int pinned_slot, double bound_val, double other_start,
                             double max_drift, double q_out[2], Eval2DJet2* eval_out) {
  double q[2];
  q[pinned_slot] = bound_val;
  q[1 - pinned_slot] = other_start;
  const PoseAxis& other_ax = (pinned_slot == 0) ? ctx.manifold.axes[ctx.ax1] : ctx.manifold.axes[ctx.ax0];
  const double q_scale = std::max(1.0, std::hypot(bound_val, other_start));

  for (int iter = 0; iter <= ctx.params.max_newton_iters; iter++) {
    if (!InAxisBounds(q[1 - pinned_slot], other_ax)) {
      return false;
    }
    const Eval2DJet2 e = Evaluate2DJet2(ctx, q, nullptr, false);
    if (!e.valid) {
      return false;
    }
    const ConvergenceState cv = CheckO1Convergence(std::fabs(e.j_reg), e.grad_reg_norm, q_scale,
                                                   ctx.params.residual_tol, iter, ctx.params.max_newton_iters);
    auto finish = [&](bool ok) {
      if (ok) {
        q_out[0] = q[0];
        q_out[1] = q[1];
        *eval_out = e;
      }
      return ok;
    };
    if (cv.converged_now) {
      return finish(true);
    }
    if (iter == ctx.params.max_newton_iters) {
      break;
    }
    const double deriv = e.grad_j_reg[1 - pinned_slot];
    if (std::fabs(deriv) < ctx.params.grad_tol) {
      return finish(cv.tol_ok);
    }
    const double step = -e.j_reg / deriv;
    if (!std::isfinite(step) || std::fabs(step) > max_drift) {
      return finish(cv.tol_ok);
    }
    if (IsStepAtFloat64Ulp(std::fabs(step), q_scale, cv)) {
      return finish(true);
    }
    const double q_next = q[1 - pinned_slot] + step;
    if (std::fabs(q_next - other_start) > max_drift || !InAxisBounds(q_next, other_ax)) {
      return finish(cv.tol_ok);
    }
    q[1 - pinned_slot] = q_next;
  }
  return false;
}

// Bisects along [q_curr, q_curr + h * tau_curr] while projecting onto J_reg = 0.
bool BisectBoundaryOnLocus(const Context2D& ctx, const double q_curr[2], const double tau_curr[2], double h,
                           double q_bnd[2], Eval2DJet2* eval_bnd, int* failing_margin_out) {
  double t_lo = 0.0;
  double t_hi = 1.0;
  q_bnd[0] = q_curr[0];
  q_bnd[1] = q_curr[1];
  bool found_advance = false;
  int fail_margin = *failing_margin_out;
  const double drift_cap = std::max(4.0 * h, 1e-4);

  for (int it = 0; it < ctx.params.bisection_iters; it++) {
    const double t_mid = 0.5 * (t_lo + t_hi);
    const double q_guess[2] = { q_curr[0] + t_mid * h * tau_curr[0], q_curr[1] + t_mid * h * tau_curr[1] };
    const ProjectResult pr = ProjectOntoCaustic(ctx, q_guess, drift_cap);
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

// Evaluates a scalar event predicate along [q_a, q_b] while projecting each midpoint onto J_reg = 0.
bool BisectOnLocusEvent(const Context2D& ctx, const double q_a[2], const double q_b[2], const double tau_a[2],
                        int dir_sign, FunctionRef<bool(const Eval2DJet2&, const double[2])> side_a_pred, double q_ev[2],
                        Eval2DJet2* eval_ev, double tau_ev[2]) {
  double qa[2] = { q_a[0], q_a[1] };
  double qb[2] = { q_b[0], q_b[1] };
  const double drift_cap = std::max(2.0 * std::hypot(q_b[0] - q_a[0], q_b[1] - q_a[1]), 1e-4);
  bool have_mid = false;
  Eval2DJet2 best_eval;
  double best_q[2] = { q_a[0], q_a[1] };
  double best_tau[2] = { tau_a[0], tau_a[1] };

  for (int it = 0; it < ctx.params.bisection_iters; it++) {
    const double q_mid[2] = { 0.5 * (qa[0] + qb[0]), 0.5 * (qa[1] + qb[1]) };
    const ProjectResult pr = ProjectOntoCaustic(ctx, q_mid, drift_cap);
    if (pr.outcome != ProjectOutcome::kConverged || pr.eval.grad_reg_norm < ctx.params.grad_tol) {
      break;
    }
    double tau_m[2];
    TangentFromGrad(pr.eval.grad_j_reg, pr.eval.grad_reg_norm, dir_sign, tau_m);
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

    double* dst = side_a_pred(pr.eval, tau_m) ? qa : qb;
    dst[0] = pr.q[0];
    dst[1] = pr.q[1];
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

}  // namespace

Eval2DJet1 Evaluate2DJet1(const Context2D& ctx, const double q_in[2], bool require_margin) {
  Eval2DJet1 out;
  Jet<2> outgoing[3];
  if (!TraceChain2D<Jet<2>>(ctx, q_in, out.param, out.pose, &out.generator_cross, &out.margin, outgoing, nullptr,
                            require_margin)) {
    return out;
  }
  for (int i = 0; i < 3; i++) {
    if (!std::isfinite(outgoing[i].a) || !std::isfinite(outgoing[i].v[0]) || !std::isfinite(outgoing[i].v[1])) {
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

// Exact closed-form J, grad J, J_reg, and grad J_reg via a single Jet2<2> TracePathChain call.
Eval2DJet2 Evaluate2DJet2(const Context2D& ctx, const double q_in[2], Corridor* corridor, bool eval_corridor) {
  Eval2DJet2 out;
  Jet2<2> outgoing[3];
  ChainInterfaceDiagnostics<Jet2<2>> diag;
  if (!TraceChain2D<Jet2<2>>(ctx, q_in, out.param, out.pose, &out.generator_cross, &out.margin, outgoing, &diag)) {
    return out;
  }
  WorldToBody(out.pose, ctx.sun_dir, out.u);

  double x11[3];
  double x12[3];
  double x22[3];
  for (int i = 0; i < 3; i++) {
    if (!std::isfinite(outgoing[i].a) || !std::isfinite(outgoing[i].v[0]) || !std::isfinite(outgoing[i].v[1]) ||
        !std::isfinite(outgoing[i].h[0][0]) || !std::isfinite(outgoing[i].h[0][1]) ||
        !std::isfinite(outgoing[i].h[1][1])) {
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

  auto d_j_comp = [&](const double d1[3], const double d2[3]) {
    double ta[3];
    double tb[3];
    so3::Cross3(d1, out.x2, ta);
    so3::Cross3(out.x1, d2, tb);
    const double sum[3] = { ta[0] + tb[0], ta[1] + tb[1], ta[2] + tb[2] };
    return so3::Dot3(out.sky, sum);
  };
  out.grad_j[0] = d_j_comp(x11, x12);
  out.grad_j[1] = d_j_comp(x12, x22);
  out.grad_norm = std::hypot(out.grad_j[0], out.grad_j[1]);
  if (!std::isfinite(out.j_val) || !std::isfinite(out.grad_norm)) {
    return out;
  }

  double w_val = 1.0;
  double grad_w[2] = { 0.0, 0.0 };
  if (ctx.slot_count >= 2) {
    const Jet2<2> w_jet = diag.discriminant[0] * diag.discriminant[ctx.slot_count - 1];
    w_val = w_jet.a;
    grad_w[0] = w_jet.v[0];
    grad_w[1] = w_jet.v[1];
  }
  out.w_val = w_val;
  out.j_reg = w_val * out.j_val;
  out.grad_j_reg[0] = w_val * out.grad_j[0] + out.j_val * grad_w[0];
  out.grad_j_reg[1] = w_val * out.grad_j[1] + out.j_val * grad_w[1];
  out.grad_reg_norm = std::hypot(out.grad_j_reg[0], out.grad_j_reg[1]);
  if (!std::isfinite(out.j_reg) || !std::isfinite(out.grad_reg_norm)) {
    return out;
  }
  for (int k = 1; k < ctx.slot_count - 1; k++) {
    out.internal_disc[k] = diag.discriminant[k].a;
  }
  out.valid = true;
  if (eval_corridor) {
    out.corridor_open = EvaluateCorridorOpen(corridor, out.pose, ctx.sun_dir, ctx.refractive_index);
  }
  return out;
}

// Safeguarded orthogonal 1D Newton projection onto J_reg(q) = 0 from q_start (unwrapped).
ProjectResult ProjectOntoCaustic(const Context2D& ctx, const double q_start[2], double max_total_drift) {
  ProjectResult res;
  res.q[0] = q_start[0];
  res.q[1] = q_start[1];
  const PoseAxis& pax0 = ctx.manifold.axes[ctx.ax0];
  const PoseAxis& pax1 = ctx.manifold.axes[ctx.ax1];

  if (!InAxisBounds(res.q[0], pax0)) {
    res.outcome = ProjectOutcome::kDomainCrossed;
    res.crossed_axis = ctx.ax0;
    return res;
  }
  if (!InAxisBounds(res.q[1], pax1)) {
    res.outcome = ProjectOutcome::kDomainCrossed;
    res.crossed_axis = ctx.ax1;
    return res;
  }
  res.eval = Evaluate2DJet2(ctx, res.q, nullptr, false);
  if (!res.eval.valid) {
    res.outcome = ProjectOutcome::kOpticalInvalid;
    res.failing_margin = res.eval.margin.failing_index;
    return res;
  }
  if (res.eval.generator_cross < ctx.params.pole_tol) {
    res.outcome = ProjectOutcome::kPoleHit;
    res.crossed_axis = PoseManifold::kAxisZenith;
    return res;
  }

  const double q_scale = std::max(1.0, std::hypot(res.q[0], res.q[1]));

  for (int iter = 0; iter <= ctx.params.max_newton_iters; iter++) {
    const double abs_j_reg = std::fabs(res.eval.j_reg);
    const ConvergenceState cv = CheckO1Convergence(abs_j_reg, res.eval.grad_reg_norm, q_scale, ctx.params.residual_tol,
                                                   iter, ctx.params.max_newton_iters);

    if (cv.converged_now) {
      res.outcome = ProjectOutcome::kConverged;
      res.iterations = iter;
      return res;
    }
    if (iter == ctx.params.max_newton_iters) {
      break;
    }
    if (res.eval.grad_reg_norm < ctx.params.grad_tol) {
      res.outcome = cv.tol_ok ? ProjectOutcome::kConverged : ProjectOutcome::kSingularity;
      res.iterations = iter;
      return res;
    }
    const double scale = -res.eval.j_reg / (res.eval.grad_reg_norm * res.eval.grad_reg_norm);
    const double dq0 = scale * res.eval.grad_j_reg[0];
    const double dq1 = scale * res.eval.grad_j_reg[1];
    const double step_norm = std::hypot(dq0, dq1);
    if (!std::isfinite(step_norm) || step_norm > std::max(4.0 * max_total_drift, 0.25)) {
      res.outcome = cv.tol_ok ? ProjectOutcome::kConverged : ProjectOutcome::kDiverged;
      res.iterations = iter;
      return res;
    }
    if (IsStepAtFloat64Ulp(step_norm, q_scale, cv)) {
      res.outcome = ProjectOutcome::kConverged;
      res.iterations = iter;
      return res;
    }

    bool step_accepted = false;
    ProjectOutcome last_fail = ProjectOutcome::kDiverged;
    int last_axis = -1;
    int last_margin = -1;
    double alpha = 1.0;
    for (int bt = 0; bt < 8; bt++, alpha *= 0.5) {
      const double q_try[2] = { res.q[0] + alpha * dq0, res.q[1] + alpha * dq1 };
      if (std::hypot(q_try[0] - q_start[0], q_try[1] - q_start[1]) > max_total_drift) {
        last_fail = ProjectOutcome::kDiverged;
        continue;
      }
      if (!InAxisBounds(q_try[0], pax0) || !InAxisBounds(q_try[1], pax1)) {
        last_fail = ProjectOutcome::kDomainCrossed;
        last_axis = !InAxisBounds(q_try[0], pax0) ? ctx.ax0 : ctx.ax1;
        continue;
      }
      const Eval2DJet2 e_try = Evaluate2DJet2(ctx, q_try, nullptr, false);
      if (!e_try.valid) {
        last_fail = ProjectOutcome::kOpticalInvalid;
        last_margin = e_try.margin.failing_index;
        continue;
      }
      if (e_try.generator_cross < ctx.params.pole_tol) {
        last_fail = ProjectOutcome::kPoleHit;
        last_axis = PoseManifold::kAxisZenith;
        continue;
      }
      const double try_dist = (e_try.grad_reg_norm > 0.0) ? (std::fabs(e_try.j_reg) / e_try.grad_reg_norm) :
                                                            std::numeric_limits<double>::infinity();
      if (std::fabs(e_try.j_reg) < abs_j_reg || try_dist <= ctx.params.residual_tol) {
        res.q[0] = q_try[0];
        res.q[1] = q_try[1];
        res.eval = e_try;
        step_accepted = true;
        break;
      }
      last_fail = ProjectOutcome::kDiverged;
    }

    if (!step_accepted) {
      res.outcome =
          (cv.tol_ok || (last_fail == ProjectOutcome::kDiverged && cv.ulp_ok)) ? ProjectOutcome::kConverged : last_fail;
      res.crossed_axis = last_axis;
      res.failing_margin = last_margin;
      res.iterations = iter;
      return res;
    }
  }
  res.outcome = ProjectOutcome::kDiverged;
  res.iterations = ctx.params.max_newton_iters;
  return res;
}

bool IsSeedCoveredByCurves(const Context2D& ctx, const Seed2D& seed, const std::vector<PoseCurve>& curves) {
  const double dedup_tol = ctx.params.dedup_tol;
  const PoseAxis& pax0 = ctx.manifold.axes[ctx.ax0];
  const PoseAxis& pax1 = ctx.manifold.axes[ctx.ax1];
  double seed_tau[2] = { 0.0, 0.0 };
  double seed_nrm[2] = { 0.0, 0.0 };
  if (seed.eval.grad_reg_norm > 0.0) {
    seed_nrm[0] = seed.eval.grad_j_reg[0] / seed.eval.grad_reg_norm;
    seed_nrm[1] = seed.eval.grad_j_reg[1] / seed.eval.grad_reg_norm;
    seed_tau[0] = -seed_nrm[1];
    seed_tau[1] = seed_nrm[0];
  }
  const double branch_tol = std::max(1e-7, 100.0 * ctx.params.residual_tol);

  for (const PoseCurve& curve : curves) {
    const size_t n = curve.nodes.size();
    if (n == 0) {
      continue;
    }
    if (n == 1) {
      const double p[2] = { curve.nodes[0].param[ctx.ax0], curve.nodes[0].param[ctx.ax1] };
      if (Dist2D(seed.q, p, pax0, pax1) <= std::min(branch_tol, std::max(dedup_tol, 1e-10))) {
        return true;
      }
      continue;
    }
    for (size_t i = 0; i + 1 < n; i++) {
      const PoseCurveNode& na = curve.nodes[i];
      const PoseCurveNode& nb = curve.nodes[i + 1];
      const double ax = DeltaAxis(seed.q[0], na.param[ctx.ax0], pax0);
      const double ay = DeltaAxis(seed.q[1], na.param[ctx.ax1], pax1);
      const double sx = DeltaAxis(nb.param[ctx.ax0], na.param[ctx.ax0], pax0);
      const double sy = DeltaAxis(nb.param[ctx.ax1], na.param[ctx.ax1], pax1);
      const double s2 = sx * sx + sy * sy;
      const double t = (s2 > 0.0) ? std::clamp((ax * sx + ay * sy) / s2, 0.0, 1.0) : 0.0;
      const double dist = std::hypot(ax - t * sx, ay - t * sy);
      if (dist <= 1e-11) {
        return true;
      }
      if (!(dedup_tol > 0.0) || dist >= dedup_tol || !(seed.eval.grad_reg_norm > 0.0)) {
        continue;
      }
      const double seg_t0 = (1.0 - t) * na.tangent[ctx.ax0] + t * nb.tangent[ctx.ax0];
      const double seg_t1 = (1.0 - t) * na.tangent[ctx.ax1] + t * nb.tangent[ctx.ax1];
      const double stn = std::hypot(seg_t0, seg_t1);
      if (!(stn > 0.0) || std::fabs((seg_t0 * seed_tau[0] + seg_t1 * seed_tau[1]) / stn) <= 0.85) {
        continue;
      }
      const double seg_len = std::sqrt(s2);
      const double p_proj[2] = { WrapAxis(na.param[ctx.ax0] + t * sx, pax0),
                                 WrapAxis(na.param[ctx.ax1] + t * sy, pax1) };
      const ProjectResult pm = ProjectOntoCaustic(ctx, p_proj, std::max(0.5 * seg_len, 2.0 * dist));
      if (pm.outcome == ProjectOutcome::kConverged && pm.eval.grad_reg_norm >= ctx.params.grad_tol &&
          (pm.eval.grad_j_reg[0] * seed.eval.grad_j_reg[0] + pm.eval.grad_j_reg[1] * seed.eval.grad_j_reg[1]) > 0.0) {
        const double tdx = DeltaAxis(seed.q[0], pm.q[0], pax0);
        const double tdy = DeltaAxis(seed.q[1], pm.q[1], pax1);
        if (std::fabs(tdx * seed_nrm[0] + tdy * seed_nrm[1]) <= branch_tol &&
            std::fabs(tdx * seed_tau[0] + tdy * seed_tau[1]) <= std::max(dedup_tol, 0.5 * seg_len + 1e-7)) {
          return true;
        }
      }
    }
  }
  return false;
}

HalfBranch2D TraceHalfBranch2D(const Context2D& ctx, const double q_seed[2], const Eval2DJet2& seed_eval, int dir_sign,
                               bool allow_closure, Corridor* corridor, const DiscoveryStop* stop) {
  HalfBranch2D out;
  const CausticContinuationParams& params = ctx.params;
  const PoseAxis& pax0 = ctx.manifold.axes[ctx.ax0];
  const PoseAxis& pax1 = ctx.manifold.axes[ctx.ax1];

  double tau_seed[2];
  TangentFromGrad(seed_eval.grad_j_reg, seed_eval.grad_reg_norm, dir_sign, tau_seed);

  double q_curr[2] = { q_seed[0], q_seed[1] };
  Eval2DJet2 e_curr = seed_eval;
  e_curr.corridor_open = EvaluateCorridorOpen(corridor, e_curr.pose, ctx.sun_dir, ctx.refractive_index);
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
                          e_curr.grad_reg_norm >= 10.0 * params.grad_tol;
  double last_moving_q[2] = { q_curr[0], q_curr[1] };
  double last_moving_tau[2] = { tau_curr[0], tau_curr[1] };
  double last_moving_e_sky[3] = { e_sky_curr[0], e_sky_curr[1], e_sky_curr[2] };
  double last_moving_s_sky = s_sky_curr;
  double last_moving_arclen = 0.0;

  auto push_node = [&](const Eval2DJet2& e, const double tau[2], double s_val, bool incoming_edge_open) {
    if (!out.nodes.empty()) {
      out.edge_corridor_open.push_back(incoming_edge_open);
    }
    const PoseCurveNode node = NodeFromEval2D(e, ctx.ax0, ctx.ax1, tau, s_val);
    out.nodes.push_back(node);
    const double zero_floor = NumericalZeroSigmaFloor(e.margin.min_effective_margin);
    out.node_stationary.push_back((node.sigma1 <= zero_floor) || (node.sky_speed <= params.speed_tol * node.sigma1));
    out.node_pose_err.push_back(ComputeRegularizedPoseErr(e, params.grad_tol));
  };

  push_node(e_curr, tau_curr, 0.0, e_curr.corridor_open);
  out.first_node_corridor_open = e_curr.corridor_open;

  double h = params.initial_step;
  double max_dist_from_seed = 0.0;
  bool moved_away = false;
  const double cos_max_turn = std::cos(params.max_turning_angle);
  const double margin_lip = MarginLip(ctx.refractive_index);

  auto commit_step = [&](const double q_next_in[2], Eval2DJet2 e_next, const double tau_next[2], double adv) {
    double margin_b = 1.0;
    if (corridor != nullptr) {
      EvaluateCorridorStateAndMargin(corridor, e_next.pose, ctx.sun_dir, ctx.refractive_index, true,
                                     &e_next.corridor_open, &margin_b);
    } else {
      e_next.corridor_open = true;
    }

    std::vector<CurveEvent> step_events;

    double e_sky_next[3]{};
    const bool have_e_sky_next = DominantLeftSingularVector(e_next.x1, e_next.x2, e_sky_next);
    if (have_e_sky_next && have_e_sky_curr && so3::Dot3(e_sky_curr, e_sky_next) < 0.0) {
      for (double& c : e_sky_next) {
        c = -c;
      }
    }
    double s_sky_next = 0.0;
    if (have_e_sky_next) {
      double v_sky[3];
      SkyVelocity2D(e_next, tau_next, v_sky);
      s_sky_next = so3::Dot3(e_sky_next, v_sky);
    }

    // 1. Cusp detection gated against O(1) regularized locus uncertainty.
    const double cusp_speed_floor =
        std::max(params.speed_tol * e_next.sigma1,
                 1e4 * ComputeRegularizedPoseErr(e_next, params.grad_tol) * std::max(e_next.sigma1, 1.0));
    const bool next_moving =
        have_e_sky_next && std::fabs(s_sky_next) > cusp_speed_floor && e_next.grad_reg_norm >= 100.0 * params.grad_tol;
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
            for (double& c : em_sky) {
              c = -c;
            }
          }
          double vm[3];
          SkyVelocity2D(em, tm, vm);
          return (so3::Dot3(em_sky, vm) > 0.0) == sign_a_pos;
        };
        if (BisectOnLocusEvent(ctx, last_moving_q, q_next_in, last_moving_tau, dir_sign, pred, q_cusp, &e_cusp,
                               tau_cusp) &&
            e_cusp.grad_reg_norm >= 100.0 * params.grad_tol) {
          const double ds_c = std::hypot(q_cusp[0] - last_moving_q[0], q_cusp[1] - last_moving_q[1]);
          CurveEvent ev;
          CopyNodeToEvent(NodeFromEval2D(e_cusp, ctx.ax0, ctx.ax1, tau_cusp, last_moving_arclen + ds_c),
                          CurveEventKind::kCusp, -1, &ev);
          step_events.push_back(ev);
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

    const double step_chord = std::hypot(q_next_in[0] - q_curr[0], q_next_in[1] - q_curr[1]);
    const double drift_cap = std::max(2.0 * std::max(adv, step_chord), 1e-4);
    auto project_u = [&](double u, double q_out[2], Eval2DJet2* e_out, double tau_out[2]) {
      const double q_guess[2] = { q_curr[0] + u * (q_next_in[0] - q_curr[0]),
                                  q_curr[1] + u * (q_next_in[1] - q_curr[1]) };
      const ProjectResult pm = ProjectOntoCaustic(ctx, q_guess, drift_cap);
      if (pm.outcome != ProjectOutcome::kConverged || pm.eval.grad_reg_norm < params.grad_tol) {
        return false;
      }
      TangentFromGrad(pm.eval.grad_j_reg, pm.eval.grad_reg_norm, dir_sign, tau_out);
      if (tau_out[0] * tau_curr[0] + tau_out[1] * tau_curr[1] < 0.0) {
        tau_out[0] = -tau_out[0];
        tau_out[1] = -tau_out[1];
      }
      q_out[0] = pm.q[0];
      q_out[1] = pm.q[1];
      *e_out = pm.eval;
      return true;
    };

    // 2. Internal TIR onset detection.
    auto eval_tir_u = [&](double u, int k, double* disc_out) {
      double qm[2];
      Eval2DJet2 em;
      double tm[2];
      if (!project_u(u, qm, &em, tm)) {
        return false;
      }
      *disc_out = em.internal_disc[k];
      return true;
    };
    auto emit_tir_u = [&](double u_tir, int k) {
      double q_tir[2];
      Eval2DJet2 e_tir;
      double tau_tir[2];
      if (project_u(u_tir, q_tir, &e_tir, tau_tir)) {
        const double ds_t = std::hypot(q_tir[0] - q_curr[0], q_tir[1] - q_curr[1]);
        CurveEvent ev;
        CopyNodeToEvent(NodeFromEval2D(e_tir, ctx.ax0, ctx.ax1, tau_tir, arclen + ds_t), CurveEventKind::kTirOnset, k,
                        &ev);
        step_events.push_back(ev);
      }
    };
    DetectStepTirOnsets(ctx.slot_count, e_curr.internal_disc, e_next.internal_disc, adv, margin_lip,
                        params.bisection_iters, eval_tir_u, emit_tir_u);

    // 3. Corridor cutoff detection and splitting on J_reg = 0.
    int cut_cnt = 0;
    double u_cuts[2] = { 0.0, 0.0 };
    if (corridor != nullptr) {
      const double margin_a = EvaluateCorridorOverlapMargin(corridor, e_curr.pose, ctx.sun_dir, ctx.refractive_index);
      auto eval_corr_u = [&](double u, bool need_margin, bool* open_out, double* margin_out) {
        double qm[2];
        Eval2DJet2 em;
        double tm[2];
        if (!project_u(u, qm, &em, tm)) {
          return false;
        }
        EvaluateCorridorStateAndMargin(corridor, em.pose, ctx.sun_dir, ctx.refractive_index, need_margin, open_out,
                                       margin_out);
        return true;
      };
      cut_cnt = FindStepCorridorCrossings(e_curr.corridor_open, margin_a, e_next.corridor_open, margin_b, adv,
                                          params.bisection_iters, eval_corr_u, u_cuts);
    }

    double q_cuts[2][2]{};
    Eval2DJet2 e_cuts[2];
    double tau_cuts[2][2]{};
    bool all_cuts_ok = (cut_cnt > 0);
    for (int c = 0; c < cut_cnt; c++) {
      if (!project_u(u_cuts[c], q_cuts[c], &e_cuts[c], tau_cuts[c])) {
        all_cuts_ok = false;
        break;
      }
    }

    if (all_cuts_ok) {
      double q_prev_p[2] = { q_curr[0], q_curr[1] };
      bool cur_open = e_curr.corridor_open;
      for (int c = 0; c < cut_cnt; c++) {
        const double ds_c = std::hypot(q_cuts[c][0] - q_prev_p[0], q_cuts[c][1] - q_prev_p[1]);
        if (ds_c > 1e-13) {
          arclen += ds_c;
          push_node(e_cuts[c], tau_cuts[c], arclen, cur_open);
          q_prev_p[0] = q_cuts[c][0];
          q_prev_p[1] = q_cuts[c][1];
        }
        CurveEvent ev;
        CopyNodeToEvent(out.nodes.back(), CurveEventKind::kCorridorCutoff, -1, &ev);
        out.events.push_back(ev);
        cur_open = !cur_open;
      }
      const double ds_rem = std::hypot(q_next_in[0] - q_prev_p[0], q_next_in[1] - q_prev_p[1]);
      if (ds_rem > 1e-13) {
        arclen += ds_rem;
        push_node(e_next, tau_next, arclen, cur_open);
      }
    } else {
      if (cut_cnt > 0) {
        out.unresolved_singularities++;
      }
      arclen += adv;
      push_node(e_next, tau_next, arclen, e_curr.corridor_open);
    }

    AppendEventsWithinArclength(step_events, out.nodes.back().arclength, &out.events);
    q_curr[0] = q_next_in[0];
    q_curr[1] = q_next_in[1];
    e_curr = e_next;
    tau_curr[0] = tau_next[0];
    tau_curr[1] = tau_next[1];
    have_e_sky_curr = have_e_sky_next;
    so3::Copy3(e_sky_next, e_sky_curr);
    s_sky_curr = s_sky_next;
  };

  auto finish_boundary = [&](bool got_bnd, const double q_bnd[2], const Eval2DJet2& e_bnd, CurveEventKind kind,
                             int detail_idx) {
    if (got_bnd && e_bnd.grad_reg_norm >= params.grad_tol) {
      const double ds = std::hypot(q_bnd[0] - q_curr[0], q_bnd[1] - q_curr[1]);
      if (ds > 1e-13) {
        double tau_b[2];
        TangentFromGrad(e_bnd.grad_j_reg, e_bnd.grad_reg_norm, dir_sign, tau_b);
        commit_step(q_bnd, e_bnd, tau_b, ds);
      }
    }
    CurveEvent ev;
    CopyNodeToEvent(out.nodes.back(), kind, detail_idx, &ev);
    out.events.push_back(ev);
  };

  for (int step_idx = 0; step_idx < params.max_steps_per_branch; step_idx++) {
    if ((step_idx & 15) == 0 && CheckStop(stop)) {
      out.stop_requested = true;
      out.budget_truncated = true;
      return out;
    }

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
      pr = ProjectOntoCaustic(ctx, q_pred, std::max(2.0 * h, 1e-4));
      if (pr.outcome == ProjectOutcome::kConverged) {
        if (pr.eval.grad_reg_norm < params.grad_tol) {
          pr.outcome = ProjectOutcome::kSingularity;
        } else {
          TangentFromGrad(pr.eval.grad_j_reg, pr.eval.grad_reg_norm, dir_sign, tau_next);
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
            if (so3::Norm3(v_sky_next) > params.speed_tol * pr.eval.sigma1) {
              sky_turn_angle = so3::AngleBetween(v_sky_curr, v_sky_next);
              sky_turn_ok = !(sky_turn_angle > params.max_turning_angle && sky_turn_angle < 2.5);
            }
          }

          const bool at_min_step = !(h > eff_min_step * 2.0);
          if ((adv > 1e-15 && proj > 0.25 * adv && turn_dot >= cos_max_turn && sky_turn_ok) ||
              (at_min_step && adv > 1e-15 && proj > 0.0 && turn_dot > 0.0)) {
            accepted = true;
            break;
          }
          pr.outcome = ProjectOutcome::kDiverged;
        }
      }

      if (h > eff_min_step * 2.0) {
        h = std::max(eff_min_step, 0.5 * h);
        continue;
      }

      if (pr.outcome == ProjectOutcome::kDomainCrossed || pr.outcome == ProjectOutcome::kPoleHit) {
        const int bnd_axis = pr.crossed_axis;
        double q_bnd[2] = { q_curr[0], q_curr[1] };
        Eval2DJet2 e_bnd = e_curr;
        bool got_bnd = false;
        if (pr.outcome == ProjectOutcome::kDomainCrossed) {
          const int pinned_slot = (bnd_axis == ctx.ax0) ? 0 : 1;
          const PoseAxis& b_ax = ctx.manifold.axes[bnd_axis];
          const double bound_val =
              (q_curr[pinned_slot] + h * tau_curr[pinned_slot] < b_ax.min_val) ? b_ax.min_val : b_ax.max_val;
          double other_guess = q_curr[1 - pinned_slot];
          if (std::fabs(tau_curr[pinned_slot]) > 1e-9) {
            other_guess += ((bound_val - q_curr[pinned_slot]) / tau_curr[pinned_slot]) * tau_curr[1 - pinned_slot];
          }
          got_bnd =
              SolveOnDomainBoundary2D(ctx, pinned_slot, bound_val, other_guess, std::max(4.0 * h, 1e-3), q_bnd, &e_bnd);
        }
        if (!got_bnd) {
          int dummy_fail = -1;
          got_bnd = BisectBoundaryOnLocus(ctx, q_curr, tau_curr, h, q_bnd, &e_bnd, &dummy_fail);
        }
        finish_boundary(got_bnd, q_bnd, e_bnd, CurveEventKind::kDomainBoundary, bnd_axis);
        return out;
      }

      if (pr.outcome == ProjectOutcome::kOpticalInvalid) {
        double q_bnd[2] = { q_curr[0], q_curr[1] };
        Eval2DJet2 e_bnd = e_curr;
        int fail_margin = pr.failing_margin;
        const bool got_bnd = BisectBoundaryOnLocus(ctx, q_curr, tau_curr, h, q_bnd, &e_bnd, &fail_margin);
        finish_boundary(got_bnd, q_bnd, e_bnd, CurveEventKind::kOpticalBoundary, fail_margin);
        return out;
      }

      const double q_probe[2] = { q_curr[0] + h * tau_curr[0], q_curr[1] + h * tau_curr[1] };
      const Eval2DJet2 e_probe = Evaluate2DJet2(ctx, q_probe, nullptr, false);
      if (!e_probe.valid) {
        finish_boundary(false, q_curr, e_curr, CurveEventKind::kOpticalBoundary, e_probe.margin.failing_index);
      } else {
        out.unresolved_singularities++;
        finish_boundary(false, q_curr, e_curr, CurveEventKind::kLocusSingularity, -1);
      }
      return out;
    }

    if (!accepted) {
      return out;
    }

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
        h_next = std::min(h_next, std::max(next_eff_min, 0.5 * next_margin / rate));
      }
    }

    for (int slot = 0; slot < 2; slot++) {
      const PoseAxis& ax = (slot == 0) ? pax0 : pax1;
      if (ax.periodic || std::fabs(tau_curr[slot]) <= 1e-9) {
        continue;
      }
      const double d_bnd = (tau_curr[slot] < 0.0) ? (q_curr[slot] - ax.min_val) : (ax.max_val - q_curr[slot]);
      if (d_bnd > 2.0 * next_eff_min && d_bnd < h_next) {
        h_next = std::min(h_next, std::max(next_eff_min, 0.5 * d_bnd / std::fabs(tau_curr[slot])));
      }
    }
    h = std::clamp(h_next, next_eff_min, params.max_step);
  }

  out.budget_truncated = true;
  return out;
}

PoseCurve StitchHalfBranches(const HalfBranch2D& forward, const HalfBranch2D& backward, PoseCurveKind kind,
                             int seed_grid_n, double speed_tol, int unresolved_singularities) {
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
        const bool dup = std::any_of(curve.events.begin(), curve.events.end(), [&](const CurveEvent& ex) {
          return ex.kind == ev.kind && ex.detail_index == ev.detail_index && std::fabs(ex.arclength) <= 1e-12;
        });
        if (!dup) {
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
      for (double& c : node.tangent) {
        c = -c;
      }
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
  BuildSegments(curve.nodes, node_stationary, edge_corridor_open, forward.first_node_corridor_open, speed_tol,
                &curve.segments);
  return curve;
}

PoseCurve StitchCausticCurve(const Context2D& ctx, const HalfBranch2D& forward, const HalfBranch2D& backward,
                             int seed_grid_n) {
  const double speed_tol = ctx.params.speed_tol;
  PoseCurve curve = StitchHalfBranches(forward, backward, PoseCurveKind::kCaustic2D, seed_grid_n, speed_tol,
                                       forward.unresolved_singularities + backward.unresolved_singularities);

  std::vector<double> node_pose_err;
  if (forward.closed || backward.nodes.size() <= 1) {
    node_pose_err = forward.node_pose_err;
  } else {
    node_pose_err.reserve(backward.nodes.size() - 1 + forward.nodes.size());
    for (size_t idx = backward.nodes.size() - 1; idx >= 1; idx--) {
      node_pose_err.push_back(backward.node_pose_err[idx]);
    }
    node_pose_err.insert(node_pose_err.end(), forward.node_pose_err.begin(), forward.node_pose_err.end());
  }

  auto sky_uncertainty_at = [&](size_t idx) {
    const PoseCurveNode& nd = curve.nodes[idx];
    const double pose_err = idx < node_pose_err.size() ? node_pose_err[idx] : 1e-14;
    return std::max(1e-14, 10.0 * pose_err * std::max(nd.sigma1, 1.0));
  };

  if (curve.nodes.size() >= 2 && !curve.segments.empty()) {
    const size_t edge_cnt = curve.nodes.size() - 1;
    std::vector<CurveSegmentKind> edge_kind(edge_cnt, CurveSegmentKind::kArc);
    std::vector<bool> edge_open(edge_cnt, true);
    for (const CurveSegment& seg : curve.segments) {
      for (int e = seg.start_idx; e < seg.end_idx && static_cast<size_t>(e) < edge_cnt; e++) {
        edge_kind[static_cast<size_t>(e)] = seg.kind;
        edge_open[static_cast<size_t>(e)] = seg.corridor_open;
      }
    }
    const double cond_speed_bar = std::max(5.0 * speed_tol, 5e-9);
    const bool loose_tol = (ctx.params.residual_tol > 1e-12);

    // When a caller passes a loose residual_tol, off-locus error can perturb both sky position and
    // tangent speed on an exact constant-image mirror branch. Locally tighten both edge endpoints
    // onto J_reg = 0 before classifying so physical segmentation stays invariant to residual_tol.
    CausticContinuationParams tight_params = ctx.params;
    tight_params.residual_tol = std::min(ctx.params.residual_tol, 1e-13);
    const Context2D tight_ctx{ ctx.table,    ctx.slots, ctx.slot_count, ctx.refractive_index, ctx.sun_dir,
                               ctx.manifold, ctx.ax0,   ctx.ax1,        tight_params };
    auto is_tight_point_image_edge = [&](size_t e, bool require_low_speed) {
      const double q0[2] = { curve.nodes[e].param[ctx.ax0], curve.nodes[e].param[ctx.ax1] };
      const double q1[2] = { curve.nodes[e + 1].param[ctx.ax0], curve.nodes[e + 1].param[ctx.ax1] };
      const double drift = std::max(10.0 * ctx.params.residual_tol, 1e-4);
      const ProjectResult p0 = ProjectOntoCaustic(tight_ctx, q0, drift);
      const ProjectResult p1 = ProjectOntoCaustic(tight_ctx, q1, drift);
      if (p0.outcome != ProjectOutcome::kConverged || p1.outcome != ProjectOutcome::kConverged ||
          p0.eval.grad_reg_norm < ctx.params.grad_tol || p1.eval.grad_reg_norm < ctx.params.grad_tol) {
        return false;
      }
      const double u0 = std::max(
          1e-14, 10.0 * ComputeRegularizedPoseErr(p0.eval, ctx.params.grad_tol) * std::max(p0.eval.sigma1, 1.0));
      const double u1 = std::max(
          1e-14, 10.0 * ComputeRegularizedPoseErr(p1.eval, ctx.params.grad_tol) * std::max(p1.eval.sigma1, 1.0));
      if (so3::AngleBetween(p0.eval.sky, p1.eval.sky) > std::max(u0, u1)) {
        return false;
      }
      if (!require_low_speed) {
        return true;
      }
      double tau0[2];
      double tau1[2];
      TangentFromGrad(p0.eval.grad_j_reg, p0.eval.grad_reg_norm, +1, tau0);
      TangentFromGrad(p1.eval.grad_j_reg, p1.eval.grad_reg_norm, +1, tau1);
      double v0[3];
      double v1[3];
      SkyVelocity2D(p0.eval, tau0, v0);
      SkyVelocity2D(p1.eval, tau1, v1);
      const double max_s1 = std::max(p0.eval.sigma1, p1.eval.sigma1);
      return std::max(so3::Norm3(v0), so3::Norm3(v1)) <= cond_speed_bar * std::max(max_s1, 1.0);
    };

    bool changed = false;
    for (size_t e = 0; e < edge_cnt; e++) {
      if (edge_kind[e] == CurveSegmentKind::kPointImage) {
        continue;
      }
      const double sky_step = so3::AngleBetween(curve.nodes[e].sky, curve.nodes[e + 1].sky);
      const double sky_unc = std::max(sky_uncertainty_at(e), sky_uncertainty_at(e + 1));
      const double max_s1 = std::max(curve.nodes[e].sigma1, curve.nodes[e + 1].sigma1);
      const double max_spd = std::max(curve.nodes[e].sky_speed, curve.nodes[e + 1].sky_speed);
      if (sky_step <= sky_unc && max_spd <= cond_speed_bar * std::max(max_s1, 1.0)) {
        edge_kind[e] = CurveSegmentKind::kPointImage;
        changed = true;
      } else if (loose_tol && sky_step <= std::max(sky_unc, 10.0 * ctx.params.residual_tol * std::max(max_s1, 1.0)) &&
                 is_tight_point_image_edge(e, /*require_low_speed=*/true)) {
        edge_kind[e] = CurveSegmentKind::kPointImage;
        changed = true;
      }
    }
    bool grew = true;
    while (grew) {
      grew = false;
      for (size_t e = 0; e < edge_cnt; e++) {
        if (edge_kind[e] == CurveSegmentKind::kPointImage) {
          continue;
        }
        const bool adj_point = (e > 0 && edge_kind[e - 1] == CurveSegmentKind::kPointImage) ||
                               (e + 1 < edge_cnt && edge_kind[e + 1] == CurveSegmentKind::kPointImage);
        if (!adj_point) {
          continue;
        }
        const double sky_step = so3::AngleBetween(curve.nodes[e].sky, curve.nodes[e + 1].sky);
        const double sky_unc = std::max(sky_uncertainty_at(e), sky_uncertainty_at(e + 1));
        const double max_s1 = std::max(curve.nodes[e].sigma1, curve.nodes[e + 1].sigma1);
        if (sky_step <= sky_unc ||
            (loose_tol && sky_step <= std::max(sky_unc, 10.0 * ctx.params.residual_tol * std::max(max_s1, 1.0)) &&
             is_tight_point_image_edge(e, /*require_low_speed=*/false))) {
          edge_kind[e] = CurveSegmentKind::kPointImage;
          changed = true;
          grew = true;
        }
      }
    }
    if (changed) {
      MergeEdgeSegments(edge_kind, edge_open, &curve.segments);
    }
  }

  if (!curve.events.empty() && curve.nodes.size() >= 3) {
    auto node_in_point_image = [&](int node_idx) {
      return std::any_of(curve.segments.begin(), curve.segments.end(), [node_idx](const CurveSegment& seg) {
        return seg.kind == CurveSegmentKind::kPointImage && node_idx >= seg.start_idx && node_idx <= seg.end_idx;
      });
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
        return ang > sky_uncertainty_at(static_cast<size_t>(idx)) && ang >= 0.05 * nd.sky_speed * ds;
      };
      int left_idx = -1;
      for (int i = static_cast<int>(split_idx) - 1; i >= 0 && !node_in_point_image(i); i--) {
        if (node_resolves_motion(i)) {
          left_idx = i;
          break;
        }
      }
      int right_idx = -1;
      for (int i = static_cast<int>(split_idx); i < static_cast<int>(curve.nodes.size()) && !node_in_point_image(i);
           i++) {
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

    curve.events.erase(
        std::remove_if(curve.events.begin(), curve.events.end(),
                       [&](const CurveEvent& ev) { return ev.kind == CurveEventKind::kCusp && !cusp_resolved(ev); }),
        curve.events.end());
  }
  return curve;
}

}  // namespace lumice::analytic::caustic_detail
