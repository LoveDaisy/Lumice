// The ice-crystal path as a continuation map (src/analytic/path_fiber.hpp), and continuation on
// real paths: LI's optical conformance cases (LI docs/phase1-math-contract.md section 11) with
// LI's seeds, scenes and expectations. The direction and its derivative are checked against
// EvaluatePath and against a central difference that does not use Jet.
//
// symmetry_semantics: none — every case traces one concrete face sequence (doc/analytic-api.md
// section 3).

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <thread>
#include <utility>
#include <vector>

#include "analytic/fiber_continuation.hpp"
#include "analytic/path_evaluation.hpp"
#include "analytic/path_fiber.hpp"
#include "analytic/so3.hpp"

namespace lumice::analytic {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kN = 1.31;
using Mat = std::array<double, 9>;

Mat Exp(double x, double y, double z) {
  const double w[3] = { x, y, z };
  Mat r{};
  so3::Exp(w, r.data());
  return r;
}

LUMICE_ANALYTIC_Crystal Prism() {
  LUMICE_ANALYTIC_Crystal c{};
  c.kind = LUMICE_ANALYTIC_CRYSTAL_PRISM;
  c.height = 1.0;
  for (double& d : c.face_distance) {
    d = 1.0;
  }
  return c;
}

// LI minimum_deviation_incident / _oracle_incident_3_5: the in-plane ray of a symmetric 60 deg
// prism path, n = 1.31.
std::array<double, 3> MinimumDeviationIncident() {
  const double external = std::asin(kN * 0.5);
  return { -std::cos(external), std::sin(external), 0.0 };
}

// A traced path held with its table and slots, the way the C wrapper holds one fiber.
struct Path {
  FaceNormalTable table;
  std::vector<int> slots;
  std::array<double, 3> incident{};
  double n = kN;

  Path(const std::vector<int>& faces, const std::array<double, 3>& incident_direction, double index = kN)
      : slots(faces.size()), incident(incident_direction), n(index) {
    EXPECT_EQ(BuildFaceNormals(Prism(), &table), Status::kOk);
    EXPECT_EQ(ResolveFaceSequence(table, faces.data(), static_cast<int>(faces.size()), slots.data()), Status::kOk);
  }
  IcePathMap Map() const { return { table, slots.data(), static_cast<int>(slots.size()), n, incident.data() }; }
  std::array<double, 3> Outgoing(const Mat& pose) const {
    std::array<double, 3> d{};
    Map().Direction(pose.data(), d.data());
    return d;
  }
};

double Length(const TraceResult& r) {
  double s = 0.0;
  for (double a : r.arclength_increments) {
    s += a;
  }
  return s;
}

double MaxResidual(const TraceResult& r) {
  double m = 0.0;
  for (double v : r.residual_norms) {
    m = std::fmax(m, v);
  }
  return m;
}

double SetDistance(const TraceResult& a, const TraceResult& b) {
  auto one_way = [](const TraceResult& x, const TraceResult& y) {
    double worst = 0.0;
    for (int i = 0; i < x.PoseCount(); i++) {
      double best = 1e300;
      for (int j = 0; j < y.PoseCount(); j++) {
        best = std::fmin(best, so3::Distance(&x.poses[9 * i], &y.poses[9 * j]));
      }
      worst = std::fmax(worst, best);
    }
    return worst;
  };
  return std::fmax(one_way(a, b), one_way(b, a));
}

std::array<double, 9> RandomRotation(std::mt19937_64& rng) {
  std::normal_distribution<double> g;
  double q[4] = { g(rng), g(rng), g(rng), g(rng) };
  const double m = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
  for (double& v : q) {
    v /= m;
  }
  const double w = q[0], x = q[1], y = q[2], z = q[3];
  return { 1 - 2 * (y * y + z * z), 2 * (x * y - w * z),     2 * (x * z + w * y),
           2 * (x * y + w * z),     1 - 2 * (x * x + z * z), 2 * (y * z - w * x),
           2 * (x * z - w * y),     2 * (y * z + w * x),     1 - 2 * (x * x + y * y) };
}

// ---------------------------------------------------------------------------------------------
// The adapter itself
// ---------------------------------------------------------------------------------------------

TEST(PathFiber, DirectionIsEvaluatePathsOutgoingDirection) {
  const std::array<double, 3> sun = { 0.36, -0.48, -0.8 };
  for (const auto& faces : { std::vector<int>{ 3, 5 }, std::vector<int>{ 3, 5, 6, 7 }, std::vector<int>{ 3, 1, 5 } }) {
    const Path path(faces, sun);
    std::mt19937_64 rng(17);
    int valid = 0;
    for (int t = 0; t < 3000; t++) {
      const auto r = RandomRotation(rng);
      std::vector<double> seg(3 * (faces.size() + 1));
      std::vector<double> trans(faces.size());
      PathOutputs o{};
      o.segment_directions = seg.data();
      o.interface_transmittances = trans.data();
      if (!EvaluatePath(path.table, path.slots.data(), static_cast<int>(faces.size()), kN, sun.data(), r.data(), &o)) {
        continue;
      }
      valid++;
      const auto d = path.Outgoing(r);
      for (int i = 0; i < 3; i++) {
        EXPECT_EQ(d[i], o.outgoing_direction[i]);
      }
    }
    EXPECT_GT(valid, 50);
  }
}

// The residual Jacobian the continuation uses (a Jet<3> run through exp) against a central
// difference of the double direction along R exp(+-h e_k): the second chain shares nothing with Jet.
TEST(PathFiber, ResidualJacobianMatchesCentralDifference) {
  const auto incident = MinimumDeviationIncident();
  const Path path({ 3, 5 }, incident);
  const Path path4({ 3, 5, 6, 7 }, { 0.36, -0.48, -0.8 });
  std::mt19937_64 rng(5);
  int checked = 0;
  for (int t = 0; t < 4000 && checked < 60; t++) {
    const auto r = RandomRotation(rng);
    for (const Path* p : { &path, &path4 }) {
      DomainEvaluation dom;
      p->Map().Domain(r.data(), &dom);
      if (!dom.valid) {
        continue;
      }
      const auto target = p->Outgoing(r);
      const TargetChart chart = MakeTargetChart(target.data());
      double a[2][3];
      fiber_detail::LocalResidualJacobian(p->Map(), chart, r.data(), a);
      for (int k = 0; k < 3; k++) {
        const double h = 1e-6;
        double wp[3] = { 0, 0, 0 };
        double wm[3] = { 0, 0, 0 };
        wp[k] = h;
        wm[k] = -h;
        Mat rp{};
        Mat rm{};
        fiber_detail::ApplyCorrection(r.data(), wp, rp.data());
        fiber_detail::ApplyCorrection(r.data(), wm, rm.data());
        const auto dp = p->Outgoing(rp);
        const auto dm = p->Outgoing(rm);
        double resp[2];
        double resm[2];
        fiber_detail::ChartResidual(chart, dp.data(), resp);
        fiber_detail::ChartResidual(chart, dm.data(), resm);
        for (int i = 0; i < 2; i++) {
          const double fd = (resp[i] - resm[i]) / (2.0 * h);
          EXPECT_NEAR(a[i][k], fd, 1e-8 * (1.0 + std::fabs(fd))) << "i " << i << " k " << k;
        }
      }
      checked++;
    }
  }
  EXPECT_GE(checked, 60);
}

// Walks from a pose whose exit Snell discriminant is positive toward one where it is negative and
// returns the pose on that geodesic where it equals `level` (bisection on the domain's own margin).
Mat PoseAtExitSnell(const Path& path, const Mat& inside, const Mat& outside, double level) {
  double rel[9];
  so3::MatTMul(inside.data(), outside.data(), rel);
  double gen[3];
  so3::Log(rel, gen);
  auto exit_snell = [&](double t) {
    const double w[3] = { t * gen[0], t * gen[1], t * gen[2] };
    Mat r{};
    fiber_detail::ApplyCorrection(inside.data(), w, r.data());
    DomainEvaluation d;
    path.Map().Domain(r.data(), &d);
    return std::make_pair(d.margins[d.margin_count - 1], r);
  };
  double lo = 0.0;
  double hi = 1.0;
  for (int i = 0; i < 200; i++) {
    const double mid = 0.5 * (lo + hi);
    (exit_snell(mid).first > level ? lo : hi) = mid;
  }
  return exit_snell(hi).second;
}

TEST(PathFiber, SnellDiscriminantWithinToleranceIsTheTirEvent) {
  const auto incident = MinimumDeviationIncident();
  const Path path({ 3, 5 }, incident);
  // An inside pose and an exit-TIR pose with the entry and both cosines fine: search.
  std::mt19937_64 rng(23);
  Mat inside{};
  Mat outside{};
  bool have_in = false;
  bool have_out = false;
  for (int t = 0; t < 200000 && !(have_in && have_out); t++) {
    const auto r = RandomRotation(rng);
    DomainEvaluation d;
    path.Map().Domain(r.data(), &d);
    if (d.valid && d.margins[3] > 1e-3 && !have_in) {
      inside = r;
      have_in = true;
    }
    if (!d.valid && d.event == FiberReason::kTirBoundary && d.margin_count == 4 && d.margins[2] > 0.2 &&
        d.margins[3] < 0.0 && have_in) {
      // Only an `outside` close enough that the geodesic from `inside` stays in the entry domain.
      double rel[9];
      so3::MatTMul(inside.data(), r.data(), rel);
      double g[3];
      so3::Log(rel, g);
      if (so3::Norm3(g) < 0.6) {
        outside = r;
        have_out = true;
      }
    }
  }
  ASSERT_TRUE(have_in && have_out);
  for (double level : { 3e-9, 5e-8 }) {
    const Mat r = PoseAtExitSnell(path, inside, outside, level);
    DomainEvaluation d;
    path.Map().Domain(r.data(), &d);
    if (d.margin_count != 4) {
      ADD_FAILURE() << "the bisection left the entry domain at level " << level;
      continue;
    }
    EXPECT_NEAR(d.margins[3], level, 1e-12);
    if (level <= kSnellEventTolerance) {
      EXPECT_FALSE(d.valid);
      EXPECT_EQ(d.event, FiberReason::kTirBoundary);
      EXPECT_EQ(d.event_margin, d.margins[3]);
    } else {
      EXPECT_TRUE(d.valid);
    }
  }
}

// ---------------------------------------------------------------------------------------------
// C05 / C06 / C03 on the synthetic 3-5 branch
// LI: optical_fixture, test_synthetic_3_5_safe_step_sweep_converges_without_fixed_step_count,
//     test_synthetic_3_5_controller_threshold_perturbation_converges_consistently,
//     test_synthetic_3_5_is_invariant_under_orthogonal_target_basis
// ---------------------------------------------------------------------------------------------

// LI _oracle_tangent_basis: reference e3 (e2 when the target is within ~25 deg of e3), first =
// normalize(reference x d), second = d x first — a different chart from MakeTargetChart's.
TargetChart OracleChart(const std::array<double, 3>& d) {
  TargetChart c;
  double ref[3] = { 0.0, 0.0, 1.0 };
  if (std::fabs(d[2]) > 0.9) {
    ref[1] = 1.0;
    ref[2] = 0.0;
  }
  double first[3];
  so3::Cross3(ref, d.data(), first);
  const double m = so3::Norm3(first);
  for (int i = 0; i < 3; i++) {
    c.direction[i] = d[i];
    c.basis[0][i] = first[i] / m;
  }
  so3::Cross3(d.data(), c.basis[0], c.basis[1]);
  return c;
}

struct Synthetic35 {
  Path path{ { 3, 5 }, MinimumDeviationIncident() };
  Mat seed = Exp(0.15, 0.08, -0.05);
  TargetChart chart = OracleChart(path.Outgoing(seed));
  TraceResult Trace(const ContinuationParams& p) const { return TraceFiber(path.Map(), chart, seed.data(), p); }
};

TEST(PathFiber, C05Synthetic35ClosesUnderEverySafeStep) {
  const Synthetic35 s;
  std::vector<double> lengths;
  for (double step : { 0.03, 0.04, 0.08 }) {
    ContinuationParams p;
    p.initial_step = step;
    const TraceResult r = s.Trace(p);
    SCOPED_TRACE(step);
    EXPECT_EQ(r.status, FiberStatus::kClosed);
    EXPECT_EQ(r.reason, FiberReason::kClosedLoop);
    EXPECT_LE(MaxResidual(r), 1e-11);
    EXPECT_LE(so3::Distance(s.seed.data(), &r.poses[9 * (r.PoseCount() - 1)]), 2e-13);
    // Every accepted pose is on the branch: the outgoing direction is the target.
    for (int i = 0; i < r.PoseCount(); i++) {
      Mat pose{};
      std::copy(&r.poses[9 * i], &r.poses[9 * i] + 9, pose.begin());
      const auto d = s.path.Outgoing(pose);
      for (int k = 0; k < 3; k++) {
        EXPECT_NEAR(d[k], s.chart.direction[k], 4e-15);
      }
    }
    lengths.push_back(Length(r));
  }
  // LI contract section 10.1: the three lengths lie in [0.9643243178593905, 0.9647199228642988] on
  // LI's build; a span below 0.0017. The discrete lengths depend on the accepted steps, which an ulp
  // may move, so the band is LI's widened by the span criterion's own resolution.
  for (double l : lengths) {
    EXPECT_GT(l, 0.9643243178593905 - 1e-4);
    EXPECT_LT(l, 0.9647199228642988 + 1e-4);
  }
  EXPECT_LE(*std::max_element(lengths.begin(), lengths.end()) - *std::min_element(lengths.begin(), lengths.end()),
            0.0017);
}

TEST(PathFiber, C06Synthetic35ControllerPerturbationConvergesToTheSameLoop) {
  const Synthetic35 s;
  const TraceResult reference = s.Trace(ContinuationParams{});
  ContinuationParams p;
  p.minimum_step = 2e-5;
  p.maximum_step = 0.10;
  p.shrink_factor = 0.4;
  p.growth_factor = 1.15;
  p.maximum_retries = 10;
  const TraceResult perturbed = s.Trace(p);
  EXPECT_EQ(perturbed.status, FiberStatus::kClosed);
  EXPECT_EQ(reference.status, FiberStatus::kClosed);
  EXPECT_LE(MaxResidual(perturbed), 1e-11);
  EXPECT_LE(std::fabs(Length(perturbed) - Length(reference)), 0.0015);
  EXPECT_LE(SetDistance(perturbed, reference), 0.012);
  EXPECT_GE(so3::Dot3(perturbed.tangents.data(), reference.tangents.data()), 1.0 - 2e-14);
}

TEST(PathFiber, C03Synthetic35IsInvariantUnderTargetBasis) {
  const Synthetic35 s;
  const TraceResult reference = s.Trace(ContinuationParams{});
  const double rotation[2][2] = { { 0.0, -1.0 }, { 1.0, 0.0 } };
  const double reflection[2][2] = { { 1.0, 0.0 }, { 0.0, -1.0 } };
  for (const auto* q : { rotation, reflection }) {
    TargetChart c = s.chart;
    for (int j = 0; j < 2; j++) {
      for (int i = 0; i < 3; i++) {
        c.basis[j][i] = s.chart.basis[0][i] * q[0][j] + s.chart.basis[1][i] * q[1][j];
      }
    }
    const TraceResult changed = TraceFiber(s.path.Map(), c, s.seed.data(), ContinuationParams{});
    EXPECT_EQ(changed.status, reference.status);
    EXPECT_EQ(changed.reason, reference.reason);
    EXPECT_GE(so3::Dot3(changed.tangents.data(), reference.tangents.data()), 1.0 - 2e-14);
    EXPECT_LE(SetDistance(changed, reference), 2e-11);
    EXPECT_NEAR(Length(changed), Length(reference), 2e-11);
  }
}

// ---------------------------------------------------------------------------------------------
// C08 on the optics: exit TIR is reported as tir_boundary before the exit square root
// LI: test_3_5_tir_is_reported_before_the_unsafe_exit_square_root (tests/test_optics.py)
// ---------------------------------------------------------------------------------------------

TEST(PathFiber, C08ExitTirSeedIsATirBoundaryEvent) {
  const Path path({ 3, 5 }, MinimumDeviationIncident());
  const Mat seed = Exp(0.5447316801391622, -1.506228738763967, -1.190186580432801);
  DomainEvaluation d;
  path.Map().Domain(seed.data(), &d);
  EXPECT_FALSE(d.valid);
  EXPECT_EQ(d.event, FiberReason::kTirBoundary);
  EXPECT_GT(d.margins[1], 0.0);  // entry Snell fine
  EXPECT_LT(d.margins[3], 0.0);  // exit Snell negative
  const double target[3] = { 0.0, 0.0, 1.0 };
  const TraceResult r = TraceFiber(path.Map(), MakeTargetChart(target), seed.data(), ContinuationParams{});
  EXPECT_EQ(r.status, FiberStatus::kEventTerminated);
  EXPECT_EQ(r.reason, FiberReason::kTirBoundary);
  EXPECT_EQ(r.PoseCount(), 0);
}

// ---------------------------------------------------------------------------------------------
// The ch06 strip pixels: short loops close once; loops along the exit TIR boundary do not crawl
// LI: test_strip_short_loops_close_at_their_single_traversal_length,
//     test_strip_boundary_hugging_loops_no_longer_exhaust_the_step_budget
// Scene numbers (canonical incident, pixel targets, n) exported from LI's canonical_scene / camera.
// ---------------------------------------------------------------------------------------------

const std::array<double, 3> kCanonicalIncident = { -0.9659258262890683, 0.0, -0.25881904510252074 };

struct StripPixel {
  std::array<double, 3> seed_coordinates;
  std::array<double, 3> target;
  double loop_length;  // LI, Mac float64; 0 when the case only asserts closure
};

const StripPixel kStripPixels[] = {
  { { -1.5066578217593831, 0.39664283175724213, 0.0890374620046349 },
    { -0.9906067002414435, -0.0004143529650651491, 0.13674133884227968 },
    1.645239 },
  { { -1.4208326302791248, 0.614453957148551, -0.0473172496332926 },
    { -0.9875787288322464, -0.00041533461187009865, 0.15712441521050854 },
    2.375620 },
  { { -1.5933577786781372, -0.12433770957520761, 0.4211233203809581 },
    { -0.9822984877235305, -0.00041646852919308787, 0.1873219356349639 },
    3.111244 },
  { { -1.614678779370118, -0.3852552208498327, 1.2983286011517758 },
    { -0.9262116241489038, -0.010358269282414756, 0.37686169021130667 },
    0.0 },
  { { -1.68895386947514, 0.14376477560238074, 1.4516482547130902 },
    { -0.9133776525444067, -0.010310238289584585, 0.4069827549404039 },
    0.0 },
};

TEST(PathFiber, C06StripLoopsCloseOnceAndBoundaryHuggersDoNotCrawl) {
  const Path path({ 3, 5 }, kCanonicalIncident);
  for (const auto& px : kStripPixels) {
    const Mat seed = Exp(px.seed_coordinates[0], px.seed_coordinates[1], px.seed_coordinates[2]);
    const TraceResult r = TraceFiber(path.Map(), MakeTargetChart(px.target.data()), seed.data(), ContinuationParams{});
    SCOPED_TRACE(px.loop_length);
    EXPECT_EQ(r.status, FiberStatus::kClosed);
    EXPECT_NE(r.reason, FiberReason::kStepBudget);
    EXPECT_LE(MaxResidual(r), 1e-11);
    EXPECT_LE(so3::Distance(seed.data(), &r.poses[9 * (r.PoseCount() - 1)]), 1e-12);
    if (px.loop_length > 0.0) {
      EXPECT_NEAR(Length(r), px.loop_length, 1e-6);
    } else {
      // Along the boundary the exit Snell margin dips under event_slowdown_margin, stays positive,
      // and no accepted step is at minimum_step (it would be, were the slowdown unconditional).
      double min_margin = 1e300;
      double min_step = 1e300;
      for (int i = 0; i < r.PoseCount(); i++) {
        Mat pose{};
        std::copy(&r.poses[9 * i], &r.poses[9 * i] + 9, pose.begin());
        DomainEvaluation d;
        path.Map().Domain(pose.data(), &d);
        min_margin = std::fmin(min_margin, d.margins[3]);
      }
      // The last increment is the closing edge, which ends wherever the section is.
      for (size_t i = 0; i + 1 < r.arclength_increments.size(); i++) {
        min_step = std::fmin(min_step, r.arclength_increments[i]);
      }
      EXPECT_LT(min_margin, ContinuationParams{}.event_slowdown_margin);
      EXPECT_GT(min_margin, 0.0);
      EXPECT_GT(min_step, 2.0 * ContinuationParams{}.minimum_step);
    }
  }
}

// ---------------------------------------------------------------------------------------------
// Pins from LI's parity fixtures (li_rev bfbd042, `scripts/export_analytic_parity.py`): inputs
// and expectations of 3-5__random__trace_fiber.json and 3-5-6-7__random__trace_fiber.json, held to
// those fixtures' own tolerances (LI docs/analytic-parity-fixtures.md sections 4-5: status/reason
// exact, summed arclength to 2e-3 relative, residuals <= 1e-11). A regression guard until the
// fixtures themselves are replayed here; step counts are deliberately not pinned.
// ---------------------------------------------------------------------------------------------

struct LiTracePin {
  const char* name;
  std::vector<int> faces;
  std::array<double, 3> target;
  Mat seed;
  std::vector<std::pair<FiberReason, double>> traces;  // +1 then -1: reason and arclength
};

TEST(PathFiber, MatchesLiParityFixturePins) {
  const std::array<double, 3> incident = { -0.9659258262890683, -0.0, -0.25881904510252074 };
  const LiTracePin pins[] = {
    { "3-5__random",
      { 3, 5 },
      { -0.9112582539764014, 8.326672684688674e-17, 0.41183539741003355 },
      { 0.3258565506826661, -0.8542836236664049, -0.4049901217469277, 0.17797600617978254, -0.36528256190089714,
        0.9137248990781696, -0.9285160470349928, -0.36982176829726143, 0.03301227183938536 },
      { { FiberReason::kClosedLoop, 5.670160732846 } } },
    { "3-5-6-7__random",
      { 3, 5, 6, 7 },
      { 0.6275025327724766, -5.551115123125783e-17, 0.7786145203912691 },
      { 0.9739245884679638, -0.15831553091898734, 0.16250258043288573, -0.1957670169785059, -0.22445280180182878,
        0.9546183608262762, -0.1146567531410371, -0.9615389396343489, -0.24959306185470612 },
      { { FiberReason::kTirBoundary, 0.9217831785332176 }, { FiberReason::kTirBoundary, 1.5453840580147638 } } },
  };
  for (const auto& pin : pins) {
    SCOPED_TRACE(pin.name);
    const Path path(pin.faces, incident);
    double traced = 0.0;
    double expected = 0.0;
    std::vector<FiberReason> reasons;
    std::vector<FiberReason> expected_reasons;
    for (size_t k = 0; k < pin.traces.size(); k++) {
      ContinuationParams p;
      p.initial_tangent_sign = k == 0 ? 1 : -1;
      const TraceResult r = TraceFiber(path.Map(), MakeTargetChart(pin.target.data()), pin.seed.data(), p);
      EXPECT_LE(MaxResidual(r), 1e-11);
      traced += Length(r);
      expected += pin.traces[k].second;
      reasons.push_back(r.reason);
      expected_reasons.push_back(pin.traces[k].first);
    }
    // Orientation-free, as the fixtures compare: the pair of reasons as a multiset, the summed length.
    std::sort(reasons.begin(), reasons.end());
    std::sort(expected_reasons.begin(), expected_reasons.end());
    EXPECT_EQ(reasons, expected_reasons);
    EXPECT_NEAR(traced, expected, 2e-3 * expected);
  }
}

// doc/analytic-api.md section 5.3: the continuation's callees (fiber_continuation, so3, jet,
// path_chain, path_fiber) hold no static or thread_local state, so concurrent traces on distinct
// outputs are bit-identical to serial ones — each thread builds its own table, as the C wrapper does.
TEST(PathFiberConcurrency, ConcurrentTracesMatchSerialResults) {
  const std::array<double, 3> incident = kCanonicalIncident;
  auto trace = [&incident](int i) {
    const StripPixel& px = kStripPixels[i % 5];
    const Path path({ 3, 5 }, incident);
    const Mat seed = Exp(px.seed_coordinates[0], px.seed_coordinates[1], px.seed_coordinates[2]);
    ContinuationParams p;
    p.initial_tangent_sign = i % 2 == 0 ? 1 : -1;
    return TraceFiber(path.Map(), MakeTargetChart(px.target.data()), seed.data(), p);
  };
  constexpr int kThreads = 8;
  std::vector<TraceResult> serial;
  for (int i = 0; i < kThreads; i++) {
    serial.push_back(trace(i));
  }
  std::vector<TraceResult> parallel(kThreads);
  std::vector<std::thread> threads;
  for (int i = 0; i < kThreads; i++) {
    threads.emplace_back([&parallel, &trace, i] { parallel[i] = trace(i); });
  }
  for (auto& t : threads) {
    t.join();
  }
  for (int i = 0; i < kThreads; i++) {
    EXPECT_EQ(parallel[i].reason, serial[i].reason) << i;
    EXPECT_EQ(parallel[i].poses, serial[i].poses) << i;
    EXPECT_EQ(parallel[i].arclength_increments, serial[i].arclength_increments) << i;
  }
}

}  // namespace
}  // namespace lumice::analytic
