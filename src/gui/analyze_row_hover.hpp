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
// rows): the session identity the map's profile device consumes, one frame behind the row write
// (the same lag the verdict anchor's hover owns). The ratio row keeps its feedback local — it
// names no spatial payload, so it never enters this identity.
enum class SurveyProfileHover { kNone, kEdge, kPeak, kSeparation };

// The interactive class-answer host's per-row hover read (§16.1's A2 pilot family): the renderer
// writes every row's standing each frame it draws them, the canvas consumes the session copy the
// NEXT frame. `verdict` is the two linked diagnose rows' OR (the verdict anchor's read);
// `profile` is the three profile reading rows' exclusive standing (the profile device's read).
struct SurveyRowHoverReadout {
  bool verdict = false;
  SurveyProfileHover profile = SurveyProfileHover::kNone;
};

}  // namespace lumice::gui

#endif  // LUMICE_GUI_ANALYZE_ROW_HOVER_HPP
