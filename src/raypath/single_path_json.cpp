#include "raypath/single_path_json.hpp"

#include <nlohmann/json.hpp>
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

nlohmann::ordered_json Conventions() {
  return {
    { "frames",
      "world: +z is the zenith, azimuth counter-clockwise from +x seen from +z; body: the crystal frame, +z its "
      "c-axis (face 1's outward normal) (doc/coordinate-convention.md)" },
    { "directions",
      "unit 3-vectors follow the light-travel convention (doc/coordinate-convention.md, Direction-Vector "
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
    { "sun_grid",
      "cell centres, row-major [i][j]: lat_i = -90 + (i + 0.5) * 180 / lat_count, lon_j = -180 + (j + 0.5) * 360 / "
      "lon_count, u = (cos lat cos lon, cos lat sin lon, sin lat) in the body frame; the longitude seam is periodic; "
      "arrays rounded to 9 significant digits" },
    { "completeness", kCompletenessNote },
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
  };
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

std::string ToJson(const SinglePathResult& result, const std::string& product_version) {
  nlohmann::ordered_json doc;
  doc[kKeySchemaVersion] = result.meta.schema_version;
  doc["generator"] = { { "lumice", product_version }, { "analytic_api_version", result.meta.analytic_api_version } };
  doc["conventions"] = Conventions();
  doc["meta"] = MetaJson(result.meta);
  if (result.outcome == Outcome::kPointMass) {
    // Rank 0: no fiber is traced, so there are no components and no discovery.
    doc["outcome"] = "point_mass";
    const PointMass& pm = result.point_mass;
    doc["point_mass"] = { { "direction", Array(pm.direction, 3) },
                          { "altitude_deg", Num(pm.altitude_deg) },
                          { "azimuth_deg", Num(pm.azimuth_deg) },
                          { "target_separation_deg", Num(pm.target_separation_deg) } };
  } else {
    doc["outcome"] = "discovered";
    nlohmann::ordered_json components = nlohmann::ordered_json::array();
    for (const auto& c : result.components) {
      components.push_back(ComponentJson(c));
    }
    doc[kKeyComponents] = std::move(components);
    nlohmann::ordered_json incomplete = nlohmann::ordered_json::array();
    for (const auto& c : result.incomplete) {
      incomplete.push_back(IncompleteJson(c));
    }
    doc[kKeyIncomplete] = std::move(incomplete);
    doc["discovery"] = DiscoveryJson(result.discovery);
    doc["reach"] = ReachJson(result.reach);
  }
  // Absent when the grid was not asked for (--grid 0).
  if (result.sun_grid.lat_count > 0) {
    doc["sun_grid"] = SunGridJson(result.sun_grid);
  }
  return doc.dump();
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

}  // namespace lumice::raypath
