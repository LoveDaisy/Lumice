// The diagnostics export's user-facing half, driven through real clicks and a
// real GL context.
//
// Three things can only be answered here, and each owns a plan §6.1 row the
// cheaper suites deliberately cannot carry:
//
//   1. The upload chain's real witnesses (AC3): the recorder freezing an
//      event around a genuine UploadXyzTexture call — sizes, PBO bytes, the
//      measured level-0 state, the clean GL check — and, on the second
//      witness, a REAL reported GL error (an oversized glTexImage2D against
//      this context's own GL_MAX_TEXTURE_SIZE), not an injection the recorder
//      was told to believe. What enters the JSON is what the chain produced,
//      read back from the same file a user receives.
//   2. The entry chain (AC1): Save menu → "Export Diagnostics..." → the
//      options popup → per-export opt-in, plus the write chain's overwrite
//      prompt with both answers landing on disk. The save dialog itself
//      blocks, so the file-level cases reach the chain through the same seam
//      the config-JSON export's case uses (RequestDiagnosticsExport); the
//      popup and prompt halves around it are driven by real clicks, which is
//      the part a composition test cannot say.
//   3. The llvmpipe arm of AC4: the report's GL block is valid here (software
//      adapter strings are a legitimate reading, not a failure), and the
//      export leaves the document's state untouched (§6.3).
//
// Threading shape (same as test_file_ops.cpp): GL and collector calls run in
// the GuiFunc on the render thread — which is also the only thread the
// recorder's contract allows — and the case reads the answers after
// YieldUntilTrue. The log sentinel in the opt-in case is written from the
// test thread on purpose: the sink is locked, and the export must see lines
// from anywhere. Assertions are IM_CHECK* throughout — a gtest EXPECT inside
// a TestFunc would record its failure in a report this suite never reads.
//
// Structure note: like every other functional suite, this file stays at
// global scope (test_gui_shared.hpp declares the Register* family there, and
// test_gui_main.cpp calls it there); only the helpers sit in an anonymous
// namespace. lumice::gui names are reached through the `gui::` alias.

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <system_error>
#include <vector>

#include "gui/app.hpp"
#include "gui/diagnostics_report.hpp"
#include "gui/gl_common.h"
#include "gui/gui_logger.hpp"
#include "gui/preview_renderer.hpp"
#include "test_gui_shared.hpp"

namespace {

using namespace lumice::gui::diagnostics;
using json = nlohmann::json;

// A frame budget for anything that has to cross to the render thread and back
// (same constant and reason as test_file_ops.cpp).
constexpr int kSettleYieldLimit = 60;

template <typename Fn>
void YieldUntilTrue(ImGuiTestContext* ctx, int max_yields, Fn&& condition) {
  for (int i = 0; i < max_yields && !condition(); ++i) {
    ctx->Yield();
  }
}

// A live-provenance context with settings a reader can tell from defaults.
UploadEventContext MakeWitnessContext(int width, int height) {
  UploadEventContext ctx;
  ctx.provenance = UploadProvenance::kLive;
  ctx.source_width = width;
  ctx.source_height = height;
  ctx.run_epoch = 12ULL;
  ctx.texture_serial = 34ULL;
  ctx.settings = SnapshotFrozenSettings(512, 0, 60.0f, 2.5f, 6, false, true);
  return ctx;
}

bool FileExists(const std::string& path) {
  std::ifstream f(path);
  return f.good();
}

std::string ReadFile(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

// ---------------------------------------------------------------------------------------------
// GL scaffolding (the same shape as test_file_ops.cpp's GlOp): the case
// describes an operation, the GuiFunc performs it on the render thread — GL
// context AND the recorder's sanctioned thread — and the case reads the
// results back. The witness uploads go through the production entry point
// with the context frozen around the call exactly as SyncFromPoller does.
// ---------------------------------------------------------------------------------------------

struct DiagOp {
  bool requested = false;
  bool done = false;

  // Operations, applied in this order within one frame.
  bool witness_upload = false;        // freeze context + one small XYZ upload + attach
  bool witness_gl_error = false;      // freeze context + one oversized XYZ upload (a real GL error)
  bool witness_uint8_error = false;   // same real failure through the uint8 path
  bool witness_fence_fail = false;    // staged XYZ upload under the armed hook: a completed
                                      // frame whose fence creation failed (with a driver code)
  bool witness_map_fail = false;      // staged XYZ upload under the armed hook: a pbo-map failure
  bool seed_mirror_and_save = false;  // CPU mirror + SaveLmcFile(save_texture) for the cycle witness
  bool cycle_open = false;            // DoOpen under the armed hook: the PRODUCTION recovery walk
  bool heal = false;                  // ClearTexture, so a polluted size cannot leak to later cases
  bool export_now = false;            // BuildDiagnosticsReportJson + RequestDiagnosticsExport
  bool clear_hook = false;            // uninstall the fault hook (same frame that armed it)

  // Inputs.
  int upload_w = 16;
  int upload_h = 8;
  bool include_logs = false;
  std::string export_path;
  std::string save_path;
  std::string open_path;
  int hook_stage = 0;  // gui::UploadResult::Stage value the armed hook fails
  int hook_error = 0;  // the code the hook injects

  void Reset() { *this = DiagOp{}; }

  bool install_requested() const { return hook_stage != 0; }
};

DiagOp g_diag_op;

void DiagnosticsGuiFunc(ImGuiTestContext*) {
  if (!g_diag_op.requested || g_diag_op.done) {
    return;
  }

  // The witness bracket: context frozen before the call, the production
  // entry point invoked, the event closed through the SAME translation the
  // production call sites use (app.hpp's CompleteUploadDiagnostics) — the
  // facts that reach the report are the chain's own structured result, never
  // a test-side re-derivation.
  if (g_diag_op.witness_upload) {
    static std::vector<float> pixels(16 * 8 * 3, 0.25f);
    Diagnostics().BeginUpload(MakeWitnessContext(g_diag_op.upload_w, g_diag_op.upload_h));
    const gui::UploadResult r = gui::g_preview.UploadXyzTexture(pixels.data(), g_diag_op.upload_w, g_diag_op.upload_h);
    gui::CompleteUploadDiagnostics(r, UploadPath::kXyz, r.ok, !r.ok && !r.old_frame_retained);
  }
  if (g_diag_op.witness_gl_error) {
    GLint max_size = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);
    const int width = max_size + 1;  // one past the context's own limit: a REAL GL_INVALID_VALUE
    const int height = 2;
    Diagnostics().BeginUpload(MakeWitnessContext(width, height));
    std::vector<float> pixels(static_cast<size_t>(width) * height * 3, 0.25f);
    const gui::UploadResult r = gui::g_preview.UploadXyzTexture(pixels.data(), width, height);
    gui::CompleteUploadDiagnostics(r, UploadPath::kXyz, r.ok, !r.ok && !r.old_frame_retained);
  }
  if (g_diag_op.witness_uint8_error) {
    GLint max_size = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);
    const int width = max_size + 1;  // one past the context's own limit: a REAL GL_INVALID_VALUE
    const int height = 2;
    Diagnostics().BeginUpload(MakeWitnessContext(width, height));
    const std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 3, 0x40);
    const gui::UploadResult r = gui::g_preview.UploadRadianceTexture(pixels.data(), width, height);
    gui::CompleteUploadDiagnostics(r, UploadPath::kSrgbRadiance, r.ok, !r.ok && !r.old_frame_retained);
  }
  if (g_diag_op.install_requested()) {
    const auto stage = static_cast<gui::UploadResult::Stage>(g_diag_op.hook_stage);
    const int err = g_diag_op.hook_error;
    gui::g_preview.SetUploadFaultHookForTest(
        [stage, err](gui::UploadResult::Stage s) -> int { return s == stage ? err : 0; });
  }
  if (g_diag_op.witness_fence_fail) {
    Diagnostics().BeginUpload(MakeWitnessContext(g_diag_op.upload_w, g_diag_op.upload_h));
    static std::vector<float> fence_pixels(16 * 8 * 3, 0.25f);
    const gui::UploadResult r =
        gui::g_preview.UploadXyzTexture(fence_pixels.data(), g_diag_op.upload_w, g_diag_op.upload_h);
    gui::CompleteUploadDiagnostics(r, UploadPath::kXyz, r.ok, !r.ok && !r.old_frame_retained);
  }
  if (g_diag_op.witness_map_fail) {
    Diagnostics().BeginUpload(MakeWitnessContext(g_diag_op.upload_w, g_diag_op.upload_h));
    static std::vector<float> map_pixels(16 * 8 * 3, 0.25f);
    const gui::UploadResult r =
        gui::g_preview.UploadXyzTexture(map_pixels.data(), g_diag_op.upload_w, g_diag_op.upload_h);
    gui::CompleteUploadDiagnostics(r, UploadPath::kXyz, r.ok, !r.ok && !r.old_frame_retained);
  }
  if (g_diag_op.seed_mirror_and_save) {
    // The XYZ display upload never touches the CPU mirror (the save source
    // refreshes from the payload in production); the cycle witness's archive
    // needs the mirror seeded explicitly so the v6 section encodes real bits.
    static std::vector<float> seed(16 * 8 * 3, 0.5f);
    gui::g_preview.UpdateCpuXyzTextureData(seed.data(), 16, 8, gui::XyzTextureMeta{});
    gui::SaveLmcFile(std::filesystem::path(g_diag_op.save_path), gui::g_state, gui::g_preview,
                     /*save_texture=*/true);
  }
  if (g_diag_op.cycle_open) {
    // The PRODUCTION recovery entry: the open's authorized walk brackets
    // each candidate with the frozen context itself.
    gui::DoOpen(std::filesystem::path(g_diag_op.open_path));
  }
  if (g_diag_op.heal) {
    gui::g_preview.ClearTexture();
  }
  if (g_diag_op.clear_hook) {
    gui::g_preview.SetUploadFaultHookForTest(nullptr);
  }
  if (g_diag_op.export_now) {
    const std::string report = gui::BuildDiagnosticsReportJson(g_diag_op.include_logs);
    gui::RequestDiagnosticsExport(std::filesystem::path(g_diag_op.export_path), report);
  }
  g_diag_op.done = true;
}

void RunDiagOp(ImGuiTestContext* ctx) {
  g_diag_op.requested = true;
  YieldUntilTrue(ctx, kSettleYieldLimit, [] { return g_diag_op.done; });
  IM_CHECK(g_diag_op.done);
}

// Best-effort teardown: a failed assertion must not leave a temp file behind.
struct TempFileCleanup {
  std::string path;
  ~TempFileCleanup() {
    std::error_code ec;
    std::filesystem::remove(path, ec);
  }
};

}  // namespace

// Global scope, like the rest of the Register*Tests family.
void RegisterDiagnosticsTests(ImGuiTestEngine* engine) {
  // -------------------------------------------------------------------------------------------
  // The entry chain: menu → options popup → per-export opt-in.
  // -------------------------------------------------------------------------------------------

  // The popup is the only place the log attachment can be chosen, and it must
  // ask again every time: a remembered "yes" would be a privacy default, not
  // an opt-in. Both directions are behavioral — the checkbox flips the very
  // flag DoExportDiagnosticsReport reads.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "diagnostics", "export_options_popup_asks_per_export");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      IM_CHECK(gui::g_diagnostics_include_logs == false);

      ctx->ItemClick("##TopBar/Save");
      ctx->Yield(2);
      // "**/" searches every window: the SaveMenu popup's registered name is
      // not worth pinning, only the item's reachability and effect are.
      ctx->ItemClick("**/Export Diagnostics...");
      ctx->Yield(2);
      IM_CHECK(ImGui::IsPopupOpen("Export Diagnostics"));

      // First open: unchecked, and ticking it moves the flag the export reads.
      IM_CHECK(gui::g_diagnostics_include_logs == false);
      ctx->ItemClick("Export Diagnostics/Include raw log");
      ctx->Yield();
      IM_CHECK(gui::g_diagnostics_include_logs == true);

      // Cancel closes with no export side effect (no dialog, no file).
      ctx->ItemClick("Export Diagnostics/Cancel");
      ctx->Yield(2);
      IM_CHECK(!ImGui::IsPopupOpen("Export Diagnostics"));

      // Second open: the choice reset. Opt-in per export, not a preference.
      ctx->ItemClick("##TopBar/Save");
      ctx->Yield(2);
      ctx->ItemClick("**/Export Diagnostics...");
      ctx->Yield(2);
      IM_CHECK(ImGui::IsPopupOpen("Export Diagnostics"));
      IM_CHECK(gui::g_diagnostics_include_logs == false);
      ctx->ItemClick("Export Diagnostics/Cancel");
      ctx->Yield();
    };
  }

  // -------------------------------------------------------------------------------------------
  // The real witness: what a genuine upload freezes, read back from the file
  // a user would send.
  // -------------------------------------------------------------------------------------------

  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "diagnostics", "upload_event_witness_reaches_the_export_file");
    t->GuiFunc = DiagnosticsGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      Diagnostics().ResetForTest();

      const std::string path = GuiTestTempPath("diag_witness.json");
      TempFileCleanup cleanup{ path };

      // One real upload through the production entry point.
      g_diag_op.Reset();
      g_diag_op.witness_upload = true;
      RunDiagOp(ctx);

      g_diag_op.Reset();
      g_diag_op.export_now = true;
      g_diag_op.export_path = path;
      g_diag_op.include_logs = false;
      RunDiagOp(ctx);
      IM_CHECK(FileExists(path));

      const json report = json::parse(ReadFile(path));
      IM_CHECK_EQ(report["schema_version"].get<int>(), 1);
      IM_CHECK(report.contains("exported_at"));

      // The event: every group the chain produced, from the real call.
      const json& ev = report["upload_history"]["last_upload"];
      IM_CHECK_STR_EQ(ev["provenance"].get<std::string>().c_str(), "live");
      IM_CHECK_EQ(ev["run_epoch"].get<unsigned long long>(), 12ULL);
      IM_CHECK_EQ(ev["texture_serial"].get<unsigned long long>(), 34ULL);
      IM_CHECK_EQ(ev["source"]["width"].get<int>(), 16);
      IM_CHECK_STR_EQ(ev["path"].get<std::string>().c_str(), "xyz");
      IM_CHECK_STR_EQ(ev["outcome"].get<std::string>().c_str(), "completed");
      IM_CHECK_STR_EQ(ev["gl_error"]["state"].get<std::string>().c_str(), "none");
      IM_CHECK(ev["actual_texture"].is_object());
      IM_CHECK_STR_EQ(ev["actual_texture"]["state"].get<std::string>().c_str(), "queried");
      IM_CHECK_EQ(ev["actual_texture"]["width"].get<int>(), 16);  // measured, and the
      IM_CHECK_EQ(ev["actual_texture"]["height"].get<int>(), 8);  // measure agrees with the request
      IM_CHECK(ev["actual_texture"]["internal_format_hex"].is_string());
      IM_CHECK_EQ(ev["requested"]["pbo_bytes"].get<unsigned long long>(), 16ULL * 8 * 3 * 2);
      IM_CHECK(ev["xyz_half_scale"].is_number());
      IM_CHECK_STR_EQ(ev["display_commit"].get<std::string>().c_str(), "advanced");
      IM_CHECK(ev["document_is_current"] == true);
      IM_CHECK_EQ(ev["view_settings_at_upload"]["resolution_tier"].get<int>(), 512);

      // A clean witness must not fabricate a failure.
      IM_CHECK(report["upload_history"]["first_failure"].is_null());
      IM_CHECK(report["current_document"]["current_document_has_upload_attempt"] == true);

      // The llvmpipe reading is valid software-adapter output (AC4): present,
      // non-empty, and a real maximum texture size.
      IM_CHECK(report["graphics_context"]["gl_renderer"].is_string());
      IM_CHECK(!report["graphics_context"]["gl_renderer"].get<std::string>().empty());
      IM_CHECK(report["graphics_context"]["gl_max_texture_size"].is_number());
      IM_CHECK(report["graphics_context"]["gl_max_texture_size"].get<int>() > 0);

      // No log opt-in: no section, no sentinel risk.
      IM_CHECK(!report.contains("log_tail"));
    };
  }

  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "diagnostics", "log_section_only_when_opted_in");
    t->GuiFunc = DiagnosticsGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      Diagnostics().ResetForTest();

      // The product attaches the log ring to the GUI logger during startup;
      // this harness does not (its sink serves only the C API callback), so
      // GUI_LOG_* lines never reach a ring here. Install the product's pair —
      // a dedicated ring swapped in as g_imgui_log_sink and attached beside
      // the logger's existing sinks — so the sentinel travels the real
      // GUI_LOG_WARNING path, then restore both globals when the case ends.
      auto ring = std::make_shared<gui::ImGuiLogSink>();
      const std::vector<spdlog::sink_ptr> sinks_before = gui::GetGuiLogger().sinks();
      const std::shared_ptr<gui::ImGuiLogSink> ring_before = gui::g_imgui_log_sink;
      gui::g_imgui_log_sink = ring;
      std::vector<spdlog::sink_ptr> sinks_with_ring = sinks_before;
      sinks_with_ring.push_back(ring);
      gui::SetGuiLoggerSinks(sinks_with_ring);
      struct SinkRestore {
        std::vector<spdlog::sink_ptr> sinks;
        std::shared_ptr<gui::ImGuiLogSink> ring;
        ~SinkRestore() {
          gui::SetGuiLoggerSinks(sinks);
          gui::g_imgui_log_sink = ring;
        }
      } restore{ sinks_before, ring_before };

      // A line only the log can carry. Written from this thread: the GUI
      // logger takes each sink's lock, and the export (render thread) must
      // see lines written from anywhere.
      GUI_LOG_WARNING("[TEST] diagnostics-log-sentinel-816 privacy probe");
      ctx->Yield(2);  // let the line land in the ring before the export reads it

      const std::string plain_path = GuiTestTempPath("diag_plain.json");
      const std::string logged_path = GuiTestTempPath("diag_logged.json");
      TempFileCleanup plain_cleanup{ plain_path };
      TempFileCleanup logged_cleanup{ logged_path };

      g_diag_op.Reset();
      g_diag_op.export_now = true;
      g_diag_op.export_path = plain_path;
      g_diag_op.include_logs = false;
      RunDiagOp(ctx);
      // A failed export must abort the case here, not fall through to a
      // json::parse on an empty read — the throw would land in abort(), which
      // this machine wedges on (see the progress log).
      IM_CHECK(FileExists(plain_path));
      const std::string plain_text = ReadFile(plain_path);
      const json plain = json::parse(plain_text);
      IM_CHECK(!plain.contains("log_tail"));
      IM_CHECK(plain_text.find("diagnostics-log-sentinel-816") == std::string::npos);

      g_diag_op.Reset();
      g_diag_op.export_now = true;
      g_diag_op.export_path = logged_path;
      g_diag_op.include_logs = true;
      RunDiagOp(ctx);
      IM_CHECK(FileExists(logged_path));
      const json logged = json::parse(ReadFile(logged_path));
      IM_CHECK(logged.contains("log_tail"));
      bool sentinel_found = false;
      for (const auto& line : logged["log_tail"]) {
        if (line["message"].get<std::string>().find("diagnostics-log-sentinel-816") != std::string::npos) {
          sentinel_found = true;
        }
      }
      IM_CHECK(sentinel_found);
    };
  }

  // A REAL reported GL error: one past this context's own
  // GL_MAX_TEXTURE_SIZE, through the production upload entry point. The 814
  // chain rejects the define (GL_INVALID_VALUE, the previous frame retained),
  // the recorder must classify it as the session's first failure carrying the
  // driver's code and the failing stage's name, and the caller's
  // not-advanced fact rides beside — the failure the acceptance owner's
  // structured result makes visible instead of the old unconditional commit.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "diagnostics", "reported_gl_error_becomes_the_first_failure");
    t->GuiFunc = DiagnosticsGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      Diagnostics().ResetForTest();

      g_diag_op.Reset();
      g_diag_op.witness_gl_error = true;
      g_diag_op.heal = true;  // same frame: drop the polluted size before any later case sees it
      RunDiagOp(ctx);

      const std::string path = GuiTestTempPath("diag_gl_error.json");
      TempFileCleanup cleanup{ path };
      g_diag_op.Reset();
      g_diag_op.export_now = true;
      g_diag_op.export_path = path;
      RunDiagOp(ctx);
      IM_CHECK(FileExists(path));

      const json report = json::parse(ReadFile(path));
      const json& ev = report["upload_history"]["first_failure"];
      IM_CHECK(ev.is_object());
      IM_CHECK_STR_EQ(ev["outcome"].get<std::string>().c_str(),
                      "failed_frame_retained");  // the define was ignored; the previous frame stands
      IM_CHECK_STR_EQ(ev["gl_error"]["state"].get<std::string>().c_str(), "reported");
      IM_CHECK_STR_EQ(ev["gl_error"]["code_hex"].get<std::string>().c_str(), "0x0501");  // GL_INVALID_VALUE
      IM_CHECK_STR_EQ(ev["error_stage"].get<std::string>().c_str(), "texture-define");
      IM_CHECK_STR_EQ(ev["display_commit"].get<std::string>().c_str(), "not_advanced");
      IM_CHECK(ev["document_is_current"] == true);
      // The witness never ran (the attempt failed before committing), and the
      // report says so instead of restating the request.
      IM_CHECK(ev["actual_texture"].is_object());
      IM_CHECK_STR_EQ(ev["actual_texture"]["state"].get<std::string>().c_str(), "not_queried");
      // The same event is the session's last failure and last attempt too.
      IM_CHECK_STR_EQ(report["upload_history"]["last_failure"]["gl_error"]["code_hex"].get<std::string>().c_str(),
                      "0x0501");
      IM_CHECK_STR_EQ(report["upload_history"]["last_upload"]["gl_error"]["code_hex"].get<std::string>().c_str(),
                      "0x0501");
    };
  }

  // The same real failure through the uint8 path — composite live uploads and
  // reopened documents ride it, which makes its retention the AC3/AC4 core:
  // a rejected define must reach the failure slots with the stage named,
  // instead of vanishing under the next successful upload.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "diagnostics", "reported_uint8_error_enters_the_failure_slots");
    t->GuiFunc = DiagnosticsGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      Diagnostics().ResetForTest();

      g_diag_op.Reset();
      g_diag_op.witness_uint8_error = true;
      g_diag_op.heal = true;  // same frame: drop the polluted size before any later case sees it
      RunDiagOp(ctx);

      const std::string path = GuiTestTempPath("diag_uint8_error.json");
      TempFileCleanup cleanup{ path };
      g_diag_op.Reset();
      g_diag_op.export_now = true;
      g_diag_op.export_path = path;
      RunDiagOp(ctx);
      IM_CHECK(FileExists(path));

      const json report = json::parse(ReadFile(path));
      const json& ev = report["upload_history"]["first_failure"];
      IM_CHECK(ev.is_object());
      IM_CHECK_STR_EQ(ev["path"].get<std::string>().c_str(), "srgb_radiance");
      IM_CHECK_STR_EQ(ev["outcome"].get<std::string>().c_str(), "failed_frame_retained");
      IM_CHECK_STR_EQ(ev["gl_error"]["state"].get<std::string>().c_str(), "reported");
      IM_CHECK_STR_EQ(ev["gl_error"]["code_hex"].get<std::string>().c_str(), "0x0501");  // GL_INVALID_VALUE
      IM_CHECK_STR_EQ(ev["error_stage"].get<std::string>().c_str(), "texture-define");
      IM_CHECK_STR_EQ(ev["display_commit"].get<std::string>().c_str(), "not_advanced");
      // The same event is the session's last failure and last attempt too.
      IM_CHECK_STR_EQ(report["upload_history"]["last_failure"]["error_stage"].get<std::string>().c_str(),
                      "texture-define");
      IM_CHECK_STR_EQ(report["upload_history"]["last_upload"]["path"].get<std::string>().c_str(), "srgb_radiance");
    };
  }

  // A completed frame whose staged fence creation failed WITH a driver
  // error: the completion must not paint the code over as checked-clean —
  // the fence branch read it, and it reaches the report under the
  // fence-create stage name with the fence fact riding beside. The retention
  // keeps the event as a failure even though the outcome is completed.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "diagnostics", "fence_create_failure_keeps_its_code");
    t->GuiFunc = DiagnosticsGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      Diagnostics().ResetForTest();

      g_diag_op.Reset();
      g_diag_op.witness_fence_fail = true;
      g_diag_op.hook_stage = static_cast<int>(gui::UploadResult::Stage::kFenceCreate);
      g_diag_op.hook_error = 0x0502;  // GL_INVALID_OPERATION: non-OOM, so the last-strip commit proceeds
      g_diag_op.heal = true;
      g_diag_op.clear_hook = true;
      RunDiagOp(ctx);

      const std::string path = GuiTestTempPath("diag_fence_fail.json");
      TempFileCleanup cleanup{ path };
      g_diag_op.Reset();
      g_diag_op.export_now = true;
      g_diag_op.export_path = path;
      RunDiagOp(ctx);
      IM_CHECK(FileExists(path));

      const json report = json::parse(ReadFile(path));
      const json& ev = report["upload_history"]["first_failure"];
      IM_CHECK(ev.is_object());
      IM_CHECK_STR_EQ(ev["outcome"].get<std::string>().c_str(), "completed");  // the frame itself displayed
      IM_CHECK(ev["fence_create_failed"].get<bool>());
      IM_CHECK_STR_EQ(ev["gl_error"]["state"].get<std::string>().c_str(), "reported");
      IM_CHECK_STR_EQ(ev["gl_error"]["code_hex"].get<std::string>().c_str(), "0x0502");
      IM_CHECK_STR_EQ(ev["error_stage"].get<std::string>().c_str(), "fence-create");
      IM_CHECK_STR_EQ(ev["display_commit"].get<std::string>().c_str(), "advanced");
    };
  }

  // A pbo-map failure under the armed hook: the attempt had already frozen
  // its staging request, so the FAILING report still says what it asked the
  // driver for — the fact a maintainer needs, not a null dressed as "never
  // reached the stage".
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "diagnostics", "staging_failure_carries_the_request");
    t->GuiFunc = DiagnosticsGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      Diagnostics().ResetForTest();

      g_diag_op.Reset();
      g_diag_op.witness_map_fail = true;
      g_diag_op.hook_stage = static_cast<int>(gui::UploadResult::Stage::kPboMap);
      g_diag_op.hook_error = 0x0502;  // non-OOM: strip 0, previous frame retained
      g_diag_op.heal = true;
      g_diag_op.clear_hook = true;
      RunDiagOp(ctx);

      const std::string path = GuiTestTempPath("diag_map_fail.json");
      TempFileCleanup cleanup{ path };
      g_diag_op.Reset();
      g_diag_op.export_now = true;
      g_diag_op.export_path = path;
      RunDiagOp(ctx);
      IM_CHECK(FileExists(path));

      const json report = json::parse(ReadFile(path));
      const json& ev = report["upload_history"]["first_failure"];
      IM_CHECK(ev.is_object());
      IM_CHECK_STR_EQ(ev["outcome"].get<std::string>().c_str(), "failed_frame_retained");
      IM_CHECK_STR_EQ(ev["error_stage"].get<std::string>().c_str(), "pbo-map");
      IM_CHECK_STR_EQ(ev["gl_error"]["state"].get<std::string>().c_str(), "reported");
      IM_CHECK_STR_EQ(ev["gl_error"]["code_hex"].get<std::string>().c_str(), "0x0502");
      IM_CHECK_EQ(ev["requested"]["pbo_bytes"].get<unsigned long long>(), 16ULL * 8 * 3 * 2);
      IM_CHECK(ev["actual_texture"]["state"] == "not_queried");
    };
  }

  // The PRODUCTION recovery walk, through the real open: the staged
  // candidate fails (armed pbo-allocate fault), the client-memory candidate
  // accepts the same frame. The report must keep BOTH facts — the losing
  // candidate's stage/error/request latched as the session's first failure,
  // the accepting candidate as last_upload with the caller's advanced fact —
  // neither erasing the other.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "diagnostics", "degraded_open_keeps_the_first_failure");
    t->GuiFunc = DiagnosticsGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      Diagnostics().ResetForTest();

      const std::string archive = GuiTestTempPath("diag_cycle_open.lmc");
      TempFileCleanup archive_cleanup{ archive };

      // Seed the mirror and save a v6 archive: the open's walk consumes the
      // file's own half bits.
      g_diag_op.Reset();
      g_diag_op.seed_mirror_and_save = true;
      g_diag_op.save_path = archive;
      RunDiagOp(ctx);
      IM_CHECK(FileExists(archive));

      g_diag_op.Reset();
      g_diag_op.cycle_open = true;
      g_diag_op.open_path = archive;
      g_diag_op.hook_stage = static_cast<int>(gui::UploadResult::Stage::kPboAllocate);
      g_diag_op.hook_error = 0x0505;  // GL_OUT_OF_MEMORY: the staged candidate revokes
      g_diag_op.clear_hook = true;
      RunDiagOp(ctx);

      const std::string path = GuiTestTempPath("diag_cycle.json");
      TempFileCleanup cleanup{ path };
      g_diag_op.Reset();
      g_diag_op.export_now = true;
      g_diag_op.export_path = path;
      RunDiagOp(ctx);
      IM_CHECK(FileExists(path));

      const json report = json::parse(ReadFile(path));
      const json& first = report["upload_history"]["first_failure"];
      IM_CHECK(first.is_object());
      IM_CHECK_STR_EQ(first["provenance"].get<std::string>().c_str(), "reopen");
      IM_CHECK_STR_EQ(first["outcome"].get<std::string>().c_str(), "failed_display_revoked");
      IM_CHECK_STR_EQ(first["error_stage"].get<std::string>().c_str(), "pbo-allocate");
      IM_CHECK_STR_EQ(first["gl_error"]["state"].get<std::string>().c_str(), "reported");
      IM_CHECK_STR_EQ(first["gl_error"]["code_hex"].get<std::string>().c_str(), "0x0505");
      // The losing candidate's staging request survived the failure.
      IM_CHECK_EQ(first["requested"]["pbo_bytes"].get<unsigned long long>(), 16ULL * 8 * 3 * 2);
      IM_CHECK(first["document_is_current"] == true);

      const json& last = report["upload_history"]["last_upload"];
      IM_CHECK_STR_EQ(last["outcome"].get<std::string>().c_str(), "completed");
      IM_CHECK_STR_EQ(last["display_commit"].get<std::string>().c_str(), "advanced");
      IM_CHECK(last["document_is_current"] == true);
      // The accepting candidate rode the client-memory tier: no staging.
      IM_CHECK(last["requested"]["pbo_bytes"].is_null());
      IM_CHECK_STR_EQ(report["upload_history"]["last_failure"]["error_stage"].get<std::string>().c_str(),
                      "pbo-allocate");  // the accepting success did not erase the walk's failure
      IM_CHECK(report["current_document"]["current_document_has_upload_attempt"] == true);
    };
  }

  // The export is a read: §6.3's regression boundary, witnessed on the live
  // document — dirty, the run epoch, the upload counters and the exact-once
  // cursor all come out exactly as they went in.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "diagnostics", "export_leaves_document_state_untouched");
    t->GuiFunc = DiagnosticsGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      Diagnostics().ResetForTest();

      g_diag_op.Reset();
      g_diag_op.witness_upload = true;
      RunDiagOp(ctx);

      const bool dirty_before = gui::g_state.dirty;
      const unsigned long long epoch_before = static_cast<unsigned long long>(gui::g_state.committed_epoch);
      const unsigned long long count_before = static_cast<unsigned long long>(gui::g_state.texture_upload_count);
      const unsigned long long serial_before =
          static_cast<unsigned long long>(gui::g_state.last_uploaded_texture_serial);

      const std::string path = GuiTestTempPath("diag_no_touch.json");
      TempFileCleanup cleanup{ path };
      g_diag_op.Reset();
      g_diag_op.export_now = true;
      g_diag_op.export_path = path;
      RunDiagOp(ctx);
      IM_CHECK(FileExists(path));

      IM_CHECK(gui::g_state.dirty == dirty_before);
      IM_CHECK_EQ(static_cast<unsigned long long>(gui::g_state.committed_epoch), epoch_before);
      IM_CHECK_EQ(static_cast<unsigned long long>(gui::g_state.texture_upload_count), count_before);
      IM_CHECK_EQ(static_cast<unsigned long long>(gui::g_state.last_uploaded_texture_serial), serial_before);
    };
  }

  // The write chain's prompt, both answers, real clicks (the part a
  // composition test cannot say): a pending export raises the modal, Cancel
  // leaves the original file byte-for-byte, Overwrite replaces it with what
  // was frozen.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "diagnostics", "overwrite_prompt_cancel_and_confirm_land");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);

      const std::string path = GuiTestTempPath("diag_overwrite.json");
      TempFileCleanup cleanup{ path };
      const char* const kExisting = "{\"not\": \"written by the gui\"}\n";
      const char* const kExported = "{\"diagnostics\": true, \"schema_version\": 1}\n";
      {
        std::ofstream out(path, std::ios::binary);
        out << kExisting;
      }
      const auto read_back = [&] { return ReadFile(path); };

      // Cancel: the prompt appears; the file it was protecting survives.
      gui::RequestDiagnosticsExport(std::filesystem::path(path), kExported);
      ctx->Yield(2);
      IM_CHECK(ImGui::IsPopupOpen("Overwrite Diagnostics File"));
      ctx->ItemClick("Overwrite Diagnostics File/Cancel");
      ctx->Yield(2);
      IM_CHECK(!ImGui::IsPopupOpen("Overwrite Diagnostics File"));
      IM_CHECK_STR_EQ(read_back().c_str(), kExisting);

      // Overwrite: the other answer, and the frozen content actually lands.
      gui::RequestDiagnosticsExport(std::filesystem::path(path), kExported);
      ctx->Yield(2);
      IM_CHECK(ImGui::IsPopupOpen("Overwrite Diagnostics File"));
      ctx->ItemClick("Overwrite Diagnostics File/Overwrite");
      ctx->Yield(2);
      IM_CHECK(!ImGui::IsPopupOpen("Overwrite Diagnostics File"));
      IM_CHECK_STR_EQ(read_back().c_str(), kExported);
    };
  }

  // Cancel at the seam: no path, no file, no success claim — the same shape
  // an NFD cancel takes.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "diagnostics", "empty_path_from_the_seam_writes_nothing");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      Diagnostics().ResetForTest();

      const std::string path = GuiTestTempPath("diag_never.json");
      TempFileCleanup cleanup{ path };

      gui::RequestDiagnosticsExport(std::filesystem::path(""), "{\"x\": 1}");
      ctx->Yield(2);
      IM_CHECK(!ImGui::IsPopupOpen("Overwrite Diagnostics File"));
      IM_CHECK(!FileExists(path));
      IM_CHECK(!Diagnostics().history().last_upload.has_value());
    };
  }

  // The full product chain by real clicks — Save menu → "Export Diagnostics..."
  // → "Export..." — with only the native save dialog answered by the one-shot
  // seam: the file a user receives is written by the product path from the
  // menu click onward, and the saved notice names the location (the plan's
  // GUI-file-chain row: never a serializer call standing in for the entry).
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "diagnostics", "menu_to_file_full_chain_via_dialog_seam");
    t->GuiFunc = DiagnosticsGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      Diagnostics().ResetForTest();

      const std::string path = GuiTestTempPath("diag_full_chain.json");
      TempFileCleanup cleanup{ path };

      // One real upload first, so the exported file carries a witnessed event.
      g_diag_op.Reset();
      g_diag_op.witness_upload = true;
      RunDiagOp(ctx);

      gui::g_diagnostics_dialog_seam_path = std::filesystem::path(path);
      ctx->ItemClick("##TopBar/Save");
      ctx->Yield(2);
      ctx->ItemClick("**/Export Diagnostics...");
      ctx->Yield(2);
      IM_CHECK(ImGui::IsPopupOpen("Export Diagnostics"));
      ctx->ItemClick("Export Diagnostics/Export...");
      ctx->Yield(2);

      // The seam is one-shot: consumed by this export, never left set.
      IM_CHECK(gui::g_diagnostics_dialog_seam_path.empty());
      IM_CHECK(FileExists(path));

      const json report = json::parse(ReadFile(path));
      const json& ev = report["upload_history"]["last_upload"];
      IM_CHECK_STR_EQ(ev["outcome"].get<std::string>().c_str(), "completed");
      IM_CHECK_STR_EQ(ev["provenance"].get<std::string>().c_str(), "live");

      // The visible promise: the notice is up and names the chosen location.
      IM_CHECK(ImGui::IsPopupOpen("Diagnostics Report Saved"));
      IM_CHECK(gui::g_diagnostics_result_path.find("diag_full_chain.json") != std::string::npos);
      ctx->ItemClick("Diagnostics Report Saved/OK");
      ctx->Yield();
      IM_CHECK(!ImGui::IsPopupOpen("Diagnostics Report Saved"));
    };
  }
}
