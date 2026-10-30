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
//   * the refused-support document keeps its independent facts beside the refusal — the
//     witness's positive event and the TIR share stand next to the named refusal, and the
//     chromatic layer's absence carries the engine's own reason (AC4's coexistence);
//   * the family document organizes as the freezing ladder the engine walked, with its per-level
//     freeze facts and the survival-filter verdicts.
//
// Cost note: the three surveys cost real, fixed engine legs (the class tint's two-lambda power
// sums dominate). That cost is the point — these documents cannot be fabricated from outside the
// engine — and it is paid here once per document, in the tier whose pools already carry the
// survey's own price.

#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "core/optics.hpp"  // IceRefractiveIndex: the engine's dispersion table, independent of any document echo
#include "gui/analyze_overview.hpp"
#include "include/lumice_base.h"
#include "include/lumice_raypath.h"
#include "include/lumice_scene.h"

namespace lumice::gui {
namespace {

struct SceneDeleter {
  void operator()(LUMICE_Scene* s) const { LUMICE_SceneDestroy(s); }
};
struct SurveyDeleter {
  void operator()(LUMICE_PathSurvey* r) const { LUMICE_PathSurveyDestroy(r); }
};

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
    const double deg = 180.0 / M_PI;
    const double closed_1307 = (2.0 * std::asin(1.307 * 0.5) - M_PI / 3.0) * deg;
    const double closed_1317 = (2.0 * std::asin(1.317 * 0.5) - M_PI / 3.0) * deg;
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
    const double a_rad = model.dispersion.certificate_apex_deg * M_PI / 180.0;
    const double deg = 180.0 / M_PI;
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

  // The edge/bright split (AC3): a certified geometric edge card and a separate bright-domain
  // card whose peak is a probe reading. The conclusions keep the two apart, and name the
  // dispersion line.
  EXPECT_TRUE(HasLine(model.conclusions, "Sharp edge"));
  EXPECT_TRUE(HasLine(model.conclusions, "certified by the closed-form minimum"));
  EXPECT_TRUE(HasLine(model.conclusions, "Brightness peaks near"));
  EXPECT_TRUE(HasLine(model.conclusions, "Wavelength dispersion"));
  EXPECT_TRUE(HasLine(model.conclusions, "Support:"));

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

// The refused-support arm (the C17 shape): the refusal and the independent facts coexist in one
// document and in one model — the named refusal, the witness's positive event, the TIR share —
// with the chromatic layer honestly unevaluated in the engine's own words.
TEST(AnalyzeOverviewChain, RefusedSupportKeepsItsIndependentFacts) {
  const auto scene = SceneFromConfig("raypath_feature_random_regular.json");
  const auto survey = RunSurvey(scene.get(), { 3, 1, 5, 7 });
  ASSERT_NE(survey.get(), nullptr);

  const SurveyOverviewModel model = BuildSurveyOverviewModel(*survey);
  ASSERT_TRUE(model.valid);
  ASSERT_EQ(model.arm, SurveyArm::kRefused);

  // The refusal names its mode, and the conclusions say what the refusal is NOT.
  EXPECT_EQ(model.evidence_state, LUMICE_PATH_STATE_REFUSED);
  EXPECT_EQ(model.evidence_mode, std::string("slab_crease_touching"));
  EXPECT_TRUE(HasLine(model.conclusions, "Support refused"));
  EXPECT_TRUE(HasLine(model.conclusions, "slab_crease_touching"));
  EXPECT_TRUE(HasLine(model.conclusions, "not a statement that the class carries no light"));

  // The witness: the declared source's own positive event, readable.
  ASSERT_TRUE(model.witness.available);
  EXPECT_TRUE(model.witness.value_available);
  EXPECT_GT(model.witness.kept_count, 0);
  EXPECT_GT(model.witness.sample_count, 0);
  EXPECT_TRUE(HasLine(model.conclusions, "independent witness"));

  // The TIR share: produced beside the refusal, both lambdas.
  ASSERT_TRUE(model.tir.available);
  EXPECT_TRUE(model.tir.value_available);
  EXPECT_GT(model.tir.share_all_tir_red, 0.0);
  EXPECT_GT(model.tir.share_all_tir_blue, 0.0);

  // The chromatic layer sits downstream of the refused axis and says so in the engine's words —
  // unevaluated, not absent, not an error (AC3's 如实区分).
  EXPECT_FALSE(model.dispersion.available);
  EXPECT_FALSE(model.diagnose.available);
  EXPECT_EQ(model.chromatic_state, LUMICE_PATH_STATE_UNEVALUATED);
  EXPECT_NE(model.chromatic_note.find("the delta axis refused"), std::string::npos);

  // The refused card is the one object, refused, with the mode as its reason.
  ASSERT_GE(model.objects.size(), 1u);
  EXPECT_EQ(model.objects[0].state, LUMICE_PATH_STATE_REFUSED);
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
  EXPECT_TRUE(HasLine(model.conclusions, "Freezing ladder"));
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

// The wide-diffusion shape (C04): the open-arc class carries a support and a bright domain but
// NO caustic edge — and its dispersion table has no minimum-deviation fold, only followed
// onsets. The overview renders exactly that: no sharp-edge conclusion, no certificate, and a
// dispersion block that says the followed onsets are the positional facts. The probe's peak
// stays a finite-resolution reading, never a fabricated "brightest point".
TEST(AnalyzeOverviewChain, WideDiffusionClassNeedsNoSharpStructure) {
  const auto scene = SceneFromConfig("raypath_feature_random_regular.json");
  const auto survey = RunSurvey(scene.get(), { 3, 1, 4, 5 });
  ASSERT_NE(survey.get(), nullptr);

  const SurveyOverviewModel model = BuildSurveyOverviewModel(*survey);
  ASSERT_TRUE(model.valid);
  ASSERT_EQ(model.arm, SurveyArm::kFullDimensional);
  EXPECT_EQ(model.evidence_state, LUMICE_PATH_STATE_FINITE);

  // No edge card came through, and the conclusions never invent one.
  bool saw_edge_card = false;
  for (const SurveyObjectRow& row : model.objects) {
    if (row.kind_name == "caustic edge") {
      saw_edge_card = true;
    }
  }
  EXPECT_FALSE(saw_edge_card);
  EXPECT_FALSE(HasLine(model.conclusions, "Sharp edge"));
  EXPECT_FALSE(model.dispersion.certificate_available);
  EXPECT_TRUE(HasLine(model.conclusions, "Support:"));
  // The probe reading is a reading: quoted as finite-resolution, not as the answer.
  EXPECT_TRUE(HasLine(model.conclusions, "Brightness peaks near"));

  // The dispersion: no minimum-deviation fold — the followed onsets are the positional facts.
  // Their direction is the engine's per-row fact (this open-arc class moves blue inside red —
  // the opposite of C01's two-face minimum), so the assertion is the label agreeing with the
  // values it labels, not a specific side.
  ASSERT_TRUE(model.dispersion.available);
  EXPECT_FALSE(model.dispersion.min_row_available);
  ASSERT_GE(model.dispersion.followed.size(), 1u);
  for (const SurveyFollowedOnsetView& row : model.dispersion.followed) {
    if (row.value_red_deg < row.value_blue_deg) {
      EXPECT_EQ(row.direction, std::string("red_inner_blue_outer"));
    } else if (row.value_blue_deg < row.value_red_deg) {
      EXPECT_EQ(row.direction, std::string("blue_inner_red_outer"));
    }
    // else: a degenerate onset (both lambdas at one corner — this class's TIR corner sits at
    // 120.000° for both) carries either name and the equal values speak for themselves.
  }
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

}  // namespace lumice::gui
