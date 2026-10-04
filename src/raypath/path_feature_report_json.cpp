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
  for (int i = 0; i < count; ++i) {
    out.push_back(Num(values[i]));
  }
  return out;
}
}  // namespace
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
    case DiagnosticGeometry::kBand:
      return "band";
    case DiagnosticGeometry::kPoint:
      return "point";
    case DiagnosticGeometry::kPolyline:
      return "polyline";
    case DiagnosticGeometry::kAtom:
      return "atom";
    case DiagnosticGeometry::kSourceRange:
      return "point";
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
           { "travelled_rad", Num(point.travelled_rad) },
           { "accepted_source_poses", point.accepted_poses } };
}
Json BracketJson(const analytic::InterfaceEventBracket& bracket) {
  return { { "predicate", bracket.kind == analytic::InterfaceWalkStop::kGeometricContact ? "raw_area > 0" :
                          bracket.kind == analytic::InterfaceWalkStop::kOpticalGate      ? "optical_domain_valid" :
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

std::string PathFeatureReportToJson(const PathFeatureReport& result, const char* lumice_version) {
  if (result.unsupported_multicrystal) {
    return Json{
      { "schema", "lumice.path-feature-report" },
      { "schema_version", 2 },
      { "outcome", "unsupported_multicrystal" },
      { "requested_path_layers", result.requested_path_layers },
      { "actual_features", Json::array() },
      { "candidates", Json::array() },
      { "unfinished", Json::array() },
      { "coverage",
        { { { "subject", "multi-crystal chain" },
            { "status", "not_supported" },
            { "reason",
              "single-crystal internal reflections are supported, but this multi-crystal request was not "
              "evaluated" } } } },
      { "budgets", { { "optical_evaluations", 0 }, { "field_component_evaluations", 0 } } }
    }.dump();
  }

  Json document = {
    { "schema", "lumice.path-feature-report" },
    { "schema_version", 2 },

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
  const auto support = DescribeProductSupport(result.snapshot);
  document["support"] = { { "pose_coordinate_count", support.pose_coordinate_count },
                          { "pose_support_dimension", support.pose_support_dimension },
                          { "shape_parameter_dimension", support.shape_parameter_dimension },
                          { "source_direction_dimension", support.source_direction_dimension },
                          { "spectral_dimension", support.spectral_dimension },
                          { "joint_optical_rank", nullptr },
                          { "rank_scope",
                            "conditional pose Jacobians are not the joint map rank; shape parameter count is not "
                            "geometric image dimension" } };
  document["scope"]["light"] = nlohmann::json(result.snapshot.light);
  document["scope"]["layers"] = Json::array();
  for (const auto& layer : result.snapshot.layers) {
    document["scope"]["layers"].push_back(
        { { "scene_layer", result.standalone_crystal ? Json(nullptr) : Json(layer.layer_index) },
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
  document["requested_outer_samples"] = result.requested_outer_samples;
  document["budget_ms"] = result.budget_ms;
  document["budgets"] = {
    { "requested_outer_samples", result.options.sampling.requested_samples },
    { "completed_outer_samples", discovery.measure.completed_samples },
    { "replicate_samples", discovery.replicate_samples },
    { "replicate_seed", result.seed ^ 0x9e3779b9u },
    { "max_optical_evaluations", result.options.sampling.max_optical_evaluations },
    { "optical_evaluations", discovery.measure.optical_evaluations + discovery.replicate_path_evaluations +
                                 discovery.event_path_evaluations + result.spectral_optical_evaluations },
    { "max_field_component_evaluations", result.options.max_field_evaluations },
    { "field_component_evaluations", discovery.field_component_evaluations + result.spectral_field_evaluations },
    { "exhausted", discovery.budget_exhausted }
  };
  document["timing"] = { { "spectral_verification", Num(result.spectral_seconds) },
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
  document["actual_features"] = Json::array();
  document["candidates"] = Json::array();
  document["unfinished"] = Json::array();
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
          { "replicate_movement_rad",
            feature.replicate_movement_rad ? Num(*feature.replicate_movement_rad) : Json(nullptr) },
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
    if (feature.band) {
      item["band"] = {
        { "levels", feature.band->levels },
        { "status", static_cast<int>(feature.band->status) },
        { "definition",
          "same-field xy level interval within the recorded transverse coordinate window; caps are numerical" },
        { "boundaries", Json::array() }
      };
      for (const auto& boundary : feature.band->boundaries) {
        Json points = Json::array();
        for (const auto& point : boundary) {
          points.push_back(FieldPointJson(point));
        }
        item["band"]["boundaries"].push_back(std::move(points));
      }
    }
    if (feature.contributor_fraction_of_estimated_y) {
      item["contributor_fraction_of_estimated_Y"] = Num(*feature.contributor_fraction_of_estimated_y);
      item["source_relation"] = "one positive contributor to the estimated aggregate field, not a unique cause";
    }
    if (!feature.field_points.empty()) {
      item["source_scope"] =
          "aggregate physical-member/spectrum/product measure; source_token, when present, is a witness only";
    }
    if (feature.source_token) {
      item["source_token"] = *feature.source_token;
      const auto& source = discovery.measure.sources[*feature.source_token];
      ProductInput input;
      const auto replay = ReplayDiagnosticSource(discovery.measure, *feature.source_token, &input);
      if (replay.Ok()) {
        const auto& layer = input.layers[0];
        const auto& spectral = input.spectrum.rows[source.spectral_row];
        item["positive_source_witness"] =
            SourceJson({ layer.shape, layer.analytic_pose, input.source.incident_direction, spectral.refractive_index,
                         *feature.source_token });
      }
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
    if (feature.boundary_source && feature.boundary_value) {
      item["declared_source_event"] = {
        { "coordinates", feature.source_parameters },
        { "source", SourceJson(*feature.boundary_source) },
        { "value", OpticalJson(*feature.boundary_value) },
        { "coordinate_units",
          "normalized uniform latent endpoints; physical transforms belong to the recorded product model" }
      };
    }
    if (feature.paired_interface) {
      const auto& paired = *feature.paired_interface;
      item["paired_interface"] = { { "slot", feature.internal_slot },
                                   { "complete", paired.complete },
                                   { "scope",
                                     "original positive source witness at fixed shape/pose/incident/member, actual "
                                     "spectrum; all exit directions, not the integrated sky field or a unique cause" },
                                   { "actual_XYZ", paired.actual_xyz },
                                   { "without_slot_XYZ", paired.without_slot_xyz } };
      if (paired.chromaticity_available) {
        item["paired_interface"]["xy_difference"] = paired.xy_difference;
      }
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
    if (feature.source_event) {
      item["geometry"]["point_scope"] = "valid-side witness of the source interval, not an exact gate image";
      if (!feature.sky_points.empty()) {
        item["geometry"]["sky_points"] = Json::array({ feature.sky_points.front() });
      }
    }
    const char* bucket = feature.evidence == DiagnosticEvidence::kActual    ? "actual_features" :
                         feature.evidence == DiagnosticEvidence::kCandidate ? "candidates" :
                                                                              "unfinished";
    document[bucket].push_back(std::move(item));
  }
  document["unfinished_reasons"] = discovery.unfinished;
  document["coverage"] = {
    { { "subject", "actual product input and physical L2" },
      { "status", "supported" },
      { "reason", "one selected crystal chain; actual distribution/shape/source/spectrum assembly" } },
    { { "subject", "bounded local discovery" },
      { "status", discovery.budget_exhausted ? "numerical_incomplete" : "supported" },
      { "reason",
        "only the recorded local windows and source seeds; no all-sky or source-topology completeness claim" } }
  };
  document["outcome"] = result.no_related_signal                                    ? "no_related_feature" :
                        discovery.budget_exhausted || !discovery.unfinished.empty() ? "partial" :
                                                                                      "completed";
  return document.dump();
}

}  // namespace lumice::raypath
