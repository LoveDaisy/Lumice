// C API bridge for lumice_render.h: annotation anchors and marker directions, pixel <-> direction
// on a view, lens limits, colour conversion and the EV anchor. Registered at the render layer.

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

#include "config/render_config.hpp"
#include "core/annotation_overlay.hpp"  // annotation::ComputeAnchors (LUMICE_ComputeAnnotationAnchors)
#include "core/ev_anchor.hpp"
#include "core/geo3d.hpp"
#include "core/lens_proj_build.hpp"  // mask_detail::PixelToWorld + the display clips (LUMICE_UnprojectPixel)
#include "core/scatter_accum.hpp"    // MakeCameraRotation (LUMICE_UnprojectPixel)
#include "include/lumice_render.h"
#include "include/lumice_scene.h"
#include "server/c_api_render_internal.hpp"
#include "server/c_api_scene_internal.hpp"  // ns::capi::IsValidMarkerId
#include "util/color_space.hpp"

namespace ns = lumice;

// LUMICE_MAX_CONFIG_MARKERS (lumice_scene.h) is spelled as a literal 6, because the id-count macro
// lives in lumice_render.h, a layer above the renderer struct that needs it. That literal is not a
// second opinion about how many ids there are: the renderer's array is EXACTLY the id space, since
// duplicates are rejected. This is the line that makes adding a seventh id to one side a compile
// error rather than a list that silently cannot hold them all — and the reason it is here is that
// this is the lowest bridge that sees both headers.
static_assert(LUMICE_MAX_CONFIG_MARKERS == LUMICE_ANNOTATION_MARKER_COUNT,
              "LUMICE_RenderParam::markers must have room for exactly the marker id space");

// =============== Annotation Anchors ===============
// Bridge only: the geometry and the curve walk live in core/annotation_overlay.hpp. What is here
// is the ABI shape — validation, the enum translation, and the one heap allocation the C caller
// releases.

namespace {

// Everything LUMICE_AnnotationAnchors's pointers point into, kept in one object so a single
// Release frees the lot. Owned through the struct's opaque `storage` handle rather than through
// the individual pointers: the caller has one thing to release, and the released state is
// expressible (all pointers NULL) so a double Release is a no-op instead of a double free.
struct AnnotationStorage {
  std::vector<LUMICE_AnnotationLabel> labels;
  std::vector<LUMICE_AnnotationMarkerPoint> marker_points;
};

// A request angle list, validated and copied. Returns false with `err` set on a malformed list.
bool ReadAngleList(const float* data, int count, int cap, std::vector<float>* out, LUMICE_ErrorCode* err) {
  if (count < 0 || count > cap) {
    *err = LUMICE_ERR_INVALID_VALUE;
    return false;
  }
  if (count > 0 && data == nullptr) {
    *err = LUMICE_ERR_NULL_ARG;
    return false;
  }
  out->assign(data, data + count);
  return true;
}

// The marker id space is declared twice — as core's MarkerId enum and as the
// LUMICE_ANNOTATION_MARKER_* macros — because the C header cannot include the C++ one. This cast
// is the ONLY place the two meet, so the equality that makes it sound is asserted right here
// rather than described in a comment somewhere: reordering either side, or adding an id to one of
// them alone, becomes a compile error instead of a marker that silently resolves to the wrong
// direction. The count line is the half that catches an ADDITION (the per-id lines all still pass
// when a seventh id is appended on one side only).
static_assert(static_cast<int>(lumice::annotation::kMarkerZenith) == LUMICE_ANNOTATION_MARKER_ZENITH,
              "core MarkerId and LUMICE_ANNOTATION_MARKER_* have diverged");
static_assert(static_cast<int>(lumice::annotation::kMarkerNadir) == LUMICE_ANNOTATION_MARKER_NADIR,
              "core MarkerId and LUMICE_ANNOTATION_MARKER_* have diverged");
static_assert(static_cast<int>(lumice::annotation::kMarkerSun) == LUMICE_ANNOTATION_MARKER_SUN,
              "core MarkerId and LUMICE_ANNOTATION_MARKER_* have diverged");
static_assert(static_cast<int>(lumice::annotation::kMarkerSubsun) == LUMICE_ANNOTATION_MARKER_SUBSUN,
              "core MarkerId and LUMICE_ANNOTATION_MARKER_* have diverged");
static_assert(static_cast<int>(lumice::annotation::kMarkerAnthelion) == LUMICE_ANNOTATION_MARKER_ANTHELION,
              "core MarkerId and LUMICE_ANNOTATION_MARKER_* have diverged");
static_assert(static_cast<int>(lumice::annotation::kMarkerAntisolar) == LUMICE_ANNOTATION_MARKER_ANTISOLAR,
              "core MarkerId and LUMICE_ANNOTATION_MARKER_* have diverged");
static_assert(static_cast<int>(lumice::annotation::kMarkerCount) == LUMICE_ANNOTATION_MARKER_COUNT,
              "a marker id was added to one side of the C API boundary only");

// The label kinds cross the same boundary the same way (`dst.kind = static_cast<int>(l.kind)` in
// LUMICE_ComputeAnnotationAnchors) and had no such guard until the fifth family was added; a
// consumer that styles by kind would otherwise learn about a divergence from a mis-coloured label.
static_assert(static_cast<int>(lumice::annotation::kLabelHorizon) == LUMICE_ANNOTATION_HORIZON,
              "core LabelKind and LUMICE_ANNOTATION_* have diverged");
static_assert(static_cast<int>(lumice::annotation::kLabelElevation) == LUMICE_ANNOTATION_ELEVATION,
              "core LabelKind and LUMICE_ANNOTATION_* have diverged");
static_assert(static_cast<int>(lumice::annotation::kLabelLongitude) == LUMICE_ANNOTATION_LONGITUDE,
              "core LabelKind and LUMICE_ANNOTATION_* have diverged");
static_assert(static_cast<int>(lumice::annotation::kLabelAngularDist) == LUMICE_ANNOTATION_ANGULAR_DIST,
              "core LabelKind and LUMICE_ANNOTATION_* have diverged");
static_assert(static_cast<int>(lumice::annotation::kLabelViewDist) == LUMICE_ANNOTATION_VIEW_DIST,
              "core LabelKind and LUMICE_ANNOTATION_* have diverged");

// A request marker id list, validated and converted. Returns false with `err` set on a malformed
// list. The range check is not redundant with ResolveMarkerDir's own default branch: it is what
// turns a caller's bad id into a reported error instead of a silent fallback to the zenith.
bool ReadMarkerIdList(const int* data, int count, std::vector<lumice::annotation::MarkerId>* out,
                      LUMICE_ErrorCode* err) {
  if (count < 0 || count > LUMICE_MAX_ANNOTATION_MARKERS) {
    *err = LUMICE_ERR_INVALID_VALUE;
    return false;
  }
  if (count > 0 && data == nullptr) {
    *err = LUMICE_ERR_NULL_ARG;
    return false;
  }
  out->clear();
  out->reserve(static_cast<size_t>(count));
  for (int i = 0; i < count; ++i) {
    if (!ns::capi::IsValidMarkerId(data[i])) {
      *err = LUMICE_ERR_INVALID_VALUE;
      return false;
    }
    out->push_back(static_cast<lumice::annotation::MarkerId>(data[i]));
  }
  return true;
}

}  // namespace


// Single owner of the LUMICE_AnnotationView -> lumice::annotation::ViewSnapshot field mapping
// (a56). Declared in c_api_render_internal.hpp so test/support/lumice_test_api.cpp's
// LUMICE_TEST_ComputeRenderDomainMask (the test-only door to the render-domain mask, which this
// product API no longer exports) can call it too instead of carrying a second hand-copied
// translation.
lumice::annotation::ViewSnapshot ToAnnotationViewSnapshot(const LUMICE_AnnotationView& v) {
  ns::annotation::ViewSnapshot view;
  view.width = v.width;
  view.height = v.height;
  view.lens_type = static_cast<ns::LensParam::LensType>(v.lens_type);
  view.fov_deg = v.lens_fov;
  view.lens_shift[0] = v.lens_shift[0];
  view.lens_shift[1] = v.lens_shift[1];
  view.overlap = v.overlap;
  view.az_deg = v.view_azimuth;
  view.el_deg = v.view_elevation;
  view.roll_deg = v.view_roll;
  view.visible = static_cast<ns::RenderConfig::VisibleRange>(v.visible);
  view.front = v.front != 0;
  return view;
}


LUMICE_ErrorCode LUMICE_ComputeAnnotationAnchors(const LUMICE_AnnotationRequest* request,
                                                 LUMICE_AnnotationAnchors* out) {
  if (!request || !out) {
    return LUMICE_ERR_NULL_ARG;
  }
  const LUMICE_AnnotationView& v = request->view;
  if (v.lens_type < 0 || v.lens_type > LUMICE_LENS_TYPE_GLOBE) {
    return LUMICE_ERR_INVALID_VALUE;
  }
  if (v.visible != LUMICE_VISIBLE_UPPER && v.visible != LUMICE_VISIBLE_LOWER && v.visible != LUMICE_VISIBLE_FULL) {
    return LUMICE_ERR_INVALID_VALUE;
  }

  lumice::annotation::Request req;
  req.view = ToAnnotationViewSnapshot(v);
  req.horizon = request->horizon != 0;
  req.reference_dir[0] = request->reference_dir[0];
  req.reference_dir[1] = request->reference_dir[1];
  req.reference_dir[2] = request->reference_dir[2];
  // The anchors are all this call computes, so the curve walk is always on; `labels` exists on the
  // core request for the CLI's mask-only calls.
  req.labels = true;

  LUMICE_ErrorCode err = LUMICE_OK;
  if (!ReadAngleList(request->elevation_deg, request->elevation_count, LUMICE_MAX_ANNOTATION_LINES, &req.elevation_deg,
                     &err) ||
      !ReadAngleList(request->longitude_deg, request->longitude_count, LUMICE_MAX_ANNOTATION_LINES, &req.longitude_deg,
                     &err) ||
      !ReadAngleList(request->angular_dist_deg, request->angular_dist_count, LUMICE_MAX_ANNOTATION_CIRCLES,
                     &req.angular_dist_deg, &err) ||
      !ReadAngleList(request->view_dist_deg, request->view_dist_count, LUMICE_MAX_ANNOTATION_CIRCLES,
                     &req.view_dist_deg, &err) ||
      !ReadMarkerIdList(request->marker_ids, request->marker_count, &req.markers, &err)) {
    return err;
  }

  std::unique_ptr<AnnotationStorage> storage;
  try {
    storage = std::make_unique<AnnotationStorage>();
    const lumice::annotation::Anchors anchors = lumice::annotation::ComputeAnchors(req);

    storage->labels.reserve(anchors.labels.size());
    for (const lumice::annotation::Label& l : anchors.labels) {
      LUMICE_AnnotationLabel dst{};
      dst.px = l.px;
      dst.py = l.py;
      dst.kind = static_cast<int>(l.kind);
      dst.index = l.index;
      dst.value_deg = l.value_deg;
      // Truncation cannot happen for any angle core formats (see LUMICE_ANNOTATION_LABEL_MAX), but
      // the copy is bounded anyway: a silently over-long text would otherwise be a buffer overrun
      // rather than a short label.
      const size_t n = std::min(l.text.size(), sizeof(dst.text) - 1);
      std::memcpy(dst.text, l.text.data(), n);
      dst.text[n] = '\0';
      storage->labels.push_back(dst);
    }

    storage->marker_points.reserve(anchors.markers.size());
    for (const lumice::annotation::CanvasPoint& p : anchors.markers) {
      storage->marker_points.push_back({ p.px, p.py, p.valid ? 1 : 0 });
    }
  } catch (...) {
    return LUMICE_ERR_UNKNOWN;
  }

  out->labels = storage->labels.empty() ? nullptr : storage->labels.data();
  out->label_count = static_cast<int>(storage->labels.size());
  out->marker_points = storage->marker_points.empty() ? nullptr : storage->marker_points.data();
  out->marker_count = static_cast<int>(storage->marker_points.size());
  out->storage = storage.release();
  return LUMICE_OK;
}


void LUMICE_ReleaseAnnotationAnchors(LUMICE_AnnotationAnchors* anchors) {
  if (!anchors || !anchors->storage) {
    return;  // NULL-safe, and idempotent on an already-released or zero-initialized struct
  }
  const std::unique_ptr<AnnotationStorage> owned(static_cast<AnnotationStorage*>(anchors->storage));
  anchors->storage = nullptr;
  // Leave no dangling view of freed memory behind, so a caller that keeps reading the struct after
  // Release sees "nothing here" rather than a use-after-free.
  anchors->labels = nullptr;
  anchors->label_count = 0;
  anchors->marker_points = nullptr;
  anchors->marker_count = 0;
}


// Normalize `in` into `out`. Shared by the two direction queries below so "a caller may pass an
// unnormalized sun_dir" is honoured in one place rather than in each of them.
// A zero-length vector is not an error here: it is handed to core as-is, whose two
// pole ids ignore it and whose sun-relative ones reflect it, and SunHorizonDir's own degenerate
// branch catches it — the same answer a near-pole sun gets.
static void NormalizeSunDir(const float in[3], float out[3]) {
  const float len = std::sqrt(in[0] * in[0] + in[1] * in[1] + in[2] * in[2]);
  if (len <= 0.0f) {
    out[0] = in[0];
    out[1] = in[1];
    out[2] = in[2];
    return;
  }
  out[0] = in[0] / len;
  out[1] = in[1] / len;
  out[2] = in[2] / len;
}

LUMICE_ErrorCode LUMICE_ResolveAnnotationMarkerDirection(int marker_id, const float sun_dir[3], float out_dir[3]) {
  if (!sun_dir || !out_dir) {
    return LUMICE_ERR_NULL_ARG;
  }
  // Not redundant with ResolveMarkerDir's own default branch: core answers an unknown id with the
  // zenith by design, and turning that into a REPORTED error is this boundary's job — the same
  // reasoning ReadMarkerIdList's range check carries.
  if (!ns::capi::IsValidMarkerId(marker_id)) {
    return LUMICE_ERR_INVALID_VALUE;
  }
  float unit[3];
  NormalizeSunDir(sun_dir, unit);
  lumice::annotation::ResolveMarkerDir(static_cast<lumice::annotation::MarkerId>(marker_id), unit, out_dir);
  return LUMICE_OK;
}


LUMICE_ErrorCode LUMICE_ResolveSunHorizonDirection(const float sun_dir[3], float out_dir[3]) {
  if (!sun_dir || !out_dir) {
    return LUMICE_ERR_NULL_ARG;
  }
  float unit[3];
  NormalizeSunDir(sun_dir, unit);
  lumice::annotation::SunHorizonDir(unit, out_dir);
  return LUMICE_OK;
}


LUMICE_ErrorCode LUMICE_UnprojectPixel(const LUMICE_AnnotationView* view, int px, int py, float out_dir[3],
                                       int* out_valid) {
  if (!view || !out_dir || !out_valid) {
    return LUMICE_ERR_NULL_ARG;
  }
  if (!ns::capi::AnnotationViewEnumsValid(*view) || view->width <= 0 || view->height <= 0) {
    return LUMICE_ERR_INVALID_VALUE;
  }
  *out_valid = 0;
  if (px < 0 || py < 0 || px >= view->width || py >= view->height) {
    return LUMICE_OK;  // not a pixel of this canvas
  }
  // The same three steps every mask and every annotation anchor of a view are built from —
  // ToRenderConfig, MakeCameraRotation, BuildProjParams — and then core's ONE per-pixel inverse.
  const ns::RenderConfig cfg = ns::annotation::ToRenderConfig(ToAnnotationViewSnapshot(*view));
  const ns::Rotation rot = ns::MakeCameraRotation(cfg);
  const float short_pix = static_cast<float>(std::min(cfg.resolution_[0], cfg.resolution_[1]));
  const lm_proj::ProjParams params = ns::BuildProjParams(cfg, rot, short_pix);
  const ns::mask_detail::MaskDir dir = ns::mask_detail::PixelToWorld(cfg, params, rot, px, py);
  if (!dir.valid) {
    return LUMICE_OK;
  }
  // Both display clips, exactly as the IN_FRAME membership test and the render-domain mask apply
  // them: a pixel the view does not show images no sky, so it is not a direction to point at.
  float forward[3];
  ns::mask_detail::CameraForward(rot, forward);
  if (!ns::mask_detail::VisibleByRange(cfg.visible_, dir.z) ||
      !ns::mask_detail::FrontVisible(cfg.front_, forward, dir.x, dir.y, dir.z)) {
    return LUMICE_OK;
  }
  out_dir[0] = dir.x;
  out_dir[1] = dir.y;
  out_dir[2] = dir.z;
  *out_valid = 1;
  return LUMICE_OK;
}

LUMICE_ErrorCode LUMICE_ProjectDirection(const LUMICE_AnnotationView* view, const float dir[3], float* out_px,
                                         float* out_py, int* out_valid) {
  if (!view || !dir || !out_px || !out_py || !out_valid) {
    return LUMICE_ERR_NULL_ARG;
  }
  if (!ns::capi::AnnotationViewEnumsValid(*view) || view->width <= 0 || view->height <= 0) {
    return LUMICE_ERR_INVALID_VALUE;
  }
  // Core's marker sampler on a caller-supplied direction: the projection, the canvas clamp and
  // the hemisphere slack are the ones LUMICE_ComputeAnnotationAnchors' marker points get.
  const ns::annotation::CanvasPoint p = ns::annotation::ProjectDirectionOnView(ToAnnotationViewSnapshot(*view), dir);
  *out_valid = p.valid ? 1 : 0;
  if (!p.valid) {
    return LUMICE_OK;
  }
  *out_px = p.px;
  *out_py = p.py;
  return LUMICE_OK;
}


// =============== Lens Type ===============
float LUMICE_MaxFov(LUMICE_LensType type) {
  return ns::MaxFov(static_cast<ns::LensParam::LensType>(type));
}

// =============== Color Conversion ===============
LUMICE_ErrorCode LUMICE_XyzToSrgbUint8(const float* xyz_in, unsigned char* out, int pixel_count,
                                       float intensity_scale) {
  if (!xyz_in || !out) {
    return LUMICE_ERR_NULL_ARG;
  }
  ns::XyzToSrgbUint8(xyz_in, out, pixel_count, intensity_scale);
  return LUMICE_OK;
}

LUMICE_ErrorCode LUMICE_XyzToSrgbUint8WithBackground(const float* xyz_in, unsigned char* out, int pixel_count,
                                                     float intensity_scale, const float* background_linear) {
  if (!xyz_in || !out || !background_linear) {
    return LUMICE_ERR_NULL_ARG;
  }
  ns::XyzToSrgbUint8(xyz_in, out, pixel_count, intensity_scale, background_linear);
  return LUMICE_OK;
}

// =============== EV Auto Anchor ===============
// Thin forwards to core/ev_anchor.hpp, the single owner of the anchor algorithm. The bare-float,
// no-NULL-check contract matches LUMICE_MaxFov and the precondition documented on the header
// declaration; it is the contract the GUI-side implementation these replaced already had.
float LUMICE_ComputeP99Y(const float* xyz_data, int img_width, int img_height, int downsample_factor) {
  return ns::ComputeP99Y(xyz_data, img_width, img_height, downsample_factor);
}

float LUMICE_ComputeEvAuto(float p99_raw_y, float snapshot_intensity, float target_white) {
  return ns::ComputeEvAuto(p99_raw_y, snapshot_intensity, target_white);
}
