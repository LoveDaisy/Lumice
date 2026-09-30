#include "config/crystal_config.hpp"

#include <array>
#include <iterator>
#include <nlohmann/json.hpp>
#include <numeric>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "core/miller_wedge.hpp"
#include "util/logger.hpp"

namespace lumice {

namespace {

// The one key covering all six face slots — its value is a 6-element array, so
// the key is written once rather than once per face.
constexpr const char* kFaceDistanceSyncKey = "face_distance";

// The pyramid-only keys, spelled here and nowhere else in the tree. Every layer
// that reads or writes them goes through ShapeWedgeAngleKeyName /
// ShapeIndicesKeyName below.
constexpr const char* kShapeKeyUpperWedgeAngle = "upper_wedge_angle";
constexpr const char* kShapeKeyLowerWedgeAngle = "lower_wedge_angle";
constexpr const char* kShapeKeyUpperIndices = "upper_indices";
constexpr const char* kShapeKeyLowerIndices = "lower_indices";

// Read the optional "sync_group" sub-map. Keys name shape scalars the same way
// the surrounding shape JSON names their distributions ("height", "prism_h",
// "upper_h", "lower_h", "face_distance") — literally the same strings, which is
// why both levels now read them from ShapeScalarSyncKeyName. Absent key = every
// scalar independent, which is why an older config file needs no edit at all.
void ReadSyncGroupJson(const nlohmann::json& j, int sync_group[kShapeScalarCount],
                       const std::pair<const char*, int>* scalar_keys, size_t scalar_key_cnt) {
  for (int i = 0; i < kShapeScalarCount; i++) {
    sync_group[i] = 0;
  }
  if (!j.contains("sync_group")) {
    return;
  }
  const auto& sg = j.at("sync_group");
  for (size_t k = 0; k < scalar_key_cnt; k++) {
    if (sg.contains(scalar_keys[k].first)) {
      sync_group[scalar_keys[k].second] = sg.at(scalar_keys[k].first).get<int>();
    }
  }
  if (sg.contains(kFaceDistanceSyncKey)) {
    size_t i = 0;
    for (const auto& elem : sg.at(kFaceDistanceSyncKey)) {
      if (i >= 6) {
        break;
      }
      sync_group[kShapeScalarFace0 + i] = elem.get<int>();
      i++;
    }
  }
}

// Write "sync_group" only when something is actually synced, so the serialized
// form of every existing config stays byte-identical.
void WriteSyncGroupJson(nlohmann::json& j, const int sync_group[kShapeScalarCount],
                        const std::pair<const char*, int>* scalar_keys, size_t scalar_key_cnt) {
  nlohmann::json sg = nlohmann::json::object();
  for (size_t k = 0; k < scalar_key_cnt; k++) {
    if (sync_group[scalar_keys[k].second] != 0) {
      sg[scalar_keys[k].first] = sync_group[scalar_keys[k].second];
    }
  }
  bool any_face = false;
  for (int i = 0; i < 6; i++) {
    any_face = any_face || sync_group[kShapeScalarFace0 + i] != 0;
  }
  if (any_face) {
    // All six, zeros included — matching how face_distance itself serializes.
    sg[kFaceDistanceSyncKey] = std::vector<int>(sync_group + kShapeScalarFace0, sync_group + kShapeScalarFace0 + 6);
  }
  if (!sg.empty()) {
    j["sync_group"] = sg;
  }
}

constexpr std::pair<const char*, int> kPrismSyncGroupKeys[] = {
  { "height", kShapeScalarHeight },
};
constexpr std::pair<const char*, int> kPyramidSyncGroupKeys[] = {
  { "prism_h", kShapeScalarPrismH },
  { "upper_h", kShapeScalarUpperH },
  { "lower_h", kShapeScalarLowerH },
};

// Returns nullptr when the table names no key for `slot`. Unreachable while each
// table covers every applicable non-face slot of its type, which
// ShapeScalarSyncKeyNameApi.AgreesWithApplicabilityOnEverySlot pins; answering
// nullptr rather than asserting keeps a slot added to the applicability map but
// not to these tables from being handed an invented key name.
const char* LookupSyncKey(const std::pair<const char*, int>* keys, size_t key_cnt, int slot) {
  for (size_t k = 0; k < key_cnt; k++) {
    if (keys[k].second == slot) {
      return keys[k].first;
    }
  }
  return nullptr;
}

}  // namespace


const char* ShapeScalarSyncKeyName(CrystalKind kind, int slot) {
  if (!IsShapeScalarApplicable(kind, slot)) {
    return nullptr;
  }
  if (slot >= kShapeScalarFace0) {
    return kFaceDistanceSyncKey;
  }
  if (kind == CrystalKind::kPrism) {
    return LookupSyncKey(kPrismSyncGroupKeys, std::size(kPrismSyncGroupKeys), slot);
  }
  return LookupSyncKey(kPyramidSyncGroupKeys, std::size(kPyramidSyncGroupKeys), slot);
}

const char* ShapeWedgeAngleKeyName(bool upper) {
  return upper ? kShapeKeyUpperWedgeAngle : kShapeKeyLowerWedgeAngle;
}

const char* ShapeIndicesKeyName(bool upper) {
  return upper ? kShapeKeyUpperIndices : kShapeKeyLowerIndices;
}


// convert to & from json object
// ========== PrismCrystalParam ==========
void to_json(nlohmann::json& j, const PrismCrystalParam& p) {
  j[ShapeScalarSyncKeyName(CrystalKind::kPrism, kShapeScalarHeight)] = p.h_;
  j[ShapeScalarSyncKeyName(CrystalKind::kPrism, kShapeScalarFace0)] = p.d_;
  // Canonicalize a local copy: serialization must not depend on the caller having
  // normalized, and must not mutate what it was handed either.
  PrismCrystalParam canon = p;
  CanonicalizeSyncGroups(canon);
  WriteSyncGroupJson(j, canon.sync_group_, kPrismSyncGroupKeys, std::size(kPrismSyncGroupKeys));
}

void from_json(const nlohmann::json& j, PrismCrystalParam& p) {
  const char* height_key = ShapeScalarSyncKeyName(CrystalKind::kPrism, kShapeScalarHeight);
  if (j.contains(height_key)) {
    j.at(height_key).get_to(p.h_);
  }

  // Face distance: default value 1.0 (1.0 = regular hexagon in FillHexCrystalCoef)
  for (auto& x : p.d_) {
    x.type = DistributionType::kNoRandom;
    x.center = 1.0f;
  }
  const char* fd_key = ShapeScalarSyncKeyName(CrystalKind::kPrism, kShapeScalarFace0);
  if (j.contains(fd_key)) {
    size_t i = 0;
    for (const auto& elem : j.at(fd_key)) {
      if (i >= 6) {
        break;
      }
      elem.get_to(p.d_[i]);
      i++;
    }
  }

  ReadSyncGroupJson(j, p.sync_group_, kPrismSyncGroupKeys, std::size(kPrismSyncGroupKeys));
  PrepareSyncGroups(p);
}


// ========== PyramidCrystalParam ==========
void to_json(nlohmann::json& j, const PyramidCrystalParam& p) {
  constexpr auto kKind = CrystalKind::kPyramid;
  j[ShapeScalarSyncKeyName(kKind, kShapeScalarPrismH)] = p.h_prs_;
  j[ShapeScalarSyncKeyName(kKind, kShapeScalarUpperH)] = p.h_pyr_u_;
  j[ShapeScalarSyncKeyName(kKind, kShapeScalarLowerH)] = p.h_pyr_l_;
  j[ShapeWedgeAngleKeyName(true)] = p.wedge_angle_u_;
  j[ShapeWedgeAngleKeyName(false)] = p.wedge_angle_l_;
  j[ShapeScalarSyncKeyName(kKind, kShapeScalarFace0)] = p.d_;
  PyramidCrystalParam canon = p;
  CanonicalizeSyncGroups(canon);
  WriteSyncGroupJson(j, canon.sync_group_, kPyramidSyncGroupKeys, std::size(kPyramidSyncGroupKeys));
}

void from_json(const nlohmann::json& j, PyramidCrystalParam& p) {
  constexpr auto kKind = CrystalKind::kPyramid;

  // Heights
  j.at(ShapeScalarSyncKeyName(kKind, kShapeScalarPrismH)).get_to(p.h_prs_);
  const char* upper_h_key = ShapeScalarSyncKeyName(kKind, kShapeScalarUpperH);
  if (j.contains(upper_h_key)) {
    j.at(upper_h_key).get_to(p.h_pyr_u_);
  }
  const char* lower_h_key = ShapeScalarSyncKeyName(kKind, kShapeScalarLowerH);
  if (j.contains(lower_h_key)) {
    j.at(lower_h_key).get_to(p.h_pyr_l_);
  }

  // Wedge angle: prefer the explicit angle, fallback to the Miller-index conversion
  for (bool upper : { true, false }) {
    float& wedge_angle = upper ? p.wedge_angle_u_ : p.wedge_angle_l_;
    const char* angle_key = ShapeWedgeAngleKeyName(upper);
    const char* indices_key = ShapeIndicesKeyName(upper);
    if (j.contains(angle_key)) {
      wedge_angle = j.at(angle_key).get<float>();
    } else if (j.contains(indices_key) && j.at(indices_key).is_array()) {
      // Any array enters here, not just a three-element one: a wrong length is something the
      // conversion owner judges (and this warning reports), where it used to make the whole branch
      // fall through in silence and leave the default 28 degrees looking like a stated value.
      const auto& idx = j.at(indices_key);
      int hkl[3]{ 0, 0, 0 };
      bool all_integers = true;
      for (size_t i = 0; i < idx.size() && i < 3; i++) {
        if (!idx[i].is_number_integer()) {
          all_integers = false;
          break;
        }
        hkl[i] = idx[i].get<int>();
      }
      MillerConversionResult r;
      r.state = MillerConversionState::kInvalid;
      if (all_integers) {
        r = ConvertMillerIndexToWedgeAngle(hkl[0], hkl[1], hkl[2], static_cast<int>(idx.size()));
      }
      if (r.state == MillerConversionState::kValid || r.state == MillerConversionState::kNoCone) {
        wedge_angle = r.wedge_angle_deg;
      } else {
        LOG_WARNING("Crystal shape \"{}\": {} is not a usable wedge angle ({}, offending index {}); keeping {:.2f}.",
                    indices_key, idx.dump(), MillerConversionStateName(r.state), r.invalid_index, wedge_angle);
      }
    }
  }

  // Face distance: default value 1.0 (1.0 = regular hexagon in FillHexCrystalCoef)
  for (auto& x : p.d_) {
    x.type = DistributionType::kNoRandom;
    x.center = 1.0f;
  }
  const char* fd_key = ShapeScalarSyncKeyName(kKind, kShapeScalarFace0);
  if (j.contains(fd_key)) {
    size_t i = 0;
    for (const auto& elem : j.at(fd_key)) {
      if (i >= 6) {
        break;
      }
      elem.get_to(p.d_[i]);
      i++;
    }
  }

  ReadSyncGroupJson(j, p.sync_group_, kPyramidSyncGroupKeys, std::size(kPyramidSyncGroupKeys));
  PrepareSyncGroups(p);
}


// ========== CrystalConfig ==========
void to_json(nlohmann::json& j, const CrystalConfig& c) {
  j["id"] = c.id_;
  j["axis"] = c.axis_;
  std::visit([&j](auto&& p) { j["shape"] = p; }, c.param_);
}

void from_json(const nlohmann::json& j, CrystalConfig& c) {
  j.at("id").get_to(c.id_);

  const auto& j_type = j.at("type");
  if (j_type == "prism") {
    c.param_ = j.at("shape").get<PrismCrystalParam>();
  } else if (j_type == "pyramid") {
    c.param_ = j.at("shape").get<PyramidCrystalParam>();
  } else {
    // Rejecting rather than logging: with no type there is no shape to parse, so `param_` would
    // keep its default-constructed prism — a crystal nobody wrote, entering the simulation while
    // the error line scrolls past. The id is prepended by the caller in config_manager.cpp.
    throw std::invalid_argument("unknown crystal type: " + j_type.dump() + ". Write either \"prism\" or \"pyramid\".");
  }

  if (j.contains("axis")) {
    j.at("axis").get_to(c.axis_);
  }
}

}  // namespace lumice
