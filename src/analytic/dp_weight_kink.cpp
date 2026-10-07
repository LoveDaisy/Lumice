#include "analytic/dp_weight_kink.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

#include "analytic/discovery.hpp"

namespace lumice::analytic {
namespace {

constexpr double kPi = 3.14159265358979323846;

double Dot3(const double a[3], const double b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void Copy3(const double a[3], double out[3]) {
  out[0] = a[0];
  out[1] = a[1];
  out[2] = a[2];
}

void Unit(double v[3]) {
  const double norm = std::sqrt(Dot3(v, v));
  for (int i = 0; i < 3; i++) {
    v[i] /= norm;
  }
}

// D_P on points of C_k through the exit-Snell closure convention (LI _arc_values): finite plain
// values pass, non-finite ones on the closure read the limit, off the closure the arc fails (LI
// raises; a failure here is the seed walk's refusal in the marched loop, the curve's status in
// the closed form).
WalkStatus ArcValues(const BoundaryWalker& walker, const std::vector<double>& points, std::vector<double>* values,
                     std::string* message) {
  values->assign(points.size() / 3, 0.0);
  for (size_t k = 0; k < values->size(); k++) {
    if (walker.D(&points[3 * k], &(*values)[k]) != RoutedDeviationStatus::kOk) {
      std::ostringstream out;
      out.precision(6);
      out << "D_P is not finite at [" << points[3 * k] << ", " << points[3 * k + 1] << ", " << points[3 * k + 2] << "]";
      *message = out.str();
      return WalkStatus::kNotFinite;
    }
  }
  return WalkStatus::kOk;
}

// The both-ways walk of one seed (LI _walk_both_ways): forward with stop_at the seed (a loop
// inside U_P closes there), then backward from the seed; an open arc's two ends name their
// stopping gates.
namespace {

// The shared walk knobs of the kink search, as walk_zero_set takes them.
BoundaryWalkOptions WalkOptionsOf(const KinkOptions& options) {
  BoundaryWalkOptions out;
  out.step = options.step;
  out.max_walk_steps = options.max_walk_steps;
  return out;
}

}  // namespace

// The angle where the circle leaves U_P between an inside and an outside sample, and the gate
// that stops it (LI _bisect_end): bisection on the angle, the gate read just outside.
std::pair<double, int> BisectEnd(const BoundaryWalker& walker, const std::function<void(double, double[3])>& at,
                                 double inside_t, double outside_t) {
  double lo = inside_t;
  double hi = outside_t;
  for (int iteration = 0; iteration < 80; iteration++) {
    const double mid = 0.5 * (lo + hi);
    double u[3];
    at(mid, u);
    if (InsideUp(walker.field(), u)) {
      lo = mid;
    } else {
      hi = mid;
    }
    if (std::fabs(hi - lo) < kArcEndAtol) {
      break;
    }
  }
  double u[3];
  at(hi, u);
  double margins[kMaxFaceCount + 2];
  const int count = walker.field().ValidityMarginsAt(u, margins);
  int gate = 0;
  for (int k = 1; k < count; k++) {
    if (margins[k] < margins[gate]) {
      gate = k;
    }
  }
  // Map the validity-subset position back to its domain margin index (LI names[argmin]).
  int domain_index = 0;
  int seen = 0;
  for (int i = 0; i < walker.margin_count(); i++) {
    if (IsValidityMarginPosition(i, walker.margin_count())) {
      if (seen == gate) {
        domain_index = i;
        break;
      }
      seen++;
    }
  }
  return { lo, domain_index };
}

// The closed form: m . u = -sqrt(n^2 - 1) clipped to U_P; `ok` false if the discriminant does not
// vanish on the circle (fall back to the walk). For n^2 - 1 >= 1 the circle is empty by
// construction (LI _circle_curve).
KinkCurve CircleCurve(const BoundaryWalker& walker, int step, int margin, const double m_in[3], double index,
                      bool* ok) {
  *ok = true;
  KinkCurve curve;
  curve.step = step;
  curve.margin = margin;
  curve.index = index;
  curve.coverage = KinkCoverage::kClosedFormAuthority;
  double m[3];
  Copy3(m_in, m);
  Unit(m);
  curve.has_normal = true;
  Copy3(m, curve.normal);

  if (index * index - 1.0 >= 1.0) {
    std::ostringstream out;
    out.precision(17);
    out << "n^2 - 1 >= 1 at n = " << index
        << ": disc_k >= 0 on all of S^2, the reflection is total everywhere (no onset)";
    curve.note = out.str();
    return curve;
  }
  const double height = std::sqrt(index * index - 1.0);
  double basis[2][3];
  TangentBasis(m, basis);
  const double radius = std::sqrt(1.0 - height * height);
  const auto at = [&](double t, double u[3]) {
    for (int i = 0; i < 3; i++) {
      u[i] = -height * m[i] + radius * (std::cos(t) * basis[0][i] + std::sin(t) * basis[1][i]);
    }
  };

  double margins[2 * kMaxFaceCount];
  double worst = 0.0;
  for (int s = 0; s < kCircleSamples; s += kCircleSamples / 16) {
    double u[3];
    at(2.0 * kPi * static_cast<double>(s) / kCircleSamples, u);
    walker.Margins(u, margins);
    worst = std::max(worst, std::fabs(margins[margin]));
  }
  if (worst > kCircleResidualAtol) {
    *ok = false;  // the derivation does not hold on this crystal: march instead
    return curve;
  }

  std::vector<unsigned char> inside(kCircleSamples);
  int inside_count = 0;
  for (int s = 0; s < kCircleSamples; s++) {
    double u[3];
    at(2.0 * kPi * static_cast<double>(s) / kCircleSamples, u);
    inside[s] = InsideUp(walker.field(), u) ? 1 : 0;
    inside_count += inside[s];
  }
  const auto push_arc = [&](std::vector<double> points, bool closed, int gate_lo, int gate_hi) {
    KinkArc arc;
    arc.points = std::move(points);
    arc.closed = closed;
    arc.end_gates[0] = gate_lo;
    arc.end_gates[1] = gate_hi;
    std::string failure;
    if (ArcValues(walker, arc.points, &arc.values, &failure) != WalkStatus::kOk) {
      // Drop the failed arc: the curve keeps no partially-filled data, the same mechanical
      // invariant as the boundary walk (status != kOk, no half-arc). The note is appended to,
      // never overwritten — it may already carry the off-sphere explanation.
      curve.status = WalkStatus::kNotFinite;
      curve.note = curve.note.empty() ? failure : curve.note + "; " + failure;
      return;
    }
    curve.arcs.push_back(std::move(arc));
  };
  if (inside_count == kCircleSamples) {
    std::vector<double> points(3 * (kCircleSamples + 1));
    for (int s = 0; s <= kCircleSamples; s++) {
      at(s == kCircleSamples ? 2.0 * kPi : 2.0 * kPi * static_cast<double>(s) / kCircleSamples, &points[3 * s]);
    }
    push_arc(std::move(points), true, -1, -1);
    return curve;
  }
  if (inside_count == 0) {
    return curve;  // the onset misses U_P entirely
  }
  // Rotate the sample order so that it starts outside: every run of inside samples is then one
  // arc (LI's roll + split, as maximal runs of the rotated mask).
  int shift = 0;
  while (inside[shift]) {
    shift++;
  }
  const double step_t = 2.0 * kPi / kCircleSamples;
  int run_start = 0;
  while (run_start < kCircleSamples) {
    if (!inside[(shift + run_start) % kCircleSamples]) {
      run_start++;
      continue;
    }
    int run_end = run_start;  // one past the run's last offset
    while (run_end < kCircleSamples && inside[(shift + run_end) % kCircleSamples]) {
      run_end++;
    }
    const int first = (shift + run_start) % kCircleSamples;
    const int length = run_end - run_start;
    const double t0 = 2.0 * kPi * static_cast<double>(first) / kCircleSamples;
    const double t1 = t0 + step_t * static_cast<double>(length - 1);
    const auto [start, start_gate] = BisectEnd(walker, at, t0, t0 - step_t);
    const auto [stop, stop_gate] = BisectEnd(walker, at, t1, t1 + step_t);
    std::vector<double> points(3 * (length + 2));
    at(start, &points[0]);
    for (int j = 0; j < length; j++) {
      at(t0 + step_t * static_cast<double>(j), &points[3 * (j + 1)]);
    }
    at(stop, &points[3 * (length + 1)]);
    push_arc(std::move(points), false, start_gate, stop_gate);
    run_start = run_end;
  }
  return curve;
}

// The marched curve: lattice seeds near the zero set, each walked both ways, seeds within a few
// steps of a walked arc dropped (LI _marched_curve).
KinkCurve MarchedCurve(const BoundaryWalker& walker, int step, int margin, const KinkOptions& options,
                       KinkBothWaysFn both_ways, void* user) {
  KinkCurve curve;
  curve.step = step;
  curve.margin = margin;
  curve.index = walker.field().RefractiveIndex();
  curve.coverage = KinkCoverage::kMarchedUncertified;

  std::vector<double> lattice;  // the strictly-inside lattice points only
  double u[3];
  for (int i = 0; i < options.lattice_n; i++) {
    double f[3];
    LatticePoint(options.lattice_n, i, f);
    for (int c = 0; c < 3; c++) {
      u[c] = -f[c];
    }
    if (!InsideUp(walker.field(), u)) {
      continue;
    }
    lattice.push_back(u[0]);
    lattice.push_back(u[1]);
    lattice.push_back(u[2]);
  }
  if (lattice.empty()) {
    return curve;
  }
  // Seeds with |disc_k| in the band, ascending (LI seeds = lattice[near][argsort(|disc|)]).
  double margins[2 * kMaxFaceCount];
  std::vector<std::pair<double, int>> seeds;  // (|disc|, lattice point index)
  for (size_t k = 0; k < lattice.size() / 3; k++) {
    walker.Margins(&lattice[3 * k], margins);
    const double size = std::fabs(margins[margin]);
    if (size < kSeedBand) {
      seeds.emplace_back(size, static_cast<int>(k));
    }
  }
  std::sort(seeds.begin(), seeds.end());
  // The covered radius as a chord (LI: 2 sin(SEED_COVERED_STEPS * WALK_STEP_RAD / 2)); brute-force
  // nearest-walked-point, the arc point count's scale (a cKDTree's query semantics).
  const double covered = 2.0 * std::sin(kSeedCoveredSteps * options.step / 2.0);
  const double covered2 = covered * covered;
  std::vector<double> walked;
  std::string first_error;
  for (const auto& [size, seed_index] : seeds) {
    const double* seed = &lattice[3 * static_cast<size_t>(seed_index)];
    bool covered_already = false;
    for (size_t k = 0; k + 2 < walked.size() && !covered_already; k += 3) {
      const double dx = seed[0] - walked[k];
      const double dy = seed[1] - walked[k + 1];
      const double dz = seed[2] - walked[k + 2];
      if (dx * dx + dy * dy + dz * dz < covered2) {
        covered_already = true;
      }
    }
    if (covered_already) {
      continue;
    }
    double start[3];
    Copy3(seed, start);
    walker.Correct(start, margin);
    walker.Margins(start, margins);
    int bad[kMaxFaceCount + 2];
    const int excluded[1] = { margin };
    if (std::fabs(margins[margin]) > 1e-12 || walker.Violated(start, excluded, 1, bad) > 0) {
      continue;  // the corrector did not settle on the zero set inside U_P: not a seed
    }
    const KinkSeedArc walked_arc = both_ways != nullptr ? both_ways(walker, start, margin, options, user) :
                                                          KinkWalkBothWays(walker, start, margin, options, nullptr);
    if (walked_arc.status != WalkStatus::kOk) {
      // One seed's walk: counted and reported, the other seeds still walked (LI's except clause).
      curve.failed_seeds++;
      if (first_error.empty()) {
        first_error = walked_arc.message;
      }
      continue;
    }
    walked.insert(walked.end(), walked_arc.arc.points.begin(), walked_arc.arc.points.end());
    curve.arcs.push_back(walked_arc.arc);
  }
  if (curve.failed_seeds > 0) {
    curve.note = std::to_string(curve.failed_seeds) + " seed walk(s) failed, first: " + first_error;
  }
  return curve;
}

}  // namespace

KinkSeedArc KinkWalkBothWays(const BoundaryWalker& walker, const double start[3], int margin,
                             const KinkOptions& options, void* /*user*/) {
  KinkSeedArc out;
  const BoundaryWalkOptions walk_options = WalkOptionsOf(options);
  const ZeroSetWalk forward = WalkZeroSet(walker, start, margin, 1.0, start, walk_options);
  if (forward.status != WalkStatus::kOk) {
    out.status = forward.status;
    out.message = forward.message;
    return out;
  }
  if (!forward.met_corner) {
    out.arc.points = forward.points;
    out.arc.closed = true;
    out.status = ArcValues(walker, out.arc.points, &out.arc.values, &out.message);
    return out;
  }
  const ZeroSetWalk backward = WalkZeroSet(walker, start, margin, -1.0, nullptr, walk_options);
  if (backward.status != WalkStatus::kOk) {
    out.status = backward.status;
    out.message = backward.message;
    return out;
  }
  // backward reversed, then forward minus its first point (the shared seed).
  const size_t count = backward.points.size() / 3;
  out.arc.points.reserve(backward.points.size() + forward.points.size() - 3);
  for (size_t k = 0; k < count; k++) {
    for (int c = 0; c < 3; c++) {
      out.arc.points.push_back(backward.points[3 * (count - 1 - k) + c]);
    }
  }
  out.arc.points.insert(out.arc.points.end(), forward.points.begin() + 3, forward.points.end());
  // The gate closest to zero at each arc end is the one the walk stopped at (LI _stopping_gate).
  const auto stopping_gate = [&walker](const double corner[3]) {
    double margins[2 * kMaxFaceCount];
    walker.Margins(corner, margins);
    int best = walker.active()[0];
    double best_size = 0.0;
    for (size_t k = 0; k < walker.active().size(); k++) {
      const int margin_k = walker.active()[k];
      const double size = std::fabs(margins[margin_k]);
      if (k == 0 || size < best_size) {
        best = margin_k;
        best_size = size;
      }
    }
    return best;
  };
  out.arc.end_gates[0] = stopping_gate(backward.corner);
  out.arc.end_gates[1] = stopping_gate(forward.corner);
  out.status = ArcValues(walker, out.arc.points, &out.arc.values, &out.message);
  return out;
}

double KinkCurve::Spread() const {
  if (arcs.empty()) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  double lo = arcs[0].values[0];
  double hi = lo;
  for (const KinkArc& arc : arcs) {
    for (double value : arc.values) {
      lo = std::min(lo, value);
      hi = std::max(hi, value);
    }
  }
  return hi - lo;
}

std::vector<KinkCurve> WeightKinks(const DeviationField& field, const KinkOptions& options, KinkBothWaysFn both_ways,
                                   void* user) {
  const BoundaryWalker walker(field);
  const int slot_count = field.SlotCount();
  double incidence[kMaxFaceCount][3];
  IncidenceNormals(field.table(), field.slots(), slot_count, incidence);
  const double* n_a = incidence[0];

  std::vector<KinkCurve> out;
  for (int step = 1; step < slot_count - 1; step++) {
    const int margin = 2 * step + 1;  // internal_step_tir_discriminant
    const double* m = incidence[step];
    if (std::fabs(Dot3(m, n_a)) <= kGreatCircleAtol) {
      bool ok = false;
      KinkCurve circle = CircleCurve(walker, step, margin, m, field.RefractiveIndex(), &ok);
      if (ok) {
        out.push_back(std::move(circle));
        continue;
      }
    }
    out.push_back(MarchedCurve(walker, step, margin, options, both_ways, user));
  }
  return out;
}

KinkCurve MarchedKink(const DeviationField& field, int step, const KinkOptions& options, KinkBothWaysFn both_ways,
                      void* user) {
  const BoundaryWalker walker(field);
  return MarchedCurve(walker, step, 2 * step + 1, options, both_ways, user);
}

}  // namespace lumice::analytic
