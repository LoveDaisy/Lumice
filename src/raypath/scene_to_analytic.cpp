#include "raypath/scene_to_analytic.hpp"

#include <cmath>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

#include "core/optics.hpp"
#include "util/sky_direction.hpp"

namespace lumice::raypath {

namespace {

// `slot` names the scalar through the config's own key table (crystal_config.hpp
// ShapeScalarSyncKeyName), so the metadata spells each field as the config file does.
NominalShapeScalar Nominal(CrystalKind kind, int slot, const Distribution& d) {
  const char* key = ShapeScalarSyncKeyName(kind, slot);
  std::string name = key != nullptr ? key : "?";
  if (slot >= kShapeScalarFace0) {
    name += "[" + std::to_string(slot - kShapeScalarFace0) + "]";
  }
  NominalShapeScalar s;
  s.name = std::move(name);
  // The centre slot is the distribution's own nominal value for every type (math.hpp Distribution:
  // Value / UniformCenter / Mean / Location all read `center`); the typed accessors assert on the
  // type, so the slot is read directly.
  s.value = d.center;
  s.distribution = d.type;
  s.spread = d.type == DistributionType::kNoRandom ? 0.0 : d.spread;
  return s;
}

void AppendFaceDistances(CrystalKind kind, const Distribution (&d)[6], CrystalConversion* out) {
  for (int i = 0; i < 6; i++) {
    out->scalars.push_back(Nominal(kind, kShapeScalarFace0 + i, d[i]));
    out->shape.face_distance[i] = d[i].center;
  }
}

}  // namespace

CrystalConversion ConvertCrystal(const CrystalParam& param) {
  CrystalConversion out;
  std::visit(
      [&out](const auto& p) {
        using T = std::decay_t<decltype(p)>;
        if constexpr (std::is_same_v<T, PrismCrystalParam>) {
          out.kind = "prism";
          out.shape.kind = analytic::CrystalShapeKind::kPrism;
          out.scalars.push_back(Nominal(CrystalKind::kPrism, kShapeScalarHeight, p.h_));
          out.shape.height = p.h_.center;
          AppendFaceDistances(CrystalKind::kPrism, p.d_, &out);
        } else {
          out.kind = "pyramid";
          out.shape.kind = analytic::CrystalShapeKind::kPyramid;
          // The kernel's `height` is the prism band (simulator.cpp CrystalMaker: CreatePyramid(wedge_u,
          // wedge_l, h_pyr_u, h_prs, h_pyr_l, d)).
          out.scalars.push_back(Nominal(CrystalKind::kPyramid, kShapeScalarPrismH, p.h_prs_));
          out.scalars.push_back(Nominal(CrystalKind::kPyramid, kShapeScalarUpperH, p.h_pyr_u_));
          out.scalars.push_back(Nominal(CrystalKind::kPyramid, kShapeScalarLowerH, p.h_pyr_l_));
          out.shape.height = p.h_prs_.center;
          out.shape.upper_h = p.h_pyr_u_.center;
          out.shape.lower_h = p.h_pyr_l_.center;
          out.shape.upper_wedge_deg = p.wedge_angle_u_;
          out.shape.lower_wedge_deg = p.wedge_angle_l_;
          out.upper_wedge_deg = p.wedge_angle_u_;
          out.lower_wedge_deg = p.wedge_angle_l_;
          AppendFaceDistances(CrystalKind::kPyramid, p.d_, &out);
        }
      },
      param);
  for (const NominalShapeScalar& s : out.scalars) {
    if (s.spread != 0.0) {
      out.shape_is_nominal = true;
    }
  }
  return out;
}

Error ResolveWavelength(const LightSourceConfig& light, std::optional<double> user_nm, WavelengthChoice* out) {
  *out = WavelengthChoice{};
  if (user_nm.has_value()) {
    out->wavelength_nm = *user_nm;
    out->source = WavelengthSource::kUser;
  } else if (const auto* wls = std::get_if<std::vector<WlParam>>(&light.spectrum_);
             wls != nullptr && wls->size() == 1) {
    out->wavelength_nm = (*wls)[0].wl_;
    out->source = WavelengthSource::kConfigSingle;
  } else {
    out->wavelength_nm = kDefaultWavelengthNm;
    out->source = WavelengthSource::kDefault;
  }
  // IceRefractiveIndex::Get answers 1.0 outside its table instead of failing: ice of index 1.
  if (!std::isfinite(out->wavelength_nm) || out->wavelength_nm < IceRefractiveIndex::kMinWaveLength ||
      out->wavelength_nm > IceRefractiveIndex::kMaxWaveLength) {
    return { ErrorCode::kWavelengthOutOfRange,
             "wavelength " + std::to_string(out->wavelength_nm) + " nm is outside the ice refractive-index table [" +
                 std::to_string(static_cast<int>(IceRefractiveIndex::kMinWaveLength)) + ", " +
                 std::to_string(static_cast<int>(IceRefractiveIndex::kMaxWaveLength)) + "] nm" };
  }
  out->refractive_index = IceRefractiveIndex::Get(out->wavelength_nm);
  return {};
}

void SunIncidentDirection(const SunParam& sun, double out[3]) {
  // SampleRayDir centres its cap at (lon = azimuth + 180, lat = -altitude), which is exactly the
  // propagation direction AltAzToDir gives for the sun's own sky point.
  AltAzToDir(static_cast<double>(sun.altitude_), static_cast<double>(sun.azimuth_), out);
}

Error ResolveSingleLayerPath(const std::vector<std::vector<int>>& layers, const analytic::FaceNormalTable& table,
                             std::vector<int>* slots) {
  slots->clear();
  if (layers.empty()) {
    return { ErrorCode::kInvalidPath, "no raypath given" };
  }
  if (layers.size() > 1) {
    return { ErrorCode::kMultiLayerUnsupported, "the raypath spans " + std::to_string(layers.size()) +
                                                    " scattering layers; only single-layer raypaths can be analysed" };
  }
  const std::vector<int>& faces = layers.front();
  if (faces.size() < 2 || faces.size() > static_cast<size_t>(analytic::kMaxFaceCount)) {
    return { ErrorCode::kInvalidPath, "a raypath has 2 to " + std::to_string(analytic::kMaxFaceCount) + " faces; got " +
                                          std::to_string(faces.size()) };
  }
  for (int fn : faces) {
    const int slot = table.SlotOf(fn);
    if (slot < 0) {
      slots->clear();
      return { ErrorCode::kFaceNotInCrystal, "face " + std::to_string(fn) + " is not a face of this crystal" };
    }
    slots->push_back(slot);
  }
  return {};
}

}  // namespace lumice::raypath
