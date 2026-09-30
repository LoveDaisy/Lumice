#ifndef CONFIG_CRYSTAL_CONFIG_H_
#define CONFIG_CRYSTAL_CONFIG_H_

#include <memory>
#include <nlohmann/json.hpp>
#include <variant>

#include "core/crystal_kind.hpp"
#include "core/crystal_param.hpp"
#include "core/math.hpp"

namespace lumice {


//! @brief The JSON key naming shape-scalar `slot` — both under `shape` itself
//!   and under `shape.sync_group`.
//!
//! @details Returns a static string, or nullptr when the slot does not apply to
//!   `kind` (or is out of range). All six face slots share the single key
//!   "face_distance", whose value is a 6-element array — callers write it once,
//!   not once per face.
//!
//!   One key serves both levels because the sub-map names each scalar exactly as
//!   the surrounding shape object does; they were always the same string, written
//!   twice. The name still says "sync" for the reason a released C API symbol
//!   (LUMICE_ShapeScalarSyncKeyName) does not get renamed to describe a widened
//!   set of callers — the contract did not change, only who asks.
//!
//!   Same motive as IsShapeScalarApplicable: this used to be a `{key, slot}`
//!   literal table copied into three translation units (here, the C API, the GUI
//!   file IO) with nothing but a comment tying them together. Drift had no
//!   symptom — the layer that drifted would silently drop the field — so the
//!   copies are gone and every layer reads this.
const char* ShapeScalarSyncKeyName(CrystalKind kind, int slot);

//! @brief The JSON key under `shape` holding a pyramidal wedge angle, in degrees.
//!
//! @details Returns a static string, never null. Takes a plain `bool` rather
//!   than a CrystalKind + slot pair, because unlike the shape scalars there is
//!   nothing for a kind to select: both wedge angles exist only on a pyramid,
//!   and every call site is already inside a `type == pyramid` branch. Two
//!   states also do not earn an enum — there is no third wedge to name.
const char* ShapeWedgeAngleKeyName(bool upper);

//! @brief The JSON key under `shape` holding a pyramidal face's Miller indices.
//!
//! @details Returns a static string, never null. The legacy read-side spelling
//!   of the same physical quantity ShapeWedgeAngleKeyName names: a parser falls
//!   back to converting these three indices into an angle when the explicit
//!   wedge-angle key is absent. Write paths never emit it. Same `bool upper`
//!   parameterization, for the same reason.
const char* ShapeIndicesKeyName(bool upper);

using CrystalParam = std::variant<PrismCrystalParam, PyramidCrystalParam>;

// Per-param shape (de)serialization. Declared here (definitions in crystal_config.cpp)
// so consumers outside that TU — e.g. the C API mesh-preview path — can build a
// PrismCrystalParam/PyramidCrystalParam from a shape JSON via `get<>` (ADL).
void to_json(nlohmann::json& j, const PrismCrystalParam& p);
void from_json(const nlohmann::json& j, PrismCrystalParam& p);
void to_json(nlohmann::json& j, const PyramidCrystalParam& p);
void from_json(const nlohmann::json& j, PyramidCrystalParam& p);

struct CrystalConfig {
  IdType id_;
  CrystalParam param_;
  AxisDistribution axis_;
};

using CrystalConfigPtrU = std::unique_ptr<CrystalConfig>;
using CrystalConfigPtrS = std::shared_ptr<CrystalConfig>;

// convert to & from json object
void to_json(nlohmann::json& j, const CrystalConfig& c);
void from_json(const nlohmann::json& j, CrystalConfig& c);

}  // namespace lumice

#endif  // CONFIG_CRYSTAL_CONFIG_H_
