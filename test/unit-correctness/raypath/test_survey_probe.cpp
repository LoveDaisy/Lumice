#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "raypath/detail/survey/survey_assembly.hpp"
#include "raypath/detail/survey/survey_probe.hpp"
#include "raypath/scene_to_analytic.hpp"
#include "survey_test_fixtures.hpp"

namespace lumice::raypath::survey {
namespace {

constexpr double kPi = 3.14159265358979323846;

// The synthetic profile shape the corpus predicts for the C01 inner edge (LI canonical h/a=2):
// zero below the edge, a jump to a finite value at it, then a fall outward — plus a variant
// with a genuine peak a few bins out. The reader must keep the two readings two things.
TEST(SurveyProbe, ReaderFindsEdgeBinAndSeparatePeakOnSyntheticProfiles) {
  // 100 bins at 0.1 degrees from 15.0: centres 15.05, 15.15, ...
  std::vector<double> centres;
  std::vector<double> values;
  std::vector<int> counts;
  const int edge_bin = 72;  // the edge sits in [22.2, 22.3]; centre 22.25
  for (int k = 0; k < 100; k++) {
    centres.push_back(15.0 + (static_cast<double>(k) + 0.5) * 0.1);
    if (k < edge_bin) {
      values.push_back(0.0);
      counts.push_back(0);
    } else {
      values.push_back(1.0 / std::sqrt(centres.back() - 22.2));  // monotone fall from the edge
      counts.push_back(10);
    }
  }
  const ProbeReadings monotone = ReadProbeProfile(centres, values, counts);
  EXPECT_TRUE(monotone.has_edge);
  EXPECT_EQ(monotone.edge_bin, edge_bin);
  EXPECT_NEAR(monotone.edge_delta_deg, 22.25, 1e-9);
  // A monotone fall puts the argmax on the first non-empty bin: the peak reading exists, is a
  // separate field, and never lands inside the edge.
  EXPECT_TRUE(monotone.has_peak);
  EXPECT_DOUBLE_EQ(monotone.peak_delta_deg, monotone.edge_delta_deg);
  EXPECT_DOUBLE_EQ(monotone.separation_deg, 0.0);

  // A genuine peak three bins out: the argmax moves off the edge bin.
  for (size_t k = static_cast<size_t>(edge_bin); k < values.size(); k++) {
    const double distance = centres[k] - 22.25;
    values[k] = std::exp(-0.5 * std::pow((distance - 0.35) / 0.3, 2.0));
  }
  const ProbeReadings peaked = ReadProbeProfile(centres, values, counts);
  EXPECT_EQ(peaked.edge_bin, edge_bin);
  EXPECT_GT(peaked.peak_delta_deg, peaked.edge_delta_deg);
  EXPECT_NEAR(peaked.peak_delta_deg, 22.65, 0.11);  // the peak bin, within half a bin of 22.65
  EXPECT_NEAR(peaked.separation_deg, peaked.peak_delta_deg - peaked.edge_delta_deg, 1e-12);
}

TEST(SurveyProbe, EmptyProfileCarriesNoReadings) {
  const ProbeReadings empty = ReadProbeProfile({ 16.0, 17.0 }, { 0.0, 0.0 }, { 0, 0 });
  EXPECT_FALSE(empty.has_edge);
  EXPECT_FALSE(empty.has_peak);
  EXPECT_DOUBLE_EQ(empty.separation_deg, 0.0);
}

// The real-assembly smoke at a small lattice (1e5): the probe's edge bin contains the closed-
// form minimum deviation (no event can lie below it), the peak reading is finite, and the
// event counts are positive.
TEST(SurveyProbe, SmallSampleRealAssemblyEdgeBinContainsTheClosedForm) {
  const ConfigManager config = RandomPrismScene();

  const auto configured = config.crystals_.find(1);
  const CrystalConversion crystal = ConvertCrystal(configured->second.param_);
  analytic::FaceNormalTable table;
  analytic::FacePolygonTable polygons;
  ASSERT_EQ(analytic::BuildFaceNormals(crystal.shape, &table, &polygons), analytic::Status::kOk);
  const PoseDensityConversion density = ConvertAxisToPoseDensity(configured->second.axis_);
  ASSERT_TRUE(density.Ok());  // the full-sphere uniform axis is the random family

  const double refractive_index = 1.306;  // 550 nm, the scene's default
  const double closed_form_deg = 2.0 * std::asin(refractive_index * 0.5) * 180.0 / kPi - 60.0;

  ProbeInput input;
  input.table = &table;
  input.polygons = &polygons;
  static const std::vector<int> representative{ 3, 5 };
  input.representative = &representative;
  input.density = &density;
  input.refractive_index = refractive_index;
  input.wavelength_nm = 550.0;
  input.incident[0] = 0.0;
  input.incident[1] = 0.0;
  input.incident[2] = -1.0;
  input.member_count = 12;
  input.sample_count = 100000;  // the seconds-scale smoke; the bench runs the full caliber

  SurveyData document;
  RunSolarRadialProbe(input, &document);
  ASSERT_EQ(document.probes.size(), 1u);
  const SurveyProbe& probe = document.probes.front();
  EXPECT_GT(probe.Value().kept_count, 0);
  EXPECT_TRUE(probe.Value().has_edge);
  EXPECT_TRUE(probe.Value().has_peak);
  EXPECT_GT(probe.Value().peak_value, 0.0);
  // The edge bin contains the closed-form delta_min: no event lies below it.
  const double half_bin = 0.05;
  EXPECT_GE(probe.Value().edge_delta_deg + half_bin, closed_form_deg - 1e-9);
  EXPECT_LE(probe.Value().edge_delta_deg - half_bin, closed_form_deg + 1e-9);
  // The profile rows carry positive effective counts where events landed.
  EXPECT_GT(probe.Value().samples[static_cast<size_t>(probe.Value().edge_bin)].k_eff, 0.0);
  // The spec echo is the caliber. The window is the whole delta axis (the C06/C09 supports sit
  // outside the original [15, 60] slice; one ruler for every class), so the row count follows:
  // 1800 bins at 0.1 degrees, the per-bin lattice size unchanged.
  EXPECT_DOUBLE_EQ(probe.spec.delta_min_deg, 0.0);
  EXPECT_DOUBLE_EQ(probe.spec.delta_max_deg, 180.0);
  EXPECT_EQ(probe.Value().samples.size(), 1800u);
  EXPECT_DOUBLE_EQ(probe.Value().samples.front().delta_deg, 0.05);
  EXPECT_DOUBLE_EQ(probe.Value().samples.back().delta_deg, 179.95);
  EXPECT_DOUBLE_EQ(probe.spec.bin_deg, kProbeBinDeg);
  EXPECT_DOUBLE_EQ(probe.spec.wedge_half_width_deg, kProbeWedgeHalfWidthDeg);
  EXPECT_EQ(probe.spec.sample_count, static_cast<long long>(input.sample_count));
  EXPECT_EQ(probe.spec.member_count, 12);
}

}  // namespace
}  // namespace lumice::raypath::survey
