#include "gui/analyze_survey.hpp"

#include <cstdio>
#include <utility>

#include "file_io.hpp"  // IWYU pragma: keep — SceneDeleter's definition (ScenePtr's owner)
#include "gui/gui_logger.hpp"

namespace lumice::gui {

namespace {

// R6: every query carries a bounded deadline, spelled as one explicit constant at the engine's
// default level. The request body carries the same value (MakeSurveyRequestForIdentity sets
// budget_ms from the same constant), so budget and deadline cannot drift apart by construction.
constexpr int kSurveyQueryDeadlineMs = LUMICE_PATH_SURVEY_DEFAULT_BUDGET_MS;

struct TokenDeleter {
  void operator()(LUMICE_PathCancelToken* token) const { LUMICE_PathCancelTokenDestroy(token); }
};
using TokenPtr = std::shared_ptr<LUMICE_PathCancelToken>;

struct WorkspaceDeleter {
  void operator()(LUMICE_PathWorkspace* workspace) const { LUMICE_PathWorkspaceDestroy(workspace); }
};
using WorkspacePtr = std::unique_ptr<LUMICE_PathWorkspace, WorkspaceDeleter>;

}  // namespace

SurveyOverviewState SurveyOverviewStateFor(const SurveyQuerySession& survey) {
  if (!survey.intent_valid) {
    return SurveyOverviewState::kNoSelection;
  }
  if (survey.cancelled_for_key) {
    return SurveyOverviewState::kCancelled;
  }
  if (survey.adopted_result) {
    return SurveyOverviewState::kCompleted;
  }
  if (!survey.adopted_error.empty()) {
    return SurveyOverviewState::kRefused;
  }
  return SurveyOverviewState::kWaiting;
}

// ---- The worker ------------------------------------------------------------------------------------

struct SurveyExecutor::Impl {
  struct Job {
    enum class Kind { kSetScene, kReleaseScene, kQuery, kShutdown };
    Kind kind = Kind::kShutdown;
    std::unique_ptr<LUMICE_Scene, SceneDeleter> scene;  // kSetScene: ownership handed over
    SurveyQueryId id;                                   // kQuery
    LUMICE_PathSurveyRequest request{};                 // kQuery; pointers rebound to the buffers
    std::vector<int> faces;                             // kQuery: the buffers outlive the query
    std::vector<int> counts;                            // kQuery
    TokenPtr token;                                     // kQuery: the cancel handle, live from issue
  };

  // -- guarded by mu --
  std::mutex mu;
  std::condition_variable cv;
  // Two slots, not one queue: a binding job (SetScene/Release — the latest wins) and a query job
  // (the latest intent wins) each survive the other's submission. One shared slot would let an
  // IssueQuery arriving between a SetScene and the worker's wake SWALLOW the binding swap — the
  // query would then run against the destroyed (or worse, the previous document's) scene. The
  // worker applies the binding before the query, so an intent issued for the new generation
  // never waits behind the swap and never runs without it.
  std::optional<Job> pending_binding;  // kSetScene / kReleaseScene / kShutdown (latest wins)
  std::optional<Job> pending_query;    // kQuery (latest wins)
  TokenPtr inflight_token;             // non-null exactly while a query is between issue and finish
  std::optional<Finished> finished;    // single slot: the newest finish overwrites an older one
  unsigned long long next_serial = 0;
  bool joined = false;

  // -- worker thread only (destroyed there, never under the GUI) --
  std::unique_ptr<LUMICE_Scene, SceneDeleter> scene;
  WorkspacePtr workspace;

  std::thread thread;
};

SurveyExecutor::SurveyExecutor() : impl_(std::make_unique<Impl>()) {
  impl_->thread = std::thread([impl = impl_.get()] {
    std::unique_lock<std::mutex> lock(impl->mu);
    for (;;) {
      impl->cv.wait(lock, [impl] { return impl->pending_binding.has_value() || impl->pending_query.has_value(); });

      // Binding first (a queued query always runs against the binding it was issued for — or,
      // for one issued across a swap, against the new binding, and the pump's gate judges the
      // outcome). A kShutdown in this slot drops any queued query by falling through to return.
      if (impl->pending_binding.has_value()) {
        Impl::Job binding = std::move(*impl->pending_binding);
        impl->pending_binding.reset();
        if (binding.kind == Impl::Job::Kind::kShutdown) {
          impl->workspace.reset();
          impl->scene.reset();
          return;  // the unique_lock's destructor releases the mutex; ShutdownAndJoin then joins
        }
        if (binding.kind == Impl::Job::Kind::kSetScene) {
          // The submitter cancelled any in-flight query before queuing this, so nothing reads
          // the old binding here — the destroy-on-worker guarantee the class comment states.
          impl->workspace.reset();
          impl->scene = std::move(binding.scene);
          if (impl->scene) {
            LUMICE_PathWorkspace* raw = nullptr;
            char err[256] = {};
            if (LUMICE_PathWorkspaceCreate(impl->scene.get(), &raw, err, sizeof(err)) == LUMICE_OK) {
              impl->workspace.reset(raw);
            } else {
              GUI_LOG_WARNING("[SurveyExecutor] workspace create failed: {} — queries refuse until the next Analyze",
                              err);
            }
          }
        } else {  // kReleaseScene
          impl->workspace.reset();
          impl->scene.reset();
        }
      }

      if (!impl->pending_query.has_value()) {
        continue;
      }
      Impl::Job job = std::move(*impl->pending_query);
      impl->pending_query.reset();
      // kQuery. Run off the lock — the loop's unique_lock is released around the compute (it is
      // the one long-running section; every other branch stays under the lock) and retaken to
      // post the finish. Releasing is not optional hygiene: the post below would lock the
      // non-recursive mutex a second time from the same thread and deadlock against itself.
      LUMICE_PathSurveyRequest request = job.request;
      request.faces = job.faces.data();
      request.layer_face_counts = job.counts.data();
      LUMICE_PathQueryOptions options{};
      options.struct_size = sizeof(options);
      options.deadline_ms = kSurveyQueryDeadlineMs;
      options.cancel = job.token.get();
      LUMICE_PathSurvey* raw = nullptr;
      char err[512] = {};
      LUMICE_ErrorCode err_code = LUMICE_OK;
      lock.unlock();
      if (impl->workspace) {
        err_code = LUMICE_PathWorkspaceQuerySurvey(impl->workspace.get(), &request, &options, &raw, err, sizeof(err));
      } else {
        // The binding was released or its workspace build failed under this query: name it
        // honestly instead of a fabricated engine refusal (R7). The gate drops it anyway
        // whenever the caller also reset the intent, which is every document-switch path.
        err_code = LUMICE_ERR_UNKNOWN;
        std::snprintf(err, sizeof(err), "scene_released: the analysis scene binding is gone");
      }
      lock.lock();
      {
        impl->inflight_token.reset();
        impl->finished =
            Finished{ job.id, err_code == LUMICE_OK, std::string(err), SurveyHandle(raw, SurveyDeleter{}) };
      }
    }
  });
}

SurveyExecutor::~SurveyExecutor() {
  ShutdownAndJoin();
}

SurveyQueryId SurveyExecutor::NextId(unsigned long long generation) {
  std::lock_guard<std::mutex> lock(impl_->mu);
  return SurveyQueryId{ generation, ++impl_->next_serial };
}

void SurveyExecutor::SetScene(std::unique_ptr<LUMICE_Scene, SceneDeleter> scene) {
  if (!scene) {
    return;
  }
  Impl::Job job;
  job.kind = Impl::Job::Kind::kSetScene;
  job.scene = std::move(scene);
  std::lock_guard<std::mutex> lock(impl_->mu);
  if (impl_->inflight_token) {
    LUMICE_PathCancelTokenRequest(impl_->inflight_token.get());
  }
  impl_->pending_binding = std::move(job);
  impl_->cv.notify_one();
}

void SurveyExecutor::IssueQuery(SurveyQueryId id, const LUMICE_PathSurveyRequest& request, std::vector<int> faces,
                                std::vector<int> counts) {
  Impl::Job job;
  job.kind = Impl::Job::Kind::kQuery;
  job.id = id;
  job.request = request;
  job.faces = std::move(faces);
  job.counts = std::move(counts);
  job.request.faces = job.faces.data();
  job.request.layer_face_counts = job.counts.data();
  {
    LUMICE_PathCancelToken* raw = nullptr;
    if (LUMICE_PathCancelTokenCreate(&raw) == LUMICE_OK) {
      job.token.reset(raw, TokenDeleter{});
    }
    // Token creation cannot realistically fail; if it did, the query still runs deadline-bounded
    // (options.cancel is then null — the engine treats it as uncancellable, not as an error).
  }
  std::lock_guard<std::mutex> lock(impl_->mu);
  if (impl_->inflight_token) {
    LUMICE_PathCancelTokenRequest(impl_->inflight_token.get());  // R6: the new intent supersedes the old
  }
  impl_->inflight_token = job.token;
  impl_->pending_query = std::move(job);
  impl_->cv.notify_one();
}

void SurveyExecutor::CancelInFlight() {
  std::lock_guard<std::mutex> lock(impl_->mu);
  if (impl_->inflight_token) {
    LUMICE_PathCancelTokenRequest(impl_->inflight_token.get());
  }
  impl_->pending_query.reset();  // the query only — a queued binding swap must survive a cancel
}

void SurveyExecutor::ReleaseScene() {
  Impl::Job job;
  job.kind = Impl::Job::Kind::kReleaseScene;
  std::lock_guard<std::mutex> lock(impl_->mu);
  if (impl_->inflight_token) {
    LUMICE_PathCancelTokenRequest(impl_->inflight_token.get());
  }
  impl_->pending_binding = std::move(job);
  impl_->cv.notify_one();
}

std::optional<SurveyExecutor::Finished> SurveyExecutor::TakeFinished() {
  std::lock_guard<std::mutex> lock(impl_->mu);
  std::optional<Finished> out;
  if (impl_->finished) {
    out = std::move(*impl_->finished);
    impl_->finished.reset();
  }
  return out;
}

bool SurveyExecutor::InFlight() const {
  std::lock_guard<std::mutex> lock(impl_->mu);
  return impl_->inflight_token != nullptr;
}

void SurveyExecutor::ShutdownAndJoin() {
  if (!impl_) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(impl_->mu);
    if (impl_->joined) {
      return;
    }
    impl_->joined = true;
    if (impl_->inflight_token) {
      LUMICE_PathCancelTokenRequest(impl_->inflight_token.get());
    }
    Impl::Job job;
    job.kind = Impl::Job::Kind::kShutdown;
    impl_->pending_binding = std::move(job);
    impl_->pending_query.reset();  // teardown drops queued queries — the intents are dead
    impl_->cv.notify_one();
  }
  if (impl_->thread.joinable()) {
    impl_->thread.join();
  }
}

}  // namespace lumice::gui
