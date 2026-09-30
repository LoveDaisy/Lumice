// C API bridge for lumice_editor.h: the crystal-kind, axis-scalar, symmetry and raypath-text
// queries an editor asks without a server. LUMICE_GetCrystalMesh, the one editor function that
// needs the crystal sampler, lives in c_api_crystal_mesh.cpp. Registered at the scene layer.

#include <algorithm>
#include <cstdio>
#include <nlohmann/json.hpp>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "config/crystal_config.hpp"
#include "config/filter_config.hpp"
#include "config/raypath_validation.hpp"
#include "core/crystal.hpp"
#include "core/def.hpp"
#include "core/math.hpp"
#include "core/miller_wedge.hpp"
#include "include/lumice_editor.h"
#include "server/c_api_scene_internal.hpp"  // CrystalToJson

namespace ns = lumice;

// =============== Crystal Kind ===============
// The three kind-taking predicates below share one shape, replacing the
// `(kind == PRISM) ? kPrism : kPyramid` ternary all three used to spell:
//
//   switch (kind) { case PRISM: …; case PYRAMID: …; }   // no `default:` label
//   <negative answer>                                    // out-of-contract value
//
// The missing `default:` is the point, not an omission. -Wswitch (in -Wall) fires at
// every one of these sites the moment LUMICE_CrystalKind gains a third enumerator, so
// an expansion cannot silently keep mapping the new kind onto Pyramid the way the
// ternary did. The trailing return then answers a value outside the enum — which no
// caller in this repo can produce (every one passes a literal or a ternary over the
// GUI's own two-valued CrystalType), and which C++ cannot even form without UB, but
// which a C caller passing an int can. Rejecting beats guessing "Pyramid" there.
int LUMICE_IsLegalFace(LUMICE_CrystalKind kind, int face) {
  switch (kind) {
    case LUMICE_CRYSTAL_PRISM:
      return ns::IsLegalFace(ns::CrystalKind::kPrism, face) ? 1 : 0;
    case LUMICE_CRYSTAL_PYRAMID:
      return ns::IsLegalFace(ns::CrystalKind::kPyramid, face) ? 1 : 0;
  }
  return 0;
}

int LUMICE_IsShapeScalarApplicable(LUMICE_CrystalKind kind, int slot) {
  switch (kind) {
    case LUMICE_CRYSTAL_PRISM:
      return ns::IsShapeScalarApplicable(ns::CrystalKind::kPrism, slot) ? 1 : 0;
    case LUMICE_CRYSTAL_PYRAMID:
      return ns::IsShapeScalarApplicable(ns::CrystalKind::kPyramid, slot) ? 1 : 0;
  }
  return 0;
}

const char* LUMICE_ShapeScalarSyncKeyName(LUMICE_CrystalKind kind, int slot) {
  switch (kind) {
    case LUMICE_CRYSTAL_PRISM:
      return ns::ShapeScalarSyncKeyName(ns::CrystalKind::kPrism, slot);
    case LUMICE_CRYSTAL_PYRAMID:
      return ns::ShapeScalarSyncKeyName(ns::CrystalKind::kPyramid, slot);
  }
  return nullptr;
}

const char* LUMICE_ShapeWedgeAngleKeyName(int upper) {
  return ns::ShapeWedgeAngleKeyName(upper != 0);
}

const char* LUMICE_ShapeIndicesKeyName(int upper) {
  return ns::ShapeIndicesKeyName(upper != 0);
}


// =============== Axis Scalars ===============
// The wire indices are passed straight through to core's, so they must BE core's. A silent
// mismatch would hand every caller a neighbouring key rather than an error.
static_assert(LUMICE_AXIS_SCALAR_ZENITH == ns::kAxisScalarZenith &&
                  LUMICE_AXIS_SCALAR_AZIMUTH == ns::kAxisScalarAzimuth &&
                  LUMICE_AXIS_SCALAR_ROLL == ns::kAxisScalarRoll && LUMICE_AXIS_SCALAR_COUNT == ns::kAxisScalarCount,
              "LUMICE_AXIS_SCALAR_* must mirror core's AxisScalar enum");

const char* LUMICE_AxisScalarKeyName(int slot) {
  return ns::AxisScalarKeyName(slot);
}

// LUMICE_DIST_* -> core's DistributionType for the three symmetry-applicability predicates below.
// Spelled as a switch rather than a cast: the header promises only that LUMICE_DIST_* stays
// self-consistent, not that it stays numerically equal to core's DistributionType. Every other
// wire-enum translation in this file (LUMICE_DIST_* above, LUMICE_LENS_TYPE_*) goes through an
// explicit switch for the same reason. False on an unrecognised value, which each caller answers
// with the negative, matching LUMICE_IsLegalFace / LUMICE_IsShapeScalarApplicable.
static bool WireDistType(int wire, ns::DistributionType* out) {
  switch (wire) {
    case LUMICE_DIST_NO_RANDOM:
      *out = ns::DistributionType::kNoRandom;
      return true;
    case LUMICE_DIST_UNIFORM:
      *out = ns::DistributionType::kUniform;
      return true;
    case LUMICE_DIST_GAUSS:
      *out = ns::DistributionType::kGaussian;
      return true;
    case LUMICE_DIST_ZIGZAG:
      *out = ns::DistributionType::kZigzag;
      return true;
    case LUMICE_DIST_LAPLACIAN:
      *out = ns::DistributionType::kLaplacian;
      return true;
    case LUMICE_DIST_GAUSS_LEGACY:
      *out = ns::DistributionType::kGaussianLegacy;
      return true;
    default:
      return false;
  }
}

int LUMICE_IsDApplicable(int azimuth_dist_type, float azimuth_full_range_deg, float roll_anchor_deg) {
  ns::DistributionType az_type{};
  if (!WireDistType(azimuth_dist_type, &az_type)) {
    return 0;
  }
  return ns::detail::IsDApplicableParams(az_type, azimuth_full_range_deg, roll_anchor_deg) ? 1 : 0;
}

int LUMICE_IsPApplicable(int roll_dist_type, float roll_full_range_deg) {
  ns::DistributionType roll_type{};
  if (!WireDistType(roll_dist_type, &roll_type)) {
    return 0;
  }
  return ns::detail::IsPApplicableParams(roll_type, roll_full_range_deg) ? 1 : 0;
}

int LUMICE_IsBApplicable(int azimuth_dist_type, float azimuth_full_range_deg, int zenith_dist_type,
                         float zenith_center_deg, float zenith_full_range_deg) {
  ns::DistributionType az_type{};
  ns::DistributionType zenith_type{};
  if (!WireDistType(azimuth_dist_type, &az_type) || !WireDistType(zenith_dist_type, &zenith_type)) {
    return 0;
  }
  // The wire speaks zenith, core's predicate latitude (= 90 - zenith) — the conversion core's own
  // axis from_json makes. The spread is a width and does not change.
  return ns::detail::IsBApplicableParams(az_type, azimuth_full_range_deg, zenith_type, 90.0f - zenith_center_deg,
                                         zenith_full_range_deg) ?
             1 :
             0;
}


LUMICE_ErrorCode LUMICE_GetCrystalSymmetry(const LUMICE_CrystalParam* crystal, LUMICE_CrystalSymmetry* out) {
  if (!crystal || !out) {
    return LUMICE_ERR_NULL_ARG;
  }
  if (crystal->type != 0 && crystal->type != 1) {
    return LUMICE_ERR_INVALID_VALUE;
  }
  // The same wire -> core translation a committed scene takes (CrystalToJson, then core's
  // from_json, which also canonicalizes the sync groups), so the answer is about the crystal the
  // engine would build.
  ns::CrystalConfig config;
  try {
    config = CrystalToJson(*crystal, 0).get<ns::CrystalConfig>();
  } catch (...) {
    return LUMICE_ERR_INVALID_CONFIG;
  }
  const ns::GeometricSymmetry g =
      std::visit([](const auto& param) { return ns::DeriveGeometricSymmetry(param); }, config.param_);
  const auto d = ns::detail::DeriveDSymmetryParams(config.axis_);
  out->rotation_step = g.p_step;
  out->vertical_mirror_mask = g.d_valid_sigma_mask;
  out->horizontal_mirror = g.b_applicable ? 1 : 0;
  out->d_effective = ns::DMirrorActive(d.d_applicable, d.sigma_a, g) ? 1 : 0;
  // P acts when the ensemble admits it and the shape leaves at least one non-identity rotation;
  // B when the ensemble admits it and the shape has the horizontal mirror — the same
  // intersections the reductions take (crystal.cpp PActive / BActive, detail::ReduceBuffer).
  out->p_effective = (ns::detail::IsPApplicable(config.axis_) && g.p_step < ns::kHexagonalFnPeriod) ? 1 : 0;
  out->b_effective = (ns::detail::IsBApplicable(config.axis_) && g.b_applicable) ? 1 : 0;
  return LUMICE_OK;
}


int LUMICE_CouldCrystalHaveFace(const LUMICE_CrystalParam* crystal, int face) {
  if (!crystal || (crystal->type != 0 && crystal->type != 1) || face < 0 || face > 255) {
    return 1;
  }
  // The same wire -> core translation LUMICE_GetCrystalSymmetry takes.
  ns::CrystalConfig config;
  try {
    config = CrystalToJson(*crystal, 0).get<ns::CrystalConfig>();
  } catch (...) {
    return 1;
  }
  const auto fn = static_cast<ns::IdType>(face);
  return std::visit([fn](const auto& param) { return ns::CouldFaceExist(param, fn); }, config.param_) ? 1 : 0;
}

int LUMICE_CouldFilterMatchFace(const LUMICE_CrystalParam* crystal, int face, int symmetry) {
  if (!crystal || (crystal->type != 0 && crystal->type != 1) || face < 0 || face > 255) {
    return 1;
  }
  // The same wire -> core translation LUMICE_GetCrystalSymmetry takes.
  ns::CrystalConfig config;
  try {
    config = CrystalToJson(*crystal, 0).get<ns::CrystalConfig>();
  } catch (...) {
    return 1;
  }
  const auto fn = static_cast<ns::IdType>(face);
  const auto sym =
      static_cast<uint8_t>(symmetry & (ns::FilterConfig::kSymP | ns::FilterConfig::kSymB | ns::FilterConfig::kSymD));
  return std::visit([&](const auto& param) { return ns::CouldFilterMatchFace(param, config.axis_, fn, sym); },
                    config.param_) ?
             1 :
             0;
}


LUMICE_ErrorCode LUMICE_ExpandRaypathClass(const LUMICE_CrystalParam* crystal, const int* faces, int face_count,
                                           int symmetry, int semantics, int* out_faces, int* out_member_count) {
  if (!crystal || !faces || !out_faces || !out_member_count) {
    return LUMICE_ERR_NULL_ARG;
  }
  // Bounded by core's kMaxHits, not by its public spelling LUMICE_MAX_RAYPATH_SEGMENT_LEN: that
  // macro lives in lumice_engine.h, a layer above this bridge. The engine bridge pins the two equal
  // with a static_assert, so this is the same number, not a second opinion about it.
  if ((crystal->type != 0 && crystal->type != 1) || face_count < 1 || face_count > static_cast<int>(ns::kMaxHits) ||
      (semantics != LUMICE_SYMMETRY_SEMANTICS_LABEL && semantics != LUMICE_SYMMETRY_SEMANTICS_PHYSICAL)) {
    return LUMICE_ERR_INVALID_VALUE;
  }
  // The same wire -> core translation LUMICE_GetCrystalSymmetry takes.
  ns::CrystalConfig config;
  try {
    config = CrystalToJson(*crystal, 0).get<ns::CrystalConfig>();
  } catch (...) {
    return LUMICE_ERR_INVALID_CONFIG;
  }
  const ns::GeometricSymmetry shape =
      std::visit([](const auto& param) { return ns::DeriveGeometricSymmetry(param); }, config.param_);
  const ns::SymmetryGating gating = ns::DeriveSymmetryGating(
      semantics == LUMICE_SYMMETRY_SEMANTICS_LABEL ? ns::SymmetrySemantics::kLabel : ns::SymmetrySemantics::kPhysical,
      shape, config.axis_);
  const auto d = ns::detail::DeriveDSymmetryParams(config.axis_);
  std::vector<ns::IdType> rp;
  rp.reserve(static_cast<size_t>(face_count));
  for (int i = 0; i < face_count; ++i) {
    rp.push_back(static_cast<ns::IdType>(faces[i]));
  }
  const auto sym =
      static_cast<uint8_t>(symmetry & (ns::FilterConfig::kSymP | ns::FilterConfig::kSymB | ns::FilterConfig::kSymD));
  std::vector<std::vector<ns::IdType>> members;
  for (auto& m : ns::ExpandRaypathByPeriod(rp, sym, d.sigma_a, d.d_applicable, gating.p_applicable, gating.b_applicable,
                                           ns::kHexagonalFnPeriod, gating.geom)) {
    if (std::find(members.begin(), members.end(), m) == members.end()) {
      members.push_back(std::move(m));
    }
  }
  // ExpandRaypathByPeriod yields at most 6 x 2 x 2 sequences; the cap is a guard, not a truncation.
  const size_t n = std::min(members.size(), static_cast<size_t>(LUMICE_MAX_RAYPATH_CLASS_MEMBERS));
  for (size_t k = 0; k < n; ++k) {
    for (int i = 0; i < face_count; ++i) {
      out_faces[k * static_cast<size_t>(face_count) + static_cast<size_t>(i)] = static_cast<int>(members[k][i]);
    }
  }
  *out_member_count = static_cast<int>(n);
  return LUMICE_OK;
}

// =============== Raypath Validation ===============
LUMICE_ErrorCode LUMICE_ValidateRaypathText(const char* text, LUMICE_CrystalKind kind,
                                            LUMICE_RaypathValidationState* out_state, char* out_msg,
                                            size_t msg_buf_size) {
  if (!text || !out_state || !out_msg) {
    return LUMICE_ERR_NULL_ARG;
  }
  // Two-value enum: extend to switch+assert when CrystalKind expands.
  auto core_kind = (kind == LUMICE_CRYSTAL_PRISM) ? ns::CrystalKind::kPrism : ns::CrystalKind::kPyramid;
  auto r = ns::ValidateRaypathText(std::string(text), core_kind);
  switch (r.state) {
    case ns::RaypathValidation::kValid:
      *out_state = LUMICE_RAYPATH_VALID;
      break;
    case ns::RaypathValidation::kIncomplete:
      *out_state = LUMICE_RAYPATH_INCOMPLETE;
      break;
    case ns::RaypathValidation::kInvalid:
      *out_state = LUMICE_RAYPATH_INVALID;
      break;
    default:
      assert(false);
      *out_state = LUMICE_RAYPATH_INVALID;
      return LUMICE_ERR_UNKNOWN;
  }
  if (msg_buf_size > 0) {
    std::snprintf(out_msg, msg_buf_size, "%s", r.message.c_str());
  }
  return LUMICE_OK;
}

LUMICE_ErrorCode LUMICE_ConvertMillerIndexToWedgeAngle(int h, int k, int l, int provided_count,
                                                       LUMICE_MillerConversionState* out_state, float* out_angle_deg,
                                                       int* out_invalid_index) {
  if (!out_state || !out_angle_deg) {
    return LUMICE_ERR_NULL_ARG;
  }
  auto r = ns::ConvertMillerIndexToWedgeAngle(h, k, l, provided_count);
  switch (r.state) {
    case ns::MillerConversionState::kValid:
      *out_state = LUMICE_MILLER_VALID;
      break;
    case ns::MillerConversionState::kNoCone:
      *out_state = LUMICE_MILLER_NO_CONE;
      break;
    case ns::MillerConversionState::kIncomplete:
      *out_state = LUMICE_MILLER_INCOMPLETE;
      break;
    case ns::MillerConversionState::kInvalid:
      *out_state = LUMICE_MILLER_INVALID;
      break;
    default:
      assert(false);
      *out_state = LUMICE_MILLER_INVALID;
      return LUMICE_ERR_UNKNOWN;
  }
  *out_angle_deg = r.wedge_angle_deg;
  if (out_invalid_index) {
    *out_invalid_index = r.invalid_index;
  }
  return LUMICE_OK;
}
