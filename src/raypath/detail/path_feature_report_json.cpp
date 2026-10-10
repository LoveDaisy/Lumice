#include "raypath/detail/path_feature_report_json.hpp"

#include <nlohmann/json.hpp>

#include "analytic/dp_boundary.hpp"
#include "analytic/dp_focus.hpp"
#include "raypath/detail/json_values.hpp"
#include "raypath/detail/measure/measure_geometry_contract.hpp"
#include "raypath/detail/measure/visibility_certificate.hpp"
#include "raypath/detail/schema3/mc_evidence.hpp"
#include "raypath/detail/schema3/no_related_feature.hpp"
#include "raypath/detail/schema3/structure_object.hpp"

namespace lumice::raypath {
using schema3::ConstantDeltaCurve;
using schema3::CorroborationAnnotation;
using schema3::EnumeratedCoverage;
using schema3::FamilySupport;
using schema3::McSpectralVerification;
using schema3::MemberSupport;
using schema3::ObjectKind;
using schema3::Schema3DiscoveryCore;
using schema3::StructureObjectRecord;
using schema3::UnattributedStructure;
namespace {
using detail::Array;
using detail::Num;
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

// One v2 discovery record's JSON face — the single owner shared by both serializers: the v2
// buckets (actual_features/candidates/unfinished) and the v3 mc_evidence.records ride the SAME
// function, so a field added to DiagnosticFeatureRecord cannot grow two divergent spellings
// (a56). `sources` receives the per-token projection the record's witness contributes.
Json FeatureRecordJson(const DiscoveryResult& discovery, const DiagnosticFeatureRecord& feature, size_t index,
                       Json* sources) {
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
    AssembledInput input;
    const auto replay = ReplayDiagnosticSource(discovery.measure, *feature.source_token, &input);
    if (replay.Ok()) {
      const auto& layer = input.layers[0];
      const auto& spectral = input.spectrum.rows[source.spectral_row];
      item["positive_source_witness"] = SourceJson({ layer.shape, layer.analytic_pose, input.source.incident_direction,
                                                     spectral.refractive_index, *feature.source_token });
    }
    (*sources)[std::to_string(*feature.source_token)] = { { "outer_sample", source.sample_index },
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
  return item;
}

// -- schema3 serialization helpers (666.3 Step 2) ----------------------------------------------

// The rule: enum spellings come from the registered *Name() functions or, where the contract
// has no registered table, from ONE local switch here (the v2 EvidenceName precedent); the
// migration ledger is read by the v3 unit tests, not spelled again by the serializer.

const char* PartitionCoverageName(PartitionContext::Coverage coverage) {
  switch (coverage) {
    case PartitionContext::Coverage::kComplete:
      return "complete";
    case PartitionContext::Coverage::kIncomplete:
      return "incomplete";
    case PartitionContext::Coverage::kUnknown:
      return "unknown";
  }
  return "unknown";
}

Json IntervalsJson(const std::vector<analytic::DeviationInterval>& intervals) {
  Json array = Json::array();
  for (const analytic::DeviationInterval& interval : intervals) {
    array.push_back({ { "lower_rad", Num(interval.lower) },
                      { "upper_rad", Num(interval.upper) },
                      { "n_components", interval.n_components },
                      { "n_closed", interval.n_closed },
                      { "n_open", interval.n_open } });
  }
  return array;
}

Json SupportBlockJson(const Schema3DiscoveryCore& core) {
  Json members = Json::array();
  for (const MemberSupport& row : core.support.members) {
    Json partition = { { "coverage", PartitionCoverageName(row.axis.context.coverage) },
                       { "walk_status", WalkStatusName(row.axis.walk_status) },
                       { "walk_closed", row.axis.walk_closed },
                       { "intervals", IntervalsJson(row.axis.intervals) } };
    if (!row.axis.regime_slug.empty()) {
      partition["escape_regime_slug"] = row.axis.regime_slug;  // the slug is DATA: the regime's name
      // G3: the typed key names a contract-registered regime and omits at the kUnset sentinel —
      // serializing "unset" would re-spell the fake default the sentinel replaced. The slug above
      // is the regime's datum; a producer that someday sets a registered regime gets the key back.
      if (row.axis.context.escape_regime != EscapeRegime::kUnset) {
        partition["escape_regime"] = EscapeRegimeName(row.axis.context.escape_regime);
      }
    }
    if (!row.axis.message.empty()) {
      partition["message"] = row.axis.message;
    }
    Json onsets = Json::array();
    for (const analytic::CriticalOnset& onset : row.endpoint_onsets) {
      Json item = { { "value_rad", Num(onset.value) },
                    { "location", OnsetLocationName(onset.location) },
                    { "source", OnsetSourceName(onset.source) },
                    { "profile", OnsetProfileName(onset.profile) },
                    { "gradient_norm", Num(onset.gradient_norm) },
                    { "has_measure_limit", onset.has_measure_limit },
                    { "multiplicity", onset.multiplicity } };
      if (onset.has_measure_limit) {
        item["measure_limit"] = Num(onset.measure_limit);
      }
      onsets.push_back(std::move(item));
    }
    Json curves = Json::array();
    for (const ConstantDeltaCurve& curve : row.constant_curves) {
      curves.push_back({ { "d_p_rad", Num(curve.d_p) },
                         { "weight_step", curve.weight_step },
                         { "wavelengths_nm", curve.wavelengths_nm },
                         { "critical_d_p_rad", Array(curve.critical_d_p) } });
    }
    members.push_back({ { "member", row.member },
                        { "partition", std::move(partition) },
                        { "endpoint_onsets", std::move(onsets) },
                        { "constant_curves", std::move(curves) } });
  }
  Json families = Json::array();
  for (const FamilySupport& family : core.support.families) {
    // A non-shared family emits an EMPTY intervals list by contract (FamilySupport: a
    // disagreeing family reports no union rather than a fabricated one; the per-member
    // intervals live on the member rows above). Not a dropped field.
    families.push_back({ { "shared", family.shared },
                         { "intervals", family.shared ? IntervalsJson(family.intervals) : Json::array() },
                         { "members", family.members },
                         { "phi_class_note", family.phi_class_note } });
  }
  return { { "members", std::move(members) }, { "families", std::move(families) } };
}

Json ChromaticJson(const StructureObjectRecord& object) {
  Json verdict = { { "kind", ChromaticVerdictKindName(object.chromatic.kind) },
                   { "color", ChromaticColorName(object.chromatic.color) },
                   { "visible", object.chromatic.visible },
                   { "coverage_complete", object.chromatic.coverage_complete },
                   { "faces", object.chromatic.faces },
                   { "n_red", Num(object.chromatic.n_red) },
                   { "n_blue", Num(object.chromatic.n_blue) } };
  if (object.chromatic.has_position) {
    verdict["position_rad"] = Num(object.chromatic.position);
  }
  Json features = Json::array();
  for (const analytic::ChromaticFeature& feature : object.chromatic.features) {
    features.push_back({ { "kind", ChromaticFeatureKindName(feature.kind) },
                         { "source", feature.source },
                         { "color", ChromaticColorName(feature.color) },
                         { "visible", feature.visible },
                         { "positive_fraction", Num(feature.positive_fraction) },
                         { "delta_red_rad", Num(feature.delta_red) },
                         { "delta_blue_rad", Num(feature.delta_blue) },
                         { "shift_rad", Num(feature.shift) },
                         { "spread_rad", Num(feature.spread) },
                         { "direction_dispersion", Num(feature.direction_dispersion) },
                         { "contrast", Num(feature.contrast) },
                         { "weight", Num(feature.weight) },
                         { "lit_fraction", Num(feature.lit_fraction) } });
  }
  verdict["features"] = std::move(features);
  if (object.chromatic.has_tint) {
    verdict["tint"] = { { "energy_red", Num(object.chromatic.tint.energy_red) },
                        { "energy_blue", Num(object.chromatic.tint.energy_blue) },
                        { "ratio", Num(object.chromatic.tint.ratio) },
                        { "tir_fraction_red", Num(object.chromatic.tint.tir_fraction_red) },
                        { "tir_fraction_blue", Num(object.chromatic.tint.tir_fraction_blue) },
                        { "direction_dispersion", Num(object.chromatic.tint.direction_dispersion) } };
  }
  verdict["notes"] = object.chromatic.notes;
  const analytic::ChromaticThresholds& t = object.chromatic_thresholds;
  Json thresholds = { { "n_red", Num(t.n_red) },
                      { "n_blue", Num(t.n_blue) },
                      { "edge_min_shift_rad", Num(t.edge_min_shift_rad) },
                      { "edge_spread_per_shift", Num(t.edge_spread_per_shift) },
                      { "calibration_white_max_deviation", Num(t.calibration_white_max_deviation) },
                      { "tint_ratio_min", Num(t.tint_ratio_min) } };
  return { { "assessed", object.chromatic_assessed },
           { "verdict", std::move(verdict) },
           { "thresholds", std::move(thresholds) } };
}

Json Schema3ObjectJson(const StructureObjectRecord& object, const CorroborationAnnotation& annotation, size_t id) {
  Json existence = { { "state", ExistenceStateName(object.existence) }, { "walk_s", Num(object.walk_s) } };
  if (object.existence == ExistenceState::kEscaped && !object.escape_regime_slug.empty()) {
    existence["escape_regime_slug"] = object.escape_regime_slug;  // the slug is DATA (G3 interim)
  }
  const VisibilityCertificate& visibility = object.visibility;
  Json visibility_json = {
    { "state", VisibilityStateName(visibility.state) },    { "lit_fraction", Num(visibility.lit_fraction) },
    { "evidence", EvidenceFormName(visibility.evidence) }, { "jets_ok", visibility.jets_ok },
    { "saw_zero_area", visibility.saw_zero_area },         { "saw_zero_transmission", visibility.saw_zero_transmission }
  };
  if (visibility.reason[0] != '\0') {
    visibility_json["reason"] = visibility.reason;
  }
  Json geometry = { { "u", Array(object.u) } };
  if (object.has_sky_position) {
    geometry["sky_position"] = Array(object.sky_position, 3);
  }
  Json corroboration = { { "state", CorroborationStateName(annotation.state) },
                         { "presence_ess", Num(annotation.presence_ess) },
                         { "match_distance_rad", Num(annotation.match_distance) },
                         { "tolerance_rad", Num(annotation.tolerance_rad) },
                         { "matched_record", annotation.matched_record },
                         { "matched_record_ess", Num(annotation.matched_record_ess) },
                         { "ruler", annotation.ruler },
                         { "reason", annotation.reason } };
  return { { "id", id },
           { "kind", ObjectKindName(object.kind) },
           { "member", object.member },
           { "slot", object.slot },
           { "phi_class_note", object.phi_class_note },
           { "existence", std::move(existence) },
           { "visibility", std::move(visibility_json) },
           { "chromatic", ChromaticJson(object) },
           { "corroboration", std::move(corroboration) },
           { "geometry", std::move(geometry) },
           { "diagnostics",
             { { "counterfactual",
                 { { "available", object.counterfactual.available },
                   { "with_slot", Num(object.counterfactual.with_slot) },
                   { "without_slot", Num(object.counterfactual.without_slot) } } } } } };
}

}  // namespace

std::string PathFeatureReportV3ToJson(const PathFeatureReport& result, const schema3::AssembledSchema3Report& assembled,
                                      const char* lumice_version) {
  if (result.unsupported_multicrystal) {
    // The early path in the v3 shape: the segments and blocks it names, empty; the refusal row.
    return Json{
      { "schema", "lumice.path-feature-report" },
      { "schema_version", kFeatureReportSchemaVersion },
      { "outcome", "unsupported_multicrystal" },
      { "requested_path_layers", result.requested_path_layers },
      { "features", { { "actual", Json::array() }, { "candidate", Json::array() }, { "unfinished", Json::array() } } },
      { "mc_evidence",
        { { "records", Json::array() },
          { "observation_options", Json::object() },
          { "spectral_verification",
            { { "available", false },
              { "movement_rad", nullptr },
              { "optical_evaluations", 0 },
              { "field_component_evaluations", 0 },
              { "seconds", 0 } } },
          { "observation_scope_note", schema3::kMcObservationScopeNote },
          { "unfinished", Json::array() } } },
      { "coverage",
        { { { "subject", "multi-crystal chain" },
            { "status", "not_supported" },
            { "reason",
              "single-crystal internal reflections are supported, but this multi-crystal request was not "
              "evaluated" } } } },
      { "budgets", { { "optical_evaluations", 0 }, { "field_component_evaluations", 0 } } }
    }.dump();
  }

  const DiscoveryResult& discovery = assembled.mc.discovery;
  Json document = {
    { "schema", "lumice.path-feature-report" },
    { "schema_version", kFeatureReportSchemaVersion },
    { "generator", { { "lumice", lumice_version }, { "analytic_api_version", analytic::kApiVersion } } },
    { "conventions",
      { { "sky", "unit world viewing direction, negative propagation" },
        { "pose", "row-major body-to-world SO(3)" },
        { "angles", "radians" },
        { "field", "normalized vMF per steradian; tangent gradients per radian and Hessians per radian squared" },
        { "weight", "product 2A/S times all interface factors times source spectral XYZ coefficient once" },
        { "uncertainty",
          "fixed-observation prefix movement, solver correction and scale response are different quantities; none is a "
          "global error certificate" },
        { "walk_s",
          "null = no walk arclength applies to this object; 0.0 = the walk was truncated before the record and no "
          "pre-truncation arclength is exposed (the covered amount is unknown, never a measured zero)" },
        { "null", "null = a non-finite number (JSON cannot spell NaN or the infinities)" },
        { "min_margin_rad",
          "unattributed_structures.min_margin_rad is null when no object image exists to take a minimum over (an empty "
          "object set leaves every finding at an infinite margin)" },
        { "features_buckets",
          "the three feature buckets are a derived view over the object records (existence x visibility); no bucket is "
          "stored on an object" },
        { "chromatic_thresholds",
          "each object's chromatic verdict carries the threshold parameters that produced the label; they are declared "
          "calibration, not universal physics" } } }
  };

  // -- scope: the input identity (the v2 scope, plus the renamed input-side blocks).
  // Built stepwise (the v2 shape): nlohmann's initializer lists decay {key, Json::array()}
  // pairs inside a nested object, so the two array-valued members are assigned, not listed.
  const auto support = DescribeSupport(result.snapshot);
  document["scope"] = { { "scene_identity", result.snapshot.scene_identity },
                        { "spectrum_scope", result.spectrum_scope },
                        { "seed", result.seed },
                        { "member_semantics", "physical_L2" },
                        { "light", nlohmann::json(result.snapshot.light) },
                        { "requested_outer_samples", result.requested_outer_samples },
                        { "budget_ms", result.budget_ms } };
  document["scope"]["support_dimensions"] = {
    { "pose_coordinate_count", support.pose_coordinate_count },
    { "pose_support_dimension", support.pose_support_dimension },
    { "shape_parameter_dimension", support.shape_parameter_dimension },
    { "source_direction_dimension", support.source_direction_dimension },
    { "spectral_dimension", support.spectral_dimension },
    { "joint_optical_rank", nullptr },
    { "rank_scope",
      "conditional pose Jacobians are not the joint map rank; shape parameter count is not "
      "geometric image dimension" }
  };
  document["scope"]["layers"] = Json::array();
  document["scope"]["physical_members"] = result.representative_input.layers[0].scope.members;
  document["scope"]["spectrum"] = Json::array();
  for (const auto& layer : result.snapshot.layers) {
    document["scope"]["layers"].push_back(
        { { "scene_layer", result.standalone_crystal ? Json(nullptr) : Json(layer.layer_index) },
          { "crystal", nlohmann::json(layer.crystal) },
          { "representative_faces", layer.representative },
          { "symmetry_bits", layer.symmetry_bits } });
  }
  for (const auto& row : result.representative_input.spectrum.rows) {
    document["scope"]["spectrum"].push_back({ { "nm", row.wavelength_nm },
                                              { "source_weight", Num(row.source_weight) },
                                              { "measure_mass", Num(row.measure_mass) },
                                              { "index", Num(row.refractive_index) },
                                              { "XYZ_coefficient", row.coefficient },
                                              { "provenance", row.provenance } });
  }

  // -- support: the delta-axis block (the schema3 new face)
  document["support"] = SupportBlockJson(assembled.core);

  // -- features: the three derived buckets over the corroboration-written records
  Json actual = Json::array();
  Json candidate = Json::array();
  Json unfinished = Json::array();
  for (size_t index = 0; index < assembled.core.objects.size(); ++index) {
    Json object = Schema3ObjectJson(assembled.core.objects[index], assembled.annotations[index], index);
    switch (schema3::BucketOf(assembled.core.objects[index])) {
      case schema3::FeatureBucket::kActual:
        actual.push_back(std::move(object));
        break;
      case schema3::FeatureBucket::kCandidate:
        candidate.push_back(std::move(object));
        break;
      case schema3::FeatureBucket::kUnfinished:
        unfinished.push_back(std::move(object));
        break;
    }
  }
  document["features"] = { { "actual", std::move(actual) },
                           { "candidate", std::move(candidate) },
                           { "unfinished", std::move(unfinished) } };

  // -- unattributed_structures: the forward detector's output, silences visible
  Json structures = Json::array();
  for (const UnattributedStructure& structure : assembled.unattributed.structures) {
    structures.push_back({ { "record_index", structure.record_index },
                           { "position", structure.position },
                           { "delta_rad", Num(structure.delta_rad) },
                           { "record_ess", Num(structure.record_ess) },
                           { "min_margin_rad", Num(structure.min_margin_rad) },
                           { "ruler", structure.ruler } });
  }
  Json unattributed = { { "structures", std::move(structures) },
                        { "skipped_no_position", assembled.unattributed.skipped_no_position },
                        { "skipped_below_ess", assembled.unattributed.skipped_below_ess } };
  if (!assembled.unattributed.error.empty()) {
    unattributed["error"] = assembled.unattributed.error;
  }
  document["unattributed_structures"] = std::move(unattributed);

  // -- mc_evidence: the demoted v2 observation record (the carry), records in build order
  Json records = Json::array();
  Json sources = Json::object();
  for (size_t index = 0; index < discovery.features.size(); ++index) {
    records.push_back(FeatureRecordJson(discovery, discovery.features[index], index, &sources));
  }
  const McSpectralVerification& spectral = assembled.mc.spectral_verification;
  document["mc_evidence"] = {
    { "records", std::move(records) },
    { "observation_options",
      { { "kernel", "normalized_vMF" },
        { "bandwidth_rad", Num(assembled.mc.observation_options.bandwidth_rad) },
        { "location_resolution_rad", Num(assembled.mc.observation_options.location_resolution_rad) },
        { "search_scope", "bounded source-seeded local structures; not exhaustive all-sky or source topology" } } },
    { "spectral_verification",
      { { "available", spectral.available },
        { "movement_rad", spectral.movement_rad ? Num(*spectral.movement_rad) : Json(nullptr) },
        { "optical_evaluations", spectral.optical_evaluations },
        { "field_component_evaluations", spectral.field_component_evaluations },
        { "seconds", Num(spectral.seconds) } } },
    { "observation_scope_note", assembled.mc.observation_scope_note },
    { "unfinished", discovery.unfinished }
  };

  // -- coverage: the bounded-search honesty rows, the vocabulary ledger row, the declared skips
  Json coverage = Json::array();
  coverage.push_back(
      { { "subject", "actual product input and physical L2" },
        { "status", "supported" },
        { "reason", "one selected crystal chain; actual distribution/shape/source/spectrum assembly" } });
  coverage.push_back(
      { { "subject", "bounded MC corroboration discovery" },
        { "status",
          discovery.budget_exhausted || !discovery.unfinished.empty() ? "numerical_incomplete" : "supported" },
        { "limitations", discovery.limitations },
        { "reason",
          "only the recorded local windows and source seeds; no all-sky or source-topology completeness "
          "claim" } });
  const EnumeratedCoverage& vocabulary = assembled.core.coverage;
  Json declared = Json::array();
  for (size_t i = 0; i < vocabulary.declared_not_produced.size(); ++i) {
    declared.push_back({ { "kind", ObjectKindName(vocabulary.declared_not_produced[i]) },
                         { "reason", vocabulary.declared_reasons[i] } });
  }
  Json produced = Json::array();
  for (ObjectKind kind : vocabulary.produced) {
    produced.push_back(ObjectKindName(kind));
  }
  coverage.push_back({ { "subject", "object-kind vocabulary" },
                       { "status", "supported" },
                       { "produced", std::move(produced) },
                       { "declared_not_produced", std::move(declared) } });
  if (!assembled.measure_skip_note.empty()) {
    coverage.push_back({ { "subject", "declared measure" },
                         { "status", "not_supported" },
                         { "reason", assembled.measure_skip_note } });
  }
  if (!assembled.density_skip_note.empty()) {
    coverage.push_back({ { "subject", "pose density conversion" },
                         { "status", "not_supported" },
                         { "reason", assembled.density_skip_note } });
  }
  document["coverage"] = std::move(coverage);

  // -- observation: narrowed to the corroboration-observation declaration (the ruler's pointer)
  document["observation"] = { { "subject", "mc_corroboration_observation" },
                              { "ruler", "mc_evidence.observation_options" } };

  // -- budgets: the v2 work account, plus the enumeration and attribution legs
  const uint64_t attribution_dp = static_cast<uint64_t>(assembled.unattributed.counts.dp_evaluations) +
                                  static_cast<uint64_t>(assembled.corroboration_counts.dp_evaluations);
  const uint64_t attribution_presence = static_cast<uint64_t>(assembled.unattributed.counts.presence_ess_queries) +
                                        static_cast<uint64_t>(assembled.corroboration_counts.presence_ess_queries);
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
    { "exhausted", discovery.budget_exhausted },
    { "enumeration",
      { { "max_optical_evaluations", assembled.core.budget.max_optical_evaluations },
        { "sampling_evaluations", assembled.core.budget.sampling_evaluations },
        { "hang_cap_ms", assembled.core.budget.hang_cap_ms },
        { "truncated", assembled.core.budget.truncated },
        { "truncation_note", assembled.core.budget.truncation_note } } },
    { "attribution", { { "dp_evaluations", attribution_dp }, { "presence_ess_queries", attribution_presence } } }
  };

  // -- timing: the v2 legs, plus the schema3 assembly's own two
  document["timing"] = { { "spectral_verification", Num(result.spectral_seconds) },
                         { "capture", Num(result.capture_seconds) },
                         { "assembly", Num(discovery.assembly_seconds) },
                         { "source_events_and_edge_observations", Num(discovery.event_seconds) },
                         { "field_discovery", Num(discovery.field_seconds) },
                         { "enumeration", Num(assembled.enumeration_seconds) },
                         { "attribution", Num(assembled.attribution_seconds) } };

  // -- sources: the projection re-derived from the carried measure's token table
  document["sources"] = std::move(sources);

  // -- the ruling (unconditional: rule A's priority does not blind the report to what B said)
  const schema3::NoRelatedRuling& ruling = assembled.ruling;
  Json ruling_json = { { "issued", ruling.issued },
                       { "partition_complete_no_escape", ruling.partition_complete_no_escape },
                       { "partition_failure", ruling.partition_failure },
                       { "all_objects_unlit_or_none", ruling.all_objects_unlit_or_none },
                       { "unlit_failure", ruling.unlit_failure },
                       { "no_sufficient_ess_unattributed", ruling.no_sufficient_ess_unattributed },
                       { "unattributed_failure", ruling.unattributed_failure },
                       { "s4_scope_declared", ruling.s4_scope_declared },
                       { "s4_failure", ruling.s4_failure },
                       { "two_d_valid_support", ruling.two_d_valid_support },
                       { "s4_scope_note", ruling.s4_scope_note },
                       { "absence_not_proven_note", ruling.absence_not_proven_note } };
  if (ruling.issued) {
    ruling_json["basis"] = NoRelatedBasisName(ruling.basis);
  }
  document["no_related_feature"] = std::move(ruling_json);

  // -- outcome: the single value; the basis rides beside it exactly when issued
  document["outcome"] = ruling.issued                                                 ? "no_related_feature" :
                        (discovery.budget_exhausted || !discovery.unfinished.empty()) ? "partial" :
                                                                                        "completed";
  if (ruling.issued) {
    document["basis"] = NoRelatedBasisName(ruling.basis);
  }
  return document.dump();
}

}  // namespace lumice::raypath
