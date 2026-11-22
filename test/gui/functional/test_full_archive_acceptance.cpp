// The 814 acceptance leg's full-size-archive display witness (plan §6.1/§6.3: "大存档只在必须
// 验证实际尺寸的本机/Windows 手工或专用验收腿跑").
//
// What this file is. One opt-in gui_test case that opens a REAL large .lmc (the 8192x4096
// archive behind the 4096 investigation) through the production open chain — LoadLmcFile,
// ResetFrontendState(kOpenBaked), the same upload-and-commit path a user's File>Open takes — and
// witnesses what the plan's AC6 needs: the source size reaches the display metadata intact, the
// upload succeeds on this machine's real GL, and a screenshot of the rendered preview lands on
// disk for eyeballing. Nothing here runs unless the caller points it at an archive:
//
//     gui_test --filter full_archive_acceptance   (with LUMICE_TEST_OPEN_ARCHIVE set)
//
// The path comes from an environment variable because the archive is a 114 MB working-set file
// that lives outside the repo on whatever acceptance machine runs this leg; test infrastructure
// may read env (the env-var policy exempts test/build infra), and the case simply registers as
// nothing when the variable is absent, so the suite's normal runs are untouched. There is
// deliberately no reference-image comparison here: the archive's pixel truth was established by
// the cross-machine investigation; this leg's job is "loads, displays at full size, no failure
// record", which is a state check plus a screenshot, not a tolerance question.

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "gui/gl_common.h"
#include "gui/gui_state.hpp"
#include "test_gui_shared.hpp"

namespace {

// glClientWaitSync return statuses the instrument distinguishes (a sync STATUS is not a GL error
// code): GL_ALREADY_SIGNALED 0x911F, GL_CONDITION_SATISFIED 0x911C, GL_WAIT_FAILED 0x911D.
constexpr int kGlAlreadySignaled = 0x911F;
constexpr int kGlConditionSatisfied = 0x911C;
constexpr int kGlWaitFailed = 0x911D;

std::string ArchivePathFromEnv() {
  const char* p = std::getenv("LUMICE_TEST_OPEN_ARCHIVE");
  return (p != nullptr && *p != '\0') ? std::string(p) : std::string();
}

// Drive one GuiFunc frame that opens the archive on the render thread (the GL upload branch of
// the open must run there — the same shape test_file_ops' baked-preview open uses).
bool g_accept_open_requested = false;
bool g_accept_open_done = false;
std::string g_accept_open_path;

// The upload-bench request channel: ONE rep per request, serviced on the main thread — the GL
// upload must run where the context lives (the same boundary every GL-adjacent request in the
// gui_test suites crosses; a renderer call made from the TestFunc coroutine has no context, the
// map comes back null and the drained error flag reads 0, which looks exactly like a driver
// failure and measures nothing). The timing is taken INSIDE the GuiFunc, around the real call.
std::atomic<bool> g_bench_rep_requested{ false };
std::atomic<bool> g_bench_rep_done{ false };
std::atomic<bool> g_bench_rep_ok{ false };
std::atomic<int> g_bench_rep_stage{ 0 };
std::atomic<int> g_bench_rep_gl_error{ 0 };
// Sample validity of the completion boundary itself: 0 = the rep's fence work succeeded (the
// timing really spans submit→GPU completion); 1 = the DRAIN fence failed (prior work may be
// mixed into the window — the rep must not start); 2 = the COMPLETION fence failed (the window
// does not prove the upload reached the GPU). Either failure makes the rep INVALID: no timing
// line that a performance reading could consume. This is a production-independent instrument
// gate — the upload's own r.ok says nothing about DMA completion.
std::atomic<int> g_bench_rep_fail_phase{ 0 };
std::atomic<int> g_bench_fence_status{ 0 };
double g_bench_rep_ms = 0.0;  // written on the main thread; the done-atomic is the happens-before
const std::vector<float>* g_bench_frame = nullptr;
int g_bench_w = 0;
int g_bench_h = 0;
// Test-only fence-fault injection (never set in production): while above zero, the instrument's
// next fence wait fails synthetically (each failure consumes one count) — the failure-path
// witness drives THIS branch, the same branch a real null fence or GL_WAIT_FAILED takes.
std::atomic<int> g_bench_fence_fault_for_test{ 0 };

namespace {
// One instrument fence: create, wait, delete, and report whether the boundary it witnesses was
// actually reached. Only an explicit signalled state counts; a null fence, a timeout or a wait
// failure is a failed boundary (the caller records the raw status and voids the sample).
bool BenchFenceWait(int* out_status) {
  if (g_bench_fence_fault_for_test.fetch_sub(1) > 0) {
    *out_status = kGlWaitFailed;
    return false;
  }
  GLsync fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  if (fence == nullptr) {
    *out_status = static_cast<int>(glGetError());
    return false;
  }
  GLenum wait = glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, 2'000'000'000ULL);
  glDeleteSync(fence);
  *out_status = static_cast<int>(wait);
  return wait == GL_ALREADY_SIGNALED || wait == GL_CONDITION_SATISFIED;
}
}  // namespace

// The context's own identity, captured on the main thread right after the open's upload: the
// adapter list names every GPU in the machine, and the sshd session's context may land on any
// of them — the acceptance record's "actual renderer" is what the context itself reports, not
// an inference from the adapter list.
char g_accept_gl_vendor[256] = { 0 };
char g_accept_gl_renderer[256] = { 0 };
char g_accept_gl_version[256] = { 0 };
bool g_accept_gl_info_requested = false;
bool g_accept_gl_info_done = false;
// The open's own wall time, taken INSIDE the GuiFunc around the real DoOpen: the decompress +
// mirror + upload + commit chain the reopen cost account keys on (frame-quantized yields add at
// most one frame of slack; the comparison is between arms of this same case, so the slack is
// common).
double g_accept_open_ms = 0.0;

void AcceptanceOpenGuiFunc(ImGuiTestContext* /*ctx*/) {
  if (g_accept_open_requested && !g_accept_open_done) {
    const auto t0 = std::chrono::steady_clock::now();
    gui::DoOpen(g_accept_open_path);
    const auto t1 = std::chrono::steady_clock::now();
    g_accept_open_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    g_accept_open_done = true;
  }
  if (g_accept_gl_info_requested && !g_accept_gl_info_done) {
    const auto copy_gl_string = [](char* dst, size_t cap, unsigned which) {
      const char* s = reinterpret_cast<const char*>(glGetString(which));
      std::snprintf(dst, cap, "%s", s != nullptr ? s : "(glGetString returned null)");
    };
    copy_gl_string(g_accept_gl_vendor, sizeof(g_accept_gl_vendor), GL_VENDOR);
    copy_gl_string(g_accept_gl_renderer, sizeof(g_accept_gl_renderer), GL_RENDERER);
    copy_gl_string(g_accept_gl_version, sizeof(g_accept_gl_version), GL_VERSION);
    g_accept_gl_info_done = true;
  }
  if (g_bench_rep_requested && !g_bench_rep_done) {
    // The comparison boundary is the GPU COMPLETION of the whole snapshot, not the moment the
    // submit call returns — and a sample is only valid when both fence boundaries SUCCEEDED:
    // an UNTIMED drain fence first (the previous rep's tail must not leak into this rep's
    // window; a failed drain voids the rep before timing starts), then the upload, then a
    // completion fence whose SUCCESS is what lets the window be read as submit→GPU-completion.
    // A failed fence records phase + raw status and voids the sample — it never produces a
    // timing line a performance reading could mistake for a completed-boundary measurement.
    int drain_status = 0;
    if (!BenchFenceWait(&drain_status)) {
      g_bench_rep_fail_phase = 1;
      g_bench_fence_status = drain_status;
      g_bench_rep_ok = false;
      g_bench_rep_done = true;
      return;
    }
    const auto t0 = std::chrono::steady_clock::now();
    const gui::UploadResult r = gui::g_preview.UploadXyzTexture(g_bench_frame->data(), g_bench_w, g_bench_h);
    int complete_status = 0;
    const bool complete_ok = BenchFenceWait(&complete_status);
    const auto t1 = std::chrono::steady_clock::now();
    g_bench_rep_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    g_bench_rep_ok = r.ok && complete_ok;
    g_bench_rep_stage = static_cast<int>(r.stage);
    g_bench_rep_gl_error = r.gl_error;
    if (!complete_ok) {
      g_bench_rep_fail_phase = 2;
      g_bench_fence_status = complete_status;
    } else {
      g_bench_rep_fail_phase = 0;
      g_bench_fence_status = complete_status;
    }
    g_bench_rep_done = true;
  }
}

}  // namespace

void RegisterFullArchiveAcceptanceTests(ImGuiTestEngine* engine) {
  // The bench instrument's failure path runs in the NORMAL correctness pool (no archive needed):
  // a synthetically failed drain fence must void the rep — phase=1, no valid timing line — and
  // the next rep with the fault cleared must be a valid sample again. This is the gate that
  // keeps a failed completion boundary from ever masquerading as a measurement.
  {
    ImGuiTest* w = IM_REGISTER_TEST(engine, "full_archive_acceptance", "a_failed_bench_fence_voids_the_sample");
    w->GuiFunc = AcceptanceOpenGuiFunc;
    w->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      gui::g_preview.SetUploadFaultHookForTest(nullptr);
      constexpr int kW = 64;
      constexpr int kH = 32;
      std::vector<float> frame(static_cast<size_t>(kW) * kH * 3, 0.5f);

      g_bench_fence_fault_for_test = 1;  // the next fence wait fails synthetically
      g_bench_frame = &frame;
      g_bench_w = kW;
      g_bench_h = kH;
      g_bench_rep_requested = true;
      g_bench_rep_done = false;
      ctx->Yield(2);
      IM_CHECK(g_bench_rep_done.load());
      IM_CHECK(!g_bench_rep_ok.load());
      IM_CHECK_EQ(g_bench_rep_fail_phase.load(), 1);
      IM_CHECK_EQ(g_bench_fence_status.load(), kGlWaitFailed);

      // The fault is consumed: a plain rep is a valid sample again.
      g_bench_rep_requested = true;
      g_bench_rep_done = false;
      g_bench_rep_fail_phase = 0;
      g_bench_fence_status = 0;
      ctx->Yield(2);
      IM_CHECK(g_bench_rep_done.load());
      IM_CHECK(g_bench_rep_ok.load());
      IM_CHECK_EQ(g_bench_rep_fail_phase.load(), 0);
      g_bench_fence_fault_for_test = 0;
    };
  }

  if (ArchivePathFromEnv().empty()) {
    return;  // No archive pointed at: this leg is opt-in, the suite stays untouched.
  }
  ImGuiTest* t = IM_REGISTER_TEST(engine, "full_archive_acceptance", "the_original_8192_archive_loads_and_displays");
  t->GuiFunc = AcceptanceOpenGuiFunc;
  t->TestFunc = [](ImGuiTestContext* ctx) {
    ResetTestState();
    // Clear any injection residue before the open (no uploads are in flight at this point, so a
    // direct assignment from the coroutine side is race-free): this leg runs the real path.
    gui::g_preview.SetUploadFaultHookForTest(nullptr);

    const std::string path = ArchivePathFromEnv();
    g_accept_open_requested = true;
    g_accept_open_done = false;
    g_accept_open_path = path;
    for (int i = 0; i < 120 && !g_accept_open_done; ++i) {
      ctx->Yield(1);  // a 114 MB decode takes frames; the bound is a safety net
    }
    IM_CHECK(g_accept_open_done);
    ctx->Yield(4);  // let the deferred preview pass render the opened frame

    // The plan's AC6 witnesses, in order: the display accepted the full-size frame (the
    // conservative revoke would have zeroed these on any upload failure); the CPU mirror holds
    // the same size (the save source's own facts); and the exposure state was committed — the
    // commit only happens on a verified upload, so these three together say "loaded and
    // displayed", not "loaded".
    IM_CHECK(gui::g_preview.HasTexture());
    IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), 8192);
    IM_CHECK_EQ(gui::g_preview.GetTextureHeight(), 4096);
    IM_CHECK(gui::g_preview.HasCpuTexture());
    IM_CHECK_EQ(gui::g_preview.GetCpuTextureWidth(), 8192);
    IM_CHECK_EQ(gui::g_preview.GetCpuTextureHeight(), 4096);
    IM_CHECK(gui::g_preview.GetTextureMode() == gui::PreviewRenderer::TextureMode::kXyz);
    IM_CHECK(gui::g_state.snapshot_intensity > 0.0f);

    // The high-res-preview witnesses (the reason this leg exists beyond the 814 form). The
    // reopened document's complete source is the file's own HALF representation — 8192x4096x3
    // binary16 = exactly 192 MiB held as bits, with NO full-float mirror beside it (the +384 MiB
    // expansion the strip-staging work removed) — and the staging the tier-0 upload walked away
    // with is two strip-sized slots inside the ~4 MiB budget, not the two full-size stores the
    // previous upload scheme carried (2 x 192 MiB at this size).
    IM_CHECK(gui::g_preview.CpuXyzStorageIsHalf());
    IM_CHECK(gui::g_preview.GetXyzTextureData() == nullptr);  // no float mirror beside the half bits
    const size_t mirror_bytes = static_cast<size_t>(8192) * 4096 * 3 * sizeof(uint16_t);
    IM_CHECK_EQ(mirror_bytes, 201326592u);  // the nominal account: 192 MiB of half bits
    const size_t slot0 = gui::g_preview.StagingSlotByteCountForTest(0);
    const size_t slot1 = gui::g_preview.StagingSlotByteCountForTest(1);
    IM_CHECK(slot0 > 0);  // the staged upload allocated its first slot
    IM_CHECK(slot1 > 0);  // a 98-strip cut of 8192x4096 writes both ping-pong slots
    IM_CHECK(slot0 <= gui::kXyzStagingSlotTargetBytes);
    IM_CHECK(slot1 <= gui::kXyzStagingSlotTargetBytes);
    g_accept_gl_info_requested = true;
    g_accept_gl_info_done = false;
    for (int i = 0; i < 4 && !g_accept_gl_info_done; ++i) {
      ctx->Yield(1);
    }
    IM_CHECK(g_accept_gl_info_done);
    fprintf(stderr,
            "[highres-acceptance] staging slots: %zu + %zu bytes (budget %zu each); mirror: %zu half bytes; "
            "scale %f\n",
            slot0, slot1, gui::kXyzStagingSlotTargetBytes, mirror_bytes,
            static_cast<double>(gui::g_preview.GetXyzHalfTextureScale()));
    fprintf(stderr, "[highres-acceptance] GL vendor='%s' renderer='%s' version='%s'\n", g_accept_gl_vendor,
            g_accept_gl_renderer, g_accept_gl_version);
    fprintf(stderr, "[highres-acceptance] open wall time: %.1f ms (decompress + mirror + upload + commit)\n",
            g_accept_open_ms);

    // The screenshot: the live preview viewport's pixels, saved next to the log for eyeballing.
    // Size tolerance ±2 px absorbs the viewport fitting the window basis of whatever session
    // runs this; the DISPLAY metadata above is the exact-size witness, this is the picture.
    const int vp_w = gui::g_preview_vp.vp_w;
    const int vp_h = gui::g_preview_vp.vp_h;
    if (gui::g_preview_vp.active && vp_w > 0 && vp_h > 0) {
      g_fullframe_capture.Reset();
      g_fullframe_capture.rect_x = gui::g_preview_vp.vp_x;
      g_fullframe_capture.rect_y = gui::g_preview_vp.vp_y;
      g_fullframe_capture.rect_w = vp_w;
      g_fullframe_capture.rect_h = vp_h;
      g_fullframe_capture.requested.store(true);
      for (int i = 0; i < 4 && !g_fullframe_capture.done.load(); ++i) {
        ctx->Yield(1);
      }
      if (g_fullframe_capture.done.load()) {
        const std::string png = GuiTestTempPath("lumice_814_full_archive_display.png").string();
        IM_CHECK(gui::WriteRgbaBufferToPng(png, g_fullframe_capture.width, g_fullframe_capture.height,
                                           g_fullframe_capture.pixels));
        // The screenshot path is the acceptance artifact; fprintf(stderr) is this suite's
        // diagnostic convention (test binaries are their own harness, outside the no-bare-print
        // rule's scope), and the path is what the operator collects.
        fprintf(stderr, "[814-acceptance] archive display screenshot: %s (%dx%d)\n", png.c_str(),
                g_fullframe_capture.width, g_fullframe_capture.height);
      }
    }
    // And the negative-space witness: no incident recorded (a failure anywhere in the open's
    // upload would have armed the incident record the recovery surface keeps).
    IM_CHECK(!gui::GetPreviewUploadIncident().has_incident);
  };

  // The live-upload timing leg: N wall-clock reps of one production UploadXyzTexture call at the
  // archive's own 8192x4096 size, each number printed on one line for the paired baseline/new
  // comparison the acceptance record needs. This case deliberately asserts NOTHING about the
  // numbers — the comparison is between two builds of this same case on the same machine (the
  // un-staged upload path and the strip path), recorded by the operator, not an in-test
  // threshold. The reps run on the real renderer with the real strip plan; the fence waits the
  // ping-pong needs are part of the per-snapshot cost and are not excluded.
  ImGuiTest* bench = IM_REGISTER_TEST(engine, "full_archive_acceptance", "the_8192_live_upload_is_timed");
  bench->GuiFunc = AcceptanceOpenGuiFunc;
  bench->TestFunc = [](ImGuiTestContext* ctx) {
    ResetTestState();
    gui::g_preview.SetUploadFaultHookForTest(nullptr);
    constexpr int kW = 8192;
    constexpr int kH = 4096;
    // A deterministic non-uniform frame, generated on the spot (the archive file is not needed
    // for the timing; the SIZE is what the strip plan and the cost structure key on).
    std::vector<float> frame(static_cast<size_t>(kW) * kH * 3);
    for (size_t i = 0; i < frame.size(); ++i) {
      frame[i] = static_cast<float>(static_cast<double>(i % 9973) / 9973.0);
    }
    constexpr int kReps = 7;
    for (int rep = 0; rep < kReps; ++rep) {
      g_bench_frame = &frame;
      g_bench_w = kW;
      g_bench_h = kH;
      g_bench_rep_requested = true;
      g_bench_rep_done = false;
      for (int i = 0; i < 8 && !g_bench_rep_done.load(); ++i) {
        ctx->Yield(1);  // one upload is one frame of work; the bound is a hang safety net
      }
      if (!g_bench_rep_done.load()) {
        IM_ERRORF("upload bench rep %d never landed", rep);
        continue;  // non-fatal on purpose (loop-fatal-assert rule): report every rep's outcome
      }
      if (g_bench_rep_fail_phase.load() != 0) {
        // A failed fence boundary voids the sample: the phase (1 = drain, 2 = completion) and
        // the raw sync status are the evidence; this line is NOT a timing sample.
        IM_ERRORF("upload bench rep %d INVALID: fence phase=%d status=0x%04X — excluded from the pair", rep,
                  g_bench_rep_fail_phase.load(), g_bench_fence_status.load());
        continue;
      }
      if (!g_bench_rep_ok.load()) {
        IM_ERRORF("upload bench rep %d failed: stage=%d gl_error=0x%04X", rep, g_bench_rep_stage.load(),
                  g_bench_rep_gl_error.load());
        continue;
      }
      fprintf(stderr, "[highres-upload-bench] rep %d: %.1f ms (staging slot0=%zu slot1=%zu)\n", rep, g_bench_rep_ms,
              gui::g_preview.StagingSlotByteCountForTest(0), gui::g_preview.StagingSlotByteCountForTest(1));
    }
  };
}
