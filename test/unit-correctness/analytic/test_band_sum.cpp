// The band sum (src/analytic/band_sum.hpp, LI docs/band-sum-contract.md sections 4-5) on hand-built
// events and pixels, where every expected number is written down from the contract's formulas:
// containment and the antipode, the left-closed right-open band, the value's constant, the empty
// band, Kish's K_eff, a subnormal contribution, the point mass's pixel and the rank-0 masses. The
// whole call against LI is test_li_parity.cpp (LiParityBandSum).
//
// symmetry_semantics: none — every case is one concrete face sequence (doc/analytic-api.md
// section 3).

#include <gtest/gtest.h>

#include <cfloat>
#include <cmath>
#include <vector>

#include "analytic/band_sum.hpp"
#include "analytic/path_rank.hpp"

namespace lumice::analytic {
namespace {

constexpr double kPi = 3.14159265358979323846;

// Direction at deviation `delta` from s = (0, 0, -1) and azimuth `az` about it.
void AtDeviation(double delta, double az, double out[3]) {
  out[0] = std::sin(delta) * std::cos(az);
  out[1] = std::sin(delta) * std::sin(az);
  out[2] = -std::cos(delta);
}

const double kIncident[3] = { 0.0, 0.0, -1.0 };

// A pixel spanning deviations [lo, hi] and azimuths [a0, a1] about s, corners in cyclic order.
struct Pixel {
  double centre[3];
  double corners[12];
  double solid_angle = 1.0;
};

Pixel Box(double lo, double hi, double a0, double a1) {
  Pixel p;
  AtDeviation(0.5 * (lo + hi), 0.5 * (a0 + a1), p.centre);
  AtDeviation(lo, a0, p.corners);
  AtDeviation(lo, a1, p.corners + 3);
  AtDeviation(hi, a1, p.corners + 6);
  AtDeviation(hi, a0, p.corners + 9);
  return p;
}

struct Table {
  std::vector<double> centre;
  std::vector<double> corners;
  std::vector<double> solid_angle;
  void Add(const Pixel& p) {
    centre.insert(centre.end(), p.centre, p.centre + 3);
    corners.insert(corners.end(), p.corners, p.corners + 12);
    solid_angle.push_back(p.solid_angle);
  }
  PixelTable View() const {
    PixelTable t;
    t.count = static_cast<int>(solid_angle.size());
    t.centre = centre.data();
    t.corners = corners.data();
    t.solid_angle = solid_angle.data();
    return t;
  }
};

SampleEvent Event(int index, double deviation, double weight) {
  SampleEvent e;
  e.event.index = index;
  e.event.deviation = deviation;
  // u and phi are read only by a non-uniform density; any consistent pair will do.
  e.event.u[2] = 1.0;
  e.event.phi[0] = std::sin(deviation);
  e.event.phi[2] = -std::cos(deviation);
  e.weight = weight;
  return e;
}

PoseDensity Random() {
  return PoseDensity(PoseDensitySpec{});
}

TEST(BandSum, ContainmentExcludesTheAntipode) {
  const Pixel p = Box(0.3, 0.4, 0.1, 0.2);
  EXPECT_TRUE(PixelContains(p.corners, p.centre));
  const double antipode[3] = { -p.centre[0], -p.centre[1], -p.centre[2] };
  EXPECT_FALSE(PixelContains(p.corners, antipode));
  double outside[3];
  AtDeviation(0.35, 0.3, outside);
  EXPECT_FALSE(PixelContains(p.corners, outside));
  // A corner and a point on an edge belong to the pixel (zero allowed).
  EXPECT_TRUE(PixelContains(p.corners, p.corners + 3));
}

TEST(BandSum, BandIsLeftClosedRightOpenAndTheValueHasTheContractsConstant) {
  const double lo = 0.30;
  const double hi = 0.34;
  Table table;
  table.Add(Box(lo, hi, 0.0, 0.01));
  // The corner band is [lo, hi] up to the rounding of acos; the events at its ends sit on the band
  // the estimator computes, read back from an empty run.
  std::vector<PixelValue> band;
  BandSumOnEvents({}, 1, kIncident, table.View(), Random(), &band);
  const double band_lo = band[0].delta_lo;
  const double band_hi = band[0].delta_hi;
  const std::vector<SampleEvent> events = {
    Event(0, 0.29, 5.0),     // below
    Event(1, band_lo, 1.0),  // at lo: in
    Event(2, 0.32, 2.0),     // in
    Event(3, 0.33, 3.0),     // in
    Event(4, band_hi, 7.0),  // at hi: out
    Event(5, 0.35, 11.0),    // above
  };
  const int n = 1000;
  std::vector<PixelValue> out;
  BandSumOnEvents(events, n, kIncident, table.View(), Random(), &out);
  ASSERT_EQ(out.size(), 1u);
  const PixelValue& v = out[0];
  EXPECT_EQ(v.status, PixelStatus::kOk);
  EXPECT_NEAR(v.delta_lo, lo, 1e-15);
  EXPECT_NEAR(v.delta_hi, hi, 1e-15);
  EXPECT_EQ(v.k, 3);
  EXPECT_EQ(v.k_rho_pos, 3);
  const double s = 1.0 + 2.0 + 3.0;
  EXPECT_NEAR(v.k_eff, s * s / (1.0 + 4.0 + 9.0), 1e-14);
  const double expected = s / (2.0 * kPi * n * (v.delta_hi - v.delta_lo) * std::sin(v.delta));
  EXPECT_NEAR(v.value / expected, 1.0, 1e-14);
}

TEST(BandSum, EmptyBandIsZeroAndDoesNotDivide) {
  Table table;
  table.Add(Box(0.5, 0.6, 0.0, 0.01));
  std::vector<PixelValue> out;
  BandSumOnEvents({ Event(0, 0.2, 1.0) }, 100, kIncident, table.View(), Random(), &out);
  EXPECT_EQ(out[0].status, PixelStatus::kOk);
  EXPECT_EQ(out[0].k, 0);
  EXPECT_EQ(out[0].value, 0.0);
  EXPECT_EQ(out[0].k_eff, 0.0);
}

TEST(BandSum, PixelsHoldingTheSunOrTheAntisunAreSingular) {
  Table table;
  // Around s (deviation 0) and around -s (deviation pi): the box's corners straddle the pole.
  Pixel sun;
  AtDeviation(0.0, 0.0, sun.centre);
  for (int k = 0; k < 4; k++) {
    AtDeviation(0.02, 0.25 * kPi + 0.5 * kPi * k, sun.corners + 3 * k);
  }
  table.Add(sun);
  Pixel antisun;
  AtDeviation(kPi, 0.0, antisun.centre);
  for (int k = 0; k < 4; k++) {
    AtDeviation(kPi - 0.02, 0.25 * kPi + 0.5 * kPi * k, antisun.corners + 3 * k);
  }
  table.Add(antisun);
  std::vector<PixelValue> out;
  BandSumOnEvents({ Event(0, 0.02, 1.0), Event(1, kPi - 0.02, 1.0) }, 100, kIncident, table.View(), Random(), &out);
  for (const PixelValue& v : out) {
    EXPECT_EQ(v.status, PixelStatus::kSingular);
    EXPECT_TRUE(std::isnan(v.value));
    EXPECT_TRUE(std::isnan(v.k_eff));
    EXPECT_EQ(v.k, 0);
    EXPECT_NEAR(v.delta_lo, v.delta_hi, 1e-12);  // the corner band of a symmetric sun pixel is one ring
  }
  EXPECT_NEAR(out[0].delta, 0.0, 1e-15);
  EXPECT_NEAR(out[1].delta, kPi, 1e-15);
}

// K_rho_pos counts c = w rho > 0 with gradual underflow: a subnormal contribution counts.
TEST(BandSum, SubnormalContributionIsCounted) {
  Table table;
  table.Add(Box(0.3, 0.4, 0.0, 0.01));
  const double tiny = DBL_MIN / 1024.0;
  ASSERT_GT(tiny, 0.0);
  std::vector<PixelValue> out;
  BandSumOnEvents({ Event(0, 0.35, tiny), Event(1, 0.36, 0.0) }, 100, kIncident, table.View(), Random(), &out);
  EXPECT_EQ(out[0].k, 2);
  EXPECT_EQ(out[0].k_rho_pos, 1);
}

// Section 5: the mass goes to the first pixel of the table that contains s, closed test; every
// other pixel is 0 with status ok.
TEST(BandSum, PointMassGoesToTheFirstPixelHoldingTheSun) {
  Table table;
  table.Add(Box(0.3, 0.4, 0.0, 0.1));  // does not hold s
  // Two pixels sharing the edge through s: the second and third rows.
  Pixel left;
  Pixel right;
  const double a[3] = { -0.01, 0.01, -1.0 };
  const double b[3] = { 0.0, 0.01, -1.0 };
  const double c[3] = { 0.0, -0.01, -1.0 };
  const double d[3] = { -0.01, -0.01, -1.0 };
  const double e[3] = { 0.01, 0.01, -1.0 };
  const double f[3] = { 0.01, -0.01, -1.0 };
  auto set = [](Pixel* p, const double* p0, const double* p1, const double* p2, const double* p3, double omega) {
    const double* src[4] = { p0, p1, p2, p3 };
    for (int k = 0; k < 4; k++) {
      double n = std::sqrt(src[k][0] * src[k][0] + src[k][1] * src[k][1] + src[k][2] * src[k][2]);
      for (int i = 0; i < 3; i++) {
        p->corners[3 * k + i] = src[k][i] / n;
      }
    }
    for (int i = 0; i < 3; i++) {
      p->centre[i] = 0.25 * (p->corners[i] + p->corners[3 + i] + p->corners[6 + i] + p->corners[9 + i]);
    }
    p->solid_angle = omega;
  };
  set(&left, a, b, c, d, 2.0);
  set(&right, b, e, f, c, 4.0);
  table.Add(left);
  table.Add(right);
  std::vector<PixelValue> out;
  int pixel = -1;
  PlacePointMass(0.5, kIncident, table.View(), &out, &pixel);
  EXPECT_EQ(pixel, 1);
  ASSERT_EQ(out.size(), 3u);
  EXPECT_EQ(out[0].status, PixelStatus::kOk);
  EXPECT_EQ(out[0].value, 0.0);
  EXPECT_EQ(out[1].status, PixelStatus::kPointMass);
  EXPECT_EQ(out[1].value, 0.5 / 2.0);
  EXPECT_EQ(out[2].status, PixelStatus::kOk);
  EXPECT_EQ(out[2].value, 0.0);
  EXPECT_TRUE(std::isnan(out[2].delta));
}

// The regular prism's 3-6 at n = 1.31, sun at 15 degrees elevation: LI's recorded rank-0 mass under
// the random density (contract section 5: 0.11816635 at N = 1e5), and the psi average of a density
// so wide it is 1 everywhere reproducing the lattice mean.
TEST(BandSum, RankZeroMassesOfThePrismsParallelPair) {
  LUMICE_ANALYTIC_Crystal crystal{};
  crystal.kind = LUMICE_ANALYTIC_CRYSTAL_PRISM;
  crystal.height = 1.0;
  for (double& x : crystal.face_distance) {
    x = 1.0;
  }
  FaceNormalTable table;
  FacePolygonTable polygons;
  ASSERT_EQ(BuildFaceNormals(crystal, &table, &polygons), Status::kOk);
  const int faces[2] = { 3, 6 };
  int slots[2];
  ASSERT_EQ(ResolveFaceSequence(table, faces, 2, slots), Status::kOk);
  ASSERT_TRUE(IsRankZeroPath(table, slots, 2));
  const double incident[3] = { -std::cos(15.0 * kPi / 180.0), 0.0, -std::sin(15.0 * kPi / 180.0) };
  IceDiscovery sampler(table, polygons, slots, 2, 1.31, incident);
  const int n = 100000;
  const std::vector<SampleEvent> events = sampler.BuildEvents(n);
  const PointMass lattice = RankZeroMass(events, n, incident, Random());
  EXPECT_EQ(lattice.method, PointMassMethod::kLatticeMean);
  EXPECT_NEAR(lattice.m, 0.11816635, 5e-9);

  PoseDensitySpec wide;
  wide.family = PoseFamily::kColumn;
  wide.zenith_mean_deg = 90.0;
  wide.zenith_std_deg = 1e9;
  const PointMass psi = RankZeroMass(events, n, incident, PoseDensity(wide));
  EXPECT_EQ(psi.method, PointMassMethod::kPsiAverage);
  EXPECT_NEAR(psi.m / lattice.m, 1.0, 1e-9);
  EXPECT_LE(psi.error, 1e-6 * psi.m);
}


// The psi average against a brute-force oracle built another way: for each event a rotation R_0
// with R_0 u = s_hat by Rodrigues' formula (not the twist frame band_sum.cpp derives), R_psi =
// Rot(s_hat, psi) R_0, rho read off R_psi's third row, and the periodic trapezoid rule on 8192
// points (spectrally accurate for a smooth periodic integrand; the narrowest peak here, sigma = 1
// degree, spans ~50 points). A plate and a Parry density, so both the zenith window's two psi
// intervals and the roll factor are exercised; a sub-sample of the prism's 3-6 events keeps it fast.
void Rotation(const double axis[3], double angle, double r[9]) {
  const double c = std::cos(angle);
  const double s = std::sin(angle);
  const double t = 1.0 - c;
  const double x = axis[0];
  const double y = axis[1];
  const double z = axis[2];
  const double m[9] = { t * x * x + c,     t * x * y - s * z, t * x * z + s * y,  //
                        t * x * y + s * z, t * y * y + c,     t * y * z - s * x,  //
                        t * x * z - s * y, t * y * z + s * x, t * z * z + c };
  for (int i = 0; i < 9; i++) {
    r[i] = m[i];
  }
}

double BruteForcePsiAverage(const double u[3], const double sun[3], const PoseDensity& density) {
  double axis[3] = { u[1] * sun[2] - u[2] * sun[1], u[2] * sun[0] - u[0] * sun[2], u[0] * sun[1] - u[1] * sun[0] };
  const double sin_a = std::sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
  const double cos_a = u[0] * sun[0] + u[1] * sun[1] + u[2] * sun[2];
  double r0[9];
  if (sin_a < 1e-12) {
    const double x[3] = { 1.0, 0.0, 0.0 };
    Rotation(x, cos_a > 0.0 ? 0.0 : kPi, r0);
  } else {
    for (double& a : axis) {
      a /= sin_a;
    }
    Rotation(axis, std::atan2(sin_a, cos_a), r0);
  }
  constexpr int kPoints = 8192;
  double sum = 0.0;
  for (int k = 0; k < kPoints; k++) {
    double twist[9];
    Rotation(sun, 2.0 * kPi * k / kPoints, twist);
    double e[3] = { 0.0, 0.0, 0.0 };
    for (int j = 0; j < 3; j++) {
      for (int l = 0; l < 3; l++) {
        e[j] += twist[6 + l] * r0[3 * l + j];
      }
    }
    sum += density.Evaluate(e[0], e[1], e[2]);
  }
  return sum / kPoints;
}

TEST(BandSum, RankZeroPsiAverageMatchesABruteForceTwist) {
  LUMICE_ANALYTIC_Crystal crystal{};
  crystal.kind = LUMICE_ANALYTIC_CRYSTAL_PRISM;
  crystal.height = 1.0;
  for (double& x : crystal.face_distance) {
    x = 1.0;
  }
  FaceNormalTable table;
  FacePolygonTable polygons;
  ASSERT_EQ(BuildFaceNormals(crystal, &table, &polygons), Status::kOk);
  const int faces[2] = { 3, 6 };
  int slots[2];
  ASSERT_EQ(ResolveFaceSequence(table, faces, 2, slots), Status::kOk);
  const double incident[3] = { -std::cos(15.0 * kPi / 180.0), 0.0, -std::sin(15.0 * kPi / 180.0) };
  const double sun[3] = { -incident[0], -incident[1], -incident[2] };
  IceDiscovery sampler(table, polygons, slots, 2, 1.31, incident);
  const int n = 20000;
  const std::vector<SampleEvent> all = sampler.BuildEvents(n);
  std::vector<SampleEvent> events;
  for (size_t i = 0; i < all.size(); i += 40) {
    events.push_back(all[i]);
  }
  ASSERT_GE(events.size(), 50u);

  PoseDensitySpec plate;
  plate.family = PoseFamily::kPlate;
  plate.zenith_std_deg = 1.0;
  PoseDensitySpec parry;
  parry.family = PoseFamily::kParry;
  parry.zenith_mean_deg = 90.0;
  parry.zenith_std_deg = 3.0;
  parry.roll_std_deg = 5.0;
  for (const PoseDensitySpec& spec : { plate, parry }) {
    const PoseDensity density(spec);
    double expected = 0.0;
    for (const SampleEvent& e : events) {
      expected += e.weight * BruteForcePsiAverage(e.event.u, sun, density);
    }
    expected /= n;
    const PointMass got = RankZeroMass(events, n, incident, density);
    SCOPED_TRACE(static_cast<int>(spec.family));
    EXPECT_GT(expected, 0.0);
    EXPECT_NEAR(got.m / expected, 1.0, 1e-6);
    EXPECT_LE(got.error, 1e-6 * got.m);
  }
}

}  // namespace
}  // namespace lumice::analytic
