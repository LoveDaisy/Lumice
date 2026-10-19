#include "raypath/detail/survey/survey_tint.hpp"

#include <cmath>

#include "analytic/discovery.hpp"
#include "core/optics.hpp"  // IceRefractiveIndex: the engine's dispersion table
#include "util/logger.hpp"

namespace lumice::raypath::survey {
namespace {

// One refusal: the tint sub-block and the chromatic block (its only leg's owner on this arm)
// carry the same mode, and no numeric field is written.
void RefuseTint(SurveyDocument* out, const char* mode) {
  SurveyWeightTint& tint = out->weight_tint.emplace();
  tint.evidence.state = EvidenceState::kRefused;
  tint.evidence.mode = mode;
  out->chromatic.state = EvidenceState::kRefused;
  out->chromatic.mode = mode;
}

}  // namespace

void AssembleWeightTint(const TintInput& input, SurveyDocument* out) {
  if (!input.density->Ok()) {
    // The axis maps to no pose density (a fixed pose, for one): the leg refuses rather than
    // approximating. The degenerate is an honest capability boundary, not a physical
    // impossibility — a single pose's weight is exactly computable in principle; what is
    // missing is a pose-density encoding for a delta distribution (kernel gap, not wired here).
    RefuseTint(out, "density_not_expressible");
    return;
  }

  std::vector<int> slots;
  if (const Error slot_error = ResolveSingleLayerPath({ *input.representative }, *input.table, &slots);
      !slot_error.Ok()) {
    // Unreachable past the assembly's request-level resolution (it resolved the same sequence),
    // but the refusal stays honest — the same discipline as the sibling chromatic legs.
    RefuseTint(out, "member_resolution");
    return;
  }

  // Two calls, one lattice size, one empty pixel table: the masses are what the ratio reads.
  const double n_red = IceRefractiveIndex::Get(kChromaticRedNm);
  const double n_blue = IceRefractiveIndex::Get(kChromaticBlueNm);
  const analytic::PoseDensity density(input.density->spec);
  const analytic::PixelTable empty_table;
  const analytic::BandSumOutput red =
      analytic::BandSum(*input.table, *input.polygons, slots.data(), static_cast<int>(slots.size()), n_red,
                        input.incident, input.sample_count, empty_table, density);
  const analytic::BandSumOutput blue =
      analytic::BandSum(*input.table, *input.polygons, slots.data(), static_cast<int>(slots.size()), n_blue,
                        input.incident, input.sample_count, empty_table, density);
  if (!red.rank_zero || !blue.rank_zero) {
    // The T0 verdict and the kernel's own rank check disagree: refuse, never a map read as a mass.
    RefuseTint(out, "not_rank_zero");
    return;
  }
  if (!(red.m > 0.0) || !(blue.m > 0.0) || !std::isfinite(red.m) || !std::isfinite(blue.m)) {
    // A degenerate band makes the ratio no number; the document must not carry a silent inf.
    RefuseTint(out, "zero_band_weight");
    return;
  }

  SurveyWeightTint& tint = out->weight_tint.emplace();
  tint.n_red = n_red;
  tint.n_blue = n_blue;
  tint.m_red = red.m;
  tint.m_blue = blue.m;
  tint.m_red_error = red.m_error;
  tint.m_blue_error = blue.m_error;
  tint.method = PointMassMethodName(red.method);
  tint.sample_count = input.sample_count;
  tint.lattice = "antipodal_fibonacci";  // band_sum's sampler kind, restated for the document
  tint.deterministic = true;
  tint.representative = *input.representative;
  tint.member_count = input.member_count;
  tint.ratio = blue.m / red.m;
  tint.direction = tint.ratio > 1.0 ?
                       SurveyTintDirection::kBlueTinted :
                       (tint.ratio < 1.0 ? SurveyTintDirection::kRedTinted : SurveyTintDirection::kNeutral);
  tint.kept_count_red = red.kept_count;
  tint.kept_count_blue = blue.kept_count;
  tint.evidence.state = EvidenceState::kFinite;
  tint.evidence.coverage.members_evaluated = 1;  // the representative; the class reads xN
  tint.evidence.coverage.geometry_basis =
      "the point-mass band sum on the class representative at the fixed " + ChromaticPairProseRedFirst() + " pair";
  tint.evidence.coverage.wavelength_nm = kChromaticBlueNm;

  out->chromatic.state = EvidenceState::kFinite;
  out->chromatic.coverage.members_evaluated = input.member_count;
  out->chromatic.coverage.wavelength_nm = kChromaticBlueNm;
  out->chromatic.coverage.geometry_basis =
      "the tint leg ran on the class representative at the fixed " + ChromaticPairProseRedFirst() + " pair";

  ILOG_INFO(GetGlobalLogger(), "[raypath survey] weight tint: method {}, ratio {:.6f} ({} samples per lambda)",
            tint.method, tint.ratio, input.sample_count);
}

void AssembleClassTint(const ClassTintInput& input, SurveyDocument* out) {
  // One refusal shape for this arm's sub-block: the block has other legs with real output, so
  // the refusal stays in the sub-block (the block-state rule is the T1 arm's own).
  const auto refuse = [out](const char* mode) {
    SurveyWeightTint& tint = out->weight_tint.emplace();
    tint.class_power_shape = true;
    tint.evidence.state = EvidenceState::kRefused;
    tint.evidence.mode = mode;
  };

  std::vector<int> slots;
  if (const Error slot_error = ResolveSingleLayerPath({ *input.representative }, *input.table, &slots);
      !slot_error.Ok()) {
    // Unreachable past the assembly's request-level resolution, but the refusal stays honest —
    // the same discipline as the sibling legs.
    refuse("member_resolution");
    return;
  }

  // Two passes, one lattice size, one shared lattice: the declared source sampled at each
  // lambda's refractive index, and the ratio reads the kept events' weights.
  const double n_red = IceRefractiveIndex::Get(kChromaticRedNm);
  const double n_blue = IceRefractiveIndex::Get(kChromaticBlueNm);
  double weight_red = 0.0;
  double weight_blue = 0.0;
  long long kept_red = 0;
  long long kept_blue = 0;
  const auto power_sum = [&](double refractive_index, double* weight, long long* kept) {
    // One discovery per call (the corridor keeps clipping scratch; the type is neither
    // copyable nor const-evaluable), so each lambda invocation builds its own.
    analytic::IceDiscovery discovery(*input.table, *input.polygons, slots.data(), static_cast<int>(slots.size()),
                                     refractive_index, input.incident);
    const std::vector<analytic::SampleEvent> events = discovery.BuildEvents(input.sample_count);
    double sum = 0.0;
    for (const analytic::SampleEvent& event : events) {
      sum += event.weight;
    }
    *weight = sum;
    *kept = static_cast<long long>(events.size());
  };
  power_sum(n_red, &weight_red, &kept_red);
  power_sum(n_blue, &weight_blue, &kept_blue);
  if (!(weight_red > 0.0) || !(weight_blue > 0.0) || !std::isfinite(weight_red) || !std::isfinite(weight_blue)) {
    // A degenerate band makes the ratio no number; the document must not carry a silent inf.
    refuse("zero_band_weight");
    return;
  }

  SurveyWeightTint& tint = out->weight_tint.emplace();
  tint.class_power_shape = true;
  tint.n_red = n_red;
  tint.n_blue = n_blue;
  tint.energy_red = weight_red / static_cast<double>(input.sample_count);
  tint.energy_blue = weight_blue / static_cast<double>(input.sample_count);
  tint.method = kClassTintMethodLatticePowerSum;
  tint.sample_count = input.sample_count;
  tint.lattice = "antipodal_fibonacci";
  tint.deterministic = true;
  tint.representative = *input.representative;
  tint.member_count = input.member_count;
  tint.ratio = weight_blue / weight_red;
  tint.direction = tint.ratio > 1.0 ?
                       SurveyTintDirection::kBlueTinted :
                       (tint.ratio < 1.0 ? SurveyTintDirection::kRedTinted : SurveyTintDirection::kNeutral);
  tint.kept_count_red = kept_red;
  tint.kept_count_blue = kept_blue;
  tint.caliber = std::string("sum(w(") + kChromaticBlueLabel + "))/sum(w(" + kChromaticRedLabel +
                 ")): class power-sum over the declared antipodal Fibonacci lattice (per-sample average "
                 "powers, kept events' w = A*T); fixed " +
                 ChromaticPairProseRedFirst() +
                 " index pair, unweighted by SPD or CMF — not a disk-integrated D65 B/R and not the "
                 "point-mass mass ratio";
  tint.evidence.state = EvidenceState::kFinite;
  tint.evidence.coverage.members_evaluated = 1;  // the representative; the class reads xN
  tint.evidence.coverage.geometry_basis =
      "the class power-sum on the class representative over the declared lattice at the fixed " +
      ChromaticPairProseRedFirst() + " pair";
  tint.evidence.coverage.wavelength_nm = kChromaticBlueNm;

  ILOG_INFO(GetGlobalLogger(), "[raypath survey] class tint: method {}, ratio {:.6f} ({} samples per lambda)",
            tint.method, tint.ratio, input.sample_count);
}

}  // namespace lumice::raypath::survey
