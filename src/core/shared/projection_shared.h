// Single-source pure-math projection forward functions.
//
// This header is THE single source of truth for the RENDER-path forward
// projection (sky/world dir -> pixel) across all three trace backends:
//   - legacy CPU  (src/core/projection.cpp, lens_proj.hpp, scatter_accum.hpp)
//   - Metal       (src/core/metal/lumice_trace.metal, mid/final exit blocks)
//   - CUDA        (src/core/backend/cuda_trace_backend.cu, EmitGateProject)
// All three compile this same file (host C++ / MSL / CUDA via lm_shims.h), so
// all 11 LensType projections render identically on every backend and cross-
// backend parity is a structural guarantee — no backend can drift (315.2-315.6).
//
// NOT covered here: the GUI display re-projection (inverse resampling of a
// fixed dual-fisheye all-sky image in the preview shader / overlay labels).
// That is a SEPARATE concern living behind the C-API boundary — src/gui/ may
// not #include core/ headers (check_policies.py gate) and GLSL cannot include
// C++ — so its lens math is deliberately duplicated. The ONLY value shared
// across that boundary is globe kGlobeCameraD (see the "must match" anchor
// below); the wider GUI lens-math duplication is tracked separately in backlog.
//
// Surface contract: scalar value semantics only. Caller owns
// pixel-layout and visibility-rejection concerns.
//
// `lm_proj::ProjXY` is a transition-stage struct that intentionally mirrors
// `lumice::projection::ProjXY` (same fields, same order, trivially copyable).
// CPU wrappers copy the result field-by-field at the boundary. A future
// consolidation step (post CUDA backend MVP) may collapse the two into one.
#ifndef LM_PROJ_SHARED_H_
#define LM_PROJ_SHARED_H_

#include "lm_shims.h"

namespace lm_proj {

struct ProjXY {
  float x;
  float y;
  bool valid;
};

// Equal-area fisheye forward: k = r_scale / sqrt(1 + dz).
// Mirrors projection.cpp:FisheyeEqualAreaForward and lumice_trace.metal :736.
LM_FN ProjXY FisheyeEqualAreaForward(float dx, float dy, float dz, float r_scale) {
  float k = r_scale / LM_SQRT(1.0f + LM_CLAMP(dz, -1.0f + 1e-6f, 1.0f));
  return { k * dx, k * dy, true };
}

// Equidistant fisheye forward: scale = r_scale * theta / (pi/2 * rho).
LM_FN ProjXY FisheyeEquidistantForward(float dx, float dy, float dz, float r_scale) {
  float rho = LM_SQRT(dx * dx + dy * dy);
  if (rho < 1e-10f) {
    return { 0.0f, 0.0f, true };
  }
  float theta = LM_ACOS(LM_CLAMP(dz, -1.0f, 1.0f));
  float scale = r_scale * theta / (LM_PI_2F * rho);
  return { scale * dx, scale * dy, true };
}

// Stereographic fisheye forward: scale = r_scale * tan(theta/2) / rho.
LM_FN ProjXY FisheyeStereographicForward(float dx, float dy, float dz, float r_scale) {
  float rho = LM_SQRT(dx * dx + dy * dy);
  if (rho < 1e-10f) {
    // rho -> 0 happens at BOTH poles, and (0,0) is the right answer at only one of them. It is
    // reachable here at the north pole only: the single-lens path culls at kFisheyeStereographicMinCz
    // (rho >= 8.7e-3 past it) and the dual-fisheye path feeds |dz| <= 1 from one hemisphere at a
    // time, so neither can arrive at the antipode. Left as-is deliberately — see the note on
    // kFisheyeEquidistantMinCz for the type that does need the distinction.
    return { 0.0f, 0.0f, true };
  }
  float theta = LM_ACOS(LM_CLAMP(dz, -1.0f, 1.0f));
  float scale = r_scale * LM_TAN(theta / 2.0f) / rho;
  return { scale * dx, scale * dy, true };
}

// Orthographic fisheye forward: r = sin(theta). Rejects dz < 0 (aliasing).
LM_FN ProjXY FisheyeOrthographicForward(float dx, float dy, float dz, float r_scale) {
  if (dz < 0.0f) {
    return { 0.0f, 0.0f, false };
  }
  return { r_scale * dx, r_scale * dy, true };
}

// Equirectangular forward: x = atan2(dy, dx), y = asin(dz).
LM_FN ProjXY RectangularForward(float dx, float dy, float dz) {
  float lon = LM_ATAN2(dy, dx);
  float lat = LM_ASIN(LM_CLAMP(dz, -1.0f, 1.0f));
  return { lon, lat, true };
}

// Linear (perspective) forward: dz<=0 rejected (behind camera); else (dx/dz, dy/dz).
// Mirrors projection.cpp:LinearForward.
LM_FN ProjXY LinearForward(float dx, float dy, float dz) {
  if (dz <= 0.0f) {
    return { 0.0f, 0.0f, false };
  }
  return { dx / dz, dy / dz, true };
}

// ============================================================================
// Unified forward projection: world/sky dir → 0..2 pixel hits.
// Single source consumed by legacy CPU (lens_proj.hpp, scatter_accum.hpp,
// server/render.cpp) and (315.3) Metal/CUDA kernels.
// ============================================================================

// POD projection parameters — host predigests all trig-heavy setup so the
// per-ray function stays branch/mul only. `proj_type` uses LensParam::LensType
// integer values (0..10; 10 = globe, reserved for 315.4).
// `rot[9]` is a row-major camera rotation matrix; read only by single-lens
// types (kLinear + 4 single-fisheye); other types treat it as unused (host
// should still fill identity for POD determinism).
struct ProjParams {
  int proj_type;
  int img_w;
  int img_h;
  int lens_shift_x;
  int lens_shift_y;
  float scale;
  float r_scale;
  float max_abs_dz;
  // Globe only (kProjGlobe): the back-side fade range, in the eye-space distance units of
  // kGlobeCameraD (unit sphere). 0 = the camera-facing hemisphere only, which is what every
  // other lens type and every non-render caller (anchor plane, annotation overlay) keeps by
  // leaving it zero. See GlobeBackFadeWeight.
  float globe_back_fade;
  float rot[9];
};

// `weight` scales the ray's energy at this hit. 1 everywhere except a globe back-side hit,
// which carries GlobeBackFadeWeight and is emitted with bump_landed = false: like the dual
// fisheye overlap ring it is a second look at energy the frame already accounts for, so it
// must not move landed_weight (and with it the exposure normalization).
struct PixelHit {
  int px;
  int py;
  bool bump_landed;
  float weight;
};

struct ProjResult {
  PixelHit hits[2];
  int count;
};

// Match LensParam::LensType integer values.
LM_CONSTANT int kProjLinear = 0;
LM_CONSTANT int kProjFisheyeEqualArea = 1;
LM_CONSTANT int kProjFisheyeEquidistant = 2;
LM_CONSTANT int kProjFisheyeStereographic = 3;
LM_CONSTANT int kProjDualFisheyeEqualArea = 4;
LM_CONSTANT int kProjDualFisheyeEquidistant = 5;
LM_CONSTANT int kProjDualFisheyeStereographic = 6;
LM_CONSTANT int kProjRectangular = 7;
LM_CONSTANT int kProjFisheyeOrthographic = 8;
LM_CONSTANT int kProjDualFisheyeOrthographic = 9;
LM_CONSTANT int kProjGlobe = 10;

// Globe lens camera distance (eye-space). The camera sits at (0,0,kGlobeCameraD)
// looking toward the unit sphere at the origin (a genuine finite-distance sphere
// perspective, NOT an orthographic alias). This is the single device-side source
// of truth for the globe projection.
//   MUST match src/gui/gui_constants.hpp:173 (GUI kGlobeCameraD, = the shader
//   `globeInverse` kGlobeCameraDist) — the CLI↔GUI globe consistency contract
//   (315.4). The constant cannot cross the C-API boundary, so this comment anchor
//   plus test/golden-analytic/projection round-trip check guard the two copies.
//   HOLDERS of this value on the CPU side read it from here rather than restating
//   it, so they add no new copy to keep in sync: projection.cpp::GlobeInverse (the
//   analytic inverse of this file's globe branch, used to build the render-domain
//   mask in lens_proj_build.hpp).
LM_CONSTANT float kGlobeCameraD = 4.0f;

// Globe back-side fade: how much of the FAR side of the sphere shows through, as a function of
// mu — the cosine between a surface point and the camera axis (the GUI's hit_eye.z; -c.z in the
// globe branch below) — and the fade range `fade`.
//
// The camera sits at distance D from a unit sphere, so a surface point's distance to it depends
// on mu alone: dist(mu) = sqrt(D^2 + 1 - 2 D mu). The silhouette is mu = 1/D, at dist =
// sqrt(D^2 - 1); a far-side point is `depth = dist(mu) - sqrt(D^2 - 1)` further away than the
// silhouette, from 0 at the rim to (D + 1) - sqrt(D^2 - 1) (~1.127 for D = 4) at the antipode of
// the camera. The weight is exponential fog, exp(-depth / fade): 1 at depth 0, so it joins the
// front side (weight 1) continuously at the rim, e^-1 (~0.368) at depth == fade, and never exactly
// 0 — `fade` is the fog's characteristic length, not a distance at which the far side vanishes.
// There is deliberately no cutoff: a piecewise tail would put a visible seam where it switches.
// `fade <= 0` is the camera-facing hemisphere only: weight 0 everywhere, by an explicit branch
// (the curve itself never reaches 0).
//
// CALLED ONLY FOR FAR-SIDE POINTS (mu <= 1/D); a near-side point is not faded at all.
//
// MUST MATCH, by hand: the GUI's CPU mirror GlobeBackFadeWeight (src/gui/preview_jacobian.hpp)
// and the shader's globeBackFadeWeight (src/gui/preview_renderer.cpp) — the C-API boundary and
// GLSL keep either from including this file. The CPU pair is compared sample by sample in
// test/unit-correctness/gui/test_globe_back_fade.cpp.
LM_FN float GlobeBackFadeWeight(float mu, float fade) {
  if (!(fade > 0.0f)) {
    return 0.0f;
  }
  const float d = kGlobeCameraD;
  const float dist = LM_SQRT(LM_FMAX(d * d + 1.0f - 2.0f * d * mu, 0.0f));
  const float depth = LM_FMAX(dist - LM_SQRT(d * d - 1.0f), 0.0f);
  return LM_EXP(-depth / fade);
}

// RenderConfig::VisibleRange's `full` value, restated as a plain int because this header is also
// compiled as MSL and CUDA and cannot see RenderConfig. lens_proj_build.hpp static_asserts the two
// equal, so reordering the enum is a compile error rather than a silently wrong gate.
LM_CONSTANT int kProjVisibleFull = 2;

// Whether a renderer needs the globe's FAR-SIDE-ONLY energy kept apart from the total. On the globe
// a pixel images two sky directions — where its ray enters the sphere (near) and where it leaves
// (far) — and `visible` is judged for each of them on its own, so the display has to be able to
// show one side's energy without the other's. That needs the far side's share as a separate sum,
// which only exists when there is a far side at all (globe, fade > 0) AND when some pixel can
// actually split: a clip is configured, i.e. `visible` != full or the front clip is on. With
// neither, every direction passes both clips, both sides are always shown, and the separate sum
// would be written and never read. (The GUI never sends `front` with the globe — its effective
// value is forced off there — but a hand-written CLI config can, and core honours it on every lens.)
//
// THE one spelling of that gate: the CPU consumer, the Metal kernel and the CUDA kernel all call
// this function, so the three backends cannot disagree about when the far-side sum exists.
LM_FN bool NeedsFarXyzShadow(int proj_type, float globe_back_fade, int visible, bool front) {
  return proj_type == kProjGlobe && globe_back_fade > 0.0f && (visible != kProjVisibleFull || front);
}

// Per-type numerical floors on `cz` for the SINGLE-lens fisheye cull below. These are not
// visibility judgements — the configured visible hemisphere is a DISPLAY clip applied by the
// render-domain mask (lens_proj_build.hpp::VisibleByRange) and never by this function, and bounds
// culling belongs to the caller. They are the points past which each type's forward formula stops
// describing the sky it was handed. Before 474.1 the whole family shared one `cz <= 0` cull, i.e. core rendered only
// theta <= 90 deg while the GUI preview re-projected out to 180 deg; these three constants are
// what that one cull became once it was taken per type. (Orthographic is the fourth, and it keeps
// `cz <= 0` — see its branch.)
//
// Each floor is set by ITS type's numerics, and they differ by three orders of magnitude for that
// reason. What they have in common is the shape of the failure: every type recovers the azimuth by
// dividing by rho = sin(theta), which collapses at the antipode of the lens axis, where every
// azimuth is equally correct and no single pixel is.
//
// kFisheyeEqualAreaMinCz — r = rho / sqrt(1 + cz) equals sqrt(1 - cz) analytically, so the RADIUS
//   is bounded (sqrt(2) at the antipode) and needs no cull of its own. Its accuracy is not: cz
//   arrives with a few ulps of error and dividing by sqrt(1 + cz) amplifies that by 1/(2(1 + cz)),
//   so the computed radius drifts ABOVE the analytic rim as the antipode is approached — past the
//   point where projection.cpp's inverse still accepts it, which is exactly the state that makes
//   the render-domain mask paint background over a lit pixel. The floor is where that stops: with
//   1 + cz >= 1e-3 the relative error stays under 1e-4, while sqrt(2 - 1e-3) already sits 2.5e-4
//   below sqrt(2), so the drift cannot reach the rim. It is also, not coincidentally, well clear of
//   FisheyeEqualAreaForward's own dz clamp at -1 + 1e-6, inside which the returned radius decays
//   toward 0 instead of converging to the rim. theta = 177.4 deg; the 2.5e-4 of rim radius given up
//   is 0.02 px on a 60 px lens scale.
//
// kFisheyeEquidistantMinCz — r = acos(cz)/(pi/2) is well conditioned in the radius all the way in
//   (acos absorbs the error into theta, where 4e-5 rad costs 3e-5 of a rim radius of 2), so this
//   type can be culled two decades closer to the antipode than equal-area. -1 + 1e-6 is where
//   FisheyeEquidistantForward's own `rho < 1e-10` guard becomes unreachable (rho >= 1.4e-3 here),
//   which is why that guard is left alone: it is shared with the dual-fisheye path, where the pole
//   is a legitimate input and (0,0) is the right answer. theta = 179.92 deg.
//
// kFisheyeStereographicMinCz — r = tan(theta/2) DIVERGES at the antipode, so this is the one type
//   whose radius itself has to be bounded. The value is cos(179.5 deg): the half angle of the
//   359 deg fov ceiling `render_config.cpp::MaxFov` already imposes on this lens, so the per-ray
//   floor and the config-level ceiling describe one boundary rather than two. It puts the rim at
//   r = tan(89.75 deg), about 229 image radii — past any frame at any usable resolution, which is
//   why the GUI preview (whose stereographic inverse has no guard at all) and core still agree
//   pixel for pixel. projection.cpp::FisheyeStereographicInverse derives its own r bound from THIS
//   constant rather than restating the angle, so the two cannot drift.
LM_CONSTANT float kFisheyeEqualAreaMinCz = -1.0f + 1e-3f;
LM_CONSTANT float kFisheyeEquidistantMinCz = -1.0f + 1e-6f;
LM_CONSTANT float kFisheyeStereographicMinCz = -0.99996192f;

// Apply the transpose of a row-major 3x3 matrix (equivalent to Rotation::ApplyInverse
// in src/core/geo3d.cpp:79). Splits out of the switch so both fisheye and linear
// single-lens branches share one implementation.
LM_FN void ApplyRotTranspose(LM_THREAD const float* rot, float in0, float in1, float in2, LM_THREAD float* o0,
                             LM_THREAD float* o1, LM_THREAD float* o2) {
  *o0 = rot[0] * in0 + rot[3] * in1 + rot[6] * in2;
  *o1 = rot[1] * in0 + rot[4] * in1 + rot[7] * in2;
  *o2 = rot[2] * in0 + rot[5] * in1 + rot[8] * in2;
}

// Inline of projection.cpp:DualFisheyeToPixel — computes pixel (fx,fy) for a
// hemisphere-normalized (x_norm,y_norm) plus which circle (upper=left / lower=right).
LM_FN void DualFisheyeToPixelXY(float x_norm, float y_norm, bool is_upper, int width, int height, LM_THREAD float* fx,
                                LM_THREAD float* fy) {
  int short_res = LM_MIN(width / 2, height);
  float r = static_cast<float>(short_res) / 2.0f;
  float cy = static_cast<float>(height) / 2.0f;
  if (is_upper) {
    float cx = static_cast<float>(width) / 2.0f - r;
    *fx = -y_norm * r + cx;
    *fy = x_norm * r + cy;
  } else {
    float cx = static_cast<float>(width) / 2.0f + r;
    *fx = y_norm * r + cx;
    *fy = x_norm * r + cy;
  }
}

// Forward-project a world-space direction to 0/1/2 pixel hits.
//   hits[0]           — main projection (bump_landed=true) if produced.
//   hits[1]           — dual-fisheye overlap dual-write (bump_landed=false).
//   count             — 0=miss/cull, 1=main only, 2=main + overlap.
// Bounds culling (px/py against 0..img_w/h-1) is intentionally NOT done here;
// callers do the range check on the returned integer pixels. This function's
// own miss contract is `count=0` (hits[0] left unwritten); the legacy CPU
// convention of translating a miss to pixel {-1,-1} is applied one layer up,
// by lens_proj.hpp's callers, not by ProjectExitToPixel itself.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
LM_FN ProjResult ProjectExitToPixel(LM_THREAD const ProjParams& p, float wx, float wy, float wz) {
  ProjResult r;
  r.count = 0;

  int t = p.proj_type;
  if (t == kProjLinear || t == kProjFisheyeEqualArea || t == kProjFisheyeEquidistant ||
      t == kProjFisheyeStereographic || t == kProjFisheyeOrthographic) {
    // Single-lens family — camera-frame cull + rot-inverse + forward.
    //
    // 478.2: this branch used to open with a `visible_range` cull, dropping any ray in the
    // hemisphere `visible` excludes before it could reach a pixel. `visible` is a DISPLAY clip,
    // not an energy cull, and it is applied uniformly to all four lens families by the
    // render-domain mask (lens_proj_build.hpp::VisibleByRange) and the GUI shader's `u_visible`.
    // Culling here made this one family disagree with the other three about where energy is
    // allowed to land — see the `visible` section of doc/configuration.md. Every remaining
    // `return r;` below is a DIFFERENT question (the forward formula stops describing the sky it
    // was handed) and must stay.
    float cx = 0.0f;
    float cy = 0.0f;
    float cz = 0.0f;
    ApplyRotTranspose(p.rot, -wx, -wy, -wz, &cx, &cy, &cz);

    ProjXY xy = { 0.0f, 0.0f, false };
    if (t == kProjLinear) {
      xy = LinearForward(cx, cy, cz);
    } else if (t == kProjFisheyeOrthographic) {
      // The one member of the family that must keep a hemisphere cull: r = sin(theta) aliases past
      // the equator (sin 120 deg == sin 60 deg), so two distinct rays would land on one pixel.
      // Structurally not widenable — see the three MinCz constants above for the ones that were.
      if (cz <= 0.0f) {
        return r;
      }
      xy = FisheyeOrthographicForward(cx, cy, cz, 1.0f);
    } else if (t == kProjFisheyeStereographic) {
      if (cz < kFisheyeStereographicMinCz) {
        return r;
      }
      xy = FisheyeStereographicForward(cx, cy, cz, 1.0f);
    } else if (t == kProjFisheyeEqualArea) {
      if (cz < kFisheyeEqualAreaMinCz) {
        return r;
      }
      xy = FisheyeEqualAreaForward(cx, cy, cz, 1.0f);
    } else {  // kProjFisheyeEquidistant
      if (cz < kFisheyeEquidistantMinCz) {
        return r;
      }
      xy = FisheyeEquidistantForward(cx, cy, cz, 1.0f);
    }
    if (!xy.valid) {
      return r;
    }
    // Screen handedness: right = +az (owner decision 2026-07-03; scrum-321.1 audit).
    // Applied only to single-lens family (Linear + 4 single-fisheye); dual-fisheye
    // reuses *Forward directly and is intentionally unaffected. Do NOT move this
    // negation into the *Forward pure functions — they are shared with dual/globe.
    xy.x = -xy.x;
    int px = static_cast<int>(
        LM_FLOOR(xy.x * p.scale + static_cast<float>(p.img_w) / 2.0f + static_cast<float>(p.lens_shift_x)));
    int py = static_cast<int>(
        LM_FLOOR(xy.y * p.scale + static_cast<float>(p.img_h) / 2.0f + static_cast<float>(p.lens_shift_y)));
    r.hits[0].px = px;
    r.hits[0].py = py;
    r.hits[0].bump_landed = true;
    r.hits[0].weight = 1.0f;
    r.count = 1;
    return r;
  }

  if (t == kProjRectangular) {
    // Rectangular follows the FULL camera pose (azimuth AND elevation AND roll), not just the
    // azimuth it used to fold into a scalar `az0`. It consumes the same camera-frame vector the
    // single-lens family does, c = R^T * (-w), and reads the map's two axes off it:
    //   lon_ref  = +c.z — the optical axis, so the boresight sits at the centre of the map;
    //   polar    = -c.y — the camera's local +y points at the world nadir under the
    //                     (-90 + roll) / (90 - el) chain, so negating it puts the zenith on top;
    //   lon_quad = -c.x — the quadrature axis, whose sign is what makes this construction
    //                     POINTWISE identical to the azimuth-only form it replaces whenever
    //                     el = roll = 0 (the pose every full-sky lens is pinned to on the GUI
    //                     side). doc/coordinate-convention.md carries the derivation;
    //                     LmProj.RectangularAtZeroElevationAndRollReproducesTheAzimuthOnlyForm
    //                     is the assertion. Other permutations satisfy "follows the pose" but
    //                     break that degeneracy, so this is not a free choice.
    float cx = 0.0f;
    float cy = 0.0f;
    float cz = 0.0f;
    ApplyRotTranspose(p.rot, -wx, -wy, -wz, &cx, &cy, &cz);
    float lon_ref = cz;
    float lon_quad = -cx;
    float polar = -cy;
    ProjXY proj = RectangularForward(lon_ref, lon_quad, polar);
    // proj.x is atan2(lon_quad, lon_ref) and so already lies in [-pi, pi]. The legacy while-loop
    // wrap that used to follow the `- az0` subtraction is therefore unreachable and is gone; the
    // modulo below is a separate, still-live concern (lon = +pi bins one column past the canvas).
    float lon = proj.x;
    int raw_x = static_cast<int>(LM_FLOOR(lon * p.scale + static_cast<float>(p.img_w) / 2.0f));
    int px = ((raw_x % p.img_w) + p.img_w) % p.img_w;
    int py = static_cast<int>(LM_FLOOR(-proj.y * p.scale + static_cast<float>(p.img_h) / 2.0f));
    r.hits[0].px = px;
    r.hits[0].py = py;
    r.hits[0].bump_landed = true;
    r.hits[0].weight = 1.0f;
    r.count = 1;
    return r;
  }

  if (t == kProjDualFisheyeEqualArea || t == kProjDualFisheyeEquidistant || t == kProjDualFisheyeStereographic ||
      t == kProjDualFisheyeOrthographic) {
    float sx = -wx;
    float sy = -wy;
    float sz = -wz;
    bool is_upper = (sz >= 0.0f);
    float z_hemi = is_upper ? sz : -sz;
    ProjXY xy = { 0.0f, 0.0f, false };
    if (t == kProjDualFisheyeEqualArea) {
      xy = FisheyeEqualAreaForward(sx, sy, z_hemi, p.r_scale);
    } else if (t == kProjDualFisheyeEquidistant) {
      xy = FisheyeEquidistantForward(sx, sy, z_hemi, p.r_scale);
    } else if (t == kProjDualFisheyeStereographic) {
      xy = FisheyeStereographicForward(sx, sy, z_hemi, p.r_scale);
    } else {  // kProjDualFisheyeOrthographic
      xy = FisheyeOrthographicForward(sx, sy, z_hemi, p.r_scale);
    }
    // Primary write — even if xy.valid==false the legacy dual-fisheye path
    // still called DualFisheyeToPixel and wrote a pixel (only ortho sets
    // valid=false, and legacy code stored the resulting pixel unchecked).
    // Preserve that behaviour: forward hits[0] regardless of xy.valid.
    float fx = 0.0f;
    float fy = 0.0f;
    DualFisheyeToPixelXY(xy.x, xy.y, is_upper, p.img_w, p.img_h, &fx, &fy);
    r.hits[0].px = static_cast<int>(LM_FLOOR(fx));
    r.hits[0].py = static_cast<int>(LM_FLOOR(fy));
    r.hits[0].bump_landed = true;
    r.hits[0].weight = 1.0f;
    r.count = 1;

    // Overlap dual-write: only in the overlap band |sz| < max_abs_dz.
    if (p.max_abs_dz > 0.0f && LM_FABS(sz) < p.max_abs_dz) {
      float z_opp = -z_hemi;
      ProjXY xy2 = { 0.0f, 0.0f, false };
      if (t == kProjDualFisheyeEqualArea) {
        xy2 = FisheyeEqualAreaForward(sx, sy, z_opp, p.r_scale);
      } else if (t == kProjDualFisheyeEquidistant) {
        xy2 = FisheyeEquidistantForward(sx, sy, z_opp, p.r_scale);
      } else if (t == kProjDualFisheyeStereographic) {
        xy2 = FisheyeStereographicForward(sx, sy, z_opp, p.r_scale);
      } else {  // kProjDualFisheyeOrthographic
        xy2 = FisheyeOrthographicForward(sx, sy, z_opp, p.r_scale);
      }
      float fx2 = 0.0f;
      float fy2 = 0.0f;
      DualFisheyeToPixelXY(xy2.x, xy2.y, !is_upper, p.img_w, p.img_h, &fx2, &fy2);
      r.hits[1].px = static_cast<int>(LM_FLOOR(fx2));
      r.hits[1].py = static_cast<int>(LM_FLOOR(fy2));
      r.hits[1].bump_landed = false;
      r.hits[1].weight = 1.0f;
      r.count = 2;
    }
    return r;
  }

  if (t == kProjGlobe) {
    // Globe: finite-distance sphere perspective. Camera at (0,0,kGlobeCameraD)
    // in eye space looks at the unit sphere; the near (visible) hemisphere
    // surface point is the sky direction. Mirrors the GUI forward ProjectGlobe
    // (preview_renderer.cpp:919) / overlay_labels.cpp globe branch, which are
    // the numerical inverse of the shader `globeInverse`.
    //
    // The GUI expresses globe in its eye_dir e = WorldToView(w). Here we reuse
    // the SAME eye vector the single-lens family uses: c = R^T * (-w)
    // (ApplyRotTranspose). Single-lens cull parity (shared cull c.z<=0 ==
    // GUI-linear cull e.z>=0) forces the value correspondence c.z = -e.z with
    // the (x,y) numerator shared verbatim between linear and globe on BOTH
    // sides. Substituting into the GUI globe form:
    //   GUI visibility  e.z >  1/D   → cull when  c.z >= -1/D
    //   GUI denom       D - e.z      →            D + c.z   (∈ [3.0, 3.75] for
    //                                             D=4, never approaches 0)
    //   GUI numerator   (e.x, e.y)   →            (c.x, c.y)   [same as linear]
    // Because linear matches the GUI and globe differs from linear by exactly
    // the same delta on both sides, globe matches the GUI by transitivity
    // (including the x/y/row pixel convention — no extra flip is introduced).
    // `p.scale` = focal = img_radius/tan(fov/2), host-computed in ComputeLensScale
    // (identical to the linear scale formula, matching GUI focal).
    float cx = 0.0f;
    float cy = 0.0f;
    float cz = 0.0f;
    ApplyRotTranspose(p.rot, -wx, -wy, -wz, &cx, &cy, &cz);
    // The camera-facing hemisphere is cz < -1/D. Everything else is the far side, which the
    // same perspective formula below images onto the same pixel as the near-side point on that
    // pixel's ray (both lie on one line through the camera), so showing it is a weight, not a
    // second projection: 0 (cull, the default) unless a back-side fade range is configured.
    bool far_side = cz >= -1.0f / kGlobeCameraD;
    float weight = 1.0f;
    if (far_side) {
      weight = GlobeBackFadeWeight(-cz, p.globe_back_fade);
      if (!(weight > 0.0f)) {
        return r;  // outside the camera-facing hemisphere and faded out → cull
      }
    }
    // denom = D - mu with mu = -cz in [-1, 1], so it lies in [D - 1, D + 1] and never nears 0
    // on either side of the sphere.
    float denom = kGlobeCameraD + cz;
    // Globe is an OUTSIDE-IN view (camera looks at a sphere from outside), which is
    // horizontally mirrored relative to the INSIDE-OUT single-lens family (sky seen
    // from within). The GUI `globeInverse` (ray-sphere from outside) carries this
    // handedness; the CLI forward must match it, so negate the horizontal (cx) term
    // — globe deliberately diverges from linear's x convention here. See scrum
    // gui-lens-math-cli-alignment (owner: globe=outside-in per GUI tooltip; GUI is
    // the source of truth for globe orientation).
    int px = static_cast<int>(
        LM_FLOOR(-cx / denom * p.scale + static_cast<float>(p.img_w) / 2.0f + static_cast<float>(p.lens_shift_x)));
    int py = static_cast<int>(
        LM_FLOOR(cy / denom * p.scale + static_cast<float>(p.img_h) / 2.0f + static_cast<float>(p.lens_shift_y)));
    r.hits[0].px = px;
    r.hits[0].py = py;
    r.hits[0].bump_landed = !far_side;
    r.hits[0].weight = weight;
    r.count = 1;
    return r;
  }

  // Anything unknown → miss.
  return r;
}

}  // namespace lm_proj

#endif  // LM_PROJ_SHARED_H_
