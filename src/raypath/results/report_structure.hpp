#ifndef LUMICE_RAYPATH_RESULTS_REPORT_STRUCTURE_HPP_
#define LUMICE_RAYPATH_RESULTS_REPORT_STRUCTURE_HPP_

#include <cmath>
#include <string>
#include <variant>
#include <vector>

#include "analytic/chromatic_types.hpp"
#include "raypath/domain/measure_geometry.hpp"
#include "raypath/results/visibility.hpp"
namespace lumice::raypath::schema3 {

enum class ObjectKind {
  kKind1,              // S1: D_P critical sets (caustics, ring edges; per-wavelength tables)
  kKind1Restricted,    // S1 restricted: family-pinned curves (the contract's canonical closed curve)
  kKind2,              // S2: dU_P gate boundaries
  kKind3,              // S2: weight kink polylines (internal TIR onsets)
  kCorridorClosed,     // S2: the corridor A=0 curve — declared-not-produced in v1 (no v8 producer;
                       //       the LI second batch A6 is its donor)
  kS3SupportBoundary,  // S3: declared-support boundary image
  kS4BranchBoundary,   // S4: A_P branch boundary — declared-not-produced in v1 (open math, owner
                       //       ruling: v1 declares the boundary, does not trace it)
  kS5DensityFeature,   // S5: declared density feature image
  kS6Junction,         // S6: junctions — opportunistic in v1, declared non-exhaustive
};

const char* ObjectKindName(ObjectKind kind);
const std::vector<ObjectKind>& RegisteredObjectKinds();

// ---------------------------------------------------------------------------
// CorroborationState: the MC-corroboration machine, v1 = type + table + the
// `unchecked` default ONLY (666.2 wires the derivation). Registered table like
// the contract's open enums.
// ---------------------------------------------------------------------------
enum class CorroborationState {
  kObserved,
  kConsistent,
  kNotObservedInsufficientEss,
  kNotObservedDespiteSufficientEss,  // the true-divergence red flag
  kUnchecked,                        // v1's constant: no MC corroboration has been consulted
};

const char* CorroborationStateName(CorroborationState state);
const std::vector<CorroborationState>& RegisteredCorroborationStates();

// ---------------------------------------------------------------------------
// The three report buckets (the read-report shape is unchanged: three buckets).
// ---------------------------------------------------------------------------
enum class FeatureBucket { kActual, kCandidate, kUnfinished };

const char* FeatureBucketName(FeatureBucket bucket);

// The bucket derivation, pure, from the two machines that own it (existence x visibility —
// the exhaustive 4x4 grid is the unit test's subject). Fail-closed arms:
//   - an existence that is not kComputed (including an UNKNOWN cast value) -> kUnfinished:
//     a non-computed object never borrows a visibility verdict.
//   - a visibility that is not one of the four known values (cast) on a computed object ->
//     kCandidate: the object IS computed, but nothing certifies it.
FeatureBucket DeriveBucket(ExistenceState existence, VisibilityState visibility);

// ---------------------------------------------------------------------------
// PairedCounterfactual: the C02 diagnostic shape (energy with the single slot's
// reflectance vs with it removed — the recorded 2.07 -> 0.75 form). v1 has NO kernel entry
// for the without-slot evaluation: the field ships `available = false` (fail-visible, the gap
// is registered in the task progress), never a plausible zero.
// ---------------------------------------------------------------------------
struct PairedCounterfactual {
  bool available = false;  // false: the kernel entry is missing (registered gap) — do not read
  double with_slot = 0.0;
  double without_slot = 0.0;
};

// ---------------------------------------------------------------------------
// StructureObjectRecord: one structural object. Field-group order follows the
// contract: identity, the four machines, geometry, diagnostics.
// ---------------------------------------------------------------------------
struct ChromaticAssessment {
  analytic::ChromaticVerdict verdict;
  analytic::ChromaticThresholds thresholds;
};
// The unassessed wire shape carries declared defaults, not an evaluated colour claim.
struct UnassessedChromatic {
  ChromaticAssessment schema_defaults;
};

struct StructureObjectRecord {
  // -- identity
  ObjectKind kind = ObjectKind::kKind1;
  std::vector<int> member;     // the physical face sequence, schema1's literal spelling
  int slot = -1;               // weight slot (kind-3: the internal step k); -1 = not applicable
  std::string phi_class_note;  // reflection-group orbit annotation; empty = none
  // -- existence (the contract's enum; the regime slug is DATA)
  ExistenceState existence = ExistenceState::kComputed;
  std::string escape_regime_slug;  // kernel EscapeRegimeName slug; read when existence == kEscaped
  // Walk arclength, the declared two-spelling convention (every write site keeps it):
  //   NaN  walk arclength does not apply to this object (closed curves, junctions);
  //   0.0  the walk was truncated BEFORE the record and the kernel exposes no pre-truncation
  //        arclength — the covered amount is UNKNOWN (declared unknown, never a measured zero).
  double walk_s = std::nan("");
  // -- visibility (the certificate, verbatim; default = the fail-closed unproven)
  VisibilityCertificate visibility{};
  // -- chromatic (the verdict verbatim + the declared-parameter snapshot)
  void ClearChromatic() { chromatic_ = UnassessedChromatic{}; }
  bool ChromaticAssessed() const { return std::holds_alternative<ChromaticAssessment>(chromatic_); }
  void SetChromatic(analytic::ChromaticVerdict verdict,
                    analytic::ChromaticThresholds thresholds = analytic::ChromaticThresholds{}) {
    chromatic_ = ChromaticAssessment{ std::move(verdict), thresholds };
  }
  // The unassessed arm exposes the declared defaults READ-ONLY: a serializer may project them,
  // but no writable path may carry an assessed verdict while ChromaticAssessed() stays false.
  const ChromaticAssessment& Chromatic() const {
    if (const auto* value = std::get_if<ChromaticAssessment>(&chromatic_))
      return *value;
    return std::get<UnassessedChromatic>(chromatic_).schema_defaults;
  }
  // Filling a verdict's fields is legal only after the explicit SetChromatic transition; reading
  // it before that throws std::bad_variant_access (the arm does not exist).
  ChromaticAssessment& MutableChromatic() { return std::get<ChromaticAssessment>(chromatic_); }
  // -- corroboration (v1: constant unchecked)
  CorroborationState corroboration = CorroborationState::kUnchecked;
  // -- geometry: sky position and u preimage, both first-class
  bool has_sky_position = false;
  double sky_position[3] = { 0.0, 0.0, 0.0 };  // unit, world frame; the spot the object sits at
  std::vector<double> u;                       // u-S^2 preimage, 3N packed (empty = none)
  // -- diagnostics
  PairedCounterfactual counterfactual{};

 private:
  std::variant<UnassessedChromatic, ChromaticAssessment> chromatic_;
};

// The record's own bucket: DeriveBucket on the two machines the record carries.
FeatureBucket BucketOf(const StructureObjectRecord& record);


}  // namespace lumice::raypath::schema3

#endif  // LUMICE_RAYPATH_RESULTS_REPORT_STRUCTURE_HPP_
