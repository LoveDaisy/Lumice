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

namespace {
using Json = nlohmann::ordered_json;

const char* EvidenceName(DiagnosticEvidence evidence) {
  switch (evidence) {
    case DiagnosticEvidence::kActual:
      return "actual";
    case DiagnosticEvidence::kCandidate:
      return "candidate";
    case DiagnosticEvidence::kUnfinished:
      return "unfinished";
  }
  return "unfinished";
}
const char* GeometryName(DiagnosticGeometry geometry) {
  switch (geometry) {
    case DiagnosticGeometry::kPoint:
      return "point";
    case DiagnosticGeometry::kPolyline:
      return "polyline";
    case DiagnosticGeometry::kAtom:
      return "atom";
    case DiagnosticGeometry::kSourceRange:
      return "source_event_range";
  }
  return "point";
}
const char* SolveName(analytic::InterfaceSolveStatus status) {
  switch (status) {
    case analytic::InterfaceSolveStatus::kConverged:
      return "converged";
    case analytic::InterfaceSolveStatus::kInvalidInput:
      return "invalid_input";
    case analytic::InterfaceSolveStatus::kUnavailable:
      return "unavailable";
    case analytic::InterfaceSolveStatus::kNoSupport:
      return "no_support_at_iterate";
    case analytic::InterfaceSolveStatus::kDegenerate:
      return "degenerate";
    case analytic::InterfaceSolveStatus::kIterationLimit:
      return "iteration_limit";
    case analytic::InterfaceSolveStatus::kBudgetExceeded:
      return "budget_exceeded";
  }
  return "invalid_input";
}
Json SourceJson(const analytic::DiagnosticInputRow& row) {
  return { { "pose", row.pose },
           { "incident", row.incident },
           { "refractive_index", Num(row.refractive_index) },
           { "source_token", row.source_token },
           { "shape",
             { { "kind", static_cast<int>(row.crystal.kind) },
               { "height", Num(row.crystal.height) },
               { "face_distances", Array(row.crystal.face_distance, 6) },
               { "upper_h", Num(row.crystal.upper_h) },
               { "lower_h", Num(row.crystal.lower_h) },
               { "upper_wedge_deg", Num(row.crystal.upper_wedge_deg) },
               { "lower_wedge_deg", Num(row.crystal.lower_wedge_deg) } } } };
}
Json OpticalJson(const analytic::DiagnosticOutputRow& row) {
  Json result = { { "input_status", static_cast<int>(row.input_status) },
                  { "path_valid", row.path_valid },
                  { "optical_failure", static_cast<int>(row.optical_failure) },
                  { "path_evaluations", row.path_evaluations } };
  if (row.path_valid) {
    result["outgoing"] = row.outgoing;
    result["interface_product"] = Num(row.interface_product);
  }
  if (row.entry_available) {
    result["entry"] = { { "status", static_cast<int>(row.entry.status) },
                        { "area", Num(row.entry.value) },
                        { "raw_area", Num(row.corridor.raw_area) },
                        { "area_threshold", Num(row.corridor.area_threshold) },
                        { "geometry_evaluated", row.corridor.geometry_evaluated } };
    if (row.corridor.geometry_evaluated) {
      Json edges = Json::array();
      for (const auto& edge : row.corridor.edge_sources) {
        edges.push_back({ { "slot", edge.path_index }, { "original_edge", edge.edge_index } });
      }
      result["entry"]["edge_lineage"] = edges;
      result["entry"]["projection_basis"] = row.corridor.projection_basis;
      result["entry"]["vertices"] = row.corridor.vertices;
    }
  }
  result["interfaces"] = Json::array();
  for (size_t slot = 0; slot < row.interfaces.size(); ++slot) {
    const auto& face = row.interfaces[slot];
    Json item = { { "slot", slot }, { "reached", face.reached } };
    if (face.reached) {
      item["incidence"] = Num(face.incidence);
      item["discriminant"] = Num(face.discriminant);
      if (face.factor_available) {
        item["factor"] = Num(face.factor);
      }
      if (face.pose_gradient_available) {
        item["discriminant_body_rotation_gradient"] = face.discriminant_pose_gradient;
      }
      if (face.index_derivative_available) {
        item["discriminant_index_derivative"] = Num(face.discriminant_index_derivative);
        item["index_derivative_error"] = Num(face.index_derivative_error);
      }
    }
    result["interfaces"].push_back(std::move(item));
  }
  return result;
}
Json InterfacePointJson(const analytic::InterfaceStationaryPoint& point) {
  return { { "status", SolveName(point.status) },
           { "source", SourceJson(point.source) },
           { "value", OpticalJson(point.value) },
           { "travelled_rad", Num(point.travelled_rad) } };
}
Json BracketJson(const analytic::InterfaceEventBracket& bracket) {
  return { { "predicate", bracket.kind == analytic::InterfaceWalkStop::kGeometricContact ?
                              "raw_area > 0" :
                              "raw_area > area_threshold" },
           { "positive", InterfacePointJson(bracket.positive) },
           { "nonpositive", InterfacePointJson(bracket.nonpositive) },
           { "source_width_rad", Num(bracket.source_width_rad) } };
}
Json JetJson(const analytic::SphericalJet& jet) {
  return { { "value", Num(jet.value) }, { "gradient", jet.gradient }, { "hessian_00_01_11", jet.hessian } };
}
Json FieldPointJson(const analytic::FieldStationaryPoint& point) {
  Json result = { { "status", static_cast<int>(point.status) },
                  { "direction", point.query.direction },
                  { "tangent_basis", point.query.basis },
                  { "bandwidth_rad", Num(point.query.bandwidth_rad) },
                  { "correction_rad", Num(point.correction_rad) },
                  { "log_y_curvatures", point.log_y_curvatures },
                  { "effective_samples_y", Num(point.field.effective_samples_y) },
                  { "outer_sample_count", point.field.outer_sample_count } };
  result["xyz"] = Json::array();
  for (const auto& jet : point.field.xyz) {
    result["xyz"].push_back(JetJson(jet));
  }
  if (point.field.chromaticity_available) {
    result["xy"] = Json::array();
    for (const auto& jet : point.field.xy) {
      result["xy"].push_back(JetJson(jet));
    }
  }
  return result;
}
}  // namespace

std::string ProductPathReportToJson(const ProductPathReport& result, const char* lumice_version) {
  Json document = {
    { "schema", "lumice.path-feature-report" },
    { "schema_version", 2 },
    { "internal_provisional", true },
    { "generator", { { "lumice", lumice_version }, { "analytic_api_version", analytic::kApiVersion } } },
    { "scope",
      { { "scene_identity", result.snapshot.scene_identity },
        { "spectrum", result.spectrum_scope },
        { "seed", result.seed },
        { "member_semantics", "physical_L2" } } },
    { "conventions",
      { { "sky", "unit world viewing direction, negative propagation" },
        { "pose", "row-major body-to-world SO(3)" },
        { "angles", "radians" },
        { "field", "normalized vMF per steradian; tangent gradients per radian and Hessians per radian squared" },
        { "weight", "product 2A/S times all interface factors times source spectral XYZ coefficient once" },
        { "uncertainty",
          "fixed-observation prefix movement, solver correction and scale response are different quantities; none is a "
          "global error certificate" } } }
  };
  document["scope"]["light"] = nlohmann::json(result.snapshot.light);
  document["scope"]["layers"] = Json::array();
  for (const auto& layer : result.snapshot.layers) {
    document["scope"]["layers"].push_back({ { "scene_layer", layer.layer_index },
                                            { "crystal", nlohmann::json(layer.crystal) },
                                            { "representative_faces", layer.representative },
                                            { "symmetry_bits", layer.symmetry_bits } });
  }
  document["physical_members"] = result.representative_input.layers[0].scope.members;
  document["spectrum"] = Json::array();
  for (const auto& row : result.representative_input.spectrum.rows) {
    document["spectrum"].push_back({ { "nm", row.wavelength_nm },
                                     { "source_weight", Num(row.source_weight) },
                                     { "measure_mass", Num(row.measure_mass) },
                                     { "index", Num(row.refractive_index) },
                                     { "XYZ_coefficient", row.coefficient },
                                     { "provenance", row.provenance } });
  }
  const auto& discovery = result.discovery;
  document["budget"] = {
    { "requested_outer_samples", result.options.sampling.requested_samples },
    { "completed_outer_samples", discovery.measure.completed_samples },
    { "max_optical_evaluations", result.options.sampling.max_optical_evaluations },
    { "optical_evaluations",
      discovery.measure.optical_evaluations + discovery.event_path_evaluations + result.spectral_optical_evaluations },
    { "max_field_component_evaluations", result.options.max_field_evaluations },
    { "field_component_evaluations", discovery.field_component_evaluations + result.spectral_field_evaluations },
    { "exhausted", discovery.budget_exhausted }
  };
  document["timing_seconds"] = { { "spectral_verification", Num(result.spectral_seconds) },
                                 { "capture", Num(result.capture_seconds) },
                                 { "assembly", Num(discovery.assembly_seconds) },
                                 { "source_events_and_edge_observations", Num(discovery.event_seconds) },
                                 { "field_discovery", Num(discovery.field_seconds) } };
  document["observation"] = { { "kernel", "normalized_vMF" },
                              { "bandwidth_rad", Num(result.options.bandwidth_rad) },
                              { "location_resolution_rad", Num(result.options.location_resolution_rad) },
                              { "search_scope",
                                "bounded source-seeded local structures; not exhaustive all-sky or source topology" } };
  document["spectral_verification"] = {
    { "optical_evaluations", result.spectral_optical_evaluations },
    { "field_component_evaluations", result.spectral_field_evaluations },
    { "movement_rad", result.spectral_movement_rad ? Num(*result.spectral_movement_rad) : Json(nullptr) }
  };
  document["features"] = Json::array();
  document["sources"] = Json::object();
  for (size_t index = 0; index < discovery.features.size(); ++index) {
    const auto& feature = discovery.features[index];
    Json item = {
      { "id", index },
      { "kind", feature.kind },
      { "evidence", EvidenceName(feature.evidence) },
      { "reason", feature.reason },
      { "geometry", { { "kind", GeometryName(feature.geometry) }, { "sky_points", feature.sky_points } } },
      { "fixed_observation",
        { { "equation", static_cast<int>(feature.equation) },
          { "level", Num(feature.level) },
          { "bandwidth_rad", Num(feature.bandwidth_rad) },
          { "prefix_movement_rad", Num(feature.prefix_movement_rad) },
          { "contrast", Num(feature.transverse_contrast) },
          { "contrast_prefix_error", Num(feature.observation_contrast_error) },
          { "minimum_ESS", Num(feature.minimum_effective_samples) } } },
      { "scale_response",
        { { "status", static_cast<int>(feature.scale_status) },
          { "movement_rad", feature.scale_movement_rad ? Num(*feature.scale_movement_rad) : Json(nullptr) } } }
    };
    if (!feature.field_points.empty()) {
      item["field_points"] = Json::array();
      for (const auto& point : feature.field_points) {
        item["field_points"].push_back(FieldPointJson(point));
      }
      item["walk_stop"] = static_cast<int>(feature.walk_stop);
    }
    if (feature.source_token) {
      item["source_token"] = *feature.source_token;
      const auto& source = discovery.measure.sources[*feature.source_token];
      document["sources"][std::to_string(*feature.source_token)] = { { "outer_sample", source.sample_index },
                                                                     { "member", source.member_index },
                                                                     { "spectral_row", source.spectral_row } };
    } else {
      item["source_scope"] =
          "aggregate over the recorded physical-member/spectrum/product measure; no unique cause asserted";
    }
    if (feature.source_connected_feature) {
      item["source_connected_feature"] = *feature.source_connected_feature;
    }
    if (feature.source_event) {
      item["source_event"] = BracketJson(*feature.source_event);
    }
    if (feature.interface_event) {
      item["internal_slot"] = feature.internal_slot;
      item["interface_event"] = InterfacePointJson(*feature.interface_event);
      item["source_curves"] = Json::array();
      for (const auto& curve : feature.interface_curves) {
        Json points = Json::array();
        for (const auto& point : curve.points) {
          points.push_back(InterfacePointJson(point));
        }
        Json events = Json::array();
        for (const auto& event : curve.events) {
          events.push_back(BracketJson(event));
        }
        item["source_curves"].push_back(
            { { "points", points }, { "events", events }, { "termination", static_cast<int>(curve.stop) } });
      }
    }
    if (feature.deviation_minimum) {
      const auto& minimum = *feature.deviation_minimum;
      item["physical_position"] = {
        { "deviation_rad", Num(minimum.deviation_rad) },   { "status", SolveName(minimum.status) },
        { "source", SourceJson(minimum.source) },          { "value", OpticalJson(minimum.value) },
        { "correction_rad", Num(minimum.correction_rad) }, { "hessian_error", Num(minimum.hessian_error) },
        { "curvatures", minimum.objective_curvatures }
      };
    }
    if (feature.orbit_axis) {
      item["source_orbit"] = { { "world_axis", *feature.orbit_axis },
                               { "begin_rad", Num(feature.orbit_begin_rad) },
                               { "end_rad", Num(feature.orbit_end_rad) },
                               { "action", "left multiply source pose by world-axis rotation" } };
    }
    if (feature.geometry == DiagnosticGeometry::kAtom) {
      item["XYZ_mass"] = feature.atom_xyz_mass;
    }
    document["features"].push_back(std::move(item));
  }
  document["unfinished"] = discovery.unfinished;
  document["outcome"] = discovery.budget_exhausted || !discovery.unfinished.empty() ? "unfinished" : "completed";
  return document.dump();
}

}  // namespace lumice::raypath
