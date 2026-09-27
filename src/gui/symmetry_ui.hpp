#ifndef LUMICE_GUI_SYMMETRY_UI_HPP
#define LUMICE_GUI_SYMMETRY_UI_HPP

// task-356.3 — Shared P/B/D symmetry UI helpers, extracted from edit_modals.cpp
// so both the filter modal (edit_modals.cpp) and the Colors window per-ref row
// (color_window.cpp) render identical checkbox behaviour (a12 单一部件).
//
// Note: IsDApplicableGuiAxis() is a PURE function (no ImGui dependency); it lives
// alongside the ImGui-based RenderSymmetryCheckboxes() only because both are
// consumed by the same two call sites. Test code needing D-applicability logic
// alone can call it without initializing an ImGui context.

#include "gui/gui_state.hpp"  // AxisDist

namespace lumice::gui {

// Returns true when the given (azimuth, roll) axis config satisfies D-symmetry
// conditions: azimuth uniform 360° AND roll mean a multiple of 30°.
//
// Delegates to core's own predicate through LUMICE_IsDApplicable — it does not mirror it. This
// used to be a transcription with a "keep in sync" note, and it had drifted: a 1e-3 tolerance
// against core's 1e-5, which on a 3.05e-5 azimuth-range residue (what a Range slider dragged to
// its stop used to store) made the checkbox report D as live while the engine had already
// dropped it. A comment asking two copies to agree is not a mechanism; one owner is.
bool IsDApplicableGuiAxis(const AxisDist& az, const AxisDist& roll);

// The axis halves of P and B, likewise delegated to core (LUMICE_IsPApplicable /
// LUMICE_IsBApplicable): P needs roll invariant under a 60° turn (uniform 360°), B needs zenith
// symmetric about 90° with a uniform-360° azimuth. `zenith` is the GUI's (the wire's) zenith.
bool IsPApplicableGuiAxis(const AxisDist& roll);
bool IsBApplicableGuiAxis(const AxisDist& az, const AxisDist& zenith);

// Which of P / B / D are PHYSICAL symmetries of one crystal — the engine's own answer
// (LUMICE_GetCrystalSymmetry), not a GUI rule: its axis (orientation distribution) for all three,
// and its SHAPE for all three (a prism with face_distance [1, 1.2, 1, 1.2, 1, 1.2] is three-fold, so
// only 120° rotations are; unlike upper and lower cones rule B out). The raypath-analysis list
// merges only under these. A filter's P/B/D does not read them — it is a label equivalence — so for
// a filter they say when a ticked element merges paths that are not physically equivalent.
// Default-constructed = everything holds, which is what a caller with no single crystal passes.
struct SymmetryAvailability {
  int rotation_step = 1;  // shape: 1: all six rotations; 2 / 3: only multiples of 120° / 180°; 6: none
  bool b = true;          // B physical: axis condition AND the shape has the horizontal mirror
  bool d = true;          // D physical: axis condition AND the shape has the mirror that axis selects
  bool d_axis = true;     // D's axis condition alone — a filter's D acts exactly when this holds
  bool p_axis = true;     // P's axis condition alone
  bool b_axis = true;     // B's axis condition alone — picks which reason the B hint gives
};

// What the P/B/D checkboxes being drawn mean — named at every call site, never inferred from the
// availability passed in (core's SymmetrySemantics, as the GUI sees it).
enum class SymmetryCheckboxMeaning {
  // A filter or a colour ref: label equivalence. Hints say when a ticked element merges physically
  // inequivalent paths on this crystal, and how to select them exactly instead.
  kFilterLabel,
  // The raypath-analysis list: merges only physically equivalent paths, per crystal.
  kAnalysisPhysical,
};

// The availability for crystal `cr`, asked of the engine through the GUI's single crystal
// translation (FillCrystalParam). A crystal the C API rejects answers "nothing acts" — the side on
// which a hint can only be over-cautious.
SymmetryAvailability SymmetryAvailabilityFor(const CrystalConfig& cr);

// Renders the "P B D" checkbox row (ImGui). `id_suffix` disambiguates ImGui
// item IDs across call sites (mirrors the existing "##filter_modal" convention;
// pass e.g. "filter_modal" or "color_ref"). Every checkbox stays writable. Under
// kFilterLabel, one whose element is not a physical symmetry of this crystal is
// followed by a small info icon whose tooltip says what ticking it merges (or,
// for D off-axis, that it does nothing); under kAnalysisPhysical the hover text
// says the list merges only physically equivalent paths.
void RenderSymmetryCheckboxes(bool& sym_p, bool& sym_b, bool& sym_d, const SymmetryAvailability& avail,
                              SymmetryCheckboxMeaning meaning, const char* id_suffix);

}  // namespace lumice::gui

#endif  // LUMICE_GUI_SYMMETRY_UI_HPP
