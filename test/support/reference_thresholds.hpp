#ifndef LUMICE_TEST_SUPPORT_REFERENCE_THRESHOLDS_HPP_
#define LUMICE_TEST_SUPPORT_REFERENCE_THRESHOLDS_HPP_

// Runtime resolver for the visual-regression PSNR thresholds, whose single source is
// test/gui/references/_thresholds.json — the file scripts/regen_gui_test_refs.py Phase B writes
// its calibration into. Every gate TU used to carry the number a second time as a literal, kept
// in step with the json by a manual copy-back after each regen; the literals are gone and the
// comparison harness reads the json instead, so a json diff IS the threshold change and the two
// copies cannot drift apart.
//
// Failure is loud, by design: a missing file, malformed JSON, or a missing
// (group, scene, threshold) entry returns ok=false with a one-line reason naming the path and the
// key, and the caller turns that into a red comparison. A broken threshold source reddens every
// PSNR comparison in the binary rather than silently comparing against some default. The json is
// a tracked file, so any checkout (CI, dev machine, reference machine) has it.
//
// Header-only, pure C++17 + <nlohmann/json.hpp>, no GL/ImGui/stb — same shape as
// pixel_diff_metrics.hpp, so the windowless gui_unit_test can include it. Not cached: the harness
// makes ~20 calls per binary and the file is ~6 KB, so a fresh parse per call is microseconds and
// the resolver holds no static state.

#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

namespace lumice::test {

struct ThresholdResolution {
  bool ok;
  double threshold;   // valid only when ok
  std::string error;  // one line, names the path and the key; empty when ok
};

// Reads one scene's threshold out of the regen-written json. `group`/`scene` are the same key
// everywhere: the "[<group>] <scene>:" stderr prefix the harness prints, the registry key
// scripts/regen_gui_test_refs.py attributes samples by, and the groups.<group>.scenes.<scene>
// path in the json itself.
inline ThresholdResolution ResolveReferenceThreshold(const char* json_path, const char* group, const char* scene) {
  std::ifstream in(json_path);
  if (!in.is_open()) {
    return { false, 0.0, std::string("threshold source not readable: ") + json_path };
  }
  nlohmann::json root;
  try {
    in >> root;
  } catch (const nlohmann::json::parse_error& e) {
    return { false, 0.0, std::string("threshold source is not valid JSON: ") + json_path + " (" + e.what() + ")" };
  }
  if (!root.is_object() || !root.contains("groups") || !root["groups"].is_object()) {
    return { false, 0.0, std::string("no 'groups' object in ") + json_path };
  }
  const nlohmann::json& groups = root["groups"];
  if (!groups.contains(group)) {
    return { false, 0.0, std::string("groups.") + group + ": group not found in " + json_path };
  }
  const nlohmann::json& group_node = groups[group];
  if (!group_node.is_object() || !group_node.contains("scenes") || !group_node["scenes"].is_object()) {
    return { false, 0.0, std::string("groups.") + group + ".scenes missing or malformed in " + json_path };
  }
  const nlohmann::json& scenes = group_node["scenes"];
  if (!scenes.contains(scene)) {
    return { false, 0.0, std::string("groups.") + group + ".scenes." + scene + ": scene not found in " + json_path };
  }
  const nlohmann::json& scene_node = scenes[scene];
  if (!scene_node.is_object() || !scene_node.contains("threshold") || !scene_node["threshold"].is_number()) {
    return { false, 0.0,
             std::string("groups.") + group + ".scenes." + scene + ".threshold missing or not a number in " +
                 json_path };
  }
  return { true, scene_node["threshold"].get<double>(), "" };
}

}  // namespace lumice::test

#endif  // LUMICE_TEST_SUPPORT_REFERENCE_THRESHOLDS_HPP_
