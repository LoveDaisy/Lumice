#include "raypath/detail/structure/class_support.hpp"

#include "core/math.hpp"
#include "raypath/scene_to_analytic.hpp"

namespace lumice::raypath::survey {

ClassSupport AssembleClassSupport(const analytic::FaceNormalTable& table, const analytic::FacePolygonTable& polygons,
                                  const std::vector<std::vector<int>>& members, double refractive_index,
                                  double wavelength_nm, unsigned symmetry_bits) {
  // One axis assembly per member; the support row is the schema3 machine's own (the same
  // assembly feeds the objects and the row — no second partition anywhere).
  ClassSupport support;
  support.assemblies.reserve(members.size());
  support.rows.reserve(members.size());
  for (const std::vector<int>& member : members) {
    std::vector<int> slots;
    if (const Error slot_error = ResolveSingleLayerPath({ member }, table, &slots); !slot_error.Ok()) {
      support.refusal_mode = "member_resolution";
      return support;
    }
    const analytic::DeviationField field(table, polygons, slots.data(), static_cast<int>(slots.size()),
                                         refractive_index);
    support.assemblies.emplace_back(structure::AssembleAxis(field));
    support.rows.push_back(structure::SupportRowOf(support.assemblies.back(), table, polygons, slots.data(),
                                                   static_cast<int>(slots.size()), refractive_index, member, {}, {}));
  }
  // Fail closed: a walk refusal or a partition escape leaves the class support unknown.
  for (const structure::MemberSupport& row : support.rows) {
    if (row.axis.walk_closed && row.axis.context.coverage == PartitionContext::Coverage::kComplete) {
      continue;
    }
    const char* mode =
        row.axis.walk_closed ? row.axis.regime_slug.c_str() : analytic::WalkStatusName(row.axis.walk_status);
    support.refusal_mode = mode == nullptr ? "walk_truncated" : mode;
    return support;
  }
  // The class-level support: the members are relabelings of one physical class, so their
  // partitions must sit on the same delta axis (SameSupport — the one ruler; a disagreement is
  // not a union to fabricate but a refusal).
  support.intervals = support.rows.front().axis.intervals;
  for (const structure::MemberSupport& row : support.rows) {
    if (!structure::SameSupport(support.intervals, row.axis.intervals)) {
      support.refusal_mode = "member_supports_disagree";
      return support;
    }
  }

  // The support_set card: the class's delta-axis support with the endpoint mechanism labels
  // (the first member's onsets at an interval endpoint — the members share the axis) and the
  // caliber the partition ran on.
  SurveyObject card;
  card.kind = SurveyObjectKind::kSupportSet;
  card.members = members;
  card.evidence.state = EvidenceState::kFinite;
  card.evidence.coverage.members_evaluated = static_cast<int>(members.size());
  card.evidence.coverage.wavelength_nm = wavelength_nm;
  SurveyObject::SupportSetPayload payload;
  payload.symmetry_bits = symmetry_bits;
  payload.member_count = static_cast<int>(members.size());
  for (const analytic::DeviationInterval& interval : support.intervals) {
    SurveyObject::Interval out_interval;
    out_interval.lower_deg = interval.lower * math::kRadToDegreeD;
    out_interval.upper_deg = interval.upper * math::kRadToDegreeD;
    out_interval.n_components = interval.n_components;
    out_interval.n_closed = interval.n_closed;
    out_interval.n_open = interval.n_open;
    payload.intervals.push_back(out_interval);
  }
  for (const analytic::CriticalOnset& onset : support.rows.front().endpoint_onsets) {
    SurveyObject::EndpointLabel label;
    label.delta_deg = onset.value * math::kRadToDegreeD;
    label.source = analytic::OnsetSourceName(onset.source);
    label.profile = analytic::OnsetProfileName(onset.profile);
    label.multiplicity = onset.multiplicity;
    payload.endpoints.push_back(std::move(label));
  }
  card.support_set = std::move(payload);
  support.card = std::move(card);
  support.ok = true;
  return support;
}


}  // namespace lumice::raypath::survey
