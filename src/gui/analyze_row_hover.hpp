#ifndef LUMICE_GUI_ANALYZE_ROW_HOVER_HPP
#define LUMICE_GUI_ANALYZE_ROW_HOVER_HPP

// The Analyze workspace's row-hover readout contract in one ImGui-free header. The overview
// renderer's interactive host produces these readouts (RenderSurveyOverviewModel's out-param);
// the map's profile device consumes them (ProfileHoverInput.hover) and GuiState holds the
// session copy. Neither view header is the right home: analyze_overview.hpp is the ImGui half
// (its closure carries imgui.h) and analyze_map_view.hpp deliberately stays off imgui.h because
// GuiState's closure must serve imgui-free consumers (the unit-correctness target compiles
// "core has no ImGui"). The types therefore get their own header, and the two view headers
// include it as peers.

namespace lumice::gui {

// Which reference-profile reading row the pointer stood on (the class answer's three profile
// rows): the session identity the map's profile device consumes, one frame behind the row write.
// Verdict, Position and ratio rows keep their highlight feedback local, outside this identity.
enum class SurveyProfileHover { kNone, kEdge, kPeak, kSeparation };

// The interactive class-answer host writes the profile rows' exclusive standing each frame;
// the canvas consumes the session copy the NEXT frame. Other row highlights remain local.
struct SurveyRowHoverReadout {
  SurveyProfileHover profile = SurveyProfileHover::kNone;
};

}  // namespace lumice::gui

#endif  // LUMICE_GUI_ANALYZE_ROW_HOVER_HPP
