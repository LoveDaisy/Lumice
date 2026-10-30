#include "gui/analyze_survey.hpp"

#include <cstdio>
#include <utility>

#include "file_io.hpp"  // IWYU pragma: keep — SceneDeleter's definition (ScenePtr's owner)
#include "gui/gui_logger.hpp"

namespace lumice::gui {

namespace {

// R6: every query carries a bounded deadline, spelled as one explicit constant at the engine's
// default level. The request body carries the same value (the request builders set budget_ms
// from the same constant), so budget and deadline cannot drift apart by construction.
constexpr int kSurveyQueryDeadlineMs = LUMICE_PATH_SURVEY_DEFAULT_BUDGET_MS;
constexpr int kMapQueryDeadlineMs = LUMICE_PATH_MAP_DEFAULT_BUDGET_MS;
// The point face's deadline is kPointQueryBudgetMs (analyze_survey.hpp) — the request body and
// the query options read the same constant, so budget and deadline cannot drift apart.
// The ray face has no deadline: the engine's representative-ray query is one synchronous
// single-shot evaluation with no deadline polling (LUMICE_PathQueryOptions 0 = none), measured
// at microseconds through the workspace (742.2).
constexpr int kRayQueryDeadlineMs = 0;

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

MapQueryState MapQueryStateFor(const MapQuerySession& map) {
  if (!map.intent_valid) {
    return MapQueryState::kNoSelection;
  }
  if (map.cancelled_for_key) {
    return MapQueryState::kCancelled;
  }
  if (map.adopted_result) {
    return MapQueryState::kCompleted;
  }
  if (!map.adopted_error.empty()) {
    return MapQueryState::kRefused;
  }
  return MapQueryState::kWaiting;
}

bool MapRefiningFor(const MapQuerySession& map) {
  return map.intent_valid && !map.cancelled_for_key && map.adopted_result != nullptr &&
         !(map.adopted_id == map.intent_id);
}

PointQueryState PointQueryStateFor(const PointQuerySession& point) {
  if (!point.intent_valid) {
    return PointQueryState::kNoIntent;
  }
  if (point.cancelled_for_key) {
    return PointQueryState::kCancelled;
  }
  if (point.adopted_result) {
    return PointQueryState::kCompleted;
  }
  if (!point.adopted_error.empty()) {
    return PointQueryState::kRefused;
  }
  return PointQueryState::kWaiting;
}

// ---- The worker ------------------------------------------------------------------------------------

struct SurveyExecutor::Impl {
  struct Job {
    enum class Kind { kSetScene, kReleaseScene, kQuery, kShutdown };
    Kind kind = Kind::kShutdown;
    QueryFace face = QueryFace::kSurvey;                // kQuery
    std::unique_ptr<LUMICE_Scene, SceneDeleter> scene;  // kSetScene: ownership handed over
    SurveyQueryId id;                                   // kQuery
    LUMICE_PathSurveyRequest request{};                 // kQuery (survey); pointers rebound to the buffers
    LUMICE_PathMapRequest map_request{};                // kQuery (map); pointers rebound to the buffers
    LUMICE_SinglePathRequest point_request{};           // kQuery (point); pointers rebound to the buffers
    LUMICE_RepresentativeRayRequest ray_request{};      // kQuery (ray); pointers rebound to the buffers
    std::vector<int> faces;                             // kQuery: the buffers outlive the query
    std::vector<int> counts;                            // kQuery
    std::vector<double> wavelengths;                    // kQuery (map/ray): explicit-λ buffer, may be empty
    std::vector<double> pose;                           // kQuery (ray): the 9-double pose buffer
    TokenPtr token;                                     // kQuery: the cancel handle, live from issue
  };

  // -- guarded by mu --
  std::mutex mu;
  std::condition_variable cv;
  // One binding slot plus one pending slot PER FACE (each face's latest intent wins; the face
  // enum's comment has the supersession-vs-queueing split). One shared query slot would both let
  // an issue SWALLOW a binding swap (the survey-side bug the two-slot shape fixed) and let a map
  // issue cancel a standing survey — the starvation the per-face split exists to prevent.
  std::optional<Job> pending_binding;  // kSetScene / kReleaseScene / kShutdown (latest wins)
  std::optional<Job> pending_survey;   // kQuery, kSurvey face (latest wins)
  std::optional<Job> pending_map;      // kQuery, kMap face (latest wins)
  std::optional<Job> pending_point;    // kQuery, kPoint face (latest wins)
  std::optional<Job> pending_ray;      // kQuery, kRay face (latest wins)
  TokenPtr inflight_token;             // the RUNNING query's cancel handle; the worker owns it
                                       // entirely — registered at pickup, cleared at finish — so it
                                       // is non-null exactly while a query computes (any face;
                                       // inflight_face says which). A QUEUED job keeps its token
                                       // inside itself; it registers at its own pickup, and dropping
                                       // it from its queue needs no cleanup. The ray face's token is
                                       // never polled by the engine (no cancel contract on that
                                       // face) but is registered just the same, so the outstanding
                                       // bookkeeping stays uniform across faces.
  QueryFace inflight_face = QueryFace::kSurvey;
  std::optional<Finished> finished_survey;  // per-face single slot: the newest finish of THAT face
  std::optional<Finished> finished_map;     // overwrites an older one the GUI has not drained
  std::optional<Finished> finished_point;
  std::optional<Finished> finished_ray;
  unsigned long long next_serial = 0;
  bool joined = false;
  // Worker-thread-only (written in the binding branch, read in the query's else arm, both on
  // the worker): why the workspace is missing, when it is missing because the create failed —
  // empty otherwise, so the query's refusal can name the real cause (R7) instead of blaming a
  // release that never happened.
  std::string binding_note;

  // -- worker thread only (destroyed there, never under the GUI) --
  std::unique_ptr<LUMICE_Scene, SceneDeleter> scene;
  WorkspacePtr workspace;

  std::thread thread;
};

SurveyExecutor::SurveyExecutor() : impl_(std::make_unique<Impl>()) {
  impl_->thread = std::thread([impl = impl_.get()] {
    std::unique_lock<std::mutex> lock(impl->mu);
    for (;;) {
      impl->cv.wait(lock, [impl] {
        return impl->pending_binding.has_value() || impl->pending_survey.has_value() || impl->pending_map.has_value() ||
               impl->pending_point.has_value() || impl->pending_ray.has_value();
      });

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
          // Held under the lock deliberately: the swap is host-side scene binding only
          // (LUMICE_PathWorkspaceCreate builds an index, no tracing — the M1-0 probe measured
          // it instant even against a built scene), so the GUI's blocked SetScene/TakeFinished
          // window is sub-millisecond. If a future scene scale ever makes it measurable, the
          // fix is the query's own shape: take the job out, unlock, swap, relock, write back.
          impl->workspace.reset();
          impl->scene = std::move(binding.scene);
          impl->binding_note.clear();
          if (impl->scene) {
            LUMICE_PathWorkspace* raw = nullptr;
            char err[256] = {};
            if (LUMICE_PathWorkspaceCreate(impl->scene.get(), &raw, err, sizeof(err)) == LUMICE_OK) {
              impl->workspace.reset(raw);
            } else {
              impl->binding_note = err;
              GUI_LOG_WARNING("[SurveyExecutor] workspace create failed: {} — queries refuse until the next Analyze",
                              err);
            }
          }
        } else {  // kReleaseScene
          impl->workspace.reset();
          impl->scene.reset();
          impl->binding_note.clear();
        }
      }

      // One query at a time, whichever face owns the first non-empty slot in a FIXED priority
      // order — survey, map, point, ray — not a fair FIFO queue: the selection's digest should
      // not wait behind a view refinement that queued behind it, the point behind the digest,
      // and the ray face's microsecond queries queue last (the slider is already superseding
      // them; starving them for a frame costs nothing).
      std::optional<Impl::Job> job_slot;
      if (impl->pending_survey.has_value()) {
        job_slot = std::move(*impl->pending_survey);
        impl->pending_survey.reset();
      } else if (impl->pending_map.has_value()) {
        job_slot = std::move(*impl->pending_map);
        impl->pending_map.reset();
      } else if (impl->pending_point.has_value()) {
        job_slot = std::move(*impl->pending_point);
        impl->pending_point.reset();
      } else if (impl->pending_ray.has_value()) {
        job_slot = std::move(*impl->pending_ray);
        impl->pending_ray.reset();
      }
      if (!job_slot.has_value()) {
        continue;
      }
      Impl::Job job = std::move(*job_slot);
      // (Re-)register this job's token as THE running handle before running it. The worker is
      // the member's only writer: an unconditional assignment here is the registration, and
      // the finish below clears it only while it still names this job.
      impl->inflight_token = job.token;
      impl->inflight_face = job.face;
      // kQuery. Run off the lock — the loop's unique_lock is released around the compute (it is
      // the one long-running section; every other branch stays under the lock) and retaken to
      // post the finish. Releasing is not optional hygiene: the post below would lock the
      // non-recursive mutex a second time from the same thread and deadlock against itself.
      LUMICE_PathQueryOptions options{};
      options.struct_size = sizeof(options);
      options.deadline_ms = job.face == QueryFace::kMap   ? kMapQueryDeadlineMs :
                            job.face == QueryFace::kPoint ? kPointQueryBudgetMs :
                            job.face == QueryFace::kRay   ? kRayQueryDeadlineMs :
                                                            kSurveyQueryDeadlineMs;
      options.cancel = job.token.get();
      LUMICE_ErrorCode err_code = LUMICE_OK;
      char err[512] = {};
      SurveyHandle survey_result;
      MapHandle map_result;
      PointHandle point_result;
      RepresentativeRayHandle ray_result;
      lock.unlock();
      if (!impl->workspace) {
        // The binding is unusable under this query. Name WHICH failure honestly (R7): a
        // released scene, or a workspace whose build failed at SetScene — the scene still
        // being alive in the latter case, its real error sitting in binding_note. The gate
        // drops it anyway whenever the caller also reset the intent, which is every
        // document-switch path.
        err_code = LUMICE_ERR_UNKNOWN;
        if (!impl->binding_note.empty()) {
          std::snprintf(err, sizeof(err), "workspace_unavailable: %s", impl->binding_note.c_str());
        } else {
          std::snprintf(err, sizeof(err), "scene_released: the analysis scene binding is gone");
        }
      } else if (job.face == QueryFace::kSurvey) {
        LUMICE_PathSurveyRequest request = job.request;
        request.faces = job.faces.data();
        request.layer_face_counts = job.counts.data();
        LUMICE_PathSurvey* raw = nullptr;
        err_code = LUMICE_PathWorkspaceQuerySurvey(impl->workspace.get(), &request, &options, &raw, err, sizeof(err));
        survey_result = SurveyHandle(raw, SurveyDeleter{});
      } else if (job.face == QueryFace::kMap) {
        LUMICE_PathMapRequest request = job.map_request;
        request.faces = job.faces.data();
        request.layer_face_counts = job.counts.data();
        request.wavelengths_nm = job.wavelengths.empty() ? nullptr : job.wavelengths.data();
        LUMICE_PathMap* raw = nullptr;
        err_code = LUMICE_PathWorkspaceQueryMap(impl->workspace.get(), &request, &options, &raw, err, sizeof(err));
        map_result = MapHandle(raw, MapDeleter{});
      } else if (job.face == QueryFace::kPoint) {
        LUMICE_SinglePathRequest request = job.point_request;
        request.faces = job.faces.data();
        request.layer_face_counts = job.counts.data();
        LUMICE_SinglePathResult* raw = nullptr;
        err_code = LUMICE_PathWorkspaceQueryPoint(impl->workspace.get(), &request, &options, &raw, err, sizeof(err));
        point_result = PointHandle(raw, PointDeleter{});
      } else {
        LUMICE_RepresentativeRayRequest request = job.ray_request;
        request.faces = job.faces.data();
        request.layer_face_counts = job.counts.data();
        request.pose = job.pose.data();
        request.wavelengths_nm = job.wavelengths.empty() ? nullptr : job.wavelengths.data();
        LUMICE_RepresentativeRay* raw = nullptr;
        err_code = LUMICE_PathWorkspaceQueryRepresentativeRay(impl->workspace.get(), &request, &options, &raw, err,
                                                              sizeof(err));
        ray_result = RepresentativeRayHandle(raw, RepresentativeRayDeleter{});
      }
      lock.lock();
      // Clear the member only while it still names THIS job's token — the worker wrote it at
      // this job's pickup, and nothing else writes it, so this is the matching unregister.
      // Then post to the job's own face's slot (the per-face mailboxes the pumps drain).
      if (impl->inflight_token == job.token) {
        impl->inflight_token.reset();
      }
      Finished finish{ job.id,
                       job.face,
                       err_code == LUMICE_OK,
                       std::string(err),
                       std::move(survey_result),
                       std::move(map_result),
                       std::move(point_result),
                       std::move(ray_result) };
      if (job.face == QueryFace::kSurvey) {
        impl->finished_survey = std::move(finish);
      } else if (job.face == QueryFace::kMap) {
        impl->finished_map = std::move(finish);
      } else if (job.face == QueryFace::kPoint) {
        impl->finished_point = std::move(finish);
      } else {
        impl->finished_ray = std::move(finish);
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
  // A binding swap kills any in-flight query of EITHER face (the new binding invalidates both);
  // the queued jobs survive and run against the new binding, their outcomes dying in the gate
  // with the intents the swap retired.
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
  job.face = QueryFace::kSurvey;
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
  // R6: the new intent supersedes whatever of ITS OWN face is RUNNING. A queued survey (if one
  // is waiting) keeps its own token inside itself — it never registered, so replacing it needs
  // no cancel and no cleanup; its successor relationship is enforced when it is eventually
  // picked up (its intent is the newest the pump issued, and the pump's gate judges outcomes by
  // id). The other face's query — running or queued — is untouched.
  if (impl_->inflight_token && impl_->inflight_face == QueryFace::kSurvey) {
    LUMICE_PathCancelTokenRequest(impl_->inflight_token.get());
  }
  impl_->pending_survey = std::move(job);
  impl_->cv.notify_one();
}

void SurveyExecutor::IssueMapQuery(SurveyQueryId id, const LUMICE_PathMapRequest& request, std::vector<int> faces,
                                   std::vector<int> counts, std::vector<double> wavelengths) {
  Impl::Job job;
  job.kind = Impl::Job::Kind::kQuery;
  job.face = QueryFace::kMap;
  job.id = id;
  job.map_request = request;
  job.faces = std::move(faces);
  job.counts = std::move(counts);
  job.wavelengths = std::move(wavelengths);
  job.map_request.faces = job.faces.data();
  job.map_request.layer_face_counts = job.counts.data();
  job.map_request.wavelengths_nm = job.wavelengths.empty() ? nullptr : job.wavelengths.data();
  {
    LUMICE_PathCancelToken* raw = nullptr;
    if (LUMICE_PathCancelTokenCreate(&raw) == LUMICE_OK) {
      job.token.reset(raw, TokenDeleter{});
    }
  }
  std::lock_guard<std::mutex> lock(impl_->mu);
  // The map face's own supersession: a newer view cancels the older map query wherever it is —
  // and never the survey's (see QueryFace's comment).
  if (impl_->inflight_token && impl_->inflight_face == QueryFace::kMap) {
    LUMICE_PathCancelTokenRequest(impl_->inflight_token.get());
  }
  impl_->pending_map = std::move(job);
  impl_->cv.notify_one();
}

void SurveyExecutor::IssuePointQuery(SurveyQueryId id, const LUMICE_SinglePathRequest& request, std::vector<int> faces,
                                     std::vector<int> counts) {
  Impl::Job job;
  job.kind = Impl::Job::Kind::kQuery;
  job.face = QueryFace::kPoint;
  job.id = id;
  job.point_request = request;
  job.faces = std::move(faces);
  job.counts = std::move(counts);
  job.point_request.faces = job.faces.data();
  job.point_request.layer_face_counts = job.counts.data();
  {
    LUMICE_PathCancelToken* raw = nullptr;
    if (LUMICE_PathCancelTokenCreate(&raw) == LUMICE_OK) {
      job.token.reset(raw, TokenDeleter{});
    }
  }
  std::lock_guard<std::mutex> lock(impl_->mu);
  // The point face's own supersession: a newer sky point cancels the older point query wherever
  // it is — and never the other faces' queries (see QueryFace's comment).
  if (impl_->inflight_token && impl_->inflight_face == QueryFace::kPoint) {
    LUMICE_PathCancelTokenRequest(impl_->inflight_token.get());
  }
  impl_->pending_point = std::move(job);
  impl_->cv.notify_one();
}

void SurveyExecutor::IssueRayQuery(SurveyQueryId id, const LUMICE_RepresentativeRayRequest& request,
                                   std::vector<int> faces, std::vector<int> counts) {
  Impl::Job job;
  job.kind = Impl::Job::Kind::kQuery;
  job.face = QueryFace::kRay;
  job.id = id;
  job.ray_request = request;
  job.faces = std::move(faces);
  job.counts = std::move(counts);
  job.pose.assign(request.pose, request.pose + 9);
  job.wavelengths.assign(request.wavelengths_nm, request.wavelengths_nm + request.wavelength_count);
  job.ray_request.faces = job.faces.data();
  job.ray_request.layer_face_counts = job.counts.data();
  {
    // The engine's rep-ray face never polls the cancel token (no deadline, no cancel contract —
    // the header's own comment), but the job registers one like every other face so the
    // outstanding-query bookkeeping stays uniform and the token's lifetime rules have no
    // per-face exception to remember.
    LUMICE_PathCancelToken* raw = nullptr;
    if (LUMICE_PathCancelTokenCreate(&raw) == LUMICE_OK) {
      job.token.reset(raw, TokenDeleter{});
    }
  }
  std::lock_guard<std::mutex> lock(impl_->mu);
  // The ray face's supersession is queue replacement: a newer sample's ray job replaces the
  // queued one, and a RUNNING ray query is left to finish (microseconds — the engine never
  // polls its cancel), its outcome dying in the adoption gate.
  impl_->pending_ray = std::move(job);
  impl_->cv.notify_one();
}

void SurveyExecutor::CancelInFlight(QueryFace face) {
  std::lock_guard<std::mutex> lock(impl_->mu);
  if (impl_->inflight_token && impl_->inflight_face == face) {
    LUMICE_PathCancelTokenRequest(impl_->inflight_token.get());
  }
  // The query only — a queued binding swap must survive a cancel.
  if (face == QueryFace::kSurvey) {
    impl_->pending_survey.reset();
  } else if (face == QueryFace::kMap) {
    impl_->pending_map.reset();
  } else if (face == QueryFace::kPoint) {
    impl_->pending_point.reset();
  } else {
    impl_->pending_ray.reset();
  }
}

void SurveyExecutor::ReleaseScene() {
  Impl::Job job;
  job.kind = Impl::Job::Kind::kReleaseScene;
  std::lock_guard<std::mutex> lock(impl_->mu);
  if (impl_->inflight_token) {
    LUMICE_PathCancelTokenRequest(impl_->inflight_token.get());
  }
  // The queued queries of all faces run against nothing after the release — drop them with it.
  impl_->pending_survey.reset();
  impl_->pending_map.reset();
  impl_->pending_point.reset();
  impl_->pending_ray.reset();
  impl_->pending_binding = std::move(job);
  impl_->cv.notify_one();
}

std::optional<SurveyExecutor::Finished> SurveyExecutor::TakeFinished(QueryFace face) {
  std::lock_guard<std::mutex> lock(impl_->mu);
  std::optional<Finished> out;
  std::optional<Finished>& slot = face == QueryFace::kSurvey ? impl_->finished_survey :
                                  face == QueryFace::kMap    ? impl_->finished_map :
                                  face == QueryFace::kPoint  ? impl_->finished_point :
                                                               impl_->finished_ray;
  if (slot) {
    out = std::move(*slot);
    slot.reset();
  }
  return out;
}

bool SurveyExecutor::InFlight() const {
  std::lock_guard<std::mutex> lock(impl_->mu);
  return impl_->inflight_token != nullptr;
}

bool SurveyExecutor::InFlight(QueryFace face) const {
  std::lock_guard<std::mutex> lock(impl_->mu);
  return impl_->inflight_token != nullptr && impl_->inflight_face == face;
}

bool SurveyExecutor::QueryOutstanding() const {
  std::lock_guard<std::mutex> lock(impl_->mu);
  return impl_->inflight_token != nullptr || impl_->pending_survey.has_value() || impl_->pending_map.has_value() ||
         impl_->pending_point.has_value() || impl_->pending_ray.has_value();
}

bool SurveyExecutor::QueryOutstanding(QueryFace face) const {
  std::lock_guard<std::mutex> lock(impl_->mu);
  if (impl_->inflight_token != nullptr && impl_->inflight_face == face) {
    return true;
  }
  switch (face) {
    case QueryFace::kSurvey:
      return impl_->pending_survey.has_value();
    case QueryFace::kMap:
      return impl_->pending_map.has_value();
    case QueryFace::kPoint:
      return impl_->pending_point.has_value();
    case QueryFace::kRay:
      return impl_->pending_ray.has_value();
  }
  return false;
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
    impl_->pending_survey.reset();  // teardown drops queued queries — the intents are dead
    impl_->pending_map.reset();
    impl_->pending_point.reset();
    impl_->pending_ray.reset();
    impl_->cv.notify_one();
  }
  if (impl_->thread.joinable()) {
    impl_->thread.join();
  }
}

}  // namespace lumice::gui
