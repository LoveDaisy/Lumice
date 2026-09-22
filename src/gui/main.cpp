#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <timeapi.h>
#include <windows.h>
#endif

#include <algorithm>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <thread>

#include "gui/analysis_panel.hpp"
#include "gui/app.hpp"
#include "gui/color_window.hpp"
#include "gui/config_summary_window.hpp"
#include "gui/defaults_panel.hpp"
#include "gui/edit_modals.hpp"
#include "gui/export_fbo_renderer.hpp"
#include "gui/file_io.hpp"
#include "gui/gl_common.h"
#include "gui/gl_init.h"
#include "gui/gui_logger.hpp"
#include "gui/gui_state_reconcile.hpp"
#include "gui/log_sink.hpp"
#include "gui/theme.hpp"
#include "gui/user_defaults.hpp"
#include "gui/window_sizing.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "include/lumice.h"  // LUMICE_GetVersionString (window title, startup log line)
#if defined(LUMICE_ENGINE_DELAY_LOADED)
#include "launcher/win_engine_loader.h"
#endif
#include "util/path_utils.hpp"

namespace gui = lumice::gui;

namespace {

// The monitor's content scale as GLFW last reported it — the startup read after the window exists,
// then every glfwSetWindowContentScaleCallback since. Initialised to 1 rather than 0 so the value is
// a usable scale at every point in the program's life (a 0 would make the layout collapse rather
// than fail). It is an INPUT the rebuild path reads, never the scale in force: that one lives in
// theme.cpp (CurrentUiScale) and there is no second copy of it.
float g_monitor_scale_x = 1.0f;
float g_monitor_scale_y = 1.0f;

// How the two scale inputs become the two ApplyVisualLanguage parameters, per platform. This is
// the policy half of doc/gui-visual-language.md §4.1's ui_scale; theme.cpp is the mechanism and
// does not know which platform it is on.
//
//   Windows / Linux: GLFW's window coordinates are PHYSICAL pixels (the vendored GLFW makes the
//   process per-monitor DPI aware on Windows unconditionally, win32_init.c), so making the UI
//   larger on a 150% monitor and rasterizing the font denser are the same operation — both are
//   layout_scale = monitor × multiplier, and raster_density stays 1.
//   macOS: window coordinates are POINTS; the OS already draws a 400pt panel the same physical
//   size on every display. What was never right was the atlas: 15px rasterized once and stretched
//   over a 2x backing store. So layout_scale = multiplier alone and the monitor's scale goes to
//   raster_density, where it sharpens glyphs without moving a single layout size.
struct UiScaleParams {
  float layout_scale;
  float raster_density;
};

UiScaleParams ResolveUiScaleParams(float monitor_scale, float user_multiplier) {
#if defined(__APPLE__)
  return { user_multiplier, monitor_scale };
#else
  return { monitor_scale * user_multiplier, 1.0f };
#endif
}

// The rebuild every scale change ends in: style + atlas (theme.cpp), the backend's font texture,
// and the window's floor/size. Runs at the top of the frame — after glfwPollEvents, where the
// content-scale callback fires, and before ImGui::NewFrame, which is the one point at which the
// atlas may be swapped: the GL context is current for the whole loop, and no draw list holds a
// glyph from the old atlas yet. The callback itself only sets the flag (below) for that reason,
// and the user-multiplier path (SetUiScaleMultiplierImmediate, app.cpp) sets the same flag, so
// "the monitor changed" and "the user turned the dial" are one path, not two.
// Style + atlas for the inputs in force, and the log line that says what they resolved to.
UiScaleParams ApplyUiScaleInputs(ImGuiIO& io) {
  IM_ASSERT(g_monitor_scale_x > 0.0f);
  // GLFW reports x and y separately; nothing in this GUI can use an anisotropic scale, and no
  // platform reports one today, so x is the scale and y is a log line if it ever disagrees.
  if (g_monitor_scale_y != g_monitor_scale_x) {
    GUI_LOG_WARNING("[GUI] UI scale: anisotropic content scale {}x{}; using x", g_monitor_scale_x, g_monitor_scale_y);
  }
  const UiScaleParams params = ResolveUiScaleParams(g_monitor_scale_x, gui::g_ui_scale_multiplier);
  gui::ApplyVisualLanguage(io, params.layout_scale, params.raster_density);
  GUI_LOG_INFO("[GUI] UI scale: monitor {} x user {} -> layout {} raster {}", g_monitor_scale_x,
               gui::g_ui_scale_multiplier, params.layout_scale, params.raster_density);
  return params;
}

// The window's floor and size follow layout_scale (PlanWindowSizeForScale): panels grow through
// UiPx(), so a window left at the 1x floor would clip them. The monitor is the one the window sits
// on, not the primary; unknown → INT_MAX, the plan's degrade arm.
void ApplyWindowFloorForScale(GLFWwindow* window, float layout_scale) {
  int cur_w = 0;
  int cur_h = 0;
  glfwGetWindowSize(window, &cur_w, &cur_h);
  gui::MonitorRect mon{};
  const bool have_mon = gui::GetCurrentMonitorWorkArea(window, &mon);
  const gui::WindowSizePlan plan =
      gui::PlanWindowSizeForScale(layout_scale, cur_w, cur_h, have_mon ? mon.w : INT_MAX, have_mon ? mon.h : INT_MAX);
  glfwSetWindowSizeLimits(window, plan.min_w, plan.min_h, GLFW_DONT_CARE, GLFW_DONT_CARE);
  if (plan.grow) {
    // Not marked programmatic on purpose: WindowSizeCallback then drops any aspect preset, which
    // is the truth — the preview region no longer has the ratio the preset promised.
    glfwSetWindowSize(window, plan.target_w, plan.target_h);
  }
  // A window sized to (nearly) the work area still has to SIT in it: the OS places a new window
  // by its own cascade and a grown one keeps its top-left, so either can end with its bottom rows
  // under the taskbar (measured on the 150% reference desktop at a 2.25x layout: status bar
  // hidden). Same clamp ApplyAspectRatio applies after its own resize, frame included.
  if (have_mon) {
    int fl = 0;
    int ft = 0;
    int fr = 0;
    int fb = 0;
    glfwGetWindowFrameSize(window, &fl, &ft, &fr, &fb);
    int px = 0;
    int py = 0;
    glfwGetWindowPos(window, &px, &py);
    int w = 0;
    int h = 0;
    glfwGetWindowSize(window, &w, &h);
    const int nx = std::max(mon.x + fl, std::min(px, mon.x + mon.w - w - fr));
    const int ny = std::max(mon.y + ft, std::min(py, mon.y + mon.h - h - fb));
    if (nx != px || ny != py) {
      glfwSetWindowPos(window, nx, ny);
    }
  }
}

void RebuildForUiScale(GLFWwindow* window, ImGuiIO& io) {
  const UiScaleParams params = ApplyUiScaleInputs(io);
  // The backend's copy of the atlas. Startup does not come through here: the backend uploads its
  // first texture on its own first NewFrame, and a Create before that would be a second GL
  // texture the backend's own upload then orphans.
  ImGui_ImplOpenGL3_DestroyFontsTexture();
  ImGui_ImplOpenGL3_CreateFontsTexture();
  ApplyWindowFloorForScale(window, params.layout_scale);
  gui::g_ui_scale_dirty = false;
}

}  // namespace

int main(int argc, char** argv) {
#if defined(LUMICE_ENGINE_DELAY_LOADED)
  // Windows shared build: the engine is a DLL chosen by CPUID (or `--isa=`) and loaded from
  // this executable's own directory. First thing in main(), BEFORE the FreeConsole() block
  // below, on purpose: a failure here is reported on stderr and in a message box, and while
  // the console is still attached the stderr line is visible too (a console launch reads it,
  // a double-click launch gets the box). Putting it after FreeConsole() would leave the box as
  // the only outlet; putting it any later would let a LUMICE_* call trigger the load from
  // inside the frame loop, where the only outlet is the process dying. The `--isa=` token is
  // consumed here, so the argv scan below never sees it. See src/launcher/win_engine_loader.c.
  if (int rc = LumiceEngineLoaderInit(&argc, argv, LUMICE_ENGINE_LOADER_REPORT_MESSAGE_BOX); rc != 0) {
    return rc;
  }
#endif
#ifdef _WIN32
  // Console subsystem (IMAGE_SUBSYSTEM_WINDOWS_CUI) gives longer thread time slices
  // than GUI subsystem, critical for the 18+ Simulator compute threads (~3.4x throughput
  // difference). For normal GUI launch, release the console so no window is visible.
  // Keep it for diagnostic modes that need stdout/stderr output.
  {
    bool keep_console = false;
    for (int i = 1; i < argc; ++i) {
      std::string_view arg(argv[i]);
      if (arg == "-v" || arg == "-d" || arg == "--log-level" || arg == "--core-log-level") {
        keep_console = true;
        break;
      }
    }
    if (!keep_console) {
      FreeConsole();
    }
  }

  // Raise timer resolution from 15.6ms to ~1ms so that cv_.wait_for() and Sleep()
  // are precise enough for our 20ms poll interval. Without this, SleepConditionVariableSRW
  // rounds up to 3 timer ticks (~47ms), causing a timing race with the 50ms commit interval.
  timeBeginPeriod(1);
#endif

  // Stage 1 of the GUI's log-sink assembly (independent from Core's spdlog; Core logs arrive via
  // the C API callback registered further down next to ConstructServerForState). It runs HERE, before
  // GLFW / GL / ImGui init and after the FreeConsole block above — see gui_logger.hpp, which owns
  // both stages and the reason the ordering is what it is.
  gui::InstallEarlyGuiSinks();

  // The first line of every GUI log: which build wrote everything below it. The same string the
  // title bar shows -- one source (LUMICE_GetVersionString), no second copy in the About/status
  // surfaces. This call (and the window-title one further down) is a LUMICE_* call, so on the
  // Windows shared build it must come after LumiceEngineLoaderInit() above -- it does, since that
  // block is the first thing in main().
  GUI_LOG_INFO("[GUI] Lumice {}", LUMICE_GetVersionString());

  // Parse --user-config / --no-user-config before the first MakeNewDocumentState() call further
  // down — that call is the only place personal defaults enter a session. Passing neither flag
  // keeps today's behavior (auto-detect the OS per-user config directory).
  //
  // The resulting state is logged unconditionally: "which defaults did this machine actually
  // read" is the first question when a document opens with settings the user did not expect,
  // and it is not answerable from the UI.
  {
    const auto parsed = gui::ParseUserConfigArg(argc, argv);
    if (parsed.missing_value) {
      GUI_LOG_WARNING("[GUI] User config: '--user-config' was given without a directory after it; ignoring that flag");
    }
    const gui::UserConfigSource source =
        gui::ResolveUserConfigSource(parsed.presence, gui::kInteractiveAppUserConfigDefault);
    gui::SetUserConfigSourceForProcess(source, parsed.explicit_dir);
    switch (source) {
      case gui::UserConfigSource::kDisabled:
        GUI_LOG_INFO("[GUI] User config: disabled (--no-user-config); new documents use factory defaults only");
        break;
      case gui::UserConfigSource::kExplicitDir:
        GUI_LOG_INFO("[GUI] User config: explicit directory '{}' (--user-config)",
                     lumice::PathToU8(parsed.explicit_dir));
        break;
      case gui::UserConfigSource::kAutoDetect:
        GUI_LOG_INFO("[GUI] User config: auto-detect (OS per-user config directory)");
        break;
    }
  }

  // Parse --skip-calibration flag early.
  bool skip_calibration = false;
  for (int i = 1; i < argc; ++i) {
    std::string_view arg(argv[i]);
    if (arg == "--skip-calibration") {
      skip_calibration = true;
    }
  }

  glfwSetErrorCallback(gui::GlfwErrorCallback);
  if (!glfwInit()) {
    GUI_LOG_ERROR("Failed to initialize GLFW");
    return 1;
  }

  // OpenGL 3.3 Core Profile
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

  // Clamp the requested initial size to what the current monitor can actually
  // display — workarea already excludes OS bars (menubar/Dock/taskbar); we
  // deduct kWindowDecorationMargin for title bar + borders. Without this,
  // kInitWindowHeight=980 can be silently shrunk by the OS on 1080p displays
  // with a large Dock or 125% DPI scaling, spawning a scrollbar on the right
  // panel. Falls through to defaults on headless / nullptr-monitor environments.
  //
  // The creation size is scaled first (doc/gui-visual-language.md §4.1): the user's multiplier is
  // read from disk here, before the window exists, and the primary monitor's content scale is
  // the best estimate of the one the window will land on — an estimate for the same reason the
  // work-area clamp below is one (multi-monitor: the window may open elsewhere), corrected by the
  // authoritative glfwGetWindowContentScale read once the window exists. Both go through the one
  // PlanWindowSizeForScale rule the runtime rebuild uses, so creation and rescale cannot disagree.
  gui::g_ui_scale_multiplier = gui::LoadUiScaleMultiplierAtStartup();
  int init_w = gui::kInitWindowWidth;
  int init_h = gui::kInitWindowHeight;
  {
    float startup_monitor_scale = 1.0f;
    int ww = INT_MAX;
    int wh = INT_MAX;
    if (GLFWmonitor* primary = glfwGetPrimaryMonitor()) {
      float xscale = 1.0f;
      float yscale = 1.0f;
      glfwGetMonitorContentScale(primary, &xscale, &yscale);
      if (xscale > 0.0f) {
        startup_monitor_scale = xscale;
      }
      int wx = 0;
      int wy = 0;
      int mw = 0;
      int mh = 0;
      glfwGetMonitorWorkarea(primary, &wx, &wy, &mw, &mh);
      if (mw > 0 && mh > 0) {
        ww = mw;
        wh = mh;
      }
    }
    const float startup_layout_scale =
        ResolveUiScaleParams(startup_monitor_scale, gui::g_ui_scale_multiplier).layout_scale;
    const gui::WindowSizePlan plan = gui::PlanWindowSizeForScale(
        startup_layout_scale, static_cast<int>(std::lround(gui::kInitWindowWidth * startup_layout_scale)),
        static_cast<int>(std::lround(gui::kInitWindowHeight * startup_layout_scale)), ww, wh);
    init_w = plan.target_w;
    init_h = plan.target_h;
  }
  // "Lumice <version>": beta users run several builds side by side, and a bare "Lumice" on every
  // window is how they got mixed up. GLFW copies the title, so the string need not outlive the
  // call.
  const std::string window_title = std::string("Lumice ") + LUMICE_GetVersionString();
  GLFWwindow* window = glfwCreateWindow(init_w, init_h, window_title.c_str(), nullptr, nullptr);
  if (!window) {
    GUI_LOG_ERROR("Failed to create GLFW window");
    glfwTerminate();
    return 1;
  }

  // The authoritative monitor scale, now that there is a window to ask about. This is the explicit
  // initialisation of g_monitor_scale_*: the callback below only ever REPLACES it, so a user-
  // multiplier change on a machine whose monitor never changed still rebuilds against a real
  // scale rather than the static default. Size limits are set below by ApplyWindowFloorForScale
  // (not RebuildForUiScale, which is deliberately skipped at startup — see the comment at its
  // call site a few lines down), from the same PlanWindowSizeForScale rule the creation size came
  // from.
  glfwGetWindowContentScale(window, &g_monitor_scale_x, &g_monitor_scale_y);
  if (!(g_monitor_scale_x > 0.0f)) {
    g_monitor_scale_x = 1.0f;
    g_monitor_scale_y = 1.0f;
  }
  glfwSetWindowContentScaleCallback(window, [](GLFWwindow*, float xscale, float yscale) {
    // Inside glfwPollEvents: not a frame boundary, so only record and flag. The rebuild is the
    // first thing the next frame does.
    if (xscale > 0.0f) {
      g_monitor_scale_x = xscale;
      g_monitor_scale_y = yscale;
      gui::g_ui_scale_dirty = true;
    }
  });
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);  // VSync on

  if (!gui::InitGLLoader()) {
    glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }

  // imgui setup
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  // Multi-viewport: lets Immediate-mode Edit Entry be dragged outside main window.
  // Staged BeginPopupModal keeps main-viewport constraint by ImGui semantics.
  io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
  io.IniFilename = nullptr;  // Disable imgui.ini persistence (also suppresses viewport position persistence)

  // Style, atlas and the window's floor for the authoritative scale — the same two steps
  // RebuildForUiScale takes at every later change, minus the texture re-upload the backend has not
  // made yet. If the primary-monitor estimate the creation size used was wrong (the window opened
  // on another monitor), the floor step grows the window here, before the first frame.
  ApplyWindowFloorForScale(window, ApplyUiScaleInputs(io).layout_scale);

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330");

  // Personal defaults apply to a NEW document only (invariant I1) — startup shows one.
  gui::g_state = gui::MakeNewDocumentState();
  gui::SurfaceUserDefaultsDowngrades();

  // skip_calibration already parsed above (before glfwSwapInterval).

  // Create the Lumice server — for the backend and worker count the document asks for, not a
  // CPU default to be swapped out on the first Run. Both are construction-time properties (see
  // MaybeReconstructServerForConstructionProperties), so a server built for the wrong one is
  // torn down and rebuilt at that first Run, and everything the calibration below warmed up on
  // it — the GPU context above all — would be thrown away with it. use_gpu_backend arrives from
  // the personal defaults in MakeNewDocumentState above; the factory default is CPU. Verified
  // (code review round 1, Suggestion): MakeNewDocumentState's user_defaults load path
  // (user_defaults.cpp, kUseGpuBackendKey) always finishes and returns before this call — it is
  // synchronous, called on the line directly above, not deferred to a later frame.
  gui::ConstructServerForState(gui::g_state);

  // Stage 2 of the log-sink assembly: the file sink, which cannot be hoisted into stage 1
  // because constructing it truncates the log file. See gui_logger.hpp.
  gui::AttachGuiFileSink();

  // Bridge Core logs into the GUI ring buffer. Kept here rather than in the sink
  // block at the top of main(): the callback only carries meaning once a server
  // exists to emit Core logs, so it pairs with ConstructServerForState above.
  LUMICE_SetLogCallback([](LUMICE_LogLevel level, const char* /*name*/, const char* message) {
    if (gui::g_imgui_log_sink) {
      auto spd_level = static_cast<spdlog::level::level_enum>(level);
      gui::g_imgui_log_sink->ReceiveExternal(spd_level, message);
    }
  });

  // Parse CLI arguments for log level.
  // --log-level / -v / -d control GUI log level (global logger, LOG_* macros).
  // --core-log-level controls Core log level (server logger, ILOG_* macros).
  // Default: both warn. -v sets GUI to info, -d sets GUI to debug.
  {
    auto parse_level = [](std::string_view s) -> LUMICE_LogLevel {
      if (s == "trace")
        return LUMICE_LOG_TRACE;
      if (s == "debug")
        return LUMICE_LOG_DEBUG;
      if (s == "verbose")
        return LUMICE_LOG_VERBOSE;
      if (s == "info")
        return LUMICE_LOG_INFO;
      if (s == "warn" || s == "warning")
        return LUMICE_LOG_WARNING;
      if (s == "error")
        return LUMICE_LOG_ERROR;
      if (s == "off")
        return LUMICE_LOG_OFF;
      return LUMICE_LOG_WARNING;
    };

    LUMICE_LogLevel gui_level = LUMICE_LOG_INFO;
    LUMICE_LogLevel core_level = LUMICE_LOG_WARNING;
    for (int i = 1; i < argc; ++i) {
      std::string_view arg(argv[i]);
      if (arg == "-v") {
        gui_level = LUMICE_LOG_VERBOSE;
      } else if (arg == "-d") {
        gui_level = LUMICE_LOG_DEBUG;
      } else if (arg == "--log-level" && i + 1 < argc) {
        gui_level = parse_level(argv[++i]);
      } else if (arg == "--core-log-level" && i + 1 < argc) {
        core_level = parse_level(argv[++i]);
      }
    }
    // Set core level via C API, GUI level via GUI logger
    LUMICE_SetLogLevel(gui::g_server, core_level);
    gui::SetGuiLogLevel(static_cast<spdlog::level::level_enum>(gui_level));
    // Sync panel dropdowns with CLI-set levels
    gui::g_state.gui_log_level = static_cast<int>(gui_level);
    gui::g_state.core_log_level = static_cast<int>(core_level);
  }

  // Initialize preview renderer
  if (!gui::g_preview.Init()) {
    GUI_LOG_ERROR("Failed to initialize preview renderer");
    return 1;
  }

  // Initialize crystal renderer (512x512 FBO — supersamples the 320px modal preview)
  if (!gui::g_crystal_renderer.Init(512, 512)) {
    GUI_LOG_ERROR("Failed to initialize crystal renderer");
    return 1;
  }
  // Initialize the modal preview's trackball to the default entry's preset
  // default view (matches what Reset View / the entry-card thumbnail show).
  // Without this the user would have to click Reset View on first modal open
  // to reach the same view the outer thumbnail already shows.
  if (!gui::g_state.layers.empty() && !gui::g_state.layers[0].entries.empty()) {
    gui::ResetCrystalViewToCrystal(gui::g_state.crystals[gui::g_state.layers[0].entries[0].crystal_id]);
  } else {
    gui::ResetCrystalView();  // legacy fallback for unexpected empty state
  }

  // Initialize thumbnail cache (must be after GL context is ready)
  if (!gui::g_thumbnail_cache.Init()) {
    GUI_LOG_ERROR("Failed to initialize thumbnail cache");
    return 1;
  }

  // Calibrate quality gate threshold by running a short simulation with default config — which
  // is also what warms the server's backend up for the first Run. Must happen after server
  // creation; it hands the run to a background thread and returns, so the main loop starts
  // without waiting for it (the first Run, and anything that wakes the poller, joins it).
  if (!skip_calibration) {
    gui::CalibrateQualityThreshold();
  }

  // Window size callback: detect user manual resize vs programmatic resize
  glfwSetWindowSizeCallback(window, gui::WindowSizeCallback);

  // Window close callback: intercept to check for unsaved changes
  glfwSetWindowCloseCallback(window, [](GLFWwindow* w) {
    if (gui::g_state.dirty) {
      glfwSetWindowShouldClose(w, GLFW_FALSE);
      gui::g_pending_action = gui::PendingAction::kQuit;
      gui::g_show_unsaved_popup = true;
    }
  });

  // Main loop
  while (!glfwWindowShouldClose(window)) {
    auto frame_start = std::chrono::steady_clock::now();
    glfwPollEvents();

    // Sync data from background server poller (non-blocking)
    gui::SyncFromPoller();

    // Live-edit: auto-commit config when parameters change during simulation.
    // DoRun builds a LUMICE_Scene and calls LUMICE_CommitScene (no JSON string roundtrip).
    // Throttled to at most once per kCommitIntervalMs.
    {
      static auto last_commit = std::chrono::steady_clock::now();
      if (gui::g_state.dirty) {
        auto ss = gui::g_state.sim_state;
        // Only auto-commit while actively simulating. The old `|| kDone` clause was vestigial:
        // any dirty edit on a kDone result reconciles to kModified the same frame, so kDone&&dirty
        // is never observed here. Under the single-owner reconcile, keeping it would risk auto-
        // rerunning a completed-then-edited result instead of the intended kModified + Revert UX.
        if (ss == gui::GuiState::SimState::kSimulating) {
          auto now = std::chrono::steady_clock::now();
          auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_commit).count();
          if (elapsed >= gui::kCommitIntervalMs) {
            // task-metal-gui-commit-backpressure: dirty is cleared iff DoRun actually
            // pushed the commit. When the gate defers (Metal first batch not yet landed),
            // DoRun returns false; keep dirty=true so the next 70ms tick retries with
            // g_state's latest edits. `last_commit` always advances so the check cadence
            // is unchanged (still 70ms retry window, plan §4 Step 4).
            //
            // ⚠️ MIRROR: test/gui/test_gui_main.cpp (g_enable_main_loop_commit path) and
            // test/gui/responsiveness/test_gui_perf.cpp (slider_drag scenario) copy this
            // same throttle+accounting block. Any change here MUST be mirrored there — the
            // gated-vs-committed distinction affects restart counting and rays accounting.
            bool committed = gui::DoRun(/*user_initiated=*/false);
            if (committed) {
              gui::g_state.dirty = false;
            }
            last_commit = now;
          }
        }
      }
    }

    // Keyboard shortcuts
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
      if (io.KeyShift) {
        gui::DoSaveAs();
      } else {
        gui::DoSave();
      }
    }
    // Save panel collapse state before any mutations (keyboard shortcuts + button clicks during rendering).
    bool prev_left_collapsed = gui::g_state.left_panel_collapsed;
    bool prev_right_collapsed = gui::g_state.right_panel_collapsed;

    // Panel collapse shortcuts: [ for left panel, ] for right panel
    if (!io.WantCaptureKeyboard) {
      if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket)) {
        gui::g_state.left_panel_collapsed = !gui::g_state.left_panel_collapsed;
      }
      if (ImGui::IsKeyPressed(ImGuiKey_RightBracket)) {
        gui::g_state.right_panel_collapsed = !gui::g_state.right_panel_collapsed;
      }
    }

    // A scale change — the monitor's (callback, during glfwPollEvents above) or the user's
    // (Settings, last frame) — lands here, before this frame's NewFrame. See RebuildForUiScale.
    if (gui::g_ui_scale_dirty) {
      RebuildForUiScale(window, io);
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Framebuffer size for glViewport (may differ from window size on HiDPI)
    int display_w = 0;
    int display_h = 0;
    glfwGetFramebufferSize(window, &display_w, &display_h);

    // Window size for imgui layout (logical pixels)
    int win_w = 0;
    int win_h = 0;
    glfwGetWindowSize(window, &win_w, &win_h);
    auto layout_width = static_cast<float>(win_w);
    auto layout_height = static_cast<float>(win_h);

    gui::RenderTopBar(layout_width);
    gui::RenderLeftPanel(layout_height);
    gui::RenderRightPanel(window, layout_width, layout_height);
    gui::RenderPreviewPanel(window, layout_width, layout_height);
    gui::RenderLogPanel(layout_width, layout_height);
    gui::RenderColorWindow(gui::g_state, gui::g_server);
    gui::RenderAnalysisPanel(gui::g_state, gui::g_server);
    gui::RenderConfigSummaryWindow(gui::g_state);
    gui::RenderStatusBar(layout_width, layout_height);
    gui::RenderEditModals(gui::g_state, window);
    gui::RenderSpectrumModal(gui::g_state);
    gui::RenderUnsavedPopup(window);
    gui::RenderSaveModifiedPopup(window);
    gui::RenderDefaultsPanel(gui::g_state);
    gui::RenderImportWarningPopup();
    gui::RenderExportOverwriteConfirmPopup();
    gui::RenderGuiWarningPopup();

    // Reset aspect ratio to Free when panel collapse state changes (window size doesn't adjust automatically).
    if (gui::g_state.left_panel_collapsed != prev_left_collapsed ||
        gui::g_state.right_panel_collapsed != prev_right_collapsed) {
      gui::g_state.aspect_preset = gui::AspectPreset::kFree;
    }

    // Field-tier effect reconcile (scrum-gui-state-reconcile T0, M6). Runs at frame TAIL — after all
    // widget-writing Render*() calls have executed and before ImGui::Render() — so a widget edit in
    // this frame lands in state.dirty this same frame, matching the legacy DIRTY_IF wrappers'
    // synchronous semantics. Placing it at frame top (inside SyncFromPoller) would leave a
    // one-frame delay behind widget writes. Coexists idempotently with legacy DIRTY_IF sites during
    // T1-T4 migration; see gui_state_reconcile.hpp for the field participation set.
    gui::ApplyGuiEffects(gui::g_state, gui::g_server, gui::ReconcileGuiEffects(gui::g_state));

    // Rendering. Everything from the viewport/clear through submitting ImGui's draw data lives in
    // RunSharedFrameRenderPass, which gui_test's frame loop calls too — that sequence has exactly
    // one implementation, so it cannot drift between the app and the harness. ImGui::Render() and
    // the display_w/display_h query above stay here on purpose (see the function's doc comment).
    ImGui::Render();
    gui::RunSharedFrameRenderPass(display_w, display_h);

    // Update and render additional platform windows (multi-viewport).
    // Must save/restore current GL context because RenderPlatformWindowsDefault
    // makes each viewport's context current in turn; without restore the main
    // window would draw into the wrong context on the next frame.
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
      GLFWwindow* backup_current_context = glfwGetCurrentContext();
      ImGui::UpdatePlatformWindows();
      ImGui::RenderPlatformWindowsDefault();
      glfwMakeContextCurrent(backup_current_context);
    }

    // Screenshot exports and the on-screen preview above share ONE render core
    // (RenderFrameContentToBoundFbo, export_fbo_renderer.cpp): the screen blits the FBO back into
    // this framebuffer, the export reads the same FBO's pixels. No deferred default-framebuffer
    // capture is needed or wanted here — the old pending_screenshot hook that read back from the
    // default FB was retired because it unavoidably captured ImGui chrome (e.g. an open Save menu)
    // together with the preview.

    glfwSwapBuffers(window);

    // Fallback frame rate limit: prevents busy-wait when VSync fails
    // (known issue on Windows+NVIDIA, GLFW #1559/#2049).
    // When VSync works, SwapBuffers already blocks ~16ms so this sleep is skipped.
    auto frame_end = std::chrono::steady_clock::now();
    auto frame_ms = std::chrono::duration_cast<std::chrono::milliseconds>(frame_end - frame_start).count();
    if (frame_ms < gui::kTargetFrameTimeMs) {
      std::this_thread::sleep_for(std::chrono::milliseconds(gui::kTargetFrameTimeMs - frame_ms));
    }
  }

  // Cleanup
  gui::JoinPendingCalibration();  // R1: a calibration still running (quit within its first ~200ms)
  gui::JoinPendingStop();         // R1: drain any in-flight async Stop before tearing down the server
  gui::g_server_poller.Stop();    // Stop poller before destroying server
  gui::g_crystal_renderer.Destroy();
  gui::g_preview.Destroy();
  gui::DestroyPreviewFrameFbo();  // the preview path's persistent FBO, owned by export_fbo_renderer.cpp
  LUMICE_DestroyServer(gui::g_server);
  gui::g_server = nullptr;

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();

#ifdef _WIN32
  timeEndPeriod(1);
#endif
  return 0;
}
