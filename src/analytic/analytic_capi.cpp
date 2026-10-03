// The C ABI of the analytic capability: the functions lumice_analytic_core.h declares. No static
// object with a side effect lives here, so this TU goes into lumice_analytic_kernel and with it into
// every library that hosts the capability — the engine libraries as well as liblumice_analytic.
// Anything that manages liblumice_analytic itself belongs in analytic_lib.cpp instead.

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <deque>
#include <memory>
#include <new>
#include <string>
#include <vector>

#include "analytic/band_sum.hpp"
#include "analytic/diagnostic_field.hpp"
#include "analytic/discovery.hpp"
#include "analytic/feature_discovery.hpp"
#include "analytic/fiber_continuation.hpp"
#include "analytic/path_evaluation.hpp"
#include "analytic/path_fiber.hpp"
#include "lumice_analytic_core.h"

namespace {

// Zero-fills a caller-allocated result struct after its leading struct_size field, within the bytes
// the caller declared (doc/analytic-api.md section 8.2) — so a caller older than the library keeps
// the memory it did not size, and struct_size itself is never touched.
template <typename T>
void ZeroAfterStructSize(T* out) {
  const size_t declared = out->struct_size;
  const size_t known = sizeof(T);
  const size_t end = declared < known ? declared : known;
  constexpr size_t kBegin = offsetof(T, struct_size) + sizeof(out->struct_size);
  if (end > kBegin) {
    std::memset(reinterpret_cast<unsigned char*>(out) + kBegin, 0, end - kBegin);
  }
}

LUMICE_ANALYTIC_ErrorCode ToErrorCode(lumice::analytic::Status status) {
  switch (status) {
    case lumice::analytic::Status::kOk:
      return LUMICE_ANALYTIC_OK;
    case lumice::analytic::Status::kInvalidValue:
      return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
    case lumice::analytic::Status::kInvalidConfig:
      return LUMICE_ANALYTIC_ERR_INVALID_CONFIG;
  }
  return LUMICE_ANALYTIC_ERR_UNKNOWN;
}

// The block behind PathEvaluation::storage: one allocation holding the segments, then the
// transmittances.
struct PathEvaluationStorage {
  std::unique_ptr<double[]> values;
};

// The smallest FiberResult a caller may pass: the version 4 layout, everything up to the first
// field version 5 appended (doc/analytic-api.md section 8.2). A struct_size from here up to, but not
// including, sizeof(FiberResult) is served as version 4 was; the appended fields are written only
// when all of them fit.
constexpr size_t kFiberResultV4Size = offsetof(LUMICE_ANALYTIC_FiberResult, branch_margin_count);
static_assert(kFiberResultV4Size == offsetof(LUMICE_ANALYTIC_FiberResult, storage) + sizeof(void*),
              "the version 4 layout ends with `storage`: new fields go after it, never before");

// The first published DiagnosticFieldResult layout is version 7, through and including `storage`.
// Keep this boundary independent of sizeof(current header): compatible fields may be appended after
// it without making a version 7 caller too small to evaluate or release.
constexpr size_t kDiagnosticFieldResultV7Size =
    offsetof(LUMICE_ANALYTIC_DiagnosticFieldResult, storage) + sizeof(void*);
static_assert(kDiagnosticFieldResultV7Size == sizeof(LUMICE_ANALYTIC_DiagnosticFieldResult),
              "the version 7 diagnostic-field layout ends with `storage`; append new fields after it");

constexpr size_t kFeatureSupportSampleV9Size = offsetof(LUMICE_ANALYTIC_FeatureSupportSample, accumulates_measure);
constexpr size_t kFeatureSupportSampleV10Size = offsetof(LUMICE_ANALYTIC_FeatureSupportSample, mapping_evidence_kind);
constexpr size_t kFeatureSupportBatchV9Size = offsetof(LUMICE_ANALYTIC_FeatureSupportBatch, cell_axis_count);
constexpr size_t kFeatureSupportBatchV11Size =
    offsetof(LUMICE_ANALYTIC_FeatureSupportBatch, parameter_descriptor_count);
static_assert(kFeatureSupportSampleV9Size ==
              offsetof(LUMICE_ANALYTIC_FeatureSupportSample, numerically_available) + sizeof(int));
static_assert(kFeatureSupportSampleV10Size ==
              offsetof(LUMICE_ANALYTIC_FeatureSupportSample, accumulates_measure) + sizeof(int));
static_assert(kFeatureSupportBatchV9Size ==
              offsetof(LUMICE_ANALYTIC_FeatureSupportBatch, edges) + sizeof(const LUMICE_ANALYTIC_FeatureSupportEdge*));
static_assert(kFeatureSupportBatchV11Size == offsetof(LUMICE_ANALYTIC_FeatureSupportBatch, cell_axes) +
                                                 sizeof(const LUMICE_ANALYTIC_FeatureSupportCellAxis*));

static_assert(static_cast<int>(lumice::analytic::FeatureEvidenceStatus::kConfirmed) ==
                  LUMICE_ANALYTIC_FEATURE_CONFIRMED &&
              static_cast<int>(lumice::analytic::FeatureEvidenceStatus::kNotSupported) ==
                  LUMICE_ANALYTIC_FEATURE_NOT_SUPPORTED);
static_assert(static_cast<int>(lumice::analytic::FeatureMechanism::kInteriorRankLoss) ==
                  LUMICE_ANALYTIC_FEATURE_INTERIOR_RANK_LOSS &&
              static_cast<int>(lumice::analytic::FeatureMechanism::kBrightnessRidge) ==
                  LUMICE_ANALYTIC_FEATURE_BRIGHTNESS_RIDGE);
static_assert(static_cast<int>(lumice::analytic::SupportMeasureKind::kAtom) == LUMICE_ANALYTIC_SUPPORT_ATOM &&
              static_cast<int>(lumice::analytic::SupportMeasureKind::kContinuous) ==
                  LUMICE_ANALYTIC_SUPPORT_CONTINUOUS);
static_assert(static_cast<int>(lumice::analytic::FeatureParameterRole::kUnspecified) ==
                  LUMICE_ANALYTIC_FEATURE_PARAMETER_UNSPECIFIED &&
              static_cast<int>(lumice::analytic::FeatureParameterRole::kPose) ==
                  LUMICE_ANALYTIC_FEATURE_PARAMETER_POSE);
static_assert(static_cast<int>(lumice::analytic::FeatureSupportScopeKind::kJoint) ==
                  LUMICE_ANALYTIC_FEATURE_SCOPE_JOINT &&
              static_cast<int>(lumice::analytic::FeatureSupportScopeKind::kConditional) ==
                  LUMICE_ANALYTIC_FEATURE_SCOPE_CONDITIONAL);

// Where each double array of one FiberResult sits in its storage block, for N poses and k margins.
// The version 4 arrays keep their order — poses (9 N), sun directions (3 N), arclength increments
// (N - 1), residual norms (N), tangents (3 N): 17 N - 1 doubles — and the version 5 double arrays
// follow when requested: branch margins (N k), normal Jacobian (N), singular values (2 N).
struct FiberDoubleLayout {
  size_t poses = 0;
  size_t sun = 0;
  size_t arclength = 0;
  size_t residuals = 0;
  size_t tangents = 0;
  size_t branch_margins = 0;
  size_t normal_jacobian = 0;
  size_t singular_values = 0;
  size_t total = 0;
};

FiberDoubleLayout FiberDoubles(size_t n, size_t k, bool diagnostics) {
  FiberDoubleLayout l;
  l.sun = l.poses + 9 * n;
  l.arclength = l.sun + 3 * n;
  l.residuals = l.arclength + (n - 1);
  l.tangents = l.residuals + n;
  l.total = l.tangents + 3 * n;
  if (diagnostics) {
    l.branch_margins = l.total;
    l.normal_jacobian = l.branch_margins + n * k;
    l.singular_values = l.normal_jacobian + n;
    l.total = l.singular_values + 2 * n;
  }
  return l;
}

// The block behind FiberResult::storage: the doubles (FiberDoubleLayout), and for a version 5
// caller the availability flags and the margin names with the pointer array that lists them. Typed
// members rather than one byte block, so no member's alignment is computed by hand.
struct FiberResultStorage {
  std::unique_ptr<double[]> values;
  std::vector<int> jacobian_available;
  std::vector<std::string> names;
  std::vector<const char*> name_pointers;
};

LUMICE_ANALYTIC_Reason ToReason(lumice::analytic::FiberReason reason) {
  using R = lumice::analytic::FiberReason;
  switch (reason) {
    case R::kClosedLoop:
      return LUMICE_ANALYTIC_REASON_CLOSED_LOOP;
    case R::kTirBoundary:
      return LUMICE_ANALYTIC_REASON_TIR_BOUNDARY;
    case R::kBranchBoundary:
      return LUMICE_ANALYTIC_REASON_BRANCH_BOUNDARY;
    case R::kPathInfeasible:
      return LUMICE_ANALYTIC_REASON_PATH_INFEASIBLE;
    case R::kVisibilityBoundary:
      return LUMICE_ANALYTIC_REASON_VISIBILITY_BOUNDARY;
    case R::kChartBoundary:
      return LUMICE_ANALYTIC_REASON_CHART_BOUNDARY;
    case R::kRankLoss:
      return LUMICE_ANALYTIC_REASON_RANK_LOSS;
    case R::kTopologyAmbiguity:
      return LUMICE_ANALYTIC_REASON_TOPOLOGY_AMBIGUITY;
    case R::kCorrectorFailure:
      return LUMICE_ANALYTIC_REASON_CORRECTOR_FAILURE;
    case R::kLinearSolveFailure:
      return LUMICE_ANALYTIC_REASON_LINEAR_SOLVE_FAILURE;
    case R::kNonFinite:
      return LUMICE_ANALYTIC_REASON_NON_FINITE;
    case R::kStepUnderflow:
      return LUMICE_ANALYTIC_REASON_STEP_UNDERFLOW;
    case R::kInvalidNumericalInput:
      return LUMICE_ANALYTIC_REASON_INVALID_NUMERICAL_INPUT;
    case R::kStepBudget:
      return LUMICE_ANALYTIC_REASON_STEP_BUDGET;
    case R::kArclengthBudget:
      return LUMICE_ANALYTIC_REASON_ARCLENGTH_BUDGET;
    case R::kEvaluationBudget:
      return LUMICE_ANALYTIC_REASON_EVALUATION_BUDGET;
  }
  return LUMICE_ANALYTIC_REASON_UNKNOWN;
}

LUMICE_ANALYTIC_FiberStatus ToStatus(lumice::analytic::FiberStatus status) {
  using S = lumice::analytic::FiberStatus;
  switch (status) {
    case S::kClosed:
      return LUMICE_ANALYTIC_FIBER_CLOSED;
    case S::kEventTerminated:
      return LUMICE_ANALYTIC_FIBER_EVENT_TERMINATED;
    case S::kNumericalFailure:
      return LUMICE_ANALYTIC_FIBER_NUMERICAL_FAILURE;
    case S::kBudgetExhausted:
      return LUMICE_ANALYTIC_FIBER_BUDGET_EXHAUSTED;
  }
  return LUMICE_ANALYTIC_FIBER_NUMERICAL_FAILURE;
}

// The C options onto LI's full option set (doc/analytic-api.md section 4.3): zero is the default,
// negative or non-finite is invalid, and the filled-in set must pass LI's own relations.
bool ToParams(const LUMICE_ANALYTIC_ContinuationOptions* options, lumice::analytic::ContinuationParams* p) {
  *p = lumice::analytic::ContinuationParams{};
  if (options == nullptr) {
    return true;
  }
  auto real = [](double v, double* field) {
    if (!std::isfinite(v) || v < 0.0) {
      return false;
    }
    if (v > 0.0) {
      *field = v;
    }
    return true;
  };
  auto count = [](int v, int* field) {
    if (v < 0) {
      return false;
    }
    if (v > 0) {
      *field = v;
    }
    return true;
  };
  return real(options->seed_residual_tolerance, &p->residual_tolerance) &&
         real(options->step_initial, &p->initial_step) && real(options->step_min, &p->minimum_step) &&
         real(options->step_max, &p->maximum_step) && count(options->max_accepted_steps, &p->maximum_accepted_steps) &&
         count(options->closure_min_steps, &p->closure_minimum_steps) &&
         real(options->closure_pose_tolerance, &p->closure_distance) && lumice::analytic::ValidateParams(*p);
}

// A problem's own inputs (the element-level checks of TraceFiberBatch). On success `slots` holds the
// resolved face sequence.
bool ValidProblem(const lumice::analytic::FaceNormalTable& table, const LUMICE_ANALYTIC_FiberProblem& problem,
                  int* slots) {
  namespace an = lumice::analytic;
  if (problem.face_count < 2 || problem.face_count > an::kMaxFaceCount || problem.faces == nullptr) {
    return false;
  }
  if (!std::isfinite(problem.refractive_index) || problem.refractive_index <= 0.0) {
    return false;
  }
  if (!an::ValidateUnitVector(problem.incident_direction) || !an::ValidateUnitVector(problem.target_direction) ||
      !an::ValidateRotation(problem.seed_pose)) {
    return false;
  }
  if (problem.initial_tangent_sign < -1 || problem.initial_tangent_sign > 1) {
    return false;
  }
  return an::ResolveFaceSequence(table, problem.faces, problem.face_count, slots) == an::Status::kOk;
}

// Copies one trace of a `face_count`-face path into its result's storage block. `struct_size` is
// the caller's (the nested results of DiscoverComponents pass the library's own): the version 5
// fields are written only when it covers all of them.
void FillFiberResult(const lumice::analytic::TraceResult& trace, const double incident[3], int face_count,
                     size_t struct_size, LUMICE_ANALYTIC_FiberResult* out) {
  out->status = ToStatus(trace.status);
  out->reason = ToReason(trace.reason);
  const int n = trace.PoseCount();
  out->pose_count = n;
  if (n == 0) {
    return;
  }
  const bool diagnostics = struct_size >= sizeof(LUMICE_ANALYTIC_FiberResult);
  const size_t un = static_cast<size_t>(n);
  const size_t k = static_cast<size_t>(trace.branch_margin_count);
  // Every margin row of an ice path has one name per BranchMarginName; the trace records the map's
  // count, which for this map is that same number.
  assert(!diagnostics || trace.branch_margin_count == lumice::analytic::BranchMarginCount(face_count));
  const FiberDoubleLayout layout = FiberDoubles(un, k, diagnostics);
  auto storage = std::make_unique<FiberResultStorage>();
  storage->values = std::make_unique<double[]>(layout.total);
  double* values = storage->values.get();
  double* poses = values + layout.poses;
  double* sun = values + layout.sun;
  double* arclength = values + layout.arclength;
  double* residuals = values + layout.residuals;
  double* tangents = values + layout.tangents;
  std::memcpy(poses, trace.poses.data(), 9 * un * sizeof(double));
  for (size_t i = 0; i < un; i++) {
    // u = R^T (-s): the sun direction seen from the crystal.
    const double* r = poses + 9 * i;
    for (int k = 0; k < 3; k++) {
      sun[3 * i + k] = -(r[0 * 3 + k] * incident[0] + r[1 * 3 + k] * incident[1] + r[2 * 3 + k] * incident[2]);
    }
  }
  if (un > 1) {
    std::memcpy(arclength, trace.arclength_increments.data(), (un - 1) * sizeof(double));
  }
  std::memcpy(residuals, trace.residual_norms.data(), un * sizeof(double));
  std::memcpy(tangents, trace.tangents.data(), 3 * un * sizeof(double));
  out->poses = poses;
  out->crystal_frame_sun_directions = sun;
  out->arclength_increments = un > 1 ? arclength : nullptr;
  out->residual_norms = residuals;
  out->tangents = tangents;
  if (diagnostics) {
    double* margins = values + layout.branch_margins;
    double* normal_jacobian = values + layout.normal_jacobian;
    double* singular_values = values + layout.singular_values;
    if (k > 0) {
      std::memcpy(margins, trace.branch_margins.data(), un * k * sizeof(double));
    }
    std::memcpy(normal_jacobian, trace.normal_jacobian.data(), un * sizeof(double));
    std::memcpy(singular_values, trace.singular_values.data(), 2 * un * sizeof(double));
    storage->jacobian_available = trace.jacobian_available;
    storage->names.reserve(k);
    for (size_t i = 0; i < k; i++) {
      storage->names.push_back(lumice::analytic::BranchMarginName(static_cast<int>(i), face_count));
    }
    // Filled after the strings stop moving: each pointer is into its own std::string.
    storage->name_pointers.reserve(k);
    for (const std::string& name : storage->names) {
      storage->name_pointers.push_back(name.c_str());
    }
    out->branch_margin_count = static_cast<int>(k);
    out->branch_margin_names = k > 0 ? storage->name_pointers.data() : nullptr;
    out->branch_margins = k > 0 ? margins : nullptr;
    out->jacobian_available = storage->jacobian_available.data();
    out->normal_jacobian = normal_jacobian;
    out->singular_values = singular_values;
  }
  out->storage = storage.release();
}

LUMICE_ANALYTIC_FiberResult* ElementAt(LUMICE_ANALYTIC_FiberResult* base, size_t stride, int i) {
  return reinterpret_cast<LUMICE_ANALYTIC_FiberResult*>(reinterpret_cast<unsigned char*>(base) +
                                                        stride * static_cast<size_t>(i));
}

void ReleaseFiberStorage(LUMICE_ANALYTIC_FiberResult* result) {
  // Reclaims the block released to the caller by FillFiberResult; destroyed at scope exit.
  std::unique_ptr<FiberResultStorage> owned(static_cast<FiberResultStorage*>(result->storage));
  ZeroAfterStructSize(result);
}

LUMICE_ANALYTIC_ErrorCode TraceFiberBatchImpl(const LUMICE_ANALYTIC_Crystal* crystal,
                                              const LUMICE_ANALYTIC_FiberProblem* problems, int count,
                                              const LUMICE_ANALYTIC_ContinuationOptions* options,
                                              LUMICE_ANALYTIC_FiberResult* out_results, size_t stride) {
  namespace an = lumice::analytic;
  if (crystal == nullptr || problems == nullptr) {
    return LUMICE_ANALYTIC_ERR_NULL_ARG;
  }
  for (int i = 0; i < count; i++) {
    if (problems[i].faces == nullptr && problems[i].face_count > 0) {
      return LUMICE_ANALYTIC_ERR_NULL_ARG;
    }
  }
  an::ContinuationParams base_params;
  if (!ToParams(options, &base_params)) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  an::FaceNormalTable table;
  if (auto status = an::BuildFaceNormals(*crystal, &table); status != an::Status::kOk) {
    return ToErrorCode(status);
  }
  int slots[an::kMaxFaceCount];
  for (int i = 0; i < count; i++) {
    const LUMICE_ANALYTIC_FiberProblem& problem = problems[i];
    LUMICE_ANALYTIC_FiberResult* out = ElementAt(out_results, stride, i);
    if (!ValidProblem(table, problem, slots)) {
      out->status = LUMICE_ANALYTIC_FIBER_NUMERICAL_FAILURE;
      out->reason = LUMICE_ANALYTIC_REASON_INVALID_NUMERICAL_INPUT;
      continue;
    }
    an::ContinuationParams params = base_params;
    params.initial_tangent_sign = problem.initial_tangent_sign == -1 ? -1 : 1;
    const an::IcePathMap map(table, slots, problem.face_count, problem.refractive_index, problem.incident_direction);
    const an::TraceResult trace =
        an::TraceFiber(map, an::MakeTargetChart(problem.target_direction), problem.seed_pose, params);
    FillFiberResult(trace, problem.incident_direction, problem.face_count, stride, out);
  }
  return LUMICE_ANALYTIC_OK;
}

// The block behind DiscoveryResult::storage: the component and candidate arrays, the FiberResults
// they point to (sized once, so the pointers stay put), and the trace blocks those results view.
struct DiscoveryResultStorage {
  std::vector<LUMICE_ANALYTIC_DiscoveredComponent> components;
  std::vector<LUMICE_ANALYTIC_IncompleteCandidate> incomplete;
  std::vector<LUMICE_ANALYTIC_FiberResult> traces;
  std::vector<std::unique_ptr<FiberResultStorage>> blocks;
};

// The C discovery options onto the kernel's settings: zero is the default, negative or non-finite
// is invalid. The dedup threshold's default is the continuation's closure distance.
struct DiscoverySettings {
  int sample_count = lumice::analytic::kDefaultDiscoverySampleCount;
  double band_half_width = lumice::analytic::kDefaultBandHalfWidth;
  double cluster_radius = lumice::analytic::kDefaultClusterRadius;
  double distance_threshold = 0.0;
};

bool ToDiscoverySettings(const LUMICE_ANALYTIC_DiscoveryOptions* options, double closure_distance,
                         DiscoverySettings* s) {
  *s = DiscoverySettings{};
  s->distance_threshold = closure_distance;
  if (options == nullptr) {
    return true;
  }
  auto real = [](double v, double* field) {
    if (!std::isfinite(v) || v < 0.0) {
      return false;
    }
    if (v > 0.0) {
      *field = v;
    }
    return true;
  };
  if (options->sample_count < 0 || options->sample_count > LUMICE_ANALYTIC_MAX_DISCOVERY_SAMPLE_COUNT) {
    return false;
  }
  if (options->sample_count > 0) {
    s->sample_count = options->sample_count;
  }
  return real(options->band_half_width, &s->band_half_width) && real(options->cluster_radius, &s->cluster_radius) &&
         real(options->distance_threshold, &s->distance_threshold);
}

LUMICE_ANALYTIC_ErrorCode DiscoverComponentsImpl(const LUMICE_ANALYTIC_Crystal* crystal,
                                                 const LUMICE_ANALYTIC_DiscoveryProblem* problem,
                                                 const LUMICE_ANALYTIC_DiscoveryOptions* options,
                                                 const LUMICE_ANALYTIC_ContinuationOptions* continuation,
                                                 LUMICE_ANALYTIC_DiscoveryResult* out) {
  namespace an = lumice::analytic;
  if (out->struct_size < sizeof(LUMICE_ANALYTIC_DiscoveryResult)) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  if (crystal == nullptr || problem == nullptr) {
    return LUMICE_ANALYTIC_ERR_NULL_ARG;
  }
  if ((problem->faces == nullptr && problem->face_count > 0) ||
      (problem->extra_seeds == nullptr && problem->extra_seed_count > 0)) {
    return LUMICE_ANALYTIC_ERR_NULL_ARG;
  }
  an::ContinuationParams params;
  DiscoverySettings settings;
  if (!ToParams(continuation, &params) || !ToDiscoverySettings(options, params.closure_distance, &settings)) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  an::FaceNormalTable table;
  an::FacePolygonTable polygons;
  if (auto status = an::BuildFaceNormals(*crystal, &table, &polygons); status != an::Status::kOk) {
    return ToErrorCode(status);
  }
  if (problem->face_count < 2 || problem->face_count > an::kMaxFaceCount || !std::isfinite(problem->refractive_index) ||
      problem->refractive_index <= 0.0 || !an::ValidateUnitVector(problem->incident_direction) ||
      !an::ValidateUnitVector(problem->target_direction) || problem->extra_seed_count < 0) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  // LI section 9.5.3: the target's component normal to the sun must exist.
  const double delta = an::TargetDeviation(problem->incident_direction, problem->target_direction);
  if (!(delta > 0.0 && delta < std::acos(-1.0))) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  for (int i = 0; i < problem->extra_seed_count; i++) {
    if (!an::ValidateRotation(problem->extra_seeds + 9 * static_cast<size_t>(i))) {
      return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
    }
  }
  int slots[an::kMaxFaceCount];
  if (an::ResolveFaceSequence(table, problem->faces, problem->face_count, slots) != an::Status::kOk) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }

  an::IceDiscovery discovery(table, polygons, slots, problem->face_count, problem->refractive_index,
                             problem->incident_direction);
  const an::DiscoveryOutput found = discovery.Discover(
      problem->target_direction, settings.sample_count, settings.band_half_width, problem->extra_seeds,
      problem->extra_seed_count, settings.cluster_radius, settings.distance_threshold, params);

  auto storage = std::make_unique<DiscoveryResultStorage>();
  auto backward_run = [](const an::IncompleteCandidate& c) {
    return c.cause == an::IncompleteCause::kArcBackwardFailed ||
           c.cause == an::IncompleteCause::kArcBackwardClosedAnomaly;
  };
  size_t trace_count = 0;
  for (const auto& c : found.components) {
    trace_count += c.kind == an::ComponentKind::kArc ? 2 : 1;
  }
  for (const auto& c : found.incomplete) {
    trace_count += backward_run(c) ? 2 : 1;
  }
  storage->traces.resize(trace_count);
  // Reserved up front: view() hands each block from a raw pointer to storage->blocks, and a
  // reallocating emplace_back that threw there would leak that block.
  storage->blocks.reserve(trace_count);
  size_t next = 0;
  auto view = [&](const an::TraceResult& trace) -> const LUMICE_ANALYTIC_FiberResult* {
    LUMICE_ANALYTIC_FiberResult* r = &storage->traces[next++];
    r->struct_size = sizeof(LUMICE_ANALYTIC_FiberResult);
    FillFiberResult(trace, problem->incident_direction, problem->face_count, r->struct_size, r);
    // The block moves to the discovery storage: the nested result is a view, not an owner.
    storage->blocks.emplace_back(static_cast<FiberResultStorage*>(r->storage));
    r->storage = nullptr;
    return r;
  };
  for (const auto& c : found.components) {
    LUMICE_ANALYTIC_DiscoveredComponent d{};
    d.kind = c.kind == an::ComponentKind::kArc ? LUMICE_ANALYTIC_COMPONENT_ARC : LUMICE_ANALYTIC_COMPONENT_CLOSED;
    std::memcpy(d.seed, c.seed, sizeof(d.seed));
    d.forward = view(c.forward);
    d.backward = c.kind == an::ComponentKind::kArc ? view(c.backward) : nullptr;
    storage->components.push_back(d);
  }
  for (const auto& c : found.incomplete) {
    LUMICE_ANALYTIC_IncompleteCandidate d{};
    switch (c.cause) {
      case an::IncompleteCause::kArcBackwardFailed:
        d.cause = LUMICE_ANALYTIC_INCOMPLETE_ARC_BACKWARD_FAILED;
        break;
      case an::IncompleteCause::kArcBackwardClosedAnomaly:
        d.cause = LUMICE_ANALYTIC_INCOMPLETE_ARC_BACKWARD_CLOSED_ANOMALY;
        break;
      case an::IncompleteCause::kUnnamedEvent:
        d.cause = LUMICE_ANALYTIC_INCOMPLETE_UNNAMED_EVENT;
        break;
      case an::IncompleteCause::kNotConverged:
        d.cause = LUMICE_ANALYTIC_INCOMPLETE_NOT_CONVERGED;
        break;
    }
    std::memcpy(d.seed, c.seed, sizeof(d.seed));
    d.forward = view(c.forward);
    d.backward = backward_run(c) ? view(c.backward) : nullptr;
    storage->incomplete.push_back(d);
  }

  out->completeness = found.Complete() ? LUMICE_ANALYTIC_COMPLETENESS_COMPLETE : LUMICE_ANALYTIC_COMPLETENESS_UNKNOWN;
  out->component_count = static_cast<int>(storage->components.size());
  out->components = storage->components.empty() ? nullptr : storage->components.data();
  out->incomplete_count = static_cast<int>(storage->incomplete.size());
  out->incomplete = storage->incomplete.empty() ? nullptr : storage->incomplete.data();
  out->pool_count = found.pool_count;
  out->extra_seed_count = found.extra_seed_count;
  out->raw_cluster_count = found.raw_cluster_count;
  out->admissible_count = found.admissible_count;
  out->dedup_merged = found.dedup_merged;
  out->arc_stitched = found.arc_stitched;
  out->arc_backward_failed = found.arc_backward_failed;
  out->arc_backward_closed_anomaly = found.arc_backward_closed_anomaly;
  out->incomplete_unnamed_event = found.incomplete_unnamed_event;
  out->incomplete_not_converged = found.incomplete_not_converged;
  out->storage = storage.release();
  return LUMICE_ANALYTIC_OK;
}

LUMICE_ANALYTIC_ErrorCode EvaluatePathImpl(const LUMICE_ANALYTIC_Crystal* crystal, const int* faces, int face_count,
                                           double refractive_index, const double incident_direction[3],
                                           const double pose[9], LUMICE_ANALYTIC_PathEvaluation* out) {
  namespace an = lumice::analytic;
  if (out->struct_size < sizeof(LUMICE_ANALYTIC_PathEvaluation)) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  if (crystal == nullptr || faces == nullptr || incident_direction == nullptr || pose == nullptr) {
    return LUMICE_ANALYTIC_ERR_NULL_ARG;
  }
  if (!std::isfinite(refractive_index) || refractive_index <= 0.0 || !an::ValidateUnitVector(incident_direction) ||
      !an::ValidateRotation(pose)) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }

  an::FaceNormalTable table;
  if (auto status = an::BuildFaceNormals(*crystal, &table); status != an::Status::kOk) {
    return ToErrorCode(status);
  }
  if (face_count < 2 || face_count > an::kMaxFaceCount) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  auto slots = std::make_unique<int[]>(static_cast<size_t>(face_count));
  if (auto status = an::ResolveFaceSequence(table, faces, face_count, slots.get()); status != an::Status::kOk) {
    return ToErrorCode(status);
  }

  const size_t segment_values = 3 * (static_cast<size_t>(face_count) + 1);
  auto storage = std::make_unique<PathEvaluationStorage>();
  storage->values = std::make_unique<double[]>(segment_values + static_cast<size_t>(face_count));
  an::PathOutputs outputs{};
  outputs.segment_directions = storage->values.get();
  outputs.interface_transmittances = storage->values.get() + segment_values;
  const bool valid =
      an::EvaluatePath(table, slots.get(), face_count, refractive_index, incident_direction, pose, &outputs);

  out->fresnel_transmission = outputs.fresnel_transmission;
  if (!valid) {
    return LUMICE_ANALYTIC_OK;  // zero-filled already: valid 0, no segments, no storage
  }
  out->valid = 1;
  for (int i = 0; i < 3; i++) {
    out->outgoing_direction[i] = outputs.outgoing_direction[i];
  }
  out->segment_count = face_count + 1;
  out->segment_directions = outputs.segment_directions;
  out->interface_transmittances = outputs.interface_transmittances;
  out->storage = storage.release();
  return LUMICE_ANALYTIC_OK;
}

struct DiagnosticFieldResultStorage {
  std::vector<LUMICE_ANALYTIC_DiagnosticInterface> interfaces;
  std::vector<LUMICE_ANALYTIC_DiagnosticMargin> domain_margins;
  std::vector<LUMICE_ANALYTIC_DiagnosticMargin> tir_margins;
  std::vector<std::string> names;
};

LUMICE_ANALYTIC_DiagnosticPathStatus ToDiagnosticPathStatus(lumice::analytic::DiagnosticPathStatus status) {
  using S = lumice::analytic::DiagnosticPathStatus;
  switch (status) {
    case S::kOk:
      return LUMICE_ANALYTIC_DIAGNOSTIC_PATH_OK;
    case S::kPathInfeasible:
      return LUMICE_ANALYTIC_DIAGNOSTIC_PATH_INFEASIBLE;
    case S::kRefractionCritical:
      return LUMICE_ANALYTIC_DIAGNOSTIC_PATH_REFRACTION_CRITICAL;
    case S::kNonFinite:
      return LUMICE_ANALYTIC_DIAGNOSTIC_PATH_NON_FINITE;
  }
  return LUMICE_ANALYTIC_DIAGNOSTIC_PATH_NON_FINITE;
}

LUMICE_ANALYTIC_DiagnosticEntryStatus ToDiagnosticEntryStatus(lumice::analytic::DiagnosticEntryStatus status) {
  using S = lumice::analytic::DiagnosticEntryStatus;
  switch (status) {
    case S::kNotEvaluated:
      return LUMICE_ANALYTIC_DIAGNOSTIC_ENTRY_NOT_EVALUATED;
    case S::kOk:
      return LUMICE_ANALYTIC_DIAGNOSTIC_ENTRY_OK;
    case S::kEntryBackface:
      return LUMICE_ANALYTIC_DIAGNOSTIC_ENTRY_BACKFACE;
    case S::kExitCriticalAngle:
      return LUMICE_ANALYTIC_DIAGNOSTIC_ENTRY_EXIT_CRITICAL;
    case S::kCorridorEmpty:
      return LUMICE_ANALYTIC_DIAGNOSTIC_ENTRY_CORRIDOR_EMPTY;
  }
  return LUMICE_ANALYTIC_DIAGNOSTIC_ENTRY_NOT_EVALUATED;
}

LUMICE_ANALYTIC_DiagnosticInterfaceKind ToDiagnosticInterfaceKind(lumice::analytic::DiagnosticInterfaceKind kind) {
  using K = lumice::analytic::DiagnosticInterfaceKind;
  switch (kind) {
    case K::kEntryTransmission:
      return LUMICE_ANALYTIC_DIAGNOSTIC_ENTRY_TRANSMISSION;
    case K::kInternalReflection:
      return LUMICE_ANALYTIC_DIAGNOSTIC_INTERNAL_REFLECTION;
    case K::kExitTransmission:
      return LUMICE_ANALYTIC_DIAGNOSTIC_EXIT_TRANSMISSION;
    case K::kExternalReflection:
      return LUMICE_ANALYTIC_DIAGNOSTIC_EXTERNAL_REFLECTION;
  }
  return LUMICE_ANALYTIC_DIAGNOSTIC_ENTRY_TRANSMISSION;
}

void FillDiagnosticMargin(const lumice::analytic::DiagnosticMargin& source, const char* name,
                          LUMICE_ANALYTIC_DiagnosticMargin* target) {
  target->name = name;
  target->interface_index = source.interface_index;
  target->value = source.value;
  target->pose_derivative_available = source.pose_derivative_available;
  target->index_derivative_available = source.index_derivative_available;
  std::copy(source.pose_gradient, source.pose_gradient + 3, target->pose_gradient);
  target->index_derivative = source.index_derivative;
}

void FillDiagnosticFieldResult(const lumice::analytic::DiagnosticFieldResult& source,
                               LUMICE_ANALYTIC_DiagnosticFieldResult* out) {
  auto storage = std::make_unique<DiagnosticFieldResultStorage>();
  storage->interfaces.reserve(source.interfaces.size());
  storage->domain_margins.resize(source.domain_margins.size());
  storage->tir_margins.resize(source.tir_margins.size());
  storage->names.reserve(source.domain_margins.size() + source.tir_margins.size());
  for (const auto& margin : source.domain_margins) {
    storage->names.push_back(margin.name);
  }
  for (const auto& margin : source.tir_margins) {
    storage->names.push_back(margin.name);
  }
  for (size_t i = 0; i < source.interfaces.size(); i++) {
    const auto& item = source.interfaces[i];
    LUMICE_ANALYTIC_DiagnosticInterface target{};
    target.interface_index = static_cast<int>(i);
    target.face_number = item.face_number;
    target.kind = ToDiagnosticInterfaceKind(item.kind);
    target.coefficient = item.coefficient;
    target.pose_derivative_available = item.pose_derivative_available;
    target.index_derivative_available = item.index_derivative_available;
    std::copy(item.pose_gradient, item.pose_gradient + 3, target.pose_gradient);
    target.index_derivative = item.index_derivative;
    storage->interfaces.push_back(target);
  }
  for (size_t i = 0; i < source.domain_margins.size(); i++) {
    FillDiagnosticMargin(source.domain_margins[i], storage->names[i].c_str(), &storage->domain_margins[i]);
  }
  const size_t name_offset = source.domain_margins.size();
  for (size_t i = 0; i < source.tir_margins.size(); i++) {
    FillDiagnosticMargin(source.tir_margins[i], storage->names[name_offset + i].c_str(), &storage->tir_margins[i]);
  }

  out->path_status = ToDiagnosticPathStatus(source.path_status);
  out->entry_status = ToDiagnosticEntryStatus(source.entry_status);
  std::copy(source.outgoing_direction, source.outgoing_direction + 3, out->outgoing_direction);
  out->entry_measure = source.entry_measure;
  out->fresnel_weight = source.fresnel_weight;
  out->interface_count = static_cast<int>(storage->interfaces.size());
  out->interfaces = storage->interfaces.empty() ? nullptr : storage->interfaces.data();
  out->domain_margin_count = static_cast<int>(storage->domain_margins.size());
  out->domain_margins = storage->domain_margins.empty() ? nullptr : storage->domain_margins.data();
  out->tir_margin_count = static_cast<int>(storage->tir_margins.size());
  out->tir_margins = storage->tir_margins.empty() ? nullptr : storage->tir_margins.data();
  out->direction_pose_jacobian_available = source.direction_pose_jacobian_available;
  out->direction_pose_hessian_available = source.direction_pose_hessian_available;
  out->direction_index_derivative_available = source.direction_index_derivative_available;
  out->entry_pose_gradient_available = source.entry_pose_gradient_available;
  out->entry_index_derivative_available = source.entry_index_derivative_available;
  std::copy(source.direction_pose_jacobian, source.direction_pose_jacobian + 9, out->direction_pose_jacobian);
  std::copy(source.direction_pose_hessian, source.direction_pose_hessian + 27, out->direction_pose_hessian);
  std::copy(source.direction_index_derivative, source.direction_index_derivative + 3, out->direction_index_derivative);
  std::copy(source.entry_pose_gradient, source.entry_pose_gradient + 3, out->entry_pose_gradient);
  out->entry_index_derivative = source.entry_index_derivative;
  out->storage = storage.release();
}

LUMICE_ANALYTIC_DiagnosticFieldResult* DiagnosticElementAt(LUMICE_ANALYTIC_DiagnosticFieldResult* base, size_t stride,
                                                           int index) {
  return reinterpret_cast<LUMICE_ANALYTIC_DiagnosticFieldResult*>(reinterpret_cast<unsigned char*>(base) +
                                                                  stride * static_cast<size_t>(index));
}

void ReleaseDiagnosticFieldStorage(LUMICE_ANALYTIC_DiagnosticFieldResult* result) {
  std::unique_ptr<DiagnosticFieldResultStorage> owned(static_cast<DiagnosticFieldResultStorage*>(result->storage));
  ZeroAfterStructSize(result);
}

LUMICE_ANALYTIC_ErrorCode EvaluateDiagnosticFieldBatchImpl(const LUMICE_ANALYTIC_Crystal* crystal, const int* faces,
                                                           int face_count,
                                                           const LUMICE_ANALYTIC_DiagnosticFieldRow* rows, int count,
                                                           LUMICE_ANALYTIC_DiagnosticFieldResult* out_results,
                                                           size_t stride) {
  namespace an = lumice::analytic;
  if (crystal == nullptr || faces == nullptr || rows == nullptr) {
    return LUMICE_ANALYTIC_ERR_NULL_ARG;
  }
  an::FaceNormalTable normals;
  an::FacePolygonTable polygons;
  if (const auto status = an::BuildFaceNormals(*crystal, &normals, &polygons); status != an::Status::kOk) {
    return ToErrorCode(status);
  }
  if (face_count < 1 || face_count > an::kMaxFaceCount) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  int slots[an::kMaxFaceCount];
  if (an::ResolveDiagnosticFaceSequence(normals, faces, face_count, slots) != an::Status::kOk) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }

  an::DiagnosticField field(normals, polygons, faces, slots, face_count);
  for (int i = 0; i < count; i++) {
    LUMICE_ANALYTIC_DiagnosticFieldResult* out = DiagnosticElementAt(out_results, stride, i);
    const auto& row = rows[i];
    if (!std::isfinite(row.refractive_index) || row.refractive_index <= 0.0 ||
        !an::ValidateUnitVector(row.incident_direction) || !an::ValidateRotation(row.pose)) {
      out->row_error = LUMICE_ANALYTIC_ERR_INVALID_VALUE;
      continue;
    }
    an::DiagnosticRowInput input;
    input.refractive_index = row.refractive_index;
    std::copy(row.incident_direction, row.incident_direction + 3, input.incident_direction);
    std::copy(row.pose, row.pose + 9, input.pose);
    FillDiagnosticFieldResult(field.Evaluate(input), out);
  }
  return LUMICE_ANALYTIC_OK;
}

// The block behind BandSumResult::storage: the pixel array.
struct BandSumResultStorage {
  std::vector<LUMICE_ANALYTIC_BandPixel> pixels;
};

bool ToPoseFamily(int family, lumice::analytic::PoseFamily* out) {
  namespace an = lumice::analytic;
  switch (family) {
    case LUMICE_ANALYTIC_POSE_RANDOM:
      *out = an::PoseFamily::kRandom;
      return true;
    case LUMICE_ANALYTIC_POSE_COLUMN:
      *out = an::PoseFamily::kColumn;
      return true;
    case LUMICE_ANALYTIC_POSE_PLATE:
      *out = an::PoseFamily::kPlate;
      return true;
    case LUMICE_ANALYTIC_POSE_PARRY:
      *out = an::PoseFamily::kParry;
      return true;
    case LUMICE_ANALYTIC_POSE_LOWITZ:
      *out = an::PoseFamily::kLowitz;
      return true;
    default:
      return false;
  }
}

LUMICE_ANALYTIC_BandPixelStatus ToBandPixelStatus(lumice::analytic::PixelStatus status) {
  switch (status) {
    case lumice::analytic::PixelStatus::kOk:
      break;
    case lumice::analytic::PixelStatus::kSingular:
      return LUMICE_ANALYTIC_BAND_PIXEL_SINGULAR;
    case lumice::analytic::PixelStatus::kPointMass:
      return LUMICE_ANALYTIC_BAND_PIXEL_POINT_MASS;
  }
  return LUMICE_ANALYTIC_BAND_PIXEL_OK;
}

// The pixel table's own inputs: every centre and corner a unit vector, every solid angle finite and
// positive.
bool ValidPixels(const LUMICE_ANALYTIC_PixelTable& pixels) {
  namespace an = lumice::analytic;
  for (int p = 0; p < pixels.pixel_count; p++) {
    const size_t i = static_cast<size_t>(p);
    if (!an::ValidateUnitVector(pixels.centre + 3 * i)) {
      return false;
    }
    for (int k = 0; k < 4; k++) {
      if (!an::ValidateUnitVector(pixels.corners + 12 * i + 3 * static_cast<size_t>(k))) {
        return false;
      }
    }
    if (!std::isfinite(pixels.solid_angle[i]) || pixels.solid_angle[i] <= 0.0) {
      return false;
    }
  }
  return true;
}

LUMICE_ANALYTIC_ErrorCode BandSumImpl(const LUMICE_ANALYTIC_Crystal* crystal,
                                      const LUMICE_ANALYTIC_BandSumProblem* problem,
                                      LUMICE_ANALYTIC_BandSumResult* out) {
  namespace an = lumice::analytic;
  if (out->struct_size < sizeof(LUMICE_ANALYTIC_BandSumResult)) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  if (crystal == nullptr || problem == nullptr) {
    return LUMICE_ANALYTIC_ERR_NULL_ARG;
  }
  const LUMICE_ANALYTIC_PixelTable& pixels = problem->pixels;
  if ((problem->faces == nullptr && problem->face_count > 0) ||
      (pixels.pixel_count > 0 &&
       (pixels.centre == nullptr || pixels.corners == nullptr || pixels.solid_angle == nullptr))) {
    return LUMICE_ANALYTIC_ERR_NULL_ARG;
  }
  an::PoseDensitySpec spec;
  if (!ToPoseFamily(problem->pose_density.family, &spec.family)) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  spec.zenith_mean_deg = problem->pose_density.zenith_mean_deg;
  spec.zenith_std_deg = problem->pose_density.zenith_std_deg;
  spec.roll_mean_deg = problem->pose_density.roll_mean_deg;
  spec.roll_std_deg = problem->pose_density.roll_std_deg;
  if (an::PoseDensityError(spec)[0] != '\0') {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  an::FaceNormalTable table;
  an::FacePolygonTable polygons;
  if (auto status = an::BuildFaceNormals(*crystal, &table, &polygons); status != an::Status::kOk) {
    return ToErrorCode(status);
  }
  if (problem->face_count < 2 || problem->face_count > an::kMaxFaceCount || !std::isfinite(problem->refractive_index) ||
      problem->refractive_index <= 0.0 || !an::ValidateUnitVector(problem->incident_direction) ||
      problem->sample_count < 1 || problem->sample_count > LUMICE_ANALYTIC_MAX_BAND_SUM_SAMPLE_COUNT ||
      pixels.pixel_count < 0 || !ValidPixels(pixels)) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  int slots[an::kMaxFaceCount];
  if (an::ResolveFaceSequence(table, problem->faces, problem->face_count, slots) != an::Status::kOk) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }

  an::PixelTable kernel_pixels;
  kernel_pixels.count = pixels.pixel_count;
  kernel_pixels.centre = pixels.centre;
  kernel_pixels.corners = pixels.corners;
  kernel_pixels.solid_angle = pixels.solid_angle;
  const an::BandSumOutput sum =
      an::BandSum(table, polygons, slots, problem->face_count, problem->refractive_index, problem->incident_direction,
                  problem->sample_count, kernel_pixels, an::PoseDensity(spec));

  auto storage = std::make_unique<BandSumResultStorage>();
  storage->pixels.reserve(sum.pixels.size());
  for (const an::PixelValue& v : sum.pixels) {
    LUMICE_ANALYTIC_BandPixel px{};
    px.status = ToBandPixelStatus(v.status);
    px.value = v.value;
    px.delta = v.delta;
    px.delta_lo = v.delta_lo;
    px.delta_hi = v.delta_hi;
    px.k = v.k;
    px.k_rho_pos = v.k_rho_pos;
    px.k_eff = v.k_eff;
    storage->pixels.push_back(px);
  }
  out->rank_zero = sum.rank_zero ? 1 : 0;
  out->kept_count = sum.kept_count;
  out->pixel_count = static_cast<int>(storage->pixels.size());
  out->pixels = storage->pixels.empty() ? nullptr : storage->pixels.data();
  if (sum.rank_zero) {
    out->point_mass = sum.m;
    out->point_mass_error = sum.m_error;
    out->point_mass_method = sum.method == an::PointMassMethod::kLatticeMean ? LUMICE_ANALYTIC_POINT_MASS_LATTICE_MEAN :
                                                                               LUMICE_ANALYTIC_POINT_MASS_TWIST_AVERAGE;
    out->point_mass_pixel = sum.point_mass_pixel;
  }
  out->storage = storage.release();
  return LUMICE_ANALYTIC_OK;
}

template <typename T>
const T* StrideElement(const T* base, uint32_t stride, int index) {
  return reinterpret_cast<const T*>(reinterpret_cast<const unsigned char*>(base) +
                                    static_cast<size_t>(stride) * static_cast<size_t>(index));
}

lumice::analytic::FeatureProvenance ToFeatureProvenance(const LUMICE_ANALYTIC_FeatureProvenance& value) {
  lumice::analytic::FeatureProvenance out;
  out.member_index = value.member_index;
  out.layer_index = value.layer_index;
  out.interface_index = value.interface_index;
  out.spectrum_node_id = value.spectrum_node_id;
  out.source_node_id = value.source_node_id;
  out.sample_index = value.sample_index;
  return out;
}

LUMICE_ANALYTIC_FeatureProvenance FromFeatureProvenance(const lumice::analytic::FeatureProvenance& value) {
  LUMICE_ANALYTIC_FeatureProvenance out{};
  out.member_index = value.member_index;
  out.layer_index = value.layer_index;
  out.interface_index = value.interface_index;
  out.spectrum_node_id = value.spectrum_node_id;
  out.source_node_id = value.source_node_id;
  out.sample_index = value.sample_index;
  return out;
}

bool ToConstraintKind(int value, lumice::analytic::ConstraintKind* out) {
  if (value < LUMICE_ANALYTIC_CONSTRAINT_DOMAIN || value > LUMICE_ANALYTIC_CONSTRAINT_WEIGHT) {
    return false;
  }
  *out = static_cast<lumice::analytic::ConstraintKind>(value);
  return true;
}

LUMICE_ANALYTIC_ErrorCode ToFeatureSupportSample(const LUMICE_ANALYTIC_FeatureSupportSample& value,
                                                 int coordinate_dimension, uint32_t support_version,
                                                 lumice::analytic::FeatureSupportSample* out) {
  namespace an = lumice::analytic;
  const size_t required_size =
      support_version == LUMICE_ANALYTIC_FEATURE_SUPPORT_VERSION_V1 ? kFeatureSupportSampleV9Size :
      support_version == LUMICE_ANALYTIC_FEATURE_SUPPORT_VERSION_V2 ? kFeatureSupportSampleV10Size :
                                                                      sizeof(LUMICE_ANALYTIC_FeatureSupportSample);
  if (value.struct_size < required_size || value.support_dimension < 0 || value.constraint_count < 0) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  if ((coordinate_dimension > 0 && value.coordinates == nullptr) ||
      (value.support_dimension > 0 && value.active_coordinates == nullptr) ||
      (value.constraint_count > 0 && value.constraints == nullptr)) {
    return LUMICE_ANALYTIC_ERR_NULL_ARG;
  }
  if (value.measure_kind < LUMICE_ANALYTIC_SUPPORT_ATOM || value.measure_kind > LUMICE_ANALYTIC_SUPPORT_CONTINUOUS ||
      (value.finite_width != 0 && value.finite_width != 1) ||
      (value.direction_jacobian_available != 0 && value.direction_jacobian_available != 1) ||
      (value.numerically_available != 0 && value.numerically_available != 1) ||
      (support_version >= LUMICE_ANALYTIC_FEATURE_SUPPORT_VERSION_V2 && value.accumulates_measure != 0 &&
       value.accumulates_measure != 1)) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  if (value.direction_jacobian != nullptr && value.direction_jacobian_column_available == nullptr) {
    return LUMICE_ANALYTIC_ERR_NULL_ARG;
  }
  if (value.direction_jacobian_available != 0 && value.direction_jacobian == nullptr) {
    return LUMICE_ANALYTIC_ERR_NULL_ARG;
  }
  if (value.constraint_count > 0 && (value.constraint_stride < sizeof(LUMICE_ANALYTIC_SupportConstraint) ||
                                     value.constraint_stride % alignof(LUMICE_ANALYTIC_SupportConstraint) != 0)) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  out->sample_id = value.sample_id;
  out->provenance = ToFeatureProvenance(value.provenance);
  out->measure_kind = static_cast<an::SupportMeasureKind>(value.measure_kind);
  out->support_dimension = value.support_dimension;
  out->finite_width = value.finite_width != 0;
  out->accumulates_measure =
      support_version >= LUMICE_ANALYTIC_FEATURE_SUPPORT_VERSION_V2 ? value.accumulates_measure != 0 : true;
  if (support_version >= LUMICE_ANALYTIC_FEATURE_SUPPORT_VERSION_V3) {
    if (value.mapping_evidence_kind < LUMICE_ANALYTIC_MAPPING_EVIDENCE_NONE ||
        value.mapping_evidence_kind > LUMICE_ANALYTIC_MAPPING_EVIDENCE_EXACT_IMAGE_DIMENSION_UPPER_BOUND) {
      return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
    }
    if (value.mapping_evidence_kind == LUMICE_ANALYTIC_MAPPING_EVIDENCE_NONE) {
      if (value.image_dimension_upper_bound != 0 || value.mapping_error_bound != 0.0) {
        return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
      }
    } else {
      out->mapping_evidence_kind = an::MappingEvidenceKind::kExactImageDimensionUpperBound;
      out->image_dimension_upper_bound = value.image_dimension_upper_bound;
      out->mapping_error_bound = value.mapping_error_bound;
    }
  }
  if (coordinate_dimension > 0) {
    out->coordinates.assign(value.coordinates, value.coordinates + coordinate_dimension);
  }
  if (value.support_dimension > 0) {
    out->active_coordinates.assign(value.active_coordinates, value.active_coordinates + value.support_dimension);
  }
  std::copy(value.direction, value.direction + 3, out->direction);
  out->weight = value.weight;
  out->direction_jacobian_available = value.direction_jacobian_available != 0;
  if (value.direction_jacobian != nullptr) {
    out->direction_jacobian.assign(value.direction_jacobian,
                                   value.direction_jacobian + 3 * static_cast<size_t>(coordinate_dimension));
    out->direction_jacobian_column_available.assign(
        value.direction_jacobian_column_available,
        value.direction_jacobian_column_available + static_cast<size_t>(coordinate_dimension));
  }
  out->direction_jacobian_error = value.direction_jacobian_error;
  out->direction_jacobian_resolution = value.direction_jacobian_resolution;
  out->numerically_available = value.numerically_available != 0;
  out->constraints.reserve(static_cast<size_t>(value.constraint_count));
  for (int index = 0; index < value.constraint_count; ++index) {
    const LUMICE_ANALYTIC_SupportConstraint& input = *StrideElement(value.constraints, value.constraint_stride, index);
    if (input.struct_size < sizeof(LUMICE_ANALYTIC_SupportConstraint)) {
      return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
    }
    if (input.name == nullptr) {
      return LUMICE_ANALYTIC_ERR_NULL_ARG;
    }
    an::SupportConstraint constraint;
    if (!ToConstraintKind(input.kind, &constraint.kind) ||
        (input.numerically_available != 0 && input.numerically_available != 1) ||
        (input.gradient_available != 0 && input.gradient_available != 1)) {
      return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
    }
    if (input.gradient_available != 0 && coordinate_dimension > 0 && input.gradient == nullptr) {
      return LUMICE_ANALYTIC_ERR_NULL_ARG;
    }
    if (input.gradient_available == 0 && input.gradient != nullptr) {
      return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
    }
    constraint.name = input.name;
    constraint.layer_index = input.layer_index;
    constraint.interface_index = input.interface_index;
    constraint.value = input.value;
    constraint.numerically_available = input.numerically_available != 0;
    constraint.gradient_available = input.gradient_available != 0;
    if (constraint.gradient_available && coordinate_dimension > 0) {
      constraint.gradient.assign(input.gradient, input.gradient + coordinate_dimension);
    }
    out->constraints.push_back(std::move(constraint));
  }
  return LUMICE_ANALYTIC_OK;
}

LUMICE_ANALYTIC_ErrorCode ToFeatureSupportBatch(const LUMICE_ANALYTIC_FeatureSupportBatch* input,
                                                lumice::analytic::FeatureSupportBatch* out) {
  namespace an = lumice::analytic;
  if (input == nullptr) {
    return LUMICE_ANALYTIC_ERR_NULL_ARG;
  }
  if (input->struct_size < kFeatureSupportBatchV9Size || input->sample_count < 0 || input->edge_count < 0 ||
      input->coordinate_dimension < 0 || (input->complete_visit != 0 && input->complete_visit != 1) ||
      (input->materialization_complete != 0 && input->materialization_complete != 1)) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  const bool dynamic_version = input->version >= LUMICE_ANALYTIC_FEATURE_SUPPORT_VERSION_V2 &&
                               input->version <= LUMICE_ANALYTIC_FEATURE_SUPPORT_VERSION;
  if (input->version != LUMICE_ANALYTIC_FEATURE_SUPPORT_VERSION_V1 && !dynamic_version) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  const size_t required_batch_size =
      input->version == LUMICE_ANALYTIC_FEATURE_SUPPORT_VERSION_V1 ? kFeatureSupportBatchV9Size :
      input->version < LUMICE_ANALYTIC_FEATURE_SUPPORT_VERSION     ? kFeatureSupportBatchV11Size :
                                                                     sizeof(LUMICE_ANALYTIC_FeatureSupportBatch);
  if ((!dynamic_version && input->coordinate_dimension > LUMICE_ANALYTIC_MAX_FEATURE_COORDINATE_DIMENSION) ||
      input->struct_size < required_batch_size) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  constexpr size_t kMaximumInputScalars = 64u * 1024u * 1024u;
  if (input->coordinate_dimension > 0 && static_cast<size_t>(input->sample_count) >
                                             kMaximumInputScalars / static_cast<size_t>(input->coordinate_dimension)) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  const int cell_axis_count = dynamic_version ? input->cell_axis_count : 0;
  const int parameter_descriptor_count =
      input->version >= LUMICE_ANALYTIC_FEATURE_SUPPORT_VERSION ? input->parameter_descriptor_count : 0;
  const int scope_count = input->version >= LUMICE_ANALYTIC_FEATURE_SUPPORT_VERSION ? input->scope_count : 0;
  if (cell_axis_count < 0 || parameter_descriptor_count < 0 || scope_count < 0) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  if ((input->sample_count > 0 && input->samples == nullptr) || (input->edge_count > 0 && input->edges == nullptr)) {
    return LUMICE_ANALYTIC_ERR_NULL_ARG;
  }
  if (dynamic_version && cell_axis_count > 0 && input->cell_axes == nullptr) {
    return LUMICE_ANALYTIC_ERR_NULL_ARG;
  }
  if ((parameter_descriptor_count > 0 && input->parameter_descriptors == nullptr) ||
      (scope_count > 0 && input->scopes == nullptr)) {
    return LUMICE_ANALYTIC_ERR_NULL_ARG;
  }
  const size_t required_sample_size =
      input->version == LUMICE_ANALYTIC_FEATURE_SUPPORT_VERSION_V1 ? kFeatureSupportSampleV9Size :
      input->version == LUMICE_ANALYTIC_FEATURE_SUPPORT_VERSION_V2 ? kFeatureSupportSampleV10Size :
                                                                     sizeof(LUMICE_ANALYTIC_FeatureSupportSample);
  if (input->sample_count > 0 && (input->sample_stride < required_sample_size ||
                                  input->sample_stride % alignof(LUMICE_ANALYTIC_FeatureSupportSample) != 0)) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  out->version = input->version;
  out->coordinate_dimension = input->coordinate_dimension;
  out->visited_row_count = input->visited_row_count;
  out->complete_visit = input->complete_visit != 0;
  out->materialization_complete = input->materialization_complete != 0;
  out->samples.reserve(static_cast<size_t>(input->sample_count));
  for (int index = 0; index < input->sample_count; ++index) {
    an::FeatureSupportSample sample;
    const LUMICE_ANALYTIC_ErrorCode code =
        ToFeatureSupportSample(*StrideElement(input->samples, input->sample_stride, index), input->coordinate_dimension,
                               input->version, &sample);
    if (code != LUMICE_ANALYTIC_OK) {
      return code;
    }
    out->samples.push_back(std::move(sample));
  }
  out->edges.reserve(static_cast<size_t>(input->edge_count));
  for (int index = 0; index < input->edge_count; ++index) {
    const LUMICE_ANALYTIC_FeatureSupportEdge& edge = input->edges[index];
    out->edges.push_back({ edge.first, edge.second, edge.parameter_distance });
  }
  out->cell_axes.reserve(static_cast<size_t>(cell_axis_count));
  for (int index = 0; index < cell_axis_count; ++index) {
    const LUMICE_ANALYTIC_FeatureSupportCellAxis& axis = input->cell_axes[index];
    out->cell_axes.push_back(
        { axis.cell_id, axis.coordinate_index, axis.lower, axis.center, axis.upper, axis.parameter_span });
  }
  out->parameter_descriptors.reserve(static_cast<size_t>(parameter_descriptor_count));
  for (int index = 0; index < parameter_descriptor_count; ++index) {
    const LUMICE_ANALYTIC_FeatureParameterDescriptor& parameter = input->parameter_descriptors[index];
    if (parameter.role < LUMICE_ANALYTIC_FEATURE_PARAMETER_UNSPECIFIED ||
        parameter.role > LUMICE_ANALYTIC_FEATURE_PARAMETER_POSE) {
      return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
    }
    out->parameter_descriptors.push_back({ static_cast<an::FeatureParameterRole>(parameter.role), parameter.group_id });
  }
  out->scopes.reserve(static_cast<size_t>(scope_count));
  for (int index = 0; index < scope_count; ++index) {
    const LUMICE_ANALYTIC_FeatureSupportScope& scope = input->scopes[index];
    if (scope.kind < LUMICE_ANALYTIC_FEATURE_SCOPE_JOINT || scope.kind > LUMICE_ANALYTIC_FEATURE_SCOPE_CONDITIONAL) {
      return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
    }
    out->scopes.push_back({ scope.scope_id, scope.cell_id, static_cast<an::FeatureSupportScopeKind>(scope.kind) });
  }
  std::string error;
  return an::ValidateFeatureSupportBatch(*out, &error) ? LUMICE_ANALYTIC_OK : LUMICE_ANALYTIC_ERR_INVALID_VALUE;
}

bool ToFeatureDiscoveryOptions(const LUMICE_ANALYTIC_FeatureDiscoveryOptions* input,
                               lumice::analytic::FeatureDiscoveryOptions* out) {
  if (input == nullptr) {
    return true;
  }
  if (input->struct_size < sizeof(LUMICE_ANALYTIC_FeatureDiscoveryOptions)) {
    return false;
  }
  auto positive_or_default = [](double value, double* destination) {
    if (!std::isfinite(value) || value < 0.0) {
      return false;
    }
    if (value > 0.0) {
      *destination = value;
    }
    return true;
  };
  if (!positive_or_default(input->margin_tolerance, &out->margin_tolerance) ||
      !positive_or_default(input->rank_relative_tolerance, &out->rank_relative_tolerance) ||
      !positive_or_default(input->sky_merge_tolerance, &out->sky_merge_tolerance) ||
      input->maximum_refinement_steps < 0 || input->sky_z_bins < 0 || input->sky_azimuth_bins < 0) {
    return false;
  }
  if (input->maximum_refinement_steps > 0) {
    out->maximum_refinement_steps = input->maximum_refinement_steps;
  }
  if (input->sky_z_bins > 0) {
    out->sky_z_bins = input->sky_z_bins;
  }
  if (input->sky_azimuth_bins > 0) {
    out->sky_azimuth_bins = input->sky_azimuth_bins;
  }
  return out->sky_z_bins >= 4 && out->sky_z_bins % 2 == 0 && out->sky_azimuth_bins >= 8 &&
         out->sky_azimuth_bins % 2 == 0;
}

struct FeatureDiscoveryResultStorage {
  std::deque<std::string> strings;
  std::vector<std::vector<const char*>> active_constraint_pointers;
  std::vector<std::vector<int>> scope_active_coordinates;
  std::vector<std::vector<LUMICE_ANALYTIC_FeatureParameterDescriptor>> scope_active_parameters;
  std::vector<LUMICE_ANALYTIC_FeatureCandidateScope> candidate_scopes;
  std::vector<LUMICE_ANALYTIC_FeatureCandidate> candidates;
  std::vector<LUMICE_ANALYTIC_FeatureMechanismRecord> mechanisms;
  std::vector<LUMICE_ANALYTIC_SkyFieldNode> sky_field;
};

LUMICE_ANALYTIC_ErrorCode FillFeatureDiscoveryResult(const lumice::analytic::FeatureDiscoveryResult& result,
                                                     LUMICE_ANALYTIC_FeatureDiscoveryResult* out) {
  auto storage = std::make_unique<FeatureDiscoveryResultStorage>();
  storage->active_constraint_pointers.reserve(result.candidates.size());
  storage->scope_active_coordinates.reserve(result.candidates.size());
  storage->scope_active_parameters.reserve(result.candidates.size());
  storage->candidate_scopes.reserve(result.candidates.size());
  storage->candidates.reserve(result.candidates.size());
  for (const lumice::analytic::FeatureCandidate& input : result.candidates) {
    storage->active_constraint_pointers.emplace_back();
    auto& names = storage->active_constraint_pointers.back();
    names.reserve(input.active_constraints.size());
    for (const std::string& name : input.active_constraints) {
      storage->strings.push_back(name);
      names.push_back(storage->strings.back().c_str());
    }
    storage->strings.push_back(input.reason);
    LUMICE_ANALYTIC_FeatureCandidate candidate{};
    candidate.mechanism = static_cast<int>(input.mechanism);
    candidate.status = static_cast<int>(input.status);
    candidate.provenance = FromFeatureProvenance(input.provenance);
    std::copy(input.direction, input.direction + 3, candidate.direction);
    candidate.support_dimension = input.support_dimension;
    candidate.mapping_rank = input.mapping_rank;
    std::copy(input.singular_values, input.singular_values + 2, candidate.singular_values);
    candidate.weighted_mass = input.weighted_mass;
    candidate.has_weight_sides = input.has_weight_sides ? 1 : 0;
    std::copy(input.weight_sides, input.weight_sides + 2, candidate.weight_sides);
    candidate.residual = input.residual;
    candidate.resolution = input.resolution;
    candidate.active_constraint_count = static_cast<int>(names.size());
    candidate.active_constraints = names.empty() ? nullptr : names.data();
    candidate.reason = storage->strings.back().c_str();
    storage->candidates.push_back(candidate);

    storage->scope_active_coordinates.push_back(input.scope_active_coordinates);
    storage->scope_active_parameters.emplace_back();
    auto& parameters = storage->scope_active_parameters.back();
    parameters.reserve(input.scope_parameters.size());
    for (const lumice::analytic::FeatureParameterDescriptor& parameter : input.scope_parameters) {
      parameters.push_back({ static_cast<int>(parameter.role), parameter.group_id });
    }
    LUMICE_ANALYTIC_FeatureCandidateScope scope{};
    scope.struct_size = sizeof(scope);
    scope.scope_id = input.scope_id;
    scope.kind = static_cast<int>(input.scope_kind);
    scope.active_coordinate_count = static_cast<int>(input.scope_active_coordinates.size());
    scope.active_coordinates =
        input.scope_active_coordinates.empty() ? nullptr : storage->scope_active_coordinates.back().data();
    scope.active_parameters = parameters.empty() ? nullptr : parameters.data();
    scope.fixed_spectrum_node_id = input.provenance.spectrum_node_id;
    scope.fixed_source_node_id = input.provenance.source_node_id;
    storage->candidate_scopes.push_back(scope);
  }
  storage->mechanisms.reserve(result.mechanisms.size());
  for (const lumice::analytic::FeatureMechanismRecord& input : result.mechanisms) {
    storage->strings.push_back(input.reason);
    LUMICE_ANALYTIC_FeatureMechanismRecord record{};
    record.mechanism = static_cast<int>(input.mechanism);
    record.status = static_cast<int>(input.status);
    record.candidate_count = input.candidate_count;
    record.reason = storage->strings.back().c_str();
    storage->mechanisms.push_back(record);
  }
  storage->sky_field.reserve(result.sky_field.size());
  for (const lumice::analytic::SkyFieldNode& input : result.sky_field) {
    LUMICE_ANALYTIC_SkyFieldNode node{};
    std::copy(input.direction, input.direction + 3, node.direction);
    node.value = input.value;
    node.normalized_value = input.normalized_value;
    node.gradient_norm = input.gradient_norm;
    std::copy(input.hessian_eigenvalues, input.hessian_eigenvalues + 2, node.hessian_eigenvalues);
    node.error = input.error;
    node.resolution = input.resolution;
    node.sample_count = input.sample_count;
    node.status = static_cast<int>(input.status);
    storage->sky_field.push_back(node);
  }
  out->visited_row_count = result.visited_row_count;
  out->evaluated_sample_count = result.evaluated_sample_count;
  out->complete_visit = result.complete_visit ? 1 : 0;
  out->materialization_complete = result.materialization_complete ? 1 : 0;
  out->candidate_count = static_cast<int>(storage->candidates.size());
  out->candidates = storage->candidates.empty() ? nullptr : storage->candidates.data();
  out->mechanism_count = static_cast<int>(storage->mechanisms.size());
  out->mechanisms = storage->mechanisms.empty() ? nullptr : storage->mechanisms.data();
  out->sky_field_count = static_cast<int>(storage->sky_field.size());
  out->sky_field = storage->sky_field.empty() ? nullptr : storage->sky_field.data();
  out->storage = storage.release();
  return LUMICE_ANALYTIC_OK;
}

LUMICE_ANALYTIC_ErrorCode DiscoverFeaturesImpl(const LUMICE_ANALYTIC_FeatureSupportBatch* batch,
                                               const LUMICE_ANALYTIC_FeatureDiscoveryOptions* options,
                                               LUMICE_ANALYTIC_FeatureReevaluateFn reevaluate, void* user_data,
                                               LUMICE_ANALYTIC_FeatureDiscoveryResult* out) {
  namespace an = lumice::analytic;
  if (out->struct_size < sizeof(LUMICE_ANALYTIC_FeatureDiscoveryResult)) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  an::FeatureSupportBatch kernel_batch;
  const LUMICE_ANALYTIC_ErrorCode batch_code = ToFeatureSupportBatch(batch, &kernel_batch);
  if (batch_code != LUMICE_ANALYTIC_OK) {
    return batch_code;
  }
  an::FeatureDiscoveryOptions kernel_options;
  if (!ToFeatureDiscoveryOptions(options, &kernel_options)) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  an::FeatureReevaluateFn kernel_callback;
  if (reevaluate != nullptr) {
    const int coordinate_dimension = kernel_batch.coordinate_dimension;
    const uint32_t support_version = kernel_batch.version;
    kernel_callback = [reevaluate, user_data, coordinate_dimension, support_version](
                          const an::FeatureReevaluationRequest& request, an::FeatureSupportSample* sample,
                          std::string* error) {
      LUMICE_ANALYTIC_FeatureReevaluationRequest c_request{};
      c_request.provenance = FromFeatureProvenance(request.provenance);
      c_request.coordinate_dimension = coordinate_dimension;
      c_request.coordinates = request.coordinates.data();
      LUMICE_ANALYTIC_FeatureSupportSample c_sample{};
      c_sample.struct_size = sizeof(c_sample);
      if (reevaluate(&c_request, &c_sample, user_data) == 0) {
        if (error != nullptr) {
          *error = "the feature re-evaluation callback reported numerical unavailability";
        }
        return false;
      }
      const LUMICE_ANALYTIC_ErrorCode code =
          ToFeatureSupportSample(c_sample, coordinate_dimension, support_version, sample);
      if (code != LUMICE_ANALYTIC_OK && error != nullptr) {
        *error = "the feature re-evaluation callback returned a malformed sample";
      }
      return code == LUMICE_ANALYTIC_OK;
    };
  }
  return FillFeatureDiscoveryResult(an::DiscoverFeatures(kernel_batch, kernel_options, kernel_callback), out);
}

}  // namespace

extern "C" {

LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_EvaluatePath(const LUMICE_ANALYTIC_Crystal* crystal, const int* faces,
                                                       int face_count, double refractive_index,
                                                       const double incident_direction[3], const double pose[9],
                                                       LUMICE_ANALYTIC_PathEvaluation* out) {
  if (out == nullptr) {
    return LUMICE_ANALYTIC_ERR_NULL_ARG;
  }
  ZeroAfterStructSize(out);
  // No exception crosses the C boundary: an allocation failure is ERR_UNKNOWN, with out zero-filled
  // (the storage it would have owned was never released to it).
  try {
    const LUMICE_ANALYTIC_ErrorCode code =
        EvaluatePathImpl(crystal, faces, face_count, refractive_index, incident_direction, pose, out);
    if (code != LUMICE_ANALYTIC_OK) {
      ZeroAfterStructSize(out);
    }
    return code;
  } catch (...) {
    ZeroAfterStructSize(out);
    return LUMICE_ANALYTIC_ERR_UNKNOWN;
  }
}

void LUMICE_ANALYTIC_ReleasePathEvaluation(LUMICE_ANALYTIC_PathEvaluation* eval) {
  if (eval == nullptr || eval->struct_size < sizeof(LUMICE_ANALYTIC_PathEvaluation)) {
    return;
  }
  // Reclaims the block released to the caller by EvaluatePath; destroyed at scope exit.
  std::unique_ptr<PathEvaluationStorage> owned(static_cast<PathEvaluationStorage*>(eval->storage));
  ZeroAfterStructSize(eval);
}

LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_EvaluateDiagnosticFieldBatch(
    const LUMICE_ANALYTIC_Crystal* crystal, const int* faces, int face_count,
    const LUMICE_ANALYTIC_DiagnosticFieldRow* rows, int count, LUMICE_ANALYTIC_DiagnosticFieldResult* out_results) {
  if (count < 0) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  if (count == 0) {
    return LUMICE_ANALYTIC_OK;
  }
  if (out_results == nullptr) {
    return LUMICE_ANALYTIC_ERR_NULL_ARG;
  }
  const size_t stride = out_results[0].struct_size;
  // A version 7 result is the frozen first-published prefix. Future appended fields are optional;
  // the caller's size remains the stride and bounds every write through ZeroAfterStructSize.
  if (stride < kDiagnosticFieldResultV7Size) {
    ZeroAfterStructSize(&out_results[0]);
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  bool uniform = true;
  for (int i = 0; i < count; i++) {
    LUMICE_ANALYTIC_DiagnosticFieldResult* out = DiagnosticElementAt(out_results, stride, i);
    uniform = uniform && out->struct_size == stride;
    ZeroAfterStructSize(out);
  }
  if (!uniform) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  try {
    const LUMICE_ANALYTIC_ErrorCode code =
        EvaluateDiagnosticFieldBatchImpl(crystal, faces, face_count, rows, count, out_results, stride);
    if (code != LUMICE_ANALYTIC_OK) {
      for (int i = 0; i < count; i++) {
        ReleaseDiagnosticFieldStorage(DiagnosticElementAt(out_results, stride, i));
      }
    }
    return code;
  } catch (...) {
    for (int i = 0; i < count; i++) {
      ReleaseDiagnosticFieldStorage(DiagnosticElementAt(out_results, stride, i));
    }
    return LUMICE_ANALYTIC_ERR_UNKNOWN;
  }
}

void LUMICE_ANALYTIC_ReleaseDiagnosticFieldResult(LUMICE_ANALYTIC_DiagnosticFieldResult* result) {
  if (result == nullptr || result->struct_size < kDiagnosticFieldResultV7Size) {
    return;
  }
  ReleaseDiagnosticFieldStorage(result);
}

LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_TraceFiberBatch(const LUMICE_ANALYTIC_Crystal* crystal,
                                                          const LUMICE_ANALYTIC_FiberProblem* problems, int count,
                                                          const LUMICE_ANALYTIC_ContinuationOptions* options,
                                                          LUMICE_ANALYTIC_FiberResult* out_results) {
  if (count < 0) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;  // the array's length is unknown: touch nothing
  }
  if (count == 0) {
    return LUMICE_ANALYTIC_OK;
  }
  if (out_results == nullptr) {
    return LUMICE_ANALYTIC_ERR_NULL_ARG;
  }
  // The stride is the caller's sizeof (doc/analytic-api.md section 8.2). Below the version 4 layout
  // it cannot be walked: only element 0 is known to exist.
  const size_t stride = out_results[0].struct_size;
  if (stride < kFiberResultV4Size) {
    ZeroAfterStructSize(&out_results[0]);
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  bool uniform = true;
  for (int i = 0; i < count; i++) {
    LUMICE_ANALYTIC_FiberResult* out = ElementAt(out_results, stride, i);
    uniform = uniform && out->struct_size == stride;
    ZeroAfterStructSize(out);
  }
  if (!uniform) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  try {
    const LUMICE_ANALYTIC_ErrorCode code = TraceFiberBatchImpl(crystal, problems, count, options, out_results, stride);
    if (code != LUMICE_ANALYTIC_OK) {
      for (int i = 0; i < count; i++) {
        ReleaseFiberStorage(ElementAt(out_results, stride, i));
      }
    }
    return code;
  } catch (...) {
    // Elements filled before the failure own their storage; give it back before zeroing.
    for (int i = 0; i < count; i++) {
      ReleaseFiberStorage(ElementAt(out_results, stride, i));
    }
    return LUMICE_ANALYTIC_ERR_UNKNOWN;
  }
}

LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_TraceFiber(const LUMICE_ANALYTIC_Crystal* crystal,
                                                     const LUMICE_ANALYTIC_FiberProblem* problem,
                                                     const LUMICE_ANALYTIC_ContinuationOptions* options,
                                                     LUMICE_ANALYTIC_FiberResult* out_result) {
  return LUMICE_ANALYTIC_TraceFiberBatch(crystal, problem, 1, options, out_result);
}

void LUMICE_ANALYTIC_ReleaseFiberResult(LUMICE_ANALYTIC_FiberResult* result) {
  if (result == nullptr || result->struct_size < kFiberResultV4Size) {
    return;
  }
  ReleaseFiberStorage(result);
}

LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_DiscoverComponents(const LUMICE_ANALYTIC_Crystal* crystal,
                                                             const LUMICE_ANALYTIC_DiscoveryProblem* problem,
                                                             const LUMICE_ANALYTIC_DiscoveryOptions* options,
                                                             const LUMICE_ANALYTIC_ContinuationOptions* continuation,
                                                             LUMICE_ANALYTIC_DiscoveryResult* out_result) {
  if (out_result == nullptr) {
    return LUMICE_ANALYTIC_ERR_NULL_ARG;
  }
  ZeroAfterStructSize(out_result);
  // No exception crosses the C boundary; the storage is released to out_result only on success.
  try {
    const LUMICE_ANALYTIC_ErrorCode code = DiscoverComponentsImpl(crystal, problem, options, continuation, out_result);
    if (code != LUMICE_ANALYTIC_OK) {
      ZeroAfterStructSize(out_result);
    }
    return code;
  } catch (...) {
    ZeroAfterStructSize(out_result);
    return LUMICE_ANALYTIC_ERR_UNKNOWN;
  }
}

void LUMICE_ANALYTIC_ReleaseDiscoveryResult(LUMICE_ANALYTIC_DiscoveryResult* result) {
  if (result == nullptr || result->struct_size < sizeof(LUMICE_ANALYTIC_DiscoveryResult)) {
    return;
  }
  // Reclaims the block released to the caller by DiscoverComponents; destroyed at scope exit.
  std::unique_ptr<DiscoveryResultStorage> owned(static_cast<DiscoveryResultStorage*>(result->storage));
  ZeroAfterStructSize(result);
}

LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_BandSum(const LUMICE_ANALYTIC_Crystal* crystal,
                                                  const LUMICE_ANALYTIC_BandSumProblem* problem,
                                                  LUMICE_ANALYTIC_BandSumResult* out_result) {
  if (out_result == nullptr) {
    return LUMICE_ANALYTIC_ERR_NULL_ARG;
  }
  ZeroAfterStructSize(out_result);
  // No exception crosses the C boundary; the storage is released to out_result only on success.
  try {
    const LUMICE_ANALYTIC_ErrorCode code = BandSumImpl(crystal, problem, out_result);
    if (code != LUMICE_ANALYTIC_OK) {
      ZeroAfterStructSize(out_result);
    }
    return code;
  } catch (...) {
    ZeroAfterStructSize(out_result);
    return LUMICE_ANALYTIC_ERR_UNKNOWN;
  }
}

void LUMICE_ANALYTIC_ReleaseBandSumResult(LUMICE_ANALYTIC_BandSumResult* result) {
  if (result == nullptr || result->struct_size < sizeof(LUMICE_ANALYTIC_BandSumResult)) {
    return;
  }
  std::unique_ptr<BandSumResultStorage> owned(static_cast<BandSumResultStorage*>(result->storage));
  ZeroAfterStructSize(result);
}

LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_DiscoverFeatures(const LUMICE_ANALYTIC_FeatureSupportBatch* batch,
                                                           const LUMICE_ANALYTIC_FeatureDiscoveryOptions* options,
                                                           LUMICE_ANALYTIC_FeatureReevaluateFn reevaluate,
                                                           void* user_data,
                                                           LUMICE_ANALYTIC_FeatureDiscoveryResult* out_result) {
  if (out_result == nullptr) {
    return LUMICE_ANALYTIC_ERR_NULL_ARG;
  }
  ZeroAfterStructSize(out_result);
  try {
    const LUMICE_ANALYTIC_ErrorCode code = DiscoverFeaturesImpl(batch, options, reevaluate, user_data, out_result);
    if (code != LUMICE_ANALYTIC_OK) {
      ZeroAfterStructSize(out_result);
    }
    return code;
  } catch (...) {
    ZeroAfterStructSize(out_result);
    return LUMICE_ANALYTIC_ERR_UNKNOWN;
  }
}

LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_GetFeatureCandidateScope(const LUMICE_ANALYTIC_FeatureDiscoveryResult* result,
                                                                   int candidate_index,
                                                                   LUMICE_ANALYTIC_FeatureCandidateScope* out_scope) {
  if (result == nullptr || out_scope == nullptr) {
    return LUMICE_ANALYTIC_ERR_NULL_ARG;
  }
  if (out_scope->struct_size < sizeof(LUMICE_ANALYTIC_FeatureCandidateScope)) {
    ZeroAfterStructSize(out_scope);
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  ZeroAfterStructSize(out_scope);
  if (result->struct_size < sizeof(LUMICE_ANALYTIC_FeatureDiscoveryResult) || result->storage == nullptr ||
      candidate_index < 0 || candidate_index >= result->candidate_count) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  const auto* storage = static_cast<const FeatureDiscoveryResultStorage*>(result->storage);
  if (static_cast<size_t>(candidate_index) >= storage->candidate_scopes.size()) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  *out_scope = storage->candidate_scopes[static_cast<size_t>(candidate_index)];
  return LUMICE_ANALYTIC_OK;
}

void LUMICE_ANALYTIC_ReleaseFeatureDiscoveryResult(LUMICE_ANALYTIC_FeatureDiscoveryResult* result) {
  if (result == nullptr || result->struct_size < sizeof(LUMICE_ANALYTIC_FeatureDiscoveryResult)) {
    return;
  }
  std::unique_ptr<FeatureDiscoveryResultStorage> owned(static_cast<FeatureDiscoveryResultStorage*>(result->storage));
  ZeroAfterStructSize(result);
}

}  // extern "C"
