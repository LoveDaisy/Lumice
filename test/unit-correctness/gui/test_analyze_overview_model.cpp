// The class overview's model rules (742.4), on a real survey document: the routing-arm
// classification, the main conclusions' content, and the read-back fidelity of the blocks the
// region renders. This file holds the rules that are cheap to document — the identity arm, on a
// fast rank-0 scene; the expensive arms (full-dimensional, family, refused) are the composition
// tier's real-scene chain (test_analyze_overview_chain.cpp), because their documents cost real
// engine legs and the composition layer is where those costs already live.
//
// Everything asserted here is a read-back: the builder invents nothing, so a number or a named
// mode in a conclusion traces to a typed reader on the same document.

#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <string>
#include <vector>

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

// A fully random prism (the C01 shape) — `1-3-6-2` is a rank-0 path on it, so the class routes
// to the identity arm: the source-image certificate plus the weight tint, nothing else.
constexpr const char* kRandomPrismJson = R"({
  "crystal": [ { "id": 1, "type": "prism", "shape": { "height": 1.0, "face_distance": [1,1,1,1,1,1] },
                 "axis": { "zenith": { "type": "uniform", "mean": 0, "std": 360 },
                           "azimuth": { "type": "uniform", "mean": 0, "std": 360 },
                           "roll": { "type": "uniform", "mean": 0, "std": 360 } } } ],
  "filter": [],
  "scene": { "light_source": { "type": "sun", "altitude": 20.0, "spectrum": "D65" },
             "ray_num": 1000, "max_hits": 7,
             "scattering": [ { "prob": 0.0, "entries": [ { "crystal": 1, "proportion": 1 } ] } ] },
  "render": []
})";

std::unique_ptr<LUMICE_Scene, SceneDeleter> MakeScene(const char* json) {
  LUMICE_Scene* s = nullptr;
  EXPECT_EQ(LUMICE_SceneFromJson(json, &s), LUMICE_OK);
  return std::unique_ptr<LUMICE_Scene, SceneDeleter>(s);
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
  request.symmetry_bits_plus_one = 0;  // the physical P|B|D class — the default read position
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

// The identity arm: the survey document of a rank-0 class reads back as exactly the two facts
// the arm has — the certified source image at the sun, and the weight tint with its caliber —
// with the positional-chromatic absence stated, not silent.
TEST(AnalyzeOverviewModel, IdentityArmReadsBackSourceImageAndTint) {
  const auto scene = MakeScene(kRandomPrismJson);
  const auto survey = RunSurvey(scene.get(), { 1, 3, 6, 2 });
  ASSERT_NE(survey.get(), nullptr);

  const SurveyOverviewModel model = BuildSurveyOverviewModel(*survey);
  ASSERT_TRUE(model.valid);
  EXPECT_EQ(model.arm, SurveyArm::kIdentity);
  EXPECT_EQ(model.execution, LUMICE_PATH_OUTCOME_COMPLETED);
  EXPECT_TRUE(model.early_stopped);
  EXPECT_EQ(model.object_count, 1u);

  // The scope's reference wavelength, read back for the always-on line.
  ASSERT_TRUE(model.scope_available);
  EXPECT_GT(model.wavelength_nm, 0.0);
  EXPECT_FALSE(model.wavelength_source.empty());
  EXPECT_GT(model.refractive_index, 1.0);
  EXPECT_TRUE(model.sun_available);
  EXPECT_NEAR(model.sun_altitude_deg, 20.0, 1e-9);

  // The conclusions name the source image, the tint, and the positional absence.
  EXPECT_TRUE(HasLine(model.conclusions, "Source image"));
  EXPECT_TRUE(HasLine(model.conclusions, "Weight tint"));
  EXPECT_TRUE(HasLine(model.conclusions, "No positional dispersion"));

  // The tint block: a reading, a direction in the engine's words, and the caliber string the
  // user can read verbatim (AC3 — the口径 is the engine's, not a GUI paraphrase).
  ASSERT_TRUE(model.tint.available);
  EXPECT_TRUE(model.tint.reading_available);
  EXPECT_GT(model.tint.ratio, 0.0);
  EXPECT_FALSE(model.tint.direction.empty());
  EXPECT_NE(model.tint.caliber.find("650nm"), std::string::npos);

  // The identity arm runs no positional chromatic leg: the chromatic block's own state is the
  // tint leg's (the tint above IS its reading), so no unevaluated note rides on the block — the
  // absence of a dispersion/diagnose half is stated by their missing views, and the "No
  // positional dispersion" conclusion above.
  EXPECT_FALSE(model.dispersion.available);
  EXPECT_FALSE(model.diagnose.available);
  EXPECT_EQ(model.chromatic_state, LUMICE_PATH_STATE_FINITE);
  EXPECT_TRUE(model.chromatic_note.empty());

  // The one object: the source image card at the sun, certified.
  ASSERT_EQ(model.objects.size(), 1u);
  EXPECT_EQ(model.objects[0].kind_name, std::string("source image"));
  EXPECT_EQ(model.objects[0].state, LUMICE_PATH_STATE_CERTIFIED);
  EXPECT_TRUE(model.objects[0].position_available);
  EXPECT_NEAR(model.objects[0].position_altitude_deg, model.sun_altitude_deg, 1e-9);

  // Nothing else ran: no probe, no family ladder, no witness, and the TIR block does not apply
  // to a direction-invariant class.
  EXPECT_FALSE(model.probe.available);
  EXPECT_FALSE(model.family.available);
  EXPECT_FALSE(model.witness.available);
}

// The pure helpers the renderer and the tests share: the state vocabulary is the survey face's
// own names, and a length-delimited text reads back through the one converter.
TEST(AnalyzeOverviewModel, VocabularyAndTextHelpers) {
  EXPECT_STREQ(SurveyStateName(LUMICE_PATH_STATE_CERTIFIED), "certified");
  EXPECT_STREQ(SurveyStateName(LUMICE_PATH_STATE_FINITE), "finite");
  EXPECT_STREQ(SurveyStateName(LUMICE_PATH_STATE_REFUSED), "refused");
  EXPECT_STREQ(SurveyStateName(LUMICE_PATH_STATE_UNEVALUATED), "unevaluated");
  EXPECT_STREQ(SurveyOutcomeName(LUMICE_PATH_OUTCOME_COMPLETED), "completed");
  EXPECT_STREQ(SurveyOutcomeName(LUMICE_PATH_OUTCOME_CANCELLED), "cancelled");
  EXPECT_STREQ(SurveyOutcomeName(LUMICE_PATH_OUTCOME_DEADLINE_EXCEEDED), "deadline exceeded");
  EXPECT_STREQ(SurveyObjectKindName(LUMICE_PATH_SURVEY_SOURCE_IMAGE), "source image");
  EXPECT_STREQ(SurveyObjectKindName(999), "object");

  LUMICE_PathText text{};
  const char* bytes = "650nm";
  text.data = bytes;
  text.length = 5;
  EXPECT_EQ(SurveyTextString(text), "650nm");
  text.data = nullptr;
  text.length = 0;
  EXPECT_EQ(SurveyTextString(text), "");
}

}  // namespace lumice::gui
