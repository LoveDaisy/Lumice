#ifndef LUMICE_RAYPATH_SURVEY_RESULT_HPP_
#define LUMICE_RAYPATH_SURVEY_RESULT_HPP_

#include <algorithm>
#include <stdexcept>

#include "raypath/domain/result_identity.hpp"
#include "raypath/results/quantity.hpp"
#include "raypath/results/survey_records.hpp"
namespace lumice::raypath::survey {

struct SurveyData {
  std::shared_ptr<const QuantityContext> quantity_context;
  GeometryScope geometry;
  int schema_version = 4;

  SurveyRequestEcho request;

  // Scope: the input the routing ran on.
  std::string scene_identity;
  std::optional<long long> scene_layer;   // absent = a standalone configured crystal
  std::vector<std::vector<int>> members;  // the resolved class members
  double wavelength_nm = 0.0;
  std::string wavelength_source;  // WavelengthSourceName
  double refractive_index = 0.0;
  std::optional<double> sun_altitude_deg;
  std::optional<double> sun_azimuth_deg;

  // Routing: the T0 leg.
  bool early_stopped = false;
  std::vector<SurveyMemberVerdict> member_verdicts;
  std::vector<SurveyObject> objects;
  std::vector<SurveyUnevaluated> unevaluated;

  // The canonical probes (the survey's quantification layer): filled only by a routing arm
  // that ran them; an early-stop document carries none.
  std::vector<SurveyProbe> probes;

  // The exception evidence legs (the schema-4 appends): document-level optional blocks beside
  // family_routing, each carrying its own SurveyEvidence. A key exists exactly when its leg
  // ran (the T1 arm and the refused-support arm run the TIR share; the MC region ESS runs
  // only when requested) — an arm that did not run a leg leaves no key at all, which reads as
  // "this arm does not carry the leg", never as a refusal.
  std::optional<TirShareResult> tir_share;
  std::optional<McRegionEssResult> mc_region_ess;

  // The family routing block (the measure-to-family arm's own record): present only on a
  // document that arm produced — an early stop, a full-dimensional route and an
  // outside-domain refusal carry no family_routing key at all.
  std::optional<FamilyRoutingBlock> family_routing;

  // The warm index (the --warm contract's survey side): always filled by the assembly — the
  // seeds the arms held (empty when none), the scope/producer receipts and the config
  // fingerprint. A reader treats a missing or incomplete key on the wire as the documented
  // warm_index_absent degradation, never as refusal material.
  WarmIndexBlock warm_index;

  // The chromatic layer: the block-level evidence plus the optional legs. A leg absent from
  // the document is one the routing arm did not run — the sub-block keys never appear without
  // their content. Block-state rules by arm: the T1 arm's chromatic legs turn the block finite
  // (or refused on a leg's hard failure); the identity arm's only leg is the tint, so the block
  // mirrors the tint's own state — finite with coverage when it computed, refused with the tint's
  // mode when it refused (the block-level state and the sub-block can never disagree).
  SurveyEvidence chromatic;
  std::optional<SurveyPositionalDispersion> positional_dispersion;
  std::optional<SurveyDiagnoseBlock> diagnose;
  std::optional<WeightTintResult> weight_tint;

  // The document-level evidence: the T0 routing verdict's own state.
  SurveyEvidence evidence;

  // Meta.
  int budget_ms = 0;  // as resolved
  double total_seconds = 0.0;
  // The budget note text, owned by the arm that produced the document: the default is the v1
  // early-stop wording (whose claim is true of that arm), and the T1 arm states its own legs.
  std::string budget_note =
      "the v1 survey's only computation is the millisecond T0 routing leg, which is no budget "
      "consumer; budget enforcement lands with the band-sum/T3 quantification legs";
};


// A completed product owns one immutable value. Copies preserve identity and storage;
// construction from revised assembly data creates a different snapshot.
class SurveyResult : public FrozenValue<SurveyData> {
 public:
  SurveyResult() : SurveyResult(SurveyData{}) {}
  explicit SurveyResult(SurveyData data) : FrozenValue<SurveyData>(Validated(std::move(data))) {}
  std::optional<std::vector<MemberRef>> ObjectMembers(ObjectRef ref) const {
    const auto* object = Resolve(ref);
    if (!object)
      return std::nullopt;
    std::vector<MemberRef> members;
    for (const auto& path : object->members) {
      const auto found = std::find(Data().members.begin(), Data().members.end(), path);
      if (found == Data().members.end())
        return std::nullopt;
      members.push_back({ Identity(), MemberId{ static_cast<size_t>(found - Data().members.begin()) } });
    }
    return members;
  }
  const SurveyProbe* ProbeFor(ObjectRef ref) const {
    const auto* object = Resolve(ref);
    const auto* domain = object ? object->Bright() : nullptr;
    return domain && domain->probe_index >= 0 && static_cast<size_t>(domain->probe_index) < Data().probes.size() ?
               &Data().probes[static_cast<size_t>(domain->probe_index)] :
               nullptr;
  }
  std::optional<ObjectRef> Reference(ObjectId id) const {
    return id.ordinal < Data().objects.size() ? std::optional<ObjectRef>{ { Identity(), id } } : std::nullopt;
  }
  const SurveyObject* Resolve(ObjectRef ref) const {
    return ref.result == Identity() && ref.object.ordinal < Data().objects.size() ?
               &Data().objects[ref.object.ordinal] :
               nullptr;
  }

  std::optional<MemberRef> Reference(MemberId id) const {
    return id.ordinal < Data().members.size() ? std::optional<MemberRef>{ { Identity(), id } } : std::nullopt;
  }
  const std::vector<int>* Resolve(MemberRef ref) const {
    return ref.result == Identity() && ref.member.ordinal < Data().members.size() ?
               &Data().members[ref.member.ordinal] :
               nullptr;
  }

 private:
  static SurveyData Validated(SurveyData data) {
    for (const SurveyObject& object : data.objects) {
      if (!object.AdjunctsValid())
        throw std::invalid_argument("inconsistent survey object adjuncts");
    }
    return data;
  }
};

}  // namespace lumice::raypath::survey

#endif  // LUMICE_RAYPATH_SURVEY_RESULT_HPP_
