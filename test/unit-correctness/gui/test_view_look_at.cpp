// The Look At presets' arithmetic: a named sky direction in, the camera angles that point at it out.
//
// THE ORACLE IS WRITTEN OUT LONGHAND HERE, ON PURPOSE. Every expected direction below is derived
// from the reflection rules stated in prose (annotation_overlay.hpp, and the header of the code
// under test), and every forward vector is rebuilt from three trigonometric lines typed into this
// file rather than obtained by calling BuildViewMatrix. An "equivalence" check that called the
// production formula on both sides would pass just as happily with the azimuth sign inverted
// everywhere — the exact failure mode task 487.1 recorded, where a shared implementation makes an
// oracle structurally blind to the thing it exists to catch.
//
// The judgement is therefore always an ANALYTIC RELATION — "the antisolar preset points 180 degrees
// away from the sun", "the sun-side horizon preset is level and shares the sun's bearing" — not a
// reproduction of what the code does.

#include <gtest/gtest.h>

#include <array>
#include <cmath>

#include "gui/annotation_anchors.hpp"     // GuiSunWorldDir — the sun as the code under test spells it
#include "gui/field_editor_registry.hpp"  // ConstraintFor — the gate and bounds the menu borrows
#include "gui/gui_state.hpp"              // kMarkerDisplayNames — the Overlay list this menu must agree with
#include "gui/view_look_at.hpp"

namespace {

using lumice::gui::LookAtId;

constexpr float kDeg2Rad = 3.14159265358979323846f / 180.0f;

// The camera's forward vector at (az, el), typed out from the convention BuildViewMatrix documents
// rather than obtained from it. This is the whole oracle: "looking at D" means forward == D.
std::array<float, 3> ForwardAt(float az_deg, float el_deg) {
  const float a = az_deg * kDeg2Rad;
  const float e = el_deg * kDeg2Rad;
  return { -std::cos(e) * std::cos(a), -std::cos(e) * std::sin(a), -std::sin(e) };
}

// The sun as the GUI means it: azimuth is not exposed, so the sun always sits in the y = 0 plane,
// and altitude = asin(-z) puts a sun ABOVE the horizon at negative z.
std::array<float, 3> SunDirAt(float altitude_deg) {
  const float s = altitude_deg * kDeg2Rad;
  return { -std::cos(s), 0.0f, -std::sin(s) };
}

float Dot(const std::array<float, 3>& a, const std::array<float, 3>& b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

// The forward vector the preset produces, or a failure the caller can assert on.
std::array<float, 3> ForwardOfPreset(LookAtId id, float sun_altitude_deg, bool* ok) {
  float az = 12345.0f;
  float el = 12345.0f;
  *ok = lumice::gui::ResolveLookAtAzEl(id, sun_altitude_deg, &az, &el);
  if (!*ok) {
    return { 0.0f, 0.0f, 0.0f };
  }
  return ForwardAt(az, el);
}

}  // namespace

// ---------------------------------------------------------------------------------------------
// WorldDirToAzEl — the pose half, pinned at hand-computed points.
// ---------------------------------------------------------------------------------------------

TEST(ViewLookAtMath, InvertsTheCameraForwardAtHandComputedPoints) {
  // Each row is read straight off forward = (-cos(el)cos(az), -cos(el)sin(az), -sin(el)):
  // world -x is the direction the camera faces at (0, 0), so it must come back as (0, 0).
  const auto check = [](float dx, float dy, float dz, float want_az, float want_el, const char* what) {
    const float dir[3] = { dx, dy, dz };
    float az = 0.0f;
    float el = 0.0f;
    lumice::gui::WorldDirToAzEl(dir, &az, &el);
    EXPECT_NEAR(az, want_az, 1e-3f) << what << " azimuth";
    EXPECT_NEAR(el, want_el, 1e-3f) << what << " elevation";
  };
  check(-1.0f, 0.0f, 0.0f, 0.0f, 0.0f, "the equirect centre");
  check(0.0f, 1.0f, 0.0f, -90.0f, 0.0f, "world +y");
  check(0.0f, -1.0f, 0.0f, 90.0f, 0.0f, "world -y");
  // World +x is a half turn from the centre. Asserted on the magnitude because +180 and -180 are
  // the same azimuth — both inside the slider's range, giving cameras no caller can tell apart —
  // and which one comes back is decided by the sign of the zero atan2 is handed.
  {
    const float back[3] = { 1.0f, 0.0f, 0.0f };
    float az = 0.0f;
    float el = 0.0f;
    lumice::gui::WorldDirToAzEl(back, &az, &el);
    EXPECT_NEAR(std::abs(az), 180.0f, 1e-3f);
    EXPECT_NEAR(el, 0.0f, 1e-3f);
  }
  // Altitude = asin(-z), so the zenith is z = -1. This sign is the one thing in this coordinate
  // family that is worth a dedicated row: the same English word means the opposite z in
  // doc/coordinate-convention.md.
  // The poles have no bearing at all, and this is the branch that says so: left to atan2's
  // signed-zero rule they would come back at -180, half a turn of camera spin on the way to a
  // direction that does not depend on the azimuth.
  check(0.0f, 0.0f, -1.0f, 0.0f, 90.0f, "the zenith");
  check(0.0f, 0.0f, 1.0f, 0.0f, -90.0f, "the nadir");
}

TEST(ViewLookAtMath, IgnoresTheMagnitudeOfTheDirectionItIsGiven) {
  const float unit[3] = { -0.6f, 0.48f, -0.64f };
  const float scaled[3] = { unit[0] * 25.0f, unit[1] * 25.0f, unit[2] * 25.0f };
  float az_u = 0.0f, el_u = 0.0f, az_s = 0.0f, el_s = 0.0f;
  lumice::gui::WorldDirToAzEl(unit, &az_u, &el_u);
  lumice::gui::WorldDirToAzEl(scaled, &az_s, &el_s);
  EXPECT_NEAR(az_s, az_u, 1e-4f);
  EXPECT_NEAR(el_s, el_u, 1e-4f);
}

TEST(ViewLookAtMath, AnswersFiniteAnglesAtThePolesRatherThanNaN) {
  // atan2(0, 0) is 0, not a NaN — the azimuth at a pole is undefined and 0 is one of the infinitely
  // many correct answers. What must never happen is a NaN reaching a camera angle: it would poison
  // the view matrix and blank the preview with no error anywhere.
  const auto check_finite = [](float dx, float dy, float dz) {
    const float dir[3] = { dx, dy, dz };
    float az = 0.0f;
    float el = 0.0f;
    lumice::gui::WorldDirToAzEl(dir, &az, &el);
    EXPECT_TRUE(std::isfinite(az)) << dx << "," << dy << "," << dz;
    EXPECT_TRUE(std::isfinite(el)) << dx << "," << dy << "," << dz;
  };
  check_finite(0.0f, 0.0f, -1.0f);
  check_finite(0.0f, 0.0f, 1.0f);
  check_finite(0.0f, 0.0f, 0.0f);         // no direction at all
  check_finite(0.0f, 0.0f, -1.0000002f);  // |z| past 1 by rounding, asin's domain edge
}

// ---------------------------------------------------------------------------------------------
// ResolveLookAtAzEl — the presets, each judged by the relation that defines it.
// ---------------------------------------------------------------------------------------------

TEST(ViewLookAtPresets, EachDirectionSatisfiesItsDefiningRelation) {
  // Three altitudes, one of them below the horizon: a sun that has set still has an anthelion and
  // a subsun, and the camera can still be pointed at them. A single altitude would let a rule that
  // confuses "negate the altitude" with "negate the bearing" pass by accident.
  const auto check_altitude = [](float alt) {
    const std::array<float, 3> sun = SunDirAt(alt);
    bool ok = false;

    const std::array<float, 3> f_sun = ForwardOfPreset(LookAtId::kSun, alt, &ok);
    ASSERT_TRUE(ok) << "sun at altitude " << alt;
    EXPECT_NEAR(Dot(f_sun, sun), 1.0f, 1e-4f) << "Sun must look straight at the sun, altitude " << alt;

    const std::array<float, 3> f_anti = ForwardOfPreset(LookAtId::kAntisolar, alt, &ok);
    ASSERT_TRUE(ok);
    EXPECT_NEAR(Dot(f_anti, sun), -1.0f, 1e-4f) << "Antisolar must look 180 degrees from the sun, altitude " << alt;

    // Subsun: the sun reflected in a horizontal surface — same bearing, negated altitude.
    const std::array<float, 3> subsun = { sun[0], sun[1], -sun[2] };
    const std::array<float, 3> f_subsun = ForwardOfPreset(LookAtId::kSubsun, alt, &ok);
    ASSERT_TRUE(ok);
    EXPECT_NEAR(Dot(f_subsun, subsun), 1.0f, 1e-4f) << "altitude " << alt;

    // Anthelion: opposite bearing, SAME altitude.
    const std::array<float, 3> anthelion = { -sun[0], -sun[1], sun[2] };
    const std::array<float, 3> f_anth = ForwardOfPreset(LookAtId::kAnthelion, alt, &ok);
    ASSERT_TRUE(ok);
    EXPECT_NEAR(Dot(f_anth, anthelion), 1.0f, 1e-4f) << "altitude " << alt;

    // The poles do not depend on the sun at all, which is half of what makes them poles.
    const std::array<float, 3> f_zen = ForwardOfPreset(LookAtId::kZenith, alt, &ok);
    ASSERT_TRUE(ok);
    EXPECT_NEAR(Dot(f_zen, { 0.0f, 0.0f, -1.0f }), 1.0f, 1e-4f) << "altitude " << alt;
    const std::array<float, 3> f_nad = ForwardOfPreset(LookAtId::kNadir, alt, &ok);
    ASSERT_TRUE(ok);
    EXPECT_NEAR(Dot(f_nad, { 0.0f, 0.0f, 1.0f }), 1.0f, 1e-4f) << "altitude " << alt;

    // Toward sun (the sun-side horizon): level, and on the sun's side rather than the opposite one. The second half
    // is what a sign slip would break, and a "parallel to the sun's bearing" test alone would not
    // notice it.
    float az_h = 0.0f;
    float el_h = 0.0f;
    ASSERT_TRUE(lumice::gui::ResolveLookAtAzEl(LookAtId::kSunHorizon, alt, &az_h, &el_h));
    EXPECT_NEAR(el_h, 0.0f, 1e-4f) << "Toward sun must be level, altitude " << alt;
    const std::array<float, 3> f_horiz = ForwardAt(az_h, el_h);
    const float sun_h_len = std::sqrt(sun[0] * sun[0] + sun[1] * sun[1]);
    ASSERT_GT(sun_h_len, 1e-3f) << "test setup: altitude " << alt << " has no bearing to compare";
    EXPECT_NEAR(f_horiz[0] * sun[0] / sun_h_len + f_horiz[1] * sun[1] / sun_h_len, 1.0f, 1e-4f)
        << "Toward sun must share the sun's bearing, not oppose it; altitude " << alt;
  };
  check_altitude(25.0f);
  check_altitude(0.0f);
  check_altitude(-15.0f);  // sun below the horizon
}

TEST(ViewLookAtPresets, EveryDirectionIsDistinctForAGenericSun) {
  // Without this, every relation above could be satisfied by an implementation that returned one
  // direction for several ids — each assertion only looks at its own row. Sized by kCount rather
  // than a literal so an entry added later is in the matrix without anyone remembering to add it.
  const float alt = 33.0f;
  std::array<std::array<float, 3>, static_cast<int>(LookAtId::kCount)> forwards{};
  for (int i = 0; i < static_cast<int>(LookAtId::kCount); ++i) {
    bool ok = false;
    forwards[i] = ForwardOfPreset(static_cast<LookAtId>(i), alt, &ok);
    EXPECT_TRUE(ok) << "id " << i;
  }
  for (int i = 0; i < static_cast<int>(LookAtId::kCount); ++i) {
    for (int j = i + 1; j < static_cast<int>(LookAtId::kCount); ++j) {
      EXPECT_LT(Dot(forwards[i], forwards[j]), 0.999f) << "ids " << i << " and " << j << " point the same way";
    }
  }
}

TEST(ViewLookAtPresets, SunSideHorizonStaysFiniteThroughTheDegenerateBand) {
  // AC5: as the sun approaches a pole its bearing stops existing, and core falls back to a FIXED
  // direction (world +x, azimuth 0) rather than a nearest-neighbour one. What this case pins is the
  // consequence for a camera angle: finite, level, and — at the two poles — the SAME answer, which
  // a nearest-neighbour rule would not give, since the float residue of cos() changes sign across
  // +/-90 and would send the two poles 180 degrees apart.
  const auto check_finite = [](float alt) {
    float az = 0.0f;
    float el = 0.0f;
    ASSERT_TRUE(lumice::gui::ResolveLookAtAzEl(LookAtId::kSunHorizon, alt, &az, &el)) << "altitude " << alt;
    EXPECT_TRUE(std::isfinite(az)) << "altitude " << alt;
    EXPECT_TRUE(std::isfinite(el)) << "altitude " << alt;
    EXPECT_NEAR(el, 0.0f, 1e-4f) << "altitude " << alt;
  };
  check_finite(89.0f);
  check_finite(90.0f - 1e-7f);
  check_finite(90.0f);
  check_finite(-(90.0f - 1e-7f));
  check_finite(-90.0f);

  float az_up = 0.0f, el_up = 0.0f, az_down = 0.0f, el_down = 0.0f;
  ASSERT_TRUE(lumice::gui::ResolveLookAtAzEl(LookAtId::kSunHorizon, 90.0f, &az_up, &el_up));
  ASSERT_TRUE(lumice::gui::ResolveLookAtAzEl(LookAtId::kSunHorizon, -90.0f, &az_down, &el_down));
  EXPECT_FLOAT_EQ(az_up, az_down) << "the fixed fallback must not depend on which pole the sun is at";
  // The fallback is world +x, a half turn from the equirect centre. Magnitude again, for the same
  // signed-zero reason as above; what this pins is that the fallback is that direction and not
  // some other one, so a change to it shows up here rather than passing silently.
  EXPECT_NEAR(std::abs(az_up), 180.0f, 1e-3f);
}

// ---------------------------------------------------------------------------------------------
// The Horizon series — four level bearings relative to the sun, AC2: one case per entry, each
// asserted to the literal number the user will read off the Azimuth slider.
//
// The numbers are literal on purpose. A relation ("+90 is a quarter turn from Toward sun") would be
// satisfied just as well by -90, which is exactly the slip these cases exist to catch: with the
// sun pinned at azimuth 0, "Sun +90°" MUST land the slider on +90, and an offset table with its
// sign inverted lands it on -90 — a different entry's number, and red here.
//
// Two altitudes, one below the horizon, and neither near a pole: the bearing is well defined at
// both, so the elevation is exactly 0 and the azimuth is exactly the offset. The pole itself, where
// core falls back to a fixed bearing, is the separate case that follows.
// ---------------------------------------------------------------------------------------------

namespace {

// One row per altitude, each in its own call so a rejected row reports and the next still runs.
void ExpectHorizonEntryAt(LookAtId id, float alt, float want_az_deg, const char* what) {
  float az = 12345.0f;
  float el = 12345.0f;
  ASSERT_TRUE(lumice::gui::ResolveLookAtAzEl(id, alt, &az, &el)) << what << " altitude " << alt;
  EXPECT_NEAR(el, 0.0f, 1e-4f) << what << " must be level, altitude " << alt;
  EXPECT_NEAR(az, want_az_deg, 1e-3f) << what << " altitude " << alt;
}

}  // namespace

TEST(ViewLookAtHorizon, TowardSunLandsOnAzimuthZero) {
  ExpectHorizonEntryAt(LookAtId::kSunHorizon, 25.0f, 0.0f, "Toward sun");
  ExpectHorizonEntryAt(LookAtId::kSunHorizon, -15.0f, 0.0f, "Toward sun");
}

TEST(ViewLookAtHorizon, SunPlus90LandsOnAzimuthPlus90) {
  ExpectHorizonEntryAt(LookAtId::kHorizonSunPlus90, 25.0f, 90.0f, "Sun +90");
  ExpectHorizonEntryAt(LookAtId::kHorizonSunPlus90, -15.0f, 90.0f, "Sun +90");
}

TEST(ViewLookAtHorizon, AwayFromSunLandsOnAzimuth180) {
  // 180 and -180 are the same bearing and both inside the slider's range; which one comes back is
  // decided by the sign of a zero, so the magnitude is what is pinned (same rule as the antisolar
  // rows elsewhere in this file).
  const auto check_altitude = [](float alt) {
    float az = 12345.0f;
    float el = 12345.0f;
    ASSERT_TRUE(lumice::gui::ResolveLookAtAzEl(LookAtId::kHorizonAntiSun, alt, &az, &el)) << "altitude " << alt;
    EXPECT_NEAR(el, 0.0f, 1e-4f) << "Away from sun must be level, altitude " << alt;
    EXPECT_NEAR(std::abs(az), 180.0f, 1e-3f) << "altitude " << alt;
  };
  check_altitude(25.0f);
  check_altitude(-15.0f);
}

TEST(ViewLookAtHorizon, SunMinus90LandsOnAzimuthMinus90) {
  ExpectHorizonEntryAt(LookAtId::kHorizonSunMinus90, 25.0f, -90.0f, "Sun -90");
  ExpectHorizonEntryAt(LookAtId::kHorizonSunMinus90, -15.0f, -90.0f, "Sun -90");
}

TEST(ViewLookAtHorizon, TheOffsetsWrapRatherThanClampAtTheFallbackBearing) {
  // With the sun at a pole core falls back to a fixed bearing on the far side (|az| = 180, pinned
  // by SunSideHorizonStaysFiniteThroughTheDegenerateBand). Adding the series' offsets to that
  // leaves the slider's interval — -180 - 90 = -270, or 180 + 90 = 270 — and the only correct
  // answer is the same bearing brought back round: +90 for "Sun -90°", -90 for "Sun +90°", 0 for
  // "Away from sun". A clamp answers -180 or 180 for one of the two ±90 entries instead — the
  // bearing of "Toward sun" — and is red here whichever sign core's fallback carries. Every other
  // combination this file exercises keeps base + offset inside the interval, so this is the one
  // place a wrap and a clamp disagree, and it is exercised at both poles.
  const auto check_entry = [](LookAtId id, float alt, float want_az, const char* what) {
    float az = 12345.0f;
    float el = 12345.0f;
    ASSERT_TRUE(lumice::gui::ResolveLookAtAzEl(id, alt, &az, &el)) << what << " altitude " << alt;
    EXPECT_NEAR(az, want_az, 1e-3f) << what << " at the fallback bearing, altitude " << alt;
    EXPECT_NEAR(el, 0.0f, 1e-4f) << what << " altitude " << alt;
  };
  const auto check_pole = [&check_entry](float alt) {
    check_entry(LookAtId::kHorizonSunMinus90, alt, 90.0f, "Sun -90");
    check_entry(LookAtId::kHorizonSunPlus90, alt, -90.0f, "Sun +90");
    check_entry(LookAtId::kHorizonAntiSun, alt, 0.0f, "Away from sun");
  };
  check_pole(90.0f);
  check_pole(-90.0f);
}

TEST(ViewLookAtHorizon, TowardSunPassesTheCoreBearingThroughUnchanged) {
  // AC3: the entry that existed before the series did must answer exactly what it answered then —
  // the bearing core returns, converted, with nothing added and nothing folded. Compared against
  // the same C API call made by hand, so the assertion holds whichever of +180 / -180 the far-side
  // fallback comes back as: a wrap that mapped one endpoint of the closed interval onto the other
  // would pass a magnitude check and fail this one.
  const auto check_altitude = [](float alt) {
    float sun_dir[3] = {};
    lumice::gui::GuiSunWorldDir(alt, sun_dir);
    float target[3] = {};
    ASSERT_EQ(LUMICE_ResolveSunHorizonDirection(sun_dir, target), LUMICE_OK) << "altitude " << alt;
    float want_az = 0.0f;
    float want_el = 0.0f;
    lumice::gui::WorldDirToAzEl(target, &want_az, &want_el);

    float az = 12345.0f;
    float el = 12345.0f;
    ASSERT_TRUE(lumice::gui::ResolveLookAtAzEl(LookAtId::kSunHorizon, alt, &az, &el)) << "altitude " << alt;
    EXPECT_FLOAT_EQ(az, want_az) << "altitude " << alt;
    EXPECT_FLOAT_EQ(el, want_el) << "altitude " << alt;
  };
  check_altitude(25.0f);
  check_altitude(0.0f);
  check_altitude(-15.0f);
  check_altitude(90.0f);
  check_altitude(-90.0f);
}

TEST(ViewLookAtPresets, RejectsAnOutOfRangeIdWithoutWritingAnything) {
  float az = 4242.0f;
  float el = 4242.0f;
  EXPECT_FALSE(lumice::gui::ResolveLookAtAzEl(static_cast<LookAtId>(-1), 20.0f, &az, &el));
  EXPECT_FALSE(
      lumice::gui::ResolveLookAtAzEl(static_cast<LookAtId>(static_cast<int>(LookAtId::kCount)), 20.0f, &az, &el));
  // A rejected preset must leave the camera where it was rather than point it somewhere arbitrary.
  EXPECT_EQ(az, 4242.0f);
  EXPECT_EQ(el, 4242.0f);
}

// ---------------------------------------------------------------------------------------------
// AC6 — the menu and the Overlay list must show ONE name per direction.
// ---------------------------------------------------------------------------------------------

TEST(ViewLookAtNames, MarkerEntriesReadTheirNameFromTheOverlayListsOwnTable) {
  for (int i = 0; i < LUMICE_ANNOTATION_MARKER_COUNT; ++i) {
    // Pointer equality, not string equality: it says the menu READS the table rather than agreeing
    // with it today, so a rename in one place cannot leave the two showing different words.
    EXPECT_EQ(lumice::gui::LookAtDisplayName(static_cast<LookAtId>(i)), lumice::gui::kMarkerDisplayNames[i])
        << "marker id " << i;
  }
  EXPECT_EQ(lumice::gui::LookAtDisplayName(static_cast<LookAtId>(static_cast<int>(LookAtId::kCount))), nullptr);
}

TEST(ViewLookAtNames, TheHorizonSeriesIsNamedRelativeToTheSunAndIsNotAMarker) {
  // AC1: the four series ids sit after the markers, and the marker predicate says so — a series
  // entry that read as a marker would be sent to the marker table, which has no row for it.
  const struct {
    LookAtId id;
    const char* name;
  } kSeries[] = {
    { LookAtId::kSunHorizon, "Toward sun" },
    { LookAtId::kHorizonSunPlus90, "Sun +90°" },
    { LookAtId::kHorizonAntiSun, "Away from sun" },
    { LookAtId::kHorizonSunMinus90, "Sun -90°" },
  };
  for (const auto& entry : kSeries) {
    EXPECT_FALSE(lumice::gui::IsMarkerLookAt(entry.id)) << entry.name;
    EXPECT_GE(static_cast<int>(entry.id), LUMICE_ANNOTATION_MARKER_COUNT) << entry.name;
    EXPECT_LT(static_cast<int>(entry.id), static_cast<int>(LookAtId::kCount)) << entry.name;
    EXPECT_STREQ(lumice::gui::LookAtDisplayName(entry.id), entry.name);
  }
}

// ---------------------------------------------------------------------------------------------
// The gate and the bounds the menu borrows from the Az/El sliders. What the panel does with them
// is a frame-level claim and lives in test/gui/functional/test_view_display_controls.cpp; what is
// asserted here is that the registry actually HAS what the panel reads — in particular a reason
// string to put in the disabled tooltip, which no gui_test case can see (SetTooltip draws through
// TextUnformatted, id == 0, invisible to the test engine's item registry).
// ---------------------------------------------------------------------------------------------

TEST(ViewLookAtGate, TheElevationConstraintCarriesEverythingTheMenuNeedsToReadOffIt) {
  lumice::gui::GuiState state;

  const auto check_lens = [&state](int lens, bool want_enabled, float want_limit) {
    state.renderer.lens_type = lens;
    const lumice::gui::FieldEditorConstraint el = lumice::gui::ConstraintFor("renderer.elevation", state);
    EXPECT_EQ(el.enabled, want_enabled) << "lens " << lens;
    if (want_enabled) {
      // The bounds the menu clamps with. Globe stopping one degree short of the pole is the case
      // that makes clamping load-bearing rather than decorative.
      EXPECT_TRUE(el.has_numeric_domain) << "lens " << lens;
      EXPECT_DOUBLE_EQ(el.max_value, want_limit) << "lens " << lens;
      EXPECT_DOUBLE_EQ(el.min_value, -want_limit) << "lens " << lens;
    } else {
      // A disabled entry with nothing to say is worse than no entry: "Look At is grey and I do not
      // know why" is the confusion this string exists to prevent.
      EXPECT_NE(el.disabled_reason, nullptr) << "lens " << lens << " is gated but offers no reason";
    }
  };
  check_lens(lumice::gui::kLensTypeLinear, true, 90.0f);
  check_lens(lumice::gui::kLensTypeGlobe, true, 89.0f);
  check_lens(lumice::gui::kLensTypeDualFisheyeEqualArea, false, 0.0f);
}

// ---------------------------------------------------------------------------------------------
// AC2 — the pose a preset is allowed to produce.
//
// This is the ONLY layer at which the clamp is judgeable. The panel redraws the Az/El sliders every
// frame and SliderWithInput clamps unconditionally, so an unclamped preset write is repaired one
// frame later and the state a gui_test case reads is identical either way — a case there is unable
// to go red on this, whatever it asserts. Verified by removing the clamp and watching the panel-
// level case stay green.
//
// What the clamp buys is the frame in between, which the preview, the overlay cache and the export
// path all read: under Globe an unclamped Zenith would put them at the pole the registry backs one
// degree off from, and the slider would then snap the view away on the next frame — which reads as
// the preset having been ignored.
// ---------------------------------------------------------------------------------------------

TEST(ViewLookAtPose, TheGlobeBoundsPullThePolesInByADegree) {
  lumice::gui::GuiState state;
  state.renderer.lens_type = lumice::gui::kLensTypeGlobe;
  const lumice::gui::FieldEditorConstraint el_c = lumice::gui::ConstraintFor("renderer.elevation", state);
  const lumice::gui::FieldEditorConstraint az_c = lumice::gui::ConstraintFor("renderer.azimuth", state);

  float az = 0.0f;
  float el = 0.0f;
  ASSERT_TRUE(lumice::gui::ResolveLookAtPose(LookAtId::kZenith, 20.0f, el_c, az_c, &az, &el));
  EXPECT_FLOAT_EQ(el, 89.0f) << "Globe must not be handed a pose its own slider cannot express";
  ASSERT_TRUE(lumice::gui::ResolveLookAtPose(LookAtId::kNadir, 20.0f, el_c, az_c, &az, &el));
  EXPECT_FLOAT_EQ(el, -89.0f);

  // The unclamped answer is the pole itself — stated so the case above is visibly a clamp and not
  // an arithmetic that happened to land on 89.
  float raw_az = 0.0f;
  float raw_el = 0.0f;
  ASSERT_TRUE(lumice::gui::ResolveLookAtAzEl(LookAtId::kZenith, 20.0f, &raw_az, &raw_el));
  EXPECT_FLOAT_EQ(raw_el, 90.0f);
}

TEST(ViewLookAtPose, ALensWhoseBoundIsThePoleGetsThePole) {
  // The other half: an implementation that hard-coded 89, or clamped to some fixed interval of its
  // own, would pass the Globe case above and fail here.
  lumice::gui::GuiState state;
  state.renderer.lens_type = lumice::gui::kLensTypeLinear;
  const lumice::gui::FieldEditorConstraint el_c = lumice::gui::ConstraintFor("renderer.elevation", state);
  const lumice::gui::FieldEditorConstraint az_c = lumice::gui::ConstraintFor("renderer.azimuth", state);

  float az = 0.0f;
  float el = 0.0f;
  ASSERT_TRUE(lumice::gui::ResolveLookAtPose(LookAtId::kZenith, 20.0f, el_c, az_c, &az, &el));
  EXPECT_FLOAT_EQ(el, 90.0f);
  ASSERT_TRUE(lumice::gui::ResolveLookAtPose(LookAtId::kNadir, 20.0f, el_c, az_c, &az, &el));
  EXPECT_FLOAT_EQ(el, -90.0f);
}

TEST(ViewLookAtPose, EveryPresetLandsInsideTheBoundsUnderEveryLens) {
  // The claim the two cases above do not make: not "these two entries are clamped correctly" but
  // "no entry, under any lens, produces a pose outside the interval" — which is what stops a
  // direction added later from slipping through with no clamp of its own.
  lumice::gui::GuiState state;
  const auto check_lens = [&state](int lens) {
    state.renderer.lens_type = lens;
    const lumice::gui::FieldEditorConstraint el_c = lumice::gui::ConstraintFor("renderer.elevation", state);
    const lumice::gui::FieldEditorConstraint az_c = lumice::gui::ConstraintFor("renderer.azimuth", state);
    if (!el_c.has_numeric_domain || !az_c.has_numeric_domain) {
      return;  // a lens with no view angle at all; the entry is disabled there
    }
    for (int i = 0; i < static_cast<int>(LookAtId::kCount); ++i) {
      for (const float alt : { 40.0f, 0.0f, -25.0f, 90.0f }) {
        float az = 0.0f;
        float el = 0.0f;
        if (!lumice::gui::ResolveLookAtPose(static_cast<LookAtId>(i), alt, el_c, az_c, &az, &el)) {
          ADD_FAILURE() << "lens " << lens << " id " << i << " altitude " << alt << " was rejected";
          continue;
        }
        EXPECT_GE(el, static_cast<float>(el_c.min_value)) << "lens " << lens << " id " << i;
        EXPECT_LE(el, static_cast<float>(el_c.max_value)) << "lens " << lens << " id " << i;
        EXPECT_GE(az, static_cast<float>(az_c.min_value)) << "lens " << lens << " id " << i;
        EXPECT_LE(az, static_cast<float>(az_c.max_value)) << "lens " << lens << " id " << i;
      }
    }
  };
  for (const int lens : lumice::gui::kLensTypePresentationOrder) {
    check_lens(lens);
  }
}
