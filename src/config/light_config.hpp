#ifndef CONFIG_LIGHT_CONFIG_H_
#define CONFIG_LIGHT_CONFIG_H_

#include <nlohmann/json.hpp>
#include <variant>
#include <vector>

#include "core/scene_values.hpp"
#include "util/illuminant_data.hpp"

namespace lumice {

// convert to/from json object
void to_json(nlohmann::json& j, const LightSourceConfig& l);
void from_json(const nlohmann::json& j, LightSourceConfig& l);

}  // namespace lumice

#endif  // CONFIG_LIGHT_CONFIG_H_
