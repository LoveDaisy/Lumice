#include "raypath/probe_json.hpp"

#include <nlohmann/json.hpp>
#include <string>

#include "raypath/detail/json_values.hpp"
#include "raypath/single_path_json.hpp"

namespace lumice::raypath {

namespace {

using detail::Array;
using detail::Num;

nlohmann::ordered_json Conventions() {
  return {
    { "frames",
      "world: +z is the zenith, azimuth counter-clockwise from +x seen from +z; the probe line lives in "
      "the (deviation, azimuth-about-the-sun) plane about the sun's propagation vector, in the band-sum "
      "pixel convention (doc/geometry/coordinate-convention.md)" },
    { "directions",
      "unit 3-vectors follow the light-travel convention (Direction-Vector Semantics): "
      "incident_direction is the propagation sun -> crystal; at_direction is the propagation of light "
      "arriving from the at sky point; for any such d that point sits at altitude asin(-d.z), azimuth "
      "atan2(y, x) - 180" },
    { "at_azimuth", "measured as the sun's azimuth is (the `analyze --center` convention)" },
    { "line_azimuth",
      "azimuth about the sun, degrees, in the probe frame the engine builds from the incident "
      "direction; the profile's spec.azimuth_deg is the axis forms' line azimuth and the arc forms' "
      "centre azimuth" },
    { "forms",
      "radial / edge-normal are delta-axis lines at the line's azimuth (edge-normal windows the same "
      "geometry at the context's nearest edge); arc / tangential are constant-delta arcs swept over "
      "azimuth; each profile's spec.semantics states what its form claims" },
    { "value",
      "per bin, per sr (the band-sum value convention; a uniform density's value is the "
      "full-annulus mean, azimuth-free)" },
    { "dark_regions",
      "each profile's bins are classified against support at that profile's wavelength; the "
      "reference context is not borrowed across the lambda grid. Inside_support_positive means "
      "events, inside_support_zero means no events inside support, outside_support is geometrically "
      "dark, unknown means no support certificate at this wavelength (named reason). Segment "
      "bounds use deviation for axis forms and azimuth for arc forms" },
    { "source",
      "warm_reused: the values are the warm document's own radial samples, checked bin-centre "
      "against the asked grid by the schema constants (bin, count, extent; centre tolerance far "
      "below the bin — a one-ulp platform wobble is representation noise, not another grid) — zero "
      "BandSum calls. recomputed: BandSum ran here (lattice-deterministic, no seed dependence). "
      "Reuse requires the same wavelength and sun, bin layout, and sample count. Non-uniform "
      "measures also require a known matching line azimuth; only the uniform density is "
      "azimuth-free. A refused profile omits the source key" },
    { "null", "a number that is not defined (NaN) is written as null" },
  };
}

nlohmann::ordered_json MetaJson(const ProbeMetadata& m) {
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
  nlohmann::ordered_json wavelengths = nlohmann::ordered_json::array();
  for (const auto& w : m.wavelengths) {
    wavelengths.push_back({ { "nm", Num(w.nm) },
                            { "source", WavelengthSourceName(w.source) },
                            { "refractive_index", Num(w.refractive_index) } });
  }
  return {
    { "crystal", crystal },
    { "faces", m.faces },
    { "sun",
      { { "altitude_deg", Num(m.sun_altitude_deg) },
        { "azimuth_deg", Num(m.sun_azimuth_deg) },
        { "incident_direction", Array(m.incident_direction, 3) } } },
    { "at",
      { { "altitude_deg", Num(m.at_altitude_deg) },
        { "azimuth_deg", Num(m.at_azimuth_deg) },
        { "direction", Array(m.at_direction, 3) },
        { "deviation_deg", Num(m.at_deviation_deg) },
        { "azimuth_about_sun_deg", Num(m.at_azimuth_about_sun_deg) } } },
    { "wavelengths", std::move(wavelengths) },
    { "budget_ms", m.budget_ms },
    { "interrupted", m.interrupted },
  };
}

nlohmann::ordered_json ProfileJson(const ProbeProfile& p) {
  nlohmann::ordered_json spec = {
    { "delta_min_deg", Num(p.delta_min_deg) },
    { "delta_max_deg", Num(p.delta_max_deg) },
    { "azimuth_deg", Num(p.azimuth_deg) },
    { "window_half_width_deg", Num(p.window_half_width_deg) },
    { "bin_deg", Num(p.bin_deg) },
    { "wedge_half_width_deg", Num(p.wedge_half_width_deg) },
    { "semantics", p.semantics },
    { "wavelength_nm", Num(p.wavelength_nm) },
    { "refractive_index", Num(p.refractive_index) },
    { "sample_count", p.sample_count },
    { "lattice", p.lattice },
    { "deterministic", p.deterministic },
    { "representative", p.representative },
    { "member_count", p.member_count },
  };
  if (p.anchor_basis.has_value()) {
    spec["anchor_basis"] = ContextEdgeBasisName(*p.anchor_basis);
    spec["anchor_deg"] = Num(p.anchor_deg);
    spec["anchor_wavelength_nm"] = Num(p.anchor_wavelength_nm);
  }
  nlohmann::ordered_json j = { { "along", ProbeAlongName(p.along) }, { "spec", std::move(spec) } };
  if (p.State() == survey::EvidenceState::kRefused) {
    // No source key on a refusal: nothing was reused or computed, and the empty string the
    // default member would serialize is a third value the vocabulary never named.
    j["evidence"] = { { "state", "refused" }, { "mode", p.RefusalReason() } };
    return j;
  }
  if (p.State() == survey::EvidenceState::kUnevaluated) {
    j["evidence"] = { { "state", "unevaluated" }, { "reason", std::get<Unevaluated>(p.claim).reason } };
    return j;
  }
  const ProbeFiniteReading& reading = *ClaimValue(p.claim);
  j["source"] = reading.source == ProbeProfileSource::kWarmReused ? "warm_reused" : "recomputed";
  if (const auto* certified = std::get_if<Certified<ProbeFiniteReading>>(&p.claim)) {
    j["evidence"] = { { "state", "certified" }, { "method", certified->method } };
  } else {
    j["evidence"] = {
      { "state", "finite" },
      { "coverage",
        { { "members_evaluated", reading.members_evaluated },
          { "geometry_basis", "nominal crystal shape at the distribution centres" },
          { "wavelength_nm", Num(p.wavelength_nm) },
          { "lattice_n", p.sample_count },
          { "bin_deg", Num(p.bin_deg) },
          { "delta_domain_deg", { Num(p.delta_min_deg), Num(p.delta_max_deg) } },
          { "deterministic", p.deterministic },
          { "representative", p.representative },
          { "member_count", p.member_count } } },
    };
  }
  nlohmann::ordered_json samples = nlohmann::ordered_json::array();
  for (const auto& s : reading.samples) {
    samples.push_back({ { "delta_deg", Num(s.delta_deg) },
                        { "azimuth_deg", Num(s.azimuth_deg) },
                        { "value", Num(s.value) },
                        { "k", s.k },
                        { "k_eff", Num(s.k_eff) } });
  }
  j["samples"] = std::move(samples);
  nlohmann::ordered_json readings = { { "has_edge", reading.readings.has_edge },
                                      { "has_peak", reading.readings.has_peak } };
  if (reading.readings.has_edge) {
    readings["edge_deg"] = Num(reading.readings.edge_deg);
  }
  if (reading.readings.has_peak) {
    readings["peak_deg"] = Num(reading.readings.peak_deg);
    readings["peak_value"] = Num(reading.readings.peak_value);
    readings["separation_deg"] = Num(reading.readings.separation_deg);
  }
  j["readings"] = std::move(readings);
  nlohmann::ordered_json segments = nlohmann::ordered_json::array();
  for (const auto& s : reading.dark_regions) {
    nlohmann::ordered_json segment = { { "kind", ProbeDarkKindName(s.kind) },
                                       { "lo_deg", Num(s.lo_deg) },
                                       { "hi_deg", Num(s.hi_deg) },
                                       { "bin_count", s.bin_count } };
    if (!s.reason.empty()) {
      segment["reason"] = s.reason;
    }
    segments.push_back(std::move(segment));
  }
  j["dark_regions"] = { { "segments", std::move(segments) } };
  return j;
}

}  // namespace

std::string ProbeToJson(const ProbeResult& value, const std::string& product_version) {
  const ProbeData& result = value.Data();
  nlohmann::ordered_json doc;
  doc["schema"] = "lumice.path-probe";
  doc["schema_version"] = 5;
  doc["generator"] = { { "lumice", product_version }, { "analytic_api_version", result.meta.analytic_api_version } };
  doc["conventions"] = Conventions();
  doc["meta"] = MetaJson(result.meta);
  doc["context"] = SurveyContextJson(result.context);
  nlohmann::ordered_json profiles = nlohmann::ordered_json::array();
  for (const ProbeProfile& p : result.profiles) {
    profiles.push_back(ProfileJson(p));
  }
  doc["profiles"] = std::move(profiles);
  if (result.has_red_edge_width) {
    doc["derived"] = { { "red_edge_width_deg", Num(result.red_edge_width_deg) },
                       { "rule",
                         "the 650 nm profile's edge reading minus the reference (the first requested "
                         "wavelength) profile's edge reading, both present" } };
  }
  return doc.dump();
}

}  // namespace lumice::raypath
