#ifndef LUMICE_GUI_CONFIG_SUMMARY_HPP
#define LUMICE_GUI_CONFIG_SUMMARY_HPP

// The content of the read-only "Summary" window (config_summary_window.cpp): one page of the
// current document, laid out for a screenshot someone else can read without opening the app.
//
// Two sections, two sources, and the split is the design:
//
//   SETTINGS — sun / simulation / render scalars. GENERATED, NEVER ENUMERATED, the same rule the
//   Settings panel's row set lives by (defaults_diff.hpp): the document is serialized through
//   SerializeGuiStateJson and its leaves are walked. Which ROOT keys reach the page is decided by
//   the field-tier registry (gui_state_tiers.hpp) — kStructSoft / kStructHard / kDisplay are the
//   document's scene semantics and are shown; kView (overlays, background image, panel layout,
//   log levels) and kSession (which windows are open, the analysis tool's state) are not what a
//   configuration IS, and would only put a background image's file path into a shared picture.
//   A GuiState field added under one of the shown tiers appears here with no change to this file;
//   test/unit-correctness/gui/test_config_summary_rows.cpp holds that as a permanent assertion.
//
//   DOCUMENT — layers, their entries, each entry's crystal, axis and filter. These live under the
//   one root key the walk above skips (`layers`, see kDiffEngineExcludedRootKeys: a key path into
//   it carries a document-local index), so they are read straight off GuiState's structure and
//   spelled by the formatters the panels already use — FormatCrystalIdentity for the crystal line,
//   AxisPresetName for the axis, FilterSummary for the filter, kShapeScalarLabels for the shape
//   rows — rather than by a second rendering of the same facts.
//
// Every value is what the GUI SHOWS, not what it commits to core. The preview commits a fixed
// dual-equal-area / 180° / view-(0,0,0) renderer and reprojects in the shader
// (file_io.hpp SceneIntent::kSimCommit); the page prints the lens the user chose, as the export
// arm (kJsonExport) does. test/composition-correctness/gui/test_config_summary_export_parity_chain.cpp
// pins the two together field by field. The corollary is that a field the GUI never shows (the
// sun's azimuth, fixed at 0 with no control) is not on this page either.
//
// No ImGui here: everything the window draws is built by BuildConfigSummary and can be asserted
// without a frame, which is what the three tests above do.

#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <vector>

#include "gui/gui_state.hpp"

namespace lumice::gui {

// One leaf of the settings walk: the serialized key path ("renderer.lens_type") and its raw value.
struct ConfigSummaryRow {
  std::string key_path;
  nlohmann::json value;
};

// Whether root key `key` of the serialized document belongs to the settings section. The rule, in
// order: a key in kDiffEngineExcludedRootKeys is out (that table says "not a flat leaf model");
// then the key's kFieldTierTable row decides — kStructSoft / kStructHard / kDisplay in, kView /
// kSession out. A key with NO row (the view keys the serializer renames, "aspect_ratio" for
// aspect_preset and the overlay_* family) is OUT: every shown-tier field is serialized under its
// own name today, and defaulting the unknown to "hidden" means a future rename costs a missing row
// — which the coverage test reports — rather than a view preference in a shared screenshot.
bool ConfigSummaryIncludesRootKey(std::string_view key);

// The settings section's rows for `state`, root keys in serialized order and leaves in walk order.
// Pure: serializes and walks, reads no file.
std::vector<ConfigSummaryRow> BuildConfigSummaryRows(const GuiState& state);

// A label / value pair as the window prints it.
struct ConfigSummaryField {
  std::string label;
  std::string value;
};

// One titled block of fields: a settings group ("Sun"), a layer ("Layer 1"), or an entry
// ("Layer 1 · Entry 2"). `level` is the nesting the window indents by (0 for a group or a layer,
// 1 for an entry under its layer).
struct ConfigSummaryGroup {
  std::string title;
  int level = 0;
  std::vector<ConfigSummaryField> fields;
};

struct ConfigSummary {
  // LUMICE_GetVersionString(): the build that produced this picture, first because "which
  // version" is the question a shared screenshot is most often answering.
  std::string version;
  std::vector<ConfigSummaryGroup> settings;
  std::vector<ConfigSummaryGroup> document;
};

// The whole page for `state`. Pure, and the only thing the window renders.
ConfigSummary BuildConfigSummary(const GuiState& state);

// The number of label/value lines the page carries — the figure the functional test counts
// against the rendered rows, so the count has one definition.
int CountConfigSummaryFields(const ConfigSummary& summary);

}  // namespace lumice::gui

#endif  // LUMICE_GUI_CONFIG_SUMMARY_HPP
