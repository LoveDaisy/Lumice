#include "raypath/detail/schema3/structure_enumeration.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "analytic/dp_contour.hpp"
#include "analytic/dp_weight_kink.hpp"
#include "analytic/reflection_group.hpp"
#include "raypath/detail/measure/visibility_certificate.hpp"

namespace lumice::raypath::schema3 {
namespace {

// The member's Phi-class annotation: the canonical (lexicographically smallest) member of its
// PBD orbit — reflection_group.hpp's label-arithmetic orbit, the family semantics' orbit.
// Empty when the orbit cannot be computed (a face outside the supported rings): the row then
// asserts no class membership and never joins an existing family.
std::string PhiClassNoteOf(const std::vector<int>& member) {
  std::vector<std::vector<int>> orbit;
  if (!analytic::PbdOrbit(member.data(), static_cast<int>(member.size()), &orbit) || orbit.empty()) {
    return "";
  }
  std::string note;
  for (size_t i = 0; i < orbit[0].size(); i++) {
    if (i > 0) {
      note += "-";
    }
    note += std::to_string(orbit[0][i]);
  }
  return note;
}

StructureObjectRecord BaseRecord(ObjectKind kind, const std::vector<int>& member) {
  StructureObjectRecord record;
  record.kind = kind;
  record.member = member;
  return record;
}

}  // namespace

int EventWordForObjectKind(ObjectKind kind) {
  switch (kind) {
    case ObjectKind::kKind3:
      return static_cast<int>(ChainEventKind::kTirBoundary);  // A1: the TIR onset carries the word
    case ObjectKind::kKind2:
      return static_cast<int>(ChainEventKind::kPathInfeasible);  // the gate boundary's pinned word
    case ObjectKind::kCorridorClosed:
      return static_cast<int>(ChainEventKind::kCorridorClosed);
    case ObjectKind::kKind1:
    case ObjectKind::kKind1Restricted:
    case ObjectKind::kS3SupportBoundary:
    case ObjectKind::kS4BranchBoundary:
    case ObjectKind::kS5DensityFeature:
    case ObjectKind::kS6Junction:
      return -1;  // no pinned schema1 word applies
  }
  return -1;  // unknown kind: no word (fail closed, same as the named non-carriers)
}

double KinkWalkArclength(analytic::WalkStatus status) {
  return status == analytic::WalkStatus::kOk ? std::nan("") : 0.0;
}

MemberEnumeration EnumerateMember(const EnumerationInput& in, const std::vector<int>& member) {
  MemberEnumeration out;
  if (in.normals == nullptr || in.polygons == nullptr || member.size() < 2 ||
      member.size() > static_cast<size_t>(analytic::kMaxFaceCount)) {
    return out;
  }
  int slots[analytic::kMaxFaceCount];
  if (analytic::ResolveFaceSequence(*in.normals, member.data(), static_cast<int>(member.size()), slots) !=
      analytic::Status::kOk) {
    return out;  // a sequence this crystal cannot host: the caller's filter, not an error
  }
  const int slot_count = static_cast<int>(member.size());
  out.phi_class_note = PhiClassNoteOf(member);

  // ONE axis assembly feeds the support row, the kind-2 chain, the corners and the kind-1
  // existence (the expensive pieces run once per member).
  const analytic::DeviationField field(*in.normals, *in.polygons, slots, slot_count, in.base_index);
  const AxisAssembly assembly = AssembleAxis(field);
  out.support = SupportRowOf(assembly, *in.normals, *in.polygons, slots, slot_count, in.base_index, member,
                             in.wavelengths_nm, in.indices);

  // -- kind-2: the gate-boundary chain (when the walk delivered one)
  if (assembly.axis.walk_closed) {
    const analytic::ChainCurve boundary_chain =
        analytic::ChainFromBoundaryPieces(assembly.record, assembly.axis.walk_status);
    if (!boundary_chain.u.empty()) {
      StructureObjectRecord gate = BaseRecord(ObjectKind::kKind2, member);
      gate.existence = ExistenceOf(boundary_chain.existence);
      gate.walk_s = std::nan("");
      gate.u = boundary_chain.u;
      out.objects.push_back(std::move(gate));
    }

    // -- s6_junction (opportunistic, declared non-exhaustive): the walk's corners, u preimage only.
    for (const analytic::WalkerCorner& corner : assembly.record.corners) {
      StructureObjectRecord junction = BaseRecord(ObjectKind::kS6Junction, member);
      junction.existence = ExistenceState::kComputed;
      junction.walk_s = std::nan("");  // walk arclength not applicable to a junction (the NaN)
      junction.u = { corner.position[0], corner.position[1], corner.position[2] };
      out.objects.push_back(std::move(junction));
    }
  }

  // -- kind-3: one object per weight-kink curve (slot = the internal step)
  const std::vector<analytic::KinkCurve> kinks = analytic::WeightKinks(field, analytic::KinkOptions{});
  for (const analytic::KinkCurve& kink : kinks) {
    const analytic::ChainCurve chain = analytic::ChainFromKinkArcs(kink);
    StructureObjectRecord kink_object = BaseRecord(ObjectKind::kKind3, member);
    kink_object.slot = kink.step;
    kink_object.existence = ExistenceOf(chain.existence);
    kink_object.walk_s = KinkWalkArclength(kink.status);
    kink_object.u = chain.u;
    out.objects.push_back(std::move(kink_object));
  }

  // One orbit stream per member, built once and shared by the kind-1 leg and the restricted
  // leg (identical parameters: a second build would sample the grid twice and hand the two
  // certificates different streams to disagree on). The stream moves out with the enumeration
  // (core.orbits — the corroboration wiring's read entry).
  const bool stream_wanted = in.measure != nullptr && in.density != nullptr && in.grid > 0;
  analytic::OrbitFiberStream orbit;
  if (stream_wanted) {
    orbit = analytic::MakeOrbitFiberStream(*in.normals, *in.polygons, slots, slot_count, *in.density, in.sun_dir,
                                           in.base_index, in.grid);
    out.stream_points_built = static_cast<long long>(orbit.samples.size());
  }

  // -- kind-1 (unrestricted): the member's critical-value structure. Existence rides the same
  // axis state; the curve body is NOT traced in v1 (A5 — the delta-axis facts live in the
  // support row), so visibility runs the kind1=null path when a measure is supplied.
  {
    StructureObjectRecord kind1 = BaseRecord(ObjectKind::kKind1, member);
    kind1.existence =
        assembly.axis.walk_closed ?
            (assembly.axis.context.coverage == PartitionContext::Coverage::kComplete ? ExistenceState::kComputed :
                                                                                       ExistenceState::kEscaped) :
            ExistenceState::kWalkTruncated;
    kind1.escape_regime_slug = assembly.axis.regime_slug;
    // walk_s spelling: NaN on a closed walk; 0.0 = truncated before the record, covered amount
    // UNKNOWN (the declared convention — never a measured zero).
    kind1.walk_s = assembly.axis.walk_closed ? std::nan("") : 0.0;
    if (in.measure != nullptr) {
      PartitionContext partition = assembly.axis.context;
      if (!stream_wanted) {
        // No stream the v1 producers can build for this measure: the visibility field ships its
        // fail-closed default (unproven), which is the registered v1 shape.
        kind1.visibility = CertifyVisibility(*in.measure, FiberSampleStream{}, nullptr, &partition, kOrbitAngularTol);
      } else if (!orbit.samples.empty()) {
        const FiberSampleStream stream = StreamOf(orbit);
        kind1.visibility = CertifyVisibility(*in.measure, stream, nullptr, &partition, kOrbitAngularTol);
      }
    }
    // chromatic: the kernel's path-level verdict (rank-0 paths have no field; skip them).
    if (slot_count >= 3) {
      kind1.chromatic =
          analytic::Diagnose(*in.normals, *in.polygons, slots, slot_count, analytic::kNRed, analytic::kNBlue);
      kind1.chromatic_assessed = true;
      kind1.chromatic_thresholds = analytic::ChromaticThresholdsSnapshot();
    }
    out.objects.push_back(std::move(kind1));
  }

  // -- kind-1 restricted: the family-pinned curve, certified against the member's own orbit
  // stream (built once, above) — the v1 carrier of the C09 unlit shape ("contour present, no
  // passage").
  if (stream_wanted && analytic::FamilyPinned(*in.normals, slots, slot_count, *in.density)) {
    const analytic::RestrictedFamilyCurve curve = analytic::MakeRestrictedFamilyCurve(
        *in.normals, *in.polygons, slots, slot_count, *in.density, in.sun_dir, in.base_index, in.wavelengths_nm.data(),
        in.indices.data(), static_cast<int>(in.indices.size()), in.grid);
    if (curve.existence == analytic::CurveExistence::kComputed && !curve.u.empty()) {
      StructureObjectRecord restricted = BaseRecord(ObjectKind::kKind1Restricted, member);
      restricted.existence = ExistenceState::kComputed;
      restricted.walk_s = std::nan("");
      restricted.u = curve.u;
      if (!orbit.samples.empty()) {
        const CriticalSetCurve kind1_curve = CurveOf(curve);
        const FiberSampleStream stream = StreamOf(orbit);
        restricted.visibility =
            CertifyVisibility(*in.measure, stream, &kind1_curve, &assembly.axis.context, kOrbitAngularTol);
      }
      out.objects.push_back(std::move(restricted));
    }
  }
  // The Phi-class annotation rides every object of the member (one orbit read per member).
  for (StructureObjectRecord& object : out.objects) {
    object.phi_class_note = out.phi_class_note;
  }
  out.orbit = std::move(orbit);
  return out;
}

namespace {

// The v1 vocabulary ledger (Table A's machine face; the human ledger with the evidence lives in
// the task progress). Every registered kind appears exactly once.
EnumeratedCoverage V1Coverage() {
  EnumeratedCoverage coverage;
  coverage.produced = { ObjectKind::kKind1, ObjectKind::kKind1Restricted, ObjectKind::kKind2, ObjectKind::kKind3,
                        ObjectKind::kS6Junction };
  coverage.declared_not_produced = { ObjectKind::kCorridorClosed, ObjectKind::kS3SupportBoundary,
                                     ObjectKind::kS4BranchBoundary, ObjectKind::kS5DensityFeature };
  coverage.declared_reasons = {
    "no v8 producer for the corridor A=0 curve (the LI second batch A6 is the donor)",
    "no 661 producer exposes a declared support boundary yet (v1 orbit supports are closed curves)",
    "owner ruling 2026-10-07: v1 declares the branch boundary, does not trace it (open math)",
    "no 661 producer exposes a density-feature image yet (v2 measure layer)",
  };
  return coverage;
}

}  // namespace

Schema3DiscoveryCore EnumerateLayer(const EnumerationInput& in, const std::vector<std::vector<int>>& members) {
  Schema3DiscoveryCore core;
  core.coverage = V1Coverage();
  long long stream_points = 0;
  std::vector<std::string> notes;
  for (const std::vector<int>& member : members) {
    MemberEnumeration one = EnumerateMember(in, member);
    for (StructureObjectRecord& object : one.objects) {
      core.objects.push_back(std::move(object));
    }
    stream_points += one.stream_points_built;
    notes.push_back(std::move(one.phi_class_note));
    core.orbits.push_back(std::move(one.orbit));
    core.support.members.push_back(std::move(one.support));
  }
  // Family clustering: the ONE implementation (support_block's ClusterFamilies — the same
  // gates the inline loop used: same orbit note, SameSupport, complete partition).
  core.support.families = ClusterFamilies(core.support.members, notes);
  // The budget: the stream points the enumeration itself built, counted where they were built
  // (one orbit stream per member with the measure side) — not estimated from a formula, which
  // is how the count drifted when a second consumer leg appeared. The kernel-internal counts
  // stay a registered gap.
  core.budget.sampling_evaluations = stream_points;
  core.budget.max_optical_evaluations = 0;  // unconstrained in v1; 666.3 wires the flag
  return core;
}

}  // namespace lumice::raypath::schema3
