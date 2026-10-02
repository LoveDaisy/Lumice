#include "raypath/path_feature_report_json.hpp"

#include <cmath>
#include <nlohmann/json.hpp>

namespace lumice::raypath {

namespace {

nlohmann::ordered_json Num(double value) {
  return std::isfinite(value) ? nlohmann::ordered_json(value) : nlohmann::ordered_json(nullptr);
}

nlohmann::ordered_json Array(const double* values, int count) {
  nlohmann::ordered_json out = nlohmann::ordered_json::array();
  for (int i = 0; i < count; i++) {
    out.push_back(Num(values[i]));
  }
  return out;
}

const char* DistributionName(DistributionType type) {
  switch (type) {
    case DistributionType::kNoRandom:
      return "fixed";
    case DistributionType::kUniform:
      return "uniform";
    case DistributionType::kGaussian:
      return "gauss";
    case DistributionType::kZigzag:
      return "zigzag";
    case DistributionType::kLaplacian:
      return "laplacian";
    case DistributionType::kGaussianLegacy:
      return "gauss_legacy";
  }
  return "unknown";
}

nlohmann::ordered_json WavelengthJson(const ReportWavelength& wavelength) {
  return {
    { "nm", Num(wavelength.wavelength_nm) },
    { "weight", Num(wavelength.weight) },
    { "refractive_index", Num(wavelength.refractive_index) },
  };
}

nlohmann::ordered_json BrightnessJson(const BrightnessEstimate& brightness) {
  nlohmann::ordered_json out = {
    { "status", CoverageStatusName(brightness.status) },
    { "measure", brightness.measure },
    { "coarse_sample_count", brightness.coarse_sample_count },
    { "fine_sample_count", brightness.fine_sample_count },
    { "fine_valid_count", brightness.fine_valid_count },
    { "fine_positive_count", brightness.fine_positive_count },
    { "coarse_mean_A_times_T", Num(brightness.coarse_mean_at) },
    { "fine_mean_A_times_T", Num(brightness.fine_mean_at) },
    { "absolute_difference", Num(brightness.absolute_difference) },
    { "weighted_mean_A_times_T", Num(brightness.weighted_mean_at) },
  };
  if (!brightness.reason.empty()) {
    out["reason"] = brightness.reason;
  }
  if (brightness.has_fixed_direction) {
    out["fixed_outgoing_direction"] = Array(brightness.fixed_direction, 3);
    out["direction_residual_max_rad"] = Num(brightness.direction_residual_max);
  }
  return out;
}

nlohmann::ordered_json PositionJson(const FeaturePosition& position) {
  nlohmann::ordered_json out = {
    { "wavelength_nm", Num(position.wavelength_nm) },
    { "refractive_index", Num(position.refractive_index) },
  };
  if (position.deviation_deg.has_value()) {
    out["deviation_deg"] = Num(*position.deviation_deg);
  }
  if (position.altitude_deg.has_value()) {
    out["altitude_deg"] = Num(*position.altitude_deg);
  }
  if (position.azimuth_deg.has_value()) {
    out["azimuth_deg"] = Num(*position.azimuth_deg);
  }
  if (position.relative_solar_azimuth_deg.has_value()) {
    out["relative_solar_azimuth_deg"] = Num(*position.relative_solar_azimuth_deg);
  }
  if (position.spherical_separation_deg.has_value()) {
    out["spherical_separation_deg"] = Num(*position.spherical_separation_deg);
  }
  return out;
}

nlohmann::ordered_json FeatureJson(const PathFeature& feature) {
  nlohmann::ordered_json positions = nlohmann::ordered_json::array();
  for (const FeaturePosition& position : feature.positions) {
    positions.push_back(PositionJson(position));
  }
  nlohmann::ordered_json metrics = nlohmann::ordered_json::object();
  for (const FeatureMetric& metric : feature.metrics) {
    metrics[metric.name] = Num(metric.value);
  }
  nlohmann::ordered_json out = {
    { "id", feature.id },
    { "kind", feature.kind },
    { "evidence_status", feature.evidence_status },
    { "mechanism", feature.mechanism },
    { "location", feature.location },
    { "interpretation", feature.interpretation },
    { "positions", positions },
    { "metrics", metrics },
  };
  if (feature.visible.has_value()) {
    out["visible"] = *feature.visible;
  }
  return out;
}

}  // namespace

std::string PathFeatureReportToJson(const PathFeatureReport& result, const char* lumice_version) {
  nlohmann::ordered_json shape = nlohmann::ordered_json::array();
  for (const NominalShapeScalar& scalar : result.meta.shape) {
    shape.push_back({ { "name", scalar.name },
                      { "value", Num(scalar.value) },
                      { "distribution", DistributionName(scalar.distribution) },
                      { "spread", Num(scalar.spread) } });
  }
  nlohmann::ordered_json wavelengths = nlohmann::ordered_json::array();
  for (const ReportWavelength& wavelength : result.wavelengths) {
    wavelengths.push_back(WavelengthJson(wavelength));
  }
  nlohmann::ordered_json members = nlohmann::ordered_json::array();
  for (const PhysicalMemberReport& member : result.members) {
    nlohmann::ordered_json member_wavelengths = nlohmann::ordered_json::array();
    for (const MemberWavelengthReport& row : member.wavelengths) {
      member_wavelengths.push_back(
          { { "wavelength", WavelengthJson(row.wavelength) }, { "brightness", BrightnessJson(row.brightness) } });
    }
    members.push_back({ { "faces", member.faces }, { "wavelengths", member_wavelengths } });
  }
  nlohmann::ordered_json features = nlohmann::ordered_json::array();
  for (const PathFeature& feature : result.features) {
    features.push_back(FeatureJson(feature));
  }
  nlohmann::ordered_json coverage = nlohmann::ordered_json::array();
  for (const CoverageItem& item : result.coverage) {
    coverage.push_back(
        { { "subject", item.subject }, { "status", CoverageStatusName(item.status) }, { "reason", item.reason } });
  }
  nlohmann::ordered_json document = {
    { "schema", "lumice.path-feature-report" },
    { "schema_version", result.meta.schema_version },
    { "generator", { { "lumice", lumice_version }, { "analytic_api_version", result.meta.analytic_api_version } } },
    { "conventions",
      { { "member_semantics",
          "physical L2 expansion under the configured shape and orientation ensemble; never an L1/PBD label orbit" },
        { "brightness", "mean finite-crystal A*T in LI's a=1 area normalisation, under the named orientation measure" },
        { "directions",
          "world propagation directions; a sky point is altitude asin(-z), with azimuth measured as the sun's" },
        { "coverage",
          "unsupported, unresolved, not detected at a stated resolution and physically unreachable are distinct "
          "states" } } },
    { "meta",
      { { "crystal",
          { { "id", result.meta.crystal_id },
            { "kind", result.meta.crystal_kind },
            { "shape", shape },
            { "shape_is_nominal", result.meta.shape_is_nominal } } },
        { "requested_faces", result.meta.requested_faces },
        { "sun",
          { { "altitude_deg", Num(result.meta.sun_altitude_deg) },
            { "azimuth_deg", Num(result.meta.sun_azimuth_deg) },
            { "incident_direction", Array(result.meta.incident_direction, 3) } } },
        { "orientation_measure", result.meta.orientation_measure },
        { "sample_count", result.meta.sample_count } } },
    { "wavelengths", wavelengths },
    { "physical_l2_members", members },
    { "features", features },
    { "coverage", coverage },
    { "limitations", result.limitations },
  };
  return document.dump();
}

}  // namespace lumice::raypath
