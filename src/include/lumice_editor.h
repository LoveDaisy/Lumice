#ifndef LUMICE_EDITOR_H_
#define LUMICE_EDITOR_H_

// Editor support: the crystal mesh for a preview, which faces and shape / axis scalars a crystal
// has, symmetry applicability, raypath text expansion and validation, and Miller-index conversion.

#include "lumice_base.h"
#include "lumice_scene.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============== Crystal Mesh ===============
// Get crystal wireframe mesh for 3D preview.
// Caller allocates LUMICE_CrystalMesh on stack, Core fills vertex/edge data.

#define LUMICE_MAX_CRYSTAL_VERTICES 128
#define LUMICE_MAX_CRYSTAL_EDGES 256
#define LUMICE_MAX_CRYSTAL_TRIANGLES 128
#define LUMICE_MAX_CRYSTAL_FACES 24
#define LUMICE_MAX_CRYSTAL_FACE_VTXPOOL 192

typedef struct LUMICE_CrystalMesh_ {
  float vertices[LUMICE_MAX_CRYSTAL_VERTICES * 3];  // [x0,y0,z0, x1,y1,z1, ...]
  int vertex_count;
  int edges[LUMICE_MAX_CRYSTAL_EDGES * 2];  // [v0,v1, v2,v3, ...] vertex index pairs
  int edge_count;
  int triangles[LUMICE_MAX_CRYSTAL_TRIANGLES * 3];  // [v0,v1,v2, ...] for surface rendering
  int triangle_count;
  // Per-edge adjacent face normals for back-face culling.
  // Edge i has two face normals: [i*6..i*6+2] and [i*6+3..i*6+5].
  // Boundary edges store the same normal twice.
  float edge_face_normals[LUMICE_MAX_CRYSTAL_EDGES * 6];
  // Per-triangle face number (matches raypath filter numbering convention):
  //   basal = 1/2; prism = 3..8; upper pyramidal = 13..18; lower = 23..28.
  // -1 for unrecognized orientations (kInvalidId in core).
  int face_numbers[LUMICE_MAX_CRYSTAL_TRIANGLES];
  // Per-face polygon topology (CCW ordered vertex indices when viewed from outside).
  // face_vtx_pool[face_vtx_offsets[i] .. face_vtx_offsets[i]+face_vtx_counts[i]-1]
  // gives the CCW vertex indices for face i. face_count=0 means not populated.
  int face_count;
  int face_numbers_by_face[LUMICE_MAX_CRYSTAL_FACES];
  int face_vtx_offsets[LUMICE_MAX_CRYSTAL_FACES];
  int face_vtx_counts[LUMICE_MAX_CRYSTAL_FACES];
  int face_vtx_pool[LUMICE_MAX_CRYSTAL_FACE_VTXPOOL];
  // Area-weighted unit-length face normals, lockstep with face_numbers_by_face /
  // face_vtx_offsets / face_vtx_counts: slot [i*3..i*3+2] is the unit normal of
  // face i for i in [0, face_count). Slots beyond face_count are unspecified.
  float face_normals[LUMICE_MAX_CRYSTAL_FACES * 3];
} LUMICE_CrystalMesh;

// Sample one concrete crystal shape from `crystal`'s distributions and build its
// preview mesh. Preview and simulation share the SAME LUMICE_CrystalParam, so there
// is no stringify step and no precision divergence between what is previewed and what
// is simulated. Sampling runs through the core single-source sampler (ns::MakeCrystal),
// so Gaussian/Laplacian/zigzag/uniform semantics and the ring-0 negative-d policy are
// never re-implemented on the GUI side.
//
// Contract:
//   - Deterministic: identical `crystal` + identical `sample_seed` => bit-identical
//     `out` mesh (the basis for T5 preview-animation / thumbnail determinism).
//   - `sample_seed` is a NO-OP for a fully non-random crystal (every shape field
//     LUMICE_DIST_NO_RANDOM): MakeCrystal never touches the RNG, so any seed yields the
//     same mesh (the basis for "preview holds still when std=0").
//   - Degenerate input never crashes: a randomized crystal may sample a shape the
//     closed-form validation gate rejects; that yields an empty-but-valid mesh
//     (all *_count == 0) and LUMICE_OK, never a SIGSEGV.
//   - The mesh is in the crystal's LOCAL frame; the axis distributions
//     (zenith/azimuth/roll) are NOT consumed here (orientation is applied at render time).
//
// Returns LUMICE_ERR_NULL_ARG if `crystal` or `out` is NULL; LUMICE_ERR_INVALID_VALUE
// for an unknown crystal->type; LUMICE_ERR_INVALID_CONFIG if the shape cannot be parsed.
LUMICE_API LUMICE_ErrorCode LUMICE_GetCrystalMesh(const LUMICE_CrystalParam* crystal, unsigned long long sample_seed,
                                                  LUMICE_CrystalMesh* out);

// =============== Config ID Range ===============
// Maximum value for LUMICE config IDs (matches core IdType = uint16_t max).
// GUI code should clamp user-editable IDs to [0, LUMICE_MAX_ID].
#define LUMICE_MAX_ID 65535

// =============== Crystal Kind ===============
// Coarse crystal classification used for raypath face-number validation.
// GUI uses this to determine which face numbers are legal for a given crystal.
// A value matching neither enumerator is REJECTED, not guessed at: every entry taking this type
// (LUMICE_IsLegalFace, LUMICE_IsShapeScalarApplicable, LUMICE_ShapeScalarSyncKeyName) answers its
// own negative — 0, 0, and NULL respectively. Note this is the same answer those functions give
// for a legitimate kind paired with an out-of-range face/slot, so it says "no", not "you passed
// garbage"; a caller computing a kind rather than writing a literal still gains nothing by
// skipping its own validation.
// This replaced an earlier lenient contract under which such a value fell through to
// LUMICE_CRYSTAL_PYRAMID. The fall-through was documented and deliberate, but a caller census
// found nothing relying on it, and it had a second cost the reject does not: it silently absorbed
// enum expansion too. The implementations now switch over the enumerators with no `default:`
// label, so adding a third kind warns (-Wswitch) at each site instead of quietly mapping it onto
// Pyramid.
typedef enum LUMICE_CrystalKind_ {
  LUMICE_CRYSTAL_PRISM,    // Basal + prism lateral faces (1,2,3-8)
  LUMICE_CRYSTAL_PYRAMID,  // All faces including upper/lower pyramidal (1,2,3-8,13-18,23-28)
} LUMICE_CrystalKind;

// Returns non-zero if `face` is a legal face number for the given crystal kind.
LUMICE_API int LUMICE_IsLegalFace(LUMICE_CrystalKind kind, int face);

// Returns non-zero if shape-scalar `slot` (a LUMICE_SHAPE_SCALAR_* index) physically exists on
// this crystal kind: a prism has .height + the six .face_distance, a pyramid has
// .upper_h/.prism_h/.lower_h + the six .face_distance. Out-of-range slots answer zero.
//
// This is core's own applicability table, not a second copy of it — the same one canonicalization
// scopes itself by when it zeroes a group declared on a slot the type does not have. Ask it rather
// than reimplementing the rule: a GUI-side copy that drifted from core is what once made the
// crystal table display a distribution the simulation did not use.
LUMICE_API int LUMICE_IsShapeScalarApplicable(LUMICE_CrystalKind kind, int slot);

// Returns the JSON key naming shape-scalar `slot` — both inside a crystal's `shape` object and
// inside its `shape.sync_group` sub-map, which name each scalar identically. NULL when the slot
// does not apply to this kind (or is out of range). The returned string is static storage — do
// not free it.
//
// All six face slots share the one key "face_distance", whose value is a 6-element array; write it
// once, not once per face. Use this instead of spelling the key names out: they are core's schema,
// a layer that misspells one silently drops the field rather than reporting an error.
//
// The name still says "sync" for compatibility with v4.13, which shipped it: the contract has not
// changed, only the set of callers that ask.
LUMICE_API const char* LUMICE_ShapeScalarSyncKeyName(LUMICE_CrystalKind kind, int slot);

// Returns the JSON key inside a crystal's `shape` object holding a pyramidal wedge angle, in
// degrees — the upper one when `upper` is non-zero, the lower one otherwise. Never NULL; static
// storage, do not free.
//
// Takes a plain flag rather than a kind + slot pair because there is nothing for a kind to select:
// both wedge angles exist only on a pyramid, and a caller is already inside a pyramid branch
// before it needs the key. Same reason there is no LUMICE_AXIS_/LUMICE_SHAPE_SCALAR_-style index
// constant to go with it — two states, no third.
LUMICE_API const char* LUMICE_ShapeWedgeAngleKeyName(int upper);

// Returns the JSON key inside a crystal's `shape` object holding a pyramidal face's Miller
// indices — the legacy read-side spelling of the quantity LUMICE_ShapeWedgeAngleKeyName names. A
// parser converts these three indices into an angle when the explicit wedge-angle key is absent;
// write paths never emit it. Never NULL; static storage, do not free.
LUMICE_API const char* LUMICE_ShapeIndicesKeyName(int upper);

// =============== Axis Scalars ===============
// Index space for the three distributions of a crystal's `axis` object, for LUMICE_AxisScalarKeyName.
//
// Named like the LUMICE_SHAPE_SCALAR_* indices and used the same way, but the two models are NOT
// parallel: an axis has no crystal kind, hence no applicability concept — all three always exist.
// The order is the serialization order only; unlike the shape scalars it is not an RNG draw order.
#define LUMICE_AXIS_SCALAR_ZENITH 0   // .zenith
#define LUMICE_AXIS_SCALAR_AZIMUTH 1  // .azimuth
#define LUMICE_AXIS_SCALAR_ROLL 2     // .roll
#define LUMICE_AXIS_SCALAR_COUNT 3

// Returns the JSON key naming axis-scalar `slot` (a LUMICE_AXIS_SCALAR_* index) inside a crystal's
// `axis` object, or NULL when `slot` is out of range. Static storage — do not free it.
//
// Note LUMICE_AXIS_SCALAR_ZENITH names the wire quantity, which is the complement of core's
// internal latitude (zenith = 90 - latitude): the key names the file format, not the field.
LUMICE_API const char* LUMICE_AxisScalarKeyName(int slot);

// Returns non-zero when D (the sigma-d mirror) is applicable to a crystal whose axis has this
// azimuth distribution and this roll anchor. D needs the azimuth to be uniform over a full turn
// and the roll anchor to sit on a multiple of 30 degrees; an axis failing either condition has its
// D flag silently ignored by the engine.
//
// Arguments are the three raw quantities the rule reads and no others:
//   azimuth_dist_type      one of LUMICE_DIST_*, from the azimuth LUMICE_Distribution's .type
//   azimuth_full_range_deg that distribution's .spread (for LUMICE_DIST_UNIFORM, the full width)
//   roll_anchor_deg        the roll LUMICE_Distribution's .center, read type-erased -- it is a
//                          tilt offset for ZIGZAG and an interval midpoint for UNIFORM, so do not
//                          read the name as "the statistical mean of the roll angle"
//
// This is core's own predicate, not a second copy of it. Ask it rather than transcribing the rule:
// a GUI-side transcription is exactly what once let a checkbox report D as live while the engine
// had already dropped it, the two having drifted to different float tolerances (1e-3 against
// 1e-5) on a difference of 3.05e-5.
LUMICE_API int LUMICE_IsDApplicable(int azimuth_dist_type, float azimuth_full_range_deg, float roll_anchor_deg);

// Returns non-zero when P (the 60-degree rotation about the c-axis) is applicable to a crystal
// whose axis has this roll distribution (v4.48). P rotates the roll angle, so it needs roll to be
// invariant under a 60-degree shift — a uniform over a full turn. Azimuth plays no part: a Parry
// arc (uniform azimuth, locked roll) lights 3-5 and leaves its rotated image 4-6 dark. An axis
// failing the condition has its P flag ignored by the engine's reduction.
//   roll_dist_type      one of LUMICE_DIST_*, from the roll LUMICE_Distribution's .type
//   roll_full_range_deg that distribution's .spread (for LUMICE_DIST_UNIFORM, the full width)
// Core's own predicate, like LUMICE_IsDApplicable — ask it rather than transcribing the rule.
LUMICE_API int LUMICE_IsPApplicable(int roll_dist_type, float roll_full_range_deg);

// Returns non-zero when B (the horizontal mirror: basal 1<->2, upper cone <-> lower cone) is
// applicable to a crystal whose axis has this azimuth and zenith distribution (v4.48). B reverses
// the c-axis, so it needs the zenith symmetric about 90 degrees AND the azimuth invariant under a
// half turn (in practice: uniform over a full turn). A plate (zenith 0) fails — its face 1 always
// faces up; a column (zenith 90) passes. Roll plays no part.
//   azimuth_dist_type / azimuth_full_range_deg  as for LUMICE_IsDApplicable
//   zenith_dist_type    one of LUMICE_DIST_*, from the zenith LUMICE_Distribution's .type
//   zenith_center_deg   that distribution's .center, in the WIRE's zenith (not core's latitude)
//   zenith_full_range_deg that distribution's .spread
// Core's own predicate, like LUMICE_IsDApplicable — ask it rather than transcribing the rule.
LUMICE_API int LUMICE_IsBApplicable(int azimuth_dist_type, float azimuth_full_range_deg, int zenith_dist_type,
                                    float zenith_center_deg, float zenith_full_range_deg);

// Which symmetry elements a crystal's shape admits (v4.47). The raypath-analysis list's P/B/D
// merges only the elements the request names AND the crystal allows — a prism with face_distance
// [1, 1.2, 1, 1.2, 1, 1.2] has a three-fold axis, not a six-fold one, and a row must not hold a
// near-face path together with a far-face path. A FILTER's P/B/D does not read this (v4.49): it is
// a label equivalence, and these fields only tell the filter editor when ticking P/B/D merges paths
// that are not physically equivalent. Read over the crystal as a random
// ENSEMBLE: six face distances drawn i.i.d. from one distribution keep every element, because
// rotating a draw relabels it into another equally likely draw; sync groups count.
typedef struct LUMICE_CrystalSymmetry_ {
  // Smallest prism-face step (1, 2, 3 or 6) of the rotations P may use: 1 = all six (a regular
  // hexagon), 2 = three-fold, 3 = two-fold, 6 = none.
  int rotation_step;
  // Bit a (0..5) set when the vertical mirror sending prism face i to (a - i) mod 6 is a symmetry.
  int vertical_mirror_mask;
  // Non-zero when the horizontal mirror (B: basal 1<->2, upper cone <-> lower cone) is a symmetry.
  // Always non-zero for a prism; for a pyramid it needs matching cones.
  int horizontal_mirror;
  // Non-zero when D acts in the analysis list's physical grouping: the axis condition
  // LUMICE_IsDApplicable reports AND the shape having the mirror that axis selects. (A filter's D
  // acts whenever LUMICE_IsDApplicable does.)
  int d_effective;
  // Non-zero when P acts in the analysis list's physical grouping (v4.48): the axis condition
  // LUMICE_IsPApplicable reports AND rotation_step < 6 (at least one rotation besides the identity).
  int p_effective;
  // Non-zero when B acts in the analysis list's physical grouping (v4.48): the axis condition
  // LUMICE_IsBApplicable reports AND horizontal_mirror.
  int b_effective;
} LUMICE_CrystalSymmetry;

// Fills *out for `crystal` (its shape, sync groups and axis). LUMICE_ERR_NULL_ARG on a NULL
// argument; LUMICE_ERR_INVALID_VALUE on an unknown type; LUMICE_ERR_INVALID_CONFIG when the
// parameters do not describe a crystal. This is core's own derivation, the one the analysis list's
// reduction runs.
LUMICE_API LUMICE_ErrorCode LUMICE_GetCrystalSymmetry(const LUMICE_CrystalParam* crystal, LUMICE_CrystalSymmetry* out);

// Returns 0 when it is certain that no crystal drawn from `crystal` has face number `face` — its
// shape leaves that face no area (face_distance [2, 1, 2, 1, 2, 1] does that to faces 3, 5 and 7),
// so a filter naming it matches nothing through it. Non-zero otherwise, including whenever the
// answer cannot be certain (a shape scalar that is neither fixed nor uniform), for a NULL or
// unusable crystal, and for a face number not legal on the crystal's kind (LUMICE_IsLegalFace
// answers that). The same check the engine logs as a warning when a scene binds such a filter to
// such a crystal (v4.48).
LUMICE_API int LUMICE_CouldCrystalHaveFace(const LUMICE_CrystalParam* crystal, int face);

// Returns 0 when a filter on `crystal` naming face `face` with P/B/D bit set `symmetry` (1 = P,
// 2 = B, 4 = D, as LUMICE_FilterParam.symmetry) certainly matches no ray through it: neither the
// face nor any face its symmetry relabels it to (D per the crystal's axis, as the engine applies it)
// can exist, in LUMICE_CouldCrystalHaveFace's sense. "3-6" with P on face_distance
// [2, 1, 2, 1, 2, 1] answers non-zero for face 3 — P relabels it to faces 4, 6 and 8, which exist.
// Non-zero in every case LUMICE_CouldCrystalHaveFace answers non-zero. The check behind the
// engine's scene-parse warning (v4.49).
LUMICE_API int LUMICE_CouldFilterMatchFace(const LUMICE_CrystalParam* crystal, int face, int symmetry);

// The two meanings a P/B/D bit set has (v4.49). A FILTER's (and a colour ref's) is a label
// equivalence: P relabels the prism faces by any multiple of 60 degrees, B swaps 1<->2 and the
// upper and lower cones, whatever the crystal. The raypath-analysis list's merges only what is
// physically equivalent on the crystal: its shape (LUMICE_GetCrystalSymmetry) and its orientation
// distribution (LUMICE_IsPApplicable / LUMICE_IsBApplicable). D is the same in both.
#define LUMICE_SYMMETRY_SEMANTICS_LABEL 0
#define LUMICE_SYMMETRY_SEMANTICS_PHYSICAL 1
// The most distinct face sequences one P/B/D class can hold: 6 rotations x 2 mirrors x 2 flips.
#define LUMICE_MAX_RAYPATH_CLASS_MEMBERS 24

// The face sequences equivalent to `faces[0..face_count)` under P/B/D bit set `symmetry` on
// `crystal`, in the meaning `semantics` (LUMICE_SYMMETRY_SEMANTICS_*): the engine's own expansion,
// each distinct sequence once, `faces` itself first. Writes *out_member_count sequences of
// `face_count` ints each, back to back, into `out_faces`, which must hold
// LUMICE_MAX_RAYPATH_CLASS_MEMBERS * face_count ints. LUMICE_ERR_NULL_ARG on a NULL pointer;
// LUMICE_ERR_INVALID_VALUE on an unknown type or semantics, or face_count outside
// 1..LUMICE_MAX_RAYPATH_SEGMENT_LEN; LUMICE_ERR_INVALID_CONFIG when the parameters do not describe
// a crystal. The one way to tell whether an analysis row (a physical class) is also a label class
// — what "exclude this row" needs to write a filter that removes exactly its members (v4.49).
LUMICE_API LUMICE_ErrorCode LUMICE_ExpandRaypathClass(const LUMICE_CrystalParam* crystal, const int* faces,
                                                      int face_count, int symmetry, int semantics, int* out_faces,
                                                      int* out_member_count);

// =============== Raypath Validation ===============
// Validation state for raypath text input (GUI border color + OK gate).
typedef enum LUMICE_RaypathValidationState_ {
  LUMICE_RAYPATH_VALID,       // All tokens valid; safe to submit
  LUMICE_RAYPATH_INCOMPLETE,  // Trailing/leading separator; user still typing
  LUMICE_RAYPATH_INVALID,     // Non-numeric tokens or illegal face numbers
} LUMICE_RaypathValidationState;

// Validate a raypath text string (dash-separated face indices, e.g. "3-5") against
// both syntax rules and face-number legality for the given crystal kind.
// ',' is retired legacy syntax: a text containing one is LUMICE_RAYPATH_INVALID with a
// dedicated out_msg naming '-' (join faces on one path) and ';' (separate paths).
// Used by GUI for raypath filter input validation.
// out_msg: human-readable error description (empty on kValid/kIncomplete).
//          Caller provides buffer; recommended size = 256.
// Returns LUMICE_ERR_NULL_ARG if text, out_state, or out_msg is NULL.
LUMICE_API LUMICE_ErrorCode LUMICE_ValidateRaypathText(const char* text, LUMICE_CrystalKind kind,
                                                       LUMICE_RaypathValidationState* out_state, char* out_msg,
                                                       size_t msg_buf_size);

// =============== Miller Index Conversion ===============
// Verdict on one Miller-index triple offered as a pyramidal wedge angle.
typedef enum LUMICE_MillerConversionState_ {
  LUMICE_MILLER_VALID,       // Three well-formed indices making a buildable angle
  LUMICE_MILLER_NO_CONE,     // h == 0: this side has no pyramidal cap; angle is 0
  LUMICE_MILLER_INCOMPLETE,  // Fewer than three indices supplied; caller still collecting
  LUMICE_MILLER_INVALID,     // Too many indices, k != 0, a negative index, or an unbuildable angle
} LUMICE_MillerConversionState;

// Convert Miller indices to a pyramidal wedge angle, and say whether they are legal at all.
//
// h/k/l are the reduced three-index wire form -- the same three numbers the `upper_indices` /
// `lower_indices` JSON arrays hold. The redundant fourth Miller-Bravais index i = -(h+k) is not
// passed: it is derivable, so accepting it would mean accepting a value that can contradict the
// other two.
//
// provided_count is how many of h/k/l the caller has actually been given. Fewer than three reads
// as LUMICE_MILLER_INCOMPLETE (a row still being typed into, not an error to show the user); more
// than three reads as LUMICE_MILLER_INVALID. Slots the count says were not supplied are ignored,
// whatever was passed in them -- so a GUI can call this on every keystroke with its widgets'
// current contents and needs no rule of its own for "has the user finished".
//
// out_angle_deg is meaningful only for LUMICE_MILLER_VALID and LUMICE_MILLER_NO_CONE; it is set to
// 0 otherwise, which means "no opinion", not "zero degrees".
//
// out_invalid_index names the slot at fault: 0 = h, 1 = k, 2 = l, or -1 when no single slot is.
// The -1 cases are deliberate: a wrong index count is nobody's slot, and an out-of-range angle
// comes from the RATIO h:l, where two individually legal integers combine into a face no mesh can
// carry -- highlighting either one would point the user at a number that is not wrong. May be NULL.
//
// This is core's own adjudication, not a description of it. Ask it rather than transcribing the
// rules: every caller that transcribed the bare formula instead (config, server and GUI each kept
// a copy) also transcribed its blind spots, and the copies then disagreed with core about what
// h == 0 means.
//
// Returns LUMICE_ERR_NULL_ARG if out_state or out_angle_deg is NULL.
LUMICE_API LUMICE_ErrorCode LUMICE_ConvertMillerIndexToWedgeAngle(int h, int k, int l, int provided_count,
                                                                  LUMICE_MillerConversionState* out_state,
                                                                  float* out_angle_deg, int* out_invalid_index);

#ifdef __cplusplus
}
#endif

#endif  // LUMICE_EDITOR_H_
