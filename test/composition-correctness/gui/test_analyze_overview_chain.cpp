// Composition chain (742.4): the class overview's model over the survey documents whose
// production costs real engine legs — the arms the unit tier cannot document cheaply, and the
// C01 dispersion anchor the acceptance asks to be pinned against the closed-form minimum.
//
// Units in the chain: the engine's survey assembly (LUMICE_AnalyzePathSurvey, the same entry the
// GUI's executor wraps) × the typed readers × BuildSurveyOverviewModel — the whole path a real
// selection's document takes before the region draws it. No frame, no ImGui.
//
// What the collaboration produces that is observable:
//   * the full-dimensional document's dispersion IS the closed-form slab minimum at both
//     lambdas — restated here from C-visible quantities only (the apex from the edge
//     certificate, the indices from the dispersion echo), the same shape AC6's independent
//     anchor asks for, with the direction the red-inside fact is stated in;
//   * the family document organizes as the freezing ladder the engine walked, with its per-level
//     freeze facts and the survival-filter verdicts.
//
// Cost note: the C01 and family surveys cost real, fixed engine legs (the class tint's two-lambda power
// sums dominate). That cost is the point — these documents cannot be fabricated from outside the
// engine — and it is paid here once per document, in the tier whose pools already carry the
// survey's own price.

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "core/optics.hpp"  // IceRefractiveIndex: the engine's dispersion table, independent of any document echo
#include "gui/analysis_panel.hpp"
#include "gui/analyze_map_view.hpp"  // PointMarkerVisible — the one per-kind visibility rule
#include "gui/analyze_overview.hpp"
#include "gui/analyze_point_view.hpp"
#include "gui/file_io.hpp"
#include "gui/gui_state.hpp"
#include "include/lumice_base.h"
#include "include/lumice_raypath.h"
#include "include/lumice_scene.h"
#include "util/sky_direction.hpp"

namespace lumice::gui {
namespace {

std::unique_ptr<LUMICE_Scene, SceneDeleter> SceneFromConfig(const char* name) {
  const std::string path = std::string(LUMICE_E2E_CONFIG_DIR) + "/" + name;
  LUMICE_Scene* s = nullptr;
  const LUMICE_ErrorCode code = LUMICE_SceneFromJsonFile(path.c_str(), &s);
  EXPECT_EQ(code, LUMICE_OK) << path;
  return std::unique_ptr<LUMICE_Scene, SceneDeleter>(code == LUMICE_OK ? s : nullptr);
}

std::unique_ptr<LUMICE_PathSurvey, SurveyDeleter> RunSurvey(const LUMICE_Scene* scene, const std::vector<int>& faces) {
  std::vector<int> counts(1, static_cast<int>(faces.size()));
  LUMICE_PathSurveyRequest request{};
  request.struct_size = sizeof(request);
  request.crystal_id = 1;
  request.faces = const_cast<int*>(faces.data());
  request.face_count = static_cast<int>(faces.size());
  request.layer_face_counts = counts.data();
  request.layer_count = 1;
  request.symmetry_bits_plus_one = 0;
  request.budget_ms = LUMICE_PATH_SURVEY_DEFAULT_BUDGET_MS;
  LUMICE_PathSurvey* raw = nullptr;
  char err[512] = {};
  const LUMICE_ErrorCode code = LUMICE_AnalyzePathSurvey(scene, &request, &raw, err, sizeof(err));
  EXPECT_EQ(code, LUMICE_OK) << err;
  return std::unique_ptr<LUMICE_PathSurvey, SurveyDeleter>(code == LUMICE_OK ? raw : nullptr);
}

bool HasLine(const std::vector<std::string>& lines, const std::string& needle) {
  for (const std::string& line : lines) {
    if (line.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

}  // namespace

// Reconstructs the reported sun/ROI/path using the existing Parry fixture (h=1.2,
// zenith/roll sigma=1 degree), NOT the unavailable original scene. Only two context
// scopes exist: all checked is physical; none and P-only both request literal.
class ParryContextChain : public ::testing::TestWithParam<std::pair<int, int>> {};

TEST_P(ParryContextChain, SurveyThenPoseUsesTheSelectedScope) {
  auto scene = SceneFromConfig("raypath_symmetry_parry.json");
  ASSERT_NE(scene, nullptr);
  SurveyExecutor executor;
  executor.SetScene(std::move(scene));
  GuiState state;
  const auto [entry_face, bits] = GetParam();
  const int exit_face = entry_face == 8 ? 1 : 2;
  const std::string display = std::to_string(entry_face) + "-" + std::to_string(exit_face);
  auto payload = std::make_shared<AnalysisPayload>();
  payload->snapshot_generation = 7;
  LUMICE_RaypathHistogramEntry row{};
  row.chain_len = 1;
  row.chain[0].crystal_id = 1;
  row.chain[0].segment_len = 2;
  row.chain[0].segment[0] = entry_face;
  row.chain[0].segment[1] = exit_face;
  std::snprintf(row.display, sizeof(row.display), "%s", display.c_str());
  payload->entries.push_back(row);
  state.analysis_result.payload = payload;
  state.analysis.selected_entry = display;
  state.analysis.symmetry_p = (bits & 1) != 0;
  state.analysis.symmetry_b = (bits & 2) != 0;
  state.analysis.symmetry_d = (bits & 4) != 0;
  AltAzToDir(36.0f, 52.0f, state.analysis.sky_point.dir);
  state.analysis.sky_point.valid = true;
  const auto wait_for = [&](auto pump, auto ready) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(45);
    while (!ready() && std::chrono::steady_clock::now() < deadline) {
      pump();
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return ready();
  };
  ASSERT_TRUE(
      wait_for([&] { PumpSurveyQuery(state, executor, 100.0); },
               [&] { return state.analysis.survey.adopted_result || !state.analysis.survey.adopted_error.empty(); }));
  ASSERT_TRUE(state.analysis.survey.adopted_error.empty()) << state.analysis.survey.adopted_error;
  LUMICE_PathSurveyScope scope{};
  scope.struct_size = sizeof(scope);
  ASSERT_EQ(LUMICE_PathSurveyGetScope(state.analysis.survey.adopted_result.get(), &scope), LUMICE_PATH_READ_OK);
  EXPECT_EQ(scope.member_kind, bits == 7 ? LUMICE_PATH_MEMBER_PHYSICAL_L2 : LUMICE_PATH_MEMBER_LITERAL);
  EXPECT_DOUBLE_EQ(scope.sun_altitude_deg, 20.0);
  LUMICE_PathSurveyInfo survey_info{};
  survey_info.struct_size = sizeof(survey_info);
  ASSERT_EQ(LUMICE_PathSurveyGetInfo(state.analysis.survey.adopted_result.get(), &survey_info), LUMICE_PATH_READ_OK);
  ASSERT_TRUE(
      wait_for([&] { PumpPointQuery(state, executor, 100.0); },
               [&] { return state.analysis.point.adopted_result || !state.analysis.point.adopted_error.empty(); }));
  ASSERT_TRUE(state.analysis.point.adopted_error.empty()) << state.analysis.point.adopted_error;
  LUMICE_PathPointInfo point_info{};
  point_info.struct_size = sizeof(point_info);
  ASSERT_EQ(LUMICE_PathPointGetInfo(state.analysis.point.adopted_result.get(), &point_info), LUMICE_PATH_READ_OK);
  EXPECT_NEAR(point_info.target_altitude_deg, 36.0, 1e-4);
  EXPECT_NEAR(point_info.target_azimuth_deg, 52.0, 1e-4);
  ASSERT_EQ(point_info.face_count, 2);
  EXPECT_EQ(point_info.faces[0], entry_face);
  EXPECT_EQ(point_info.faces[1], exit_face);
  LUMICE_PathContext context{};
  context.struct_size = sizeof(context);
  // Pose deliberately does not request a context block. The Workspace/C tests exercise
  // actual context/profile consumption; automatic warm still supplies validated solver starts.
  EXPECT_EQ(LUMICE_PathPointGetContext(state.analysis.point.adopted_result.get(), &context),
            LUMICE_PATH_READ_NOT_AVAILABLE);
  std::printf(
      "Parry reconstruction path=%s PBD=%d context_selection=%d survey_execution=%d members=%zu "
      "point_execution=%d context_requested=0\n",
      display.c_str(), bits, state.analysis.point.key.selection.symmetry_bits_plus_one, survey_info.execution,
      scope.routing_member_count, point_info.execution);

  // Change only symmetry while a point is in flight. The old identity must not land
  // under the new selection, even though the literal fiber path and target did not move.
  state.analysis.symmetry_p = state.analysis.symmetry_b = state.analysis.symmetry_d = bits != 7;
  PumpPointQuery(state, executor, 101.0);
  const auto first = state.analysis.point.intent_id;
  state.analysis.symmetry_p = state.analysis.symmetry_b = state.analysis.symmetry_d = bits == 7;
  PumpPointQuery(state, executor, 102.0);
  EXPECT_NE(state.analysis.point.intent_id, first);
  EXPECT_EQ(state.analysis.point.adopted_result, nullptr);
  ASSERT_TRUE(
      wait_for([&] { PumpPointQuery(state, executor, 103.0); },
               [&] { return state.analysis.point.adopted_result || !state.analysis.point.adopted_error.empty(); }));
  EXPECT_TRUE(state.analysis.point.adopted_error.empty()) << state.analysis.point.adopted_error;
  EXPECT_EQ(state.analysis.point.key.selection.symmetry_bits_plus_one, bits == 7 ? 0 : 1);
  executor.ShutdownAndJoin();
}

INSTANTIATE_TEST_SUITE_P(ReportedPaths, ParryContextChain,
                         ::testing::Values(std::make_pair(8, 7), std::make_pair(8, 0), std::make_pair(8, 1),
                                           std::make_pair(4, 7), std::make_pair(4, 0), std::make_pair(4, 1)));

// The C01 anchor: the 3-5 class on the fully random prism, read into the overview model, with
// the dispersion restated from the document's own quantities against the closed-form slab
// minimum 2*asin(n*sin(A/2)) - A — the relation the certified edge's certificate already names.
TEST(AnalyzeOverviewChain, FullDimensionalDispersionAnchorsToTheClosedForm) {
  const auto scene = SceneFromConfig("raypath_feature_random_regular.json");
  const auto survey = RunSurvey(scene.get(), { 3, 5 });
  ASSERT_NE(survey.get(), nullptr);

  const SurveyOverviewModel model = BuildSurveyOverviewModel(*survey);
  ASSERT_TRUE(model.valid);
  ASSERT_EQ(model.arm, SurveyArm::kFullDimensional);
  EXPECT_EQ(model.execution, LUMICE_PATH_OUTCOME_COMPLETED);
  EXPECT_FALSE(model.early_stopped);

  // The dispersion block: both lambdas named by the engine's own labels, the min-deviation row
  // present, red inside blue, and the displacement equal to the row's own spread.
  ASSERT_TRUE(model.dispersion.available);
  EXPECT_EQ(model.dispersion.red_label, std::string("650nm"));
  EXPECT_EQ(model.dispersion.blue_label, std::string("450nm"));
  ASSERT_TRUE(model.dispersion.min_row_available);
  EXPECT_LT(model.dispersion.value_red_deg, model.dispersion.value_blue_deg);
  EXPECT_EQ(model.dispersion.direction, std::string("red_inner_blue_outer"));
  EXPECT_NEAR(model.dispersion.displacement_deg, model.dispersion.value_blue_deg - model.dispersion.value_red_deg,
              1e-9);
  EXPECT_GE(model.dispersion.followed.size(), 1u);
  for (const SurveyFollowedOnsetView& row : model.dispersion.followed) {
    EXPECT_EQ(row.direction, std::string("red_inner_blue_outer"));
  }

  // The anchor (AC6), in two independent legs:
  //
  // 1. The corpus leg — pure arithmetic, no engine in the loop. The cases census registers the
  //    C01 two-face anchor as n=1.307/1.317 → δ=21.612°/22.371° through the same closed form.
  //    Those registered values reproduce to their own rounding granularity (three decimals of
  //    degrees), which is what the band below holds — the formula itself is pinned to the
  //    corpus, so a wrong relation cannot pass by agreeing with a wrong table.
  {
    // Not `M_PI`: that one is a POSIX extension, and MSVC only defines it behind
    // _USE_MATH_DEFINES (same shape as test_preview_projection_chain.cpp's kPiF).
    constexpr double kPi = 3.14159265358979323846;
    const double deg = 180.0 / kPi;
    const double closed_1307 = (2.0 * std::asin(1.307 * 0.5) - kPi / 3.0) * deg;
    const double closed_1317 = (2.0 * std::asin(1.317 * 0.5) - kPi / 3.0) * deg;
    EXPECT_NEAR(closed_1307, 21.612, 5e-4);
    EXPECT_NEAR(closed_1317, 22.371, 5e-4);
  }
  //
  // 2. The engine leg — the indices come from the engine's own dispersion table
  //    (IceRefractiveIndex), NOT from the document's echo, so a wrong table entry cannot
  //    certify itself: the closed form is restated from the certificate's apex and the table's
  //    n, and the echo is separately required to equal the table. The kernel's two-face interior
  //    minimum IS the closed form, so the restatement's only error source is the acos of the
  //    apex itself — a 1e-4° band keeps decades of margin.
  const double table_n_red = IceRefractiveIndex::Get(650.0);
  const double table_n_blue = IceRefractiveIndex::Get(450.0);
  EXPECT_NEAR(model.dispersion.n_red, table_n_red, 1e-12);
  EXPECT_NEAR(model.dispersion.n_blue, table_n_blue, 1e-12);
  if (model.dispersion.certificate_available) {
    constexpr double kPi = 3.14159265358979323846;
    const double a_rad = model.dispersion.certificate_apex_deg * kPi / 180.0;
    const double deg = 180.0 / kPi;
    const double closed_red = (2.0 * std::asin(table_n_red * std::sin(0.5 * a_rad)) - a_rad) * deg;
    const double closed_blue = (2.0 * std::asin(table_n_blue * std::sin(0.5 * a_rad)) - a_rad) * deg;
    EXPECT_NEAR(closed_red, model.dispersion.value_red_deg, 1e-4);
    EXPECT_NEAR(closed_blue, model.dispersion.value_blue_deg, 1e-4);
    // The certificate itself is honest: its cross-check sits inside its own tolerance.
    EXPECT_LE(model.dispersion.certificate_cross_check_deg, model.dispersion.certificate_tolerance_deg);
    EXPECT_EQ(model.dispersion.certificate_formula, std::string("2*asin(n*sin(A/2)) - A"));
  } else {
    FAIL() << "the C01 class's caustic edge should carry the closed-form certificate";
  }

  // The edge/bright split (AC3), organized as the first-class inner-edge object (817.2): the
  // class answer's FIRST line is the object's main sentence (about 21.92°, the light-begins
  // claim), the premise note names the whole-sky fact at the survey's fixed λ, and the four
  // readings live in the object's view — the flat lines they came from do not repeat. The
  // support set keeps its own line (another structure, still readable).
  ASSERT_FALSE(model.conclusions.empty());
  EXPECT_EQ(model.conclusions.front().find("Inner edge (3-5): about 21.92° from the sun"), 0u)
      << model.conclusions.front();
  EXPECT_NE(model.conclusions.front().find("light begins"), std::string::npos);
  EXPECT_TRUE(HasLine(std::vector<std::string>{ model.inner_edge.premise_note }, "Whole-sky"));
  EXPECT_NE(model.inner_edge.premise_note.find("550 nm"), std::string::npos) << model.inner_edge.premise_note;
  EXPECT_TRUE(HasLine(model.conclusions, "Support:"));

  // The qualified object itself: the geometric edge (the certificate card's own number), the
  // profile cut's two finite-resolution readings, and the fixed chromatic pair — the four
  // readings' numbers pinned to the same closed-form anchors the dispersion block holds above.
  ASSERT_TRUE(model.inner_edge.qualified);
  EXPECT_EQ(model.inner_edge.name, std::string("3-5"));
  EXPECT_GE(model.inner_edge.edge_object_index, 0);
  EXPECT_NEAR(model.inner_edge.edge_deg, 21.9161264398004, 1e-9);
  EXPECT_TRUE(model.inner_edge.certificate_available);
  EXPECT_NEAR(model.inner_edge.wavelength_nm, model.wavelength_nm, 1e-9);
  ASSERT_TRUE(model.inner_edge.first_bin.available);
  EXPECT_NEAR(model.inner_edge.first_bin.deg, 21.95, 1e-9);
  ASSERT_TRUE(model.inner_edge.highest_bin.available);
  EXPECT_NEAR(model.inner_edge.highest_bin.deg, 22.05, 1e-9);
  EXPECT_NEAR(model.inner_edge.bin_deg, 0.1, 1e-9);
  EXPECT_EQ(model.inner_edge.name, std::string("3-5"));
  EXPECT_TRUE(model.inner_edge.member_kind_available);
  EXPECT_EQ(model.inner_edge.member_kind, LUMICE_PATH_MEMBER_PHYSICAL_L2);
  ASSERT_TRUE(model.inner_edge.dispersion_available);
  EXPECT_NEAR(model.inner_edge.dispersion_red_deg, model.dispersion.value_red_deg, 1e-12);
  EXPECT_NEAR(model.inner_edge.dispersion_blue_deg, model.dispersion.value_blue_deg, 1e-12);
  EXPECT_NEAR(model.inner_edge.dispersion_gap_deg, 0.5809619687491399, 1e-9);
  EXPECT_EQ(model.inner_edge.dispersion_red_label, std::string("650nm"));
  EXPECT_EQ(model.inner_edge.dispersion_blue_label, std::string("450nm"));
  // The migrated readings leave no flat echo behind: the qualified card's edge line, the
  // member-matched bright peak and the dispersion summary are the object's now.
  EXPECT_FALSE(HasLine(model.conclusions, "Sharp edge at"));
  EXPECT_FALSE(HasLine(model.conclusions, "Brightness peaks near"));
  EXPECT_FALSE(HasLine(model.conclusions, "Wavelength dispersion"));

  // The probe block: the reference-wavelength profile, with readings and samples whose empty
  // bins are NaN gaps (the profile's far wings hold empty bins — the gap rule is load-bearing).
  ASSERT_TRUE(model.probe.available);
  EXPECT_NEAR(model.probe.wavelength_nm, model.wavelength_nm, 1e-9);
  EXPECT_TRUE(model.probe.edge_available);
  EXPECT_TRUE(model.probe.peak_available);
  ASSERT_GE(model.probe.sample_value.size(), 2u);
  EXPECT_EQ(model.probe.sample_value.size(), model.probe.sample_delta_deg.size());
  bool saw_nan_gap = false;
  double max_value = 0.0;
  for (const double v : model.probe.sample_value) {
    if (std::isnan(v)) {
      saw_nan_gap = true;
    } else {
      max_value = std::max(max_value, v);
    }
  }
  EXPECT_TRUE(saw_nan_gap);
  EXPECT_GT(max_value, 0.0);

  // The weight-side reading is present and separate: the diagnose block and the tint block both
  // carry their own basis/caliber strings.
  EXPECT_TRUE(model.diagnose.available);
  EXPECT_FALSE(model.diagnose.verdict_kind.empty());
  ASSERT_TRUE(model.tint.available);
  EXPECT_TRUE(model.tint.reading_available);
  EXPECT_NE(model.tint.caliber.find("not a disk-integrated D65"), std::string::npos);

  // The dispersion basis names its geometry in the engine's own words (the口径 the user reads).
  EXPECT_NE(model.dispersion.evidence_basis.find("index pair"), std::string::npos);
}

// The family arm: the freezing ladder the engine walked organizes as levels — frozen dims with
// their centers and widths, free dims named, per-level sampling and survival-filter verdicts.
TEST(AnalyzeOverviewChain, FamilyArmOrganizesAsTheFreezingLadder) {
  const auto scene = SceneFromConfig("raypath_feature_column.json");
  const auto survey = RunSurvey(scene.get(), { 3, 5 });
  ASSERT_NE(survey.get(), nullptr);

  const SurveyOverviewModel model = BuildSurveyOverviewModel(*survey);
  ASSERT_TRUE(model.valid);
  ASSERT_EQ(model.arm, SurveyArm::kFamily);
  // The first-class inner-edge object is the random family's own: a family-restricted measure is
  // not rotation invariant, so its readable angular distances never upgrade to the whole-sky
  // object, whatever cards the ladder delivers beside it.
  EXPECT_FALSE(model.inner_edge.qualified);

  ASSERT_TRUE(model.family.available);
  ASSERT_GE(model.family.levels.size(), 2u);
  // The first freeze names its axis with a center and a width; the last level kills candidates
  // by the survival filter. Both are the ladder's own facts, carried through.
  EXPECT_FALSE(model.family.levels[1].frozen_dims.empty());
  EXPECT_NE(model.family.levels[1].frozen_dims[0].find("zenith"), std::string::npos);
  EXPECT_GT(model.family.levels[1].samples, 0);
  EXPECT_GE(model.family.levels.back().objects_killed, 0);

  int surviving = 0;
  for (const SurveyFamilyLevelView& level : model.family.levels) {
    surviving += level.objects_proposed - level.objects_killed;
  }
  EXPECT_GT(surviving, 0);
  // The ladder's mechanism sentence left the conclusions (the layering's owner example: the
  // frozen-dim enumeration is the diagnostics group's content); what stays is the consequence in one
  // user sentence.
  EXPECT_FALSE(HasLine(model.conclusions, "Freezing ladder"));
  EXPECT_TRUE(HasLine(model.conclusions, "survive the ladder"));

  // The ladder's image cards come through as objects tagged with their level, and the tint leg
  // ran for the family too.
  ASSERT_FALSE(model.objects.empty());
  bool saw_level_tag = false;
  for (const SurveyObjectRow& row : model.objects) {
    if (row.family_level >= 0) {
      saw_level_tag = true;
    }
  }
  EXPECT_TRUE(saw_level_tag);
  ASSERT_TRUE(model.tint.available);
  EXPECT_TRUE(model.tint.reading_available);
}

// The deadline document (plan Step 2's "a partial document still shows its produced legs"):
// a one-millisecond query deadline cuts the survey in the T0 routing leg, so the document
// carries the deadline outcome and no produced quantities. The cut rides the workspace query
// face — the same face the GUI's executor wraps, and the one where a deadline actually wires
// into the query's stop (the direct LUMICE_AnalyzePathSurvey entry echoes budget_ms without
// enforcing it; its budget is the workspace/options callers' contract). Two invariants, both
// cheap and machine-speed-independent (they hang on flags, not on which legs beat the clock):
//   * the arm classification does NOT read a timed-out document as the identity arm —
//     early_stopped is the identity fast path's own flag, and a deadline document has it false
//     whatever any later leg managed (the engine-side half of that invariant is asserted here
//     on the document itself, the GUI-side half in the model);
//   * the model stays honest about the partial: the deadline outcome, no tint block, and no
//     conclusion line claiming a physical fact about the class.
TEST(AnalyzeOverviewChain, DeadlineDocumentKeepsItsArmAndItsProducedLegs) {
  const auto scene = SceneFromConfig("raypath_feature_random_regular.json");
  LUMICE_PathWorkspace* raw_workspace = nullptr;
  char err[512] = {};
  ASSERT_EQ(LUMICE_PathWorkspaceCreate(scene.get(), &raw_workspace, err, sizeof(err)), LUMICE_OK) << err;
  std::unique_ptr<LUMICE_PathWorkspace, void (*)(LUMICE_PathWorkspace*)> workspace(raw_workspace,
                                                                                   LUMICE_PathWorkspaceDestroy);

  std::vector<int> faces{ 3, 5 };
  std::vector<int> counts(1, 2);
  LUMICE_PathSurveyRequest request{};
  request.struct_size = sizeof(request);
  request.crystal_id = 1;
  request.faces = faces.data();
  request.face_count = static_cast<int>(faces.size());
  request.layer_face_counts = counts.data();
  request.layer_count = 1;
  request.symmetry_bits_plus_one = 0;
  request.budget_ms = LUMICE_PATH_SURVEY_DEFAULT_BUDGET_MS;
  LUMICE_PathQueryOptions options{};
  options.struct_size = sizeof(options);
  options.deadline_ms = 1;
  LUMICE_PathSurvey* raw = nullptr;
  ASSERT_EQ(LUMICE_PathWorkspaceQuerySurvey(workspace.get(), &request, &options, &raw, err, sizeof(err)), LUMICE_OK)
      << err;
  std::unique_ptr<LUMICE_PathSurvey, SurveyDeleter> survey(raw);
  ASSERT_NE(survey.get(), nullptr);

  // The engine-side invariant: the deadline document does not carry the identity fast path's
  // early-stop flag.
  LUMICE_PathSurveyInfo info{};
  info.struct_size = sizeof(info);
  ASSERT_EQ(LUMICE_PathSurveyGetInfo(survey.get(), &info), LUMICE_PATH_READ_OK);
  EXPECT_EQ(info.execution, LUMICE_PATH_OUTCOME_DEADLINE_EXCEEDED);
  EXPECT_EQ(info.early_stopped, 0);

  const SurveyOverviewModel model = BuildSurveyOverviewModel(*survey);
  ASSERT_TRUE(model.valid);
  EXPECT_EQ(model.execution, LUMICE_PATH_OUTCOME_DEADLINE_EXCEEDED);
  EXPECT_FALSE(model.early_stopped);
  EXPECT_NE(model.arm, SurveyArm::kIdentity);

  // The partial is stated, not papered over: no tint block (the leg never ran to a reading),
  // and no conclusion line claims anything physical about the class.
  EXPECT_FALSE(model.tint.available);
  for (const std::string& line : model.conclusions) {
    EXPECT_EQ(line.find("Source image"), std::string::npos) << line;
    EXPECT_EQ(line.find("certified"), std::string::npos) << line;
  }
}

// The map overlay layers' display facts (the layer plan's Step 1), read back from the real C01
// 3-5 document — the reach/edge payloads the map's arc layers draw. Every number here is a
// typed reader's own: the merged range's endpoints trace to LUMICE_PathSurveyGetSupportInterval,
// the edge's coincidence flag to the card's own declaration, and the turn positions to the
// diagnose features' visible discriminants — never to the TIR quantity/share calibers.
TEST(AnalyzeOverviewChain, OverlayFactsReadBackSupportEdgeAndTurns) {
  const auto scene = SceneFromConfig("raypath_feature_random_regular.json");
  const auto survey = RunSurvey(scene.get(), { 3, 5 });
  ASSERT_NE(survey.get(), nullptr);

  const SurveyOverlayFacts facts = BuildSurveyOverlayFacts(*survey);

  // The reach layer: C01's two delivered intervals meet at their shared endpoint, so the merged
  // reach is ONE continuous range — no unreachable gap invented between exactly-touching
  // intervals, and not the full [0, 180] range (the bounds draw as arcs, not as the text form).
  ASSERT_EQ(facts.reach_intervals.size(), 1u);
  EXPECT_FALSE(facts.reach_full_range);
  EXPECT_GT(facts.reach_intervals.front().lower_deg, 21.0);
  EXPECT_LT(facts.reach_intervals.front().lower_deg, 23.0);
  EXPECT_GT(facts.reach_intervals.front().upper_deg, 43.5);
  EXPECT_LE(facts.reach_intervals.front().upper_deg, 180.0);

  // The edge layer: the class's certified caustic edge, whose card declares the coincidence with
  // support — the flag the map's shared stroke keys on, passing through verbatim. `object_index`
  // is the card's own object index — the selection's owner key — and names the SAME card the
  // overview model's qualified inner-edge object points at.
  ASSERT_GE(facts.edges.size(), 1u);
  EXPECT_TRUE(facts.edges.front().coincident_with_support);
  EXPECT_TRUE(facts.edges.front().certificate_available);
  EXPECT_TRUE(facts.edges.front().concluded);
  EXPECT_NEAR(facts.edges.front().position_deg, facts.reach_intervals.front().lower_deg, 0.05)
      << "the shared stroke's geometry: the edge sits at the support's inner bound";
  {
    // The card's index, read back the way the builder reads it: the first caustic-edge object.
    int edge_index = -1;
    LUMICE_PathSurveyInfo info{};
    info.struct_size = sizeof(info);
    ASSERT_EQ(LUMICE_PathSurveyGetInfo(survey.get(), &info), LUMICE_PATH_READ_OK);
    for (size_t i = 0; i < info.object_count; ++i) {
      LUMICE_PathSurveyObject obj{};
      obj.struct_size = sizeof(obj);
      if (LUMICE_PathSurveyGetObject(survey.get(), i, &obj) == LUMICE_PATH_READ_OK &&
          obj.kind == LUMICE_PATH_SURVEY_CAUSTIC_EDGE) {
        edge_index = static_cast<int>(i);
        break;
      }
    }
    ASSERT_GE(edge_index, 0);
    EXPECT_EQ(facts.edges.front().object_index, static_cast<size_t>(edge_index));
    const SurveyOverviewModel model = BuildSurveyOverviewModel(*survey);
    ASSERT_TRUE(model.inner_edge.qualified);
    EXPECT_EQ(model.inner_edge.edge_object_index, edge_index);
  }

  // The turn layer: whatever visible internal-reflection discriminants the document carries,
  // each is a parsed turn (a face number from the source grammar, positions inside the axis) —
  // and the TIR quantity/share readers never produce a position on their own (a share without a
  // visible discriminant feature leaves the layer empty, not decorated).
  for (const SurveyOverlayFacts::TirTurn& turn : facts.tir_turns) {
    EXPECT_GE(turn.step, 0) << turn.source;
    EXPECT_GT(turn.delta_red_deg, 0.0);
    EXPECT_LT(turn.delta_red_deg, 180.0);
    EXPECT_GT(turn.delta_blue_deg, 0.0);
    EXPECT_LT(turn.delta_blue_deg, 180.0);
  }
  bool has_visible_tir_feature = false;
  LUMICE_PathDiagnose diagnose{};
  diagnose.struct_size = sizeof(diagnose);
  if (LUMICE_PathSurveyGetDiagnose(survey.get(), &diagnose) == LUMICE_PATH_READ_OK) {
    for (size_t i = 0; i < diagnose.feature_count; ++i) {
      LUMICE_PathDiagnoseFeature feature{};
      feature.struct_size = sizeof(feature);
      if (LUMICE_PathSurveyGetDiagnoseFeature(survey.get(), i, &feature) != LUMICE_PATH_READ_OK ||
          feature.visible == 0) {
        continue;
      }
      const std::string source = SurveyTextString(feature.source);
      if (source.rfind("internal_", 0) == 0 && source.find("_tir_discriminant") != std::string::npos) {
        has_visible_tir_feature = true;
      }
    }
  }
  EXPECT_EQ(facts.tir_turns.empty(), !has_visible_tir_feature)
      << "the turn layer reads exactly the visible internal-reflection discriminants";
}

// The layer plan's AC4 switching half, integrated on real documents at this tier: the
// identity arm's document carries THE positioned row the map's points group draws (the
// certified source image at the sun), and the per-kind predicate — the one rule the draw loop,
// hit collection and legend consume — hides exactly that row under its own gate while the
// parent gate removes everything. The full two-kind switching matrix lives at the unit tier
// (the controlled fixture: all six kinds, both sides of every gate); no canonical corpus
// document currently carries two positioned kinds, so the real-row leg holds the integration,
// not the matrix.
TEST(AnalyzeOverviewChain, IdentityDocumentRowSwitchesUnderItsOwnKindGate) {
  const auto scene = SceneFromConfig("raypath_feature_random_regular.json");
  const auto survey = RunSurvey(scene.get(), { 1, 3, 6, 2 });
  ASSERT_NE(survey.get(), nullptr);

  const SurveyOverviewModel model = BuildSurveyOverviewModel(*survey);
  ASSERT_TRUE(model.valid);
  ASSERT_EQ(model.arm, SurveyArm::kIdentity) << "the rank-0 chain routes to the identity arm on this scene";
  ASSERT_EQ(model.objects.size(), 1u);
  ASSERT_TRUE(model.objects[0].position_available);
  EXPECT_EQ(model.objects[0].kind_name, "source image");

  SurveyLayerGates gates;  // everything on: the positioned row is visible
  EXPECT_TRUE(PointMarkerVisible(model.objects[0], gates));
  // The row's own kind gate hides exactly it; another kind's gate (the co-located pair's other
  // half) would not — asserted here on the real row for the source-image half, the pair's
  // regular/collapse halves at the unit tier's controlled fixture.
  gates.points_kinds.source_image = false;
  EXPECT_FALSE(PointMarkerVisible(model.objects[0], gates));
  gates.points_kinds = SurveyPointKindGates{};
  gates.points.graphics = false;
  EXPECT_FALSE(PointMarkerVisible(model.objects[0], gates));
}

// The full-dimensional C01 document's own shape at the facts layer: no positioned object cards
// (its support/edge/bright cards are deviation-axis structures), so the map's points group
// names the honest empty state for this class — the per-kind gates stand with nothing to hide.
TEST(AnalyzeOverviewChain, FullDimensionalDocumentCarriesNoPositionedCards) {
  const auto scene = SceneFromConfig("raypath_feature_random_regular.json");
  const auto survey = RunSurvey(scene.get(), { 3, 5 });
  ASSERT_NE(survey.get(), nullptr);
  const SurveyOverviewModel model = BuildSurveyOverviewModel(*survey);
  ASSERT_TRUE(model.valid);
  bool any_positioned = false;
  for (const SurveyObjectRow& row : model.objects) {
    any_positioned = any_positioned || row.position_available;
  }
  EXPECT_FALSE(any_positioned);
}

// The TIR caliber's metadata premise, on the real chromatic document the layer's turns draw
// from: the dispersion block can carry its red/blue wavelength labels WITHOUT a minimum-deviation
// row (the producer fills the labels unconditionally, the min row is its own conditional), and
// the same document can hold visible internal-reflection discriminants — the exact shape where a
// min-row-gated caliber would wrongly report "wavelength unknown". The draw-side wiring is the
// unit tier's; this holds the document shape the assembly must read.
TEST(AnalyzeOverviewChain, ChromaticDocumentCarriesLabelsAndTurnsWithoutMinRow) {
  const auto scene = SceneFromConfig("raypath_feature_random_regular.json");
  const auto survey = RunSurvey(scene.get(), { 3, 1, 5 });
  ASSERT_NE(survey.get(), nullptr);

  const SurveyOverviewModel model = BuildSurveyOverviewModel(*survey);
  ASSERT_TRUE(model.valid);
  ASSERT_TRUE(model.dispersion.available) << "the chromatic document carries the dispersion block";
  EXPECT_FALSE(model.dispersion.red_label.empty());
  EXPECT_FALSE(model.dispersion.blue_label.empty());
  EXPECT_FALSE(model.dispersion.min_row_available) << "the witness shape: labels without a minimum-deviation row";

  const SurveyOverlayFacts facts = BuildSurveyOverlayFacts(*survey);
  ASSERT_FALSE(facts.tir_turns.empty()) << "the same document carries visible internal-reflection turns";
  for (const SurveyOverlayFacts::TirTurn& turn : facts.tir_turns) {
    EXPECT_GE(turn.step, 0) << turn.source;
  }
  // If this document qualifies an inner edge (a concluded, coincident edge card), the pair's
  // absence is the object's named state — never a silent reading (AC5's honesty at the object
  // tier). Unqualified documents keep the pair in the flat conclusion as before.
  if (model.inner_edge.qualified) {
    EXPECT_FALSE(model.inner_edge.dispersion_available);
    EXPECT_FALSE(model.inner_edge.dispersion_unavailable.empty());
  }
}

}  // namespace lumice::gui
