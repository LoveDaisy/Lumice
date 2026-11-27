#ifndef LUMICE_ANALYTIC_CAUSTIC_INTERNAL_HPP_
#define LUMICE_ANALYTIC_CAUSTIC_INTERNAL_HPP_

// Internal types and cross-module helpers shared by the caustic continuation implementation TUs.
// Not part of any public or exported analytic surface.

#include <algorithm>
#include <cmath>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "analytic/caustic_continuation.hpp"
#include "analytic/path_chain.hpp"
#include "analytic/so3.hpp"

namespace lumice::analytic::caustic_detail {

// Minimum step size (radians) accepted by ValidateParams; sub-1e-10 steps approach float64 ULP
// stagnation on O(1) angular coordinates and are rejected cleanly at validation time.
constexpr double kMinAllowedStepRad = 1e-10;

// Non-owning, zero-allocation type-erased callable for synchronous internal callbacks.
template <typename Sig>
class FunctionRef;

template <typename Ret, typename... Args>
class FunctionRef<Ret(Args...)> {
 public:
  template <typename F, typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, FunctionRef> &&
                                                    std::is_invocable_r_v<Ret, F&, Args...>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  FunctionRef(F&& fn) noexcept
      : obj_(const_cast<void*>(static_cast<const void*>(std::addressof(fn)))),
        call_([](void* obj, Args... args) -> Ret {
          return (*static_cast<std::remove_reference_t<F>*>(obj))(std::forward<Args>(args)...);
        }) {}

  Ret operator()(Args... args) const { return call_(obj_, std::forward<Args>(args)...); }

 private:
  void* obj_ = nullptr;
  Ret (*call_)(void*, Args...) = nullptr;
};

// Condition-aware floor separating an identically constant (rank-0) map from a small rank-1/2 map.
inline double NumericalZeroSigmaFloor(double min_effective_margin) {
  return 1e-13 / std::sqrt(std::max(min_effective_margin, 1e-8));
}

inline bool CheckStop(const DiscoveryStop* stop) {
  if (stop == nullptr) {
    return false;
  }
  if (stop->token != nullptr) {
    stop->token->NotePoll(kPollFamilyCurve);
  }
  return stop->Stopped();
}

inline double WrapAxis(double v, const PoseAxis& ax) {
  if (!ax.periodic) {
    return v;
  }
  const double span = ax.Span();
  double r = std::fmod(v - ax.min_val, span);
  return ax.min_val + (r < 0.0 ? r + span : r);
}

inline double DeltaAxis(double a, double b, const PoseAxis& ax) {
  double d = a - b;
  if (ax.periodic) {
    const double span = ax.Span();
    d = std::fmod(d + 0.5 * span, span);
    d = (d < 0.0 ? d + span : d) - 0.5 * span;
  }
  return d;
}

inline bool InAxisBounds(double v, const PoseAxis& ax, double eps = 0.0) {
  return ax.periodic || (v >= ax.min_val - eps && v <= ax.max_val + eps);
}

inline double Dist2D(const double a[2], const double b[2], const PoseAxis& ax0, const PoseAxis& ax1) {
  return std::hypot(DeltaAxis(a[0], b[0], ax0), DeltaAxis(a[1], b[1], ax1));
}

inline double GridCoord(int idx, int n, const PoseAxis& ax) {
  return ax.periodic ? ax.min_val + ax.Span() * (static_cast<double>(idx) + 0.5) / static_cast<double>(n) :
                       ax.min_val + ax.Span() * static_cast<double>(idx) / static_cast<double>(n - 1);
}

// Conservative Lipschitz bound on optical / internal-TIR margins with respect to crystal angles.
inline double MarginLip(double refractive_index) {
  return std::max(6.0, 3.0 * refractive_index * refractive_index);
}

inline void CopyNodeToEvent(const PoseCurveNode& node, CurveEventKind kind, int detail_index, CurveEvent* ev) {
  ev->kind = kind;
  ev->arclength = node.arclength;
  so3::Copy3(node.param, ev->param);
  so3::Copy3(node.sky, ev->sky);
  std::copy_n(node.pose, 9, ev->pose);
  ev->detail_index = detail_index;
}

// Shared immutable context passed across 2D evaluation, projection, and continuation helpers.
struct Context2D {
  const FaceNormalTable& table;
  const int* slots;
  int slot_count;
  double refractive_index;
  const double* sun_dir;
  const PoseManifold& manifold;
  int ax0;
  int ax1;
  const CausticContinuationParams& params;
};

enum class ProbeStatus { kValue, kFound, kAbort };

struct Eval1D {
  bool valid = false;
  OpticalMarginVerdict margin;
  double param[3]{};
  double tangent[3]{};
  double pose[9]{};
  double u[3]{};
  double sky[3]{};
  double dx[3]{};
  double sky_speed = 0.0;
  double internal_disc[kMaxFaceCount]{};
  bool corridor_open = true;
  double corridor_margin = 1.0;
};

struct Eval2DJet1 {
  bool valid = false;
  OpticalMarginVerdict margin;
  double param[3]{};
  double pose[9]{};
  double sky[3]{};
  double x1[3]{};
  double x2[3]{};
  double sigma1 = 0.0;
  double sigma2 = 0.0;
  double j_val = 0.0;
  double generator_cross = 0.0;
};

struct Eval2DJet2 {
  bool valid = false;
  OpticalMarginVerdict margin;
  double param[3]{};
  double pose[9]{};
  double u[3]{};
  double sky[3]{};
  double x1[3]{};
  double x2[3]{};
  double sigma1 = 0.0;
  double sigma2 = 0.0;
  double j_val = 0.0;
  double grad_j[2]{};
  double grad_norm = 0.0;
  // Snell-desingularized J_reg = w * J (w = entry_disc * exit_disc for slot_count >= 2, else 1)
  // and its exact gradient grad J_reg = w * grad J + J * grad w.
  double j_reg = 0.0;
  double grad_j_reg[2]{};
  double grad_reg_norm = 0.0;
  double w_val = 1.0;
  double generator_cross = 0.0;
  double internal_disc[kMaxFaceCount]{};
  bool corridor_open = true;
};

enum class ProjectOutcome { kConverged, kDomainCrossed, kOpticalInvalid, kPoleHit, kSingularity, kDiverged };

struct ProjectResult {
  ProjectOutcome outcome = ProjectOutcome::kDiverged;
  double q[2]{};  // unwrapped coordinates relative to q_start
  Eval2DJet2 eval;
  int iterations = 0;
  int crossed_axis = -1;
  int failing_margin = -1;
};

struct Seed2D {
  double q[2]{};
  Eval2DJet2 eval;
  double priority = 0.0;
};

// Invariant: node_stationary.size() == nodes.size(), edge_corridor_open.size() == max(0, nodes - 1).
// For 2D rank-2 branches, node_pose_err.size() == nodes.size().
struct HalfBranch2D {
  std::vector<PoseCurveNode> nodes;
  std::vector<bool> node_stationary;
  std::vector<double> node_pose_err;
  std::vector<bool> edge_corridor_open;
  bool first_node_corridor_open = true;
  std::vector<CurveEvent> events;
  bool closed = false;
  bool budget_truncated = false;
  bool stop_requested = false;
  int unresolved_singularities = 0;
};

// caustic_events.cpp
bool EvaluateCorridorOpen(Corridor* corridor, const double pose[9], const double sun_dir[3], double refractive_index);
double EvaluateCorridorOverlapMargin(const Corridor* corridor, const double pose[9], const double sun_dir[3],
                                     double refractive_index);
void EvaluateCorridorStateAndMargin(Corridor* corridor, const double pose[9], const double sun_dir[3],
                                    double refractive_index, bool need_margin, bool* open_out, double* margin_out);
bool GoldenSectionProbe(double lo, double hi, bool maximize, int iters, FunctionRef<ProbeStatus(double, double*)> probe,
                        double* x_out);
void DetectStepTirOnsets(int slot_count, const double disc_a[kMaxFaceCount], const double disc_b[kMaxFaceCount],
                         double step_len, double margin_lip, int bisection_iters,
                         FunctionRef<bool(double, int, double*)> eval_disc, FunctionRef<void(double, int)> emit_tir);
int FindStepCorridorCrossings(bool open_a, double margin_a, bool open_b, double margin_b, double step_len,
                              int bisection_iters, FunctionRef<bool(double, bool, bool*, double*)> eval_corr,
                              double u_cuts_out[2]);
bool DetectStationaryPoint1D(const Eval1D& e_a, const Eval1D& e_b, double stationary_tol, int bisection_iters,
                             FunctionRef<Eval1D(double)> eval_u, double* u_stat_out, Eval1D* e_stat_out);
double GeneratorCrossNorm2D(const double param[3], int ax0, int ax1);
bool DominantLeftSingularVector(const double x1[3], const double x2[3], double e_sky[3]);
void AppendEventsWithinArclength(const std::vector<CurveEvent>& src, double max_arclength,
                                 std::vector<CurveEvent>* dst);
void ClipEventsToArclength(double max_arclength, std::vector<CurveEvent>* events);
Family1DParams MakeFamily1DParams(const CausticContinuationParams& params);
void MergeEdgeSegments(const std::vector<CurveSegmentKind>& edge_kind, const std::vector<bool>& edge_corridor_open,
                       std::vector<CurveSegment>* segments);
void BuildSegments(const std::vector<PoseCurveNode>& nodes, const std::vector<bool>& node_stationary,
                   const std::vector<bool>& edge_corridor_open, bool first_node_corridor_open, double speed_tol,
                   std::vector<CurveSegment>* segments);

// family_1d_continuation.cpp
Eval1D Evaluate1DAlongTangent(const FaceNormalTable& table, const int* slots, int slot_count, double refractive_index,
                              const double sun_dir[3], const double param_rad[3], const double tangent_dir[3],
                              Corridor* corridor, bool eval_corridor);
Eval1D Evaluate1DAtPose(const FaceNormalTable& table, const int* slots, int slot_count, double refractive_index,
                        const double sun_dir[3], const double param_rad[3], int vary_ax, Corridor* corridor,
                        bool eval_corridor);
PoseCurveNode NodeFromEval1D(const Eval1D& e, double arclength);
Eval1D BisectOpticalBoundary1D(FunctionRef<Eval1D(double, bool)> eval_fn, double t_valid, double t_invalid, int iters,
                               double* t_out, int* failing_index_out);
bool FindInteriorOpticalTransition1D(FunctionRef<Eval1D(double, bool)> eval_fn, double t_a, double m_a, double t_b,
                                     double m_b, double margin_lip, bool seek_valid, double* t_out);
CausticTraceResult Trace1DWithEvaluator(const PoseAxis& pax, int free_ax, int slot_count, double margin_lip,
                                        FunctionRef<Eval1D(double, bool)> eval_fn, bool track_corridor,
                                        const Family1DParams& params, const DiscoveryStop* stop);

// caustic_2d_continuation.cpp
// When require_margin is true (default), Eval2DJet1::valid reflects full continuation margin validity.
// When false, Eval2DJet1::valid reflects raw chain_ok so 1D-bisected optical boundary endpoints
// sitting within 1 ULP of kSnellEventTolerance still receive exact 2D Jet<2> SVD (sigma1, sigma2).
Eval2DJet1 Evaluate2DJet1(const Context2D& ctx, const double q_in[2], bool require_margin = true);
Eval2DJet2 Evaluate2DJet2(const Context2D& ctx, const double q_in[2], Corridor* corridor, bool eval_corridor);
ProjectResult ProjectOntoCaustic(const Context2D& ctx, const double q_start[2], double max_total_drift);
bool IsSeedCoveredByCurves(const Context2D& ctx, const Seed2D& seed, const std::vector<PoseCurve>& curves);
HalfBranch2D TraceHalfBranch2D(const Context2D& ctx, const double q_seed[2], const Eval2DJet2& seed_eval, int dir_sign,
                               bool allow_closure, Corridor* corridor, const DiscoveryStop* stop);
PoseCurve StitchHalfBranches(const HalfBranch2D& forward, const HalfBranch2D& backward, PoseCurveKind kind,
                             int seed_grid_n, double speed_tol, int unresolved_singularities);
PoseCurve StitchCausticCurve(const Context2D& ctx, const HalfBranch2D& forward, const HalfBranch2D& backward,
                             int seed_grid_n);

}  // namespace lumice::analytic::caustic_detail

#endif  // LUMICE_ANALYTIC_CAUSTIC_INTERNAL_HPP_
