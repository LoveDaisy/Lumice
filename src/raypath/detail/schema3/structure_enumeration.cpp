#include "raypath/detail/schema3/structure_enumeration.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <string>

#include "analytic/dp_contour.hpp"
#include "analytic/dp_weight_kink.hpp"
#include "raypath/detail/measure/visibility_certificate.hpp"

namespace lumice::raypath::schema3 {
namespace {

// The orbit tolerance the certificate calls run at — NOT a free knob: the measured floor is
// test_geometry_source.cpp's acos-at-1 finding (~1.5e-8 rad), so 1e-6 (three decades of margin).
constexpr double kOrbitAngularTol = 1e-6;

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
    kink_object.walk_s = std::nan("");
    kink_object.u = chain.u;
    if (kink.status != analytic::WalkStatus::kOk) {
      kink_object.walk_s = 0.0;  // the kink walk's own refusal: the arcs it managed, declared
    }
    out.objects.push_back(std::move(kink_object));
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
    kind1.walk_s = assembly.axis.walk_closed ? std::nan("") : 0.0;
    if (in.measure != nullptr) {
      PartitionContext partition = assembly.axis.context;
      if (in.density != nullptr && in.grid > 0) {
        const analytic::OrbitFiberStream orbit = analytic::MakeOrbitFiberStream(
            *in.normals, *in.polygons, slots, slot_count, *in.density, in.sun_dir, in.base_index, in.grid);
        if (!orbit.samples.empty()) {
          const FiberSampleStream stream = StreamOf(orbit);
          kind1.visibility = CertifyVisibility(*in.measure, stream, nullptr, &partition, kOrbitAngularTol);
        }
      } else {
        // No stream the v1 producers can build for this measure: the visibility field ships its
        // fail-closed default (unproven), which is the registered v1 shape.
        kind1.visibility = CertifyVisibility(*in.measure, FiberSampleStream{}, nullptr, &partition, kOrbitAngularTol);
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

  // -- kind-1 restricted: the family-pinned curve, certified against the orbit stream — the v1
  // carrier of the C09 unlit shape ("contour present, no passage").
  if (in.measure != nullptr && in.density != nullptr && in.grid > 0 &&
      analytic::FamilyPinned(*in.normals, slots, slot_count, *in.density)) {
    const analytic::RestrictedFamilyCurve curve = analytic::MakeRestrictedFamilyCurve(
        *in.normals, *in.polygons, slots, slot_count, *in.density, in.sun_dir, in.base_index, in.wavelengths_nm.data(),
        in.indices.data(), static_cast<int>(in.indices.size()), in.grid);
    if (curve.existence == analytic::CurveExistence::kComputed && !curve.u.empty()) {
      StructureObjectRecord restricted = BaseRecord(ObjectKind::kKind1Restricted, member);
      restricted.existence = ExistenceState::kComputed;
      restricted.walk_s = std::nan("");
      restricted.u = curve.u;
      const analytic::OrbitFiberStream orbit = analytic::MakeOrbitFiberStream(
          *in.normals, *in.polygons, slots, slot_count, *in.density, in.sun_dir, in.base_index, in.grid);
      if (!orbit.samples.empty()) {
        const CriticalSetCurve kind1_curve = CurveOf(curve);
        const FiberSampleStream stream = StreamOf(orbit);
        restricted.visibility =
            CertifyVisibility(*in.measure, stream, &kind1_curve, &assembly.axis.context, kOrbitAngularTol);
      }
      out.objects.push_back(std::move(restricted));
    }
  }
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
  std::vector<MemberSupport> shared_rows;
  for (const std::vector<int>& member : members) {
    MemberEnumeration one = EnumerateMember(in, member);
    for (StructureObjectRecord& object : one.objects) {
      core.objects.push_back(std::move(object));
    }
    // Family aggregation: rows whose support intervals are IDENTICAL (within the partition's
    // merge constant) share one FamilySupport — the machine form of, e.g., C06's eight PBD
    // variants sharing their support. A row clusters with the first row it matches; a refused
    // row matches nothing.
    if (!one.support.member.empty() && one.support.axis.context.coverage == PartitionContext::Coverage::kComplete) {
      bool clustered = false;
      for (FamilySupport& family : core.support.families) {
        // Compare against the family's own intervals (carried on the FamilySupport).
        if (family.shared && family.intervals.size() == one.support.axis.intervals.size()) {
          bool same = true;
          for (size_t i = 0; i < family.intervals.size(); i++) {
            same = same && std::fabs(family.intervals[i].lower - one.support.axis.intervals[i].lower) <=
                               analytic::kExtremumAtol;
            same = same && std::fabs(family.intervals[i].upper - one.support.axis.intervals[i].upper) <=
                               analytic::kExtremumAtol;
          }
          if (same) {
            family.members.push_back(one.support.member);
            clustered = true;
            break;
          }
        }
      }
      if (!clustered) {
        FamilySupport family;
        family.shared = true;
        family.intervals = one.support.axis.intervals;
        family.members.push_back(one.support.member);
        core.support.families.push_back(std::move(family));
      }
    }
    core.support.members.push_back(std::move(one.support));
  }
  // The budget: what the enumeration itself spawned (the declared-resolution samplings; the
  // kernel-internal counts stay a registered gap).
  long long samplings = 0;
  if (in.measure != nullptr && in.density != nullptr && in.grid > 0) {
    samplings += static_cast<long long>(in.grid) * (1 + static_cast<long long>(members.size()));
  }
  core.budget.sampling_evaluations = samplings;
  core.budget.max_optical_evaluations = 0;  // unconstrained in v1; 666.3 wires the flag
  return core;
}

}  // namespace lumice::raypath::schema3
