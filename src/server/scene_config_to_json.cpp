// The encode half of the ConfigScratch JSON<->config codec: ConfigScratch (and its per-item C
// structs) -> core config JSON. ConfigToJson is its entry point; the per-item encoders it is built
// from are also declared in c_api_scene_internal.hpp because the scene bridge's Add*/Set* family
// encodes one item at a time through the same functions. The decode half is scene_json_to_config.cpp.
// Registered at the scene layer in cmake/lumice_layers.cmake, so it may include nothing above it.

#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <vector>

#include "config/crystal_config.hpp"
#include "config/raypath_color_config.hpp"  // ns::kDefaultCompositeMode (single-source default)
#include "config/render_config.hpp"
#include "core/crystal.hpp"
#include "core/math.hpp"
#include "include/lumice_scene.h"
#include "server/c_api_scene_internal.hpp"
#include "util/color_space.hpp"
#include "util/logger.hpp"

namespace ns = lumice;


// =============== ConfigScratch <-> JSON (internal; see c_api_scene_internal.hpp) ===============
// Serialize a LUMICE_Distribution to its core-compatible JSON wire form. NO_RANDOM emits a
// bare number (center), matching core Distribution::to_json's short-circuit branch
// (math.cpp: `obj = dist.center`); the other 5 types emit {"type","mean","std"} objects. The
// on-disk keys stay "mean"/"std" (core's published config format) even though the C struct
// field names are center/spread.
static nlohmann::json DistributionToJson(const LUMICE_Distribution& d) {
  if (d.type == LUMICE_DIST_NO_RANDOM) {
    return d.center;  // bare number; aligns with core Distribution::to_json for kNoRandom
  }
  nlohmann::json j;
  switch (d.type) {
    case LUMICE_DIST_UNIFORM:
      j["type"] = "uniform";
      break;
    case LUMICE_DIST_GAUSS:
      j["type"] = "gauss";
      break;
    case LUMICE_DIST_ZIGZAG:
      j["type"] = "zigzag";
      break;
    case LUMICE_DIST_LAPLACIAN:
      j["type"] = "laplacian";
      break;
    case LUMICE_DIST_GAUSS_LEGACY:
      j["type"] = "gauss_legacy";
      break;
    default:
      LOG_ERROR("Unknown LUMICE_Distribution.type: {}", d.type);
      j["type"] = "gauss";
      break;
  }
  j["mean"] = d.center;
  j["std"] = d.spread;
  return j;
}

// Encode a P/B/D symmetry bitmask (1=P, 2=B, 4=D) as its canonical string form. Single home
// for the wire encoding shared between LUMICE_FilterParam and LUMICE_ColorPredicate — callers
// each own their own emit-condition policy (FilterParam emits unconditionally; ColorPredicate
// emits only when non-default), the helper itself is oblivious to that decision.
std::string SymmetryBitsToString(int bits) {
  std::string sym;
  if (bits & 1) {
    sym += "P";
  }
  if (bits & 2) {
    sym += "B";
  }
  if (bits & 4) {
    sym += "D";
  }
  return sym;
}

// Emit the arm fields of a Color Predicate (its symmetry key included). Mirrors
// the raypath / entry_exit / direction / crystal switch in JsonToFilter/ConfigToJson but
// WITHOUT id/action/composition — Design 2 color predicates are single-atom and carry no
// filter identity. A predicate whose type is LUMICE_FILTER_TYPE_UNSET intentionally emits NO
// arm fields at all: the resulting ref JSON is just {"layer", "crystal"} (plus optional
// "symmetry" if non-default), which core RaypathColorRef::from_json interprets as match-all
// (whole-crystal color). See lumice_scene.h LUMICE_ColorPredicate for the UNSET-as-match-all
// rationale.
static void ColorPredicateToJson(const LUMICE_ColorPredicate& p, nlohmann::json& j) {
  switch (p.type) {
    case LUMICE_FILTER_TYPE_UNSET:
      // Match-all: emit no arm fields (wire form for NoneFilterParam default).
      break;
    case LUMICE_FILTER_TYPE_NONE:
      j["type"] = "none";
      break;
    case LUMICE_FILTER_TYPE_RAYPATH: {
      j["type"] = "raypath";
      nlohmann::json rp = nlohmann::json::array();
      for (int k = 0; k < p.raypath_count; k++) {
        rp.push_back(p.raypath[k]);
      }
      j["raypath"] = rp;
      break;
    }
    case LUMICE_FILTER_TYPE_ENTRY_EXIT:
      j["type"] = "entry_exit";
      if (p.ee_entry >= 0) {
        j["entry"] = p.ee_entry;
      }
      if (p.ee_exit >= 0) {
        j["exit"] = p.ee_exit;
      }
      if (p.ee_min_len > 1) {
        j["min_len"] = p.ee_min_len;
      }
      if (p.ee_max_len >= 0) {
        j["max_len"] = p.ee_max_len;
      }
      break;
    case LUMICE_FILTER_TYPE_DIRECTION:
      j["type"] = "direction";
      j["az"] = p.dir_az;
      j["el"] = p.dir_el;
      j["radii"] = p.dir_radii;
      break;
    case LUMICE_FILTER_TYPE_CRYSTAL:
      j["type"] = "crystal";
      j["crystal_id"] = p.crystal_id;
      break;
    default:
      // COMPLEX and any out-of-range discriminant is unsupported for color predicates.
      throw std::invalid_argument("LUMICE_ColorPredicate.type is invalid for a color predicate: " +
                                  std::to_string(p.type));
  }
  // Symmetry is a common field regardless of predicate arm (including match-all). Mirrors
  // core RaypathColorRef::to_json — emitted ONLY when non-default (kSymNone omitted), which
  // is DIFFERENT from LUMICE_FilterParam's "always emit (empty when no bits)" convention.
  // Keeps legacy / no-symmetry wire form byte-identical (AC3).
  if (p.symmetry != 0) {
    j["symmetry"] = SymmetryBitsToString(p.symmetry);
  }
}

// The sync_group schema key names come from core (ns::ShapeScalarSyncKeyName), which owns them.
// This layer used to carry its own copy of the {key, slot} table; the core from_json looks the keys
// up by name, so a single character of drift meant the C API emitted a key core never reads and a
// declared sync group silently degenerated back to "all independent" with no warning anywhere.
// The face slots all answer "face_distance", whose value is one 6-element array, so they are
// handled as a block rather than per slot.

// struct -> JSON. Passes sync_group through verbatim: canonicalization (drop inapplicable slots,
// collapse singletons, renumber) and leader normalization belong to core's from_json, which every
// path that actually consumes this JSON runs exactly once. Re-implementing them here would be a
// second authority for the same semantics.
// Writes nothing at all when no scalar is synced, keeping the wire form of every pre-v4.13 config
// byte-identical. Mirrors WriteSyncGroupJson in src/config/crystal_config.cpp.
static void WriteSyncGroupJson(nlohmann::json& shape_j, const int sync_group[LUMICE_SHAPE_SCALAR_COUNT],
                               ns::CrystalKind kind) {
  nlohmann::json sg = nlohmann::json::object();
  for (int slot = 0; slot < LUMICE_SHAPE_SCALAR_FACE_0; slot++) {
    // A null key means the slot does not apply to this type, so an inapplicable declaration never
    // reaches the wire in the first place.
    const char* key = ns::ShapeScalarSyncKeyName(kind, slot);
    if (key != nullptr && sync_group[slot] != 0) {
      sg[key] = sync_group[slot];
    }
  }
  bool any_face = false;
  for (int i = 0; i < 6; i++) {
    any_face = any_face || sync_group[LUMICE_SHAPE_SCALAR_FACE_0 + i] != 0;
  }
  const char* face_key = ns::ShapeScalarSyncKeyName(kind, LUMICE_SHAPE_SCALAR_FACE_0);
  if (any_face && face_key != nullptr) {
    // All six, zeros included — matching how face_distance itself serializes.
    sg[face_key] =
        std::vector<int>(sync_group + LUMICE_SHAPE_SCALAR_FACE_0, sync_group + LUMICE_SHAPE_SCALAR_FACE_0 + 6);
  }
  if (!sg.empty()) {
    shape_j["sync_group"] = sg;
  }
}

// Non-static (declared in server/c_api_scene_internal.hpp) so both ConfigToJson and
// LUMICE_GetCrystalMesh consume the SAME crystal-shape translation table. Returns
// {"type","shape"} only — id/axis are the caller's responsibility (ConfigToJson adds
// them; the mesh-preview path does not want them).
nlohmann::json CrystalShapeToJson(const LUMICE_CrystalParam& cr) {
  nlohmann::json j;
  const auto kind = (cr.type == 0) ? ns::CrystalKind::kPrism : ns::CrystalKind::kPyramid;
  if (cr.type == 0) {
    j["type"] = "prism";
    j["shape"][ns::ShapeScalarSyncKeyName(kind, LUMICE_SHAPE_SCALAR_HEIGHT)] = DistributionToJson(cr.height);
  } else {
    j["type"] = "pyramid";
    j["shape"][ns::ShapeScalarSyncKeyName(kind, LUMICE_SHAPE_SCALAR_PRISM_H)] = DistributionToJson(cr.prism_h);
    j["shape"][ns::ShapeScalarSyncKeyName(kind, LUMICE_SHAPE_SCALAR_UPPER_H)] = DistributionToJson(cr.upper_h);
    j["shape"][ns::ShapeScalarSyncKeyName(kind, LUMICE_SHAPE_SCALAR_LOWER_H)] = DistributionToJson(cr.lower_h);
    j["shape"][ns::ShapeWedgeAngleKeyName(true)] = cr.upper_wedge_angle;
    j["shape"][ns::ShapeWedgeAngleKeyName(false)] = cr.lower_wedge_angle;
  }
  // face_distance: emit all 6 unconditionally (mirrors core crystal_config.cpp::to_json's
  // `j["face_distance"] = p.d_`). The "only when non-default" shortcut no longer fits now that
  // each element is a distribution (a NO_RANDOM 1.0 default vs a randomized 1.0 are different
  // wire forms), so always round-trip every element.
  nlohmann::json fd = nlohmann::json::array();
  for (int fi = 0; fi < 6; fi++) {
    fd.push_back(DistributionToJson(cr.face_distance[fi]));
  }
  j["shape"][ns::ShapeScalarSyncKeyName(kind, LUMICE_SHAPE_SCALAR_FACE_0)] = fd;
  // Only the keys applicable to this crystal type are emitted, so an inapplicable declaration
  // never reaches the wire in the first place (core's canonicalization zeroes such slots anyway,
  // as defense in depth).
  WriteSyncGroupJson(j["shape"], cr.sync_group, kind);
  return j;
}

// Per-item wire encoders — the single source of truth for each subsystem's JSON shape, shared
// by both the batch path (ConfigToJson below) and the incremental Scene path (LUMICE_SceneAdd*).
// Splitting ConfigToJson's formerly-inline loops into these keeps the two paths from drifting;
// the caller owns id assignment (ConfigToJson passes the struct's .id; the Scene passes its own
// sequential id). These throw std::invalid_argument on an invalid item — the batch caller wraps
// the whole ConfigToJson in try/catch, the Scene callers wrap each Add in try/catch.

// Full crystal object: {"type","shape",...} from CrystalShapeToJson plus id + axis distributions.
// Non-static (declared in server/c_api_scene_internal.hpp): the editor bridge's symmetry queries
// translate a wire crystal to core's CrystalConfig through it.
nlohmann::json CrystalToJson(const LUMICE_CrystalParam& cr, int id) {
  nlohmann::json j = CrystalShapeToJson(cr);  // {"type","shape"} single-source translation
  j["id"] = id;
  j["axis"][ns::AxisScalarKeyName(ns::kAxisScalarZenith)] = DistributionToJson(cr.zenith);
  j["axis"][ns::AxisScalarKeyName(ns::kAxisScalarAzimuth)] = DistributionToJson(cr.azimuth);
  j["axis"][ns::AxisScalarKeyName(ns::kAxisScalarRoll)] = DistributionToJson(cr.roll);
  return j;
}

// Encode the type-specific arm fields of a SIMPLE filter (none/raypath/entry_exit/direction/
// crystal) into `j`. Does NOT write id/action/symmetry — those are common fields the caller
// writes around this. Throws for COMPLEX (needs composition context — caller handles it) and for
// UNSET/out-of-range (the zero-init guard). Mirrors core config/filter_config.cpp::to_json.
// SYNC: the per-type field list here mirrors the parse side in JsonToFilter; adding/removing a
// filter type or field requires updating both.
void EncodeSimpleFilterBody(const LUMICE_FilterParam& f, nlohmann::json& j) {
  switch (f.type) {
    case LUMICE_FILTER_TYPE_NONE:
      j["type"] = "none";
      break;
    case LUMICE_FILTER_TYPE_RAYPATH: {
      j["type"] = "raypath";
      nlohmann::json rp = nlohmann::json::array();
      for (int k = 0; k < f.raypath_count; k++) {
        rp.push_back(f.raypath[k]);
      }
      j["raypath"] = rp;
      break;
    }
    case LUMICE_FILTER_TYPE_ENTRY_EXIT:
      j["type"] = "entry_exit";
      if (f.ee_entry >= 0) {
        j["entry"] = f.ee_entry;
      }
      if (f.ee_exit >= 0) {
        j["exit"] = f.ee_exit;
      }
      if (f.ee_min_len > 1) {
        j["min_len"] = f.ee_min_len;
      }
      if (f.ee_max_len >= 0) {
        j["max_len"] = f.ee_max_len;
      }
      break;
    case LUMICE_FILTER_TYPE_DIRECTION:
      j["type"] = "direction";
      j["az"] = f.dir_az;
      j["el"] = f.dir_el;
      j["radii"] = f.dir_radii;
      break;
    case LUMICE_FILTER_TYPE_CRYSTAL:
      j["type"] = "crystal";
      j["crystal_id"] = f.crystal_id;
      break;
    case LUMICE_FILTER_TYPE_COMPLEX:
      // A complex filter cannot be encoded from the LUMICE_FilterParam alone — it needs its
      // composition. ConfigToJson handles COMPLEX before reaching here; LUMICE_SceneAddFilter
      // rejects it (the caller must use LUMICE_SceneAddComplexFilter).
      throw std::invalid_argument("complex filter cannot be encoded as a simple filter: " + std::to_string(f.type));
    default:
      // UNSET (zero-init guard) or an out-of-range discriminant: fail fast rather than silently
      // emitting a wrong/empty filter.
      throw std::invalid_argument("LUMICE_FilterParam.type is unset or invalid: " + std::to_string(f.type));
  }
}

// Encode a complex filter's sum-of-products composition as the wire "composition" array: each
// clause is a bare id (1 term) or an array of ids (multi-term), mirroring core to_json.
nlohmann::json CompositionArrayToJson(const LUMICE_ComplexComposition& comp) {
  nlohmann::json composition = nlohmann::json::array();
  for (int cl = 0; cl < comp.clause_count; cl++) {
    int term_n = 0;
    const int* terms_p = LUMICE_CompositionClauseTerms(&comp, cl, &term_n);
    if (term_n == 1 && terms_p != nullptr) {
      composition.push_back(terms_p[0]);
    } else {
      nlohmann::json terms = nlohmann::json::array();
      for (int t = 0; t < term_n; t++) {
        terms.push_back(terms_p[t]);
      }
      composition.push_back(terms);
    }
  }
  return composition;
}

// Map LUMICE_LENS_TYPE_* to its core enumerator. Explicit switch (not a numeric cast) so a future
// reorder of either enumeration surfaces as a compile/throw rather than a silently aliased
// projection. Throws std::invalid_argument on an unknown value.
static ns::LensParam::LensType MapLensTypeFromCApi(int lens_type) {
  switch (lens_type) {
    case LUMICE_LENS_TYPE_LINEAR:
      return ns::LensParam::kLinear;
    case LUMICE_LENS_TYPE_FISHEYE_EQUAL_AREA:
      return ns::LensParam::kFisheyeEqualArea;
    case LUMICE_LENS_TYPE_FISHEYE_EQUIDISTANT:
      return ns::LensParam::kFisheyeEquidistant;
    case LUMICE_LENS_TYPE_FISHEYE_STEREOGRAPHIC:
      return ns::LensParam::kFisheyeStereographic;
    case LUMICE_LENS_TYPE_DUAL_FISHEYE_EQUAL_AREA:
      return ns::LensParam::kDualFisheyeEqualArea;
    case LUMICE_LENS_TYPE_DUAL_FISHEYE_EQUIDISTANT:
      return ns::LensParam::kDualFisheyeEquidistant;
    case LUMICE_LENS_TYPE_DUAL_FISHEYE_STEREOGRAPHIC:
      return ns::LensParam::kDualFisheyeStereographic;
    case LUMICE_LENS_TYPE_RECTANGULAR:
      return ns::LensParam::kRectangular;
    case LUMICE_LENS_TYPE_FISHEYE_ORTHOGRAPHIC:
      return ns::LensParam::kFisheyeOrthographic;
    case LUMICE_LENS_TYPE_DUAL_FISHEYE_ORTHOGRAPHIC:
      return ns::LensParam::kDualFisheyeOrthographic;
    case LUMICE_LENS_TYPE_GLOBE:
      return ns::LensParam::kGlobe;
    default:
      throw std::invalid_argument("LUMICE_RenderParam.lens_type is invalid: " + std::to_string(lens_type));
  }
}

// Map LUMICE_VISIBLE_* to its core enumerator. Throws std::invalid_argument on an unknown value.
static ns::RenderConfig::VisibleRange MapVisibleFromCApi(int visible) {
  switch (visible) {
    case LUMICE_VISIBLE_UPPER:
      return ns::RenderConfig::kUpper;
    case LUMICE_VISIBLE_LOWER:
      return ns::RenderConfig::kLower;
    case LUMICE_VISIBLE_FULL:
      return ns::RenderConfig::kFull;
    default:
      throw std::invalid_argument("LUMICE_RenderParam.visible is invalid: " + std::to_string(visible));
  }
}

static ns::RenderConfig::EvMode MapEvModeFromCApi(int ev_mode) {
  switch (ev_mode) {
    case LUMICE_EV_MODE_RELATIVE:
      return ns::RenderConfig::kRelative;
    case LUMICE_EV_MODE_ABSOLUTE:
      return ns::RenderConfig::kAbsolute;
    default:
      throw std::invalid_argument("LUMICE_RenderParam.ev_mode is invalid: " + std::to_string(ev_mode));
  }
}

// Fail-loud on an out-of-range value, like the three mappers above and unlike core's
// RenderConfig::Tone::from_json, which warns and falls back. Not an inconsistency: that one reads a
// STRING out of a document a human may have hand-written, where "screen" is a meaningful recovery;
// this one reads an INT a caller passed in code, where an out-of-range value is a programming error
// with no sensible recovery to guess at.
static ns::RenderConfig::Tone MapToneFromCApi(int tone) {
  switch (tone) {
    case LUMICE_TONE_SCREEN:
      return ns::RenderConfig::kScreen;
    case LUMICE_TONE_PRINT:
      return ns::RenderConfig::kPrint;
    default:
      throw std::invalid_argument("LUMICE_RenderParam.tone is invalid: " + std::to_string(tone));
  }
}

// Same fail-loud contract as MapToneFromCApi above, for the same reason.
static ns::RenderConfig::DisplayMode MapDisplayModeFromCApi(int display_mode) {
  switch (display_mode) {
    case LUMICE_DISPLAY_MODE_NORMAL:
      return ns::RenderConfig::kDisplayNormal;
    case LUMICE_DISPLAY_MODE_CHANNEL_BR:
      return ns::RenderConfig::kDisplayChannelBr;
    default:
      throw std::invalid_argument("LUMICE_RenderParam.display_mode is invalid: " + std::to_string(display_mode));
  }
}

// Copy `count` C grid lines into the core vector form so nlohmann's to_json(GridLineParam) owns
// the wire shape (single source for the per-field optional/default logic).
static std::vector<ns::GridLineParam> GridLinesToCore(const LUMICE_GridLine* lines, int count) {
  std::vector<ns::GridLineParam> out;
  out.reserve(static_cast<size_t>(count));
  for (int i = 0; i < count; i++) {
    ns::GridLineParam g;
    g.value_ = lines[i].value;
    g.width_ = lines[i].width;
    g.opacity_ = lines[i].opacity;
    g.color_[0] = lines[i].color[0];
    g.color_[1] = lines[i].color[1];
    g.color_[2] = lines[i].color[2];
    out.push_back(g);
  }
  return out;
}

// Copy `count` C marker entries into the core vector form, so nlohmann's to_json(MarkerStyleParam)
// owns the wire shape — the id SPELLING in particular, which then has one table in the tree.
// Twin of GridLinesToCore above.
static std::vector<ns::MarkerStyleParam> MarkerStylesToCore(const LUMICE_MarkerStyle* markers, int count) {
  std::vector<ns::MarkerStyleParam> out;
  out.reserve(static_cast<size_t>(count));
  for (int i = 0; i < count; i++) {
    ns::MarkerStyleParam m;
    m.id_ = static_cast<ns::MarkerRefId>(markers[i].id);
    m.enabled_ = markers[i].enabled != 0;
    m.color_[0] = markers[i].color[0];
    m.color_[1] = markers[i].color[1];
    m.color_[2] = markers[i].color[2];
    out.push_back(m);
  }
  return out;
}

// Full renderer object. Every field of the C struct is written verbatim; the numeric encodings
// (lens f<->fov trigonometry, grid-line defaults) come from core's to_json overloads rather than
// being re-derived here, so there is exactly one implementation of each formula.
// Throws std::invalid_argument on an invalid lens_type / visible / grid count; callers wrap.
// The grid-count check lives here (not only in the entry points) because this is the single place
// that dereferences angular_dist[]/view_dist[]/elevation_grid[]/longitude_grid[] — ConfigToJson
// accepts a caller-assembled struct with no bounds pass of its own.
nlohmann::json RendererToJson(const LUMICE_RenderParam& r, int id) {
  if (r.angular_dist_count < 0 || r.angular_dist_count > LUMICE_MAX_CONFIG_GRID_LINES || r.view_dist_count < 0 ||
      r.view_dist_count > LUMICE_MAX_CONFIG_GRID_LINES || r.elevation_grid_count < 0 ||
      r.elevation_grid_count > LUMICE_MAX_CONFIG_GRID_LINES || r.longitude_grid_count < 0 ||
      r.longitude_grid_count > LUMICE_MAX_CONFIG_GRID_LINES) {
    throw std::invalid_argument(
        "LUMICE_RenderParam grid count out of range: angular_dist=" + std::to_string(r.angular_dist_count) +
        ", view_dist=" + std::to_string(r.view_dist_count) + ", elevation=" + std::to_string(r.elevation_grid_count) +
        ", longitude=" + std::to_string(r.longitude_grid_count));
  }
  // Same check for the marker list, and for the same reason the grid counts are checked here: this
  // is the single place that dereferences markers[], and ConfigToJson accepts a caller-assembled
  // struct with no bounds pass of its own. The per-entry id range is checked too — an out-of-range
  // id would index MarkerStyleParam's name table, and core's to_json clamps rather than reports.
  if (r.markers_count < 0 || r.markers_count > LUMICE_MAX_CONFIG_MARKERS) {
    throw std::invalid_argument("LUMICE_RenderParam markers_count out of range: " + std::to_string(r.markers_count));
  }
  for (int i = 0; i < r.markers_count; i++) {
    if (!ns::capi::IsValidMarkerId(r.markers[i].id)) {
      throw std::invalid_argument("LUMICE_RenderParam markers[" + std::to_string(i) +
                                  "].id out of range: " + std::to_string(r.markers[i].id));
    }
  }
  {
    const std::vector<ns::MarkerStyleParam> core_markers = MarkerStylesToCore(r.markers, r.markers_count);
    ns::MarkerRefId dup{};
    if (ns::HasDuplicateMarkerId(core_markers, &dup)) {
      throw std::invalid_argument("LUMICE_RenderParam markers[] names the same id twice: " +
                                  std::to_string(static_cast<int>(dup)));
    }
  }
  nlohmann::json jr;
  jr["id"] = id;
  jr["lens"] = ns::LensParam{ MapLensTypeFromCApi(r.lens_type), r.lens_fov };
  jr["lens_shift"] = { r.lens_shift[0], r.lens_shift[1] };
  jr["resolution"] = { r.resolution_w, r.resolution_h };
  jr["view"] = ns::ViewParam{ r.view_azimuth, r.view_elevation, r.view_roll };
  jr["visible"] = MapVisibleFromCApi(r.visible);
  // Its own top-level key, not a "visible" enumerator: the two clips are orthogonal and AND
  // together. Twin of core's to_json in render_config.cpp.
  jr["front"] = r.front != 0;
  // Back to sRGB on the way out — the struct field is linear, the JSON key is not.
  jr["background"] = { ns::LinearToSrgb(r.background[0]), ns::LinearToSrgb(r.background[1]),
                       ns::LinearToSrgb(r.background[2]) };
  // Same linear-struct / sRGB-key split as `background` above, and the same conversion.
  jr["paper"] = { ns::LinearToSrgb(r.paper[0]), ns::LinearToSrgb(r.paper[1]), ns::LinearToSrgb(r.paper[2]) };
  jr["ray_color"] = { r.ray_color[0], r.ray_color[1], r.ray_color[2] };
  jr["intensity_factor"] = r.intensity_factor;
  jr["overlap"] = r.overlap;
  jr["globe_back_fade"] = r.globe_back_fade;
  jr["ev_mode"] = MapEvModeFromCApi(r.ev_mode);
  // Assigned as a core Tone, not as a string literal: core's own to_json then owns the two
  // spellings, exactly as MapEvModeFromCApi's result does for "relative" / "absolute".
  jr["tone"] = MapToneFromCApi(r.tone);
  jr["display_mode"] = MapDisplayModeFromCApi(r.display_mode);
  jr["grid"]["angular_dist"] = GridLinesToCore(r.angular_dist, r.angular_dist_count);
  jr["grid"]["view_dist"] = GridLinesToCore(r.view_dist, r.view_dist_count);
  jr["grid"]["elevation"] = GridLinesToCore(r.elevation_grid, r.elevation_grid_count);
  jr["grid"]["longitude"] = GridLinesToCore(r.longitude_grid, r.longitude_grid_count);
  jr["grid"]["horizon"] = r.horizon != 0;
  // The other three families' line switches, beside the lists they gate. Same key names core's own
  // to_json writes, for the same reason the label block below states.
  jr["grid"]["elevation_line"] = r.elevation_line != 0;
  jr["grid"]["longitude_line"] = r.longitude_line != 0;
  jr["grid"]["angular_dist_line"] = r.angular_dist_line != 0;
  jr["grid"]["view_dist_line"] = r.view_dist_line != 0;
  // The three text-label switches, next to the lines they annotate. Same key names core's own
  // to_json writes (render_config.cpp), which is what test_json_parser_parity.cpp compares.
  jr["grid"]["horizon_label"] = r.horizon_label != 0;
  jr["grid"]["label"] = r.grid_label != 0;
  jr["grid"]["angular_dist_label"] = r.angular_dist_label != 0;
  jr["grid"]["view_dist_label"] = r.view_dist_label != 0;
  // Through core's own to_json, like every other value here: the key names and the sRGB convention
  // then have exactly one spelling in the tree.
  ns::ZenithNadirParam zn;
  zn.enabled_ = r.zenith_nadir != 0;
  zn.radius_px_ = r.zenith_nadir_radius_px;
  zn.opacity_ = r.zenith_nadir_opacity;
  zn.color_[0] = r.zenith_nadir_color[0];
  zn.color_[1] = r.zenith_nadir_color[1];
  zn.color_[2] = r.zenith_nadir_color[2];
  jr["grid"]["zenith_nadir"] = zn;
  // The marker family, through core's own to_json for the same reason everything else here is: the
  // key names and the id spellings then have exactly one implementation, which is what
  // test_json_parser_parity.cpp compares this decoder against.
  jr["grid"]["markers"] = MarkerStylesToCore(r.markers, r.markers_count);
  jr["grid"]["markers_opacity"] = r.markers_opacity;
  jr["grid"]["markers_radius_px"] = r.markers_radius_px;
  return jr;
}

// One scattering layer: {"prob", "entries":[{"crystal","proportion"[,"filter"]}]}.
nlohmann::json ScatterLayerToJson(const LUMICE_ScatterLayer& layer) {
  nlohmann::json jl;
  jl["prob"] = layer.probability;
  jl["entries"] = nlohmann::json::array();
  for (int k = 0; k < layer.entry_count; k++) {
    const auto& e = layer.entries[k];
    nlohmann::json je;
    je["crystal"] = e.crystal_id >= 0 ? e.crystal_id : 1;
    je["proportion"] = e.proportion;
    if (e.filter_id >= 0) {
      je["filter"] = e.filter_id;
    }
    jl["entries"].push_back(je);
  }
  return jl;
}

// One raypath color class. combine/visible/solo emitted only when non-default (mirrors core
// to_json). Throws on an invalid combine or an invalid match predicate.
nlohmann::json ColorClassToJson(const LUMICE_ColorClass& cls) {
  nlohmann::json jc;
  jc["color"] = { cls.color[0], cls.color[1], cls.color[2] };
  if (cls.combine == LUMICE_COLOR_COMBINE_ALL) {
    jc["combine"] = "all";
  } else if (cls.combine != LUMICE_COLOR_COMBINE_ANY) {
    throw std::invalid_argument("LUMICE_ColorClass.combine is invalid: " + std::to_string(cls.combine));
  }
  if (!cls.visible) {
    jc["visible"] = false;
  }
  if (cls.solo) {
    jc["solo"] = true;
  }
  nlohmann::json match = nlohmann::json::array();
  for (int k = 0; k < cls.match_count; k++) {
    const auto& ref = cls.match[k];
    nlohmann::json jr;
    jr["layer"] = ref.layer;
    jr["crystal"] = ref.crystal;
    ColorPredicateToJson(ref.predicate, jr);
    match.push_back(jr);
  }
  jc["match"] = match;
  return jc;
}

// Map LUMICE_COLOR_MODE_* to its wire string. Throws std::invalid_argument on an invalid mode.
const char* ColorModeToString(int mode) {
  switch (mode) {
    case LUMICE_COLOR_MODE_DOMINANT:
      return "dominant";
    case LUMICE_COLOR_MODE_ADDITIVE:
      return "additive";
    case LUMICE_COLOR_MODE_PAINTER:
      return "painter";
    default:
      throw std::invalid_argument("raypath_color_mode is invalid: " + std::to_string(mode));
  }
}

// Map LUMICE_RAY_ALLOCATION_* to its wire string. Throws std::invalid_argument on an invalid mode.
const char* RayAllocationModeToString(int mode) {
  switch (mode) {
    case LUMICE_RAY_ALLOCATION_PROPORTIONAL:
      return "proportional";
    case LUMICE_RAY_ALLOCATION_ADAPTIVE:
      return "adaptive";
    default:
      throw std::invalid_argument("ray_allocation mode is invalid: " + std::to_string(mode));
  }
}

// Non-static (declared in server/c_api_scene_internal.hpp) so unit tests can assert the
// emitted filter JSON shape field by field. See that header for rationale.
nlohmann::json ConfigToJson(const ConfigScratch& c) {
  using json = nlohmann::json;
  json root;

  // Crystals
  root["crystal"] = json::array();
  for (int i = 0; i < c.crystal_count; i++) {
    root["crystal"].push_back(CrystalToJson(c.crystals[i], c.crystals[i].id));
  }

  // Filters
  root["filter"] = json::array();
  for (int i = 0; i < c.filter_count; i++) {
    const auto& f = c.filters[i];
    json j;
    j["id"] = f.id;
    j["action"] = f.action == 0 ? "filter_in" : "filter_out";

    // COMPLEX resolves its composition from this config's compositions[] pool (the Scene path
    // gets it directly via LUMICE_SceneAddComplexFilter). Simple arms defer to the shared
    // encoder. The zero-init guard / invalid-type throw lives in EncodeSimpleFilterBody; every
    // caller of this function wraps it and maps the throw to LUMICE_ERR_INVALID_CONFIG.
    if (f.type == LUMICE_FILTER_TYPE_COMPLEX) {
      j["type"] = "complex";
      if (f.composition_index < 0 || f.composition_index >= c.composition_count) {
        throw std::invalid_argument("LUMICE_FilterParam.composition_index out of range: " +
                                    std::to_string(f.composition_index));
      }
      j["composition"] = CompositionArrayToJson(c.compositions[f.composition_index]);
    } else {
      EncodeSimpleFilterBody(f, j);
    }

    // symmetry is a common FilterConfig field (see core filter_config.cpp::to_json, which
    // emits it for every type before the per-type fields), so it stays outside the switch
    // and applies to all arms — do not fold it into the raypath case. Emitted
    // UNCONDITIONALLY (empty string when no bits set) to stay byte-isomorphic with core,
    // whose to_json always writes j["symmetry"] = sym.
    j["symmetry"] = SymmetryBitsToString(f.symmetry);
    root["filter"].push_back(j);
  }

  // Scene
  json scene;
  scene["light_source"]["type"] = "sun";
  scene["light_source"]["altitude"] = c.sun_altitude;
  scene["light_source"]["azimuth"] = c.sun_azimuth;
  scene["light_source"]["diameter"] = c.sun_diameter;
  if (c.spectrum_count > 0) {
    // Discrete custom spectrum. Shape matches core light_config.cpp::SpectrumToJson.
    json spectrum = json::array();
    for (int i = 0; i < c.spectrum_count; i++) {
      json e;
      e["wavelength"] = c.spectrum_entries[i].wavelength;
      e["weight"] = c.spectrum_entries[i].weight;
      spectrum.push_back(e);
    }
    scene["light_source"]["spectrum"] = spectrum;
  } else {
    scene["light_source"]["spectrum"] = c.spectrum ? c.spectrum : "D65";
  }

  if (c.infinite) {
    scene["ray_num"] = "infinite";
  } else {
    scene["ray_num"] = c.ray_num;
  }
  scene["max_hits"] = c.max_hits;
  // geom_clock: emit only when set (mirrors core proj_config.cpp::to_json's `if (geom_clock_ != 0)`).
  if (c.geom_clock != 0) {
    scene["geom_clock"] = c.geom_clock;
  }
  // ray_allocation: emit only when the document carried it, verbatim (see ConfigScratch).
  if (c.ray_allocation[0] != '\0') {
    scene["ray_allocation"] = c.ray_allocation;
  }

  scene["scattering"] = json::array();
  for (int i = 0; i < c.scatter_count; i++) {
    scene["scattering"].push_back(ScatterLayerToJson(c.scattering[i]));
  }
  root["scene"] = scene;

  // Renderers — Core always produces dual equal-area fisheye texture (full-globe, equal-area).
  // GUI shader reprojects from this format to the user's display projection.
  root["render"] = json::array();
  for (int i = 0; i < c.renderer_count; i++) {
    root["render"].push_back(RendererToJson(c.renderers[i], c.renderers[i].id));
  }

  // Raypath color classes. Only emit when non-empty so the mono/no-color case
  // matches the pre-v4.7 JSON shape byte-for-byte (AC4). The wire form always uses the object
  // shape `{"mode": ..., "classes": [...]}` — core RaypathColorConfig::from_json accepts both
  // the bare-array (dominant-only) and object shapes, so emitting the object form does not
  // constrain the reader; it simplifies the emit path (no mode string comparison branch).
  if (c.raypath_color_count > 0) {
    nlohmann::json classes = nlohmann::json::array();
    for (int i = 0; i < c.raypath_color_count; i++) {
      classes.push_back(ColorClassToJson(c.raypath_color[i]));
    }
    nlohmann::json rc;
    rc["mode"] = ColorModeToString(c.raypath_color_mode);
    rc["classes"] = classes;
    root["raypath_color"] = rc;
  }

  return root;
}
