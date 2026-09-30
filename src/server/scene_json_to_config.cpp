// The decode half of the ConfigScratch JSON<->config codec: core config JSON -> ConfigScratch.
// ParseConfigString / JsonToConfig are its entry points (declared in c_api_scene_internal.hpp,
// together with JsonToRenderer, which the scene bridge's renderer getter also calls). The encode
// half is scene_config_to_json.cpp.
// Registered at the scene layer in cmake/lumice_layers.cmake, so it may include nothing above it.

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <exception>
#include <map>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "config/crystal_config.hpp"
#include "config/raypath_color_config.hpp"  // ns::kDefaultCompositeMode (single-source default)
#include "config/render_config.hpp"
#include "core/crystal.hpp"
#include "core/math.hpp"
#include "core/miller_wedge.hpp"
#include "include/lumice_scene.h"
#include "server/c_api_scene_internal.hpp"
#include "util/color_space.hpp"
#include "util/logger.hpp"

namespace ns = lumice;


// Decode a P/B/D symmetry string back into its bitmask. Unknown characters are ignored (same
// tolerance as the previous inline loops); paired with SymmetryBitsToString above.
static int SymmetryStringToBits(const std::string& s) {
  int bits = 0;
  for (char ch : s) {
    if (ch == 'P') {
      bits |= 1;
    } else if (ch == 'B') {
      bits |= 2;
    } else if (ch == 'D') {
      bits |= 4;
    }
  }
  return bits;
}

// JSON -> struct. Absent "sync_group" leaves every slot 0 (all independent), which is why an
// older config file needs no edit. Mirrors ReadSyncGroupJson in src/config/crystal_config.cpp.
static void ReadSyncGroupJson(const nlohmann::json& shape_j, int sync_group[LUMICE_SHAPE_SCALAR_COUNT],
                              ns::CrystalKind kind) {
  for (int i = 0; i < LUMICE_SHAPE_SCALAR_COUNT; i++) {
    sync_group[i] = 0;
  }
  if (!shape_j.contains("sync_group")) {
    return;
  }
  const auto& sg = shape_j.at("sync_group");
  for (int slot = 0; slot < LUMICE_SHAPE_SCALAR_FACE_0; slot++) {
    const char* key = ns::ShapeScalarSyncKeyName(kind, slot);
    if (key != nullptr && sg.contains(key)) {
      sync_group[slot] = sg.at(key).get<int>();
    }
  }
  const char* face_key = ns::ShapeScalarSyncKeyName(kind, LUMICE_SHAPE_SCALAR_FACE_0);
  if (face_key != nullptr && sg.contains(face_key)) {
    size_t i = 0;
    for (const auto& elem : sg.at(face_key)) {
      if (i >= 6) {
        break;
      }
      sync_group[LUMICE_SHAPE_SCALAR_FACE_0 + i] = elem.get<int>();
      i++;
    }
  }
}

// Inverse of MapLensTypeFromCApi. Total over the core enumeration (no default arm) so adding a
// projection to core breaks the build here instead of decoding to a wrong C API constant.
static int MapLensTypeToCApi(ns::LensParam::LensType type) {
  switch (type) {
    case ns::LensParam::kLinear:
      return LUMICE_LENS_TYPE_LINEAR;
    case ns::LensParam::kFisheyeEqualArea:
      return LUMICE_LENS_TYPE_FISHEYE_EQUAL_AREA;
    case ns::LensParam::kFisheyeEquidistant:
      return LUMICE_LENS_TYPE_FISHEYE_EQUIDISTANT;
    case ns::LensParam::kFisheyeStereographic:
      return LUMICE_LENS_TYPE_FISHEYE_STEREOGRAPHIC;
    case ns::LensParam::kDualFisheyeEqualArea:
      return LUMICE_LENS_TYPE_DUAL_FISHEYE_EQUAL_AREA;
    case ns::LensParam::kDualFisheyeEquidistant:
      return LUMICE_LENS_TYPE_DUAL_FISHEYE_EQUIDISTANT;
    case ns::LensParam::kDualFisheyeStereographic:
      return LUMICE_LENS_TYPE_DUAL_FISHEYE_STEREOGRAPHIC;
    case ns::LensParam::kRectangular:
      return LUMICE_LENS_TYPE_RECTANGULAR;
    case ns::LensParam::kFisheyeOrthographic:
      return LUMICE_LENS_TYPE_FISHEYE_ORTHOGRAPHIC;
    case ns::LensParam::kDualFisheyeOrthographic:
      return LUMICE_LENS_TYPE_DUAL_FISHEYE_ORTHOGRAPHIC;
    case ns::LensParam::kGlobe:
      return LUMICE_LENS_TYPE_GLOBE;
  }
  throw std::invalid_argument("unmapped core LensType: " + std::to_string(static_cast<int>(type)));
}


// =============== Configuration Parsing (JSON -> ConfigScratch) ===============
// Symmetric inverse of ConfigToJson. Decomposed into per-section helpers.

// Parse one distribution IN PLACE. `*out` must already hold the default this slot carries in
// core, because core's Distribution::from_json overwrites ONLY the keys present in the JSON and
// leaves the rest of the destination untouched — "type without mean" keeps the destination's
// mean, and a bare number keeps its spread. Every call site below therefore pre-fills `*out` with
// the matching core-side default before calling in, rather than relying on the zeroed struct.
static LUMICE_ErrorCode JsonToDistribution(const nlohmann::json& j, LUMICE_Distribution* out) {
  // Bare number => NO_RANDOM (deterministic value). Mirrors core Distribution::from_json's
  // is_number() branch, which sets type + center and does NOT touch spread. This is the common
  // case for shape fields with randomization off, and the whole reason no_random was previously a
  // translation gap: axis JSON always carried a type object so the missing no_random case never
  // fired until shape scalars became distributions.
  if (j.is_number()) {
    out->type = LUMICE_DIST_NO_RANDOM;
    out->center = j.get<float>();
    return LUMICE_OK;
  }
  if (!j.is_object()) {
    return LUMICE_ERR_INVALID_VALUE;
  }
  // "type" is required, mirroring core's Distribution::from_json. This must be rejected HERE and
  // not left to core: the CLI reaches core only through this parser, whose output is re-serialized
  // by ConfigToJson — and that writer emits `type` unconditionally from the struct, so the missing
  // key would already have been filled in before core ever saw it. A check living only in core
  // would leave core's own test green while the CLI silently accepted the document.
  //
  // Reporting is by return code alone, as everywhere else in this function; the three axis call
  // sites in JsonToCrystal add the LOG_ERROR that carries the migration guidance, because only
  // they know which crystal and which slot. This one cannot: the shape scalars parse through here
  // too, and it has no way to tell them apart.
  if (!j.contains("type")) {
    return LUMICE_ERR_MISSING_FIELD;
  }
  auto type_str = j.at("type").get<std::string>();
  // Defensive: tolerate an explicit {"type":"no_random", ...} object on the READ side even though
  // the WRITE side (DistributionToJson) only ever produces a bare number for NO_RANDOM.
  if (type_str == "no_random") {
    out->type = LUMICE_DIST_NO_RANDOM;
    if (j.contains("mean")) {
      out->center = j.at("mean").get<float>();
    }
    return LUMICE_OK;
  }
  if (type_str == "uniform") {
    out->type = LUMICE_DIST_UNIFORM;
  } else if (type_str == "gauss") {
    out->type = LUMICE_DIST_GAUSS;
  } else if (type_str == "zigzag") {
    out->type = LUMICE_DIST_ZIGZAG;
  } else if (type_str == "laplacian") {
    out->type = LUMICE_DIST_LAPLACIAN;
  } else if (type_str == "gauss_legacy") {
    out->type = LUMICE_DIST_GAUSS_LEGACY;
  } else {
    // Core maps an unrecognized type string to the FIRST entry of its
    // NLOHMANN_JSON_SERIALIZE_ENUM table (a silent misread), which is not a tolerance worth
    // mirroring; rejecting is the deliberate exception to "align with core".
    return LUMICE_ERR_INVALID_VALUE;
  }
  if (j.contains("mean")) {
    out->center = j.at("mean").get<float>();
  }
  if (j.contains("std")) {
    out->spread = j.at("std").get<float>();
  }
  return LUMICE_OK;
}

static const char* MapSpectrumString(const std::string& s) {
  static const std::map<std::string, const char*> kSpectrumMap = {
    { "D65", "D65" }, { "D55", "D55" }, { "D50", "D50" }, { "D75", "D75" }, { "A", "A" }, { "E", "E" },
  };
  auto it = kSpectrumMap.find(s);
  return it != kSpectrumMap.end() ? it->second : nullptr;
}

static LUMICE_ErrorCode JsonToCrystal(const nlohmann::json& cj, LUMICE_CrystalParam* cr) {
  if (!cj.contains("id") || !cj.contains("type")) {
    return LUMICE_ERR_MISSING_FIELD;
  }
  cr->id = cj.at("id").get<int>();

  // Type is dispatched BEFORE "shape" is required: core only reaches j.at("shape") inside a
  // recognized type branch, so an unknown type is an invalid VALUE, not a missing field.
  auto type_str = cj.at("type").get<std::string>();
  if (type_str != "prism" && type_str != "pyramid") {
    return LUMICE_ERR_INVALID_VALUE;
  }
  if (!cj.contains("shape")) {
    return LUMICE_ERR_MISSING_FIELD;  // core: j.at("shape")
  }
  // One derivation for the whole function: the check above already narrowed type_str to the two
  // known spellings, so every key query below — per-type and type-independent alike — asks core's
  // one table with this same kind.
  const auto kind = (type_str == "prism") ? ns::CrystalKind::kPrism : ns::CrystalKind::kPyramid;
  if (type_str == "prism") {
    cr->type = 0;
    // height is OPTIONAL: core PrismCrystalParam::h_ defaults to a deterministic 1.0 and
    // from_json only overwrites it when the key is present.
    cr->height = LUMICE_Distribution{ LUMICE_DIST_NO_RANDOM, 1.0f, 0.0f };
    const char* height_key = ns::ShapeScalarSyncKeyName(kind, LUMICE_SHAPE_SCALAR_HEIGHT);
    if (cj.at("shape").contains(height_key)) {
      if (auto err = JsonToDistribution(cj.at("shape").at(height_key), &cr->height); err != LUMICE_OK) {
        return err;
      }
    }
  } else if (type_str == "pyramid") {
    cr->type = 1;
    const auto& shape = cj.at("shape");
    // prism_h is required (core: j.at(prism_h)); the two pyramidal heights are optional and
    // default to a deterministic 0.0 (PyramidCrystalParam::h_pyr_u_ / h_pyr_l_).
    const char* prism_h_key = ns::ShapeScalarSyncKeyName(kind, LUMICE_SHAPE_SCALAR_PRISM_H);
    if (!shape.contains(prism_h_key)) {
      return LUMICE_ERR_MISSING_FIELD;
    }
    if (auto err = JsonToDistribution(shape.at(prism_h_key), &cr->prism_h); err != LUMICE_OK) {
      return err;
    }
    cr->upper_h = LUMICE_Distribution{ LUMICE_DIST_NO_RANDOM, 0.0f, 0.0f };
    cr->lower_h = LUMICE_Distribution{ LUMICE_DIST_NO_RANDOM, 0.0f, 0.0f };
    const char* upper_h_key = ns::ShapeScalarSyncKeyName(kind, LUMICE_SHAPE_SCALAR_UPPER_H);
    if (shape.contains(upper_h_key)) {
      if (auto err = JsonToDistribution(shape.at(upper_h_key), &cr->upper_h); err != LUMICE_OK) {
        return err;
      }
    }
    const char* lower_h_key = ns::ShapeScalarSyncKeyName(kind, LUMICE_SHAPE_SCALAR_LOWER_H);
    if (shape.contains(lower_h_key)) {
      if (auto err = JsonToDistribution(shape.at(lower_h_key), &cr->lower_h); err != LUMICE_OK) {
        return err;
      }
    }
    // Wedge angles default to 28° (PyramidCrystalParam::wedge_angle_u_ / _l_) when neither the
    // explicit angle nor the Miller indices are given — the zeroed struct would mean 0°.
    cr->upper_wedge_angle = 28.0f;
    cr->lower_wedge_angle = 28.0f;
    // Wedge angle: prefer the explicit angle, fallback to the Miller-index conversion
    for (bool upper : { true, false }) {
      float& wedge_angle = upper ? cr->upper_wedge_angle : cr->lower_wedge_angle;
      const char* angle_key = ns::ShapeWedgeAngleKeyName(upper);
      const char* indices_key = ns::ShapeIndicesKeyName(upper);
      if (shape.contains(angle_key) && shape.at(angle_key).is_number()) {
        wedge_angle = shape.at(angle_key).get<float>();
      } else if (shape.contains(indices_key) && shape.at(indices_key).is_array()) {
        // Any array enters here, not just a three-element one — see the twin in
        // config/crystal_config.cpp's from_json(PyramidCrystalParam): a wrong length is a verdict
        // the owner makes, not a reason to leave the branch and let the default pass for a value.
        const auto& idx = shape.at(indices_key);
        int hkl[3]{ 0, 0, 0 };
        bool all_integers = true;
        for (size_t i = 0; i < idx.size() && i < 3; i++) {
          if (!idx[i].is_number_integer()) {
            all_integers = false;
            break;
          }
          hkl[i] = idx[i].get<int>();
        }
        ns::MillerConversionResult r;
        r.state = ns::MillerConversionState::kInvalid;
        if (all_integers) {
          r = ns::ConvertMillerIndexToWedgeAngle(hkl[0], hkl[1], hkl[2], static_cast<int>(idx.size()));
        }
        if (r.state == ns::MillerConversionState::kValid || r.state == ns::MillerConversionState::kNoCone) {
          wedge_angle = r.wedge_angle_deg;
        } else {
          LOG_WARNING("Crystal shape \"{}\": {} is not a usable wedge angle ({}, offending index {}); keeping {:.2f}.",
                      indices_key, idx.dump(), ns::MillerConversionStateName(r.state), r.invalid_index, wedge_angle);
        }
      }
    }
  } else {
    return LUMICE_ERR_INVALID_VALUE;
  }

  // face_distance: every element defaults to a deterministic 1.0 (regular hexagon). A shorter
  // array leaves the remaining faces at that default and a longer one is truncated at 6 — core
  // fills element-by-element and breaks at index 6 rather than demanding exactly six entries.
  for (int k = 0; k < 6; k++) {
    cr->face_distance[k] = LUMICE_Distribution{ LUMICE_DIST_NO_RANDOM, 1.0f, 0.0f };
  }
  const char* fd_key = ns::ShapeScalarSyncKeyName(kind, LUMICE_SHAPE_SCALAR_FACE_0);
  if (cj.at("shape").contains(fd_key)) {
    const auto& fd = cj.at("shape").at(fd_key);
    if (!fd.is_array()) {
      return LUMICE_ERR_INVALID_VALUE;
    }
    const int n = std::min(6, static_cast<int>(fd.size()));
    for (int k = 0; k < n; k++) {
      if (auto err = JsonToDistribution(fd[k], &cr->face_distance[k]); err != LUMICE_OK) {
        return err;
      }
    }
  }

  // Sync groups: read verbatim, only the keys this crystal type owns. Canonicalization and
  // leader normalization stay in core's from_json (see WriteSyncGroupJson's note); this struct
  // holds what the caller / config file declared.
  ReadSyncGroupJson(cj.at("shape"), cr->sync_group, kind);

  // Axis distributions. The two cases are deliberately NOT symmetric, mirroring core:
  //   - `axis` absent      -> AxisDistribution's default constructor: zenith/azimuth/roll all
  //                           deterministic 0 (its latitude 90 IS zenith 0).
  //   - `axis` present     -> `zenith` is required; `azimuth` / `roll` default to a full 360°
  //                           uniform sweep, NOT to 0.
  // Collapsing them into one "all three optional, default 0" rule silently changes the sampled
  // orientation for a partial `axis` object.
  cr->zenith = LUMICE_Distribution{ LUMICE_DIST_NO_RANDOM, 0.0f, 0.0f };
  cr->azimuth = LUMICE_Distribution{ LUMICE_DIST_NO_RANDOM, 0.0f, 0.0f };
  cr->roll = LUMICE_Distribution{ LUMICE_DIST_NO_RANDOM, 0.0f, 0.0f };
  if (cj.contains("axis")) {
    const auto& axis = cj.at("axis");
    // The three rejections below log, unlike the rest of this function: each narrows a published
    // contract, and a hand-written config outside this repo cannot be enumerated, so the message
    // IS the migration guidance its author gets. It names the crystal, the slot, and both legal
    // ways to write it. The "how to write it" half comes from core (FormatAxisSlotHint) rather
    // than being spelled again here — core's parser rejects the same documents, and two copies of
    // that guidance would drift with nothing comparing them.
    const char* zenith_key = ns::AxisScalarKeyName(ns::kAxisScalarZenith);
    if (!axis.contains(zenith_key)) {
      LOG_ERROR(
          "JsonToCrystal: crystal[id={}].axis is present but has no \"{}\", which is required "
          "whenever `axis` is written at all (omit `axis` entirely to get the default orientation "
          "instead). {}",
          cr->id, zenith_key, ns::FormatAxisSlotHint(zenith_key));
      return LUMICE_ERR_MISSING_FIELD;
    }
    // Core reads the zenith key into its latitude slot and then flips it (latitude = 90 - zenith),
    // unconditionally — so a zenith object that omits `mean` inherits the latitude default 90,
    // which is zenith 90 on this side of the flip (NOT zenith 0).
    // Turns a bare LUMICE_ERR_MISSING_FIELD from JsonToDistribution — which is only ever the
    // absent "type" — into the same guidance core throws, with this crystal and this slot named.
    auto report_slot = [cr](const char* slot_key, LUMICE_ErrorCode err) {
      if (err == LUMICE_ERR_MISSING_FIELD) {
        LOG_ERROR("JsonToCrystal: crystal[id={}].axis.{} is a distribution object with no \"type\". {}", cr->id,
                  slot_key, ns::FormatAxisSlotHint(slot_key));
      }
    };
    cr->zenith = LUMICE_Distribution{ LUMICE_DIST_NO_RANDOM, 90.0f, 0.0f };
    if (auto err = JsonToDistribution(axis.at(zenith_key), &cr->zenith); err != LUMICE_OK) {
      report_slot(zenith_key, err);
      return err;
    }
    cr->azimuth = LUMICE_Distribution{ LUMICE_DIST_UNIFORM, 0.0f, 360.0f };
    cr->roll = LUMICE_Distribution{ LUMICE_DIST_UNIFORM, 0.0f, 360.0f };
    const char* azimuth_key = ns::AxisScalarKeyName(ns::kAxisScalarAzimuth);
    if (axis.contains(azimuth_key)) {
      if (auto err = JsonToDistribution(axis.at(azimuth_key), &cr->azimuth); err != LUMICE_OK) {
        report_slot(azimuth_key, err);
        return err;
      }
    }
    const char* roll_key = ns::AxisScalarKeyName(ns::kAxisScalarRoll);
    if (axis.contains(roll_key)) {
      if (auto err = JsonToDistribution(axis.at(roll_key), &cr->roll); err != LUMICE_OK) {
        report_slot(roll_key, err);
        return err;
      }
    }
  }
  return LUMICE_OK;
}

static LUMICE_ErrorCode JsonToFilter(const nlohmann::json& fj, LUMICE_FilterParam* f) {
  if (!fj.contains("id") || !fj.contains("type")) {
    return LUMICE_ERR_MISSING_FIELD;
  }
  f->id = fj.at("id").get<int>();

  // action is OPTIONAL and defaults to filter_in (core FilterConfig::from_json). An unrecognized
  // action string is still rejected here: core silently keeps filter_in for one, which turns a
  // typo into a wrong-but-running filter.
  f->action = 0;
  if (fj.contains("action")) {
    auto action_str = fj.at("action").get<std::string>();
    if (action_str == "filter_in") {
      f->action = 0;
    } else if (action_str == "filter_out") {
      f->action = 1;
    } else {
      return LUMICE_ERR_INVALID_VALUE;
    }
  }

  // Type-specific fields (JSON -> struct arm). Field names/defaults mirror core
  // config/filter_config.cpp::from_json. Parse does lossless mapping ONLY: value
  // validation (e.g. entry_exit min_len >= 1, max_len <= kMaxHits) stays single-source
  // in core and fires at commit time (surfaced by LUMICE_CommitScene). -1 encodes
  // an absent optional (wildcard / no bound).
  // SYNC: the per-type field list here mirrors the emit side in ConfigToJson (above);
  // adding/removing a filter type or field requires updating both.
  auto type_str = fj.at("type").get<std::string>();
  if (type_str == "none") {
    f->type = LUMICE_FILTER_TYPE_NONE;
  } else if (type_str == "raypath") {
    f->type = LUMICE_FILTER_TYPE_RAYPATH;
    // Required (core: j.at("raypath")). Accepting it as absent used to produce an empty raypath
    // filter that matches nothing, for a config core would have refused outright.
    if (!fj.contains("raypath")) {
      return LUMICE_ERR_MISSING_FIELD;
    }
    if (!fj.at("raypath").is_array()) {
      return LUMICE_ERR_INVALID_VALUE;
    }
    const auto& rp = fj.at("raypath");
    f->raypath_count = static_cast<int>(rp.size());
    if (f->raypath_count > LUMICE_MAX_CONFIG_RAYPATH_LEN) {
      return LUMICE_ERR_INVALID_CONFIG;
    }
    for (int k = 0; k < f->raypath_count; k++) {
      f->raypath[k] = rp[k].get<int>();
    }
  } else if (type_str == "entry_exit") {
    f->type = LUMICE_FILTER_TYPE_ENTRY_EXIT;
    f->ee_entry = (fj.contains("entry") && !fj.at("entry").is_null()) ? fj.at("entry").get<int>() : -1;
    f->ee_exit = (fj.contains("exit") && !fj.at("exit").is_null()) ? fj.at("exit").get<int>() : -1;
    f->ee_min_len = (fj.contains("min_len") && !fj.at("min_len").is_null()) ? fj.at("min_len").get<int>() : 1;
    f->ee_max_len = (fj.contains("max_len") && !fj.at("max_len").is_null()) ? fj.at("max_len").get<int>() : -1;
  } else if (type_str == "direction") {
    f->type = LUMICE_FILTER_TYPE_DIRECTION;
    f->dir_az = fj.at("az").get<float>();
    f->dir_el = fj.at("el").get<float>();
    f->dir_radii = fj.at("radii").get<float>();
  } else if (type_str == "crystal") {
    f->type = LUMICE_FILTER_TYPE_CRYSTAL;
    f->crystal_id = fj.at("crystal_id").get<int>();
  } else if (type_str == "complex") {
    // Placeholder: type is set here, but composition + composition_index are resolved by
    // the second pass in JsonToConfig (needs all simple filters parsed first so composition
    // cell ids can be validated). -1 marks "not yet resolved".
    f->type = LUMICE_FILTER_TYPE_COMPLEX;
    f->composition_index = -1;
  } else {
    return LUMICE_ERR_INVALID_VALUE;  // unknown type string
  }

  // Symmetry: common field for all types. "PBD" -> bitmask.
  f->symmetry = fj.contains("symmetry") ? SymmetryStringToBits(fj.at("symmetry").get<std::string>()) : 0;
  return LUMICE_OK;
}

// Parse the arm fields of one Color Predicate from JSON, mirroring ColorPredicateToJson.
// A ref JSON with no "type" key means match-all: sets predicate.type = UNSET, which the
// C-API commit path re-serializes as "no predicate fields on the wire", the same form
// core RaypathColorRef::from_json treats as NoneFilterParam / whole-crystal. The `symmetry`
// key is parsed at the tail and is independent of the arm type — including match-all, where
// (UNSET + non-default symmetry) is a legal state.
static LUMICE_ErrorCode JsonToColorPredicate(const nlohmann::json& j, LUMICE_ColorPredicate* p) {
  std::memset(p, 0, sizeof(*p));
  p->ee_entry = -1;
  p->ee_exit = -1;
  p->ee_min_len = 1;
  p->ee_max_len = -1;
  if (!j.contains("type")) {
    p->type = LUMICE_FILTER_TYPE_UNSET;  // match-all — falls through to shared symmetry tail.
  } else {
    auto type_str = j.at("type").get<std::string>();
    if (type_str == "none") {
      p->type = LUMICE_FILTER_TYPE_NONE;
    } else if (type_str == "raypath") {
      p->type = LUMICE_FILTER_TYPE_RAYPATH;
      if (j.contains("raypath") && j.at("raypath").is_array()) {
        const auto& rp = j.at("raypath");
        p->raypath_count = static_cast<int>(rp.size());
        if (p->raypath_count > LUMICE_MAX_CONFIG_RAYPATH_LEN) {
          return LUMICE_ERR_INVALID_CONFIG;
        }
        for (int k = 0; k < p->raypath_count; k++) {
          p->raypath[k] = rp[k].get<int>();
        }
      }
    } else if (type_str == "entry_exit") {
      p->type = LUMICE_FILTER_TYPE_ENTRY_EXIT;
      p->ee_entry = (j.contains("entry") && !j.at("entry").is_null()) ? j.at("entry").get<int>() : -1;
      p->ee_exit = (j.contains("exit") && !j.at("exit").is_null()) ? j.at("exit").get<int>() : -1;
      p->ee_min_len = (j.contains("min_len") && !j.at("min_len").is_null()) ? j.at("min_len").get<int>() : 1;
      p->ee_max_len = (j.contains("max_len") && !j.at("max_len").is_null()) ? j.at("max_len").get<int>() : -1;
    } else if (type_str == "direction") {
      p->type = LUMICE_FILTER_TYPE_DIRECTION;
      p->dir_az = j.at("az").get<float>();
      p->dir_el = j.at("el").get<float>();
      p->dir_radii = j.at("radii").get<float>();
    } else if (type_str == "crystal") {
      p->type = LUMICE_FILTER_TYPE_CRYSTAL;
      p->crystal_id = j.at("crystal_id").get<int>();
    } else {
      // Anything else (including "complex") is not a legal color predicate.
      return LUMICE_ERR_INVALID_VALUE;
    }
  }
  // Symmetry: common field for all arms (including match-all). Missing key => 0 / kSymNone.
  p->symmetry = j.contains("symmetry") ? SymmetryStringToBits(j.at("symmetry").get<std::string>()) : 0;
  return LUMICE_OK;
}

// Parse a single color class from JSON (mirrors core ColorClassConfig::from_json). Missing
// fields fall back to core defaults: combine="any", visible=true, solo=false.
static LUMICE_ErrorCode JsonToColorClass(const nlohmann::json& j, LUMICE_ColorClass* cls) {
  std::memset(cls, 0, sizeof(*cls));
  cls->visible = 1;
  cls->solo = 0;
  cls->combine = LUMICE_COLOR_COMBINE_ANY;

  if (!j.contains("color") || !j.at("color").is_array() || j.at("color").size() != 3) {
    return LUMICE_ERR_MISSING_FIELD;
  }
  const auto& jc = j.at("color");
  cls->color[0] = jc.at(0).get<float>();
  cls->color[1] = jc.at(1).get<float>();
  cls->color[2] = jc.at(2).get<float>();

  if (j.contains("combine")) {
    auto s = j.at("combine").get<std::string>();
    if (s == "any") {
      cls->combine = LUMICE_COLOR_COMBINE_ANY;
    } else if (s == "all") {
      cls->combine = LUMICE_COLOR_COMBINE_ALL;
    } else {
      return LUMICE_ERR_INVALID_VALUE;
    }
  }
  if (j.contains("visible")) {
    cls->visible = j.at("visible").get<bool>() ? 1 : 0;
  }
  if (j.contains("solo")) {
    cls->solo = j.at("solo").get<bool>() ? 1 : 0;
  }

  if (!j.contains("match") || !j.at("match").is_array()) {
    return LUMICE_ERR_MISSING_FIELD;
  }
  const auto& match = j.at("match");
  if (static_cast<int>(match.size()) > LUMICE_MAX_CONFIG_COLOR_REFS) {
    return LUMICE_ERR_INVALID_CONFIG;
  }
  cls->match_count = static_cast<int>(match.size());
  for (int k = 0; k < cls->match_count; k++) {
    const auto& jr = match[k];
    auto& ref = cls->match[k];
    if (!jr.contains("layer") || !jr.contains("crystal")) {
      return LUMICE_ERR_MISSING_FIELD;
    }
    ref.layer = jr.at("layer").get<int>();
    ref.crystal = jr.at("crystal").get<int>();
    auto err = JsonToColorPredicate(jr, &ref.predicate);
    if (err != LUMICE_OK) {
      return err;
    }
  }
  return LUMICE_OK;
}

// Parse the top-level "raypath_color" JSON (both bare-array and object forms) into
// ConfigScratch. Mirrors core RaypathColorConfig::from_json.
static LUMICE_ErrorCode JsonToRaypathColor(const nlohmann::json& j, ConfigScratch* out) {
  const nlohmann::json* classes_arr = nullptr;
  // Single-source default: track the core `kDefaultCompositeMode` (currently
  // "painter" per doc §4.8) so a bare-array config / object with no "mode"
  // field resolves to the SAME enum here as it does in core from_json.
  std::string mode_str = ns::kDefaultCompositeMode;
  if (j.is_array()) {
    classes_arr = &j;
  } else if (j.is_object()) {
    if (j.contains("mode")) {
      mode_str = j.at("mode").get<std::string>();
    }
    if (j.contains("classes") && j.at("classes").is_array()) {
      classes_arr = &j.at("classes");
    }
  } else {
    return LUMICE_ERR_INVALID_VALUE;
  }

  if (mode_str == "dominant") {
    out->raypath_color_mode = LUMICE_COLOR_MODE_DOMINANT;
  } else if (mode_str == "additive") {
    out->raypath_color_mode = LUMICE_COLOR_MODE_ADDITIVE;
  } else {
    // "painter" is the default; any other unknown mode degrades to painter here
    // to mirror core ParseCompositeMode's fallback (see doc §4.8).
    out->raypath_color_mode = LUMICE_COLOR_MODE_PAINTER;
  }

  if (classes_arr == nullptr) {
    // Explicitly clear any prior allocation (Release is null-safe).
    ConfigReleaseColorClasses(out);
    return LUMICE_OK;
  }
  if (static_cast<int>(classes_arr->size()) > LUMICE_MAX_CONFIG_COLOR_CLASSES) {
    return LUMICE_ERR_INVALID_CONFIG;
  }
  const int count = static_cast<int>(classes_arr->size());
  if (count == 0) {
    ConfigReleaseColorClasses(out);
    return LUMICE_OK;
  }
  // Implicit allocation: the caller cannot know the class count before parsing, so we allocate
  // on their behalf here. Ownership is
  // transferred to `out`; the caller must eventually call
  // ConfigReleaseColorClasses (or use a RAII guard).
  LUMICE_ColorClass* classes = ConfigCreateColorClasses(out, count);
  if (!classes) {
    // count is bounded above by LUMICE_MAX_CONFIG_COLOR_CLASSES; only OOM reaches here.
    return LUMICE_ERR_INVALID_CONFIG;
  }
  // Validate/fill each class. On mid-array failure release the allocation so the caller
  // does not observe a half-populated array (mirrors the spectrum_entries discipline).
  for (int i = 0; i < count; i++) {
    auto err = JsonToColorClass((*classes_arr)[i], &classes[i]);
    if (err != LUMICE_OK) {
      ConfigReleaseColorClasses(out);
      return err;
    }
  }
  return LUMICE_OK;
}

// Reference-integrity helpers for the scene block. Crystals and filters are parsed into `out`
// before the scene block is, so a scattering entry's ids can be checked against what was actually
// declared — the C-API-side equivalent of core's m.crystals_.at() / m.filters_.at() lookups.
static bool ConfigHasCrystal(const ConfigScratch& c, int id) {
  for (int i = 0; i < c.crystal_count; i++) {
    if (c.crystals[i].id == id) {
      return true;
    }
  }
  return false;
}

static bool ConfigHasFilter(const ConfigScratch& c, int id) {
  for (int i = 0; i < c.filter_count; i++) {
    if (c.filters[i].id == id) {
      return true;
    }
  }
  return false;
}

// Parse the JSON "scene" subsection (light source + simulation params) into a ConfigScratch.
// Named ...SceneParams (not JsonToScene) to avoid colliding, on sight, with the public opaque
// type LUMICE_Scene — this file-static parser has always been about the scene PARAMS block only.
static LUMICE_ErrorCode JsonToSceneParams(const nlohmann::json& scene, ConfigScratch* out) {
  // Light source: required, and so are its `type` / `altitude` / `spectrum` keys (core
  // LightSourceConfig::from_json reads all three with .at()). A `type` other than "sun" is
  // rejected rather than mirrored: core only logs and leaves its SunParam at the all-zero default.
  {
    if (!scene.contains("light_source")) {
      return LUMICE_ERR_MISSING_FIELD;
    }
    const auto& ls = scene.at("light_source");
    if (!ls.contains("type") || !ls.contains("altitude") || !ls.contains("spectrum")) {
      return LUMICE_ERR_MISSING_FIELD;
    }
    if (ls.at("type").get<std::string>() != "sun") {
      return LUMICE_ERR_INVALID_VALUE;
    }
    out->sun_altitude = ls.at("altitude").get<float>();
    if (ls.contains("azimuth")) {
      out->sun_azimuth = ls.at("azimuth").get<float>();
    }
    if (ls.contains("diameter")) {
      out->sun_diameter = ls.at("diameter").get<float>();
    }
    {
      const auto& sp = ls.at("spectrum");
      if (sp.is_string()) {
        out->spectrum = MapSpectrumString(sp.get<std::string>());
        if (!out->spectrum) {
          return LUMICE_ERR_INVALID_VALUE;
        }
        out->spectrum_count = 0;
      } else if (sp.is_array()) {
        const int count = static_cast<int>(sp.size());
        if (count > LUMICE_MAX_CONFIG_SPECTRUM_ENTRIES) {
          return LUMICE_ERR_INVALID_CONFIG;
        }
        // Validate + fill every entry BEFORE publishing spectrum_count, so a mid-array failure returns
        // an error without leaving the struct claiming N entries with only some slots written (callers
        // that reuse a non-zeroed struct would otherwise read garbage). spectrum_count is the last write.
        out->spectrum_count = 0;
        for (int i = 0; i < count; i++) {
          const auto& e = sp[i];
          if (!e.is_object() || !e.contains("wavelength") || !e.contains("weight")) {
            return LUMICE_ERR_MISSING_FIELD;
          }
          out->spectrum_entries[i].wavelength = e.at("wavelength").get<float>();
          out->spectrum_entries[i].weight = e.at("weight").get<float>();
        }
        out->spectrum_count = count;
        out->spectrum = "D65";  // fallback string kept for downstream defaults
      } else {
        return LUMICE_ERR_INVALID_VALUE;
      }
    }
  }

  // Ray num: required (core: j_scene.at("ray_num")).
  if (!scene.contains("ray_num")) {
    return LUMICE_ERR_MISSING_FIELD;
  }
  {
    const auto& rn = scene.at("ray_num");
    if (rn.is_string() && rn.get<std::string>() == "infinite") {
      out->infinite = 1;
      out->ray_num = 0;
    } else if (rn.is_number()) {
      out->infinite = 0;
      out->ray_num = rn.get<LUMICE_RayCount>();
    } else {
      return LUMICE_ERR_INVALID_VALUE;
    }
  }

  // max_hits: required (core: j_scene.at("max_hits")). Its RANGE check stays single-source in
  // core and fires at commit, matching the geom_clock convention below.
  if (!scene.contains("max_hits")) {
    return LUMICE_ERR_MISSING_FIELD;
  }
  out->max_hits = scene.at("max_hits").get<int>();

  // geom_clock: lossless parse only, no range check (range {0}∪[1,64] is validated single-source
  // in core config_manager.cpp at commit, matching the ray_num/max_hits convention here).
  if (scene.contains("geom_clock")) {
    out->geom_clock = scene.at("geom_clock").get<int>();
  }

  // ray_allocation: verbatim pass-through (see ConfigScratch::ray_allocation). Only its TYPE is
  // checked here; which spellings mean what is core's decision, made at commit.
  out->ray_allocation[0] = '\0';
  if (scene.contains("ray_allocation")) {
    const auto& ra = scene.at("ray_allocation");
    if (!ra.is_string()) {
      return LUMICE_ERR_INVALID_VALUE;
    }
    const std::string spelled = ra.get<std::string>();
    std::snprintf(out->ray_allocation, sizeof(out->ray_allocation), "%s", spelled.c_str());
  }

  // Scattering: required (core: j_scene.at("scattering")), and so is each layer's "entries"
  // array. Every entry must name an EXISTING crystal, and a referenced filter must exist too —
  // core resolves both through m.crystals_.at() / m.filters_.at(), which throw on a dangling id.
  // That check is only possible here because crystals and filters are parsed before the scene
  // block, the same order core uses.
  if (!scene.contains("scattering")) {
    return LUMICE_ERR_MISSING_FIELD;
  }
  {
    const auto& scat = scene.at("scattering");
    if (!scat.is_array()) {
      return LUMICE_ERR_INVALID_VALUE;
    }
    if (static_cast<int>(scat.size()) > LUMICE_MAX_CONFIG_SCATTER_LAYERS) {
      return LUMICE_ERR_INVALID_CONFIG;
    }
    out->scatter_count = static_cast<int>(scat.size());
    for (int i = 0; i < out->scatter_count; i++) {
      const auto& lj = scat[i];
      auto& layer = out->scattering[i];
      // "prob" is required, mirroring core's ParseScatteringInfo. This must be checked HERE and
      // not left to core: the CLI reaches core only via this function, which then re-serializes
      // the parsed struct through ConfigToJson — and that writer emits `prob` unconditionally,
      // so a missing key would be silently filled in before core ever saw it. Unlike the other
      // missing-field branches in this function (which report through the return code alone),
      // this one also logs: the message IS the migration guidance for a contract narrowing.
      if (!lj.contains("prob")) {
        LOG_ERROR(
            "JsonToSceneParams: scene.scattering[{}] is missing required field \"prob\" "
            "(multi-scattering probability). The historical default was 0.0; add \"prob\": 0 "
            "explicitly to keep that behavior.",
            i);
        return LUMICE_ERR_MISSING_FIELD;
      }
      layer.probability = lj.at("prob").get<float>();
      if (!lj.contains("entries") || !lj.at("entries").is_array()) {
        return LUMICE_ERR_MISSING_FIELD;
      }
      const auto& entries = lj.at("entries");
      if (static_cast<int>(entries.size()) > LUMICE_MAX_CONFIG_SCATTER_ENTRIES) {
        return LUMICE_ERR_INVALID_CONFIG;
      }
      layer.entry_count = static_cast<int>(entries.size());
      for (int k = 0; k < layer.entry_count; k++) {
        const auto& ej = entries[k];
        auto& e = layer.entries[k];
        if (!ej.contains("crystal")) {
          return LUMICE_ERR_MISSING_FIELD;
        }
        e.crystal_id = ej.at("crystal").get<int>();
        if (!ConfigHasCrystal(*out, e.crystal_id)) {
          return LUMICE_ERR_INVALID_CONFIG;
        }
        // proportion defaults to 100 (core ScatteringSetting's crystal_proportion_); the zeroed
        // struct would mean "this crystal never gets picked".
        e.proportion = 100.0f;
        if (ej.contains("proportion")) {
          e.proportion = ej.at("proportion").get<float>();
        }
        e.filter_id = -1;
        if (ej.contains("filter")) {
          e.filter_id = ej.at("filter").get<int>();
          if (!ConfigHasFilter(*out, e.filter_id)) {
            return LUMICE_ERR_INVALID_CONFIG;
          }
        }
      }
    }
  }
  return LUMICE_OK;
}

// Inverse of MapVisibleFromCApi. Total over the core enumeration (no default arm) so adding a
// visibility range to core breaks the build here instead of decoding to a wrong C API constant —
// same fail-loud contract as MapLensTypeToCApi.
static int MapVisibleToCApi(ns::RenderConfig::VisibleRange visible) {
  switch (visible) {
    case ns::RenderConfig::kUpper:
      return LUMICE_VISIBLE_UPPER;
    case ns::RenderConfig::kLower:
      return LUMICE_VISIBLE_LOWER;
    case ns::RenderConfig::kFull:
      return LUMICE_VISIBLE_FULL;
  }
  throw std::invalid_argument("unmapped core VisibleRange: " + std::to_string(static_cast<int>(visible)));
}

// Inverse of MapEvModeFromCApi. Total over the core enumeration (no default arm), same fail-loud
// contract as MapLensTypeToCApi / MapVisibleToCApi.
static int MapEvModeToCApi(ns::RenderConfig::EvMode ev_mode) {
  switch (ev_mode) {
    case ns::RenderConfig::kRelative:
      return LUMICE_EV_MODE_RELATIVE;
    case ns::RenderConfig::kAbsolute:
      return LUMICE_EV_MODE_ABSOLUTE;
  }
  throw std::invalid_argument("unmapped core EvMode: " + std::to_string(static_cast<int>(ev_mode)));
}

// Inverse of MapToneFromCApi, same fail-loud contract as the three above.
static int MapToneToCApi(ns::RenderConfig::Tone tone) {
  switch (tone) {
    case ns::RenderConfig::kScreen:
      return LUMICE_TONE_SCREEN;
    case ns::RenderConfig::kPrint:
      return LUMICE_TONE_PRINT;
  }
  throw std::invalid_argument("unmapped core Tone: " + std::to_string(static_cast<int>(tone)));
}

// Inverse of MapDisplayModeFromCApi, same fail-loud contract.
static int MapDisplayModeToCApi(ns::RenderConfig::DisplayMode display_mode) {
  switch (display_mode) {
    case ns::RenderConfig::kDisplayNormal:
      return LUMICE_DISPLAY_MODE_NORMAL;
    case ns::RenderConfig::kDisplayChannelBr:
      return LUMICE_DISPLAY_MODE_CHANNEL_BR;
  }
  throw std::invalid_argument("unmapped core DisplayMode: " + std::to_string(static_cast<int>(display_mode)));
}

// The two enum-valued renderer fields need a string pre-check: NLOHMANN_JSON_SERIALIZE_ENUM maps
// an unrecognized value to the FIRST table entry ("linear" / "upper"), so a typo'd projection
// would be silently misread rather than reported. Same deliberate exception to "align with core"
// already taken for the distribution / crystal type strings above.
static bool IsKnownLensTypeString(const std::string& s) {
  return s == "linear" || s == "fisheye_equal_area" || s == "fisheye_equidistant" || s == "fisheye_stereographic" ||
         s == "dual_fisheye_equal_area" || s == "dual_fisheye_equidistant" || s == "dual_fisheye_stereographic" ||
         s == "rectangular" || s == "fisheye_orthographic" || s == "dual_fisheye_orthographic" || s == "globe";
}

static bool IsKnownVisibleString(const std::string& s) {
  return s == "upper" || s == "lower" || s == "full";
}

// Same reason as the two above: core's EvMode table maps an unrecognized string to its first
// entry ("relative"), so without this pre-check a typo'd mode would be silently misread as the
// default instead of reported.
static bool IsKnownEvModeString(const std::string& s) {
  return s == "relative" || s == "absolute";
}

// The pre-check that makes this decoder REJECT an unknown tone, where core's ParseRenderConfig
// warns and falls back to "screen". The two are deliberately not aligned, and each follows its own
// file's rule rather than the other's: this function serves LUMICE_SceneFromJson /
// LUMICE_SceneFromJsonFile, whose every other enum-valued renderer field (lens type, visible,
// ev_mode) already rejects, and a C API that reported OK while quietly substituting a value would
// leave its caller with no way to learn that anything happened. core's parser has a logger to say
// so, and refusing to load a whole document over one appearance string would cost more there than
// it protects. test_json_parser_parity.cpp compares the two decoders on LEGAL values only, for
// exactly this reason.
static bool IsKnownToneString(const std::string& s) {
  return s == "screen" || s == "print";
}

// The display_mode twin of IsKnownToneString: rejected here, warned-and-defaulted in core, for the
// reason stated above.
static bool IsKnownDisplayModeString(const std::string& s) {
  return s == "normal" || s == "channel_br";
}

// Decode one field with core's own from_json (single source for the f->fov trigonometry, the
// MaxFov range check and the per-field optional/default logic), mapping nlohmann's exception
// taxonomy onto C API error codes: a missing key (out_of_range id 403) is MISSING_FIELD,
// everything else (type mismatch, out-of-range fov, too-short focal length) is INVALID_VALUE.
template <typename T>
static LUMICE_ErrorCode DecodeCoreField(const nlohmann::json& j, T& dst) {
  try {
    j.get_to(dst);
  } catch (const nlohmann::json::out_of_range& e) {
    return e.id == 403 ? LUMICE_ERR_MISSING_FIELD : LUMICE_ERR_INVALID_VALUE;
  } catch (const nlohmann::json::exception&) {
    return LUMICE_ERR_INVALID_VALUE;
  }
  return LUMICE_OK;
}

// Decode one grid-line array ("grid.angular_dist" / "grid.elevation" / "grid.longitude") into the
// fixed-capacity C array.
static LUMICE_ErrorCode JsonToGridLines(const nlohmann::json& arr_j, LUMICE_GridLine* out, int* out_count) {
  if (!arr_j.is_array()) {
    return LUMICE_ERR_INVALID_VALUE;
  }
  if (static_cast<int>(arr_j.size()) > LUMICE_MAX_CONFIG_GRID_LINES) {
    return LUMICE_ERR_INVALID_CONFIG;
  }
  *out_count = static_cast<int>(arr_j.size());
  for (int k = 0; k < *out_count; k++) {
    ns::GridLineParam g;
    const LUMICE_ErrorCode err = DecodeCoreField(arr_j[k], g);
    if (err != LUMICE_OK) {
      return err;
    }
    out[k].value = g.value_;
    out[k].width = g.width_;
    out[k].opacity = g.opacity_;
    out[k].color[0] = g.color_[0];
    out[k].color[1] = g.color_[1];
    out[k].color[2] = g.color_[2];
  }
  return LUMICE_OK;
}

// Decode "grid.markers" into the fixed-capacity C array. Twin of JsonToGridLines above, with two
// rules that array does not have, both of them "reject" rather than "repair":
//   - an unknown id string is refused by core's MarkerStyleParam::from_json, which throws rather
//     than falling back the way NLOHMANN_JSON_SERIALIZE_ENUM would; DecodeCoreField turns that into
//     LUMICE_ERR_INVALID_VALUE;
//   - a duplicate id is refused here, through the SAME HasDuplicateMarkerId config_manager.cpp
//     calls, so the two decoders cannot drift into two answers.
static LUMICE_ErrorCode JsonToMarkerStyles(const nlohmann::json& arr_j, LUMICE_MarkerStyle* out, int* out_count) {
  if (!arr_j.is_array()) {
    return LUMICE_ERR_INVALID_VALUE;
  }
  // Length first, so an over-long list is one comparison rather than six decodes. It is nearly
  // unreachable given the duplicate rule below — with only six legal ids, a seventh entry must
  // repeat one — but "nearly" is doing work that a bounds check should not have to delegate.
  if (static_cast<int>(arr_j.size()) > LUMICE_MAX_CONFIG_MARKERS) {
    return LUMICE_ERR_INVALID_CONFIG;
  }
  std::vector<ns::MarkerStyleParam> core_markers;
  core_markers.reserve(arr_j.size());
  for (const auto& entry : arr_j) {
    ns::MarkerStyleParam m;
    const LUMICE_ErrorCode err = DecodeCoreField(entry, m);
    if (err != LUMICE_OK) {
      return err;
    }
    core_markers.push_back(m);
  }
  ns::MarkerRefId dup{};
  if (ns::HasDuplicateMarkerId(core_markers, &dup)) {
    return LUMICE_ERR_INVALID_CONFIG;
  }
  *out_count = static_cast<int>(core_markers.size());
  for (int k = 0; k < *out_count; k++) {
    out[k].id = static_cast<int>(core_markers[k].id_);
    out[k].enabled = core_markers[k].enabled_ ? 1 : 0;
    out[k].color[0] = core_markers[k].color_[0];
    out[k].color[1] = core_markers[k].color_[1];
    out[k].color[2] = core_markers[k].color_[2];
  }
  return LUMICE_OK;
}

// Decode ONE renderer object — the shape RendererToJson writes — into a LUMICE_RenderParam.
// This is the loop body of JsonToRenderers, lifted out so LUMICE_SceneGetRenderer can read a
// single scene entry back through the same decoder LUMICE_SceneFromJson/File runs over the whole
// array: one JSON -> C struct routine, so a field gained on one path cannot be missed on the
// other. `out` is written in place from whatever it holds (JsonToRenderers hands it a zeroed
// ConfigScratch slot; the getter zeroes it first).
LUMICE_ErrorCode JsonToRenderer(const nlohmann::json& rj, LUMICE_RenderParam* out) {
  auto& r = *out;
  // id and resolution are both required (core ParseRenderConfig: j.at("id") / j.at("resolution")).
  if (!rj.contains("id") || !rj.contains("resolution")) {
    return LUMICE_ERR_MISSING_FIELD;
  }
  r.id = rj.at("id").get<int>();
  if (!rj.at("resolution").is_array() || rj.at("resolution").size() != 2) {
    return LUMICE_ERR_INVALID_VALUE;
  }
  r.resolution_w = rj.at("resolution")[0].get<int>();
  r.resolution_h = rj.at("resolution")[1].get<int>();
  // intensity_factor defaults to 1.0 in core RenderConfig; the zeroed struct would mean a
  // zero-brightness renderer.
  r.intensity_factor = 1.0f;
  if (rj.contains("intensity_factor")) {
    r.intensity_factor = rj.at("intensity_factor").get<float>();
  }
  if (rj.contains("overlap")) {
    r.overlap = std::max(0.0f, rj.at("overlap").get<float>());
  }
  if (rj.contains("globe_back_fade")) {
    r.globe_back_fade = std::max(0.0f, rj.at("globe_back_fade").get<float>());
  }
  // Mirrors core RenderConfig::ev_mode_'s member initializer (kRelative); the zeroed struct
  // already holds it, but stating it keeps this decoder's defaults readable in one place.
  r.ev_mode = LUMICE_EV_MODE_RELATIVE;
  if (rj.contains("ev_mode")) {
    if (!rj.at("ev_mode").is_string() || !IsKnownEvModeString(rj.at("ev_mode").get<std::string>())) {
      return LUMICE_ERR_INVALID_VALUE;
    }
    auto ev_mode = ns::RenderConfig::kRelative;
    const LUMICE_ErrorCode err = DecodeCoreField(rj.at("ev_mode"), ev_mode);
    if (err != LUMICE_OK) {
      return err;
    }
    r.ev_mode = MapEvModeToCApi(ev_mode);
  }
  // Mirrors core RenderConfig::tone_'s member initializer (kScreen); like ev_mode above the
  // zeroed struct already holds it, and stating it keeps this decoder's defaults readable in one
  // place.
  r.tone = LUMICE_TONE_SCREEN;
  if (rj.contains("tone")) {
    if (!rj.at("tone").is_string() || !IsKnownToneString(rj.at("tone").get<std::string>())) {
      return LUMICE_ERR_INVALID_VALUE;
    }
    auto tone = ns::RenderConfig::kScreen;
    const LUMICE_ErrorCode err = DecodeCoreField(rj.at("tone"), tone);
    if (err != LUMICE_OK) {
      return err;
    }
    r.tone = MapToneToCApi(tone);
  }
  // Mirrors core RenderConfig::display_mode_'s member initializer, same shape as `tone` above.
  r.display_mode = LUMICE_DISPLAY_MODE_NORMAL;
  if (rj.contains("display_mode")) {
    if (!rj.at("display_mode").is_string() || !IsKnownDisplayModeString(rj.at("display_mode").get<std::string>())) {
      return LUMICE_ERR_INVALID_VALUE;
    }
    auto display_mode = ns::RenderConfig::kDisplayNormal;
    const LUMICE_ErrorCode err = DecodeCoreField(rj.at("display_mode"), display_mode);
    if (err != LUMICE_OK) {
      return err;
    }
    r.display_mode = MapDisplayModeToCApi(display_mode);
  }

  // ---- Fields the struct gained in v4.11 (previously parsed and thrown away) ----
  // Every default below mirrors the corresponding core RenderConfig member initializer
  // (config/render_config.hpp), and every present-key decode runs through core's own from_json.
  ns::LensParam lens{ ns::LensParam::kLinear, 90.0f };
  if (rj.contains("lens")) {
    const auto& lens_j = rj.at("lens");
    if (!lens_j.is_object()) {
      return LUMICE_ERR_INVALID_VALUE;
    }
    // Core's LensParam::from_json does j.at("type") — the key is required, not optional.
    if (!lens_j.contains("type")) {
      return LUMICE_ERR_MISSING_FIELD;
    }
    if (!lens_j.at("type").is_string() || !IsKnownLensTypeString(lens_j.at("type").get<std::string>())) {
      return LUMICE_ERR_INVALID_VALUE;
    }
    const LUMICE_ErrorCode err = DecodeCoreField(lens_j, lens);
    if (err != LUMICE_OK) {
      return err;
    }
  }
  r.lens_type = MapLensTypeToCApi(lens.type_);
  r.lens_fov = lens.fov_;

  r.lens_shift[0] = 0;
  r.lens_shift[1] = 0;
  if (rj.contains("lens_shift")) {
    const LUMICE_ErrorCode err = DecodeCoreField(rj.at("lens_shift"), r.lens_shift);
    if (err != LUMICE_OK) {
      return err;
    }
  }

  ns::ViewParam view{};
  if (rj.contains("view")) {
    const LUMICE_ErrorCode err = DecodeCoreField(rj.at("view"), view);
    if (err != LUMICE_OK) {
      return err;
    }
  }
  r.view_azimuth = view.az_;
  r.view_elevation = view.el_;
  r.view_roll = view.ro_;

  r.visible = LUMICE_VISIBLE_UPPER;
  if (rj.contains("visible")) {
    if (!rj.at("visible").is_string() || !IsKnownVisibleString(rj.at("visible").get<std::string>())) {
      return LUMICE_ERR_INVALID_VALUE;
    }
    auto visible = ns::RenderConfig::kUpper;
    const LUMICE_ErrorCode err = DecodeCoreField(rj.at("visible"), visible);
    if (err != LUMICE_OK) {
      return err;
    }
    r.visible = MapVisibleToCApi(visible);
  }

  // Absent key = no clip, which is what a config predating the field means. Twin of core's
  // ParseRenderConfig.
  r.front = 0;
  if (rj.contains("front")) {
    bool front = false;
    const LUMICE_ErrorCode err = DecodeCoreField(rj.at("front"), front);
    if (err != LUMICE_OK) {
      return err;
    }
    r.front = front ? 1 : 0;
  }

  // Twin of core's warning in config_manager.cpp::ParseRenderConfig; see the rationale there.
  if (rj.contains("background_color")) {
    ILOG_WARN(ns::GetGlobalLogger(),
              "render[id={}]: unknown key \"background_color\" is ignored; the background color key is "
              "\"background\" (sRGB triple)",
              r.id);
  }
  // The JSON key is sRGB (what a color picker shows); LUMICE_RenderParam::background is linear
  // (what PostSnapshot's additive blend needs) — see the field's comment in lumice_scene.h. The default
  // needs no conversion: 0 is a fixed point of both directions. Twin of the encode side in
  // RendererToJson, and of core's own conversion in config_manager.cpp::ParseRenderConfig.
  r.background[0] = r.background[1] = r.background[2] = 0.0f;
  if (rj.contains("background")) {
    const LUMICE_ErrorCode err = DecodeCoreField(rj.at("background"), r.background);
    if (err != LUMICE_OK) {
      return err;
    }
    for (float& c : r.background) {
      c = ns::SrgbToLinear(c);
    }
  }

  // WHITE, not zero, and the only default in this block that the zeroed struct does not already
  // hold: core's RenderConfig::paper_ initializes to {1,1,1}. Zeroing it here would make this
  // decoder hand back black paper for every document that omits the key — a divergence from
  // ParseRenderConfig, which the parity gate compares whole RenderConfigs to catch, and under the
  // subtractive operator an all-black page. Same linear-struct / sRGB-key split as `background`.
  r.paper[0] = r.paper[1] = r.paper[2] = 1.0f;
  if (rj.contains("paper")) {
    const LUMICE_ErrorCode err = DecodeCoreField(rj.at("paper"), r.paper);
    if (err != LUMICE_OK) {
      return err;
    }
    for (float& c : r.paper) {
      c = ns::SrgbToLinear(c);
    }
  }

  // Core's default is the {-1,-1,-1} "use the natural spectral color" sentinel, NOT black.
  r.ray_color[0] = r.ray_color[1] = r.ray_color[2] = -1.0f;
  if (rj.contains("ray_color")) {
    const LUMICE_ErrorCode err = DecodeCoreField(rj.at("ray_color"), r.ray_color);
    if (err != LUMICE_OK) {
      return err;
    }
  }

  r.angular_dist_count = 0;
  r.view_dist_count = 0;
  r.elevation_grid_count = 0;
  r.longitude_grid_count = 0;
  r.horizon = 0;  // core RenderConfig::horizon_ defaults to false
  // 1, NOT 0, and the only default in this block that is not zero-shaped: core's three
  // *_grid_line_ members default to TRUE, because a document written before those keys existed
  // already drew the lines its angle lists name. Zeroing them here instead would make the C API
  // decoder render every legacy config without its grid — a divergence from ParseRenderConfig,
  // which the parity gate compares whole RenderConfigs to catch.
  r.elevation_line = 1;
  r.longitude_line = 1;
  r.angular_dist_line = 1;
  r.view_dist_line = 1;
  // Same default and same reason as `horizon` above: core's *_label_ fields are opt-in.
  r.horizon_label = 0;
  r.grid_label = 0;
  r.angular_dist_label = 0;
  r.view_dist_label = 0;
  // The marker block's defaults come from core's struct rather than being spelled again here,
  // and they are written BEFORE the "grid" branch so a document with no key at all lands on the
  // same four values ParseRenderConfig leaves. Zeroing them instead would be a real divergence
  // between the two decoders, not a harmless one: three of the four defaults are non-zero, and
  // the parity gate compares whole RenderConfigs.
  {
    const ns::ZenithNadirParam kZenithNadirDefaults;
    r.zenith_nadir = kZenithNadirDefaults.enabled_ ? 1 : 0;
    r.zenith_nadir_radius_px = kZenithNadirDefaults.radius_px_;
    r.zenith_nadir_opacity = kZenithNadirDefaults.opacity_;
    std::copy(std::begin(kZenithNadirDefaults.color_), std::end(kZenithNadirDefaults.color_),
              std::begin(r.zenith_nadir_color));
  }
  // The marker family's defaults, same rule and same reason as the block above: taken from core's
  // struct rather than spelled again here, and written BEFORE the "grid" branch so a document
  // with no key at all lands where ParseRenderConfig leaves it. Two of the three are non-zero, so
  // zeroing them instead would be a real divergence between the two decoders.
  {
    const ns::RenderConfig kRenderDefaults;
    r.markers_count = 0;
    r.markers_opacity = kRenderDefaults.markers_opacity_;
    r.markers_radius_px = kRenderDefaults.markers_radius_px_;
  }
  if (rj.contains("grid")) {
    const auto& gj = rj.at("grid");
    if (!gj.is_object()) {
      return LUMICE_ERR_INVALID_VALUE;
    }
    // "central" is the pre-rename spelling of "angular_dist"; read it as an alias with the new
    // key winning, exactly as ParseRenderConfig does. The two decoders have to agree — that is
    // what test_json_parser_parity.cpp checks — so this branch and that one stay identical in
    // shape. Only the new key is ever written (RendererToJson above).
    if (gj.contains("angular_dist")) {
      const LUMICE_ErrorCode err = JsonToGridLines(gj.at("angular_dist"), r.angular_dist, &r.angular_dist_count);
      if (err != LUMICE_OK) {
        return err;
      }
    } else if (gj.contains("central")) {
      const LUMICE_ErrorCode err = JsonToGridLines(gj.at("central"), r.angular_dist, &r.angular_dist_count);
      if (err != LUMICE_OK) {
        return err;
      }
    }
    // The axis-referenced twin. No legacy spelling: the key was born with this name.
    if (gj.contains("view_dist")) {
      const LUMICE_ErrorCode err = JsonToGridLines(gj.at("view_dist"), r.view_dist, &r.view_dist_count);
      if (err != LUMICE_OK) {
        return err;
      }
    }
    if (gj.contains("elevation")) {
      const LUMICE_ErrorCode err = JsonToGridLines(gj.at("elevation"), r.elevation_grid, &r.elevation_grid_count);
      if (err != LUMICE_OK) {
        return err;
      }
    }
    if (gj.contains("longitude")) {
      const LUMICE_ErrorCode err = JsonToGridLines(gj.at("longitude"), r.longitude_grid, &r.longitude_grid_count);
      if (err != LUMICE_OK) {
        return err;
      }
    }
    if (gj.contains("horizon")) {
      bool outline = true;
      const LUMICE_ErrorCode err = DecodeCoreField(gj.at("horizon"), outline);
      if (err != LUMICE_OK) {
        return err;
      }
      r.horizon = outline ? 1 : 0;
    }
    // The other three families' line switches, read the same way the label block below is and
    // kept a SEPARATE array from it: the loop bodies are identical, but a line switch and a label
    // switch are two different questions about a family, and one table holding both would have to
    // be split again the first time either group grows or loses a member.
    {
      const std::pair<const char*, int*> kLineKeys[] = {
        { "elevation_line", &r.elevation_line },
        { "longitude_line", &r.longitude_line },
        { "angular_dist_line", &r.angular_dist_line },
        { "view_dist_line", &r.view_dist_line },
      };
      for (const auto& [key, field] : kLineKeys) {
        if (!gj.contains(key)) {
          continue;
        }
        bool on = false;
        const LUMICE_ErrorCode err = DecodeCoreField(gj.at(key), on);
        if (err != LUMICE_OK) {
          return err;
        }
        *field = on ? 1 : 0;
      }
    }
    // The three text-label switches. One loop over (key, field) rather than three copies of the
    // same six lines: they differ only in which key names which int, and a fourth family would
    // otherwise be a fourth chance to paste the wrong field name in.
    {
      const std::pair<const char*, int*> kLabelKeys[] = {
        { "horizon_label", &r.horizon_label },
        { "label", &r.grid_label },
        { "angular_dist_label", &r.angular_dist_label },
        { "view_dist_label", &r.view_dist_label },
      };
      for (const auto& [key, field] : kLabelKeys) {
        if (!gj.contains(key)) {
          continue;
        }
        bool on = false;
        const LUMICE_ErrorCode err = DecodeCoreField(gj.at(key), on);
        if (err != LUMICE_OK) {
          return err;
        }
        *field = on ? 1 : 0;
      }
    }
    if (gj.contains("zenith_nadir")) {
      // Seeded with the defaults above so a PARTIAL object keeps them for the keys it omits,
      // which is what core's from_json does with the same input.
      ns::ZenithNadirParam zn;
      zn.enabled_ = r.zenith_nadir != 0;
      zn.radius_px_ = r.zenith_nadir_radius_px;
      zn.opacity_ = r.zenith_nadir_opacity;
      std::copy(std::begin(r.zenith_nadir_color), std::end(r.zenith_nadir_color), std::begin(zn.color_));
      const LUMICE_ErrorCode err = DecodeCoreField(gj.at("zenith_nadir"), zn);
      if (err != LUMICE_OK) {
        return err;
      }
      r.zenith_nadir = zn.enabled_ ? 1 : 0;
      r.zenith_nadir_radius_px = zn.radius_px_;
      r.zenith_nadir_opacity = zn.opacity_;
      std::copy(std::begin(zn.color_), std::end(zn.color_), std::begin(r.zenith_nadir_color));
    }
    if (gj.contains("markers")) {
      const LUMICE_ErrorCode err = JsonToMarkerStyles(gj.at("markers"), r.markers, &r.markers_count);
      if (err != LUMICE_OK) {
        return err;
      }
    }
    // The two family-wide appearance keys, siblings of the array rather than members of it —
    // the shape core's to_json writes, which is what test_json_parser_parity.cpp compares.
    if (gj.contains("markers_opacity")) {
      const LUMICE_ErrorCode err = DecodeCoreField(gj.at("markers_opacity"), r.markers_opacity);
      if (err != LUMICE_OK) {
        return err;
      }
    }
    if (gj.contains("markers_radius_px")) {
      const LUMICE_ErrorCode err = DecodeCoreField(gj.at("markers_radius_px"), r.markers_radius_px);
      if (err != LUMICE_OK) {
        return err;
      }
    }
  }
  return LUMICE_OK;
}

static LUMICE_ErrorCode JsonToRenderers(const nlohmann::json& render_arr, ConfigScratch* out) {
  const int render_count = static_cast<int>(render_arr.size());
  if (render_count > LUMICE_MAX_CONFIG_RENDERERS) {
    ILOG_ERROR(ns::GetGlobalLogger(), "config has {} \"render\" entries, exceeding the limit of {}", render_count,
               LUMICE_MAX_CONFIG_RENDERERS);
    return LUMICE_ERR_INVALID_CONFIG;
  }
  out->renderer_count = render_count;
  for (int i = 0; i < out->renderer_count; i++) {
    if (const LUMICE_ErrorCode err = JsonToRenderer(render_arr[i], &out->renderers[i]); err != LUMICE_OK) {
      return err;
    }
  }
  return LUMICE_OK;
}

// Resolve a complex filter's "composition" JSON into a ConfigScratch compositions[] slot and
// set f->composition_index. Validates reference integrity: each term id must reference an
// existing SIMPLE (non-complex) filter, and clause/term/composition counts respect the ABI
// bounds. Must run after all simple filters are parsed (two-pass, mirrors config_manager.cpp).
static LUMICE_ErrorCode JsonToComplexComposition(const nlohmann::json& fj, ConfigScratch* out, LUMICE_FilterParam* f) {
  if (!fj.contains("composition") || !fj.at("composition").is_array()) {
    return LUMICE_ERR_MISSING_FIELD;
  }
  const auto& cmp = fj.at("composition");
  // v4.9: build the full (term_counts, term_ids) triple on the
  // stack first, then commit atomically via LUMICE_CompositionSetClauses AFTER all
  // validation succeeds. This also removes the pre-v4.9 "composition_count++ before
  // per-clause validation" partial-write hazard (§3.5 in the plan).
  const int clause_count = static_cast<int>(cmp.size());
  if (out->composition_count >= LUMICE_MAX_CONFIG_COMPLEX || clause_count > LUMICE_MAX_CONFIG_CLAUSES) {
    return LUMICE_ERR_INVALID_CONFIG;
  }
  std::vector<int> term_counts_vec;
  std::vector<int> term_ids_vec;
  term_counts_vec.reserve(static_cast<size_t>(clause_count));
  for (int cl = 0; cl < clause_count; cl++) {
    const auto& clause = cmp[cl];
    // A clause is either a bare id (1-term) or an array of ids (matches core to_json).
    if (clause.is_array()) {
      const int tn = static_cast<int>(clause.size());
      if (tn > LUMICE_MAX_CONFIG_TERMS) {
        return LUMICE_ERR_INVALID_CONFIG;
      }
      term_counts_vec.push_back(tn);
      for (int t = 0; t < tn; t++) {
        term_ids_vec.push_back(clause[t].get<int>());
      }
    } else {
      term_counts_vec.push_back(1);
      term_ids_vec.push_back(clause.get<int>());
    }
  }
  // Reference integrity: each term must name an existing SIMPLE filter (dangling ids and
  // references to a complex filter are rejected — the latter matches core semantics, which
  // only allow SimpleFilterParam in a composition, so cycles are impossible by construction).
  for (int ref_id : term_ids_vec) {
    bool found_simple = false;
    for (int k = 0; k < out->filter_count; k++) {
      if (out->filters[k].id == ref_id && out->filters[k].type != LUMICE_FILTER_TYPE_COMPLEX) {
        found_simple = true;
        break;
      }
    }
    if (!found_simple) {
      return LUMICE_ERR_INVALID_CONFIG;
    }
  }
  const int comp_idx = out->composition_count;
  LUMICE_ComplexComposition* comp = &out->compositions[comp_idx];
  auto err = LUMICE_CompositionSetClauses(comp, clause_count, term_counts_vec.data(), term_ids_vec.data());
  if (err != LUMICE_OK) {
    return err;
  }
  // Publish the composition to the config only after Set succeeds — matches the "advance
  // count last" pattern already used elsewhere in this file (e.g. spectrum_entries).
  out->composition_count = comp_idx + 1;
  f->composition_index = comp_idx;
  return LUMICE_OK;
}

LUMICE_ErrorCode JsonToConfig(const nlohmann::json& root, ConfigScratch* out) {
  // v4.8: raypath_color is an owning heap pointer. If `out` was reused across two Parse
  // calls, memset would clobber the pointer without freeing it — release first so the
  // memset that follows sees a defensibly-null field. Release is null-safe / idempotent.
  ConfigReleaseColorClasses(out);
  // v4.9: compositions[i].term_ids/term_counts are also owning
  // heap pointers — same memset-would-leak hazard as raypath_color; release first.
  ConfigReleaseCompositions(out);
  std::memset(out, 0, sizeof(ConfigScratch));
  out->spectrum = "D65";  // Safe default (memset leaves nullptr)

  // Crystals (required)
  if (!root.contains("crystal") || !root.at("crystal").is_array()) {
    return LUMICE_ERR_MISSING_FIELD;
  }
  const auto& crystals = root.at("crystal");
  if (static_cast<int>(crystals.size()) > LUMICE_MAX_CONFIG_CRYSTALS) {
    return LUMICE_ERR_INVALID_CONFIG;
  }
  out->crystal_count = static_cast<int>(crystals.size());
  for (int i = 0; i < out->crystal_count; i++) {
    auto err = JsonToCrystal(crystals[i], &out->crystals[i]);
    if (err != LUMICE_OK) {
      return err;
    }
  }

  // Filters: the key is required (core iterates j.at("filter")), though an empty array is fine.
  if (!root.contains("filter") || !root.at("filter").is_array()) {
    return LUMICE_ERR_MISSING_FIELD;
  }
  {
    const auto& filters = root.at("filter");
    if (static_cast<int>(filters.size()) > LUMICE_MAX_CONFIG_FILTERS) {
      return LUMICE_ERR_INVALID_CONFIG;
    }
    out->filter_count = static_cast<int>(filters.size());
    // Two-pass (mirrors core config_manager.cpp): pass 1 parses simple filters and marks
    // complex ones as placeholders; pass 2 resolves complex compositions once every simple
    // filter's id/type is known (needed to validate composition references).
    for (int i = 0; i < out->filter_count; i++) {
      auto err = JsonToFilter(filters[i], &out->filters[i]);
      if (err != LUMICE_OK) {
        return err;
      }
    }
    for (int i = 0; i < out->filter_count; i++) {
      if (out->filters[i].type != LUMICE_FILTER_TYPE_COMPLEX) {
        continue;
      }
      auto err = JsonToComplexComposition(filters[i], out, &out->filters[i]);
      if (err != LUMICE_OK) {
        return err;
      }
    }
  }

  // Scene (required)
  if (!root.contains("scene")) {
    return LUMICE_ERR_MISSING_FIELD;
  }
  auto err = JsonToSceneParams(root.at("scene"), out);
  if (err != LUMICE_OK) {
    return err;
  }

  // Renderers: required (core iterates j.at("render")).
  if (!root.contains("render") || !root.at("render").is_array()) {
    return LUMICE_ERR_MISSING_FIELD;
  }
  err = JsonToRenderers(root.at("render"), out);
  if (err != LUMICE_OK) {
    return err;
  }

  // Raypath color classes (optional, Design 2)
  if (root.contains("raypath_color")) {
    err = JsonToRaypathColor(root.at("raypath_color"), out);
    if (err != LUMICE_OK) {
      return err;
    }
  }

  return LUMICE_OK;
}


// Internal JSON-string -> ConfigScratch reader. Declared in c_api_scene_internal.hpp so the
// parser-parity / strictness tests can drive it directly; production code reaches the same
// validator through JsonToScene below.
LUMICE_ErrorCode ParseConfigString(const char* json_str, ConfigScratch* out) {
  if (!json_str || !out) {
    return LUMICE_ERR_NULL_ARG;
  }

  try {
    auto root = nlohmann::json::parse(json_str);
    return JsonToConfig(root, out);
  } catch (const nlohmann::json::parse_error&) {
    return LUMICE_ERR_INVALID_JSON;
  } catch (const std::exception&) {
    // Catches nlohmann::json::exception (malformed/out-of-range values) AND std::invalid_argument
    // from JsonToConfig's decode-direction Map*ToCApi calls (unmapped core enumerator) — both are
    // "the document decoded to something invalid", not a parse-syntax error.
    return LUMICE_ERR_INVALID_VALUE;
  }
}
