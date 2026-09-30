#ifndef LUMICE_RENDER_H_
#define LUMICE_RENDER_H_

// Rendering math with no Server or Scene lifetime: annotation anchors, pixel <-> direction
// projection, lens types and their field-of-view limits, XYZ -> sRGB conversion and exposure.

#include "lumice_base.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============== Annotation Anchors ===============
// Where a view's auxiliary-line LABELS and its reference-point MARKERS land, in pixels. The lines
// themselves — the celestial horizon, parallels (constant altitude), meridians (constant azimuth),
// circles of constant angular distance from a direction (the sun, in every use so far) — are level
// sets of three world-space angle fields, and a consumer draws them from that definition on its
// own hardware (the CLI renderer rasterizes them on the CPU through core's in-process C++; the GUI
// preview evaluates them per fragment in its shader). What neither can derive locally is where
// along a curve its text should sit, and where a named direction lands on this canvas: that needs
// a forward projection walked along the curve, and that is what this query answers. ANCHORS AND
// POINTS ONLY — colour, line width, glyphs and collision avoidance belong to whoever draws.
//
// THIS IS A PER-FRAME CALL, BY DESIGN. It runs a curve walk (a few hundred forward projections per
// requested curve) and one projection per requested marker: tens of microseconds for a full grid,
// with nothing in it proportional to the canvas — `width`/`height` decide only whether a point is
// inside the frame. An interactive consumer calls it on every frame the view changes, so the text
// and the points move with the picture rather than catching up after it stops. (Its predecessor,
// LUMICE_ComputeAnnotationOverlay, returned width*height masks and carried the opposite warning; see
// the v4.28 note at LUMICE_API_VERSION.)
//
// Sanity ceilings on the request lists. As with the LUMICE_MAX_CONFIG_* family these guard against
// malformed input rather than expressing a design limit; a request past one is rejected with
// LUMICE_ERR_INVALID_VALUE rather than truncated.
// WIDENED (v4.18) from 360 to 1024, after the GUI's coordinate grid became a caller: at the
// narrowest field of view it allows (1 deg) the adaptive step is 0.5 deg, which is 720 meridians
// over the half-open (-180, 180]. 360 was rejecting that as malformed when it is the ordinary
// grid, and truncating instead would have been worse than rejecting — the surviving half of the
// list covers only negative azimuth, so a view looking east would have lost every meridian on
// screen while reporting success. Widening a validation ceiling breaks no caller: nothing sizes an
// array from it, and code compiled against the old value simply never sends more than it did.
#define LUMICE_MAX_ANNOTATION_LINES 1024
#define LUMICE_MAX_ANNOTATION_CIRCLES 64

// Which family a label belongs to. The consumer decides appearance from this; core encodes none.
#define LUMICE_ANNOTATION_HORIZON 0
#define LUMICE_ANNOTATION_ELEVATION 1
#define LUMICE_ANNOTATION_LONGITUDE 2
#define LUMICE_ANNOTATION_ANGULAR_DIST 3
// Circles about the camera's optical axis (LUMICE_AnnotationRequest::view_dist_deg). Same geometry
// as ANGULAR_DIST with a different centre; a distinct kind so a consumer can style and switch it on
// its own. ADDED (v4.39).
#define LUMICE_ANNOTATION_VIEW_DIST 4

// Longest label text core produces, including the terminating NUL. Values are at most
// "-180.0" plus a two-byte UTF-8 degree sign.
#define LUMICE_ANNOTATION_LABEL_MAX 16

// Named reference directions that can be reported as canvas POINTS (see
// LUMICE_AnnotationRequest::marker_ids). Two are absolute, four are defined relative to
// `reference_dir` — the sun, in every use so far:
//   SUBSUN     the sun reflected in a horizontal surface: same azimuth, negated altitude
//   ANTHELION  opposite azimuth, SAME altitude
//   ANTISOLAR  the full antipode of the sun: opposite azimuth AND altitude
// Values are pinned to core's lumice::annotation::MarkerId by static_assert at the single place
// that converts between the two; neither side may be reordered on its own.
#define LUMICE_ANNOTATION_MARKER_ZENITH 0
#define LUMICE_ANNOTATION_MARKER_NADIR 1
#define LUMICE_ANNOTATION_MARKER_SUN 2
#define LUMICE_ANNOTATION_MARKER_SUBSUN 3
#define LUMICE_ANNOTATION_MARKER_ANTHELION 4
#define LUMICE_ANNOTATION_MARKER_ANTISOLAR 5
#define LUMICE_ANNOTATION_MARKER_COUNT 6

// Sanity ceiling on the marker request list, in the same sense as LUMICE_MAX_ANNOTATION_LINES: it
// guards against a malformed count, it is not a design limit. Deliberately larger than
// LUMICE_ANNOTATION_MARKER_COUNT because duplicates are legal — asking for the same marker twice
// reports it twice rather than being an error.
#define LUMICE_MAX_ANNOTATION_MARKERS 16

// The view an overlay is computed for. Separate from LUMICE_RenderParam on purpose: this carries
// `front`, which the renderer has no field for, and it describes a pure computation with no Scene
// or Server lifetime around it. `width`/`height` are the CANVAS the answer is expressed in, which
// need not be the render resolution — a GUI panel showing a re-projected all-sky texture passes
// its own on-screen pixel size and gets anchors in that space.
typedef struct LUMICE_AnnotationView_ {
  int width;
  int height;
  int lens_type;   // LUMICE_LENS_TYPE_*
  float lens_fov;  // degrees
  int lens_shift[2];
  float overlap;  // dual-fisheye overlap zone |sky.z| threshold (sin value); 0 = none
  float view_azimuth;
  float view_elevation;
  float view_roll;
  int visible;  // LUMICE_VISIBLE_*
  // Non-zero clips everything to the camera-facing hemisphere, ON TOP OF `visible`. The two are
  // independent: `visible` says which half of the sky exists, `front` says the viewer only wants
  // what is in front of them.
  int front;
} LUMICE_AnnotationView;

// Which curves to place labels on, and which markers to place. Angle lists are caller-owned and
// read only for the duration of the call (the same borrow rule as the Scene family's leaf structs),
// so a stack array is fine. A NULL list with a zero count means "none of that category". A curve
// listed here gets its label anchors; the curve's own pixels are the caller's to draw from the same
// angle, so a family the caller draws but does not label need not be listed at all.
typedef struct LUMICE_AnnotationRequest_ {
  LUMICE_AnnotationView view;

  // The celestial horizon (altitude 0). Its own flag rather than a 0 entry in `elevation_deg`,
  // because every consumer so far colours it separately.
  int horizon;

  const float* elevation_deg;  // parallels, degrees
  int elevation_count;
  const float* longitude_deg;  // meridians, degrees
  int longitude_count;

  // Circles of constant angular distance from `reference_dir`. The direction need not be
  // normalized; a zero vector falls back to the zenith.
  const float* angular_dist_deg;
  int angular_dist_count;
  float reference_dir[3];

  // Named reference directions (LUMICE_ANNOTATION_MARKER_*) to report canvas positions for; NULL
  // with a zero count means none. Borrowed for the duration of the call, like the angle lists
  // above. Duplicates are legal and are reported once each, in request order. The two poles are
  // asked for here like any other id (MARKER_ZENITH / MARKER_NADIR); there is no separate switch.
  const int* marker_ids;
  int marker_count;

  // ADDED (v4.39). Circles of constant angular distance from the camera's OPTICAL AXIS, which core
  // derives from `view`'s az/el/roll — the camera-referenced twin of angular_dist_deg above. No
  // direction field: unlike angular_dist_deg (referenced to the caller's `reference_dir`) the axis
  // is determined by the request already. Borrowed for the duration of the call, like the lists
  // above; NULL with a zero count means none. Labels come back with kind
  // LUMICE_ANNOTATION_VIEW_DIST and `index` into THIS list.
  const float* view_dist_deg;
  int view_dist_count;
} LUMICE_AnnotationRequest;
// Exact-size pin, same duty and same rule as LUMICE_RenderParam's (lumice_scene.h). Pointer-bearing, so the
// number is the LP64 / LLP64 one every supported target shares (8-byte pointers).
#if defined(__cplusplus)
static_assert(sizeof(LUMICE_AnnotationRequest) == 144,
              "LUMICE_AnnotationRequest layout changed — bump LUMICE_API_VERSION");
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(LUMICE_AnnotationRequest) == 144,
               "LUMICE_AnnotationRequest layout changed — bump LUMICE_API_VERSION");
#endif

// One label: where to put it, what it says, and which curve it came from.
typedef struct LUMICE_AnnotationLabel_ {
  float px;  // canvas pixel, x right
  float py;  // canvas pixel, y down
  int kind;  // LUMICE_ANNOTATION_*
  // Index into the request list this label's curve came from, or -1 for the horizon (which comes
  // from no list). Lets a consumer map a label back to its line without parsing the text.
  int index;
  float value_deg;
  char text[LUMICE_ANNOTATION_LABEL_MAX];
} LUMICE_AnnotationLabel;

// Where one requested marker landed. `valid` is non-zero only when the direction is imaged by the
// lens, inside the canvas, and inside the requested hemisphere — the same three conditions a label
// anchor has to meet. px/py are unspecified when it is zero.
typedef struct LUMICE_AnnotationMarkerPoint_ {
  float px;  // canvas pixel, x right
  float py;  // canvas pixel, y down
  int valid;
} LUMICE_AnnotationMarkerPoint;

// The result. The caller allocates this struct (stack is fine); core allocates what the pointers
// point at, and LUMICE_ReleaseAnnotationAnchors frees it. Every pointer below is owned by core and
// stays valid until that call — the same acquire/release discipline LUMICE_Scene and
// LUMICE_ResultFrame use, with the same rule: exactly one Release per successful Compute, and
// nothing dereferenced afterwards.
typedef struct LUMICE_AnnotationAnchors_ {
  // One entry per label core placed, in no particular order; `kind` and `index` say which curve
  // each belongs to. NULL when there are none.
  const LUMICE_AnnotationLabel* labels;
  int label_count;

  // Parallel to the request's marker_ids: marker_points[i] is where marker_ids[i] landed.
  // marker_count equals the request's, and the array is NULL when it is zero.
  const LUMICE_AnnotationMarkerPoint* marker_points;
  int marker_count;

  // Opaque handle to the storage the pointers above live in. Do not read, write, copy or free it;
  // pass this struct to LUMICE_ReleaseAnnotationAnchors exactly once instead. Copying the struct
  // copies the handle, so only ONE copy may be released — treat it as a move, not a value.
  void* storage;
} LUMICE_AnnotationAnchors;

// Compute the anchors for one view. `*out` is fully overwritten on success and left untouched on
// failure, so a failed call leaves nothing to release.
//
// Contract:
//   - Pure and deterministic: identical `request` => identical output. No Server, no Scene, no
//     global state, and safe to call from any thread (including concurrently with a running
//     simulation — it shares nothing with one).
//   - A degenerate view (width or height <= 0) is not an error: it yields label_count = 0,
//     marker_count = 0 and NULL pointers, which still must be Released.
//   - The forward projection is the one the trace backends run, so an anchor sits on the curve a
//     consumer draws from the same angle field.
//
// Returns LUMICE_ERR_NULL_ARG if `request` or `out` is NULL, or a list pointer is NULL with a
// non-zero count; LUMICE_ERR_INVALID_VALUE for an unknown lens_type / visible, a negative count,
// a count past LUMICE_MAX_ANNOTATION_LINES / _CIRCLES / _MARKERS, or a marker id outside
// [0, LUMICE_ANNOTATION_MARKER_COUNT); LUMICE_ERR_UNKNOWN on allocation failure.
LUMICE_API LUMICE_ErrorCode LUMICE_ComputeAnnotationAnchors(const LUMICE_AnnotationRequest* request,
                                                            LUMICE_AnnotationAnchors* out);

// Release the storage a successful LUMICE_ComputeAnnotationAnchors allocated, and NULL out the
// pointers so a double release is a no-op rather than a double free. NULL-safe, and safe on an
// already-released or zero-initialized struct (same wording as LUMICE_SceneDestroy: calling it on
// a live result exactly once is required; calling it on anything else does nothing).
LUMICE_API void LUMICE_ReleaseAnnotationAnchors(LUMICE_AnnotationAnchors* anchors);

// A named reference direction as a WORLD DIRECTION, for a caller that wants to POINT THE CAMERA at
// it rather than find where it lands on a canvas. LUMICE_ComputeAnnotationAnchors answers the
// second question and needs a whole view (lens, fov, resolution, hemisphere policy) to do it; these
// two need none of that and cost O(1). They are thin forwards to core's single owner of the symbol
// rule, so a view preset and a drawn marker cannot drift into two spellings of "where is the
// subsun".
//
// The convention is the one every direction in this family uses: the direction light TRAVELS, so
// altitude = asin(-z) and the ZENITH IS z = -1. `sun_dir` need not be a unit vector — it is
// normalized on entry, exactly like LUMICE_AnnotationRequest::reference_dir — and is ignored
// outright by the two pole ids.
//
// Returns LUMICE_ERR_NULL_ARG if `sun_dir` or `out_dir` is NULL; LUMICE_ERR_INVALID_VALUE if
// `marker_id` is outside [0, LUMICE_ANNOTATION_MARKER_COUNT). `*out_dir` is untouched on failure.
LUMICE_API LUMICE_ErrorCode LUMICE_ResolveAnnotationMarkerDirection(int marker_id, const float sun_dir[3],
                                                                    float out_dir[3]);

// The sun's azimuth carried down to the horizon: the unit vector with the sun's horizontal
// direction and zero altitude. Deliberately NOT a LUMICE_ANNOTATION_MARKER_* id and deliberately a
// second function rather than a seventh value of the one above — a marker id's contract is that
// asking "where does it land on this canvas" is meaningful, and this direction has no landing-point
// semantics to offer. Same normalization contract as above.
//
// Degenerate near the poles, where the sun's horizontal component vanishes and its azimuth is
// undefined: below a measured threshold the result falls back to world +x (azimuth 0), a fixed
// direction rather than a nearest-neighbour one because at exactly +/-90 degrees the recovered
// azimuth is 180 degrees away from the one just short of it.
//
// Returns LUMICE_ERR_NULL_ARG if `sun_dir` or `out_dir` is NULL. `*out_dir` is untouched on failure.
LUMICE_API LUMICE_ErrorCode LUMICE_ResolveSunHorizonDirection(const float sun_dir[3], float out_dir[3]);

// Pixel -> world direction, the inverse of the projection LUMICE_ComputeAnnotationAnchors and the
// IN_FRAME membership test project with — for turning a click on a rendered canvas into a cone
// centre. Pure computation, no Server or Scene lifetime, like the annotation anchors.
//
// `px`/`py` are a PIXEL INDEX on the `view`'s width x height canvas (the direction returned is that
// pixel's centre — the same "+0.5" convention the renderer's own masks are built with). Integer on
// purpose: core has exactly one pixel-to-direction inverse, it takes a pixel, and a float overload
// here would be a second implementation of the lens math with its own rounding — the divergence
// the single inverse exists to prevent. Round a sub-pixel position down before calling.
//
// `*out_valid` is 1 iff the pixel images sky: inside the canvas, inside the lens's image domain
// (a fisheye's circle, a dual fisheye's two discs), and not clipped away by `visible` / `front`.
// `out_dir` is written only then, as a unit vector in the direction light TRAVELS (see the
// convention at LUMICE_ResolveAnnotationMarkerDirection); on 0 it is untouched. Returns
// LUMICE_ERR_NULL_ARG for a NULL view / out_dir / out_valid, LUMICE_ERR_INVALID_VALUE for an
// unknown lens_type / visible or a non-positive width / height.
LUMICE_API LUMICE_ErrorCode LUMICE_UnprojectPixel(const LUMICE_AnnotationView* view, int px, int py, float out_dir[3],
                                                  int* out_valid);

// World direction -> canvas position, the forward of LUMICE_UnprojectPixel above and the sampler
// LUMICE_ComputeAnnotationAnchors' marker points come from, on a direction of the caller's own —
// for keeping a marker that is defined by a direction (the GUI's analysis cone centre) on the
// picture as the view changes. Pure computation, no Server or Scene lifetime, no allocation: one
// projection, safe to call every frame.
//
// `dir` is the direction light TRAVELS (the convention at LUMICE_ResolveAnnotationMarkerDirection;
// what LUMICE_UnprojectPixel returns). It need not be normalized; a zero vector reads as the zenith,
// as LUMICE_AnnotationRequest::reference_dir does.
//
// `*out_px` / `*out_py` are a CONTINUOUS position on the `view`'s width x height canvas, in the
// same pixel space and with the same clamp as LUMICE_AnnotationMarkerPoint::px / py: x right,
// y down, origin at the top-left corner, clamped to [0, width-1] x [0, height-1]. Float on purpose
// where LUMICE_UnprojectPixel's input is an int: a pixel index is discrete by nature, a projected
// landing point is not, and this is the marker family's answer, not a second inverse.
//
// `*out_valid` is 1 iff the direction lands on the canvas under the MARKER policy — imaged by the
// lens, inside the canvas, and inside the `visible` / `front` hemisphere with the same half-degree
// slack every named marker gets at the edge. That is deliberately not LUMICE_UnprojectPixel's
// exact render-domain verdict: a marker is drawn beside the other markers and should appear and
// disappear as they do. `out_px` / `out_py` are written only on 1; on 0 they are untouched.
// Returns LUMICE_ERR_NULL_ARG for a NULL view / dir / out_px / out_py / out_valid,
// LUMICE_ERR_INVALID_VALUE for an unknown lens_type / visible or a non-positive width / height.
LUMICE_API LUMICE_ErrorCode LUMICE_ProjectDirection(const LUMICE_AnnotationView* view, const float dir[3],
                                                    float* out_px, float* out_py, int* out_valid);

// =============== Lens Type ===============
// Lens projection type. Values match Core's LensParam::LensType enum (index 0-10).
// Used by GUI to look up per-lens FOV limits without including config/render_config.hpp.
typedef enum LUMICE_LensType_ {
  LUMICE_LENS_LINEAR = 0,
  LUMICE_LENS_FISHEYE_EQUAL_AREA = 1,
  LUMICE_LENS_FISHEYE_EQUIDISTANT = 2,
  LUMICE_LENS_FISHEYE_STEREOGRAPHIC = 3,
  LUMICE_LENS_DUAL_FISHEYE_EQUAL_AREA = 4,
  LUMICE_LENS_DUAL_FISHEYE_EQUIDISTANT = 5,
  LUMICE_LENS_DUAL_FISHEYE_STEREOGRAPHIC = 6,
  LUMICE_LENS_RECTANGULAR = 7,
  LUMICE_LENS_FISHEYE_ORTHOGRAPHIC = 8,
  LUMICE_LENS_DUAL_FISHEYE_ORTHOGRAPHIC = 9,
  LUMICE_LENS_GLOBE = 10,
} LUMICE_LensType;

// Returns the maximum valid FOV (degrees) for the given lens type.
// Used by GUI to clamp the FOV slider upper bound when the user switches lens type.
LUMICE_API float LUMICE_MaxFov(LUMICE_LensType type);

// =============== Color Conversion ===============
// Batch XYZ float -> sRGB uint8 conversion with per-pixel intensity scale.
// xyz_in:          flat array of XYZ tristimulus values, 3 floats per pixel
//                  (length = pixel_count * 3).
// out:             caller-allocated uint8 buffer of length pixel_count * 3.
// pixel_count:     number of pixels to convert.
// intensity_scale: scalar applied per-pixel to XYZ before XYZ->sRGB conversion.
// Returns LUMICE_ERR_NULL_ARG if xyz_in or out is NULL; LUMICE_OK otherwise.
LUMICE_API LUMICE_ErrorCode LUMICE_XyzToSrgbUint8(const float* xyz_in, unsigned char* out, int pixel_count,
                                                  float intensity_scale);

// Same conversion with an additive background composited into it — the sibling an editor needs to
// bake a frame that matches what the renderer put on screen, since the renderer paints the sky
// behind the halo and a bake without it produces a different picture from the same data.
//
// background_linear: LINEAR RGB, 3 floats, added to the halo's radiance AFTER the XYZ->RGB matrix
//                    and BEFORE the clamp and the sRGB transfer curve. That placement is the whole
//                    contract: it is what makes a pixel carrying no halo energy come back as
//                    exactly the sRGB triple a color picker showed. Adding after the curve instead
//                    gamma-encodes the color a second time (0.2 would render as byte 123, not 51).
//                    Linear because that is the space the addition means something in — the same
//                    convention LUMICE_RenderParam::background uses, and the same reason both JSON
//                    parsers convert at their boundary. A C++ caller inside this codebase gets here
//                    from a picker value via lumice::SrgbToLinearRgb (src/util/color_space.hpp, an
//                    inline header function — no separate C API for this conversion); an external
//                    C API consumer applies the standard sRGB EOTF inverse itself.
// Returns LUMICE_ERR_NULL_ARG if any pointer argument is NULL; LUMICE_OK otherwise.
LUMICE_API LUMICE_ErrorCode LUMICE_XyzToSrgbUint8WithBackground(const float* xyz_in, unsigned char* out,
                                                                int pixel_count, float intensity_scale,
                                                                const float* background_linear);

// =============== EV Auto Anchor ===============
// P99 anchor of the auto-EV pipeline (doc/ev-pipeline-architecture.md §2.2/§2.5).
// When downsample_factor > 1 the Y channel is box-summed onto a
// (img_width/f) x (img_height/f) coarse grid, the P99 is taken over the non-zero coarse bins
// and divided by f^2, so the result is a **fine-equivalent** P99 rather than a true per-pixel Y
// statistic. Falls back to the fine per-pixel P99 when downsample_factor <= 1 or the coarse grid
// collapses to zero dimensions.
//
// The coarse and fine paths are not two precisions of one statistic: on a sparse scene they were
// measured 64x apart and respond to sample count with very different slopes, so which one a
// caller picks changes auto-EV by several stops. Pick deliberately.
//
// xyz_data is a borrowed view of at least img_width*img_height*3 floats (3 floats/pixel,
// Y = channel 1), read only for the duration of the call; a raw pointer carries no length, so the
// dimensions passed in are the only bound this function has. Returns 0 if no positive Y entries
// exist.
LUMICE_API float LUMICE_ComputeP99Y(const float* xyz_data, int img_width, int img_height, int downsample_factor);

// P99-anchored auto-EV in stops: log2(target_linear / (p99_raw_y / snapshot_intensity)), clamped
// to [-6, 6]. target_linear is the sRGB reverse transform of target_white (0-255 scale). Feed it
// the value LUMICE_ComputeP99Y returned, with the FINE snapshot_intensity even when that P99 came
// from the coarse path — the /f^2 above is what makes the two consistent. Returns 0 if
// snapshot_intensity or p99_raw_y is non-positive.
LUMICE_API float LUMICE_ComputeEvAuto(float p99_raw_y, float snapshot_intensity, float target_white);

#ifdef __cplusplus
}
#endif

#endif  // LUMICE_RENDER_H_
