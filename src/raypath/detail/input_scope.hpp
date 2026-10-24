#ifndef LUMICE_RAYPATH_DETAIL_INPUT_SCOPE_HPP_
#define LUMICE_RAYPATH_DETAIL_INPUT_SCOPE_HPP_

#include "config/config_manager.hpp"
#include "raypath/detail/input_assembly.hpp"
#include "raypath/scene_to_analytic.hpp"

namespace lumice::raypath::survey {

// The class scope both consumers read (the survey document's assembly and the probe query's
// inline arm), resolved in one place so the scope rule has one statement: physical P|B|D by
// default, the literal sequence at bits 0; a configured but unused crystal stays a valid
// standalone diagnostic object (the report's layer rule).
struct CrystalSceneScope : MemberScope {
  PoseDensityConversion density;  // the axis's conversion (the caller gates Ok() itself)
  std::string identity;           // the snapshot identity (scene allocation or standalone)
};

// `snapshot_out` (optional) receives the resolved scene snapshot — the MC-ESS leg reads it; the
// probe's inline arm has no leg for it and passes null.
Error ResolveCrystalSceneScope(const ConfigManager& config, const PathSelection& selection, CrystalSceneScope* out,
                               InputSnapshot* snapshot_out = nullptr);


}  // namespace lumice::raypath::survey

#endif  // LUMICE_RAYPATH_DETAIL_INPUT_SCOPE_HPP_
