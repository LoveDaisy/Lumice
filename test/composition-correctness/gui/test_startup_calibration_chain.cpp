// Composition chain: the startup calibration run as the first Run's warm-up.
//
// Units in the chain: app × server_poller × the server on the far side of the C API.
//
// What the collaboration produces that is observable: the first Run a user presses lands on a
// server whose backend, consumer layout and process-level one-time costs were already paid by the
// calibration run that started before the window did — and nothing of that run ever reaches the
// screen. Two things had to be true at once for that, and each is pinned here from the other's
// side:
//   - the server the calibration warms up is the server the first Run uses. The backend and the
//     worker count are construction-time properties: a server built for the wrong ones is torn
//     down at the first Run, and the warm-up with it. So startup has to build from the document,
//     through the same mapping the property-change reconstruction builds from;
//   - the calibration is a real run on that server, on a background thread, and stays invisible
//     only because the poller — the single publisher of what the preview shows — is never woken
//     while it is in flight. Every path that starts or wakes the poller, and every path that
//     destroys the server, joins the run first. The failure the join guards against has no tell:
//     a preview quietly showing the default document at 100k rays with no Run pressed looks like
//     a feature, and a server freed under a thread still committing to it is a crash that only
//     happens when the user is fast.
//
// The propositions need a live server (the run is real) but no window and no frame: the join is
// the whole subject, and it is observable through CalibrationPending, the server's state and the
// poller's published snapshot.

#include <gtest/gtest.h>

#include <memory>
#include <utility>

#include "gui/app.hpp"
#include "gui/file_io.hpp"
#include "gui/gui_state.hpp"
#include "gui/server_poller.hpp"
#include "lumice.h"
#include "support/scoped_result_frame.hpp"

namespace lumice::gui {
namespace {

// The app's server global for the duration of one case, built the way the test harnesses build
// it (LUMICE_CreateServer + a tracker reset) unless a case constructs its own. The join in the
// destructor is what lets an EXPECT failure mid-case still tear down without freeing the server
// under the calibration thread — the same order production takes at shutdown.
class ScopedAppServer {
 public:
  ScopedAppServer() {
    DoNew();
    ClearGuiWarning();
    g_server = LUMICE_CreateServer();
    ResetServerConstructionTrackers();
  }
  ~ScopedAppServer() {
    JoinPendingCalibration();
    JoinPendingStop();
    g_server_poller.Stop();
    if (g_server != nullptr) {
      LUMICE_StopServer(g_server);
      LUMICE_DestroyServer(g_server);
      g_server = nullptr;
    }
    ResetServerConstructionTrackers();
    ClearGuiWarning();
    g_state.run_intent = RunIntent::kNone;
    g_state.committed_epoch = 0;
    g_state.dirty = false;
  }
  ScopedAppServer(const ScopedAppServer&) = delete;
  ScopedAppServer& operator=(const ScopedAppServer&) = delete;
};

LUMICE_ServerState QueryState(LUMICE_Server* server) {
  LUMICE_ServerState state = LUMICE_SERVER_RUNNING;
  LUMICE_QueryServerState(server, &state);
  return state;
}

// The current document's commit scene at a chosen ray budget — what CalibrateQualityThreshold
// builds, with the budget in the caller's hands so a case can make the run outlast its assertion.
ScenePtr CalibrationSceneWithRays(int rays) {
  ScenePtr scene = BuildScene(g_state, SceneIntent::kSimCommit);
  if (scene) {
    LUMICE_SceneSetSimParams(scene.get(), /*infinite=*/0, rays, g_state.sim.max_hits, /*geom_clock=*/0);
  }
  return scene;
}

}  // namespace

// ---------------------------------------------------------------------------------------------
// The mapping from the document to the server configuration, over both construction-time
// properties. The GPU row compares against ResolveGpuBackend() on this very host rather than
// against a named backend: which GPU (or none) a machine has is not the proposition.

TEST(StartupCalibrationChain, ServerConfigFollowsTheDocumentsConstructionProperties) {
  GuiState state = InitDefaultState();

  state.use_gpu_backend = false;
  state.worker_count = 3;
  LUMICE_ServerConfig cpu = ServerConfigForState(state);
  EXPECT_EQ(cpu.preferred_backend, LUMICE_BACKEND_CPU);
  EXPECT_EQ(cpu.num_workers, 3);
  EXPECT_EQ(cpu.sim_seed, 0u);

  state.use_gpu_backend = true;
  state.worker_count = 0;
  LUMICE_ServerConfig gpu = ServerConfigForState(state);
  EXPECT_EQ(gpu.preferred_backend, ResolveGpuBackend());
  EXPECT_EQ(gpu.num_workers, 0);
  EXPECT_EQ(gpu.sim_seed, 0u);
}

// A server built for the document is the server the first Run keeps: the reconstruction check
// DoRun performs finds nothing to change. This is the whole point of building from the document
// at startup — under LUMICE_CreateServer() a GPU-preferring document was rebuilt here, and the
// calibration's warm-up was discarded with the old server.
TEST(StartupCalibrationChain, AServerBuiltForTheDocumentIsNotRebuiltAtTheFirstRun) {
  struct Row {
    const char* name;
    bool use_gpu;
    int workers;
  };
  const Row kRows[] = {
    { "CPU, automatic worker count", false, 0 },
    { "CPU, an explicit worker count", false, 2 },
    // Resolves to CPU on a host without a GPU; the tracker still records the request, which is
    // what DoRun compares the document against, so the row holds either way.
    { "GPU preferred", true, 0 },
  };
  for (const Row& row : kRows) {
    ScopedAppServer scoped;
    LUMICE_DestroyServer(g_server);  // the fixture's harness-style server; this case builds its own
    g_state.use_gpu_backend = row.use_gpu;
    g_state.worker_count = row.workers;
    ConstructServerForState(g_state);
    if (g_server == nullptr) {
      ADD_FAILURE() << row.name << ": no server constructed";
      continue;
    }
    LUMICE_Server* built = g_server;
    EXPECT_EQ(g_server_is_gpu, row.use_gpu) << row.name;
    EXPECT_EQ(g_server_worker_count, row.workers) << row.name;

    EXPECT_FALSE(MaybeReconstructServerForConstructionProperties()) << row.name;
    EXPECT_EQ(g_server, built) << row.name;
  }
}

// ---------------------------------------------------------------------------------------------
// The calibration run itself: it runs (the server has data afterwards), it leaves the server
// IDLE, and the poller was never woken by it — the published snapshot is the same object before
// and after, which is the one observable a poll cannot leave unchanged. Then the first real Run
// is accepted on that same server, which is the user-visible half of "warmed up, not wedged".

TEST(StartupCalibrationChain, TheWarmUpRunsToIdleWithoutWakingThePoller) {
  ScopedAppServer scoped;
  const std::shared_ptr<const PreviewSnapshot> before = g_server_poller.LoadSnapshot();

  CalibrateQualityThreshold();
  // Handed off, not run inline: the scene build returned while the run is still the background
  // thread's.
  EXPECT_TRUE(CalibrationPending());
  JoinPendingCalibration();
  EXPECT_FALSE(CalibrationPending());

  EXPECT_EQ(QueryState(g_server), LUMICE_SERVER_IDLE);
  {
    LUMICE_StatsResult stats{};
    lumice::test::ScopedResultFrame frame(g_server);
    LUMICE_FrameGetStats(frame.get(), &stats);
    EXPECT_GT(stats.sim_ray_num, 0ULL) << "the calibration did not run";
  }
  EXPECT_EQ(g_server_poller.LoadSnapshot(), before) << "the poller published during the calibration";

  // The first real Run lands on the warmed-up server.
  EXPECT_TRUE(DoRun(/*user_initiated=*/true));
  EXPECT_NE(QueryState(g_server), LUMICE_SERVER_IDLE);
}

// A second dispatch while one is in flight joins the first rather than dropping its handle: the
// future is single-owner, exactly as the async Stop's is.
TEST(StartupCalibrationChain, ASecondDispatchJoinsTheFirst) {
  ScopedAppServer scoped;
  RunCalibrationInBackground(CalibrationSceneWithRays(5'000'000));
  ASSERT_TRUE(CalibrationPending());
  RunCalibrationInBackground(CalibrationSceneWithRays(1'000));
  EXPECT_TRUE(CalibrationPending());
  JoinPendingCalibration();
  EXPECT_EQ(QueryState(g_server), LUMICE_SERVER_IDLE);
}

// ---------------------------------------------------------------------------------------------
// The race the joins exist for: a construction-time property changes — the user flips the GPU
// toggle and presses Run — while the calibration is still committing to the server that is about
// to be destroyed. The run is given a budget that cannot complete before the reconstruction is
// asked for (5M rays; the reconstruction call follows the dispatch by microseconds), so the
// reconstruction meets a calibration genuinely in flight, and has to have waited for it — the
// pending handle is gone when it returns — before the old server was freed. Without that wait
// the handle is still pending on return, and the thread is committing to freed memory.

TEST(StartupCalibrationChain, AReconstructionWhileTheWarmUpIsRunningWaitsForIt) {
  ScopedAppServer scoped;
  LUMICE_Server* const warmed = g_server;
  RunCalibrationInBackground(CalibrationSceneWithRays(5'000'000));
  ASSERT_TRUE(CalibrationPending());

  g_state.worker_count = g_server_worker_count + 1;  // a construction-time property moved
  EXPECT_TRUE(MaybeReconstructServerForConstructionProperties());
  EXPECT_FALSE(CalibrationPending()) << "the old server was destroyed with the calibration still on it";
  ASSERT_NE(g_server, nullptr);
  EXPECT_NE(g_server, warmed);
  EXPECT_EQ(QueryState(g_server), LUMICE_SERVER_IDLE);
}

}  // namespace lumice::gui
