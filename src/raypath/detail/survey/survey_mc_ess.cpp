#include "raypath/detail/survey/survey_mc_ess.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>

#include "analytic/so3.hpp"
#include "core/math.hpp"  // math::kRadToDegreeD — the deviation's unit conversion
#include "util/logger.hpp"

namespace lumice::raypath::survey {
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

void AssembleMcRegionEss(const McRegionEssInput& input, SurveyDocument* out) {
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
  // The deadline is the survey budget guardrail; the draw count is capped at the request
  // boundary, so the optical-evaluation ceiling adds nothing the deadline does not already
  // enforce and stays out of the way.
  budget.max_optical_evaluations = ~uint64_t{ 0 };
  budget.deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(input.budget_ms);
  DiagnosticMeasure measure;
  if (const Error error = BuildDiagnosticMeasure(sampler, budget, &measure); !error.Ok()) {
    SurveyMcRegionEss& ess = out->mc_region_ess.emplace();
    ess.evidence.state = EvidenceState::kRefused;
    ess.evidence.mode = "sampler_unavailable";
    return;
  }
  if (measure.budget_exhausted || measure.completed_samples == 0) {
    SurveyMcRegionEss& ess = out->mc_region_ess.emplace();
    ess.evidence.state = EvidenceState::kRefused;
    ess.evidence.mode = "budget_exhausted";
    return;
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

  SurveyMcRegionEss& ess = out->mc_region_ess.emplace();
  ess.seed = kSurveyMcEssSeed;
  ess.draws = input.draws;
  ess.lambda_nm = input.wavelength_nm;
  ess.band_deg = kSurveyMcEssBandDeg;
  ess.bands.resize(kSurveyMcEssBandCount);
  for (int i = 0; i < kSurveyMcEssBandCount; i++) {
    SurveyMcRegionEssBand& row = ess.bands[static_cast<size_t>(i)];
    row.delta_min_deg = i * kSurveyMcEssBandDeg;
    row.delta_max_deg = (i + 1) * kSurveyMcEssBandDeg;
    row.ess = bands[static_cast<size_t>(i)].Ess();
    row.draw_count = bands[static_cast<size_t>(i)].draw_count;
    row.sum_weight = bands[static_cast<size_t>(i)].sum_weight;
  }
  ess.deterministic = false;  // seeded random; a given seed reproduces the sample
  ess.evidence.state = EvidenceState::kFinite;
  ess.evidence.coverage.members_evaluated = input.member_count;
  ess.evidence.coverage.geometry_basis =
      "the MC corroboration sampler's complete outer draws at the survey's scene snapshot, "
      "Y-channel weights banded by solar-radial deviation; a sampling-quality measurement, not "
      "a corroboration verdict (no agrees/silent/conflicts word applies)";
  ess.evidence.coverage.wavelength_nm = input.wavelength_nm;

  const double peak_ess =
      std::max_element(ess.bands.begin(), ess.bands.end(),
                       [](const SurveyMcRegionEssBand& a, const SurveyMcRegionEssBand& b) { return a.ess < b.ess; })
          ->ess;
  ILOG_INFO(GetGlobalLogger(),
            "[raypath survey] mc region ess: {} draws at seed {}, lambda {:.1f} nm, {} completed, peak band ESS {:.1f}",
            ess.draws, ess.seed, ess.lambda_nm, measure.completed_samples, peak_ess);
}

}  // namespace lumice::raypath::survey
