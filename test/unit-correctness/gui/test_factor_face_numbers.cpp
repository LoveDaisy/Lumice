// FactorFaceNumbers: the faces a filter row names, as the Edit Entry modal hands them to
// LUMICE_CouldCrystalHaveFace to warn about a face the crystal never has. Built on the same
// expansion the commit path uses, so a face the row commits is a face the warning looks at.

#include <gtest/gtest.h>

#include <vector>

#include "gui/raypath_segments.hpp"

namespace lumice::gui {
namespace {

std::vector<int> FacesOfRow(const char* text) {
  std::vector<int> all;
  for (const auto& factor : ParseSummandText(text)) {
    const auto faces = FactorFaceNumbers(factor);
    all.insert(all.end(), faces.begin(), faces.end());
  }
  return all;
}

TEST(FactorFaceNumbers, RaypathAlternativesAndEntryExitListsAllCount) {
  EXPECT_EQ(FacesOfRow("3-5"), (std::vector<int>{ 3, 5 }));
  EXPECT_EQ(FacesOfRow("5-3-5"), (std::vector<int>{ 3, 5 }));
  EXPECT_EQ(FacesOfRow("1-3;4-7"), (std::vector<int>{ 1, 3, 4, 7 }));
  EXPECT_EQ(FacesOfRow("entry:2 & exit:4,6"), (std::vector<int>{ 2, 4, 6 }));
}

TEST(FactorFaceNumbers, LengthBoundsAndWildcardsNameNoFace) {
  EXPECT_EQ(FacesOfRow("len:2-3"), std::vector<int>{});
  EXPECT_EQ(FacesOfRow("entry:3 & len:2-3"), (std::vector<int>{ 3 }));
}

}  // namespace
}  // namespace lumice::gui
