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

  doc["chromatic"] = EvidenceJson(document.chromatic);
  doc["evidence"] = EvidenceJson(document.evidence);
  doc["budget"] = { { "requested_ms", document.budget_ms },
                    { "enforced", false },
                    { "note",
                      "the v1 survey's only computation is the millisecond T0 routing leg, which is no budget "
                      "consumer; budget enforcement lands with the band-sum/T3 quantification legs" } };
  doc["timing"] = { { "total_seconds", Num(document.total_seconds) } };
  return doc.dump();
}

}  // namespace lumice::raypath::survey
