#ifndef RAYPATH_DETAIL_SURVEY_ASSEMBLY_HPP_
#define RAYPATH_DETAIL_SURVEY_ASSEMBLY_HPP_

// The survey's T0 assembly: the request's class under the selected scope, routed by the kernel's
// rank-0 certificate. Every member's classification is the kernel's own (analytic::Classify —
// the LUMICE_ANALYTIC_ClassifyCriticalStructure implementation, whose rank-0 arm is
// IsRankZeroPath's M = I and zero-wedge check); nothing here re-derives a geometry verdict.

#include "config/config_manager.hpp"
#include "raypath/detail/input_assembly.hpp"
#include "raypath/detail/input_scope.hpp"
#include "raypath/scene_to_analytic.hpp"
#include "raypath/survey_report.hpp"
#include "raypath/survey_result.hpp"

namespace lumice::raypath::survey {

// Validates the request, expands the class and routes at T0. All members rank-0 -> exactly one
// source_image card (the sun's preimage) and the early stop; any other member verdict -> the
// unevaluated skeleton, with the T0 facts recorded. Request refusals reuse the report request's
// error semantics: unknown_crystal_id, invalid_path, multi_layer_unsupported, face_not_in_crystal,
// wavelength_out_of_range, crystal_rejected.
struct SurveyAssemblyOptions {
  // The query-face context mode (the point command's no-warm path): T0–T2 structure legs only —
  // no T3 quantification (the probe, the chromatic legs, the tint, the witness lattice). The
  // budget note says so instead of claiming legs that did not run.
  bool context_only = false;
};

Error AssembleSurvey(const ConfigManager& config, const SurveyRequest& request, SurveyResult* out,
                     const SurveyAssemblyOptions& options = {});

}  // namespace lumice::raypath::survey

#endif  // RAYPATH_DETAIL_SURVEY_ASSEMBLY_HPP_
