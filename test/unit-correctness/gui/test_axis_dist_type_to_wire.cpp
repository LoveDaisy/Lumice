// AxisDistTypeToWire is the single table FillAxisDist (file_io.cpp, GUI -> LUMICE_Scene export)
// and IsDApplicableGuiAxis (symmetry_ui.cpp, via IsDApplicableGuiAxis's callers) both key off, but
// test_c_api.cpp's IsDApplicableApi.* cases drive LUMICE_IsDApplicable with LUMICE_DIST_* literals
// directly and never call this function — so nothing in the tree pins its GUI-order ->
// wire-order mapping against the literal wire constants it is supposed to produce. This does,
// one EXPECT_EQ per AxisDistType, each against the literal LUMICE_DIST_* macro rather than a
// second lookup table (a table-vs-table comparison would just re-assert the switch against
// itself).

#include <gtest/gtest.h>

#include "gui/gui_state.hpp"
#include "include/lumice.h"

namespace {

using lumice::gui::AxisDistType;
using lumice::gui::AxisDistTypeToWire;

TEST(AxisDistTypeToWireTest, EveryValueMapsToItsLiteralWireConstant) {
  EXPECT_EQ(AxisDistTypeToWire(AxisDistType::kGauss), LUMICE_DIST_GAUSS);
  EXPECT_EQ(AxisDistTypeToWire(AxisDistType::kUniform), LUMICE_DIST_UNIFORM);
  EXPECT_EQ(AxisDistTypeToWire(AxisDistType::kZigzag), LUMICE_DIST_ZIGZAG);
  EXPECT_EQ(AxisDistTypeToWire(AxisDistType::kLaplacian), LUMICE_DIST_LAPLACIAN);
  EXPECT_EQ(AxisDistTypeToWire(AxisDistType::kGaussLegacy), LUMICE_DIST_GAUSS_LEGACY);
}

}  // namespace
