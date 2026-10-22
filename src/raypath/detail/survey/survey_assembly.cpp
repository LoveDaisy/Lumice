#include "raypath/detail/survey/survey_assembly.hpp"

#include <algorithm>
#include <chrono>

#include "analytic/dp_boundary.hpp"
#include "analytic/dp_focus.hpp"
#include "analytic/path_rank.hpp"
#include "analytic/pose_density.hpp"
#include "raypath/detail/input_assembly.hpp"
#include "raypath/detail/physical_member_scope.hpp"
#include "raypath/detail/survey/survey_family.hpp"
#include "raypath/detail/survey/survey_mc_ess.hpp"
#include "raypath/detail/survey/survey_probe.hpp"
#include "raypath/detail/survey/survey_t1.hpp"
#include "raypath/detail/survey/survey_tint.hpp"
#include "raypath/detail/survey/survey_tir_share.hpp"
#include "raypath/detail/survey/survey_witness.hpp"
#include "raypath/scene_to_analytic.hpp"
#include "util/logger.hpp"

namespace lumice::raypath::survey {
namespace {

bool ValidPathLayers(const std::vector<std::vector<int>>& layers) {
  return !layers.empty() && std::all_of(layers.begin(), layers.end(), [](const auto& layer) {
    return layer.size() >= 2 && layer.size() <= 64 &&
           std::all_of(layer.begin(), layer.end(), [](int face) { return face > 0; });
  });
}

// The T0 verdict for one member: the kernel's classification, recorded with its own evidence
// state. `density` may be inexpressible — the rank-0 check is pure geometry and does not consume
// it, so the certificate arm runs either way and only the mechanism roll-up depends on it.
SurveyMemberVerdict MemberVerdictOf(const analytic::FaceNormalTable& table, const analytic::FacePolygonTable& polygons,
                                    const std::vector<int>& member, const PoseDensityConversion& density,
                                    double refractive_index, bool* any_escaped, std::string* escape_mode) {
  SurveyMemberVerdict verdict;
  verdict.member = member;
  std::vector<int> slots;
  // A class member is a relabeling of the representative under the same crystal, so its slots
  // resolve whenever the representative's do. A failure here is therefore an internal
  // inconsistency rather than a member property; the member is refused as such (mode
  // "member_resolution") — never an empty or defaulted verdict.
  const Error slot_error = ResolveSingleLayerPath({ member }, table, &slots);
  if (!slot_error.Ok()) {
    verdict.rank_zero = false;
    verdict.mechanism_unavailable = "member face sequence did not resolve: " + slot_error.message;
    verdict.evidence.state = EvidenceState::kRefused;
    verdict.evidence.mode = "member_resolution";
    *any_escaped = true;
    *escape_mode = "member_resolution";
    return verdict;
  }
  if (density.Ok()) {
    const analytic::FocusingClassification classification = analytic::Classify(
        table, polygons, slots.data(), static_cast<int>(slots.size()), density.spec, refractive_index);
    verdict.rank_zero = !classification.escaped && classification.halo_map_rank == 0;
    if (verdict.rank_zero) {
      verdict.mechanism = classification.Mechanism();
      verdict.evidence.state = EvidenceState::kCertified;
      verdict.evidence.method = verdict.mechanism;
      return verdict;
    }
    if (classification.escaped) {
      verdict.evidence.state = EvidenceState::kRefused;
      verdict.evidence.mode = analytic::WalkStatusName(classification.escape_status);
      *any_escaped = true;
      *escape_mode = verdict.evidence.mode;
      return verdict;
    }
    verdict.mechanism = classification.Mechanism();
    verdict.evidence.state = EvidenceState::kFinite;
    verdict.evidence.coverage.members_evaluated = 1;
    return verdict;
  }
  // The axis is not expressible as a pose density: the rank-0 certificate still runs (pure
  // geometry), the mechanism roll-up does not, and the card says why rather than staying silent.
  verdict.rank_zero = analytic::IsRankZeroPath(table, slots.data(), static_cast<int>(slots.size()));
  if (verdict.rank_zero) {
    verdict.mechanism = kCertifiedMethodPointMass;
    verdict.evidence.state = EvidenceState::kCertified;
    verdict.evidence.method = kCertifiedMethodPointMass;
    return verdict;
  }
  verdict.mechanism_unavailable = "axis distribution not expressible as a pose density: " + density.unsupported;
  verdict.evidence.state = EvidenceState::kFinite;
  verdict.evidence.coverage.members_evaluated = 1;
  return verdict;
}

// The two TIR-share call sites below build the same input (the leg's own defaults carry the
// rest), so the wiring stays one line per arm like the ESS leg's shared wrapper.
TirShareInput TirShareInputOf(const ConfigManager& config, const analytic::FaceNormalTable& table,
                              const analytic::FacePolygonTable& polygons, const analytic::CrystalShape& crystal,
                              const std::vector<int>& representative, int member_count) {
  TirShareInput input;
  input.table = &table;
  input.polygons = &polygons;
  input.crystal = &crystal;
  input.representative = &representative;
  SunIncidentDirection(config.scene_.light_source_.param_, input.incident);
  input.member_count = member_count;
  return input;
}

// The opt-in MC region ESS leg's shared assembly (both T1 arms run it when the request asked):
// the survey's own scene snapshot at its resolved reference lambda, with the at-sun direction
// the delta bands read. Appends the leg's note to the budget note the caller already wrote —
// the note names what actually ran: a refused leg never claims the draws it did not run.
void AssembleMcRegionEssWhenRequested(const ConfigManager& config, const InputSnapshot& snapshot,
                                      const SurveyRequest& request, int member_count, double wavelength_nm,
                                      SurveyDocument* out) {
  if (request.mc_ess_draws <= 0) {
    return;
  }
  double sun_hat[3];
  SunIncidentDirection(config.scene_.light_source_.param_, sun_hat);
  McRegionEssInput ess_input;
  ess_input.snapshot = &snapshot;
  ess_input.wavelength_nm = wavelength_nm;
  for (int k = 0; k < 3; k++) {
    ess_input.sun_dir[static_cast<size_t>(k)] = -sun_hat[k];
  }
  ess_input.member_count = member_count;
  ess_input.draws = request.mc_ess_draws;
  ess_input.budget_ms = request.budget_ms;
  AssembleMcRegionEss(ess_input, out);
  const SurveyMcRegionEss& ess = *out->mc_region_ess;
  if (ess.evidence.state == EvidenceState::kFinite) {
    out->budget_note += " The requested MC region ESS leg ran " + std::to_string(request.mc_ess_draws) +
                        " seeded draws at the reference lambda; it is the one budget-bounded leg (the "
                        "survey budget is its deadline guardrail) — the enumeration's unenforced legs "
                        "are the others.";
  } else {
    out->budget_note += " The requested MC region ESS leg was refused (" + ess.evidence.mode +
                        "): " + std::to_string(request.mc_ess_draws) +
                        " draws were requested and no bands were published.";
  }
}

}  // namespace

Error AssembleSurvey(const ConfigManager& config, const SurveyRequest& request, SurveyDocument* out) {
  if (out == nullptr) {
    return { ErrorCode::kInvalidArgument, "null document output" };
  }
  *out = {};
  const auto begin = std::chrono::steady_clock::now();

  if (!ValidPathLayers(request.path_layers)) {
    return { ErrorCode::kInvalidPath, "non-empty layers with 2..64 positive face numbers are required" };
  }
  if (request.symmetry_bits > 7 || request.budget_ms <= 0 || request.budget_ms > kMaxSurveyBudgetMs) {
    return { ErrorCode::kInvalidArgument, "invalid survey symmetry bits or budget request" };
  }
  if (request.mc_ess_draws < 0 || request.mc_ess_draws > kSurveyMcEssMaxDraws) {
    return { ErrorCode::kInvalidArgument, "invalid survey mc ess draws request" };
  }

  const auto configured = config.crystals_.find(request.crystal_id);
  if (configured == config.crystals_.end()) {
    return { ErrorCode::kUnknownCrystalId, "unknown crystal id" };
  }

  // The wavelength: the user's if given, else the scene's single discrete wavelength, else the
  // default — the same resolution the target-fiber request spells, with its error.
  WavelengthChoice wavelength;
  if (Error error = ResolveWavelength(config.scene_.light_source_, request.wavelength_nm, &wavelength); !error.Ok()) {
    return error;
  }

  // The nominal crystal's geometry: the rank-0 certificate is algebraic on this shape.
  const CrystalConversion crystal = ConvertCrystal(configured->second.param_);
  analytic::FaceNormalTable table;
  analytic::FacePolygonTable polygons;
  switch (analytic::BuildFaceNormals(crystal.shape, &table, &polygons)) {
    case analytic::Status::kOk:
      break;
    case analytic::Status::kInvalidConfig:
      return { ErrorCode::kCrystalRejected,
               "the engine builds no crystal from entry " + std::to_string(request.crystal_id) + "'s nominal shape" };
    case analytic::Status::kInvalidValue:
      return { ErrorCode::kCrystalRejected, "crystal entry " + std::to_string(request.crystal_id) +
                                                "'s nominal shape has a value the kernel cannot take" };
  }
  // The request's own path: single layer, faces the crystal has (the report's error semantics).
  std::vector<int> slots;
  if (Error error = ResolveSingleLayerPath(request.path_layers, table, &slots); !error.Ok()) {
    return error;
  }

  // The class: physical P|B|D by default, the literal sequence at bits 0. A configured but
  // unused crystal stays a valid standalone diagnostic object (the report's layer rule).
  SceneConfig scene = config.scene_;
  size_t layer_index = scene.ms_.size();
  for (size_t i = 0; i < scene.ms_.size(); ++i) {
    if (std::any_of(scene.ms_[i].setting_.begin(), scene.ms_[i].setting_.end(),
                    [&](const auto& setting) { return setting.crystal_.id_ == request.crystal_id; })) {
      layer_index = i;
      break;
    }
  }
  if (layer_index == scene.ms_.size()) {
    ScatteringSetting setting{};
    setting.crystal_ = configured->second;
    setting.crystal_proportion_ = 1;
    scene.ms_.push_back({ 0, { setting } });
  }
  const std::string identity = layer_index >= config.scene_.ms_.size() ?
                                   "standalone configured crystal (not a scene allocation)" :
                                   "selected scene entry snapshot";
  const std::vector<LayerSelection> selection{ { layer_index, request.crystal_id, request.path_layers[0],
                                                 request.symmetry_bits } };
  InputSnapshot snapshot;
  if (Error error = CaptureInput(scene, identity, selection, &snapshot); !error.Ok()) {
    return error;
  }
  PhysicalMemberScope scope;
  if (Error error = ResolvePhysicalMemberScope(snapshot.layers.front(), &scope); !error.Ok()) {
    return error;
  }
  const PoseDensityConversion density = ConvertAxisToPoseDensity(configured->second.axis_);

  // Scope and echo.
  out->schema_version = kSurveySchemaVersion;
  out->request.crystal_id = static_cast<long long>(request.crystal_id);
  out->request.path_layers = request.path_layers;
  out->request.symmetry_bits = request.symmetry_bits;
  out->request.wavelength_given = request.wavelength_nm.has_value();
  out->request.wavelength_nm = request.wavelength_nm.value_or(0.0);
  out->request.budget_ms = request.budget_ms;
  out->scene_identity = identity;
  if (layer_index < config.scene_.ms_.size()) {
    out->scene_layer = static_cast<long long>(layer_index);
  }
  out->members = scope.members;
  out->wavelength_nm = wavelength.wavelength_nm;
  out->wavelength_source = WavelengthSourceName(wavelength.source);
  out->refractive_index = wavelength.refractive_index;
  out->sun_altitude_deg = config.scene_.light_source_.param_.altitude_;
  out->sun_azimuth_deg = config.scene_.light_source_.param_.azimuth_;
  out->chromatic.state = EvidenceState::kUnevaluated;
  out->chromatic.reason = "survey v1: chromatic layer not wired";

  // The T0 leg, one verdict per member. The source_image position is the sun itself: a rank-0
  // path maps every pose to the incident direction, so the class's sky object IS the sun.
  bool all_rank_zero = true;
  bool any_escaped = false;
  std::string escape_mode;
  for (const auto& member : scope.members) {
    SurveyMemberVerdict verdict =
        MemberVerdictOf(table, polygons, member, density, wavelength.refractive_index, &any_escaped, &escape_mode);
    all_rank_zero = all_rank_zero && verdict.rank_zero;
    if (verdict.evidence.state == EvidenceState::kFinite) {
      verdict.evidence.coverage.members_evaluated = 1;
      verdict.evidence.coverage.wavelength_nm = wavelength.wavelength_nm;
    }
    out->member_verdicts.push_back(std::move(verdict));
  }

  if (all_rank_zero) {
    // The identity certificate closes the branch: one card for the whole class (the sky-side
    // object; the members are its preimage decomposition), then the early stop — no boundary
    // walk beyond the certificate's own, no delta axis, no fiber, no field work.
    out->early_stopped = true;
    SurveyObject card;
    card.kind = SurveyObjectKind::kSourceImage;
    card.members = scope.members;
    card.has_position = true;
    card.position_altitude_deg = out->sun_altitude_deg;
    card.position_azimuth_deg = out->sun_azimuth_deg;
    card.evidence.state = EvidenceState::kCertified;
    card.evidence.method = kCertifiedMethodPointMass;
    card.certificate = SurveyCertificate{};
    out->objects.push_back(std::move(card));
    out->evidence.state = EvidenceState::kCertified;
    out->evidence.method = kCertifiedMethodPointMass;
    // The identity arm's chromatic leg: the weight tint (the direction question a rank-0 path
    // still has). The block-level chromatic state and the budget note are the leg's own.
    TintInput tint_input;
    tint_input.table = &table;
    tint_input.polygons = &polygons;
    tint_input.representative = &request.path_layers[0];
    tint_input.density = &density;  // the same conversion the routing gate checked
    SunIncidentDirection(config.scene_.light_source_.param_, tint_input.incident);
    tint_input.member_count = static_cast<int>(scope.members.size());
    AssembleWeightTint(tint_input, out);
    out->budget_note = "the identity arm's legs are the T0 routing verdict (milliseconds) and the " +
                       ChromaticPairProseBlueFirst() + " band-sum tint pair (" +
                       std::to_string(kSurveyTintSampleCount) +
                       " samples per lambda); neither is budget-enforced yet; enforcement lands "
                       "with the quantification legs";
  } else {
    // No early stop: the T0 facts stand (finite — a not-rank-0 verdict is as algebraic as a
    // rank-0 one, and an escaped walk refuses loudly). A random-family class with an
    // expressible density routes into the T1 full-dimensional arm — the delta machine (the
    // T1 rank must be taken on the family, and full-dimensional IS the family here). Anything
    // else stays unevaluated with the reason naming exactly which leg is not wired.
    out->early_stopped = false;
    const bool full_dimensional = density.Ok() && density.spec.family == analytic::PoseFamily::kRandom;
    if (full_dimensional) {
      const T1Outcome outcome = AssembleT1Objects(table, polygons, scope.members, request.path_layers[0],
                                                  wavelength.refractive_index, wavelength.wavelength_nm, out);
      if (!outcome.ok) {
        // Fail closed downstream of the support: the cards after a refused support are not
        // produced, and the document carries the refusal. The support refusal says the support
        // is unknown, not that the class carries no light — the witness leg hangs the declared
        // source's positive event on the SAME refused card (the refused+finite coexistence the
        // evidence layering asks for); the chromatic legs sit downstream of the delta axis and
        // honestly say so instead of the stale v1 reason.
        WitnessInput witness_input;
        witness_input.table = &table;
        witness_input.polygons = &polygons;
        witness_input.crystal = &crystal.shape;
        witness_input.representative = &request.path_layers[0];
        SunIncidentDirection(config.scene_.light_source_.param_, witness_input.incident);
        witness_input.refractive_index = wavelength.refractive_index;
        witness_input.wavelength_nm = wavelength.wavelength_nm;
        witness_input.member_count = static_cast<int>(scope.members.size());
        AssembleRefusedSupport(witness_input, scope.members, outcome.refusal_mode,
                               "chromatic legs not run: the delta axis refused (" + outcome.refusal_mode +
                                   "), and the chromatic legs sit downstream of it",
                               "the refused-support arm's legs are the T0 routing verdict, the "
                               "per-member partition attempt (refused: " +
                                   outcome.refusal_mode + "), the " + std::to_string(kSurveyWitnessSampleCount) +
                                   "-sample witness lattice, and the TIR share leg's re-classification "
                                   "of the declared lattice at " +
                                   std::to_string(kSurveyTirShareSampleCount) +
                                   " samples per lambda; none is budget-enforced yet; enforcement "
                                   "lands with the quantification legs",
                               true, out);
        // The exception evidence leg on the same refused path: the partition's refusal says
        // nothing about the internal-reflection weight split, which the declared lattice still
        // answers (the witness's independence principle, one block down).
        AssembleTirShare(TirShareInputOf(config, table, polygons, crystal.shape, request.path_layers[0],
                                         static_cast<int>(scope.members.size())),
                         out);
        AssembleMcRegionEssWhenRequested(config, snapshot, request, static_cast<int>(scope.members.size()),
                                         wavelength.wavelength_nm, out);
        any_escaped = true;
        escape_mode = outcome.refusal_mode;
      } else {
        // The arm's own budget note: the T1 legs are the document's computation, and the note
        // says what ran instead of repeating the early-stop arm's millisecond-only claim.
        out->budget_note =
            "the full-dimensional arm's legs (per-member delta partition, the band-sum probe, "
            "the chromatic pair, the class tint's two-lambda power sum at " +
            std::to_string(kSurveyClassTintSampleCount) +
            " samples per lambda, and the TIR share "
            "leg's re-classification of the same declared lattice at " +
            std::to_string(kSurveyTirShareSampleCount) +
            " samples per lambda) are not budget-enforced yet; enforcement lands with the "
            "quantification legs";
        // The canonical probe: the class's radial profile at the document's reference
        // wavelength (the same ruler the certified edge reads). Never refuses — an empty
        // event set serializes as the honest zero profile.
        ProbeInput probe_input;
        probe_input.table = &table;
        probe_input.polygons = &polygons;
        probe_input.representative = &request.path_layers[0];
        probe_input.density = &density;  // the same conversion the routing gate checked
        probe_input.refractive_index = wavelength.refractive_index;
        probe_input.wavelength_nm = wavelength.wavelength_nm;
        SunIncidentDirection(config.scene_.light_source_.param_, probe_input.incident);
        probe_input.member_count = static_cast<int>(scope.members.size());
        RunSolarRadialProbe(probe_input, out);
        // The chromatic layer's v1 legs on the representative (orthogonal to the position
        // objects; the dispersion and diagnose legs are separate kernel paths, separate
        // sub-blocks).
        AssembleChromaticLayer(table, polygons, request.path_layers[0], density.spec,
                               static_cast<int>(scope.members.size()), out);
        // The arm's own tint leg: the class power-sum over the declared lattice (a different
        // reading and field family from the identity arm's point-mass masses; its refusals
        // stay in the sub-block — the block already carries legs with real output).
        ClassTintInput class_tint_input;
        class_tint_input.table = &table;
        class_tint_input.polygons = &polygons;
        class_tint_input.representative = &request.path_layers[0];
        SunIncidentDirection(config.scene_.light_source_.param_, class_tint_input.incident);
        class_tint_input.member_count = static_cast<int>(scope.members.size());
        AssembleClassTint(class_tint_input, out);
        // The exception evidence leg: the TIR share over the same declared lattice (a
        // document-level block of its own, beside family_routing; its refusals never touch the
        // chromatic block).
        AssembleTirShare(TirShareInputOf(config, table, polygons, crystal.shape, request.path_layers[0],
                                         static_cast<int>(scope.members.size())),
                         out);
        AssembleMcRegionEssWhenRequested(config, snapshot, request, static_cast<int>(scope.members.size()),
                                         wavelength.wavelength_nm, out);
      }
    } else {
      // The family-restricted route (gap 1's own): the measure is read in the concentration
      // language and gated at the v1 routing domain — an outside-domain shape is refused with
      // its named mode on a support card (plus the witness's positive fact), an in-domain
      // measure walks the freezing ladder (survey_family.hpp). The retired v1 skeleton — every
      // deeper kind parked in unevaluated with a not-wired reason — is gone: a non-random axis
      // is routed or refused, never parked.
      const FamilyMeasureReading measure = ReadFamilyMeasure(configured->second.axis_);
      if (!measure.ok) {
        WitnessInput witness_input;
        witness_input.table = &table;
        witness_input.polygons = &polygons;
        witness_input.crystal = &crystal.shape;
        witness_input.representative = &request.path_layers[0];
        SunIncidentDirection(config.scene_.light_source_.param_, witness_input.incident);
        witness_input.refractive_index = wavelength.refractive_index;
        witness_input.wavelength_nm = wavelength.wavelength_nm;
        witness_input.member_count = static_cast<int>(scope.members.size());
        AssembleRefusedSupport(witness_input, scope.members, measure.refusal_mode,
                               "chromatic legs not run: the measure shape is outside the routing v1 domain, and "
                               "the legs sit downstream of the routing",
                               "the refused-measure arm's legs are the T0 routing verdict and the " +
                                   std::to_string(kSurveyWitnessSampleCount) +
                                   "-sample witness lattice; the routing v1 domain names the shapes it refuses "
                                   "(measure_shape_outside_routing_domain), never approximates them; none is "
                                   "budget-enforced yet; enforcement lands with the quantification legs",
                               false, out);
        any_escaped = true;
        escape_mode = measure.refusal_mode;
      } else {
        FamilyArmInput family_input;
        family_input.table = &table;
        family_input.polygons = &polygons;
        family_input.crystal = &crystal.shape;
        family_input.members = &scope.members;
        family_input.representative = &request.path_layers[0];
        family_input.measure = measure.measure;
        family_input.single_pose = measure.single_pose;
        family_input.density = &density;
        SunIncidentDirection(config.scene_.light_source_.param_, family_input.incident);
        family_input.refractive_index = wavelength.refractive_index;
        family_input.wavelength_nm = wavelength.wavelength_nm;
        const FamilyArmOutcome family_outcome = AssembleFamilyRouting(family_input, out);
        if (!family_outcome.structural_output) {
          any_escaped = true;
          escape_mode = family_outcome.refusal_mode;
        }
      }
    }
    if (any_escaped) {
      out->evidence.state = EvidenceState::kRefused;
      out->evidence.mode = escape_mode;
    } else {
      out->evidence.state = EvidenceState::kFinite;
      out->evidence.coverage.members_evaluated = static_cast<int>(scope.members.size());
      out->evidence.coverage.wavelength_nm = wavelength.wavelength_nm;
    }
  }

  out->budget_ms = request.budget_ms;
  out->total_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
  ILOG_INFO(GetGlobalLogger(), "[raypath survey] crystal {}, path class routed: {} member(s), early stop {}",
            request.crystal_id, scope.members.size(), out->early_stopped ? "taken" : "not taken");
  return {};
}

}  // namespace lumice::raypath::survey
