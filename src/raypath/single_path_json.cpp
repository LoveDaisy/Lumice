#include "raypath/single_path_json.hpp"

#include <cmath>
#include <limits>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <vector>

#include "raypath/detail/json_values.hpp"

namespace lumice::raypath {

namespace {

// The keys the reader shares with the writer.
constexpr const char* kKeySchemaVersion = "schema_version";
constexpr const char* kKeyComponents = "components";
constexpr const char* kKeyIncomplete = "incomplete";
constexpr const char* kKeySeed = "seed";
constexpr int kSeedLength = 9;

using detail::Array;
using detail::GridNum;
using detail::Num;

}  // namespace

// The config's own spelling of a distribution; a plain number in the config is "fixed".
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

namespace {

nlohmann::ordered_json Conventions() {
  return {
    { "frames",
      "world: +z is the zenith, azimuth counter-clockwise from +x seen from +z; body: the crystal frame, +z its "
      "c-axis (face 1's outward normal) (doc/geometry/coordinate-convention.md)" },
    { "directions",
      "unit 3-vectors follow the light-travel convention (doc/geometry/coordinate-convention.md, Direction-Vector "
      "Semantics): incident_direction is the propagation sun -> crystal; target_direction is the propagation of "
      "light arriving from the target sky point (the kernel seeks exits displayed there); outgoing_direction is "
      "the exit propagation, displayed at the sky point it comes from; for any such d that point sits at "
      "altitude asin(-d.z), azimuth atan2(y, x) - 180" },
    { "target_azimuth", "measured as the sun's azimuth is (the `analyze --center` convention)" },
    { "pose", "row-major 3x3 body -> world rotation R = Rz(azimuth - 180) Ry(-zenith) Rz(roll), 9 numbers" },
    { "angles",
      "degrees; zenith in [0, 180], azimuth and roll in (-180, 180]; where zenith is 0 or 180 only azimuth +- roll "
      "is defined, so roll is 0, azimuth carries the whole angle and degenerate is true" },
    { "sun_in_crystal", "u = R^T (sun direction): where the sun sits in the crystal frame" },
    { "units",
      "keys ending in _deg are degrees, _rad radians, _nm nanometres; entry_measure is an area in the crystal's "
      "length unit squared; arclength_increments are radians on SO(3)" },
    { "null", "a number that is not defined (NaN) is written as null" },
    { "factors",
      "the per-pose factor decomposition is multiplicative, in real units, never additive "
      "percentages: rho is the declared joint pose density w.r.t. dlambda dphi droll (radians, "
      "angle Lebesgue, declared measure); entry_measure_a2 is the entry area over the crystal "
      "length unit squared (a = 1 in this repo's configs — a declared identity, not a "
      "normalization); total_transmission is the interface product; k_carea = 1/(J_perp + "
      "factors.epsilon), J_perp the pose's normal Jacobian, null where it cannot be formed; "
      "factor_product = rho * entry_measure_a2 * total_transmission * k_carea, null where any "
      "factor is unavailable. measure_registration declares the support kind and any registered "
      "read-as-zero gap of the declared measure — a rho of 0 there is 'unanswered', not 'dark'" },
    { "sun_grid",
      "cell centres, row-major [i][j]: lat_i = -90 + (i + 0.5) * 180 / lat_count, lon_j = -180 + (j + 0.5) * 360 / "
      "lon_count, u = (cos lat cos lon, cos lat sin lon, sin lat) in the body frame; the longitude seam is periodic; "
      "arrays rounded to 9 significant digits" },
    { "completeness", kCompletenessNote },
    { "survey_context",
      "where the target sky point sits in the survey's structure (doc/raypath/raypath-cli-output.md, the "
      "--warm contract): source warm (the given schema-4 document's facts) or inline (a context-only T0-T2 "
      "assembly on this request's config); every field carries its own state — certified (the support "
      "partition's containment fact), finite (a computed reading), unevaluated with a named reason — and the "
      "value fields exist exactly when the state says something was read. Support and edge require "
      "the producing wavelength; a missing or different wavelength leaves them unevaluated "
      "(scope_field_missing / scope_value_mismatch). The band-sum reading additionally requires "
      "the producing sun; it is the original survey line's nearest sample, not a target-sky "
      "intensity. warm_index_absent declares degradation to the inline context" },
    { "reach",
      "whether delta (target deviation) lies in the range of D over every valid sun direction, probed on its own "
      "grid (independent of sun_grid) with the validity boundary bisected; false: delta is outside "
      "[deviation_min_rad - tolerance_rad, deviation_max_rad + tolerance_rad], no pose of the infinite crystal "
      "reaches the target at the probe's resolution; true: not excluded, which promises no component (the valid "
      "set need not be connected, and the finite crystal may pass no ray along the fiber)" },
  };
}

nlohmann::ordered_json MetaJson(const SinglePathMetadata& m) {
  nlohmann::ordered_json shape = nlohmann::ordered_json::array();
  for (const auto& s : m.shape) {
    shape.push_back({ { "name", s.name },
                      { "value", Num(s.value) },
                      { "distribution", DistributionName(s.distribution) },
                      { "spread", Num(s.spread) } });
  }
  nlohmann::ordered_json crystal = {
    { "id", m.crystal_id },
    { "kind", m.crystal_kind },
    { "shape", shape },
    { "shape_is_nominal", m.shape_is_nominal },
  };
  if (m.crystal_kind == "pyramid") {
    crystal["upper_wedge_deg"] = Num(m.upper_wedge_deg);
    crystal["lower_wedge_deg"] = Num(m.lower_wedge_deg);
  }
  return {
    { "crystal", crystal },
    { "faces", m.faces },
    { "sun",
      { { "altitude_deg", Num(m.sun_altitude_deg) },
        { "azimuth_deg", Num(m.sun_azimuth_deg) },
        { "diameter_deg", Num(m.sun_diameter_deg) },
        { "incident_direction", Array(m.incident_direction, 3) } } },
    { "target",
      { { "altitude_deg", Num(m.target_altitude_deg) },
        { "azimuth_deg", Num(m.target_azimuth_deg) },
        { "direction", Array(m.target_direction, 3) },
        { "deviation_deg", Num(m.target_deviation_deg) } } },
    { "wavelength",
      { { "nm", Num(m.wavelength_nm) },
        { "source", WavelengthSourceName(m.wavelength_source) },
        { "refractive_index", Num(m.refractive_index) } } },
    { "discovery_settings",
      { { "sample_count", m.sample_count },
        { "band_half_width_rad", Num(m.band_half_width_rad) },
        { "cluster_radius_rad", Num(m.cluster_radius_rad) },
        { "distance_threshold_rad", Num(m.distance_threshold_rad) },
        { "warm_seed_count", m.warm_seed_count } } },
    { "budget_ms", m.budget_ms },
    { "interrupted", m.interrupted },
  };
}

nlohmann::ordered_json TraceEndJson(const TraceEnd& e) {
  return { { "status", TraceStatusName(e.status) },
           { "reason", TraceReasonName(e.reason) },
           { "pose_count", e.pose_count } };
}

nlohmann::ordered_json PointJson(const PointDetail& p) {
  return {
    { "pose", Array(p.pose, 9) },
    { "angles",
      { { "zenith_deg", Num(p.angles.zenith_deg) },
        { "azimuth_deg", Num(p.angles.azimuth_deg) },
        { "roll_deg", Num(p.angles.roll_deg) },
        { "degenerate", p.angles.degenerate } } },
    { "sun_in_crystal", Array(p.sun_in_crystal, 3) },
    { "residual_norm", Num(p.residual_norm) },
    { "valid", p.valid },
    { "outgoing_direction", Array(p.outgoing_direction, 3) },
    { "segment_directions", Array(p.segment_directions) },
    { "interface_transmittances", Array(p.interface_transmittances) },
    { "total_transmission", Num(p.total_transmission) },
    { "entry_measure", Num(p.entry_measure) },
    { "rho", Num(p.rho) },
    { "entry_measure_a2", Num(p.entry_measure_a2) },
    { "jacobian",
      { { "value", Num(p.jacobian) },
        { "singular_values", Array(p.singular_values, 2) },
        { "available", p.jacobian_available } } },
    { "k_carea", Num(p.k_carea) },
    { "factor_product", Num(p.factor_product) },
  };
}

// The component-level factor bookkeeping: the declared epsilon, the arclength integral of the
// factor product, and the completeness declaration (procedural, the discovery note's own
// discipline — a usable-product count, never a promise every pose was readable).
nlohmann::ordered_json FactorsJson(const ComponentFactors& f) {
  return {
    { "epsilon", kFactorsEpsilon },
    { "fiber_integral",
      f.integral_available ? nlohmann::ordered_json(Num(f.fiber_integral)) : nlohmann::ordered_json(nullptr) },
    { "fiber_integral_rule",
      "left-endpoint sum of factor_product * arclength over consecutive points; segments whose "
      "first point carries no usable product (invalid pose or Jacobian unavailable) are skipped "
      "and counted in completeness" },
    { "completeness",
      { { "available_points", f.available_points },
        { "total_points", f.total_points },
        { "note",
          "available_points counts the points whose factor_product is usable; the integral covers "
          "the arclength those points open, not the component's full length" } } },
  };
}

// The declared measure's registration (the discovered branch's document-level block): the support
// kind and any registered read-as-zero gap, so a rho of 0 is never read as dark by default.
nlohmann::ordered_json MeasureRegistrationJson(const MeasureRegistration& m) {
  nlohmann::ordered_json j = { { "support_kind", m.support_kind },
                               { "fast_path", m.fast_path },
                               { "registered_gap", m.registered_gap } };
  if (m.registered_gap) {
    j["note"] = m.note;
  }
  return j;
}

// The rank-0 branch's orbit factor block: the declared measure's spin orbit, its per-theta factor
// readings, and the quadrature integral — or the honest unavailable state, exactly one of the two
// shapes, never a defaulted number.
nlohmann::ordered_json OrbitFactorsJson(const OrbitFactors& o) {
  nlohmann::ordered_json j = { { "available", o.available }, { "measure_support", o.measure_support } };
  if (!o.available) {
    j["reason"] = o.reason;
    return j;
  }
  nlohmann::ordered_json points = nlohmann::ordered_json::array();
  for (const OrbitFactorPoint& p : o.points) {
    points.push_back({ { "theta_rad", Num(p.theta_rad) },
                       { "rho_theta", Num(p.rho_theta) },
                       { "entry_measure_a2", Num(p.entry_measure_a2) },
                       { "total_transmission", Num(p.total_transmission) },
                       { "valid", p.valid } });
  }
  j["sample_count"] = o.sample_count;
  j["integral"] = Num(o.integral);
  j["integral_rule"] =
      "fiber_quadrature kFiberParameter quadrature: sum of rho_theta * "
      "entry_measure_a2 * total_transmission * dtheta over kept samples "
      "(A*T > 0 and valid), midpoint grid over [0, 2 pi)";
  j["kept_count"] = o.kept_count;
  j["points"] = std::move(points);
  return j;
}

nlohmann::ordered_json ComponentJson(const FiberComponent& c) {
  nlohmann::ordered_json j = {
    { "kind", ComponentKindName(c.kind) },
    { kKeySeed, Array(c.seed, kSeedLength) },
    { "forward", TraceEndJson(c.forward) },
  };
  // A closed component's backward trace was never run; its fields would mean nothing.
  if (c.kind == ComponentKind::kArc) {
    j["backward"] = TraceEndJson(c.backward);
  }
  j["seed_index"] = c.seed_index;
  j["arclength_increments"] = Array(c.arclength_increments);
  j["factors"] = FactorsJson(c.factors);
  nlohmann::ordered_json points = nlohmann::ordered_json::array();
  for (const auto& p : c.points) {
    points.push_back(PointJson(p));
  }
  j["points"] = std::move(points);
  return j;
}

nlohmann::ordered_json IncompleteJson(const IncompleteComponent& c) {
  nlohmann::ordered_json j = {
    { "cause", IncompleteCauseName(c.cause) },
    { kKeySeed, Array(c.seed, kSeedLength) },
    { "forward", TraceEndJson(c.forward) },
  };
  // The backward trace runs only for the two arc_backward causes.
  if (c.cause == IncompleteCause::kArcBackwardFailed || c.cause == IncompleteCause::kArcBackwardClosedAnomaly) {
    j["backward"] = TraceEndJson(c.backward);
  }
  return j;
}

nlohmann::ordered_json DiscoveryJson(const DiscoverySummary& d) {
  return {
    { "complete", d.complete },
    { "pool_count", d.pool_count },
    { "extra_seed_count", d.extra_seed_count },
    { "raw_cluster_count", d.raw_cluster_count },
    { "admissible_count", d.admissible_count },
    { "dedup_merged", d.dedup_merged },
    { "arc_stitched", d.arc_stitched },
    { "arc_backward_failed", d.arc_backward_failed },
    { "arc_backward_closed_anomaly", d.arc_backward_closed_anomaly },
    { "incomplete_unnamed_event", d.incomplete_unnamed_event },
    { "incomplete_not_converged", d.incomplete_not_converged },
  };
}

nlohmann::ordered_json SunGridJson(const SunSphereGrid& g) {
  nlohmann::ordered_json deviation = nlohmann::ordered_json::array();
  nlohmann::ordered_json valid = nlohmann::ordered_json::array();
  nlohmann::ordered_json entry = nlohmann::ordered_json::array();
  for (size_t i = 0; i < g.deviation_rad.size(); i++) {
    deviation.push_back(GridNum(g.deviation_rad[i]));
    valid.push_back(static_cast<int>(g.valid[i]));
    entry.push_back(GridNum(g.entry_measure[i]));
  }
  return { { "lat_count", g.lat_count },
           { "lon_count", g.lon_count },
           { "deviation_rad", std::move(deviation) },
           { "valid", std::move(valid) },
           { "entry_measure", std::move(entry) } };
}

}  // namespace

// The survey context block: every field's state always serialized, the value fields exactly when
// their state says something was read — object's only under "certified", edge's and band_sum's
// only under "finite" (so an unevaluated distance can never be misread as the certified 0 that
// means "inside the support").
nlohmann::ordered_json SurveyContextJson(const SurveyContextBlock& c) {
  nlohmann::ordered_json j = { { "source", ContextSourceName(c.source) },
                               { "warm_index_absent", c.warm_index_absent } };
  if (c.reference_wavelength_nm > 0.0) {
    j["reference_wavelength_nm"] = Num(c.reference_wavelength_nm);
  }
  nlohmann::ordered_json object = { { "state", survey::EvidenceStateName(c.object.state) } };
  if (!c.object.reason.empty()) {
    object["reason"] = c.object.reason;
  }
  if (c.object.state == survey::EvidenceState::kCertified) {
    object["target_inside_support"] = c.target_inside_support;
    object["kind"] = c.object_kind ? survey::SurveyObjectKindName(*c.object_kind) : "";
    object["members"] = c.object_members;
    object["distance_deg"] = Num(c.object_distance_deg);
  }
  j["object"] = std::move(object);
  nlohmann::ordered_json edge = { { "state", survey::EvidenceStateName(c.edge.state) } };
  if (!c.edge.reason.empty()) {
    edge["reason"] = c.edge.reason;
  }
  if (c.edge.state == survey::EvidenceState::kFinite) {
    edge["distance_deg"] = Num(c.edge_distance_deg);
    edge["basis"] = ContextEdgeBasisName(c.edge_basis);
  }
  j["edge"] = std::move(edge);
  nlohmann::ordered_json band = { { "state", survey::EvidenceStateName(c.band_sum.state) } };
  if (!c.band_sum.reason.empty()) {
    band["reason"] = c.band_sum.reason;
  }
  if (c.band_sum.state == survey::EvidenceState::kFinite) {
    band["value"] = Num(c.band_sum_value);
    band["delta_deg"] = Num(c.band_sum_delta_deg);
    band["bin_deg"] = Num(c.band_sum_bin_deg);
    band["caliber"] = c.band_sum_caliber;
  }
  j["band_sum"] = std::move(band);
  return j;
}

namespace {

nlohmann::ordered_json ReachJson(const ReachSummary& r) {
  return {
    { "target_in_range", r.target_in_range },          { "target_deviation_rad", Num(r.target_deviation_rad) },
    { "deviation_min_rad", Num(r.deviation_min_rad) }, { "deviation_max_rad", Num(r.deviation_max_rad) },
    { "tolerance_rad", Num(r.tolerance_rad) },         { "probe_lat_count", r.probe_lat_count },
  };
}

Error Invalid(const std::string& what) {
  return { ErrorCode::kInvalidArgument, "warm seeds: " + what };
}

}  // namespace

static std::string EncodePoint(const PointData& result, const SurveyContextBlock& context,
                               const std::string& product_version) {
  nlohmann::ordered_json doc;
  doc[kKeySchemaVersion] = result.meta.schema_version;
  doc["generator"] = { { "lumice", product_version }, { "analytic_api_version", result.meta.analytic_api_version } };
  doc["conventions"] = Conventions();
  doc["meta"] = MetaJson(result.meta);
  if (result.OutcomeValue() == Outcome::kPointMass) {
    // Rank 0: no fiber is traced, so there are no components and no discovery.
    doc["outcome"] = "point_mass";
    const PointMass& pm = result.Mass();
    nlohmann::ordered_json pm_json = { { "direction", Array(pm.direction, 3) },
                                       { "altitude_deg", Num(pm.altitude_deg) },
                                       { "azimuth_deg", Num(pm.azimuth_deg) },
                                       { "target_separation_deg", Num(pm.target_separation_deg) } };
    pm_json["orbit_factors"] = OrbitFactorsJson(pm.orbit_factors);
    doc["point_mass"] = std::move(pm_json);
  } else {
    doc["outcome"] = "discovered";
    doc["measure_registration"] = MeasureRegistrationJson(result.Discovered().measure_registration);
    nlohmann::ordered_json components = nlohmann::ordered_json::array();
    for (const auto& c : result.Discovered().components) {
      components.push_back(ComponentJson(c));
    }
    doc[kKeyComponents] = std::move(components);
    nlohmann::ordered_json incomplete = nlohmann::ordered_json::array();
    for (const auto& c : result.Discovered().incomplete) {
      incomplete.push_back(IncompleteJson(c));
    }
    doc[kKeyIncomplete] = std::move(incomplete);
    doc["discovery"] = DiscoveryJson(result.Discovered().discovery);
    doc["reach"] = ReachJson(result.Discovered().reach);
  }
  // Absent when the grid was not asked for (--grid 0).
  if (result.sun_grid.lat_count > 0) {
    doc["sun_grid"] = SunGridJson(result.sun_grid);
  }
  // The survey context: where the target sits in the survey's structure (always written; every
  // field carries its own state, so absence can never read as answered).
  // The value encoder receives the completed context explicitly. Only the legacy envelope
  // adapter below may call SurveyContextOf.
  doc["survey_context"] = SurveyContextJson(context);
  return doc.dump();
}

std::string ToJson(const SinglePathResult& result, const std::string& product_version) {
  return EncodePoint(result, SurveyContextOf(result), product_version);
}

std::string ToJson(const PointResult& result, const std::string& product_version) {
  if (!result.Data().Completed()) {
    throw std::invalid_argument("point: an uncompleted value has no publication");
  }
  return EncodePoint(result.Data(), result.Data().survey_context, product_version);
}

Error ParseWarmSeeds(const std::string& json_text, std::vector<double>* seeds) {
  nlohmann::json doc = nlohmann::json::parse(json_text, nullptr, /*allow_exceptions=*/false);
  if (doc.is_discarded()) {
    return Invalid("not a JSON document");
  }
  if (!doc.is_object() || !doc.contains(kKeySchemaVersion) || !doc[kKeySchemaVersion].is_number_integer()) {
    return Invalid("not a `Lumice raypath` output (no integer schema_version)");
  }
  const int version = doc[kKeySchemaVersion].get<int>();
  if (version != kSchemaVersion) {
    return Invalid("schema_version " + std::to_string(version) + " is not the version this build reads (" +
                   std::to_string(kSchemaVersion) + ")");
  }
  std::vector<double> found;
  for (const char* list_key : { kKeyComponents, kKeyIncomplete }) {
    if (!doc.contains(list_key)) {
      continue;  // a point-mass output has neither list
    }
    const auto& list = doc[list_key];
    if (!list.is_array()) {
      return Invalid(std::string(list_key) + " is not an array");
    }
    for (size_t i = 0; i < list.size(); i++) {
      const std::string where = std::string(list_key) + "[" + std::to_string(i) + "]." + kKeySeed;
      if (!list[i].is_object() || !list[i].contains(kKeySeed)) {
        return Invalid(where + " is missing");
      }
      const auto& seed = list[i][kKeySeed];
      if (!seed.is_array() || seed.size() != static_cast<size_t>(kSeedLength)) {
        return Invalid(where + " is not " + std::to_string(kSeedLength) + " numbers");
      }
      for (const auto& x : seed) {
        if (!x.is_number()) {
          return Invalid(where + " holds a value that is not a number");
        }
        found.push_back(x.get<double>());
      }
    }
  }
  seeds->insert(seeds->end(), found.begin(), found.end());
  return {};
}

Error DecodeWarmCandidate(const std::string& json_text, WarmCandidate* candidate) {
  *candidate = {};
  auto* seeds = &candidate->seeds;
  auto* survey = &candidate->facts;
  nlohmann::json doc = nlohmann::json::parse(json_text, nullptr, /*allow_exceptions=*/false);
  if (doc.is_discarded()) {
    return Invalid("not a JSON document");
  }
  if (!doc.is_object()) {
    return Invalid("not a warm document (not an object)");
  }
  // The schema-4 arm first (a survey document also carries an integer schema_version — the
  // family name is the discriminator): a schema-4 document through the consistency gates, then
  // the harvest.
  const bool is_survey =
      doc.contains("schema") && doc["schema"].is_string() && doc["schema"].get<std::string>() == "lumice.path-survey";
  if (!is_survey) {
    // The schema-1 arm, spelled exactly as ParseWarmSeeds's: a target-fiber output is a seed
    // sheet, not a survey index — the survey context stays unset and the query computes it
    // inline (the product owner sees a seed sheet, not a survey index).
    return ParseWarmSeeds(json_text, seeds);
  }
  survey->family = WarmDocumentFamily::kSurvey;
  const auto refuse = [](const std::string& why) { return Error{ ErrorCode::kInvalidArgument, "warm index: " + why }; };
  // Decode the request echo; the product owner compares its physical identity.
  if (!doc.contains("request") || !doc["request"].is_object()) {
    return refuse("the document carries no request echo");
  }
  const auto& request = doc["request"];
  if (!request.contains("crystal_id") || !request["crystal_id"].is_number_integer()) {
    return refuse("crystal gate: the document's request.crystal_id does not match this query");
  }
  candidate->crystal_id = request["crystal_id"].get<long long>();
  candidate->decode_stage = WarmDecodeStage::kPath;
  if (!request.contains("path_layers") || !request["path_layers"].is_array()) {
    return refuse("path gate: the document's request carries no path_layers");
  }
  for (const auto& layer : request["path_layers"]) {
    if (!layer.is_array())
      return refuse("path gate: the document's request.path_layers does not match this query");
    std::vector<int> faces;
    for (const auto& face : layer) {
      if (!face.is_number_integer())
        return refuse("path gate: the document's request.path_layers does not match this query");
      faces.push_back(face.get<int>());
    }
    candidate->path_layers.push_back(std::move(faces));
  }
  candidate->decode_stage = WarmDecodeStage::kMeasure;
  if (doc.contains("scope") && doc["scope"].is_object() && doc["scope"].contains("scene_layer")) {
    const auto& layer = doc["scope"]["scene_layer"];
    if (layer.is_null()) {
      candidate->location = ScopeLocation{ SelectionLocation::kStandalone, 0 };
    } else if (layer.is_number_integer() && layer.get<long long>() >= 0) {
      candidate->location =
          ScopeLocation{ SelectionLocation::kSceneLayer, static_cast<size_t>(layer.get<long long>()) };
    } else {
      return refuse("corrupt warm document: scope.scene_layer is not a nonnegative layer or null");
    }
  }
  // Measure echo extraction: the family arm's measure echo. Only the family-arm documents carry
  // family_routing; its absence is data (the arm did not run), so the gate skips with the
  // declaration carried in the survey source — the caller compares the echo when it is present.
  if (doc.contains("family_routing") && doc["family_routing"].is_object() &&
      doc["family_routing"].contains("measure")) {
    const auto read_dim = [](const nlohmann::json& dim, SurveyWarmSource::MeasureDimEcho* out) {
      if (!dim.is_object() || !dim.contains("kind") || !dim["kind"].is_string() || !dim.contains("center_deg") ||
          !dim["center_deg"].is_number() || !dim.contains("width_deg") || !dim["width_deg"].is_number()) {
        return false;
      }
      out->kind = dim["kind"].get<std::string>();
      out->center_deg = dim["center_deg"].get<double>();
      out->width_deg = dim["width_deg"].get<double>();
      return true;
    };
    const auto& measure = doc["family_routing"]["measure"];
    // Present-but-malformed is corrupt data, not absence: the document claims the family arm
    // ran and echoes its measure, but the echo does not read back. Refusing keeps that distinct
    // from "the arm did not run" (the key absent, a declared skip).
    if (!measure.is_object() || !measure.contains("zenith") || !measure.contains("azimuth") ||
        !measure.contains("roll") || !read_dim(measure["zenith"], &survey->measure_zenith) ||
        !read_dim(measure["azimuth"], &survey->measure_azimuth) || !read_dim(measure["roll"], &survey->measure_roll)) {
      return refuse("corrupt warm document: family_routing.measure is present but malformed");
    }
    if (measure.contains("measure_kind")) {
      if (measure["measure_kind"] != "ideal_frozen")
        return refuse("corrupt warm document: unknown family measure kind");
      survey->quantity.measure = MeasureKind::kIdealFrozen;
    }
    survey->has_measure_echo = true;
  }
  // Decode the index receipt; no fingerprint is compared at this boundary.
  const bool index_block =
      doc.contains("warm_index") && doc["warm_index"].is_object() && doc["warm_index"].contains("seeds") &&
      doc["warm_index"]["seeds"].is_array() && doc["warm_index"].contains("seed_sources") &&
      doc["warm_index"]["seed_sources"].is_array() && doc["warm_index"].contains("reuse_scope") &&
      doc["warm_index"]["reuse_scope"].is_string() && doc["warm_index"].contains("produced_for") &&
      doc["warm_index"]["produced_for"].is_string() && doc["warm_index"].contains("config_fingerprint") &&
      doc["warm_index"]["config_fingerprint"].is_string();
  if (!index_block) {
    // The D2 degradation: a pre-warm document (every schema-4 document written before this build)
    // or a truncated one. Not a refusal — the query degrades, visibly, to its own inline context.
    survey->index_present = false;
    return {};
  }
  candidate->config_fingerprint = doc["warm_index"]["config_fingerprint"].get<std::string>();
  survey->index_present = true;
  candidate->decode_stage = WarmDecodeStage::kPayload;

  // The harvest: the seeds...
  const auto& doc_seeds = doc["warm_index"]["seeds"];
  const auto& doc_sources = doc["warm_index"]["seed_sources"];
  if (doc_seeds.size() != doc_sources.size()) {
    return refuse("the document's warm_index seeds and seed_sources disagree in length");
  }
  for (size_t i = 0; i < doc_seeds.size(); i++) {
    const auto& seed = doc_seeds[i];
    const std::string where = "warm_index.seeds[" + std::to_string(i) + "]";
    if (!seed.is_array() || seed.size() != static_cast<size_t>(kSeedLength)) {
      return refuse(where + " is not " + std::to_string(kSeedLength) + " numbers");
    }
    for (const auto& x : seed) {
      if (!x.is_number()) {
        return refuse(where + " holds a value that is not a number");
      }
    }
    for (const auto& x : seed) {
      seeds->push_back(x.get<double>());
    }
  }
  // ...the routing verdict...
  if (doc.contains("routing") && doc["routing"].is_object() && doc["routing"].contains("early_stopped") &&
      doc["routing"]["early_stopped"].is_boolean()) {
    survey->early_stopped = doc["routing"]["early_stopped"].get<bool>();
  }
  if (doc.contains("scope") && doc["scope"].is_object()) {
    const auto& scope = doc["scope"];
    if (scope.contains("wavelength_nm") && scope["wavelength_nm"].is_number()) {
      survey->wavelength_nm = scope["wavelength_nm"].get<double>();
      survey->has_wavelength = std::isfinite(survey->wavelength_nm) && survey->wavelength_nm > 0.0;
    }
    survey->wavelength_source = scope.contains("wavelength_source") && scope["wavelength_source"].is_string() ?
                                    scope["wavelength_source"].get<std::string>() :
                                    "";
    if (scope.contains("geometry_kind")) {
      if (scope["geometry_kind"] == "realized")
        survey->geometry.kind = GeometryKind::kRealized;
      else if (scope["geometry_kind"] == "declared_population")
        survey->geometry.kind = GeometryKind::kDeclaredPopulation;
      else
        return refuse("corrupt warm document: unknown geometry kind");
    }
    if (scope.contains("sun_altitude_deg") && scope["sun_altitude_deg"].is_number()) {
      survey->sun_altitude_deg = scope["sun_altitude_deg"].get<double>();
      survey->has_sun_altitude = std::isfinite(survey->sun_altitude_deg);
    }
    if (scope.contains("sun_azimuth_deg") && scope["sun_azimuth_deg"].is_number()) {
      survey->sun_azimuth_deg = scope["sun_azimuth_deg"].get<double>();
      survey->has_sun_azimuth = std::isfinite(survey->sun_azimuth_deg);
    }
  }
  // ...the support (every support card's intervals) and the certified edge. Members are the
  // card's first layer, the entry faces the partition ran on — the same reading the in-memory
  // harvest arm spells, so the two arms answer identically on any document (the dual-arm oracle
  // test pins it). A support_set whose interval list harvests nothing reads as no support: the
  // flag is set once after the loop, from what actually landed.
  if (doc.contains("objects") && doc["objects"].is_array()) {
    for (const auto& card : doc["objects"]) {
      if (!card.is_object()) {
        continue;
      }
      if (card.contains("support_set") && card["support_set"].is_object() &&
          card["support_set"].contains("intervals") && card["support_set"]["intervals"].is_array()) {
        std::vector<int> members;
        if (card.contains("members") && card["members"].is_array() && !card["members"].empty() &&
            card["members"][0].is_array()) {
          for (const auto& face : card["members"][0]) {
            if (face.is_number_integer()) {
              members.push_back(face.get<int>());
            }
          }
        }
        for (const auto& interval : card["support_set"]["intervals"]) {
          if (!interval.is_object() || !interval.contains("lower_deg") || !interval["lower_deg"].is_number() ||
              !interval.contains("upper_deg") || !interval["upper_deg"].is_number()) {
            continue;
          }
          SurveyWarmSource::SupportInterval out;
          out.lower_deg = interval["lower_deg"].get<double>();
          out.upper_deg = interval["upper_deg"].get<double>();
          out.members = members;
          if (card.contains("members") && card["members"].is_array()) {
            for (const auto& member : card["members"]) {
              if (member.is_array() && std::all_of(member.begin(), member.end(),
                                                   [](const auto& face) { return face.is_number_integer(); })) {
                out.source_members.push_back(member.get<std::vector<int>>());
              }
            }
          }
          survey->intervals.push_back(std::move(out));
        }
      }
      if (card.contains("caustic_edge") && card["caustic_edge"].is_object() &&
          card["caustic_edge"].contains("position_deg") && card["caustic_edge"]["position_deg"].is_number()) {
        survey->has_edge = true;
        survey->edge_deg = card["caustic_edge"]["position_deg"].get<double>();
        if (card.contains("members") && card["members"].is_array()) {
          for (const auto& member : card["members"]) {
            if (member.is_array() &&
                std::all_of(member.begin(), member.end(), [](const auto& face) { return face.is_number_integer(); })) {
              survey->edge_members.push_back(member.get<std::vector<int>>());
            }
          }
        }
      }
    }
    survey->has_support = !survey->intervals.empty();
  }
  // ...the probe's radial profile.
  if (doc.contains("probes") && doc["probes"].is_array() && !doc["probes"].empty() && doc["probes"][0].is_object() &&
      doc["probes"][0].contains("samples") && doc["probes"][0]["samples"].is_array()) {
    const auto& probe = doc["probes"][0];
    survey->has_probe = true;
    if (probe.contains("spec") && probe["spec"].is_object()) {
      const auto& spec = probe["spec"];
      if (spec.contains("member_aggregation")) {
        if (spec["member_aggregation"] != "physical_class")
          return refuse("corrupt warm document: unknown member aggregation");
        survey->quantity.aggregation = MemberAggregation::kPhysicalClass;
      }
      if (spec.contains("measure_kind")) {
        if (spec["measure_kind"] != "ideal_frozen")
          return refuse("corrupt warm document: unknown probe measure kind");
        survey->quantity.measure = MeasureKind::kIdealFrozen;
      }
      if (spec.contains("azimuth_deg") && spec["azimuth_deg"].is_number()) {
        survey->probe_azimuth_deg = spec["azimuth_deg"].get<double>();
        survey->has_probe_azimuth = std::isfinite(survey->probe_azimuth_deg);
      } else if (!spec.contains("azimuth_deg") && probe.contains("kind") && probe["kind"] == "solar_radial" &&
                 doc.contains("generator") && doc["generator"].is_object() && doc["generator"].contains("lumice") &&
                 doc["generator"]["lumice"].is_string() &&
                 (doc["generator"]["lumice"] == "4.7.4-dev" || doc["generator"]["lumice"] == "4.7.5" ||
                  doc["generator"]["lumice"] == "4.7.5-dev") &&
                 doc["warm_index"]["produced_for"] == "lumice.path-survey schema 4, analytic api v8") {
        survey->has_probe_azimuth = true;
        survey->probe_azimuth_deg = 0.0;
      }
    }
    if (probe.contains("spec") && probe["spec"].is_object() && probe["spec"].contains("bin_deg") &&
        probe["spec"]["bin_deg"].is_number()) {
      survey->probe_bin_deg = probe["spec"]["bin_deg"].get<double>();
    }
    if (probe.contains("spec") && probe["spec"].is_object() && probe["spec"].contains("sample_count") &&
        probe["spec"]["sample_count"].is_number_integer()) {
      survey->probe_sample_count = static_cast<int>(probe["spec"]["sample_count"].get<long long>());
    }
    // Every row with a delta_deg lands in the harvest, the grid's shape preserved: a value the
    // writer spelled null (an undefined number) parses as NaN — the row exists in the document,
    // and a reader that dropped it would hand the next query a silently shorter grid. (The
    // in-memory harvest arm reads the same rows verbatim; this is the two arms agreeing.)
    for (const auto& sample : probe["samples"]) {
      if (!sample.is_object() || !sample.contains("delta_deg") || !sample["delta_deg"].is_number()) {
        continue;
      }
      SurveyWarmSource::ProbeSample out;
      out.delta_deg = sample["delta_deg"].get<double>();
      constexpr double nan = std::numeric_limits<double>::quiet_NaN();
      out.value = sample.contains("value") && sample["value"].is_number() ? sample["value"].get<double>() : nan;
      if (sample.contains("k") && sample["k"].is_number_integer()) {
        out.k = sample["k"].get<int>();
      }
      if (sample.contains("k_eff") && sample["k_eff"].is_number()) {
        out.k_eff = sample["k_eff"].get<double>();
      }
      survey->probe_samples.push_back(std::move(out));
    }
  }
  return {};
}

}  // namespace lumice::raypath
