#include "raypath/product_input_assembly.hpp"

#include <cmath>

#include "core/color_util.hpp"
#include "core/optics.hpp"
#include "raypath/scene_to_analytic.hpp"
#include "util/illuminant.hpp"

namespace lumice::raypath {
namespace {

Error PhysicalSpectralRow(float wl, double source_weight, double mass, SpectrumOrigin origin,
                          const std::string& provenance, SpectralRow* out) {
  if (!std::isfinite(wl) || wl < 350.f || wl > 900.f) {
    return { ErrorCode::kWavelengthOutOfRange, "wavelength outside ice refractive-index table" };
  }
  if (!std::isfinite(source_weight) || source_weight < 0 || !std::isfinite(mass) || mass < 0) {
    return { ErrorCode::kInvalidArgument, "spectral weights must be finite and nonnegative" };
  }
  SpectralRow row;
  row.wavelength_nm = wl;
  row.source_weight = source_weight;
  row.measure_mass = mass;
  row.refractive_index = IceRefractiveIndex::Get(wl);
  row.origin = origin;
  row.provenance = provenance;
  const float unit = 1.f;
  SpectrumToXyz(wl, &unit, nullptr, row.xyz_basis.data());
  for (int j = 0; j < 3; ++j)
    row.coefficient[j] = mass * source_weight * row.xyz_basis[j];
  *out = std::move(row);
  return {};
}

bool InContinuousBand(float wl) {
  return std::isfinite(wl) && wl >= 380.f && wl < 780.f;
}
bool UnitDraw(float u) {
  return std::isfinite(u) && u >= 0.f && u <= 1.f;
}

}  // namespace

Error AssembleDiscreteSpectrum(const LightSourceConfig& light, AssembledSpectrum* out) {
  *out = {};
  const auto* discrete = std::get_if<std::vector<WlParam>>(&light.spectrum_);
  if (!discrete || discrete->empty())
    return { ErrorCode::kInvalidArgument, "a nonempty discrete spectrum is required" };
  AssembledSpectrum result;
  for (size_t i = 0; i < discrete->size(); ++i) {
    SpectralRow row;
    const auto& wl = (*discrete)[i];
    const auto error = PhysicalSpectralRow(wl.wl_, wl.weight_, 1, SpectrumOrigin::kDiscreteSum,
                                           "discrete slot " + std::to_string(i), &row);
    if (!error.Ok())
      return error;
    result.rows.push_back(std::move(row));
  }
  *out = std::move(result);
  return {};
}

Error AssembleSampledSpectrum(const LightSourceConfig& light, const ProductWavelengthSample& sample,
                              AssembledSpectrum* out) {
  *out = {};
  double weight = 0;
  if (sample.provenance.empty())
    return { ErrorCode::kInvalidArgument, "sample provenance is required" };
  if (const auto* discrete = std::get_if<std::vector<WlParam>>(&light.spectrum_)) {
    if (!sample.discrete_slot || *sample.discrete_slot >= discrete->size() ||
        (*discrete)[*sample.discrete_slot].wl_ != sample.wavelength_nm) {
      return { ErrorCode::kInvalidArgument, "sample must identify its discrete source slot" };
    }
    weight = (*discrete)[*sample.discrete_slot].weight_;
  } else {
    if (sample.discrete_slot || !InContinuousBand(sample.wavelength_nm)) {
      return { ErrorCode::kInvalidArgument, "continuous product sample must lie in [380,780)" };
    }
    weight = GetIlluminantSpd(std::get<IlluminantType>(light.spectrum_), sample.wavelength_nm);
  }
  SpectralRow row;
  const auto error =
      PhysicalSpectralRow(sample.wavelength_nm, weight, 1, SpectrumOrigin::kProductSample, sample.provenance, &row);
  if (!error.Ok())
    return error;
  out->rows.push_back(std::move(row));
  return {};
}

Error AssembleSpectrumQuadrature(const LightSourceConfig& light, const SpectrumQuadrature& quadrature,
                                 AssembledSpectrum* out) {
  *out = {};
  const auto* type = std::get_if<IlluminantType>(&light.spectrum_);
  if (!type || quadrature.nodes.empty() || quadrature.rule.empty() ||
      quadrature.evaluation_budget < quadrature.nodes.size() ||
      (quadrature.estimated_error &&
       (!std::isfinite(*quadrature.estimated_error) || *quadrature.estimated_error < 0))) {
    return { ErrorCode::kInvalidArgument, "continuous quadrature requires explicit nodes, rule and budget" };
  }
  AssembledSpectrum result;
  for (const auto& node : quadrature.nodes) {
    if (!InContinuousBand(node.wavelength_nm))
      return { ErrorCode::kInvalidArgument, "quadrature node outside [380,780)" };
    SpectralRow row;
    const auto error = PhysicalSpectralRow(node.wavelength_nm, GetIlluminantSpd(*type, node.wavelength_nm),
                                           node.probability_mass, SpectrumOrigin::kQuadrature, quadrature.rule, &row);
    if (!error.Ok())
      return error;
    result.rows.push_back(std::move(row));
  }
  result.quadrature = quadrature;
  *out = std::move(result);
  return {};
}

Error AssembleSource(const SunParam& sun, const ProductSourceSample& sample, AssembledSource* out) {
  *out = {};
  if (!std::isfinite(sun.altitude_) || !std::isfinite(sun.azimuth_) || !std::isfinite(sun.diameter_) ||
      sun.diameter_ < 0 || sun.diameter_ > 360 || !UnitDraw(sample.draw.radial_uniform) ||
      !UnitDraw(sample.draw.azimuth_uniform) || sample.provenance.empty()) {
    return { ErrorCode::kInvalidArgument, "invalid sun domain or cap draw" };
  }
  AssembledSource result;
  result.domain = sun;
  result.sample = sample;
  SunIncidentDirection(sun, result.center_direction.data());
  const auto cap =
      MakeSphericalCapTransform((sun.azimuth_ + 180.f) * math::kDegreeToRad, -sun.altitude_ * math::kDegreeToRad,
                                (sun.diameter_ / 2.f) * math::kDegreeToRad);
  result.product_direction = TransformSphericalCap(cap, sample.draw);
  double norm = 0;
  for (int i = 0; i < 3; ++i) {
    result.incident_direction[i] = result.product_direction[i];
    norm += result.incident_direction[i] * result.incident_direction[i];
  }
  for (auto& v : result.incident_direction)
    v /= std::sqrt(norm);
  *out = std::move(result);
  return {};
}

}  // namespace lumice::raypath
