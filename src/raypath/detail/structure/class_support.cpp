#include "raypath/detail/structure/class_support.hpp"

#include "raypath/query_control.hpp"
#include "raypath/scene_to_analytic.hpp"

namespace lumice::raypath::structure {

ClassSupport AssembleClassSupport(const analytic::FaceNormalTable& table, const analytic::FacePolygonTable& polygons,
                                  const std::vector<std::vector<int>>& members, double refractive_index,
                                  const analytic::DiscoveryStop* stop) {
  // One axis assembly per member; the support row is the schema3 machine's own (the same
  // assembly feeds the objects and the row — no second partition anywhere).
  ClassSupport support;
  support.assemblies.reserve(members.size());
  support.rows.reserve(members.size());
  for (const std::vector<int>& member : members) {
    // The structure assembly's outer poll (per member: field + walk + partition + row).
    if (stop != nullptr) {
      if (stop->token != nullptr) {
        stop->token->NotePoll(raypath::kPollStructureMember);
      }
      if (stop->Stopped()) {
        support.refusal_mode = "stop_requested";
        return support;
      }
    }
    std::vector<int> slots;
    if (const Error slot_error = ResolveSingleLayerPath({ member }, table, &slots); !slot_error.Ok()) {
      support.refusal_mode = "member_resolution";
      return support;
    }
    const analytic::DeviationField field(table, polygons, slots.data(), static_cast<int>(slots.size()),
                                         refractive_index);
    support.assemblies.emplace_back(structure::AssembleAxis(field, stop));
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

  support.ok = true;
  return support;
}


}  // namespace lumice::raypath::structure
