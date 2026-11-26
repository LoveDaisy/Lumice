#ifndef LUMICE_GUI_CRYSTAL_PREVIEW_HPP
#define LUMICE_GUI_CRYSTAL_PREVIEW_HPP

#include "gui/axis_presets.hpp"
#include "imgui.h"
#include "include/lumice_editor.h"

namespace lumice::gui {

struct CrystalConfig;
struct GuiState;

// Seed the session selection once, after startup defaults resolve. Never call from document IO
// or ResetCrystalView; modal Combo edits remain live across New/Open/Revert.
void InitializeCrystalPreviewSession(const GuiState& startup_state);

// Crystal preview trackball state. g_crystal_rotation is the OBSERVATION rotation O: the user's
// drag/wheel view of the scene, composed around world axes by ApplyTrackballRotation. It no longer
// contains the crystal's physical pose — the representative pose R is derived per frame from the
// live axis buffers (DrawCrystalAxisGizmo's contract, exercised by RenderCrystalPreviewPane), and
// the composite the mesh is drawn with is O·R. Writers of O: ResetCrystalView (legacy), the
// representative-view resets, and drag; Reset All and parameter edits deliberately leave it alone.
extern float g_crystal_rotation[16];
extern float g_crystal_zoom;
extern int g_crystal_style;
extern int g_crystal_mesh_hash;

// Crystal preview helpers
int CrystalParamHash(const CrystalConfig& c);

// Returns true if the crystal has an active shape-field randomization (any consumed ShapeDist with
// type != kNoRandom), i.e. the preview mesh would vary with sample_seed. Only the shape fields that
// the current c.type actually feeds into LUMICE_CrystalParam are checked (kPrism → height; kPyramid
// → prism_h/upper_h/lower_h), plus face_distance[6] which both types consume. This mirrors the
// branch in BuildCrystalMeshData so the animation gate never spins on a field the mesh ignores.
bool HasActiveShapeRandomization(const CrystalConfig& c);
// Legacy GUI initialization path only; always returns Rx(20°). Any new
// reset-view logic should use ResetCrystalViewToRepresentative below. Used by main GUI startup
// and the test harness where no axis preset / distribution params are in scope.
void ResetCrystalView();
// Reset the preview's OBSERVATION to identity and the zoom to its default. The composite view then
// lands on the live buffers' classified family's representative pose (R is derived per frame, so
// there is nothing else to store); for an in-domain triple this is byte-identical to the old
// DefaultPreviewRotation(label, params) seat. Modal open / card-to-card switch, label chip
// switches and the Reset View button all mean this same gesture: re-seat on the ACTUAL family's
// representative view — a drifted (out-of-domain) label must not impose its old family's pose.
void ResetCrystalViewToRepresentative();
void ApplyTrackballRotation(float dx, float dy);

// Fixed preview sample seed. The GUI CrystalConfig shape fields are now first-class distributions,
// so a randomized shape DOES vary with the seed; this constant deliberately pins the preview to a
// single deterministic draw (seed 0) so the modal preview and thumbnails stay stable frame-to-frame
// and match the four tracked crystal reference images. It keeps the call sites (edit_modals.cpp,
// thumbnail_cache.cpp) on the SAME value rather than each inlining a literal. `inline constexpr`
// gives one definition across TUs.
// TODO(gui-preview-animation, gui-shape-randomization): replace with a real, per-frame-varying seed
// source so the preview animates while a shape distribution is active.
inline constexpr unsigned long long kPreviewFixedSampleSeed = 0;

// Build crystal mesh data from config: construct a LUMICE_CrystalParam, sample via
// LUMICE_GetCrystalMesh, then apply Y-Z swap + AABB normalization. `sample_seed` selects the
// shape draw (fixed to kPreviewFixedSampleSeed for a stable preview; a randomized shape yields a
// different but deterministic mesh per seed).
// Caller allocates LUMICE_CrystalMesh on stack (fixed-size arrays, no free needed). Returns true on success.
bool BuildCrystalMeshData(const CrystalConfig& cr, unsigned long long sample_seed, LUMICE_CrystalMesh* out);

// Build crystal mesh from config, apply Y-Z swap and AABB normalization,
// then upload to g_crystal_renderer. Returns the computed param hash, or 0 on failure.
int BuildAndUploadCrystalMesh(const CrystalConfig& cr, unsigned long long sample_seed);

// Access the most-recently uploaded crystal mesh (Y-Z swapped + AABB normalized).
// Returns nullptr if no mesh has been built yet or after ResetLastCrystalMesh().
// The returned pointer refers to file-scope singleton state in crystal_preview.cpp;
// the address remains stable across calls — only the pointed-to contents are
// overwritten by each BuildAndUploadCrystalMesh() call.
const LUMICE_CrystalMesh* GetLastCrystalMesh();

// Invalidate the cached mesh. Used in GUI tests to ensure per-case state isolation.
void ResetLastCrystalMesh();

// --------------------------------------------------------------------------------------------------
// The crystal preview's axis family-DOF portrait (task 841).
//
// Contract (consumed by RenderCrystalPreviewPane, which composes the two poses once per frame):
//   * O = the observation rotation (g_crystal_rotation) — the user's drag/wheel view;
//   * R = the representative physical pose, DERIVED per frame by DefaultPreviewRotation from the
//     classified family of the live axis buffers (typical chain for built-in families, live means
//     for Custom, isometric sentinel for Random). R is never stored — it is a pure function of the
//     buffers, so a reclassification or a Custom mean edit re-seats the physical pose on the next
//     submitted frame without touching O.
//   * The mesh, the face numbers and the BODY-frame portrait primitives (characteristic axis +
//     small DOF arrow) are drawn with O·R; the WORLD-frame primitives (azimuth ring, world-up
//     reference) are drawn with O alone. Observation drag therefore moves every primitive on
//     screen while preserving the physical relations between the two groups (the Plate c axis
//     stays coaxial with the ring's vertical, the Column/Lowitz axes stay horizontal).
// --------------------------------------------------------------------------------------------------

// One frame's record of what the portrait overlay actually submitted. Production never reads it;
// gui_test asserts on THESE values (real submitted matrices and primitive anchors) rather than
// recomputing the derivation, so the tests cannot pass on a parallel implementation that draws
// nothing. Overwritten every frame the preview pane submits; frame_index distinguishes fresh from
// stale reads.
struct CrystalAxisGizmoSnapshot {
  bool drawn = false;   // did the overlay draw any primitive this frame
  int frame_index = 0;  // ImGui::GetFrameCount() of the submit
  AxisPreset family = AxisPreset::kCustom;
  bool az_ring_drawn = false;
  bool body_axis_drawn = false;
  bool small_arrow_drawn = false;
  AxisGizmoSmallArrow small_arrow_kind = AxisGizmoSmallArrow::kNone;
  bool world_up_ref_drawn = false;
  // The three poses below are recorded AT THEIR CONSUMPTION SITES, not at an input boundary:
  // body_rotation is read by the same expression that feeds the body primitives' ComputeMvp,
  // world_rotation the one that feeds the world primitives' ComputeMvp, and mesh_render_rotation
  // is taken at the pane's g_crystal_renderer.Render call site (what the GL mesh actually
  // consumed). A consumption-site bug therefore flows into the record it contradicts.
  float body_rotation[16];
  float world_rotation[16];
  float mesh_render_rotation[16];
  // Screen anchors PROJECTED THROUGH the real MVPs (never re-derived by the tests): the az ring
  // arc's end pixel and the body axis tip's pixel. They are what the drawn strokes actually
  // follow, so anchor-based assertions stay sensitive to consumption bugs that a matrix record
  // alone could be edited around. Valid per group that drew (see az_ring_drawn /
  // body_axis_drawn).
  bool ring_end_px_valid = false;
  float ring_end_px[2];
  bool body_tip_px_valid = false;
  float body_tip_px[2];
  float axis_dir_mesh[3];        // unit direction of the drawn axis object (mesh frame)
  float axis_tip_mesh[3];        // exposed axis end hosting the small arrow (mesh frame)
  float small_arc_axis_mesh[3];  // rotation axis the small arrow indicates (mesh frame)
  float ring_normal_mesh[3];     // the az ring's rotation axis: world vertical in mesh coordinates
  float up_ref_dir_mesh[3];      // world vertical reference direction (mesh coordinates)
};

// The last frame's portrait submission. Valid only after a preview pane frame ran; pair with
// frame_index to detect staleness.
const CrystalAxisGizmoSnapshot& GetLastAxisGizmoSnapshot();
// Test-isolation reset (gui_test's per-case boundary): a stale "drawn" record must not answer the
// next case's negative assertions.
void ResetAxisGizmoSnapshot();
// The mesh-consumption witness: the pane calls this with the exact argument expression it hands
// to g_crystal_renderer.Render, so the snapshot's mesh_render_rotation mirrors what the GL pass
// actually consumed (a swapped Render argument flows into the record the tests check it against).
void RecordMeshRenderRotation(const float m[16]);

// Draw the family-DOF portrait over the crystal preview image and record the snapshot. `plan` is
// the ComputeAxisGizmoPlan answer for `live_axis` computed by the caller ONCE for this frame (the
// same read that derived the representative pose and drives the badge); `observation` is O and
// `body_rotation` is the O·R composite THE MESH WAS RENDERED WITH this frame — the caller composes
// both once, so the drawn primitives and the recorded matrices cannot disagree with the mesh about
// the frame's pose. Only the portrait-private geometry (axis object direction, Custom tilt axis)
// is derived here. `zoom` must be the zoom the mesh was rendered with (ComputeMvp alignment).
void DrawCrystalAxisGizmo(const AxisGizmoPlan& plan, const AxisDist live_axis[3], const float observation[16],
                          const float body_rotation[16], ImVec2 image_pos, float image_px, ImDrawList* draw_list,
                          float zoom);

}  // namespace lumice::gui

#endif  // LUMICE_GUI_CRYSTAL_PREVIEW_HPP
