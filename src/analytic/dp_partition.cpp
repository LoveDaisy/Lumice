#include "analytic/dp_partition.hpp"

#include <algorithm>
#include <cmath>
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

void Cross3(const double a[3], const double b[3], double out[3]) {
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}

// Union-find with path halving and union by size: the shared substrate of the k-NN graph and the
// chart masks.
class DisjointSet {
 public:
  explicit DisjointSet(int count) : parent_(count), size_(count, 1) {
    for (int i = 0; i < count; i++) {
      parent_[i] = i;
    }
  }

  int Find(int x) {
    while (parent_[x] != x) {
      parent_[x] = parent_[parent_[x]];
      x = parent_[x];
    }
    return x;
  }

  void Unite(int a, int b) {
    a = Find(a);
    b = Find(b);
    if (a == b) {
      return;
    }
    if (size_[a] < size_[b]) {
      std::swap(a, b);
    }
    parent_[b] = a;
    size_[a] += size_[b];
  }

 private:
  std::vector<int> parent_;
  std::vector<int> size_;
};

}  // namespace

// ---- the escape hatch --------------------------------------------------------------------------------

const char* EscapeRegimeName(EscapeRegime regime) {
  switch (regime) {
    case EscapeRegime::kNotDiskUnaudited:
      return "not_disk_unaudited";
    case EscapeRegime::kNotDiskUnconverged:
      return "not_disk_unconverged";
    case EscapeRegime::kNotDiskConfirmed:
      return "not_disk_confirmed";
    case EscapeRegime::kNotDiskCorrected:
      return "not_disk_corrected";
    case EscapeRegime::kSlabCreaseContradiction:
      return "slab_crease_contradiction";
    case EscapeRegime::kSlabCreaseNotCarried:
      return "slab_crease_not_carried";
    case EscapeRegime::kSlabCreaseTouching:
      return "slab_crease_touching";
    case EscapeRegime::kSlabCreaseClosedRidge:
      return "slab_crease_closed_ridge";
    case EscapeRegime::kMultipleInteriorCriticalPoints:
      return "multiple_interior_critical_points";
    case EscapeRegime::kLoopExtremaNotAlternating:
      return "loop_extrema_not_alternating";
    case EscapeRegime::kOddBoundaryCrossings:
      return "odd_boundary_crossings";
    case EscapeRegime::kInteriorCriticalPointNotSimple:
      return "interior_critical_point_not_simple";
    case EscapeRegime::kSublevelNotReachingBoundary:
      return "sublevel_not_reaching_boundary";
  }
  return "unknown_escape_regime";
}

const char* AuditVerdictName(AuditVerdict verdict) {
  switch (verdict) {
    case AuditVerdict::kConfirmed:
      return "confirmed";
    case AuditVerdict::kCorrected:
      return "corrected";
    case AuditVerdict::kUnconverged:
      return "unconverged";
  }
  return "unknown_audit_verdict";
}

// ---- the lattice component counts ---------------------------------------------------------------------

namespace {

// k-NN (k = 8) connected components of a point set, edges longer than 3x the median dropped (LI
// _component_count). Brute force: n^2 distance evaluations with a bounded 8-best insertion scan;
// n = 20000 (4e8 evaluations, ~1 s measured) is the only call site's scale, a bucket grid is the
// recorded fallback if a heavier caller appears.
int ComponentCount(const std::vector<double>& points) {
  const int n = static_cast<int>(points.size() / 3);
  if (n < 9) {
    return n > 0 ? 1 : 0;
  }
  const double* p = points.data();
  // The 8 nearest mates of every point and their squared chord lengths (the filter is on lengths
  // against 3x their median, and the median of squares is the square of the median — a monotone
  // map — so the filter compares squares against 9x median(square)).
  std::vector<double> mate2(8 * static_cast<size_t>(n));
  std::vector<int> mate(8 * static_cast<size_t>(n));
  double best2[8];
  int best_j[8];
  for (int i = 0; i < n; i++) {
    const double xi = p[3 * i];
    const double yi = p[3 * i + 1];
    const double zi = p[3 * i + 2];
    int filled = 0;
    for (int j = 0; j < n; j++) {
      if (j == i) {
        continue;
      }
      const double dx = p[3 * j] - xi;
      const double dy = p[3 * j + 1] - yi;
      const double dz = p[3 * j + 2] - zi;
      const double d2 = dx * dx + dy * dy + dz * dz;
      if (filled < 8) {
        best2[filled] = d2;
        best_j[filled] = j;
        filled++;
        if (filled == 8) {
          // A single insertion sort of the first eight; after that only bounded insertions.
          for (int a = 1; a < 8; a++) {
            const double v2 = best2[a];
            const int vj = best_j[a];
            int b = a - 1;
            while (b >= 0 && best2[b] > v2) {
              best2[b + 1] = best2[b];
              best_j[b + 1] = best_j[b];
              b--;
            }
            best2[b + 1] = v2;
            best_j[b + 1] = vj;
          }
        }
        continue;
      }
      if (d2 < best2[7]) {
        int b = 7;
        while (b > 0 && best2[b - 1] > d2) {
          best2[b] = best2[b - 1];
          best_j[b] = best_j[b - 1];
          b--;
        }
        best2[b] = d2;
        best_j[b] = j;
      }
    }
    for (int k = 0; k < 8; k++) {
      mate2[8 * static_cast<size_t>(i) + k] = best2[k];
      mate[8 * static_cast<size_t>(i) + k] = best_j[k];
    }
  }
  const double median2 = [n, &mate2] {
    std::vector<double> copy(mate2);
    const auto mid = copy.begin() + static_cast<std::ptrdiff_t>(copy.size() / 2);
    std::nth_element(copy.begin(), mid, copy.end());
    return *mid;
  }();
  DisjointSet set(n);
  for (int i = 0; i < n; i++) {
    for (int k = 0; k < 8; k++) {
      if (mate2[8 * static_cast<size_t>(i) + k] <= 9.0 * median2) {
        set.Unite(i, mate[8 * static_cast<size_t>(i) + k]);
      }
    }
  }
  int roots = 0;
  for (int i = 0; i < n; i++) {
    if (set.Find(i) == i) {
      roots++;
    }
  }
  return roots;
}

}  // namespace

// ---- the chart-grid audit -----------------------------------------------------------------------------

AuditVerdict AuditVerdictRollup(const int* domain_counts, const int* complement_counts, int count, int lattice_domain,
                                int lattice_complement) {
  const auto series_status = [](const int* counts, int n, int lattice) {
    for (int i = 0; i < n; i++) {
      if (counts[i] == -1) {
        return AuditVerdict::kUnconverged;
      }
    }
    for (int i = 1; i < n; i++) {
      if (counts[i] != counts[0]) {
        return AuditVerdict::kUnconverged;
      }
    }
    return counts[0] == lattice ? AuditVerdict::kConfirmed : AuditVerdict::kCorrected;
  };
  const AuditVerdict domain_status = series_status(domain_counts, count, lattice_domain);
  const AuditVerdict complement_status = series_status(complement_counts, count, lattice_complement);
  const auto rank = [](AuditVerdict v) {
    return v == AuditVerdict::kUnconverged ? 2 : (v == AuditVerdict::kCorrected ? 1 : 0);
  };
  return rank(domain_status) >= rank(complement_status) ? domain_status : complement_status;
}

bool ChartComponentCounts(const unsigned char* valid, const unsigned char* off_chart, int grid, int* domain,
                          int* complement) {
  const int total = grid * grid;
  for (int i = 0; i < total; i++) {
    if (off_chart[i] && valid[i]) {
      return false;  // a valid node on the pushed rim breaches the chart premise: no counts
    }
  }
  const auto neighbour_unite = [&](DisjointSet& set, unsigned char value) {
    for (int i = 0; i < grid; i++) {
      for (int j = 0; j < grid; j++) {
        const int node = i * grid + j;
        if (valid[node] != value) {
          continue;
        }
        if (j + 1 < grid && valid[node + 1] == value) {
          set.Unite(node, node + 1);
        }
        if (i + 1 < grid && valid[node + grid] == value) {
          set.Unite(node, node + grid);
        }
      }
    }
  };
  DisjointSet domain_set(total);
  neighbour_unite(domain_set, 1);
  *domain = 0;
  for (int i = 0; i < total; i++) {
    if (valid[i] && domain_set.Find(i) == i) {
      (*domain)++;
    }
  }
  DisjointSet complement_set(total);
  neighbour_unite(complement_set, 0);
  std::vector<unsigned char> rim_root(total, 0);
  for (int i = 0; i < total; i++) {
    if (off_chart[i]) {
      rim_root[complement_set.Find(i)] = 1;  // an invalid rim node's component reaches the rim
    }
  }
  *complement = 1;  // the far hemisphere: every component reaching the pushed rim, merged
  for (int i = 0; i < total; i++) {
    if (!valid[i] && complement_set.Find(i) == i && !rim_root[i]) {
      (*complement)++;  // an island of the complement inside the chart
    }
  }
  return true;
}

namespace {

// The grid x grid orthographic chart of the entry hemisphere (LI _chart_grid, itself the
// deliberate re-implementation of scripts/verify_dp_field_intervals.py's chart): u = x e1 +
// y e2 + sqrt(1 - x^2 - y^2) n_a, off-chart nodes (r >= 1) pushed radially onto the rim
// u . n_a = 0, everything renormalized.
void ChartGrid(int grid, const double n_a[3], std::vector<double>& u, std::vector<unsigned char>& off_chart) {
  double axis[3] = { 0.0, 0.0, 0.0 };
  int k = 0;
  for (int i = 1; i < 3; i++) {
    if (std::fabs(n_a[i]) < std::fabs(n_a[k])) {
      k = i;
    }
  }
  axis[k] = 1.0;
  double e1[3];
  Cross3(n_a, axis, e1);
  const double e1_norm = std::sqrt(Dot3(e1, e1));
  for (int i = 0; i < 3; i++) {
    e1[i] /= e1_norm;
  }
  double e2[3];
  Cross3(n_a, e1, e2);

  u.assign(static_cast<size_t>(grid) * grid * 3, 0.0);
  off_chart.assign(static_cast<size_t>(grid) * grid, 0);
  const double step = 2.0 / (grid - 1);
  for (int i = 0; i < grid; i++) {
    const double x = i == grid - 1 ? 1.0 : -1.0 + i * step;
    for (int j = 0; j < grid; j++) {
      const double y = j == grid - 1 ? 1.0 : -1.0 + j * step;
      const double r2 = x * x + y * y;
      const double r = std::sqrt(r2);
      const bool off = r >= 1.0;
      const double scale = off ? 1.0 / std::max(r, 1e-300) : 1.0;
      const double height = std::sqrt(std::max(1.0 - r2, 0.0));
      double v[3];
      for (int c = 0; c < 3; c++) {
        v[c] = x * scale * e1[c] + y * scale * e2[c] + height * n_a[c];
      }
      const double norm = std::sqrt(Dot3(v, v));
      const size_t node = static_cast<size_t>(i) * grid + j;
      for (int c = 0; c < 3; c++) {
        u[3 * node + c] = v[c] / norm;
      }
      off_chart[node] = off ? 1 : 0;
    }
  }
}

// The U_P mask on the chart nodes, through the one gate authority.
void ChartValid(const DeviationField& field, const std::vector<double>& u, std::vector<unsigned char>& valid) {
  valid.assign(u.size() / 3, 0);
  double margins[kMaxFaceCount + 2];
  for (size_t node = 0; node < valid.size(); node++) {
    const int count = field.ValidityMarginsAt(&u[3 * node], margins);
    bool inside = true;
    for (int m = 0; m < count; m++) {
      if (!(margins[m] > 0.0)) {
        inside = false;
        break;
      }
    }
    valid[node] = inside ? 1 : 0;
  }
}

}  // namespace

// ---- the topology -------------------------------------------------------------------------------------

DomainTopology DomainTopologyOf(const DeviationField& field, int lattice_n, const int* ladder, int ladder_count) {
  // The lattice points are LI fibonacci_sphere's f_i = -LatticePoint (the store's antipodal
  // convention, discovery.hpp).
  std::vector<double> lattice(static_cast<size_t>(lattice_n) * 3);
  for (int i = 0; i < lattice_n; i++) {
    double f[3];
    LatticePoint(lattice_n, i, f);
    for (int c = 0; c < 3; c++) {
      lattice[3 * static_cast<size_t>(i) + c] = -f[c];
    }
  }
  std::vector<double> domain_points;
  std::vector<double> complement_points;
  double margins[kMaxFaceCount + 2];
  for (int i = 0; i < lattice_n; i++) {
    const int count = field.ValidityMarginsAt(&lattice[3 * static_cast<size_t>(i)], margins);
    bool inside = true;
    for (int m = 0; m < count; m++) {
      if (!(margins[m] > 0.0)) {
        inside = false;
        break;
      }
    }
    std::vector<double>& target = inside ? domain_points : complement_points;
    for (int c = 0; c < 3; c++) {
      target.push_back(lattice[3 * static_cast<size_t>(i) + c]);
    }
  }
  int domain = ComponentCount(domain_points);
  int complement = ComponentCount(complement_points);

  DomainTopology topology;
  topology.lattice_n = lattice_n;
  topology.domain_components = domain;
  topology.complement_components = complement;
  const bool audit_armed = ladder != nullptr ? ladder_count > 0 : kDefaultAuditLadderCount > 0;
  if (!audit_armed || (domain <= 1 && complement <= 1)) {
    return topology;  // the trigger gate: the audit's cost is paid only on a plural count
  }
  const int* grids = ladder != nullptr ? ladder : kDefaultAuditLadder;
  const int grid_count = ladder != nullptr ? ladder_count : kDefaultAuditLadderCount;

  ChartAudit audit;
  audit.lattice_domain_count = domain;
  audit.lattice_complement_count = complement;
  audit.grids.assign(grids, grids + grid_count);
  audit.domain_counts.assign(grid_count, 0);
  audit.complement_counts.assign(grid_count, 0);
  for (int g = 0; g < grid_count; g++) {
    std::vector<double> u;
    std::vector<unsigned char> off_chart;
    ChartGrid(grids[g], field.EntryNormal(), u, off_chart);
    std::vector<unsigned char> valid;
    ChartValid(field, u, valid);
    int chart_domain = 0;
    int chart_complement = 0;
    if (!ChartComponentCounts(valid.data(), off_chart.data(), grids[g], &chart_domain, &chart_complement)) {
      audit.domain_counts[g] = -1;
      audit.complement_counts[g] = -1;
    } else {
      audit.domain_counts[g] = chart_domain;
      audit.complement_counts[g] = chart_complement;
    }
  }
  audit.verdict =
      AuditVerdictRollup(audit.domain_counts.data(), audit.complement_counts.data(), grid_count, domain, complement);
  if (audit.verdict != AuditVerdict::kUnconverged) {
    // The adjudicated counts replace the lattice's (the audit record keeps both).
    topology.domain_components = audit.domain_counts[0];
    topology.complement_components = audit.complement_counts[0];
  }
  topology.has_grid_audit = true;
  topology.grid_audit = std::move(audit);
  return topology;
}

// ---- the degenerate fold set -------------------------------------------------------------------------

std::vector<std::pair<int, int>> CircularRuns(const unsigned char* mask, int n) {
  std::vector<std::pair<int, int>> runs;
  int any = 0;
  int first_false = -1;
  for (int i = 0; i < n; i++) {
    if (mask[i]) {
      any++;
    } else if (first_false < 0) {
      first_false = i;
    }
  }
  if (any == 0) {
    return runs;
  }
  if (any == n) {
    runs.emplace_back(0, n);
    return runs;
  }
  // Rotate so index 0 of the rolled mask is False: no run crosses it, and the starts/ends pairing
  // is linear (LI _circular_runs).
  const int rotate = first_false;
  const auto rolled = [mask, n, rotate](int k) { return mask[(k + rotate) % n] != 0; };
  int run_start = -1;
  for (int k = 0; k < n; k++) {
    const bool here = rolled(k);
    const bool next = rolled((k + 1) % n);
    if (!here && next) {
      run_start = k + 1;
    } else if (here && !next) {
      // k is the run's last index; started at run_start (>= 1, never across index 0).
      runs.emplace_back((run_start + rotate) % n, k - run_start + 1);
    }
  }
  std::sort(runs.begin(), runs.end());
  return runs;
}

DegenerateFoldSet BuildDegenerateFoldSet(const DeviationField& field, int circle_samples) {
  DegenerateFoldSet fold_set;
  const FoldScreen& screen = field.fold();
  if (!screen.has_axis) {
    return fold_set;  // M = I: no critical set to locate
  }
  for (int i = 0; i < 3; i++) {
    fold_set.axis[i] = screen.axis[i];
  }
  fold_set.has_axis = true;
  fold_set.axis_point_count = 2;
  for (int sign = 0; sign < 2; sign++) {
    DegenerateFoldSet::AxisPoint& point = fold_set.axis_points[sign];
    const double s = sign == 0 ? 1.0 : -1.0;
    for (int i = 0; i < 3; i++) {
      point.position[i] = s * screen.axis[i];
    }
    double margins[kMaxFaceCount + 2];
    const int count = field.ValidityMarginsAt(point.position, margins);
    point.location = LocateByValidityMargins(margins, count);
  }

  double basis[2][3];
  TangentBasis(screen.axis, basis);
  std::vector<double> binding(circle_samples);
  std::vector<unsigned char> inside(circle_samples);
  double margins[kMaxFaceCount + 2];
  for (int k = 0; k < circle_samples; k++) {
    const double t = 2.0 * kPi * static_cast<double>(k) / static_cast<double>(circle_samples);
    double u[3];
    for (int i = 0; i < 3; i++) {
      u[i] = std::cos(t) * basis[0][i] + std::sin(t) * basis[1][i];
    }
    const int count = field.ValidityMarginsAt(u, margins);
    double smallest = margins[0];
    for (int m = 1; m < count; m++) {
      smallest = std::min(smallest, margins[m]);
    }
    binding[k] = smallest;
    inside[k] = smallest > kBoundaryMarginAtol ? 1 : 0;
  }
  const std::vector<std::pair<int, int>> arcs = CircularRuns(inside.data(), circle_samples);
  int inside_count = 0;
  for (int k = 0; k < circle_samples; k++) {
    inside_count += inside[k];
  }
  fold_set.circle_interior_fraction = static_cast<double>(inside_count) / static_cast<double>(circle_samples);
  fold_set.crease_interior_arcs = static_cast<int>(arcs.size());
  for (const auto& [start, length] : arcs) {
    double contact = binding[start];
    for (int k = 0; k < length; k++) {
      contact = std::min(contact, binding[(start + k) % circle_samples]);
    }
    if (contact > kCreaseContactMargin) {
      fold_set.crease_closed_ridge = true;
    }
  }
  std::vector<unsigned char> touching(circle_samples);
  for (int k = 0; k < circle_samples; k++) {
    touching[k] = std::fabs(binding[k]) <= kCreaseContactMargin ? 1 : 0;
  }
  const double spacing = 2.0 * kPi / static_cast<double>(circle_samples);
  for (const auto& [start, length] : CircularRuns(touching.data(), circle_samples)) {
    if (static_cast<double>(length) * spacing >= kCreaseTouchingArcRad) {
      fold_set.crease_touching_arc = true;
    }
  }
  return fold_set;
}

std::vector<InteriorCriticalPoint> SlabInteriorCriticalPoints(const DeviationField& field,
                                                              const DegenerateFoldSet& fold_set) {
  std::vector<InteriorCriticalPoint> points;
  for (int k = 0; k < fold_set.axis_point_count; k++) {
    const auto& axis_point = fold_set.axis_points[k];
    if (axis_point.location != DomainLocation::kInterior) {
      continue;
    }
    InteriorCriticalPoint point;
    for (int i = 0; i < 3; i++) {
      point.position[i] = axis_point.position[i];
    }
    point.value = field.Sample(axis_point.position).d_value;  // the slab form: exact at the axis
    point.kind = CriticalKind::kDegenerate;
    points.push_back(point);
  }
  return points;
}

// ---- the slab-crease three-check gate -----------------------------------------------------------------

namespace {

// LI's {:.3%} of the interior fraction ("22.764%").
std::string Percent3(double fraction) {
  std::ostringstream out;
  out.setf(std::ios::fixed);
  out.precision(3);
  out << fraction * 100.0 << '%';
  return out.str();
}

}  // namespace

bool SlabCreaseGates(const DeviationField& field, const DegenerateFoldSet& fold_set, const BoundaryLoopData& loop,
                     EscapeRegime* regime, std::string* message) {
  const auto fail = [regime, message](EscapeRegime which, std::string text) {
    if (regime != nullptr) {
      *regime = which;
    }
    if (message != nullptr) {
      *message = std::move(text);
    }
    return false;
  };
  const std::string fraction = Percent3(fold_set.circle_interior_fraction);
  if (fold_set.crease_interior_arcs == 0) {
    return fail(EscapeRegime::kSlabCreaseContradiction,
                "the slab crease evidence contradicts the premise: circle_interior_fraction = " + fraction +
                    " but the crease sampling holds no interior arc of the crease u . n_M = 0 inside U_P");
  }
  // The blade: D_P at any crease point — d_slab of a tangent basis vector of the fold axis,
  // constant along the crease of a rotation or mirror slab.
  double basis[2][3];
  TangentBasis(fold_set.axis, basis);
  const double blade = field.DSlab(basis[0]);
  bool carried = false;
  for (const BoundaryCriticalPoint& point : loop.critical_points) {
    if (point.kind == CriticalKind::kMaximum && point.strict && std::fabs(point.value - blade) <= kExtremumAtol) {
      carried = true;
      break;
    }
  }
  if (!carried) {
    return fail(EscapeRegime::kSlabCreaseNotCarried,
                "the slab crease u . n_M = 0 runs through U_P (" + fraction +
                    " of its sampling) but its blade value is not carried by the boundary walk as a strict "
                    "local maximum");
  }
  if (fold_set.crease_touching_arc) {
    return fail(EscapeRegime::kSlabCreaseTouching,
                "the slab crease u . n_M = 0 touches or runs along dU_P over an arc of its sampling "
                "(a non-transversal contact: its crossings cannot be counted as extrema)");
  }
  if (fold_set.crease_closed_ridge) {
    return fail(EscapeRegime::kSlabCreaseClosedRidge,
                "an interior arc of the slab crease u . n_M = 0 never reaches dU_P (a closed ridge: "
                "the level loops around it are not the boundary walk's to count)");
  }
  return true;
}

// ---- the partition -------------------------------------------------------------------------------------

namespace {

// LI's kind vocabulary in messages ('minimum' / 'maximum' / ...).
const char* CriticalKindName(CriticalKind kind) {
  switch (kind) {
    case CriticalKind::kMinimum:
      return "minimum";
    case CriticalKind::kMaximum:
      return "maximum";
    case CriticalKind::kSaddle:
      return "saddle";
    case CriticalKind::kDegenerate:
      return "degenerate";
  }
  return "unknown";
}

// A short, faithful double rendering for message texts (LI's f-strings print reprs; the stable
// part of every message is its prefix, the numbers are diagnostics).
std::string Num(double value) {
  std::ostringstream out;
  out.precision(17);
  out << value;
  return out.str();
}

std::string IntList(const std::vector<int>& values) {
  std::ostringstream out;
  out << '(';
  for (size_t i = 0; i < values.size(); i++) {
    if (i > 0) {
      out << ", ";
    }
    out << values[i];
  }
  out << ')';
  return out.str();
}

// D_P on the part of a kSideRingRad ring around `point` inside U_P (LI _side_values): the ring in
// the tangent basis of the point, the strictly-inside members by the gates, their d_value (the
// slab form for a degenerate fold).
std::vector<double> SideValues(const DeviationField& field, const double point[3]) {
  double basis[2][3];
  TangentBasis(point, basis);
  std::vector<double> values;
  double margins[kMaxFaceCount + 2];
  for (int k = 0; k < kSideRingDirections; k++) {
    const double t = 2.0 * kPi * static_cast<double>(k) / static_cast<double>(kSideRingDirections);
    double u[3];
    for (int i = 0; i < 3; i++) {
      u[i] = std::cos(kSideRingRad) * point[i] +
             std::sin(kSideRingRad) * (std::cos(t) * basis[0][i] + std::sin(t) * basis[1][i]);
    }
    const int count = field.ValidityMarginsAt(u, margins);
    bool inside = true;
    for (int m = 0; m < count; m++) {
      if (!(margins[m] > 0.0)) {
        inside = false;
        break;
      }
    }
    if (inside) {
      values.push_back(field.Sample(u).d_value);
    }
  }
  return values;
}

PartitionResult Escape(EscapeRegime regime, std::string message) {
  PartitionResult result;
  result.escaped = true;
  result.regime = regime;
  result.message = std::move(message);
  return result;  // intervals stay empty — the mechanical invariant of PartitionResult
}

}  // namespace

PartitionResult IntervalPartition(const DeviationField& field, const std::vector<InteriorCriticalPoint>& interior,
                                  const DegenerateFoldSet* fold_set, const BoundaryLoopData& loop,
                                  const DomainTopology& topology) {
  if (!topology.IsDisk()) {
    if (!topology.has_grid_audit) {
      // No audit ran: a hand-built topology, or an empty audit ladder — the pre-52.7 escape.
      return Escape(EscapeRegime::kNotDiskUnaudited,
                    "U_P is not a disk on the " + std::to_string(topology.lattice_n) +
                        "-point lattice: " + std::to_string(topology.domain_components) + " component(s), complement " +
                        std::to_string(topology.complement_components));
    }
    const ChartAudit& audit = topology.grid_audit;
    if (audit.verdict == AuditVerdict::kUnconverged) {
      return Escape(EscapeRegime::kNotDiskUnconverged,
                    "U_P is not a disk: lattice " + std::to_string(topology.lattice_n) + " says " +
                        std::to_string(audit.lattice_domain_count) + "/" +
                        std::to_string(audit.lattice_complement_count) + ", chart grids " + IntList(audit.grids) +
                        " say " + IntList(audit.domain_counts) + "/" + IntList(audit.complement_counts) +
                        "; counts are not resolution-converged, the topology is not established");
    }
    if (audit.verdict == AuditVerdict::kConfirmed) {
      return Escape(EscapeRegime::kNotDiskConfirmed,
                    "U_P is not a disk: " + std::to_string(topology.domain_components) + " component(s), complement " +
                        std::to_string(topology.complement_components) + " (lattice " +
                        std::to_string(topology.lattice_n) + " and chart grids " + IntList(audit.grids) + " agree)");
    }
    // Corrected: the grids agree on a count the lattice got wrong, and the adjudicated counts
    // still fail the disk test.
    return Escape(EscapeRegime::kNotDiskCorrected,
                  "U_P is not a disk: " + std::to_string(topology.domain_components) + " component(s), complement " +
                      std::to_string(topology.complement_components) + " (chart grids " + IntList(audit.grids) +
                      " agree, correcting the lattice " + std::to_string(topology.lattice_n) + " counts " +
                      std::to_string(audit.lattice_domain_count) + "/" +
                      std::to_string(audit.lattice_complement_count) + ")");
  }

  if (fold_set != nullptr && fold_set->circle_interior_fraction > 0.0) {
    EscapeRegime regime = EscapeRegime::kSlabCreaseContradiction;
    std::string message;
    if (!SlabCreaseGates(field, *fold_set, loop, &regime, &message)) {
      return Escape(regime, std::move(message));
    }
  }

  if (interior.size() > 1) {
    std::string kinds;
    for (size_t i = 0; i < interior.size(); i++) {
      if (i > 0) {
        kinds += ", ";
      }
      kinds += CriticalKindName(interior[i].kind);
    }
    return Escape(EscapeRegime::kMultipleInteriorCriticalPoints,
                  std::to_string(interior.size()) + " interior critical points: " + kinds);
  }

  const std::vector<BoundaryCriticalPoint>& extrema = loop.critical_points;
  for (size_t i = 0; i < extrema.size(); i++) {
    if (extrema[i].kind == extrema[(i + 1) % extrema.size()].kind) {
      std::string kinds = "[";
      for (size_t k = 0; k < extrema.size(); k++) {
        if (k > 0) {
          kinds += ", ";
        }
        kinds += CriticalKindName(extrema[k].kind);
      }
      kinds += "]";
      return Escape(EscapeRegime::kLoopExtremaNotAlternating, "loop extrema do not alternate: " + kinds);
    }
  }
  std::vector<double> values;
  values.reserve(extrema.size());
  for (const BoundaryCriticalPoint& point : extrema) {
    values.push_back(point.value);
  }
  if (values.empty() && !loop.has_plateau) {
    // An inconsistent record: a non-constant loop with no extremum cannot exist (its restriction
    // to the loop attains one), and an interior point against such a record is refused rather
    // than read out of bounds — LI's own path here is an unhandled ValueError on the empty min.
    return Escape(EscapeRegime::kLoopExtremaNotAlternating, "loop extrema do not alternate: [] (an empty record)");
  }

  // The interior extremum: its kind (the ring probe decides a degenerate point's side), the edge
  // it must reach first, and the closed-loop range between them.
  double closed_range[2] = { 0.0, 0.0 };
  bool has_closed_range = false;
  if (!interior.empty()) {
    const InteriorCriticalPoint& point = interior[0];
    CriticalKind kind = point.kind;
    if (kind != CriticalKind::kMinimum && kind != CriticalKind::kMaximum) {
      if (kind == CriticalKind::kDegenerate && field.fold().degenerate) {
        const std::vector<double> ring = SideValues(field, point.position);
        bool all_above = ring.size() == static_cast<size_t>(kSideRingDirections);
        bool all_below = all_above;
        for (double side : ring) {
          all_above = all_above && side > point.value;
          all_below = all_below && side < point.value;
        }
        if (all_above) {
          kind = CriticalKind::kMinimum;
        } else if (all_below) {
          kind = CriticalKind::kMaximum;
        }
      }
      if (kind != CriticalKind::kMinimum && kind != CriticalKind::kMaximum) {
        std::ostringstream position;
        position.precision(17);
        position << "[" << point.position[0] << ", " << point.position[1] << ", " << point.position[2] << "]";
        return Escape(EscapeRegime::kInteriorCriticalPointNotSimple,
                      "interior critical point of kind '" + std::string(CriticalKindName(point.kind)) + "' at " +
                          position.str() + ", D = " + Num(point.value));
      }
    }
    const double v = point.value;
    double edge = 0.0;
    std::vector<const double*> touching_positions;
    if (loop.has_plateau) {
      // A constant loop: every point of dU_P is both its minimum and its maximum, at the plateau.
      edge = loop.plateau_value;
      touching_positions.push_back(loop.first_point);
    } else {
      edge = values[0];
      for (double value : values) {
        edge = kind == CriticalKind::kMinimum ? std::min(edge, value) : std::max(edge, value);
      }
      for (const BoundaryCriticalPoint& extremum : extrema) {
        if (extremum.kind == kind && std::fabs(extremum.value - edge) <= kExtremumAtol) {
          touching_positions.push_back(extremum.position);
        }
      }
    }
    const double sign = kind == CriticalKind::kMinimum ? 1.0 : -1.0;
    bool reaches = false;
    for (const double* position : touching_positions) {
      for (double side : SideValues(field, position)) {
        if (sign * (side - edge) < -1e-12) {
          reaches = true;
          break;
        }
      }
      if (reaches) {
        break;
      }
    }
    if (!reaches || sign * (edge - v) <= 0.0) {
      const char* kind_name = CriticalKindName(kind);
      return Escape(EscapeRegime::kSublevelNotReachingBoundary,
                    std::string("interior ") + kind_name + " D = " + Num(v) + " and loop " + kind_name + " " +
                        Num(edge) +
                        ": the sublevel component of the interior extremum is not shown to reach dU_P first at "
                        "the loop extremum");
    }
    closed_range[0] = std::min(v, edge);
    closed_range[1] = std::max(v, edge);
    has_closed_range = true;
  }

  // The critical values: interior, loop extrema, corners and the plateau constant, merged within
  // kExtremumAtol (LI critical_values).
  std::vector<double> raw;
  raw.reserve(interior.size() + extrema.size() + loop.corners.size() + 1);
  for (const InteriorCriticalPoint& point : interior) {
    raw.push_back(point.value);
  }
  for (const BoundaryCriticalPoint& point : extrema) {
    raw.push_back(point.value);
  }
  for (const LoopCorner& corner : loop.corners) {
    raw.push_back(corner.value);
  }
  if (loop.has_plateau) {
    raw.push_back(loop.plateau_value);
  }
  std::sort(raw.begin(), raw.end());
  std::vector<double> breaks;
  for (double value : raw) {
    if (breaks.empty() || value - breaks.back() > kExtremumAtol) {
      breaks.push_back(value);
    }
  }

  PartitionResult result;
  for (size_t i = 0; i + 1 < breaks.size(); i++) {
    const double delta = 0.5 * (breaks[i] + breaks[i + 1]);
    int crossings = 0;
    for (size_t k = 0; k < values.size(); k++) {
      const double lo = std::min(values[k], values[(k + 1) % values.size()]);
      const double hi = std::max(values[k], values[(k + 1) % values.size()]);
      if (lo < delta && delta < hi) {
        crossings++;
      }
    }
    if (crossings % 2 != 0) {
      return Escape(EscapeRegime::kOddBoundaryCrossings,
                    "odd number of boundary crossings (" + std::to_string(crossings) + ") at delta = " + Num(delta));
    }
    DeviationInterval interval;
    interval.lower = breaks[i];
    interval.upper = breaks[i + 1];
    interval.n_closed = has_closed_range && closed_range[0] < delta && delta < closed_range[1] ? 1 : 0;
    interval.n_open = crossings / 2;
    interval.n_components = interval.n_closed + interval.n_open;
    result.intervals.push_back(interval);
  }
  return result;
}

}  // namespace lumice::analytic
