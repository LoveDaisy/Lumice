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

ClassSupport AssembleClassSupport(const analytic::FaceNormalTable& table, const analytic::FacePolygonTable& polygons,
                                  const std::vector<std::vector<int>>& members, double refractive_index);


}  // namespace lumice::raypath::structure
#endif
