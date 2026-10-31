#include "raypath/detail/survey/survey_family.hpp"

#include <algorithm>
#include <cmath>

#include "analytic/pose_density.hpp"
#include "core/math.hpp"
#include "raypath/detail/survey/survey_probe.hpp"
#include "raypath/detail/survey/survey_t1.hpp"
#include "raypath/detail/survey/survey_tint.hpp"
#include "raypath/detail/survey/survey_witness.hpp"
#include "raypath/query_control.hpp"
#include "util/logger.hpp"

namespace lumice::raypath::survey {
namespace {
constexpr double kRadToDeg = math::kRadToDegreeD;
// The document-level escape rule's authority: the arm's document carries a finite structural
// reading iff some card says so itself. The witness is a payload on its card with its own
// state — a refused card carrying a finite witness is still not a finite structural object.
bool AnyFiniteObject(const SurveyData& document) {
  for (const SurveyObject& card : document.objects) {
    if (card.State() == EvidenceState::kFinite) {
      return true;
    }
  }
  return false;
}

SurveyObject FamilyCardOf(const structure::FamilySurvivor& fact, double wavelength_nm) {
  SurveyObject card;
  card.SetKind(SurveyKindOf(fact.kind));
  card.members = { fact.member };
  card.has_position = fact.has_position;
  card.position_altitude_deg = fact.position_altitude_deg;
  card.position_azimuth_deg = fact.position_azimuth_deg;
  card.SetState(EvidenceState::kFinite);
  card.Coverage().members_evaluated = 1;
  card.Coverage().wavelength_nm = wavelength_nm;
  SurveyObject::BandPayload band;
  band.level = fact.level;
  band.frozen = fact.frozen;
  band.blur_deg = fact.blur_deg;
  band.scale_deg = fact.scale_deg;
  band.clamped = fact.clamped;
  card.band = std::move(band);
  card.family_origin = SurveyObject::FamilyOriginPayload{ fact.sample_count, fact.crossing_count };
  if (fact.has_domain) {
    SurveyObject::BrightDomainPayload domain;
    domain.domain_lower_deg = fact.domain_lower_deg;
    domain.domain_upper_deg = fact.domain_upper_deg;
    card.SetBright(std::move(domain));
  }
  return card;
}

}  // namespace

namespace {

// The refused corner's stop must survive the arm's own budget notes: every post-corner path in
// this arm sets its note, overwriting the sentence AssembleRefusedSupport appended (the
// assembly-level arms have no post-append setter; this arm does), so the arm re-appends the
// sentence on its way out. Idempotent: the one path that reaches a return without an
// intervening setter already carries the sentence from the helper's own append.
void AppendCornerWitnessStop(const FamilyArmInput& input, bool corner_witness_stopped, SurveyData* out) {
  if (!corner_witness_stopped || input.stop == nullptr) {
    return;
  }
  const char* sentence = input.stop->Why() == analytic::StopReason::kDeadline ?
                             " The witness lattice leg stopped at the query's unified deadline." :
                             " The witness lattice leg stopped at a cooperative poll (explicit cancel).";
  if (out->budget_note.find(sentence) == std::string::npos) {
    out->budget_note += sentence;
  }
}

}  // namespace

FamilyArmOutcome AssembleFamilyRouting(const FamilyArmInput& input, SurveyData* out) {
  FamilyRoutingBlock& block = out->family_routing.emplace();
  block.measure = input.measure;
  double sun_hat[3];
  for (int c = 0; c < 3; c++) {
    sun_hat[c] = -input.incident[c];  // family_rank.hpp's convention: sun_hat points AT the sun
  }
  const int member_count = static_cast<int>(input.members->size());

  // Level 0's support: the delta machine's own (orientation-distribution independent), with the
  // density-support tail rows the measure's gauss zenith contributes.
  const structure::ClassSupport support = structure::AssembleClassSupport(*input.table, *input.polygons, *input.members,
                                                                          input.refractive_index, input.stop);
  if (support.ok) {
    out->objects.push_back(SupportCardOf(support, *input.members, input.wavelength_nm, out->request.symmetry_bits));
  }
  // The partition refusal's scope is the level-0 card itself: the delta partition is a geometry
  // fact about the support, the orientation ladder below a measure fact that runs regardless
  // (its only partition read is bright_domain's domain bounds, gated at the card). The refused
  // card keeps the witness leg's positive fact — the support is unknown, not zero, and never a
  // fabricated union.
  std::string level0_refusal_mode;
  bool corner_witness_stopped = false;  // the arm's exits re-append the corner's stop sentence
  if (!support.ok) {
    level0_refusal_mode = support.refusal_mode;
    block.level0_refusal_mode = support.refusal_mode;
    quantity::WitnessInput witness_input;
    witness_input.table = input.table;
    witness_input.polygons = input.polygons;
    witness_input.crystal = input.crystal;
    witness_input.representative = input.representative;
    for (int c = 0; c < 3; c++) {
      witness_input.incident[c] = input.incident[c];
    }
    witness_input.refractive_index = input.refractive_index;
    witness_input.wavelength_nm = input.wavelength_nm;
    witness_input.member_count = member_count;
    // The query's stop rides the corner's witness leg too — the assembly-level refused-support
    // calls pass it, and this corner is the same leg (the sibling tint/probe legs below pass it
    // as well); an unwired stop would leave this lattice the one unstopped survey leg.
    witness_input.stop = input.stop;
    // Slice A's single refused-card entry, carrying the gate-order ruling's own scope: the
    // card joins the ladder's cards (clear_objects=false), the chromatic/tint stamp below is
    // overwritten whenever the density-expressible legs run, and it survives only in the
    // refused x density-invalid corner, where its reason names the density cause.
    const std::string corner_chromatic_reason = "chromatic legs not run: the axis maps to no pose density (" +
                                                input.density->unsupported +
                                                "); the legs follow objects, not the partition";
    const std::string corner_budget_note =
        "the family arm's legs are the T0 routing verdict, the per-member delta partition "
        "attempt (refused: " +
        support.refusal_mode +
        "), the ladder's per-level rank scans "
        "and the " +
        std::to_string(quantity::kSurveyWitnessSampleCount) +
        "-sample witness lattice on the refused level-0 card; none is budget-enforced yet; "
        "enforcement lands with the quantification legs";
    AssembleRefusedSupport(witness_input, *input.members, support.refusal_mode, corner_chromatic_reason,
                           corner_budget_note, RefusedSupportObjects::kAppendToObjects, out);
    // The corner card is the one just appended: read the leg's own refusal back for the arm's
    // exits (every post-corner note setter overwrites the helper's sentence).
    const SurveyObject& corner_card = out->objects.back();
    corner_witness_stopped = corner_card.Witness() != nullptr &&
                             corner_card.Witness()->State() == EvidenceState::kRefused &&
                             corner_card.Witness()->Mode() == "stop_requested";
  }
  if (support.ok && input.density->Ok() && input.measure.zenith.kind == FamilyDimKind::kGauss) {
    // AC5's boundedness-role separation: the width went into the ladder; the support's own tail
    // bound (the band sum's +-12 sigma window floor) is a row on the support card.
    const analytic::PoseDensity density(input.density->spec);
    double lower = 0.0;
    double upper = 0.0;
    density.ZenithWindow(&lower, &upper);
    FamilySupportTail tail;
    tail.dim = "zenith";
    tail.lower_deg = lower * kRadToDeg;
    tail.upper_deg = upper * kRadToDeg;
    out->objects.back().Support()->density_tails.push_back(std::move(tail));
  }

  structure::FamilyStructureInput structure_input;
  structure_input.table = input.table;
  structure_input.members = input.members;
  structure_input.measure = input.measure;
  structure_input.single_pose = input.single_pose;
  structure_input.refractive_index = input.refractive_index;
  std::copy(input.incident, input.incident + 3, structure_input.incident);
  structure::FamilyStructureResult structure = structure::EvaluateFamilyStructure(structure_input, support);
  if (!structure.refusal_mode.empty()) {
    AppendCornerWitnessStop(input, corner_witness_stopped, out);
    return { false, structure.refusal_mode };
  }
  block.ladder = std::move(structure.ladder);
  for (const structure::FamilySurvivor& fact : structure.survivors) {
    if (input.stop != nullptr) {
      if (input.stop->token != nullptr) {
        input.stop->token->NotePoll(raypath::kPollSurveyT0);  // the family arm's own boundaries
      }
      if (input.stop->Stopped()) {
        break;  // the survivor cards past the cut are not fabricated; the ladder stands as built
      }
    }
    out->objects.push_back(FamilyCardOf(fact, input.wavelength_nm));
  }
  out->warm_index.seeds.insert(out->warm_index.seeds.end(), structure.seeds.begin(), structure.seeds.end());
  const bool rank_ran = structure.rank_ran;
  if (input.single_pose) {
    const double* pose = structure.single_pose.data();
    // The 0D weight tint: the single-pose direct read, never a band sum (the design's own
    // sentence) — the tint module's own entry. The positional chromatic legs say they have no
    // structure to follow.
    quantity::SinglePoseTintInput tint_input;
    tint_input.crystal = input.crystal;
    tint_input.representative = input.representative;
    for (int c = 0; c < 9; c++) {
      tint_input.pose[c] = pose[c];
    }
    for (int c = 0; c < 3; c++) {
      tint_input.incident[c] = input.incident[c];
    }
    tint_input.member_count = member_count;
    AssembleSinglePoseTint(tint_input, out);
    const std::string positional_reason =
        "positional chromatic legs not run: a single pose has no positional structure to follow";
    out->positional_dispersion = SurveyPositionalDispersion{};
    out->positional_dispersion->evidence.state = EvidenceState::kUnevaluated;
    out->positional_dispersion->evidence.reason = positional_reason;
    out->diagnose = SurveyDiagnoseBlock{};
    out->diagnose->evidence.state = EvidenceState::kUnevaluated;
    out->diagnose->evidence.reason = positional_reason;
    block.evidence.state = EvidenceState::kFinite;
    block.evidence.coverage.members_evaluated = member_count;
    block.evidence.coverage.geometry_basis =
        level0_refusal_mode.empty() ?
            std::string("the single pose's direct image evaluation and the delta partition") :
            "the single pose's direct image evaluation (the level-0 delta partition refused: " + level0_refusal_mode +
                ")";
    block.evidence.coverage.wavelength_nm = input.wavelength_nm;
    if (level0_refusal_mode.empty()) {
      out->budget_note =
          "the 0D family arm's legs are the T0 routing verdict, the per-member delta partition, "
          "the single-pose image evaluation and the single-pose weight at the " +
          ChromaticPairProseRedFirst() +
          " pair; none is budget-enforced yet; enforcement lands with the quantification legs";
    } else {
      out->budget_note =
          "the 0D family arm's legs are the T0 routing verdict, the per-member delta partition "
          "attempt (refused: " +
          level0_refusal_mode + "), the " + std::to_string(quantity::kSurveyWitnessSampleCount) +
          "-sample witness lattice on the refused level-0 card, the single-pose image evaluation "
          "and the single-pose weight at the " +
          ChromaticPairProseRedFirst() +
          " pair; none is budget-enforced yet; enforcement lands with the quantification legs";
    }
    AppendCornerWitnessStop(input, corner_witness_stopped, out);
    return { AnyFiniteObject(*out), level0_refusal_mode };
  }

  // The weight legs: the three kernel shapes the density conversion carries run the probe, the
  // chromatic pair and the class tint (the probe on the representative, its readings landing on
  // the representative's own bright_domain card); an axis the conversion cannot carry keeps
  // every leg's named refusal — a fixed zenith or roll is legal structure with no density, and
  // the legs say so instead of silently skipping. Context-only (the query face's no-warm mode)
  // keeps the structure alone: every T3 leg is skipped, the budget note says so.
  if (input.context_only) {
    out->budget_note =
        "the context-only assembly's legs are the T0 routing verdict and the T0-T2 structure legs "
        "(the delta partition and the freezing ladder); the T3 quantification legs (the probe, "
        "the chromatic pair, the tint, the witness lattice) are skipped";
  } else if (input.density->Ok()) {
    if (level0_refusal_mode.empty()) {
      out->budget_note =
          "the family arm's legs are the T0 routing verdict, the per-member delta partition, the "
          "ladder's per-member per-level rank scans (" +
          std::to_string(structure::kFamilyLadderGrid) + " samples per free dimension), the band-sum probe, the " +
          "chromatic pair and the class tint's two-lambda power sum (" +
          std::to_string(quantity::kSurveyClassTintSampleCount) +
          " samples per lambda); none is budget-enforced yet; enforcement lands with the "
          "quantification legs";
    } else {
      out->budget_note =
          "the family arm's legs are the T0 routing verdict, the per-member delta partition "
          "attempt (refused: " +
          level0_refusal_mode + "), the ladder's per-member per-level rank scans (" +
          std::to_string(structure::kFamilyLadderGrid) + " samples per free dimension), the " +
          std::to_string(quantity::kSurveyWitnessSampleCount) +
          "-sample witness lattice on the refused level-0 card, the band-sum probe, the chromatic "
          "pair and the class tint's two-lambda power sum (" +
          std::to_string(quantity::kSurveyClassTintSampleCount) +
          " samples per lambda); none is budget-enforced yet; enforcement lands with the "
          "quantification legs";
    }
    ProbeInput probe_input;
    probe_input.table = input.table;
    probe_input.polygons = input.polygons;
    probe_input.representative = input.representative;
    probe_input.density = input.density;
    probe_input.refractive_index = input.refractive_index;
    probe_input.wavelength_nm = input.wavelength_nm;
    for (int c = 0; c < 3; c++) {
      probe_input.incident[c] = input.incident[c];
    }
    probe_input.member_count = member_count;
    probe_input.stop = input.stop;
    probe_input.fill_member = input.representative;
    RunSolarRadialProbe(probe_input, out);
    AssembleChromaticLayer(*input.table, *input.polygons, *input.representative, input.density->spec, member_count, out,
                           input.stop);
    quantity::ClassTintInput class_tint_input;
    class_tint_input.table = input.table;
    class_tint_input.polygons = input.polygons;
    class_tint_input.representative = input.representative;
    for (int c = 0; c < 3; c++) {
      class_tint_input.incident[c] = input.incident[c];
    }
    class_tint_input.member_count = member_count;
    class_tint_input.stop = input.stop;
    AssembleClassTint(class_tint_input, out);
  } else {
    const std::string density_reason = "no quantification legs ran: the axis maps to no pose density (" +
                                       input.density->unsupported +
                                       "), and the probe/chromatic/tint legs sit downstream of the density conversion";
    out->chromatic.state = EvidenceState::kUnevaluated;
    out->chromatic.reason = density_reason;
    out->weight_tint = WeightTintResult{};
    out->weight_tint->SetState(EvidenceState::kUnevaluated);
    out->weight_tint->Reason() = density_reason;
    if (level0_refusal_mode.empty()) {
      out->budget_note =
          "the family arm's legs are the T0 routing verdict, the per-member delta partition and "
          "the ladder's per-member per-level rank scans (" +
          std::to_string(structure::kFamilyLadderGrid) +
          " samples per free dimension); no quantification leg ran (the axis maps to no pose "
          "density); none is budget-enforced yet; enforcement lands with the quantification legs";
    } else {
      out->budget_note = std::string(
                             "the family arm's legs are the T0 routing verdict, the per-member delta "
                             "partition attempt (refused: ") +
                         level0_refusal_mode + "), " +
                         (rank_ran ? "the ladder's per-member per-level rank scans (" +
                                         std::to_string(structure::kFamilyLadderGrid) + " samples per free dimension)" :
                                     "the ladder's level enumeration (no level got a rank answer)") +
                         ", and the " + std::to_string(quantity::kSurveyWitnessSampleCount) +
                         "-sample witness lattice on the refused level-0 card; no quantification leg ran (the "
                         "axis maps to no pose density); none is budget-enforced yet; enforcement lands with "
                         "the quantification legs";
    }
  }
  // The block's state reads the legs' own outputs: finite once any leg produced one. The one
  // non-finite corner (the gate-order ruling's own definition): a refused level-0 partition AND
  // a density the conversion cannot carry — every quantification leg silent (the ladder's rank
  // scans are density-independent geometry and may still have produced readings), and the block
  // says so instead of claiming a reading nothing produced.
  const bool legs_all_silent =
      out->chromatic.state == EvidenceState::kUnevaluated && out->weight_tint->State() == EvidenceState::kUnevaluated;
  if (!level0_refusal_mode.empty() && legs_all_silent) {
    block.evidence.state = EvidenceState::kUnevaluated;
    block.evidence.reason =
        "the level-0 delta partition refused (" + level0_refusal_mode + ") and the axis maps to no pose density (" +
        input.density->unsupported +
        "): " + (rank_ran ? "no quantification leg produced a reading" : "no family-routing leg produced a reading");
  } else {
    block.evidence.state = EvidenceState::kFinite;
    block.evidence.coverage.members_evaluated = member_count;
    block.evidence.coverage.geometry_basis =
        (!level0_refusal_mode.empty() && !rank_ran) ?
            "the ladder's level enumeration on the nominal crystal shape (no level got a rank answer)" :
            "the per-member restricted-family rank scans at " + std::to_string(structure::kFamilyLadderGrid) +
                " samples per free dimension on the nominal crystal shape";
    block.evidence.coverage.wavelength_nm = input.wavelength_nm;
  }
  ILOG_INFO(GetGlobalLogger(), "[raypath survey] family routing: {} ladder level(s), {} object card(s)",
            block.ladder.size(), out->objects.size());
  AppendCornerWitnessStop(input, corner_witness_stopped, out);
  return { AnyFiniteObject(*out), level0_refusal_mode };
}

}  // namespace lumice::raypath::survey
