// The diagnostics recorder and report assembly (src/gui/diagnostics_report.*).
//
// What this file owns: everything that is pure over its inputs. The report's
// honesty rules are the propositions — plan §6.1's first four rows at their
// cheapest sufficient layer:
//   1. The JSON parses and carries schema v1 with the promised sections; the
//      two sides of "absent" (field not obtained vs. nothing to report) are
//      distinguishable in the shape itself.
//   2. Missing inputs are absent or null with a note, never zero, never an
//      empty string wearing a value's clothes — a maintainer must not be able
//      to mistake "not collected" for a real reading.
//   3. The upload-event taxonomy round-trips: every outcome, every GL-error
//      check state, the entry-residue split, and the queried/not-queried
//      actual-texture block spell out exactly what the collector was told.
//   4. Privacy by whitelist: the only free-text that can enter the default
//      report is the log section, which exists if and only if the caller opted
//      in — a sentinel planted in a log line appears exactly then and never
//      otherwise, and no structural key anywhere can carry it.
//   5. The retention latch: first failure survives later successes, the
//      latest attempt updates, intermediate attempts are counted as dropped,
//      and a document switch can never make an old event pose as the current
//      document's.
//
// Deliberately NOT here: anything needing GL (the real upload chain witnesses
// live in gui_test/functional/test_diagnostics_export.cpp) and anything
// needing ResetFrontendState's unit interactions (composition-correctness/gui/
// test_diagnostics_context_chain.cpp). Platform collectors are never called in
// this binary — the tests feed synthetic values.

#include <gtest/gtest.h>

#if !defined(_WIN32)
#include <sys/resource.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <cstring>
#endif

#include <cmath>
#include <limits>
#include <nlohmann/json.hpp>
#include <string>

#include "gui/diagnostics_report.hpp"
#include "gui/file_io.hpp"

namespace lumice::gui {
namespace {

using namespace lumice::gui::diagnostics;
using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Fixtures. One valid input, minimal but real-shaped; individual tests mutate
// what they probe.
// ---------------------------------------------------------------------------

FrozenViewSettings MakeSettings() {
  FrozenViewSettings s;
  s.resolution_tier = 4096;
  s.lens_type = 1;  // Fisheye Equal Area
  s.fov = 180.0f;
  s.ray_num_millions = 5.0f;
  s.max_hits = 8;
  s.ray_num_infinite = false;
  s.ray_allocation_adaptive = true;
  return s;
}

UploadEventContext MakeContext(UploadProvenance provenance = UploadProvenance::kLive) {
  UploadEventContext ctx;
  ctx.provenance = provenance;
  ctx.source_width = 1024;
  ctx.source_height = 512;
  ctx.settings = MakeSettings();
  if (provenance == UploadProvenance::kLive) {
    ctx.run_epoch = 7ULL;
    ctx.texture_serial = 42ULL;
  }
  return ctx;
}

UploadFacts MakeCompletedFacts() {
  UploadFacts facts;
  facts.path = UploadPath::kXyz;
  facts.outcome = UploadOutcome::kCompleted;
  facts.requested_width = 1024;
  facts.requested_height = 512;
  facts.pbo_bytes = static_cast<size_t>(1024) * 512 * 3 * 2;
  facts.xyz_half_scale = 0.5f;
  facts.gl_error_state = GlErrorCheckState::kNone;
  facts.actual_queried = true;
  facts.actual_width = 1024;
  facts.actual_height = 512;
  facts.actual_internal_format = 0x88B1;  // GL_RGB16F
  return facts;
}

DiagnosticsExportInput MakeInput() {
  DiagnosticsExportInput input;
  input.environment.app_version = "4.7.7-test";
  input.environment.engine_isa_level = "avx2";
  input.platform_info.os_name = "TestOS";
  input.platform_info.os_version = "1.2.3";
  input.platform_info.cpu_model = "Test CPU";
  input.platform_info.total_physical_memory_bytes = 123456789ULL;
  input.gl_info.vendor = "TestVendor";
  input.gl_info.renderer = "TestRenderer";
  input.gl_info.version = "3.3 TestDriver 999";
  input.gl_info.max_texture_size = 16384;
  input.route.preferred_backend = "Metal";
  input.route.active_backend = "CPU";
  input.route.backend_fell_back_mid_run = true;
  input.current.document_serial = 3;
  input.current.server_generation = 5;
  input.current.origin = DocumentOrigin::kReopened;
  input.current.has_texture = true;
  input.current.texture_mode = "xyz";
  input.current.display_width = 1024;
  input.current.display_height = 512;
  input.current.committed_epoch = 7ULL;
  input.current.last_uploaded_texture_serial = 42ULL;
  input.current.texture_upload_count = 4;
  input.current.settings_at_export = MakeSettings();
  return input;
}

// Drive the collector through one closed event.
void DriveUpload(DiagnosticsCollector& c, const UploadEventContext& ctx, const UploadFacts& facts) {
  c.BeginUpload(ctx);
  c.CompleteUpload(facts);
}

// A stage failed without leaving an error flag (the null-map shape): the
// flag was checked at that boundary and stood clean, the failure is real and
// non-GL, the stage name is the report's fact — and the staging request the
// attempt had already frozen rides along (the plan existed before the GL
// work; a failure must still say what it asked the driver for).
UploadFacts FailedMapFacts() {
  UploadFacts facts;
  facts.path = UploadPath::kXyz;
  facts.outcome = UploadOutcome::kFailedDisplayRevoked;
  facts.requested_width = 8192;
  facts.requested_height = 4096;
  facts.pbo_bytes = static_cast<size_t>(8192) * 4096 * 3 * 2;
  facts.gl_error_state = GlErrorCheckState::kNone;
  facts.error_stage = "pbo-map";
  return facts;
}

// ---------------------------------------------------------------------------
// 1. Schema: parses, carries v1, sections present, absent-vs-empty distinct.
// ---------------------------------------------------------------------------

TEST(DiagnosticsReportSchema, ParsesAndCarriesVersionOne) {
  const std::string text = BuildDiagnosticsJson(MakeInput(), nullptr);
  ASSERT_NO_THROW({ json parsed = json::parse(text); });
  const json parsed = json::parse(text);

  ASSERT_TRUE(parsed.contains("schema_version"));
  EXPECT_EQ(parsed["schema_version"], 1);
  EXPECT_TRUE(parsed.contains("exported_at"));
  EXPECT_TRUE(parsed["exported_at"].is_string());

  // The sections the maintainer's triage walks, all present on a full input.
  for (const char* key :
       { "application", "platform", "graphics_context", "simulation_route", "current_document", "upload_history" }) {
    EXPECT_TRUE(parsed.contains(key)) << key;
  }

  // Absent log opt-in: the KEY is absent, which is how "the user chose not to
  // attach" reads — an empty array would mean "opted in, ring was empty".
  EXPECT_FALSE(parsed.contains("log_tail"));

  // The export destination path is never embedded (plan §3.3).
  EXPECT_EQ(text.find("lumice_diagnostics.json"), std::string::npos);
}

TEST(DiagnosticsReportSchema, LogOptInMakesTheSectionPresentAndRoundTrips) {
  DiagnosticsExportInput input = MakeInput();
  std::vector<LogLine> lines = { { "warning", "sentinel-log-line-816" }, { "info", "another line" } };

  const json parsed = json::parse(BuildDiagnosticsJson(input, &lines));
  ASSERT_TRUE(parsed.contains("log_tail"));
  ASSERT_EQ(parsed["log_tail"].size(), 2U);
  EXPECT_EQ(parsed["log_tail"][0]["level"], "warning");
  EXPECT_EQ(parsed["log_tail"][0]["message"], "sentinel-log-line-816");
}

TEST(DiagnosticsReportSchema, TimestampsAreIso8601UtcWithMilliseconds) {
  // 2026-11-22 04:05:06.789 UTC, constructed independently of "now" so the
  // formatting is pinned, not just shape-checked.
  std::tm tm_utc{};
  tm_utc.tm_year = 126;  // 2026 - 1900
  tm_utc.tm_mon = 10;    // November
  tm_utc.tm_mday = 22;
  tm_utc.tm_hour = 4;
  tm_utc.tm_min = 5;
  tm_utc.tm_sec = 6;
  tm_utc.tm_isdst = 0;
#ifdef _WIN32
  const std::time_t epoch = _mkgmtime(&tm_utc);
#else
  const std::time_t epoch = timegm(&tm_utc);
#endif
  const auto tp = std::chrono::system_clock::from_time_t(epoch) + std::chrono::milliseconds(789);

  EXPECT_EQ(FormatIso8601Utc(tp), "2026-11-22T04:05:06.789Z");
}

// ---------------------------------------------------------------------------
// 2. Missing-state honesty.
// ---------------------------------------------------------------------------

TEST(DiagnosticsReportMissing, EmptyPlatformReadsAsNullWithNotesNotZero) {
  DiagnosticsExportInput input = MakeInput();
  input.platform_info = PlatformInfo{};  // nothing collected
  input.platform_info.notes.push_back("cpu_model: source unavailable");

  const json parsed = json::parse(BuildDiagnosticsJson(input, nullptr));
  EXPECT_TRUE(parsed["platform"]["os_name"].is_null());
  EXPECT_TRUE(parsed["platform"]["cpu_model"].is_null());
  EXPECT_EQ(parsed["platform"]["cpu_model"], nullptr);  // not 0, not ""
  ASSERT_EQ(parsed["platform"]["collection_notes"].size(), 1U);
  EXPECT_EQ(parsed["platform"]["collection_notes"][0], "cpu_model: source unavailable");
}

TEST(DiagnosticsReportMissing, AbsentGlAndRouteFieldsAreNullAndDoNotHideTheRest) {
  DiagnosticsExportInput input = MakeInput();
  input.gl_info = GlInfo{};
  input.gl_info.notes.push_back("gl_vendor: glGetString returned null (no bound context?)");
  input.route = SimRouteInfo{};  // no server

  const json parsed = json::parse(BuildDiagnosticsJson(input, nullptr));
  EXPECT_TRUE(parsed["graphics_context"]["gl_vendor"].is_null());
  EXPECT_TRUE(parsed["graphics_context"]["gl_max_texture_size"].is_null());
  EXPECT_TRUE(parsed["simulation_route"]["active_backend"].is_null());
  EXPECT_TRUE(parsed["simulation_route"]["preferred_backend"].is_null());
  // What WAS available survives beside the gaps.
  EXPECT_EQ(parsed["application"]["version"], "4.7.7-test");
  EXPECT_EQ(parsed["graphics_context"]["gl_version"], nullptr);
}

TEST(DiagnosticsReportMissing, NonFiniteFloatsDoNotEnterTheReportAsNumbers) {
  DiagnosticsExportInput input = MakeInput();
  input.current.settings_at_export.fov = std::numeric_limits<float>::quiet_NaN();

  const json parsed = json::parse(BuildDiagnosticsJson(input, nullptr));
  const json& fov = parsed["current_document"]["view_settings_at_export"]["fov"];
  ASSERT_TRUE(fov.is_object());
  EXPECT_TRUE(fov["value"].is_null());
  EXPECT_EQ(fov["state"], "non_finite");
}

TEST(DiagnosticsReportMissing, UploadEventWithoutHistoryReadsAsExplicitNulls) {
  DiagnosticsExportInput input = MakeInput();
  input.history = UploadHistory{};
  input.current_document_has_upload = false;

  const json parsed = json::parse(BuildDiagnosticsJson(input, nullptr));
  EXPECT_TRUE(parsed["upload_history"]["first_failure"].is_null());
  EXPECT_TRUE(parsed["upload_history"]["last_failure"].is_null());
  EXPECT_TRUE(parsed["upload_history"]["last_upload"].is_null());
  EXPECT_EQ(parsed["upload_history"]["dropped_event_count"], 0);
  EXPECT_EQ(parsed["current_document"]["current_document_has_upload_attempt"], false);
}

// ---------------------------------------------------------------------------
// 3. The event taxonomy round-trips.
// ---------------------------------------------------------------------------

TEST(DiagnosticsEventTaxonomy, CompletedXyzEventCarriesEveryFactsGroup) {
  DiagnosticsCollector c;
  DriveUpload(c, MakeContext(), MakeCompletedFacts());

  DiagnosticsExportInput input = MakeInput();
  input.history = c.history();

  const json parsed = json::parse(BuildDiagnosticsJson(input, nullptr));
  const json& ev = parsed["upload_history"]["last_upload"];
  EXPECT_EQ(ev["provenance"], "live");
  EXPECT_EQ(ev["run_epoch"], 7ULL);
  EXPECT_EQ(ev["texture_serial"], 42ULL);
  EXPECT_EQ(ev["source"]["width"], 1024);
  EXPECT_EQ(ev["source"]["height"], 512);
  EXPECT_EQ(ev["path"], "xyz");
  EXPECT_EQ(ev["outcome"], "completed");
  EXPECT_EQ(ev["gl_error"]["state"], "none");
  EXPECT_EQ(ev["requested"]["width"], 1024);
  EXPECT_EQ(ev["requested"]["pbo_bytes"].get<unsigned long long>(), 1024ULL * 512 * 3 * 2);
  ASSERT_TRUE(ev["actual_texture"].is_object());
  EXPECT_EQ(ev["actual_texture"]["state"], "queried");
  EXPECT_EQ(ev["actual_texture"]["width"], 1024);
  EXPECT_EQ(ev["actual_texture"]["internal_format_hex"], "0x88B1");
  EXPECT_EQ(ev["view_settings_at_upload"]["resolution_tier"], 4096);
  // Fields the witness context did not carry read as explicit nulls, never as
  // fabricated zeros; the generation counts from the first server instance.
  EXPECT_TRUE(ev["bundle_epoch"].is_null());
  EXPECT_TRUE(ev["active_backend_at_upload"].is_null());
  EXPECT_TRUE(ev["error_stage"].is_null());
  EXPECT_TRUE(ev["fence_create_failed"].is_null());
  EXPECT_EQ(ev["server_generation"], 1ULL);
  EXPECT_EQ(ev["display_commit"], "unknown");  // caller had not attached yet
}

TEST(DiagnosticsEventTaxonomy, ReportedGlErrorCarriesTheCodeAndCountsAsFailure) {
  UploadFacts facts = MakeCompletedFacts();
  facts.gl_error_state = GlErrorCheckState::kReported;
  facts.gl_error_code = 0x501;  // GL_INVALID_VALUE
  facts.error_stage = "tex-update";

  DiagnosticsCollector c;
  DriveUpload(c, MakeContext(), facts);

  DiagnosticsExportInput input = MakeInput();
  input.history = c.history();
  const json parsed = json::parse(BuildDiagnosticsJson(input, nullptr));
  const json& ev = parsed["upload_history"]["first_failure"];
  EXPECT_EQ(ev["gl_error"]["state"], "reported");
  EXPECT_EQ(ev["gl_error"]["code_hex"], "0x0501");
  EXPECT_EQ(ev["error_stage"], "tex-update");
  EXPECT_EQ(parsed["upload_history"]["last_failure"], ev);  // first and last agree on one event
}

TEST(DiagnosticsEventTaxonomy, ServerGenerationNamespacesEpochsAcrossReconstruction) {
  DiagnosticsCollector c;
  DriveUpload(c, MakeContext(), MakeCompletedFacts());
  ASSERT_TRUE(c.history().last_upload.has_value());
  const unsigned long long first_generation = c.history().last_upload->server_generation;

  // A server reconstruction (backend/worker swap) restarts the epoch authority
  // at 0 inside the same document; the bumped generation is what keeps a
  // pre-swap run distinguishable from a post-swap one.
  c.OnServerReconstructed();
  EXPECT_EQ(c.server_generation(), first_generation + 1);
  DriveUpload(c, MakeContext(), MakeCompletedFacts());
  ASSERT_TRUE(c.history().last_upload.has_value());
  EXPECT_EQ(c.history().last_upload->server_generation, first_generation + 1);
}

TEST(DiagnosticsEventTaxonomy, FenceFailureRidesCompletedOutcomeAndStaysRetainedAfterASuccess) {
  // The r4-counted retention gap, pinned at the module level: a completed
  // frame whose fence creation failed is an observed failure fact, so (a)
  // the event itself latches into the failure slots, and (b) a LATER
  // successful upload cannot erase it from the retention — the frame is
  // displayed, but the torn-down slot's next write runs unguarded, and that
  // is exactly the fact a hardware report must outlive the recovery.
  DiagnosticsCollector c;
  UploadFacts fence_fail = MakeCompletedFacts();
  fence_fail.fence_create_failed = true;  // glFenceSync returned null after the commit
  DriveUpload(c, MakeContext(), fence_fail);

  const UploadHistory& h = c.history();
  ASSERT_TRUE(h.first_failure.has_value());  // completed + fence fact = a failure for the retention
  EXPECT_EQ(h.first_failure->facts.fence_create_failed, true);
  EXPECT_EQ(h.last_failure->facts.fence_create_failed, true);
  {
    UploadEvent event;
    event.facts = fence_fail;
    EXPECT_TRUE(IsUploadFailure(event));
  }

  // A later successful upload becomes last_upload but must not overwrite the
  // failure slots: the fence fact survives the success it rode in on.
  UploadFacts good = MakeCompletedFacts();
  good.requested_width = 512;
  good.fence_create_failed = std::nullopt;
  DriveUpload(c, MakeContext(), good);
  ASSERT_TRUE(h.last_upload.has_value());
  EXPECT_EQ(h.last_upload->facts.outcome, UploadOutcome::kCompleted);
  EXPECT_FALSE(h.last_upload->facts.fence_create_failed.value_or(false));
  ASSERT_TRUE(h.first_failure.has_value());
  EXPECT_EQ(h.first_failure->facts.fence_create_failed, true);
  ASSERT_TRUE(h.last_failure.has_value());
  EXPECT_EQ(h.last_failure->facts.fence_create_failed, true);

  // And the exported report carries all three sides of the story.
  DiagnosticsExportInput input = MakeInput();
  input.history = c.history();
  input.current_document_has_upload = true;
  const json parsed = json::parse(BuildDiagnosticsJson(input, nullptr));
  EXPECT_EQ(parsed["upload_history"]["first_failure"]["fence_create_failed"], true);
  EXPECT_EQ(parsed["upload_history"]["last_failure"]["fence_create_failed"], true);
  EXPECT_EQ(parsed["upload_history"]["last_upload"]["outcome"], "completed");
  EXPECT_TRUE(parsed["upload_history"]["last_upload"]["fence_create_failed"].is_null());
  // The fence-failed event keeps its completed outcome: the frame itself is
  // displayed; the fence fact rides beside, not instead of, it.
  EXPECT_EQ(parsed["upload_history"]["first_failure"]["outcome"], "completed");
}

TEST(DiagnosticsEventTaxonomy, SyncStatusIsRecordedAsReceivedNeverAsAGlError) {
  // The fence-wait branch reports whichever its read produced; a timeout is
  // the sync STATUS, not a driver error, and the report must keep the two
  // apart instead of mislabeling it.
  UploadFacts facts = MakeCompletedFacts();
  facts.outcome = UploadOutcome::kFailedFrameRetained;
  facts.gl_error_state = GlErrorCheckState::kSyncStatus;
  facts.gl_error_code = 0x1905;  // GL_TIMEOUT_EXPIRED, the sync call's return
  facts.error_stage = "fence-wait";
  facts.pbo_bytes = 768U;

  DiagnosticsCollector c;
  DriveUpload(c, MakeContext(), facts);
  DiagnosticsExportInput input = MakeInput();
  input.history = c.history();
  const json parsed = json::parse(BuildDiagnosticsJson(input, nullptr));
  const json& ev = parsed["upload_history"]["first_failure"];
  EXPECT_EQ(ev["gl_error"]["state"], "sync_status");
  EXPECT_EQ(ev["gl_error"]["code_hex"], "0x1905");
  EXPECT_EQ(ev["error_stage"], "fence-wait");
}

TEST(DiagnosticsEventTaxonomy, CompletedFenceFailureCarriesItsOwnErrorCodeAndStage) {
  // A completed frame whose fence creation stood behind a driver error: the
  // completed outcome must not paint the code over as checked-clean — the
  // fence branch read it, and it rides with the fence-create stage name.
  UploadFacts facts = MakeCompletedFacts();
  facts.fence_create_failed = true;
  facts.gl_error_state = GlErrorCheckState::kReported;
  facts.gl_error_code = 0x502;  // GL_INVALID_OPERATION, read by the fence branch
  facts.error_stage = "fence-create";

  DiagnosticsCollector c;
  DriveUpload(c, MakeContext(), facts);
  DiagnosticsExportInput input = MakeInput();
  input.history = c.history();
  const json parsed = json::parse(BuildDiagnosticsJson(input, nullptr));
  const json& ev = parsed["upload_history"]["first_failure"];
  EXPECT_EQ(ev["outcome"], "completed");
  EXPECT_EQ(ev["gl_error"]["state"], "reported");
  EXPECT_EQ(ev["gl_error"]["code_hex"], "0x0502");
  EXPECT_EQ(ev["error_stage"], "fence-create");
  EXPECT_EQ(ev["fence_create_failed"], true);
}

TEST(DiagnosticsReportSchema, NonUtf8BytesDegradeInsteadOfThrowing) {
  DiagnosticsExportInput input = MakeInput();
  std::vector<LogLine> lines;
  // 0xB0 0xAE is a lone continuation byte — invalid UTF-8 a raw log line can
  // absolutely carry. The export must not throw; the text survives readably.
  lines.push_back({ "info", std::string("sentinel before \xB0\xAE after") });

  std::string text;
  ASSERT_NO_THROW({ text = BuildDiagnosticsJson(input, &lines); });
  ASSERT_FALSE(text.empty());
  const json parsed = json::parse(text);
  bool sentinel_found = false;
  for (const auto& line : parsed["log_tail"]) {
    if (line["message"].get<std::string>().find("sentinel before") != std::string::npos) {
      sentinel_found = true;
    }
  }
  EXPECT_TRUE(sentinel_found);
}

TEST(DiagnosticsEventTaxonomy, StageFailureWithoutGlErrorCarriesTheStageAndCountsAsAFailure) {
  DiagnosticsCollector c;
  DriveUpload(c, MakeContext(), FailedMapFacts());

  DiagnosticsExportInput input = MakeInput();
  input.history = c.history();
  const json parsed = json::parse(BuildDiagnosticsJson(input, nullptr));
  const json& ev = parsed["upload_history"]["last_upload"];
  EXPECT_EQ(ev["outcome"], "failed_display_revoked");
  // The flag was checked at the failing boundary and stood clean: the
  // failure is real and non-GL, named by its stage — never spun into
  // "no error, so success".
  EXPECT_EQ(ev["gl_error"]["state"], "none");
  EXPECT_EQ(ev["error_stage"], "pbo-map");
  ASSERT_TRUE(ev["actual_texture"].is_object());
  EXPECT_EQ(ev["actual_texture"]["state"], "not_queried");
  // The attempt froze its staging request before any GL work: even a failure
  // reports what it asked the driver for (the plan predated the failing map).
  EXPECT_EQ(ev["requested"]["pbo_bytes"].get<unsigned long long>(), 8192ULL * 4096 * 3 * 2);
}

TEST(DiagnosticsEventTaxonomy, ReopenEventCarriesNoRunIdentityAndRadiancePath) {
  UploadFacts facts = MakeCompletedFacts();
  facts.path = UploadPath::kSrgbRadiance;
  facts.pbo_bytes = std::nullopt;  // the uint8 path has no PBO
  facts.xyz_half_scale = std::nullopt;
  // Synthetic not-checked facts: serialization must spell them honestly. (In
  // the product only the invalid-input rejection produces this state now.)
  facts.gl_error_state = GlErrorCheckState::kNotChecked;

  DiagnosticsCollector c;
  DriveUpload(c, MakeContext(UploadProvenance::kReopen), facts);

  DiagnosticsExportInput input = MakeInput();
  input.history = c.history();
  const json parsed = json::parse(BuildDiagnosticsJson(input, nullptr));
  const json& ev = parsed["upload_history"]["last_upload"];
  EXPECT_EQ(ev["provenance"], "reopen");
  EXPECT_TRUE(ev["run_epoch"].is_null());
  EXPECT_TRUE(ev["texture_serial"].is_null());
  EXPECT_EQ(ev["path"], "srgb_radiance");
  EXPECT_EQ(ev["gl_error"]["state"], "not_checked");
  EXPECT_EQ(ev["requested"]["pbo_bytes"], nullptr);
  EXPECT_FALSE(ev.contains("xyz_half_scale"));
}

// ---------------------------------------------------------------------------
// 4. Privacy: the whitelist is structural — only the log section carries
// free text, and only on opt-in.
// ---------------------------------------------------------------------------

TEST(DiagnosticsReportPrivacy, SentinelLogTextAppearsOnlyUnderExplicitOptIn) {
  const char* const kSentinel = "sentinel-private-path-816 /Users/someone/secret.lmc";

  DiagnosticsCollector c;
  UploadFacts facts = MakeCompletedFacts();
  facts.gl_error_state = GlErrorCheckState::kReported;
  facts.gl_error_code = 0x505;
  DriveUpload(c, MakeContext(), facts);

  DiagnosticsExportInput input = MakeInput();
  input.history = c.history();

  // Default: no log section anywhere, and the sentinel — planted ONLY in the
  // log lines — must not leak through any other string field.
  const std::string default_text = BuildDiagnosticsJson(input, nullptr);
  EXPECT_EQ(default_text.find("sentinel-private-path-816"), std::string::npos);
  const json default_parsed = json::parse(default_text);
  EXPECT_FALSE(default_parsed.contains("log_tail"));

  std::vector<LogLine> lines = { { "warning", kSentinel } };
  const json opted_in = json::parse(BuildDiagnosticsJson(input, &lines));
  ASSERT_TRUE(opted_in.contains("log_tail"));
  EXPECT_EQ(opted_in["log_tail"][0]["message"], kSentinel);
}

TEST(DiagnosticsReportPrivacy, NoStructuralKeyCanCarryAPathOrHostName) {
  // The whitelist is a fixed field set; this pins its top-level shape so a
  // future "helpful" addition of a path-bearing key fails here before it
  // ships. The values are also real-shaped: serials and sizes, not strings.
  DiagnosticsExportInput input = MakeInput();
  const json parsed = json::parse(BuildDiagnosticsJson(input, nullptr));

  for (const char* forbidden : { "path", "file", "file_path", "document_path", "host", "hostname", "user", "username",
                                 "machine_name", "env", "environment_variables" }) {
    EXPECT_FALSE(parsed.contains(forbidden)) << forbidden;
  }
  // The identity fields that DO exist are anonymous numbers.
  EXPECT_EQ(parsed["current_document"]["document_serial"], 3);
  EXPECT_EQ(parsed["current_document"]["server_generation"], 5ULL);
  EXPECT_TRUE(parsed["current_document"]["display"].is_object());
}

TEST(DiagnosticsRetention, CurrentAndEventServerGenerationsShipSideBySide) {
  // The reader-facing pin for server reconstruction: a failure left under
  // generation 1, then a worker swap (epochs restart at 0), then an export
  // before any new upload — the current block carries generation 2 while the
  // retained event carries its own 1, so the reader can tell the old
  // failure apart from the current run even though both documents match and
  // the epoch numbers collide.
  DiagnosticsCollector c;
  DriveUpload(c, MakeContext(), MakeCompletedFacts());
  ASSERT_TRUE(c.history().last_upload.has_value());
  EXPECT_EQ(c.history().last_upload->server_generation, 1ULL);
  c.OnServerReconstructed();

  DiagnosticsExportInput input = MakeInput();
  input.current.server_generation = c.server_generation();
  input.history = c.history();
  const json parsed = json::parse(BuildDiagnosticsJson(input, nullptr));
  EXPECT_EQ(parsed["current_document"]["server_generation"], 2ULL);
  EXPECT_EQ(parsed["upload_history"]["last_upload"]["server_generation"], 1ULL);
}

TEST(DiagnosticsRetention, UngatheredServerGenerationReadsAsNullNotZero) {
  DiagnosticsExportInput input = MakeInput();
  input.current.server_generation = 0;  // not gathered
  const json parsed = json::parse(BuildDiagnosticsJson(input, nullptr));
  EXPECT_TRUE(parsed["current_document"]["server_generation"].is_null());
}

// ---------------------------------------------------------------------------
// 5. The retention latch and document identity.
// ---------------------------------------------------------------------------

TEST(DiagnosticsRetention, FirstFailureSurvivesALaterSuccess) {
  DiagnosticsCollector c;

  DriveUpload(c, MakeContext(), FailedMapFacts());  // the failure
  UploadFacts good = MakeCompletedFacts();
  good.requested_width = 512;
  DriveUpload(c, MakeContext(), good);  // later success

  const UploadHistory& h = c.history();
  ASSERT_TRUE(h.first_failure.has_value());
  EXPECT_EQ(h.first_failure->facts.outcome, UploadOutcome::kFailedDisplayRevoked);
  ASSERT_TRUE(h.last_upload.has_value());
  EXPECT_EQ(h.last_upload->facts.outcome, UploadOutcome::kCompleted);
  EXPECT_EQ(h.last_failure->facts.outcome, UploadOutcome::kFailedDisplayRevoked);  // still the only failure
  EXPECT_EQ(DroppedEventCount(h), 0U);                                             // both attempts are represented
}

TEST(DiagnosticsRetention, SecondFailureKeepsFirstAndUpdatesLast) {
  DiagnosticsCollector c;
  DriveUpload(c, MakeContext(), FailedMapFacts());
  UploadFacts retained_fail = FailedMapFacts();
  retained_fail.outcome = UploadOutcome::kFailedFrameRetained;
  retained_fail.error_stage = "tex-update";
  retained_fail.gl_error_state = GlErrorCheckState::kReported;
  retained_fail.gl_error_code = 0x501;
  retained_fail.requested_width = 4096;
  DriveUpload(c, MakeContext(), retained_fail);

  EXPECT_EQ(c.history().first_failure->facts.outcome, UploadOutcome::kFailedDisplayRevoked);
  EXPECT_EQ(c.history().last_failure->facts.outcome, UploadOutcome::kFailedFrameRetained);
  EXPECT_EQ(c.history().last_failure->facts.requested_width, 4096);
}

TEST(DiagnosticsRetention, IntermediateAttemptsAreDroppedAndCounted) {
  DiagnosticsCollector c;
  for (int i = 0; i < 5; ++i) {
    UploadFacts good = MakeCompletedFacts();
    good.requested_width = 100 + i;
    DriveUpload(c, MakeContext(), good);
  }
  // Five same-outcome attempts, one summary slot: four are gone, and the
  // report's dropped count says so instead of reading as a complete record.
  EXPECT_EQ(c.history().total_event_count, 5U);
  EXPECT_EQ(c.history().last_upload->facts.requested_width, 104);
  EXPECT_EQ(DroppedEventCount(c.history()), 4U);

  // And the assembly prints it.
  DiagnosticsExportInput input = MakeInput();
  input.history = c.history();
  input.current_document_has_upload = true;
  const json parsed = json::parse(BuildDiagnosticsJson(input, nullptr));
  EXPECT_EQ(parsed["upload_history"]["dropped_event_count"], 4);
}

TEST(DiagnosticsRetention, BeginWithoutUploadRecordsNothing) {
  DiagnosticsCollector c;
  c.BeginUpload(MakeContext());  // caller began but never drove an upload
  DriveUpload(c, MakeContext(), MakeCompletedFacts());

  // The stale slot is discarded without a count — it never became an
  // attempt — and the real attempt lands as the only recorded event.
  EXPECT_EQ(c.history().total_event_count, 1U);
  EXPECT_EQ(DroppedEventCount(c.history()), 0U);
  EXPECT_EQ(c.history().last_upload->facts.outcome, UploadOutcome::kCompleted);
}

TEST(DiagnosticsRetention, CompleteWithoutBeginProducesNoEvent) {
  DiagnosticsCollector c;
  c.CompleteUpload(MakeCompletedFacts());  // no context was frozen
  EXPECT_FALSE(c.history().last_upload.has_value());
  EXPECT_FALSE(c.current_document_has_upload());
}

TEST(DiagnosticsRetention, DisplayCommitAttachesToTheJustClosedEvent) {
  DiagnosticsCollector c;
  DriveUpload(c, MakeContext(), FailedMapFacts());
  c.AttachDisplayCommit(DisplayCommit::kAdvanced);
  EXPECT_EQ(c.history().last_upload->display_commit, DisplayCommit::kAdvanced);
  // A later Begin/Complete pair must not inherit it.
  DriveUpload(c, MakeContext(), MakeCompletedFacts());
  EXPECT_EQ(c.history().last_upload->display_commit, DisplayCommit::kUnknown);
}

TEST(DiagnosticsDocumentIdentity, ResetBumpsTheSerialAndAgesTheOldEvents) {
  DiagnosticsCollector c;
  DriveUpload(c, MakeContext(), MakeCompletedFacts());
  c.OnDocumentReset(DocumentOrigin::kNew);

  EXPECT_EQ(c.document_serial(), 2U);
  EXPECT_FALSE(c.current_document_has_upload());            // the new document has seen nothing
  EXPECT_EQ(c.history().last_upload->document_serial, 1U);  // the old event keeps its identity

  DiagnosticsExportInput input = MakeInput();
  input.current.document_serial = c.document_serial();
  input.history = c.history();
  input.current_document_has_upload = c.current_document_has_upload();
  const json parsed = json::parse(BuildDiagnosticsJson(input, nullptr));
  EXPECT_EQ(parsed["upload_history"]["last_upload"]["document_serial"], 1);
  EXPECT_EQ(parsed["upload_history"]["last_upload"]["document_is_current"], false);
  EXPECT_EQ(parsed["current_document"]["current_document_has_upload_attempt"], false);
}

TEST(DiagnosticsDocumentIdentity, NewDocumentUploadIsCurrentAndOldHistoryStaysAged) {
  DiagnosticsCollector c;
  DriveUpload(c, MakeContext(), FailedMapFacts());      // doc 1 fails
  c.OnDocumentReset(DocumentOrigin::kReopened);         // doc 2
  DriveUpload(c, MakeContext(), MakeCompletedFacts());  // doc 2 succeeds

  EXPECT_EQ(c.history().first_failure->document_serial, 1U);
  EXPECT_EQ(c.history().last_upload->document_serial, 2U);
  EXPECT_TRUE(c.current_document_has_upload());

  DiagnosticsExportInput input = MakeInput();
  input.current.document_serial = c.document_serial();
  input.current.origin = DocumentOrigin::kReopened;
  input.history = c.history();
  input.current_document_has_upload = c.current_document_has_upload();
  const json parsed = json::parse(BuildDiagnosticsJson(input, nullptr));
  EXPECT_EQ(parsed["upload_history"]["first_failure"]["document_is_current"], false);
  EXPECT_EQ(parsed["upload_history"]["last_upload"]["document_is_current"], true);
  EXPECT_EQ(parsed["current_document"]["current_document_has_upload_attempt"], true);
}

TEST(DiagnosticsCollector, ResetForTestReturnsToTheVirginState) {
  DiagnosticsCollector c;
  DriveUpload(c, MakeContext(), FailedMapFacts());
  c.OnDocumentReset(DocumentOrigin::kNew);
  c.ResetForTest();
  EXPECT_EQ(c.document_serial(), 1U);
  EXPECT_FALSE(c.history().last_upload.has_value());
  EXPECT_FALSE(c.current_document_has_upload());
}

// ---------------------------------------------------------------------------
// Failure classification.
// ---------------------------------------------------------------------------

TEST(DiagnosticsFailureClassification, RejectionGlErrorAndFenceFactAreFailuresCompletedCleanIsNot) {
  UploadEvent event;
  event.facts = FailedMapFacts();
  EXPECT_TRUE(IsUploadFailure(event));

  event.facts.outcome = UploadOutcome::kRejectedInvalidInput;
  EXPECT_TRUE(IsUploadFailure(event));

  event.facts = MakeCompletedFacts();
  event.facts.gl_error_state = GlErrorCheckState::kReported;
  EXPECT_TRUE(IsUploadFailure(event));

  event.facts.gl_error_state = GlErrorCheckState::kNone;
  EXPECT_FALSE(IsUploadFailure(event));

  // A completed outcome carrying the fence fact is a failure for the
  // retention — the slot's next write runs unguarded.
  event.facts.fence_create_failed = true;
  EXPECT_TRUE(IsUploadFailure(event));

  // An explicitly-absent fence fact (nullopt, "not observed") does not.
  event.facts.fence_create_failed = std::nullopt;
  EXPECT_FALSE(IsUploadFailure(event));
}

#if !defined(_WIN32)
// The saved notice must ride a COMPLETED write: the shared WriteTextFile
// reports success only after the stream's write-back has actually landed. A
// file-size limit fails the write at flush/close time — after the open and
// the buffered insertion both "succeeded" — and that deferred failure is
// exactly the shape a truncated, unparsable export arrives as. The positive
// control after the restore proves the false came from the deferred failure,
// not from a broken helper.
TEST(DiagnosticsWriteContract, DeferredWriteFailureIsReportedNotSwallowed) {
  namespace fs = std::filesystem;
  const fs::path path = fs::temp_directory_path() / "lumice_diagnostics_deferred_write_fail.txt";
  rlimit original{};
  ASSERT_EQ(getrlimit(RLIMIT_FSIZE, &original), 0) << std::strerror(errno);

  void (*previous_sigxfsz)(int) = signal(SIGXFSZ, SIG_IGN);
  rlimit tiny{};
  tiny.rlim_cur = 1;
  tiny.rlim_max = original.rlim_max;
  ASSERT_EQ(setrlimit(RLIMIT_FSIZE, &tiny), 0);

  const bool reported = WriteTextFile(path, std::string(100, 'x'));

  ASSERT_EQ(setrlimit(RLIMIT_FSIZE, &original), 0);
  signal(SIGXFSZ, previous_sigxfsz);

  std::error_code ec;
  fs::remove(path, ec);

  EXPECT_FALSE(reported) << "a write that failed at close time was reported as success";

  // Positive control, same helper, limits restored: the normal path succeeds.
  EXPECT_TRUE(WriteTextFile(path, std::string(100, 'x')));
  fs::remove(path, ec);
}
#endif

}  // namespace
}  // namespace lumice::gui
