#ifndef LUMICE_RAYPATH_FEATURE_DISCOVERY_ADAPTER_HPP_
#define LUMICE_RAYPATH_FEATURE_DISCOVERY_ADAPTER_HPP_

// One-way product adapter: enumerate the actual SceneMeasure and materialize the pure analytic
// support contract. The analytic layer never includes or links this file.

#include "analytic/feature_discovery.hpp"
#include "raypath/scene_measure.hpp"

namespace lumice::raypath {

constexpr uint64_t kMaxMaterializedFeatureSupportRows = 1048576;

Error BuildFeatureSupportBatch(const ConfigManager& config, const SceneMeasureRequest& request,
                               analytic::FeatureSupportBatch* batch, SceneMeasureResult* measure,
                               analytic::FeatureReevaluateFn* reevaluate = nullptr);

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_FEATURE_DISCOVERY_ADAPTER_HPP_
