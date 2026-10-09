#ifndef LUMICE_RAYPATH_DETAIL_SCHEMA3_STRUCTURE_ENUMERATION_HPP_
#define LUMICE_RAYPATH_DETAIL_SCHEMA3_STRUCTURE_ENUMERATION_HPP_

// schema3 report core, enumeration driver (scrum 666.1): per member, run the vocabulary legs
// the v1 producers exist for and assemble the StructureObjectRecords. The consumers downstream
// (666.2's mc_evidence/corroboration wiring, 666.3's serialization) read Schema3DiscoveryCore.
//
// What v1 PRODUCES per member (the vocabulary ledger's machine face is EnumeratedCoverage; the
// human ledger lives in the task progress's Table A):
//   kind_2              the gate-boundary chain from the axis assembly's walk record;
//   kind_3              one object per weight-kink curve (slot = the internal step);
//   kind_1              the member's critical-value structure: existence from the partition
//                       state, chromatic from the kernel's path-level Diagnose, visibility —
//                       A5's kind1=null path when a declared measure is supplied (the stream
//                       alone decides; unlit is unreachable without a curve — precisely the
//                       registered v1 shape);
//   kind_1_restricted   when FamilyPinned holds and a measure/sun are supplied: the restricted
//                       family curve (the contract's canonical closed kind-1 curve) certified
//                       against the orbit fiber stream — the C09 unlit shape's v1 carrier;
//   s6_junction         opportunistic: the walk record's corners, u preimage only, declared
//                       non-exhaustive (coverage says so).
// What v1 DECLARES but does not produce (each with its registered reason in the coverage):
//   corridor_closed     no v8 producer (the LI second batch A6 is the donor);
//   s4_branch_boundary  owner ruling 2026-10-07: v1 declares the branch boundary, does not
//                       trace it (open math, v2);
//   s3_support_boundary no 661 producer exposes a declared support BOUNDARY yet (the orbit
//                       supports of the v1 configs are closed curves — boundary-less);
//   s5_density_feature  no 661 producer exposes a density-feature image yet (v2 measure layer).
//
// The chromatic leg carries the kernel's verdict VERBATIM plus the declared-threshold snapshot;
// corroboration ships `unchecked` (666.2 wires the derivation); the paired counterfactual ships
// `available=false` (A4: the kernel has no remove-one-slot-reflectance entry — registered gap,
// fail-visible). Events: the chain objects keep the contour layer's -1 (no LI authority maps
// the schema1 words onto curve points); the word each OBJECT kind would carry is A1's
// registration below, pinned by test.

#include <chrono>
#include <string>
#include <vector>

#include "analytic/dp_chromatic.hpp"
#include "analytic/pose_density.hpp"
#include "raypath/detail/measure/declared_density.hpp"
#include "raypath/detail/schema3/structure_object.hpp"
#include "raypath/detail/schema3/support_block.hpp"

namespace lumice::raypath::schema3 {

// A1's event-word registration (the plan's assumption A1 — tir_boundary attaches to the kind-3
// TIR onset, NOT a second kind-2; the scrum text's "kind-2/kind-2" is the suspected typo): the
// ChainEventKind value an object kind's events carry, or -1 when no pinned word applies.
int EventWordForObjectKind(ObjectKind kind);

// The report-side carry bound on one object's u preimage (points). A curve denser than this is
// real kernel output, but carrying it is the report leg's own cost: the object ships the
// declared truncated face (existence kWalkTruncated, walk_s 0.0, no u — the covered amount is
// UNKNOWN, never a downsampled body) and the budget's truncation note names it. Sits ~27x above
// the largest measured legal corpus curve (a closed-form circle's 3600 samples) and ~8x below
// the degenerate-family pathology (corpus C10's 780k-point duplicated marched arcs), whose
// carried JSON would have been tens of megabytes per object.
constexpr size_t kMaxCarriedCurvePoints = 100000;

// The vocabulary ledger's machine face (Table A): every registered kind appears EXACTLY once,
// as produced or declared-not-produced, with the reason string for the declared ones.
struct EnumeratedCoverage {
  std::vector<ObjectKind> produced;
  std::vector<ObjectKind> declared_not_produced;
  std::vector<std::string> declared_reasons;  // parallel to declared_not_produced
};

// Cost accounting over the caller's budget vocabulary. v1 counts what the enumeration itself
// spawns (the declared-resolution samplings); the kernel-internal walk/Newton evaluation counts
// are NOT introspectable and stay a registered gap (the G4 family).
//
// The anti-hang fields are the one ENFORCED bound in the budget vocabulary (the quality budgets
// above are report-only by A4's ruling): when the caller installs a deadline, members or legs a
// deadline cut ship the honest truncated face and say so here — a robustness floor against
// pathological inputs, not a quality knob.
struct EnumerationBudget {
  long long max_optical_evaluations = 0;  // <= 0: unconstrained (666.3 wires the flag)
  long long sampling_evaluations = 0;     // orbit + restricted stream points built
  long long hang_cap_ms = 0;              // the installed anti-hang cap; 0 = none installed
  bool truncated = false;                 // the cap fired somewhere in this enumeration
  std::string truncation_note;            // what was cut (empty when not truncated)
};

// One layer's inputs. The pointers are borrowed and must outlive the call; the measure side is
// optional — without it the restricted/orbit legs are skipped (declared in the coverage).
struct EnumerationInput {
  const analytic::FaceNormalTable* normals = nullptr;
  const analytic::FacePolygonTable* polygons = nullptr;
  double base_index = 0.0;
  std::vector<double> wavelengths_nm;  // the n-continuation table (parallel to indices; may be empty)
  std::vector<double> indices;
  const UMarginal* measure = nullptr;
  const analytic::PoseDensitySpec* density = nullptr;
  // The sun direction (unit, world frame). It must be the SAME sun the declared measure was
  // built against: a mismatch parks every orbit sample outside the declared support and the
  // certificates fail closed to unproven/no_in_support_samples with no further diagnostic.
  double sun_dir[3] = { 0.0, 0.0, 1.0 };
  int grid = 720;  // the orbit stream / restricted curve's declared resolution
};

// The core's consumption face (666.2/666.3 read this). `orbits` is parallel to
// `support.members` (one orbit stream per member row, same order, one slot each — empty when
// no measure side ran); EnumerateLayer is its only writer and the streams' only read entry
// past this header: MemberEnumeration.orbit below is the move-out staging field, not a second
// home for the same stream.
struct Schema3DiscoveryCore {
  std::vector<StructureObjectRecord> objects;
  SupportBlock support;
  std::vector<analytic::OrbitFiberStream> orbits;
  EnumeratedCoverage coverage;
  EnumerationBudget budget;
};

// One member's objects and support row.
struct MemberEnumeration {
  std::vector<StructureObjectRecord> objects;
  MemberSupport support;
  // The member's Phi-class annotation: the canonical (lexicographically smallest) member of its
  // PBD orbit — reflection_group.hpp's label-arithmetic orbit, the family semantics' orbit.
  // Empty when the orbit cannot be computed (a face outside the kernel's supported rings): the
  // row then asserts no class membership and never joins an existing family.
  std::string phi_class_note;
  // The stream points this member's enumeration actually built (0 = no measure side ran).
  long long stream_points_built = 0;
  // The member's orbit stream, MOVED out to Schema3DiscoveryCore.orbits by EnumerateLayer
  // (staging field — do not read past the layer call; the core's slot is the home).
  analytic::OrbitFiberStream orbit;
  // Non-empty when the anti-hang deadline or the carry bound cut a leg of this member (what
  // was cut and why); the layer folds these into the budget's truncation note.
  std::string truncation_note;
};

// The walk-arclength spelling, one owner (every write site calls this — the convention rides
// the function, not per-site comments): kOk -> NaN ("a walk arclength does not apply"); every
// refusal -> 0.0 ("the walk was truncated BEFORE the record and the kernel exposes no
// pre-truncation arclength — the covered amount is UNKNOWN", never a measured zero).
double KinkWalkArclength(analytic::WalkStatus status);

// Enumerates one member (one fixed face sequence). The member must resolve on the crystal (a
// rejected sequence returns an empty enumeration — the caller's filter, not an error). The
// deadline is the caller's anti-hang cap: past it, the remaining legs ship the honest
// truncated face (kind-1 kWalkTruncated + walk_s 0.0 when the axis assembly itself did not
// run; true existence with the later legs absent when it did) and the member's truncation
// note says what was cut. The default is no deadline (max()), which every existing caller
// and test keeps.
MemberEnumeration EnumerateMember(
    const EnumerationInput& in, const std::vector<int>& member,
    std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::time_point::max());

// Enumerates a layer: every member, then the family aggregate. A row joins a family only when
// it shares BOTH the reflection-group orbit (the carried Phi-class note) and the intervals
// (SameSupport) — the Phi-class expression of, e.g., C06's eight PBD variants. The deadline is
// EnumerateMember's (a member the cap cut still contributes its row, so the support block
// stays one row per member); the budget's truncation fields report the cuts.
Schema3DiscoveryCore EnumerateLayer(
    const EnumerationInput& in, const std::vector<std::vector<int>>& members,
    std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::time_point::max());

}  // namespace lumice::raypath::schema3

#endif  // LUMICE_RAYPATH_DETAIL_SCHEMA3_STRUCTURE_ENUMERATION_HPP_
