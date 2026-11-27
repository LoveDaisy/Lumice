#ifndef LUMICE_ANALYTIC_CAUSTIC_CONTINUATION_HPP_
#define LUMICE_ANALYTIC_CAUSTIC_CONTINUATION_HPP_

// Continuous 1D family tracing and 2D predictor-corrector implicit curve continuation on J(q) = 0.
//
// Operates strictly on a single literal face sequence (no crystal symmetry expansion). Uses exact
// first-order (Jet<1>, Jet<2>) and second-order (Jet2<2>) forward-mode automatic differentiation
// through TracePathChain — zero finite differences for J or grad J.
//
// Internal analytic header: not part of the C ABI.

#include <vector>

#include "analytic/discovery.hpp"
#include "analytic/entry_measure.hpp"
#include "analytic/path_evaluation.hpp"
#include "analytic/pose_curve.hpp"

namespace lumice::analytic {

// Tuning parameters and budgets for 1D pose-manifold tracing.
struct Family1DParams {
  int scan_samples = 360;           // initial uniform scan along the free axis
  double max_step = 0.05;           // maximum parameter step (radians)
  double min_step = 1e-6;           // minimum adaptive step (radians)
  double max_turning_angle = 0.15;  // sky-tangent turning budget per step (radians)
  double speed_tol = 1e-6;          // sky_speed <= speed_tol => point-image classification
  double stationary_tol = 1e-5;     // ||dx/dt|| ceiling to confirm a bisected stationary point
  int bisection_iters = 52;         // exact event bisection iterations
  int max_nodes_per_branch = 8000;  // node budget per connected valid interval
};

// Tuning parameters and budgets for 2D J(q) = 0 caustic continuation.
//
// The 2D Newton corrector and continuation loop operate on the Snell-desingularized defining
// function J_reg(q) = w(q) * J(q), where w(q) = entry_disc * exit_disc for slot_count >= 2 (1 for
// 1-face reflection). J_reg shares the exact zero set of J on the valid domain while keeping
// ||grad J_reg|| = O(1) all the way to grazing Snell cliffs (exit_disc -> 0).
struct CausticContinuationParams {
  int seed_grid_n = 36;                 // coarse Jet<2> pre-scan resolution per free axis
  double rank_tol = 1e-7;               // sigma2 <= rank_tol * sigma1 => collapsed (rank <= 1)
  double residual_tol = 1e-12;          // O(1) normal parameter-space distance target
                                        // |J_reg(q)| / ||grad J_reg(q)|| (radians); away from
                                        // grazing cliffs (w = O(1)), implies raw |J| <= O(tol)
  double grad_tol = 1e-9;               // ||grad J_reg|| floor (below => kLocusSingularity)
  double speed_tol = 1e-9;              // sky_speed <= speed_tol * sigma1 => point-image
  double pole_tol = 1e-9;               // ||omega_0 x omega_1|| floor (below => Euler pole boundary)
  double initial_step = 0.02;           // initial continuation step (radians)
  double min_step = 1e-6;               // minimum step before boundary / singularity stop (radians, >= 1e-10)
  double max_step = 0.04;               // maximum continuation step (radians)
  double max_turning_angle = 0.15;      // tangent turning budget per step (radians)
  double event_slowdown_margin = 0.02;  // optical margin threshold for step slowdown
  int max_newton_iters = 12;            // orthogonal Newton iterations per step
  int max_steps_per_branch = 8000;      // step budget per half-branch
  int max_curves = 64;                  // maximum traced curves returned
  double dedup_tol = 0.04;              // parameter-space deduplication threshold (radians)
  double closure_tol = 0.05;            // parameter-space closure threshold (radians)
  int bisection_iters = 52;             // exact event bisection iterations
};

// Validates that every field of Family1DParams / CausticContinuationParams is finite, positive
// where required, within the engineering valid domain (min_step >= 1e-10 rad), and properly
// ordered (e.g. 1e-10 <= min_step <= initial_step <= max_step).
bool ValidateParams(const Family1DParams& params);
bool ValidateParams(const CausticContinuationParams& params);

// Aggregate output of TraceFamily1D and TraceCausticLocus.
struct CausticTraceResult {
  std::vector<PoseCurve> curves;
  CurveCoverage coverage;
  int generic_rank = -1;  // -1 if no valid sample, else 0, 1, or 2
  bool stop_requested = false;
};

// Traces a 1D PoseManifold (e.g. Plate, Parry) over its valid intervals, bisecting optical/domain
// boundaries, stationary points (||dx/dt|| = 0), internal TIR onsets, and optional corridor cutoffs.
// `sun_dir` is a unit vector pointing toward the sun (sky = -outgoing, incident = -sun_dir).
CausticTraceResult TraceFamily1D(const FaceNormalTable& table, const int* slots, int slot_count,
                                 double refractive_index, const double sun_dir[3], const PoseManifold& manifold,
                                 Corridor* corridor = nullptr, const Family1DParams& params = {},
                                 const DiscoveryStop* stop = nullptr);

// Traces the 2D caustic locus J(q) = 0 on a 2D PoseManifold (e.g. Column, Lowitz, TiltedSpin) via
// coarse Jet<2> pre-scan + bidirectional predictor-corrector continuation with exact Jet2<2>
// J and grad J. Rank-deficient 2D families (sigma2 <= rank_tol * sigma1 everywhere) are returned
// cleanly as PoseCurveKind::kCollapsed2D without invoking J = 0 continuation.
// `sun_dir` is a unit vector pointing toward the sun (sky = -outgoing, incident = -sun_dir).
CausticTraceResult TraceCausticLocus(const FaceNormalTable& table, const int* slots, int slot_count,
                                     double refractive_index, const double sun_dir[3], const PoseManifold& manifold,
                                     Corridor* corridor = nullptr, const CausticContinuationParams& params = {},
                                     const DiscoveryStop* stop = nullptr);

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_CAUSTIC_CONTINUATION_HPP_
