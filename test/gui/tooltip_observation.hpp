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

inline bool ObserveTooltip(ImGuiTestContext* ctx, const ImVec2& target, const char* expected,
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
  IM_CHECK_RETV(rendered.find(expected) != std::string::npos, false);
  ctx->LogInfo("Tooltip: scale=%.2f window=%.1fx%.1f content=%.1fx%.1f text=%s", gui::CurrentUiScale(), tooltip->Size.x,
               tooltip->Size.y, tooltip->ContentSize.x, tooltip->ContentSize.y, expected);
  // Release-host premise: this host renders the bare ImGui::SetTooltip with no wrap ruler (the
  // wrapping migration of the axis label path is main-only, outside this port's authorized set),
  // so the wrap-width and line-count assertions are not ported. What stays witnessable here is
  // machine-independent: the full text is laid out inside the tooltip's own inner window —
  // InnerRect, not the raster clip, because the product clamps the window to the monitor work
  // area and an unwrapped line at larger scales is wider than any constrained display (2006px
  // at 1.5x vs this machine's 1600px work area), which clips InnerClipRect without touching the
  // layout. On-screen containment is asserted when the frame physically fits the display; when
  // it does not, no placement could show it and this witness is not claimed.
  IM_CHECK_RETV(tooltip->ContentSize.x <= tooltip->InnerRect.GetWidth() + 1.0f, false);
  IM_CHECK_RETV(tooltip->ContentSize.y <= tooltip->InnerRect.GetHeight() + 1.0f, false);
  IM_CHECK_RETV(tooltip->Pos.x >= 0.0f && tooltip->Pos.y >= 0.0f, false);
  if (tooltip->Size.x <= ImGui::GetIO().DisplaySize.x && tooltip->Size.y <= ImGui::GetIO().DisplaySize.y) {
    IM_CHECK_RETV(tooltip->Pos.x + tooltip->Size.x <= ImGui::GetIO().DisplaySize.x + 1.0f, false);
    IM_CHECK_RETV(tooltip->Pos.y + tooltip->Size.y <= ImGui::GetIO().DisplaySize.y + 1.0f, false);
  }
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
