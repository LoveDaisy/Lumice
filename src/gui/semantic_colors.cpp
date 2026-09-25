#include "gui/semantic_colors.hpp"

namespace lumice::gui {

namespace {

// Every canonical value below is one of the literals that already existed at a
// call site — the most-reused one, or the one the visual-language blueprint
// named. Nothing here is a freshly picked compromise shade: folding five
// near-duplicate ambers onto an existing amber moves a few channel percent,
// while folding them onto a new average would move every one of them.
constexpr ImVec4 kGoodText{ 0.40f, 0.80f, 0.40f, 1.0f };   // was: Ready status text
constexpr ImVec4 kGoodFill{ 0.06f, 0.24f, 0.06f, 1.0f };   // was: VALID validation cell
constexpr ImVec4 kWarnText{ 1.00f, 0.75f, 0.20f, 1.0f };   // was: 3 call sites, verbatim
constexpr ImVec4 kWarnFill{ 0.45f, 0.28f, 0.12f, 1.0f };   // was: Resolution input FrameBg
constexpr ImVec4 kDestrText{ 0.90f, 0.30f, 0.30f, 1.0f };  // was: invalid predicate border
constexpr ImVec4 kDestrFill{ 0.27f, 0.06f, 0.06f, 1.0f };  // was: INVALID validation cell

// Good-button triple. Kept beside the grade it belongs to rather than in
// destructive_style.cpp, which owns only the destructive triple.
constexpr ImVec4 kGoodBtnNormal{ 0.15f, 0.45f, 0.15f, 1.0f };
constexpr ImVec4 kGoodBtnHovered{ 0.20f, 0.55f, 0.20f, 1.0f };
constexpr ImVec4 kGoodBtnActive{ 0.10f, 0.35f, 0.10f, 1.0f };

// Continue-button triple: an action-identity colour, not a grade (semantic_colors.hpp). Chosen
// from three screenshotted candidates by hue distance from the four colours it shares the bar
// with — Run's green, Stop's red, the palette's blue button, and the accent-tinted Colors button;
// candidates and the reasons for this one are in doc/gui-visual-language.md §4.8.
constexpr ImVec4 kContinueBtnNormal{ 0.38f, 0.24f, 0.58f, 1.0f };
constexpr ImVec4 kContinueBtnHovered{ 0.46f, 0.31f, 0.68f, 1.0f };
constexpr ImVec4 kContinueBtnActive{ 0.30f, 0.18f, 0.48f, 1.0f };

ImVec4 WithAlpha(const ImVec4& c, float alpha) {
  return ImVec4(c.x, c.y, c.z, alpha);
}

}  // namespace

ImVec4 GoodTextColor() {
  return kGoodText;
}

ImVec4 GoodFillColor(float alpha) {
  return WithAlpha(kGoodFill, alpha);
}

void PushGoodButtonStyle() {
  ImGui::PushStyleColor(ImGuiCol_Button, kGoodBtnNormal);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kGoodBtnHovered);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, kGoodBtnActive);
}

void PopGoodButtonStyle() {
  ImGui::PopStyleColor(3);
}

void PushContinueButtonStyle() {
  ImGui::PushStyleColor(ImGuiCol_Button, kContinueBtnNormal);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kContinueBtnHovered);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, kContinueBtnActive);
}

void PopContinueButtonStyle() {
  ImGui::PopStyleColor(3);
}

ImVec4 WarningTextColor() {
  return kWarnText;
}

ImVec4 WarningFillColor(float alpha) {
  return WithAlpha(kWarnFill, alpha);
}

ImVec4 DestructiveTextColor() {
  return kDestrText;
}

ImVec4 DestructiveFillColor(float alpha) {
  return WithAlpha(kDestrFill, alpha);
}

}  // namespace lumice::gui
