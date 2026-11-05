// The Analyze workspace's wheel arbitration, shared by every region that reads the wheel for
// a gesture (the map disk's zoom, the raypath 3D's orbit zoom). One rule, two claimants:
//
//  - A region CLAIMS the wheel key (SetKeyOwner, one owner id per region) for exactly the
//    frames its claiming zone is hovered, and releases only its own claim (or an empty one).
//    Per-region ids are what let two regions render in the same frame without clobbering
//    each other: the later region must not release the earlier one's standing claim, and a
//    pointer moving straight from one region to the other must not drop the gesture.
//  - A region CONSUMES the wheel only while SOME region's claim is standing at the frame's
//    start. The native wheel dispatch runs in NewFrame, before any rendering, and reads the
//    owner the previous frame left; a claim set this frame cannot stop this frame's dispatch.
//    So on the frame a region just came under a stationary pointer (the host band scrolled
//    it there), no claim was standing, the dispatch has already scrolled the ancestor with
//    that event — and consuming it again in the gestures would be the double consumption
//    twice over. One frame later the claim is standing and the region owns the wheel for
//    real.
//
// The stickiness decays on its own: ImGui clears the pending owner whenever the wheel key is
// not down, so leaving the workspace (no region renders, nothing claims or releases) never
// strands the claim.
//
// The standing read is latched per ImGui frame: SetKeyOwner writes the current owner as well
// as the pending one, so a region rendered after another region's claim could no longer read
// the frame-start state — the first caller in the frame snapshots it, the rest reuse that.

#pragma once

#include "imgui.h"
#include "imgui_internal.h"  // SetKeyOwner / ImHashStr / KeysOwnerData / ImGuiKeyOwner_*

namespace lumice::gui {

inline ImGuiID AnalyzeMapWheelOwnerId() {
  return ImHashStr("analyze-wheel-map");
}

inline ImGuiID AnalyzeRay3DWheelOwnerId() {
  return ImHashStr("analyze-wheel-ray3d");
}

// True when a claim of one of this workspace's regions was standing at the frame's start.
inline bool AnalyzeRegionWheelStanding() {
  static int latch_frame = -1;
  static bool latch_standing = false;
  const int frame = ImGui::GetFrameCount();
  if (latch_frame != frame) {
    const ImGuiKeyOwnerData& data =
        ImGui::GetCurrentContext()->KeysOwnerData[ImGuiKey_MouseWheelY - ImGuiKey_NamedKey_BEGIN];
    latch_frame = frame;
    latch_standing = data.OwnerCurr == AnalyzeMapWheelOwnerId() || data.OwnerCurr == AnalyzeRay3DWheelOwnerId();
  }
  return latch_standing;
}

// Refresh the wheel's ownership for the next frame's dispatch: claim while `hovered`, and
// release only an own-or-empty claim (another region's standing claim is left alone).
inline void AnalyzeRegionWheelClaim(bool hovered, ImGuiID my_id) {
  ImGuiKeyOwnerData& data = ImGui::GetCurrentContext()->KeysOwnerData[ImGuiKey_MouseWheelY - ImGuiKey_NamedKey_BEGIN];
  if (hovered) {
    ImGui::SetKeyOwner(ImGuiKey_MouseWheelY, my_id);
  } else if (data.OwnerNext == my_id || data.OwnerNext == ImGuiKeyOwner_NoOwner) {
    ImGui::SetKeyOwner(ImGuiKey_MouseWheelY, ImGuiKeyOwner_NoOwner);
  }
}

}  // namespace lumice::gui
