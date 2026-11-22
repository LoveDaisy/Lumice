#include "gui/preview_renderer.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "gui/forward_projection.hpp"
#include "gui/gl_common.h"
#include "gui/gui_logger.hpp"
#include "gui/xyz_half_codec.hpp"
#include "util/annotation_line_width.hpp"

namespace lumice::gui {

// clang-format off
static const char* kVertexShader = R"glsl(
#version 330 core
layout(location = 0) in vec2 a_pos;
out vec2 v_ndc;
void main() {
  gl_Position = vec4(a_pos, 0.0, 1.0);
  v_ndc = a_pos;  // Pass NDC [-1,1] to fragment shader
}
)glsl";

// The fragment shader is compiled TWICE, and the second copy differs from the first by one
// preprocessor symbol: LUMICE_GLOBE_BACK_FADE, which compiles in the globe lens's far-side sample
// (globeFarDir and its use in main()). Render() binds the variant only while a globe frame has a
// fade range > 0; every other frame runs the program without it.
//
// WHY TWO PROGRAMS rather than one branch on u_globe_back_fade. With the far-side code present
// behind a runtime branch the GPU compiler schedules the NEAR side's arithmetic differently, and
// that was measured, not guessed: globe frames at fade 0 came back with 2-7 pixels one LSB off the
// shader as it was before the fade existed, and removing the branch at compile time made them
// byte-identical again. A range of 0 promises the old picture exactly, so the old picture is the
// program without the symbol, and the claim holds by construction rather than by a compiler's
// continued goodwill. The variant is a strict superset: without the symbol `has_back` is a
// constant false and every arm that reads it folds to the original expression.
//
// The #version line lives apart (kFragmentShaderVersion) because it has to precede the #define.
static const char* kFragmentShaderVersion = "#version 330 core\n";
static const char* kGlobeBackFadeDefine = "#define LUMICE_GLOBE_BACK_FADE 1\n";
static const char* kFragmentShader = R"glsl(

in vec2 v_ndc;               // NDC position [-1, 1]

uniform sampler2D u_texture;
uniform vec2 u_resolution;   // viewport size in pixels
uniform int u_lens_type;     // 0=linear, 1-3=fisheye, 4-6=dual fisheye, 7=rectangular, 8=fisheye_orthographic, 9=dual_fisheye_orthographic, 10=globe
uniform float u_fov;         // full FOV in degrees
uniform mat3 u_view_matrix;  // view-to-world rotation (inverse view)
uniform int u_visible;       // 0=upper, 1=lower, 2=full
uniform int u_front;         // 1=discard back hemisphere
#ifdef LUMICE_GLOBE_BACK_FADE
uniform float u_globe_back_fade;  // globe only: far-side fade range (> 0 whenever this variant is bound)
#endif
uniform float u_intensity_scale;  // = intensity_factor / per_pixel_intensity (0 = RGB mode)
uniform int u_tex_mode;           // kTexModeSrgbComposited / kTexModeXyz / kTexModeSrgbRadiance
// Whether the sim layer may be sampled at all (PreviewRenderer::sim_layer_valid_). The no-image
// state — a revoked display after an OOM, a cleared document whose blank has not (yet) landed —
// must not depend on "the blank redefinition will succeed": zero means the fetch below is skipped
// entirely, the zero-energy colour stands in for the sky, and NOT ONE texel of whatever the
// storage actually holds reaches the screen. Background photo and overlay keep drawing.
uniform int u_sim_valid;
// kTexModeXyz only: the texture holds xyz / u_xyz_scale as float16 (see src/gui/xyz_half_codec.hpp),
// so one multiply at the sample site restores the frame's linear XYZ before anything reads it.
uniform float u_xyz_scale;
uniform vec3 u_background;        // sky colour, LINEAR RGB (see PreviewParams::background_color_linear)
uniform vec3 u_paper;             // paper colour, LINEAR RGB (see PreviewParams::paper_color_linear)
uniform int u_tone;               // 0 = screen (additive), 1 = print (subtractive) — config::RenderConfig::Tone
uniform int u_display_mode;       // 0 = normal, 1 = channel B-R — config::RenderConfig::DisplayMode
uniform sampler2D u_bg_texture;
uniform float u_max_abs_dz;      // overlap zone |sky.z| threshold (0 = no blend)
uniform float u_r_scale;         // projection r_scale for overlap normalization
uniform int u_bg_enabled;
uniform float u_overlay_alpha;
uniform vec2 u_bg_uv_scale;
uniform vec2 u_bg_uv_offset;

// Auxiliary line overlay uniforms.
//
// The four curve families — the celestial horizon, the parallels and meridians of the coordinate
// grid, the circles of constant angular distance from a reference direction — are evaluated HERE,
// per fragment, from this fragment's own world direction. Each is a level set of a world-space
// angle field, and the field, the levels and the line-width rule are the same definition the CLI
// renderer evaluates on the CPU (src/core/annotation_overlay.cpp over
// mask_detail::LevelSetMaskFromField); the two are one curve computed by two evaluators, and
// test/gui/parity/test_gui_cli_export_parity.cpp is the gate that keeps their pixels on top of
// each other. What this buys over having core rasterize the curve and uploading a mask is the
// thing the picture itself already has: it is re-projected every frame, so the lines drawn from
// the same per-fragment direction move with it, on the same frame, at no CPU cost and with no
// debounce.
uniform int u_show_horizon;
uniform int u_show_grid;
uniform int u_show_sun_circles;
uniform int u_show_view_dist;
// The level lists, PACKED FOUR TO A vec4. Not `float u_x[N]`: default-block uniform packing is
// implementation-defined and a float array commonly costs one whole vec4 register per element,
// which at these lengths would exceed GL_MAX_FRAGMENT_UNIFORM_COMPONENTS on drivers that report
// the common 4096. Packed, the lists take 516 registers (2064 components) and fit with room.
// Element i of a list is list[i >> 2][i & 3] (levelAt below).
//
// The two grid families share ONE array: the parallels occupy [0, u_elevation_count), the
// meridians [u_elevation_count, u_elevation_count + u_longitude_count), each range sorted
// ascending by the uploader. One array rather than two so that the nearest-level search reads
// the uniform in place through an index range — a uniform array handed to a GLSL function as a
// parameter is passed BY VALUE, and this driver honours that with a copy of all 256 vec4 per
// call per fragment (measured: 3 ms -> 950 ms a frame). The literal sizes are
// 2 * kMaxOverlayLevels / 4 and kMaxAnnotationCircles / 4 (preview_renderer.hpp / gui_constants.hpp);
// static_asserts after this string pin the two spellings together.
uniform vec4 u_grid_levels_deg[512];
uniform int u_elevation_count;
uniform int u_longitude_count;
uniform vec4 u_angular_dist_deg[4];
uniform int u_angular_dist_count;
// The view circles' levels: the same packing and the same capacity as the sun circles', in an
// array of their own — the two families have separate switches, colours and alphas, so they are
// two lists and not two ranges of one (the grid packs its two families together because they
// share one appearance; these do not). Centred on the optical axis, -u_view_matrix[2], which is
// why there is no u_reference_dir twin beside them.
uniform vec4 u_view_dist_deg[4];
uniform int u_view_dist_count;
// The direction the angular-distance circles are centred on (the sun), a unit vector in the world
// frame this shader's world_dir lives in — the direction light TRAVELS, altitude = asin(-z).
uniform vec3 u_reference_dir;
// The line-width rule's three constants, uploaded from src/util/annotation_line_width.hpp rather
// than written here: GLSL cannot include the header, and the alternative — the same three digits
// spelled a second time in this string — is exactly the drift the header exists to prevent.
uniform float u_line_fwidth_min_deg;
uniform float u_line_fwidth_max_deg;
uniform float u_line_half_width_px;
uniform vec3 u_horizon_color;
uniform vec3 u_grid_color;
uniform vec3 u_sun_circles_color;
uniform vec3 u_view_dist_color;
uniform float u_horizon_alpha;
uniform float u_grid_alpha;
uniform float u_sun_circles_alpha;
uniform float u_view_dist_alpha;

// Sky reference-point ring marker uniforms. One array slot per core marker id
// (LUMICE_ANNOTATION_MARKER_*), so the CPU uploads all six in two calls and the
// loop below needs no count.
// Screen positions are center-origin, y-up, in pixels — same convention as
// `pos = v_ndc * u_resolution * 0.5` (see main()). Sentinel (-9999, -9999)
// trivially fails the distance test so that ring is skipped, which is how a
// marker the user switched off is expressed: there is no enable uniform.
// The literal 6 is LUMICE_ANNOTATION_MARKER_COUNT; GLSL cannot include the
// header, so a static_assert after this string pins the two spellings together.
uniform vec2 u_marker_screen_pos[6];
uniform vec3 u_marker_color[6];
uniform float u_markers_radius_px;
uniform float u_markers_alpha;

// Lens border uniforms: the outline of the projection's own valid image circle
// (where the inverse function's domain guard flips w from 1 to 0). Purely a
// function of u_lens_type / u_fov / u_resolution, so the shader derives the
// radius itself rather than taking a CPU-precomputed screen position.
uniform int u_show_lens_border;
uniform vec3 u_lens_border_color;
uniform float u_lens_border_alpha;

const float PI = 3.14159265358979323846;

// Source-texture semantics. Mirrors PreviewRenderer::TextureMode (preview_renderer.hpp) — the two
// enumerations are one contract and their values must stay in step.
//
//   kTexModeSrgbComposited  8-bit sRGB texels that ALREADY carry the sky colour, drawn as they are.
//                           Two producers: ClearTexture()'s 1x1 black pixel, and a .lmc written
//                           before the format carried radiance-only textures (v <= 3). Neither can
//                           be corrected here: the sky is summed into the texel and un-summing it
//                           is not invertible where the bake clipped.
//   kTexModeXyz             float XYZ radiance: the live simulation, and a .lmc from format v5 on,
//                           which stores the live frame's floats and reopens through this branch.
//   kTexModeSrgbRadiance    8-bit sRGB texels carrying the halo's radiance ALONE, exposure already
//                           applied, no sky. Everything a display still owes the picture — the
//                           lens's relative illumination, then the sky — is applied below, by the
//                           same two steps and in the same order as the XYZ branch. Producers: the
//                           raypath-colour composite the server bakes, and a v4 .lmc.
const int kTexModeSrgbComposited = 0;
const int kTexModeXyz = 1;
const int kTexModeSrgbRadiance = 2;

// Algorithm synced with CPU: src/util/color_space.hpp (GamutClipXyz + XyzToLinearRgb + LinearToSrgb)
// Matrix values from src/util/color_data.hpp kXyzToRgb (C++ row-major → GLSL column-major)
const mat3 kXyzToRgb = mat3(
     3.2404542, -0.9692660,  0.0556434,   // column 0
    -1.5371385,  1.8760108, -0.2040259,   // column 1
    -0.4985314,  0.0415560,  1.0572252    // column 2
);
const vec3 kWhitePointD65 = vec3(0.95047, 1.00000, 1.08883);

// The XYZ->sRGB chain, deliberately in two halves with a seam between them.
//
// The seam is where the background colour is added, and it has to be there: the addition is a
// radiance sum, so it belongs in LINEAR RGB, before the clamp and before the sRGB transfer curve.
// Add it after the gamma instead and a pixel carrying no halo energy comes back gamma-encoded a
// second time, i.e. NOT the colour the user picked. Keep the two calls adjacent at every call
// site — the halves are not independently meaningful.
//
// CPU equivalents: lumice::XyzToSrgbUint8(..., background) in src/util/color_space.hpp composes
// the same two halves around the same seam, and so does RenderConsumer::PostSnapshot's
// use_real_color branch (src/server/render.cpp). Three implementations of one chain: one has to be
// GLSL, and the other two are separate because the render loop writes bytes as it composites while
// the batch converter is handed a finished buffer. Nothing enforces that they agree; the pixel
// comparison in test/gui/functional/test_preview_background.cpp is what holds this one to the CPU
// one.
//
// The .lmc bake is NOT one of them any more: it reaches the background-free overload, because the
// sky is a setting rather than part of the stored picture and joins right here instead. See
// app.cpp's RefreshCpuTextureForSave and PreviewRenderer::TextureMode.
)glsl"
// MSVC caps a single string literal at 16384 bytes (C2026); the shader source is split into
// adjacent literals the compiler concatenates back into the identical bytes. Split points are
// function boundaries, picked when a segment would cross the cap — see check_policies.py's
// msvc-string-literal-limit rule.
R"glsl(
// Gamut clip + XYZ->RGB matrix. NOT clamped and NOT gamma-encoded: the background composites here.
vec3 xyzToLinearRgb(vec3 xyz) {
    // Normalize by accumulated intensity
    xyz *= u_intensity_scale;
    // Gray point (D65 white scaled by luminance Y)
    vec3 gray = kWhitePointD65 * xyz.y;
    // Gamut clipping: scale color toward gray to keep RGB in [0,1]
    float s = 1.0;
    vec3 diff = xyz - gray;
    mat3 kXyzToRgbT = transpose(kXyzToRgb);
    for (int j = 0; j < 3; j++) {
        float a = dot(-gray, kXyzToRgbT[j]);
        float b = dot(diff, kXyzToRgbT[j]);
        if (a * b > 0.0 && a / b < s) s = a / b;
    }
    xyz = diff * s + gray;
    // XYZ→RGB matrix multiply. The clamp the CPU sibling applies here is a no-op after the gamut
    // clip above (the clip is defined as the scaling that lands this product inside [0,1]); the
    // clamp that matters runs in clampAndGamma, after the background has been added.
    return kXyzToRgb * xyz;
}

// The inverse of clampAndGamma's transfer curve, for the one branch that starts from sRGB BYTES
// and still has linear-light work left to do. Mirrors lumice::SrgbToLinear (src/util/color_space.hpp).
vec3 srgbToLinear(vec3 srgb) {
    return mix(srgb / 12.92, pow((srgb + 0.055) / 1.055, vec3(2.4)), step(0.04045, srgb));
}

// Clamp + sRGB gamma (branchless via mix+step). The second half of the chain above.
vec3 clampAndGamma(vec3 rgb) {
    rgb = clamp(rgb, 0.0, 1.0);
    return mix(rgb * 12.92, 1.055 * pow(rgb, vec3(1.0/2.4)) - 0.055, step(0.0031308, rgb));
}

// The subtractive (u_tone == 1) operator: ink on paper, replacing the additive `radiance + sky`
// wherever it applies.
//
// HAND-TRANSCRIBED from src/util/ink_transfer.hpp (kInkGamma / InkOpticalDensity /
// InkTransmittance), which is the authority. GLSL cannot #include a C++ header, so this is a copy
// and not a shared implementation — the same arrangement, and for the same reason, as the relIllum*
// block's relationship to src/gui/preview_jacobian.hpp. kInkGamma below MUST equal
// lumice::kInkGamma; nothing in the build enforces that, and what does instead is
// test/gui/functional/test_preview_print_mode.cpp, which renders through this shader and compares
// against values the C++ function produced.
//
// `e` is the exposed scalar — CIE Y after u_intensity_scale and the lens's relative illumination —
// which is the same quantity the CLI calls xyz[1] in RenderConsumer::PostSnapshot.
const float kInkGamma = 11.0;
vec3 subtractiveInk(float e, vec3 paper) {
    // GLSL has no log10 builtin; log(x)/log(10) is the whole of the difference from the C++ line.
    float density = kInkGamma * log(1.0 + max(e, 0.0)) / log(10.0);
    return paper * pow(10.0, -density);
}

// The channel-B-R display mode (u_display_mode == 1): halo-only post-gamma sRGB B - R at the same
// exposure and view, as a grey offset — mid grey is zero, bluer is lighter, redder is darker.
//
// HAND-TRANSCRIBED from src/util/channel_math.hpp (kChannelBrGain / ChannelMathBrGray), which is
// the authority; it MUST equal that function, and kChannelBrGain below MUST equal
// lumice::kChannelBrGain. GLSL cannot #include a C++ header, so this is a copy, held to the C++
// one by deterministic GL pixel oracles in preview_background and the output-routing parity tests.
const float kChannelBrGain = 2.0;
float channelMathBrGray(float r_srgb, float b_srgb) {
    return clamp(0.5 + kChannelBrGain * (b_srgb - r_srgb), 0.0, 1.0);
}

// One annotation layer composited over what is already there.
//
// Structurally the same as BlendAnnotation() in src/server/render.cpp — one tone branch, the same
// algebra, and print reading no colour at all — but NOT signature-compatible, and deliberately so:
// this side keeps its own two independent multipliers (the edge falloff `t` and the layer's alpha)
// where the CPU caller has already folded them into one. Aligning the signatures would mean
// changing one side's arithmetic to match the other's spelling.
//
// print multiplies by (1 - t*alpha), i.e. the existing opacity IS the ink coverage. That is what
// makes an annotation visible on white paper by construction rather than by picking a colour that
// contrasts, and why print needs no per-mode palette.
vec3 blendAnnotationColor(vec3 base, vec3 lineColor, float t, float alpha, int tone) {
    return (tone == 1) ? base * (1.0 - t * alpha) : mix(base, lineColor, t * alpha);
}

// Pure math: equal-area fisheye projection (matches C++ FisheyeEqualAreaForward).
// Input: direction components (dx, dy, dz) where dz is along pole axis.
// Output: normalized disc coordinates, r=1 at equator (r_scale=1.0).
vec2 fisheyeEAProject(float dx, float dy, float dz, float r_scale) {
  float k = r_scale / sqrt(1.0 + dz);
  return vec2(k * dx, k * dy);
}

// Layout + UV: convert normalized disc coords to texture UV.
// Matches C++ DualFisheyeToPixel convention (90-deg rotation + hemisphere mirroring).
vec2 dualFisheyeToUV(vec2 xy_norm, bool is_upper) {
  vec2 tex_res = vec2(textureSize(u_texture, 0));
  float short_res = min(tex_res.x * 0.5, tex_res.y);
  float R = short_res * 0.5;

  vec2 pixel;
  if (is_upper) {
    pixel = vec2(-xy_norm.y * R + tex_res.x * 0.5 - R,
                  xy_norm.x * R + tex_res.y * 0.5);
  } else {
    pixel = vec2( xy_norm.y * R + tex_res.x * 0.5 + R,
                  xy_norm.x * R + tex_res.y * 0.5);
  }
  // `pixel` is in core's binning convention (ProjectExitToPixel takes floor of it), so texel `px`
  // owns [px, px+1) and its centre is px + 0.5 — see doc/geometry/coordinate-convention.md §11. GL's own
  // addressing is stated in exactly those terms: `uv * tex_res` = px + 0.5 hits the centre of
  // texel px. So the conversion is the plain division and nothing else. Adding 0.5 here applies
  // the same half-texel centring a SECOND time, which does not read a different texel — it moves
  // every fragment onto a texel corner and turns every sample into a 2x2 bilinear average.
  return pixel / tex_res;
}
)glsl"
R"glsl(
// Convert world direction to dual equal-area fisheye UV (single hemisphere, no blend).
vec2 dirToDualFisheye(vec3 d) {
  vec3 sky = -d;
  bool is_upper = (sky.z >= 0.0);
  float z_hemi = is_upper ? sky.z : -sky.z;
  vec2 xy_norm = fisheyeEAProject(sky.x, sky.y, z_hemi, u_r_scale);
  return dualFisheyeToUV(xy_norm, is_upper);
}

// Sample dual fisheye texture with overlap blending in the equator zone.
vec3 sampleDualFisheye(vec3 world_dir) {
  vec3 sky = -world_dir;
  float z_abs = abs(sky.z);
  bool is_upper = (sky.z >= 0.0);

  if (u_max_abs_dz > 0.0 && z_abs < u_max_abs_dz) {
    // Overlap zone: blend primary and secondary hemispheres.
    // Tent weight: t=0.5 at equator (z_abs=0), t=0 at boundary (z_abs=max_abs_dz).
    // primary weight = 1-t, secondary weight = t.
    float t = (u_max_abs_dz - z_abs) / (2.0 * u_max_abs_dz);

    // Primary: same hemisphere, z_hemi >= 0
    float z_hemi_pri = is_upper ? sky.z : -sky.z;
    vec2 xy_pri = fisheyeEAProject(sky.x, sky.y, z_hemi_pri, u_r_scale);
    vec2 uv_pri = dualFisheyeToUV(xy_pri, is_upper);
    // Secondary: opposite hemisphere, z_hemi < 0 (past equator)
    float z_hemi_opp = is_upper ? -sky.z : sky.z;  // = -|sky.z| < 0
    vec2 xy_sec = fisheyeEAProject(sky.x, sky.y, z_hemi_opp, u_r_scale);
    vec2 uv_sec = dualFisheyeToUV(xy_sec, !is_upper);

    vec3 c1 = texture(u_texture, uv_pri).rgb;
    vec3 c2 = texture(u_texture, uv_sec).rgb;
    return mix(c1, c2, t);
  }

  // Non-overlap: single hemisphere
  vec2 uv = dirToDualFisheye(world_dir);
  return texture(u_texture, uv).rgb;
}

)glsl";

static const char* kProjectionShader = R"glsl(
// ============================ Relative illumination ==========================================
//
// The target lens's per-pixel solid angle, normalized to its on-axis value. The CLI bins rays into
// the lens it renders, so its pixels carry energy and the projection's Jacobian is baked in; this
// shader resamples an equal-area all-sky texture, whose texels carry radiance, so without this
// factor the preview is missing the projection's own natural vignetting. Every equal-area branch
// returns exactly 1, so equal-area previews are unchanged bit-for-bit.
//
// THIS IS THE LIVE COPY. src/gui/preview_jacobian.hpp mirrors it on the CPU and carries the full
// derivation, the reason the normalization is on-axis rather than against the source texture's
// texel solid angle, and the sources; test/unit-correctness/gui/test_preview_jacobian.cpp checks
// the mirror against numerical integration rather than against this file, so a transcription error
// made in both places still fails. Keep the two in lockstep.
const float kSingularityGuardPixels = 0.5;

float relIllumRectilinear(float rho, float focal) {
  float c = focal / sqrt(focal * focal + rho * rho);
  return c * c * c;  // cos^3(theta)
}

// type: 0=equal_area, 1=equidistant, 2=stereographic, 3=orthographic. r_norm is 1 at the image
// circle; img_radius is that circle in pixels and is read only by the orthographic clamp.
float relIllumFisheye(int type, float r_norm, float half_fov, float img_radius) {
  if (type == 0) {
    return 1.0;                      // equal area: constant by definition
  } else if (type == 1) {            // equidistant: sin(theta)/theta
    float theta = r_norm * half_fov;
    if (theta < 1e-3) return 1.0 - theta * theta / 6.0;  // series, not a division
    return sin(theta) / theta;
  } else if (type == 2) {            // stereographic: cos^4(theta/2)
    float t = r_norm * tan(half_fov * 0.5);
    float c = 1.0 / sqrt(1.0 + t * t);
    float c2 = c * c;
    return c2 * c2;
  }
  // orthographic: 1/cos(theta), held half a pixel inside the rim it diverges at.
  float r_max = img_radius > kSingularityGuardPixels ? 1.0 - kSingularityGuardPixels / img_radius : 0.0;
  float s = min(r_norm, r_max) * sin(half_fov);
  return 1.0 / sqrt(max(1.0 - s * s, 1e-12));
}

float relIllumEquirect(float lat) {
  return max(cos(lat), 0.0);
}

// `far_side` picks the root: the near (camera-facing) surface point, or the far one the same ray
// meets on its way out of the sphere (globeFarDir). The same Jacobian serves both, with |D mu - 1|:
// on the far side the map rho(psi) runs backwards, and the pixel's solid angle is the magnitude.
// CPU twin: RelIllumGlobe / RelIllumGlobeFar (src/gui/preview_jacobian.hpp).
float relIllumGlobeRoot(float rho, float focal, bool far_side) {
  // kGlobeCameraDist, repeated here for the same reason globeInverse repeats it.
  const float D = 4.0;
  float rho_limb = focal / sqrt(D * D - 1.0);
  float rho_max = max(rho_limb - kSingularityGuardPixels, 0.0);
  float k = min(rho, rho_max) / focal;
  float k2 = k * k;
  float disc = max(1.0 - k2 * (D * D - 1.0), 0.0);
  float mu = far_side ? (D * k2 - sqrt(disc)) / (k2 + 1.0) : (D * k2 + sqrt(disc)) / (k2 + 1.0);
  float denom = max(abs(D * mu - 1.0), 1e-12);
  float num = D - mu;
  return num * num * num / (denom * (D - 1.0) * (D - 1.0));
}

float relIllumGlobe(float rho, float focal) {
  return relIllumGlobeRoot(rho, focal, false);
}

#ifdef LUMICE_GLOBE_BACK_FADE
// Globe back-side fade weight — MUST MATCH lm_proj::GlobeBackFadeWeight
// (src/core/shared/projection_shared.h), which states the geometry, and its CPU mirror
// GlobeBackFadeWeight (src/gui/preview_jacobian.hpp). mu is the far-side point's hit_eye.z.
float globeBackFadeWeight(float mu, float fade) {
  if (!(fade > 0.0)) return 0.0;
  const float D = 4.0;
  float dist = sqrt(max(D * D + 1.0 - 2.0 * D * mu, 0.0));
  float depth = max(dist - sqrt(D * D - 1.0), 0.0);
  return exp(-depth / fade);
}
#endif

// Compute view direction from pixel for linear projection
// Returns false (via w component) if outside valid range
// `ri` returns the relative illumination at this pixel; see the block above.
vec4 linearInverse(vec2 pos, float half_fov, out float ri) {
  float short_edge = min(u_resolution.x, u_resolution.y);
  float focal = short_edge * 0.5 / tan(half_fov);
  ri = relIllumRectilinear(length(pos), focal);
  vec3 d = normalize(vec3(pos, -focal));
  return vec4(d, 1.0);
}

// Compute view direction for fisheye projections
// type: 0=equal_area, 1=equidistant, 2=stereographic, 3=orthographic
vec4 fisheyeInverse(vec2 pos, float half_fov, int type, out float ri) {
  float img_radius = min(u_resolution.x, u_resolution.y) * 0.5;  // short_edge/2 — matches Core's short_pix_/2
  float r = length(pos) / img_radius;
  ri = relIllumFisheye(type, r, half_fov, img_radius);

  float theta;
  if (type == 0) {        // equal area: r_norm = sin(θ/2) / sin(fov/4)
    float s = r * sin(half_fov * 0.5);
    if (s > 1.0) return vec4(0.0, 0.0, 0.0, 0.0);  // asin domain guard
    theta = 2.0 * asin(s);
  } else if (type == 1) { // equidistant: r_norm = θ / half_fov
    theta = r * half_fov;
    if (theta >= PI) return vec4(0.0, 0.0, 0.0, 0.0);
  } else if (type == 2) { // stereographic: r_norm = tan(θ/2) / tan(fov/4)
    theta = 2.0 * atan(r * tan(half_fov * 0.5));
  } else {                // orthographic (type == 3): r_norm = sin(θ) / sin(fov/2)
    float s = r * sin(half_fov);
    if (s > 1.0) return vec4(0.0, 0.0, 0.0, 0.0);  // asin domain guard
    theta = asin(s);
  }

  float phi = atan(pos.y, pos.x);
  return vec4(sin(theta) * cos(phi), sin(theta) * sin(phi), -cos(theta), 1.0);
}

// Dual fisheye: left circle = upper hemisphere, right circle = lower hemisphere
// Returns world-space direction (no view matrix needed) — matches Core convention.
// Core uses az = atan2(-d.y, -d.x) and pixel mapping with PI/2±az offset.
vec4 dualFisheyeInverse(vec2 pos, int type, out float ri) {
  float short_res = min(u_resolution.x * 0.5, u_resolution.y);
  float circle_radius = short_res * 0.5;
  ri = 1.0;  // set for real once the hemisphere, and so the local radius, is known

  // Left circle center at (-circle_radius, 0), right at (+circle_radius, 0)
  vec2 left_pos = pos - vec2(-circle_radius, 0.0);
  vec2 right_pos = pos - vec2(circle_radius, 0.0);

  float left_r = length(left_pos) / circle_radius;
  float right_r = length(right_pos) / circle_radius;

  bool in_left = left_r <= 1.0;
  bool in_right = right_r <= 1.0;
  if (!in_left && !in_right) return vec4(0.0, 0.0, 0.0, 0.0);

  vec2 use_pos = in_left ? left_pos : right_pos;
  float use_r = in_left ? left_r : right_r;
  // Each half of a dual fisheye images one hemisphere, so its half-FOV is fixed at pi/2 whatever
  // u_fov says, and the local radius is measured from that circle's own centre.
  ri = relIllumFisheye(type, use_r, PI * 0.5, circle_radius);

  float theta;
  float half_pi = PI * 0.5;
  if (type == 0) {        // equal area
    float s = use_r * sin(half_pi * 0.5);
    if (s > 1.0) return vec4(0.0, 0.0, 0.0, 0.0);
    theta = 2.0 * asin(s);
  } else if (type == 1) { // equidistant
    theta = use_r * half_pi;
  } else if (type == 2) { // stereographic
    theta = 2.0 * atan(use_r * tan(half_pi * 0.5));
  } else {                // orthographic (type == 3): dual fov is fixed 180°/hemi, sin(half_pi)=1
    // use_r is already normalised to [0, 1] via circle_radius division above.
    float s = use_r;
    if (s > 1.0) return vec4(0.0, 0.0, 0.0, 0.0);
    theta = asin(s);
  }

  // Inverse of Core's forward: pixel (x,y) uses cos(PI/2±az), sin(PI/2±az)
  // so phi_pixel = PI/2 + az (upper) or PI/2 - az (lower)
  float phi = atan(use_pos.y, use_pos.x);
  float st = sin(theta);
  vec3 d;
  if (in_left) {
    // Upper hemisphere: az = phi - PI/2
    // d = (-st*cos(az), -st*sin(az), -cos(theta))
    d = vec3(-st * sin(phi), st * cos(phi), -cos(theta));
  } else {
    // Lower hemisphere: az = PI/2 - phi
    // d = (-st*cos(az), -st*sin(az), cos(theta))
    d = vec3(-st * sin(phi), -st * cos(phi), cos(theta));
  }

  return vec4(d, 1.0);
}
)glsl"
R"glsl(
// Rectangular (equirectangular): always full-sky, returns world-space direction.
// Matches Core: scale = min(width/2, height) / PI
vec4 rectangularInverse(vec2 pos, out float ri) {
  float short_res = min(u_resolution.x * 0.5, u_resolution.y);
  float scale = short_res / PI;  // pixels per radian
  float lon = pos.x / scale;
  float lat = -pos.y / scale;
  ri = relIllumEquirect(lat);
  if (abs(lat) > PI * 0.5) return vec4(0.0, 0.0, 0.0, 0.0);

  vec3 d = vec3(-cos(lat) * cos(lon), -cos(lat) * sin(lon), -sin(lat));
  return vec4(d, 1.0);
}

// Globe (outside-in unit sphere): pinhole camera at (0,0,+D) in eye space looking
// toward -z; sphere of radius 1 sits at the origin. Returns the world-space unit
// vector pointing from the sphere center to the ray-sphere hit. needs_view_transform
// must be set false by the caller (u_view_matrix is applied here directly).
//
// kGlobeCameraDist = 4.0 chosen so default fov=30° gives sphere angular diameter
// ~28.96° (~96% of short edge), matching the crystal preview's zoom=1 distance scale.
// Treating hit_eye as a direction is valid because the sphere center coincides with
// the world origin (camera always looks at the sphere center); if that ever changes
// this function must switch to mat4 + explicit world-space ray-sphere intersection.
vec4 globeInverse(vec2 pos, float half_fov, out float ri) {
  // kGlobeCameraDist must match kGlobeCameraD in src/gui/gui_constants.hpp.
  const float kGlobeCameraDist = 4.0;
  float short_edge = min(u_resolution.x, u_resolution.y);
  float focal = short_edge * 0.5 / tan(half_fov);
  ri = relIllumGlobe(length(pos), focal);
  vec3 d = normalize(vec3(pos, -focal));  // ray dir in eye space

  // Solve |O + t*d|^2 = 1 with O = (0, 0, D):
  //   t^2 + 2*D*d.z*t + (D^2 - 1) = 0
  float b = kGlobeCameraDist * d.z;  // = dot(O, d)
  float c = kGlobeCameraDist * kGlobeCameraDist - 1.0;
  float disc = b * b - c;
  if (disc < 0.0) return vec4(0.0, 0.0, 0.0, 0.0);  // ray misses sphere
  float t = -b - sqrt(disc);                        // closest positive root (d.z < 0)
  if (t <= 0.0) return vec4(0.0, 0.0, 0.0, 0.0);    // hit behind camera

  vec3 hit_eye = vec3(0.0, 0.0, kGlobeCameraDist) + t * d;  // unit-length on sphere
  vec3 hit_world = u_view_matrix * hit_eye;
  return vec4(normalize(hit_world), 1.0);
}

// Whether a world direction passes the display clips: `visible` (the half-sky kept) AND `front`.
// ONE function for every direction a fragment shows — the near point the picture is sampled at and,
// on the globe, the far point seen through it — because each is shown or hidden by where IT points,
// never by the other one sharing its pixel.
//
// SYNC:visible-hemisphere-predicate — core states the same rule in C++, as lens_proj_build.hpp's
// VisibleByRange (`kUpper && wz > 0` -> not visible) and FrontVisible. GLSL cannot call into it, so
// the rule is written twice on purpose; the two are kept honest by test_visible_mask_gui_parity.cpp,
// which compares the resulting masks pixel by pixel across every lens type and all three values.
// Both sides are a DISPLAY clip and neither may become an energy cull.
bool dirPassesClip(vec3 world_dir) {
  // In equirect convention: lat = asin(-dz), lat > 0 means upper sky
  float lat = asin(clamp(-world_dir.z, -1.0, 1.0));
  bool keep = true;
  if (u_visible == 0 && lat < 0.0) keep = false;   // upper: discard lower hemisphere
  if (u_visible == 1 && lat > 0.0) keep = false;   // lower: discard upper hemisphere
  // u_front: discard back hemisphere (AND with base). u_view_matrix[2] = -forward
  // (see BuildViewMatrix), so dot > 0 means world_dir is behind the camera.
  if (u_front == 1 && dot(world_dir, u_view_matrix[2]) > 0.0) keep = false;
  return keep;
}

// The FAR surface point on the same ray as globeInverse — where the ray leaves the sphere — for the
// back-side fade. w = 1 only when the ray meets the sphere AND the point is not faded out; `ri`
// receives its relative illumination and `weight` its fade weight. Same solve as globeInverse,
// other root (-b + sqrt(disc)). Two callers: main(), for the far side's light, and overlayAuxLines,
// for the far side's curves (at the pixel and its two neighbours). Markers and labels never read
// it — they stay on the near side.
#ifdef LUMICE_GLOBE_BACK_FADE
vec4 globeFarDir(vec2 pos, float half_fov, out float ri, out float weight) {
  const float kGlobeCameraDist = 4.0;  // see globeInverse
  float short_edge = min(u_resolution.x, u_resolution.y);
  float focal = short_edge * 0.5 / tan(half_fov);
  ri = relIllumGlobeRoot(length(pos), focal, true);
  weight = 0.0;
  vec3 d = normalize(vec3(pos, -focal));
  float b = kGlobeCameraDist * d.z;
  float c = kGlobeCameraDist * kGlobeCameraDist - 1.0;
  float disc = b * b - c;
  if (disc < 0.0) return vec4(0.0, 0.0, 0.0, 0.0);
  float t = -b + sqrt(disc);
  if (t <= 0.0) return vec4(0.0, 0.0, 0.0, 0.0);
  vec3 hit_eye = vec3(0.0, 0.0, kGlobeCameraDist) + t * d;
  weight = globeBackFadeWeight(hit_eye.z, u_globe_back_fade);
  if (!(weight > 0.0)) return vec4(0.0, 0.0, 0.0, 0.0);
  return vec4(normalize(u_view_matrix * hit_eye), 1.0);
}
#endif

// THE inverse: the fragment-space position `pos` (centre-origin, y-up pixels) to the world
// direction the active lens images there, with w = 1 when the lens images it at all and 0 when
// it does not. Every branch below writes `ri`, the target lens's relative illumination at that
// pixel; an unknown u_lens_type leaves it at the identity, alongside the black w = 0 it already
// produces. `pos_ovl` receives the position in the overlay's own space: the core-pixel-inverse
// family (dual fisheye 4-6/9, rectangular 7) inverts Core's y-DOWN pixel layout
// (DualFisheyeToPixel / RectangularForward) while `pos` is y-UP (GL NDC), so for exactly that
// family the y is flipped — feeding y-up straight in would flip the display vertically against
// the CLI render. The view-matrix family (linear/fisheye/ortho/globe) is self-consistent and keeps
// raw pos. The same flip is mirrored in overlay_labels.cpp (PixelToWorldDir / WorldDirToPixel) so
// grid labels, markers and mouse interaction follow the flipped content. See scrum
// gui-lens-math-cli-alignment (owner: dual-fisheye/rectangular follow CLI).
//
// ONE function because it has TWO callers: main(), for the picture, and overlayAuxLines, which
// evaluates it at a fragment's right and lower neighbours to take the forward differences the
// annotation line rule is defined on. The direction a line is drawn from and the direction the
// picture is sampled at must be the same inverse, or the line lands beside the picture.
vec4 inverseWorldDir(vec2 pos, float half_fov, out float ri, out vec2 pos_ovl) {
  pos_ovl = pos;
  ri = 1.0;
  vec4 result = vec4(0.0, 0.0, 0.0, 0.0);
  bool needs_view_transform = true;
  if (u_lens_type == 0) {
    result = linearInverse(pos, half_fov, ri);
  } else if (u_lens_type >= 1 && u_lens_type <= 3) {
    result = fisheyeInverse(pos, half_fov, u_lens_type - 1, ri);
  } else if (u_lens_type >= 4 && u_lens_type <= 6) {
    pos_ovl = vec2(pos.x, -pos.y);
    result = dualFisheyeInverse(pos_ovl, u_lens_type - 4, ri);
    needs_view_transform = false;  // Core dual fisheye works in world space
  } else if (u_lens_type == 7) {
    pos_ovl = vec2(pos.x, -pos.y);
    result = rectangularInverse(pos_ovl, ri);
    needs_view_transform = false;  // Core rectangular works in world space
  } else if (u_lens_type == 8) {   // kLensTypeFisheyeOrthographic
    result = fisheyeInverse(pos, half_fov, 3, ri);
  } else if (u_lens_type == 9) {   // kLensTypeDualFisheyeOrthographic
    pos_ovl = vec2(pos.x, -pos.y);
    result = dualFisheyeInverse(pos_ovl, 3, ri);
    needs_view_transform = false;  // Core dual fisheye works in world space
  } else if (u_lens_type == 10) {  // kLensTypeGlobe
    result = globeInverse(pos, half_fov, ri);
    needs_view_transform = false;  // globeInverse already returned world-space dir
  }
  // else: unknown u_lens_type → result.w = 0 (black). static_assert in gui_state.hpp
  // pins kLensTypeCount so any out-of-range value is a compile-time catchable mismatch.
  if (result.w >= 0.5 && needs_view_transform) {
    result.xyz = u_view_matrix * result.xyz;
  }
  return result;
}

)glsl";

static const char* kFragmentAfterProjection = R"glsl(
// The local gradient of an angle field, clamped as src/util/annotation_line_width.hpp's rule
// clamps it — the one number the line's width and its edge are both measured in. `fw` is the
// caller's forward-difference sum against the right/bottom neighbour pixels (NOT the hardware
// fwidth() intrinsic — its 2x2-quad granularity was measured to miscount on rectilinear scenes,
// see overlayAuxLines below); the CIRCULAR field (azimuth) must additionally not take a raw
// difference across the seam, see wrapAngleDiffDeg below. CPU twin:
// mask_detail::LevelSetMaskFromField (src/core/lens_proj_build.hpp), the same forward-difference
// shape.
float lineGradientDeg(float fw) {
  return clamp(fw, u_line_fwidth_min_deg, u_line_fwidth_max_deg);
}

// An angle difference in degrees folded into [-180, 180). For CIRCULAR fields only (azimuth): a
// naive difference across the +/-180 seam reads the 179 -> -179 step as 358 instead of 2, both
// when the local gradient is measured (fwidth would report a wall of gradient along the seam and
// the clamp would turn it into a 3 px band down the anti-meridian) and when the distance to a
// level is measured. Altitude ([-90, 90]) and angular distance ([0, 180]) have no seam and must
// NOT go through this. CPU twin: mask_detail::WrapAngleDiffDeg, whose range is (-180, 180]; the
// two differ only at exactly 180, where both sides take the absolute value and agree.
float wrapAngleDiffDeg(float d) {
  return d - 360.0 * floor((d + 180.0) / 360.0);
}

// Whether a fragment at distance `d_deg` (already wrapped for a circular field) from a level is
// on the line, given the local gradient `grad` in degrees per pixel: the SET the CPU marks,
// |d| < grad * kAnnotationLineHalfWidthPx, and nothing more. 1 or 0.
//
// HARD-EDGED ON PURPOSE, and the purpose is measured. Two antialiased shapes were tried here and
// both were rejected by test/gui/parity/test_gui_cli_export_parity.cpp, whose subject is that the
// preview and the CLI draw ONE line: a smoothstep from the level out to the half-width (the
// profile this function had before the curves were ever rasterized by core) integrates to half the
// CLI's ink and read 18 / 15 / 20 dB against thresholds of 27 / 27 / 34 on its three scenes; the
// pixel-coverage ramp of the same set (50 % contour on the CLI's edge, same ink) recovered the
// positions to under half a pixel but still read 24 / 15 / 27 — a one-pixel ramp on both edges of
// every line is a few thousand half-covered pixels per frame against the CLI's hard band. The
// hard set is what the mask sampled NEAREST used to give, so it is also the look the preview has
// shipped with; antialiasing it is a decision for both renderers at once (src/server/render.cpp
// PostSnapshot composites the same set with no ramp), not for this side alone.
float lineCoverage(float d_deg, float grad) {
  return abs(d_deg) < grad * u_line_half_width_px ? 1.0 : 0.0;
}

// Element i of the packed grid level array.
float gridLevelAt(int i) {
  return u_grid_levels_deg[i >> 2][i & 3];
}

// The distance, in degrees, from `field_deg` to the NEAREST level in u_grid_levels_deg[begin, end),
// a range sorted ascending (the uploader sorts; see UploadGridLevels). Coverage falls off
// monotonically with distance, so the largest coverage over a list is the coverage of its nearest
// level, and finding that is a binary search — a dozen steps — where a linear pass over the list
// is up to a thousand (720 meridians and 360 parallels at the narrowest field of view, per
// fragment, per frame; measured at 1600x1200 on an M2 Max as +10 ms a frame linearly, most of a
// 60 fps budget spent on an edge case).
//
// `circular` folds the field onto the azimuth circle: the nearest level may then be the range's
// first or last element reached across the +/-180 seam, so those two are tried through
// wrapAngleDiffDeg on top of the two neighbours the search finds.
float nearestGridLevelDistDeg(float field_deg, int begin, int end, bool circular) {
  if (end <= begin) return 1e9;
  // First index in [begin, end) whose level is >= field_deg. 12 iterations halve 1024 to one.
  int lo = begin;
  int hi = end;
  for (int it = 0; it < 12; ++it) {
    if (lo >= hi) break;
    int mid = (lo + hi) >> 1;
    if (gridLevelAt(mid) < field_deg) lo = mid + 1; else hi = mid;
  }
  float d = 1e9;
  if (lo < end) d = min(d, abs(field_deg - gridLevelAt(lo)));
  if (lo > begin) d = min(d, abs(field_deg - gridLevelAt(lo - 1)));
  if (circular) {
    d = min(d, abs(wrapAngleDiffDeg(field_deg - gridLevelAt(begin))));
    d = min(d, abs(wrapAngleDiffDeg(field_deg - gridLevelAt(end - 1))));
  }
  return d;
}

// Sky reference-point ring markers — drawn last so they sit on top of all other overlays. Their
// positions are NOT derived from the fragment's direction: a marker is a named direction core
// projects (LUMICE_ComputeAnnotationAnchors), handed over as pixel-space uniforms. CPU passes
// sentinel (-9999, -9999) for a marker that is switched off, offscreen or behind the camera; the
// distance test rejects it naturally, which is why there is no per-marker enable to read.
//
// A constant-bound loop over the whole id space rather than six unrolled blocks: the bound is a
// compile-time constant so there is no dynamic branching, and the alternative is the same
// arithmetic written six times. Order is id order, and the LATER id wins where two rings overlap
// — the same last-writer rule the three curve families follow.
//
// pos_pix: pixel-space position of the current fragment, center-origin (0,0), y-up. Matches the
// CPU helper ProjectWorldDirToScreen for marker overlays.
vec3 overlayMarkers(vec3 color, vec2 pos_pix) {
  const float kRingHalfWidthPx = 1.5;
  for (int i = 0; i < 6; ++i) {
    float d = length(pos_pix - u_marker_screen_pos[i]);
    float t = 1.0 - smoothstep(0.0, kRingHalfWidthPx, abs(d - u_markers_radius_px));
    color = blendAnnotationColor(color, u_marker_color[i], t, u_markers_alpha, u_tone);
  }
  return color;
}

)glsl"
R"glsl(
// The four curve families (grid, sun circles, view circles, horizon) at one direction, composited
// over `color` with every family's alpha multiplied by `alpha_scale`.
//
// world_dir: the unit world-space direction the curves are evaluated at, in the convention every
// annotation direction uses — altitude = asin(-z), azimuth = atan2(-y, -x). nx / ny: the directions
// at the fragment's right and lower neighbours (w = 1 where imaged), from which the local gradient
// of each field is taken — see the comment on the forward difference below.
//
// TWO callers, both in overlayAuxLines: the near side at alpha_scale 1 (the picture's own
// direction, exactly the lines the preview always drew) and, in the LUMICE_GLOBE_BACK_FADE variant
// only, the globe's far side at alpha_scale = its fade weight, so the far side's lines fade with
// the same curve as its light. Markers are not drawn here: they are named directions projected by
// core onto the near side, and the far side draws none (nor does the CLI).
vec3 drawAuxCurves(vec3 world_dir, vec4 nx, vec4 ny, vec3 color, float alpha_scale) {
  const float DEG = 180.0 / PI;

  // The four angle fields. Each formula is the shader-side twin of a core function, named here so
  // a change on either side has somewhere to look:
  //   altitude_deg      mask_detail::AltitudeDeg           (src/core/lens_proj_build.hpp)
  //   azimuth_deg       annotation::AzimuthDegOfDir        (src/core/annotation_overlay.cpp)
  //   angular_dist_deg  annotation::AngularDistDegOfDir    (src/core/annotation_overlay.cpp)
  //   view_dist_val     the same function about the camera forward (RequestFrame::forward there)
  // The optical axis is -u_view_matrix[2]: BuildViewMatrix's third column is -forward (the front
  // clip in main() reads the same column for the same fact). For the full-sky lenses, whose
  // picture skips the view matrix, the panel pins the view angles to zero every frame, so this
  // column is the same axis core derives from the zeroed angles it is asked with.
  float altitude_deg = asin(clamp(-world_dir.z, -1.0, 1.0)) * DEG;
  float azimuth_deg = atan(-world_dir.y, -world_dir.x) * DEG;
  float angular_dist_deg = acos(clamp(dot(world_dir, u_reference_dir), -1.0, 1.0)) * DEG;
  vec3 view_axis = -u_view_matrix[2];
  float view_dist_val = acos(clamp(dot(world_dir, view_axis), -1.0, 1.0)) * DEG;

  // The local gradient of each field, in degrees per pixel, as the FORWARD DIFFERENCE against the
  // right and lower neighbours — the same two differences, against the same two pixels, that
  // mask_detail::LevelSetMaskFromField takes on the CPU (src/core/lens_proj_build.hpp), with the
  // same three rules: a neighbour the lens does not image contributes nothing; the last column /
  // row differences backwards instead; a pixel with no imaged neighbour at all gets no line.
  //
  // NOT the hardware fwidth(). That derivative is taken across a 2x2 quad, so half the fragments
  // read a backward difference and the quad's helper invocations reach outside the lens's domain
  // at its rim; each is a pixel here and there that one renderer lights and the other does not,
  // and against a hard-edged line a whole-frame comparison of the two arms pays for every such
  // pixel at full amplitude (measured on the rectilinear parity scene as 213 pixels for 1.8 dB).
  // Three inverse projections per fragment instead of one is the price of the two evaluators
  // agreeing on which pixels are the line, and it is a few trig calls.
  if (nx.w < 0.5 && ny.w < 0.5) {
    // No local scale can be measured here, so no curve is drawn — the CPU's rule, and the one
    // that keeps a horizon out of a corner that images nothing.
    return color;
  }
  float fw_alt = 0.0;
  float fw_az = 0.0;
  float fw_dist = 0.0;
  float fw_view = 0.0;
  if (nx.w >= 0.5) {
    fw_alt += abs(asin(clamp(-nx.z, -1.0, 1.0)) * DEG - altitude_deg);
    fw_az += abs(wrapAngleDiffDeg(atan(-nx.y, -nx.x) * DEG - azimuth_deg));
    fw_dist += abs(acos(clamp(dot(nx.xyz, u_reference_dir), -1.0, 1.0)) * DEG - angular_dist_deg);
    fw_view += abs(acos(clamp(dot(nx.xyz, view_axis), -1.0, 1.0)) * DEG - view_dist_val);
  }
  if (ny.w >= 0.5) {
    fw_alt += abs(asin(clamp(-ny.z, -1.0, 1.0)) * DEG - altitude_deg);
    fw_az += abs(wrapAngleDiffDeg(atan(-ny.y, -ny.x) * DEG - azimuth_deg));
    fw_dist += abs(acos(clamp(dot(ny.xyz, u_reference_dir), -1.0, 1.0)) * DEG - angular_dist_deg);
    fw_view += abs(acos(clamp(dot(ny.xyz, view_axis), -1.0, 1.0)) * DEG - view_dist_val);
  }
  float grad_alt = lineGradientDeg(fw_alt);
  float grad_az = lineGradientDeg(fw_az);
  float grad_dist = lineGradientDeg(fw_dist);
  float grad_view = lineGradientDeg(fw_view);

  // Coordinate grid — drawn first so the other lines overlay on top. Parallels and meridians share
  // one colour and one alpha, so their coverages are merged before the blend, exactly as the CLI
  // composites the two families' masks into one layer.
  if (u_show_grid != 0) {
    int ec = u_elevation_count;
    float t = lineCoverage(nearestGridLevelDistDeg(altitude_deg, 0, ec, false), grad_alt);
    t = max(t, lineCoverage(nearestGridLevelDistDeg(azimuth_deg, ec, ec + u_longitude_count, true), grad_az));
    color = blendAnnotationColor(color, u_grid_color, t, u_grid_alpha * alpha_scale, u_tone);
  }

  // Circles of constant angular distance from u_reference_dir. A linear pass: the list is at most
  // kMaxAnnotationCircles long, so there is nothing for a search to save.
  if (u_show_sun_circles != 0) {
    float t = 0.0;
    for (int i = 0; i < u_angular_dist_count; ++i) {
      t = max(t, lineCoverage(angular_dist_deg - u_angular_dist_deg[i >> 2][i & 3], grad_dist));
    }
    color = blendAnnotationColor(color, u_sun_circles_color, t, u_sun_circles_alpha * alpha_scale, u_tone);
  }

  // Circles of constant angular distance from the optical axis. AFTER the sun circles and BEFORE
  // the horizon, which is the CLI's compositing order for the same layers (render.cpp
  // PostSnapshot: grid, sun circles, view circles, outline, markers) — the order is a parity
  // contract, not a taste: where two families cross, whichever is blended second wins the pixel,
  // and the two renderers have to agree on which that is.
  if (u_show_view_dist != 0) {
    float t = 0.0;
    for (int i = 0; i < u_view_dist_count; ++i) {
      t = max(t, lineCoverage(view_dist_val - u_view_dist_deg[i >> 2][i & 3], grad_view));
    }
    color = blendAnnotationColor(color, u_view_dist_color, t, u_view_dist_alpha * alpha_scale, u_tone);
  }

  // Horizon line (altitude = 0) — drawn last of the curves so it's most visible.
  if (u_show_horizon != 0) {
    color = blendAnnotationColor(color, u_horizon_color, lineCoverage(altitude_deg, grad_alt), u_horizon_alpha * alpha_scale, u_tone);
  }

  return color;
}

// Overlay auxiliary lines on top of final_color.
//
// world_dir: this fragment's unit world-space direction (the one the picture was sampled at).
// The caller only reaches here for a fragment the lens images and where at least one of its
// directions passes the clips. near_on / far_on say which: the near side's curves and the markers
// are drawn only when the near direction passes (the CLI's `drawable` mask), and on the globe the
// far side's curves only when the FAR direction itself passes (the CLI's far_side masks) — each
// direction by its own verdict, as main() shows each side's light.
// pos: the fragment's raw position (centre-origin, y-up), from which the neighbours' directions
// are re-derived; pos_pix: the same in the overlay's space (flipped for the CPI family), which is
// what the marker positions are stated in.
vec3 overlayAuxLines(vec3 world_dir, vec3 color, vec2 pos_pix, vec2 pos, float half_fov, bool near_on, bool far_on) {
  // The right and lower neighbours drawAuxCurves differences against. "Lower" is the next IMAGE
  // row, which in this y-up fragment space is pos.y - 1; the last row/column test is the CPU's
  // `px + 1 < width` written in centre-origin coordinates.
  vec2 half_res = u_resolution * 0.5;
  vec2 step_x = vec2(pos.x + 0.5 < half_res.x ? 1.0 : -1.0, 0.0);
  vec2 step_y = vec2(0.0, -(pos.y - 0.5 > -half_res.y ? 1.0 : -1.0));
#ifdef LUMICE_GLOBE_BACK_FADE
  // The globe's far side, seen through the near one: its curves are drawn FIRST, faded by the same
  // weight its light is (lm_proj::GlobeBackFadeWeight and its mirrors), so the near side's lines —
  // drawn next, at full alpha — win any pixel both touch. The CLI composites the same order
  // (src/server/render.cpp, the far-side layers before their near-side twins). Relative
  // illumination is not applied: it is an energy term, and a line is not energy.
  if (u_lens_type == 10 && u_globe_back_fade > 0.0) {
    float back_ri_unused;
    float back_w = 0.0;
    float nb_w_unused;
    vec4 back = globeFarDir(pos, half_fov, back_ri_unused, back_w);
    if (far_on && back.w >= 0.5) {
      vec4 bnx = globeFarDir(pos + step_x, half_fov, back_ri_unused, nb_w_unused);
      vec4 bny = globeFarDir(pos + step_y, half_fov, back_ri_unused, nb_w_unused);
      color = drawAuxCurves(back.xyz, bnx, bny, color, back_w);
    }
  }
#endif
  if (!near_on) {
    return color;
  }
  float ri_unused;
  vec2 ovl_unused;
  vec4 nx = inverseWorldDir(pos + step_x, half_fov, ri_unused, ovl_unused);
  vec4 ny = inverseWorldDir(pos + step_y, half_fov, ri_unused, ovl_unused);
  color = drawAuxCurves(world_dir, nx, ny, color, 1.0);
  return overlayMarkers(color, pos_pix);
}

// Lens border: outline the projection's own valid image region — the locus where
// the active inverse function's domain guard flips w from 1 to 0. Only the fisheye
// family has one: the single-lens asin guards (equal-area / equidistant /
// orthographic) and dual fisheye's hard circle clip. linear / single-lens
// stereographic / rectangular / globe have no such circle and are skipped.
// Keep in lockstep with fisheyeInverse / dualFisheyeInverse above; the CPU-side
// classifier is LensHasBorder() in gui_constants.hpp.
//
// pos: center-origin, y-up pixel position (the raw `pos` of main(), NOT pos_ovl).
// The dual-fisheye y flip is irrelevant here because both border circles are
// centered on y = 0 and are therefore symmetric under it.
vec3 overlayLensBorder(vec3 color, vec2 pos, float half_fov) {
  if (u_show_lens_border == 0) return color;

  const float kBorderHalfWidthPx = 1.5;
  const float kMinSin = 1e-4;  // below this the boundary radius diverges: no border exists

  if ((u_lens_type >= 4 && u_lens_type <= 6) || u_lens_type == 9) {
    // Dual fisheye: two hard-clipped circles, identical for all four variants
    // (the clip lives in dualFisheyeInverse before the per-type theta branch).
    float short_res = min(u_resolution.x * 0.5, u_resolution.y);
    float circle_radius = short_res * 0.5;
    float dl = abs(length(pos - vec2(-circle_radius, 0.0)) - circle_radius);
    float tl = 1.0 - smoothstep(0.0, kBorderHalfWidthPx, dl);
    color = blendAnnotationColor(color, u_lens_border_color, tl, u_lens_border_alpha, u_tone);
    float dr = abs(length(pos - vec2(circle_radius, 0.0)) - circle_radius);
    float tr = 1.0 - smoothstep(0.0, kBorderHalfWidthPx, dr);
    color = blendAnnotationColor(color, u_lens_border_color, tr, u_lens_border_alpha, u_tone);
    return color;
  }

  // Single-lens fisheye family. r_boundary is in units of img_radius, read straight
  // off the corresponding guard in fisheyeInverse:
  //   equal area   (type 0): s = r * sin(half_fov/2) > 1  ->  r = 1 / sin(half_fov/2)
  //   equidistant  (type 1): theta = r * half_fov >= PI   ->  r = PI / half_fov
  //   orthographic (type 3): s = r * sin(half_fov) > 1    ->  r = 1 / sin(half_fov)
  float r_boundary = 0.0;
  if (u_lens_type == 1) {
    float sn = sin(half_fov * 0.5);
    if (sn < kMinSin) return color;
    r_boundary = 1.0 / sn;
  } else if (u_lens_type == 2) {
    if (half_fov < kMinSin) return color;
    r_boundary = PI / half_fov;
  } else if (u_lens_type == 8) {
    float sn = sin(half_fov);
    if (sn < kMinSin) return color;
    r_boundary = 1.0 / sn;
  } else {
    return color;  // 0 linear, 3 stereographic, 7 rectangular, 10 globe: no border
  }

  float img_radius = min(u_resolution.x, u_resolution.y) * 0.5;
  float radius_px = img_radius * r_boundary;
  float d = abs(length(pos) - radius_px);
  float t = 1.0 - smoothstep(0.0, kBorderHalfWidthPx, d);
  return blendAnnotationColor(color, u_lens_border_color, t, u_lens_border_alpha, u_tone);
}
)glsl"
R"glsl(
out vec4 frag_color;

void main() {
  vec2 pos = v_ndc * u_resolution * 0.5;  // Convert NDC [-1,1] to pixel offset from center (y-up)
  float half_fov = u_fov * 0.5 * PI / 180.0;

  // The inverse, and the overlay-space position (see inverseWorldDir for the y flip it applies
  // to the core-pixel-inverse family).
  vec2 pos_ovl = pos;  // pixel pos handed to overlayAuxLines (flipped for the CPI family)
  float rel_illum = 1.0;
  vec4 result = inverseWorldDir(pos, half_fov, rel_illum, pos_ovl);

  // Eliminated early returns so bg mixing can always execute at the end.
  //
  // The starting value is this mode's ZERO-ENERGY colour, and it stands for every pixel the branch
  // below leaves alone: outside the lens's image circle, in the half-sky `visible` discards, behind
  // the camera under `front`. Under print that is bare paper — which is what subtractiveInk(0.0,
  // u_paper) returns, written here in its already-simplified form since 10^0 is 1. The CLI states
  // the same rule at its own two spellings (the masked-pixel branch and FillZeroEnergyImage in
  // src/server/render.cpp).
  vec3 final_color = (u_tone == 1) ? clampAndGamma(u_paper) : vec3(0.0);
  vec3 world_dir = vec3(0.0);
  bool pixel_visible = false;
  // On the globe, whether this pixel's FAR direction passes the clips by itself. Always false
  // outside the LUMICE_GLOBE_BACK_FADE variant (a constant there, so every test of it folds away).
#ifdef LUMICE_GLOBE_BACK_FADE
  bool far_pixel_visible = false;
#else
  const bool far_pixel_visible = false;
#endif

  if (result.w >= 0.5) {
    world_dir = result.xyz;

    // The near direction's clip. A DISPLAY clip, never an energy cull: the branch below never
    // samples the texture for an excluded direction, and core's twin (SYNC:visible-mask-zero)
    // zeroes one that already accumulated.
    pixel_visible = dirPassesClip(world_dir);

    // Globe back-side fade: the far side of the sphere, seen THROUGH the near side, added as
    // light (a halo is an emitter, not a surface, so the near side does not occlude it). Its
    // texel and its weight (relative illumination x fade) join the near side's inside the
    // linear-energy sums below, before the gamut clip and the tone operator, which is exactly
    // where the CLI's forward projection adds the same rays into the same pixel
    // (lm_proj::ProjectExitToPixel, globe branch). Clipped by the FAR direction's own verdict —
    // one pixel images two sky directions here, generally at different altitudes, and each is
    // shown or hidden on its own (SYNC:globe-far-own-visibility, RenderConsumer::PostSnapshot):
    // under `upper`, looking down, the near point can be in the kept half and the far one not, or
    // the other way round. Not in kTexModeSrgbComposited, whose texels carry a baked sky that a
    // second sample would add twice. Compiled in only for the LUMICE_GLOBE_BACK_FADE variant (see
    // kFragmentShader); in the other one `has_back` is a constant false and the arms below that
    // read it fold to the pre-fade expressions.
#ifdef LUMICE_GLOBE_BACK_FADE
    bool has_back = false;
    vec3 back_tex = vec3(0.0);
    float back_gain = 0.0;
    if (u_lens_type == 10 && u_globe_back_fade > 0.0) {
      float back_ri = 1.0;
      float back_w = 0.0;
      vec4 back = globeFarDir(pos, half_fov, back_ri, back_w);
      far_pixel_visible = back.w >= 0.5 && dirPassesClip(back.xyz);
      if (far_pixel_visible && u_sim_valid == 1 && u_tex_mode != kTexModeSrgbComposited) {
        has_back = true;
        back_tex = sampleDualFisheye(back.xyz);
        if (u_tex_mode == kTexModeXyz) {
          back_tex *= u_xyz_scale;
        }
        back_gain = back_ri * back_w;
      }
    }
#else
    const bool has_back = false;
    const vec3 back_tex = vec3(0.0);
    const float back_gain = 0.0;
#endif

    // The near side's texel and sky exist only where the near direction passes; a pixel whose near
    // point is clipped but whose far point is not shows the far side's light alone, over the
    // zero-energy colour (the sky belongs to the near hemisphere: SYNC:globe-far-own-visibility).
    if (pixel_visible || has_back) {
      // The near texel. Outside the LUMICE_GLOBE_BACK_FADE variant this block is only ever entered
      // with pixel_visible true, so the guard exists in the variant alone and the other program's
      // text is the pre-fade one.
      vec3 tex_color = vec3(0.0);
#ifdef LUMICE_GLOBE_BACK_FADE
      if (pixel_visible) {
#endif
        // The sim-valid gate is the no-image contract's render-side half: while it is 0 the
        // fetch does not run at all, tex_color stays the zero-energy vec3(0.0), and every branch
        // below behaves exactly as it would for an empty frame (paper under print, black
        // otherwise). See sim_layer_valid_ in the header.
        if (u_sim_valid == 1) {
          tex_color = sampleDualFisheye(world_dir);
          if (u_tex_mode == kTexModeXyz) {
            // Undo the float16 storage scale here, once, so every consumer below — the gamut clip,
            // the print branch's tex_color.y — reads the frame's own linear XYZ.
            tex_color *= u_xyz_scale;
          }
        }
#ifdef LUMICE_GLOBE_BACK_FADE
      }
#endif
      if (u_tex_mode != kTexModeSrgbComposited) {
        // Linear-light radiance for this pixel. The two source formats decode differently and
        // agree from here on: vignetting, then sky, then the transfer curve, in that order and by
        // these same lines. That shared tail is the whole point — it is what makes a document
        // reopened from disk render the picture the live view was showing when it was saved.
        vec3 radiance_linear;
        if (u_tex_mode == kTexModeXyz) {
          // The projection's own natural vignetting, applied to the energy BEFORE the exposure
          // scale multiplies it: it states how much of this direction's radiance the target lens's
          // pixel collects, so it belongs to the quantity being exposed rather than to the
          // exposure. See the relIllum* block above and src/gui/preview_jacobian.hpp.
          // Two spellings of one sum: the no-far-side arm is the pre-fade expression verbatim, so
          // that the variant without LUMICE_GLOBE_BACK_FADE folds to exactly it. Same for the two
          // arms below.
          if (has_back && pixel_visible) {
            radiance_linear = xyzToLinearRgb(tex_color * rel_illum + back_tex * back_gain);
          } else if (has_back) {  // far side only
            radiance_linear = xyzToLinearRgb(back_tex * back_gain);
          } else {
            radiance_linear = xyzToLinearRgb(tex_color * rel_illum);
          }
        } else {
          // kTexModeSrgbRadiance: the exposure was already applied when these bytes were baked, so
          // there is no u_intensity_scale here and no gamut clip to redo — only the decode. The
          // vignetting is still a display-time property of the TARGET lens, not of the stored
          // picture, so it is applied here rather than baked, exactly as in the branch above. What
          // makes that possible is that the bake no longer sums the sky into the texel: scaling a
          // composited texel would dim the sky too, which neither the CLI nor this shader does.
          if (has_back && pixel_visible) {
            radiance_linear = srgbToLinear(tex_color) * rel_illum + srgbToLinear(back_tex) * back_gain;
          } else if (has_back) {  // far side only
            radiance_linear = srgbToLinear(back_tex) * back_gain;
          } else {
            radiance_linear = srgbToLinear(tex_color) * rel_illum;
          }
        }

        // The sky colour joins the halo here: inside this gate, and in linear RGB between the two
        // halves of the colour chain. Both placements are load-bearing.
        //
        // Inside the gate, because outside it nothing was ever projected — beyond the lens's image
        // circle, in the half-sky `visible` discards, behind the camera under `front`. Painting
        // those would turn a 180 deg fisheye into a solid rectangle of sky with an invisible circle
        // inside it. Between the halves, because the addition is a radiance sum: after the transfer
        // curve instead, a pixel with no halo energy comes back gamma-encoded twice and stops being
        // the colour the user picked.
        //
        // KNOWN, DELIBERATE DIVERGENCE FROM THE CLI, on single-lens fisheyes at wide FOV. The CLI's
        // projection is a forward one and a single lens images one hemisphere, so its background
        // stops at the equator (theta = 90 deg) whatever the FOV. This shader runs the same law
        // backwards from a pixel and stops only at that law's own domain, which for equal area at
        // fov=180 reaches theta = 180 deg — a radius of sqrt(2) image radii. It can, because it is
        // re-projecting an all-sky source texture rather than imaging a scene through one lens. So
        // between those two radii is an annulus this renderer paints and the CLI leaves black.
        // NEITHER SIDE IS A BUG. Do NOT narrow this gate to match the CLI: what a wide-FOV
        // single-lens preview should show is an open product question, not a defect, and answering
        // it here would answer it silently. test/gui/functional/test_preview_background.cpp pins
        // the annulus so that whenever it IS answered, it lands as a red.
        //
        // Not in kTexModeSrgbComposited. Those texels are a 1x1 black clear, or a .lmc written
        // before the format carried radiance-only textures — whose pixels were baked with whatever
        // background was in effect when they were saved. Adding here too would apply it twice, and
        // subtracting the old one back out is not invertible where the bake clipped.
#ifdef LUMICE_GLOBE_BACK_FADE
        if (!pixel_visible) {
          // The far side only (the near direction is clipped): its light over the zero-energy
          // colour with no sky — the sky is the near hemisphere's ground — which is the CLI's
          // `!paint_bg` arm with light present. Print inks the far side's exposed Y onto bare
          // paper; the kTexModeSrgbRadiance print gap below takes the screen result, as it does
          // everywhere else.
          if (u_tone == 1 && u_tex_mode == kTexModeXyz) {
            tex_color = clampAndGamma(subtractiveInk(back_tex.y * back_gain * u_intensity_scale, u_paper));
          } else {
            tex_color = clampAndGamma(radiance_linear);
          }
        } else
#endif
        if (u_tone == 1 && u_tex_mode == kTexModeXyz) {
          // print: ink on paper replaces the whole additive statement above. The gamut clip and the
          // matrix are skipped along with the sky — print is greyscale by construction, so what is
          // taken is the exposed scalar alone. tex_color.y is CIE Y and the two multipliers are the
          // ones xyzToLinearRgb would have applied to it, in the same order, so this is the same
          // number the CLI calls xyz[1] (src/server/render.cpp).
          float e = has_back ? (tex_color.y * rel_illum + back_tex.y * back_gain) * u_intensity_scale
                             : tex_color.y * rel_illum * u_intensity_scale;
          tex_color = clampAndGamma(subtractiveInk(e, u_paper));
        } else if (u_tone == 1) {
          // kTexModeSrgbRadiance under print: a KNOWN GAP, confined to the v<=4 .lmc format (and
          // the composite preview) and held here on purpose rather than left to look supported.
          // These texels had their exposure baked in when they were written, so the exposed scalar
          // `e` the operator needs is not directly available — recovering it would mean inverting
          // the bake through the standard luminance coefficients, which is a relationship nothing
          // in this repo has verified. A document saved from format v5 on never reaches this
          // branch: its texture is the live frame's linear XYZ and takes the kTexModeXyz arm above,
          // print included. An older document keeps the screen result until the next simulation
          // run (or a re-save after one) replaces the texture with a v5 one.
          // See doc/rendering/print-mode-subtractive-ink.md.
          tex_color = clampAndGamma(radiance_linear + u_background);
        } else {
          // Both texture modes have halo-only linear light here, after visibility and vignetting.
          // B-R must read it before sky is added, not try to subtract sky after gamma/clipping.
          tex_color = u_display_mode == 1 ? clampAndGamma(radiance_linear)
                                        : clampAndGamma(radiance_linear + u_background);
        }
      }
      final_color = tex_color;
    }
  }

  // The channel-B-R display mode, on halo-only post-gamma channels (sky excluded), and
  // on EVERY pixel, the unimaged ones too: their zero-energy black is R == B and reads mid grey, the
  // same rule RenderConsumer::PostSnapshot applies on all of its frame paths (ApplyDisplayMode in
  // src/server/render.cpp). final_color is already post-gamma here, so no round trip is needed; the
  // CLI has one only because its annotation blend runs in linear. Before the overlays, so the grid,
  // the markers and the lens border are drawn ON the diagnostic image in their own colours. Inert
  // under print, which never computes R and B separately. The background PHOTO is excluded one
  // level up (BgPhotoOnScreen, app.cpp), so u_bg_enabled is never set together with this.
  // Legacy composited texels have no recoverable halo-only signal: diagnose the baked pixel as
  // before. The load notice reports this limitation; no inverse sky subtraction is attempted.
  if (u_display_mode == 1 && u_tone != 1) {
    final_color = vec3(channelMathBrGray(final_color.r, final_color.b));
  }

  // Background image overlay (contain mode with letterbox)
  if (u_bg_enabled != 0) {
    vec2 bg_uv = v_ndc * u_bg_uv_scale + u_bg_uv_offset;
    vec3 bg_color = vec3(0.0);  // Black letterbox for out-of-bounds UV
    if (bg_uv.x >= 0.0 && bg_uv.x <= 1.0 && bg_uv.y >= 0.0 && bg_uv.y <= 1.0) {
      bg_color = texture(u_bg_texture, bg_uv).rgb;
    }
    final_color = bg_color * (1.0 - u_overlay_alpha) + final_color * u_overlay_alpha;
  }

  // Auxiliary line overlay (on top of everything, only where some direction of this pixel passes
  // the clips — see overlayAuxLines for which side's curves each flag admits)
  if (result.w >= 0.5 && (pixel_visible || far_pixel_visible)) {
    final_color = overlayAuxLines(world_dir, final_color, pos_ovl, pos, half_fov, pixel_visible, far_pixel_visible);
  }

  // Lens border — deliberately OUTSIDE the `result.w >= 0.5 && pixel_visible` gate.
  // It draws the lens image circle itself, an optical property of the projection,
  // so it must stay whole regardless of which half-sky the user chose to display
  // (u_visible / u_front) and regardless of whether the pixel maps to a direction
  // at all. That is exactly the black region users cannot tell from the background.
  final_color = overlayLensBorder(final_color, pos, half_fov);

  frag_color = vec4(final_color, 1.0);
}
)glsl";

// The `6` written into u_marker_screen_pos[6] / u_marker_color[6] and into the loop bound of
// overlayAuxLines above. GLSL cannot include lumice_render.h, so the two spellings of the id-space size
// are pinned here instead of merely being expected to match: growing the marker family without
// widening the arrays would upload six of N points and silently drop the rest.
static_assert(LUMICE_ANNOTATION_MARKER_COUNT == 6,
              "the fragment shader hard-codes 6 marker uniform slots; update kFragmentShader's "
              "u_marker_screen_pos[6] / u_marker_color[6] and its loop bound together with this");

// The `512` and `4` written into u_grid_levels_deg[512] / u_angular_dist_deg[4] / u_view_dist_deg[4]
// above, pinned the same way: four levels per vec4, two grid families in one array (so that length
// is 2 * capacity / 4), and each ring family in an array of its own (capacity / 4).
static_assert(kMaxOverlayLevels % 4 == 0 && 2 * kMaxOverlayLevels / 4 == 512,
              "the fragment shader hard-codes vec4 u_grid_levels_deg[512]; update kFragmentShader together "
              "with kMaxOverlayLevels");
static_assert(kMaxAnnotationCircles % 4 == 0 && kMaxAnnotationCircles / 4 == 4,
              "the fragment shader hard-codes vec4 u_angular_dist_deg[4] and vec4 u_view_dist_deg[4]; update "
              "kFragmentShader together with kMaxAnnotationCircles");

// clang-format on

// The kTexMode* block inside the fragment shader above and PreviewRenderer::TextureMode are one
// contract across a language boundary GL gives us no way to check: Render() sends the C++ value
// and the shader compares it against its own literals. These assertions pin the C++ half to the
// numbers written up there, so a change made only here is a build error. The other direction —
// editing the GLSL literals alone — the compiler cannot see, so ADDING A FOURTH MODE means
// touching all four places: the enum (preview_renderer.hpp), the const block in the shader
// string, the branch in the shader body that reads u_tex_mode, and this assertion list.
static_assert(static_cast<int>(PreviewRenderer::TextureMode::kSrgbComposited) == 0,
              "kTexModeSrgbComposited in the fragment shader is 0");
static_assert(static_cast<int>(PreviewRenderer::TextureMode::kXyz) == 1, "kTexModeXyz in the fragment shader is 1");
static_assert(static_cast<int>(PreviewRenderer::TextureMode::kSrgbRadiance) == 2,
              "kTexModeSrgbRadiance in the fragment shader is 2");

static unsigned int CheckCompiledShader(unsigned int shader) {
  int success;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char log[512];
    glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
    GUI_LOG_ERROR("Shader compile error: {}", log);
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

static unsigned int CompileShader(unsigned int type, const char* source) {
  if (PreviewRenderer::ShaderCompileFaultArmedForTest()) {
    return 0;  // test seam: a compile failure with nothing created
  }
  unsigned int shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);
  return CheckCompiledShader(shader);
}

// The fragment shader from its parts — version, optional define, body; see kFragmentShader.
static unsigned int CompileFragmentShader(bool globe_back_fade) {
  if (PreviewRenderer::ShaderCompileFaultArmedForTest()) {
    return 0;  // test seam: a compile failure with nothing created
  }
  unsigned int shader = glCreateShader(GL_FRAGMENT_SHADER);
  const char* parts[5] = { kFragmentShaderVersion, globe_back_fade ? kGlobeBackFadeDefine : "", kFragmentShader,
                           kProjectionShader, kFragmentAfterProjection };
  glShaderSource(shader, 5, parts, nullptr);
  glCompileShader(shader);
  return CheckCompiledShader(shader);
}

// 0 on a link failure (logged).
static unsigned int LinkProgram(unsigned int vs, unsigned int fs) {
  unsigned int program = glCreateProgram();
  glAttachShader(program, vs);
  glAttachShader(program, fs);
  glLinkProgram(program);
  int success;
  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if (!success) {
    char log[512];
    glGetProgramInfoLog(program, sizeof(log), nullptr, log);
    GUI_LOG_ERROR("Shader link error: {}", log);
    glDeleteProgram(program);
    return 0;
  }
  return program;
}

bool PreviewRenderer::Init() {
  // Compile shaders
  unsigned int vs = CompileShader(GL_VERTEX_SHADER, kVertexShader);
  unsigned int fs = CompileFragmentShader(false);
  unsigned int fs_back_fade = CompileFragmentShader(true);
  if (!vs || !fs || !fs_back_fade) {
    // Partial-init cleanup: this renderer used to only exist at process scope, where a failed
    // init preceded exit and the leak was invisible; the transient full-source export renderer
    // can now fail init repeatedly inside a live application, and every shader left here would
    // stay leaked in a context that keeps running.
    if (vs != 0) {
      glDeleteShader(vs);
    }
    if (fs != 0) {
      glDeleteShader(fs);
    }
    if (fs_back_fade != 0) {
      glDeleteShader(fs_back_fade);
    }
    return false;
  }

  shader_program_ = LinkProgram(vs, fs);
  globe_back_fade_program_ = LinkProgram(vs, fs_back_fade);
  if (!shader_program_ || !globe_back_fade_program_) {
    if (shader_program_ != 0) {
      glDeleteProgram(shader_program_);
      shader_program_ = 0;
    }
    if (globe_back_fade_program_ != 0) {
      glDeleteProgram(globe_back_fade_program_);
      globe_back_fade_program_ = 0;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    glDeleteShader(fs_back_fade);
    return false;
  }

  glDeleteShader(vs);
  glDeleteShader(fs);
  glDeleteShader(fs_back_fade);

  // Fullscreen quad (two triangles, NDC coordinates)
  // clang-format off
  float quad_vertices[] = {
    -1.0f, -1.0f,
     1.0f, -1.0f,
    -1.0f,  1.0f,
     1.0f, -1.0f,
     1.0f,  1.0f,
    -1.0f,  1.0f,
  };
  // clang-format on

  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &vbo_);

  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
  glBindVertexArray(0);

  // Create equirect texture
  glGenTextures(1, &texture_);
  glBindTexture(GL_TEXTURE_2D, texture_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);  // dual fisheye: no horizontal wrap
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);
  // Initialize with 1x1 black pixel so the texture has valid GL storage.
  // This allows the shader to sample black (transparent) when no simulation data exists,
  // enabling background-only rendering before the simulation is started.
  // Shared with Render()'s needs_gl_blank_ consumer so a single code path owns
  // "sim layer reset to black" (both init and post-ClearTexture reuse the same
  // GL sequence + tex_mode_ reset side effect).
  UploadBlankSimTexture();

  // Create background texture.
  //
  // The background is a photograph, and it is nearly always larger than the viewport it is fitted
  // into: a 6000x4000 frame in a ~900px preview panel is a 4x reduction, and the loader only halves
  // images past 4096 on a side, so the texture reaching this point is still several times the
  // viewport. GL_LINEAR averages 2x2 texels however large the footprint under a fragment is, so at
  // that reduction it reads 4 texels of a 16-texel block and calls the result the block's colour —
  // undersampling, which turns photographic grain into speckle that reshuffles itself whenever the
  // view moves. GL_LINEAR_MIPMAP_LINEAR reads a mip level chosen for the actual footprint instead,
  // where the averaging has already been done. MAG stays GL_LINEAR: magnification has no footprint
  // to cover and no mip level above 0 to reach for.
  glGenTextures(1, &bg_texture_);
  glBindTexture(GL_TEXTURE_2D, bg_texture_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);

  return true;
}

void PreviewRenderer::Destroy() {
  for (int i = 0; i < 2; ++i) {
    if (pbo_fence_[i] != nullptr) {
      glDeleteSync(static_cast<GLsync>(pbo_fence_[i]));
      pbo_fence_[i] = nullptr;
    }
  }
  for (int i = 0; i < 2; ++i) {
    if (pbo_[i] != 0) {
      glDeleteBuffers(1, &pbo_[i]);
      pbo_[i] = 0;
    }
  }
  pbo_byte_count_ = { 0, 0 };
  pbo_index_ = 0;

  if (texture_) {
    glDeleteTextures(1, &texture_);
    texture_ = 0;
  }
  if (bg_texture_) {
    glDeleteTextures(1, &bg_texture_);
    bg_texture_ = 0;
  }
  if (vbo_) {
    glDeleteBuffers(1, &vbo_);
    vbo_ = 0;
  }
  if (vao_) {
    glDeleteVertexArrays(1, &vao_);
    vao_ = 0;
  }
  if (shader_program_) {
    glDeleteProgram(shader_program_);
    shader_program_ = 0;
  }
  if (globe_back_fade_program_) {
    glDeleteProgram(globe_back_fade_program_);
    globe_back_fade_program_ = 0;
  }
  tex_width_ = 0;
  tex_height_ = 0;
  tex_data_.clear();
  tex_xyz_data_.clear();
  tex_xyz_meta_ = XyzTextureMeta{};
  cpu_tex_mode_ = TextureMode::kSrgbComposited;
  cpu_tex_width_ = 0;
  cpu_tex_height_ = 0;
  bg_width_ = 0;
  bg_height_ = 0;
  bg_aspect_ = 1.0f;
}

void PreviewRenderer::ClearTexture() {
  tex_width_ = 0;
  tex_height_ = 0;
  tex_data_.clear();
  tex_xyz_data_.clear();
  tex_xyz_meta_ = XyzTextureMeta{};
  cpu_tex_mode_ = TextureMode::kSrgbComposited;
  cpu_tex_width_ = 0;
  cpu_tex_height_ = 0;
  // CPU-side (no GL): the display stops being sampleable the moment the document is gone, not
  // when the deferred blank finally lands — between here and that blank, whatever the storage
  // still holds belongs to no document.
  sim_layer_valid_ = false;
  // A fresh document is a fresh allocation climate: the blank retry budget resets with it.
  blank_attempts_ = 0;
  // GL reset deferred to next Render() (main thread) — this method is called
  // from gui_test coroutine workers with no GL context; a direct gl* call
  // would SIGILL. See preview_renderer.hpp needs_gl_blank_ contract.
  needs_gl_blank_ = true;
}

// Upload a 1x1 black pixel into the GL texture_ storage and reset tex_mode_.
// Reused by Init() (initial GL storage) and Render() (post-ClearTexture
// deferred reset). Must be called on the main thread (owns GL context).
// Does NOT touch tex_width_/tex_height_ — those track "does the app have
// real sim/loaded pixel data" (drives HasTexture()) and must stay 0 across
// this call so ClearTexture()'s CPU-side "cleared" state remains visible to
// callers like Save. Side effect: tex_mode_ = kSrgbComposited, because a 1x1 RGB
// uint8 pixel is not XYZ float data — leaving a stale mode would mislead the
// shader on the next real upload path. Composited rather than radiance-only on
// purpose: this pixel means "there is nothing to show", and the radiance mode
// would paint the sky colour over the whole frame instead of leaving it black.
void PreviewRenderer::UploadBlankSimTexture() {
  static const unsigned char kBlack[3] = { 0, 0, 0 };
  glBindTexture(GL_TEXTURE_2D, texture_);
  // The hook consults here too (before the real call): the blank redefinition is a genuine
  // allocation that can fail under the same pressure, and a test must be able to witness the
  // no-sample gate holding while the blank cannot land.
  int forced = HookFaultFor(UploadResult::Stage::kTexImage);
  GLenum err = GL_NO_ERROR;
  if (forced == 0) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, kBlack);
    err = glGetError();
  } else {
    err = static_cast<GLenum>(forced);
  }
  glBindTexture(GL_TEXTURE_2D, 0);
  if (err != GL_NO_ERROR) {
    // The blank itself failed to allocate (a real OOM leaves the GL state undefined — spec §2.5,
    // and the reviewer's point stands even for a 1x1): the storage behind texture_ is not
    // trustworthy, so the no-sample gate stays CLOSED. The retry is BOUNDED: each frame's
    // re-attempt is an allocation the machine just refused, so after kMaxBlankAttempts the
    // attempt stops until a successful upload or a document switch re-arms it — the gate stays
    // closed the whole time, so stopping costs nothing visually and the log stops growing.
    ++blank_attempts_;
    if (blank_attempts_ >= kMaxBlankAttempts) {
      needs_gl_blank_ = false;
      GUI_LOG_ERROR(
          "[GL] UploadBlankSimTexture: glTexImage2D failed (gl_error=0x{:04X}) — retry budget spent; the "
          "display stays un-sampled until the next verified upload",
          err);
    } else {
      GUI_LOG_ERROR(
          "[GL] UploadBlankSimTexture: glTexImage2D failed (gl_error=0x{:04X}) — sim layer stays "
          "un-sampled, blank re-armed",
          err);
    }
    tex_mode_ = TextureMode::kSrgbComposited;
    return;
  }
  tex_mode_ = TextureMode::kSrgbComposited;
  sim_layer_valid_ = true;
  needs_gl_blank_ = false;
  blank_attempts_ = 0;
}

void PreviewRenderer::UpdateCpuTextureData(const unsigned char* data, int width, int height, TextureMode mode) {
  if (!data || width <= 0 || height <= 0) {
    return;
  }
  size_t byte_count = static_cast<size_t>(width) * height * 3;
  tex_data_.assign(data, data + byte_count);
  tex_xyz_data_.clear();
  tex_xyz_meta_ = XyzTextureMeta{};
  cpu_tex_mode_ = mode;
  cpu_tex_width_ = width;
  cpu_tex_height_ = height;
}

void PreviewRenderer::UpdateCpuXyzTextureData(const float* data, int width, int height, const XyzTextureMeta& meta) {
  if (!data || width <= 0 || height <= 0) {
    return;
  }
  size_t float_count = static_cast<size_t>(width) * height * 3;
  tex_xyz_data_.assign(data, data + float_count);
  tex_xyz_meta_ = meta;
  tex_data_.clear();
  cpu_tex_mode_ = TextureMode::kXyz;
  cpu_tex_width_ = width;
  cpu_tex_height_ = height;
}

UploadResult PreviewRenderer::UploadTexture(const unsigned char* data, int width, int height) {
  return UploadUint8Texture(data, width, height, TextureMode::kSrgbComposited);
}

UploadResult PreviewRenderer::UploadRadianceTexture(const unsigned char* data, int width, int height) {
  return UploadUint8Texture(data, width, height, TextureMode::kSrgbRadiance);
}

// Human-readable stage names for the upload logs: the log is what an owner without a debugger
// reads first, and a bare integer stage code would have to be decoded against the header.
PreviewRenderer::UploadFaultHook PreviewRenderer::s_upload_fault_hook_all_ = nullptr;
int PreviewRenderer::s_shader_compile_fault_for_test_ = 0;

void PreviewRenderer::SetUploadFaultHookForAllForTest(UploadFaultHook hook) {
  s_upload_fault_hook_all_ = std::move(hook);
}

void PreviewRenderer::SetShaderCompileFaultForTest(int remaining_failures) {
  s_shader_compile_fault_for_test_ = remaining_failures;
}

void PreviewRenderer::RevokeDisplayForContextOom() {
  // The conservative revoke itself logs the ruling; the caller supplies the context by being the
  // one that observed the OOM on a renderer sharing this context.
  ConservativeRevoke(UploadResult::Stage::kTexImage, GL_OUT_OF_MEMORY, tex_width_, tex_height_);
}

const char* UploadStageName(UploadResult::Stage stage) {
  switch (stage) {
    case UploadResult::Stage::kNone:
      return "none";
    case UploadResult::Stage::kInvalidInput:
      return "invalid-input";
    case UploadResult::Stage::kFenceWait:
      return "fence-wait";
    case UploadResult::Stage::kPboAllocate:
      return "pbo-allocate";
    case UploadResult::Stage::kPboMap:
      return "pbo-map";
    case UploadResult::Stage::kPboUnmap:
      return "pbo-unmap";
    case UploadResult::Stage::kTexImage:
      return "texture-define";
    case UploadResult::Stage::kTexUpdate:
      return "texture-update";
    case UploadResult::Stage::kFenceCreate:
      return "fence-create";
    case UploadResult::Stage::kStorageMismatch:
      return "storage-mismatch";
    case UploadResult::Stage::kCpuAllocation:
      return "cpu-allocation";
  }
  return "unknown";
}

// The GL store backing one staging slot is gone: drop the buffer object and forget its size so
// the slot's next cycle allocates fresh. See TearDownPboSlot in the header for why deleting a
// buffer that an in-flight DMA may still be reading is safe.
void PreviewRenderer::ReleaseStagingSlots() {
  for (int slot = 0; slot < 2; ++slot) {
    if (pbo_fence_[slot] != nullptr) {
      glDeleteSync(static_cast<GLsync>(pbo_fence_[slot]));
      pbo_fence_[slot] = nullptr;
    }
    TearDownPboSlot(slot);
  }
  pbo_index_ = 0;
}

void PreviewRenderer::TearDownPboSlot(int slot) {
  if (pbo_[slot] != 0) {
    glDeleteBuffers(1, &pbo_[slot]);
    pbo_[slot] = 0;
  }
  pbo_byte_count_[slot] = 0;
}

UploadResult PreviewRenderer::ConservativeRevoke(UploadResult::Stage stage, int gl_error, int req_w, int req_h) {
  UploadResult r;
  r.ok = false;
  r.stage = stage;
  r.gl_error = gl_error;
  r.request_width = req_w;
  r.request_height = req_h;
  r.old_frame_retained = false;
  // The display stops describing any frame AND stops being sampleable: the storage behind it may
  // be corrupt, and querying its dimensions back cannot prove otherwise — an OOM leaves the
  // results of the GL undefined (OpenGL 3.3 core §2.5), so "the size query still agrees" is not
  // evidence of health, and the deferred blank (even a 1x1) can fail under the same pressure.
  // u_sim_valid=0 keeps whatever the storage holds off the screen until a verified blank or a
  // verified upload re-arms it. The mirror is deliberately untouched: the save source outlives
  // the display failure.
  tex_width_ = 0;
  tex_height_ = 0;
  sim_layer_valid_ = false;
  needs_gl_blank_ = true;
  GUI_LOG_ERROR(
      "[GL] texture upload failed: stage={} gl_error=0x{:04X} request={}x{} — display revoked, blank "
      "re-armed; save source retained",
      UploadStageName(stage), gl_error, req_w, req_h);
  return r;
}

// The GL half both uint8 entry points share. `mode` is the only thing that differs, and it is a
// property of what the CALLER baked, which is why it is a parameter here rather than a guess.
UploadResult PreviewRenderer::UploadUint8Texture(const unsigned char* data, int width, int height, TextureMode mode) {
  UploadResult r;
  r.request_width = width;
  r.request_height = height;
  if (!texture_ || !data || width <= 0 || height <= 0) {
    r.stage = UploadResult::Stage::kInvalidInput;
    return r;
  }

  glBindTexture(GL_TEXTURE_2D, texture_);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);  // RGB data may not be 4-byte aligned

  // Re-allocate when switching from float to uint8 or size changed; update in place otherwise.
  // The hook is consulted BEFORE the real call and skips it when it forces a failure: an ordinary
  // GL error's contract is "the offending command was ignored with no side effect", and skipping
  // the call reproduces exactly that end state (the storage never changes) — which is what makes
  // the injected case faithful enough for a pixel-readback witness. Neither display metadata nor
  // the CPU mirror commits here: they commit together below, only after the call has been
  // error-checked and the level-0 storage has been witness-verified. An OOM means the storage is
  // undefined (spec §2.5) and the conservative revoke applies.
  const UploadResult::Stage tex_stage =
      (width != tex_width_ || height != tex_height_ || tex_mode_ == TextureMode::kXyz) ?
          UploadResult::Stage::kTexImage :
          UploadResult::Stage::kTexUpdate;
  int forced_tex = HookFaultFor(tex_stage);
  if (forced_tex == 0) {
    if (tex_stage == UploadResult::Stage::kTexImage) {
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    } else {
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, data);
    }
  }
  GLenum err = forced_tex ? static_cast<GLenum>(forced_tex) : glGetError();
  if (err == GL_OUT_OF_MEMORY) {
    glBindTexture(GL_TEXTURE_2D, 0);
    return ConservativeRevoke(tex_stage, static_cast<int>(err), width, height);
  }
  if (err != GL_NO_ERROR) {
    glBindTexture(GL_TEXTURE_2D, 0);
    // Ordinary error: the command was ignored — the previous frame and its metadata stand, and
    // the storage is untouched, so the no-sample gate stays open.
    GUI_LOG_ERROR("[GL] texture upload failed: stage={} gl_error=0x{:04X} request={}x{} — previous frame retained",
                  UploadStageName(tex_stage), err, width, height);
    r.stage = tex_stage;
    r.gl_error = static_cast<int>(err);
    r.old_frame_retained = true;
    return r;
  }

  // Storage witness: level-0 dimensions (and internal format) must agree with the request. The
  // query is necessary evidence that the define landed — it proves nothing about CONTENT, which
  // is what the injection cases below read back for. A disagreement means the frame cannot be
  // claimed even though the define reported success: revoke rather than display an unverified
  // frame.
  int forced_mismatch = HookFaultFor(UploadResult::Stage::kStorageMismatch);
  GLint stored_w = 0;
  GLint stored_h = 0;
  GLint stored_fmt = 0;
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &stored_w);
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &stored_h);
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &stored_fmt);
  if (forced_mismatch || stored_w != static_cast<GLint>(width) || stored_h != static_cast<GLint>(height) ||
      stored_fmt != GL_RGB8) {
    glBindTexture(GL_TEXTURE_2D, 0);
    return ConservativeRevoke(UploadResult::Stage::kStorageMismatch, forced_mismatch, width, height);
  }

  // Success — now and only now commit. Display first (the frame IS on the GPU and verified), then
  // the CPU mirror under the mode the caller declared: a reopened pre-v4 document re-saved
  // without a run must go back out flagged composited, not radiance-only. The newest real frame
  // cancels any pending deferred blank, the sim layer is sampleable again, and the blank retry
  // budget resets (the pressure that spent it did not stop this upload).
  needs_gl_blank_ = false;
  sim_layer_valid_ = true;
  blank_attempts_ = 0;
  if (tex_stage == UploadResult::Stage::kTexImage) {
    tex_width_ = width;
    tex_height_ = height;
  }
  tex_mode_ = mode;
  size_t byte_count = static_cast<size_t>(width) * height * 3;
  tex_data_.assign(data, data + byte_count);
  tex_xyz_data_.clear();
  tex_xyz_meta_ = XyzTextureMeta{};
  cpu_tex_mode_ = mode;
  cpu_tex_width_ = width;
  cpu_tex_height_ = height;
  r.ok = true;
  glBindTexture(GL_TEXTURE_2D, 0);
  return r;
}

UploadResult PreviewRenderer::UploadXyzTexture(const float* data, int width, int height) {
  UploadResult r;
  r.request_width = width;
  r.request_height = height;
  if (!texture_ || !data || width <= 0 || height <= 0) {
    r.stage = UploadResult::Stage::kInvalidInput;
    return r;
  }
  // Does NOT touch the CPU-side mirror (tex_data_ / tex_xyz_data_): this runs at the poll rate,
  // and Save refreshes the mirror from the retained poller payload instead — see
  // UpdateCpuXyzTextureData.
  //
  // The texture is GL_RGB16F, and the float16 bits it receives are made HERE, by the same codec
  // the .lmc writer runs on the same floats (src/gui/xyz_half_codec.hpp) — not by handing the
  // driver GL_FLOAT data and letting it convert. That is what makes a reopened document's texture
  // bit-identical to the live one's: quantize(dequantize(quantize(x))) == quantize(x), and no
  // second converter is in either path to disagree. The scale the halves were divided by rides
  // along in tex_xyz_scale_ and reaches the shader as u_xyz_scale from Render().
  //
  // Every stage below reports its failure through the returned UploadResult and leaves the
  // display metadata (tex_width_/tex_height_/tex_mode_/tex_xyz_scale_) describing whatever was
  // truthfully displayed before: an ordinary GL error leaves the offending command ignored with
  // no side effect, so the previous frame stands untouched; an OOM at a texture stage takes the
  // conservative revoke instead, because after it nothing about the texture's content is
  // provable. The per-stage staging-slot teardowns exist so a failure can never leave the NEXT
  // cycle writing into a buffer whose guard (fence) is gone.
  const size_t component_count = static_cast<size_t>(width) * height * 3;
  const size_t byte_count = component_count * sizeof(uint16_t);
  const float scale = ComputeXyzHalfScale(data, component_count);
  int w = pbo_index_;

  // Wait for prior fence (ensure GPU finished reading PBO[w]). Consulted BEFORE the real wait:
  // skipping it and taking the failure branch leaves the same end state as a real timeout (fence
  // deleted, DMA possibly still in flight — which the slot teardown below answers).
  if (pbo_fence_[w] != nullptr) {
    auto sync = static_cast<GLsync>(pbo_fence_[w]);
    int forced_wait = HookFaultFor(UploadResult::Stage::kFenceWait);
    GLenum wait_result = forced_wait ? static_cast<GLenum>(forced_wait) :
                                       glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, 2'000'000'000ULL);
    glDeleteSync(sync);
    pbo_fence_[w] = nullptr;
    if (forced_wait || wait_result == GL_TIMEOUT_EXPIRED || wait_result == GL_WAIT_FAILED) {
      // A sync STATUS is not a GL error code, but a failed wait is allowed to leave a driver
      // error flag set (an invalid sync object is the spec's own example). Reading the flag HERE
      // — before any teardown — does two things recording the raw status cannot: it attributes
      // the failure to the driver's actual error when there is one, and it DRAINS the flag so it
      // cannot leak past this candidate's boundary into the next candidate's error reads, where
      // it would label a successful texture write as failed with the new frame already on the
      // GPU — the exact pixel-vs-metadata split this file exists to prevent. When the driver set
      // no flag (a plain timeout), the status code itself is the evidence that remains.
      const GLenum wait_err = glGetError();
      GUI_LOG_WARNING(
          "[GL] UploadXyzTexture: fence wait failed (result=0x{:X} driver_error=0x{:04X}), tearing "
          "down staging slot {}; skipping frame",
          wait_result, wait_err, w);
      TearDownPboSlot(w);
      r.stage = UploadResult::Stage::kFenceWait;
      r.gl_error = wait_err != GL_NO_ERROR ? static_cast<int>(wait_err) : static_cast<int>(wait_result);
      r.old_frame_retained = true;
      return r;
    }
  }

  // Lazy-init + resize PBO. The size registration below is a SUCCESS record: writing it before
  // the allocation is known good would let a failed glBufferData leave a slot claiming a size it
  // does not hold, and the next cycle would skip the allocation it actually needs.
  if (pbo_[w] == 0) {
    glGenBuffers(1, &pbo_[w]);
  }
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_[w]);
  if (pbo_byte_count_[w] != byte_count) {
    // The hook is consulted BEFORE the real call and skips it when it forces a failure — a real
    // GL_OUT_OF_MEMORY leaves the old store undefined-but-alive, and skipping the call reproduces
    // exactly that state. Either way the slot must not claim the new size.
    int forced_alloc = HookFaultFor(UploadResult::Stage::kPboAllocate);
    GLenum alloc_err = GL_NO_ERROR;
    if (forced_alloc == 0) {
      glBufferData(GL_PIXEL_UNPACK_BUFFER, static_cast<GLsizeiptr>(byte_count), nullptr, GL_STREAM_DRAW);
      alloc_err = glGetError();
    } else {
      alloc_err = static_cast<GLenum>(forced_alloc);
    }
    if (alloc_err != GL_NO_ERROR) {
      // Unified OOM ruling (review round 1): the spec leaves the GL's WHOLE state undefined after
      // GL_OUT_OF_MEMORY (§2.5) — it is not scoped to the object the failing call was about to
      // fill, so "the texture was not touched yet" does not make the old frame trustworthy. A
      // real OOM here therefore takes the same conservative revoke as a texture-stage OOM; an
      // ordinary allocation error leaves the command ignored with no side effect and only skips
      // the frame. Both paths tear the slot down for a clean retry, and neither may register the
      // size.
      TearDownPboSlot(w);
      if (alloc_err == GL_OUT_OF_MEMORY) {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        return ConservativeRevoke(UploadResult::Stage::kPboAllocate, static_cast<int>(alloc_err), width, height);
      }
      GUI_LOG_ERROR("[GL] UploadXyzTexture: PBO allocation failed (gl_error=0x{:04X}, {} bytes), skipping frame",
                    alloc_err, byte_count);
      glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
      r.stage = UploadResult::Stage::kPboAllocate;
      r.gl_error = static_cast<int>(alloc_err);
      r.old_frame_retained = true;
      return r;
    }
    pbo_byte_count_[w] = byte_count;
  }

  // Map PBO and write data. The hook is consulted BEFORE the real map: a forced failure after a
  // successful map would leave the buffer mapped, a state the real null-map failure never
  // produces.
  int forced_map = HookFaultFor(UploadResult::Stage::kPboMap);
  void* ptr = forced_map ?
                  nullptr :
                  glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, static_cast<GLsizeiptr>(byte_count),
                                   GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
  if (!ptr) {
    // A null map may or may not have left an error flag (GL_OUT_OF_MEMORY is allowed here);
    // drain and attribute whatever the driver reported, then apply the same OOM ruling as above.
    GLenum map_err = forced_map ? static_cast<GLenum>(forced_map) : glGetError();
    TearDownPboSlot(w);
    if (map_err == GL_OUT_OF_MEMORY) {
      glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
      return ConservativeRevoke(UploadResult::Stage::kPboMap, static_cast<int>(map_err), width, height);
    }
    GUI_LOG_ERROR("[GL] UploadXyzTexture: glMapBufferRange failed (gl_error=0x{:04X}), skipping frame", map_err);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    r.stage = UploadResult::Stage::kPboMap;
    r.gl_error = static_cast<int>(map_err);
    r.old_frame_retained = true;
    return r;
  }
  // Quantize straight into the mapped PBO: one pass, no staging copy.
  QuantizeXyzToHalf(data, component_count, scale, static_cast<uint16_t*>(ptr));
  // The REAL unmap runs unconditionally — the injected failure is consulted after it (a
  // successful unmap followed by a forced failure has the same observable end state as a real
  // corrupt-data unmap failure: buffer unmapped, contents distrusted and never uploaded).
  // Consulting before the call instead would strand a permanently mapped buffer: the failure
  // branch assumes an unmapped buffer, and only an executed glUnmapBuffer produces one.
  bool unmapped = glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER) == GL_TRUE;
  int forced_unmap = HookFaultFor(UploadResult::Stage::kPboUnmap);
  if (forced_unmap || !unmapped) {
    GLenum unmap_err = forced_unmap ? static_cast<GLenum>(forced_unmap) : glGetError();
    GUI_LOG_WARNING(
        "[GL] UploadXyzTexture: glUnmapBuffer failed (gl_error=0x{:04X}), buffer data may be corrupt, "
        "skipping frame",
        unmap_err);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    r.stage = UploadResult::Stage::kPboUnmap;
    r.gl_error = static_cast<int>(unmap_err);
    r.old_frame_retained = true;
    return r;
  }

  // Upload from PBO to texture (async DMA): the PBO stays bound through the define — the null
  // pixel pointer is what makes it read from the buffer. The define/witness/commit tail is the
  // NoStaging path's too; only the fence differs (a slot DMA needs guarding, client memory does
  // not), and staging_slot >= 0 tells the tail to create it.
  glBindTexture(GL_TEXTURE_2D, texture_);
  // Rows are width*3*2 bytes: even, but a multiple of 4 only for even widths, so the unpack
  // alignment has to say 2 (the sRGB path says 1 for the same reason, and each path sets its own
  // value every upload, so a failure leaving it here strands nothing).
  glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
  UploadResult tail = UploadXyzBitsTail(nullptr, width, height, scale, /*staging_slot=*/w);
  return tail;
}

UploadResult PreviewRenderer::UploadXyzTextureNoStaging(const float* data, int width, int height) {
  UploadResult r;
  r.request_width = width;
  r.request_height = height;
  if (!texture_ || !data || width <= 0 || height <= 0) {
    r.stage = UploadResult::Stage::kInvalidInput;
    return r;
  }
  // Same codec, same floats, same scale rule as the PBO path — the only difference is WHERE the
  // half bits live while the driver copies them. The transient CPU buffer is the recovery's whole
  // staging cost: one plane of half bits, freed at return, no second full-size GL allocation.
  const size_t component_count = static_cast<size_t>(width) * height * 3;
  const float scale = ComputeXyzHalfScale(data, component_count);
  std::vector<uint16_t> half_bits;
  try {
    half_bits.resize(component_count);
    QuantizeXyzToHalf(data, component_count, scale, half_bits.data());
  } catch (const std::bad_alloc&) {
    // The host could not hold the client-memory staging copy: report it like any other failed
    // stage instead of letting std::bad_alloc escape the recovery state machine — an exhausted
    // machine must land in the latch-and-unavailable state, not terminate the process. No GL
    // call was made, so there is nothing to clean up or attribute.
    GUI_LOG_ERROR("[GL] UploadXyzTextureNoStaging: host allocation failed ({} components), skipping frame",
                  component_count);
    r.stage = UploadResult::Stage::kCpuAllocation;
    return r;
  }

  // No staging buffer may be bound through the define: with one bound, the "client pointer"
  // argument would be read as a PBO offset and the upload would take garbage.
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, texture_);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
  return UploadXyzBitsTail(half_bits.data(), width, height, scale, /*staging_slot=*/-1);
}

UploadResult PreviewRenderer::UploadXyzBitsTail(const void* define_data, int width, int height, float scale,
                                                int staging_slot) {
  UploadResult r;
  r.request_width = width;
  r.request_height = height;
  const UploadResult::Stage tex_stage =
      (width != tex_width_ || height != tex_height_ || tex_mode_ != TextureMode::kXyz) ?
          UploadResult::Stage::kTexImage :
          UploadResult::Stage::kTexUpdate;
  // The hook is consulted BEFORE the real call and skips it when it forces a failure: an ordinary
  // GL error's contract is "the offending command was ignored with no side effect", and skipping
  // the call reproduces exactly that end state (the texture never changes, and on the PBO path no
  // DMA is in flight either). That end-state equivalence is what makes the injected case faithful
  // enough for a pixel-readback witness — a forced failure AFTER a real define would leave the
  // new frame on the GPU and the witness would be asserting against a fiction.
  int forced_tex = HookFaultFor(tex_stage);
  if (forced_tex == 0) {
    if (tex_stage == UploadResult::Stage::kTexImage) {
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_HALF_FLOAT, define_data);
    } else {
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_HALF_FLOAT, define_data);
    }
  }
  GLenum err = forced_tex ? static_cast<GLenum>(forced_tex) : glGetError();
  if (err == GL_OUT_OF_MEMORY) {
    // The GL's state is undefined after this error (spec §2.5): the display must stop describing
    // a frame AND stop being sampleable, and on the PBO path the slot's DMA fate is unknowable —
    // tear the slot down so no later cycle writes into it.
    if (staging_slot >= 0) {
      glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
      TearDownPboSlot(staging_slot);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    return ConservativeRevoke(tex_stage, static_cast<int>(err), width, height);
  }
  if (err != GL_NO_ERROR) {
    // Ordinary error: the offending command was ignored with no side effect — the previous frame
    // stands, and on the PBO path the texture never read the PBO (no DMA in flight), so the slot
    // is safe to reuse without a teardown.
    if (staging_slot >= 0) {
      glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    GUI_LOG_ERROR("[GL] texture upload failed: stage={} gl_error=0x{:04X} request={}x{} — previous frame retained",
                  UploadStageName(tex_stage), err, width, height);
    r.stage = tex_stage;
    r.gl_error = static_cast<int>(err);
    r.old_frame_retained = true;
    return r;
  }

  // Storage witness: level-0 dimensions and internal format must agree with the request before
  // the frame can be called displayed. The query is necessary evidence that the define landed —
  // it is not a content proof (the injection cases read content back for that). A disagreement,
  // real or injected, means the frame cannot be claimed: revoke conservatively, and tear the
  // slot down since the define may have executed and its DMA fate is unguarded.
  int forced_mismatch = HookFaultFor(UploadResult::Stage::kStorageMismatch);
  GLint stored_w = 0;
  GLint stored_h = 0;
  GLint stored_fmt = 0;
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &stored_w);
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &stored_h);
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &stored_fmt);
  if (forced_mismatch || stored_w != static_cast<GLint>(width) || stored_h != static_cast<GLint>(height) ||
      stored_fmt != GL_RGB16F) {
    if (staging_slot >= 0) {
      glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
      TearDownPboSlot(staging_slot);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    return ConservativeRevoke(UploadResult::Stage::kStorageMismatch, forced_mismatch, width, height);
  }

  // Fence for the staging slot's DMA — PBO path only. A real failure here is not one thing:
  // glFenceSync can come back null under ordinary pressure (frame fine, guard for the NEXT write
  // gone — answer is teardown, commit stands) or under memory exhaustion, where the GL's state is
  // undefined (§2.5) and the frame cannot be trusted after all. Read the real error and let it
  // rule — and the injected failure takes the branch its synthesized error names, same as every
  // stage above. The client-memory path skips this stage entirely: nothing asynchronous happened.
  if (staging_slot >= 0) {
    pbo_fence_[staging_slot] = static_cast<void*>(glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0));
    int forced_fence = HookFaultFor(UploadResult::Stage::kFenceCreate);
    if (forced_fence || !pbo_fence_[staging_slot]) {
      GLenum fence_err = forced_fence ? static_cast<GLenum>(forced_fence) : glGetError();
      if (pbo_fence_[staging_slot]) {
        glDeleteSync(static_cast<GLsync>(pbo_fence_[staging_slot]));
        pbo_fence_[staging_slot] = nullptr;
      }
      TearDownPboSlot(staging_slot);
      if (fence_err == GL_OUT_OF_MEMORY) {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        return ConservativeRevoke(UploadResult::Stage::kFenceCreate, static_cast<int>(fence_err), width, height);
      }
      GUI_LOG_WARNING(
          "[GL] UploadXyzTexture: glFenceSync failed (gl_error=0x{:04X}), tearing down staging slot {}; "
          "frame committed",
          fence_err, staging_slot);
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  }

  // Success — now and only now commit the display facts, re-arm the no-sample gate (the frame on
  // the GPU is verified), and make the newest write win over any pending deferred blank (see
  // UploadUint8Texture for rationale).
  needs_gl_blank_ = false;
  sim_layer_valid_ = true;
  blank_attempts_ = 0;
  if (tex_stage == UploadResult::Stage::kTexImage) {
    tex_width_ = width;
    tex_height_ = height;
  }
  tex_mode_ = TextureMode::kXyz;
  tex_xyz_scale_ = scale;
  glBindTexture(GL_TEXTURE_2D, 0);
  if (staging_slot >= 0) {
    pbo_index_ = 1 - pbo_index_;
  }
  r.ok = true;
  return r;
}

void PreviewRenderer::RaiseRealGlInvalidOperationForTest() {
  // GL_UNPACK_ALIGNMENT accepts 1/2/4/8 only: 3 is rejected with GL_INVALID_VALUE and changes no
  // state (the old alignment stays). Deliberately NOT drained — the caller arranges the read;
  // this is what lets a witness place a driver-set error flag ahead of a branch whose contract
  // is to read and clear it.
  glPixelStorei(GL_UNPACK_ALIGNMENT, 3);
}

void PreviewRenderer::UploadBgTexture(const unsigned char* data, int width, int height) {
  if (!bg_texture_ || !data || width <= 0 || height <= 0) {
    return;
  }

  glBindTexture(GL_TEXTURE_2D, bg_texture_);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);  // RGB data may not be 4-byte aligned

  if (width != bg_width_ || height != bg_height_) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    bg_width_ = width;
    bg_height_ = height;
  } else {
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, data);
  }

  // After the branches meet, so that both are covered. The MIN filter set in Init() samples mip
  // levels, and only level 0 was written above; without this the smaller levels hold whatever the
  // previous image left there. That is a live hazard on the glTexSubImage2D branch in particular:
  // it overwrites the pixels in place and leaves the rest of the chain untouched, so a user
  // swapping one photograph for another of the same size would keep seeing the old one wherever the
  // fit minifies.
  glGenerateMipmap(GL_TEXTURE_2D);

  bg_aspect_ = static_cast<float>(width) / static_cast<float>(height);
  glBindTexture(GL_TEXTURE_2D, 0);
}

void PreviewRenderer::ClearBackground() {
  bg_width_ = 0;
  bg_height_ = 0;
  bg_aspect_ = 1.0f;
}

// Shared with the diagnostic scalar-map renderer. The source is inserted verbatim in both
// programs; the map does not consume illumination, exposure or the simulation texture.
const char* PreviewProjectionGlsl() {
  return kProjectionShader;
}

// Build view-to-world rotation matrix from elevation, azimuth, roll (degrees).
//
// Convention: at (az=0, el=0, roll=0), the camera looks at the equirect center,
// which is world direction (-1, 0, 0).  Increasing azimuth rotates the camera
// rightward in the equirect (toward increasing longitude / -Y direction).
// Increasing elevation tilts the camera upward (toward upper sky / -Z direction).
//
// Camera basis at (az=a, el=e, roll=0):
//   forward = (-cos(e)*cos(a), -cos(e)*sin(a), -sin(e))   [equirect az=a, el=e]
//   right   = (sin(a), -cos(a), 0)                         [horizontal, toward +lon]
//   up      = (sin(e)*cos(a), sin(e)*sin(a), -cos(e))      [completes RH basis]
//
// Roll rotates right and up around the forward axis:
//   col0' = cos(r)*right + sin(r)*up
//   col1' = -sin(r)*right + cos(r)*up
//   col2  = -forward  (unchanged by roll)
//
// OpenGL column-major: out[col*3 + row].
void BuildViewMatrix(float elevation_deg, float azimuth_deg, float roll_deg, float out[9]) {
  constexpr float kDeg2Rad = 3.14159265358979323846f / 180.0f;
  float a = azimuth_deg * kDeg2Rad;
  float e = elevation_deg * kDeg2Rad;
  float r = roll_deg * kDeg2Rad;

  float ca = std::cos(a), sa = std::sin(a);
  float ce = std::cos(e), se = std::sin(e);
  float cr = std::cos(r), sr = std::sin(r);

  // Column 0 = cos(r)*right + sin(r)*up
  out[0] = cr * sa + sr * se * ca;
  out[1] = -cr * ca + sr * se * sa;
  out[2] = -sr * ce;

  // Column 1 = -sin(r)*right + cos(r)*up
  out[3] = -sr * sa + cr * se * ca;
  out[4] = sr * ca + cr * se * sa;
  out[5] = -cr * ce;

  // Column 2 = -forward = (cos(e)*cos(a), cos(e)*sin(a), sin(e))
  out[6] = ce * ca;
  out[7] = ce * sa;
  out[8] = se;
}

// Sentinel value indicating the world direction does not project to a renderable
// screen position under the current lens / camera setup. Kept identical to the
// magic shader value so the GPU distance test trivially rejects it.
static constexpr std::array<float, 2> kProjectSentinel = { kOverlaySentinel, kOverlaySentinel };

static bool IsInViewport(float px, float py, float vp_w, float vp_h) {
  // Half-pixel margin keeps points landing exactly on the viewport edge
  // (e.g. fisheye r = img_radius for the equator) inside the visible region
  // — the shader naturally clips anything truly outside.
  constexpr float kEdgeMargin = 0.5f;
  return std::abs(px) <= vp_w * 0.5f + kEdgeMargin && std::abs(py) <= vp_h * 0.5f + kEdgeMargin;
}

// Apply transpose(view_matrix) * world_dir. Column-major view-to-world.
static void WorldToView(const ViewProjection& vp, const float world_dir[3], float out_view[3]) {
  float vm[9];
  BuildViewMatrix(vp.elevation, vp.azimuth, vp.roll, vm);
  for (int c = 0; c < 3; ++c) {
    out_view[c] = vm[c * 3 + 0] * world_dir[0] + vm[c * 3 + 1] * world_dir[1] + vm[c * 3 + 2] * world_dir[2];
  }
}

// The screen entry owns only the view transform and rectangular viewport policy. All per-lens
// equations/domain facts live in forward_projection.hpp, also used by the inverse-mirror entry.
std::array<float, 2> ProjectWorldDirToScreen(const ViewProjection& vp, const float world_dir[3], int vp_w, int vp_h,
                                             bool clip_viewport) {
  float local[3] = { world_dir[0], world_dir[1], world_dir[2] };
  if (!LensIsFullSky(vp.lens_type))
    WorldToView(vp, world_dir, local);
  const auto point = ProjectLocalDirection(local, static_cast<float>(vp_w), static_cast<float>(vp_h), vp.lens_type,
                                           vp.fov, ForwardPolicy::kScreen);
  if (!point.valid ||
      (clip_viewport && !IsInViewport(point.px, point.py, static_cast<float>(vp_w), static_cast<float>(vp_h))))
    return kProjectSentinel;
  return { point.px, point.py };
}

// See declaration in preview_renderer.hpp for contract.
//
// Each branch is the reciprocal of the corresponding forward projection's radial
// derivative dr/dtheta evaluated at theta = 0 (the frame center), i.e. it answers
// "one pixel of screen radius is how many radians of view angle?". img_radius is
// min(vp_w, vp_h)/2 — the same definition ProjectWorldDirToScreen uses, so the gain
// and the projection that consumes it share one pixel/DPI convention and window
// resize needs no extra handling. The table below is that 1:1 law; the returned
// value is it scaled by kDragSensitivity (see the constant for why).
//
//   lens                        forward r(theta)                       dtheta/dr at 0
//   --------------------------  -------------------------------------  ---------------------------
//   kLensTypeLinear             r = focal*tan(theta),                   tan(hf) / R
//                               focal = R/tan(hf)          (ProjectLinear)
//   kLensTypeFisheyeEqualArea   r/R = sin(theta/2)/sin(hf/2)   (ProjectFisheye, type 0)
//                                                                       2*sin(hf/2) / R
//   kLensTypeFisheyeEquidist    r/R = theta/hf                 (ProjectFisheye, type 1)
//                                                                       hf / R   (exact for all theta)
//   kLensTypeFisheyeStereogr.   r/R = tan(theta/2)/tan(hf/2)   (ProjectFisheye, type 2)
//                                                                       2*tan(hf/2) / R
//   kLensTypeFisheyeOrthogr.    r/R = sin(theta)/sin(hf)       (ProjectFisheye, type 3)
//                                                                       sin(hf) / R
//   kLensTypeGlobe              r = focal*x_eye/(D - z_eye)    (ProjectGlobe)
//                                                                       (D-1)*tan(hf) / R
//   (hf = half_fov in radians, R = img_radius, D = kGlobeCameraD)
//
// LUMICE_MaxFov() keeps every expression finite over each lens's legal FOV range
// (linear caps at 179°, short of tan's pole at 180°; globe at 90°), so no extra
// numerical guard is needed beyond the degenerate-viewport check.
// bg_uv = v_ndc * scale + offset maps NDC [-1,1] to texture UV [0,1] with a contain fit
// (letterbox on aspect mismatch), then the user's pan/zoom on top. See the header for the
// conventions; the identity case is asserted bit-for-bit in test_preview_renderer.cpp.
BgUvTransform ComputeBgUvTransform(int vp_w, int vp_h, float bg_aspect, float pan_x, float pan_y, float zoom) {
  float vp_aspect = static_cast<float>(vp_w) / static_cast<float>(vp_h);
  float sx = 1.0f;
  float sy = 1.0f;
  if (vp_aspect > bg_aspect) {
    sx = bg_aspect / vp_aspect;
  } else {
    sy = vp_aspect / bg_aspect;
  }
  BgUvTransform t{};
  t.scale_x = 0.5f / (sx * zoom);
  // Y flip: stbi loads top-down, GL texture origin is bottom-left
  t.scale_y = -0.5f / (sy * zoom);
  t.offset_x = 0.5f + pan_x;
  t.offset_y = 0.5f + pan_y;
  return t;
}

NdcPoint ScreenDeltaToNdcDelta(float dx_pt, float dy_pt, float dpi_scale_x, float dpi_scale_y, int vp_w, int vp_h) {
  if (vp_w <= 0 || vp_h <= 0) {
    return NdcPoint{ 0.0f, 0.0f };
  }
  return NdcPoint{ dx_pt * dpi_scale_x * 2.0f / static_cast<float>(vp_w),
                   -dy_pt * dpi_scale_y * 2.0f / static_cast<float>(vp_h) };
}

NdcPoint ScreenPosToNdc(float x_pt, float y_pt, float dpi_scale_x, float dpi_scale_y, int vp_w, int vp_h) {
  const NdcPoint d = ScreenDeltaToNdcDelta(x_pt, y_pt, dpi_scale_x, dpi_scale_y, vp_w, vp_h);
  // The viewport's top-left corner is NDC (-1, +1); the displacement above is measured from it.
  return NdcPoint{ -1.0f + d.x, 1.0f + d.y };
}

std::optional<BgPixelIndex> BgUvToPixelIndex(float u, float v, int img_w, int img_h) {
  if (img_w <= 0 || img_h <= 0) {
    return std::nullopt;
  }
  // Same square the shader tests before it samples u_bg_texture (preview_renderer's fragment
  // source, background overlay stage) — outside it the shader paints the black letterbox, so
  // there is no photo pixel to report.
  if (!(u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f)) {
    return std::nullopt;
  }
  // floor, then clamp: u == 1.0f is inside the square by the test above but floors to img_w,
  // one past the last column.
  int col = static_cast<int>(std::floor(u * static_cast<float>(img_w)));
  int row = static_cast<int>(std::floor(v * static_cast<float>(img_h)));
  col = std::max(0, std::min(img_w - 1, col));
  row = std::max(0, std::min(img_h - 1, row));
  return BgPixelIndex{ col, row };
}

std::optional<std::array<float, 3>> SampleBgColorAtScreenPos(float x_pt, float y_pt, const BgSampleGeometry& geom,
                                                             const std::vector<unsigned char>& pixels) {
  const NdcPoint ndc = ScreenPosToNdc(x_pt, y_pt, geom.dpi_scale_x, geom.dpi_scale_y, geom.vp_w, geom.vp_h);
  const BgUvTransform t = ComputeBgUvTransform(geom.vp_w, geom.vp_h, geom.bg_aspect, geom.pan_x, geom.pan_y, geom.zoom);
  // The shader's own line, on the CPU: bg_uv = v_ndc * u_bg_uv_scale + u_bg_uv_offset.
  const float u = ndc.x * t.scale_x + t.offset_x;
  const float v = ndc.y * t.scale_y + t.offset_y;
  const std::optional<BgPixelIndex> idx = BgUvToPixelIndex(u, v, geom.img_w, geom.img_h);
  if (!idx) {
    return std::nullopt;
  }
  const size_t offset =
      (static_cast<size_t>(idx->row) * static_cast<size_t>(geom.img_w) + static_cast<size_t>(idx->col)) * 3u;
  if (offset + 2u >= pixels.size()) {
    // geom's dimensions and the buffer disagree — no copy loaded, or a stale one. Report "nothing
    // here" rather than reading past the end.
    return std::nullopt;
  }
  return std::array<float, 3>{ static_cast<float>(pixels[offset]) / 255.0f,
                               static_cast<float>(pixels[offset + 1]) / 255.0f,
                               static_cast<float>(pixels[offset + 2]) / 255.0f };
}

float ComputeDragGainDegPerPixel(int lens_type, float fov_deg, int vp_w, int vp_h) {
  constexpr float kPi = 3.14159265358979323846f;
  // Pre-fov-aware sensitivity. Only reachable via the default branch below, which
  // no caller should hit — app_panels.cpp only drags when !LensIsFullSky. It is the
  // historical feel value itself, so kDragSensitivity is deliberately NOT applied to it.
  constexpr float kLegacyGainDegPerPixel = 0.3f;

  // How many screen pixels the content moves per dragged pixel. The closed forms below
  // are the 1:1 law (one pixel of mouse motion, one pixel of content motion at the frame
  // center); this multiplier re-anchors that law without touching its shape.
  //
  // 1:1 is a hard constraint on a touchscreen — the finger is physically on the content —
  // but on a mouse it is only one candidate anchor, and it lost the owner's drag test:
  // measured against the historical 0.3 deg/px on a 900 px short side, the 1:1 law is 2.4x
  // slower at fov=90°, 4.1x at 60°, 8.8x at 30°, and only overtakes it above fov=134° —
  // i.e. slower everywhere in daily use. 2.5 puts linear at fov=90°/900 px at 0.318 deg/px,
  // within 6% of the constant people are already used to.
  //
  // What this does NOT change is the property the fov-aware law exists for: the screen
  // displacement per dragged pixel stays constant across each lens's whole FOV range. Only
  // the value of that constant moves, from 1 to k.
  constexpr float kDragSensitivity = 2.5f;

  if (vp_w <= 0 || vp_h <= 0) {
    return 0.0f;  // degenerate viewport: produce no rotation this frame
  }
  float img_radius = std::min(static_cast<float>(vp_w), static_cast<float>(vp_h)) * 0.5f;
  float half_fov = fov_deg * 0.5f * kPi / 180.0f;

  float rad_per_px = 0.0f;
  if (lens_type == kLensTypeLinear) {
    rad_per_px = std::tan(half_fov) / img_radius;
  } else if (lens_type == kLensTypeFisheyeEqualArea) {
    rad_per_px = 2.0f * std::sin(half_fov * 0.5f) / img_radius;
  } else if (lens_type == kLensTypeFisheyeEquidist) {
    rad_per_px = half_fov / img_radius;
  } else if (lens_type == kLensTypeFisheyeStereographic) {
    rad_per_px = 2.0f * std::tan(half_fov * 0.5f) / img_radius;
  } else if (lens_type == kLensTypeFisheyeOrthographic) {
    rad_per_px = std::sin(half_fov) / img_radius;
  } else if (lens_type == kLensTypeGlobe) {
    rad_per_px = (kGlobeCameraD - 1.0f) * std::tan(half_fov) / img_radius;
  } else {
    // Full-sky (or a newly added) lens type. A release-visible warning rather than a
    // bare assert: assert compiles out, and a new lens type that is draggable but
    // missing from this switch would otherwise silently fall back to the old,
    // fov-blind feel with nothing to notice it by.
    // Warn once per lens type, not once per frame: this is called from the drag handler, so an
    // unthrottled warning would emit tens of lines per second and bury the very signal it exists
    // to raise. The latch is per type rather than a single global flag so a second unknown type
    // still gets its own line instead of being swallowed by the first one.
    //
    // thread_local rather than a bare static, even though every caller today is the ImGui
    // main loop and a bare static would do: the promise this latch makes is "one line per
    // lens type per thread", which degrades gracefully (an extra line) if someone ever
    // calls this off the UI thread, whereas a bare static would be an unsynchronized
    // read-modify-write and thus a data race. The cost of the choice is that promise's
    // "per thread" qualifier, stated here rather than left for a reader to infer.
    static thread_local unsigned int warned_lens_types = 0u;
    const bool latchable = lens_type >= 0 && lens_type < static_cast<int>(sizeof(warned_lens_types) * 8);
    const unsigned int bit = latchable ? (1u << lens_type) : 0u;
    if (!latchable || (warned_lens_types & bit) == 0u) {
      warned_lens_types |= bit;
      GUI_LOG_WARNING("ComputeDragGainDegPerPixel: no drag-gain law for lens_type {}; falling back to {} deg/px",
                      lens_type, kLegacyGainDegPerPixel);
    }
    return kLegacyGainDegPerPixel;
  }
  return rad_per_px * kDragSensitivity * 180.0f / kPi;
}

// The two grid level lists into their shared packed vec4 uniform array, and the circles into
// theirs. Each list is clamped to its capacity — the shader reads `count` elements and never past
// what was uploaded — copied into a zero-padded scratch buffer so the last vec4 is whole, and the
// two grid ranges are SORTED ascending, which is the precondition the shader's nearest-level
// search (nearestGridLevelDistDeg) rests on. Sorting here rather than asking callers to is what
// makes that precondition a fact of the upload instead of a contract every caller has to know
// about; the order of a level list carries no meaning anywhere else.
static void UploadGridLevels(unsigned int program, const std::vector<float>& elevation_deg,
                             const std::vector<float>& longitude_deg) {
  const int ec = std::min(static_cast<int>(elevation_deg.size()), kMaxOverlayLevels);
  const int lc = std::min(static_cast<int>(longitude_deg.size()), kMaxOverlayLevels);
  const int vec4_count = (ec + lc + 3) / 4;
  std::vector<float> packed(static_cast<size_t>(vec4_count) * 4, 0.0f);
  std::copy(elevation_deg.begin(), elevation_deg.begin() + ec, packed.begin());
  std::copy(longitude_deg.begin(), longitude_deg.begin() + lc, packed.begin() + ec);
  std::sort(packed.begin(), packed.begin() + ec);
  std::sort(packed.begin() + ec, packed.begin() + ec + lc);
  // "[0]" rather than the bare array name, for the reason the marker upload below gives.
  if (vec4_count > 0) {
    glUniform4fv(glGetUniformLocation(program, "u_grid_levels_deg[0]"), vec4_count, packed.data());
  }
  glUniform1i(glGetUniformLocation(program, "u_elevation_count"), ec);
  glUniform1i(glGetUniformLocation(program, "u_longitude_count"), lc);
}

// One ring family's level list, packed four to a vec4. Called once per family with that family's
// two uniform names — the packing is written once, the destination is a parameter, so the sun
// circles and the view circles cannot come to pack their lists two different ways.
static void UploadCircleLevels(unsigned int program, const char* deg_uniform_first_elem, const char* count_uniform,
                               const std::vector<float>& circle_deg) {
  const int count = std::min(static_cast<int>(circle_deg.size()), kMaxAnnotationCircles);
  const int vec4_count = (count + 3) / 4;
  std::vector<float> packed(static_cast<size_t>(vec4_count) * 4, 0.0f);
  std::copy(circle_deg.begin(), circle_deg.begin() + count, packed.begin());
  if (vec4_count > 0) {
    glUniform4fv(glGetUniformLocation(program, deg_uniform_first_elem), vec4_count, packed.data());
  }
  glUniform1i(glGetUniformLocation(program, count_uniform), count);
}

void PreviewRenderer::Render(int vp_x, int vp_y, int vp_w, int vp_h, const PreviewParams& params) {
  // The far-side variant only while it has something to draw; see kFragmentShader.
  const bool globe_back_fade = params.view_proj.lens_type == kLensTypeGlobe && params.view_proj.globe_back_fade > 0.0f;
  const unsigned int program = globe_back_fade ? globe_back_fade_program_ : shader_program_;
  if (!program || !texture_ || vp_w <= 0 || vp_h <= 0) {
    return;
  }

  // Consume any deferred GL blank request queued by ClearTexture() before
  // any texture binding / sampling below runs, so the sim layer samples a
  // fresh 1x1 black instead of the previous scene's stale pixels. Must run
  // before glActiveTexture/glBindTexture(texture_) below to avoid stashing
  // state; UploadBlankSimTexture unbinds itself and — since the blank can
  // fail under real pressure — now owns the flag: it clears needs_gl_blank_
  // and re-arms sim_layer_valid_ only on a verified define, and leaves both
  // in the revoked state for the next frame's retry when the define fails.
  if (needs_gl_blank_) {
    UploadBlankSimTexture();
  }

  // Save/restore GL state that ImGui might depend on
  GLint prev_program;
  glGetIntegerv(GL_CURRENT_PROGRAM, &prev_program);
  GLint prev_vao;
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prev_vao);
  GLint prev_viewport[4];
  glGetIntegerv(GL_VIEWPORT, prev_viewport);

  glViewport(vp_x, vp_y, vp_w, vp_h);
  glUseProgram(program);

  // Set uniforms
  glUniform2f(glGetUniformLocation(program, "u_resolution"), static_cast<float>(vp_w), static_cast<float>(vp_h));
  glUniform1i(glGetUniformLocation(program, "u_lens_type"), params.view_proj.lens_type);
  glUniform1f(glGetUniformLocation(program, "u_fov"), params.view_proj.fov);
  glUniform1i(glGetUniformLocation(program, "u_visible"), params.view_proj.visible);
  glUniform1i(glGetUniformLocation(program, "u_front"), params.view_proj.front ? 1 : 0);
  if (globe_back_fade) {
    glUniform1f(glGetUniformLocation(program, "u_globe_back_fade"), params.view_proj.globe_back_fade);
  }
  glUniform1i(glGetUniformLocation(program, "u_tex_mode"), static_cast<int>(tex_mode_));
  // The no-image gate: 0 makes the shader skip the sim fetch entirely (see sim_layer_valid_).
  // Set on both programs' source, so one lookup per frame whichever variant is bound.
  glUniform1i(glGetUniformLocation(program, "u_sim_valid"), sim_layer_valid_ ? 1 : 0);
  glUniform1f(glGetUniformLocation(program, "u_xyz_scale"), tex_xyz_scale_);
  glUniform3f(glGetUniformLocation(program, "u_background"), params.background_color_linear[0],
              params.background_color_linear[1], params.background_color_linear[2]);
  glUniform3f(glGetUniformLocation(program, "u_paper"), params.paper_color_linear[0], params.paper_color_linear[1],
              params.paper_color_linear[2]);
  glUniform1i(glGetUniformLocation(program, "u_tone"), params.tone);
  glUniform1i(glGetUniformLocation(program, "u_display_mode"), params.display_mode);
  glUniform1f(glGetUniformLocation(program, "u_intensity_scale"), params.exposure.intensity_scale);
  glUniform1f(glGetUniformLocation(program, "u_max_abs_dz"), params.source.max_abs_dz);
  glUniform1f(glGetUniformLocation(program, "u_r_scale"), params.source.r_scale);

  float view_matrix[9];
  BuildViewMatrix(params.view_proj.elevation, params.view_proj.azimuth, params.view_proj.roll, view_matrix);
  glUniformMatrix3fv(glGetUniformLocation(program, "u_view_matrix"), 1, GL_FALSE, view_matrix);

  // Bind equirect texture to unit 0
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_);
  glUniform1i(glGetUniformLocation(program, "u_texture"), 0);

  // Background image uniforms
  glUniform1i(glGetUniformLocation(program, "u_bg_enabled"), params.bg.enabled ? 1 : 0);
  if (params.bg.enabled) {
    glUniform1f(glGetUniformLocation(program, "u_overlay_alpha"), params.bg.alpha);

    // CPU-side contain-fit + pan/zoom UV calculation (ComputeBgUvTransform, declared in the
    // header so it is unit-testable without a GL context).
    const BgUvTransform bg_uv =
        ComputeBgUvTransform(vp_w, vp_h, params.bg.aspect, params.bg.pan_x, params.bg.pan_y, params.bg.zoom);
    glUniform2f(glGetUniformLocation(program, "u_bg_uv_scale"), bg_uv.scale_x, bg_uv.scale_y);
    glUniform2f(glGetUniformLocation(program, "u_bg_uv_offset"), bg_uv.offset_x, bg_uv.offset_y);

    // Bind bg texture to unit 1
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bg_texture_);
    glUniform1i(glGetUniformLocation(program, "u_bg_texture"), 1);
    glActiveTexture(GL_TEXTURE0);
  }

  // Auxiliary line overlay uniforms
  const auto& ov = params.overlay;
  glUniform1i(glGetUniformLocation(program, "u_show_horizon"), ov.show_horizon ? 1 : 0);
  glUniform1i(glGetUniformLocation(program, "u_show_grid"), ov.show_grid ? 1 : 0);
  glUniform1i(glGetUniformLocation(program, "u_show_sun_circles"), ov.show_sun_circles ? 1 : 0);
  glUniform1i(glGetUniformLocation(program, "u_show_view_dist"), ov.show_view_dist ? 1 : 0);
  // The curve definitions: level lists, packed four to a vec4 (see the uniform block in
  // kFragmentShader for why), the circles' centre, and the line-width rule's constants — the
  // latter from their single owner in src/util/, never as digits in the GLSL source.
  UploadGridLevels(program, ov.elevation_deg, ov.longitude_deg);
  UploadCircleLevels(program, "u_angular_dist_deg[0]", "u_angular_dist_count", ov.angular_dist_deg);
  UploadCircleLevels(program, "u_view_dist_deg[0]", "u_view_dist_count", ov.view_dist_deg);
  glUniform3f(glGetUniformLocation(program, "u_reference_dir"), ov.reference_dir[0], ov.reference_dir[1],
              ov.reference_dir[2]);
  glUniform1f(glGetUniformLocation(program, "u_line_fwidth_min_deg"), kAnnotationLineFwidthMinDeg);
  glUniform1f(glGetUniformLocation(program, "u_line_fwidth_max_deg"), kAnnotationLineFwidthMaxDeg);
  glUniform1f(glGetUniformLocation(program, "u_line_half_width_px"), kAnnotationLineHalfWidthPx);
  glUniform3f(glGetUniformLocation(program, "u_horizon_color"), ov.horizon_color[0], ov.horizon_color[1],
              ov.horizon_color[2]);
  glUniform3f(glGetUniformLocation(program, "u_grid_color"), ov.grid_color[0], ov.grid_color[1], ov.grid_color[2]);
  glUniform3f(glGetUniformLocation(program, "u_sun_circles_color"), ov.sun_circles_color[0], ov.sun_circles_color[1],
              ov.sun_circles_color[2]);
  glUniform3f(glGetUniformLocation(program, "u_view_dist_color"), ov.view_dist_color[0], ov.view_dist_color[1],
              ov.view_dist_color[2]);
  glUniform1f(glGetUniformLocation(program, "u_horizon_alpha"), ov.horizon_alpha);
  glUniform1f(glGetUniformLocation(program, "u_grid_alpha"), ov.grid_alpha);
  glUniform1f(glGetUniformLocation(program, "u_sun_circles_alpha"), ov.sun_circles_alpha);
  glUniform1f(glGetUniformLocation(program, "u_view_dist_alpha"), ov.view_dist_alpha);

  // "[0]" and not the bare array name: glGetUniformLocation is specified to resolve the FIRST
  // ELEMENT of an array, and while most drivers also accept the bare name, the spec does not
  // require it. A miss returns -1, which glUniform* then silently ignores — so the failure mode
  // this spelling avoids is six markers that never draw and no error anywhere.
  // std::array's elements are contiguous and each inner array is exactly its two/three floats, so
  // one *fv call with a count of six uploads the whole family.
  static_assert(sizeof(ov.marker_screen_pos) == sizeof(float) * 2 * LUMICE_ANNOTATION_MARKER_COUNT,
                "marker_screen_pos must be a flat float[N][2] for the single glUniform2fv upload");
  static_assert(sizeof(ov.marker_color) == sizeof(float) * 3 * LUMICE_ANNOTATION_MARKER_COUNT,
                "marker_color must be a flat float[N][3] for the single glUniform3fv upload");
  glUniform2fv(glGetUniformLocation(program, "u_marker_screen_pos[0]"), LUMICE_ANNOTATION_MARKER_COUNT,
               ov.marker_screen_pos[0].data());
  glUniform3fv(glGetUniformLocation(program, "u_marker_color[0]"), LUMICE_ANNOTATION_MARKER_COUNT,
               ov.marker_color[0].data());
  glUniform1f(glGetUniformLocation(program, "u_markers_radius_px"), ov.markers_radius_px);
  glUniform1f(glGetUniformLocation(program, "u_markers_alpha"), ov.markers_alpha);

  glUniform1i(glGetUniformLocation(program, "u_show_lens_border"), ov.show_lens_border ? 1 : 0);
  glUniform3f(glGetUniformLocation(program, "u_lens_border_color"), ov.lens_border_color[0], ov.lens_border_color[1],
              ov.lens_border_color[2]);
  glUniform1f(glGetUniformLocation(program, "u_lens_border_alpha"), ov.lens_border_alpha);

  // Draw fullscreen quad
  glBindVertexArray(vao_);
  glDrawArrays(GL_TRIANGLES, 0, 6);

  // Restore state
  glBindVertexArray(static_cast<GLuint>(prev_vao));
  glUseProgram(static_cast<GLuint>(prev_program));
  glViewport(prev_viewport[0], prev_viewport[1], prev_viewport[2], prev_viewport[3]);
}

}  // namespace lumice::gui
