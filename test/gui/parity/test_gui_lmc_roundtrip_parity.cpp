// Save/reopen parity — the gate that makes "a reopened .lmc shows the picture that was on screen
// when it was saved" a proposition something can turn red on, at byte exactness.
//
// The proposition. A live run puts one result frame on screen: its xyz_buffer is uploaded through
// PreviewRenderer::UploadXyzTexture, and its exposure measurements land in GuiState, from where
// the Display panel's per-frame ComputeMonoExposure turns them into the shader's exposure
// uniform. Save (RefreshCpuTextureForSave → SaveLmcFile, src/gui/app.cpp / file_io.cpp) writes
// THAT frame's floats and THOSE measurements into the file's texture section. Open (LoadLmcFile →
// ResetFrontendState's kOpenBaked branch) uploads the floats through the same entry point and
// writes the same measurements back into GuiState. Both documents then render through the same
// kTexModeXyz shader branch with the same inputs, so the two frames must be byte-identical — not
// close, identical. A dB threshold here would be a statement that the two paths are allowed to
// differ, which is the thing this encoding was introduced to remove.
//
// What the previous encoding could not do, and why this is a parity case rather than a visual one.
// Up to .lmc v4 the texture section was an 8-bit sRGB bake of the halo under the exposure in
// effect at save time, and the bake clipped: every texel brighter than white went to 255 before
// it was written, so the bilinear filter in a reopened document ran on clipped bytes while the
// live view's ran on the unclipped energy and clipped afterwards. Over a hot spot 65x over white
// (a subsun at fov 4, the scene this case takes its first row from) that is a visibly different
// picture, not a quantization error — the two were compared at 28 dB PSNR and called consistent.
// The bake also froze the exposure into the pixels, so the EV slider, the exposure mode, the paper
// colour under print mode all stopped working on a reopened document. Storing the unexposed
// linear energy removes the whole class: there is no second bake to diverge from the first.
//
// Rows. Two scenes, each run to completion, saved, reopened and compared:
//   * the trigger — a plate population imaged at fov 4 pointing at the subsun under absolute
//     exposure, i.e. the regime where the old bake's clipping was measured to matter. The case
//     asserts as a PREMISE that the scene really does drive texels over white at the exposure
//     under test; a row that never exceeded 1.0 would compare the two paths only where they had
//     always agreed.
//   * an ordinary all-sky document under relative exposure, so the default mode and the
//     dual-fisheye source's full extent are in the comparison too.
//
// What is compared. Both arms are read through RenderExportToRgba fed the frame's published
// g_preview_vp params — the export arm, not a screen readback. That arm is pinned byte-identical
// to the screen by test_gui_preview_export_parity.cpp beside this file, so comparing export to
// export loses nothing and removes the chrome-overlap hazard that file spends a section on.
//
// The mechanism check. After the byte comparison, each row moves the EV slider and flips the
// exposure mode on the REOPENED document and asserts the picture changes. That is the AC the old
// encoding failed outright (exposure baked in), stated as the smallest observable: not that the
// new brightness is right — the byte comparison already established the reopened chain is the
// live one — but that the reopened document is being re-lit at all.
//
// From format v6 the texture is float16 with one global scale, and the identity got STRONGER
// rather than looser: the live preview's own GPU texture is GL_RGB16F, quantized by the same codec
// on the same floats the file section is (src/gui/xyz_half_codec.hpp), so the file holds the bits
// the screen was sampling. The byte comparison above is unchanged and now covers that. What the
// bump adds is a third leg per row, the pre-v6 document: a v5 file (float32 section, assembled by
// hand here from the frame's own floats because no writer produces one any more) is opened,
// captured, saved — which writes v6 — and reopened; the v5 capture, the v6 re-save's capture and
// the live frame must all be byte-identical. That is the claim "an old document opens to what a
// re-save of it shows", stated on pixels.
//
// Cadence and assets. No committed reference image, so this case must never enter
// scripts/regen_gui_test_refs.py's GROUPS registry (doc/testing-architecture.md §4.10). Like the
// rest of the parity tag it runs on a developer machine with a real GL context via
// ./scripts/test.sh {quick,full,pr} and in no CI job today (§7.5).

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "gui/export_fbo_renderer.hpp"
#include "gui/mono_exposure_scale.hpp"
#include "gui/overlay_labels.hpp"
#include "imgui.h"
#include "support/pixel_diff_metrics.hpp"
#include "test_gui_shared.hpp"

namespace {

// Main-thread work the coroutine cannot do itself: DoOpen (its kOpenBaked branch uploads a GL
// texture) and RenderExportToRgba (an FBO render). Same request/answer scaffolding every
// GL-touching suite here uses, and a non-capturing function pointer because ImGuiTestGuiFunc is
// a raw `void (*)(ImGuiTestContext*)`.
struct MainThreadRequests {
  bool open_requested = false;
  bool open_done = false;
  std::filesystem::path open_path;

  bool export_requested = false;
  bool export_done = false;
  // Snapshot inputs, copied out by the coroutine next to the request so the export consumes what
  // the captured frame published rather than the (by then, newer) global.
  gui::PreviewParams params;
  std::vector<gui::CurveLabelSet> curve_labels;
  int dst_w = 0;
  int dst_h = 0;
  float dpi_x = 1.0f;
  float dpi_y = 1.0f;
  std::vector<unsigned char> rgba;

  void Reset() { *this = MainThreadRequests{}; }
};

MainThreadRequests g_req;

void LmcRoundtripGuiFunc(ImGuiTestContext* /*ctx*/) {
  if (g_req.open_requested && !g_req.open_done) {
    gui::DoOpen(g_req.open_path);
    g_req.open_done = true;
    g_req.open_requested = false;
  }
  if (g_req.export_requested && !g_req.export_done) {
    g_req.rgba = gui::RenderExportToRgba(gui::g_preview, g_req.params, g_req.dst_w, g_req.dst_h, g_req.curve_labels,
                                         g_req.dpi_x, g_req.dpi_y);
    g_req.export_done = true;
    g_req.export_requested = false;
  }
}

// One export-arm frame of the current preview, from the params the panel published for it.
// Yields first so the panel has synced renderer state into g_preview_vp.params (one frame behind
// a change to g_state.renderer) — every caller here changes something just before capturing.
struct Frame {
  std::vector<unsigned char> rgba;
  int w = 0;
  int h = 0;
};

bool CaptureFrame(ImGuiTestContext* ctx, Frame& out) {
  ctx->Yield(3);
  IM_CHECK_RETV(gui::g_preview_vp.active, false);
  g_req.export_requested = false;
  g_req.export_done = false;
  g_req.rgba.clear();
  g_req.params = gui::g_preview_vp.params;
  g_req.curve_labels = gui::g_preview_vp.curve_labels;
  g_req.dst_w = gui::g_preview_vp.vp_w;
  g_req.dst_h = gui::g_preview_vp.vp_h;
  g_req.dpi_x = gui::g_preview_vp.dpi_scale_x;
  g_req.dpi_y = gui::g_preview_vp.dpi_scale_y;
  g_req.export_requested = true;
  ctx->Yield(2);
  IM_CHECK_RETV(g_req.export_done, false);
  IM_CHECK_RETV(!g_req.rgba.empty(), false);
  out.rgba = g_req.rgba;
  out.w = g_req.dst_w;
  out.h = g_req.dst_h;
  return true;
}

// Byte-exact under the shared pixel ruler at tau = 0. Reports the count and the first mismatch so
// a red names a pixel rather than saying "false is not true".
int CountPixelDiffs(const char* tag, const Frame& a, const Frame& b) {
  const lumice::test::PixelDiffResult r = lumice::test::ComputePixelDiff(a.rgba.data(), b.rgba.data(), a.w, a.h,
                                                                         /*channels=*/4, /*tau=*/0);
  if (r.n_diff > 0) {
    for (size_t i = 0; i + 3 < a.rgba.size(); i += 4) {
      if (a.rgba[i] != b.rgba[i] || a.rgba[i + 1] != b.rgba[i + 1] || a.rgba[i + 2] != b.rgba[i + 2]) {
        const int px = static_cast<int>(i / 4);
        fprintf(stderr, "[lmc_roundtrip_parity] %s: first mismatch at (%d,%d): a=(%u,%u,%u) b=(%u,%u,%u)\n", tag,
                px % a.w, px / a.w, a.rgba[i], a.rgba[i + 1], a.rgba[i + 2], b.rgba[i], b.rgba[i + 1], b.rgba[i + 2]);
        break;
      }
    }
  }
  fprintf(stderr, "[lmc_roundtrip_parity] %s: %dx%d, %d pixels differ (largest blob %d, dmax %d)\n", tag, a.w, a.h,
          r.n_diff, r.max_cc, r.dmax);
  return r.n_diff;
}

// The exposure the Display panel applies to the texture now on screen, from the same function it
// uses. Read here to state the trigger row's premise in the shader's own units.
float CurrentIntensityScale() {
  lumice::gui::MonoExposureInput in;
  in.exposure_offset = gui::g_state.renderer.exposure_offset;
  in.ev_auto = gui::g_state.ev_auto;
  in.snapshot_intensity = gui::g_state.snapshot_intensity;
  in.snapshot_emitted_energy = gui::g_state.snapshot_emitted_energy;
  in.total_pixels = gui::g_preview.GetTextureWidth() * gui::g_preview.GetTextureHeight();
  return lumice::gui::ComputeMonoExposure(gui::g_state.renderer.ev_mode, in).intensity_scale;
}

// Largest exposed CIE Y over the CPU mirror of the texture — the quantity the shader clamps at
// 1.0 (before the lens's relative illumination, which only ever attenuates).
float MaxExposedY(float intensity_scale) {
  const float* xyz = gui::g_preview.GetXyzTextureData();
  if (xyz == nullptr) {
    return 0.0f;
  }
  const size_t n = static_cast<size_t>(gui::g_preview.GetTextureWidth()) * gui::g_preview.GetTextureHeight();
  float max_y = 0.0f;
  for (size_t i = 0; i < n; ++i) {
    max_y = std::fmax(max_y, xyz[i * 3 + 1]);
  }
  return max_y * intensity_scale;
}

// A v5 .lmc, assembled from a v6 file the production writer just made and the frame's own floats
// (the CPU mirror Save was fed). The container header, the JSON section and its offsets are the
// writer's; only the texture section is replaced, by the documented v5 layout: the same 32-byte
// header with raw_byte_count in float32 units and the last field the reserved 0u, then a zlib
// stream of the floats as stored (uncompressed) deflate blocks — the format says zlib, not
// "compressed", and a stored stream needs no compressor here while inflating through the same
// stbi call a real v5 stream does. Offsets restated from the file_io.cpp header comment, the
// same way test_lmc_load_atomicity_chain.cpp restates them.
bool WriteV5FloatFileFrom(const std::filesystem::path& v6_path, const std::filesystem::path& v5_path) {
  std::ifstream in(v6_path, std::ios::binary);
  std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (bytes.size() < 44u) {
    return false;
  }
  uint32_t flags = 0;
  uint64_t tex_offset = 0;
  std::memcpy(&flags, bytes.data() + 8, sizeof(flags));
  std::memcpy(&tex_offset, bytes.data() + 28, sizeof(tex_offset));
  if ((flags & 0x8u) == 0u || tex_offset == 0u || bytes.size() < tex_offset + 32u) {
    return false;  // premise: the writer made a v6 float16 file with a texture
  }
  const float* xyz = gui::g_preview.GetXyzTextureData();
  const int w = gui::g_preview.GetTextureWidth();
  const int h = gui::g_preview.GetTextureHeight();
  if (xyz == nullptr || w <= 0 || h <= 0) {
    return false;
  }
  const size_t raw_bytes = static_cast<size_t>(w) * h * 3 * sizeof(float);
  std::vector<unsigned char> section(bytes.begin() + static_cast<std::ptrdiff_t>(tex_offset),
                                     bytes.begin() + static_cast<std::ptrdiff_t>(tex_offset) + 32);
  const uint32_t raw_u32 = static_cast<uint32_t>(raw_bytes);
  const uint32_t zero = 0;
  std::memcpy(section.data() + 24, &raw_u32, sizeof(raw_u32));
  std::memcpy(section.data() + 28, &zero, sizeof(zero));
  section.push_back(0x78);  // zlib header: deflate, 32K window
  section.push_back(0x01);  // fastest level, no dictionary, check bits valid
  const auto* raw = reinterpret_cast<const unsigned char*>(xyz);
  uint32_t a = 1;
  uint32_t b = 0;
  for (size_t pos = 0; pos < raw_bytes;) {
    const size_t len = std::min<size_t>(65535u, raw_bytes - pos);
    const bool last = pos + len == raw_bytes;
    section.push_back(last ? 0x01 : 0x00);  // BFINAL, BTYPE=00 (stored)
    section.push_back(static_cast<unsigned char>(len & 0xFFu));
    section.push_back(static_cast<unsigned char>((len >> 8) & 0xFFu));
    section.push_back(static_cast<unsigned char>(~len & 0xFFu));
    section.push_back(static_cast<unsigned char>((~len >> 8) & 0xFFu));
    section.insert(section.end(), raw + pos, raw + pos + len);
    for (size_t i = pos; i < pos + len; ++i) {  // adler32, big-endian on the wire
      a = (a + raw[i]) % 65521u;
      b = (b + a) % 65521u;
    }
    pos += len;
  }
  const uint32_t adler = (b << 16) | a;
  section.push_back(static_cast<unsigned char>(adler >> 24));
  section.push_back(static_cast<unsigned char>(adler >> 16));
  section.push_back(static_cast<unsigned char>(adler >> 8));
  section.push_back(static_cast<unsigned char>(adler));

  bytes.resize(static_cast<size_t>(tex_offset));
  bytes.insert(bytes.end(), section.begin(), section.end());
  const uint32_t version = 5;
  const uint32_t v5_flags = (flags & ~0x8u) | 0x4u;
  const uint64_t tex_size = section.size();
  std::memcpy(bytes.data() + 4, &version, sizeof(version));
  std::memcpy(bytes.data() + 8, &v5_flags, sizeof(v5_flags));
  std::memcpy(bytes.data() + 36, &tex_size, sizeof(tex_size));
  std::ofstream out(v5_path, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) {
    return false;
  }
  out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  return out.good();
}

// Drop the live run (or whatever document is open) and reopen `path` through the production
// Open path on the main thread, leaving the panel settled on the loaded document.
bool SeverAndOpen(ImGuiTestContext* ctx, const std::filesystem::path& path) {
  if (gui::g_server != nullptr) {
    gui::g_server_poller.Stop();
    LUMICE_StopServer(gui::g_server);
    LUMICE_DestroyServer(gui::g_server);
    gui::g_server = nullptr;
  }
  gui::DoNew();
  ctx->Yield(2);
  IM_CHECK_RETV(!gui::g_preview.HasTexture(), false);

  g_req.open_done = false;
  g_req.open_path = path;
  g_req.open_requested = true;
  ctx->Yield(2);
  IM_CHECK_RETV(g_req.open_done, false);
  IM_CHECK_RETV(gui::g_state.current_file_path == path, false);
  IM_CHECK_RETV(gui::g_preview.HasTexture(), false);
  // The reopened document takes the live shader branch, not the legacy 8-bit one.
  IM_CHECK_RETV(gui::g_preview.GetTextureMode() == gui::PreviewRenderer::TextureMode::kXyz, false);
  // Let ReconcileSimState take the kLoaded intent to kDone and the panel republish params.
  ctx->Yield(3);
  IM_CHECK_RETV(gui::g_state.sim_state == gui::GuiState::SimState::kDone, false);
  return true;
}

struct Scene {
  const char* name;
  void (*apply)();
  // The exposure mode the scene saves under; asserted back after Open as the JSON half of AC4.
  int ev_mode;
  // Whether the row must drive texels over white — the regime the old bake clipped.
  bool expects_overexposure;
};

// The trigger: plates, imaged at fov 4 straight at the subsun, absolute exposure at -8 stops.
// Same shape as the beta document that produced the observation (fov 4, absolute EV -8), on the
// harness's smallest simulation resolution and a finite budget so the row completes in seconds.
void ApplySubsunHotspotScene() {
  auto& c = gui::g_state.crystals[0];
  c.type = gui::CrystalType::kPrism;
  c.height = 0.3f;  // a plate
  c.zenith = gui::AxisDist{ gui::AxisDistType::kGauss, 0.0f, 0.5f };
  c.azimuth = gui::AxisDist{ gui::AxisDistType::kUniform, 0.0f, 360.0f };
  c.roll = gui::AxisDist{ gui::AxisDistType::kUniform, 0.0f, 360.0f };
  gui::g_state.sun.altitude = 20.0f;
  gui::g_state.sim.infinite = false;
  gui::g_state.sim.ray_num_millions = 0.5f;
  gui::g_state.sim.max_hits = 8;
  auto& r = gui::g_state.renderer;
  r.lens_type = 0;  // Linear
  r.fov = 4.0f;
  r.elevation = -20.0f;  // the subsun: as far below the horizon as the sun is above it
  r.azimuth = 0.0f;
  r.sim_resolution_index = 0;
  r.visible = 2;
  r.ev_mode = 1;  // absolute
  r.exposure_offset = -8.0f;
}

// An ordinary all-sky document: the default prism population, the dual-fisheye source shown
// whole, relative exposure at the slider's default.
void ApplyAllSkyScene() {
  gui::g_state.sun.altitude = 20.0f;
  gui::g_state.sim.infinite = false;
  gui::g_state.sim.ray_num_millions = 0.5f;
  gui::g_state.sim.max_hits = 8;
  auto& r = gui::g_state.renderer;
  r.lens_type = 4;  // Dual Fisheye Equal Area
  r.fov = 180.0f;
  r.elevation = 0.0f;
  r.azimuth = 0.0f;
  r.sim_resolution_index = 0;
  r.visible = 2;
  r.ev_mode = 0;  // relative
  r.exposure_offset = 0.0f;
}

constexpr Scene kScenes[] = {
  { "subsun_hotspot_fov4_absolute_ev-8", ApplySubsunHotspotScene, /*ev_mode=*/1, /*expects_overexposure=*/true },
  { "all_sky_dual_fisheye_relative", ApplyAllSkyScene, /*ev_mode=*/0, /*expects_overexposure=*/false },
};

// Drive the seeded finite run to completion, then let the poller settle. Returns false on the
// wall-clock bound so the caller fails at its own line. Wall clock rather than a frame count
// because --fixed-dt decouples the two (see test_run_lifecycle.cpp's DriveUntil).
bool RunToDoneAndSettle(ImGuiTestContext* ctx) {
  gui::g_state.stats_sim_ray_num = 0;
  gui::g_state.texture_upload_count = 0;
  gui::DoRun(/*user_initiated=*/true);
  IM_CHECK_RETV(gui::g_state.run_intent == gui::RunIntent::kRunning, false);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while ((gui::g_state.sim_state != gui::GuiState::SimState::kDone ||
          gui::g_state.run_intent != gui::RunIntent::kRunCompleted || gui::g_state.texture_upload_count == 0) &&
         std::chrono::steady_clock::now() < deadline) {
    ctx->Yield();
  }
  IM_CHECK_RETV(gui::g_state.sim_state == gui::GuiState::SimState::kDone, false);
  IM_CHECK_RETV(gui::g_state.texture_upload_count > 0, false);
  // Completion implies the poller has drained (doc/gui-preview-lifecycle-architecture.md I7), so
  // the upload that is on screen now is the final one. A few more frames make that a checked
  // fact rather than a belief: the premise assertions below hold the upload count still.
  ctx->Yield(5);
  return true;
}

// One row, as a function rather than a loop body: an IM_CHECK failing here returns from THIS
// row only, so the rows after it still run and a red names every scene that failed rather than
// the first one (scripts/check_loop_fatal_asserts.py).
void RunScene(ImGuiTestContext* ctx, const Scene& scene) {
  ctx->LogInfo("scene: %s", scene.name);
  ResetTestState();
  g_req.Reset();
  scene.apply();
  gui::g_server = LUMICE_CreateServer();
  IM_CHECK(gui::g_server != nullptr);

  IM_CHECK(RunToDoneAndSettle(ctx));
  IM_CHECK(gui::g_preview.HasTexture());
  IM_CHECK(gui::g_preview.GetTextureMode() == gui::PreviewRenderer::TextureMode::kXyz);
  // Off the preview so no hover highlight lands in either capture.
  ctx->MouseMoveToPos(ImVec2(-100.0f, -100.0f));

  // --- Live arm: screenshot A, then Save, with the upload count pinned across both. ---
  const unsigned long long uploads_at_a = gui::g_state.texture_upload_count;
  Frame a;
  IM_CHECK(CaptureFrame(ctx, a));
  const float live_snapshot_intensity = gui::g_state.snapshot_intensity;
  const float live_emitted_energy = gui::g_state.snapshot_emitted_energy;
  const float live_anchor = gui::g_state.p99_raw_y;
  const float live_ev_auto = gui::g_state.ev_auto;
  const int live_effective_pixels = gui::g_state.effective_pixels;
  IM_CHECK(live_snapshot_intensity > 0.0f);
  IM_CHECK(live_emitted_energy > 0.0f);

  const std::filesystem::path lmc_path = GuiTestTempPath(std::string("lumice_roundtrip_") + scene.name + ".lmc");
  gui::g_state.current_file_path = lmc_path;
  gui::DoSave();
  IM_CHECK(std::filesystem::exists(lmc_path));
  // Premise: nothing was uploaded between the capture and the save, so the file holds the
  // frame A was rendered from. A moving count here is a harness ordering problem, and it
  // should read as one rather than as a mysterious pixel difference below.
  IM_CHECK_EQ(gui::g_state.texture_upload_count, uploads_at_a);
  // The trigger row's premise: at this exposure the scene really does put texels over white,
  // i.e. into the range the old bake clipped. Measured off the very mirror Save just wrote.
  const float max_exposed_y = MaxExposedY(CurrentIntensityScale());
  fprintf(stderr, "[lmc_roundtrip_parity] %s: max exposed Y on the saved texture = %.3f\n", scene.name, max_exposed_y);
  if (scene.expects_overexposure) {
    IM_CHECK_GT(max_exposed_y, 1.0f);
  }

  // The pre-v6 leg's input, built now while the mirror still holds the live frame's floats.
  const std::filesystem::path v5_path = GuiTestTempPath(std::string("lumice_roundtrip_") + scene.name + "_v5.lmc");
  IM_CHECK(WriteV5FloatFileFrom(lmc_path, v5_path));

  // --- Reopened arm: sever the live run entirely, so what comes back can only have come from the
  // file, then the production Open path on the main thread. ---
  IM_CHECK(SeverAndOpen(ctx, lmc_path));
  // AC4's mechanism: the measurements that expose the texture came back with it, to the bit.
  IM_CHECK_EQ(gui::g_state.snapshot_intensity, live_snapshot_intensity);
  IM_CHECK_EQ(gui::g_state.snapshot_emitted_energy, live_emitted_energy);
  IM_CHECK_EQ(gui::g_state.p99_raw_y, live_anchor);
  IM_CHECK_EQ(gui::g_state.ev_auto, live_ev_auto);
  IM_CHECK_EQ(gui::g_state.effective_pixels, live_effective_pixels);
  // And the settings the JSON carries, the two the exposure reads.
  IM_CHECK_EQ(gui::g_state.renderer.ev_mode, scene.ev_mode);

  Frame b;
  IM_CHECK(CaptureFrame(ctx, b));
  IM_CHECK_EQ(b.w, a.w);
  IM_CHECK_EQ(b.h, a.h);
  // AC3: byte-identical. tau = 0, and the count itself must be 0 — not the largest blob, the
  // count: a single differing pixel is a divergence between the two chains.
  IM_CHECK_EQ(CountPixelDiffs(scene.name, a, b), 0);

  // --- AC4: the reopened document is re-lit by the display controls. ---
  gui::g_state.renderer.exposure_offset += 1.0f;
  Frame c;
  IM_CHECK(CaptureFrame(ctx, c));
  IM_CHECK_GT(CountPixelDiffs("ev_plus_one_stop_vs_reopened", b, c), 0);
  gui::g_state.renderer.exposure_offset -= 1.0f;
  gui::g_state.renderer.ev_mode = 1 - gui::g_state.renderer.ev_mode;
  Frame d;
  IM_CHECK(CaptureFrame(ctx, d));
  IM_CHECK_GT(CountPixelDiffs("ev_mode_flipped_vs_reopened", b, d), 0);
  gui::g_state.renderer.ev_mode = 1 - gui::g_state.renderer.ev_mode;

  // --- The pre-v6 leg: v5 open -> capture -> Save (writes v6) -> reopen -> capture. ---
  // The v5 floats are the live frame's floats, so the upload quantizes them to the bits the live
  // frame was sampled from: x == a. The Save then encodes the mirror the v5 load filled (the
  // payload the live run retained was dropped by the document switch — Save cannot be reading
  // it), and y == x is quantize∘dequantize∘quantize == quantize on real pixels.
  IM_CHECK(SeverAndOpen(ctx, v5_path));
  Frame x;
  IM_CHECK(CaptureFrame(ctx, x));
  IM_CHECK_EQ(x.w, a.w);
  IM_CHECK_EQ(x.h, a.h);
  IM_CHECK_EQ(CountPixelDiffs("v5_opened_vs_live", a, x), 0);

  const std::filesystem::path resaved_path =
      GuiTestTempPath(std::string("lumice_roundtrip_") + scene.name + "_v5_resaved.lmc");
  gui::g_state.current_file_path = resaved_path;
  gui::DoSave();
  IM_CHECK(std::filesystem::exists(resaved_path));
  // Premise: the re-save is a v6 file, i.e. the writer really did re-encode the v5 floats.
  {
    std::ifstream in(resaved_path, std::ios::binary);
    uint32_t hdr[3] = { 0, 0, 0 };
    in.read(reinterpret_cast<char*>(hdr), sizeof(hdr));
    IM_CHECK_EQ(hdr[1], 6u);
    IM_CHECK_EQ(hdr[2] & 0xCu, 0x8u);
  }

  IM_CHECK(SeverAndOpen(ctx, resaved_path));
  Frame y;
  IM_CHECK(CaptureFrame(ctx, y));
  IM_CHECK_EQ(y.w, a.w);
  IM_CHECK_EQ(y.h, a.h);
  IM_CHECK_EQ(CountPixelDiffs("v5_resaved_as_v6_vs_v5_opened", x, y), 0);

  std::filesystem::remove(lmc_path);
  std::filesystem::remove(v5_path);
  std::filesystem::remove(resaved_path);
}

}  // namespace

void RegisterLmcRoundtripParityTests(ImGuiTestEngine* engine) {
  ImGuiTest* t = IM_REGISTER_TEST(engine, "lmc_roundtrip_parity", "a_reopened_document_shows_the_frame_it_saved");
  t->GuiFunc = LmcRoundtripGuiFunc;
  t->TestFunc = [](ImGuiTestContext* ctx) {
    for (const Scene& scene : kScenes) {
      RunScene(ctx, scene);
    }
  };
}
