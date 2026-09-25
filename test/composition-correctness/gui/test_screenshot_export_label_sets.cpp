// The Screenshot export's labels: core's anchors -> the per-family builders -> the export options.
//
// The removed "Include Overlay in Screenshot" gated the text and let the lines through; the export
// options gate them separately, and this is the text half (the line half is
// test/unit-correctness/gui/test_screenshot_export_options.cpp). Three propositions, over one real
// anchor computation so every family has anchors to lose:
//   - the prefilled selection builds exactly the sets the preview's own label block builds, in the
//     same order — the "all ticked exports what it did before" claim for text;
//   - clearing one family's label bit removes that family's set and leaves the others as they were;
//   - the reference points are one family: clearing the bit removes every name, keeping it keeps
//     each point's own label switch.
// Also: a set label bit cannot add a family whose label the panel has off.
//
// Families are identified by colour and collision group, not by position in the vector: the horizon
// shares the grid's group, and a count alone would pass a vector that lost one family and kept a
// duplicate of another.

#include <gtest/gtest.h>

#include <cstddef>
#include <vector>

#include "gui/annotation_anchors.hpp"
#include "gui/app.hpp"
#include "gui/gui_constants.hpp"
#include "gui/gui_state.hpp"
#include "gui/overlay_labels.hpp"
#include "gui/screenshot_export_options.hpp"
#include "lumice.h"

namespace gui = lumice::gui;

namespace {

constexpr int kCanvasW = 512;
constexpr int kCanvasH = 512;
constexpr float kW = static_cast<float>(kCanvasW);
constexpr float kH = static_cast<float>(kCanvasH);

// Dual fisheye equal-area at full sky images every family and all six reference points, so no
// builder returns early for want of anchors.
gui::AnnotationAnchors ComputeFullSkyView() {
  gui::AnnotationViewInput vin;
  vin.lens_type = gui::kLensTypeDualFisheyeEqualArea;
  vin.fov = 180.0f;
  vin.visible = gui::kVisibleFull;
  vin.overlap = gui::kDualFisheyeOverlap;
  vin.sun_altitude_deg = 20.0f;
  vin.angular_dist_deg = { 22.0f, 46.0f };
  vin.view_dist_deg = { 45.0f };
  vin.elevation_deg = gui::ComputeGridElevationAngles(30.0f);
  vin.longitude_deg = gui::ComputeGridLongitudeAngles(30.0f);
  vin.horizon = true;
  for (int i = 0; i < LUMICE_ANNOTATION_MARKER_COUNT; ++i) {
    vin.marker_ids.push_back(i);
  }
  gui::AnnotationAnchors cache;
  cache.Compute(gui::MakeAnnotationViewKey(vin, kCanvasW, kCanvasH));
  return cache;
}

gui::GuiState AllLabelsOn() {
  gui::GuiState state;
  state.show_horizon_label = true;
  state.show_grid_label = true;
  state.show_sun_circles_label = true;
  state.show_view_dist_label = true;
  for (gui::MarkerAppearance& m : state.markers) {
    m.label = true;
  }
  return state;
}

// What the preview's label block (app_panels.cpp) builds for `state`, spelled out independently of
// the function under test: the four family builders in their order, then the markers.
std::vector<gui::CurveLabelSet> PreviewLabelSets(const gui::AnnotationAnchors& cache, const gui::GuiState& state) {
  std::vector<gui::CurveLabelSet> sets;
  if (state.show_horizon_label) {
    sets.push_back(gui::BuildHorizonLabelSet(cache, state, kW, kH));
  }
  if (state.show_sun_circles_label) {
    sets.push_back(gui::BuildSunCirclesLabelSet(cache, state, kW, kH));
  }
  if (state.show_view_dist_label) {
    sets.push_back(gui::BuildViewDistLabelSet(cache, state, kW, kH));
  }
  if (state.show_grid_label) {
    sets.push_back(gui::BuildGridLabelSet(cache, state, kW, kH));
  }
  for (gui::CurveLabelSet& set : gui::BuildMarkerLabelSets(cache, state, kW, kH)) {
    sets.push_back(std::move(set));
  }
  return sets;
}

bool SameSet(const gui::CurveLabelSet& a, const gui::CurveLabelSet& b) {
  if (a.anchors.size() != b.anchors.size() || a.group != b.group || a.alpha != b.alpha || a.has_bg != b.has_bg) {
    return false;
  }
  for (int c = 0; c < 3; ++c) {
    if (a.color[c] != b.color[c]) {
      return false;
    }
  }
  for (std::size_t i = 0; i < a.anchors.size(); ++i) {
    const gui::CurveLabelAnchor& p = a.anchors[i];
    const gui::CurveLabelAnchor& q = b.anchors[i];
    if (p.px != q.px || p.py != q.py || p.text != q.text) {
      return false;
    }
  }
  return true;
}

void ExpectSameSets(const std::vector<gui::CurveLabelSet>& got, const std::vector<gui::CurveLabelSet>& want) {
  if (got.size() != want.size()) {
    ADD_FAILURE() << "got " << got.size() << " sets, want " << want.size();
    return;
  }
  for (std::size_t i = 0; i < got.size(); ++i) {
    EXPECT_TRUE(SameSet(got[i], want[i])) << "set " << i;
  }
}

// Does `sets` hold a set with this family's colour and group?
bool HasFamily(const std::vector<gui::CurveLabelSet>& sets, const float* color, int group) {
  for (const gui::CurveLabelSet& s : sets) {
    if (s.group == group && s.color[0] == color[0] && s.color[1] == color[1] && s.color[2] == color[2]) {
      return true;
    }
  }
  return false;
}

std::size_t CountGroup(const std::vector<gui::CurveLabelSet>& sets, int group) {
  std::size_t n = 0;
  for (const gui::CurveLabelSet& s : sets) {
    n += s.group == group ? 1 : 0;
  }
  return n;
}

}  // namespace

TEST(ScreenshotExportLabelSets, ThePrefilledSelectionBuildsThePreviewsSets) {
  const gui::AnnotationAnchors cache = ComputeFullSkyView();
  ASSERT_TRUE(cache.HasResult());

  gui::GuiState all_on = AllLabelsOn();
  gui::GuiState mixed = AllLabelsOn();
  mixed.show_grid_label = false;
  mixed.markers[2].label = false;
  for (const gui::GuiState* state : { &all_on, &mixed }) {
    const std::vector<gui::CurveLabelSet> want = PreviewLabelSets(cache, *state);
    if (want.size() < 5u) {
      ADD_FAILURE() << "premise: several families have anchors to lose, got " << want.size() << " sets";
      continue;
    }
    ExpectSameSets(
        gui::BuildScreenshotExportLabelSets(cache, *state, gui::MakeScreenshotExportSelectionFromState(*state), kW, kH),
        want);
  }
}

// One family's label bit at a time: its set goes, every other set stays exactly as the preview has it.
TEST(ScreenshotExportLabelSets, ClearingALabelBitDropsOnlyThatFamilysText) {
  const gui::AnnotationAnchors cache = ComputeFullSkyView();
  ASSERT_TRUE(cache.HasResult());
  const gui::GuiState state = AllLabelsOn();

  struct Case {
    const char* name;
    bool gui::ScreenshotExportSelection::*bit;
    bool gui::GuiState::*panel_switch;
    const float* color;
    int group;
  };
  const Case cases[] = {
    { "horizon", &gui::ScreenshotExportSelection::horizon_label, &gui::GuiState::show_horizon_label,
      state.horizon_color, gui::kGroupGrid },
    { "grid", &gui::ScreenshotExportSelection::grid_label, &gui::GuiState::show_grid_label, state.grid_color,
      gui::kGroupGrid },
    { "sun circles", &gui::ScreenshotExportSelection::sun_circles_label, &gui::GuiState::show_sun_circles_label,
      state.sun_circles_color, gui::kGroupSunCircles },
    { "view circles", &gui::ScreenshotExportSelection::view_dist_label, &gui::GuiState::show_view_dist_label,
      state.view_dist_color, gui::kGroupViewCircles },
  };
  for (const Case& c : cases) {
    SCOPED_TRACE(c.name);
    gui::ScreenshotExportSelection sel = gui::MakeScreenshotExportSelectionFromState(state);
    sel.*c.bit = false;
    const std::vector<gui::CurveLabelSet> got = gui::BuildScreenshotExportLabelSets(cache, state, sel, kW, kH);

    // The oracle: the preview's sets with that family's panel switch off.
    gui::GuiState without = state;
    without.*c.panel_switch = false;
    ExpectSameSets(got, PreviewLabelSets(cache, without));
    EXPECT_FALSE(HasFamily(got, c.color, c.group)) << "the family's text is still in the export";
  }
}

// A label bit set for a family the panel has off adds nothing.
TEST(ScreenshotExportLabelSets, ASetBitCannotAddTextThePanelHasOff) {
  const gui::AnnotationAnchors cache = ComputeFullSkyView();
  ASSERT_TRUE(cache.HasResult());
  gui::GuiState state;
  state.show_horizon_label = false;
  state.show_grid_label = false;
  state.show_sun_circles_label = false;
  state.show_view_dist_label = false;
  for (gui::MarkerAppearance& m : state.markers) {
    m.label = false;
  }
  EXPECT_TRUE(gui::BuildScreenshotExportLabelSets(cache, state, gui::ScreenshotExportSelection{}, kW, kH).empty());
}

// The reference points' names go as one family, and keeping the family keeps each point's switch.
TEST(ScreenshotExportLabelSets, ReferencePointNamesGoAsOneFamilyAndKeepTheirOwnSwitches) {
  const gui::AnnotationAnchors cache = ComputeFullSkyView();
  ASSERT_TRUE(cache.HasResult());
  gui::GuiState state = AllLabelsOn();
  state.markers[1].label = false;
  state.markers[4].label = false;

  gui::ScreenshotExportSelection keep = gui::MakeScreenshotExportSelectionFromState(state);
  const std::vector<gui::CurveLabelSet> kept = gui::BuildScreenshotExportLabelSets(cache, state, keep, kW, kH);
  EXPECT_EQ(CountGroup(kept, gui::kGroupMarkers), static_cast<std::size_t>(LUMICE_ANNOTATION_MARKER_COUNT - 2))
      << "each point's own label switch was not kept";

  gui::ScreenshotExportSelection drop = keep;
  drop.markers_label = false;
  const std::vector<gui::CurveLabelSet> dropped = gui::BuildScreenshotExportLabelSets(cache, state, drop, kW, kH);
  EXPECT_EQ(CountGroup(dropped, gui::kGroupMarkers), 0u);
  EXPECT_EQ(dropped.size(), kept.size() - (LUMICE_ANNOTATION_MARKER_COUNT - 2)) << "another family went with them";
}
