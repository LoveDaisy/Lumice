#ifndef SERVER_C_API_SCENE_INTERNAL_H_
#define SERVER_C_API_SCENE_INTERNAL_H_

// Scene-layer internals of the C API bridges: the handle behind LUMICE_Scene, the ConfigScratch
// JSON<->config codec, and the predicates the bridges above scene share with it. Includes nothing
// above the scene layer, so every bridge may include it. NOT part of the public C API: do not
// include it from src/gui/ or ship it to consumers.

#include <nlohmann/json.hpp>
#include <string>

#include "include/lumice_scene.h"

// LUMICE_Scene_ wraps an nlohmann::json `root` that is byte-isomorphic with the config wire
// format ConfigToJson emits (root["crystal"]/["filter"]/["scene"]/["render"]/["raypath_color"]).
// The Add*/Set* family reuses the SAME per-item encoders as ConfigToJson (single source), just
// one item at a time. Lifecycle mirrors LUMICE_Server_ (new/delete); the json copy constructor
// is a deep copy, so SceneClone is a one-liner with no hand-written field copying. Defined here
// rather than in the scene bridge because the engine bridge commits it and the single-path bridge
// analyses it.
struct LUMICE_Scene_ {
  nlohmann::json root;
};

// Test-only exposure of a LUMICE_Scene handle's internal JSON representation. NOT part of the
// public C API — do not include from src/gui/ or ship to consumers. Lets unit tests assert the
// Add*/Set* encoding matches ConfigToJson's established wire shape field-by-field, and lets tests
// verify SceneClone independence without waiting for a public scene-to-JSON serializer.
// Precondition: scene != nullptr.
const nlohmann::json& SceneRoot(const LUMICE_Scene* scene);

namespace lumice::capi {

// Is `id` one of the LUMICE_ANNOTATION_MARKER_* values? Shared by every place that accepts an id
// from a caller — a renderer's marker list here, and the annotation request and the marker
// direction query in the render bridge. One predicate, so a seventh id cannot be admitted by one
// and not the other.
// Spelled against LUMICE_MAX_CONFIG_MARKERS rather than LUMICE_ANNOTATION_MARKER_COUNT: the id
// macros live in lumice_render.h, a layer above this header. The two are the same number by
// construction — the renderer's marker array is EXACTLY the id space, since duplicates are
// rejected — and the render bridge, which sees both headers, pins them with a static_assert.
// The check is not redundant with core's own handling: annotation::ResolveMarkerDir answers an
// unknown id with the zenith rather than an error, deliberately (core does not assume its C++
// callers validated). Turning a caller's bad id into a REPORTED error instead of a silent fallback
// is this boundary's job.
inline bool IsValidMarkerId(int id) {
  return id >= 0 && id < LUMICE_MAX_CONFIG_MARKERS;
}

}  // namespace lumice::capi

// =============== ConfigScratch: the demoted wide config struct ===============
// This is the former public `LUMICE_Config` value struct, removed from the public ABI in v4.12
// and demoted to an implementation detail here. It survives for exactly one reason: JsonToConfig
// (the reader that fills it) and ConfigToJson (the writer that drains it) are the single
// validated JSON<->config implementation in this codebase — they carry every field mapping,
// bounds check, and strictness rule the handle path inherits. LUMICE_SceneFromJson / _FromJsonFile
// parse into a ConfigScratch and re-encode it into the new handle's root rather than growing a
// second, drifting JSON reader.
//
// KNOWN TECHNICAL DEBT (recorded so it is not rediscovered as an unexplained smell): that makes
// the JSON->Scene path a double hop (text -> ConfigScratch -> JSON root) rather than a direct
// parse into the handle. It was a deliberate trade — rewriting a validated parser is a far bigger
// risk than one extra re-encode on a non-hot path — not an oversight. See
// doc/capi-lifecycle-architecture.md for the same note on the architecture side.
//
// Deliberately NOT re-asserted here: the old 160 KB `static_assert` ABI ceiling. It existed
// because the type was public and callers could put it on their stack / pass it by value across
// the .so boundary. ConfigScratch never leaves this translation unit's control and is always
// heap-allocated (see JsonToScene), so its size is an ordinary implementation concern.
//
// It is still an OWNING type (raypath_color, and each compositions[i]'s term_ids/term_counts),
// so it must never be copied by value — enforced by scripts/check_policies.py's
// `no-config-by-value-copy` gate, which lists it in GUARDED_OWNING_TYPES. Use ConfigScratchGuard
// for scope-bound release.
struct ConfigScratch {
  // Crystals
  LUMICE_CrystalParam crystals[LUMICE_MAX_CONFIG_CRYSTALS];
  int crystal_count;

  // Filters
  LUMICE_FilterParam filters[LUMICE_MAX_CONFIG_FILTERS];
  int filter_count;

  // Complex-filter compositions (referenced by filters[].composition_index for COMPLEX type).
  LUMICE_ComplexComposition compositions[LUMICE_MAX_CONFIG_COMPLEX];
  int composition_count;

  // Renderers
  LUMICE_RenderParam renderers[LUMICE_MAX_CONFIG_RENDERERS];
  int renderer_count;

  // Scene: light source
  float sun_altitude;
  float sun_azimuth;
  float sun_diameter;
  const char* spectrum;  // e.g. "D65", "D50", "A", "E"; ignored when spectrum_count > 0
  // Discrete custom spectrum. When spectrum_count > 0, the spectrum string is ignored and this
  // list is used instead. spectrum_count == 0 means "use the spectrum string".
  LUMICE_SpectrumEntry spectrum_entries[LUMICE_MAX_CONFIG_SPECTRUM_ENTRIES];
  int spectrum_count;

  // Scene: simulation
  int infinite;             // 1=infinite rays, 0=finite
  LUMICE_RayCount ray_num;  // only used when infinite==0
  int max_hits;
  // Geometry-resampling clock K: how many rays reuse one sampled crystal instance before
  // resampling. 0 = disabled (core derives a SimBatch-based default); else must be in
  // [1, kGeomClockMax=64] (validated at commit by core config_manager.cpp).
  int geom_clock;
  // scene.ray_allocation, carried VERBATIM (the string as the document spelled it, or empty
  // when absent) so that core's parse of the re-encoded document is the one that decides what it
  // means — its enum table maps an unrecognized spelling onto proportional and warns, and
  // decoding here would silently swallow that warning. Sized for the two legal values with room
  // for a misspelling to survive the round trip and be reported.
  char ray_allocation[32];

  // Scene: scattering
  LUMICE_ScatterLayer scattering[LUMICE_MAX_CONFIG_SCATTER_LAYERS];
  int scatter_count;

  // Raypath color classes. raypath_color_count == 0 disables the color
  // path entirely. Ownership: raypath_color is owned by this struct and must be allocated /
  // released only through ConfigCreateColorClasses / ConfigReleaseColorClasses. Zero-initializing
  // this struct (e.g. `ConfigScratch cfg{};`) leaves raypath_color == nullptr /
  // raypath_color_count == 0, which is the "no color classes" state and is safe to release
  // (Release is idempotent / null-safe).
  LUMICE_ColorClass* raypath_color;
  int raypath_color_count;
  int raypath_color_mode;  // LUMICE_COLOR_MODE_DOMINANT / _ADDITIVE / _PAINTER
};

// Allocate `count` zero-initialized LUMICE_ColorClass entries and attach them to `cfg`, returning
// a pointer aliasing `cfg->raypath_color` for the caller to fill. `cfg == nullptr`, `count < 0`,
// or `count > LUMICE_MAX_CONFIG_COLOR_CLASSES` → returns nullptr with `cfg` untouched.
// `count == 0` releases any existing allocation and returns nullptr. `count > 0` is
// create-or-replace (a prior allocation is released first). Allocator is calloc/free.
LUMICE_ColorClass* ConfigCreateColorClasses(ConfigScratch* cfg, int count);

// Release the raypath_color allocation owned by `cfg`, if any. Idempotent and null-safe.
void ConfigReleaseColorClasses(ConfigScratch* cfg);

// Release the composition storage owned by every record in
// `cfg->compositions[0..composition_count)`. Does NOT touch composition_count or the inline
// compositions[] array itself. Idempotent and null-safe.
void ConfigReleaseCompositions(ConfigScratch* cfg);

// RAII for a ConfigScratch's two heap-backed owning field groups (raypath_color allocation +
// each compositions[i].term_ids/term_counts allocation). Attach immediately after declaration,
// BEFORE any call that may populate those fields:
//
//     ConfigScratch cfg{};
//     ConfigScratchGuard guard(cfg);
//     ParseConfigString(json_str, &cfg);   // may allocate raypath_color and compositions
//
// Non-copyable and non-movable so two guards can never point at the same scratch (which would
// double-Release both owning field groups).
class ConfigScratchGuard {
 public:
  explicit ConfigScratchGuard(ConfigScratch& cfg) noexcept : cfg_(&cfg) {}
  ~ConfigScratchGuard() {
    ConfigReleaseColorClasses(cfg_);
    ConfigReleaseCompositions(cfg_);
  }
  ConfigScratchGuard(const ConfigScratchGuard&) = delete;
  ConfigScratchGuard& operator=(const ConfigScratchGuard&) = delete;
  ConfigScratchGuard(ConfigScratchGuard&&) = delete;
  ConfigScratchGuard& operator=(ConfigScratchGuard&&) = delete;

 private:
  ConfigScratch* cfg_;
};

// Parse a JSON string into a ConfigScratch — the validated reader half described above, exposed
// so the parser-parity / strictness tests can exercise it directly against core's own parser
// instead of only through the Scene handle it feeds. Error codes are the ones
// LUMICE_SceneFromJson surfaces: LUMICE_ERR_NULL_ARG / _INVALID_JSON / _MISSING_FIELD /
// _INVALID_VALUE / _INVALID_CONFIG. May allocate `out`'s owning fields even on failure — pair it
// with a ConfigScratchGuard.
LUMICE_ErrorCode ParseConfigString(const char* json_str, ConfigScratch* out);

// Serialize a ConfigScratch into the core config JSON schema (the same shape
// core config/filter_config.cpp::to_json produces). Exposed here — rather than kept
// file-static in the scene bridge — so tests can assert the emitted shape field by field, and so the
// Scene tests can use it as the oracle their Add*/Set* encoding must match.
//
// Throws std::invalid_argument if any filter has an unset/invalid LUMICE_FilterParam.type.
nlohmann::json ConfigToJson(const ConfigScratch& c);

// Translate a single crystal's shape into the core crystal JSON schema, returning
// {"type": ..., "shape": {...}} WITHOUT the "id"/"axis" keys. This is the single
// mapping table shared by ConfigToJson (which adds id/axis around it) and
// LUMICE_GetCrystalMesh (which only needs type+shape to sample a preview mesh). Kept
// out of "id"/"axis" on purpose — those are meaningless for a stateless mesh preview;
// a dedicated test asserts they are absent so the boundary cannot silently drift.
nlohmann::json CrystalShapeToJson(const LUMICE_CrystalParam& cr);

// The full crystal object: CrystalShapeToJson's {"type", "shape"} plus "id" and the axis
// distributions — one entry of ConfigToJson's "crystal" array. Shared with the editor bridge,
// whose symmetry queries read a wire crystal through core's CrystalConfig parser. Throws
// std::invalid_argument on an invalid crystal.
nlohmann::json CrystalToJson(const LUMICE_CrystalParam& cr, int id);

// ---------- The codec's per-item halves the scene bridge also calls ----------
// The Add*/Set* family builds the handle's root one item at a time through the SAME encoders
// ConfigToJson is assembled from, the renderer getter reads one entry back through the same
// decoder JsonToConfig runs over the whole array, and JsonToScene parses through JsonToConfig
// itself. They are declared here only because the codec and the bridge are separate translation
// units (scene_config_to_json.cpp / scene_json_to_config.cpp / c_api_scene.cpp); nothing outside
// the scene layer should need them. Each keeps the contract stated at its definition.

// Encoders, defined in scene_config_to_json.cpp.
std::string SymmetryBitsToString(int bits);
void EncodeSimpleFilterBody(const LUMICE_FilterParam& f, nlohmann::json& j);
nlohmann::json CompositionArrayToJson(const LUMICE_ComplexComposition& comp);
nlohmann::json RendererToJson(const LUMICE_RenderParam& r, int id);
nlohmann::json ScatterLayerToJson(const LUMICE_ScatterLayer& layer);
nlohmann::json ColorClassToJson(const LUMICE_ColorClass& cls);
const char* ColorModeToString(int mode);
const char* RayAllocationModeToString(int mode);

// Decoders, defined in scene_json_to_config.cpp.
LUMICE_ErrorCode JsonToRenderer(const nlohmann::json& rj, LUMICE_RenderParam* out);
LUMICE_ErrorCode JsonToConfig(const nlohmann::json& root, ConfigScratch* out);

#endif  // SERVER_C_API_SCENE_INTERNAL_H_
