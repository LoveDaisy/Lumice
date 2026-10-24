#ifndef LUMICE_RAYPATH_DETAIL_STRUCTURE_CLASS_SUPPORT_HPP_
#define LUMICE_RAYPATH_DETAIL_STRUCTURE_CLASS_SUPPORT_HPP_

#include "raypath/detail/structure/support.hpp"
#include "raypath/results/survey_records.hpp"

namespace lumice::raypath::survey {

struct ClassSupport {
  std::optional<SurveyObject> card;
  bool ok = false;
  std::string refusal_mode;  // the open refusal mode when !ok
  std::vector<structure::AxisAssembly> assemblies;
  std::vector<structure::MemberSupport> rows;
  std::vector<analytic::DeviationInterval> intervals;  // the class-level support (== rows.front())
};

ClassSupport AssembleClassSupport(const analytic::FaceNormalTable& table, const analytic::FacePolygonTable& polygons,
                                  const std::vector<std::vector<int>>& members, double refractive_index,
                                  double wavelength_nm, unsigned symmetry_bits);


}  // namespace lumice::raypath::survey
#endif
