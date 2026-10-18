#include "raypath/detail/survey/survey_tint.hpp"

#include <cmath>

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

}  // namespace lumice::raypath::survey
