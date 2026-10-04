#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <future>

#include "lumice_analytic_core.h"

namespace {
LUMICE_ANALYTIC_DiagnosticSource Source() {
  LUMICE_ANALYTIC_DiagnosticSource row{};
  row.crystal.height = 1;
  std::fill_n(row.crystal.face_distance, 6, 1);
  const double pose[]{ std::cos(1.), -std::sin(1.), 0, std::sin(1.), std::cos(1.), 0, 0, 0, 1 };
  std::copy_n(pose, 9, row.pose);
  row.incident[0] = -std::cos(.35);
  row.incident[2] = -std::sin(.35);
  row.refractive_index = 1.31;
  row.token = 57;
  return row;
}
TEST(DiagnosticCapi, RowsOwnershipRootSizeAndFourConcurrentConsumers) {
  const int faces[]{ 3, 5 };
  auto good = Source();
  auto bad = good;
  bad.pose[0] = 5;
  const LUMICE_ANALYTIC_DiagnosticSource rows[]{ good, bad, good };
  auto run = [&]() {
    LUMICE_ANALYTIC_DiagnosticResult result{};
    result.struct_size = sizeof(result);
    if (LUMICE_ANALYTIC_EvaluateDiagnosticBatch(faces, 2, rows, 3, &result) != LUMICE_ANALYTIC_OK) {
      return false;
    }
    const bool ok = result.optical_count == 3 && result.optical[0].path_valid && result.optical[1].input_status != 0 &&
                    result.optical[2].path_valid && result.optical[0].source.token == 57 &&
                    result.optical[0].direction_index_available && result.optical[0].corridor_vertex_count >= 3;
    LUMICE_ANALYTIC_ReleaseDiagnosticResult(&result);
    LUMICE_ANALYTIC_ReleaseDiagnosticResult(&result);
    return ok && result.storage == nullptr && result.optical == nullptr && result.struct_size == sizeof(result);
  };
  std::array<std::future<bool>, 4> threads;
  for (auto& thread : threads) {
    thread = std::async(std::launch::async, run);
  }
  for (auto& thread : threads) {
    EXPECT_TRUE(thread.get());
  }
  constexpr size_t kBase = offsetof(LUMICE_ANALYTIC_DiagnosticResult, curve_point_count);
  alignas(LUMICE_ANALYTIC_DiagnosticResult) unsigned char bytes[sizeof(LUMICE_ANALYTIC_DiagnosticResult)];
  std::fill_n(bytes, sizeof(bytes), 0x6d);
  auto* partial = reinterpret_cast<LUMICE_ANALYTIC_DiagnosticResult*>(bytes);
  partial->struct_size = kBase;
  ASSERT_EQ(LUMICE_ANALYTIC_EvaluateDiagnosticBatch(faces, 2, rows, 3, partial), LUMICE_ANALYTIC_OK);
  EXPECT_EQ(partial->optical_count, 3u);
  LUMICE_ANALYTIC_ReleaseDiagnosticResult(partial);
  for (size_t j = kBase; j < sizeof(bytes); ++j) {
    EXPECT_EQ(bytes[j], 0x6d);
  }
  partial->struct_size = sizeof(uint32_t);
  EXPECT_EQ(LUMICE_ANALYTIC_EvaluateDiagnosticBatch(faces, 2, rows, 3, partial), LUMICE_ANALYTIC_ERR_INVALID_VALUE);
  EXPECT_EQ(LUMICE_ANALYTIC_EvaluateDiagnosticBatch(faces, 2, rows, 3, nullptr), LUMICE_ANALYTIC_ERR_NULL_ARG);
  LUMICE_ANALYTIC_ReleaseDiagnosticResult(nullptr);
}
TEST(DiagnosticCapi, DeviationAndFieldBudgetsRemainNumericOutcomes) {
  const int faces[]{ 3, 5 };
  auto source = Source();
  LUMICE_ANALYTIC_DiagnosticResult result{};
  result.struct_size = sizeof(result);
  ASSERT_EQ(LUMICE_ANALYTIC_CorrectDeviationBatch(faces, 2, &source, 1, 512, 5000, &result), LUMICE_ANALYTIC_OK);
  ASSERT_EQ(result.optical_count, 1u);
  EXPECT_EQ(result.optical[0].solve_status, 0);
  EXPECT_NEAR(result.optical[0].deviation_rad, 2 * std::asin(1.31 * .5) - 3.14159265358979323846 / 3, 1e-10);
  EXPECT_LE(result.path_evaluations, 512u);
  LUMICE_ANALYTIC_ReleaseDiagnosticResult(&result);
  LUMICE_ANALYTIC_WeightedSkySample sample{};
  sample.direction[2] = 1;
  sample.xyz_weight[0] = sample.xyz_weight[1] = sample.xyz_weight[2] = 1;
  const double seed[]{ 0, 0, 1 };
  ASSERT_EQ(LUMICE_ANALYTIC_TraceWeightedSkyField(&sample, 1, seed, 0, 0, .02, 2, 0, 1000, &result),
            LUMICE_ANALYTIC_OK);
  EXPECT_EQ(result.component_evaluations, 0u);
  EXPECT_EQ(result.termination, 5);
  EXPECT_EQ(result.field_count, 0u);
  LUMICE_ANALYTIC_ReleaseDiagnosticResult(&result);
}
}  // namespace
