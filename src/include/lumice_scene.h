#ifndef LUMICE_SCENE_H_
#define LUMICE_SCENE_H_

// Scene description: the configuration value types (crystals, filters, complex compositions,
// scattering layers, spectrum, colour classes, renderer parameters) and the opaque LUMICE_Scene's
// build and JSON API.

#include "lumice_base.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============== Configuration bounds ===============
// Per-kind soft capacity ceilings enforced by the Scene build API (LUMICE_SceneAdd* /
// LUMICE_SceneSet*) and by the JSON readers behind LUMICE_SceneFromJson / _FromJsonFile.
// They are pure validation bounds — nothing is dimensioned by them any more (the wide
// LUMICE_Config value struct whose inline arrays they used to size was removed in v4.12).
// Cross-references between items use integer IDs (crystal_id, filter_id) assigned by the
// Scene's Add* calls and resolved internally by Core.

#define LUMICE_MAX_CONFIG_CRYSTALS 256
#define LUMICE_MAX_CONFIG_FILTERS 256
#define LUMICE_MAX_CONFIG_RENDERERS 4
#define LUMICE_MAX_CONFIG_SCATTER_LAYERS 8
#define LUMICE_MAX_CONFIG_SCATTER_ENTRIES 256
#define LUMICE_MAX_CONFIG_RAYPATH_LEN 32
// Discrete-spectrum entry cap. Mirrors core wl_pool.hpp::kWlPoolSizeMax (255).
#define LUMICE_MAX_CONFIG_SPECTRUM_ENTRIES 255
// Complex (sum-of-products) filter composition bounds. See LUMICE_ComplexComposition.
// All three are sanity ceilings against malformed .lmc/JSON input: LUMICE_MAX_CONFIG_COMPLEX
// caps complex-filter records per scene, LUMICE_MAX_CONFIG_CLAUSES / _TERMS cap a single
// filter's OR/AND fan-out. Clause/term storage is heap-allocated via
// LUMICE_CompositionSetClauses (v4.9). Widen (breaking bump) if needed.
#define LUMICE_MAX_CONFIG_COMPLEX 32    // max complex-filter composition records per config
#define LUMICE_MAX_CONFIG_CLAUSES 4096  // sanity ceiling: max OR clauses per complex filter
#define LUMICE_MAX_CONFIG_TERMS 64      // sanity ceiling: max AND terms per clause
// Raypath color-class (Design 2) ABI bounds. Same "widen (breaking bump)" rule as
// LUMICE_MAX_CONFIG_COMPLEX / _CLAUSES / _TERMS. LUMICE_MAX_CONFIG_COLOR_CLASSES upper-aligns
// with core ComponentTable::kMaxBits (64), the deduped predicate-atom budget of a scene.
// LUMICE_MAX_CONFIG_COLOR_REFS is a per-class ref-count ceiling with generous headroom over
// the OR/AND expansion seen in practice.
#define LUMICE_MAX_CONFIG_COLOR_CLASSES 64
#define LUMICE_MAX_CONFIG_COLOR_REFS 32
// Per-renderer grid-line ceiling (angular_dist[] / view_dist[] / elevation_grid[] / longitude_grid[]
// inline arrays in LUMICE_RenderParam). Same "widen (breaking bump)" rule as the constants above. 64 matches the
// order of magnitude of the other sanity ceilings; the shipped corpus peaks at 1 angular-distance
// line.
#define LUMICE_MAX_CONFIG_GRID_LINES 64

// Per-renderer marker ceiling (LUMICE_RenderParam::markers[]). NOT a sanity ceiling like the
// constants above, and the difference matters: those pick a round number with headroom because the
// thing they bound is unbounded in principle. This one is EXACT. A renderer's marker list rejects
// duplicate ids, and the id space itself has LUMICE_ANNOTATION_MARKER_COUNT members, so a list that
// passed validation cannot physically hold more entries than there are ids. Widening it would not
// admit anything.
// Spelled as a literal rather than as LUMICE_ANNOTATION_MARKER_COUNT because that macro is defined
// in lumice_render.h's annotation section, which this header does not include, and the C
// preprocessor needs it visible at the point of use; c_api.cpp carries the static_assert that pins
// the two together, so a seventh id added to one side alone is a compile error rather than a
// silently truncated array.
#define LUMICE_MAX_CONFIG_MARKERS 6

// BREAKING (v4.10): LUMICE_AxisDist renamed+widened to
// LUMICE_Distribution and now serves ANY randomizable scalar (axis angles AND crystal shape
// quantities), mirroring core's single `Distribution` type (src/core/math.hpp). The distribution
// type constants were renamed LUMICE_AXIS_DIST_* -> LUMICE_DIST_* AND their numeric values were
// reordered so that NO_RANDOM == 0 (see the zero-init contract note below). Fields renamed
// mean/std -> center/spread (matching core's neutral center/spread naming). Callers must recompile.
//
// Distribution type constants for LUMICE_Distribution.type. Values deliberately match core
// DistributionType's enum order (src/core/math.hpp) so "zero-init == not random" holds in both
// layers with the same integer. The C API translates via a hand-written JSON string switch
// (c_api.cpp), NOT an integer cast, so the values need only stay self-consistent here.
#define LUMICE_DIST_NO_RANDOM 0
#define LUMICE_DIST_UNIFORM 1
#define LUMICE_DIST_GAUSS 2
#define LUMICE_DIST_ZIGZAG 3
#define LUMICE_DIST_LAPLACIAN 4
#define LUMICE_DIST_GAUSS_LEGACY 5

// A randomizable scalar. `center`/`spread` roles by type (照抄 core src/core/math.hpp):
//   NO_RANDOM     center = the deterministic value itself; spread = unused
//   UNIFORM       center = mean;                           spread = full range
//   GAUSS         center = mean;                           spread = std dev
//   ZIGZAG        center = mean;                           spread = amplitude
//   LAPLACIAN     center = mean;                           spread = scale
//   GAUSS_LEGACY  center = mean;                           spread = std dev (legacy no-Jacobian)
// Units are field-dependent (axis angles: degrees; face_distance: dimensionless ratio) — the
// distribution type is unit-agnostic, exactly as core's Distribution.
//
// ZERO-INIT CONTRACT: LUMICE_DIST_NO_RANDOM == 0 is a design promise. After `LUMICE_Distribution
// d{}` (or memset(0)), `type == NO_RANDOM` and the scalar is `center == 0` — i.e. NOT random.
// This fixes the pre-v4.10 trap where LUMICE_AXIS_DIST_GAUSS == 0 made a zero-inited struct a
// "std=0 gauss" instead of "not random".
typedef struct LUMICE_Distribution_ {
  int type;      // LUMICE_DIST_NO_RANDOM / UNIFORM / GAUSS / ZIGZAG / LAPLACIAN / GAUSS_LEGACY
  float center;  // role depends on type (see table above)
  float spread;  // role depends on type (see table above); unused for NO_RANDOM
} LUMICE_Distribution;

// Index space for LUMICE_CrystalParam.sync_group[] — one slot per randomizable shape scalar.
// A prism only owns HEIGHT + the six faces, a pyramid only owns UPPER_H/PRISM_H/LOWER_H + the six
// faces; a slot that does not apply to the crystal type is simply never read for that type.
//
// ⚠️ The order mirrors core ShapeScalar (src/config/crystal_config.hpp) VERBATIM, and that order is
// the RNG draw order — which is what makes "a group's leader = its lowest-index applicable member"
// identical to "the member drawn first", so no second ordering definition is needed on either side.
// It is therefore DELIBERATELY NOT the field declaration order of LUMICE_CrystalParam below
// (height / prism_h / upper_h / lower_h — note UPPER_H and PRISM_H are swapped relative to it).
// Do not "tidy up" the divergence: aligning these two orders silently redefines which member of a
// mixed group owns the distribution. The core header carries the same warning.
#define LUMICE_SHAPE_SCALAR_HEIGHT 0   // .height — prism only
#define LUMICE_SHAPE_SCALAR_UPPER_H 1  // .upper_h — pyramid only
#define LUMICE_SHAPE_SCALAR_PRISM_H 2  // .prism_h — pyramid only
#define LUMICE_SHAPE_SCALAR_LOWER_H 3  // .lower_h — pyramid only
#define LUMICE_SHAPE_SCALAR_FACE_0 4   // .face_distance[0] — both types
#define LUMICE_SHAPE_SCALAR_FACE_1 5
#define LUMICE_SHAPE_SCALAR_FACE_2 6
#define LUMICE_SHAPE_SCALAR_FACE_3 7
#define LUMICE_SHAPE_SCALAR_FACE_4 8
#define LUMICE_SHAPE_SCALAR_FACE_5 9
#define LUMICE_SHAPE_SCALAR_COUNT 10

// BREAKING (v4.10): the five shape scalars below
// (height/prism_h/upper_h/lower_h/face_distance[6]) were promoted from bare float to
// LUMICE_Distribution so a randomizable shape can be expressed through the C struct path (they
// map to core PrismCrystalParam.h_/d_[6] and PyramidCrystalParam.h_prs_/h_pyr_u_/h_pyr_l_, all
// already Distribution in core). upper_wedge_angle/lower_wedge_angle stay bare float, mirroring
// core's wedge_angle_u_/l_ (not Distribution). Layout changed; callers must recompile.
//
// BREAKING (v4.13): `sync_group[LUMICE_SHAPE_SCALAR_COUNT]` appended at the end (and the ten
// LUMICE_SHAPE_SCALAR_* index constants added above). Core has expressed shape-scalar sync groups
// since v4.12, but this struct had no slot for them, so the C API — the only path a config file,
// the GUI or a python caller ever takes — dropped the declaration on the floor: core always
// received all-zero and nothing warned. Layout changed; callers must recompile. Behavior does not:
// a zero-initialized struct means every scalar independent, exactly as before.
typedef struct LUMICE_CrystalParam_ {
  int id;
  int type;  // 0=prism, 1=pyramid

  // Prism
  LUMICE_Distribution height;

  // Pyramid
  LUMICE_Distribution prism_h;
  LUMICE_Distribution upper_h;
  LUMICE_Distribution lower_h;
  float upper_wedge_angle;  // degrees, angle between pyramidal face and c-axis (bare float, not Distribution)
  float lower_wedge_angle;  // degrees

  // Face distance (distance from center to each of the 6 prism faces), each a distribution.
  // Zero-init makes each element {NO_RANDOM, center=0, spread=0} == degenerate (zero-distance)
  // geometry: this is the SAME "caller must initialize" contract the pre-v4.10 bare float[6]{}
  // already carried, not a new trap. Regular hexagonal prism default = six {NO_RANDOM, 1.0f, 0.0f}.
  LUMICE_Distribution face_distance[6];

  // Axis distributions. Sampled values feed the rotation chain (angles in degrees)
  // R = Rz(azimuth - 180°) * Ry(-zenith) * Rz(roll); see doc/coordinate-convention.md.
  LUMICE_Distribution zenith;
  LUMICE_Distribution azimuth;
  LUMICE_Distribution roll;

  // Shape-scalar sync groups (v4.13), indexed by LUMICE_SHAPE_SCALAR_*: 0 = independent,
  // 1..N = group id. Members of one group share a SINGLE random draw — the group's first
  // applicable member (lowest LUMICE_SHAPE_SCALAR_* index) consumes the RNG and owns the
  // distribution; the rest reuse its value without consuming anything, and a member whose own
  // distribution differs is overwritten (with a warning, not silently). Mirrors core
  // PrismCrystalParam/PyramidCrystalParam::sync_group_ (src/config/crystal_config.hpp).
  //
  // ZERO-INIT CONTRACT: after `LUMICE_CrystalParam cp{}` (or memset(0)) every scalar is
  // independent — IDENTICAL to pre-v4.13 behavior, and the serialized JSON stays byte-identical
  // (the "sync_group" key is emitted only when something is actually synced). No existing caller
  // needs to change anything.
  //
  // Group ids are canonicalized by core on parse: singleton groups collapse to 0 and surviving
  // groups are renumbered 1..N by first appearance, so {2,1,2,1,2,1} and {1,2,1,2,1,2} are the
  // same partition and compare equal. Ids need not be dense or ordered on the way in.
  int sync_group[LUMICE_SHAPE_SCALAR_COUNT];
} LUMICE_CrystalParam;

// Filter type discriminant for LUMICE_FilterParam.type.
// 0 = UNSET is a deliberate zero-init guard: a struct built via memset/aggregate
// initialization without an explicit type lands on UNSET and is rejected at commit
// (LUMICE_ERR_INVALID_CONFIG) rather than being silently treated as "none". Callers
// that want the no-op "none" filter must set LUMICE_FILTER_TYPE_NONE explicitly.
#define LUMICE_FILTER_TYPE_UNSET 0
#define LUMICE_FILTER_TYPE_NONE 1
#define LUMICE_FILTER_TYPE_RAYPATH 2
#define LUMICE_FILTER_TYPE_ENTRY_EXIT 3
#define LUMICE_FILTER_TYPE_DIRECTION 4
#define LUMICE_FILTER_TYPE_CRYSTAL 5
// Reserved: complex (sum-of-products) filter reference encoding lands in a follow-up.
// Until then a filter with this type has no ConfigToJson case and is rejected at commit.
#define LUMICE_FILTER_TYPE_COMPLEX 6

// BREAKING (v4.5): LUMICE_FilterParam extended from raypath-only to a 5-arm tagged
// union (None/Raypath/EntryExit/Direction/Crystal). Layout changed; callers must
// recompile against this header. `type` selects the active arm; arm-specific fields are
// prefixed by arm (raypath_*, ee_*, dir_*, crystal_*). Field naming/units mirror core
// config/filter_config.hpp. -1 sentinels encode optional fields.
typedef struct LUMICE_FilterParam_ {
  int id;
  int action;  // 0=filter_in, 1=filter_out
  // Symmetry is a common field for ALL filter types (mirrors core FilterConfig.symmetry_,
  // emitted by filter_config.cpp::to_json before the per-type fields), not raypath-only.
  int symmetry;  // bitmask: 1=P, 2=B, 4=D
  int type;      // LUMICE_FILTER_TYPE_* (UNSET=0 is rejected at commit)

  // Raypath arm (type == LUMICE_FILTER_TYPE_RAYPATH)
  int raypath[LUMICE_MAX_CONFIG_RAYPATH_LEN];
  int raypath_count;

  // EntryExit arm (type == LUMICE_FILTER_TYPE_ENTRY_EXIT). -1 sentinels below.
  // NOTE: ee_min_len/ee_max_len mirror core's to_json emit conditions (min_len emitted
  // only when > 1; max_len only when >= 0). An out-of-contract value like ee_min_len == 0
  // is therefore emitted as "absent" and normalized to the core default (1) at commit
  // rather than rejected here. Callers must supply ee_min_len >= 1.
  // -1 is the ONLY sentinel: any other negative value is undefined (treated as wildcard/
  // absent, not rejected). These fields are int (matching the raypath[] convention); core
  // stores IdType(uint16_t) for entry/exit and size_t for the lengths, so keep entry/exit
  // in [0, 65535] and lengths reasonably small.
  int ee_entry;    // entry face id; -1 = wildcard (any entry face)
  int ee_exit;     // exit face id;  -1 = wildcard (any exit face)
  int ee_min_len;  // path length lower bound (>= 1)
  int ee_max_len;  // path length upper bound; -1 = no upper bound

  // Direction arm (type == LUMICE_FILTER_TYPE_DIRECTION). Degrees.
  float dir_az;     // azimuth (lon)
  float dir_el;     // elevation (lat)
  float dir_radii;  // angular radius (scalar, not an array)

  // Crystal arm (type == LUMICE_FILTER_TYPE_CRYSTAL)
  int crystal_id;

  // Complex arm (type == LUMICE_FILTER_TYPE_COMPLEX). Historically an index into the removed
  // LUMICE_Config's compositions[] pool; on the Scene path the composition is passed alongside
  // the filter to LUMICE_SceneAddComplexFilter, which IGNORES this field.
  int composition_index;
} LUMICE_FilterParam;

// Sum-of-products composition for a complex filter, passed to LUMICE_SceneAddComplexFilter
// alongside the filter it belongs to. The outer level is an OR over clauses; each clause
// is an AND over its terms; each term is the ID of a SIMPLE filter in the same scene
// (referenced by id — reorder-robust — not by array index; a term may never reference another
// complex filter, matching core config semantics).
//
// BREAKING (v4.9): storage layout changed from inline
// `clauses[16][8]` / `term_counts[16]` to a pair of owned heap pointers with a
// clause-major flat encoding. Rationale: at 16×8 inline the ceiling was too low for
// real "OR of several hundred raypaths" use cases, and naively widening the inline array
// (e.g. to 4096×64) would have blown the stack budget of the wide value struct that carried
// these records at the time. Callers populate one record via LUMICE_CompositionSetClauses and
// must release via LUMICE_CompositionReleaseClauses. Do NOT copy this struct by value —
// term_ids/term_counts are owning pointers; aliasing copies would double-free on double
// Release (enforced by scripts/check_policies.py's `no-config-by-value-copy` gate).
// LUMICE_SceneAddComplexFilter deep-copies the record, so a caller's composition can be
// released as soon as that call returns.
//
// Fields:
//   term_ids     — owned; flat array of simple-filter IDs, clause-major
//                  (clause 0's terms, then clause 1's terms, …). Length = sum(term_counts[0..clause_count)).
//   term_counts  — owned; term_counts[c] is the AND-term count of clause c. Length = clause_count.
//   clause_count — number of OR clauses in this composition. 0 is a valid "OR of nothing" state
//                  (both pointers nullptr); >0 requires both pointers non-null.
//
// Use LUMICE_CompositionClauseTerms to iterate a specific clause without recomputing the offset.
typedef struct LUMICE_ComplexComposition_ {
  int* term_ids;     // owned; flat AND-term simple-filter IDs, clause-major (see block comment)
  int* term_counts;  // owned; term_counts[c] = clause c's AND-term count; length == clause_count
  int clause_count;
} LUMICE_ComplexComposition;

typedef struct LUMICE_ScatterEntry_ {
  int crystal_id;
  float proportion;
  int filter_id;  // -1 = none
} LUMICE_ScatterEntry;

typedef struct LUMICE_ScatterLayer_ {
  float probability;
  LUMICE_ScatterEntry entries[LUMICE_MAX_CONFIG_SCATTER_ENTRIES];
  int entry_count;
} LUMICE_ScatterLayer;

typedef struct LUMICE_SpectrumEntry_ {
  float wavelength;  // nm
  float weight;      // relative weight (unnormalized; core normalizes)
} LUMICE_SpectrumEntry;
#if defined(__cplusplus)
static_assert(sizeof(LUMICE_SpectrumEntry) == 2 * sizeof(float),
              "LUMICE_SpectrumEntry must be tightly packed (2 floats, no padding) for ABI stability");
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(LUMICE_SpectrumEntry) == 2 * sizeof(float),
               "LUMICE_SpectrumEntry must be tightly packed (2 floats, no padding) for ABI stability");
#endif

// =============== Raypath Color Classes (BREAKING v4.7) ===============
// Design 2 (2026-07-08, doc/gui-custom-spectrum-and-raypath-color.md §4.0): each color class
// is decoupled from the physical filter. A class has an RGB color + a set of "match" refs;
// each ref is a placement-scoped predicate {layer, crystal, predicate} that decides which
// surviving rays get color-tagged. Predicate types are a NARROWED reuse of LUMICE_FilterParam
// (raypath / entry_exit / direction / crystal / none) — no id, action, composition, complex.
// Per-ref symmetry (P/B/D bitmask) is carried as a common field on the predicate (v4.9):
// matching semantics mirror the physical filter's symmetry (both feed the same
// Crystal::ReduceRaypath expansion on the core side).

// A predicate is a match rule, not a filter. Field naming mirrors the equivalent arms of
// LUMICE_FilterParam. type selects the active arm:
//   LUMICE_FILTER_TYPE_UNSET (0) — DELIBERATELY DIFFERENT from LUMICE_FilterParam's zero-init
//     guard: for a color PREDICATE, UNSET means "match-all whole-crystal" (aligns with core's
//     RaypathColorRef default `NoneFilterParam{}`, whose wire form is "no `type` key"). The
//     UNSET-reject convention on LUMICE_FilterParam guards against silently defaulting a
//     physical filter to no-op; a color predicate has no such physical-safety risk, so
//     zero-init reasonably means "whole-crystal color tag on this placement".
//   LUMICE_FILTER_TYPE_{NONE, RAYPATH, ENTRY_EXIT, DIRECTION, CRYSTAL} — same field semantics
//     as the LUMICE_FilterParam arms; see there.
//   LUMICE_FILTER_TYPE_COMPLEX is REJECTED (Design 2 color predicates are single-atom).
//
// BREAKING (v4.9): added `symmetry` field to LUMICE_ColorPredicate. Layout changed; callers
// must recompile against this header.
typedef struct LUMICE_ColorPredicate_ {
  // Symmetry is a common field for ALL predicate arms (mirrors LUMICE_FilterParam.symmetry /
  // core RaypathColorRef.symmetry_), not raypath-only. Bitmask: 1=P, 2=B, 4=D; 0=kSymNone
  // (literal single-orientation match — default, wire-omitted; see RaypathColorRef::to_json).
  int symmetry;
  int type;  // LUMICE_FILTER_TYPE_* (UNSET=0 means match-all; COMPLEX rejected at commit)

  // Raypath arm
  int raypath[LUMICE_MAX_CONFIG_RAYPATH_LEN];
  int raypath_count;

  // EntryExit arm. -1 sentinels; ee_min_len semantics mirror LUMICE_FilterParam.
  int ee_entry;
  int ee_exit;
  int ee_min_len;
  int ee_max_len;

  // Direction arm (degrees)
  float dir_az;
  float dir_el;
  float dir_radii;

  // Crystal arm
  int crystal_id;
} LUMICE_ColorPredicate;

// One placement-scoped color ref = the atom `{layer, crystal_id, predicate}`. Fields carry
// the same identifiers used elsewhere in scene config (scattering layer index, crystal id).
typedef struct LUMICE_ColorClassRef_ {
  int layer;    // scattering layer index (0-based)
  int crystal;  // crystal id
  LUMICE_ColorPredicate predicate;
} LUMICE_ColorClassRef;

// Combine strategy over the match[] refs (mirrors core ColorClassCombine).
#define LUMICE_COLOR_COMBINE_ANY 0
#define LUMICE_COLOR_COMBINE_ALL 1

// One color class = an RGB color, a boolean combine over its refs, per-class display-time
// visibility. A class carries `match[]` refs (semantic bits, decides which rays contribute)
// and display-time appearance (color, visible, solo — mutable via LUMICE_SetRaypathColors
// without re-simulation). match[]/combine are STRUCTURAL: changing them re-simulates.
//
// WARNING (A4): visible/solo are plain 0/1 booleans; zero-initializing `LUMICE_ColorClass{}`
// lands visible=0 (INVISIBLE), which is the OPPOSITE of the core JSON default `true`.
// Callers must explicitly set visible=1 for the class to appear in composited output — the
// class is otherwise silently omitted from the compositor. This mirrors LUMICE_FILTER_TYPE_UNSET's
// "zero-init requires explicit follow-up" discipline: every writer that assembles a color class
// (GUI scene builder, JSON reader, hand-written caller) must set it.
typedef struct LUMICE_ColorClass_ {
  float color[3];                                            // linear RGB in [0, 1]
  int combine;                                               // LUMICE_COLOR_COMBINE_ANY / _ALL
  int visible;                                               // 0 = hidden, non-zero = visible (see WARNING above)
  int solo;                                                  // non-zero = restrict composite to solo'd classes
  LUMICE_ColorClassRef match[LUMICE_MAX_CONFIG_COLOR_REFS];  // predicate atoms
  int match_count;
} LUMICE_ColorClass;

// Composite modes for the display-time compositor (mirrors core CompositeMode / the JSON
// "mode" field: "dominant" | "additive" | "painter"). Default painter matches the wire
// default (doc §4.8); painter uses the class list's
// z-order (see LUMICE_SetRaypathColors).
#define LUMICE_COLOR_MODE_DOMINANT 0
#define LUMICE_COLOR_MODE_ADDITIVE 1
#define LUMICE_COLOR_MODE_PAINTER 2

// Per-crystal ray allocation modes (mirrors the JSON scene.ray_allocation field:
// "proportional" | "adaptive", doc/configuration.md). Proportional deals each crystal its
// population share of the rays; adaptive deals by the crystals' measured per-ray energy variance
// (online Neyman allocation) — same expected image, noise made more even across crystals.
#define LUMICE_RAY_ALLOCATION_PROPORTIONAL 0
#define LUMICE_RAY_ALLOCATION_ADAPTIVE 1

// Lens projection kinds. Values mirror the declaration order of core LensParam::LensType, but the
// C API<->core mapping is an explicit switch, so a future reorder on either side cannot silently
// alias one projection onto another.
#define LUMICE_LENS_TYPE_LINEAR 0
#define LUMICE_LENS_TYPE_FISHEYE_EQUAL_AREA 1
#define LUMICE_LENS_TYPE_FISHEYE_EQUIDISTANT 2
#define LUMICE_LENS_TYPE_FISHEYE_STEREOGRAPHIC 3
#define LUMICE_LENS_TYPE_DUAL_FISHEYE_EQUAL_AREA 4
#define LUMICE_LENS_TYPE_DUAL_FISHEYE_EQUIDISTANT 5
#define LUMICE_LENS_TYPE_DUAL_FISHEYE_STEREOGRAPHIC 6
#define LUMICE_LENS_TYPE_RECTANGULAR 7
#define LUMICE_LENS_TYPE_FISHEYE_ORTHOGRAPHIC 8
#define LUMICE_LENS_TYPE_DUAL_FISHEYE_ORTHOGRAPHIC 9
#define LUMICE_LENS_TYPE_GLOBE 10

// Which anchor the exposure scale is measured against (mirrors core RenderConfig::EvMode).
//   RELATIVE — anchor to the frame's own P99. The image keeps its look as ray_num grows, but the
//              config alone does not determine output brightness (ray_num co-determines it).
//   ABSOLUTE — anchor to the EMITTED energy, so two simulations at the same EV are comparable.
// RELATIVE == 0 is the default: a zero-initialized LUMICE_RenderParam asks for the mode that
// reproduces what the GUI displays, which is also what a config with no "ev_mode" key means.
#define LUMICE_EV_MODE_RELATIVE 0
#define LUMICE_EV_MODE_ABSOLUTE 1

// Which operator turns accumulated radiance into pixels (mirrors core RenderConfig::Tone).
//   SCREEN — the additive operator: out = clamp(L * c + background). Monotonically non-decreasing
//            in radiance, so a light background can only stay light.
//   PRINT  — the subtractive (density) operator of doc/print-mode-subtractive-ink.md: ink is laid
//            ON the paper, out = paper * 10^(-D), so a white paper CAN go dark.
// SCREEN == 0 is the default: a zero-initialized LUMICE_RenderParam asks for the operator this API
// has always used, which is also what a config with no "tone" key means.
#define LUMICE_TONE_SCREEN 0
#define LUMICE_TONE_PRINT 1

// What the finished screen image is shown as (mirrors core RenderConfig::DisplayMode).
//   NORMAL     — the image itself.
//   CHANNEL_BR — the "bluer or redder here" diagnostic: gray = clamp(0.5 + 2 * (B - R), 0, 1) on
//                the post-gamma sRGB channels of the NORMAL image (src/util/channel_math.hpp).
// NORMAL == 0 is the default, so a zero-initialized LUMICE_RenderParam and a config with no
// "display_mode" key both mean the existing picture.
#define LUMICE_DISPLAY_MODE_NORMAL 0
#define LUMICE_DISPLAY_MODE_CHANNEL_BR 1

// Which half of the celestial sphere the renderer draws (mirrors core RenderConfig::VisibleRange).
#define LUMICE_VISIBLE_UPPER 0
#define LUMICE_VISIBLE_LOWER 1
#define LUMICE_VISIBLE_FULL 2

// One overlay grid line (mirrors core GridLineParam). `value` is the angular distance from the
// sun (angular_dist), the elevation (elevation_grid) or the azimuth (longitude_grid) in degrees;
// the rest is appearance.
typedef struct LUMICE_GridLine_ {
  float value;
  float width;
  float opacity;
  float color[3];
} LUMICE_GridLine;

// One entry of a renderer's marker list: WHICH named sky direction gets a ring, and what colour.
//
// `id` is a LUMICE_ANNOTATION_MARKER_* value — the same id space LUMICE_AnnotationRequest uses, so
// "which direction" has one vocabulary across the API. An id outside [0, MARKER_COUNT) is an error,
// never a silent fallback to the zenith.
//
// Colour is per entry; radius and opacity are NOT. They live on LUMICE_RenderParam as one pair for
// the whole family, because a set of reference points is read as a family — telling them apart is
// what colour is for, and a ring at a different size or a different transparency reads as a
// different KIND of thing rather than as a different point. `color` is sRGB, like
// LUMICE_GridLine.color and unlike `background`.
typedef struct LUMICE_MarkerStyle_ {
  int id;
  int enabled;
  float color[3];
} LUMICE_MarkerStyle;

// BREAKING (v4.3): norm_mode field removed; struct layout changed. Callers must recompile against this header.
// BREAKING (v4.11): extended from the 6-field projection-agnostic subset to the full renderer
// description (lens / lens_shift / view / visible / background / ray_color / grid /
// horizon). Before this, those fields had no home in the struct, so every C API entry
// point that re-encodes a renderer (LUMICE_SceneFromJson/File, LUMICE_SceneAddRenderer)
// silently replaced them with a hardcoded
// dual_fisheye_equal_area/fov180/view000/visible=full/black-background renderer — a config could
// parse cleanly and then be simulated with a projection the caller never asked for. Callers must
// recompile.
// BREAKING (v4.16): opacity field removed; struct layout changed. The core RenderConfig field it
// mirrored had no drawing consumer anywhere in the tree since the first commit — it parsed,
// serialized and compared, but never reached a pixel, so every caller setting it was configuring
// nothing. Removed rather than implemented: the renderer composites into a single image with no
// layer to be transparent against. Callers must recompile.
//
// WARNING: a zero-initialized `LUMICE_RenderParam{}` is NOT a committable state — lens_fov = 0 is
// rejected as an invalid FOV for every lens type. Callers must set at least lens_type/lens_fov
// explicitly (same "zero-init requires explicit follow-up" discipline as LUMICE_ColorClass's
// visible field and LUMICE_FILTER_TYPE_UNSET). No implicit non-zero default is baked in on
// purpose: an implicit default silently substituted for the caller's intent is exactly the defect
// this version fixes.
typedef struct LUMICE_RenderParam_ {
  int id;
  int resolution_w;
  int resolution_h;
  float intensity_factor;
  float overlap;   // Dual fisheye overlap zone |sky.z| threshold (sin value). 0 = no overlap.
  int lens_type;   // LUMICE_LENS_TYPE_*
  float lens_fov;  // degrees; valid range depends on lens_type (core MaxFov)
  int lens_shift[2];
  float view_azimuth;
  float view_elevation;
  float view_roll;
  int visible;  // LUMICE_VISIBLE_*
  // Linear RGB — it is added to the halo's radiance before the sRGB transfer curve, so it has to
  // live in the same space the addition does. The JSON "background" key is sRGB instead (what a
  // color picker shows); both JSON parsers convert at their boundary, so a caller writing this
  // struct directly passes linear while a caller writing JSON writes sRGB.
  float background[3];
  // Fixed ray tint in linear RGB, or {-1,-1,-1} (core's default sentinel) for "use the natural
  // spectral color". Zero-init means an all-black tint, NOT the sentinel.
  float ray_color[3];
  // Non-zero = draw a line along the celestial horizon (altitude 0). Opt-in: core's
  // RenderConfig::horizon_ defaults to false, so a zero-initialized struct asks for
  // no annotation, which is what the JSON path also gives a config with no "grid" object.
  int horizon;
  // Circles of constant angular distance from the sun, in degrees (22 and 46 being the halos
  // every consumer draws). RENDERED: the CLI renderer builds each entry's mask through core's
  // in-process annotation layer and composites it with that entry's own `opacity` and
  // `color`. `width` is read and round-tripped but does NOT affect the image — the mask
  // generator derives its own local half-width and takes no width input.
  // RENAMED (v4.17) from central_grid / central_grid_count; see the BREAKING note at
  // LUMICE_API_VERSION.
  LUMICE_GridLine angular_dist[LUMICE_MAX_CONFIG_GRID_LINES];
  int angular_dist_count;
  // Parallels: lines of constant elevation, in degrees. RENDERED as of v4.18 — the CLI renderer
  // builds each entry's mask through core's in-process annotation layer and composites it with
  // that entry's own `opacity` and `color`, exactly as it does for angular_dist. `width` is read and
  // round-tripped but does NOT affect the image.
  // The model mismatch that kept this unrendered for years (this schema names every parallel
  // individually, while the GUI derives ONE FOV-adaptive step and one shared colour) is resolved
  // in favour of the schema: the explicit list is the model, and the GUI's adaptive step is a
  // display-side convenience it expands into this list when it exports.
  LUMICE_GridLine elevation_grid[LUMICE_MAX_CONFIG_GRID_LINES];
  int elevation_grid_count;
  // ADDED (v4.18). Meridians: lines of constant azimuth, in degrees, measured the way
  // LUMICE_AnnotationRequest::longitude_deg measures them. Same rendering and appearance contract
  // as elevation_grid above (own opacity/color per entry, `width` inert).
  LUMICE_GridLine longitude_grid[LUMICE_MAX_CONFIG_GRID_LINES];
  int longitude_grid_count;
  // ADDED (v4.16): LUMICE_EV_MODE_*. Appended at the end of the struct, and RELATIVE == 0 so a
  // zero-initialized param keeps the documented default rather than silently opting into the
  // absolute anchor.
  int ev_mode;
  // ADDED (v4.19). The zenith / nadir ring markers. Non-zero `zenith_nadir` = draw them; opt-in for
  // the same reason `horizon` is, so a zero-initialized struct asks for no annotation.
  //
  // APPEARANCE ONLY. WHERE the rings land comes from the annotation anchors (the CLI renderer's
  // in-process marker points, LUMICE_ComputeAnnotationAnchors's marker_points for a C consumer,
  // which is also what the GUI preview reads): this struct carries no position, and there is
  // nothing here for a consumer to project.
  //
  // One block for the PAIR, not one per marker: the GUI has a single switch, colour picker and
  // radius slider for both, and core's ZenithNadirParam mirrors that. `zenith_nadir_color` is
  // sRGB, the convention LUMICE_GridLine.color uses and `background` does not.
  //
  // WARNING: unlike `horizon`, the three appearance fields have NON-ZERO defaults on the JSON side
  // (radius 8 px, opacity 0.6, colour {0.8, 0.2, 0.2} — core's ZenithNadirParam). A
  // zero-initialized struct with `zenith_nadir` set and nothing else asks for a zero-radius,
  // fully transparent, black ring, i.e. no visible marker. Set all four, or go through JSON.
  int zenith_nadir;
  float zenith_nadir_radius_px;
  float zenith_nadir_opacity;
  float zenith_nadir_color[3];
  // ADDED (v4.20). Non-zero = clip away the hemisphere BEHIND the camera, keeping only what the
  // camera faces. A SECOND clip dimension, independent of `visible` and ANDed with it, which is why
  // it is its own field rather than a `visible` enumerator. Opt-in: core's RenderConfig::front_
  // defaults to false, so a zero-initialized struct clips nothing.
  int front;
  // ADDED (v4.21). Non-zero = draw the TEXT labels for that annotation family — the angle each
  // line stands for, formatted by core ("22\u00b0"). Opt-in, like every annotation field above.
  //
  // INDEPENDENT OF THE LINE SWITCHES, at the geometry layer only: `horizon_label` with `horizon`
  // zero draws the horizon's numbers without its line, and since v4.26 gave the other three
  // families a line switch of their own the same now holds for them (`grid_label` with
  // `elevation_line` / `longitude_line` zero, `angular_dist_label` with `angular_dist_line` zero).
  // NOT independent at the compositing layer —
  // a label is painted in its family's own colour and opacity (each LUMICE_GridLine's `opacity` /
  // `color` for the two grid families and the circles; the horizon's fixed constants for the
  // horizon), so a line at opacity 0 is invisible together with its labels. See the v4.21 note at
  // LUMICE_API_VERSION for why that asymmetry is deliberate rather than an oversight.
  //
  // `grid_label` covers BOTH grid families, parallels and meridians, matching the single grid
  // label switch the GUI has. The circles get their own because the GUI has a separate one.
  // There is no zenith/nadir member: those markers carry no text.
  int horizon_label;
  int grid_label;
  int angular_dist_label;
  // ADDED (v4.25). The reference-point markers: N named sky directions, each drawn as a
  // pixel-space ring, each with its own colour. The generalization of `zenith_nadir` above, which
  // stays exactly as it is (see the v4.25 note at LUMICE_API_VERSION).
  //
  // WHICH OF THE TWO GETS DRAWN is decided at render time by ONE rule, stated here because it is
  // the only place a caller sees both: a NON-EMPTY `markers` list wins outright, and
  // `zenith_nadir` is consulted only when `markers_count` is 0. Not a merge — a config that lists
  // markers describes its whole marker set, and quietly adding two more rings from a legacy field
  // it also happens to carry would draw something nobody asked for.
  // The rule lives in the renderer rather than in the JSON decoder on purpose: every path that
  // builds a renderer (this struct, a JSON document, a direct core RenderConfig) then gets the same
  // arbitration, instead of each producer having to remember to apply it.
  //
  // APPEARANCE ONLY, like `zenith_nadir`: WHERE each ring lands comes from
  // LUMICE_ComputeAnnotationAnchors's marker_points, and there is no position to set here.
  //
  // `markers_opacity` / `markers_radius_px` are FAMILY-WIDE — see LUMICE_MarkerStyle for why only
  // colour is per entry.
  //
  // WARNING, the same one `zenith_nadir` carries and for the same reason: these two have NON-ZERO
  // defaults on the JSON side (radius 8 px, opacity 0.6 — core's RenderConfig). A zero-initialized
  // struct with markers listed and nothing else asks for zero-radius, fully transparent rings, i.e.
  // no visible marker. Set both, or go through JSON.
  //
  // Duplicate ids are rejected, not deduplicated: one marker has one colour, so a list naming a
  // marker twice is a question with no answer, and picking either occurrence silently would answer
  // it anyway.
  LUMICE_MarkerStyle markers[LUMICE_MAX_CONFIG_MARKERS];
  int markers_count;
  float markers_opacity;
  float markers_radius_px;
  // ADDED (v4.26). Non-zero = draw that family's LINES. One per angle list: `elevation_line` gates
  // elevation_grid[], `longitude_line` gates longitude_grid[], `angular_dist_line` gates
  // angular_dist[]. The horizon's equivalent is `horizon` above, which these three are modelled on.
  //
  // WARNING, and it is the reverse of every other annotation flag in this struct: their JSON
  // default is TRUE, so a ZERO-INITIALIZED struct asks for no lines even where it carries a full
  // angle list — while a document with no such key gets lines. `horizon` has no such trap (false on
  // both sides). Set all three, or go through JSON.
  // The direction is deliberate rather than an oversight. An absent `horizon` key drew nothing
  // because that flag is what turns the annotation on at all; an absent `elevation_line` key
  // belongs to a config whose non-empty `elevation` list was ALREADY drawing lines, so defaulting
  // it off would silently change what every existing config renders.
  //
  // WHAT THEY GATE is the LINE only, which is the half that makes "labels without lines"
  // expressible: the label geometry is decided by `grid_label` / `angular_dist_label` above and is
  // NOT affected by these, exactly as `horizon_label` is not affected by `horizon`. An EMPTY angle
  // list has no line to draw whatever the flag says, so the two ways a family can be absent never
  // contradict and there is no priority rule to learn.
  int elevation_line;
  int longitude_line;
  int angular_dist_line;
  // ADDED (v4.27). The print display mode: which tone-reproduction operator runs, and the ground
  // it composites onto. LUMICE_TONE_* for `tone`; `paper` is a LINEAR RGB triple, like `background`
  // above and unlike the JSON key of the same name, which is sRGB.
  //
  // WARNING, the same shape as `zenith_nadir`'s: `paper` has a NON-ZERO default on the JSON side
  // (white). A zero-initialized struct therefore names BLACK paper, and under the subtractive
  // operator that renders an all-black page — out = paper * 10^(-D) is zero for every pixel. Set
  // it, or go through JSON. Avoiding exactly this state one tick-box away is why `paper` is its
  // own field rather than a reuse of `background`.
  //
  // LIVE as of v4.27's operator: LUMICE_TONE_PRINT makes PostSnapshot take the density transfer
  // curve of src/util/ink_transfer.hpp instead of the additive one, greyscale by construction — the
  // gamut clip, the XYZ->RGB matrix and `ray_color` are all skipped. See
  // doc/print-mode-subtractive-ink.md for what the mode does and does not promise.
  int tone;
  float paper[3];
  // ADDED (v4.39). Circles of constant angular distance from the camera's OPTICAL AXIS — the
  // view's own forward direction (elevation / azimuth / roll), independent of lens_shift — the
  // camera-referenced twin of angular_dist[] above. RENDERED the same way: the CLI renderer builds
  // each entry's mask through core's in-process annotation layer and composites it with that
  // entry's own `opacity` and `color`; `width` is read and round-tripped but inert. No direction
  // field of its own — unlike angular_dist[] (referenced to the sun) the axis needs no caller
  // input; see the v4.39 note at LUMICE_API_VERSION.
  LUMICE_GridLine view_dist[LUMICE_MAX_CONFIG_GRID_LINES];
  int view_dist_count;
  // Its line and label switches, same contract as `angular_dist_line` / `angular_dist_label`
  // above: `view_dist_line` defaults TRUE on the JSON side (a filled list draws without touching
  // the flag) and is zero in a zero-initialised struct like its three siblings; `view_dist_label`
  // defaults false (text nobody asked for must not appear in a document that predates the field),
  // and drives the label geometry independently of the line switch.
  int view_dist_line;
  int view_dist_label;
  // ADDED (v4.45). Globe lens only (LUMICE_LENS_TYPE_GLOBE; every other type ignores it): how far
  // behind the sphere's silhouette its far side stays visible, fading out with distance from the
  // camera like fog, added onto the near side. In the eye-space units the globe camera distance
  // is stated in (unit sphere, camera 4 away), so the far side's deepest point, straight behind
  // the centre, sits ~1.127 behind the silhouette. 0 — the zero-initialized value and the JSON
  // default — shows the camera-facing hemisphere only, which is the look before this field.
  // Negative values are clamped to 0. JSON key "globe_back_fade".
  float globe_back_fade;
  // ADDED (v4.46). LUMICE_DISPLAY_MODE_* — see the constants. A post-process on the pixels the
  // screen operator produced, independent of `tone` as a field; under LUMICE_TONE_PRINT it is kept
  // but has no effect (print never computes R and B separately).
  int display_mode;
} LUMICE_RenderParam;
// The exact-size pin, the same duty RenderConfig's own carries on the C++ side: a field appended
// to this struct is an ABI event that has to be declared at LUMICE_API_VERSION, and the two
// numbers moving together is what shows the declaration was made. `>=`-style pins (LUMICE_RayCount
// above) guard an invariant that never changes; this one is expected to change, once per append,
// and every change is a bump. LUMICE_GridLine is 24 bytes (six 4-byte fields) and
// LUMICE_MarkerStyle 20, so the arrays account for 4 * 64 * 24 + 6 * 20 of it.
#if defined(__cplusplus)
static_assert(sizeof(LUMICE_RenderParam) == 6460, "LUMICE_RenderParam layout changed — bump LUMICE_API_VERSION");
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(LUMICE_RenderParam) == 6460, "LUMICE_RenderParam layout changed — bump LUMICE_API_VERSION");
#endif

// =============== Scene (opaque handle) ===============
// LUMICE_Scene (opaque type declared up top) is THE configuration container of this API and the
// only way to describe a simulation. It is built incrementally: adding a subsystem is adding a
// function (never an ABI break to existing callers), there is no MAX_* compile-time ceiling on
// the number of items, and there is exactly one commit entry point. The handle owns all its
// state; callers manage it exclusively through the lifecycle functions.
//
// The Add*/Set* family covers nine subsystems. The leaf POD structs above (LUMICE_CrystalParam /
// FilterParam / ComplexComposition / RenderParam / ScatterLayer / SpectrumEntry / ColorClass)
// pass in by const pointer — the Scene deep-copies every input value immediately, so the
// caller's leaf struct may be a stack temporary that is discarded/reused right after the call
// returns (no "must outlive commit" lifetime reasoning). Type + lifecycle + leaf writes are
// joined by the serialization half (SceneToJson / SceneFromJson / SceneFromJsonFile, below) and
// by the commit entry point (LUMICE_CommitScene, below).
//
// v4.12 removed the wide `LUMICE_Config` value struct this family replaced, along with its three
// commit entry points and its parse/serialize/ownership helpers. See the BREAKING note at
// LUMICE_API_VERSION for the full removed-symbol list.
//
// Naming: this family uses Noun-Verb order (SceneCreate / SceneAddCrystal / …), coexisting with
// the repo's existing Verb-Noun names (LUMICE_CreateServer / LUMICE_DestroyServer). Both are
// accepted conventions; neither is "the new standard".

// ---------- Lifecycle ----------
// Allocate an empty scene. The caller owns the returned handle and MUST eventually pass it to
// LUMICE_SceneDestroy. Returns NULL only on allocation failure.
LUMICE_API LUMICE_Scene* LUMICE_SceneCreate(void);

// Deep-copy `scene` into a brand-new, fully independent handle (no aliasing with the original).
// This is the value the old wide-struct semantics really bought — atomic modal edit / Cancel —
// now a single call instead of a ~128 KB stack copy. Mutating the clone never affects the
// original and vice versa; each must be Destroyed independently. Returns NULL if `scene` is
// NULL or on allocation failure.
LUMICE_API LUMICE_Scene* LUMICE_SceneClone(const LUMICE_Scene* scene);

// Release a scene handle. NULL-safe no-op (same contract as LUMICE_DestroyServer). Each handle
// must be Destroyed exactly once; destroying the same handle twice is undefined behavior (this
// mirrors LUMICE_DestroyServer and every other handle in this API — there is no double-free
// sentinel).
LUMICE_API void LUMICE_SceneDestroy(LUMICE_Scene* scene);

// ---------- Incremental build: leaf POD in, sequential id out ----------
// Every Add* appends one item and writes its 0-based sequence id (its index among items of the
// same kind, in insertion order) to *out_id. The Scene assigns this id itself and IGNORES any
// `.id` field on the incoming POD. Cross-referencing fields the caller constructs later
// (LUMICE_FilterParam.crystal_id, LUMICE_ScatterEntry.crystal_id/filter_id, a composition's
// term ids) MUST use these returned out_id values, not a caller-chosen id.
//
// Validation is Add-time: each call validates the one item's shape/enums before writing, and
// returns an error code (never throws across the C ABI). On any validation failure the scene is
// left unchanged (no partial write). Returns LUMICE_ERR_NULL_ARG when scene / the input pointer
// / out_id is NULL; LUMICE_ERR_INVALID_CONFIG on an invalid item (bad enum, out-of-range count,
// or exceeding the soft per-kind capacity given by the matching LUMICE_MAX_CONFIG_*).
LUMICE_API LUMICE_ErrorCode LUMICE_SceneAddCrystal(LUMICE_Scene* scene, const LUMICE_CrystalParam* crystal,
                                                   int* out_id);
// SceneAddFilter handles the SIMPLE filter arms only (none/raypath/entry_exit/direction/crystal);
// a filter with type == LUMICE_FILTER_TYPE_COMPLEX is rejected (LUMICE_ERR_INVALID_CONFIG) —
// use LUMICE_SceneAddComplexFilter for those.
LUMICE_API LUMICE_ErrorCode LUMICE_SceneAddFilter(LUMICE_Scene* scene, const LUMICE_FilterParam* filter, int* out_id);
// Add a complex (sum-of-products) filter in one call: the filter identity plus its composition.
// The Scene DEEP-COPIES composition->term_ids / term_counts into its own state immediately, so
// the caller's LUMICE_ComplexComposition (and its heap arrays) can be released/reused right
// after this returns. `filter->type` and `filter->composition_index` are ignored (type is
// forced to complex; the composition is taken from `composition`, not looked up by index).
LUMICE_API LUMICE_ErrorCode LUMICE_SceneAddComplexFilter(LUMICE_Scene* scene, const LUMICE_FilterParam* filter,
                                                         const LUMICE_ComplexComposition* composition, int* out_id);
LUMICE_API LUMICE_ErrorCode LUMICE_SceneAddRenderer(LUMICE_Scene* scene, const LUMICE_RenderParam* renderer,
                                                    int* out_id);
// Read one renderer back, the inverse of LUMICE_SceneAddRenderer: *out is the LUMICE_RenderParam
// the engine will actually use for that entry — core's defaults applied for every key the source
// omitted, enum-valued fields as LUMICE_LENS_TYPE_* / LUMICE_VISIBLE_* / LUMICE_EV_MODE_* /
// LUMICE_TONE_* constants. Works the same on a handle built incrementally and on one loaded
// through LUMICE_SceneFromJson / FromJsonFile (v4.40).
// `index` is the entry's 0-based ARRAY POSITION in insertion order — the value space of
// SceneAddRenderer's *out_id — and NOT the entry's `.id` field. The two coincide for a handle
// built with SceneAddRenderer (it assigns ids sequentially) but need not for one loaded from
// JSON, where `render[].id` keeps whatever the document declared: a document whose first entry
// says `"id": 7` reads back at index 0 with out->id == 7. To find an entry by declared id,
// enumerate from index 0 and compare out->id. There is no count getter: enumerate until the
// call fails, exactly as LUMICE_FrameGetRaypathAnalysis is read to its sentinel.
// Returns LUMICE_ERR_NULL_ARG for a NULL scene / out; LUMICE_ERR_INVALID_VALUE when `index` is
// negative or >= the number of renderers (the natural loop terminator); LUMICE_ERR_INVALID_CONFIG
// if the stored entry cannot be decoded (not reachable through this API's own writers).
LUMICE_API LUMICE_ErrorCode LUMICE_SceneGetRenderer(const LUMICE_Scene* scene, int index, LUMICE_RenderParam* out);
LUMICE_API LUMICE_ErrorCode LUMICE_SceneAddScatterLayer(LUMICE_Scene* scene, const LUMICE_ScatterLayer* layer,
                                                        int* out_id);
LUMICE_API LUMICE_ErrorCode LUMICE_SceneAddColorClass(LUMICE_Scene* scene, const LUMICE_ColorClass* color_class,
                                                      int* out_id);

// ---------- Scalar / whole-group settings, grouped by subsystem ----------
// Each Set* is idempotent (last write wins) and callable in any order. Returns
// LUMICE_ERR_NULL_ARG for a NULL scene, LUMICE_ERR_INVALID_CONFIG / _INVALID_VALUE for an
// invalid value.
//
// Light source + spectrum interact: a discrete spectrum (SceneSetCustomSpectrum with count > 0)
// takes precedence over the `spectrum` string, matching the core config's own rule.
// So SceneSetLightSource does NOT overwrite a discrete spectrum already set, and the two
// call orders (SetLightSource then SetCustomSpectrum, or the reverse) converge to the same
// result. SceneSetCustomSpectrum with count == 0 clears the discrete spectrum (falls back to the
// default "D65" string).
LUMICE_API LUMICE_ErrorCode LUMICE_SceneSetLightSource(LUMICE_Scene* scene, float sun_altitude, float sun_azimuth,
                                                       float sun_diameter, const char* spectrum);
LUMICE_API LUMICE_ErrorCode LUMICE_SceneSetCustomSpectrum(LUMICE_Scene* scene, const LUMICE_SpectrumEntry* entries,
                                                          int count);
// scene.ray_allocation ("proportional" | "adaptive", doc/configuration.md) is deliberately NOT a
// parameter of LUMICE_SceneSetSimParams (positional; adding one would break every caller). It has
// its own setter below, LUMICE_SceneSetRayAllocation (v4.38); a handle that never calls it omits
// the key, and LUMICE_SceneFromJson / ToJson still carry a document's spelling verbatim.
LUMICE_API LUMICE_ErrorCode LUMICE_SceneSetSimParams(LUMICE_Scene* scene, int infinite, LUMICE_RayCount ray_num,
                                                     int max_hits, int geom_clock);
// mode: LUMICE_RAY_ALLOCATION_PROPORTIONAL / _ADAPTIVE. Any other value is rejected with
// LUMICE_ERR_INVALID_CONFIG and leaves the scene untouched.
LUMICE_API LUMICE_ErrorCode LUMICE_SceneSetRayAllocation(LUMICE_Scene* scene, int mode);
LUMICE_API LUMICE_ErrorCode LUMICE_SceneSetColorMode(LUMICE_Scene* scene, int raypath_color_mode);

// ---------- Serialization: decoupled from commit ----------
// These are the JSON authoring half of the handle API and are DELIBERATELY independent of
// LUMICE_Server: SceneFromJson / SceneFromJsonFile only produce a handle, never commit or
// re-simulate (that stays the exclusive job of the commit entry points). Round-trip is lossless:
// SceneFromJson(ToJson(scene)) is semantically equal to the original.
//
// SceneToJson serializes `scene` into `out_buf` using an snprintf-style buffer contract: pass
// out_buf == NULL (or buf_size == 0) to query the length only; on a
// too-small buffer the output is truncated but always NUL-terminated, and *out_len (when non-NULL)
// always reports the full, untruncated length. Returns LUMICE_ERR_NULL_ARG for a NULL scene,
// LUMICE_ERR_INVALID_CONFIG if the scene cannot be serialized (e.g. a Set* call was fed a string
// that is not valid UTF-8).
LUMICE_API LUMICE_ErrorCode LUMICE_SceneToJson(const LUMICE_Scene* scene, char* out_buf, size_t buf_size,
                                               size_t* out_len);

// SceneFromJson / SceneFromJsonFile parse and validate a full scene JSON document and, on success,
// allocate a brand-new handle written to *out_scene (the caller owns it and MUST eventually pass
// it to LUMICE_SceneDestroy). On ANY failure *out_scene is set to NULL and no handle is leaked, so
// the caller never Destroys a handle that was not produced. Error-code semantics:
// LUMICE_ERR_NULL_ARG (NULL json_str/filename or out_scene), LUMICE_ERR_INVALID_JSON (syntax
// error), LUMICE_ERR_MISSING_FIELD (a required field absent), LUMICE_ERR_INVALID_VALUE /
// LUMICE_ERR_INVALID_CONFIG (bad enum / value / exceeds a LUMICE_MAX_CONFIG_* soft cap),
// LUMICE_ERR_FILE_NOT_FOUND (SceneFromJsonFile: file cannot be opened).
LUMICE_API LUMICE_ErrorCode LUMICE_SceneFromJson(const char* json_str, LUMICE_Scene** out_scene);
LUMICE_API LUMICE_ErrorCode LUMICE_SceneFromJsonFile(const char* filename, LUMICE_Scene** out_scene);

// =============== Complex-Composition storage lifecycle (BREAKING v4.9) ===============
// Populate `comp` in one shot from an application-owned (clause_count, term_counts[], term_ids[])
// triple. This is the ONLY supported writer for the composition storage — direct field writes
// (previously legal against the old inline `clauses[16][8]`) are no longer defined.
//
// This is a one-shot value write (Set), not a two-phase allocate-then-fill-in-place: every
// production caller already holds the complete
// (clause_count, term_counts, term_ids) triple on the stack before calling, so there is no
// caller-visible "allocated but not yet populated" intermediate state to expose. Reach for this
// same shape (Set over Create) for a future owning field only if callers likewise assemble the
// full value before writing it in.
//
// Semantics:
//   - `comp == nullptr` → returns LUMICE_ERR_NULL_ARG.
//   - `clause_count < 0` or `clause_count > LUMICE_MAX_CONFIG_CLAUSES` → returns
//     LUMICE_ERR_INVALID_CONFIG, `comp` left untouched.
//   - `clause_count > 0 && term_counts == nullptr` → LUMICE_ERR_NULL_ARG.
//   - Any `term_counts[i] < 0` or `term_counts[i] > LUMICE_MAX_CONFIG_TERMS` →
//     LUMICE_ERR_INVALID_CONFIG (full clause-count validation runs before any allocation;
//     `comp` untouched on rejection).
//   - `term_ids == nullptr` is only rejected (LUMICE_ERR_NULL_ARG) once `sum(term_counts[0..
//     clause_count))` (total term count) is computed and found > 0 — i.e. `term_ids` may
//     legitimately be null when every clause has 0 terms; see LUMICE_CompositionClauseTerms's
//     doc comment for the resulting storage state.
//   - `clause_count == 0` → release any existing allocation and land in the "OR of nothing"
//     state (term_ids/term_counts nullptr, clause_count 0). term_counts/term_ids inputs
//     are ignored in this branch.
//   - `clause_count > 0` → this call is CREATE-OR-REPLACE: if `comp` already held a prior
//     allocation, it is released before the new one is allocated. On OOM, `comp` falls back
//     to the "OR of nothing" state (fully-cleared, safe to Release again) and the function
//     returns LUMICE_ERR_INVALID_CONFIG.
//   - `term_ids` must be a clause-major flat array of length `sum(term_counts[0..clause_count))`.
//     Elements are simple-filter IDs; reference-integrity checks (each id resolves to an
//     existing SIMPLE filter in the same scene) are the CALLER's responsibility
//     (the c_api / GUI writers already run this pre-check).
//   - Allocator is calloc/free; callers must eventually release via
//     LUMICE_CompositionReleaseClauses (or its config-wide sibling / RAII guard).
LUMICE_API LUMICE_ErrorCode LUMICE_CompositionSetClauses(LUMICE_ComplexComposition* comp, int clause_count,
                                                         const int* term_counts, const int* term_ids);

// Release the term_ids / term_counts allocations owned by `comp`, if any. Idempotent and null-safe:
//   - `comp == nullptr` → no-op.
//   - Otherwise → free both pointers (either may already be null), then leave `comp` in the
//     "OR of nothing" state (both nullptr, clause_count=0).
LUMICE_API void LUMICE_CompositionReleaseClauses(LUMICE_ComplexComposition* comp);

// Read-only convenience accessor: return a pointer to clause `clause_index`'s first AND-term
// inside `comp->term_ids` (i.e. the address of `term_ids[prefix_sum(term_counts[0..clause_index))]`)
// and write `term_counts[clause_index]` into `*out_term_count`. Encapsulates the prefix-sum
// offset math so callers (ConfigToJson emit, tests, GUI diagnostics) don't recompute it.
//
// Semantics:
//   - `comp == nullptr`                                       → returns nullptr, `*out_term_count`
//                                                                 set to 0 if non-null.
//   - `clause_index < 0` or `clause_index >= comp->clause_count` → returns nullptr, `*out_term_count`
//                                                                 set to 0 if non-null.
//   - Otherwise → `*out_term_count` (if non-null) is always set to `term_counts[clause_index]`.
//     The returned pointer is nullptr whenever `comp->term_ids` itself is null — which is the
//     legitimate state when every clause in the composition has 0 terms (LUMICE_CompositionSetClauses
//     skips that allocation entirely in that case); this applies even to a 0-term clause, so callers
//     must not unconditionally dereference a non-null out_term_count as "safe to iterate".
//     When `comp->term_ids` is non-null, the returned pointer (including for a 0-term clause) is a
//     valid address into that buffer for the (possibly empty) slice.
LUMICE_API const int* LUMICE_CompositionClauseTerms(const LUMICE_ComplexComposition* comp, int clause_index,
                                                    int* out_term_count);

#ifdef __cplusplus
}
#endif

#endif  // LUMICE_SCENE_H_
