#ifndef RAYPATH_DETAIL_SURVEY_ASSEMBLY_HPP_
#define RAYPATH_DETAIL_SURVEY_ASSEMBLY_HPP_

// The survey's T0 assembly: the request's class under the selected scope, routed by the kernel's
// rank-0 certificate. Every member's classification is the kernel's own (analytic::Classify —
// the LUMICE_ANALYTIC_ClassifyCriticalStructure implementation, whose rank-0 arm is
// IsRankZeroPath's M = I and zero-wedge check); nothing here re-derives a geometry verdict.

#include "config/config_manager.hpp"
#include "raypath/detail/input_assembly.hpp"
#include "raypath/detail/survey/survey_document.hpp"
#include "raypath/scene_to_analytic.hpp"
#include "raypath/survey_report.hpp"

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

Error AssembleSurvey(const ConfigManager& config, const SurveyRequest& request, SurveyDocument* out,
                     const SurveyAssemblyOptions& options = {});

// The class scope both consumers read (the survey document's assembly and the probe query's
// inline arm), resolved in one place so the scope rule has one statement: physical P|B|D by
// default, the literal sequence at bits 0; a configured but unused crystal stays a valid
// standalone diagnostic object (the report's layer rule).
struct CrystalSceneScope {
  std::vector<std::vector<int>> members;  // the physical P|B|D members the crystal resolves to
  PoseDensityConversion density;          // the axis's conversion (the caller gates Ok() itself)
  std::string identity;                   // the snapshot identity (scene allocation or standalone)
  bool in_scene = false;                  // false: the crystal is no scene allocation (standalone)
  size_t layer_index = 0;                 // the allocated scene layer, meaningful when in_scene
};

// `snapshot_out` (optional) receives the resolved scene snapshot — the MC-ESS leg reads it; the
// probe's inline arm has no leg for it and passes null.
Error ResolveCrystalSceneScope(const ConfigManager& config, IdType crystal_id,
                               const std::vector<std::vector<int>>& path_layers, uint8_t symmetry_bits,
                               CrystalSceneScope* out, InputSnapshot* snapshot_out = nullptr);

}  // namespace lumice::raypath::survey

#endif  // RAYPATH_DETAIL_SURVEY_ASSEMBLY_HPP_
