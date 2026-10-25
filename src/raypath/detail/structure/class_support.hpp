#ifndef LUMICE_RAYPATH_DETAIL_STRUCTURE_CLASS_SUPPORT_HPP_
#define LUMICE_RAYPATH_DETAIL_STRUCTURE_CLASS_SUPPORT_HPP_

#include "raypath/detail/structure/support.hpp"

namespace lumice::raypath::structure {

struct ClassSupport {
  bool ok = false;
  std::string refusal_mode;  // the open refusal mode when !ok
  std::vector<structure::AxisAssembly> assemblies;
  std::vector<structure::MemberSupport> rows;
  std::vector<analytic::DeviationInterval> intervals;  // the class-level support (== rows.front())
};

// `stop` (optional) polls per member (the structure assembly's outer boundary) and rides into
// each member's walk; a stop fails closed with refusal_mode "stop_requested" — the honest
// truncation marker, never a fabricated support.
ClassSupport AssembleClassSupport(const analytic::FaceNormalTable& table, const analytic::FacePolygonTable& polygons,
                                  const std::vector<std::vector<int>>& members, double refractive_index,
                                  const analytic::DiscoveryStop* stop = nullptr);


}  // namespace lumice::raypath::structure
#endif
