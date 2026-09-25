// "The channel-B-R display mode takes over the colour channel", asserted on the server/CLI side.
//
// The same rule doc/print-mode-subtractive-ink.md §7 states for print — a mode that takes over the
// colour channel says which colour fields it leaves unread, and nothing is dropped silently — applied
// to the diagnostic display mode. What differs from print, and why this is its own file rather than
// more cases in test_print_mode_colour_exclusions.cpp:
//
//   - under tone=print the mode itself is inert (print never computes R and B separately), so that
//     combination gets one notice naming the display mode, not per-field notices for a picture that
//     is not being drawn;
//   - annotation colours are NOT dropped: the diagnostic draws the overlays on top in their own
//     colours, so a coloured grid line must stay silent here (the opposite of print's instance 4);
//   - the raypath-colour composite is excluded, and the exclusion is cross-checked against the
//     notice on one real run each, for the reason the print file gives: two separately-passing
//     assertions would still permit "warned but did not exclude".

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>

#include "server/server.hpp"
#include "support/live_server.hpp"
#include "support/log_capture.hpp"
#include "support/scoped_result_frame.hpp"

namespace lumice {
namespace {

using test::LogCapture;

constexpr const char* kInertUnderPrintNotice = "display_mode=channel_br, which has no effect under";
constexpr const char* kCompositeNotice = "display_mode=channel_br, so the raypath-colour composite";
constexpr const char* kRayColorNotice = "sets ray_color under display_mode=channel_br";

bool Mentions(const std::string& haystack, const char* needle) {
  return haystack.find(needle) != std::string::npos;
}

// A minimal committable document, zero rays: every warning case is decided while reading the config.
nlohmann::json MakeBaseConfig() {
  nlohmann::json root;
  root["crystal"] =
      nlohmann::json::array({ { { "id", 1 }, { "type", "prism" }, { "shape", { { "height", 1.0f } } } } });
  root["filter"] = nlohmann::json::array();

  nlohmann::json scene;
  scene["light_source"] = {
    { "type", "sun" }, { "altitude", 20.0f }, { "azimuth", 0.0f }, { "diameter", 0.5f }, { "spectrum", "D65" }
  };
  scene["ray_num"] = 1000;
  scene["max_hits"] = 4;
  scene["scattering"] = nlohmann::json::array(
      { { { "prob", 0.0f }, { "entries", nlohmann::json::array({ { { "crystal", 1 }, { "proportion", 1.0f } } }) } } });
  root["scene"] = scene;

  nlohmann::json rn;
  rn["id"] = 1;
  rn["lens"] = { { "type", "fisheye_equal_area" }, { "fov", 180.0f } };
  rn["resolution"] = { 32, 32 };
  rn["view"] = { { "elevation", 0.0f }, { "azimuth", 0.0f }, { "roll", 0.0f } };
  rn["visible"] = "full";
  rn["background"] = { 0.0f, 0.0f, 0.0f };
  rn["intensity_factor"] = 1.0f;
  root["render"] = nlohmann::json::array({ rn });
  return root;
}

nlohmann::json WithColorClass(nlohmann::json root) {
  root["raypath_color"] = { { "mode", "dominant" },
                            { "classes", nlohmann::json::array({ { { "color", { 1.0f, 0.0f, 0.0f } },
                                                                   { "match", nlohmann::json::array({
                                                                                  { { "layer", 0 }, { "crystal", 1 } },
                                                                              }) } } }) } };
  return root;
}

nlohmann::json WithRayColor(nlohmann::json root) {
  root["render"][0]["ray_color"] = { 1.0f, 0.0f, 0.0f };
  return root;
}

nlohmann::json WithAnnotationColour(nlohmann::json root) {
  root["render"][0]["grid"]["elevation"] =
      nlohmann::json::array({ { { "value", 22.0f }, { "color", { 1.0f, 0.0f, 0.0f } } } });
  return root;
}

nlohmann::json WithDisplayMode(nlohmann::json root, const char* mode) {
  root["render"][0]["display_mode"] = mode;
  return root;
}

nlohmann::json WithTone(nlohmann::json root, const char* tone) {
  root["render"][0]["tone"] = tone;
  return root;
}

std::string CommitAndCaptureLog(const nlohmann::json& config) {
  LogCapture capture;
  Server server(1);
  EXPECT_TRUE(server.CommitConfig(config).IsSuccess());
  server.Terminate();
  return capture.Text();
}

}  // namespace

// Under print the one thing worth saying is that the mode does nothing — and only that: the
// per-field notices would describe a picture print is not drawing.
TEST(ChannelMathColourExclusions, UnderPrintTheModeSaysItIsInertAndNothingElse) {
  const std::string log = CommitAndCaptureLog(
      WithTone(WithRayColor(WithColorClass(WithDisplayMode(MakeBaseConfig(), "channel_br"))), "print"));
  EXPECT_TRUE(Mentions(log, kInertUnderPrintNotice)) << log;
  EXPECT_FALSE(Mentions(log, kCompositeNotice)) << log;
  EXPECT_FALSE(Mentions(log, kRayColorNotice)) << log;

  EXPECT_FALSE(Mentions(CommitAndCaptureLog(WithTone(MakeBaseConfig(), "print")), kInertUnderPrintNotice))
      << "print with display_mode normal has nothing inert to report";
  EXPECT_FALSE(Mentions(CommitAndCaptureLog(WithDisplayMode(MakeBaseConfig(), "channel_br")), kInertUnderPrintNotice))
      << "channel_br under screen is live";
}

// Scene-level class table AND this renderer in channel_br — either half alone stays silent.
TEST(ChannelMathColourExclusions, AColourClassConfigSaysTheCompositeIsDropped) {
  EXPECT_TRUE(
      Mentions(CommitAndCaptureLog(WithDisplayMode(WithColorClass(MakeBaseConfig()), "channel_br")), kCompositeNotice));

  EXPECT_FALSE(
      Mentions(CommitAndCaptureLog(WithDisplayMode(WithColorClass(MakeBaseConfig()), "normal")), kCompositeNotice));
  EXPECT_FALSE(Mentions(CommitAndCaptureLog(WithDisplayMode(MakeBaseConfig(), "channel_br")), kCompositeNotice));
}

// The {-1,-1,-1} sentinel is "no tint" and must stay silent.
TEST(ChannelMathColourExclusions, ARayColorSaysTheDiagnosticIsNotMeaningful) {
  EXPECT_TRUE(
      Mentions(CommitAndCaptureLog(WithDisplayMode(WithRayColor(MakeBaseConfig()), "channel_br")), kRayColorNotice));

  EXPECT_FALSE(
      Mentions(CommitAndCaptureLog(WithDisplayMode(WithRayColor(MakeBaseConfig()), "normal")), kRayColorNotice));
  EXPECT_FALSE(Mentions(CommitAndCaptureLog(WithDisplayMode(MakeBaseConfig(), "channel_br")), kRayColorNotice));
}

// The deliberate difference from print: the overlays keep their colours on the diagnostic image, so
// a coloured grid line is not a dropped field and must not be reported as one.
TEST(ChannelMathColourExclusions, AnAnnotationColourIsNotReported) {
  const std::string log = CommitAndCaptureLog(WithDisplayMode(WithAnnotationColour(MakeBaseConfig()), "channel_br"));
  EXPECT_EQ(log.find("channel_br"), std::string::npos) << log;
}

namespace {

std::string ColorSceneWithDisplayMode(const char* mode) {
  auto root = nlohmann::json::parse(test::kColorSceneJson);
  root["render"][0]["display_mode"] = mode;
  return root.dump();
}

bool RunAndHasComposite(const char* mode) {
  test::LiveServer server;
  EXPECT_TRUE(server.Run(ColorSceneWithDisplayMode(mode).c_str(), 20000));
  test::ScopedResultFrame frame(server.get());
  EXPECT_EQ(frame.err(), LUMICE_OK);
  LUMICE_RenderResult comp[LUMICE_MAX_RENDER_RESULTS + 1]{};
  EXPECT_EQ(LUMICE_FrameGetComposite(frame.get(), comp, LUMICE_MAX_RENDER_RESULTS), LUMICE_OK);
  return comp[0].img_buffer != nullptr;
}

}  // namespace

// The exclusion on a real run, cross-checked against the notice read off the same commit.
TEST(ChannelMathColourExclusions, TheCompositeIsExcludedAndTheNoticeAgreesWithIt) {
  bool normal_has_composite = false;
  bool channel_has_composite = true;
  std::string normal_log;
  std::string channel_log;
  {
    LogCapture capture;
    normal_has_composite = RunAndHasComposite("normal");
    normal_log = capture.Text();
  }
  {
    LogCapture capture;
    channel_has_composite = RunAndHasComposite("channel_br");
    channel_log = capture.Text();
  }

  EXPECT_TRUE(normal_has_composite) << "display_mode normal must still composite — the control arm";
  EXPECT_FALSE(channel_has_composite) << "channel_br must produce no composite for a colour-configured renderer";
  EXPECT_TRUE(Mentions(channel_log, kCompositeNotice)) << "the channel arm dropped the composite; it must say so";
  EXPECT_FALSE(Mentions(normal_log, kCompositeNotice)) << "the normal arm composited; it must not claim otherwise";
}

}  // namespace lumice
