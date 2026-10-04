#ifndef LUMICE_BASE_H_
#define LUMICE_BASE_H_

// The foundation every other capability header builds on: symbol visibility (LUMICE_API), the ABI
// version and its history, the opaque handle types, error codes, the ray-count type, logging and the
// product version. Every other lumice_*.h includes it; it includes none of them.

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Symbol visibility. Which functions a shared library exports is decided at link time by that
// library's export list, generated from its surface headers by scripts/gen_export_list.py (root
// CMakeLists.txt, lumice_apply_export_list; the headers are listed in cmake/export_surfaces.cmake)
// — liblumice exports exactly the functions declared in the lumice_*.h capability headers and the
// analytic capability, and liblumice_testapi / liblumice_analytic, built from the same objects,
// export their own lists. LUMICE_API does not export anything; it only makes a declaration eligible:
//   - GCC/Clang: default visibility. The engine objects are compiled with -fvisibility=hidden in
//     Release, and a hidden symbol cannot be exported by any list. The generator refuses a
//     declaration without the marker, so one cannot silently drop out of the library.
//   - Windows: the .def file does the exporting (no __declspec(dllexport) anywhere), so what is
//     left is the consumer-side dllimport, and only for a consumer that really links the engine
//     DLL. LUMICE_SHARED_DEFINE is an INTERFACE definition of the shared `lumice` target, so the
//     shells linking it see dllimport, while lumice_obj itself and every test/bench target that
//     links lumice_obj's objects directly see an empty macro — a dllimport there would ask the
//     linker for an __imp_ symbol no static link provides.
#if defined(_WIN32)
#if defined(LUMICE_SHARED_DEFINE)
#define LUMICE_API __declspec(dllimport)
#else
#define LUMICE_API
#endif
#else
#define LUMICE_API __attribute__((visibility("default")))
#endif

// =============== Constants ===============
// ABI version, encoded as major*100 + minor (v4.10 -> 410). Before this macro existed, the
// v4.3~v4.9 BREAKING bumps lived only in prose comments, so a caller linking against a header
// newer/older than the .so it loads had NO compile-time guard — a layout mismatch was silent UB.
// Callers can now pin the ABI they were built against, e.g.:
//   static_assert(LUMICE_API_VERSION >= 412, "Lumice header too old for this integration");
// Bump on every BREAKING change to the public symbol set / struct layout.
//
// BREAKING (v4.12): the wide `LUMICE_Config` value struct and everything that only existed to
// feed it are REMOVED. Gone: LUMICE_Config; the three commit entry points
// LUMICE_CommitConfig / LUMICE_CommitConfigFromFile / LUMICE_CommitConfigStruct;
// LUMICE_ParseConfigString / LUMICE_ParseConfigFile / LUMICE_ConfigToJson;
// LUMICE_ConfigCreateColorClasses / LUMICE_ConfigReleaseColorClasses /
// LUMICE_ConfigReleaseCompositions; and the C++ RAII header src/include/lumice_config_scope.hpp.
// LUMICE_Scene (added v4.11-era, see the "Scene (opaque handle)" section) is now the ONLY way to
// describe and commit a configuration: build with SceneCreate + Add*/Set* or parse with
// LUMICE_SceneFromJson / _FromJsonFile, then commit with LUMICE_CommitScene. No shim, no alias —
// callers of the removed symbols fail to compile, which is the intended migration signal.
// The LUMICE_MAX_CONFIG_* ceilings survive the struct: they are now the Scene's Add*/Set*
// per-kind soft caps, not the dimensions of an inline array.
//
// BREAKING (v4.14): LUMICE_StatsResult gains a fourth field, `orientation_num`
// — the count of distinct crystal ORIENTATIONS sampled, previously invisible
// (only the geometry count was reported, and on the commonest halo setup, a
// fixed shape under a random axis, that count is 1 no matter how richly the
// orientation was sampled). This is an APPEND, not a removal or a reorder, so
// existing field offsets are unchanged; but sizeof(LUMICE_StatsResult) grows
// from 24 to 32, so a caller that was NOT recompiled and passes an array of the
// old struct to LUMICE_FrameGetStats will have it written past the end.
// Recompile against this header.
//
// BREAKING (v4.15): the six server-taking result getters are REMOVED and replaced by the
// LUMICE_ResultFrame handle. Gone: LUMICE_GetRenderResults / LUMICE_GetCompositeResults /
// LUMICE_GetRawXyzResults / LUMICE_GetRawXyzAndCompositeResults / LUMICE_GetStatsResults /
// LUMICE_GetCachedStats. Read results by acquiring a frame — LUMICE_AcquireResultFrame, then
// LUMICE_FrameGetRender / _FrameGetComposite / _FrameGetRawXyz / _FrameGetStats, then
// LUMICE_ReleaseResultFrame. The buffers a frame hands out stay valid until THAT frame is
// released, instead of until the next getter call on the server; that is the whole point of the
// change — the old contract let one caller's read invalidate another's still-in-use pixels.
// Two old contracts disappear with their functions, both now structural: the combined
// xyz+composite getter existed to guarantee one generation, which any two reads off one frame
// have by construction; and the cached-stats getter's "may be stale, never triggers a snapshot"
// mode is gone — a frame carries the stats of the snapshot it is.
//
// BREAKING (v4.16): LUMICE_RenderParam loses its `opacity` field; struct layout changed.
// See the struct's own BREAKING note for why it went rather than gained an implementation.
// Recompile against this header.
// BREAKING (v4.16): LUMICE_RenderParam gains a trailing field, `ev_mode`
// (LUMICE_EV_MODE_RELATIVE / _ABSOLUTE), selecting which anchor the exposure scale is measured
// against. This is an APPEND, so every existing field keeps its offset; but sizeof() grows, so a
// caller that was NOT recompiled hands the API a shorter struct and the new field is read past
// the end of it. Recompile against this header.
// Note the accompanying DEFAULT change, which is a behavior break independent of the ABI one: a
// config with no "ev_mode" key now renders RELATIVE (anchored to the frame's own P99, i.e. what
// the GUI displays), where the v4.15-era CLI was unconditionally absolute. RELATIVE == 0 keeps
// that default reachable from a zero-initialized struct.
//
// BREAKING (v4.17): LUMICE_RenderParam.central_grid / central_grid_count renamed to
// angular_dist / angular_dist_count, and the JSON key "grid.central" to "grid.angular_dist".
// The field is the angular distance from the sun, which "central" never said; the rename makes
// the schema name what the number is. Type, order and offset are unchanged (sizeof() and the
// binary layout are identical), so this breaks SOURCE compatibility only: a caller naming the
// field, or using a designated initializer for it, fails to compile until renamed. The JSON
// decoders read "grid.central" as an alias forever (new key wins when both appear); the encoder
// only ever writes "grid.angular_dist".
// Note the accompanying BEHAVIOR change: these lines are now DRAWN by the CLI renderer, where
// v4.16 parsed and round-tripped them while drawing nothing. A config that already carried
// grid.central entries renders differently under v4.17. `width` is still not honored (the mask
// generator has no line-width input); `value`, `opacity` and `color` are.
//
// BREAKING (v4.18): LUMICE_RenderParam gains a trailing pair of fields, `longitude_grid` /
// `longitude_grid_count` — the meridians (lines of constant azimuth), the twin of the parallels
// `elevation_grid` already described. They are APPENDED after elevation_grid_count and before
// ev_mode, so every field up to elevation_grid_count keeps its offset while ev_mode moves and
// sizeof() grows; a caller that was NOT recompiled hands the API a shorter struct and has its
// ev_mode read from the wrong place. Recompile against this header. The JSON key is
// "grid.longitude" — "longitude" and not "azimuth" because the annotation layer already names
// this concept that way in public symbols (LUMICE_ANNOTATION_LONGITUDE,
// LUMICE_AnnotationRequest::longitude_deg).
// Note the accompanying BEHAVIOR change, which has no ABI half: `elevation_grid` is now DRAWN by
// the CLI renderer, where every version since it was introduced parsed and round-tripped it while
// drawing nothing. A config that already carried grid.elevation entries renders differently under
// v4.18. As with angular_dist, `width` is still not honored; `value`, `opacity` and `color` are.
//
// BREAKING (v4.19): LUMICE_RenderParam gains a trailing block of four fields, `zenith_nadir` /
// `zenith_nadir_radius_px` / `zenith_nadir_opacity` / `zenith_nadir_color` — the pixel-space ring
// markers at the zenith and the nadir. They are APPENDED after ev_mode, so every existing field
// keeps its offset while sizeof() grows; a caller that was NOT recompiled hands the API a shorter
// struct and the new fields are read past the end of it. Recompile against this header. The JSON
// key is "grid.zenith_nadir", an object rather than a list because the two markers are one control
// (one switch, one colour, one radius) and neither of them is a line with a value.
// Note the accompanying BEHAVIOR change, which has no ABI half: the CLI renderer now DRAWS these
// markers, and the GUI preview stops computing their screen position with its own duplicate
// projection (preview_renderer.cpp ProjectWorldDirToScreen) and reads
// LUMICE_ComputeAnnotationOverlay's zenith_px/py instead — so both drawers now take their geometry
// from one place, as angular_dist and the two grid families already do.
//
// BREAKING (v4.20): LUMICE_RenderParam gains a trailing `front` field — the front-hemisphere clip,
// APPENDED after zenith_nadir_color, so every existing field keeps its offset while sizeof() grows;
// a caller that was NOT recompiled hands the API a shorter struct and the new field is read past
// the end of it. Recompile against this header. The JSON key is a top-level "front" boolean on the
// renderer object, deliberately NOT a fourth "visible" enumerator: the two are orthogonal clips
// that AND together, and core's NLOHMANN_JSON_SERIALIZE_ENUM maps an unregistered "visible" string
// to the FIRST table entry (upper) without an error, so folding them would fail silently.
// Note the accompanying BEHAVIOR change, which has no ABI half: the clip was GUI-preview-only (a
// shader crop), and the GUI refused to export a document that had it on rather than write a config
// that renders differently. It is now a core field, so the CLI reproduces it — the background sky
// mask and every annotation (horizon, angular_dist, elevation, longitude, zenith/nadir) are clipped
// by the same rule the preview shader applies.
//
// BREAKING (v4.21): LUMICE_RenderParam gains a trailing block of three fields, `horizon_label` /
// `grid_label` / `angular_dist_label` — whether the CLI renderer draws the TEXT labels beside each
// annotation family's lines. They are APPENDED after `front`, so every existing field keeps its
// offset while sizeof() grows; a caller that was NOT recompiled hands the API a shorter struct and
// the new fields are read past the end of it. Recompile against this header. The JSON keys are
// "grid.horizon_label", "grid.label" and "grid.angular_dist_label".
// Note the accompanying BEHAVIOR change, which has no ABI half: the CLI renderer now has a font at
// all. LUMICE_ComputeAnnotationOverlay has returned label anchors and formatted text since v4.17,
// but only the GUI could turn them into pixels (ImGui's font needs a GL context); core now embeds
// a typeface and rasterizes them itself, so an exported config reproduces the preview's text and
// not merely its lines.
// Two layers, and the distinction is the contract: these switches decide whether the label
// GEOMETRY is computed, independently of the family's own line switch (`horizon_label` with
// `horizon` zero still draws the horizon's numbers). They do NOT give a label its own opacity —
// the compositor gives it the family's own colour and alpha, so a grid line at opacity 0 takes its
// labels with it. That mirrors the GUI, where a label has always inherited its family's
// appearance; a core that diverged here would create the very GUI/CLI mismatch the annotation
// layer exists to remove.
//
// v4.23: LUMICE_RawXyzResult gains `axis_solid_angle`, the missing half of the exposure anchor.
// `anchor_l99_sky` (v4.22) is a RADIANCE and `xyz_buffer` holds a radiance INTEGRATED over each
// pixel's solid angle, so the anchor alone never let a caller reproduce the renderer's own
// relative-mode scale — the conversion factor was known only inside core. It is published here,
// which makes the promise this header already made about `emitted_energy` ("a consumer can
// reproduce that scale") true for the relative branch too. NOT a size change: it lands in the
// four bytes of tail padding `anchor_l99_sky` left behind, so sizeof stays 72 and every existing
// offset is unchanged. A caller that ignores it is unaffected; a caller that mirrors the struct
// should still add the field so its own padding matches.
//
// BREAKING (v4.22): LUMICE_RawXyzResult gains a trailing `anchor_l99_sky` field — the
// session's exposure anchor, measured on a fixed full-sky buffer instead of on the
// renderer's own output. It is APPENDED after `epoch`, and unlike `emitted_energy` (which
// fitted into pre-existing alignment padding) there is no spare room left, so this one
// GROWS the struct: 64 -> 72 bytes. A caller that was NOT recompiled hands the API a
// shorter buffer and LUMICE_FrameGetRawXyz writes past the end of it. Recompile against
// this header, and update any mirrored struct definition (ctypes, FFI) in lockstep.
// No behavior change accompanies it: nothing in the renderer reads the new field yet.
//
// BREAKING (v4.24): the annotation overlay generalizes its single hardcoded zenith/nadir pair into
// a list of NAMED reference directions. New symbols: LUMICE_AnnotationMarkerPoint, the
// LUMICE_ANNOTATION_MARKER_* id family with LUMICE_ANNOTATION_MARKER_COUNT and
// LUMICE_MAX_ANNOTATION_MARKERS, LUMICE_AnnotationRequest::marker_ids / marker_count, and
// LUMICE_AnnotationOverlay::marker_points / marker_count.
// Every one of them is APPENDED at the PHYSICAL end of its struct — after `want_labels` in the
// request, after `storage` in the overlay — not at the position semantics would suggest (beside
// zenith_px/zenith_valid). That is the whole compatibility strategy and the reason the overlay's
// new fields read out of order: both structs already carry published fields, so ANY insertion
// would move offsets that compiled callers hold. sizeof() grows for both; a caller that was NOT
// recompiled hands the API a shorter struct and the new fields are read or written past the end of
// it. Recompile against this header.
// Nothing is removed and nothing changes meaning: `zenith_nadir`, zenith_px/py/valid and
// nadir_px/py/valid all stay, with the behavior they had. They are no longer a separate
// IMPLEMENTATION, though — that field and MARKER_ZENITH / MARKER_NADIR now run through one
// direction table and one sampler inside core, so the two routes return the same points by
// construction. A caller that only zero-initializes its request (marker_count = 0) sees no change
// at all, which is why the GUI and the CLI needed no edit for this version.
//
// BREAKING (v4.25): LUMICE_RenderParam gains a trailing block of four fields — `markers` (an array
// of the new LUMICE_MarkerStyle, capacity LUMICE_MAX_CONFIG_MARKERS), `markers_count`,
// `markers_opacity` and `markers_radius_px`. They are APPENDED after `angular_dist_label`, so every
// existing field keeps its offset while sizeof() grows; a caller that was NOT recompiled hands the
// API a shorter struct and the new fields are read past the end of it. Recompile against this
// header. The JSON keys are "grid.markers" (an array of {"id", "enabled", "color"} objects),
// "grid.markers_opacity" and "grid.markers_radius_px".
// This is the RENDERER half of what v4.24 did to the annotation overlay: v4.24 let a caller ASK
// where six named directions land, this one lets a config say which of them to DRAW and in what
// colour. `id` is spelled as the same LUMICE_ANNOTATION_MARKER_* value in both, and as a string in
// JSON ("zenith" / "nadir" / "sun" / "subsun" / "anthelion" / "antisolar").
// Nothing is removed and nothing changes meaning: `zenith_nadir` and its three appearance fields
// stay, and a config that carries only them renders the pixels it always did. Where both are
// present, the non-empty `markers` list wins and `zenith_nadir` is ignored — the full statement of
// that rule, and why it lives in the renderer rather than in either JSON decoder, is at the
// `markers` field itself.
// Note the accompanying BEHAVIOR change, which has no ABI half: the CLI renderer's marker stage was
// two hardcoded rings computed ONCE at consumer construction; it is now an N-entry loop, and the
// positions are recomputed on ResetWith as well. That second half is not tidiness — four of the six
// directions are defined relative to the sun, and the sun is exactly what ResetWith can change.
// A caller that leaves markers_count at 0 sees no change at all.
//
// BREAKING (v4.26): LUMICE_RenderParam gains a trailing block of three fields — `elevation_line`,
// `longitude_line` and `angular_dist_line`, whether the CLI renderer draws each of those three
// families' LINES. They are APPENDED after `markers_radius_px`, so every existing field keeps its
// offset while sizeof() grows; a caller that was NOT recompiled hands the API a shorter struct and
// the new fields are read past the end of it. Recompile against this header. The JSON keys are
// "grid.elevation_line", "grid.longitude_line" and "grid.angular_dist_line".
// This completes what v4.21 started. That version made the TEXT independent of the LINE at the
// geometry layer, but only the horizon could actually be asked for text without its line: it was
// the one family with a line switch (`horizon`) separate from its angle list. For the other three,
// "draw this family" WAS "is the angle list non-empty", so a producer wanting their numbers had to
// fill the list and get the lines with it. The GUI's exporter took the other horn and dropped the
// numbers (src/gui/file_io.cpp), which is the defect this closes.
// DEFAULT TRUE on the JSON side, unlike every other annotation flag in this struct — see the
// WARNING at the fields themselves. Nothing is removed and nothing changes meaning: a config that
// carries none of the three keys renders the pixels it always did.
//
// BREAKING (v4.27): LUMICE_RenderParam gains a trailing pair — `tone` (LUMICE_TONE_*, which
// operator turns accumulated radiance into pixels) and `paper` (the ground colour that operator
// lays ink on). They are APPENDED after `angular_dist_line`, so every existing field keeps its
// offset while sizeof() grows; a caller that was NOT recompiled hands the API a shorter struct and
// the new fields are read past the end of it. Recompile against this header. The JSON keys are
// "render.tone" and "render.paper".
// Nothing is removed and nothing changes meaning: LUMICE_TONE_SCREEN == 0 is the operator this API
// has always used, so a zero-initialized `tone` asks for the existing behaviour.
// `paper` does NOT follow that pattern and is the second trap of the `zenith_nadir` kind in this
// struct: its JSON default is WHITE, so a zero-initialized struct names BLACK paper. Under the
// subtractive operator black paper renders an all-black page, since out = paper * 10^(-D). Set it,
// or go through JSON. The reason `paper` is a field of its own rather than a reuse of `background`
// is exactly this degenerate state — see doc/print-mode-subtractive-ink.md decision D5.
// Both fields are LIVE: LUMICE_TONE_PRINT selects the subtractive operator in the renderer
// (src/server/render.cpp's PostSnapshot, through src/util/ink_transfer.hpp) and in the GUI's preview
// shader, and `paper` is the ground it lays ink on.
//
// BREAKING (v4.28): the annotation overlay query no longer returns rasterized masks. REMOVED:
// LUMICE_ComputeAnnotationOverlay, LUMICE_ReleaseAnnotationOverlay, the LUMICE_AnnotationOverlay
// struct (its `drawable` / `horizon` / `elevation` / `longitude` / `angular_dist` masks and its
// `zenith_*` / `nadir_*` fields with it), and the `zenith_nadir` and `want_labels` fields of
// LUMICE_AnnotationRequest — the struct's layout changes, so every caller recompiles. ADDED:
// LUMICE_ComputeAnnotationAnchors / LUMICE_ReleaseAnnotationAnchors and the LUMICE_AnnotationAnchors
// struct, which carry the label anchors and the marker points the old result also carried, and
// nothing proportional to the canvas.
// The reason is a change of contract, not of shape. The old call cost a width*height
// inverse-projection sweep and was documented as "not a per-frame call"; its one interactive
// consumer answered by freezing every annotation for the duration of a camera drag. The curves are
// level sets of three world-space angle fields, which a fragment shader evaluates for free from the
// direction it already has — so a consumer that re-projects per frame draws the curves itself, from
// the definition, and asks core only where the text and the points go. The new call is designed to
// be made every frame: it runs the curve walk and the point sampling alone, tens of microseconds,
// and there is no field in it a caller could accidentally scale with the canvas.
// The CLI renderer is unaffected: it composites through core's C++ ComputeOverlay in-process, and
// still bakes the same masks into its finished image. LUMICE_AnnotationLabel,
// LUMICE_AnnotationMarkerPoint, LUMICE_AnnotationView, the LUMICE_ANNOTATION_* constants and the
// error contract are unchanged. `zenith_nadir` goes because `marker_ids` superseded it in v4.24 and
// nothing in this tree set it; `want_labels` goes because the anchors are all this call computes.
//
// BREAKING (v4.29): the analysis run reaches the C API. ADDED, nothing removed or reordered:
// the "Raypath Analysis Run" section (LUMICE_RaypathAnalysisRequest, LUMICE_RaypathChainSegment,
// LUMICE_RaypathHistogramEntry, LUMICE_RaypathAnalysisInfo, their LUMICE_RAYPATH_* / LUMICE_MAX_RAYPATH_*
// constants, LUMICE_StartRaypathAnalysis, LUMICE_FrameGetRaypathAnalysisInfo,
// LUMICE_FrameGetRaypathAnalysis, LUMICE_UnprojectPixel) and LUMICE_GetActiveBackend beside
// LUMICE_SetPreferredBackend. A caller compiled against v4.28 keeps every offset it holds; the bump
// records that the exported symbol set grew, per the rule at the top of this block.
// Behaviour that comes with it, and has no ABI half: a server has TWO kinds of run now. A render
// run (LUMICE_CommitScene) and an analysis run (LUMICE_StartRaypathAnalysis) share one lifecycle —
// the same LUMICE_GetSimLifecycle / LUMICE_GetDrainStatus / LUMICE_AcquireResultFrame — and
// exclude each other: starting one while the other is in progress returns LUMICE_ERR_SERVER
// rather than interrupting it. An analysis run always traces on the CPU, whatever
// LUMICE_SetPreferredBackend or LUMICE_TRACE_BACKEND says; LUMICE_GetActiveBackend is the readable
// form of that. A frame acquired during an analysis run has no render / raw-XYZ rows (their getters
// write their sentinel at out[0]) and carries the histogram instead; the next LUMICE_CommitScene
// switches back, and its frames carry no histogram.
//
// v4.30: LUMICE_RaypathAnalysisInfo gains a trailing `snapshot_generation` — APPENDED, sizeof
// grows (20 -> 32), recompile. It is the same counter LUMICE_RawXyzResult::snapshot_generation
// carries, exported on the analysis side because an analysis frame has no raw-XYZ row to read it
// off. Without it a consumer had no way to tell "a new histogram arrived" from "the same result
// observed again": `present` is a property of the frame, true on every poll for as long as the
// result is held, so a GUI keying its list refresh (and the selection it clears with it) on
// `present` would clear the user's selection every frame. Nothing else moved.
//
// v4.31: LUMICE_ProjectDirection — ADDED, nothing removed or reordered; the exported symbol set
// grew, per the rule at the top of this block. It is the forward half LUMICE_UnprojectPixel is the
// inverse of, on a direction the caller holds rather than one of the six named marker ids
// LUMICE_ComputeAnnotationAnchors projects: the same sampler as those markers (projection, canvas
// clamp, half-degree hemisphere slack), so a consumer that keeps a direction of its own — the GUI
// keeps its analysis cone centre as one — can place it on the picture every frame and have it move
// with the view exactly as the zenith or the sun marker does. Zero allocation, no storage handle:
// it is designed to be called per frame from a hover test.
//
// BREAKING (v4.32): LUMICE_RaypathAnalysisRequest gains a trailing `infinite` / `ray_num` pair —
// APPENDED, sizeof grows (88 -> 96), recompile. An analysis run now carries its OWN ray budget
// instead of tracing the committed scene's `ray_num` (LUMICE_SceneSetSimParams) — a GUI can let
// the user trade a quicker answer against a fuller histogram without touching the document, and a
// CLI caller can size the run for the question asked. `infinite` set to
// LUMICE_RAYPATH_RAY_BUDGET_SCENE_DEFAULT keeps the pre-v4.32 behaviour (the scene's own budget);
// it is a sentinel outside the boolean domain for the same reason the request's symmetry default
// (LUMICE_RAYPATH_SYMMETRY_SESSION_DEFAULT, removed in v4.33) sat outside 0..7 — a zero-initialized
// request asks for zero rays, not for the default, so a caller that wants the default says so.
// Nothing else moved.
//
// BREAKING (v4.33): symmetry moves from the analysis REQUEST to the analysis READ. Three changes.
// (1) LUMICE_RaypathAnalysisRequest loses `chain_id_symmetry` and the header loses
// LUMICE_RAYPATH_SYMMETRY_SESSION_DEFAULT — `infinite` moves up (offset 84 -> 80; sizeof stays 96,
// the vacated int becoming padding before `ray_num`), recompile: a v4.32 caller's `infinite`
// would land in that padding and its request would ask for zero rays. A run now records every
// chain at its FINEST (no reduction at all) — the
// reduction is no longer a property of the run. (2) LUMICE_FrameGetRaypathAnalysisInfo and
// LUMICE_FrameGetRaypathAnalysis each take a `chain_id_symmetry` (a P/B/D bit set 0..7, else
// LUMICE_ERR_INVALID_VALUE): the entries of a frame are reduced under it and merged AT READ TIME,
// on the server, so a consumer can show the same finished result under any symmetry without
// re-running — and the two calls must be given the SAME value, since `entry_count` is the merged
// row count under that symmetry. (3) `display` changes format (below): "3-5" for a single-
// crystal layer, "C1(3-5)" where the layer holds several crystals, layers joined by " -> ". The
// old "crystal1(3-5)" text was the interning table's diagnostic form; it never leaves core now.
// LUMICE_RaypathHistogramEntry's layout is unchanged (LUMICE_RAYPATH_DISPLAY_MAX still covers the
// longest text the new format can need — its derivation is at the constant).
//
// BREAKING (v4.34): LUMICE_RaypathAnalysisRequest loses its cone stop target (the LUMICE_RayCount
// that followed `cone_ring_count`) and with it the cone early stop as a mechanism — `infinite`
// moves up (offset 80 -> 72), `ray_num` moves up (88 -> 80), sizeof shrinks (96 -> 88),
// recompile: a v4.33 caller's `infinite` would land in the new
// trailing padding and its request would ask for zero rays. An analysis run's length is now
// decided by exactly two things in every ROI mode: its own ray budget (`infinite` / `ray_num`) and
// LUMICE_StopServer. A run stopped that way keeps what it accumulated: the frame published after
// LUMICE_StopServer returns carries the histogram consumed up to the stop (before v4.34 a stop
// could discard the batches consumed since the last poll, leaving the pre-stop frame in place).
// Nothing else moved.
//
// BREAKING (v4.35): the analysis record is BOUNDED, and says so. Two structs grow at the tail,
// recompile. LUMICE_RaypathHistogramEntry gains `error_bound` (sizeof 5600 -> 5608):
// the server keeps a fixed number of rows (Space-Saving), and a row that took over an evicted
// row's slot carries that row's energy as its own uncertainty — the chain's true energy is
// within [energy - error_bound, energy]; 0 for a row that never took a slot over, which every
// row of a run that fit is. LUMICE_RaypathAnalysisInfo gains `other_energy` / `other_count` /
// `truncated_chain_count` / `max_row_error` (sizeof 32 -> 64): the rays whose chain the
// producer's interning table had no room for, as one bucket that is not a row (so Σ entries +
// other is every counted ray, at every symmetry); how many times a chain hit that full record;
// and the largest `error_bound` over the frame's rows (0 = no eviction happened, the record
// is exact). Nothing is removed or reordered; a run that never overflows either bound reads
// exactly as it did in v4.34, with the four new fields 0.
//
// BREAKING (v4.36): LUMICE_StartRaypathAnalysis takes the scene to analyse, as a LUMICE_Scene
// between `server` and `request` — the same handle LUMICE_CommitScene takes — and needs no
// LUMICE_CommitScene before it. Recompile: a v4.35 caller's `request` pointer would be read as the
// scene. Semantics: the analysis is a submission of its own document. It advances the lifecycle
// epoch as a commit does, so a frame of it is never mistaken for the last render's; it does not
// touch the render's committed config, so the LUMICE_CommitScene after it judges consumer reuse
// against the last render, exactly as it would have with no analysis in between. The
// "no scene committed" rejection (LUMICE_ERR_INVALID_CONFIG) is gone with the requirement; a
// scene the server cannot use is rejected with the code LUMICE_CommitScene would give it.
//
// BREAKING (v4.37): LUMICE_SimLifecycleResult gains `session_kind` (sizeof 16 -> 24), one of
// LUMICE_SessionKind: which kind of run the CURRENT session is — LUMICE_SESSION_RENDER after
// every LUMICE_CommitScene and before any run, LUMICE_SESSION_ANALYSIS from
// LUMICE_StartRaypathAnalysis until the next commit (a Stop does not reset it). Pure append,
// nothing removed or reordered; a value-initialised struct reads 0 = RENDER, which is also the
// server's own default. Recompile: a v4.36 caller's struct is 8 bytes short of the write.
// Why it exists: LUMICE_CommitScene's consumer-reuse decision refuses to reuse the previous
// session's consumers when that session was an analysis, and a client that predicts the
// decision (the GUI does, to know whether to stop its poller first) had no way to read that
// term — it could only keep a shadow flag of its own, a second copy of the server's judgement.
// This field is the server's copy, read back. Read it BEFORE the commit whose decision you
// are predicting: the commit itself is what flips it back to RENDER.
//
// ADDED (v4.38): LUMICE_SceneSetRayAllocation, a pure append. scene.ray_allocation ("proportional"
// | "adaptive") used to reach a scene handle only through LUMICE_SceneFromJson / FromJsonFile; a
// programmatic author (the GUI's Simulation panel) now sets it with LUMICE_RAY_ALLOCATION_* the
// way LUMICE_SceneSetColorMode takes LUMICE_COLOR_MODE_*. Nothing else moved; a handle that never
// calls it still omits the key and still round-trips a document's spelling verbatim.
//
// BREAKING (v4.39): a fifth annotation family, the view-distance circles — circles of constant
// angular distance from the camera's OPTICAL AXIS, the axis-referenced twin of angular_dist (which
// is referenced to the sun). Two structs grow at the tail, recompile. LUMICE_RenderParam gains
// `view_dist[]` / `view_dist_count` / `view_dist_line` / `view_dist_label` after `paper`
// (sizeof 4904 -> 6452); LUMICE_AnnotationRequest gains `view_dist_deg` / `view_dist_count` after
// `marker_count` (sizeof 128 -> 144). LUMICE_ANNOTATION_VIEW_DIST (4) is the label kind. JSON keys
// "grid.view_dist", "grid.view_dist_line", "grid.view_dist_label", shaped exactly like the
// angular_dist three; absent keys leave the family off, so a document that predates it renders as
// it did. Nothing is removed or reordered; a zero-initialised LUMICE_RenderParam keeps the family
// off (count 0) but, like the other three line switches, names `view_dist_line` = 0 where the JSON
// default is true — go through JSON or set it.
// WHAT IS NOT THERE, deliberately: no `reference_dir_view`. The centre is the view's own forward
// (elevation / azimuth / roll), which core already derives for the front-hemisphere clip, so a
// caller has nothing to supply — and the axis is independent of `lens_shift` by construction: a
// shifted lens moves the axis's PIXEL, not the axis, and the circles follow the axis. A second
// direction field would have been a second copy of a quantity the request already determines.
//
// ADDED (v4.40): LUMICE_SceneGetRenderer, a pure append — the read-back inverse of
// LUMICE_SceneAddRenderer. Until now a scene handle could only be written to and serialized; a
// consumer that needed one renderer's resolved parameters (the CLI's `analyze --roi frame`, which
// frames its ROI on the config's render[] entry) had to re-parse the document with core's own
// parser, a second copy of the defaults. The getter returns the LUMICE_RenderParam the engine
// will use, defaults applied, and addresses entries by array index, not by `.id` — see its note
// at the declaration. Nothing else moved; no struct changed.
//
// ADDED (v4.41): LUMICE_GetEngineIsaLevel, a pure append — the ISA tier the ENGINE was compiled
// for, answered by the engine itself at run time. Until now the only carrier of that fact was a
// compile-time macro in the executable's own translation unit, which is the wrong place once the
// engine is a separately built DLL chosen at start-up: the Windows release ships one shell and
// two engine DLLs (baseline / x86-64-v3), and a shell that reports its own macro reports the tier
// it was configured with, never the tier it loaded — measured on the reference box as a v3 engine
// reporting "baseline". Nothing else moved; no struct changed.
//
// ADDED (v4.42): LUMICE_GetVersionString, a pure append — the product version string (CMake's
// project(VERSION), the single source scripts/version.py also reads and writes), answered by the
// engine at run time so the CLI's `--version`, the GUI title bar, both startup log lines and the
// .lmc `app_version` field all read one shared value instead of each carrying its own copy. A
// non-tagged build (LUMICE_RELEASE_BUILD=OFF, the CMake default) appends "-dev" so a locally
// built binary is never mistaken for a tagged release; release.yml sets it ON. Nothing else
// moved; no struct changed.
//
// BREAKING (v4.43): LUMICE_RaypathAnalysisRequest gains a trailing `chain_capacity` — APPENDED
// after `ray_num`, sizeof grows (88 -> 96: the int lands at 88 and the struct's 8-byte alignment
// pads it to 96), recompile. An analysis run can now size its own chain record: the number of
// distinct finest chains the run keeps exact, which until now was one compiled-in constant
// (16384, calibrated on the GUI's multi-scatter reference scenes) that a single-crystal
// `--symmetry none` scene with more distinct chains than that (a hexagonal prism's 8-face set
// alone is 29 212) overflowed into the "other" bucket with no way to ask for more. `0` keeps the
// pre-v4.43 behaviour (the default capacity) — zero is the "unset" spelling here, unlike
// `infinite`'s sentinel, because a zero-capacity record has no honest meaning to preserve — so a
// zero-initialized request, which is what the GUI sends, is byte-for-byte the old request. The
// same value sizes both halves of the record (the per-worker interning table and the server's
// histogram); see LUMICE_MAX_RAYPATH_CHAIN_CAPACITY for the bound and the memory it buys.
// Nothing else moved.
//
// ADDED (v4.44): LUMICE_ContinueRender, a pure append — trace more rays INTO the accumulation the
// last render run left behind, instead of starting over. Until now every run start was a reset:
// LUMICE_CommitScene clears the render planes, the ray/energy totals and the exposure anchor even
// when it reuses the consumers, so a user who wanted a less noisy picture of a finished run could
// only throw it away and trace a bigger one from zero. No struct changed. One documented meaning
// widened: the lifecycle epoch now advances on EVERY run start — commit, analysis, and this
// continuation — rather than on reset-causing commits only (see LUMICE_SimLifecycleResult.epoch).
// A continuation is a new run as far as the drain signal and frame freshness are concerned; had
// it kept the epoch, LUMICE_GetDrainStatus would report the previous run's "drained" from its
// first instant.
//
// BREAKING (v4.45): LUMICE_RenderParam gains a trailing `globe_back_fade` — APPENDED after
// `view_dist_label`, sizeof grows (6452 -> 6456), recompile. The globe lens can now show the far
// side of its sphere, faded with distance from the camera; 0, the zero-initialized value, is the
// camera-facing hemisphere alone, i.e. the image every earlier version drew. Nothing else moved.
//
// BREAKING (v4.46): LUMICE_RenderParam gains a trailing `display_mode` (LUMICE_DISPLAY_MODE_*) —
// APPENDED after `globe_back_fade`, so every existing field keeps its offset while sizeof() grows
// (6456 -> 6460); a caller that was NOT recompiled hands the API a shorter struct and the new field
// is read past the end of it. Recompile against this header. The JSON key is "render.display_mode"
// ("normal" / "channel_br"). It selects what the finished screen image is SHOWN as: the image
// itself, or the channel-B-R diagnostic — the post-gamma sRGB B - R of each pixel as a grey offset
// (formula in src/util/channel_math.hpp). LIVE in the renderer (PostSnapshot) and in the GUI's
// preview shader; inert under LUMICE_TONE_PRINT, and a colour-classed scene produces no raypath
// composite while it is on. Nothing is removed and nothing changes meaning:
// LUMICE_DISPLAY_MODE_NORMAL == 0, so a zero-initialized struct asks for the existing picture.
//
// ADDED (v4.47): LUMICE_GetCrystalSymmetry + LUMICE_CrystalSymmetry, a pure append — which of the
// P/B/D symmetry elements a crystal's SHAPE really has, and whether D acts on it at all. Raypath
// reduction (filters' symmetry, the analysis list's merge) used to assume every prism and pyramid
// is a regular hexagon; it now intersects the request with the crystal's shape, so D can be
// dropped for a reason LUMICE_IsDApplicable (axis only) cannot see. No struct changed and
// LUMICE_IsDApplicable keeps its meaning.
//
// BREAKING (v4.48): LUMICE_CrystalSymmetry gains trailing `p_effective` and `b_effective` —
// APPENDED after `d_effective`, so every existing field keeps its offset while sizeof() grows
// (16 -> 24); LUMICE_GetCrystalSymmetry writes the whole struct, so a caller that was NOT
// recompiled hands it a shorter one and the two new fields land past its end. Recompile against
// this header. ADDED alongside: LUMICE_CouldCrystalHaveFace (whether a filter naming a face can
// ever match through it on this crystal), and LUMICE_IsPApplicable and LUMICE_IsBApplicable, the axis halves of
// P and B (the counterparts of LUMICE_IsDApplicable). The engine used to apply P and B whatever the
// crystal's orientation distribution; it now applies P only when roll is invariant under a 60°
// shift and B only when the zenith is symmetric about 90° with a uniform azimuth, so a Parry arc no
// longer merges 3-5 with 4-6 and a plate no longer merges 1-3 with 2-3. Nothing is removed.
//
// ADDED (v4.49): LUMICE_CouldFilterMatchFace, a pure append — LUMICE_CouldCrystalHaveFace with the
// filter's own P/B/D taken into account — and LUMICE_ExpandRaypathClass with its
// LUMICE_SYMMETRY_SEMANTICS_* / LUMICE_MAX_RAYPATH_CLASS_MEMBERS constants. BEHAVIOR (v4.49): a FILTER's P/B/D is a
// label equivalence again, as it was up to v4.46: P is the label six-fold rotation and B the label horizontal mirror
// whatever the crystal's shape and orientation distribution, D reads the roll mean as before. The
// narrowing v4.47 and v4.48 describe above now applies to the raypath-analysis list's grouping
// only (one row = one physical class); LUMICE_GetCrystalSymmetry and LUMICE_IsPApplicable /
// LUMICE_IsBApplicable keep their meaning and serve that grouping and the filter editor's hints.
// No struct changed.
//
// ADDED (v4.50): single-path analysis, a pure append — LUMICE_SinglePathRequest,
// LUMICE_SinglePathResult (opaque), LUMICE_AnalyzeSinglePath, LUMICE_SinglePathResultToJson and
// LUMICE_SinglePathResultDestroy: the fiber, per-pose detail and sun-direction sphere of ONE
// single-layer raypath of one crystal entry over one sky point (doc/raypath-analysis.md section
// 5.1.8; the `Lumice raypath` subcommand; output fields in doc/raypath-cli-output.md). No existing
// symbol or struct changed. ADDED alongside: LUMICE_SINGLE_PATH_MAX_SAMPLE_COUNT.
// BEHAVIOR (v4.50): LUMICE_SetLogLevel accepts a NULL server and then
// sets the engine-wide log level (it used to do nothing), since a single-path caller has no server.
//
// ADDED (v4.51): target-free path feature reports, a pure append —
// LUMICE_PathFeatureReportRequest, LUMICE_PathFeatureReport (opaque),
// LUMICE_AnalyzePathFeatureReport, LUMICE_PathFeatureReportToJson and
// LUMICE_PathFeatureReportDestroy, plus their sample/wavelength bounds. The new JSON schema is
// separate from the unchanged v4.50 target-fiber schema.
// ADDED (v4.52): report budget/observation/layer request suffix. Target-free reports now use
// schema 2 and the actual product spectrum/measure; old target/fiber/warm schema stays 1.
#define LUMICE_API_VERSION 452
#define LUMICE_MAX_RENDER_RESULTS 16
#define LUMICE_MAX_STATS_RESULTS 1

// =============== Opaque Types ===============
typedef struct LUMICE_Server_ LUMICE_Server;
// Opaque, incrementally-built scene container. See the "Scene (opaque handle)" section of
// lumice_scene.h for its lifecycle + Add*/Set* build API. Declared here alongside the other opaque
// handle types; the build functions live after the leaf POD structs they consume.
typedef struct LUMICE_Scene_ LUMICE_Scene;
// Opaque, immutable, reference-counted snapshot of ALL results (mono render, composite,
// raw XYZ, stats) as of one server-side snapshot. Acquiring one gives the caller a real
// share of the data's lifetime: every buffer reachable through it stays valid until the
// caller releases the frame, no matter what the server does meanwhile. See the "Results"
// section of lumice_engine.h for the acquire/release + read functions.
typedef struct LUMICE_ResultFrame_ LUMICE_ResultFrame;

// =============== Error Codes ===============
typedef enum LUMICE_ErrorCode_ {
  LUMICE_OK = 0,
  LUMICE_ERR_NULL_ARG,
  LUMICE_ERR_INVALID_JSON,
  LUMICE_ERR_INVALID_CONFIG,
  LUMICE_ERR_MISSING_FIELD,
  LUMICE_ERR_INVALID_VALUE,
  LUMICE_ERR_FILE_NOT_FOUND,
  LUMICE_ERR_SERVER,
  LUMICE_ERR_UNKNOWN,
} LUMICE_ErrorCode;

// =============== Log Levels ===============
typedef enum LUMICE_LogLevel_ {
  LUMICE_LOG_TRACE,
  LUMICE_LOG_DEBUG,
  LUMICE_LOG_VERBOSE,
  LUMICE_LOG_INFO,
  LUMICE_LOG_WARNING,
  LUMICE_LOG_ERROR,
  LUMICE_LOG_OFF,
} LUMICE_LogLevel;

// =============== Ray Count Type ===============
// 64-bit count type for ray / ray-segment / crystal totals. Must stay >= 64-bit:
// `unsigned long` is only 32-bit on Windows (LLP64), which silently truncated
// totals above 2^32 (the status-bar ray-count rollover reported by Windows users).
// Used for every field that carries an actual ray-count value across the C API.
typedef unsigned long long LUMICE_RayCount;
#if defined(__cplusplus)
static_assert(sizeof(LUMICE_RayCount) >= 8, "LUMICE_RayCount must be 64-bit (Windows unsigned long is 32-bit)");
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(LUMICE_RayCount) >= 8, "LUMICE_RayCount must be 64-bit (Windows unsigned long is 32-bit)");
#endif

// =============== Logging ===============
// Sets `server`'s loggers and the engine-wide (global) logger to `level`. `server` may be NULL
// (v4.50): then only the engine-wide level is set — for a caller that uses no server at all, such
// as one that only calls LUMICE_AnalyzeSinglePath.
LUMICE_API void LUMICE_SetLogLevel(LUMICE_Server* server, LUMICE_LogLevel level);

// Log callback: receives all Core log messages. Called from Core logging threads.
// Parameters: level, logger name (e.g. "Server", "Simulator"), pre-formatted message.
// The callback must be thread-safe.
typedef void (*LUMICE_LogCallback)(LUMICE_LogLevel level, const char* logger_name, const char* message);

// Register a log callback. Pass NULL to disable. Must be called BEFORE LUMICE_CreateServer()
// for full coverage, but can also be called later (subsequent log messages will be forwarded).
LUMICE_API void LUMICE_SetLogCallback(LUMICE_LogCallback callback);

// =============== Product Version ===============
// The product version string: "X.Y.Z" for a tagged release build (LUMICE_RELEASE_BUILD=ON) or
// "X.Y.Z-dev" otherwise. Single source: project(VERSION) in the top-level CMakeLists.txt,
// generated into a build-tree-only header by configure_file and read back here — the CLI's
// `--version`, the GUI window title, both startup log lines and the .lmc `app_version` field all
// call this instead of carrying their own copy. Never NULL; static storage, do not free.
LUMICE_API const char* LUMICE_GetVersionString(void);

// =============== Engine Build Provenance ===============
// The ISA tier this engine was compiled for: "baseline", "x86-64-v3", "x86-64-v4" or "native".
// Never NULL; static storage, do not free. The value is the tier name only when the matching
// -march flag was actually applied to the engine's objects, and "baseline" otherwise — so a Debug
// or MinSizeRel build, and every build by real MSVC cl.exe (which has no equivalent flag),
// answers "baseline" no matter what was asked of the configure. It is the engine's own answer:
// in a build where the engine is a shared library, it is the tier of the library that this
// process actually loaded, which the executable's own compile cannot know. The CLI's
// `[BENCHMARK]` JSON `isa` key is this string.
LUMICE_API const char* LUMICE_GetEngineIsaLevel(void);

#ifdef __cplusplus
}
#endif

#endif  // LUMICE_BASE_H_
