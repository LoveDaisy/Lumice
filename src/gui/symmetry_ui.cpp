#include "gui/symmetry_ui.hpp"

#include <cmath>
#include <cstdio>

#include "IconsFontAwesome6.h"
#include "gui/file_io.hpp"
#include "gui/theme.hpp"
#include "imgui.h"
#include "lumice.h"

namespace lumice::gui {

namespace {

constexpr const char* kDAxisTooltipText =
    "D applies when azimuth = uniform 360\xc2\xb0 and roll mean is a multiple of 30\xc2\xb0.\n"
    "Current config does not meet this condition, so D has no effect.";
constexpr const char* kDShapeTooltipText =
    "This crystal's shape (face distances) is not mirror-symmetric about the plane its\n"
    "roll selects, so D has no effect.";
constexpr const char* kBTooltipText =
    "The upper and lower pyramid parts differ (height or wedge angle),\n"
    "so B has no effect on this crystal.";

// Transparent SmallButton as a hover target for a tooltip (TextDisabled lacks a stable item ID
// needed by the test engine). `icon_id` is the ImGui id of the button.
void InfoIcon(const char* icon_id, const char* tooltip) {
  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
  ImGui::SmallButton(icon_id);
  ImGui::PopStyleVar();
  ImGui::PopStyleColor(4);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tooltip);
  }
}

}  // namespace

bool IsDApplicableGuiAxis(const AxisDist& az, const AxisDist& roll) {
  return LUMICE_IsDApplicable(AxisDistTypeToWire(az.type), az.std, roll.mean) != 0;
}

SymmetryAvailability SymmetryAvailabilityFor(const CrystalConfig& cr) {
  LUMICE_CrystalParam param{};
  FillCrystalParam(cr, &param);
  LUMICE_CrystalSymmetry sym{};
  SymmetryAvailability out;
  out.d_axis = IsDApplicableGuiAxis(cr.azimuth, cr.roll);
  if (LUMICE_GetCrystalSymmetry(&param, &sym) != LUMICE_OK) {
    out.rotation_step = 6;
    out.b = false;
    out.d = false;
    return out;
  }
  out.rotation_step = sym.rotation_step;
  out.b = sym.horizontal_mirror != 0;
  out.d = sym.d_effective != 0;
  return out;
}

void RenderSymmetryCheckboxes(bool& sym_p, bool& sym_b, bool& sym_d, const SymmetryAvailability& avail,
                              const char* id_suffix) {
  // P/B/D Checkboxes: unconditional under H5 (raypath / EE / AND mix all consume
  // crystal symmetry at the core layer). The pre-H5 `sym_active` gate was already
  // effectively constant.
  char id_buf[64];
  std::snprintf(id_buf, sizeof(id_buf), "P##%s", id_suffix);
  Checkbox(id_buf, &sym_p);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Prism-face reflection symmetry");
  }
  if (avail.rotation_step != 1) {
    char tip[192];
    if (avail.rotation_step >= 6) {
      std::snprintf(tip, sizeof(tip),
                    "This crystal's shape (face distances) has no rotational symmetry,\n"
                    "so P has no effect.");
    } else {
      std::snprintf(tip, sizeof(tip),
                    "This crystal's shape (face distances) repeats only every %d\xc2\xb0,\n"
                    "so P merges only paths rotated by multiples of that.",
                    avail.rotation_step * 60);
    }
    std::snprintf(id_buf, sizeof(id_buf), ICON_FA_CIRCLE_INFO "##p_tooltip_icon_%s", id_suffix);
    InfoIcon(id_buf, tip);
  }
  ImGui::SameLine();
  std::snprintf(id_buf, sizeof(id_buf), "B##%s", id_suffix);
  Checkbox(id_buf, &sym_b);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Basal-face reflection symmetry");
  }
  if (!avail.b) {
    std::snprintf(id_buf, sizeof(id_buf), ICON_FA_CIRCLE_INFO "##b_tooltip_icon_%s", id_suffix);
    InfoIcon(id_buf, kBTooltipText);
  }
  ImGui::SameLine();
  std::snprintf(id_buf, sizeof(id_buf), "D##%s", id_suffix);
  Checkbox(id_buf, &sym_d);
  if (!avail.d) {
    std::snprintf(id_buf, sizeof(id_buf), ICON_FA_CIRCLE_INFO "##d_tooltip_icon_%s", id_suffix);
    InfoIcon(id_buf, avail.d_axis ? kDShapeTooltipText : kDAxisTooltipText);
  }
}

}  // namespace lumice::gui
