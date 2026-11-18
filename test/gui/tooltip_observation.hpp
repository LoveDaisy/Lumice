#pragma once

#include <string>

#include "gui/app.hpp"
#include "gui/theme.hpp"
#include "imgui_internal.h"
#include "test_gui_shared.hpp"
#include "test_screenshot.hpp"

// Shared observation only: the design ruler stays independent of the production tooltip helper.
inline ImGuiWindow* ActiveTooltipWindow() {
  for (ImGuiWindow* window : GImGui->Windows) {
    if ((window->Flags & ImGuiWindowFlags_Tooltip) && window->Active && !window->Hidden &&
        window->LastFrameActive == GImGui->FrameCount) {
      return window;
    }
  }
  return nullptr;
}

inline bool ObserveTooltip(ImGuiTestContext* ctx, const ImVec2& target, const char* expected, bool multiline = true,
                           const char* capture_name = nullptr) {
  ctx->MouseMoveToPos(ImVec2(1.0f, 1.0f));
  ctx->Yield(3);
  IM_CHECK_RETV(ActiveTooltipWindow() == nullptr, false);
  ctx->MouseMoveToPos(target);
  ctx->Yield(3);
  ImGui::LogToBuffer(0);
  // Between-frame logging otherwise finishes at End() of the implicit fallback window.
  GImGui->LogWindow = nullptr;
  ctx->Yield();
  const std::string rendered = GImGui->LogBuffer.c_str();
  ImGui::LogFinish();
  ImGuiWindow* tooltip = ActiveTooltipWindow();
  IM_CHECK_RETV(tooltip != nullptr, false);
  if (rendered.find(expected) == std::string::npos) {
    ctx->LogInfo("Expected tooltip text missing; frame text: %s", rendered.c_str());
  }
  IM_CHECK_RETV(rendered.find(expected) != std::string::npos, false);
  ctx->LogInfo("Tooltip: scale=%.2f window=%.1fx%.1f content=%.1fx%.1f text=%s", gui::CurrentUiScale(), tooltip->Size.x,
               tooltip->Size.y, tooltip->ContentSize.x, tooltip->ContentSize.y, expected);
  const float text_limit = gui::UiPx(400.0f);
  IM_CHECK_RETV(tooltip->Size.x <= text_limit + 2.0f * tooltip->WindowPadding.x + 1.0f, false);
  IM_CHECK_RETV(tooltip->ContentSize.x <= text_limit + 1.0f, false);
  if (multiline) {
    IM_CHECK_RETV(tooltip->ContentSize.y >= 2.0f * ImGui::GetFontSize(), false);
  } else {
    IM_CHECK_RETV(tooltip->Size.x < text_limit * 0.75f, false);
  }
  // Compare the layout footprint to the inner window, not the border-inset raster clip:
  // a zero-padding footer inherits zero tooltip padding, while glyph ink is shorter than its advance.
  IM_CHECK_RETV(tooltip->ContentSize.y <= tooltip->InnerRect.GetHeight() + 1.0f, false);
  IM_CHECK_RETV(tooltip->ScrollMax.y == 0.0f, false);
  IM_CHECK_RETV(tooltip->Pos.x >= 0.0f && tooltip->Pos.y >= 0.0f, false);
  IM_CHECK_RETV(tooltip->Pos.x + tooltip->Size.x <= ImGui::GetIO().DisplaySize.x + 1.0f, false);
  IM_CHECK_RETV(tooltip->Pos.y + tooltip->Size.y <= ImGui::GetIO().DisplaySize.y + 1.0f, false);
  if (capture_name != nullptr && g_keep_export_png) {
    g_fullframe_capture.Reset();
    g_fullframe_capture.requested.store(true);
    for (int i = 0; i < 10 && !g_fullframe_capture.done.load(); ++i) {
      ctx->Yield();
    }
    IM_CHECK_RETV(g_fullframe_capture.done.load(), false);
    const auto rgb = lumice::test::StripAlpha(g_fullframe_capture.pixels.data(), g_fullframe_capture.width,
                                              g_fullframe_capture.height);
    const auto path = GuiTestTempPath(capture_name).string();
    IM_CHECK_RETV(
        lumice::test::SavePng(path.c_str(), rgb.data(), g_fullframe_capture.width, g_fullframe_capture.height, 3),
        false);
  }
  return true;
}

class ScopedTooltipUiScale {
 public:
  explicit ScopedTooltipUiScale(ImGuiTestContext* ctx)
      : ctx_(ctx), previous_(gui::g_ui_scale_multiplier), basis_(ctx) {}
  ~ScopedTooltipUiScale() {
    gui::SetUiScaleMultiplierImmediate(previous_);
    ctx_->Yield(4);
  }

 private:
  ImGuiTestContext* ctx_;
  float previous_;
  ScopedWindowBasis basis_;
};
