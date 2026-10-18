#include "raypath/detail/survey/survey_json.hpp"

#include <nlohmann/json.hpp>

#include "raypath/detail/json_values.hpp"
#include "raypath/detail/survey/evidence.hpp"
#include "raypath/single_path_analysis.hpp"  // WavelengthSourceName

namespace lumice::raypath::survey {
namespace {

using Json = nlohmann::ordered_json;
using detail::Num;

// One evidence block. `state` is always written (a missing state must never read as settled);
// the accompanying field follows the state: certified -> method, refused -> mode, unevaluated ->
// reason, finite -> coverage. Nested initializer lists decay arrays inside nlohmann objects, so
// the array-valued parts of the document are assigned stepwise at their build sites.
Json EvidenceJson(const SurveyEvidence& evidence) {
  Json block = { { "state", EvidenceStateName(evidence.state) } };
  switch (evidence.state) {
    case EvidenceState::kCertified:
      block["method"] = evidence.method;
      break;
    case EvidenceState::kFinite:
      block["coverage"] = { { "members_evaluated", evidence.coverage.members_evaluated },
                            { "geometry_basis", evidence.coverage.geometry_basis },
                            { "wavelength_nm", Num(evidence.coverage.wavelength_nm) } };
      break;
    case EvidenceState::kRefused:
      block["mode"] = evidence.mode;
      break;
    case EvidenceState::kUnevaluated:
      block["reason"] = evidence.reason;
      break;
  }
  return block;
}

Json LayersJson(const std::vector<std::vector<int>>& layers) {
  Json array = Json::array();
  for (const auto& layer : layers) {
    Json faces = Json::array();
    for (int face : layer) {
      faces.push_back(face);
    }
    array.push_back(std::move(faces));
  }
  return array;
}

Json ObjectJson(const SurveyObject& object) {
  Json card = { { "kind", SurveyObjectKindName(object.kind) } };
  card["members"] = LayersJson(object.members);
  if (object.has_position) {
    card["position"] = { { "altitude_deg", Num(object.position_altitude_deg) },
                         { "azimuth_deg", Num(object.position_azimuth_deg) } };
  }
  card["evidence"] = EvidenceJson(object.evidence);
  if (object.certificate.has_value()) {
    card["certificate"] = { { "criterion", "fold matrix M = I and zero wedge (analytic IsRankZeroPath)" },
                            { "fold_tolerance", Num(object.certificate->fold_tolerance) },
                            { "wedge_tolerance_deg", Num(object.certificate->wedge_tolerance_deg) } };
  }
  if (object.support_set.has_value()) {
    Json intervals = Json::array();
    for (const SurveyObject::Interval& interval : object.support_set->intervals) {
      intervals.push_back({ { "lower_deg", Num(interval.lower_deg) },
                            { "upper_deg", Num(interval.upper_deg) },
                            { "n_components", interval.n_components },
                            { "n_closed", interval.n_closed },
                            { "n_open", interval.n_open } });
    }
    Json endpoints = Json::array();
    for (const SurveyObject::EndpointLabel& endpoint : object.support_set->endpoints) {
      endpoints.push_back({ { "delta_deg", Num(endpoint.delta_deg) },
                            { "source", endpoint.source },
                            { "profile", endpoint.profile },
                            { "multiplicity", endpoint.multiplicity } });
    }
    card["support_set"] = { { "intervals", std::move(intervals) },
                            { "endpoints", std::move(endpoints) },
                            { "symmetry_bits", object.support_set->symmetry_bits },
                            { "member_count", object.support_set->member_count } };
  }
  if (object.caustic_edge.has_value()) {
    Json edge = { { "position_deg", Num(object.caustic_edge->position_deg) },
                  { "profile", object.caustic_edge->profile },
                  { "coincident_with_support", object.caustic_edge->coincident_with_support } };
    if (object.caustic_edge->has_measure_limit) {
      edge["measure_limit"] = Num(object.caustic_edge->measure_limit);
    }
    if (object.caustic_edge->certificate.has_value()) {
      const SurveyObject::EdgeCertificate& certificate = *object.caustic_edge->certificate;
      edge["certificate"] = { { "formula", certificate.formula },
                              { "apex_angle_deg", Num(certificate.apex_angle_deg) },
                              { "cross_check_deg", Num(certificate.cross_check_deg) },
                              { "tolerance_deg", Num(certificate.tolerance_deg) } };
    }
    card["caustic_edge"] = std::move(edge);
  }
  if (object.bright_domain.has_value()) {
    Json domain = { { "domain_lower_deg", Num(object.bright_domain->domain_lower_deg) },
                    { "domain_upper_deg", Num(object.bright_domain->domain_upper_deg) } };
    if (object.bright_domain->has_peak) {
      domain["peak"] = { { "delta_deg", Num(object.bright_domain->peak_delta_deg) },
                         { "value", Num(object.bright_domain->peak_value) } };
    }
    if (object.bright_domain->has_edge_reading) {
      domain["edge_reading_deg"] = Num(object.bright_domain->edge_delta_deg);
    }
    domain["kept_count"] = object.bright_domain->kept_count;
    if (object.bright_domain->probe_index >= 0) {
      domain["probe_index"] = object.bright_domain->probe_index;
    }
    card["bright_domain"] = std::move(domain);
  }
  return card;
}

Json VerdictJson(const SurveyMemberVerdict& verdict) {
  Json entry = { { "member", LayersJson({ verdict.member })[0] },
                 { "rank_zero", verdict.rank_zero },
                 { "mechanism", verdict.mechanism.empty() ? Json(nullptr) : Json(verdict.mechanism) } };
  if (!verdict.mechanism_unavailable.empty()) {
    entry["mechanism_unavailable"] = verdict.mechanism_unavailable;
  }
  entry["evidence"] = EvidenceJson(verdict.evidence);
  return entry;
}

Json UnevaluatedJson(const SurveyUnevaluated& entry) {
  Json kinds = Json::array();
  for (const SurveyObjectKind kind : entry.kinds) {
    kinds.push_back(SurveyObjectKindName(kind));
  }
  return { { "kinds", std::move(kinds) }, { "reason", entry.reason } };
}

Json ProbeJson(const SurveyProbe& probe) {
  Json spec = { { "delta_min_deg", Num(probe.spec.delta_min_deg) },
                { "delta_max_deg", Num(probe.spec.delta_max_deg) },
                { "bin_deg", Num(probe.spec.bin_deg) },
                { "wavelength_nm", Num(probe.spec.wavelength_nm) },
                { "refractive_index", Num(probe.spec.refractive_index) },
                { "sample_count", probe.spec.sample_count },
                { "lattice", probe.spec.lattice },
                { "deterministic", probe.spec.deterministic },
                { "representative", LayersJson({ probe.spec.representative })[0] },
                { "member_count", probe.spec.member_count } };
  Json samples = Json::array();
  for (const SurveyProbeSample& sample : probe.samples) {
    samples.push_back({ { "delta_deg", Num(sample.delta_deg) },
                        { "value", Num(sample.value) },
                        { "k", sample.k },
                        { "k_eff", Num(sample.k_eff) } });
  }
  Json out = { { "kind", probe.kind },
               { "spec", std::move(spec) },
               { "evidence", EvidenceJson(probe.evidence) },
               { "samples", std::move(samples) } };
  if (probe.has_edge) {
    out["edge"] = { { "bin", probe.edge_bin }, { "delta_deg", Num(probe.edge_delta_deg) } };
  }
  if (probe.has_peak) {
    out["peak"] = { { "delta_deg", Num(probe.peak_delta_deg) }, { "value", Num(probe.peak_value) } };
  }
  out["separation_deg"] = Num(probe.separation_deg);
  out["kept_count"] = probe.kept_count;
  return out;
}

Json DiagnoseJson(const SurveyDiagnoseBlock& block) {
  Json features = Json::array();
  for (const SurveyDiagnoseFeature& feature : block.features) {
    features.push_back({ { "kind", feature.kind },
                         { "source", feature.source },
                         { "color", feature.color },
                         { "visible", feature.visible },
                         { "delta_red_deg", Num(feature.delta_red_deg) },
                         { "delta_blue_deg", Num(feature.delta_blue_deg) },
                         { "shift_deg", Num(feature.shift_deg) } });
  }
  Json notes = Json::array();
  for (const std::string& note : block.notes) {
    notes.push_back(note);
  }
  Json out = { { "verdict_kind", block.verdict_kind },
               { "color", block.color },
               { "visible", block.visible },
               { "coverage_complete", block.coverage_complete },
               { "notes", std::move(notes) },
               { "n_red", Num(block.n_red) },
               { "n_blue", Num(block.n_blue) },
               { "features", std::move(features) },
               { "evidence", EvidenceJson(block.evidence) } };
  if (block.has_position) {
    out["position_deg"] = Num(block.position_deg);
  }
  return out;
}

}  // namespace

std::string SurveyDocumentToJson(const SurveyDocument& document, const char* lumice_version) {
  Json doc = { { "schema", "lumice.path-survey" },
               { "schema_version", document.schema_version },
               { "generator", { { "lumice", lumice_version }, { "analytic_api_version", analytic::kApiVersion } } } };

  doc["request"] = { { "crystal_id", document.request.crystal_id },
                     { "path_layers", LayersJson(document.request.path_layers) },
                     { "symmetry_bits", document.request.symmetry_bits },
                     { "wavelength_nm",
                       document.request.wavelength_given ? Json(Num(document.request.wavelength_nm)) : Json(nullptr) },
                     { "budget_ms", document.request.budget_ms } };

  doc["scope"] = { { "scene_identity", document.scene_identity },
                   { "scene_layer", document.scene_layer.has_value() ? Json(*document.scene_layer) : Json(nullptr) },
                   { "members", LayersJson(document.members) },
                   { "wavelength_nm", Num(document.wavelength_nm) },
                   { "wavelength_source", document.wavelength_source },
                   { "refractive_index", Num(document.refractive_index) },
                   { "sun_altitude_deg", Num(document.sun_altitude_deg) },
                   { "sun_azimuth_deg", Num(document.sun_azimuth_deg) } };

  Json routing = { { "early_stopped", document.early_stopped } };
  Json verdicts = Json::array();
  for (const SurveyMemberVerdict& verdict : document.member_verdicts) {
    verdicts.push_back(VerdictJson(verdict));
  }
  routing["members"] = std::move(verdicts);
  doc["routing"] = std::move(routing);

  Json objects = Json::array();
  for (const SurveyObject& object : document.objects) {
    objects.push_back(ObjectJson(object));
  }
  doc["objects"] = std::move(objects);

  Json unevaluated = Json::array();
  for (const SurveyUnevaluated& entry : document.unevaluated) {
    unevaluated.push_back(UnevaluatedJson(entry));
  }
  doc["unevaluated"] = std::move(unevaluated);

  // Probes are a T1-leg product: an early-stop document carries no probes key at all (the
  // schema-4 append discipline keeps the v1 documents byte-stable).
  if (!document.probes.empty()) {
    Json probes = Json::array();
    for (const SurveyProbe& probe : document.probes) {
      probes.push_back(ProbeJson(probe));
    }
    doc["probes"] = std::move(probes);
  }

  doc["chromatic"] = EvidenceJson(document.chromatic);
  if (document.positional_dispersion.has_value()) {
    const SurveyPositionalDispersion& dispersion = *document.positional_dispersion;
    doc["chromatic"]["positional_dispersion"] = {
      { "red_label", dispersion.red_label },
      { "blue_label", dispersion.blue_label },
      { "n_red", Num(dispersion.n_red) },
      { "n_blue", Num(dispersion.n_blue) },
      { "value_red_deg", Num(dispersion.value_red_deg) },
      { "value_blue_deg", Num(dispersion.value_blue_deg) },
      { "displacement_deg", Num(dispersion.displacement_deg) },
      { "direction", dispersion.direction },
      { "evidence", EvidenceJson(dispersion.evidence) },
    };
  }
  if (document.diagnose.has_value()) {
    doc["chromatic"]["diagnose"] = DiagnoseJson(*document.diagnose);
  }
  if (document.weight_tint.has_value()) {
    doc["chromatic"]["weight_tint"] = EvidenceJson(*document.weight_tint);
  }
  doc["evidence"] = EvidenceJson(document.evidence);
  doc["budget"] = { { "requested_ms", document.budget_ms }, { "enforced", false }, { "note", document.budget_note } };
  doc["timing"] = { { "total_seconds", Num(document.total_seconds) } };
  return doc.dump();
}

}  // namespace lumice::raypath::survey
