// Summary page -> BuildExportJsonOrWarn: the two descriptions of one document agree.
//
// The page (src/gui/config_summary.*) prints what the GUI SHOWS; the export arm
// (file_io.hpp SceneIntent::kJsonExport) writes what the CLI needs to draw the picture on screen.
// Both are "the document as the user sees it", from two encoders, so this chain walks EVERY leaf
// of the exported core JSON and demands one of two things of it: a rule that finds the same value
// on the page, or an entry in the explicit exclusion table below with a reason. A leaf that
// matches neither fails the case — that meta-assertion is what keeps a new exported field from
// slipping past unexamined, and it is the whole difference between "parity" and "the fields I
// thought of".
//
// Two things this chain is the red-state for, both from the design constraint the page was built
// under: anchoring the page to the COMMIT arm (the lens would read dual_fisheye_equal_area on a
// linear document — the lens rule below goes red), and printing a field the GUI never shows (the
// sun's azimuth: exported, fixed at 0, no control — asserted absent from the Sun group). Verified
// red once by hand on each, the way the AC asks; the assertions are what remain.
//
// Number spelling: the settings section formats through FormatDiffValue (the diff engine's
// display form), the document section through the edit modal's own slot formats, so a rule
// compares NUMBERS re-spelled the page's way rather than raw strings — "1.0" and "1" are the same
// value read off two encoders, not a divergence.
//
// Labels are the PANEL's words, spelled here as literals on purpose ("Rays(M)", "EV Anchor",
// "Sky Color"): this file pins what a reader of the page sees, and reading the registry's
// LabelFor back would make the assertion true of any spelling at all. That the registry's word is
// the panel's word is test_config_summary_labels.cpp's job.
//
// A third thing the page does that the export does not: it prints only the fields the panel
// currently shows or enables. So a settings rule is conditional on the registry's own gate — a
// field whose ConstraintFor(...).enabled is false (Sky Color under Print, Roll under a full-sky
// lens, Rays(M) with Infinite rays on) is asserted ABSENT from the page while the export still
// writes it, and ExpectPageMatchesExport closes the loop in both directions over every registered
// leaf: enabled <=> on the page. The three documents at the bottom each take one of those gates.

#include <gtest/gtest.h>

#include <cctype>
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <regex>
#include <string>
#include <utility>
#include <vector>

#include "gui/app.hpp"
#include "gui/axis_presets.hpp"
#include "gui/config_summary.hpp"
#include "gui/defaults_diff.hpp"
#include "gui/field_editor_registry.hpp"
#include "gui/file_io.hpp"
#include "gui/gui_state.hpp"
#include "gui/raypath_segments.hpp"
#include "gui/shape_scalar_domain.hpp"
#include "support/scene_json_helpers.hpp"

namespace lumice::gui {
namespace {

using nlohmann::json;

// ---- The page, indexed for lookups --------------------------------------------------------

const ConfigSummaryGroup* FindGroup(const ConfigSummary& page, const std::string& title) {
  for (const auto& g : page.settings) {
    if (g.title == title) {
      return &g;
    }
  }
  for (const auto& g : page.document) {
    if (g.title == title) {
      return &g;
    }
  }
  return nullptr;
}

// The value printed under `label` in group `title`, or nullopt with a failure recorded.
std::optional<std::string> PageValue(const ConfigSummary& page, const std::string& title, const std::string& label) {
  const ConfigSummaryGroup* g = FindGroup(page, title);
  if (g == nullptr) {
    ADD_FAILURE() << "the page has no group titled \"" << title << "\"";
    return std::nullopt;
  }
  for (const auto& f : g->fields) {
    if (f.label == label) {
      return f.value;
    }
  }
  ADD_FAILURE() << "group \"" << title << "\" has no row labelled \"" << label << "\"";
  return std::nullopt;
}

// Whether group `title` carries a row labelled `label`, with no failure recorded either way.
bool HasRow(const ConfigSummary& page, const std::string& title, const std::string& label) {
  const ConfigSummaryGroup* g = FindGroup(page, title);
  if (g == nullptr) {
    return false;
  }
  for (const auto& f : g->fields) {
    if (f.label == label) {
      return true;
    }
  }
  return false;
}

std::string Fmt(const char* fmt, double v) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), fmt, v);
  return buf;
}

// The export's number as the page would spell it, compared against the page's text. The settings
// rows go through FormatDiffValue; a document row gets the format its slot uses, and the page's
// value is allowed to carry more after it (a randomized "1.000 ± 0.100 uniform", an axis's
// "Gauss · Mean 90 · Std 1") — so the check is "the page's value contains the number spelled
// this way", and the caller says which format.
void ExpectPageNumber(const ConfigSummary& page, const std::string& title, const std::string& label, const json& v,
                      const char* fmt = nullptr) {
  const auto got = PageValue(page, title, label);
  if (!got.has_value()) {
    return;
  }
  const std::string want = fmt == nullptr ? FormatDiffValue(v) : Fmt(fmt, v.get<double>());
  EXPECT_NE(got->find(want), std::string::npos) << title << " / " << label << ": page says \"" << *got
                                                << "\", export has " << v.dump() << " (\"" << want << "\")";
}

void ExpectPageText(const ConfigSummary& page, const std::string& title, const std::string& label,
                    const std::string& want) {
  const auto got = PageValue(page, title, label);
  if (!got.has_value()) {
    return;
  }
  EXPECT_NE(got->find(want), std::string::npos)
      << title << " / " << label << ": page says \"" << *got << "\", export has \"" << want << "\"";
}

// A settings row behind the registry's gate: when the panel enables `key_path` right now the page
// carries the row and its value is checked; when it does not, the row must be absent — the export
// still writes the field, and this is where the two are allowed to differ.
void ExpectGatedNumber(const GuiState& state, const ConfigSummary& page, const char* key_path, const std::string& title,
                       const std::string& label, const json& v) {
  if (ConstraintFor(key_path, state).enabled) {
    ExpectPageNumber(page, title, label, v);
  } else {
    EXPECT_FALSE(HasRow(page, title, label))
        << key_path << " is disabled on the panel but " << title << " / " << label << " is on the page";
  }
}

void ExpectGatedText(const GuiState& state, const ConfigSummary& page, const char* key_path, const std::string& title,
                     const std::string& label, const std::string& want) {
  if (ConstraintFor(key_path, state).enabled) {
    ExpectPageText(page, title, label, want);
  } else {
    EXPECT_FALSE(HasRow(page, title, label))
        << key_path << " is disabled on the panel but " << title << " / " << label << " is on the page";
  }
}

// ---- The export, flattened ---------------------------------------------------------------

struct Leaf {
  std::string path;  // "render[0].lens.type", "crystal[1].shape.face_distance[3]"
  json value;
};

void Flatten(const std::string& prefix, const json& node, std::vector<Leaf>& out) {
  if (node.is_object()) {
    for (auto it = node.begin(); it != node.end(); ++it) {
      Flatten(prefix.empty() ? it.key() : prefix + "." + it.key(), *it, out);
    }
    return;
  }
  if (node.is_array()) {
    // A numeric array is one leaf (a colour, a canvas, a face-distance sextet) — the same rule the
    // page's settings rows use; an array of objects is walked.
    bool all_scalar = true;
    for (const auto& e : node) {
      all_scalar = all_scalar && !e.is_structured();
    }
    if (all_scalar && !node.empty()) {
      out.push_back({ prefix, node });
      return;
    }
    for (size_t i = 0; i < node.size(); ++i) {
      Flatten(prefix + "[" + std::to_string(i) + "]", node[i], out);
    }
    if (node.empty()) {
      out.push_back({ prefix, node });
    }
    return;
  }
  out.push_back({ prefix, node });
}

// ---- The rules --------------------------------------------------------------------------

// Everything the export writes that the page does not carry, each with the reason. A leaf
// matching one of these is accounted for; nothing else is silently skipped.
struct Exclusion {
  const char* pattern;
  const char* reason;
};
const Exclusion kExcluded[] = {
  { R"(^scene\.light_source\.type$)", "constant \"sun\"; not a setting" },
  { R"(^scene\.light_source\.azimuth$)",
    "fixed at 0 with no control (doc/gui-state-governance.md §9) — asserted ABSENT from the page below" },
  { R"(^render\[0\]\.id$)", "bookkeeping" },
  { R"(^crystal\[\d+\]\.id$)", "bookkeeping: the core id, which the page maps back to the pool id" },
  { R"(^filter$)", "an empty filter list: nothing to show" },
  { R"(^filter\[\d+\]\.id$)", "bookkeeping" },
  { R"(^filter\[\d+\]\.type$)", "the filter's wire encoding (raypath / entry_exit / complex), not a setting" },
  { R"(^filter\[\d+\]\.composition$)",
    "the OR node a multi-row sum of products expands to; its members are asserted row by row" },
  { R"(^scene\.scattering\[\d+\]\.entries\[\d+\]\.filter$)",
    "the core filter id; the page shows the filter by its editor summary instead (checked per filter)" },
  { R"(^render\[0\]\.ray_color$)",
    "the export writes core's \"use the material colour\" sentinel [-1,-1,-1] and never reads the "
    "GuiState field (gui_state.hpp RenderConfigResimFields); the page shows the stored field the "
    "Settings panel's read-only row shows" },
  { R"(^render\[0\]\.lens_shift$)", "no GUI control exists (file_io.hpp, kJsonExport's first documented exception)" },
  { R"(^render\[0\]\.overlap$)", "the commit texture's seam band; 0 on the export arm, never shown" },
  { R"(^render\[0\]\.grid(\..*)?$)",
    "overlay annotations are kView-tier view preferences, which the page hides by tier "
    "(test_config_summary_rows.cpp); the export writes them because the CLI has no overlay stage" },
};

using Rule = std::function<void(const std::smatch&, const json&)>;

struct RuleEntry {
  std::regex pattern;
  Rule rule;
};

// The pool id the page prints for a scene's crystal id (the inverse of the export's numbering).
int PoolIdForCoreId(const GuiState& state, int core_id) {
  for (const auto& [pool, core] : ComputeCrystalPoolToCoreIdMap(state)) {
    if (core == core_id) {
      return pool;
    }
  }
  return -1;
}

std::string EntryTitle(int layer_idx, int entry_idx) {
  return "Layer " + std::to_string(DisplayLayerNumber(layer_idx)) + " · Entry " +
         std::to_string(DisplayEntryNumber(entry_idx));
}

// Which (layer, entry) groups on the page show scene crystal `core_id`: every entry whose crystal
// maps to it. A crystal's rows are asserted on each of them.
std::vector<std::string> EntryTitlesForCoreCrystal(const GuiState& state, int core_id) {
  const int pool = PoolIdForCoreId(state, core_id);
  std::vector<std::string> titles;
  for (size_t l = 0; l < state.layers.size(); ++l) {
    for (size_t e = 0; e < state.layers[l].entries.size(); ++e) {
      if (state.layers[l].entries[e].crystal_id == pool) {
        titles.push_back(EntryTitle(static_cast<int>(l), static_cast<int>(e)));
      }
    }
  }
  EXPECT_FALSE(titles.empty()) << "scene crystal " << core_id << " (pool " << pool << ") is on no entry";
  return titles;
}

// Shape-slot lookups by the export's key names (LUMICE_ShapeScalarSyncKeyName's spellings).
int ShapeSlotForExportKey(const std::string& key) {
  static const std::map<std::string, int> kSlots = {
    { "height", LUMICE_SHAPE_SCALAR_HEIGHT },
    { "prism_h", LUMICE_SHAPE_SCALAR_PRISM_H },
    { "upper_h", LUMICE_SHAPE_SCALAR_UPPER_H },
    { "lower_h", LUMICE_SHAPE_SCALAR_LOWER_H },
  };
  const auto it = kSlots.find(key);
  return it == kSlots.end() ? -1 : it->second;
}

std::vector<RuleEntry> BuildRules(const GuiState& state, const ConfigSummary& page, const json& doc) {
  std::vector<RuleEntry> rules;
  const auto add = [&](const char* pattern, Rule rule) { rules.push_back({ std::regex(pattern), std::move(rule) }); };

  // ---- scene ----
  add(R"(^scene\.light_source\.altitude$)",
      [&](const std::smatch&, const json& v) { ExpectPageNumber(page, "Sun", "Altitude", v); });
  add(R"(^scene\.light_source\.diameter$)",
      [&](const std::smatch&, const json& v) { ExpectPageNumber(page, "Sun", "Diameter", v); });
  add(R"(^scene\.light_source\.spectrum$)",
      [&](const std::smatch&, const json& v) { ExpectPageText(page, "Sun", "Spectrum", v.get<std::string>()); });
  add(R"(^scene\.light_source\.spectrum\[\d+\]\.(wavelength|weight)$)", [&](const std::smatch&, const json& v) {
    // A custom spectrum is exported as its table, walked here per cell; the page prints the same
    // table as one row (the array leaf the .lmc carries) and names the preset slot "custom".
    ExpectPageText(page, "Sun", "Spectrum", "custom");
    ExpectPageNumber(page, "Sun", "Custom spectrum", v);
  });
  add(R"(^scene\.max_hits$)",
      [&](const std::smatch&, const json& v) { ExpectPageNumber(page, "Simulation", "Max hits", v); });
  add(R"(^scene\.ray_allocation$)", [&](const std::smatch&, const json& v) {
    // No main-panel control (field_editor_registry.cpp: has_main_panel_surface=false), so the page
    // files it under its own "Settings" heading and not beside Rays(M).
    ExpectPageText(page, kSettingsPopupOnlyGroupTitle, "Ray allocation", v.get<std::string>());
    EXPECT_FALSE(HasRow(page, "Simulation", "Ray allocation"));
  });
  add(R"(^scene\.ray_num$)", [&](const std::smatch&, const json& v) {
    // The page shows the slider's unit (millions); the export the count — or the word "infinite",
    // in which case the panel greys the slider and the page carries Infinite rays = true and no
    // Rays(M) row.
    if (v.is_string()) {
      EXPECT_EQ(v.get<std::string>(), "infinite");
      EXPECT_FALSE(HasRow(page, "Simulation", "Rays(M)"));
      ExpectPageText(page, "Simulation", "Infinite rays", "true");
      return;
    }
    ExpectGatedNumber(state, page, "sim.ray_num_millions", "Simulation", "Rays(M)", json(v.get<double>() / 1.0e6));
  });
  add(R"(^scene\.scattering\[(\d+)\]\.prob$)", [&](const std::smatch& m, const json& v) {
    ExpectPageNumber(page, "Layer " + std::to_string(std::stoi(m[1]) + 1), "Multi-scatter prob.", v, "%.2f");
  });
  add(R"(^scene\.scattering\[(\d+)\]\.entries\[(\d+)\]\.crystal$)", [&](const std::smatch& m, const json& v) {
    // The page leads the crystal row with the POOL id (FormatCrystalIdentity's "#N"), so the
    // export's core id has to map back to the pool id of this very entry.
    const int l = std::stoi(m[1]);
    const int e = std::stoi(m[2]);
    const int pool = PoolIdForCoreId(state, v.get<int>());
    ASSERT_GE(pool, 0);
    EXPECT_EQ(state.layers[l].entries[e].crystal_id, pool);
    ExpectPageText(page, EntryTitle(l, e), "Crystal", "#" + std::to_string(pool));
  });
  add(R"(^scene\.scattering\[(\d+)\]\.entries\[(\d+)\]\.proportion$)", [&](const std::smatch& m, const json& v) {
    // An excluded entry is exported at proportion 0 with its weight kept; the page keeps the
    // weight too and says Enabled = false, which is what the user sees on the card.
    const int l = std::stoi(m[1]);
    const int e = std::stoi(m[2]);
    const EntryCard& card = state.layers[l].entries[e];
    if (card.enabled) {
      ExpectPageNumber(page, EntryTitle(l, e), "Weight", v, "%.1f");
      ExpectPageText(page, EntryTitle(l, e), "Enabled", "true");
    } else {
      EXPECT_FLOAT_EQ(v.get<float>(), 0.0f);
      ExpectPageText(page, EntryTitle(l, e), "Enabled", "false");
    }
  });

  // ---- render ----
  add(R"(^render\[0\]\.lens\.type$)",
      [&](const std::smatch&, const json& v) { ExpectPageText(page, "Render", "Lens Type", v.get<std::string>()); });
  // The view fields the full-sky gate covers: fov, the three pose angles, visible and front (roll
  // and front under the wider full-sky-or-globe gate). Each is asserted absent when its gate is
  // shut, present with the export's value when open.
  add(R"(^render\[0\]\.lens\.fov$)",
      [&](const std::smatch&, const json& v) { ExpectGatedNumber(state, page, "renderer.fov", "Render", "FOV", v); });
  add(R"(^render\[0\]\.view\.(elevation|azimuth|roll)$)", [&](const std::smatch& m, const json& v) {
    std::string label = m[1];
    label[0] = static_cast<char>(label[0] - 'a' + 'A');
    ExpectGatedNumber(state, page, ("renderer." + m[1].str()).c_str(), "Render", label, v);
  });
  add(R"(^render\[0\]\.visible$)", [&](const std::smatch&, const json& v) {
    ExpectGatedText(state, page, "renderer.visible", "Render", "Visible", v.get<std::string>());
  });
  add(R"(^render\[0\]\.front$)", [&](const std::smatch&, const json& v) {
    ExpectGatedText(state, page, "renderer.front", "Render", "Front", v.get<bool>() ? "true" : "false");
  });
  add(R"(^render\[0\]\.tone$)",
      [&](const std::smatch&, const json& v) { ExpectPageText(page, "Render", "Mode", v.get<std::string>()); });
  add(R"(^render\[0\]\.ev_mode$)",
      [&](const std::smatch&, const json& v) { ExpectPageText(page, "Render", "EV Anchor", v.get<std::string>()); });
  // The two grounds: the panel shows ONE swatch, Sky Color under Screen and Paper Color under
  // Print, so the page carries one row — and the export always writes both fields.
  add(R"(^render\[0\]\.background$)", [&](const std::smatch&, const json& v) {
    ExpectGatedNumber(state, page, "renderer.background", "Render", "Sky Color", v);
  });
  add(R"(^render\[0\]\.paper$)", [&](const std::smatch&, const json& v) {
    ExpectGatedNumber(state, page, "renderer.paper", "Render", "Paper Color", v);
  });
  add(R"(^render\[0\]\.intensity_factor$)", [&](const std::smatch&, const json& v) {
    // The export bakes 2^exposure_offset (doc/ev-pipeline-architecture.md §2.4); the page shows
    // the EV the user set. Same quantity, the user's spelling.
    ExpectPageNumber(page, "Render", "EV", json(std::log2(v.get<double>())));
  });
  add(R"(^render\[0\]\.resolution$)", [&](const std::smatch&, const json& v) {
    // The canvas is the sim resolution stretched to the aspect preset (file_io.cpp, the export
    // arm's canvas block): its SHORT edge is the resolution the user picked, and that is what
    // the page prints — the aspect preset itself is a view preference, hidden by tier.
    ASSERT_EQ(v.size(), 2u);
    ExpectPageNumber(page, "Render", "Resolution", json(std::min(v[0].get<int>(), v[1].get<int>())));
  });

  // ---- crystal ----
  add(R"(^crystal\[(\d+)\]\.type$)", [&](const std::smatch& m, const json& v) {
    const std::string want = v.get<std::string>() == "prism" ? "Prism" : "Pyramid";
    for (const auto& title : EntryTitlesForCoreCrystal(state, std::stoi(m[1]))) {
      ExpectPageText(page, title, "Crystal", want);
    }
  });
  add(R"(^crystal\[(\d+)\]\.axis\.(zenith|azimuth|roll)\.(type|mean|std)$)", [&](const std::smatch& m, const json& v) {
    std::string label = m[2];
    label[0] = static_cast<char>(label[0] - 'a' + 'A');
    for (const auto& title : EntryTitlesForCoreCrystal(state, std::stoi(m[1]))) {
      if (m[3] == "type") {
        // The wire word ("gauss") vs the combo label ("Gauss"): compare case-insensitively on
        // the leading letters, which is the same word.
        const auto got = PageValue(page, title, label);
        if (got.has_value()) {
          std::string page_lower = *got;
          for (auto& c : page_lower) {
            c = static_cast<char>(std::tolower(c));
          }
          std::string wire = v.get<std::string>();
          EXPECT_EQ(page_lower.rfind(wire.substr(0, 5), 0), 0u)
              << title << " / " << label << ": \"" << *got << "\" vs wire \"" << wire << "\"";
        }
      } else {
        ExpectPageNumber(page, title, label, v, "%.3g");
      }
    }
  });
  add(R"(^crystal\[(\d+)\]\.shape\.(height|prism_h|upper_h|lower_h)$)", [&](const std::smatch& m, const json& v) {
    const int slot = ShapeSlotForExportKey(m[2]);
    ASSERT_GE(slot, 0);
    // A fixed scalar exports as a bare number, a randomized one as {type, mean, std}; the page
    // prints the centre in the slot's format either way, and the spread after it.
    const char* fmt = ShapeScalarDomainFor(slot).fmt;
    for (const auto& title : EntryTitlesForCoreCrystal(state, std::stoi(m[1]))) {
      if (v.is_number()) {
        ExpectPageNumber(page, title, kShapeScalarLabels[slot], v, fmt);
      } else {
        ExpectPageNumber(page, title, kShapeScalarLabels[slot], v["mean"], fmt);
        ExpectPageNumber(page, title, kShapeScalarLabels[slot], v["std"], fmt);
        ExpectPageText(page, title, kShapeScalarLabels[slot], v["type"].get<std::string>());
      }
    }
  });
  add(R"(^crystal\[(\d+)\]\.shape\.(height|prism_h|upper_h|lower_h)\.(type|mean|std)$)",
      [&](const std::smatch& m, const json& v) {
        // The randomized form walked to its scalars (Flatten only keeps numeric arrays whole).
        const int slot = ShapeSlotForExportKey(m[2]);
        ASSERT_GE(slot, 0);
        for (const auto& title : EntryTitlesForCoreCrystal(state, std::stoi(m[1]))) {
          if (m[3] == "type") {
            ExpectPageText(page, title, kShapeScalarLabels[slot], v.get<std::string>());
          } else {
            ExpectPageNumber(page, title, kShapeScalarLabels[slot], v, ShapeScalarDomainFor(slot).fmt);
          }
        }
      });
  add(R"(^crystal\[(\d+)\]\.shape\.(upper|lower)_wedge_angle$)", [&](const std::smatch& m, const json& v) {
    for (const auto& title : EntryTitlesForCoreCrystal(state, std::stoi(m[1]))) {
      ExpectPageNumber(page, title, m[2] == "upper" ? "Upper A" : "Lower A", v, "%.3f");
    }
  });
  add(R"(^crystal\[(\d+)\]\.shape\.face_distance$)", [&](const std::smatch& m, const json& v) {
    // Six fixed faces export as one numeric array; each is its own page row.
    ASSERT_EQ(v.size(), 6u);
    for (const auto& title : EntryTitlesForCoreCrystal(state, std::stoi(m[1]))) {
      for (int i = 0; i < 6; ++i) {
        ExpectPageNumber(page, title, kShapeScalarLabels[LUMICE_SHAPE_SCALAR_FACE_0 + i], v[i],
                         ShapeScalarDomainFor(LUMICE_SHAPE_SCALAR_FACE_0 + i).fmt);
      }
    }
  });
  add(R"(^crystal\[(\d+)\]\.shape\.face_distance\[(\d+)\]\.(type|mean|std)$)",
      [&](const std::smatch& m, const json& v) {
        // A randomized face turns the sextet into an array of objects, walked per face.
        const int slot = LUMICE_SHAPE_SCALAR_FACE_0 + std::stoi(m[2]);
        for (const auto& title : EntryTitlesForCoreCrystal(state, std::stoi(m[1]))) {
          if (m[3] == "type") {
            ExpectPageText(page, title, kShapeScalarLabels[slot], v.get<std::string>());
          } else {
            ExpectPageNumber(page, title, kShapeScalarLabels[slot], v, ShapeScalarDomainFor(slot).fmt);
          }
        }
      });
  add(R"(^crystal\[(\d+)\]\.shape\.face_distance\[(\d+)\]$)", [&](const std::smatch& m, const json& v) {
    // A fixed face beside randomized siblings: a bare number inside the object array.
    const int slot = LUMICE_SHAPE_SCALAR_FACE_0 + std::stoi(m[2]);
    for (const auto& title : EntryTitlesForCoreCrystal(state, std::stoi(m[1]))) {
      ExpectPageNumber(page, title, kShapeScalarLabels[slot], v, ShapeScalarDomainFor(slot).fmt);
    }
  });
  add(R"(^crystal\[(\d+)\]\.shape\.sync_group.*$)", [&](const std::smatch& m, const json& v) {
    // The page marks a grouped slot "· sync N"; the export lists the group per slot name. Every
    // non-zero group number in the export has to appear on that crystal's page rows.
    for (const auto& title : EntryTitlesForCoreCrystal(state, std::stoi(m[1]))) {
      const ConfigSummaryGroup* g = FindGroup(page, title);
      if (g == nullptr) {
        ADD_FAILURE() << "no group " << title;
        continue;
      }
      const auto has_marker = [&](int group) {
        const std::string marker = "sync " + std::to_string(group);
        for (const auto& f : g->fields) {
          if (f.value.find(marker) != std::string::npos) {
            return true;
          }
        }
        return false;
      };
      // One slot's group, or the face sextet's six.
      const json groups = v.is_array() ? v : json::array({ v });
      for (const auto& g_id : groups) {
        if (g_id.is_number_integer() && g_id.get<int>() != 0) {
          EXPECT_TRUE(has_marker(g_id.get<int>())) << title << ": sync group " << g_id.get<int>() << " not on the page";
        }
      }
    }
  });
  add(R"(^crystal\[(\d+)\]\.name$)", [&](const std::smatch& m, const json& v) {
    for (const auto& title : EntryTitlesForCoreCrystal(state, std::stoi(m[1]))) {
      ExpectPageText(page, title, "Crystal", v.get<std::string>());
    }
  });

  // ---- filter ----
  // The export's filters are CORE filters: BuildScene expands each GUI filter's sum of products
  // into one raypath / entry-exit filter per row plus, for a multi-row SoP, a "complex" node
  // composing them (file_io.cpp ExpandSopToClauses). Entries reference the top node. So the page
  // rows a core filter must be found on are the entries whose top node is it, or composes it.
  const auto entries_reaching_filter = [&](int export_idx) {
    std::vector<std::string> titles;
    const json& filters = doc["filter"];
    const json& scattering = doc["scene"]["scattering"];
    for (size_t l = 0; l < scattering.size(); ++l) {
      const json& entries = scattering[l]["entries"];
      for (size_t e = 0; e < entries.size(); ++e) {
        if (!entries[e].contains("filter")) {
          continue;
        }
        const int top = entries[e]["filter"].get<int>();
        bool reaches = top == export_idx;
        if (!reaches && filters[top].contains("composition")) {
          for (const auto& member : filters[top]["composition"]) {
            reaches = reaches || member.get<int>() == export_idx;
          }
        }
        if (reaches) {
          titles.push_back(EntryTitle(static_cast<int>(l), static_cast<int>(e)));
        }
      }
    }
    EXPECT_FALSE(titles.empty()) << "exported filter " << export_idx << " is on no entry";
    return titles;
  };
  // The page's filter text for an entry: the card summary plus, for a multi-row SoP, the rows.
  const auto page_filter_text = [&](const std::string& title) {
    std::string joined;
    const ConfigSummaryGroup* g = FindGroup(page, title);
    if (g == nullptr) {
      ADD_FAILURE() << "no group " << title;
      return joined;
    }
    for (const auto& f : g->fields) {
      if (f.label == "Filter" || f.label.rfind("Filter row", 0) == 0) {
        joined += f.value + "\n";
      }
    }
    return joined;
  };
  add(R"(^filter\[(\d+)\]\.action$)", [&](const std::smatch& m, const json& v) {
    const std::string want = v.get<std::string>() == "filter_in" ? " In" : " Out";
    for (const auto& title : entries_reaching_filter(std::stoi(m[1]))) {
      ExpectPageText(page, title, "Filter", want);
    }
  });
  add(R"(^filter\[(\d+)\]\.symmetry$)", [&](const std::smatch& m, const json& v) {
    // FilterSummary's suffix is the letters of the symmetries that are ON — the same letters, in
    // the same order, the export writes.
    for (const auto& title : entries_reaching_filter(std::stoi(m[1]))) {
      const auto got = PageValue(page, title, "Filter");
      if (got.has_value()) {
        const size_t sp = got->rfind(' ');
        const std::string sym = sp == std::string::npos ? "" : got->substr(sp + 1);
        EXPECT_EQ(sym == "In" || sym == "Out" ? "" : sym, v.get<std::string>())
            << title << ": the page says \"" << *got << "\"";
      }
    }
  });
  add(R"(^filter\[(\d+)\]\.raypath$)", [&](const std::smatch& m, const json& v) {
    // The face-number list, as the editor's row text spells it ("3-5-1").
    std::string text;
    for (const auto& face : v) {
      text += (text.empty() ? "" : "-") + std::to_string(face.get<int>());
    }
    for (const auto& title : entries_reaching_filter(std::stoi(m[1]))) {
      const std::string joined = page_filter_text(title);
      EXPECT_NE(joined.find(text), std::string::npos)
          << title << ": raypath \"" << text << "\" is not among the page's filter rows:\n"
          << joined;
    }
  });
  add(R"(^filter\[(\d+)\]\.(entry|exit)$)", [&](const std::smatch& m, const json& v) {
    // An entry-exit row: each face number the export names is in the page's filter text.
    const json faces = v.is_array() ? v : json::array({ v });
    for (const auto& title : entries_reaching_filter(std::stoi(m[1]))) {
      const std::string joined = page_filter_text(title);
      for (const auto& face : faces) {
        EXPECT_NE(joined.find(std::to_string(face.get<int>())), std::string::npos)
            << title << ": " << m[2] << " face " << face.dump() << " is not among the page's filter rows:\n"
            << joined;
      }
    }
  });
  add(R"(^filter\[(\d+)\]\.(min_len|max_len|length)$)", [&](const std::smatch& m, const json& v) {
    for (const auto& title : entries_reaching_filter(std::stoi(m[1]))) {
      const std::string joined = page_filter_text(title);
      EXPECT_NE(joined.find(std::to_string(v.get<int>())), std::string::npos)
          << title << ": " << m[2] << " " << v.dump() << " is not among the page's filter rows:\n"
          << joined;
    }
  });
  return rules;
}

// The chain itself, over one document.
void ExpectPageMatchesExport(const GuiState& state, const char* scenario) {
  SCOPED_TRACE(scenario);
  std::string out;
  std::string warning;
  ASSERT_TRUE(BuildExportJsonOrWarn(state, &out, &warning)) << warning;
  const json doc = json::parse(out);
  const ConfigSummary page = BuildConfigSummary(state);

  std::vector<Leaf> leaves;
  Flatten("", doc, leaves);
  ASSERT_FALSE(leaves.empty());

  std::vector<std::regex> excluded;
  for (const auto& ex : kExcluded) {
    excluded.emplace_back(ex.pattern);
  }
  const auto rules = BuildRules(state, page, doc);

  int matched = 0;
  for (const auto& leaf : leaves) {
    bool accounted = false;
    for (const auto& ex : excluded) {
      if (std::regex_match(leaf.path, ex)) {
        accounted = true;
        break;
      }
    }
    if (accounted) {
      continue;
    }
    for (const auto& rule : rules) {
      std::smatch m;
      if (std::regex_match(leaf.path, m, rule.pattern)) {
        rule.rule(m, leaf.value);
        accounted = true;
        ++matched;
        break;
      }
    }
    // The meta-assertion: a leaf the export writes that no rule reads and no exclusion names.
    EXPECT_TRUE(accounted) << "exported leaf \"" << leaf.path << "\" = " << leaf.value.dump()
                           << " is neither matched to the page nor explicitly excluded";
  }
  EXPECT_GT(matched, 10) << "the rules matched almost nothing — the export shape has moved";

  // Both directions at once, over every registered settings leaf rather than only the ones the
  // rules above happen to name: a leaf the panel enables is on the page under its panel label,
  // and a leaf on the page is one the panel enables. The label is the registry's here (this is
  // the membership claim; the spelling claim is the rules' literals above) and a popup-only leaf
  // is looked for under the "Settings" heading, which has no gate.
  for (const auto& row : BuildConfigSummaryRows(state)) {
    const FieldEditorEntry* editor = FindFieldEditor(row.key_path);
    if (editor == nullptr) {
      continue;
    }
    const std::string root = row.key_path.substr(0, row.key_path.find('.'));
    const std::string leaf = row.key_path.substr(root.size() + 1);
    const char* declared = LabelFor(row.key_path);
    // An undeclared label is the key's own words ("ray_allocation" -> "Ray allocation"); compared
    // case-insensitively with underscores as spaces so the page's capitalisation is not restated.
    std::string label = declared != nullptr ? declared : leaf;
    const auto same_words = [](std::string a, std::string b) {
      for (std::string* t : { &a, &b }) {
        for (auto& c : *t) {
          c = c == '_' ? ' ' : static_cast<char>(std::tolower(c));
        }
      }
      return a == b;
    };
    const auto has_row_words = [&](const char* title) {
      const ConfigSummaryGroup* g = FindGroup(page, title);
      if (g == nullptr) {
        return false;
      }
      for (const auto& f : g->fields) {
        if (same_words(f.label, label)) {
          return true;
        }
      }
      return false;
    };
    if (!editor->has_main_panel_surface) {
      EXPECT_TRUE(has_row_words(kSettingsPopupOnlyGroupTitle))
          << row.key_path << " has no main-panel control and is not under the Settings heading";
      for (const char* title : { "Sun", "Simulation", "Render" }) {
        EXPECT_FALSE(has_row_words(title)) << row.key_path << " is under " << title << " as well";
      }
      continue;
    }
    const char* title = root == "sun" ? "Sun" : root == "sim" ? "Simulation" : root == "renderer" ? "Render" : "";
    if (*title == '\0') {
      ADD_FAILURE() << row.key_path << ": a registered leaf under a root this chain has no group title for";
      continue;
    }
    const bool enabled = ConstraintFor(row.key_path, state).enabled;
    EXPECT_EQ(has_row_words(title), enabled) << row.key_path << ": panel " << (enabled ? "enables" : "disables")
                                             << " it, page " << (has_row_words(title) ? "prints" : "omits") << " it";
  }

  // The other half of the design constraint: nothing on the page that the GUI does not show. The
  // one exported field that is user-INvisible by design is the sun's azimuth; the Sun group must
  // not have an Azimuth row (the Render group's Azimuth is the camera's, a real control).
  const ConfigSummaryGroup* sun = FindGroup(page, "Sun");
  ASSERT_NE(sun, nullptr);
  for (const auto& f : sun->fields) {
    EXPECT_NE(f.label, "Azimuth") << "the sun's azimuth is on the page, and there is no control for it";
  }
  ASSERT_TRUE(doc["scene"]["light_source"].contains("azimuth"))
      << "the export no longer writes the sun azimuth; the absence assertion above is vacuous";
}

// ---- Documents ---------------------------------------------------------------------------

void SeedDefaultDocument() {
  DoNew();
}

// Every renderer field off its default, the linear lens the constraint names, and the front
// clip on — the document on which a commit-arm anchoring is most visibly wrong.
void SeedLinearFrontDocument() {
  DoNew();
  g_state.renderer.lens_type = kLensTypeLinear;
  g_state.renderer.fov = 55.0f;
  g_state.renderer.elevation = 30.0f;
  g_state.renderer.azimuth = 77.0f;
  g_state.renderer.roll = 5.0f;
  g_state.renderer.visible = 0;
  g_state.renderer.front = true;
  g_state.renderer.exposure_offset = 1.5f;
  g_state.renderer.ev_mode = 1;
  g_state.renderer.tone = 1;
  g_state.renderer.sim_resolution_index = 2;
  g_state.renderer.background[0] = 0.1f;
  g_state.renderer.background[1] = 0.2f;
  g_state.renderer.background[2] = 0.3f;
  g_state.renderer.paper[0] = 0.9f;
  g_state.renderer.paper[1] = 0.8f;
  g_state.renderer.paper[2] = 0.7f;
  g_state.sun.altitude = 33.0f;
  g_state.sun.diameter = 0.7f;
  g_state.sun.spectrum_index = 4;
  g_state.sim.max_hits = 12;
  g_state.sim.ray_num_millions = 2.5f;
  g_state.sim.ray_allocation_adaptive = false;
  g_state.aspect_preset = AspectPreset::k16x9;
}

// Two layers, three entries, both crystal types, a Plate preset, randomized and synced shape
// scalars, a named raypath filter, a multi-row SoP filter, an excluded entry, and a custom
// spectrum — the document section with every branch taken.
void SeedRichDocument() {
  DoNew();
  g_state.crystals.assign(2, CrystalConfig{});
  CrystalConfig& plate = g_state.crystals[0];
  plate.name = "plate";
  plate.type = CrystalType::kPrism;
  plate.height = ShapeDist{ ShapeDistType::kUniform, 0.3f, 0.05f };
  plate.zenith = AxisDist{ AxisDistType::kGauss, 0.0f, 2.5f };
  plate.azimuth = kAzFullUniform;
  plate.roll = kRollFreeUniform;
  CrystalConfig& pyr = g_state.crystals[1];
  pyr.type = CrystalType::kPyramid;
  pyr.prism_h = 1.7f;
  pyr.upper_h = 0.35f;
  pyr.lower_h = 0.15f;
  pyr.upper_alpha = 31.5f;
  pyr.lower_alpha = 25.0f;
  pyr.face_distance[2] = ShapeDist{ ShapeDistType::kGauss, 0.9f, 0.1f };
  pyr.face_distance[0].sync_group = 1;
  pyr.face_distance[1].sync_group = 1;
  pyr.zenith = AxisDist{ AxisDistType::kLaplacian, 90.0f, 3.0f };
  pyr.azimuth = AxisDist{ AxisDistType::kZigzag, 10.0f, 20.0f };
  pyr.roll = kRollLockedGauss;

  g_state.filters.assign(2, FilterConfig{});
  g_state.filters[0].name = "cza";
  g_state.filters[0].SetRaypath(RaypathParams{ "3-5-1" });
  g_state.filters[0].action = 0;
  g_state.filters[0].sym_p = true;
  g_state.filters[0].sym_b = false;
  g_state.filters[0].sym_d = true;
  FilterConfig& sop = g_state.filters[1];
  sop.name = "two rows";
  sop.action = 1;
  sop.sym_p = false;
  sop.sym_b = true;
  sop.sym_d = false;
  for (const std::string& row : { std::string("3-5"), std::string("1-3-2") }) {
    sop.param.push_back(SummandText{ row, ParseSummandText(row) });
  }

  Layer first;
  first.probability = 0.35f;
  EntryCard a;
  a.crystal_id = 0;
  a.filter_id = 0;
  a.proportion = 60.0f;
  EntryCard b;
  b.crystal_id = 1;
  b.filter_id = 1;
  b.proportion = 40.0f;
  first.entries = { a, b };
  g_state.filters.push_back(FilterConfig{});
  FilterConfig& ee = g_state.filters[2];
  EntryExitParams ep;
  ep.entry_text = "3";
  ep.exit_text = "5";
  ep.length_mode = 1;
  ep.min_len = 4;
  ee.SetEntryExit(ep);

  Layer second;
  second.probability = 0.0f;
  EntryCard c;
  c.crystal_id = 0;
  c.proportion = 25.0f;
  c.enabled = false;
  EntryCard d;
  d.crystal_id = 1;
  d.filter_id = 2;
  d.proportion = 75.0f;
  second.entries = { c, d };
  g_state.layers = { first, second };

  g_state.sun.spectrum_index = kCustomSpectrumIndex;
  g_state.sun.custom_spectrum = { { 450.0f, 1.0f }, { 550.0f, 0.5f }, { 620.0f, 0.25f } };
}

// The default document under a full-sky lens: fov, the pose angles, visible and front all stop
// applying on the panel (NotUnderFullSky / NotUnderFullSkyOrGlobe), while the export keeps
// writing each of them.
void SeedFullSkyDocument() {
  DoNew();
  g_state.renderer.lens_type = kLensTypeDualFisheyeEqualArea;
  g_state.renderer.fov = 180.0f;
  g_state.renderer.elevation = 12.0f;
  g_state.renderer.azimuth = 34.0f;
}

// The default document with Infinite rays on: the Rays(M) slider is greyed on the panel, the
// export writes a count regardless.
void SeedInfiniteRaysDocument() {
  DoNew();
  g_state.sim.infinite = true;
  g_state.sim.ray_num_millions = 7.0f;
}

}  // namespace

TEST(ConfigSummaryExportParityChain, DefaultDocument) {
  SeedDefaultDocument();
  ExpectPageMatchesExport(g_state, "default");
  // Screen tone: the sky's row, not the paper's — spelled out beside the gated rule so the two
  // grounds' exclusivity is a stated fact of this document and not only a consequence of the gate.
  const ConfigSummary page = BuildConfigSummary(g_state);
  EXPECT_TRUE(HasRow(page, "Render", "Sky Color"));
  EXPECT_FALSE(HasRow(page, "Render", "Paper Color"));
  // Rays(M) on, Ray allocation under its own heading and nowhere else.
  EXPECT_TRUE(HasRow(page, "Simulation", "Rays(M)"));
  EXPECT_TRUE(HasRow(page, kSettingsPopupOnlyGroupTitle, "Ray allocation"));
  EXPECT_FALSE(HasRow(page, "Simulation", "Ray allocation"));
}

TEST(ConfigSummaryExportParityChain, FullSkyLensHidesTheViewFieldsThePanelGreys) {
  SeedFullSkyDocument();
  ExpectPageMatchesExport(g_state, "full_sky");
  const ConfigSummary page = BuildConfigSummary(g_state);
  for (const char* label : { "FOV", "Elevation", "Azimuth", "Roll", "Visible", "Front" }) {
    EXPECT_FALSE(HasRow(page, "Render", label)) << label;
  }
  EXPECT_EQ(PageValue(page, "Render", "Lens Type").value_or(""), "dual_fisheye_equal_area");
  // The export still carries them: the page is what differs, not the document.
  std::string out;
  std::string warning;
  ASSERT_TRUE(BuildExportJsonOrWarn(g_state, &out, &warning)) << warning;
  const json doc = json::parse(out);
  EXPECT_TRUE(doc["render"][0].contains("fov") || doc["render"][0]["lens"].contains("fov"));
  EXPECT_TRUE(doc["render"][0].contains("view"));
}

TEST(ConfigSummaryExportParityChain, InfiniteRaysHidesTheRayTotalThePanelGreys) {
  SeedInfiniteRaysDocument();
  ExpectPageMatchesExport(g_state, "infinite");
  const ConfigSummary page = BuildConfigSummary(g_state);
  EXPECT_FALSE(HasRow(page, "Simulation", "Rays(M)"));
  EXPECT_EQ(PageValue(page, "Simulation", "Infinite rays").value_or(""), "true");
  std::string out;
  std::string warning;
  ASSERT_TRUE(BuildExportJsonOrWarn(g_state, &out, &warning)) << warning;
  EXPECT_TRUE(json::parse(out)["scene"].contains("ray_num"));
}

TEST(ConfigSummaryExportParityChain, LinearFrontDocumentPrintsTheUsersLens) {
  SeedLinearFrontDocument();
  ExpectPageMatchesExport(g_state, "linear_front");
  // Spelled out as well as reached through the rules: the constraint's own example. The commit
  // arm would say dual_fisheye_equal_area / 180 / view (0,0,0) here.
  const ConfigSummary page = BuildConfigSummary(g_state);
  EXPECT_EQ(PageValue(page, "Render", "Lens Type").value_or(""), "linear");
  EXPECT_EQ(PageValue(page, "Render", "FOV").value_or(""), "55");
  EXPECT_EQ(PageValue(page, "Render", "Azimuth").value_or(""), "77");
  EXPECT_EQ(PageValue(page, "Render", "Visible").value_or(""), "upper");
  EXPECT_EQ(PageValue(page, "Render", "Front").value_or(""), "true");
  EXPECT_EQ(PageValue(page, "Render", "EV").value_or(""), "1.5");
  EXPECT_EQ(PageValue(page, "Render", "Resolution").value_or(""), "2048");
  // This document is under Print (tone = 1): the paper's row, not the sky's — the mirror image of
  // DefaultDocument's pair, so the two cases together are the exclusivity in both tones.
  EXPECT_EQ(PageValue(page, "Render", "Mode").value_or(""), "print");
  EXPECT_TRUE(HasRow(page, "Render", "Paper Color"));
  EXPECT_FALSE(HasRow(page, "Render", "Sky Color"));
}

TEST(ConfigSummaryExportParityChain, RichDocument) {
  SeedRichDocument();
  ExpectPageMatchesExport(g_state, "rich");
  const ConfigSummary page = BuildConfigSummary(g_state);
  EXPECT_EQ(PageValue(page, "Layer 1 · Entry 1", "Axis").value_or(""), "Plate");
  EXPECT_EQ(PageValue(page, "Layer 1 · Entry 1", "Filter").value_or(""), "3-5-1 In PD");
  EXPECT_EQ(PageValue(page, "Layer 1 · Entry 1", "Filter name").value_or(""), "cza");
  EXPECT_EQ(PageValue(page, "Layer 1 · Entry 2", "Filter row 1").value_or(""), "3-5");
  EXPECT_EQ(PageValue(page, "Layer 1 · Entry 2", "Filter row 2").value_or(""), "1-3-2");
  EXPECT_EQ(PageValue(page, "Layer 2 · Entry 1", "Enabled").value_or(""), "false");
  EXPECT_EQ(PageValue(page, "Layer 1", "Multi-scatter prob.").value_or(""), "0.35");
  EXPECT_FALSE(page.version.empty());
}

}  // namespace lumice::gui
