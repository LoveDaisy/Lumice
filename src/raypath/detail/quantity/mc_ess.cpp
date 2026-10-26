#include "raypath/detail/quantity/mc_ess.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>

#include "analytic/so3.hpp"
#include "core/math.hpp"  // math::kRadToDegreeD — the deviation's unit conversion
#include "raypath/detail/diagnostic_sampler.hpp"
#include "raypath/query_control.hpp"
#include "util/logger.hpp"

namespace lumice::raypath::quantity {
using survey::EvidenceState;
namespace {

// One band's Kish accumulation, in the PresenceEss grouping discipline: rows arrive in outer
// draw order (the measure builder's emission order), one draw is one sampling act even when
// it contributed several member rows, so the per-draw weight closes before the next draw's
// first row and the Kish ratio reads the closed groups.
struct BandAccumulator {
  uint64_t current_draw = 0;
  bool group_open = false;
  double group = 0.0;
  double sum_weight = 0.0;
  double sum_squared = 0.0;
  long long draw_count = 0;

  void CloseGroup() {
    if (group_open && group > 0.0) {
      sum_weight += group;
      sum_squared += group * group;
      draw_count++;
    }
    group_open = false;
    group = 0.0;
  }
  void Add(uint64_t draw, double y) {
    // Rows arrive in the measure builder's emission order; a decreasing draw id would mean the
    // grouping below silently splits a draw's weight across two groups and undercounts the ESS.
    assert(!group_open || draw >= current_draw);
    if (!group_open || draw != current_draw) {
      CloseGroup();
      current_draw = draw;
      group_open = true;
    }
    group += y;
  }
  double Ess() const { return sum_squared > 0.0 ? sum_weight * sum_weight / sum_squared : 0.0; }
};

}  // namespace

McRegionEssResult EvaluateMcRegionEss(const McRegionEssInput& input) {
  McRegionEssResult ess;
  ess.SetState(EvidenceState::kFinite);
  ess.Value().quantity =
      DescribeQuantity(QuantityKind::kMcEss, QuantityUnit::kDimensionless, QuantityMethod::kKishOuterDraw,
                       ReferenceMeasure::kActualProduct, DensityWeighting::kActualSamplingWeights);
  ess.Value().quantity.shape = GeometryKind::kDeclaredPopulation;
  ess.Value().quantity.aggregation = MemberAggregation::kPhysicalClass;
  ess.Value().quantity.source =
      input.snapshot->light.param_.diameter_ > 0 ? SourceObservation::kSolarDisk : SourceObservation::kSolarCenter;
  DeclareQuantity(&ess, ess.Value().quantity,
                  input.snapshot->layers.front().representative.empty() ?
                      std::vector<std::vector<int>>{} :
                      std::vector<std::vector<int>>{ input.snapshot->layers.front().representative },
                  input.wavelength_nm);
  PhysicalMemberScope evaluated_members;
  if (ResolvePhysicalMemberScope(input.snapshot->layers.front(), &evaluated_members).Ok()) {
    ClaimScope scope = ess.Scope();
    scope.members = static_cast<const MemberScope&>(evaluated_members);
    if (input.member_count >= 0 && static_cast<size_t>(input.member_count) < scope.members.members.size()) {
      scope.members.members.resize(static_cast<size_t>(input.member_count));
    }
    ess.BindScope(scope);
  }
  // The single-lambda spectrum request: the survey's reference wavelength through the sampled
  // branch. A discrete light must own a slot AT the reference wavelength (the survey's own
  // resolution guarantees it when the wavelength came from the scene, not from the user);
  // anything else is the sampler's honest refusal.
  WavelengthSample sample{};
  sample.wavelength_nm = static_cast<float>(input.wavelength_nm);
  sample.provenance = "survey mc region ess leg";
  if (const auto* discrete = std::get_if<std::vector<WlParam>>(&input.snapshot->light.spectrum_)) {
    for (size_t i = 0; i < discrete->size(); ++i) {
      if ((*discrete)[i].wl_ == sample.wavelength_nm) {
        sample.discrete_slot = i;
        break;
      }
    }
  }

  DiagnosticSampler sampler(*input.snapshot, static_cast<uint32_t>(kSurveyMcEssSeed),
                            SpectrumRequest{ std::move(sample) });
  SamplingBudget budget;
  budget.requested_samples = static_cast<uint64_t>(input.draws);
  // In a unified query the pre-resolved deadline wins even if this leg starts later;
  // the legacy leg keeps its own budget window.
  budget.max_optical_evaluations = ~uint64_t{ 0 };
  if (input.query_deadline) {
    budget.deadline = *input.query_deadline;
    if (input.budget_ms > 0) {
      budget.deadline =
          std::min(budget.deadline, std::chrono::steady_clock::now() + std::chrono::milliseconds(input.budget_ms));
    }
  } else {
    BudgetRequest guardrail_request;
    guardrail_request.budget_ms = input.budget_ms;
    budget.deadline = ResolveLeg(BudgetWindow::kLegGuardrail, guardrail_request).stop.deadline;
  }
  analytic::DiscoveryStop merged_stop;
  if (input.stop != nullptr) {
    merged_stop.deadline = budget.deadline;
    merged_stop.interrupted = input.stop->interrupted;
    merged_stop.token = input.stop->token;
    budget.stop = &merged_stop;
  }
  DiagnosticMeasure measure;
  if (const Error error = BuildDiagnosticMeasure(sampler, budget, &measure); !error.Ok()) {
    ess.SetState(EvidenceState::kRefused);
    ess.Mode() = "sampler_unavailable";
    ILOG_INFO(GetGlobalLogger(), "[raypath survey] mc region ess refused: sampler_unavailable ({} draws requested)",
              input.draws);
    return CompleteQuantity(std::move(ess));
  }
  // BuildDiagnosticMeasure's flag contract: the flag is set only by its pre-draw and mid-draw
  // deadline/ceiling checks — never after a draw completes — so a measure with every requested
  // draw completed never carries it, and an empty measure always does. The completed_samples
  // disjunct below is fail-closed defense, not a second reachable refusal cause.
  if (measure.budget_exhausted || measure.completed_samples == 0) {
    ess.SetState(EvidenceState::kRefused);
    ess.Mode() = "budget_exhausted";
    // The refused leg's only record outside the {state, mode} block: a partial measure spent
    // real compute before the deadline, and the audit reader sees it nowhere else.
    ILOG_INFO(GetGlobalLogger(), "[raypath survey] mc region ess refused: budget_exhausted ({} of {} draws completed)",
              measure.completed_samples, input.draws);
    return CompleteQuantity(std::move(ess));
  }

  std::vector<BandAccumulator> bands(kSurveyMcEssBandCount);
  for (const analytic::WeightedSkySample& component : measure.components) {
    // The declared member scope: the survey snapshot carries exactly this class's members, so
    // every row is a class row today — the bound states the reader contract rather than
    // filtering a known-empty case.
    const size_t token = static_cast<size_t>(component.source_token);
    if (token >= measure.sources.size() ||
        measure.sources[token].member_index >= static_cast<size_t>(input.member_count)) {
      continue;
    }
    const double y = component.xyz_weight[1];
    if (!(y > 0.0)) {
      continue;  // a zero-spectral-signal row changes no band's numbers
    }
    const double delta_deg =
        analytic::so3::AngleBetween(input.sun_dir, component.direction.data()) * math::kRadToDegreeD;
    int band = static_cast<int>(delta_deg / kSurveyMcEssBandDeg);
    if (band < 0) {
      band = 0;
    } else if (band >= kSurveyMcEssBandCount) {
      band = kSurveyMcEssBandCount - 1;  // exactly 180 degrees lands in the last band
    }
    bands[static_cast<size_t>(band)].Add(component.sample_index, y);
  }
  for (BandAccumulator& band : bands) {
    band.CloseGroup();
  }

  ess.Value().seed = kSurveyMcEssSeed;
  ess.Value().draws = input.draws;
  ess.Value().lambda_nm = input.wavelength_nm;
  ess.Value().band_deg = kSurveyMcEssBandDeg;
  ess.Value().bands.resize(kSurveyMcEssBandCount);
  for (int i = 0; i < kSurveyMcEssBandCount; i++) {
    McRegionEssBand& row = ess.Value().bands[static_cast<size_t>(i)];
    row.delta_min_deg = i * kSurveyMcEssBandDeg;
    row.delta_max_deg = (i + 1) * kSurveyMcEssBandDeg;
    row.ess = bands[static_cast<size_t>(i)].Ess();
    row.draw_count = bands[static_cast<size_t>(i)].draw_count;
    row.sum_weight = bands[static_cast<size_t>(i)].sum_weight;
    row.sum_squared_weight = bands[static_cast<size_t>(i)].sum_squared;
  }
  ess.Value().deterministic = false;  // seeded random; a given seed reproduces the sample
  ess.SetState(EvidenceState::kFinite);
  ess.Coverage().members_evaluated = input.member_count;
  ess.Coverage().geometry_basis =
      "the MC corroboration sampler's complete outer draws at the survey's scene snapshot, "
      "Y-channel weights banded by solar-radial deviation; a sampling-quality measurement, not "
      "a corroboration verdict (no agrees/silent/conflicts word applies)";
  ess.Coverage().wavelength_nm = input.wavelength_nm;

  const double peak_ess =
      std::max_element(ess.Value().bands.begin(), ess.Value().bands.end(),
                       [](const McRegionEssBand& a, const McRegionEssBand& b) { return a.ess < b.ess; })
          ->ess;
  ILOG_INFO(GetGlobalLogger(),
            "[raypath survey] mc region ess: {} draws at seed {}, lambda {:.1f} nm, {} completed, peak band ESS {:.1f}",
            ess.Value().draws, ess.Value().seed, ess.Value().lambda_nm, measure.completed_samples, peak_ess);
  return CompleteQuantity(std::move(ess));
}

}  // namespace lumice::raypath::quantity
