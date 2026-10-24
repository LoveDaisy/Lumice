#include "raypath/detail/input_scope.hpp"

#include <algorithm>
#include <utility>

namespace lumice::raypath::survey {

ScopeLocation ResolveSelectionLocation(const ConfigManager& config, const PathSelection& selected) {
  const auto& scene = config.scene_;
  size_t layer_index = selected.scene_layer.value_or(scene.ms_.size());
  if (!selected.scene_layer)
    for (size_t i = 0; i < scene.ms_.size(); ++i) {
      if (std::any_of(scene.ms_[i].setting_.begin(), scene.ms_[i].setting_.end(),
                      [&](const auto& setting) { return setting.crystal_.id_ == selected.crystal_id; })) {
        layer_index = i;
        break;
      }
    }
  return { layer_index < scene.ms_.size() ? SelectionLocation::kSceneLayer : SelectionLocation::kStandalone,
           layer_index };
}

Error ResolveCrystalSceneScope(const ConfigManager& config, const PathSelection& selected, CrystalSceneScope* out,
                               InputSnapshot* snapshot_out) {
  const auto crystal_id = selected.crystal_id;
  const auto& path_layers = selected.path_layers;
  const auto symmetry_bits = selected.symmetry_bits;
  if (path_layers.empty())
    return { ErrorCode::kInvalidPath, "no path layers" };
  const auto configured = config.crystals_.find(crystal_id);
  if (configured == config.crystals_.end()) {
    return { ErrorCode::kUnknownCrystalId, "unknown crystal id" };
  }
  // The class: physical P|B|D by default, the literal sequence at bits 0. A configured but
  // unused crystal stays a valid standalone diagnostic object (the report's layer rule).
  SceneConfig scene = config.scene_;
  const ScopeLocation resolved = ResolveSelectionLocation(config, selected);
  const size_t layer_index = resolved.layer_index;
  if (resolved.location == SelectionLocation::kStandalone && !selected.scene_layer) {
    ScatteringSetting setting{};
    setting.crystal_ = configured->second;
    setting.crystal_proportion_ = 1;
    scene.ms_.push_back({ 0, { setting } });
  }
  out->location = resolved.location;
  out->layer_index = layer_index;
  out->identity = out->location == SelectionLocation::kSceneLayer ?
                      "selected scene entry snapshot" :
                      "standalone configured crystal (not a scene allocation)";
  const std::vector<LayerSelection> selection{ { layer_index, crystal_id, path_layers[0], symmetry_bits,
                                                 out->location } };
  InputSnapshot snapshot;
  if (Error error = CaptureInput(scene, out->identity, selection, &snapshot); !error.Ok()) {
    return error;
  }
  PhysicalMemberScope scope;
  if (Error error = ResolvePhysicalMemberScope(snapshot.layers.front(), &scope); !error.Ok()) {
    return error;
  }
  out->members = std::move(scope.members);
  out->member_kind = scope.member_kind;
  out->density = ConvertAxisToPoseDensity(configured->second.axis_);
  if (snapshot_out != nullptr) {
    *snapshot_out = std::move(snapshot);
  }
  return {};
}

}  // namespace lumice::raypath::survey
