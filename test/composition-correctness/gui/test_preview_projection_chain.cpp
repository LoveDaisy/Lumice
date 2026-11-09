// Composition chain: the preview's lens model, shared by everything drawn on top of it.
//
// Units in the chain: app_panels × preview_renderer × overlay_labels × gui_state.
//
// overlay_labels is a full member here, not fixture scaffolding: it carries the second, independent
// implementation of the same lens model (pixel -> direction), and the strongest case below is that
// the two implementations must invert each other. Neither unit can make that check alone.
//
// What the collaboration produces that is observable: where an overlay marker lands on the preview,
// and how far the image moves when the user drags it. Both are answers to the same question — what
// angle does this pixel correspond to — and both are computed from the renderer sub-state through
// the same lens. When they stop agreeing, nothing errors: the zenith marker simply sits somewhere
// that is not the zenith, which reads as a simulation that got the sky wrong, and dragging under a
// wide-angle lens becomes unusably fast, which reads as a broken mouse.
//
// Derived from the src call graph: app_panels.cpp -> preview_renderer is 11 call sites, and app.cpp
// -> preview_renderer is 16 — between them the projection helpers, the background aspect and the
// upload entry points.

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <unordered_set>

#include "gui/analysis_panel.hpp"
#include "gui/analyze_map_view.hpp"
#include "gui/app.hpp"  // AnnotationViewInputFor
#include "gui/gui_state.hpp"
#include "gui/overlay_labels.hpp"
#include "gui/preview_renderer.hpp"
#include "include/lumice_scene.h"
#include "nlohmann/json.hpp"
#include "server/c_api_map_storage.hpp"
#include "support/map_stress_fixture.hpp"

namespace lumice::gui {
namespace {

constexpr float kSentinel = -9999.0f;

bool IsSentinel(const std::array<float, 2>& p) {
  return p[0] == kSentinel && p[1] == kSentinel;
}

RenderConfig LensAt(int lens_type, float fov_deg, float elevation_deg) {
  RenderConfig rc;
  rc.lens_type = lens_type;
  rc.fov = fov_deg;
  rc.elevation = elevation_deg;
  rc.azimuth = 0.0f;
  rc.roll = 0.0f;
  return rc;
}

template <class T>
size_t CapacityBytes(const std::vector<T>& values) {
  return values.capacity() * sizeof(T);
}
size_t MemberCapacity(const raypath::MemberScope& members) {
  size_t bytes = CapacityBytes(members.members);
  for (const auto& member : members.members)
    bytes += CapacityBytes(member);
  return bytes;
}
size_t ContextCapacity(const std::shared_ptr<const raypath::QuantityContext>& context,
                       std::unordered_set<const void*>& visited) {
  if (!context || !visited.insert(context.get()).second)
    return 0;
  size_t bytes = sizeof(*context) + MemberCapacity(context->members);
  if (const auto* spectrum = std::get_if<std::vector<WlParam>>(&context->light.spectrum_))
    bytes += CapacityBytes(*spectrum);
  return bytes;
}
size_t QuantityCapacity(const raypath::QuantitySpec& spec, std::unordered_set<const void*>& visited) {
  return CapacityBytes(spec.wavelengths_nm) + ContextCapacity(spec.context, visited);
}
nlohmann::json MapCapacityRow(const LUMICE_PathMap* handle) {
  const auto& data = handle->result.Data();
  const auto& finite = std::get<raypath::Finite<raypath::MapValues>>(data.claim);
  const auto& map = finite.value.map;
  std::unordered_set<const void*> contexts;
  size_t meta = sizeof(*handle) + sizeof(data) + sizeof(raypath::ResultIdentity) + sizeof(LUMICE_PathMapPixelColumns);
  meta += MemberCapacity(data.members) + CapacityBytes(data.meta.faces) + CapacityBytes(data.meta.shape);
  meta += data.meta.crystal_kind.capacity() + 1;
  for (const auto& scalar : data.meta.shape)
    meta += scalar.name.capacity() + 1;
  meta += CapacityBytes(map.member_kept_counts) + finite.coverage.geometry_basis.capacity() + 1;
  meta += QuantityCapacity(finite.value.quantity, contexts) + MemberCapacity(finite.scope.members);
  if (finite.scope.quantity)
    meta += QuantityCapacity(*finite.scope.quantity, contexts);
  meta += ContextCapacity(finite.scope.input, contexts);
  if (map.point_mass) {
    meta += CapacityBytes(map.point_mass->methods);
    for (const auto& method : map.point_mass->methods)
      meta += method.capacity() + 1;
  }
  if (handle->method_pointers)
    meta += sizeof(*handle->method_pointers) + CapacityBytes(*handle->method_pointers);
  const auto& c = *handle->pixels;
  const size_t columns = CapacityBytes(c.delta_deg) + CapacityBytes(c.azimuth_deg) + CapacityBytes(c.values) +
                         CapacityBytes(c.counts) + CapacityBytes(c.status);
  // Capacities are actual owners. String SSO is conservatively counted again; allocator and
  // shared_ptr control blocks are separately reserved, not pretended to be measured RSS.
  return { { "cell_size", sizeof(map.pixels[0]) },
           { "typed_capacity", map.pixels.capacity() },
           { "typed_bytes", CapacityBytes(map.pixels) },
           { "column_capacities",
             { c.delta_deg.capacity(), c.azimuth_deg.capacity(), c.values.capacity(), c.counts.capacity(),
               c.status.capacity() } },
           { "column_bytes", columns },
           { "metadata_object_and_capacity_bytes", meta },
           { "control_reserve_bytes", 4096 },
           { "payload_capacity_bound_bytes", CapacityBytes(map.pixels) + columns + meta + 4096 } };
}

TEST(FrozenMapStorageChain, ActualMaximumSourceOwnersHaveBoundedCapacity) {
  LUMICE_Scene* raw_scene = nullptr;
  const std::string path = std::string(LUMICE_E2E_CONFIG_DIR) + "/raypath_feature_random_regular.json";
  ASSERT_EQ(LUMICE_SceneFromJsonFile(path.c_str(), &raw_scene), LUMICE_OK);
  std::unique_ptr<LUMICE_Scene, decltype(&LUMICE_SceneDestroy)> scene(raw_scene, LUMICE_SceneDestroy);
  std::vector<MapHandle> sources;
  nlohmann::json rows = nlohmann::json::array();
  size_t total = 0;
  for (int slot = 0; slot < 3; ++slot) {
    const auto request = lumice::test::MaximumMapRequest(slot);
    const auto measure = [&]() -> bool {
      LUMICE_PathMap* raw = nullptr;
      char error[512]{};
      const auto code = LUMICE_AnalyzePathMap(scene.get(), &request, &raw, error, sizeof(error));
      if (code != LUMICE_OK) {
        ADD_FAILURE() << error;
        return false;
      }
      sources.emplace_back(raw, MapDeleter{});
      LUMICE_PathMapPixels pixels{};
      pixels.struct_size = sizeof(pixels);
      EXPECT_EQ(LUMICE_PathMapGetPixels(raw, &pixels), LUMICE_PATH_READ_OK);
      if (pixels.pixel_count != LUMICE_PATH_MAP_MAX_PIXELS) {
        ADD_FAILURE() << pixels.pixel_count;
        return false;
      }
      auto row = MapCapacityRow(raw);
      total += row["payload_capacity_bound_bytes"].get<size_t>();
      rows.push_back(std::move(row));
      return true;
    };
    if (!measure())
      return;
  }
  scene.reset();
  const nlohmann::json report = { { "scope", "three actual maximum C result owners; net payload, not process RSS" },
                                  { "sources", rows },
                                  { "quantity_capacity_bound_bytes", total } };
  RecordProperty("map_storage_metrics", report.dump());
  EXPECT_GT(total, 0u);
  EXPECT_LT(total, 96u * 1024u * 1024u);  // display resources and temporaries are measured in the GL leg
}

// ---------------------------------------------------------------------------------------------
// E13 — the overlay's inverse and the marker projection's forward are inverses of each other.
//
// Two units answer the same question from opposite ends. overlay_labels maps a pixel back to the
// direction it shows (that is how a label knows what it is labelling); preview_renderer maps a
// direction forward to the pixel a marker is drawn at. They are separate implementations of one
// lens model, and nothing forces them to agree — when they drift, the zenith marker is simply drawn
// somewhere that is not the zenith, which reads as a simulation that got the sky wrong.
//
// Round-tripping through both is the check neither unit can make on its own, which is what puts it
// in this layer rather than in either unit's own tests.

TEST(PreviewProjectionChain, PixelToDirectionAndDirectionToPixelAreInverses) {
  constexpr float kResX = 800.0f;
  constexpr float kResY = 600.0f;

  struct LensCase {
    const char* name;
    int lens_type;
    float fov;
    float elevation;
  };
  const LensCase kLenses[] = {
    { "linear 60", LUMICE_LENS_TYPE_LINEAR, 60.0f, 0.0f },
    { "linear 20", LUMICE_LENS_TYPE_LINEAR, 20.0f, 0.0f },
    { "equal-area 120", LUMICE_LENS_TYPE_FISHEYE_EQUAL_AREA, 120.0f, 0.0f },
    { "equal-area 120, looking up", LUMICE_LENS_TYPE_FISHEYE_EQUAL_AREA, 120.0f, 45.0f },
    { "equidistant 180", LUMICE_LENS_TYPE_FISHEYE_EQUIDISTANT, 180.0f, 0.0f },
  };
  // Offsets from the viewport centre, in pixels. The centre itself is the anchor every other
  // marker is placed relative to, so it leads.
  const float kOffsets[][2] = { { 0.0f, 0.0f }, { 40.0f, 0.0f }, { 0.0f, 40.0f }, { -60.0f, 30.0f } };

  for (const LensCase& lens : kLenses) {
    RenderConfig rc = LensAt(lens.lens_type, lens.fov, lens.elevation);
    const ViewProjection vp = BuildPreviewViewProjFromRenderer(rc);
    float view_matrix[9] = {};
    BuildViewMatrix(vp.elevation, vp.azimuth, vp.roll, view_matrix);

    for (const auto& offset : kOffsets) {
      float wx = 0.0f;
      float wy = 0.0f;
      float wz = 0.0f;
      bool valid = false;
      detail::PixelToWorldDirForTesting(offset[0], offset[1], kResX, kResY, lens.lens_type, lens.fov, view_matrix, &wx,
                                        &wy, &wz, &valid);
      if (!valid) {
        continue;  // the pixel is outside this lens's domain; nothing is drawn there either
      }

      const float world_dir[3] = { wx, wy, wz };
      const std::array<float, 2> screen =
          ProjectWorldDirToScreen(vp, world_dir, static_cast<int>(kResX), static_cast<int>(kResY));
      if (IsSentinel(screen)) {
        ADD_FAILURE() << lens.name << " at (" << offset[0] << "," << offset[1]
                      << "): the inverse produced a direction the forward projection cannot place";
        continue;  // this combination has no coordinates to compare; the rest still get checked
      }
      EXPECT_NEAR(screen[0], offset[0], 1.0f) << lens.name << " x";
      EXPECT_NEAR(screen[1], offset[1], 1.0f) << lens.name << " y";
    }
  }
}

// The round trip above would also pass if BOTH directions were the identity on some degenerate
// path, so this pins that the projection actually separates directions: two different pixels must
// come back as two different pixels.
TEST(PreviewProjectionChain, DistinctPixelsStayDistinctThroughTheRoundTrip) {
  const RenderConfig rc = LensAt(LUMICE_LENS_TYPE_FISHEYE_EQUAL_AREA, 120.0f, 0.0f);
  const ViewProjection vp = BuildPreviewViewProjFromRenderer(rc);
  float view_matrix[9] = {};
  BuildViewMatrix(vp.elevation, vp.azimuth, vp.roll, view_matrix);

  auto round_trip = [&](float px, float py) {
    float wx = 0.0f;
    float wy = 0.0f;
    float wz = 0.0f;
    bool valid = false;
    detail::PixelToWorldDirForTesting(px, py, 800.0f, 600.0f, vp.lens_type, vp.fov, view_matrix, &wx, &wy, &wz, &valid);
    EXPECT_TRUE(valid);
    const float dir[3] = { wx, wy, wz };
    return ProjectWorldDirToScreen(vp, dir, 800, 600);
  };

  const std::array<float, 2> a = round_trip(0.0f, 0.0f);
  const std::array<float, 2> b = round_trip(80.0f, 0.0f);
  ASSERT_FALSE(IsSentinel(a));
  ASSERT_FALSE(IsSentinel(b));
  EXPECT_GT(std::abs(b[0] - a[0]), 10.0f) << "two pixels 80 apart collapse to the same place";
}

// A direction behind the camera has no screen position, and the answer for that is a sentinel
// rather than a number. Clamping instead would pin an off-screen marker to the viewport edge,
// where it reads as a real feature sitting at the horizon.
TEST(PreviewProjectionChain, ADirectionBehindTheCameraHasNoScreenPosition) {
  const RenderConfig rc = LensAt(LUMICE_LENS_TYPE_LINEAR, 60.0f, 0.0f);
  const ViewProjection vp = BuildPreviewViewProjFromRenderer(rc);
  float view_matrix[9] = {};
  BuildViewMatrix(vp.elevation, vp.azimuth, vp.roll, view_matrix);

  float wx = 0.0f;
  float wy = 0.0f;
  float wz = 0.0f;
  bool valid = false;
  detail::PixelToWorldDirForTesting(0.0f, 0.0f, 800.0f, 600.0f, vp.lens_type, vp.fov, view_matrix, &wx, &wy, &wz,
                                    &valid);
  ASSERT_TRUE(valid);

  const float behind[3] = { -wx, -wy, -wz };
  EXPECT_TRUE(IsSentinel(ProjectWorldDirToScreen(vp, behind, 800, 600)));
}

// ---------------------------------------------------------------------------------------------
// E13 (drag half) — the drag law's whole purpose is that the ratio is CONSTANT.
//
// The value of the constant is calibration and may be retuned. What may not change is that content
// at the centre of the frame moves the same number of screen pixels per dragged pixel whatever the
// lens, the field of view or the viewport size. A gain that scales with FOV is what makes a
// wide-angle preview impossible to aim.

TEST(PreviewProjectionChain, DragMovesTheSameScreenDistancePerPixelAcrossFovAndViewport) {
  constexpr int kVpW = 800;
  constexpr int kVpH = 600;
  const int kLenses[] = { LUMICE_LENS_TYPE_LINEAR, LUMICE_LENS_TYPE_FISHEYE_EQUAL_AREA,
                          LUMICE_LENS_TYPE_FISHEYE_EQUIDISTANT };
  const float kFovs[] = { 20.0f, 60.0f, 120.0f };

  for (int lens : kLenses) {
    for (float fov : kFovs) {
      const float gain = ComputeDragGainDegPerPixel(lens, fov, kVpW, kVpH);
      if (!(gain > 0.0f)) {
        ADD_FAILURE() << "lens " << lens << " fov " << fov << ": no drag gain";
        continue;
      }

      // Turn the angular gain back into a screen distance: take the direction at the centre of the
      // frame, rotate it by one drag-pixel's worth of angle, and ask where that lands. That round
      // trip is the actual invariant — an angular gain can be "constant" while the projection it
      // feeds is not.
      const RenderConfig rc = LensAt(lens, fov, 0.0f);
      const ViewProjection vp = BuildPreviewViewProjFromRenderer(rc);
      float view_matrix[9] = {};
      BuildViewMatrix(vp.elevation, vp.azimuth, vp.roll, view_matrix);

      float cx = 0.0f;
      float cy = 0.0f;
      float cz = 0.0f;
      bool valid = false;
      detail::PixelToWorldDirForTesting(0.0f, 0.0f, static_cast<float>(kVpW), static_cast<float>(kVpH), lens, fov,
                                        view_matrix, &cx, &cy, &cz, &valid);
      if (!valid) {
        ADD_FAILURE() << "lens " << lens << " fov " << fov << ": the frame centre has no direction";
        continue;
      }

      // Rotate the centre direction by `gain` degrees about an axis perpendicular to it. Any
      // perpendicular will do: the drag law is isotropic by construction.
      // Not `M_PI`: that one is a POSIX extension, and MSVC only defines it when
      // `_USE_MATH_DEFINES` is set before <cmath> — so using it here builds on the
      // two Unix legs and breaks the Windows one, which is exactly how it got in.
      constexpr float kPiF = 3.14159265358979323846f;
      const float theta = gain * kPiF / 180.0f;
      float ax = -cy;
      float ay = cx;
      float az = 0.0f;
      const float axis_len = std::sqrt(ax * ax + ay * ay + az * az);
      if (!(axis_len > 1e-6f)) {
        ADD_FAILURE() << "lens " << lens << " fov " << fov << ": no perpendicular to rotate about";
        continue;
      }
      ax /= axis_len;
      ay /= axis_len;
      az /= axis_len;
      const float ct = std::cos(theta);
      const float st = std::sin(theta);
      const float dot = ax * cx + ay * cy + az * cz;
      const float offset_dir[3] = {
        cx * ct + (ay * cz - az * cy) * st + ax * dot * (1.0f - ct),
        cy * ct + (az * cx - ax * cz) * st + ay * dot * (1.0f - ct),
        cz * ct + (ax * cy - ay * cx) * st + az * dot * (1.0f - ct),
      };
      const std::array<float, 2> moved = ProjectWorldDirToScreen(vp, offset_dir, kVpW, kVpH);
      if (IsSentinel(moved)) {
        ADD_FAILURE() << "lens " << lens << " fov " << fov << ": one drag-pixel of rotation leaves the frame";
        continue;
      }

      const float pixels = std::sqrt(moved[0] * moved[0] + moved[1] * moved[1]);
      // The tolerance is wide on purpose: this asserts the invariant (a constant of order one
      // pixel), not the calibrated value, which the drag law's own header says may be retuned.
      EXPECT_GT(pixels, 0.5f) << "lens " << lens << " fov " << fov << ": dragging barely moves anything";
      EXPECT_LT(pixels, 20.0f) << "lens " << lens << " fov " << fov << ": one dragged pixel throws the view";
    }
  }
}

// The gain law's own edge cases — a degenerate viewport, and a full-sky lens that has no drag
// interaction and answers with the historical constant rather than with zero — are statements about
// ComputeDragGainDegPerPixel alone, and are asserted one layer down in
// test/unit-correctness/gui/test_preview_renderer.cpp (DragGain.*). What is above is the part that
// needs the projection as well: that the angular gain, fed back through the projection, actually
// moves the picture by a constant number of pixels.

// ---------------------------------------------------------------------------------------------
// E13 (roll half) — the lens types that ignore roll must see zero roll, at every consumer.
//
// BuildPreviewViewProjFromRenderer is where that normalisation happens once, so the overlay and the
// shader cannot disagree about whether the sky is tilted. A consumer that read rc.roll directly
// would draw its markers rotated against an image that is not.

TEST(PreviewProjectionChain, LensTypesThatIgnoreRollSeeZeroRollInTheProjection) {
  int zeroed = 0;
  for (int lens : { LUMICE_LENS_TYPE_LINEAR, LUMICE_LENS_TYPE_FISHEYE_EQUAL_AREA, LUMICE_LENS_TYPE_FISHEYE_EQUIDISTANT,
                    LUMICE_LENS_TYPE_DUAL_FISHEYE_EQUAL_AREA, LUMICE_LENS_TYPE_RECTANGULAR, LUMICE_LENS_TYPE_GLOBE }) {
    RenderConfig rc = LensAt(lens, 90.0f, 0.0f);
    rc.roll = 30.0f;
    const float effective = EffectiveRollForLens(lens, 30.0f);
    EXPECT_FLOAT_EQ(BuildPreviewViewProjFromRenderer(rc).roll, effective)
        << "lens " << lens << ": the projection's roll is not the one the lens rule produced";
    zeroed += (effective == 0.0f) ? 1 : 0;
  }
  // And the rule itself has to actually zero SOMETHING, or the normalisation is a no-op that every
  // consumer could have skipped and the loop above is vacuous.
  EXPECT_GT(zeroed, 0) << "no lens discards roll";
}

// The front clip's twin of the roll test above, across EVERY lens rather than a sample, and for both
// consumers that turn a RenderConfig into a view: the preview's ViewProjection (the shader's u_front)
// and the annotation anchors' request. Each must carry the effective clip — false under a lens the
// clip does not apply to, however the stored choice reads — and never the stored value itself.
TEST(PreviewProjectionChain, LensTypesThatIgnoreFrontSeeNoFrontClipInTheProjectionOrTheAnchors) {
  GuiState state;
  int zeroed = 0;
  int kept = 0;
  for (int lens = 0; lens < kLensTypeCount; ++lens) {
    RenderConfig rc = LensAt(lens, 90.0f, 0.0f);
    rc.front = true;
    const bool effective = EffectiveFrontForLens(lens, true);
    EXPECT_EQ(BuildPreviewViewProjFromRenderer(rc).front, effective)
        << "lens " << lens << ": the projection's front clip is not the one the lens rule produced";
    EXPECT_EQ(AnnotationViewInputFor(state, rc).front, effective)
        << "lens " << lens << ": the anchors' front clip is not the one the lens rule produced";
    zeroed += effective ? 0 : 1;
    kept += effective ? 1 : 0;
  }
  // Both directions have to occur, or the loop is vacuous: a rule that zeroes nothing is the bug
  // this pins, and one that zeroes everything would pass the loop while killing the clip outright.
  EXPECT_GT(zeroed, 0) << "no lens discards the front clip";
  EXPECT_GT(kept, 0) << "no lens keeps the front clip";
}

}  // namespace
}  // namespace lumice::gui
