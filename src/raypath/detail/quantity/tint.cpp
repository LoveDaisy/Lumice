#include "raypath/detail/quantity/tint.hpp"

#include <cmath>

#include "analytic/diagnostic_batch.hpp"
#include "analytic/discovery.hpp"
#include "core/optics.hpp"  // IceRefractiveIndex: the engine's dispersion table
#include "util/logger.hpp"

namespace lumice::raypath::survey {
namespace {
SurveyWeightTint CompleteTint(SurveyWeightTint tint) {
  if (auto* value = tint.ValueOrNull())
    value->quantity.method = value->Method();
  return CompleteQuantity(std::move(tint));
}
}  // namespace

SurveyWeightTint EvaluateWeightTint(const TintInput& input) {
  SurveyWeightTint tint;
  tint.SetState(EvidenceState::kFinite);
  QuantitySpec asked =
      DescribeQuantity(QuantityKind::kTintRatio, QuantityUnit::kDimensionless, QuantityMethod::kPointMassBandSum,
                       ReferenceMeasure::kHaar, DensityWeighting::kPoseDensity);
  asked.spectrum = SpectralObservation::kFixedChromaticPair;
  asked.wavelengths_nm = { kChromaticRedNm, kChromaticBlueNm };
  DeclareQuantity(&tint, asked, { *input.representative }, std::nullopt);

  if (!input.density->Ok()) {
    // The axis maps to no pose density (a fixed pose, for one): the leg refuses rather than
    // approximating. The degenerate is an honest capability boundary, not a physical
    // impossibility — a single pose's weight is exactly computable in principle; what is
    // missing is a pose-density encoding for a delta distribution (kernel gap, not wired here).
    tint.SetState(EvidenceState::kRefused);
    tint.Mode() = "density_not_expressible";
    return CompleteTint(std::move(tint));
  }

  std::vector<int> slots;
  if (const Error slot_error = ResolveSingleLayerPath({ *input.representative }, *input.table, &slots);
      !slot_error.Ok()) {
    // Unreachable past the assembly's request-level resolution (it resolved the same sequence),
    // but the refusal stays honest — the same discipline as the sibling chromatic legs.
    tint.SetState(EvidenceState::kRefused);
    tint.Mode() = "member_resolution";
    return CompleteTint(std::move(tint));
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
    tint.SetState(EvidenceState::kRefused);
    tint.Mode() = "not_rank_zero";
    return CompleteTint(std::move(tint));
  }
  if (!(red.m > 0.0) || !(blue.m > 0.0) || !std::isfinite(red.m) || !std::isfinite(blue.m)) {
    // A degenerate band makes the ratio no number; the document must not carry a silent inf.
    tint.SetState(EvidenceState::kRefused);
    tint.Mode() = "zero_band_weight";
    return CompleteTint(std::move(tint));
  }

  tint.Value().n_red = n_red;
  tint.Value().n_blue = n_blue;
  tint.Value().Mass().red = red.m;
  tint.Value().Mass().blue = blue.m;
  tint.Value().Mass().red_error = red.m_error;
  tint.Value().Mass().blue_error = blue.m_error;
  tint.Value().Mass().protocol = red.method == analytic::PointMassMethod::kPsiAverage ? TintMassProtocol::kPsiAverage :
                                                                                        TintMassProtocol::kLatticeMean;
  tint.Value().quantity =
      DescribeQuantity(QuantityKind::kTintRatio, QuantityUnit::kDimensionless,
                       red.method == analytic::PointMassMethod::kPsiAverage ? QuantityMethod::kPointMassPsiAverage :
                                                                              QuantityMethod::kPointMassLatticeMean,
                       ReferenceMeasure::kHaar, DensityWeighting::kPoseDensity);
  tint.Value().quantity.spectrum = SpectralObservation::kFixedChromaticPair;
  tint.Value().quantity.wavelengths_nm = { kChromaticRedNm, kChromaticBlueNm };
  tint.Value().sample_count = input.sample_count;
  tint.Value().lattice = "antipodal_fibonacci";  // band_sum's sampler kind, restated for the document
  tint.Value().deterministic = true;
  tint.Value().representative = *input.representative;
  tint.Value().member_count = input.member_count;
  tint.Value().ratio = blue.m / red.m;
  tint.Value().direction = tint.Value().ratio > 1.0 ? SurveyTintDirection::kBlueTinted :
                                                      (tint.Value().ratio < 1.0 ? SurveyTintDirection::kRedTinted :
                                                                                  SurveyTintDirection::kNeutral);
  tint.Value().kept_count_red = red.kept_count;
  tint.Value().kept_count_blue = blue.kept_count;
  tint.SetState(EvidenceState::kFinite);
  tint.Coverage().members_evaluated = 1;  // the representative; the class reads xN
  tint.Coverage().geometry_basis =
      "the point-mass band sum on the class representative at the fixed " + ChromaticPairProseRedFirst() + " pair";
  tint.Coverage().wavelength_nm = kChromaticBlueNm;


  ILOG_INFO(GetGlobalLogger(), "[raypath survey] weight tint: method {}, ratio {:.6f} ({} samples per lambda)",
            TintMethodName(tint.Value().Method()), tint.Value().ratio, input.sample_count);
  return CompleteTint(std::move(tint));
}

SurveyWeightTint EvaluateClassTint(const ClassTintInput& input) {
  SurveyWeightTint tint;
  tint.SetState(EvidenceState::kFinite);
  QuantitySpec asked =
      DescribeQuantity(QuantityKind::kTintRatio, QuantityUnit::kDimensionless, QuantityMethod::kClassLatticePowerSum,
                       ReferenceMeasure::kHaar, DensityWeighting::kNone);
  asked.spectrum = SpectralObservation::kFixedChromaticPair;
  asked.wavelengths_nm = { kChromaticRedNm, kChromaticBlueNm };
  DeclareQuantity(&tint, asked, { *input.representative }, std::nullopt);

  // One refusal shape for this arm's sub-block: the block has other legs with real output, so
  // the refusal stays in the sub-block (the block-state rule is the T1 arm's own).


  std::vector<int> slots;
  if (const Error slot_error = ResolveSingleLayerPath({ *input.representative }, *input.table, &slots);
      !slot_error.Ok()) {
    // Unreachable past the assembly's request-level resolution, but the refusal stays honest —
    // the same discipline as the sibling legs.
    tint.Value().reading = TintClassPowerReading{};
    tint.SetState(EvidenceState::kRefused);
    tint.Mode() = "member_resolution";
    return CompleteTint(std::move(tint));
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
    tint.Value().reading = TintClassPowerReading{};
    tint.SetState(EvidenceState::kRefused);
    tint.Mode() = "zero_band_weight";
    return CompleteTint(std::move(tint));
  }

  tint.Value().reading = TintClassPowerReading{};
  tint.Value().n_red = n_red;
  tint.Value().n_blue = n_blue;
  tint.Value().Power().red = weight_red / static_cast<double>(input.sample_count);
  tint.Value().Power().blue = weight_blue / static_cast<double>(input.sample_count);
  tint.Value().quantity =
      DescribeQuantity(QuantityKind::kTintRatio, QuantityUnit::kDimensionless, QuantityMethod::kClassLatticePowerSum,
                       ReferenceMeasure::kHaar, DensityWeighting::kNone);
  tint.Value().quantity.spectrum = SpectralObservation::kFixedChromaticPair;
  tint.Value().quantity.wavelengths_nm = { kChromaticRedNm, kChromaticBlueNm };
  tint.Value().reading_unit = QuantityUnit::kArea;
  tint.Value().sample_count = input.sample_count;
  tint.Value().lattice = "antipodal_fibonacci";
  tint.Value().deterministic = true;
  tint.Value().representative = *input.representative;
  tint.Value().member_count = input.member_count;
  tint.Value().ratio = weight_blue / weight_red;
  tint.Value().direction = tint.Value().ratio > 1.0 ? SurveyTintDirection::kBlueTinted :
                                                      (tint.Value().ratio < 1.0 ? SurveyTintDirection::kRedTinted :
                                                                                  SurveyTintDirection::kNeutral);
  tint.Value().kept_count_red = kept_red;
  tint.Value().kept_count_blue = kept_blue;
  tint.Value().caliber = std::string("sum(w(") + kChromaticBlueLabel + "))/sum(w(" + kChromaticRedLabel +
                         ")): class power-sum over the declared antipodal Fibonacci lattice (per-sample average "
                         "powers, kept events' w = A*T); fixed " +
                         ChromaticPairProseRedFirst() +
                         " index pair, unweighted by SPD or CMF — not a disk-integrated D65 B/R and not the "
                         "point-mass mass ratio";
  tint.SetState(EvidenceState::kFinite);
  tint.Coverage().members_evaluated = 1;  // the representative; the class reads xN
  tint.Coverage().geometry_basis =
      "the class power-sum on the class representative over the declared lattice at the fixed " +
      ChromaticPairProseRedFirst() + " pair";
  tint.Coverage().wavelength_nm = kChromaticBlueNm;

  ILOG_INFO(GetGlobalLogger(), "[raypath survey] class tint: method {}, ratio {:.6f} ({} samples per lambda)",
            TintMethodName(tint.Value().Method()), tint.Value().ratio, input.sample_count);
  return CompleteTint(std::move(tint));
}

SurveyWeightTint EvaluateSinglePoseTint(const SinglePoseTintInput& input) {
  SurveyWeightTint tint;
  tint.SetState(EvidenceState::kFinite);
  QuantitySpec asked =
      DescribeQuantity(QuantityKind::kTintRatio, QuantityUnit::kDimensionless, QuantityMethod::kSinglePoseDirect,
                       ReferenceMeasure::kNone, DensityWeighting::kNone);
  asked.spectrum = SpectralObservation::kFixedChromaticPair;
  asked.wavelengths_nm = { kChromaticRedNm, kChromaticBlueNm };
  DeclareQuantity(&tint, asked, { *input.representative }, std::nullopt);

  const double n_red = IceRefractiveIndex::Get(kChromaticRedNm);
  const double n_blue = IceRefractiveIndex::Get(kChromaticBlueNm);
  double m_red = 0.0;
  double m_blue = 0.0;
  bool tint_ok = true;
  for (int lambda = 0; lambda < 2 && tint_ok; lambda++) {
    const bool red = lambda == 0;
    std::vector<analytic::DiagnosticInputRow> rows(1);
    analytic::DiagnosticInputRow& row = rows.front();
    row.crystal = *input.crystal;
    for (int c = 0; c < 9; c++) {
      row.pose[static_cast<size_t>(c)] = input.pose[c];
    }
    for (int c = 0; c < 3; c++) {
      row.incident[static_cast<size_t>(c)] = input.incident[c];
    }
    row.refractive_index = red ? n_red : n_blue;
    row.source_token = 0;
    std::vector<analytic::DiagnosticOutputRow> values;
    if (!analytic::EvaluateDiagnosticBatch(*input.representative, rows, {}, &values) || values.size() != 1 ||
        !values[0].path_valid || !values[0].entry_available || !(values[0].entry.value > 0.0)) {
      // The corridor's honest data (an empty corridor for a doubled-back chain, an edge-on
      // entry): the weight is unavailable, never defaulted.
      ILOG_VERBOSE(GetGlobalLogger(), "[raypath survey] single-pose weight refused: path_valid={} entry status {}",
                   values.size() == 1 && values[0].path_valid,
                   values.size() == 1 ? static_cast<int>(values[0].entry.status) : -1);
      tint_ok = false;
      break;
    }
    (red ? m_red : m_blue) = values[0].entry.value * values[0].interface_product;
  }
  if (tint_ok && m_red > 0.0 && m_blue > 0.0) {
    tint.Value().n_red = n_red;
    tint.Value().n_blue = n_blue;
    tint.Value().Mass().red = m_red;
    tint.Value().Mass().blue = m_blue;
    tint.Value().Mass().protocol = TintMassProtocol::kSinglePoseDirect;
    tint.Value().quantity =
        DescribeQuantity(QuantityKind::kTintRatio, QuantityUnit::kDimensionless, QuantityMethod::kSinglePoseDirect,
                         ReferenceMeasure::kNone, DensityWeighting::kNone);
    tint.Value().quantity.spectrum = SpectralObservation::kFixedChromaticPair;
    tint.Value().quantity.wavelengths_nm = { kChromaticRedNm, kChromaticBlueNm };
    tint.Value().reading_unit = QuantityUnit::kArea;
    tint.Value().sample_count = 1;
    tint.Value().lattice = "single_pose";
    tint.Value().deterministic = true;
    tint.Value().representative = *input.representative;
    tint.Value().member_count = input.member_count;
    tint.Value().ratio = m_blue / m_red;
    tint.Value().direction = tint.Value().ratio > 1.0 ? SurveyTintDirection::kBlueTinted :
                             tint.Value().ratio < 1.0 ? SurveyTintDirection::kRedTinted :
                                                        SurveyTintDirection::kNeutral;
    tint.Value().kept_count_red = 1;
    tint.Value().kept_count_blue = 1;
    tint.Value().caliber = std::string("w(") + kChromaticBlueLabel + ")/w(" + kChromaticRedLabel +
                           "): single-pose w = A*T at the fixed pose, the diagnostic batch's own "
                           "re-evaluation; fixed " +
                           ChromaticPairProseRedFirst() +
                           " index pair, unweighted by SPD or CMF — not a band sum and not the "
                           "point-mass mass ratio";
    tint.SetState(EvidenceState::kFinite);
    tint.Coverage().members_evaluated = 1;  // the representative; the class reads xN
    tint.Coverage().geometry_basis =
        "the diagnostic batch at the axis's single pose at the " + ChromaticPairProseRedFirst() + " pair";
    tint.Coverage().wavelength_nm = kChromaticBlueNm;
  } else {
    tint.SetState(EvidenceState::kRefused);
    tint.Mode() = "single_pose_weight_unavailable";
  }
  return CompleteTint(std::move(tint));
}

}  // namespace lumice::raypath::survey
