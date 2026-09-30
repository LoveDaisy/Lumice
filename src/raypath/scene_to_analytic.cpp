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

namespace {

// A distribution's type as the config file spells it ("fixed" for a scalar, which the file writes
// as a bare number and the enum's JSON table does not name).
std::string DistributionTypeText(DistributionType type) {
  if (type == DistributionType::kNoRandom) {
    return "fixed";
  }
  return nlohmann::json(type).get<std::string>();
}

std::string Describe(const char* slot, const Distribution& d) {
  return std::string(slot) + " of type " + DistributionTypeText(d.type);
}

}  // namespace

PoseDensityConversion ConvertAxisToPoseDensity(const AxisDistribution& axis) {
  PoseDensityConversion out;
  if (axis.IsFullSphereUniform()) {
    out.spec.family = analytic::PoseFamily::kRandom;
    return out;
  }
  if (!axis.IsAzRotationallySymmetric()) {
    out.unsupported = "the azimuth must be uniform over 360 degrees (got " + Describe("an azimuth", axis.azimuth_dist) +
                      (axis.azimuth_dist.type == DistributionType::kUniform ?
                           " over " + std::to_string(axis.azimuth_dist.spread) + " degrees" :
                           std::string()) +
                      ")";
    return out;
  }
  const Distribution& zenith = axis.latitude_dist;
  if (zenith.type != DistributionType::kGaussian) {
    out.unsupported = "the zenith must be a Gaussian (got " + Describe("a zenith", zenith) +
                      "); a uniform zenith is expressible only as the full-sphere random axis";
    return out;
  }
  const bool roll_free = axis.IsRollRotationallySymmetric();
  if (!roll_free && axis.roll_dist.type != DistributionType::kGaussian) {
    out.unsupported = "the roll must be uniform over 360 degrees or a Gaussian (got " +
                      Describe("a roll", axis.roll_dist) +
                      (axis.roll_dist.type == DistributionType::kUniform ?
                           " over " + std::to_string(axis.roll_dist.spread) + " degrees" :
                           std::string()) +
                      ")";
    return out;
  }
  // Internal latitude -> external zenith (math.cpp to_json(AxisDistribution)).
  const double zenith_mean = 90.0 - static_cast<double>(zenith.center);
  const bool near_vertical = zenith_mean < 45.0;
  analytic::PoseDensitySpec& spec = out.spec;
  spec.zenith_mean_deg = zenith_mean;
  spec.zenith_std_deg = zenith.spread;
  if (roll_free) {
    spec.family = near_vertical ? analytic::PoseFamily::kPlate : analytic::PoseFamily::kColumn;
  } else {
    spec.family = near_vertical ? analytic::PoseFamily::kLowitz : analytic::PoseFamily::kParry;
    spec.roll_mean_deg = axis.roll_dist.center;
    spec.roll_std_deg = axis.roll_dist.spread;
  }
  const char* invalid = analytic::PoseDensityError(spec);
  if (invalid[0] != '\0') {
    out.unsupported = std::string("the axis maps to an invalid pose density: ") + invalid;
  }
  return out;
}

}  // namespace lumice::raypath
