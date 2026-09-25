// The summary line under the edit modal's crystal preview (FormatCrystalPreviewSummary), and the
// height-scalar selection it shares with the Summary window's Shape table
// (HeightScalarFieldsForCrystal).
//
// The expected strings are assembled from the same formatters the line is specified to reuse
// (AxisPresetLabel, FormatAxisDistCell, CrystalTypeName, kShapeScalarLabels, FormatShapeDistCell)
// rather than written out as literal numbers: the claim under test is "this line spells every fact
// the way the Summary page does", and a second hand-written copy of "%.3g" would be a second
// notation to keep in step — the thing the line exists not to have.
//
// One unit's pure logic, no frame and no input event, hence gui_unit_test and not gui_test.

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "gui/axis_presets.hpp"
#include "gui/config_summary.hpp"
#include "gui/edit_modals.hpp"
#include "gui/gui_state.hpp"
#include "gui/shape_scalar_domain.hpp"
#include "include/lumice.h"

namespace gui = lumice::gui;

namespace {

const gui::AxisPresetEntry& PresetRow(gui::AxisPreset id) {
  return gui::kAxisPresets[static_cast<size_t>(id)];
}

std::string ShapePart(const gui::ShapeDist& dist, int slot) {
  return std::string(" · ") + gui::kShapeScalarLabels[slot] + " " + gui::FormatShapeDistCell(dist, slot);
}

TEST(CrystalPreviewSummary, PrismColumnLineIsPresetZenithTypeAndHeight) {
  const auto& row = PresetRow(gui::AxisPreset::kColumn);
  const gui::AxisDist axis[3] = { row.zenith, row.azimuth, row.roll };
  gui::CrystalConfig crystal;
  crystal.type = gui::CrystalType::kPrism;
  crystal.height = { gui::ShapeDistType::kNoRandom, 1.5f, 0.0f };
  // A stale axis copy inside the crystal buffer must not leak into the line: the modal's axes live
  // in their own buffer, and `axis` is the authoritative argument.
  crystal.zenith = { gui::AxisDistType::kUniform, 10.0f, 5.0f };

  const std::string expected = std::string(gui::AxisPresetLabel(gui::AxisPreset::kColumn)) + " · zenith " +
                               gui::FormatAxisDistCell(axis[0]) + " · Prism" +
                               ShapePart(crystal.height, LUMICE_SHAPE_SCALAR_HEIGHT);
  EXPECT_EQ(gui::FormatCrystalPreviewSummary(crystal, axis), expected);
}

TEST(CrystalPreviewSummary, PyramidCustomLineListsItsThreeHeightsInModalOrder) {
  // A zenith at 45° matches no preset.
  const gui::AxisDist axis[3] = { { gui::AxisDistType::kGauss, 45.0f, 3.0f },
                                  { gui::AxisDistType::kUniform, 0.0f, 360.0f },
                                  { gui::AxisDistType::kUniform, 0.0f, 360.0f } };
  ASSERT_EQ(gui::ClassifyAxisPreset(axis[0], axis[1], axis[2]), gui::AxisPreset::kCustom);
  gui::CrystalConfig crystal;
  crystal.type = gui::CrystalType::kPyramid;
  crystal.prism_h = { gui::ShapeDistType::kNoRandom, 1.0f, 0.0f };
  crystal.upper_h = { gui::ShapeDistType::kUniform, 0.4f, 0.1f };
  crystal.lower_h = { gui::ShapeDistType::kNoRandom, 0.2f, 0.0f };

  const std::string expected = std::string("Custom · zenith ") + gui::FormatAxisDistCell(axis[0]) + " · Pyramid" +
                               ShapePart(crystal.prism_h, LUMICE_SHAPE_SCALAR_PRISM_H) +
                               ShapePart(crystal.upper_h, LUMICE_SHAPE_SCALAR_UPPER_H) +
                               ShapePart(crystal.lower_h, LUMICE_SHAPE_SCALAR_LOWER_H);
  EXPECT_EQ(gui::FormatCrystalPreviewSummary(crystal, axis), expected);
}

TEST(CrystalPreviewSummary, RandomPresetZenithTakesTheFullCircleShortForm) {
  const auto& row = PresetRow(gui::AxisPreset::kRandom);
  const gui::AxisDist axis[3] = { row.zenith, row.azimuth, row.roll };
  ASSERT_EQ(gui::ClassifyAxisPreset(axis[0], axis[1], axis[2]), gui::AxisPreset::kRandom);
  gui::CrystalConfig crystal;
  crystal.type = gui::CrystalType::kPrism;

  const std::string line = gui::FormatCrystalPreviewSummary(crystal, axis);
  EXPECT_EQ(line.rfind(std::string("Random · zenith ") + gui::FormatAxisDistCell(axis[0]) + " · Prism", 0), 0u) << line;
}

TEST(CrystalPreviewSummary, SyncGroupSuffixIsKeptVerbatim) {
  const auto& row = PresetRow(gui::AxisPreset::kPlate);
  const gui::AxisDist axis[3] = { row.zenith, row.azimuth, row.roll };
  gui::CrystalConfig crystal;
  crystal.type = gui::CrystalType::kPrism;
  crystal.height = { gui::ShapeDistType::kGauss, 0.3f, 0.05f };
  crystal.height.sync_group = 2;

  const std::string line = gui::FormatCrystalPreviewSummary(crystal, axis);
  const std::string height_cell = gui::FormatShapeDistCell(crystal.height, LUMICE_SHAPE_SCALAR_HEIGHT);
  ASSERT_NE(height_cell.find(" · sync 2"), std::string::npos) << height_cell;
  EXPECT_NE(line.find(height_cell), std::string::npos) << line;
}

TEST(HeightScalarFieldsForCrystal, PrismHasHeightOnly) {
  gui::CrystalConfig crystal;
  crystal.type = gui::CrystalType::kPrism;
  const std::vector<gui::HeightScalarField> fields = gui::HeightScalarFieldsForCrystal(crystal);
  ASSERT_EQ(fields.size(), 1u);
  EXPECT_EQ(fields[0].slot, LUMICE_SHAPE_SCALAR_HEIGHT);
  EXPECT_EQ(fields[0].dist, &crystal.height);
}

TEST(HeightScalarFieldsForCrystal, PyramidHasPrismUpperLowerInThatOrder) {
  gui::CrystalConfig crystal;
  crystal.type = gui::CrystalType::kPyramid;
  const std::vector<gui::HeightScalarField> fields = gui::HeightScalarFieldsForCrystal(crystal);
  ASSERT_EQ(fields.size(), 3u);
  EXPECT_EQ(fields[0].slot, LUMICE_SHAPE_SCALAR_PRISM_H);
  EXPECT_EQ(fields[0].dist, &crystal.prism_h);
  EXPECT_EQ(fields[1].slot, LUMICE_SHAPE_SCALAR_UPPER_H);
  EXPECT_EQ(fields[1].dist, &crystal.upper_h);
  EXPECT_EQ(fields[2].slot, LUMICE_SHAPE_SCALAR_LOWER_H);
  EXPECT_EQ(fields[2].dist, &crystal.lower_h);
}

}  // namespace
