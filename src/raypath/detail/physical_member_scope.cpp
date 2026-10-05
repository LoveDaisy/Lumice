#include "raypath/detail/physical_member_scope.hpp"

#include <algorithm>
#include <limits>

#include "core/raypath.hpp"

namespace lumice::raypath {

Error ResolvePhysicalMemberScope(const PhysicalMemberRequest& request, PhysicalMemberScope* out) {
  *out = {};
  if (request.scene_identity.empty() || request.semantics != SymmetrySemantics::kPhysical ||
      (request.symmetry_bits & ~(sym::kSymP | sym::kSymB | sym::kSymD)) != 0) {
    return { ErrorCode::kInvalidArgument,
             "physical scope requires identity, explicit physical semantics and P/B/D bits" };
  }
  if (request.representative.size() < 2 || request.representative.size() > analytic::kMaxFaceCount) {
    return { ErrorCode::kInvalidPath, "invalid representative length" };
  }
  std::vector<IdType> path;
  for (int face : request.representative) {
    if (face <= 0 || face > std::numeric_limits<IdType>::max()) {
      return { ErrorCode::kInvalidPath, "face number cannot be represented" };
    }
    path.push_back(static_cast<IdType>(face));
  }
  PhysicalMemberScope result;
  result.snapshot = request;
  const auto shape = std::visit([](const auto& p) { return DeriveGeometricSymmetry(p); }, request.crystal.param_);
  result.gating = DeriveSymmetryGating(request.semantics, shape, request.crystal.axis_);
  const auto d = detail::DeriveDSymmetryParams(request.crystal.axis_);
  const auto& gating = result.gating;
  for (const auto& member :
       ExpandRaypathByPeriod(path, request.symmetry_bits, d.sigma_a, d.d_applicable, gating.p_applicable,
                             gating.b_applicable, kHexagonalFnPeriod, gating.geom)) {
    std::vector<int> converted(member.begin(), member.end());
    if (std::find(result.members.begin(), result.members.end(), converted) == result.members.end()) {
      result.members.push_back(std::move(converted));
    }
  }
  if (result.members.empty())
    result.members.push_back(request.representative);
  *out = std::move(result);
  return {};
}
}  // namespace lumice::raypath
