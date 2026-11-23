// Composition chain: what a document switch and a revert do to the diagnostics
// recorder's identity, driven through the real document entry points.
//
// Units in the chain: app (DoNew / DoOpen / DoRevert — and through them
// ResetFrontendState) × diagnostics_report (the recorder + the JSON assembly)
// × gui_state.
//
// What the collaboration produces that is observable: every retained upload
// event carries the anonymous serial of the document it belonged to, and a
// document switch re-arms the "current document has seen an upload attempt"
// fact. The proposition this pins is plan §3.2's identity law — an old
// document's failure must be exportable as complete history while being
// explicitly NOT the current document's, and the current context must be able
// to say "no upload happened here" even though history holds older events.
// Revert is the carve-out, the same one test_document_switch_chain pins for
// the status bar: it restores configuration into the SAME document, so the
// serial and the origin must not move.
//
// Deliberately NOT here: the reopen-with-texture entry (kOpenBaked) — its
// upload needs a GL context, so its witness lives in
// gui_test/functional/test_diagnostics_export.cpp; and the pure JSON/privacy
// rules, which test_diagnostics_report.cpp holds. The upload events here are
// frozen through the collector API directly: the production sites freeze the
// same fields (the live path in SyncFromPoller, the reopen path in
// ResetFrontendState), and what this chain needs from THEM is the bracket
// (ResetFrontendState → Begin/Complete), not a second GL implementation.

#include <gtest/gtest.h>

#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
#include <system_error>

#include "gui/app.hpp"
#include "gui/diagnostics_report.hpp"
#include "gui/file_io.hpp"
#include "gui/gui_state.hpp"

namespace lumice::gui {
namespace {

using namespace lumice::gui::diagnostics;
using json = nlohmann::json;

// One live-shaped attempt frozen through the collector, with settings a
// maintainer can tell apart from the defaults.
void DriveLiveUpload(DiagnosticsCollector& c) {
  UploadEventContext ctx;
  ctx.provenance = UploadProvenance::kLive;
  ctx.source_width = 1024;
  ctx.source_height = 512;
  ctx.run_epoch = 9ULL;
  ctx.texture_serial = 77ULL;
  ctx.settings = SnapshotFrozenSettings(4096, 1, 180.0f, 5.0f, 8, false, true);
  c.BeginUpload(ctx);

  UploadFacts facts;
  facts.path = UploadPath::kXyz;
  facts.outcome = UploadOutcome::kFailedDisplayRevoked;  // a failure: it latches first_failure
  facts.requested_width = 1024;
  facts.requested_height = 512;
  facts.gl_error_state = GlErrorCheckState::kNone;
  facts.error_stage = "pbo-map";  // the stage failed without leaving a flag
  c.CompleteUpload(facts);
}

// Assembles the report the same way the production gatherer does for the
// parts that need no GL: collector state in, assembly out. (The real gatherer
// additionally reads the live GL context and the OS — both out of scope for a
// windowless process.)
json ReportFor(const DiagnosticsCollector& c) {
  DiagnosticsExportInput input;
  input.current.document_serial = c.document_serial();
  input.current.origin = c.current_document_origin();
  input.history = c.history();
  input.current_document_has_upload = c.current_document_has_upload();
  return json::parse(BuildDiagnosticsJson(input, nullptr));
}

TEST(DiagnosticsContextChain, DocumentSwitchAgesHistoryAndRearmsTheCurrentFact) {
  DiagnosticsCollector& c = Diagnostics();
  c.ResetForTest();

  DoNew();  // document 2 — document 1 existed before this process touched anything
  EXPECT_EQ(c.document_serial(), 2U);
  EXPECT_EQ(c.current_document_origin(), DocumentOrigin::kNew);
  EXPECT_FALSE(c.current_document_has_upload());

  DriveLiveUpload(c);
  EXPECT_TRUE(c.current_document_has_upload());

  // A blank .lmc open (no texture section) is a document switch of its own:
  // the serial moves, the current fact re-arms, the history event keeps the
  // serial it was frozen under.
  const std::filesystem::path path = std::filesystem::temp_directory_path() / "lumice_diagnostics_context_chain.lmc";
  struct Cleanup {
    std::filesystem::path path;
    ~Cleanup() {
      std::error_code ec;
      std::filesystem::remove(path, ec);  // best-effort: a teardown failure must not fail the case
    }
  } cleanup{ path };
  ASSERT_TRUE(SaveLmcFile(path, g_state, g_preview, /*save_texture=*/false));

  DoOpen(path);
  EXPECT_EQ(c.document_serial(), 3U);
  EXPECT_EQ(c.current_document_origin(), DocumentOrigin::kReopenedNoTexture);
  EXPECT_FALSE(c.current_document_has_upload()) << "the new document inherited the previous document's upload fact";
  ASSERT_TRUE(c.history().last_upload.has_value());
  EXPECT_EQ(c.history().last_upload->document_serial, 2U);

  const json report = ReportFor(c);
  EXPECT_EQ(report["upload_history"]["last_upload"]["document_serial"], 2);
  EXPECT_EQ(report["upload_history"]["last_upload"]["document_is_current"], false);
  EXPECT_EQ(report["current_document"]["origin"], "reopened_no_texture");
  EXPECT_EQ(report["current_document"]["current_document_has_upload_attempt"], false);
}

TEST(DiagnosticsContextChain, RevertKeepsTheSameDocumentIdentity) {
  DiagnosticsCollector& c = Diagnostics();
  c.ResetForTest();

  DoNew();
  DriveLiveUpload(c);
  const unsigned long long serial_after_upload = c.document_serial();
  EXPECT_TRUE(c.current_document_has_upload());

  // Revert restores configuration into the SAME document: no serial bump, no
  // origin change, and the upload fact stays — the picture on screen still
  // belongs to this document.
  g_state.last_committed_state = GuiState::ConfigSnapshot::From(g_state);
  DoRevert();
  EXPECT_EQ(c.document_serial(), serial_after_upload);
  EXPECT_TRUE(c.current_document_has_upload());
  EXPECT_EQ(c.history().last_upload->document_serial, serial_after_upload);

  const json report = ReportFor(c);
  EXPECT_EQ(report["upload_history"]["last_upload"]["document_is_current"], true);
}

TEST(DiagnosticsContextChain, FirstFailureFromAnOldDocumentStillExportsAsHistory) {
  DiagnosticsCollector& c = Diagnostics();
  c.ResetForTest();

  DoNew();
  DriveLiveUpload(c);  // document 2 fails once — the session's first failure
  DoNew();             // document 3, clean

  // The latch survives the switch: first_failure is still document 2's, the
  // current document reads as upload-free, and the two facts are in the same
  // report without contaminating each other.
  EXPECT_EQ(c.history().first_failure->document_serial, 2U);
  EXPECT_FALSE(c.current_document_has_upload());

  const json report = ReportFor(c);
  EXPECT_EQ(report["upload_history"]["first_failure"]["document_serial"], 2);
  EXPECT_EQ(report["upload_history"]["first_failure"]["outcome"], "failed_display_revoked");
  EXPECT_EQ(report["upload_history"]["last_failure"]["document_is_current"], false);
  EXPECT_EQ(report["current_document"]["current_document_has_upload_attempt"], false);
  EXPECT_EQ(report["current_document"]["document_serial"], 3);
}

}  // namespace
}  // namespace lumice::gui
