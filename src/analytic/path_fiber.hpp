#ifndef LUMICE_ANALYTIC_PATH_FIBER_HPP_
#define LUMICE_ANALYTIC_PATH_FIBER_HPP_

// The ice-crystal path as a continuation map (fiber_continuation.hpp's Map): LI's path_problem
// (src/lumice_integral/optics.py). Direction is the path chain's outgoing direction (path_chain.hpp,
// the chain EvaluatePath runs), in double or Jet<3>; Domain is the chain's gate walk with LI's two
// continuation adaptations — its margins are the validity margins only (no internal TIR
// discriminant), and an entry or exit Snell discriminant at or below kSnellEventTolerance is already
// the tir_boundary event.
//
// Holds pointers to the caller's table and slots: construct it where they outlive it (one fiber, or
// one batch). No allocation per pose.

#include <limits>

#include "analytic/fiber_continuation.hpp"
#include "analytic/path_chain.hpp"
#include "analytic/path_evaluation.hpp"

namespace lumice::analytic {

// LI optics.SNELL_EVENT_TOLERANCE: within this Snell discriminant of the critical angle the square
// root leaves the corrector unable to converge in the last ~1e-10 of margin, so continuation stops
// there as a tir_boundary event (the arclength cut off is ~sqrt(tolerance / curvature), ~1e-4 rad).
constexpr double kSnellEventTolerance = 1e-8;

static_assert(kMaxFaceCount + 2 <= kMaxDomainMargins, "a path's validity margins must fit a DomainEvaluation");

class IcePathMap {
 public:
  // `slots` holds `slot_count` resolved slots of `table` (ResolveFaceSequence), 2 <= slot_count <=
  // kMaxFaceCount; `incident_direction` is a world unit vector.
  IcePathMap(const FaceNormalTable& table, const int* slots, int slot_count, double refractive_index,
             const double incident_direction[3])
      : table_(&table), slots_(slots), slot_count_(slot_count), refractive_index_(refractive_index),
        incident_{ incident_direction[0], incident_direction[1], incident_direction[2] } {}

  void Domain(const double r[9], DomainEvaluation* out) const;

  // The outgoing direction; NaN where the chain's own gates reject the pose (continuation only asks
  // after Domain accepted it, so this is reached only if a Jet run rounds across a gate the double
  // run passed, and then reports non_finite). The incident direction and the index are constants
  // of the map: seeded into the scalar type with zero derivative.
  template <class S>
  void Direction(const S r[9], S out[3]) const {
    const S incident[3] = { S(incident_[0]), S(incident_[1]), S(incident_[2]) };
    if (!TracePathChain<S>(*table_, slots_, slot_count_, S(refractive_index_), incident, r, out, nullptr, nullptr)) {
      for (int i = 0; i < 3; i++) {
        out[i] = S(std::numeric_limits<double>::quiet_NaN());
      }
    }
  }

 private:
  const FaceNormalTable* table_;
  const int* slots_;
  int slot_count_;
  double refractive_index_;
  double incident_[3];
};

// The wave 2 diagnostics of one pose (LI docs/analytic-parity-fixtures.md section 3.1:
// `branch_margins`, `failed_gate`, `jacobian_available`, `normal_jacobian`, `singular_values`), from
// the same chain EvaluatePath runs and the same fiber_detail::NormalJacobianAt a trace records at each accepted
// pose — so a trace's per-pose arrays can be checked against a fresh evaluation at its poses.
//   valid:        EvaluatePath's `valid` (no Snell event tolerance: that is continuation's, not the
//                 path's).
//   margins:      the validity margins in BranchMarginName order. All BranchMarginCount of them when
//                 valid; when not, the ones the chain evaluated before it stopped.
//   failed_gate:  when not valid, the index of the first margin that is not > 0 (LI's order); -1 when
//                 valid.
//   jacobian:     available exactly when valid (the normal Jacobian exists only on the smooth branch);
//                 otherwise unavailable with NaN numbers.
struct PathDiagnostics {
  bool valid = false;
  int margin_count = 0;
  double margins[kMaxFaceCount + 2]{};
  int failed_gate = -1;
  NormalJacobian jacobian;
};

PathDiagnostics EvaluatePathDiagnostics(const FaceNormalTable& table, const int* slots, int slot_count,
                                        double refractive_index, const double incident_direction[3],
                                        const double pose[9]);

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_PATH_FIBER_HPP_
