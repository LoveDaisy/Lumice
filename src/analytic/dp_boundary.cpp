#include "analytic/dp_boundary.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <string>

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

void Copy3(const double a[3], double out[3]) {
  out[0] = a[0];
  out[1] = a[1];
  out[2] = a[2];
}

// angle(a, b) in atan2 form (LI boundary._angle).
double AngleBetween(const double a[3], const double b[3]) {
  double cross[3];
  Cross3(a, b, cross);
  return std::atan2(std::sqrt(Dot3(cross, cross)), Dot3(a, b));
}

void Unit(double v[3]) {
  const double norm = std::sqrt(Dot3(v, v));
  for (int i = 0; i < 3; i++) {
    v[i] /= norm;
  }
}

bool Contains(const std::vector<int>& values, int value) {
  return std::find(values.begin(), values.end(), value) != values.end();
}

// The walk's path id for message texts (LI path_id_of: "3-5-6-7").
std::string PathId(const BoundaryWalker& walker) {
  return PathIdOf(walker.field().table(), walker.field().slots(), walker.field().SlotCount());
}

std::string PointList(const double u[3]) {
  std::ostringstream out;
  out.precision(6);
  out << "[" << u[0] << ", " << u[1] << ", " << u[2] << "]";
  return out.str();
}

}  // namespace

// ---- margins by index ----------------------------------------------------------------------------------

std::string MarginName(int slot_count, int margin) {
  const int exit = 2 * (slot_count - 1);
  if (margin == kEntryIncidence) {
    return "entry_incidence_cosine";
  }
  if (margin == kEntrySnell) {
    return "entry_snell_discriminant";
  }
  if (margin == exit) {
    return "exit_incidence_cosine";
  }
  if (margin == exit + 1) {
    return "exit_snell_discriminant";
  }
  std::ostringstream out;
  out << "internal_" << margin / 2 << (margin % 2 == 0 ? "_incidence_cosine" : "_tir_discriminant");
  return out.str();
}

std::string PathIdOf(const FaceNormalTable& table, const int* slots, int slot_count) {
  std::ostringstream out;
  for (int k = 0; k < slot_count; k++) {
    if (k > 0) {
      out << '-';
    }
    out << table.face_number[slots[k]];
  }
  return out.str();
}

// ---- margin identities and great circles ----------------------------------------------------------------

void IncidenceNormals(const FaceNormalTable& table, const int* slots, int slot_count, double out[kMaxFaceCount][3]) {
  Copy3(table.normal[slots[0]], out[0]);
  for (int k = 1; k < slot_count - 1; k++) {
    // R_{k-1}: the fold of the reflections before step k — FoldMatrixOf over the (k + 1)-prefix
    // (LI incidence_normals: fold_matrix(crystal, faces[:k + 1])).
    double r[9];
    FoldMatrixOf(table, slots, k + 1, r);
    const double* n_k = table.normal[slots[k]];
    for (int i = 0; i < 3; i++) {
      out[k][i] = r[0 * 3 + i] * n_k[0] + r[1 * 3 + i] * n_k[1] + r[2 * 3 + i] * n_k[2];
    }
  }
  double m[9];
  FoldMatrixOf(table, slots, slot_count, m);
  const double* n_b = table.normal[slots[slot_count - 1]];
  for (int i = 0; i < 3; i++) {
    out[slot_count - 1][i] = m[0 * 3 + i] * n_b[0] + m[1 * 3 + i] * n_b[1] + m[2 * 3 + i] * n_b[2];
  }
}

std::vector<std::pair<int, int>> IdenticalMargins(const FaceNormalTable& table, const int* slots, int slot_count) {
  std::vector<std::pair<int, int>> out;
  double incidence[kMaxFaceCount][3];
  IncidenceNormals(table, slots, slot_count, incidence);
  const int internal_count = slot_count - 2;  // faces[1:-1], internal step j sits at face position j + 1
  for (int j = 0; j + 2 < internal_count; j++) {
    // The side-face azimuth (face - 3) * 60 of the triple's three faces.
    int azimuth[3];
    bool all_side = true;
    for (int t = 0; t < 3; t++) {
      const int face = table.face_number[slots[j + 1 + t]];
      if (face >= 3 && face <= 8) {
        azimuth[t] = (face - 3) * 60;
      } else {
        all_side = false;
        break;
      }
    }
    if (!all_side) {
      continue;
    }
    const int first = ((azimuth[1] - azimuth[0]) % 360 + 360) % 360;
    const int second = ((azimuth[2] - azimuth[1]) % 360 + 360) % 360;
    bool agree = true;
    for (int i = 0; i < 3; i++) {
      if (std::fabs(incidence[j + 3][i] - incidence[j + 1][i]) > kIdentityNormalAtol) {
        agree = false;
        break;
      }
    }
    if (first != second || (first != 60 && first != 300) || !agree) {
      continue;
    }
    int kept = j + 1;
    // Chains of triples all map to the first: follow an already-dropped kept margin to its head.
    bool chained = true;
    while (chained) {
      chained = false;
      for (const auto& pair : out) {
        if (pair.first == 2 * kept) {
          kept = pair.second / 2;
          chained = true;
          break;
        }
      }
    }
    out.emplace_back(2 * (j + 3), 2 * kept);          // incidence cosine
    out.emplace_back(2 * (j + 3) + 1, 2 * kept + 1);  // TIR discriminant
  }
  return out;
}

std::vector<std::pair<int, std::array<double, 3>>> GreatCircleMargins(const DeviationField& field, int samples) {
  const FaceNormalTable& table = field.table();
  const int* slots = field.slots();
  const int slot_count = field.SlotCount();
  double incidence[kMaxFaceCount][3];
  IncidenceNormals(table, slots, slot_count, incidence);
  const double* n_a = incidence[0];

  std::vector<std::pair<int, std::array<double, 3>>> out;
  double margins[2 * kMaxFaceCount];
  for (int slot = 0; slot < slot_count; slot++) {
    const int margin = 2 * slot;  // the incidence cosine of this slot
    // The entry margin's m IS n_a (never orthogonal); it is considered unconditionally.
    if (slot != 0 && std::fabs(Dot3(incidence[slot], n_a)) > kGreatCircleAtol) {
      continue;
    }
    double m[3];
    Copy3(incidence[slot], m);
    Unit(m);
    double basis[2][3];
    TangentBasis(m, basis);  // e1 = unit(m x least-aligned axis), e2 = m x e1 (LI's own recipe)
    double worst = 0.0;
    for (int s = 0; s < samples; s++) {
      const double t = 2.0 * kPi * static_cast<double>(s) / static_cast<double>(samples);
      double u[3];
      for (int i = 0; i < 3; i++) {
        u[i] = std::cos(t) * basis[0][i] + std::sin(t) * basis[1][i];
      }
      field.DomainMarginsAt(u, margins);
      worst = std::max(worst, std::fabs(margins[margin]));
    }
    if (worst > kCoincidentAtol) {
      continue;
    }
    field.DomainMarginsAt(m, margins);
    const double sign = margins[margin] > 0.0 ? 1.0 : (margins[margin] < 0.0 ? -1.0 : 0.0);
    out.emplace_back(margin, std::array<double, 3>{ sign * m[0], sign * m[1], sign * m[2] });
  }
  return out;
}

// ---- the walker -----------------------------------------------------------------------------------------

const char* WalkStatusName(WalkStatus status) {
  switch (status) {
    case WalkStatus::kOk:
      return "ok";
    case WalkStatus::kStepsExhausted:
      return "steps_exhausted";
    case WalkStatus::kStartNoPoint:
      return "start_no_point";
    case WalkStatus::kStartCoversAll:
      return "start_covers_all";
    case WalkStatus::kStartNoEdge:
      return "start_no_edge";
    case WalkStatus::kCornerNotSimple:
      return "corner_not_simple";
    case WalkStatus::kNotClosed:
      return "not_closed";
    case WalkStatus::kNotFinite:
      return "not_finite";
    case WalkStatus::kBadOrientation:
      return "bad_orientation";
  }
  return "unknown_walk_status";
}

BoundaryWalker::BoundaryWalker(const DeviationField& field) : field_(field) {
  slot_count_ = field.SlotCount();
  margin_count_ = 2 * slot_count_;
  identical_ = IdenticalMargins(field.table(), field.slots(), slot_count_);
  for (const auto& [dropped, kept] : identical_) {
    kept_of_[dropped] = kept;
  }
  for (const auto& [margin, normal] : GreatCircleMargins(field, 64)) {
    has_circle_[margin] = true;
    circle_normal_[margin][0] = normal[0];
    circle_normal_[margin][1] = normal[1];
    circle_normal_[margin][2] = normal[2];
  }
  // The gates of U_P: the validity subset with the identity-dropped margins removed (LI
  // Walker.active). The subset's layout rule is stated once, in dp_field.
  for (int i = 0; i < margin_count_; i++) {
    if (IsValidityMarginPosition(i, margin_count_) && kept_of_[i] == 0) {
      active_.push_back(i);
    }
  }
}

void BoundaryWalker::TangentOf(const double u[3], const MarginJet& jet, int margin, double out[3]) const {
  const double radial = Dot3(jet.gradient[margin], u);
  for (int i = 0; i < 3; i++) {
    out[i] = jet.gradient[margin][i] - radial * u[i];
  }
}

void BoundaryWalker::TangentGradient(const double u[3], int margin, double out[3]) const {
  MarginJet jet;
  MarginsJacobian(u, &jet);
  TangentOf(u, jet, margin, out);
}

RoutedDeviationStatus BoundaryWalker::D(const double u[3], double* out) const {
  const FieldSample sample = field_.SampleOptical(u);
  return RoutedDeviation(sample, field_.fold().degenerate, out);
}

double BoundaryWalker::DOnExitTir(const double u[3]) const {
  return field_.SampleOptical(u).d_p_grazing;
}

void BoundaryWalker::Correct(double u[3], int margin) const {
  if (HasCircle(margin)) {
    const double* normal = CircleNormal(margin);
    const double dot = Dot3(u, normal);
    for (int i = 0; i < 3; i++) {
      u[i] = u[i] - dot * normal[i];
    }
    Unit(u);
    double margins[2 * kMaxFaceCount];
    Margins(u, margins);
    if (margins[margin] < 0.0) {
      // A one-ulp nudge onto the non-negative side (LI correct's closed-form branch).
      for (int i = 0; i < 3; i++) {
        u[i] += 1e-16 * normal[i];
      }
      Unit(u);
    }
    return;
  }
  MarginJet jet;
  for (int iteration = 0; iteration < 30; iteration++) {
    MarginsJacobian(u, &jet);
    double g[3];
    TangentOf(u, jet, margin, g);
    if (std::fabs(jet.margins[margin]) <= 1e-15) {
      break;
    }
    const double gg = Dot3(g, g);
    for (int i = 0; i < 3; i++) {
      u[i] -= (jet.margins[margin] / gg) * g[i];
    }
    Unit(u);
  }
  for (int iteration = 0; iteration < 8; iteration++) {
    MarginsJacobian(u, &jet);
    if (jet.margins[margin] >= 0.0) {
      break;
    }
    double g[3];
    TangentOf(u, jet, margin, g);
    const double push = (-jet.margins[margin]) / Dot3(g, g) + 1e-16 / std::sqrt(Dot3(g, g));
    for (int i = 0; i < 3; i++) {
      u[i] += push * g[i];
    }
    Unit(u);
  }
}

void BoundaryWalker::Direction(const double u[3], int margin, double out[3]) const {
  if (HasCircle(margin)) {
    Cross3(CircleNormal(margin), u, out);
  } else {
    double g[3];
    TangentGradient(u, margin, g);
    Cross3(g, u, out);
  }
  Unit(out);
}

void BoundaryWalker::Advance(const double u[3], int margin, const double tangent[3], double step, double out[3]) const {
  if (HasCircle(margin)) {
    double along[3];
    Cross3(CircleNormal(margin), u, along);
    Unit(along);
    const double align = Dot3(along, tangent) >= 0.0 ? 1.0 : -1.0;
    for (int i = 0; i < 3; i++) {
      out[i] = std::cos(step) * u[i] + std::sin(step) * align * along[i];
    }
    Unit(out);
    return;
  }
  for (int i = 0; i < 3; i++) {
    out[i] = u[i] + step * tangent[i];
  }
  Unit(out);
  Correct(out, margin);
}

void BoundaryWalker::RefineCorner(double u[3], int a, int b) const {
  const auto residual = [this, a, b](const double at[3]) {
    double margins[2 * kMaxFaceCount];
    Margins(at, margins);
    return std::max(std::fabs(margins[a]), std::fabs(margins[b]));
  };
  double best[3];
  Copy3(u, best);
  double best_residual = residual(u);
  for (int iteration = 0; iteration < 30; iteration++) {
    MarginJet jet;
    MarginsJacobian(u, &jet);
    double basis[2][3];
    TangentBasis(u, basis);
    double ga[3];
    double gb[3];
    TangentOf(u, jet, a, ga);
    TangentOf(u, jet, b, gb);
    const double jac[2][2] = { { Dot3(ga, basis[0]), Dot3(ga, basis[1]) }, { Dot3(gb, basis[0]), Dot3(gb, basis[1]) } };
    const double det = jac[0][0] * jac[1][1] - jac[0][1] * jac[1][0];
    if (std::fabs(det) < 1e-14) {
      break;
    }
    // solve(jac, -[m_a, m_b]) by the 2x2 inverse.
    const double step[2] = { (-jet.margins[a] * jac[1][1] + jet.margins[b] * jac[0][1]) / det,
                             (jet.margins[a] * jac[1][0] - jet.margins[b] * jac[0][0]) / det };
    if (std::sqrt(step[0] * step[0] + step[1] * step[1]) > 1e-3) {
      break;
    }
    for (int i = 0; i < 3; i++) {
      u[i] += step[0] * basis[0][i] + step[1] * basis[1][i];
    }
    Unit(u);
    const double current = residual(u);
    if (current < best_residual) {
      Copy3(u, best);
      best_residual = current;
    }
    if (current <= 1e-16) {
      break;
    }
  }
  Copy3(best, u);
}

int BoundaryWalker::Violated(const double u[3], const int* excluded, int excluded_count,
                             int out[kMaxFaceCount + 2]) const {
  double margins[2 * kMaxFaceCount];
  Margins(u, margins);
  int count = 0;
  for (int margin : active_) {
    bool is_excluded = false;
    for (int k = 0; k < excluded_count; k++) {
      if (excluded[k] == margin) {
        is_excluded = true;
        break;
      }
    }
    // `not (m >= -atol)`: a NaN margin violates too — fail closed (LI violated).
    if (!is_excluded && !(margins[margin] >= -kViolationAtol)) {
      out[count++] = margin;
    }
  }
  return count;
}

int BoundaryWalker::MostViolated(const double u[3], const int* names, int count) const {
  MarginJet jet;
  MarginsJacobian(u, &jet);
  double g[3];
  int best = names[0];
  double best_distance = 0.0;
  for (int k = 0; k < count; k++) {
    TangentOf(u, jet, names[k], g);
    const double distance = jet.margins[names[k]] / std::sqrt(Dot3(g, g));
    if (k == 0 || distance < best_distance) {
      best = names[k];
      best_distance = distance;
    }
  }
  return best;
}

int BoundaryWalker::CoincidentWith(const double u[3], int margin, int out[kMaxFaceCount + 2]) const {
  double margins[2 * kMaxFaceCount];
  Margins(u, margins);
  int count = 0;
  for (int active : active_) {
    if (active != margin && std::fabs(margins[active]) <= kCoincidentAtol) {
      out[count++] = active;
    }
  }
  return count;
}

// ---- the walks ------------------------------------------------------------------------------------------

namespace {

// A point of dU_P and its margin: bisect between a lattice point of U_P and an outside neighbour
// (LI _start_point). The lattice is LI fibonacci_sphere — discovery.hpp's LatticePoint negated
// (dp_partition's convention).
struct StartPoint {
  WalkStatus status = WalkStatus::kOk;
  std::string message;
  double u[3] = {};
  int margin = -1;
};

bool InsideUp(const DeviationField& field, const double u[3]) {
  double margins[kMaxFaceCount + 2];
  const int count = field.ValidityMarginsAt(u, margins);
  for (int k = 0; k < count; k++) {
    if (!(margins[k] > 0.0)) {
      return false;
    }
  }
  return true;
}

StartPoint FindStartPoint(const BoundaryWalker& walker, int lattice_n) {
  const DeviationField& field = walker.field();
  const std::string path_id = PathId(walker);
  std::vector<double> lattice(static_cast<size_t>(lattice_n) * 3);
  std::vector<unsigned char> valid(static_cast<size_t>(lattice_n));
  int valid_count = 0;
  for (int i = 0; i < lattice_n; i++) {
    double f[3];
    LatticePoint(lattice_n, i, f);
    for (int c = 0; c < 3; c++) {
      lattice[3 * static_cast<size_t>(i) + c] = -f[c];
    }
    valid[i] = InsideUp(field, &lattice[3 * static_cast<size_t>(i)]) ? 1 : 0;
    valid_count += valid[i];
  }
  StartPoint start;
  if (valid_count == 0) {
    start.status = WalkStatus::kStartNoPoint;
    start.message = "U_P of " + path_id + " has no point on a " + std::to_string(lattice_n) + "-point lattice";
    return start;
  }
  if (valid_count == lattice_n) {
    start.status = WalkStatus::kStartCoversAll;
    start.message = "U_P covers the whole lattice: no boundary";
    return start;
  }
  // The first valid row with an invalid member among its 6 nearest neighbours (LI's cKDTree k=7
  // row scan). The neighbour order among equidistant ties is free — any outside neighbour
  // bisects onto dU_P, and every anchor of the walk is start-point independent.
  int row = -1;
  int out_neighbour = -1;
  for (int i = 0; i < lattice_n && row < 0; i++) {
    if (!valid[i]) {
      continue;
    }
    const double* p = &lattice[3 * static_cast<size_t>(i)];
    double best2[6];
    int best_j[6];
    int filled = 0;
    for (int j = 0; j < lattice_n; j++) {
      if (j == i) {
        continue;
      }
      const double* q = &lattice[3 * static_cast<size_t>(j)];
      const double d2 = (p[0] - q[0]) * (p[0] - q[0]) + (p[1] - q[1]) * (p[1] - q[1]) + (p[2] - q[2]) * (p[2] - q[2]);
      if (filled < 6) {
        best2[filled] = d2;
        best_j[filled] = j;
        filled++;
        if (filled == 6) {
          for (int a = 1; a < 6; a++) {
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
      if (d2 < best2[5]) {
        int b = 5;
        while (b > 0 && best2[b - 1] > d2) {
          best2[b] = best2[b - 1];
          best_j[b] = best_j[b - 1];
          b--;
        }
        best2[b] = d2;
        best_j[b] = j;
      }
    }
    for (int k = 0; k < 6; k++) {
      if (!valid[best_j[k]]) {
        row = i;
        out_neighbour = best_j[k];
        break;
      }
    }
  }
  if (row < 0) {
    start.status = WalkStatus::kStartNoEdge;
    start.message = "U_P of " + path_id + " has no lattice point next to its boundary";
    return start;
  }
  double inside[3];
  double out[3];
  for (int c = 0; c < 3; c++) {
    inside[c] = lattice[3 * static_cast<size_t>(row) + c];
    out[c] = lattice[3 * static_cast<size_t>(out_neighbour) + c];
  }
  int bad[kMaxFaceCount + 2];
  for (int iteration = 0; iteration < 200; iteration++) {
    double mid[3] = { inside[0] + out[0], inside[1] + out[1], inside[2] + out[2] };
    Unit(mid);
    if (walker.Violated(mid, nullptr, 0, bad) == 0) {
      Copy3(mid, inside);
    } else {
      Copy3(mid, out);
    }
    if (AngleBetween(inside, out) < 1e-12) {
      break;
    }
  }
  const int bad_count = walker.Violated(out, nullptr, 0, bad);
  start.margin = walker.MostViolated(out, bad, bad_count);
  walker.Correct(inside, start.margin);
  Copy3(inside, start.u);
  return start;
}

// The one vanishing margin at a corner whose zero set continues dU_P (U_P on its left); not
// unique is kCornerNotSimple (LI _outgoing).
WalkStatus OutgoingMargin(const BoundaryWalker& walker, const double corner[3], int incoming,
                          const std::vector<int>& incoming_coincident, int* outgoing, std::string* message) {
  MarginJet jet;
  walker.MarginsJacobian(corner, &jet);
  double corner_mutable[3];
  Copy3(corner, corner_mutable);
  std::vector<int> zero;
  for (int margin : walker.active()) {
    if (std::fabs(jet.margins[margin]) <= kZeroMarginAtol) {
      zero.push_back(margin);
    }
  }
  std::vector<int> accepted;
  for (int margin : zero) {
    if (margin == incoming || Contains(incoming_coincident, margin)) {
      continue;
    }
    double g[3];
    walker.TangentGradient(corner, margin, g);
    if (std::sqrt(Dot3(g, g)) < kTouchingGradientAtol) {
      continue;
    }
    double direction[3];
    walker.Direction(corner, margin, direction);
    double probe[3];
    walker.Advance(corner, margin, direction, kCornerProbeRad, probe);
    int coincident[kMaxFaceCount + 2];
    const int coincident_count = walker.CoincidentWith(probe, margin, coincident);
    std::vector<int> excluded;
    excluded.push_back(margin);
    for (int k = 0; k < coincident_count; k++) {
      excluded.push_back(coincident[k]);
    }
    int bad[kMaxFaceCount + 2];
    if (walker.Violated(probe, excluded.data(), static_cast<int>(excluded.size()), bad) == 0) {
      accepted.push_back(margin);
    }
  }
  if (accepted.size() != 1) {
    *outgoing = -1;
    std::ostringstream out;
    out << "corner " << PointList(corner) << " of " << PathId(walker) << " continues along (";
    for (size_t k = 0; k < accepted.size(); k++) {
      out << (k > 0 ? ", " : "") << MarginName(walker.margin_count(), accepted[k]);
    }
    out << ") (vanishing margins (";
    for (size_t k = 0; k < zero.size(); k++) {
      out << (k > 0 ? ", " : "") << MarginName(walker.margin_count(), zero[k]);
    }
    out << ")): not a simple boundary loop";
    *message = out.str();
    return WalkStatus::kCornerNotSimple;
  }
  *outgoing = accepted[0];
  return WalkStatus::kOk;
}

// The corner's record (LI _corner_record). Fails with kNotFinite when D_P refuses the point.
WalkStatus MakeCornerRecord(const BoundaryWalker& walker, const double u[3], int incoming, int outgoing,
                            const std::vector<int>& coincident, WalkerCorner* corner, std::string* message) {
  MarginJet jet;
  walker.MarginsJacobian(u, &jet);
  double u_mutable[3];
  Copy3(u, u_mutable);
  std::vector<int> zero;
  for (int margin : walker.active()) {
    if (std::fabs(jet.margins[margin]) <= kZeroMarginAtol || margin == incoming || margin == outgoing) {
      zero.push_back(margin);
    }
  }
  const int edges[2] = { incoming, outgoing };
  double edge_gradients[2][3];
  for (int e = 0; e < 2; e++) {
    walker.TangentGradient(u, edges[e], edge_gradients[e]);
    Unit(edge_gradients[e]);
  }
  for (int margin : zero) {
    if (margin == incoming || margin == outgoing || Contains(coincident, margin)) {
      continue;
    }
    double g[3];
    walker.TangentGradient(u, margin, g);
    Unit(g);
    double sine_min = 0.0;
    for (int e = 0; e < 2; e++) {
      double cross[3];
      Cross3(g, edge_gradients[e], cross);
      const double sine = std::sqrt(Dot3(cross, cross));
      sine_min = e == 0 ? sine : std::min(sine_min, sine);
    }
    (sine_min < kTangentSineAtol ? corner->tangent : corner->transversal).push_back(margin);
  }
  corner->residual = 0.0;
  for (int margin : zero) {
    corner->residual = std::max(corner->residual, std::fabs(jet.margins[margin]));
  }
  for (int margin : zero) {
    if (Contains(coincident, margin)) {
      corner->coincident.push_back(margin);
    }
  }
  Copy3(u, corner->position);
  corner->incoming = incoming;
  corner->outgoing = outgoing;
  corner->margins = zero;
  if (walker.D(u, &corner->value) != RoutedDeviationStatus::kOk) {
    *message = "D_P is not finite at " + PointList(u) + " on " + PathId(walker);
    return WalkStatus::kNotFinite;
  }
  return WalkStatus::kOk;
}

// Refine the sample extremum points[i] on the piece between its neighbours, golden section (LI
// _golden_extremum). On the exit TIR curve of a non-slab path the search and the returned value
// are DOnExitTir (the Holder-1/2 noise of a plain sample is ~sqrt(1e-16) = 1e-8 rad, which moved
// a flat extremum's position by ~7e-4 rad on 3-5 — task home-wsl-dp-field-diffs); elsewhere D
// itself. A refusal of D anywhere fails the refinement (LI lets the raise propagate).
WalkStatus GoldenExtremum(const BoundaryWalker& walker, const BoundaryPiece& piece, int i, CriticalKind kind,
                          double position[3], double* value, std::string* message) {
  double a[3];
  double b[3];
  if (i == 0) {
    // Only on a loop without corners, whose single piece ends where it starts.
    Copy3(&piece.points[3 * (static_cast<int>(piece.points.size() / 3) - 2)], a);
  } else {
    Copy3(&piece.points[3 * (i - 1)], a);
  }
  Copy3(&piece.points[3 * (i + 1)], b);
  const double sign = kind == CriticalKind::kMinimum ? 1.0 : -1.0;
  const bool exit_tir_piece = piece.margin == ExitSnellOf(walker.slot_count()) && !walker.field().fold().degenerate;

  const auto point = [&](double lambda, double out[3]) {
    for (int c = 0; c < 3; c++) {
      out[c] = (1.0 - lambda) * a[c] + lambda * b[c];
    }
    Unit(out);
    walker.Correct(out, piece.margin);
  };
  bool refused = false;
  const auto f = [&](double lambda) {
    double at[3];
    point(lambda, at);
    double at_value = 0.0;
    if (exit_tir_piece) {
      at_value = walker.DOnExitTir(at);
    } else if (walker.D(at, &at_value) != RoutedDeviationStatus::kOk) {
      refused = true;
      return 0.0;
    }
    return sign * at_value;
  };

  const double ratio = (std::sqrt(5.0) - 1.0) / 2.0;
  double lo = 0.0;
  double hi = 1.0;
  double x1 = hi - ratio * (hi - lo);
  double x2 = lo + ratio * (hi - lo);
  double f1 = f(x1);
  double f2 = f(x2);
  for (int iteration = 0; iteration < 80 && !refused; iteration++) {
    if (f1 <= f2) {
      hi = x2;
      x2 = x1;
      f2 = f1;
      x1 = hi - ratio * (hi - lo);
      f1 = f(x1);
    } else {
      lo = x1;
      x1 = x2;
      f1 = f2;
      x2 = lo + ratio * (hi - lo);
      f2 = f(x2);
    }
    if (hi - lo < 1e-12) {
      break;
    }
  }
  if (refused) {
    *message = "D_P is not finite on the golden section of a " + MarginName(walker.margin_count(), piece.margin) +
               " piece of " + PathId(walker);
    return WalkStatus::kNotFinite;
  }
  point(0.5 * (lo + hi), position);
  if (exit_tir_piece) {
    *value = walker.DOnExitTir(position);
  } else if (walker.D(position, value) != RoutedDeviationStatus::kOk) {
    *message = "D_P is not finite at " + PointList(position) + " on " + PathId(walker);
    return WalkStatus::kNotFinite;
  }
  return WalkStatus::kOk;
}

}  // namespace

ZeroSetWalk WalkZeroSet(const BoundaryWalker& walker, const double start[3], int margin, double orientation,
                        const double* stop_at, const BoundaryWalkOptions& options) {
  ZeroSetWalk walk;
  if (orientation != 1.0 && orientation != -1.0) {
    walk.status = WalkStatus::kBadOrientation;
    std::ostringstream out;
    out.precision(17);
    out << "orientation must be 1 or -1, got " << orientation;
    walk.message = out.str();
    return walk;
  }
  const double step = options.step;
  double direction[3];
  walker.Direction(start, margin, direction);
  double probe[3];
  walker.Advance(start, margin, direction, std::min(step, 1e-3), probe);
  int start_coincident[kMaxFaceCount + 2];
  int probe_coincident[kMaxFaceCount + 2];
  const int start_count = walker.CoincidentWith(start, margin, start_coincident);
  const int probe_count = walker.CoincidentWith(probe, margin, probe_coincident);
  for (int k = 0; k < start_count; k++) {
    for (int j = 0; j < probe_count; j++) {
      if (probe_coincident[j] == start_coincident[k]) {
        walk.coincident.push_back(start_coincident[k]);
        break;
      }
    }
  }
  std::vector<int> excluded = walk.coincident;
  excluded.push_back(margin);

  walk.points.resize(3);
  Copy3(start, &walk.points[0]);
  double u[3];
  Copy3(start, u);
  double arc = 0.0;
  for (int taken = 0; taken < options.max_walk_steps; taken++) {
    double tangent[3];
    walker.Direction(u, margin, tangent);
    for (int i = 0; i < 3; i++) {
      tangent[i] *= orientation;
    }
    // Closure on distance alone (module docstring): within one step of the seed after at least two
    // steps of arc — the arc bound rules out the departure transient a heading test would miss.
    if (stop_at != nullptr && arc >= 2.0 * step && AngleBetween(u, stop_at) <= step) {
      if (AngleBetween(u, stop_at) > 0.0) {
        const size_t at = walk.points.size();
        walk.points.resize(at + 3);
        Copy3(stop_at, &walk.points[at]);
      }
      return walk;  // met_corner false: back at the stop point
    }
    double nxt[3];
    walker.Advance(u, margin, tangent, step, nxt);
    int bad[kMaxFaceCount + 2];
    const int excluded_count = static_cast<int>(excluded.size());
    if (walker.Violated(nxt, excluded.data(), excluded_count, bad) == 0) {
      arc += AngleBetween(u, nxt);
      const size_t at = walk.points.size();
      walk.points.resize(at + 3);
      Copy3(nxt, &walk.points[at]);
      Copy3(nxt, u);
      continue;
    }
    // Bisect the crossing on the step length, then name the gate that turns negative there.
    double lo = 0.0;
    double hi = step;
    double hi_point[3];
    Copy3(nxt, hi_point);
    for (int iteration = 0; iteration < 80; iteration++) {
      const double mid = 0.5 * (lo + hi);
      double p[3];
      walker.Advance(u, margin, tangent, mid, p);
      if (walker.Violated(p, excluded.data(), excluded_count, bad) > 0) {
        hi = mid;
        Copy3(p, hi_point);
      } else {
        lo = mid;
      }
      if (hi - lo < 1e-15) {
        break;
      }
    }
    const int hi_bad_count = walker.Violated(hi_point, excluded.data(), excluded_count, bad);
    const int crossing = walker.MostViolated(hi_point, bad, hi_bad_count);
    double corner[3];
    walker.Advance(u, margin, tangent, lo, corner);
    walker.RefineCorner(corner, margin, crossing);
    const size_t at = walk.points.size();
    walk.points.resize(at + 3);
    Copy3(corner, &walk.points[at]);
    walk.met_corner = true;
    Copy3(corner, walk.corner);
    return walk;
  }
  walk.status = WalkStatus::kStepsExhausted;
  const double* reach = stop_at != nullptr ? stop_at : start;
  std::ostringstream out;
  out << "boundary walk of " << PathId(walker) << " did not reach a corner in " << options.max_walk_steps
      << " steps (gate " << MarginName(walker.margin_count(), margin) << ", arc " << arc << " rad, end-to-seed angle "
      << AngleBetween(u, reach) << " rad, seed " << PointList(reach) << ", end " << PointList(u) << ")";
  walk.message = out.str();
  return walk;
}

std::vector<PlateauExtremum> PlateauExtremaOf(const double* values, int count, double atol, bool* is_plateau,
                                              double* plateau_value) {
  *is_plateau = false;
  *plateau_value = 0.0;
  // Compress runs of equal values (LI _plateau_extrema).
  std::vector<std::pair<int, double>> runs;
  for (int i = 0; i < count; i++) {
    if (!runs.empty() && std::fabs(values[i] - runs.back().second) <= atol) {
      continue;
    }
    runs.emplace_back(i, values[i]);
  }
  const bool wrap_merged = runs.size() > 1 && std::fabs(runs[0].second - runs.back().second) <= atol;
  int popped_start = -1;
  if (wrap_merged) {
    popped_start = runs.back().first;
    runs.pop_back();
  }
  const int r = static_cast<int>(runs.size());
  if (r == 1) {
    *is_plateau = true;
    *plateau_value = runs[0].second;
    return {};
  }
  std::vector<int> lengths(r);
  for (int j = 0; j < r; j++) {
    int end;
    if (j + 1 < r) {
      end = runs[j + 1].first;
    } else if (wrap_merged) {
      end = popped_start;  // the merged first run owns [popped_start, count)
    } else {
      end = count;
    }
    lengths[j] = end - runs[j].first;
  }
  if (wrap_merged) {
    lengths[0] += count - popped_start;  // the popped tail run's samples belong to the merged first run
  }
  std::vector<PlateauExtremum> out;
  for (int j = 0; j < r; j++) {
    const double prev_v = runs[j > 0 ? j - 1 : r - 1].second;
    const double v = runs[j].second;
    const double next_v = runs[(j + 1) % r].second;
    if (v < prev_v && v < next_v) {
      out.push_back({ runs[j].first, CriticalKind::kMinimum, lengths[j] });
    } else if (v > prev_v && v > next_v) {
      out.push_back({ runs[j].first, CriticalKind::kMaximum, lengths[j] });
    }
  }
  return out;
}

// ---- the loop assembly -----------------------------------------------------------------------------------

namespace {

// The piece record: values through the closure convention (LI _piece; a refusal fails the walk).
WalkStatus MakePiece(const BoundaryWalker& walker, int margin, const std::vector<double>& points,
                     const std::vector<int>& coincident, BoundaryPiece* piece, std::string* message) {
  piece->margin = margin;
  piece->great_circle = walker.HasCircle(margin);
  if (piece->great_circle) {
    Copy3(walker.CircleNormal(margin), piece->circle_normal);
  }
  piece->points = points;
  piece->coincident.clear();
  for (int active : walker.active()) {
    if (Contains(coincident, active)) {
      piece->coincident.push_back(active);
    }
  }
  piece->values.resize(points.size() / 3);
  for (size_t k = 0; k < piece->values.size(); k++) {
    if (walker.D(&points[3 * k], &piece->values[k]) != RoutedDeviationStatus::kOk) {
      *message = "D_P is not finite at " + PointList(&points[3 * k]) + " on " + PathId(walker);
      return WalkStatus::kNotFinite;
    }
  }
  return WalkStatus::kOk;
}

// Local extrema of D_P along the loop (corners as they are, piece samples refined by golden
// section) and a constant loop's value (LI _loop_critical_points).
WalkStatus LoopCriticalPoints(const BoundaryWalker& walker, const std::vector<BoundaryPiece>& pieces,
                              const std::vector<WalkerCorner>& corners, std::vector<BoundaryCriticalPoint>* critical,
                              bool* has_plateau, double* plateau_value, std::string* message) {
  // Loop samples: each piece without its last point (the next piece starts there); remember owners.
  std::vector<std::pair<int, int>> owners;
  std::vector<double> values;
  for (size_t p = 0; p < pieces.size(); p++) {
    const size_t point_count = pieces[p].points.size() / 3;
    for (size_t i = 0; i + 1 < point_count; i++) {
      owners.emplace_back(static_cast<int>(p), static_cast<int>(i));
      values.push_back(pieces[p].values[i]);
    }
  }
  const std::vector<PlateauExtremum> extrema =
      PlateauExtremaOf(values.data(), static_cast<int>(values.size()), kExtremumAtol, has_plateau, plateau_value);
  for (const PlateauExtremum& extremum : extrema) {
    const auto [p_index, i] = owners[extremum.index];
    const BoundaryPiece& piece = pieces[p_index];
    BoundaryCriticalPoint point;
    point.kind = extremum.kind;
    point.strict = extremum.run_length == 1;
    if (i == 0 && !corners.empty()) {
      // The sample at a piece's start IS the previous corner (Python's corners[-1] for piece 0).
      const WalkerCorner& corner = corners[p_index > 0 ? p_index - 1 : static_cast<int>(corners.size()) - 1];
      Copy3(corner.position, point.position);
      point.value = corner.value;
      point.corner = true;
      critical->push_back(point);
      continue;
    }
    double position[3];
    double value = 0.0;
    const WalkStatus status = GoldenExtremum(walker, piece, i, extremum.kind, position, &value, message);
    if (status != WalkStatus::kOk) {
      return status;
    }
    Copy3(position, point.position);
    point.value = value;
    point.corner = false;
    critical->push_back(point);
  }
  return WalkStatus::kOk;
}

}  // namespace

WalkResult WalkBoundary(const DeviationField& field, const BoundaryWalkOptions& options, BoundaryWalkRecord* record) {
  WalkResult result;
  const BoundaryWalker walker(field);
  const StartPoint start = FindStartPoint(walker, options.lattice_n);
  if (start.status != WalkStatus::kOk) {
    result.status = start.status;
    result.message = start.message;
    return result;
  }
  std::vector<BoundaryPiece> pieces;
  std::vector<WalkerCorner> corners;
  std::string message;

  // 1. find the first corner (or come back to the start: a smooth loop).
  ZeroSetWalk first = WalkZeroSet(walker, start.u, start.margin, 1.0, start.u, options);
  if (first.status != WalkStatus::kOk) {
    result.status = first.status;
    result.message = first.message;
    return result;
  }
  if (!first.met_corner) {
    BoundaryPiece piece;
    if (MakePiece(walker, start.margin, first.points, first.coincident, &piece, &message) != WalkStatus::kOk) {
      result.status = WalkStatus::kNotFinite;
      result.message = message;
      return result;
    }
    pieces.push_back(std::move(piece));
  } else {
    double first_corner[3];
    Copy3(first.corner, first_corner);
    int incoming = start.margin;
    std::vector<int> incoming_coincident = first.coincident;
    double u[3];
    Copy3(first_corner, u);
    bool closed = false;
    for (int round = 0; round < 1000 && !closed; round++) {
      // 2. pick the outgoing margin, probe past the corner, walk the piece to the next corner.
      int outgoing = -1;
      if (OutgoingMargin(walker, u, incoming, incoming_coincident, &outgoing, &message) != WalkStatus::kOk) {
        result.status = WalkStatus::kCornerNotSimple;
        result.message = message;
        return result;
      }
      double direction[3];
      walker.Direction(u, outgoing, direction);
      double begin[3];
      walker.Advance(u, outgoing, direction, kCornerProbeRad, begin);
      ZeroSetWalk piece_walk = WalkZeroSet(walker, begin, outgoing, 1.0, nullptr, options);
      if (piece_walk.status != WalkStatus::kOk) {
        result.status = piece_walk.status;
        result.message = piece_walk.message;
        return result;
      }
      // The piece starts at the corner itself, then the walked points (LI's [u, *points]).
      std::vector<double> points(3);
      Copy3(u, &points[0]);
      points.insert(points.end(), piece_walk.points.begin(), piece_walk.points.end());
      BoundaryPiece piece;
      if (MakePiece(walker, outgoing, points, piece_walk.coincident, &piece, &message) != WalkStatus::kOk) {
        result.status = WalkStatus::kNotFinite;
        result.message = message;
        return result;
      }
      pieces.push_back(std::move(piece));
      incoming = outgoing;
      incoming_coincident = piece_walk.coincident;
      Copy3(piece_walk.corner, u);  // stop_at null: the walk ended at a corner or already failed
      if (AngleBetween(piece_walk.corner, first_corner) <= kCornerCloseRad) {
        closed = true;
      }
    }
    if (!closed) {
      result.status = WalkStatus::kNotClosed;
      result.message = "boundary walk of " + PathId(walker) + " did not close";
      return result;
    }
    // 3. the loop starts and ends at `first_corner`: pieces[i] runs from corner i - 1 to corner i.
    // Glue the last point to the first corner exactly and take its value (LI _replace_last_point).
    BoundaryPiece& last = pieces.back();
    Copy3(first_corner, &last.points[last.points.size() - 3]);
    if (walker.D(first_corner, &last.values.back()) != RoutedDeviationStatus::kOk) {
      result.status = WalkStatus::kNotFinite;
      result.message = "D_P is not finite at " + PointList(first_corner) + " on " + PathId(walker);
      return result;
    }
    for (size_t i = 0; i < pieces.size(); i++) {
      const BoundaryPiece& piece = pieces[i];
      const BoundaryPiece& after = pieces[(i + 1) % pieces.size()];
      std::vector<int> corner_coincident = piece.coincident;
      for (int margin : after.coincident) {
        if (!Contains(corner_coincident, margin)) {
          corner_coincident.push_back(margin);
        }
      }
      WalkerCorner corner;
      if (MakeCornerRecord(walker, &piece.points[piece.points.size() - 3], piece.margin, after.margin,
                           corner_coincident, &corner, &message) != WalkStatus::kOk) {
        result.status = WalkStatus::kNotFinite;
        result.message = message;
        return result;
      }
      corners.push_back(std::move(corner));
    }
  }

  bool has_plateau = false;
  double plateau_value = 0.0;
  if (LoopCriticalPoints(walker, pieces, corners, &result.loop.critical_points, &has_plateau, &plateau_value,
                         &message) != WalkStatus::kOk) {
    result.status = WalkStatus::kNotFinite;
    result.message = message;
    return result;
  }
  result.loop.has_plateau = has_plateau;
  result.loop.plateau_value = plateau_value;
  for (const WalkerCorner& corner : corners) {
    LoopCorner consumer;
    Copy3(corner.position, consumer.position);
    consumer.value = corner.value;
    result.loop.corners.push_back(consumer);
  }
  Copy3(&pieces[0].points[0], result.loop.first_point);
  if (record != nullptr) {
    record->pieces = std::move(pieces);
    record->corners = std::move(corners);
    record->identical = walker.identical();
    record->great_circles.clear();
    for (int margin = 0; margin < walker.margin_count(); margin++) {
      if (walker.HasCircle(margin)) {
        record->great_circles.push_back(margin);
      }
    }
    record->data = result.loop;
  }
  return result;
}

}  // namespace lumice::analytic
