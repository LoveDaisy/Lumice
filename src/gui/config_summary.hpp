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
//   Three more things about each leaf are read off the field editor registry
//   (field_editor_registry.hpp), the table that already owns them for the main panel, so the
//   page prints what the user PERCEIVES on the panel rather than what the serializer spells:
//     * its LABEL is the panel's own word (LabelFor: "Rays(M)", "EV Anchor", "Sky Color"), the
//       same object the widget call site builds its id from; a leaf with no declared label is
//       spelled from its key, as every leaf was before labels had a home;
//     * whether it APPLIES right now (Constraint(state).enabled — the panel's BeginDisabled
//       expression): a field the panel greys out or does not draw is not on the page either, so
//       under Print the row is Paper Color and not Sky Color, under a full-sky lens there is no
//       Roll row, and with Infinite rays on there is no Rays(M) row. No tone or lens comparison is
//       made here; the registry's gate is the one that is read;
//     * whether it has a MAIN-PANEL control at all (has_main_panel_surface, on the leaf's entry or
//       on its root key's tier row): a field only the Settings popup can edit is listed under a
//       heading of its own, "Settings", after the three panel groups, so a reader looking for it
//       on the Sun / Simulation / View / Display panels is told where it actually lives.
//
//   DOCUMENT — layers, their entries, each entry's crystal, axis and filter. These live under the
//   one root key the walk above skips (`layers`, see kDiffEngineExcludedRootKeys: a key path into
//   it carries a document-local index), so they are read straight off GuiState's structure. The
//   entries of a layer are RECORDS OF ONE SHAPE — the same crystal / axis / filter facts for each
//   — so they are laid out as two tables per layer, headers printed once and one entry per row,
//   cross-referenced by the entry's number. The split is the edit modal's own: Crystals is the
//   card and the Axis and Filter tabs (identity, enabled, weight, the three axis distributions,
//   the filter — the columns that carry words), Shape is the Crystal tab (the type's own scalars,
//   then the six faces — thirteen columns of short numbers). A label / value line per field, the
//   settings section's shape, would print every label once per entry, and on a three-entry
//   document that was half the column. Column labels and cell text
//   come from what the panels already use — FormatCrystalIdentity for the crystal cell,
//   AxisPresetName for the axis preset, FilterSummary for the filter, kShapeScalarLabels for the
//   shape columns — rather than from a second rendering of the same facts; a cell whose column
//   does not apply to that row (a prism's Prism H) is empty, not "-" or "0".
//
//   A randomized scalar and an axis distribution are spelled by ONE formatter
//   (FormatShapeDistCell / FormatAxisDistCell below): `<letter> <centre>(<spread>)`, the letter
//   naming the distribution (DistributionLegend spells the letters out once, under the tables) and
//   the two numbers being the panel's own controls, unconverted — a uniform's spread is the full
//   width the Range box shows, never a ± half-width, which is what the notation avoids saying.
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

// A label / value pair as the window prints it, one display line each.
struct ConfigSummaryField {
  std::string label;
  std::string value;
};

// One titled block of settings fields ("Sun", "Simulation", "Render", "Settings").
struct ConfigSummaryGroup {
  std::string title;
  std::vector<ConfigSummaryField> fields;
};

// The settings group that collects every field with no main-panel control (see the header
// comment). Always last among the settings groups, and present only when such a field exists.
inline constexpr const char* kSettingsPopupOnlyGroupTitle = "Settings";

// A table of the document section: the header row once, then one row per entry. Every row has
// exactly columns.size() cells, by construction (the builders size a row from the column list and
// fill by index) — an ImGui table given fewer cells than columns does not complain, it shifts the
// rest of the row over, so the invariant is held here rather than checked on screen. An empty
// cell is a column that does not apply to that row.
struct ConfigSummaryTable {
  std::vector<std::string> columns;
  struct Row {
    std::vector<std::string> cells;
  };
  std::vector<Row> rows;
};

// One layer of the document: its heading line ("Layer 1 · Multi-scatter prob. 0.50 · 2 entries",
// spelled here at build time as the settings group titles are — a heading is a title, not a
// value, and takes no formatter) and its two tables. Row i of both tables is the layer's i-th
// entry, and both carry its number in their first column ("#"), so a reader can pair a crystal's
// shape with its row in the other table. A layer with no entries has two empty tables.
struct ConfigSummaryLayer {
  std::string heading;
  ConfigSummaryTable crystals;
  ConfigSummaryTable shape;
};

struct ConfigSummary {
  // LUMICE_GetVersionString(): the build that produced this picture, first because "which
  // version" is the question a shared screenshot is most often answering.
  std::string version;
  std::vector<ConfigSummaryGroup> settings;
  std::vector<ConfigSummaryLayer> document;
};

// The whole page for `state`. Pure, and the only thing the window renders.
ConfigSummary BuildConfigSummary(const GuiState& state);

// The number of label/value fields the settings section carries — the figure the functional
// test counts the rendered settings lines against. The document section is tables, whose rows
// are counted directly.
int CountConfigSummaryFields(const ConfigSummary& summary);

// The page as plain text, for the window's "Copy as text": the version line; each settings group
// as its title and then one `label<TAB>value` line per field; each layer as its heading and then
// its two tables, header row and rows alike as tab-joined cells; DistributionLegend last when
// there is a document. Blocks are separated by a blank line. Built from the page model — the same
// object the window draws — and never from what ImGui drew, so a field on the page is on the
// clipboard by construction (test_config_summary_rows.cpp holds that for every field and cell).
std::string FormatConfigSummaryAsText(const ConfigSummary& summary);

// ---- The distribution notation -------------------------------------------------------------
//
// The one spelling of "a value drawn from a distribution" on this page, for the three axis
// distributions and every randomizable shape scalar alike:
//
//   fixed                       "1.000"            (a shape scalar with no randomization)
//   randomized                  "U 0.900(0.100)"   letter, centre, spread in parentheses
//   full-circle uniform axis    "U"                (IsFullUniform360: mean 0, range 360 — the
//                                                   Random preset's azimuth and roll, folded so
//                                                   the commonest cell is the shortest)
//
// The letter is DistributionLetter's (G / U / Z / L / G*); the centre and the spread are the two
// numbers on the panel's own controls (Mean and Std / Range / Amplitude / Scale for an axis,
// centre and spread for a shape scalar), printed as they are. In particular a uniform's spread
// is the FULL width, as the Range box says — "0.900(0.100)" is [0.85, 0.95] — and is never halved
// into a ±: the composition-correctness chain compares these cells against the exported document,
// which carries the panel's numbers, and a converted spelling goes red there.

// The letter for a distribution type: "G" Gauss, "U" Uniform, "Z" Zigzag, "L" Laplacian,
// "G*" Gauss (legacy).
const char* DistributionLetter(AxisDistType type);
// The same, keyed by the wire spelling both ShapeDistTypeToString and AxisDistTypeJsonName
// produce ("gauss", "uniform", ...), which is how a shape scalar's type reaches the table without
// a second enum-to-letter switch. An unknown spelling reads "?": visible on the page, never a
// crash, and asserted by test so it cannot be mistaken for a sixth distribution.
const char* DistributionLetterForWireName(std::string_view wire_name);
// A shape scalar of slot `slot` (LUMICE_SHAPE_SCALAR_*), in the slot's own number format
// (ShapeScalarDomainFor), with " · sync N" after it when the scalar is in a sync group.
std::string FormatShapeDistCell(const ShapeDist& dist, int slot);
// An axis distribution, in the axis modal's "%.3g".
std::string FormatAxisDistCell(const AxisDist& axis);
// "G Gauss · U Uniform · Z Zigzag · L Laplacian · G* Gauss (legacy)": the letters spelled out,
// printed once under the document tables. Built from the same table as DistributionLetter and
// the combo's own labels (AxisDistTypeLabel), so it cannot list a letter the cells do not use.
std::string DistributionLegend();

}  // namespace lumice::gui

#endif  // LUMICE_GUI_CONFIG_SUMMARY_HPP
