#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "analytic/analytic_callback_sink.hpp"
#include "lumice_analytic.h"
#include "util/logger.hpp"

// liblumice_analytic goes silent by removing GetDefaultConsoleSink() from its copy of
// GetSharedSink() while it loads, and hands diagnostics to the host through AnalyticCallbackSink
// (src/analytic/analytic_lib.cpp, doc/analytic-api.md section 6). These cases drive the same two
// steps in this process, through the same LOG_WARNING macro the engine's real warnings use
// (crystal.cpp, geo3d_closedform.cpp): the library exposes no function that warns yet, so the
// mechanism is pinned here. That the library really runs the removal at load time is pinned at the
// .so boundary by test/e2e-correctness/test_analytic_log_sink.py.
//
// GetSharedSink() and the global logger are process-wide, so every case restores both before it
// returns, and uses EXPECT_* only so no failure can skip the restore.
//
// Each case asserts the mechanism twice: structurally (the default sink is detached from, then
// re-attached to, GetSharedSink()) on every platform, and by its output on stderr where that output
// is observable. On Windows it is not: spdlog's stderr colour sink there writes to the console
// HANDLE it fetched at construction (GetStdHandle), not to file descriptor 2, so gtest's
// CaptureStderr -- a redirect of fd 2 -- captures nothing either way.

namespace lumice {
namespace {

#if defined(_WIN32)
constexpr bool kConsoleSinkOutputCapturable = false;
#else
constexpr bool kConsoleSinkOutputCapturable = true;
#endif

bool DefaultConsoleSinkAttached() {
  const auto& sinks = GetSharedSink()->sinks();
  return std::find(sinks.begin(), sinks.end(), GetDefaultConsoleSink()) != sinks.end();
}

struct Received {
  LUMICE_ANALYTIC_LogLevel level;
  std::string logger_name;
  std::string message;
};

std::vector<Received>& Inbox() {
  static std::vector<Received> inbox;
  return inbox;
}

void RecordCallback(LUMICE_ANALYTIC_LogLevel level, const char* logger_name, const char* message) {
  Inbox().push_back({ level, logger_name, message });
}

// Pins the global logger at its default level for the case, whatever an earlier case set.
class ScopedDefaultLevel {
 public:
  ScopedDefaultLevel() : saved_(GetGlobalLogger().GetSpdLogger()->level()) {
    GetGlobalLogger().SetLevel(LogLevel::kInfo);
  }
  ~ScopedDefaultLevel() { GetGlobalLogger().GetSpdLogger()->set_level(saved_); }

 private:
  spdlog::level::level_enum saved_;
};

std::string WarnAndCaptureStderr(const std::string& text) {
  testing::internal::CaptureStderr();
  LOG_WARNING("{}", text);
  return testing::internal::GetCapturedStderr();
}

TEST(DefaultConsoleSinkRemoval, RemovingItSilencesAWarning) {
  ScopedDefaultLevel level;

  // Baseline: the warning path is live and reaches stderr, so the silence below is not an empty call.
  EXPECT_TRUE(DefaultConsoleSinkAttached());
  std::string before = WarnAndCaptureStderr("console-sink baseline");
  if (kConsoleSinkOutputCapturable) {
    EXPECT_NE(before.find("console-sink baseline"), std::string::npos) << "stderr was: " << before;
  }

  GetSharedSink()->remove_sink(GetDefaultConsoleSink());
  const bool attached_while_removed = DefaultConsoleSinkAttached();
  std::string after = WarnAndCaptureStderr("console-sink removed");
  GetSharedSink()->add_sink(GetDefaultConsoleSink());

  EXPECT_FALSE(attached_while_removed);
  EXPECT_TRUE(DefaultConsoleSinkAttached());
  if (kConsoleSinkOutputCapturable) {
    EXPECT_EQ(after, "");
  }
}

TEST(DefaultConsoleSinkRemoval, CallbackReceivesTheWarningAtWarningLevel) {
  ScopedDefaultLevel level;
  Inbox().clear();

  auto sink = std::make_shared<analytic::AnalyticCallbackSink>();
  sink->set_formatter(CreateLumiceFormatter(kLogPattern));
  sink->SetCallback(&RecordCallback);
  GetSharedSink()->remove_sink(GetDefaultConsoleSink());
  GetSharedSink()->add_sink(sink);
  const bool attached_while_removed = DefaultConsoleSinkAttached();

  std::string console = WarnAndCaptureStderr("crystal is degenerate");

  sink->SetCallback(nullptr);
  std::string after_null = WarnAndCaptureStderr("after NULL callback");

  GetSharedSink()->remove_sink(sink);
  GetSharedSink()->add_sink(GetDefaultConsoleSink());

  EXPECT_FALSE(attached_while_removed);
  EXPECT_TRUE(DefaultConsoleSinkAttached());
  if (kConsoleSinkOutputCapturable) {
    EXPECT_EQ(console, "");
    EXPECT_EQ(after_null, "");
  }
  ASSERT_EQ(Inbox().size(), 1u);  // fatal only after the restore above
  const Received& r = Inbox()[0];
  // LOG_WARNING maps to spdlog::err (Logger::ToSpdLevel), numerically LUMICE_ANALYTIC_LOG_WARNING.
  EXPECT_EQ(r.level, LUMICE_ANALYTIC_LOG_WARNING);
  EXPECT_EQ(r.logger_name, "global");
  EXPECT_NE(r.message.find("[W] crystal is degenerate"), std::string::npos) << r.message;
  EXPECT_NE(r.message.back(), '\n');
}

}  // namespace
}  // namespace lumice
