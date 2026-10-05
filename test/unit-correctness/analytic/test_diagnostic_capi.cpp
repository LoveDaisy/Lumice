#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <future>
#include <vector>

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
  LUMICE_ANALYTIC_DiagnosticResult backing{};
  auto* bytes = reinterpret_cast<unsigned char*>(&backing);
  std::fill_n(bytes, sizeof(backing), 0x6d);
  auto* partial = &backing;
  partial->struct_size = kBase;
  ASSERT_EQ(LUMICE_ANALYTIC_EvaluateDiagnosticBatch(faces, 2, rows, 3, partial), LUMICE_ANALYTIC_OK);
  EXPECT_EQ(partial->optical_count, 3u);
  LUMICE_ANALYTIC_ReleaseDiagnosticResult(partial);
  for (size_t j = kBase; j < sizeof(backing); ++j) {
    EXPECT_EQ(bytes[j], 0x6d);
  }
  partial->struct_size = sizeof(uint32_t);
  EXPECT_EQ(LUMICE_ANALYTIC_EvaluateDiagnosticBatch(faces, 2, rows, 3, partial), LUMICE_ANALYTIC_ERR_INVALID_VALUE);
  EXPECT_EQ(LUMICE_ANALYTIC_EvaluateDiagnosticBatch(faces, 2, rows, 3, nullptr), LUMICE_ANALYTIC_ERR_NULL_ARG);
  LUMICE_ANALYTIC_ReleaseDiagnosticResult(nullptr);
}
TEST(DiagnosticCapi, DeviationStopsBeforeMaterializingTheUnprocessedTail) {
  const int faces[]{ 3, 5 };
  auto source = Source();
  std::vector<LUMICE_ANALYTIC_DiagnosticSource> rows(100000, source);
  LUMICE_ANALYTIC_DiagnosticResult result{};
  result.struct_size = sizeof(result);
  ASSERT_EQ(LUMICE_ANALYTIC_CorrectDeviationBatch(faces, 2, rows.data(), rows.size(), 0, 5000, &result),
            LUMICE_ANALYTIC_OK);
  EXPECT_EQ(result.optical_count, 0u);
  EXPECT_EQ(result.path_evaluations, 0u);
  EXPECT_EQ(result.termination, 6);
  LUMICE_ANALYTIC_ReleaseDiagnosticResult(&result);

  // Less than a single jet: the interrupted row is retained, not the tail.
  ASSERT_EQ(LUMICE_ANALYTIC_CorrectDeviationBatch(faces, 2, rows.data(), rows.size(), 1, 5000, &result),
            LUMICE_ANALYTIC_OK);
  ASSERT_EQ(result.optical_count, 1u);
  EXPECT_EQ(result.optical[0].solve_status, 6);
  EXPECT_EQ(result.optical[0].source.token, source.token);
  EXPECT_EQ(result.path_evaluations, 0u);
  EXPECT_EQ(result.termination, 6);
  LUMICE_ANALYTIC_ReleaseDiagnosticResult(&result);

  const auto begin = std::chrono::steady_clock::now();
  ASSERT_EQ(LUMICE_ANALYTIC_CorrectDeviationBatch(faces, 2, rows.data(), rows.size(), 1000000000, 1, &result),
            LUMICE_ANALYTIC_OK);
  const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
  EXPECT_LT(result.optical_count, rows.size());
  EXPECT_EQ(result.termination, 6);
  EXPECT_LT(elapsed, 1.0);
  LUMICE_ANALYTIC_ReleaseDiagnosticResult(&result);
}

TEST(DiagnosticCapi, DeviationValidatesTheCallBeforeStoppingAndIsolatesBadRows) {
  const int faces[]{ 3, 5 };
  const int invalid_faces[]{ 0, 5 };
  auto good = Source();
  auto bad = good;
  bad.pose[0] = 5;
  bad.token = 58;
  const LUMICE_ANALYTIC_DiagnosticSource rows[]{ good, bad, good };
  LUMICE_ANALYTIC_DiagnosticResult result{};
  result.struct_size = sizeof(result);
  EXPECT_EQ(LUMICE_ANALYTIC_CorrectDeviationBatch(invalid_faces, 2, rows, 3, 0, 5000, &result),
            LUMICE_ANALYTIC_ERR_INVALID_VALUE);
  EXPECT_EQ(result.storage, nullptr);
  EXPECT_EQ(LUMICE_ANALYTIC_CorrectDeviationBatch(faces, 2, nullptr, 3, 0, 5000, &result),
            LUMICE_ANALYTIC_ERR_INVALID_VALUE);
  ASSERT_EQ(LUMICE_ANALYTIC_CorrectDeviationBatch(faces, 2, rows, 3, 4096, 5000, &result), LUMICE_ANALYTIC_OK);
  ASSERT_EQ(result.optical_count, 3u);
  EXPECT_EQ(result.termination, 0);
  EXPECT_EQ(result.optical[0].solve_status, 0);
  EXPECT_NE(result.optical[1].input_status, 0);
  EXPECT_EQ(result.optical[1].source.token, 58u);
  EXPECT_EQ(result.optical[2].solve_status, 0);
  LUMICE_ANALYTIC_ReleaseDiagnosticResult(&result);
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
TEST(DiagnosticCapi, SharedPathSyntaxPrecedesBudgetsForEveryOpticalEntry) {
  const auto source = Source();
  const auto check = [&](int middle, uint64_t budget) {
    const int faces[]{ 3, middle, 5 };
    LUMICE_ANALYTIC_DiagnosticResult result{};
    result.struct_size = sizeof(result);
    EXPECT_EQ(LUMICE_ANALYTIC_EvaluateDiagnosticBatch(faces, 3, &source, 1, &result),
              LUMICE_ANALYTIC_ERR_INVALID_VALUE);
    EXPECT_EQ(result.storage, nullptr);
    LUMICE_ANALYTIC_ReleaseDiagnosticResult(&result);
    EXPECT_EQ(LUMICE_ANALYTIC_TraceDiagnosticInterface(faces, 3, &source, 1, 0, 2, budget, 5000, &result),
              LUMICE_ANALYTIC_ERR_INVALID_VALUE);
    EXPECT_EQ(result.storage, nullptr);
    LUMICE_ANALYTIC_ReleaseDiagnosticResult(&result);
    EXPECT_EQ(LUMICE_ANALYTIC_CorrectDeviationBatch(faces, 3, &source, 1, budget, 5000, &result),
              LUMICE_ANALYTIC_ERR_INVALID_VALUE);
    EXPECT_EQ(result.storage, nullptr);
    LUMICE_ANALYTIC_ReleaseDiagnosticResult(&result);
  };
  check(0, 0);
  check(0, 1000);
  check(-1, 0);
  check(-1, 1000);
  const int faces[]{ 3, 1, 5 };
  LUMICE_ANALYTIC_DiagnosticResult result{};
  result.struct_size = sizeof(result);
  ASSERT_EQ(LUMICE_ANALYTIC_TraceDiagnosticInterface(faces, 3, &source, 1, 0, 2, 0, 5000, &result), LUMICE_ANALYTIC_OK);
  EXPECT_EQ(result.termination, 6);
  LUMICE_ANALYTIC_ReleaseDiagnosticResult(&result);
  const int missing[]{ 3, 99, 5 };
  ASSERT_EQ(LUMICE_ANALYTIC_TraceDiagnosticInterface(missing, 3, &source, 1, 0, 2, 1000, 5000, &result),
            LUMICE_ANALYTIC_OK);
  EXPECT_EQ(result.termination, 4);
  LUMICE_ANALYTIC_ReleaseDiagnosticResult(&result);
}

TEST(DiagnosticCapi, OpticalBracketInterruptionRetainsPointsButNotAPhysicalEndpoint) {
  // A positive TIR source whose reverse step crosses the exit optical gate.
  auto source = Source();
  const double pose[]{ 0.28904806222663315, -0.444630682913122,  0.8477940631634778,
                       -0.629765589733407,  0.5786729389388005,  0.5182016323089559,
                       -0.7210038278059024, -0.6836967058222104, -0.11274881257509889 };
  std::copy_n(pose, 9, source.pose);
  source.incident[0] = -.9999999999999962;
  source.incident[1] = -8.742277657347553e-8;
  source.incident[2] = 0;
  source.refractive_index = 1.3110129100622272;
  const int faces[]{ 7, 2, 5 };
  LUMICE_ANALYTIC_DiagnosticResult result{};
  result.struct_size = sizeof(result);
  ASSERT_EQ(LUMICE_ANALYTIC_TraceDiagnosticInterface(faces, 3, &source, 1, 1, 256, 4096, 5000, &result),
            LUMICE_ANALYTIC_OK);
  ASSERT_EQ(result.termination, 3);
  ASSERT_EQ(result.source_event_count, 1u);
  const auto complete_cost = result.path_evaluations;
  const auto point_count = result.curve_point_count;
  ASSERT_GT(complete_cost, 30u);
  EXPECT_LE(result.source_events[0].source_width_rad, 1e-7);
  LUMICE_ANALYTIC_ReleaseDiagnosticResult(&result);
  const auto interrupted = [&](uint64_t budget) {
    ASSERT_EQ(LUMICE_ANALYTIC_TraceDiagnosticInterface(faces, 3, &source, 1, 1, 256, budget, 5000, &result),
              LUMICE_ANALYTIC_OK);
    EXPECT_EQ(result.termination, 6);
    EXPECT_EQ(result.curve_point_count, point_count);
    EXPECT_GT(result.curve_point_count, 0u);
    EXPECT_EQ(result.source_event_count, 0u);
    EXPECT_LE(result.path_evaluations, budget);
    LUMICE_ANALYTIC_ReleaseDiagnosticResult(&result);
  };
  interrupted(30);
  if (HasFatalFailure()) {
    return;
  }
  interrupted(complete_cost - 6);
  if (HasFatalFailure()) {
    return;
  }
  // The previously complete source-event suffix stays readable after growth.
  constexpr size_t kEvents = offsetof(LUMICE_ANALYTIC_DiagnosticResult, field_terminal_available);
  std::memset(&result, 0x6d, sizeof(result));
  result.struct_size = kEvents;
  ASSERT_EQ(LUMICE_ANALYTIC_TraceDiagnosticInterface(faces, 3, &source, 1, 1, 256, 4096, 5000, &result),
            LUMICE_ANALYTIC_OK);
  ASSERT_EQ(result.source_event_count, 1u);
  EXPECT_EQ(result.curve_point_count, point_count);
  EXPECT_LE(result.source_events[0].source_width_rad, 1e-7);
  EXPECT_EQ(result.field_terminal_available, 0x6d6d6d6d);
  EXPECT_EQ(result.field_terminal_status, 0x6d6d6d6d);
  LUMICE_ANALYTIC_ReleaseDiagnosticResult(&result);
  EXPECT_EQ(result.source_events, nullptr);
  EXPECT_EQ(result.field_terminal_available, 0x6d6d6d6d);
  EXPECT_EQ(result.field_terminal_status, 0x6d6d6d6d);
}

TEST(DiagnosticCapi, InterruptedDeviationReturnsOneConsistentIteration) {
  const auto source = Source();
  const int faces[]{ 3, 5 };
  LUMICE_ANALYTIC_DiagnosticResult partial{};
  partial.struct_size = sizeof(partial);
  ASSERT_EQ(LUMICE_ANALYTIC_CorrectDeviationBatch(faces, 2, &source, 1, 21, 5000, &partial), LUMICE_ANALYTIC_OK);
  ASSERT_EQ(partial.optical_count, 1u);
  EXPECT_EQ(partial.optical[0].solve_status, 6);
  EXPECT_TRUE(partial.optical[0].path_valid);
  EXPECT_TRUE(partial.optical[0].direction_pose_available);
  EXPECT_FALSE(partial.optical[0].deviation_available);
  LUMICE_ANALYTIC_ReleaseDiagnosticResult(&partial);
  LUMICE_ANALYTIC_DiagnosticResult baseline{};
  baseline.struct_size = sizeof(baseline);
  // The first complete derivative snapshot; no line-search evaluation fits.
  ASSERT_EQ(LUMICE_ANALYTIC_CorrectDeviationBatch(faces, 2, &source, 1, 22, 5000, &baseline), LUMICE_ANALYTIC_OK);
  ASSERT_EQ(baseline.optical_count, 1u);
  const auto check = [&](uint64_t budget) {
    SCOPED_TRACE(budget);
    LUMICE_ANALYTIC_DiagnosticResult result{};
    result.struct_size = sizeof(result);
    ASSERT_EQ(LUMICE_ANALYTIC_CorrectDeviationBatch(faces, 2, &source, 1, budget, 5000, &result), LUMICE_ANALYTIC_OK);
    ASSERT_EQ(result.optical_count, 1u);
    const auto& row = result.optical[0];
    EXPECT_EQ(row.solve_status, 6);
    ASSERT_TRUE(row.deviation_available);
    const auto* u = row.source.incident;
    const auto* v = row.outgoing;
    const double cross[]{ u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0] };
    EXPECT_NEAR(row.deviation_rad,
                std::atan2(std::hypot(cross[0], cross[1], cross[2]), u[0] * v[0] + u[1] * v[1] + u[2] * v[2]), 1e-14);
    // Neither the next derivative pass nor its stencil fits: every diagnostic
    // and the source pose must still belong to the first complete snapshot.
    for (int j = 0; j < 9; ++j) {
      EXPECT_EQ(row.source.pose[j], baseline.optical[0].source.pose[j]);
    }
    EXPECT_EQ(row.correction_rad, baseline.optical[0].correction_rad);
    EXPECT_EQ(row.hessian_error, baseline.optical[0].hessian_error);
    EXPECT_EQ(row.objective_curvatures[0], baseline.optical[0].objective_curvatures[0]);
    EXPECT_EQ(row.objective_curvatures[1], baseline.optical[0].objective_curvatures[1]);
    LUMICE_ANALYTIC_ReleaseDiagnosticResult(&result);
  };
  for (uint64_t budget = 23; budget <= 44; ++budget) {
    check(budget);
    if (HasFatalFailure()) {
      break;
    }
  }
  LUMICE_ANALYTIC_ReleaseDiagnosticResult(&baseline);
}

TEST(DiagnosticCapi, FieldTerminalReasonIsSeparateFromConvergedGeometry) {
  LUMICE_ANALYTIC_WeightedSkySample sample{};
  sample.direction[2] = 1;
  const double seed[]{ 0, 0, 1 };
  const auto check = [&](int equation, int expected, uint64_t budget, const double* q) {
    LUMICE_ANALYTIC_DiagnosticResult result{};
    result.struct_size = sizeof(result);
    ASSERT_EQ(LUMICE_ANALYTIC_TraceWeightedSkyField(&sample, 1, q, equation, .3, .02, 2, budget, 5000, &result),
              LUMICE_ANALYTIC_OK);
    EXPECT_EQ(result.field_terminal_available, 1);
    EXPECT_EQ(result.field_terminal_status, expected);
    EXPECT_EQ(result.field_count, expected == 0 ? 1u : 0u);
    EXPECT_EQ(result.termination, expected == 0 ? 3 : expected == 5 ? 5 : 2);
    LUMICE_ANALYTIC_ReleaseDiagnosticResult(&result);
    EXPECT_EQ(result.field_terminal_available, 0);
  };
  check(0, 2, 1000, seed);  // Zero measure is no signal, not physical absence.
  if (HasFatalFailure()) {
    return;
  }
  check(2, 2, 1000, seed);  // The curve entry retains the same reason.
  if (HasFatalFailure()) {
    return;
  }
  std::fill_n(sample.xyz_weight, 3, 1);
  check(2, 3, 1000, seed);  // Constant chromaticity has no level-set normal.
  if (HasFatalFailure()) {
    return;
  }
  check(0, 5, 0, seed);
  if (HasFatalFailure()) {
    return;
  }
  const double distant[]{ std::sin(.3), 0, std::cos(.3) };
  // At most 32 corrections of .01 rad: this farther seed cannot reach the peak.
  const double far_seed[]{ std::sin(.4), 0, std::cos(.4) };
  check(0, 4, 1000, far_seed);
  if (HasFatalFailure()) {
    return;
  }
  check(0, 0, 1000, distant);
  if (HasFatalFailure()) {
    return;
  }
  LUMICE_ANALYTIC_DiagnosticResult partial{};
  partial.struct_size = offsetof(LUMICE_ANALYTIC_DiagnosticResult, field_terminal_status);
  partial.field_terminal_status = 0x6d6d6d6d;
  ASSERT_EQ(LUMICE_ANALYTIC_TraceWeightedSkyField(&sample, 1, seed, 0, 0, .02, 2, 1000, 5000, &partial),
            LUMICE_ANALYTIC_OK);
  EXPECT_EQ(partial.field_count, 1u);
  EXPECT_EQ(partial.field_terminal_available, 0);  // Never expose half a group.
  EXPECT_EQ(partial.field_terminal_status, 0x6d6d6d6d);
  LUMICE_ANALYTIC_ReleaseDiagnosticResult(&partial);
  EXPECT_EQ(partial.field_terminal_status, 0x6d6d6d6d);
}
}  // namespace
