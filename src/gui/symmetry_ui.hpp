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

// Which of P / B / D actually act on one crystal — the engine's own answer (LUMICE_GetCrystalSymmetry),
// not a GUI rule. A reduction uses only the elements the checkbox asks for AND the crystal allows:
// its axis for D, and its SHAPE for all three (a prism with face_distance [1, 1.2, 1, 1.2, 1, 1.2]
// is three-fold, so P rotates only by 120°; unlike upper and lower cones rule B out).
// Default-constructed = everything acts, which is what a caller with no single crystal passes.
struct SymmetryAvailability {
  int rotation_step = 1;  // 1: P uses all six rotations; 2 / 3: only multiples of 120° / 180°; 6: none
  bool b = true;          // the shape has the horizontal mirror
  bool d = true;          // D acts: axis condition AND the shape has the mirror that axis selects
  bool d_axis = true;     // the axis condition alone — picks which reason the D hint gives
};

// The availability for crystal `cr`, asked of the engine through the GUI's single crystal
// translation (FillCrystalParam). A crystal the C API rejects answers "nothing acts" — the side on
// which a hint can only be over-cautious.
SymmetryAvailability SymmetryAvailabilityFor(const CrystalConfig& cr);

// Renders the "P B D" checkbox row (ImGui). `id_suffix` disambiguates ImGui
// item IDs across call sites (mirrors the existing "##filter_modal" convention;
// pass e.g. "filter_modal" or "color_ref"). Every checkbox stays writable; one
// whose element does not (fully) act on this crystal is followed by a small
// info icon whose tooltip says why.
void RenderSymmetryCheckboxes(bool& sym_p, bool& sym_b, bool& sym_d, const SymmetryAvailability& avail,
                              const char* id_suffix);

}  // namespace lumice::gui

#endif  // LUMICE_GUI_SYMMETRY_UI_HPP
