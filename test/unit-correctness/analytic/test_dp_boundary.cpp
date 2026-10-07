// The dU_P walk (src/analytic/dp_boundary.hpp, LI dp_field.boundary ported for scrum 660.3)
// against the LI anchors dumped by the task's dump_anchors.py (LI commit 4d0184c6): margin
// identities, great-circle detection, the five-fixture piece/corner structure, the closure-limit
// convention, the 1-2-1 plateau loop, the distance-only closure criterion, and the plateau-extrema
// tables. Every tolerance is an assertion constant (cross-ISA discipline); the implementation
// carries none.
//
// symmetry_semantics: none — no symmetry reduction is involved.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "analytic/discovery.hpp"
#include "analytic/dp_boundary.hpp"
#include "analytic/dp_partition.hpp"
#include "analytic/path_evaluation.hpp"

namespace lumice::analytic {
namespace {

constexpr double kN131 = 1.31;       // LI's default ice index (the boundary fixtures' n)
constexpr double kN550 = 1.3110129;  // LI's rounded n(550) of the closure-corner fixture
constexpr double kPi = 3.14159265358979323846;

double Deg(double rad) {
  return rad * 180.0 / kPi;
}

LUMICE_ANALYTIC_Crystal Prism() {
  LUMICE_ANALYTIC_Crystal c{};
  c.kind = LUMICE_ANALYTIC_CRYSTAL_PRISM;
  c.height = 1.0;
  for (int i = 0; i < 6; i++) {
    c.face_distance[i] = 1.0;
  }
  return c;
}

// The beta crystal of the 52.x fixtures: fd = [2, 1, 1, 2, 1, 1], h = 3.
LUMICE_ANALYTIC_Crystal Beta() {
  LUMICE_ANALYTIC_Crystal c{};
  c.kind = LUMICE_ANALYTIC_CRYSTAL_PRISM;
  c.height = 3.0;
  const double fd[6] = { 2.0, 1.0, 1.0, 2.0, 1.0, 1.0 };
  for (int i = 0; i < 6; i++) {
    c.face_distance[i] = fd[i];
  }
  return c;
}

struct Fixture {
  FaceNormalTable normals;
  FacePolygonTable polygons;

  explicit Fixture(const LUMICE_ANALYTIC_Crystal& crystal) {
    EXPECT_EQ(BuildFaceNormals(crystal, &normals, &polygons), Status::kOk);
  }

  DeviationField Field(const int* faces, int count, double n) const {
    int slots[kMaxFaceCount];
    EXPECT_EQ(ResolveFaceSequence(normals, faces, count, slots), Status::kOk);
    return DeviationField(normals, polygons, slots, count, n);
  }
};

double Angle(const double a[3], const double b[3]) {
  const double cross[3] = { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };
  const double dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  return std::atan2(std::sqrt(cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2]), dot);
}

double MinAngleToPoints(const double u[3], const std::vector<double>& points) {
  double best = 1e9;
  for (size_t k = 0; k + 2 < points.size(); k += 3) {
    best = std::min(best, Angle(u, &points[k]));
  }
  return best;
}

// The five walk fixtures of LI test_dp_field_boundary (canonical prism, n = 1.31).
const int kWalkFixture35[2] = { 3, 5 };
const int kWalkFixture13[2] = { 1, 3 };
const int kWalkFixture316[3] = { 3, 1, 6 };
const int kWalkFixture132[3] = { 1, 3, 2 };
const int kWalkFixture35673[5] = { 3, 5, 6, 7, 3 };
struct WalkFixture {
  const char* name;
  const int* faces;
  int count;
};
const WalkFixture kWalkFixtures[] = {
  { "3-5", kWalkFixture35, 2 },    { "1-3", kWalkFixture13, 2 },          { "3-1-6", kWalkFixture316, 3 },
  { "1-3-2", kWalkFixture132, 3 }, { "3-5-6-7-3", kWalkFixture35673, 5 },
};

// ---------------------------------------------------------------------------------------------
// Margin identities and great circles
// ---------------------------------------------------------------------------------------------

// LI test_identical_margins_of_60_degree_side_face_triples: three consecutive reflections off
// side faces stepping by the same +-60 degrees make the third step's margins the first's; steps
// of 120 / -60, a basal face in the triple and a two-reflection path keep both.
TEST(DPBoundary, IdenticalMarginsOfSixtyDegreeSideFaceTriples) {
  const Fixture f(Prism());
  const auto pairs_of = [&f](const int* faces, int count) {
    int slots[kMaxFaceCount];
    EXPECT_EQ(ResolveFaceSequence(f.normals, faces, count, slots), Status::kOk);
    return IdenticalMargins(f.normals, slots, count);
  };
  const auto names_of = [&f](const std::vector<std::pair<int, int>>& pairs, int slot_count) {
    std::vector<std::pair<std::string, std::string>> out;
    for (const auto& [dropped, kept] : pairs) {
      out.emplace_back(MarginName(slot_count, dropped), MarginName(slot_count, kept));
    }
    std::sort(out.begin(), out.end());
    return out;
  };
  const int a[5] = { 3, 5, 6, 7, 3 };
  const int b[5] = { 3, 4, 5, 6, 3 };
  const int c[5] = { 3, 8, 7, 6, 3 };
  EXPECT_EQ(names_of(pairs_of(a, 5), 5), (std::vector<std::pair<std::string, std::string>>{
                                             { "internal_3_incidence_cosine", "internal_1_incidence_cosine" },
                                             { "internal_3_tir_discriminant", "internal_1_tir_discriminant" } }));
  EXPECT_EQ(names_of(pairs_of(b, 5), 5), (std::vector<std::pair<std::string, std::string>>{
                                             { "internal_3_incidence_cosine", "internal_1_incidence_cosine" },
                                             { "internal_3_tir_discriminant", "internal_1_tir_discriminant" } }));
  bool found = false;
  for (const auto& [dropped, kept] : pairs_of(c, 5)) {
    if (MarginName(5, dropped) == "internal_3_tir_discriminant" &&
        MarginName(5, kept) == "internal_1_tir_discriminant") {
      found = true;
    }
  }
  EXPECT_TRUE(found);
  const int neg1[5] = { 3, 5, 7, 6, 3 };
  const int neg2[5] = { 3, 5, 1, 7, 3 };
  EXPECT_TRUE(pairs_of(neg1, 5).empty());
  EXPECT_TRUE(pairs_of(neg2, 5).empty());
  EXPECT_TRUE(pairs_of(kWalkFixture316, 3).empty());
}

// LI test_dropped_margins_equal_the_kept_ones_as_functions: on 20000 random directions the
// dropped margin equals the kept one it was mapped to (the identity is a function identity, not
// a coincidence of the triple's azimuths).
TEST(DPBoundary, DroppedMarginsEqualTheKeptOnesAsFunctions) {
  const Fixture f(Prism());
  for (const WalkFixture& fixture : kWalkFixtures) {
    const std::vector<std::pair<int, int>> pairs = [&] {
      int slots[kMaxFaceCount];
      EXPECT_EQ(ResolveFaceSequence(f.normals, fixture.faces, fixture.count, slots), Status::kOk);
      return IdenticalMargins(f.normals, slots, fixture.count);
    }();
    if (pairs.empty()) {
      continue;
    }
    const DeviationField field = f.Field(fixture.faces, fixture.count, kN131);
    double margins[2 * kMaxFaceCount];
    uint64_t state = 1;  // a tiny deterministic generator, the values not the sequence matter
    for (int k = 0; k < 20000; k++) {
      double u[3] = { static_cast<double>(state % 1009) - 504.0, static_cast<double>((state / 1009) % 997) - 498.0,
                      static_cast<double>((state / 1001803) % 1009) - 504.0 };
      state = state * 6364136223846793005ULL + 1442695040888963407ULL;
      const double norm = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
      if (norm < 1.0) {
        continue;
      }
      for (int i = 0; i < 3; i++) {
        u[i] /= norm;
      }
      field.DomainMarginsAt(u, margins);
      for (const auto& [dropped, kept] : pairs) {
        EXPECT_LE(std::fabs(margins[dropped] - margins[kept]), 1e-14) << fixture.name;
      }
    }
  }
}

// LI test_great_circle_margins_are_decided_per_path: the entry margin always; an internal / exit
// incidence cosine iff m . n_a = 0 (and it checks out on the circle).
TEST(DPBoundary, GreatCircleMarginsAreDecidedPerPath) {
  const Fixture f(Prism());
  const auto circle_names = [&f](const int* faces, int count) {
    const DeviationField field = f.Field(faces, count, kN131);
    std::vector<std::string> out;
    for (const auto& [margin, normal] : GreatCircleMargins(field, 64)) {
      out.push_back(MarginName(count, margin));
    }
    std::sort(out.begin(), out.end());
    return out;
  };
  EXPECT_EQ(circle_names(kWalkFixture35, 2), (std::vector<std::string>{ "entry_incidence_cosine" }));
  EXPECT_EQ(circle_names(kWalkFixture13, 2),
            (std::vector<std::string>{ "entry_incidence_cosine", "exit_incidence_cosine" }));
  EXPECT_EQ(circle_names(kWalkFixture316, 3),
            (std::vector<std::string>{ "entry_incidence_cosine", "internal_1_incidence_cosine" }));
  EXPECT_EQ(circle_names(kWalkFixture132, 3),
            (std::vector<std::string>{ "entry_incidence_cosine", "internal_1_incidence_cosine" }));
  // m = n_5 for the first reflection of 3-5-6-7-3, n_5 . n_3 = -1/2: not a great circle, marched.
  EXPECT_EQ(circle_names(kWalkFixture35673, 5), (std::vector<std::string>{ "entry_incidence_cosine" }));
}

// LI test_entry_margin_vanishes_on_its_great_circle: |entry margin| <= 1e-15 on 3600 points of
// n_a's equator (the closed form the circle branch rests on).
TEST(DPBoundary, EntryMarginVanishesOnItsGreatCircle) {
  const Fixture f(Prism());
  for (const WalkFixture& fixture : kWalkFixtures) {
    const DeviationField field = f.Field(fixture.faces, fixture.count, kN131);
    const double* n_a = field.EntryNormal();
    double basis[2][3];
    TangentBasis(n_a, basis);
    double margins[2 * kMaxFaceCount];
    double worst = 0.0;
    for (int k = 0; k < 3600; k++) {
      const double t = 2.0 * kPi * static_cast<double>(k) / 3600.0;
      double u[3];
      for (int i = 0; i < 3; i++) {
        u[i] = std::cos(t) * basis[0][i] + std::sin(t) * basis[1][i];
      }
      field.DomainMarginsAt(u, margins);
      worst = std::max(worst, std::fabs(margins[kEntryIncidence]));
    }
    EXPECT_LE(worst, 1e-15) << fixture.name;
  }
}

// ---------------------------------------------------------------------------------------------
// The walk
// ---------------------------------------------------------------------------------------------

// (pieces in walk order as margins, from an arbitrary first corner): the loop is compared up to
// rotation (LI EXPECTED_PIECES).
struct ExpectedPieces {
  const char* name;
  std::vector<std::string> margins;
};
const ExpectedPieces kExpectedPieces[] = {
  { "3-5", { "entry_incidence_cosine", "exit_snell_discriminant" } },
  { "1-3", { "entry_incidence_cosine", "exit_snell_discriminant" } },
  // A partial internal reflection keeps the point in U_P: the slabs are lunes of two circles.
  { "3-1-6", { "entry_incidence_cosine", "internal_1_incidence_cosine" } },
  { "1-3-2", { "entry_incidence_cosine", "internal_1_incidence_cosine" } },
  { "3-5-6-7-3",
    { "internal_1_incidence_cosine", "entry_incidence_cosine", "internal_2_incidence_cosine",
      "entry_incidence_cosine" } },
};

bool IsRotationOf(const std::vector<std::string>& got, const std::vector<std::string>& want) {
  const int n = static_cast<int>(want.size());
  for (int shift = 0; shift < n; shift++) {
    bool match = true;
    for (int k = 0; k < n && match; k++) {
      match = got[static_cast<size_t>((k + shift) % n)] == want[static_cast<size_t>(k)];
    }
    if (match) {
      return true;
    }
  }
  return false;
}

// LI test_walk_closes_with_the_expected_pieces: the pieces' margins in walk order (up to
// rotation), the corner count, each piece's endpoints at its corners, the incoming/outgoing
// adjacency, and the kind matching the great-circle table.
TEST(DPBoundary, WalkClosesWithTheExpectedPieces) {
  const Fixture f(Prism());
  for (const ExpectedPieces& expected : kExpectedPieces) {
    int faces[5] = {};
    int count = 0;
    for (const WalkFixture& fixture : kWalkFixtures) {
      if (std::string(fixture.name) == expected.name) {
        count = fixture.count;
        for (int k = 0; k < count; k++) {
          faces[k] = fixture.faces[k];
        }
      }
    }
    if (count <= 0) {
      ADD_FAILURE() << "fixture not found for " << expected.name;
      continue;
    }
    const DeviationField field = f.Field(faces, count, kN131);
    BoundaryWalkRecord record;
    const WalkResult result = WalkBoundary(field, BoundaryWalkOptions{}, &record);
    if (result.status != WalkStatus::kOk) {
      ADD_FAILURE() << result.message << " (" << expected.name << ")";
      continue;
    }
    std::vector<std::string> margins;
    for (const BoundaryPiece& piece : record.pieces) {
      margins.push_back(MarginName(count, piece.margin));
    }
    EXPECT_TRUE(IsRotationOf(margins, expected.margins)) << expected.name;
    if (record.corners.size() != expected.margins.size() || record.pieces.size() != expected.margins.size()) {
      ADD_FAILURE() << expected.name << ": " << record.pieces.size() << " pieces, " << record.corners.size()
                    << " corners, expected " << expected.margins.size();
      continue;
    }
    for (size_t i = 0; i < record.pieces.size(); i++) {
      const BoundaryPiece& piece = record.pieces[i];
      const WalkerCorner& corner = record.corners[i];
      const WalkerCorner& previous = record.corners[i > 0 ? i - 1 : record.corners.size() - 1];
      // Bitwise: the piece's endpoints ARE the corner positions (the walk appends the corner it
      // refined and glues the last point to the first corner) — an angle comparison is not
      // compiler-proof here (an FMA-contracted a1*a2 - a2*a1 is not exactly zero).
      for (int c = 0; c < 3; c++) {
        EXPECT_EQ(piece.points[piece.points.size() - 3 + c], corner.position[c]) << expected.name << " component " << c;
        EXPECT_EQ(piece.points[c], previous.position[c]) << expected.name << " component " << c;
      }
      EXPECT_EQ(corner.incoming, piece.margin) << expected.name;
      EXPECT_EQ(corner.outgoing, record.pieces[(i + 1) % record.pieces.size()].margin) << expected.name;
      EXPECT_EQ(piece.great_circle, std::find(record.great_circles.begin(), record.great_circles.end(), piece.margin) !=
                                        record.great_circles.end())
          << expected.name;
    }
  }
}

// LI test_pieces_stay_on_their_zero_set_inside_the_closure: on its own gate within 2e-15, on the
// closed side of every other gate, coincident margins within kCoincidentAtol.
TEST(DPBoundary, PiecesStayOnTheirZeroSetInsideTheClosure) {
  const Fixture f(Prism());
  double margins[2 * kMaxFaceCount];
  for (const WalkFixture& fixture : kWalkFixtures) {
    const DeviationField field = f.Field(fixture.faces, fixture.count, kN131);
    BoundaryWalkRecord record;
    const WalkResult walk_result = WalkBoundary(field, BoundaryWalkOptions{}, &record);
    if (walk_result.status != WalkStatus::kOk) {
      ADD_FAILURE() << fixture.name << ": " << walk_result.message;
      continue;
    }
    for (const BoundaryPiece& piece : record.pieces) {
      const size_t point_count = piece.points.size() / 3;
      for (size_t k = 0; k < point_count; k++) {
        field.DomainMarginsAt(&piece.points[3 * k], margins);
        EXPECT_LE(std::fabs(margins[piece.margin]), 2e-15) << fixture.name << " point " << k;
        for (int i = 0; i < 2 * fixture.count; i++) {
          if (IsValidityMarginPosition(i, 2 * fixture.count) && i != piece.margin &&
              std::find(piece.coincident.begin(), piece.coincident.end(), i) == piece.coincident.end()) {
            EXPECT_GE(margins[i], -kViolationAtol) << fixture.name << " gate " << i << " point " << k;
          }
        }
        for (int margin : piece.coincident) {
          EXPECT_LE(std::fabs(margins[margin]), kCoincidentAtol) << fixture.name << " point " << k;
        }
      }
    }
  }
}

// LI test_corners_are_exact: the two-margin Newton settles every corner to <= 3e-16 residual with
// no transversal vanishing margin.
TEST(DPBoundary, CornersAreExact) {
  const Fixture f(Prism());
  for (const WalkFixture& fixture : kWalkFixtures) {
    const DeviationField field = f.Field(fixture.faces, fixture.count, kN131);
    BoundaryWalkRecord record;
    const WalkResult walk_result = WalkBoundary(field, BoundaryWalkOptions{}, &record);
    if (walk_result.status != WalkStatus::kOk) {
      ADD_FAILURE() << fixture.name << ": " << walk_result.message;
      continue;
    }
    for (const WalkerCorner& corner : record.corners) {
      // LI pins <= 3e-16 on its JAX/BLAS evaluation path; this port's FMA path lands the two
      // internal_2 corners of 3-5-6-7-3 at 8.9e-16 — both sides are "converged to rounding" of
      // the same Newton, the residual is an evaluation artifact, not a geometric quantity (the
      // corner positions and values themselves match the anchors).
      EXPECT_LE(corner.residual, 1e-15) << fixture.name;
      EXPECT_TRUE(corner.transversal.empty()) << fixture.name;
    }
  }
}

// LI test_corners_on_the_entry_circle_match_a_1d_scan_with_path_domain: independent of the walk —
// a bisection on the entry great circle with the plain gate predicate finds the 3-5 corners.
TEST(DPBoundary, CornersOnTheEntryCircleMatchA1DScanWithPathDomain) {
  const Fixture f(Prism());
  const DeviationField field = f.Field(kWalkFixture35, 2, kN131);
  BoundaryWalkRecord record;
  ASSERT_EQ(WalkBoundary(field, BoundaryWalkOptions{}, &record).status, WalkStatus::kOk);
  ASSERT_EQ(record.corners.size(), 2u);

  const double* n_a = field.EntryNormal();
  double basis[2][3];
  TangentBasis(n_a, basis);
  const auto inside = [&](double t) {
    double u[3];
    for (int i = 0; i < 3; i++) {
      u[i] = std::cos(t) * basis[0][i] + std::sin(t) * basis[1][i] + 1e-9 * n_a[i];
    }
    const double norm = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
    for (int i = 0; i < 3; i++) {
      u[i] /= norm;
    }
    double margins[kMaxFaceCount + 2];
    const int count = field.ValidityMarginsAt(u, margins);
    for (int k = 0; k < count; k++) {
      if (!(margins[k] > 0.0)) {
        return false;
      }
    }
    return true;
  };
  const int grid = 3600;
  std::vector<double> found;
  double lo_flag = inside(0.0);
  for (int k = 0; k < grid; k++) {
    const double t0 = 2.0 * kPi * static_cast<double>(k) / grid;
    const double t1 = 2.0 * kPi * static_cast<double>(k + 1) / grid;
    const bool flag1 = k + 1 == grid ? inside(0.0) : inside(t1);
    if (lo_flag == flag1) {
      continue;
    }
    double lo = t0;
    double hi = t1;
    for (int iteration = 0; iteration < 60; iteration++) {
      const double mid = 0.5 * (lo + hi);
      if (inside(mid) == lo_flag) {
        lo = mid;
      } else {
        hi = mid;
      }
    }
    double u[3];
    for (int i = 0; i < 3; i++) {
      u[i] = std::cos(lo) * basis[0][i] + std::sin(lo) * basis[1][i];
    }
    found.push_back(Angle(u, record.corners[0].position) < Angle(u, record.corners[1].position) ?
                        Angle(u, record.corners[0].position) :
                        Angle(u, record.corners[1].position));
    found[found.size() - 1] = std::min(Angle(u, record.corners[0].position), Angle(u, record.corners[1].position));
    lo_flag = flag1;
  }
  ASSERT_EQ(found.size(), 2u);
  for (double distance : found) {
    EXPECT_LE(distance, 1e-10);
  }
}

// LI test_walker_d_serves_the_closure_limit_at_the_3_5_6_7_corner: the exit-Snell convention on
// 3-5-6-7 at n(550) — a finite branch is off the limit by ~sqrt(disc), and the walk completes
// with the triple-gate corners at 50.16174445450327 deg (the Holder-1/2 scale).
TEST(DPBoundary, WalkerDServesTheClosureLimitAtThe3567Corner) {
  const Fixture f(Prism());
  const int faces[4] = { 3, 5, 6, 7 };
  const DeviationField field = f.Field(faces, 4, kN550);
  const BoundaryWalker walker(field);
  const double u[3] = { 0.0, -0.48947414775668885, 0.8720178086930698 };
  double value = 0.0;
  ASSERT_EQ(walker.D(u, &value), RoutedDeviationStatus::kOk);
  const double limit = field.SampleOptical(u).d_p_exit_limit;
  EXPECT_TRUE(std::isfinite(value));
  EXPECT_NEAR(value, limit, 1e-7);

  BoundaryWalkRecord record;
  const WalkResult result = WalkBoundary(field, BoundaryWalkOptions{}, &record);
  ASSERT_EQ(result.status, WalkStatus::kOk) << result.message;
  EXPECT_EQ(record.pieces.size(), 2u);
  std::vector<std::string> margins;
  for (const BoundaryPiece& piece : record.pieces) {
    margins.push_back(MarginName(4, piece.margin));
  }
  std::sort(margins.begin(), margins.end());
  EXPECT_EQ(margins, (std::vector<std::string>{ "exit_snell_discriminant", "internal_2_incidence_cosine" }));
  ASSERT_EQ(record.corners.size(), 2u);
  for (const WalkerCorner& corner : record.corners) {
    std::vector<std::string> zero;
    for (int margin : corner.margins) {
      zero.push_back(MarginName(4, margin));
    }
    std::sort(zero.begin(), zero.end());
    EXPECT_EQ(zero, (std::vector<std::string>{ "entry_incidence_cosine", "exit_snell_discriminant",
                                               "internal_2_incidence_cosine" }));
    EXPECT_NEAR(Deg(corner.value), 50.16174445450327, 2e-5);
  }
  // The loop's own values stay finite and the minimum is the closure value.
  double minimum = 1e9;
  for (const BoundaryPiece& piece : record.pieces) {
    for (double value_k : piece.values) {
      EXPECT_TRUE(std::isfinite(value_k));
      minimum = std::min(minimum, value_k);
    }
  }
  EXPECT_NEAR(Deg(minimum), 50.16174445450327, 2e-5);
}

// LI test_walker_d_off_the_closure_still_raises: pushed 1e-4 off the middle of the walked
// exit-Snell piece, on the far side of the curve, D refuses (fail closed) while every other gate
// stays positive.
TEST(DPBoundary, WalkerDOffTheClosureStillFails) {
  const Fixture f(Prism());
  const int faces[4] = { 3, 5, 6, 7 };
  const DeviationField field = f.Field(faces, 4, kN550);
  const BoundaryWalker walker(field);
  BoundaryWalkRecord record;
  ASSERT_EQ(WalkBoundary(field, BoundaryWalkOptions{}, &record).status, WalkStatus::kOk);
  const BoundaryPiece* piece = nullptr;
  for (const BoundaryPiece& candidate : record.pieces) {
    if (candidate.margin == ExitSnellOf(4)) {
      piece = &candidate;
    }
  }
  ASSERT_NE(piece, nullptr);
  const double* mid = &piece->points[3 * (piece->points.size() / 6)];
  double g[3];
  walker.TangentGradient(mid, ExitSnellOf(4), g);
  const double g_norm = std::sqrt(g[0] * g[0] + g[1] * g[1] + g[2] * g[2]);
  for (int i = 0; i < 3; i++) {
    g[i] /= g_norm;
  }
  const int exit = ExitSnellOf(4);
  double margins[2 * kMaxFaceCount];
  const auto exit_disc = [&](const double at[3]) {
    walker.Margins(at, margins);
    return margins[exit];
  };
  double probe[3];
  for (int i = 0; i < 3; i++) {
    probe[i] = mid[i] + 1e-6 * g[i];
  }
  const double sign = exit_disc(probe) < 0.0 ? 1.0 : -1.0;
  double out[3];
  for (int i = 0; i < 3; i++) {
    out[i] = mid[i] + sign * 1e-4 * g[i];
  }
  const double norm = std::sqrt(out[0] * out[0] + out[1] * out[1] + out[2] * out[2]);
  for (int i = 0; i < 3; i++) {
    out[i] /= norm;
  }
  walker.Margins(out, margins);
  EXPECT_LT(margins[exit], -1000.0 * kViolationAtol);
  for (int margin : walker.active()) {
    if (margin != exit) {
      EXPECT_GT(margins[margin], 0.0);
    }
  }
  double value = 0.0;
  EXPECT_EQ(walker.D(out, &value), RoutedDeviationStatus::kNotFinite);
}

// LI test_liljequist_corners_carry_every_vanishing_margin: 3-5-6-7-3 — three gates vanish at
// every corner, two bound U_P, exit_snell is coincident, the dropped internal_3 is not listed.
TEST(DPBoundary, LiljequistCornersCarryEveryVanishingMargin) {
  const Fixture f(Prism());
  const DeviationField field = f.Field(kWalkFixture35673, 5, kN131);
  BoundaryWalkRecord record;
  ASSERT_EQ(WalkBoundary(field, BoundaryWalkOptions{}, &record).status, WalkStatus::kOk);
  const int entry = kEntryIncidence;
  const int internal1 = 2;  // internal_1_incidence_cosine
  const int internal2 = 4;  // internal_2_incidence_cosine
  const int exit_snell = ExitSnellOf(5);
  int per_grazing[2] = { 0, 0 };
  for (const WalkerCorner& corner : record.corners) {
    const bool grazing_1 = corner.incoming == internal1 || corner.outgoing == internal1;
    const bool grazing_2 = corner.incoming == internal2 || corner.outgoing == internal2;
    EXPECT_TRUE(grazing_1 != grazing_2);
    EXPECT_TRUE(corner.incoming == entry || corner.outgoing == entry);
    per_grazing[grazing_1 ? 0 : 1]++;
    const auto has = [corner](int margin) {
      return std::find(corner.margins.begin(), corner.margins.end(), margin) != corner.margins.end();
    };
    EXPECT_TRUE(has(entry) && has(grazing_1 ? internal1 : internal2) && has(exit_snell));
    EXPECT_TRUE(corner.tangent.empty());
    EXPECT_TRUE(corner.transversal.empty());
    EXPECT_EQ(corner.coincident, std::vector<int>{ exit_snell });
    for (int margin : corner.margins) {
      EXPECT_EQ(MarginName(5, margin).find("_tir_discriminant"), std::string::npos)
          << "no internal TIR discriminant is a gate";
    }
    EXPECT_FALSE(has(6)) << "internal_3 was dropped before walking";
    EXPECT_NEAR(corner.value, 0.0, 1e-7);
  }
  EXPECT_EQ(per_grazing[0], 2);
  EXPECT_EQ(per_grazing[1], 2);
}

// LI test_liljequist_corner_positions_on_the_entry_circle: the entry circle x = 0 parametrised as
// (0, cos t, sin t) carries the corners at t = +-60.7534083 / +-119.2465917 degrees.
TEST(DPBoundary, LiljequistCornerPositionsOnTheEntryCircle) {
  const Fixture f(Prism());
  const DeviationField field = f.Field(kWalkFixture35673, 5, kN131);
  BoundaryWalkRecord record;
  ASSERT_EQ(WalkBoundary(field, BoundaryWalkOptions{}, &record).status, WalkStatus::kOk);
  std::vector<double> azimuths;
  for (const WalkerCorner& corner : record.corners) {
    azimuths.push_back(Deg(std::atan2(corner.position[2], corner.position[1])));
  }
  std::sort(azimuths.begin(), azimuths.end());
  const double expected[] = { -119.2465917, -60.7534083, 60.7534083, 119.2465917 };
  ASSERT_EQ(azimuths.size(), 4u);
  for (size_t k = 0; k < 4; k++) {
    EXPECT_NEAR(azimuths[k], expected[k], 1e-6);
  }
}

// LI test_walk_accounts_for_every_lattice_edge_point: every lattice point of U_P with an outside
// neighbour among its 6 nearest is next to the walked loop (the completeness spot check).
TEST(DPBoundary, WalkAccountsForEveryLatticeEdgePoint) {
  const Fixture f(Prism());
  const int lattice_n = 20000;
  const double spacing = std::sqrt(4.0 * kPi / lattice_n);
  for (const WalkFixture& fixture : kWalkFixtures) {
    const DeviationField field = f.Field(fixture.faces, fixture.count, kN131);
    BoundaryWalkRecord record;
    const WalkResult walk_result = WalkBoundary(field, BoundaryWalkOptions{}, &record);
    if (walk_result.status != WalkStatus::kOk) {
      ADD_FAILURE() << fixture.name << ": " << walk_result.message;
      continue;
    }
    std::vector<double> loop;
    for (const BoundaryPiece& piece : record.pieces) {
      loop.insert(loop.end(), piece.points.begin(), piece.points.end());
    }
    std::vector<double> lattice(static_cast<size_t>(lattice_n) * 3);
    std::vector<unsigned char> valid(static_cast<size_t>(lattice_n));
    double margins[kMaxFaceCount + 2];
    for (int i = 0; i < lattice_n; i++) {
      double g[3];
      LatticePoint(lattice_n, i, g);
      for (int c = 0; c < 3; c++) {
        lattice[3 * static_cast<size_t>(i) + c] = -g[c];
      }
      const int count = field.ValidityMarginsAt(&lattice[3 * static_cast<size_t>(i)], margins);
      bool inside = true;
      for (int k = 0; k < count; k++) {
        if (!(margins[k] > 0.0)) {
          inside = false;
          break;
        }
      }
      valid[i] = inside ? 1 : 0;
    }
    // For every valid point: is one of its 6 nearest neighbours outside? (bounded insertion scan,
    // the same shape as the partition's ComponentCount.)
    double worst = 0.0;
    for (int i = 0; i < lattice_n; i++) {
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
      bool edge = false;
      for (int k = 0; k < 6; k++) {
        if (!valid[best_j[k]]) {
          edge = true;
          break;
        }
      }
      if (edge) {
        worst = std::max(worst, MinAngleToPoints(p, loop));
      }
    }
    EXPECT_LE(worst, 1.5 * spacing) << fixture.name;
  }
}

// LI test_restricted_extrema_come_in_mirror_pairs: the two-face fixtures are symmetric under the
// mirror through n_a and n_b, so the extrema of the entry and exit pieces pair up in value.
TEST(DPBoundary, RestrictedExtremaComeInMirrorPairs) {
  const Fixture f(Prism());
  const int two_face[2][2] = { { 3, 5 }, { 1, 3 } };
  for (const auto& faces : two_face) {
    const DeviationField field = f.Field(faces, 2, kN131);
    BoundaryWalkRecord record;
    const WalkResult pair_result = WalkBoundary(field, BoundaryWalkOptions{}, &record);
    if (pair_result.status != WalkStatus::kOk) {
      ADD_FAILURE() << pair_result.message;
      continue;
    }
    std::vector<double> values;
    for (const BoundaryCriticalPoint& point : record.data.critical_points) {
      values.push_back(point.value);
    }
    std::sort(values.begin(), values.end());
    if (values.size() % 2 != 0) {
      ADD_FAILURE() << "odd extremum count";
      continue;
    }
    for (size_t k = 0; k < values.size(); k += 2) {
      EXPECT_LE(std::fabs(values[k] - values[k + 1]), 5e-8);  // exit-TIR pieces carry ~1e-8 sqrt error
    }
  }
}

// LI test_walk_zero_set_orientation_reverses_the_walk: from the middle of a dU_P piece,
// orientation +1 ends on the piece's last corner and -1 on its first; the two halves retrace the
// piece. Covers both steppers (1-3-2 is a lune of circles, 3-5-6-7-3 has marched pieces).
TEST(DPBoundary, WalkZeroSetOrientationReversesTheWalk) {
  const Fixture f(Prism());
  const char* with_kinds[] = { "1-3-2", "3-5-6-7-3", "3-5" };
  for (const char* name : with_kinds) {
    const WalkFixture* fixture = nullptr;
    for (const WalkFixture& candidate : kWalkFixtures) {
      if (std::string(candidate.name) == name) {
        fixture = &candidate;
      }
    }
    if (fixture == nullptr) {
      ADD_FAILURE() << "fixture not found for " << name;
      continue;
    }
    const DeviationField field = f.Field(fixture->faces, fixture->count, kN131);
    const BoundaryWalker walker(field);
    BoundaryWalkRecord record;
    const WalkResult orient_result = WalkBoundary(field, BoundaryWalkOptions{}, &record);
    if (orient_result.status != WalkStatus::kOk) {
      ADD_FAILURE() << name << ": " << orient_result.message;
      continue;
    }
    bool saw_circle = false;
    bool saw_marched = false;
    for (const BoundaryPiece& piece : record.pieces) {
      const size_t point_count = piece.points.size() / 3;
      if (point_count < 8) {
        continue;
      }
      const double* middle = &piece.points[3 * (point_count / 2)];
      const ZeroSetWalk forward = WalkZeroSet(walker, middle, piece.margin, 1.0, nullptr, BoundaryWalkOptions{});
      const ZeroSetWalk backward = WalkZeroSet(walker, middle, piece.margin, -1.0, nullptr, BoundaryWalkOptions{});
      EXPECT_EQ(forward.status, WalkStatus::kOk) << forward.message;
      EXPECT_EQ(backward.status, WalkStatus::kOk) << backward.message;
      EXPECT_TRUE(forward.met_corner) << name;
      EXPECT_TRUE(backward.met_corner) << name;
      EXPECT_LT(Angle(forward.corner, &piece.points[piece.points.size() - 3]), 1e-9) << name;
      EXPECT_LT(Angle(backward.corner, &piece.points[0]), 1e-9) << name;
      for (size_t k = 0; k + 2 < forward.points.size(); k += 3) {
        EXPECT_LT(MinAngleToPoints(&forward.points[k], piece.points), 2.0 * kWalkStepRad) << name;
      }
      for (size_t k = 0; k + 2 < backward.points.size(); k += 3) {
        EXPECT_LT(MinAngleToPoints(&backward.points[k], piece.points), 2.0 * kWalkStepRad) << name;
      }
      saw_circle = saw_circle || piece.great_circle;
      saw_marched = saw_marched || !piece.great_circle;
    }
    if (std::string(name) == "1-3-2") {
      EXPECT_TRUE(saw_circle);
      EXPECT_FALSE(saw_marched);
    }
    if (std::string(name) == "3-5-6-7-3") {
      EXPECT_TRUE(saw_marched);
    }
  }
}

// LI test_walk_zero_set_rejects_other_orientations.
TEST(DPBoundary, WalkZeroSetRejectsOtherOrientations) {
  const Fixture f(Prism());
  const DeviationField field = f.Field(kWalkFixture35, 2, kN131);
  const BoundaryWalker walker(field);
  BoundaryWalkRecord record;
  ASSERT_EQ(WalkBoundary(field, BoundaryWalkOptions{}, &record).status, WalkStatus::kOk);
  const BoundaryPiece& piece = record.pieces[0];
  const ZeroSetWalk walk = WalkZeroSet(walker, &piece.points[3], piece.margin, 0.5, nullptr, BoundaryWalkOptions{});
  EXPECT_EQ(walk.status, WalkStatus::kBadOrientation);
  EXPECT_NE(walk.message.find("orientation"), std::string::npos);
}

// LI test_mirror_slab_1_2_1_is_one_constant_crease_loop: dU_P is the entry great circle alone, a
// corner-free loop of constant D_P = 0 (exit_snell = entry^2 on the whole domain, the slab
// identity). The loop's perimeter is an integer multiple of the step (2 pi / step = 1440): the
// exact return is credited only by the distance criterion.
TEST(DPBoundary, MirrorSlab121IsOneConstantCreaseLoop) {
  const Fixture f(Prism());
  const int faces[3] = { 1, 2, 1 };
  const DeviationField field = f.Field(faces, 3, kN131);
  EXPECT_DOUBLE_EQ(2.0 * kPi / kWalkStepRad, 1440.0);
  BoundaryWalkRecord record;
  const WalkResult result = WalkBoundary(field, BoundaryWalkOptions{}, &record);
  ASSERT_EQ(result.status, WalkStatus::kOk) << result.message;
  ASSERT_EQ(record.pieces.size(), 1u);
  const BoundaryPiece& piece = record.pieces[0];
  EXPECT_EQ(piece.points.size() / 3, 1441u);  // 1440 advances + the seed: the exact return adds no duplicate
  EXPECT_EQ(piece.margin, kEntryIncidence);
  EXPECT_TRUE(piece.great_circle);
  ASSERT_TRUE(piece.great_circle);
  EXPECT_NEAR(piece.circle_normal[0], 0.0, 1e-15);
  EXPECT_NEAR(piece.circle_normal[1], 0.0, 1e-15);
  EXPECT_NEAR(std::fabs(piece.circle_normal[2]), 1.0, 1e-15);
  EXPECT_EQ(piece.coincident, std::vector<int>{ ExitSnellOf(3) });
  EXPECT_TRUE(record.corners.empty());
  EXPECT_TRUE(record.data.critical_points.empty());
  EXPECT_TRUE(result.loop.has_plateau);
  EXPECT_NEAR(result.loop.plateau_value, 0.0, 1e-7);
  // The exact return is the closure: first and last point identical.
  for (int i = 0; i < 3; i++) {
    EXPECT_DOUBLE_EQ(piece.points[i], piece.points[piece.points.size() - 3 + i]);
  }
  for (double value : piece.values) {
    EXPECT_LE(std::fabs(value), 1e-7);
  }
}

// LI test_closure_needs_two_steps_of_arc_and_credits_the_exact_return: on the 1-2-1 equator with
// a half-circle step the first advance lands on the antipode (within one step of the seed, where
// only the arc bound holds the walk back) and the second lands back on it an ulp off — the
// closure is credited and the exact seed glued as the endpoint.
TEST(DPBoundary, ClosureNeedsTwoStepsOfArcAndCreditsTheExactReturn) {
  const Fixture f(Prism());
  const int faces[3] = { 1, 2, 1 };
  const DeviationField field = f.Field(faces, 3, kN131);
  const BoundaryWalker walker(field);
  const double start[3] = { 1.0, 0.0, 0.0 };  // a point of the entry equator, u . c = 0
  BoundaryWalkOptions options;
  options.step = kPi;
  const ZeroSetWalk walk = WalkZeroSet(walker, start, kEntryIncidence, 1.0, start, options);
  ASSERT_EQ(walk.status, WalkStatus::kOk) << walk.message;
  EXPECT_FALSE(walk.met_corner);
  ASSERT_EQ(walk.points.size(), 12u);  // seed, antipode, return an ulp off, seed glued
  EXPECT_DOUBLE_EQ(walk.points[0], walk.points[9]);
  EXPECT_DOUBLE_EQ(walk.points[1], walk.points[10]);
  EXPECT_DOUBLE_EQ(walk.points[2], walk.points[11]);
  const double antipode[3] = { -1.0, 0.0, 0.0 };
  EXPECT_LT(Angle(&walk.points[3], antipode), 1e-15);
  EXPECT_LT(Angle(&walk.points[6], start), 1e-15);
  EXPECT_EQ(walk.coincident, std::vector<int>{ ExitSnellOf(3) });
}

// ---------------------------------------------------------------------------------------------
// Plateau extrema of a cyclic sequence (LI _plateau_extrema's tables)
// ---------------------------------------------------------------------------------------------

TEST(DPBoundary, PlateauExtremaRunLengths) {
  struct Row {
    std::vector<double> values;
    std::vector<std::tuple<int, CriticalKind, int>> expected;
  };
  const auto min_max = [](int index, const char* kind, int run) {
    return std::make_tuple(index, kind == std::string("minimum") ? CriticalKind::kMinimum : CriticalKind::kMaximum,
                           run);
  };
  const Row rows[] = {
    // Wrapped 2-sample maximum plateau {8, 0} (seam merge) and an interior 2-sample maximum
    // {4, 5}; the 5s are not extrema.
    { { 7, 5, 3, 5, 7, 7, 5, 3, 7.0 },
      { min_max(0, "maximum", 2), min_max(4, "maximum", 2), min_max(2, "minimum", 1), min_max(7, "minimum", 1) } },
    // Wrapped 2-sample minimum plateau {4, 0}; the strict maxima at 1 and 3 keep run length 1.
    { { 5, 7, 3, 7, 5.0 },
      { min_max(0, "minimum", 2), min_max(1, "maximum", 1), min_max(2, "minimum", 1), min_max(3, "maximum", 1) } },
    // Interior 2-sample plateau maximum away from the seam.
    { { 3, 5, 5, 4.0 }, { min_max(0, "minimum", 1), min_max(1, "maximum", 2) } },
    // Strict extrema only, no wrap merge (first and last values differ).
    { { 1, 3, 2, 3, 0.0 },
      { min_max(1, "maximum", 1), min_max(3, "maximum", 1), min_max(2, "minimum", 1), min_max(4, "minimum", 1) } },
  };
  for (const Row& row : rows) {
    bool is_plateau = true;
    double plateau = 0.0;
    const std::vector<PlateauExtremum> got =
        PlateauExtremaOf(row.values.data(), static_cast<int>(row.values.size()), 1e-7, &is_plateau, &plateau);
    EXPECT_FALSE(is_plateau);
    if (got.size() != row.expected.size()) {
      ADD_FAILURE() << "extremum count " << got.size() << " != " << row.expected.size();
      continue;
    }
    std::vector<std::tuple<int, CriticalKind, int>> actual;
    for (const PlateauExtremum& extremum : got) {
      actual.emplace_back(extremum.index, extremum.kind, extremum.run_length);
    }
    std::sort(actual.begin(), actual.end());
    auto expected = row.expected;
    std::sort(expected.begin(), expected.end());
    EXPECT_EQ(actual, expected);
  }
}

TEST(DPBoundary, PlateauExtremaConstantLoop) {
  bool is_plateau = false;
  double plateau = 0.0;
  const std::vector<double> values = { 4.2, 4.2, 4.2, 4.2 };
  const std::vector<PlateauExtremum> got =
      PlateauExtremaOf(values.data(), static_cast<int>(values.size()), 1e-7, &is_plateau, &plateau);
  EXPECT_TRUE(is_plateau);
  EXPECT_NEAR(plateau, 4.2, 0.0);
  EXPECT_TRUE(got.empty());
}

}  // namespace
}  // namespace lumice::analytic

namespace lumice::analytic {}  // namespace lumice::analytic
