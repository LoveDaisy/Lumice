// C API bridge for lumice_scene.h: the Scene handle and the ConfigScratch storage it owns. The
// ConfigScratch JSON<->config codec the handle's FromJson / Add* / Set* paths go through lives in
// scene_config_to_json.cpp (encode) and scene_json_to_config.cpp (decode), declared in
// c_api_scene_internal.hpp.
// Registered at the scene layer in cmake/lumice_layers.cmake, so it may include nothing above it.

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <utility>

#include "config/raypath_color_config.hpp"  // ns::kDefaultCompositeMode (single-source default)
#include "include/lumice_scene.h"
#include "server/c_api_scene_internal.hpp"
#include "util/logger.hpp"
#include "util/path_utils.hpp"

namespace ns = lumice;


// =============== Scene (opaque handle) ===============
// Empty-scene skeleton: the same shape ConfigToJson emits for a zero-initialized ConfigScratch,
// minus the optional raypath_color (absent until SetColorMode/AddColorClass — the same
// "count == 0 → omit the key" isomorphism the batch path keeps). Scene scalars default to a
// default config's values; Set* overwrites them.
static nlohmann::json MakeEmptySceneRoot() {
  nlohmann::json root;
  root["crystal"] = nlohmann::json::array();
  root["filter"] = nlohmann::json::array();
  nlohmann::json scene;
  scene["light_source"]["type"] = "sun";
  scene["light_source"]["altitude"] = 0.0f;
  scene["light_source"]["azimuth"] = 0.0f;
  scene["light_source"]["diameter"] = 0.0f;
  scene["light_source"]["spectrum"] = "D65";
  scene["ray_num"] = static_cast<LUMICE_RayCount>(0);
  scene["max_hits"] = 0;
  scene["scattering"] = nlohmann::json::array();
  root["scene"] = scene;
  root["render"] = nlohmann::json::array();
  return root;
}

// Create root["raypath_color"] = {"mode": <core default>, "classes": []} if absent, so mode and
// classes always coexist once either SetColorMode or AddColorClass has touched the color path.
// The key stays entirely absent until then (matches ConfigToJson's "raypath_color_count == 0 →
// omit" byte-isomorphism). Uses the SAME core single-source default the batch reader falls back
// to (ns::kDefaultCompositeMode).
static void EnsureRaypathColor(nlohmann::json& root) {
  if (!root.contains("raypath_color")) {
    nlohmann::json rc;
    rc["mode"] = ns::kDefaultCompositeMode;
    rc["classes"] = nlohmann::json::array();
    root["raypath_color"] = rc;
  }
}

// ---------- Lifecycle ----------
LUMICE_Scene* LUMICE_SceneCreate(void) {
  // Build the skeleton first so a throw (bad_alloc) leaks nothing.
  return new LUMICE_Scene_{ MakeEmptySceneRoot() };
}


LUMICE_Scene* LUMICE_SceneClone(const LUMICE_Scene* scene) {
  if (!scene) {
    return nullptr;
  }
  // nlohmann::json's copy constructor deep-copies the whole tree — no aliasing with the source.
  return new LUMICE_Scene_(*scene);
}


void LUMICE_SceneDestroy(LUMICE_Scene* scene) {
  delete scene;  // delete nullptr is a no-op (NULL-safe, same as LUMICE_DestroyServer)
}


// ---------- Incremental build: Add* ----------
LUMICE_ErrorCode LUMICE_SceneAddCrystal(LUMICE_Scene* scene, const LUMICE_CrystalParam* crystal, int* out_id) {
  if (!scene || !crystal || !out_id) {
    return LUMICE_ERR_NULL_ARG;
  }
  auto& arr = scene->root["crystal"];
  if (arr.size() >= LUMICE_MAX_CONFIG_CRYSTALS) {
    return LUMICE_ERR_INVALID_CONFIG;
  }
  const int id = static_cast<int>(arr.size());
  // Encode into a temporary first: any throw leaves the scene untouched (no partial write).
  nlohmann::json j;
  try {
    j = CrystalToJson(*crystal, id);
  } catch (const std::exception& e) {
    LOG_ERROR("LUMICE_SceneAddCrystal: invalid crystal: {}", e.what());
    return LUMICE_ERR_INVALID_CONFIG;
  }
  arr.push_back(std::move(j));
  *out_id = id;
  return LUMICE_OK;
}


LUMICE_ErrorCode LUMICE_SceneAddFilter(LUMICE_Scene* scene, const LUMICE_FilterParam* filter, int* out_id) {
  if (!scene || !filter || !out_id) {
    return LUMICE_ERR_NULL_ARG;
  }
  auto& arr = scene->root["filter"];
  if (arr.size() >= LUMICE_MAX_CONFIG_FILTERS) {
    return LUMICE_ERR_INVALID_CONFIG;
  }
  // Defensive bound on the fixed-size raypath[] the struct carries (ConfigToJson trusts this;
  // the Scene validates at Add-time so an out-of-range count returns an error, not an OOB read).
  if (filter->type == LUMICE_FILTER_TYPE_RAYPATH &&
      (filter->raypath_count < 0 || filter->raypath_count > LUMICE_MAX_CONFIG_RAYPATH_LEN)) {
    return LUMICE_ERR_INVALID_CONFIG;
  }
  const int id = static_cast<int>(arr.size());
  nlohmann::json j;
  j["id"] = id;
  j["action"] = filter->action == 0 ? "filter_in" : "filter_out";
  try {
    // EncodeSimpleFilterBody rejects COMPLEX (use SceneAddComplexFilter) and UNSET/invalid.
    EncodeSimpleFilterBody(*filter, j);
  } catch (const std::exception& e) {
    LOG_ERROR("LUMICE_SceneAddFilter: invalid filter: {}", e.what());
    return LUMICE_ERR_INVALID_CONFIG;
  }
  j["symmetry"] = SymmetryBitsToString(filter->symmetry);
  arr.push_back(std::move(j));
  *out_id = id;
  return LUMICE_OK;
}


LUMICE_ErrorCode LUMICE_SceneAddComplexFilter(LUMICE_Scene* scene, const LUMICE_FilterParam* filter,
                                              const LUMICE_ComplexComposition* composition, int* out_id) {
  if (!scene || !filter || !composition || !out_id) {
    return LUMICE_ERR_NULL_ARG;
  }
  auto& arr = scene->root["filter"];
  if (arr.size() >= LUMICE_MAX_CONFIG_FILTERS) {
    return LUMICE_ERR_INVALID_CONFIG;
  }
  // Validate the composition storage before writing anything (clause/term bounds +
  // null-pointer defense). term_ids may legitimately be null iff every
  // clause has 0 terms (total_terms == 0) — see LUMICE_CompositionSetClauses.
  const auto& comp = *composition;
  if (comp.clause_count < 0 || comp.clause_count > LUMICE_MAX_CONFIG_CLAUSES) {
    return LUMICE_ERR_INVALID_CONFIG;
  }
  if (comp.clause_count > 0 && comp.term_counts == nullptr) {
    return LUMICE_ERR_NULL_ARG;
  }
  size_t total_terms = 0;
  for (int cl = 0; cl < comp.clause_count; cl++) {
    if (comp.term_counts[cl] < 0 || comp.term_counts[cl] > LUMICE_MAX_CONFIG_TERMS) {
      return LUMICE_ERR_INVALID_CONFIG;
    }
    total_terms += static_cast<size_t>(comp.term_counts[cl]);
  }
  if (total_terms > 0 && comp.term_ids == nullptr) {
    return LUMICE_ERR_NULL_ARG;
  }
  const int id = static_cast<int>(arr.size());
  nlohmann::json j;
  j["id"] = id;
  j["action"] = filter->action == 0 ? "filter_in" : "filter_out";
  j["type"] = "complex";
  // Deep-copy the composition into the JSON now: term_ids/term_counts are read and encoded, so
  // the caller's LUMICE_ComplexComposition may be released/reused right after this returns.
  j["composition"] = CompositionArrayToJson(comp);
  j["symmetry"] = SymmetryBitsToString(filter->symmetry);
  arr.push_back(std::move(j));
  *out_id = id;
  return LUMICE_OK;
}


LUMICE_ErrorCode LUMICE_SceneAddRenderer(LUMICE_Scene* scene, const LUMICE_RenderParam* renderer, int* out_id) {
  if (!scene || !renderer || !out_id) {
    return LUMICE_ERR_NULL_ARG;
  }
  // Grid counts index the fixed-capacity inline arrays RendererToJson reads; validate before the
  // encode so an out-of-range count cannot walk off the end of
  // angular_dist[]/view_dist[]/elevation_grid[]/longitude_grid[].
  if (renderer->angular_dist_count < 0 || renderer->angular_dist_count > LUMICE_MAX_CONFIG_GRID_LINES ||
      renderer->view_dist_count < 0 || renderer->view_dist_count > LUMICE_MAX_CONFIG_GRID_LINES ||
      renderer->elevation_grid_count < 0 || renderer->elevation_grid_count > LUMICE_MAX_CONFIG_GRID_LINES ||
      renderer->longitude_grid_count < 0 || renderer->longitude_grid_count > LUMICE_MAX_CONFIG_GRID_LINES) {
    return LUMICE_ERR_INVALID_CONFIG;
  }
  auto& arr = scene->root["render"];
  if (arr.size() >= LUMICE_MAX_CONFIG_RENDERERS) {
    return LUMICE_ERR_INVALID_CONFIG;
  }
  const int id = static_cast<int>(arr.size());
  // Encode first (throws on an invalid lens_type / visible) so a failure leaves the scene
  // untouched — mirrors LUMICE_SceneAddColorClass.
  nlohmann::json jr;
  try {
    jr = RendererToJson(*renderer, id);
  } catch (const std::exception& e) {
    LOG_ERROR("LUMICE_SceneAddRenderer: invalid renderer: {}", e.what());
    return LUMICE_ERR_INVALID_CONFIG;
  }
  arr.push_back(std::move(jr));
  *out_id = id;
  return LUMICE_OK;
}

LUMICE_ErrorCode LUMICE_SceneGetRenderer(const LUMICE_Scene* scene, int index, LUMICE_RenderParam* out) {
  if (!scene || !out) {
    return LUMICE_ERR_NULL_ARG;
  }
  // Both construction paths leave root["render"][i] in the one shape RendererToJson writes —
  // SceneAddRenderer writes it directly, SceneFromJson/File re-encodes the parsed config through
  // ConfigToJson — so one decoder (JsonToRenderer, the body of the FromJson path's own loop) reads
  // either back with core's defaults already applied. The skeleton always seeds the "render" key;
  // the .at() sits inside the try all the same, so a handle that somehow lost it reports an error
  // code instead of throwing across the C boundary.
  try {
    const auto& arr = scene->root.at("render");
    if (index < 0 || static_cast<size_t>(index) >= arr.size()) {
      return LUMICE_ERR_INVALID_VALUE;
    }
    *out = LUMICE_RenderParam{};
    return JsonToRenderer(arr[index], out);
  } catch (const std::exception& e) {
    LOG_ERROR("LUMICE_SceneGetRenderer: could not decode renderer[{}]: {}", index, e.what());
    return LUMICE_ERR_INVALID_CONFIG;
  }
}


LUMICE_ErrorCode LUMICE_SceneAddScatterLayer(LUMICE_Scene* scene, const LUMICE_ScatterLayer* layer, int* out_id) {
  if (!scene || !layer || !out_id) {
    return LUMICE_ERR_NULL_ARG;
  }
  if (layer->entry_count < 0 || layer->entry_count > LUMICE_MAX_CONFIG_SCATTER_ENTRIES) {
    return LUMICE_ERR_INVALID_CONFIG;
  }
  auto& scene_j = scene->root["scene"];
  // The skeleton always seeds an empty scattering array; guard defensively in case a scene built
  // by a future FromJson path lacks it.
  if (!scene_j.contains("scattering") || !scene_j["scattering"].is_array()) {
    scene_j["scattering"] = nlohmann::json::array();
  }
  auto& arr = scene_j["scattering"];
  if (arr.size() >= LUMICE_MAX_CONFIG_SCATTER_LAYERS) {
    return LUMICE_ERR_INVALID_CONFIG;
  }
  const int id = static_cast<int>(arr.size());
  arr.push_back(ScatterLayerToJson(*layer));
  *out_id = id;
  return LUMICE_OK;
}


LUMICE_ErrorCode LUMICE_SceneAddColorClass(LUMICE_Scene* scene, const LUMICE_ColorClass* color_class, int* out_id) {
  if (!scene || !color_class || !out_id) {
    return LUMICE_ERR_NULL_ARG;
  }
  if (color_class->match_count < 0 || color_class->match_count > LUMICE_MAX_CONFIG_COLOR_REFS) {
    return LUMICE_ERR_INVALID_CONFIG;
  }
  // Encode first (throws on invalid combine / predicate) so a failure leaves the scene untouched
  // — do NOT create the raypath_color key before this succeeds.
  nlohmann::json jc;
  try {
    jc = ColorClassToJson(*color_class);
  } catch (const std::exception& e) {
    LOG_ERROR("LUMICE_SceneAddColorClass: invalid color class: {}", e.what());
    return LUMICE_ERR_INVALID_CONFIG;
  }
  // Soft cap check must peek at the current class count WITHOUT creating the key.
  int existing = 0;
  if (scene->root.contains("raypath_color") && scene->root["raypath_color"].contains("classes")) {
    existing = static_cast<int>(scene->root["raypath_color"]["classes"].size());
  }
  if (existing >= LUMICE_MAX_CONFIG_COLOR_CLASSES) {
    return LUMICE_ERR_INVALID_CONFIG;
  }
  EnsureRaypathColor(scene->root);
  auto& arr = scene->root["raypath_color"]["classes"];
  const int id = static_cast<int>(arr.size());
  arr.push_back(std::move(jc));
  *out_id = id;
  return LUMICE_OK;
}


// ---------- Scalar / whole-group settings: Set* ----------
LUMICE_ErrorCode LUMICE_SceneSetLightSource(LUMICE_Scene* scene, float sun_altitude, float sun_azimuth,
                                            float sun_diameter, const char* spectrum) {
  if (!scene) {
    return LUMICE_ERR_NULL_ARG;
  }
  auto& ls = scene->root["scene"]["light_source"];
  ls["type"] = "sun";
  ls["altitude"] = sun_altitude;
  ls["azimuth"] = sun_azimuth;
  ls["diameter"] = sun_diameter;
  // A discrete spectrum (an array set by SceneSetCustomSpectrum) wins over the string, mirroring
  // the ConfigScratch spectrum_count > 0 rule — do NOT clobber it here. Only write the string
  // when no discrete spectrum is currently set. This makes the two call orders converge.
  if (!ls.contains("spectrum") || !ls["spectrum"].is_array()) {
    ls["spectrum"] = spectrum ? spectrum : "D65";
  }
  return LUMICE_OK;
}


LUMICE_ErrorCode LUMICE_SceneSetCustomSpectrum(LUMICE_Scene* scene, const LUMICE_SpectrumEntry* entries, int count) {
  if (!scene) {
    return LUMICE_ERR_NULL_ARG;
  }
  if (count < 0 || count > LUMICE_MAX_CONFIG_SPECTRUM_ENTRIES) {
    return LUMICE_ERR_INVALID_CONFIG;
  }
  if (count > 0 && entries == nullptr) {
    return LUMICE_ERR_NULL_ARG;
  }
  auto& ls = scene->root["scene"]["light_source"];
  if (count == 0) {
    // Clear the discrete spectrum: fall back to the default string (matches spectrum_count == 0).
    ls["spectrum"] = "D65";
  } else {
    nlohmann::json spectrum = nlohmann::json::array();
    for (int i = 0; i < count; i++) {
      nlohmann::json e;
      e["wavelength"] = entries[i].wavelength;
      e["weight"] = entries[i].weight;
      spectrum.push_back(e);
    }
    ls["spectrum"] = spectrum;
  }
  return LUMICE_OK;
}


LUMICE_ErrorCode LUMICE_SceneSetSimParams(LUMICE_Scene* scene, int infinite, LUMICE_RayCount ray_num, int max_hits,
                                          int geom_clock) {
  if (!scene) {
    return LUMICE_ERR_NULL_ARG;
  }
  auto& scene_j = scene->root["scene"];
  if (infinite) {
    scene_j["ray_num"] = "infinite";
  } else {
    scene_j["ray_num"] = ray_num;
  }
  scene_j["max_hits"] = max_hits;
  // geom_clock: emit only when set (mirrors ConfigToJson's `if (geom_clock != 0)`); erase on 0 so
  // a prior non-zero value is cleared and the "omit when 0" isomorphism holds.
  if (geom_clock != 0) {
    scene_j["geom_clock"] = geom_clock;
  } else {
    scene_j.erase("geom_clock");
  }
  return LUMICE_OK;
}


LUMICE_ErrorCode LUMICE_SceneSetRayAllocation(LUMICE_Scene* scene, int mode) {
  if (!scene) {
    return LUMICE_ERR_NULL_ARG;
  }
  const char* mode_str = nullptr;
  try {
    mode_str = RayAllocationModeToString(mode);
  } catch (const std::exception& e) {
    LOG_ERROR("LUMICE_SceneSetRayAllocation: {}", e.what());
    return LUMICE_ERR_INVALID_CONFIG;
  }
  scene->root["scene"]["ray_allocation"] = mode_str;
  return LUMICE_OK;
}


LUMICE_ErrorCode LUMICE_SceneSetColorMode(LUMICE_Scene* scene, int raypath_color_mode) {
  if (!scene) {
    return LUMICE_ERR_NULL_ARG;
  }
  const char* mode_str = nullptr;
  try {
    mode_str = ColorModeToString(raypath_color_mode);
  } catch (const std::exception& e) {
    LOG_ERROR("LUMICE_SceneSetColorMode: {}", e.what());
    return LUMICE_ERR_INVALID_CONFIG;
  }
  EnsureRaypathColor(scene->root);
  scene->root["raypath_color"]["mode"] = mode_str;
  return LUMICE_OK;
}


// Test-only accessor (declared in server/c_api_scene_internal.hpp). See that header for rationale.
const nlohmann::json& SceneRoot(const LUMICE_Scene* scene) {
  return scene->root;
}


// ---------- Serialization: decoupled from commit ----------
// SceneToJson lives in the Scene section because it only depends on scene->root (no JsonToConfig).
// Buffer contract is snprintf-style (see lumice_scene.h). root.dump() can throw type_error if a
// Set* stored a non-UTF-8 string (SetLightSource/SetCustomSpectrum take an unvalidated const char*),
// so the dump is guarded — the exception must not cross the C ABI boundary.
LUMICE_ErrorCode LUMICE_SceneToJson(const LUMICE_Scene* scene, char* out_buf, size_t buf_size, size_t* out_len) {
  if (!scene) {
    return LUMICE_ERR_NULL_ARG;
  }
  std::string json_str;
  try {
    json_str = scene->root.dump();
  } catch (const std::exception& e) {
    LOG_ERROR("LUMICE_SceneToJson: failed to serialize scene: {}", e.what());
    return LUMICE_ERR_INVALID_CONFIG;
  }
  if (out_len) {
    *out_len = json_str.size();
  }
  if (out_buf && buf_size > 0) {
    size_t n = std::min(json_str.size(), buf_size - 1);
    std::memcpy(out_buf, json_str.data(), n);
    out_buf[n] = '\0';
  }
  return LUMICE_OK;
}


// =============== ConfigScratch raypath-color storage (internal) ===============
// See c_api_scene_internal.hpp for the ownership contract. calloc/free (not new[]/delete[]) is kept
// from when this pair was public C API: the allocation shape must stay interchangeable with
// LUMICE_CompositionSetClauses's, which still crosses the C ABI and must remain releasable
// without a C++ runtime — a documented exception to the "no raw new/delete" rule (AGENTS.md).
LUMICE_ColorClass* ConfigCreateColorClasses(ConfigScratch* cfg, int count) {
  if (!cfg) {
    return nullptr;
  }
  if (count < 0 || count > LUMICE_MAX_CONFIG_COLOR_CLASSES) {
    return nullptr;
  }
  // Create-or-replace: any existing allocation must be released before we overwrite the
  // pointer. Guards the "consecutive Create with different counts" pattern from leaking
  // the previous allocation, and pairs with the memset-before-Release entry in JsonToConfig
  // (which calls Release explicitly to make the intent obvious).
  if (cfg->raypath_color) {
    std::free(cfg->raypath_color);  // NOLINT(cppcoreguidelines-no-malloc): C ABI boundary; see block comment above.
    cfg->raypath_color = nullptr;
  }
  if (count == 0) {
    // Explicitly skip calloc(0, ...) — implementation-defined behavior. Zero classes is a
    // valid state (mono-only); leave pointer nullptr and count 0.
    cfg->raypath_color_count = 0;
    return nullptr;
  }
  auto* buf = static_cast<LUMICE_ColorClass*>(
      std::calloc(  // NOLINT(cppcoreguidelines-no-malloc): C ABI boundary; see block comment above.
          static_cast<size_t>(count), sizeof(LUMICE_ColorClass)));
  if (!buf) {
    // OOM: leave cfg in the "no classes" state, callers must check the return value.
    cfg->raypath_color_count = 0;
    return nullptr;
  }
  cfg->raypath_color = buf;
  cfg->raypath_color_count = count;
  return buf;
}

void ConfigReleaseColorClasses(ConfigScratch* cfg) {
  if (!cfg) {
    return;
  }
  if (cfg->raypath_color) {
    std::free(cfg->raypath_color);  // NOLINT(cppcoreguidelines-no-malloc): C ABI boundary; see block comment above.
    cfg->raypath_color = nullptr;
  }
  cfg->raypath_color_count = 0;
}


// =============== Complex-Composition storage lifecycle (BREAKING v4.9) ===============
// calloc/free (not new[]/delete[]) deliberately: LUMICE_ComplexComposition crosses the C ABI,
// and non-C++ bindings must be able to release the composition storage without a C++ runtime —
// a documented exception to the "no raw new/delete" project rule (AGENTS.md).

LUMICE_ErrorCode LUMICE_CompositionSetClauses(LUMICE_ComplexComposition* comp, int clause_count, const int* term_counts,
                                              const int* term_ids) {
  if (!comp) {
    return LUMICE_ERR_NULL_ARG;
  }
  if (clause_count < 0 || clause_count > LUMICE_MAX_CONFIG_CLAUSES) {
    return LUMICE_ERR_INVALID_CONFIG;
  }
  // term_counts is required whenever clause_count > 0 (every clause needs its own count).
  // term_ids, however, must NOT be required upfront: a composition where every clause has 0
  // terms (total_terms == 0) is legitimate (LUMICE_CompositionClauseTerms's doc comment already
  // treats it as such), and a caller with
  // total_terms == 0 may reasonably pass term_ids == nullptr for that empty flat buffer — e.g.
  // JsonToComplexComposition's std::vector<int> term_ids_vec, whose .data() is nullptr when
  // never push_back'd into. Requiring non-null unconditionally rejected exactly that legitimate
  // shape at the ONLY writer, before any consumer ever saw it (code-review round 2, Major —
  // round 1's fix only patched the read-side check, not this entry point). So the term_ids
  // null-check is deferred until total_terms is known.
  if (clause_count > 0 && term_counts == nullptr) {
    return LUMICE_ERR_NULL_ARG;
  }
  // Full validation BEFORE any allocation so an invalid input leaves `comp` untouched
  // (mirrors the "no partial writes on rejection" contract other Set-style API here follow).
  // Upper bounds are enforced here (not just by callers) because this is documented as
  // the ONLY supported writer, including non-C++ bindings that cannot be trusted to
  // pre-check LUMICE_MAX_CONFIG_CLAUSES/_TERMS themselves (code-review round 1, Major).
  size_t total_terms = 0;
  for (int cl = 0; cl < clause_count; cl++) {
    if (term_counts[cl] < 0 || term_counts[cl] > LUMICE_MAX_CONFIG_TERMS) {
      return LUMICE_ERR_INVALID_CONFIG;
    }
    total_terms += static_cast<size_t>(term_counts[cl]);
  }
  if (total_terms > 0 && term_ids == nullptr) {
    return LUMICE_ERR_NULL_ARG;
  }

  // Create-or-replace: release any prior allocation before overwriting the pointers,
  // guarding "consecutive Set with different clause_count" from leaking the previous
  // allocation. Pairs with the memset-before-Release entry in JsonToConfig.
  LUMICE_CompositionReleaseClauses(comp);

  if (clause_count == 0) {
    // "OR of nothing" state — no allocation, both pointers stay nullptr.
    return LUMICE_OK;
  }

  auto* counts_buf =
      static_cast<int*>(std::calloc(  // NOLINT(cppcoreguidelines-no-malloc): C ABI boundary; see block comment above.
          static_cast<size_t>(clause_count), sizeof(int)));
  if (!counts_buf) {
    // OOM: fall back to "OR of nothing" (already released above); mirror ColorClasses OOM policy.
    return LUMICE_ERR_INVALID_CONFIG;
  }
  int* ids_buf = nullptr;
  if (total_terms > 0) {
    ids_buf =
        static_cast<int*>(std::calloc(  // NOLINT(cppcoreguidelines-no-malloc): C ABI boundary; see block comment above.
            total_terms, sizeof(int)));
    if (!ids_buf) {
      std::free(counts_buf);  // NOLINT(cppcoreguidelines-no-malloc): C ABI boundary.
      return LUMICE_ERR_INVALID_CONFIG;
    }
  }
  // Both allocations succeeded — commit.
  for (int cl = 0; cl < clause_count; cl++) {
    counts_buf[cl] = term_counts[cl];
  }
  for (size_t i = 0; i < total_terms; i++) {
    ids_buf[i] = term_ids[i];
  }
  comp->term_counts = counts_buf;
  comp->term_ids = ids_buf;
  comp->clause_count = clause_count;
  return LUMICE_OK;
}

void LUMICE_CompositionReleaseClauses(LUMICE_ComplexComposition* comp) {
  if (!comp) {
    return;
  }
  if (comp->term_counts) {
    std::free(comp->term_counts);  // NOLINT(cppcoreguidelines-no-malloc): C ABI boundary; see block comment above.
    comp->term_counts = nullptr;
  }
  if (comp->term_ids) {
    std::free(comp->term_ids);  // NOLINT(cppcoreguidelines-no-malloc): C ABI boundary; see block comment above.
    comp->term_ids = nullptr;
  }
  comp->clause_count = 0;
}

void ConfigReleaseCompositions(ConfigScratch* cfg) {
  if (!cfg) {
    return;
  }
  // Composition_count is a plain int written by callers; iterate up to whatever they set,
  // but never past the ABI ceiling (defends against a garbage / uninitialized count value).
  int n = cfg->composition_count;
  if (n < 0) {
    n = 0;
  }
  if (n > LUMICE_MAX_CONFIG_COMPLEX) {
    n = LUMICE_MAX_CONFIG_COMPLEX;
  }
  for (int i = 0; i < n; i++) {
    LUMICE_CompositionReleaseClauses(&cfg->compositions[i]);
  }
  // Leave composition_count alone — this Release only frees the intra-record heap storage;
  // the inline compositions[] array itself is part of ConfigScratch's own layout.
}

const int* LUMICE_CompositionClauseTerms(const LUMICE_ComplexComposition* comp, int clause_index, int* out_term_count) {
  if (!comp) {
    if (out_term_count) {
      *out_term_count = 0;
    }
    return nullptr;
  }
  if (clause_index < 0 || clause_index >= comp->clause_count) {
    if (out_term_count) {
      *out_term_count = 0;
    }
    return nullptr;
  }
  // Prefix-sum offset into the flat term_ids buffer.
  size_t offset = 0;
  for (int cl = 0; cl < clause_index; cl++) {
    offset += static_cast<size_t>(comp->term_counts[cl]);
  }
  if (out_term_count) {
    *out_term_count = comp->term_counts[clause_index];
  }
  return (comp->term_ids != nullptr) ? (comp->term_ids + offset) : nullptr;
}


// ---------- Serialization: decoupled from commit (JSON -> new Scene) ----------
// JsonToScene reuses the established JsonToConfig validator (single source of truth — no parallel
// JSON reader that could drift from it) by parsing into a temporary ConfigScratch, then re-encodes
// that struct via ConfigToJson into the new handle's root. It NEVER touches LUMICE_Server: this is
// the serialization half of the handle API, deliberately independent of commit/re-sim.
//
// This double hop (text -> ConfigScratch -> JSON root) is the known, deliberate technical debt
// recorded at ConfigScratch's declaration in c_api_scene_internal.hpp — the alternative was rewriting a
// validated parser, a far larger risk than one re-encode on a non-hot path.
//
// The temporary ConfigScratch is heap-allocated (it is large; this scrum's history includes a real
// 512 KB stack overflow from an oversized on-stack copy of this struct). Its owning heap fields
// (raypath_color, compositions[].term_ids/term_counts) are freed on EVERY return path by the
// custom deleter — JsonToConfig may leave them partially allocated when it fails mid-parse (same
// hazard JsonToConfig itself guards against with its memset-before-Release entry).
namespace {
struct ConfigDeleter {
  void operator()(ConfigScratch* cfg) const {
    if (cfg) {
      ConfigReleaseColorClasses(cfg);
      ConfigReleaseCompositions(cfg);
      delete cfg;
    }
  }
};
}  // namespace

static LUMICE_ErrorCode JsonToScene(const nlohmann::json& root, LUMICE_Scene** out_scene) {
  *out_scene = nullptr;  // contract: *out_scene is NULL on any failure, no handle to Destroy
  std::unique_ptr<ConfigScratch, ConfigDeleter> tmp(new ConfigScratch());
  auto err = JsonToConfig(root, tmp.get());
  if (err != LUMICE_OK) {
    return err;
  }
  try {
    *out_scene = new LUMICE_Scene_{ ConfigToJson(*tmp) };
  } catch (const std::exception& e) {
    LOG_ERROR("LUMICE_SceneFromJson: failed to re-encode parsed config: {}", e.what());
    return LUMICE_ERR_INVALID_CONFIG;
  }
  return LUMICE_OK;
}


LUMICE_ErrorCode LUMICE_SceneFromJson(const char* json_str, LUMICE_Scene** out_scene) {
  if (out_scene) {
    *out_scene = nullptr;
  }
  if (!json_str || !out_scene) {
    return LUMICE_ERR_NULL_ARG;
  }

  try {
    auto root = nlohmann::json::parse(json_str);
    return JsonToScene(root, out_scene);
  } catch (const nlohmann::json::parse_error&) {
    return LUMICE_ERR_INVALID_JSON;
  } catch (const std::exception&) {
    // See ParseConfigString for why this is std::exception, not nlohmann::json::exception:
    // JsonToScene's inner JsonToConfig call runs the same decode-direction Map*ToCApi throws.
    return LUMICE_ERR_INVALID_VALUE;
  }
}


LUMICE_ErrorCode LUMICE_SceneFromJsonFile(const char* filename, LUMICE_Scene** out_scene) {
  if (out_scene) {
    *out_scene = nullptr;
  }
  if (!filename || !out_scene) {
    return LUMICE_ERR_NULL_ARG;
  }

  std::ifstream file(lumice::PathFromU8(filename));
  if (!file.is_open()) {
    return LUMICE_ERR_FILE_NOT_FOUND;
  }

  try {
    auto root = nlohmann::json::parse(file);
    return JsonToScene(root, out_scene);
  } catch (const nlohmann::json::parse_error&) {
    return LUMICE_ERR_INVALID_JSON;
  } catch (const std::exception&) {
    // See ParseConfigString for why this is std::exception, not nlohmann::json::exception.
    return LUMICE_ERR_INVALID_VALUE;
  }
}
