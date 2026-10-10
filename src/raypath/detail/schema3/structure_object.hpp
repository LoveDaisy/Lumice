#ifndef LUMICE_RAYPATH_DETAIL_SCHEMA3_STRUCTURE_OBJECT_HPP_
#define LUMICE_RAYPATH_DETAIL_SCHEMA3_STRUCTURE_OBJECT_HPP_

// schema3 report core, object record (scrum 666.1, the pure data layer — no kernel, no measure
// call): one structural object of the redesigned feature report. The contract is the owner's
// schema2-redesign conclusions section 5 (2026-10-07) via scrum.md section 3, and it is quoted
// here, not reinvented: an object carries FOUR typed sub-state-machines — existence /
// visibility / chromatic / corroboration — and its report bucket is DERIVED from two of them,
// never stored.
//
//   existence     computed | escaped(regime name) | walk_truncated | s4_declared. The enum is the
//                 measure contract's ExistenceState (its registered table and names are the
//                 single authority); `escaped` carries the kernel's regime slug as DATA (the
//                 kernel EscapeRegimeName's string, open set, report side fail-closed on unknown
//                 — the contract header's own words). G3 ruled (owner 2026-10-09): the contract's
//                 typed escape_regime carries the kUnset sentinel by default and production never
//                 writes it; the slug flows HERE, at the object layer, as the regime's datum.
//   visibility    the measure layer's VisibilityCertificate verbatim (state + lit_fraction +
//                 evidence form + jets_ok + the discriminated A/T flags + reason). One authority:
//                 CertifyVisibility produces it, this record only carries it.
//   chromatic     the kernel's ChromaticVerdict verbatim (the LI criterion shape) PLUS the
//                 declared-threshold snapshot (ChromaticThresholds) — 0.5 deg / spread-per-shift
//                 are declared parameters, not universal physics, so the record carries which
//                 criterion produced the label (issue: "thresholds travel with the verdict").
//                 `assessed` says whether a chromatic classification ran at all (kind-2 chains
//                 can skip it; false keeps the default verdict and the snapshot).
//   corroboration observed | consistent | not_observed_insufficient_ess |
//                 not_observed_despite_sufficient_ess (the red flag) | unchecked. v1 (this task)
//                 has NO derivation rule: every record ships `unchecked`, and the MC->state
//                 derivation is 666.2's. The携带不变形 tests pin the default and the table.
//
// Bucket derivation (the ONLY rule this module owns): computed + certified -> actual;
// computed + partial/unlit/unproven -> candidate; anything not computed -> unfinished. An UNKNOWN
// enum value (a cast-constructed one) fails closed: it is not computed (existence) or not
// certified (visibility), so it can never reach actual — the tests pin the routing, not a
// promise. corroboration does NOT enter the derivation (not_observed_despite_sufficient_ess is a
// red-flag annotation, not a bucket mover — scrum.md section 3).
//
// Identity: the object kind is an OPEN enum with a registered table (the S1-S6 vocabulary
// landing); member is the physical face sequence (schema1's own spelling, unresolved-literal);
// slot names the weight slot a kind-3 object belongs to (-1 otherwise); phi_class_note carries
// the reflection-group orbit annotation the object shares with its class (empty = none).
//
// Geometry: the sky position and the u-space preimage are BOTH first-class explicit fields (the
// design's own requirement — neither is derived from the other in this module).

#include <cmath>
#include <string>
#include <vector>

#include "analytic/dp_chromatic.hpp"
#include "raypath/detail/measure/measure_geometry_contract.hpp"
#include "raypath/detail/measure/visibility_certificate.hpp"

namespace lumice::raypath::schema3 {

// ---------------------------------------------------------------------------
// ObjectKind: the S1-S6 vocabulary as object kinds. Open: a new value is a new
// registration row + the coverage test's extension, in the same change (a50,
// the contract's registered-table mechanism).
// ---------------------------------------------------------------------------
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
  bool chromatic_assessed = false;
  analytic::ChromaticVerdict chromatic{};
  analytic::ChromaticThresholds chromatic_thresholds = analytic::ChromaticThresholdsSnapshot();
  // -- corroboration (v1: constant unchecked)
  CorroborationState corroboration = CorroborationState::kUnchecked;
  // -- geometry: sky position and u preimage, both first-class
  bool has_sky_position = false;
  double sky_position[3] = { 0.0, 0.0, 0.0 };  // unit, world frame; the spot the object sits at
  std::vector<double> u;                       // u-S^2 preimage, 3N packed (empty = none)
  // -- diagnostics
  PairedCounterfactual counterfactual{};
};

// The record's own bucket: DeriveBucket on the two machines the record carries.
FeatureBucket BucketOf(const StructureObjectRecord& record);

}  // namespace lumice::raypath::schema3

#endif  // LUMICE_RAYPATH_DETAIL_SCHEMA3_STRUCTURE_OBJECT_HPP_
