// Where the preview puts a sky direction on screen, and how far the sky moves when the user drags.
//
// The two halves of this file are one loop: ProjectWorldDirToScreen decides where a direction
// lands, and the drag law decides how much the view has to turn per pixel of mouse travel to move
// that landing point by a fixed amount. The drag cases use the projection as their oracle, so the
// projection's own anchors are asserted here too rather than in a file that could drift from it.
//
// Pure CPU: no GL context and no ImGui, which is why this lives in gui_unit_test rather than
// gui_test.
//
// The contract under test is not "the closed form matches the closed form" (that would be a
// tautology): it is the end-to-end statement the drag interaction actually owes the user —
// feed the gain through the SAME azimuth/elevation update rule app_panels.cpp uses, then let
// the UNMODIFIED production projection (ProjectWorldDirToScreen / BuildViewMatrix) say where
// the reference sky point landed. If the hand-derived derivative in preview_renderer.cpp is
// off by a factor, a sign, or disagrees with the projection's img_radius / theta conventions,
// the round trip misses and no amount of "the formula looks right" hides it.
//
// A note on step size, which is the honest boundary of this design: the gain is a DERIVATIVE
// (exact at the frame center only, to first order). At the top of a lens's FOV range the
// angular resolution at the center is enormous — linear at fov=179° is ~54.7 deg per pixel —
// so a "20 px drag" there is a full revolution and no linearization survives it. The probe
// tests therefore normalize the step to a fixed small ANGLE; the realistic-drag test uses
// whole-pixel deltas and states its tolerance as the analytic second-order bound for the
// angle each case actually sweeps, so the budget follows the sensitivity instead of being a
// hand-tuned number that silently goes stale when the sensitivity changes.

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

#include "gui/gui_constants.hpp"
#include "gui/preview_downgrade.hpp"
#include "gui/preview_renderer.hpp"

namespace {

using lumice::gui::ComputeDragGainDegPerPixel;
using lumice::gui::ProjectWorldDirToScreen;
using lumice::gui::ViewProjection;

constexpr float kSentinelLimit = -9000.f;  // any helper output <= this is the sentinel
constexpr float kPi = 3.14159265358979323846f;

// Screen pixels the content must travel per dragged pixel — the owner-calibrated sensitivity
// (AC1's revised anchor). Restated here as a literal rather than imported from the production
// side on purpose: this value is a product decision, so the test's job is to hold it, not to
// agree with whatever the implementation currently says. Changing kDragSensitivity in
// preview_renderer.cpp without changing this turns every round trip below red, which is the
// intended signal. The FOV-independence of the ratio — the actual defect this task fixes — is
// asserted separately and does not depend on the value.
constexpr float kExpectedSensitivity = 2.5f;

bool IsSentinel(const std::array<float, 2>& p) {
  return p[0] <= kSentinelLimit && p[1] <= kSentinelLimit;
}

ViewProjection MakeVp(int lens_type, float fov, float elev, float az) {
  ViewProjection vp;
  vp.lens_type = lens_type;
  vp.fov = fov;
  vp.elevation = elev;
  vp.azimuth = az;
  vp.roll = 0.0f;
  vp.visible = lumice::gui::kVisibleFull;
  return vp;
}

// The world point that sits exactly at the frame center when azimuth = elevation = roll = 0.
//
// BuildViewMatrix documents the camera as looking at world (-1,0,0) in that pose, and that is
// the reference used by the five inside-out lenses. Globe is the exception: ProjectGlobe
// reinterprets the WorldToView output as a POSITION in eye space rather than a view direction,
// so (-1,0,0) becomes eye_dir.z = -1, which fails its eye_dir.z > 1/kGlobeCameraD visibility
// test and returns the sentinel; (+1,0,0) is the one that lands at the center. GlobeReference
// (below) is a standing check on that asymmetry, so the choice here is measured, not assumed.
const float* ReferenceWorldDir(int lens_type) {
  static const float kFront[3] = { -1.0f, 0.0f, 0.0f };
  static const float kBack[3] = { 1.0f, 0.0f, 0.0f };
  return lens_type == lumice::gui::kLensTypeGlobe ? kBack : kFront;
}

// Verbatim mirror of the update rule in app_panels.cpp's drag branch — including the globe
// sign flip, which exists so a right/down drag moves the sphere right/down like the inside-out
// lenses. Copied rather than re-derived: a test that invents its own sign convention would
// pass while the app drags backwards.
//
//   if (rc.lens_type == kLensTypeGlobe) { rc.azimuth += delta.x * g; rc.elevation -= delta.y * g; }
//   else                                { rc.azimuth -= delta.x * g; rc.elevation += delta.y * g; }
//
// The azimuth wrap and the elevation clamp are carried over too, so the mirror is the whole
// branch rather than the convenient half of it. Neither fires for the step sizes used here —
// every case stays well inside ±180° / ±89° — but leaving them out would make the helper a
// selective copy, and the next person to grow the step sizes would not find out.
ViewProjection ApplyDrag(int lens_type, float fov, float gain_deg_per_px, float delta_x, float delta_y) {
  float az = 0.0f;
  float el = 0.0f;
  if (lens_type == lumice::gui::kLensTypeGlobe) {
    az += delta_x * gain_deg_per_px;
    el -= delta_y * gain_deg_per_px;
  } else {
    az -= delta_x * gain_deg_per_px;
    el += delta_y * gain_deg_per_px;
  }
  if (az > 180.0f) {
    az -= 360.0f;
  } else if (az < -180.0f) {
    az += 360.0f;
  }
  float el_lim = (lens_type == lumice::gui::kLensTypeGlobe) ? 89.0f : 90.0f;
  el = std::max(-el_lim, std::min(el_lim, el));
  return MakeVp(lens_type, fov, el, az);
}

struct LensCase {
  int lens_type;
  const char* name;
  float max_fov;  // LUMICE_MaxFov for this lens, per src/config/render_config.cpp MaxFov()
  float min_fov;  // lowest FOV at which ProjectWorldDirToScreen is usable as an oracle — see below
};

// The six lens types that currently enable dragging, i.e. exactly !LensIsFullSky.
// AC2's "all lens types with drag enabled" is this list.
//
// min_fov is a property of the ORACLE, not of the gain. ProjectFisheye recovers the view angle
// with acos(-view_dir.z), and 1 - cos(theta) ~ theta^2/2 falls under the float ulp at 1.0
// (5.96e-8) once theta drops below ~3.5e-4 rad — cos returns exactly 1.0f and the angle is
// gone. Measured: at fisheye fov=1° a 10 px probe is theta = 2.18e-4 rad and the oracle reports
// theta = 0, i.e. 100% of the angle lost. ProjectLinear and ProjectGlobe divide instead of
// taking an acos, so they hold their precision all the way down and keep min_fov = 1.
// The fisheye band below 8° is instead covered by SmallFovLawsConvergeToEquidistant, which
// checks the four fisheye laws against an independent analytic limit rather than a projection.
const std::vector<LensCase>& DraggableLenses() {
  static const std::vector<LensCase> kCases = {
    { lumice::gui::kLensTypeLinear, "linear", 179.0f, 1.0f },
    { lumice::gui::kLensTypeFisheyeEqualArea, "fisheye_equal_area", 360.0f, 8.0f },
    { lumice::gui::kLensTypeFisheyeEquidist, "fisheye_equidistant", 360.0f, 8.0f },
    { lumice::gui::kLensTypeFisheyeStereographic, "fisheye_stereographic", 359.0f, 8.0f },
    { lumice::gui::kLensTypeFisheyeOrthographic, "fisheye_orthographic", 180.0f, 8.0f },
    { lumice::gui::kLensTypeGlobe, "globe", 90.0f, 1.0f },
  };
  return kCases;
}

struct Viewport {
  int w;
  int h;
  const char* name;
};

// Square, landscape, portrait, and a 2x-DPI landscape (AC3: the gain must be expressed in
// framebuffer pixels, so the 2x viewport is the same physical feel at twice the pixel delta).
const std::vector<Viewport>& Viewports() {
  static const std::vector<Viewport> kVps = {
    { 800, 800, "800x800" },
    { 1280, 720, "1280x720" },
    { 600, 900, "600x900" },
    { 2560, 1440, "2560x1440_hidpi" },
  };
  return kVps;
}

// The probe is sized by ANGLE, not by pixels, because both error sources that bound it are
// angular: the linearization error above (worst case tan(theta)/theta - 1 = theta^2/3, so
// 1.3e-4 at 0.02 rad) and the oracle's float floor below (see DraggableLenses). 0.02 rad sits
// between them with roughly an order of magnitude of headroom on each side.
//
// The pixel cap is the one place the frame gets a say: at narrow FOV, 0.02 rad is a large
// fraction of the whole field, so the probe is also held to a drag whose RESULTING content
// displacement is 40% of img_radius, keeping the projected point comfortably inside the
// viewport (and inside the fisheye imaging circle) rather than clipping to the sentinel.
// The displacement is kExpectedSensitivity times the drag, hence the division.
constexpr float kProbeAngleRad = 0.02f;
constexpr float kProbeFrameFraction = 0.4f;

// 2e-3 is ~15x the worst linearization term in play and ~25x the worst oracle float noise,
// while any real defect in a gain branch (a missing factor of 2, a sin/tan swap, a wrong
// img_radius) is a several-percent-to-several-hundred-percent effect. It separates the two.
constexpr float kRoundTripTol = 2e-3f;

float ProbePixels(float gain_deg_per_px, int vp_w, int vp_h) {
  float gain_rad_per_px = gain_deg_per_px * kPi / 180.0f;
  float img_radius = std::min(static_cast<float>(vp_w), static_cast<float>(vp_h)) * 0.5f;
  return std::min(kProbeFrameFraction * img_radius / kExpectedSensitivity, kProbeAngleRad / gain_rad_per_px);
}

std::string Label(const LensCase& lc, float fov, const Viewport& vp, float dx, float dy) {
  return std::string(lc.name) + " fov=" + std::to_string(fov) + " vp=" + vp.name + " delta=(" + std::to_string(dx) +
         "," + std::to_string(dy) + ")";
}

// One round trip: drag by (dx, dy) screen pixels, reproject the reference sky point, and
// report where it landed. Screen coords are the projection's own y-up pixel frame, so a
// mouse delta of +dy (ImGui y is DOWN-positive) must move the content to py = -dy.
std::array<float, 2> RoundTrip(const LensCase& lc, float fov, const Viewport& vp, float dx, float dy) {
  float gain = ComputeDragGainDegPerPixel(lc.lens_type, fov, vp.w, vp.h);
  ViewProjection dragged = ApplyDrag(lc.lens_type, fov, gain, dx, dy);
  return ProjectWorldDirToScreen(dragged, ReferenceWorldDir(lc.lens_type), vp.w, vp.h);
}

// FOV ladder for a lens: its oracle-usable floor, a narrow value, mid-range, and two rungs up
// against its own MaxFov. The near-max rungs are the ones that matter — that is where a
// uniform fov/height approximation would have been wrong by ~70x for linear and stereographic,
// which is the whole reason this task took the per-lens closed form.
std::vector<float> FovLadder(const LensCase& lc) {
  std::vector<float> ladder = { lc.min_fov, 15.0f, lc.max_fov * 0.5f, lc.max_fov * 0.9f, lc.max_fov - 0.5f };
  ladder.erase(
      std::remove_if(ladder.begin(), ladder.end(), [&lc](float f) { return f < lc.min_fov || f > lc.max_fov; }),
      ladder.end());
  return ladder;
}

}  // namespace

// ---------------------------------------------------------------------------------------
// Precondition for every round trip below: the reference world point really is the frame
// center at the identity pose, and globe really is the one that needs (+1,0,0). If this
// flips, the round-trip assertions are measuring the wrong thing.
// ---------------------------------------------------------------------------------------
TEST(DragGain, ReferencePointIsFrameCenterAndGlobesIsMirrored) {
  for (const auto& lc : DraggableLenses()) {
    auto vp = MakeVp(lc.lens_type, 60.0f, 0.0f, 0.0f);
    auto p = ProjectWorldDirToScreen(vp, ReferenceWorldDir(lc.lens_type), 800, 600);
    if (IsSentinel(p)) {
      ADD_FAILURE() << lc.name << ": the reference direction projected to the sentinel";
      continue;  // no pixel to compare for this lens; the rest still get checked
    }
    EXPECT_NEAR(p[0], 0.0f, 1e-3f) << lc.name;
    EXPECT_NEAR(p[1], 0.0f, 1e-3f) << lc.name;
  }

  // ...and the asymmetry that choice rests on, both ways round: globe cannot show the inside-out
  // lenses' front direction (eye_dir.z = -1), and linear cannot show globe's.
  const float front[3] = { -1.0f, 0.0f, 0.0f };
  const float back[3] = { 1.0f, 0.0f, 0.0f };
  auto globe = MakeVp(lumice::gui::kLensTypeGlobe, 60.0f, 0.0f, 0.0f);
  EXPECT_TRUE(IsSentinel(ProjectWorldDirToScreen(globe, front, 800, 600)));
  EXPECT_FALSE(IsSentinel(ProjectWorldDirToScreen(globe, back, 800, 600)));
  auto linear = MakeVp(lumice::gui::kLensTypeLinear, 60.0f, 0.0f, 0.0f);
  EXPECT_FALSE(IsSentinel(ProjectWorldDirToScreen(linear, front, 800, 600)));
  EXPECT_TRUE(IsSentinel(ProjectWorldDirToScreen(linear, back, 800, 600)));
}

// ---------------------------------------------------------------------------------------
// AC1 / AC2 / AC3 — the main event. For every draggable lens, across its own full FOV range,
// on square / landscape / portrait / HiDPI viewports, in both directions on both axes: one
// pixel of drag moves the content kExpectedSensitivity pixels. Constant across the FOV range
// is the whole point — the same assertion with the same tolerance holds at fov=1° and at
// fov=MaxFov.
// ---------------------------------------------------------------------------------------
TEST(DragGain, DraggingMovesContentAtAFixedPixelRatioOnBothAxes) {
  for (const auto& lc : DraggableLenses()) {
    for (float fov : FovLadder(lc)) {
      for (const auto& vp : Viewports()) {
        float gain = ComputeDragGainDegPerPixel(lc.lens_type, fov, vp.w, vp.h);
        if (!(gain > 0.0f)) {
          ADD_FAILURE() << Label(lc, fov, vp, 0, 0) << ": non-positive drag gain";
          continue;  // no gain to probe with for this combination; the rest still get checked
        }
        const float step = ProbePixels(gain, vp.w, vp.h);
        for (const bool horizontal : { true, false }) {
          for (float d : { step, -step }) {
            const float dx = horizontal ? d : 0.0f;
            const float dy = horizontal ? 0.0f : d;
            auto p = RoundTrip(lc, fov, vp, dx, dy);
            if (IsSentinel(p)) {
              ADD_FAILURE() << Label(lc, fov, vp, dx, dy) << ": round trip landed on the sentinel";
              continue;
            }
            // Content follows the cursor: +dx of mouse motion => +k*dx of screen-x motion.
            // ImGui's mouse y is DOWN-positive and the projection's py is UP-positive, so the
            // vertical arm is py = -k*dy. That arm is also where a globe sign regression would
            // surface: its elevation update is -= where the others are +=.
            //
            // Written as a ratio against the expectation so kRoundTripTol stays a RELATIVE
            // budget; comparing p/d against k directly would silently hand it k times the slack.
            const float along = horizontal ? p[0] : p[1];
            const float across = horizontal ? p[1] : p[0];
            EXPECT_NEAR(along / (d * kExpectedSensitivity), horizontal ? 1.0f : -1.0f, kRoundTripTol)
                << Label(lc, fov, vp, dx, dy);
            EXPECT_NEAR(across, 0.0f, std::abs(d) * kExpectedSensitivity * kRoundTripTol) << Label(lc, fov, vp, dx, dy);
          }
        }
      }
    }
  }
}

// ---------------------------------------------------------------------------------------
// The same contract stated in whole mouse pixels, for the FOV band where a real drag is
// small enough that the first-order gain is a good description of a 20 px motion. This is
// the reading of AC1 closest to the user's hand; the probe tests above are what cover the
// extreme ends of the FOV range, where no whole-pixel step can be linear.
// ---------------------------------------------------------------------------------------
TEST(DragGain, RealisticDragStepsAtModerateFov) {
  struct Combo {
    int lens_type;
    const char* name;
    float fov;
  };
  const Combo kCombos[] = {
    { lumice::gui::kLensTypeLinear, "linear", 30.0f },
    { lumice::gui::kLensTypeLinear, "linear", 90.0f },
    { lumice::gui::kLensTypeLinear, "linear", 150.0f },
    { lumice::gui::kLensTypeFisheyeEqualArea, "fisheye_equal_area", 120.0f },
    { lumice::gui::kLensTypeFisheyeEqualArea, "fisheye_equal_area", 360.0f },
    { lumice::gui::kLensTypeFisheyeEquidist, "fisheye_equidistant", 180.0f },
    { lumice::gui::kLensTypeFisheyeEquidist, "fisheye_equidistant", 360.0f },
    { lumice::gui::kLensTypeFisheyeStereographic, "fisheye_stereographic", 120.0f },
    { lumice::gui::kLensTypeFisheyeOrthographic, "fisheye_orthographic", 180.0f },
    { lumice::gui::kLensTypeGlobe, "globe", 30.0f },
    { lumice::gui::kLensTypeGlobe, "globe", 90.0f },
  };
  const Viewport vp = { 1280, 720, "1280x720" };

  // The tolerance is DERIVED per case, not tuned: the gain is a first derivative, so the
  // round trip is off by each law's second-order term in the swept angle theta. Over the six
  // laws those terms are theta^2/3 (linear, globe), theta^2/12 (stereographic), theta^2/6
  // (orthographic), theta^2/24 (equal-area) and exactly 0 (equidistant), so theta^2/3 bounds
  // them all; 1.5x covers the theta^4 remainder, and a 0.5% floor keeps the near-zero cases
  // from being pinned tighter than float noise.
  //
  // A single hand-tuned constant was what this test used to have, and raising the sensitivity
  // to k=2.5 multiplied every swept angle by 2.5 and quadrupled these terms — linear at
  // fov=150° went from 1.5% to 10.3%. A fixed budget would have had to be widened by hand,
  // which reads identically to widening it to hide a defect. Deriving it removes the choice.
  auto tolerance_for = [](float gain_deg_per_px, float pixels) {
    float theta = std::abs(pixels) * gain_deg_per_px * kPi / 180.0f;
    return std::max(0.005f, 1.5f * theta * theta / 3.0f);
  };

  for (const auto& c : kCombos) {
    LensCase lc = { c.lens_type, c.name, 0.0f };
    float gain = ComputeDragGainDegPerPixel(c.lens_type, c.fov, vp.w, vp.h);
    if (!(gain > 0.0f)) {
      ADD_FAILURE() << c.name << ": non-positive drag gain";
      continue;  // no gain to probe with for this combo; the rest still get checked
    }
    for (float d : { 5.0f, 20.0f, -5.0f, -20.0f }) {
      float tol = tolerance_for(gain, d);
      // The budget must stay well under the effects it has to separate: the smallest defect
      // this suite is built to catch is a missing factor (>=2x, i.e. 100%).
      if (!(tol < 0.2f)) {
        ADD_FAILURE() << Label(lc, c.fov, vp, d, 0.0f) << ": derived tolerance " << tol << " exceeds the budget";
        continue;  // the tolerance itself is unusable for this step; the rest still get checked
      }

      auto px = RoundTrip(lc, c.fov, vp, d, 0.0f);
      if (IsSentinel(px)) {
        ADD_FAILURE() << Label(lc, c.fov, vp, d, 0.0f) << ": horizontal round trip landed on the sentinel";
        continue;
      }
      EXPECT_NEAR(px[0] / (d * kExpectedSensitivity), 1.0f, tol) << Label(lc, c.fov, vp, d, 0.0f);

      auto py = RoundTrip(lc, c.fov, vp, 0.0f, d);
      if (IsSentinel(py)) {
        ADD_FAILURE() << Label(lc, c.fov, vp, 0.0f, d) << ": vertical round trip landed on the sentinel";
        continue;
      }
      EXPECT_NEAR(py[1] / (d * kExpectedSensitivity), -1.0f, tol) << Label(lc, c.fov, vp, 0.0f, d);
    }
  }
}

// ---------------------------------------------------------------------------------------
// AC3, stated structurally rather than through the round trip: the gain is inversely
// proportional to framebuffer pixels. Doubling the DPI doubles the pixel delta of the same
// physical hand motion and halves the gain, so the rotation — the feel — is unchanged.
// ---------------------------------------------------------------------------------------
TEST(DragGain, TheGainIsGovernedByTheShorterSideAndScalesInverselyWithIt) {
  for (const auto& lc : DraggableLenses()) {
    for (float fov : FovLadder(lc)) {
      float g1 = ComputeDragGainDegPerPixel(lc.lens_type, fov, 1280, 720);
      float g2 = ComputeDragGainDegPerPixel(lc.lens_type, fov, 2560, 1440);
      if (!(g1 > 0.0f)) {
        ADD_FAILURE() << lc.name << " fov=" << fov << ": non-positive drag gain";
        continue;  // no gain to form a ratio with for this fov; the rest still get checked
      }
      EXPECT_NEAR(g2 * 2.0f / g1, 1.0f, 1e-5f) << lc.name << " fov=" << fov;
    }
    // img_radius is min(w, h)/2 — the same definition ProjectWorldDirToScreen uses — so a viewport
    // that is wide but the same height gives the same gain, and the short side is what governs.
    // Pinning this keeps the two from drifting apart into a non-square mismatch.
    float square = ComputeDragGainDegPerPixel(lc.lens_type, 60.0f, 720, 720);
    EXPECT_FLOAT_EQ(ComputeDragGainDegPerPixel(lc.lens_type, 60.0f, 2000, 720), square) << lc.name;
    EXPECT_FLOAT_EQ(ComputeDragGainDegPerPixel(lc.lens_type, 60.0f, 720, 2000), square) << lc.name;
  }
}

// ---------------------------------------------------------------------------------------
// Closed-form anchors. These separate "the derivative is wrong" from "the projection is
// consuming it wrongly" when a round trip above goes red: pick combinations whose value is
// exact by hand, so a failure here indicts the formula alone.
// ---------------------------------------------------------------------------------------
TEST(DragGain, ClosedFormAnchors) {
  constexpr float kRad2Deg = 180.0f / kPi;
  // Every anchor is the 1:1 closed form times the sensitivity, so a k that stopped being
  // applied — or got applied to the legacy fallback too — shows up here as well.
  constexpr float kK = kExpectedSensitivity;

  // linear, fov=90 => half_fov=45°, tan(45°)=1 => 1/img_radius rad/px. img_radius = 400.
  EXPECT_NEAR(ComputeDragGainDegPerPixel(lumice::gui::kLensTypeLinear, 90.0f, 800, 800), kK * kRad2Deg / 400.0f, 1e-5f);

  // fisheye equidistant, fov=180 => half_fov=pi/2 => (pi/2)/img_radius rad/px, i.e. the
  // whole 90° half-field spans exactly img_radius pixels. img_radius = 300.
  EXPECT_NEAR(ComputeDragGainDegPerPixel(lumice::gui::kLensTypeFisheyeEquidist, 180.0f, 800, 600), kK * 90.0f / 300.0f,
              1e-5f);

  // fisheye orthographic, fov=180 => sin(90°)=1 => 1/img_radius rad/px. img_radius = 300.
  EXPECT_NEAR(ComputeDragGainDegPerPixel(lumice::gui::kLensTypeFisheyeOrthographic, 180.0f, 800, 600),
              kK * kRad2Deg / 300.0f, 1e-5f);

  // fisheye equal area, fov=180 => 2*sin(45°)=sqrt(2) => sqrt(2)/img_radius rad/px.
  EXPECT_NEAR(ComputeDragGainDegPerPixel(lumice::gui::kLensTypeFisheyeEqualArea, 180.0f, 800, 600),
              kK * std::sqrt(2.0f) * kRad2Deg / 300.0f, 1e-5f);

  // fisheye stereographic, fov=180 => 2*tan(45°)=2 => 2/img_radius rad/px.
  EXPECT_NEAR(ComputeDragGainDegPerPixel(lumice::gui::kLensTypeFisheyeStereographic, 180.0f, 800, 600),
              kK * 2.0f * kRad2Deg / 300.0f, 1e-5f);

  // globe, fov=90 => (D-1)*tan(45°) = 3 => 3/img_radius rad/px. img_radius = 300.
  EXPECT_NEAR(ComputeDragGainDegPerPixel(lumice::gui::kLensTypeGlobe, 90.0f, 800, 600), kK * 3.0f * kRad2Deg / 300.0f,
              1e-4f);
}

// ---------------------------------------------------------------------------------------
// Narrow-FOV coverage for the fisheye family, which the round trip above cannot reach (the
// oracle's acos loses the angle — see DraggableLenses).
//
// The check is against an independent physical fact rather than against the implementation:
// every projection is locally equidistant near its optical axis, so as fov -> 0 all five
// inside-out laws must converge to the SAME angular resolution, half_fov / img_radius. This
// is not a restatement of any branch — it is the property that pins each branch's constant
// factor. Drop the 2 from the equal-area law, or write sin where tan belongs, and the ratio
// leaves 1 immediately, at exactly the FOV band nothing else covers.
//
// Globe is the meaningful exception and is asserted at its own factor: its camera sits at
// D = kGlobeCameraD sphere radii away, so a given rotation of the sphere sweeps (D-1) = 3x
// the screen distance a pinhole at the same fov would give. Pinning 3 here is what would
// catch the camera distance and the gain law drifting apart.
// ---------------------------------------------------------------------------------------
TEST(DragGain, SmallFovLawsConvergeToEquidistant) {
  constexpr int kW = 800;
  constexpr int kH = 600;  // img_radius = 300
  for (float fov : { 1.0f, 2.0f, 5.0f }) {
    float reference = ComputeDragGainDegPerPixel(lumice::gui::kLensTypeFisheyeEquidist, fov, kW, kH);
    if (!(reference > 0.0f)) {
      ADD_FAILURE() << "fov=" << fov << ": non-positive reference drag gain";
      continue;  // no reference to form a ratio with for this fov; the rest still get checked
    }
    // The equidistant law IS half_fov / img_radius, exactly and at every FOV — it is the
    // limit itself, so it is the natural yardstick for the other five. (Scaled by the
    // sensitivity, like every branch; the five ratios below are unaffected by that scale,
    // which is why this one line is where k enters this test.)
    EXPECT_NEAR(reference, kExpectedSensitivity * (fov * 0.5f) / 300.0f, 1e-6f) << "fov=" << fov;

    // Every law's departure from the shared limit is second order in half_fov, with 1/3 the
    // largest coefficient among the six (tan(hf)/hf - 1 = hf^2/3, hit by both linear and
    // globe), so hf^2 bounds them all with 3x to spare. Compared as a RELATIVE deviation:
    // globe's expected ratio is 3, and an absolute tolerance would silently give it three
    // times the budget the others get — which is exactly how the first version of this
    // assertion went red at fov=5° while claiming a 1e-3 bound.
    float half_fov_rad = fov * 0.5f * kPi / 180.0f;
    float tol = std::max(1e-4f, half_fov_rad * half_fov_rad);
    for (const auto& lc : DraggableLenses()) {
      float expected_ratio = (lc.lens_type == lumice::gui::kLensTypeGlobe) ? 3.0f : 1.0f;
      float g = ComputeDragGainDegPerPixel(lc.lens_type, fov, kW, kH);
      EXPECT_NEAR(g / reference / expected_ratio, 1.0f, tol) << lc.name << " fov=" << fov;
    }
  }
}

// The 0.3 deg/px the drag used before this became FOV-aware, quantified on this 800x600
// viewport: at fov=1° the constant was ~72x too fast (0.3 vs 0.0042 deg/px) and at fov=179°
// ~180x too slow (0.3 vs 54.7). The sensitivity calibration deliberately keeps the two laws
// close in the mid band — 2.3x apart at the 30° default here — so the SPREAD is what this
// pins, and it is the defect AC1 names: a regression to any single constant fails one end.
TEST(DragGain, TheEdgesOfTheGainLawSpanTheRangeDegradeSafelyAndKeepTheLegacyFallback) {
  float narrow = ComputeDragGainDegPerPixel(lumice::gui::kLensTypeLinear, 1.0f, 800, 600);
  float wide = ComputeDragGainDegPerPixel(lumice::gui::kLensTypeLinear, 179.0f, 800, 600);
  EXPECT_LT(narrow, 0.3f / 50.0f);
  EXPECT_GT(wide, 0.3f * 100.0f);

  // Degenerate viewports happen during window setup and on a collapsed panel: answering with a
  // sensible zero rather than dividing by it is what keeps the first frame after a resize from
  // teleporting the view.
  EXPECT_FLOAT_EQ(ComputeDragGainDegPerPixel(lumice::gui::kLensTypeLinear, 60.0f, 0, 600), 0.0f);
  EXPECT_FLOAT_EQ(ComputeDragGainDegPerPixel(lumice::gui::kLensTypeLinear, 60.0f, 800, 0), 0.0f);
  EXPECT_FLOAT_EQ(ComputeDragGainDegPerPixel(lumice::gui::kLensTypeLinear, 60.0f, -1, -1), 0.0f);

  // Full-sky lenses never reach the drag branch (app_panels.cpp guards on !LensIsFullSky), so this
  // is the defensive fallback, not a live path: it must stay finite and match the legacy constant
  // rather than return 0 or NaN.
  //
  // Exactly 0.3, NOT 0.3 * kExpectedSensitivity: the fallback IS the historical feel value, and the
  // sensitivity constant exists to bring the closed forms up to it, not to be stacked on top of it.
  // Applying k to this branch too would be a silent 2.5x on the one path that is supposed to change
  // nothing, so the exact compare here is the guard against that.
  for (int lt : lumice::gui::kFullSkyLensTypes) {
    EXPECT_FLOAT_EQ(ComputeDragGainDegPerPixel(lt, 180.0f, 800, 600), 0.3f) << "lens_type " << lt;
  }
}

// ---------------------------------------------------------------------------------------
// ProjectWorldDirToScreen, the forward projection the drag round trips above use as their
// oracle — and which the overlay's label placement uses for real. Every row below is an
// anchor whose pixel is derivable in closed form from the shader's own projection law, so a
// changed formula turns red rather than agreeing with itself.
//
// The world frame at elevation = azimuth = roll = 0 puts the camera's forward at (-1, 0, 0),
// the zenith at (0, 0, -1) and the nadir at (0, 0, +1) — z-down, matching overlay_labels.cpp.
// Screen coordinates are the projection's own y-up pixel frame, centred on the viewport.
//
// The sentinel rows are as load-bearing as the positional ones: a direction the lens cannot
// show must come back as the sentinel and not as a plausible pixel, or the overlay draws a
// label for a point that is not on screen.
// ---------------------------------------------------------------------------------------
TEST(PreviewRenderer, EveryProjectionPutsItsAnchorDirectionsWhereItsGeometryDemands) {
  constexpr float kPxEps = 1.0f;  // +-1 px against the analytic expectation

  struct ProjectionCase {
    const char* name;
    int lens_type;
    float fov;
    float elevation;
    int vp_w;
    int vp_h;
    std::array<float, 3> world_dir;
    bool expect_sentinel;
    float px;  // ignored when expect_sentinel
    float py;
  };
  // Directions reused across rows.
  constexpr std::array<float, 3> kFront = { -1.0f, 0.0f, 0.0f };
  constexpr std::array<float, 3> kZenith = { 0.0f, 0.0f, -1.0f };
  constexpr std::array<float, 3> kNadir = { 0.0f, 0.0f, 1.0f };

  const ProjectionCase kCases[] = {
    // Linear pinhole: forward lands dead centre; the zenith sits exactly on the camera plane
    // (view_dir.z == 0), which is the perspective singularity rather than a wide angle.
    { "linear, forward", lumice::gui::kLensTypeLinear, 90.0f, 0.0f, 800, 600, kFront, false, 0.0f, 0.0f },
    { "linear, the zenith is on the camera plane", lumice::gui::kLensTypeLinear, 90.0f, 0.0f, 800, 600, kZenith, true,
      0.0f, 0.0f },

    // Equal-area fisheye looking straight up: the zenith is now the optical axis, and the
    // horizon lands on the imaging circle. img_radius = min(800, 800) / 2 = 400, and the
    // world forward maps to view (0, -1, 0), i.e. theta = pi/2 at phi = -pi/2 — the bottom of
    // the image, where r_norm = 1 makes r exactly img_radius.
    { "fisheye equal-area at the zenith, looking up", lumice::gui::kLensTypeFisheyeEqualArea, 180.0f, 90.0f, 800, 800,
      kZenith, false, 0.0f, 0.0f },
    { "fisheye equal-area, the horizon on the circle", lumice::gui::kLensTypeFisheyeEqualArea, 180.0f, 90.0f, 800, 800,
      kFront, false, 0.0f, -400.0f },

    // Equirectangular: latitude maps linearly to py. 800x400 gives scale = 400/pi, so the
    // poles land at +-(pi/2)(400/pi) = +-200 and the equator at 0.
    { "rectangular, the zenith", lumice::gui::kLensTypeRectangular, 180.0f, 0.0f, 800, 400, kZenith, false, 0.0f,
      -200.0f },
    { "rectangular, the nadir", lumice::gui::kLensTypeRectangular, 180.0f, 0.0f, 800, 400, kNadir, false, 0.0f,
      200.0f },
    { "rectangular, the equator ahead", lumice::gui::kLensTypeRectangular, 180.0f, 0.0f, 800, 400, kFront, false, 0.0f,
      0.0f },

    // Dual fisheye: one disc per hemisphere, so the two poles are the two disc centres.
    // 800x400 gives circle_radius = 200, hence centres at -+200.
    { "dual fisheye, the zenith is the left disc centre", lumice::gui::kLensTypeDualFisheyeEqualArea, 180.0f, 0.0f, 800,
      400, kZenith, false, -200.0f, 0.0f },
    { "dual fisheye, the nadir is the right disc centre", lumice::gui::kLensTypeDualFisheyeEqualArea, 180.0f, 0.0f, 800,
      400, kNadir, false, 200.0f, 0.0f },

    // Globe, viewed from outside at distance kGlobeCameraD. Tilting the camera to look upward
    // brings the zenith to the near pole, dead centre; the nadir is then around the back of the
    // sphere and must be culled rather than drawn over the front of it.
    { "globe, the near pole", lumice::gui::kLensTypeGlobe, 30.0f, -90.0f, 800, 800, kZenith, false, 0.0f, 0.0f },
    { "globe, the far pole is behind the sphere", lumice::gui::kLensTypeGlobe, 30.0f, -90.0f, 800, 800, kNadir, true,
      0.0f, 0.0f },
  };

  for (const ProjectionCase& c : kCases) {
    auto vp = MakeVp(c.lens_type, c.fov, c.elevation, 0.0f);
    auto p = ProjectWorldDirToScreen(vp, c.world_dir.data(), c.vp_w, c.vp_h);
    EXPECT_EQ(IsSentinel(p), c.expect_sentinel) << c.name;
    if (c.expect_sentinel) {
      continue;
    }
    EXPECT_NEAR(p[0], c.px, kPxEps) << c.name;
    EXPECT_NEAR(p[1], c.py, kPxEps) << c.name;
  }
}

// A fisheye narrower than 180 degrees in a non-square viewport leaves black bars: pixels that
// are inside the rectangle but outside the imaging circle. A direction that lands there is not
// visible, and the projection has to say so rather than return a pixel in the bar.
//
// At elevation = azimuth = roll = 0 the view matrix maps world w to view (-w.y, -w.z, w.x), so
// a direction 60 degrees off the optical axis is w = (-1/2, -sqrt(3)/2, 0). At fov=90 its
// r_norm is (pi/3)/(pi/4) = 4/3, outside the circle; at fov=180 it is (pi/3)/(pi/2) = 2/3,
// comfortably inside. Same direction, same viewport: only the imaging circle moved.
TEST(PreviewRenderer, ADirectionOutsideTheImagingCircleIsNotGivenAPixelInTheBlackBars) {
  const float dir_60deg[3] = { -0.5f, -0.86602540378f, 0.0f };

  auto narrow = MakeVp(lumice::gui::kLensTypeFisheyeEquidist, 90.0f, 0.0f, 0.0f);
  EXPECT_TRUE(IsSentinel(ProjectWorldDirToScreen(narrow, dir_60deg, 1920, 1080)));

  auto wide = MakeVp(lumice::gui::kLensTypeFisheyeEquidist, 180.0f, 0.0f, 0.0f);
  EXPECT_FALSE(IsSentinel(ProjectWorldDirToScreen(wide, dir_60deg, 1920, 1080)));
}

// ---------------------------------------------------------------------------------------
// The background overlay's UV transform, and the modifier key that arms its canvas gestures.
//
// Both are pure and live in preview_renderer.hpp for the same reason the drag gain does: a GL
// context is not needed to state what they owe, so they are asserted here rather than in a
// suite that must open a window.
// ---------------------------------------------------------------------------------------

// The identity case is the whole backward-compatibility argument, so it is asserted bit for bit
// rather than within a tolerance. An .lmc written before bg_offset_x/bg_offset_y/bg_scale existed
// deserializes to (0, 0, 1); if that does not reproduce the historical hard-coded centered fit
// EXACTLY, every such document renders slightly differently after this change and there is no
// compat branch anywhere to catch it.
TEST(BgTransform, TheIdentityTransformReproducesTheHistoricalCenteredFitBitForBit) {
  struct Row {
    int vp_w;
    int vp_h;
    float bg_aspect;
  };
  // Square, both letterbox directions, and a wide viewport against a wide image.
  const Row kRows[] = {
    { 800, 800, 1.0f },
    { 1920, 1080, 1.0f },
    { 800, 800, 16.0f / 9.0f },
    { 1920, 1080, 4.0f / 3.0f },
  };

  for (const Row& row : kRows) {
    // The expression the production code carried inline before ComputeBgUvTransform existed.
    const float vp_aspect = static_cast<float>(row.vp_w) / static_cast<float>(row.vp_h);
    float sx = 1.0f;
    float sy = 1.0f;
    if (vp_aspect > row.bg_aspect) {
      sx = row.bg_aspect / vp_aspect;
    } else {
      sy = vp_aspect / row.bg_aspect;
    }

    const auto t = lumice::gui::ComputeBgUvTransform(row.vp_w, row.vp_h, row.bg_aspect, 0.0f, 0.0f, 1.0f);
    EXPECT_FLOAT_EQ(t.scale_x, 0.5f / sx) << "vp " << row.vp_w << "x" << row.vp_h;
    EXPECT_FLOAT_EQ(t.scale_y, -0.5f / sy) << "vp " << row.vp_w << "x" << row.vp_h;
    EXPECT_FLOAT_EQ(t.offset_x, 0.5f) << "vp " << row.vp_w << "x" << row.vp_h;
    EXPECT_FLOAT_EQ(t.offset_y, 0.5f) << "vp " << row.vp_w << "x" << row.vp_h;
  }
}

// Zoom and pan are independent knobs: zoom moves only the scale, pan moves only the offset. If
// zoom leaked into the offset the image would slide sideways while being scaled (the classic
// "zoom is not about the center" bug); if pan leaked into the scale the photo would breathe while
// being dragged. Both are the kind of coupling that looks fine in a still screenshot.
TEST(BgTransform, ZoomScalesOnlyTheScaleAndPanShiftsOnlyTheOffset) {
  const auto base = lumice::gui::ComputeBgUvTransform(1920, 1080, 4.0f / 3.0f, 0.0f, 0.0f, 1.0f);

  for (float zoom : { 0.25f, 0.5f, 2.0f, 4.0f }) {
    const auto z = lumice::gui::ComputeBgUvTransform(1920, 1080, 4.0f / 3.0f, 0.0f, 0.0f, zoom);
    EXPECT_FLOAT_EQ(z.scale_x, base.scale_x / zoom) << "zoom " << zoom;
    EXPECT_FLOAT_EQ(z.scale_y, base.scale_y / zoom) << "zoom " << zoom;
    EXPECT_FLOAT_EQ(z.offset_x, base.offset_x) << "zoom " << zoom;
    EXPECT_FLOAT_EQ(z.offset_y, base.offset_y) << "zoom " << zoom;
  }

  for (float pan : { -0.75f, -0.1f, 0.1f, 0.75f }) {
    const auto p = lumice::gui::ComputeBgUvTransform(1920, 1080, 4.0f / 3.0f, pan, -pan, 1.0f);
    EXPECT_FLOAT_EQ(p.scale_x, base.scale_x) << "pan " << pan;
    EXPECT_FLOAT_EQ(p.scale_y, base.scale_y) << "pan " << pan;
    EXPECT_FLOAT_EQ(p.offset_x, base.offset_x + pan) << "pan " << pan;
    EXPECT_FLOAT_EQ(p.offset_y, base.offset_y - pan) << "pan " << pan;
  }
}

// A unit of pan is a full texture width whatever the zoom, so the two knobs stay usable together:
// the slider's ±2 range means the same thing zoomed in as zoomed out. Stated as the round trip
// that actually matters — the NDC point that samples a given texel — because that is the property
// the canvas drag inverts to turn a mouse delta into a pan delta.
TEST(BgTransform, PanIsMeasuredInTextureWidthsIndependentlyOfZoom) {
  for (float zoom : { 0.5f, 1.0f, 3.0f }) {
    const auto t = lumice::gui::ComputeBgUvTransform(1024, 1024, 1.0f, 0.0f, 0.0f, zoom);
    const auto panned = lumice::gui::ComputeBgUvTransform(1024, 1024, 1.0f, 0.25f, 0.0f, zoom);

    // The NDC coordinate that samples u = 0.5 shifts by exactly pan / scale_x, i.e. a quarter of
    // the texture width expressed in the CURRENT zoom's NDC units.
    const float ndc_before = (0.5f - t.offset_x) / t.scale_x;
    const float ndc_after = (0.5f - panned.offset_x) / panned.scale_x;
    EXPECT_FLOAT_EQ(ndc_after - ndc_before, -0.25f / t.scale_x) << "zoom " << zoom;
  }
}

// The letterbox branch is picked by the aspect comparison alone; pan and zoom must not be able to
// flip which side gets the bars. A viewport wider than the image squeezes x (sx < 1) and leaves y
// alone, and the other way round — the same split the pixel-level letterbox case in
// test_background_overlay.cpp checks end to end.
TEST(BgTransform, TheLetterboxBranchIsChosenByAspectAloneAndSurvivesPanAndZoom) {
  // Viewport 2:1 against a 1:1 image — bars left and right, so x is the squeezed axis.
  for (float zoom : { 0.5f, 1.0f, 2.0f }) {
    const auto t = lumice::gui::ComputeBgUvTransform(1600, 800, 1.0f, 0.3f, -0.2f, zoom);
    // sx = 1/2, sy = 1  =>  |scale_x| = 2 * |scale_y|
    EXPECT_FLOAT_EQ(t.scale_x, 1.0f / zoom);
    EXPECT_FLOAT_EQ(t.scale_y, -0.5f / zoom);
  }

  // Viewport 1:1 against a 2:1 image — bars top and bottom, so y is the squeezed axis.
  for (float zoom : { 0.5f, 1.0f, 2.0f }) {
    const auto t = lumice::gui::ComputeBgUvTransform(800, 800, 2.0f, 0.3f, -0.2f, zoom);
    EXPECT_FLOAT_EQ(t.scale_x, 0.5f / zoom);
    EXPECT_FLOAT_EQ(t.scale_y, -1.0f / zoom);
  }
}

// The hint's wording, which is all that is left of the modifier's platform split.
//
// An earlier revision arbitrated the KEY per platform (Cmd on macOS, Alt elsewhere) and pinned
// both branches here. That is gone: ImGui rewrites Super+LeftClick into a right click under
// ConfigMacOSXBehaviors, so the Cmd branch could not have worked on the platform it was written
// for — a fact no assertion about a pure arbitration function could have surfaced, since the
// function was right and its premise was wrong. The gesture now reads io.KeyAlt everywhere and
// there is no behavioural branch left to pin; what remains is a label, checked here so the two
// spellings cannot silently drift into naming a key the handler does not read.
TEST(BgTransform, TheModifierHintNamesTheKeyThisPlatformEngraves) {
#if defined(__APPLE__)
  EXPECT_STREQ(lumice::gui::kBgModifierName, "Option");
#else
  EXPECT_STREQ(lumice::gui::kBgModifierName, "Alt");
#endif
}

// ---- The recovery ladder's pure pixel math (preview_downgrade.hpp) ----
//
// The prefiltered downgrade candidates the 814 ladder offers when neither the staging buffers nor
// the full-size texture fit. Two laws are under witness, because they are the two the plan's
// exposure contracts lean on and the two a clever-looking implementation silently breaks. Law 1 —
// average, not sum, in the linear source domain — is what keeps a constant-radiance frame
// gain-free through a downgrade (the relative mode shows the same brightness) and keeps every
// texel's per-original-texel unit (the absolute mode, which divides by the ORIGINAL pixel count,
// stays correct). Law 2 — every candidate derived from the original frame by step count — is what
// keeps a retry from compounding blur: the ladder re-derives from the retained source, it never
// downgrades a downgrade. Headless by construction: the header carries no GL, no GuiState and no
// ImGui.

namespace preview_downgrade_test {

std::vector<float> ConstantPlane(int w, int h, float v) {
  return std::vector<float>(static_cast<size_t>(w) * h * 3, v);
}

// A coordinate pattern whose every texel differs, so a wrong stencil (a skip instead of an
// average, a transposed read) cannot land on the right answer by accident.
std::vector<float> CoordinatePlane(int w, int h, int seed) {
  std::vector<float> tex(static_cast<size_t>(w) * h * 3);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const size_t idx = (static_cast<size_t>(y) * w + x) * 3;
      for (int c = 0; c < 3; ++c) {
        tex[idx + c] = static_cast<float>(((x * 7 + y * 5 + c * 13 + seed) % 61) - 30) / 30.0f;
      }
    }
  }
  return tex;
}

}  // namespace preview_downgrade_test

// Law 1, relative-mode form: a constant radiance downgrades to the same constant — no gain, no
// bias. A summing implementation fails this by exactly 4x; a clamping one fails on bright values.
TEST(PreviewDowngrade, ConstantRadianceDowngradesGainFree) {
  using namespace preview_downgrade_test;
  // One step above the floor: 256x128 downgrades to 128x64. Smaller planes are below the
  // ladder's minimum valid dual-disc size and are refused by design (CanDowngradeOnce), which is
  // its own case below.
  constexpr int kW = 256;
  constexpr int kH = 128;
  for (float v : { 0.0f, 0.5f, 1.0f, 12.75f }) {
    const std::vector<float> src = ConstantPlane(kW, kH, v);
    std::vector<float> dst;
    int out_w = 0;
    int out_h = 0;
    const bool applied = lumice::gui::DowngradeXyzPlaneBounded(src.data(), kW, kH, 1, &out_w, &out_h, dst);
    // Non-fatal + continue (loop-fatal-assert rule): one failed value must not hide the others.
    if (!applied) {
      ADD_FAILURE() << "the step did not apply for v=" << v;
      continue;
    }
    EXPECT_EQ(out_w, kW / 2);
    EXPECT_EQ(out_h, kH / 2);
    for (size_t i = 0; i < dst.size(); ++i) {
      // Non-fatal on purpose (loop-fatal-assert rule): one bad texel must not hide the rest.
      // The candidate runs over the CANONICAL-half input, so a constant survives its own
      // quantize-dequantize round trip: the tolerance is one fp16 ulp's worth of relative error,
      // not the raw sum/4 exactness. A summing implementation still fails this by exactly 4x.
      EXPECT_NEAR(dst[i], v, std::max(1e-6f, std::abs(v) * 2e-3f))
          << "texel " << i << " of a constant " << v << " plane moved";
    }
  }
}

// Law 1, exact-stencil form: each destination texel is the plain area average of its source
// block in the linear domain, per component, with no reweighting and no skip. Pinning the number
// (not just the range) is what rejects a skip-stencil (nearest-neighbour resample in disguise)
// and a corner-weighted one alike. The isolated block sits in a zero plane at the floor-legal
// size, so the destination texel at the block's corner carries the block's own average alone.
TEST(PreviewDowngrade, CandidateIsThePlainBlockAverage) {
  constexpr int kW = 256;
  constexpr int kH = 128;
  std::vector<float> src(static_cast<size_t>(kW) * kH * 3, 0.0f);
  const float blocks[4][3] = { { 1.0f, 2.0f, 3.0f }, { 3.0f, 2.0f, 1.0f }, { 5.0f, 4.0f, 7.0f }, { 7.0f, 4.0f, 5.0f } };
  for (int y = 0; y < 2; ++y) {
    for (int x = 0; x < 2; ++x) {
      for (int c = 0; c < 3; ++c) {
        src[(static_cast<size_t>(y) * kW + x) * 3 + c] = blocks[y * 2 + x][c];
      }
    }
  }
  std::vector<float> dst;
  int out_w = 0;
  int out_h = 0;
  ASSERT_TRUE(lumice::gui::DowngradeXyzPlaneBounded(src.data(), kW, kH, 1, &out_w, &out_h, dst));
  ASSERT_EQ(out_w, kW / 2);
  ASSERT_EQ(out_h, kH / 2);
  // The canonical round trip moves each raw value by at most a fp16 ulp's worth; a skip stencil
  // would be off by ~25%, a corner weighting by ~12% — orders above the tolerance.
  const float tol = 7.0f * 2e-3f;
  EXPECT_NEAR(dst[0], (1.0f + 3.0f + 5.0f + 7.0f) / 4.0f, tol);
  EXPECT_NEAR(dst[1], (2.0f + 2.0f + 4.0f + 4.0f) / 4.0f, tol);
  EXPECT_NEAR(dst[2], (3.0f + 1.0f + 7.0f + 5.0f) / 4.0f, tol);
  // Every other destination texel saw only zeros.
  for (size_t i = 3; i < dst.size(); ++i) {
    EXPECT_NEAR(dst[i], 0.0f, tol) << "texel " << i << " of the zero field moved";
  }
}

// Law 2, bounded form: a candidate depends on (source, step count) ALONE. The direct block
// computation makes this hold by construction — it always reads the original plane — and the
// assertion pins it: a repeated derivation is bit-identical, and a derivation against a copy of
// the source matches one against the source, so a retry that re-derives from the retained
// source cannot depend on how many failed attempts came before it. (This is the property the
// old nested form expressed as compose-equality; with one-pass blocks the two orders differ by
// one extra canonical round trip at a rounding boundary, which is a value-class difference, not
// the law.)
TEST(PreviewDowngrade, StepsAlwaysDeriveFromTheOriginalPlane) {
  using namespace preview_downgrade_test;
  constexpr int kW = 512;
  constexpr int kH = 256;
  const std::vector<float> src = CoordinatePlane(kW, kH, /*seed=*/5);
  std::vector<float> dst_a;
  int w = 0;
  int h = 0;
  ASSERT_TRUE(lumice::gui::DowngradeXyzPlaneBounded(src.data(), kW, kH, 2, &w, &h, dst_a));
  ASSERT_EQ(w, kW / 4);
  ASSERT_EQ(h, kH / 4);

  std::vector<float> dst_b;
  int w2 = 0;
  int h2 = 0;
  ASSERT_TRUE(lumice::gui::DowngradeXyzPlaneBounded(src.data(), kW, kH, 2, &w2, &h2, dst_b));
  EXPECT_EQ(dst_a, dst_b);

  const std::vector<float> src_copy = src;  // the retained-source copy a retry would hold
  std::vector<float> dst_copy;
  int wc = 0;
  int hc = 0;
  ASSERT_TRUE(lumice::gui::DowngradeXyzPlaneBounded(src_copy.data(), kW, kH, 2, &wc, &hc, dst_copy));
  EXPECT_EQ(dst_a, dst_copy);
}

// The candidate runs over the CANONICAL-half input — the same values the .lmc codec stores — so
// a live frame's candidate and a reopened document's candidate are derived from one input, not
// two nearly-equal ones. The reference here canonicalizes the plane itself through the codec and
// averages it with the plain nested form; the bounded function must agree with that reference,
// not invent its own input domain.
TEST(PreviewDowngrade, CandidateRunsOverTheCanonicalHalfInput) {
  using namespace preview_downgrade_test;
  constexpr int kW = 256;
  constexpr int kH = 128;
  const std::vector<float> src = CoordinatePlane(kW, kH, /*seed=*/9);
  std::vector<float> dst;
  int out_w = 0;
  int out_h = 0;
  ASSERT_TRUE(lumice::gui::DowngradeXyzPlaneBounded(src.data(), kW, kH, 1, &out_w, &out_h, dst));
  ASSERT_EQ(out_w, kW / 2);
  ASSERT_EQ(out_h, kH / 2);

  // Reference: canonicalize the whole plane through the codec, then quarter-sum each 2x2 block.
  const size_t count = static_cast<size_t>(kW) * kH * 3;
  std::vector<uint16_t> half(count);
  std::vector<float> canonical(count);
  lumice::gui::QuantizeXyzToHalf(src.data(), count, lumice::gui::ComputeXyzHalfScale(src.data(), count), half.data());
  lumice::gui::DequantizeHalfToXyz(half.data(), count, lumice::gui::ComputeXyzHalfScale(src.data(), count),
                                   canonical.data());
  std::vector<float> reference(static_cast<size_t>(kW / 2) * (kH / 2) * 3, 0.0f);
  for (int y = 0; y < kH / 2; ++y) {
    for (int x = 0; x < kW / 2; ++x) {
      for (int c = 0; c < 3; ++c) {
        const float a = canonical[(static_cast<size_t>(y * 2) * kW + x * 2) * 3 + c];
        const float b = canonical[(static_cast<size_t>(y * 2) * kW + x * 2 + 1) * 3 + c];
        const float e = canonical[(static_cast<size_t>((y * 2 + 1)) * kW + x * 2) * 3 + c];
        const float f = canonical[(static_cast<size_t>((y * 2 + 1)) * kW + x * 2 + 1) * 3 + c];
        reference[(static_cast<size_t>(y) * (kW / 2) + x) * 3 + c] = 0.25f * (a + b + e + f);
      }
    }
  }
  for (size_t i = 0; i < dst.size(); ++i) {
    EXPECT_NEAR(dst[i], reference[i], std::max(1e-5f, std::abs(dst[i]) * 2e-3f)) << "texel " << i;
  }
}

// A host allocation failure is a REPORTED stage, not a process exit: the fault seam throws from
// the bounded function's own buffer allocations, the ladder's caller converts it, and after the
// fault is cleared the same candidate computes.
TEST(PreviewDowngrade, AllocationFailureThrowsAndRecovers) {
  using namespace preview_downgrade_test;
  constexpr int kW = 256;
  constexpr int kH = 128;
  const std::vector<float> src = ConstantPlane(kW, kH, 0.5f);
  std::vector<float> dst;
  int out_w = 0;
  int out_h = 0;
  lumice::gui::SetDowngradeAllocFaultForTest(/*remaining_failures=*/1);
  EXPECT_THROW(lumice::gui::DowngradeXyzPlaneBounded(src.data(), kW, kH, 1, &out_w, &out_h, dst), std::bad_alloc);
  lumice::gui::SetDowngradeAllocFaultForTest(/*remaining_failures=*/0);
  EXPECT_TRUE(lumice::gui::DowngradeXyzPlaneBounded(src.data(), kW, kH, 1, &out_w, &out_h, dst));
  EXPECT_EQ(out_w, kW / 2);
}

// The ladder terminates: from a full-size dual-disc frame the sizes halve until the floor
// (8192x4096 -> 128x64 in six steps), and the seventh step — which would cross it — is refused.
// Finite candidates by construction, plan 814 §3.2.
TEST(PreviewDowngrade, LadderStopsAtTheMinimumDualDiscFloor) {
  int w = 0;
  int h = 0;
  ASSERT_TRUE(lumice::gui::DowngradedSize(8192, 4096, 6, &w, &h));
  EXPECT_EQ(w, 128);
  EXPECT_EQ(h, 64);
  EXPECT_FALSE(lumice::gui::DowngradedSize(8192, 4096, 7, &w, &h));
  EXPECT_FALSE(lumice::gui::CanDowngradeOnce(128, 64));
  // A frame already at the floor has no next candidate; steps == 0 is the frame itself.
  EXPECT_TRUE(lumice::gui::DowngradedSize(128, 64, 0, &w, &h));
  EXPECT_FALSE(lumice::gui::DowngradedSize(128, 64, 1, &w, &h));
}

// The half-source twin must be INDISTINGUISHABLE from the float twin run on the canonical values:
// a held frame's bits ARE its canonical values (the bits were produced by QuantizeXyzToHalf), so
// dequantizing one row is exactly the float twin's quantize-then-dequantize of that row. Pinning
// byte equality between the twins is what forbids the half variant from ever growing a second
// averaging path that drifts.
TEST(PreviewDowngrade, HalfSourceTwinMatchesTheFloatTwinOnTheCanonicalValues) {
  // 512x256 takes two steps to the floor-sized 128x64; a 256x128 plane would cross the floor on
  // the second step and the step would (correctly) refuse.
  constexpr int kW = 512;
  constexpr int kH = 256;
  const auto coordinate = [](int x, int y, int c) {
    return static_cast<float>(((x * 7 + y * 5 + c * 13) % 61) - 30) / 30.0f * 40.0f;
  };
  std::vector<float> floats(static_cast<size_t>(kW) * kH * 3);
  for (int y = 0; y < kH; ++y) {
    for (int x = 0; x < kW; ++x) {
      for (int c = 0; c < 3; ++c) {
        floats[(static_cast<size_t>(y) * kW + x) * 3 + c] = coordinate(x, y, c);
      }
    }
  }
  const float scale = lumice::gui::ComputeXyzHalfScale(floats.data(), floats.size());
  std::vector<uint16_t> bits(floats.size());
  lumice::gui::QuantizeXyzToHalf(floats.data(), floats.size(), scale, bits.data());

  std::vector<float> from_floats;
  int fw = 0;
  int fh = 0;
  ASSERT_TRUE(lumice::gui::DowngradeXyzPlaneBounded(floats.data(), kW, kH, 2, &fw, &fh, from_floats));
  std::vector<float> from_bits;
  int bw = 0;
  int bh = 0;
  ASSERT_TRUE(lumice::gui::DowngradeXyzHalfPlaneBounded(bits.data(), scale, kW, kH, 2, &bw, &bh, from_bits));
  ASSERT_EQ(fw, bw);
  ASSERT_EQ(fh, bh);
  ASSERT_EQ(from_floats.size(), from_bits.size());
  for (size_t i = 0; i < from_floats.size(); ++i) {
    EXPECT_EQ(from_floats[i], from_bits[i]) << "texel " << i << ": the twins disagree";
  }

  // Law 1 still holds through the twin: a constant half plane downgrades to the same constant.
  std::vector<float> constant(static_cast<size_t>(kW) * kH * 3, 8.0f);
  const float c_scale = lumice::gui::ComputeXyzHalfScale(constant.data(), constant.size());
  std::vector<uint16_t> c_bits(constant.size());
  lumice::gui::QuantizeXyzToHalf(constant.data(), constant.size(), c_scale, c_bits.data());
  std::vector<float> c_dst;
  int cw = 0;
  int ch = 0;
  ASSERT_TRUE(lumice::gui::DowngradeXyzHalfPlaneBounded(c_bits.data(), c_scale, kW, kH, 1, &cw, &ch, c_dst));
  ASSERT_EQ(cw, kW / 2);
  ASSERT_EQ(ch, kH / 2);
  for (size_t i = 0; i < c_dst.size(); ++i) {
    EXPECT_NEAR(c_dst[i], 8.0f, 8.0f * 2e-3f) << "texel " << i << " of a constant half plane moved";
  }

  // Every candidate derives from the ORIGINAL plane by step count — the twin reads the caller's
  // bits, so re-running it must reproduce the same candidate (no compounding across attempts).
  std::vector<float> again;
  int aw = 0;
  int ah = 0;
  ASSERT_TRUE(lumice::gui::DowngradeXyzHalfPlaneBounded(bits.data(), scale, kW, kH, 2, &aw, &ah, again));
  ASSERT_EQ(again, from_bits);

  // The allocation fault propagates from the twin like it does from the float twin.
  lumice::gui::SetDowngradeAllocFaultForTest(1);
  std::vector<float> faulted;
  int xw = 0;
  int xh = 0;
  EXPECT_THROW(lumice::gui::DowngradeXyzHalfPlaneBounded(bits.data(), scale, kW, kH, 1, &xw, &xh, faulted),
               std::bad_alloc);
  lumice::gui::SetDowngradeAllocFaultForTest(0);
}

// Mirror retirement: a representation switch and a document clear must release the RETIRED
// mirror's heap block, not merely empty it — capacity() is the witness, and the null-pointer
// queries cannot carry it (a cleared-but-allocated vector passes them while holding its old
// allocation). The move handover also consumes the caller's buffer, which is what keeps the
// production open at ONE full frame instead of two.
TEST(PreviewRenderer, MirrorRetirementReleasesTheRetiredRepresentationsStorage) {
  namespace g = lumice::gui;
  g::PreviewRenderer renderer;  // no Init: the mirror writers are CPU-only
  constexpr int kW = 8;
  constexpr int kH = 4;
  const size_t float_bytes = static_cast<size_t>(kW) * kH * 3 * sizeof(float);
  const size_t half_bytes = static_cast<size_t>(kW) * kH * 3 * sizeof(uint16_t);
  g::XyzTextureMeta meta;

  std::vector<float> floats(static_cast<size_t>(kW) * kH * 3, 1.0f);
  renderer.UpdateCpuXyzTextureData(floats.data(), kW, kH, meta);
  // Allocation sizes are >= (a standard library may round a growth up); RELEASE is == 0, the
  // one exact statement a swap-based retirement makes on every implementation.
  EXPECT_GE(renderer.CpuMirrorCapacityBytesForTest(), float_bytes);
  EXPECT_TRUE(renderer.CpuXyzStorageIsHalf() == false);

  // The production open's handover: MOVE the half buffer in. The float mirror retires with its
  // storage, the moved-from vector leaves empty, and the mirror's heap is the half's alone.
  std::vector<uint16_t> bits(static_cast<size_t>(kW) * kH * 3, 0x3C00u);  // half 1.0
  renderer.UpdateCpuXyzHalfTextureData(std::move(bits), kW, kH, 1.0f, meta);
  EXPECT_TRUE(bits.empty()) << "the move handover consumed the caller's buffer";
  EXPECT_TRUE(renderer.GetXyzTextureData() == nullptr);
  EXPECT_EQ(renderer.CpuMirrorCapacityBytesForTest(), half_bytes)
      << "the retired float representation's storage must be released, not kept as capacity";
  EXPECT_TRUE(renderer.CpuXyzStorageIsHalf());

  // The copy writer (tests seeding from their own bytes) retires the same way.
  std::vector<uint16_t> bits2(static_cast<size_t>(kW) * kH * 3, 0x0000u);
  renderer.UpdateCpuXyzHalfTextureData(bits2.data(), kW, kH, 2.0f, meta);
  EXPECT_GE(renderer.CpuMirrorCapacityBytesForTest(), half_bytes);
  EXPECT_LE(renderer.CpuMirrorCapacityBytesForTest(), 2 * half_bytes);

  // A uint8 mirror write retires BOTH XYZ representations' storage.
  std::vector<unsigned char> srgb(static_cast<size_t>(kW) * kH * 3, 7);
  renderer.UpdateCpuTextureData(srgb.data(), kW, kH);
  const size_t srgb_bytes = static_cast<size_t>(kW) * kH * 3;
  EXPECT_GE(renderer.CpuMirrorCapacityBytesForTest(), srgb_bytes);
  EXPECT_LT(renderer.CpuMirrorCapacityBytesForTest(), srgb_bytes + half_bytes)
      << "uint8 bytes only: both XYZ representations' storage released";

  // Document clear retires everything: the heap is empty, not merely element-free.
  renderer.UpdateCpuXyzHalfTextureData(std::vector<uint16_t>(static_cast<size_t>(kW) * kH * 3, 0x3C00u), kW, kH, 1.0f,
                                       meta);
  EXPECT_GT(renderer.CpuMirrorCapacityBytesForTest(), 0u);
  renderer.ClearTexture();
  EXPECT_EQ(renderer.CpuMirrorCapacityBytesForTest(), 0u)
      << "a cleared document keeps no mirror allocation of any representation";
}

// The strip staging plan: rows are what the budget buys, the override only shrinks, a small
// frame is one strip, the tail strip is the remaining rows (never an empty strip), and the slot
// allocation is the capacity the plan actually holds — the numbers the renderer maps, fences and
// subimages against, asserted here so the GL code never re-derives a second cut.
TEST(XyzStripPlan, BudgetOverrideAndTailAreAllRowGranular) {
  namespace g = lumice::gui;
  // The production cut of the archive size: the budget buys whole rows, the count is the ceil,
  // and the two slots together stay inside the ~4 MiB staging budget.
  const g::XyzStripPlan big = g::ComputeXyzStripPlan(8192, 4096, 0);
  EXPECT_EQ(big.row_bytes, static_cast<size_t>(8192) * 3 * sizeof(uint16_t));
  EXPECT_GT(big.rows_per_strip, 0);
  EXPECT_EQ(big.slot_bytes, static_cast<size_t>(big.rows_per_strip) * big.row_bytes);
  EXPECT_EQ(big.strip_count, (4096 + big.rows_per_strip - 1) / big.rows_per_strip);
  EXPECT_LE(2 * big.slot_bytes, 2 * g::kXyzStagingSlotTargetBytes);
  EXPECT_LT(2 * big.slot_bytes, 8192.0 * 4096.0 * 3.0 * 2.0) << "staging is a fraction of the frame, not a copy";

  // A small frame is ONE strip: the plan never invents empty strips, and the slot is exactly the
  // frame's own byte count.
  const g::XyzStripPlan small = g::ComputeXyzStripPlan(16, 10, 0);
  EXPECT_EQ(small.rows_per_strip, 10);
  EXPECT_EQ(small.strip_count, 1);
  EXPECT_EQ(small.slot_bytes, static_cast<size_t>(16) * 10 * 3 * sizeof(uint16_t));

  // The override only shrinks: 2-row strips of a 9-row frame are 2,2,2,2,1 — four full strips
  // and a one-row tail, the cut the multi-strip witnesses ride.
  const g::XyzStripPlan capped = g::ComputeXyzStripPlan(16, 9, 2);
  EXPECT_EQ(capped.rows_per_strip, 2);
  EXPECT_EQ(capped.strip_count, 5);
  EXPECT_EQ(capped.slot_bytes, 2 * capped.row_bytes);
  // The override cannot push past the budget rule: a cap above what ~2 MiB buys is ignored.
  const g::XyzStripPlan uncapped = g::ComputeXyzStripPlan(8192, 4096, 100000);
  EXPECT_EQ(uncapped.rows_per_strip, big.rows_per_strip);

  // Odd width: a row is still one unit (width*6 bytes, even, alignment 2), never split.
  const g::XyzStripPlan odd = g::ComputeXyzStripPlan(15, 9, 2);
  EXPECT_EQ(odd.row_bytes, static_cast<size_t>(15) * 3 * sizeof(uint16_t));
  EXPECT_EQ(odd.strip_count, 5);

  // Degenerate sizes stay sane: a one-row frame is one one-row strip; a width so wide the
  // budget buys less than a row still gets that row.
  const g::XyzStripPlan tiny = g::ComputeXyzStripPlan(4, 1, 0);
  EXPECT_EQ(tiny.rows_per_strip, 1);
  EXPECT_EQ(tiny.strip_count, 1);
  const g::XyzStripPlan wide = g::ComputeXyzStripPlan(400000, 3, 0);
  EXPECT_EQ(wide.rows_per_strip, 1);
  EXPECT_EQ(wide.strip_count, 3);
}
