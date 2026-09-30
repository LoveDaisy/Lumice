#include "gui/symmetry_ui.hpp"

#include <cmath>
#include <cstdio>

#include "IconsFontAwesome6.h"
#include "gui/file_io.hpp"
#include "gui/theme.hpp"
#include "imgui.h"
#include "lumice_base.h"
#include "lumice_editor.h"
#include "lumice_scene.h"

namespace lumice::gui {

namespace {

// Filter-side hints (kFilterLabel). A filter's P/B/D relabels face numbers whatever the crystal,
// so the hint's job is to say when that merges paths that are not physically alike, and how to
// select them exactly instead: untick the element and write each wanted path as its own row.
#define LUMICE_EXACT_ROUTE "\nTo select them separately, untick it and write each path as its own row."
constexpr const char* kDAxisTooltipText =
    "D applies when azimuth = uniform 360\xc2\xb0 and roll mean is a multiple of 30\xc2\xb0.\n"
    "Current config does not meet this condition, so D has no effect.";
constexpr const char* kDShapeTooltipText =
    "This crystal's shape (face distances) is not mirror-symmetric about the plane its\n"
    "roll selects, so D merges paths that are not physically equivalent." LUMICE_EXACT_ROUTE;
constexpr const char* kPAxisTooltipText =
    "Roll is not uniform 360\xc2\xb0, so a path and its 60\xc2\xb0 rotations are not equally likely\n"
    "(a Parry arc lights 3-5, not 4-6). P still merges them." LUMICE_EXACT_ROUTE;
constexpr const char* kBShapeTooltipText =
    "The upper and lower pyramid parts differ (height or wedge angle), so B merges\n"
    "paths through different cones." LUMICE_EXACT_ROUTE;
constexpr const char* kBAxisTooltipText =
    "Zenith is not symmetric about 90\xc2\xb0 or azimuth is not uniform 360\xc2\xb0, so a path and its\n"
    "B mirror (1<->2, 13..18<->23..28) are not equally likely. B still merges them." LUMICE_EXACT_ROUTE;

constexpr const char* kPFilterHover = "P: also match every 60\xc2\xb0 rotation of the path's prism-face labels";
constexpr const char* kBFilterHover = "B: also match the path with 1<->2 and 13..18<->23..28 swapped";
constexpr const char* kDFilterHover = "D: also match the path mirrored in the plane the roll mean selects";
constexpr const char* kPAnalysisHover =
    "P: merge 60\xc2\xb0 rotations, only where each crystal's shape and orientation make them\n"
    "physically equivalent (unlike a filter's P, which merges face labels)";
constexpr const char* kBAnalysisHover =
    "B: merge the 1<->2 / upper<->lower cone mirror, only where it is physically equivalent";
constexpr const char* kDAnalysisHover =
    "D: merge the vertical mirror the roll selects, only where it is physically equivalent";

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

bool IsPApplicableGuiAxis(const AxisDist& roll) {
  return LUMICE_IsPApplicable(AxisDistTypeToWire(roll.type), roll.std) != 0;
}

bool IsBApplicableGuiAxis(const AxisDist& az, const AxisDist& zenith) {
  return LUMICE_IsBApplicable(AxisDistTypeToWire(az.type), az.std, AxisDistTypeToWire(zenith.type), zenith.mean,
                              zenith.std) != 0;
}

SymmetryAvailability SymmetryAvailabilityFor(const CrystalConfig& cr) {
  LUMICE_CrystalParam param{};
  FillCrystalParam(cr, &param);
  LUMICE_CrystalSymmetry sym{};
  SymmetryAvailability out;
  out.d_axis = IsDApplicableGuiAxis(cr.azimuth, cr.roll);
  out.p_axis = IsPApplicableGuiAxis(cr.roll);
  out.b_axis = IsBApplicableGuiAxis(cr.azimuth, cr.zenith);
  if (LUMICE_GetCrystalSymmetry(&param, &sym) != LUMICE_OK) {
    out.rotation_step = 6;
    out.b = false;
    out.d = false;
    return out;
  }
  out.rotation_step = sym.rotation_step;
  out.b = sym.b_effective != 0;
  out.d = sym.d_effective != 0;
  return out;
}

void RenderSymmetryCheckboxes(bool& sym_p, bool& sym_b, bool& sym_d, const SymmetryAvailability& avail,
                              SymmetryCheckboxMeaning meaning, const char* id_suffix) {
  const bool physical = meaning == SymmetryCheckboxMeaning::kAnalysisPhysical;
  // P/B/D Checkboxes: unconditional under H5 (raypath / EE / AND mix all consume
  // crystal symmetry at the core layer). The pre-H5 `sym_active` gate was already
  // effectively constant.
  char id_buf[64];
  std::snprintf(id_buf, sizeof(id_buf), "P##%s", id_suffix);
  Checkbox(id_buf, &sym_p);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", physical ? kPAnalysisHover : kPFilterHover);
  }
  if (!physical && !avail.p_axis) {
    std::snprintf(id_buf, sizeof(id_buf), ICON_FA_CIRCLE_INFO "##p_tooltip_icon_%s", id_suffix);
    InfoIcon(id_buf, kPAxisTooltipText);
  } else if (!physical && avail.rotation_step != 1) {
    char tip[320];
    if (avail.rotation_step >= 6) {
      std::snprintf(tip, sizeof(tip),
                    "This crystal's shape (face distances) has no rotational symmetry, so P merges\n"
                    "paths through faces that differ." LUMICE_EXACT_ROUTE);
    } else {
      std::snprintf(tip, sizeof(tip),
                    "This crystal's shape (face distances) repeats only every %d\xc2\xb0, so P also merges\n"
                    "paths through faces that differ." LUMICE_EXACT_ROUTE,
                    avail.rotation_step * 60);
    }
    std::snprintf(id_buf, sizeof(id_buf), ICON_FA_CIRCLE_INFO "##p_tooltip_icon_%s", id_suffix);
    InfoIcon(id_buf, tip);
  }
  ImGui::SameLine();
  std::snprintf(id_buf, sizeof(id_buf), "B##%s", id_suffix);
  Checkbox(id_buf, &sym_b);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", physical ? kBAnalysisHover : kBFilterHover);
  }
  if (!physical && !avail.b) {
    std::snprintf(id_buf, sizeof(id_buf), ICON_FA_CIRCLE_INFO "##b_tooltip_icon_%s", id_suffix);
    InfoIcon(id_buf, avail.b_axis ? kBShapeTooltipText : kBAxisTooltipText);
  }
  ImGui::SameLine();
  std::snprintf(id_buf, sizeof(id_buf), "D##%s", id_suffix);
  Checkbox(id_buf, &sym_d);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", physical ? kDAnalysisHover : kDFilterHover);
  }
  if (!physical && !avail.d) {
    std::snprintf(id_buf, sizeof(id_buf), ICON_FA_CIRCLE_INFO "##d_tooltip_icon_%s", id_suffix);
    InfoIcon(id_buf, avail.d_axis ? kDShapeTooltipText : kDAxisTooltipText);
  }
}

}  // namespace lumice::gui
