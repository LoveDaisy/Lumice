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
struct EnumerationBudget {
  long long max_optical_evaluations = 0;  // <= 0: unconstrained (666.3 wires the flag)
  long long sampling_evaluations = 0;     // orbit + restricted stream points built
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

// The core's consumption face (666.2/666.3 read this).
struct Schema3DiscoveryCore {
  std::vector<StructureObjectRecord> objects;
  SupportBlock support;
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
};

// Enumerates one member (one fixed face sequence). The member must resolve on the crystal (a
// rejected sequence returns an empty enumeration — the caller's filter, not an error).
MemberEnumeration EnumerateMember(const EnumerationInput& in, const std::vector<int>& member);

// Enumerates a layer: every member, then the family aggregate. A row joins a family only when
// it shares BOTH the reflection-group orbit (the carried Phi-class note) and the intervals
// (SameSupport) — the Phi-class expression of, e.g., C06's eight PBD variants.
Schema3DiscoveryCore EnumerateLayer(const EnumerationInput& in, const std::vector<std::vector<int>>& members);

}  // namespace lumice::raypath::schema3

#endif  // LUMICE_RAYPATH_DETAIL_SCHEMA3_STRUCTURE_ENUMERATION_HPP_
