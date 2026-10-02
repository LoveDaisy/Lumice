#include "raypath/path_feature_report_json.hpp"

#include <algorithm>
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

const char* DiagnosticPathStatusName(analytic::DiagnosticPathStatus status) {
  switch (status) {
    case analytic::DiagnosticPathStatus::kOk:
      return "ok";
    case analytic::DiagnosticPathStatus::kPathInfeasible:
      return "path_infeasible";
    case analytic::DiagnosticPathStatus::kRefractionCritical:
      return "refraction_critical";
    case analytic::DiagnosticPathStatus::kNonFinite:
      return "non_finite";
  }
  return "non_finite";
}

const char* DiagnosticEntryStatusName(analytic::DiagnosticEntryStatus status) {
  switch (status) {
    case analytic::DiagnosticEntryStatus::kNotEvaluated:
      return "not_evaluated";
    case analytic::DiagnosticEntryStatus::kOk:
      return "ok";
    case analytic::DiagnosticEntryStatus::kEntryBackface:
      return "entry_backface";
    case analytic::DiagnosticEntryStatus::kExitCriticalAngle:
      return "exit_critical_angle";
    case analytic::DiagnosticEntryStatus::kCorridorEmpty:
      return "corridor_empty";
  }
  return "not_evaluated";
}

nlohmann::ordered_json SceneMeasureJson(const SceneMeasureResult& measure) {
  nlohmann::ordered_json factors = nlohmann::ordered_json::array();
  for (const MeasureFactorDescriptor& factor : measure.factors) {
    factors.push_back({ { "layer_index", factor.layer_index },
                        { "name", factor.name },
                        { "distribution", DistributionName(factor.distribution) },
                        { "latent_id", factor.latent_id },
                        { "support_dimension", factor.support_dimension },
                        { "center", Num(factor.center) },
                        { "spread", Num(factor.spread) },
                        { "parameterization", factor.parameterization },
                        { "measure", factor.measure },
                        { "normalization", factor.normalization } });
  }
  nlohmann::ordered_json spectrum = nlohmann::ordered_json::array();
  for (const SpectrumMeasureNode& node : measure.spectrum_nodes) {
    spectrum.push_back({ { "node_id", node.node_id },
                         { "source", node.source },
                         { "wavelength_nm", Num(node.wavelength_nm) },
                         { "weight", Num(node.weight) },
                         { "refractive_index", Num(node.refractive_index) } });
  }
  nlohmann::ordered_json sun = nlohmann::ordered_json::array();
  for (const SunMeasureNode& node : measure.sun_nodes) {
    sun.push_back({ { "node_id", node.node_id },
                    { "mass", Num(node.mass) },
                    { "incident_direction", Array(node.incident_direction, 3) } });
  }
  constexpr size_t kMaxJsonRows = 64;
  nlohmann::ordered_json rows = nlohmann::ordered_json::array();
  for (size_t row_index = 0; row_index < std::min(kMaxJsonRows, measure.rows.size()); row_index++) {
    const SceneMeasureRow& row = measure.rows[row_index];
    nlohmann::ordered_json layers = nlohmann::ordered_json::array();
    for (const SceneMeasureLayerRow& layer : row.layers) {
      nlohmann::ordered_json shape = nlohmann::ordered_json::array();
      for (const ShapeScalarSample& scalar : layer.shape) {
        shape.push_back({ { "name", scalar.name },
                          { "value", Num(scalar.value) },
                          { "latent_id", scalar.latent_id },
                          { "sync_group", scalar.sync_group },
                          { "leader_slot", scalar.leader_slot },
                          { "raw_value", Num(scalar.raw_value) },
                          { "absolute_value_fold", scalar.absolute_value_fold },
                          { "mapping_jacobian", Num(scalar.mapping_jacobian) } });
      }
      nlohmann::ordered_json interfaces = nlohmann::ordered_json::array();
      for (const analytic::DiagnosticInterface& interface : layer.field.interfaces) {
        interfaces.push_back({ { "face", interface.face_number }, { "coefficient", Num(interface.coefficient) } });
      }
      nlohmann::ordered_json domain_margins = nlohmann::ordered_json::array();
      for (const analytic::DiagnosticMargin& margin : layer.field.domain_margins) {
        domain_margins.push_back(
            { { "name", margin.name }, { "interface_index", margin.interface_index }, { "value", Num(margin.value) } });
      }
      nlohmann::ordered_json tir_margins = nlohmann::ordered_json::array();
      for (const analytic::DiagnosticMargin& margin : layer.field.tir_margins) {
        tir_margins.push_back(
            { { "name", margin.name }, { "interface_index", margin.interface_index }, { "value", Num(margin.value) } });
      }
      nlohmann::ordered_json layer_json = {
        { "layer_index", layer.layer_index },
        { "source_sun_node_id", layer.source_sun_node_id },
        { "source_spectrum_node_id", layer.source_spectrum_node_id },
        { "source_wavelength_nm", Num(layer.source_wavelength_nm) },
        { "crystal_id", layer.crystal_id },
        { "crystal_kind", layer.crystal_kind },
        { "faces", layer.faces },
        { "shape", shape },
        { "pose_lon_lat_roll_rad", Array(layer.pose_lon_lat_roll_rad, 3) },
        { "pose_support_rank", layer.pose_support_rank },
        { "pose_tangent_drotation", Array(layer.pose_tangent_drotation, 27) },
        { "incident_direction", Array(layer.incident_direction, 3) },
        { "outgoing_direction", Array(layer.outgoing_direction, 3) },
        { "crystal_share", Num(layer.crystal_share) },
        { "continuation_mass", Num(layer.continuation_mass) },
        { "total_surface_area", Num(layer.total_surface_area) },
        { "entry_measure", Num(layer.entry_measure) },
        { "normalized_entry_factor", Num(layer.normalized_entry_factor) },
        { "fresnel_weight", Num(layer.fresnel_weight) },
        { "status", SceneMeasureStatusName(layer.status) },
        { "field",
          { { "path_status", DiagnosticPathStatusName(layer.field.path_status) },
            { "entry_status", DiagnosticEntryStatusName(layer.field.entry_status) },
            { "interfaces", interfaces },
            { "domain_margins", domain_margins },
            { "tir_margins", tir_margins } } },
      };
      if (!layer.reason.empty()) {
        layer_json["reason"] = layer.reason;
      }
      if (layer.crystal_kind == "pyramid") {
        layer_json["upper_wedge_deg"] = Num(layer.upper_wedge_deg);
        layer_json["lower_wedge_deg"] = Num(layer.lower_wedge_deg);
      }
      layers.push_back(std::move(layer_json));
    }
    nlohmann::ordered_json latents = nlohmann::ordered_json::array();
    for (const LatentMeasureSample& latent : row.latents) {
      latents.push_back({ { "latent_id", latent.latent_id },
                          { "layer_index", latent.layer_index },
                          { "name", latent.name },
                          { "base_measure", LatentBaseMeasureName(latent.base_measure) },
                          { "coordinate", Num(latent.coordinate) },
                          { "proposal_density_or_mass", Num(latent.proposal_density_or_mass) },
                          { "target_density_or_mass", Num(latent.target_density_or_mass) },
                          { "mapping_jacobian", Num(latent.mapping_jacobian) },
                          { "mapping", latent.mapping },
                          { "status", SceneMeasureStatusName(latent.status) } });
    }
    nlohmann::ordered_json row_json = {
      { "spectrum_node_id", row.spectrum_node_id },
      { "sun_node_id", row.sun_node_id },
      { "member_chain_index", row.member_chain_index },
      { "sample_index", row.sample_index },
      { "replay_seed", row.replay_seed },
      { "wavelength_nm", Num(row.wavelength_nm) },
      { "spectrum_weight", Num(row.spectrum_weight) },
      { "sun_mass", Num(row.sun_mass) },
      { "joint_sample_mass", Num(row.joint_sample_mass) },
      { "joint_proposal_density", Num(row.joint_proposal_density) },
      { "joint_proposal_density_status", SceneMeasureNumericStatusName(row.joint_proposal_density_status) },
      { "joint_target_density", Num(row.joint_target_density) },
      { "joint_target_density_status", SceneMeasureNumericStatusName(row.joint_target_density_status) },
      { "joint_log_proposal_density", Num(row.joint_log_proposal_density) },
      { "joint_log_target_density", Num(row.joint_log_target_density) },
      { "joint_importance_weight", Num(row.joint_importance_weight) },
      { "joint_importance_weight_status", SceneMeasureNumericStatusName(row.joint_importance_weight_status) },
      { "global_weight", Num(row.global_weight) },
      { "global_weight_status", SceneMeasureNumericStatusName(row.global_weight_status) },
      { "contribution", Num(row.contribution) },
      { "contribution_status", SceneMeasureNumericStatusName(row.contribution_status) },
      { "status", SceneMeasureStatusName(row.status) },
      { "evaluation_status", SceneMeasureStatusName(row.evaluation_status) },
      { "latents", latents },
      { "layers", layers },
    };
    if (!row.reason.empty()) {
      row_json["reason"] = row.reason;
    }
    if (!row.evaluation_reason.empty()) {
      row_json["evaluation_reason"] = row.evaluation_reason;
    }
    rows.push_back(std::move(row_json));
  }
  nlohmann::ordered_json out = {
    { "status", SceneMeasureStatusName(measure.status) },
    { "seed", measure.seed },
    { "requested_sample_count", measure.requested_sample_count },
    { "evaluated_row_count", measure.evaluated_row_count },
    { "stored_row_count", measure.stored_row_count },
    { "stored_row_selection", measure.stored_row_selection },
    { "units", measure.units },
    { "normalization", measure.normalization },
    { "factors", factors },
    { "spectrum_nodes", spectrum },
    { "sun_nodes", sun },
    { "member_chains", measure.member_chains },
    { "coarse_contribution", Num(measure.coarse_contribution) },
    { "coarse_contribution_status", SceneMeasureNumericStatusName(measure.coarse_contribution_status) },
    { "total_contribution", Num(measure.total_contribution) },
    { "total_contribution_status", SceneMeasureNumericStatusName(measure.total_contribution_status) },
    { "absolute_error_estimate", Num(measure.absolute_error_estimate) },
    { "absolute_error_estimate_status", SceneMeasureNumericStatusName(measure.absolute_error_estimate_status) },
    { "joint_sampling_error_estimate", Num(measure.joint_sampling_error_estimate) },
    { "joint_sampling_error_estimate_status",
      SceneMeasureNumericStatusName(measure.joint_sampling_error_estimate_status) },
    { "sun_node_error_estimate", Num(measure.sun_node_error_estimate) },
    { "sun_node_error_estimate_status", SceneMeasureNumericStatusName(measure.sun_node_error_estimate_status) },
    { "spectrum_node_error_estimate", Num(measure.spectrum_node_error_estimate) },
    { "spectrum_node_error_estimate_status",
      SceneMeasureNumericStatusName(measure.spectrum_node_error_estimate_status) },
    { "sampled_measure_mass", Num(measure.sampled_measure_mass) },
    { "sampled_measure_mass_status", SceneMeasureNumericStatusName(measure.sampled_measure_mass_status) },
    { "status_counts",
      { { "confirmed", measure.status_counts.confirmed },
        { "zero_weight", measure.status_counts.zero_weight },
        { "physically_unreachable", measure.status_counts.physically_unreachable },
        { "numerical_incomplete", measure.status_counts.numerical_incomplete },
        { "not_supported", measure.status_counts.not_supported } } },
    { "sampled_rows", rows },
    { "sampled_rows_truncated", measure.rows_truncated || measure.rows.size() > kMaxJsonRows },
  };
  if (!measure.reason.empty()) {
    out["reason"] = measure.reason;
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
  nlohmann::ordered_json meta = nlohmann::ordered_json::object();
  meta["crystal"] = { { "id", result.meta.crystal_id },
                      { "kind", result.meta.crystal_kind },
                      { "shape", shape },
                      { "shape_is_nominal", result.meta.shape_is_nominal } };
  meta["requested_faces"] = result.meta.requested_faces;
  if (result.meta.schema_version >= 3) {
    meta["requested_path_layers"] = result.meta.requested_path_layers;
    meta["layer_crystal_ids"] = result.meta.layer_crystal_ids;
  }
  meta["sun"] = { { "altitude_deg", Num(result.meta.sun_altitude_deg) },
                  { "azimuth_deg", Num(result.meta.sun_azimuth_deg) },
                  { "incident_direction", Array(result.meta.incident_direction, 3) } };
  meta["orientation_measure"] = result.meta.orientation_measure;
  meta["sample_count"] = result.meta.sample_count;
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
    { "meta", meta },
    { "wavelengths", wavelengths },
    { "scene_measure", SceneMeasureJson(result.scene_measure) },
    { "physical_l2_members", members },
    { "features", features },
    { "coverage", coverage },
    { "limitations", result.limitations },
  };
  if (result.meta.schema_version < 3) {
    document.erase("scene_measure");
  }
  return document.dump();
}

}  // namespace lumice::raypath
