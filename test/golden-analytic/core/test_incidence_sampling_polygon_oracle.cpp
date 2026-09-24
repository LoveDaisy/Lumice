// Golden-analytic harness for crystal entry-point incidence sampling (口径 B).
//
// This is the analytic ground-truth gate for the entry-point sampler
// `InitRay_p_fid`. The sampler picks a face area-weighted by projected area and
// then a point uniformly within the chosen face; that target is a closed form of
// the crystal geometry, so its correctness is judged against an analytic oracle
// (test/support/incidence_sampling_oracle.hpp) rather than a frozen reference
// sampler. The gate exists so a later refactor of the sampler (triangle → polygon
// granularity) can be accepted on distribution-invariance without a bit-exact
// fallback.
//
// Discipline this file enforces:
//   * OracleMathSelfProof — the analytic math is verified against hand-computed
//     values FIRST, independent of any sampling code (else "sampler self-proof"
//     would only be the oracle validating its own formulas).
//   * Ac1SelfProof / Ac2SelfProof — the CURRENT production sampler is verified
//     green across prism + pyramid + degenerate face-drop fixtures × several
//     incidence directions. This is the oracle-self-proof precondition: only a
//     sampler-vs-oracle agreement on the existing sampler lets a future change
//     distinguish "sampler correct / oracle blind" from "sampler wrong".
//   * Ac1RedStateCatchesBiasedWeight — an intentionally biased sampler (area
//     weight `|d·n|·A` instead of `max(-d·n·A, 0)`, i.e. dropping the front-face
//     sign) MUST be rejected, proving the comparator has teeth.
//
// The statistical thresholds (k-sigma widths, moment tolerance) are calibrated
// constants: see DISABLED_CalibrationScan below and the comments at each
// constant for the observed margins they were set from.

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include "core/crystal.hpp"
#include "core/geo3d.hpp"
#include "core/math.hpp"
#include "core/shared/pcg_shared.h"
#include "core/simulator.hpp"
#include "support/incidence_sampling_oracle.hpp"

namespace lumice {
namespace {

using test_support::Ac1Verdict;
using test_support::Ac2Verdict;
using test_support::EntrySamples;

// Double-precision pi, local to this file. `M_PI` is not portable (MSVC leaves it
// undefined unless `_USE_MATH_DEFINES` is set before <cmath>), and `math::kPi` is
// float — too coarse for the 1e-12/1e-9 geometric tolerances asserted below.
constexpr double kPiDouble = 3.14159265358979323846;

// ---- Statistical parameters ---------------------------------------------------
// Sample count per (fixture, direction). Large enough that even faces receiving
// a few-percent share get thousands of hits (AC2 needs a stable per-face moment
// estimate). DISABLED_CalibrationScan over 12 seeds at this N observed
// AC1 |z|=4.45, AC2 centroid σ=4.44, AC2 moment σ=2.48; the thresholds below
// carry a safety margin over that envelope.
constexpr size_t kSampleN = 150000;

// AC1 binomial acceptance width. Calibration max |z| over all
// (seed × fixture × direction × face) was 4.45; 6.0 leaves headroom without
// letting the biased sampler (z in the hundreds) slip through.
constexpr double kAc1KSigma = 6.0;

// AC2 centroid per-axis SE combined z-score cap. Calibration max 4.44; 6.5 gives
// margin. In-face centroid must sit on the analytic (fan-mixture) centroid within
// sampling noise.
constexpr double kAc2CentroidKSigma = 6.5;

// AC2 covariance-trace deviation cap, as a count-scaled z-score (relative
// deviation / sqrt(2/count)). Calibration max was 2.48 across 12 seeds (uniform
// distributions have low kurtosis, so the estimator is tighter than the Gaussian
// sqrt(2/count) reference — z stays small); 6.0 gives ample margin while still
// catching a non-uniform in-face distribution (which shifts the trace by many
// sigma at these sample counts).
constexpr double kAc2MomentKSigma = 6.0;

// ---- Fixtures ----------------------------------------------------------------
struct CrystalFixture {
  const char* label;
  Crystal crystal;
};

std::vector<CrystalFixture> MakeFixtures() {
  std::vector<CrystalFixture> fx;
  // Prism: slender / regular / flat — spans the aspect-ratio range.
  fx.push_back({ "prism_h0.2", Crystal::CreatePrism(0.2f) });
  fx.push_back({ "prism_h1.2", Crystal::CreatePrism(1.2f) });
  fx.push_back({ "prism_h5.0", Crystal::CreatePrism(5.0f) });

  // Well-conditioned pyramids (from the closed-form pyramid test's regular pool).
  const float kUnitDist[6] = { 1, 1, 1, 1, 1, 1 };
  fx.push_back({ "pyr_shoulder_a", Crystal::CreatePyramid(28.0f, 28.0f, 0.6f, 1.0f, 0.6f, kUnitDist) });
  fx.push_back({ "pyr_shoulder_b", Crystal::CreatePyramid(60.0f, 30.0f, 1.2f, 0.4f, 0.7f, kUnitDist) });

  // Degenerate face-drop pyramids — reuse the fixed configurations that are
  // known to collapse at least one face (MakeFaceDropBatch in
  // test_closed_form_pyramid.cpp). Not re-invented degenerate numbers.
  // MakeFaceDropBatch has anonymous-namespace (internal) linkage in that
  // translation unit, so it cannot be called from here directly — the
  // dist[] literals below are a hand-copy and MUST be kept numerically in
  // sync with MakeFaceDropBatch's face-drop samples if those ever change.
  const float kDrop0[6] = { 0.3f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
  const float kDrop1[6] = { 1.0f, 0.4f, 1.0f, 0.5f, 1.0f, 1.0f };
  const float kDrop2[6] = { 1.0f, 1.0f, 0.35f, 1.0f, 1.0f, 0.9f };
  fx.push_back({ "pyr_drop_0", Crystal::CreatePyramid(28.0f, 28.0f, 1.0f, 1.0f, 1.0f, kDrop0) });
  fx.push_back({ "pyr_drop_1", Crystal::CreatePyramid(30.0f, 30.0f, 1.0f, 0.5f, 1.0f, kDrop1) });
  fx.push_back({ "pyr_drop_2", Crystal::CreatePyramid(45.0f, 60.0f, 0.8f, 0.6f, 0.8f, kDrop2) });
  return fx;
}

// A fixed, geometry-agnostic direction pool. Directions are filtered per fixture
// by the oracle to those that actually illuminate ≥2 faces, then a spread across
// the max-probability range is chosen (near-normal through near-grazing).
std::vector<std::array<float, 3>> CandidateDirections() {
  std::vector<std::array<float, 3>> dirs;
  const float axes[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
  for (const auto& a : axes) {
    dirs.push_back({ a[0], a[1], a[2] });
  }
  const float s = 1.0f / std::sqrt(3.0f);
  for (int sx = -1; sx <= 1; sx += 2) {
    for (int sy = -1; sy <= 1; sy += 2) {
      for (int sz = -1; sz <= 1; sz += 2) {
        dirs.push_back({ sx * s, sy * s, sz * s });
      }
    }
  }
  // Horizontal directions at offset azimuths — near-grazing on the side faces.
  for (int k = 0; k < 6; k++) {
    const float az = (static_cast<float>(k) * 60.0f + 15.0f) * math::kDegreeToRad;
    dirs.push_back({ std::cos(az), std::sin(az), 0.0f });
  }
  return dirs;
}

// For one fixture, select up to `max_dirs` directions spread across the
// max-probability spectrum, each illuminating ≥2 present faces.
std::vector<std::array<float, 3>> SelectDirections(const Crystal& crystal, size_t max_dirs = 4) {
  const auto pool = CandidateDirections();
  struct Scored {
    std::array<float, 3> d;
    double max_p;
  };
  std::vector<Scored> valid;
  for (const auto& d : pool) {
    const auto probs = test_support::ComputeProjectedFaceAreaDistribution(crystal.CfGeom(), d.data());
    int positive = 0;
    double max_p = 0.0;
    for (double p : probs) {
      if (p > 1e-6) {
        positive++;
      }
      max_p = std::max(max_p, p);
    }
    if (positive >= 2) {
      valid.push_back({ d, max_p });
    }
  }
  std::sort(valid.begin(), valid.end(), [](const Scored& a, const Scored& b) { return a.max_p > b.max_p; });
  std::vector<std::array<float, 3>> out;
  if (valid.empty()) {
    return out;
  }
  const size_t n = valid.size();
  const size_t take = std::min(max_dirs, n);
  for (size_t i = 0; i < take; i++) {
    // Spread indices across [0, n-1] so we cover high-max_p (near-normal) to
    // low-max_p (near-grazing / multi-face).
    const size_t idx = (take == 1) ? 0 : (i * (n - 1)) / (take - 1);
    out.push_back(valid[idx].d);
  }
  return out;
}

// ---- Layer 1 self-proof: analytic math vs hand-computed values ----------------
TEST(IncidenceSamplingOracle, OracleMathSelfProof) {
  namespace ts = test_support;

  // Non-axis-aligned triangle, area = 3 (base 2 on x, height 3 on z).
  {
    const double tri[9] = { 0, 0, 0, 2, 0, 0, 0, 0, 3 };
    EXPECT_NEAR(ts::PolygonArea3D(tri, 3), 3.0, 1e-9);
  }

  // Unit square in the xy-plane: area 1, normal +z, centroid (0.5,0.5),
  // var = 1/12 per axis, zero covariance. Local basis: origin=v0, e1=+x, e2=+y.
  {
    const double sq[12] = { 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0 };
    EXPECT_NEAR(ts::PolygonArea3D(sq, 4), 1.0, 1e-12);
    double n[3];
    ts::PolygonNewellNormal(sq, 4, n);
    EXPECT_NEAR(n[0], 0.0, 1e-12);
    EXPECT_NEAR(n[1], 0.0, 1e-12);
    EXPECT_NEAR(std::abs(n[2]), 1.0, 1e-12);
    ts::FaceBasis2D basis = ts::BuildFaceLocalBasis(sq, 4, n);
    double uv[8];
    for (int k = 0; k < 4; k++) {
      ts::ProjectToLocal2D(sq + k * 3, basis, &uv[k * 2], &uv[k * 2 + 1]);
    }
    ts::Polygon2DMoments m = ts::ComputePolygon2DMoments(uv, 4);
    EXPECT_NEAR(m.area, 1.0, 1e-12);
    EXPECT_NEAR(m.cu, 0.5, 1e-12);
    EXPECT_NEAR(m.cv, 0.5, 1e-12);
    EXPECT_NEAR(m.var_u, 1.0 / 12.0, 1e-12);
    EXPECT_NEAR(m.var_v, 1.0 / 12.0, 1e-12);
    EXPECT_NEAR(m.cov_uv, 0.0, 1e-12);

    // Cross-check the fan path against the whole-polygon path: the unit square
    // fanned from corner 0 is triangles [v0,v1,v2] and [v0,v2,v3]; the
    // equal-area mixture of their per-triangle moments must reproduce the
    // polygon moments (this binds ComputeTriangle2DMoments + the mixture formula
    // to ComputePolygon2DMoments, so a bug in either path is caught).
    const double t0[6] = { uv[0], uv[1], uv[2], uv[3], uv[4], uv[5] };
    const double t1[6] = { uv[0], uv[1], uv[4], uv[5], uv[6], uv[7] };
    ts::Polygon2DMoments m0 = ts::ComputeTriangle2DMoments(t0);
    ts::Polygon2DMoments m1 = ts::ComputeTriangle2DMoments(t1);
    const double w0 = m0.area;
    const double w1 = m1.area;
    const double ws = w0 + w1;
    const double mix_cu = (w0 * m0.cu + w1 * m1.cu) / ws;
    const double mix_cv = (w0 * m0.cv + w1 * m1.cv) / ws;
    const double e_uu = (w0 * (m0.var_u + m0.cu * m0.cu) + w1 * (m1.var_u + m1.cu * m1.cu)) / ws;
    const double e_vv = (w0 * (m0.var_v + m0.cv * m0.cv) + w1 * (m1.var_v + m1.cv * m1.cv)) / ws;
    EXPECT_NEAR(mix_cu, m.cu, 1e-12);
    EXPECT_NEAR(mix_cv, m.cv, 1e-12);
    EXPECT_NEAR(e_uu - mix_cu * mix_cu, m.var_u, 1e-12);
    EXPECT_NEAR(e_vv - mix_cv * mix_cv, m.var_v, 1e-12);
  }

  // Regular unit-circumradius hexagon in the xy-plane. Independent, memorization-
  // free invariants: area = 3√3/2·R², 3D centroid reconstructs to origin, and the
  // in-plane distribution is isotropic (var_u ≈ var_v, cov ≈ 0).
  {
    double hex[18];
    for (int k = 0; k < 6; k++) {
      const double a = static_cast<double>(k) * (kPiDouble / 3.0);
      hex[k * 3 + 0] = std::cos(a);
      hex[k * 3 + 1] = std::sin(a);
      hex[k * 3 + 2] = 0.0;
    }
    EXPECT_NEAR(ts::PolygonArea3D(hex, 6), 3.0 * std::sqrt(3.0) / 2.0, 1e-9);
    double n[3];
    ts::PolygonNewellNormal(hex, 6, n);
    EXPECT_NEAR(std::abs(n[2]), 1.0, 1e-12);
    ts::FaceBasis2D basis = ts::BuildFaceLocalBasis(hex, 6, n);
    double uv[12];
    for (int k = 0; k < 6; k++) {
      ts::ProjectToLocal2D(hex + k * 3, basis, &uv[k * 2], &uv[k * 2 + 1]);
    }
    ts::Polygon2DMoments m = ts::ComputePolygon2DMoments(uv, 6);
    // Reconstruct the 3D centroid from local coords → must be the origin.
    double c3d[3];
    for (int i = 0; i < 3; i++) {
      c3d[i] = basis.origin[i] + m.cu * basis.e1[i] + m.cv * basis.e2[i];
    }
    EXPECT_NEAR(c3d[0], 0.0, 1e-9);
    EXPECT_NEAR(c3d[1], 0.0, 1e-9);
    EXPECT_NEAR(c3d[2], 0.0, 1e-9);
    // Isotropy: equal variances, negligible covariance.
    EXPECT_NEAR(m.var_u, m.var_v, 1e-9);
    EXPECT_NEAR(m.cov_uv, 0.0, 1e-9);
    EXPECT_GT(m.var_u, 0.0);
  }
}

// ---- Fixture sanity: every fixture builds a non-degenerate polyhedron ---------
TEST(IncidenceSamplingOracle, FixturesAreValid) {
  for (auto& f : MakeFixtures()) {
    EXPECT_GE(f.crystal.PolygonFaceCount(), 4u) << f.label << " produced too few faces";
    EXPECT_GE(SelectDirections(f.crystal).size(), 1u) << f.label << " yielded no usable direction";
  }
}

// ---- AC1 self-proof: production sampler matches the projected-area target ------
TEST(IncidenceSamplingOracle, Ac1SelfProof) {
  auto fixtures = MakeFixtures();
  uint32_t seed = 12345;
  for (auto& f : fixtures) {
    const auto dirs = SelectDirections(f.crystal);
    ASSERT_FALSE(dirs.empty()) << f.label;
    for (const auto& d : dirs) {
      EntrySamples samples =
          test_support::DriveEntrySampling(f.crystal, d.data(), kSampleN, seed++, lm_pcg::kEntryKeepFloorCpu);
      Ac1Verdict v = test_support::CheckProjectedAreaDistribution(f.crystal, d.data(), samples.face, kAc1KSigma);
      EXPECT_TRUE(v.pass) << f.label << " dir=(" << d[0] << "," << d[1] << "," << d[2] << ")"
                          << " max|z|=" << v.max_abs_z << " worst_face=" << v.worst_face
                          << " max_rel_dev=" << v.max_relative_dev << " zero_leak=" << v.zero_weight_leak;
    }
  }
}

// ---- AC2 self-proof: production sampler is uniform within each face ------------
TEST(IncidenceSamplingOracle, Ac2SelfProof) {
  auto fixtures = MakeFixtures();
  uint32_t seed = 777;
  for (auto& f : fixtures) {
    const auto dirs = SelectDirections(f.crystal);
    ASSERT_FALSE(dirs.empty()) << f.label;
    for (const auto& d : dirs) {
      EntrySamples samples =
          test_support::DriveEntrySampling(f.crystal, d.data(), kSampleN, seed++, lm_pcg::kEntryKeepFloorCpu);
      Ac2Verdict v =
          test_support::CheckInFaceUniformity(f.crystal, d.data(), samples, kAc2CentroidKSigma, kAc2MomentKSigma);
      EXPECT_TRUE(v.pass) << f.label << " dir=(" << d[0] << "," << d[1] << "," << d[2] << ")"
                          << " max_centroid_sigma=" << v.max_centroid_dev_sigma
                          << " max_moment_sigma=" << v.max_moment_dev_sigma
                          << " max_moment_rel=" << v.max_moment_relative_dev << " worst_face=" << v.worst_face;
    }
  }
}

// ---- Red state: a biased sampler must be caught by the AC1 comparator ----------
//
// The biased sampler exists ONLY here (production `InitRay_p_fid` is untouched).
// It replicates the production loop body but injects the exact bias named in the
// issue: the area weight drops the front-face sign, using |d·n|·A instead of
// max(-d·n·A, 0). This lights back faces that the correct target gives zero
// weight, so the AC1 comparator (built on the true, unmodified oracle) must
// reject it. Every non-weight primitive (RandomSample, SampleTrianglePoint, and
// the cf_geom fan sub-triangles carrying vtx/normal/area/face_id) is the
// production implementation.
EntrySamples SampleEntryPointBiased(const Crystal& crystal, const float d[3], size_t n, uint32_t seed) {
  RandomNumberGenerator::GetInstance().SetSeed(seed);
  const CrystalGeom& cf = crystal.CfGeom();
  std::vector<detail::EntrySubTri> sub(detail::CountEntrySubTris(cf));
  if (!sub.empty()) {
    detail::BuildEntrySubTris(cf, sub.data());
  }
  const size_t total_faces = sub.size();

  std::vector<float> proj_prob(total_faces);
  EntrySamples out;
  out.face.resize(n);
  out.point.resize(n);
  for (size_t i = 0; i < n; i++) {
    for (size_t j = 0; j < total_faces; j++) {
      // BIAS: absolute value instead of max(-dot, 0) — lights back faces too.
      proj_prob[j] = std::abs(Dot3(d, sub[j].n)) * sub[j].area;
    }
    int tri_id = 0;
    RandomSample(static_cast<int>(total_faces), proj_prob.data(), &tri_id);
    float p[3];
    SampleTrianglePoint(sub[tri_id].v, p);
    out.point[i] = { p[0], p[1], p[2] };
    out.face[i] = sub[tri_id].face_id;
  }
  return out;
}

TEST(IncidenceSamplingOracle, Ac1RedStateCatchesBiasedWeight) {
  // Pick a fixture + direction where flipping the sign clearly re-weights face
  // selection: a strongly directional incidence (one face near-normal, its
  // opposite face fully back-lit). A prism with a near-normal side incidence is
  // the cleanest such case — the back side face has target weight 0 but the
  // biased |dot| version gives it the SAME weight as the front face.
  Crystal prism = Crystal::CreatePrism(1.2f);
  const auto dirs = SelectDirections(prism);
  ASSERT_FALSE(dirs.empty());

  // Use the most directional (highest max_p) direction — index 0 after the
  // spread selection is the near-normal one.
  const std::array<float, 3> d = dirs.front();

  // Sanity: the unbiased sampler passes on this same direction (green baseline).
  {
    EntrySamples good = test_support::DriveEntrySampling(prism, d.data(), kSampleN, 2024, lm_pcg::kEntryKeepFloorCpu);
    Ac1Verdict vg = test_support::CheckProjectedAreaDistribution(prism, d.data(), good.face, kAc1KSigma);
    ASSERT_TRUE(vg.pass) << "baseline (unbiased) sampler unexpectedly failed; red-state test is not isolating "
                            "the injected bias. max|z|="
                         << vg.max_abs_z;
  }

  // The biased sampler must be rejected — fed to the SAME comparator + oracle.
  EntrySamples bad = SampleEntryPointBiased(prism, d.data(), kSampleN, 2024);
  Ac1Verdict vb = test_support::CheckProjectedAreaDistribution(prism, d.data(), bad.face, kAc1KSigma);
  EXPECT_FALSE(vb.pass) << "biased sampler slipped past AC1 — comparator has no teeth. max|z|=" << vb.max_abs_z
                        << " zero_leak=" << vb.zero_weight_leak;
  // The bias should be gross, not marginal (far beyond the k-sigma band).
  EXPECT_GT(vb.max_abs_z, 10.0 * kAc1KSigma)
      << "injected bias barely exceeded threshold — pick a more directional case to avoid flakiness";
}

// ---- AC3: projected-area entry weight over the joint (orientation, shape) -----
//
// A crystal of shape g in orientation o intercepts sunlight in proportion to
// its projected area A(o, g, d). Under the equal-surface-area convention that
// folds each instance's own S(g) into its entry weight rather than treating S
// as a shared constant (Lumice shapes carry no absolute size, so a count share
// needs a size convention and this is the one `proportion` documents), the
// weight entering the crystals must be distributed as p(o)·p(g)·A(o, g, d)/S(g)
// — an entry weight A/(S(g)/2) per ray — while the entry sampler used to give
// every ray it was dealt the same weight. The entry supplies the factor through
// the estimator family lm_pcg::entry_acceptance — keep with q = min(1, a/f),
// weight a kept ray by max(a, f), f = 1/c — and every member has the same
// expectation, so these cases run the production sampler at the CPU's own
// member, at the GPU kernels' (c = ∞), and at one in between (c = 2), dealing
// rays from a known p(o)·p(g) — uniformly random orientations (as crystal-local
// directions) on each fixture shape — and judging each member with the same
// oracle and comparator.

constexpr double kAc3KSigma = 5.0;
constexpr size_t kAc3RaysPerShape = 60000;
constexpr int kAc3PolarBins = 8;
// Float accumulation of A and S over <= 64 sub-tris of O(1) area against the
// oracle's double precision: the same 1e-5 AC4's DeviceAcceptProb case pins.
constexpr double kAc3WeightAbsTol = 1e-5;
// The members judged: the CPU's (c = 1), the midpoint (c = 2), the GPU's (c = ∞).
constexpr float kAc3MidKeepFloor = 0.5f;
const std::array<float, 3> kAc3KeepFloors = { lm_pcg::kEntryKeepFloorCpu, kAc3MidKeepFloor,
                                              lm_pcg::kEntryKeepFloorMetal };
static_assert(lm_pcg::kEntryKeepFloorMetal == lm_pcg::kEntryKeepFloorCuda,
              "one GPU member judged here stands for both kernels");

// Uniform directions on the sphere from the test's own generator (normalized
// Gaussian triples), not from the production orientation sampler.
std::vector<std::array<float, 3>> UniformSphereDirections(size_t n, uint32_t seed) {
  std::mt19937 gen(seed);
  std::normal_distribution<double> normal(0.0, 1.0);
  std::vector<std::array<float, 3>> dirs;
  dirs.reserve(n);
  while (dirs.size() < n) {
    const double x = normal(gen);
    const double y = normal(gen);
    const double z = normal(gen);
    const double len = std::sqrt(x * x + y * y + z * z);
    if (len < 1e-12) {
      continue;
    }
    dirs.push_back({ static_cast<float>(x / len), static_cast<float>(y / len), static_cast<float>(z / len) });
  }
  return dirs;
}

// Orientation bin: the direction's polar angle to the crystal c axis, in equal
// |d_z| bands (equal solid angle each) — basal-on at one end, edge-on at the other.
int PolarBin(const std::array<float, 3>& d) {
  const int b = static_cast<int>(std::abs(d[2]) * kAc3PolarBins);
  return std::min(b, kAc3PolarBins - 1);
}

struct Ac3Draw {
  std::vector<int> bin_of;
  std::vector<double> area_ratio;
  std::vector<double> weight;
  std::vector<bool> kept;
  size_t bin_cnt = 0;
};

// Records one fixture's rays: bin (fixture, polar band), the oracle's a_i, and
// the sampler's entry weight and keep verdict (an entry face) per ray.
void AppendFixtureDraw(const CrystalFixture& fixture, size_t fi, const std::vector<std::array<float, 3>>& dirs,
                       const EntrySamples& s, Ac3Draw* out) {
  const auto faces = test_support::BuildPresentFaceGeom(fixture.crystal.CfGeom());
  for (size_t i = 0; i < dirs.size(); i++) {
    const double d[3] = { dirs[i][0], dirs[i][1], dirs[i][2] };
    out->bin_of.push_back(static_cast<int>(fi) * kAc3PolarBins + PolarBin(dirs[i]));
    out->area_ratio.push_back(test_support::ComputeEntryAcceptance(faces, d));
    out->weight.push_back(s.weight[i]);
    out->kept.push_back(s.face[i] != kInvalidId);
  }
}

// Deal kAc3RaysPerShape uniformly oriented rays to every fixture (p(g) uniform
// over the fixtures) through the production sampler at `keep_floor`.
Ac3Draw DrawAcrossFixtures(const std::vector<CrystalFixture>& fixtures, uint32_t seed, float keep_floor) {
  Ac3Draw out;
  out.bin_cnt = fixtures.size() * kAc3PolarBins;
  for (size_t fi = 0; fi < fixtures.size(); fi++) {
    const auto dirs = UniformSphereDirections(kAc3RaysPerShape, seed + static_cast<uint32_t>(fi));
    const EntrySamples s =
        test_support::DriveEntrySamplingDirs(fixtures[fi].crystal, dirs, seed + 1000 + fi, keep_floor);
    AppendFixtureDraw(fixtures[fi], fi, dirs, s, &out);
  }
  return out;
}

test_support::EntryFamilyVerdict JudgeFamily(const Ac3Draw& draw, float keep_floor) {
  return test_support::CheckEntryFamily(draw.bin_of, draw.area_ratio, keep_floor, draw.weight, draw.kept, draw.bin_cnt,
                                        kAc3KSigma, kAc3WeightAbsTol);
}

void ExpectEntryFamilyPass(const std::vector<CrystalFixture>& fixtures, const Ac3Draw& draw, float keep_floor) {
  const auto v = JudgeFamily(draw, keep_floor);
  for (size_t k = 0; k < v.bins.size(); k++) {
    const auto& b = v.bins[k];
    EXPECT_LE(std::max(std::abs(b.kept_z), std::abs(b.weight_z)), kAc3KSigma)
        << "keep_floor " << keep_floor << ", " << fixtures[k / kAc3PolarBins].label << " polar band "
        << k % kAc3PolarBins << ": kept " << b.kept << " of " << b.dealt << " (expected " << b.kept_expected << "), Σw "
        << b.weight_observed << " (oracle Σa " << b.weight_expected << ")";
  }
  EXPECT_TRUE(v.pass) << "keep_floor " << keep_floor << ": max|w-m|=" << v.max_weight_dev << " at ray " << v.worst_ray
                      << ", max|z|=" << v.max_abs_z << " at bin " << v.worst_bin;
}

TEST(IncidenceSamplingOracle, Ac3EntryFamilyFollowsProjectedAreaOverOrientationAndShape) {
  const auto fixtures = MakeFixtures();
  for (const float keep_floor : kAc3KeepFloors) {
    ExpectEntryFamilyPass(fixtures, DrawAcrossFixtures(fixtures, 31337, keep_floor), keep_floor);
  }
}

// The same members, judged by a route that shares nothing with the oracle's
// geometry: Cauchy's surface-area formula says a convex body's projected area
// averaged over uniform orientations is S/4, so a uniformly oriented ray's mean
// entry contribution (its weight if kept, 0 if discarded) is (S/4) / (S/2) = 1/2
// on EVERY convex shape and for every member. A per-shape mean away from 1/2
// means the sampler's A or S is wrong, whatever the oracle says.
TEST(IncidenceSamplingOracle, Ac3UniformOrientationMeanWeightIsHalfOnEveryConvexShape) {
  const auto fixtures = MakeFixtures();
  for (const float keep_floor : kAc3KeepFloors) {
    for (size_t fi = 0; fi < fixtures.size(); fi++) {
      const auto dirs = UniformSphereDirections(kAc3RaysPerShape, 4242 + static_cast<uint32_t>(fi));
      const EntrySamples s = test_support::DriveEntrySamplingDirs(fixtures[fi].crystal, dirs, 5151 + fi, keep_floor);
      double sum_w = 0.0;
      for (size_t i = 0; i < dirs.size(); i++) {
        sum_w += s.face[i] != kInvalidId ? s.weight[i] : 0.0;
      }
      const double n = static_cast<double>(dirs.size());
      // Each contribution lies in [0, 1] (max(a, f) <= 1): Var <= 1/4 bounds the spread.
      EXPECT_NEAR(sum_w / n, 0.5, kAc3KSigma * std::sqrt(0.25 / n))
          << "keep_floor " << keep_floor << ", " << fixtures[fi].label;
    }
  }
}

// Red states on the same dealt rays: the comparator must reject, at every
// member, the sampler that gave every ray the same weight (before the
// projected-area factor existed); and it must tell the members apart — the
// rays of one member judged as another fail, so a passing check pins which
// member ran, not only the shared expectation.
TEST(IncidenceSamplingOracle, Ac3RedStateCatchesUnitWeightAndWrongMember) {
  const auto fixtures = MakeFixtures();
  for (const float keep_floor : kAc3KeepFloors) {
    Ac3Draw unit = DrawAcrossFixtures(fixtures, 31337, keep_floor);
    std::fill(unit.weight.begin(), unit.weight.end(), 1.0);
    std::fill(unit.kept.begin(), unit.kept.end(), true);
    const auto vu = JudgeFamily(unit, keep_floor);
    EXPECT_FALSE(vu.pass) << "keep_floor " << keep_floor;
    EXPECT_GT(vu.max_abs_z, 10.0 * kAc3KSigma)
        << "keep_floor " << keep_floor << ": unit weight should be a gross departure, not a marginal one";
  }
  const Ac3Draw all_kept = DrawAcrossFixtures(fixtures, 31337, lm_pcg::kEntryKeepFloorMetal);
  const auto vw = JudgeFamily(all_kept, lm_pcg::kEntryKeepFloorCpu);
  EXPECT_FALSE(vw.pass) << "c = ∞ rays passed as c = 1 — the comparator does not pin the member";
  const Ac3Draw discarding = DrawAcrossFixtures(fixtures, 31337, lm_pcg::kEntryKeepFloorCpu);
  const auto vd = JudgeFamily(discarding, lm_pcg::kEntryKeepFloorMetal);
  EXPECT_FALSE(vd.pass) << "c = 1 rays passed as c = ∞ — the comparator does not pin the member";
}

// ---- AC4: A(o, g, d) <= S(g) / 2 for every convex shape and direction ----------
//
// The entry weight divides by S/2 and clamps at 1; the clamp must never bind, or
// the entered weight stops being proportional to A. It never does for a
// convex body: its lit and unlit sides project onto the same shadow, each with
// area A, and together they are its whole surface, so 2A <= S. This pins that
// argument mechanically over the fixtures plus the extremes (a thin plate
// seen face-on is where A comes closest to S/2), and checks the production
// entry table's S against the oracle's.
TEST(IncidenceSamplingOracle, Ac4ProjectedAreaNeverExceedsHalfTheSurface) {
  auto fixtures = MakeFixtures();
  fixtures.push_back({ "prism_h0.05", Crystal::CreatePrism(0.05f) });
  fixtures.push_back({ "prism_h10", Crystal::CreatePrism(10.0f) });
  auto dirs = UniformSphereDirections(4000, 9001);
  for (const auto& d : CandidateDirections()) {
    dirs.push_back(d);
  }
  for (const auto& f : fixtures) {
    const CrystalGeom& cf = f.crystal.CfGeom();
    const double s = test_support::ComputeSurfaceArea(cf);
    if (s <= 0.0) {
      ADD_FAILURE() << f.label << ": empty fixture";
      continue;
    }

    std::vector<detail::EntrySubTri> sub(detail::CountEntrySubTris(cf));
    detail::BuildEntrySubTris(cf, sub.data());
    double s_prod = 0.0;
    for (const auto& t : sub) {
      s_prod += t.area;
    }
    EXPECT_NEAR(s_prod, s, 1e-5 * s) << f.label << ": the sampler's S disagrees with the oracle's";

    const auto faces = test_support::BuildPresentFaceGeom(cf);
    double max_ratio = 0.0;
    std::array<float, 3> worst_d{};
    for (const auto& dv : dirs) {
      const double d[3] = { dv[0], dv[1], dv[2] };
      const double ratio = test_support::ComputeProjectedArea(faces, d) / (0.5 * s);
      if (ratio > max_ratio) {
        max_ratio = ratio;
        worst_d = dv;
      }
    }
    EXPECT_LE(max_ratio, 1.0 + 1e-9) << f.label << " worst d=(" << worst_d[0] << "," << worst_d[1] << "," << worst_d[2]
                                     << ")";
    if (std::string(f.label) == "prism_h0.05") {
      // The face-on plate must come close to the bound, or this case is not
      // exercising the edge it claims to: there A is the basal area B and S is
      // 2B plus the thin side band, so A/(S/2) = B/(B + side/2) ≈ 0.899.
      EXPECT_GT(max_ratio, 0.85) << f.label;
    }
  }
}

// ---- Calibration scan (disabled; run manually to (re)derive thresholds) -------
// Run with: golden_analytic_test --gtest_also_run_disabled_tests \
//   --gtest_filter='*DISABLED_CalibrationScan*'
// Prints the max AC1 |z|, AC2 centroid sigma, and AC2 moment relative deviation
// across several seeds × fixtures × directions, so the constants above can be set
// with margin over the observed sampling-noise envelope.
TEST(IncidenceSamplingOracle, DISABLED_CalibrationScan) {
  auto fixtures = MakeFixtures();
  double max_ac1_z = 0.0;
  double max_ac2_centroid = 0.0;
  double max_ac2_moment_sigma = 0.0;
  double max_ac2_moment_rel = 0.0;
  const std::vector<uint32_t> seeds = { 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233 };
  for (uint32_t seed : seeds) {
    for (auto& f : fixtures) {
      const auto dirs = SelectDirections(f.crystal);
      for (const auto& d : dirs) {
        EntrySamples s =
            test_support::DriveEntrySampling(f.crystal, d.data(), kSampleN, seed, lm_pcg::kEntryKeepFloorCpu);
        // Use a very loose threshold so pass/fail never short-circuits the scan.
        Ac1Verdict v1 = test_support::CheckProjectedAreaDistribution(f.crystal, d.data(), s.face, 1e9);
        Ac2Verdict v2 = test_support::CheckInFaceUniformity(f.crystal, d.data(), s, 1e9, 1e9);
        max_ac1_z = std::max(max_ac1_z, v1.max_abs_z);
        max_ac2_centroid = std::max(max_ac2_centroid, v2.max_centroid_dev_sigma);
        max_ac2_moment_sigma = std::max(max_ac2_moment_sigma, v2.max_moment_dev_sigma);
        max_ac2_moment_rel = std::max(max_ac2_moment_rel, v2.max_moment_relative_dev);
      }
    }
  }
  std::printf(
      "[calibration] max AC1 |z|=%.3f  max AC2 centroid sigma=%.3f  max AC2 moment sigma=%.3f  max AC2 moment "
      "rel=%.5f\n",
      max_ac1_z, max_ac2_centroid, max_ac2_moment_sigma, max_ac2_moment_rel);
}

// ================================================================================
// CPU-only white-box gate for the GPU entry-sampler. The GPU backends
// (Metal/CUDA) feed their device entry sampler a triangle SoA built on the fly
// from cf_geom_ via detail::BuildEntrySubTris (the same helper CPU
// InitRay_p_fid uses). This section proves the device selection math is correct
// without any GPU hardware:
//   DeviceSampler{Ac1,Ac2,RedState} — the ACTUAL device sampling routines
//   (lm_pcg::sample_triangle + lm_pcg::categorical_sample from
//   src/core/shared/pcg_shared.h, the exact code the gen_root/transit kernels
//   run) driven over that SoA reproduce the analytic projected-area target
//   (AC1) + in-face uniformity (AC2), and a biased weight is still rejected
//   (teeth): "if the upload feeds the right data, the device selection math is
//   analytically correct."
//   DeviceSampler{Ac3,UniformOrientationMeanWeightIsHalf,RedStateCatchesUnitWeight}
//   and DeviceAcceptProbMatchesOracleAndStaysBelowOne — the same for the
//   projected-area entry weight the kernels multiply in
//   (lm_pcg::entry_acceptance at kEntryKeepFloorMetal / kEntryKeepFloorCuda).
// ================================================================================

// SoA layout identical to the device geometry pool: tri_vtx[9*T] / tri_norm[3*T]
// / tri_area[T] / tri_to_poly[T]. Built from cf_geom_ exactly as the GPU host
// upload will (BuildEntrySubTris → scatter), so a bug in either would surface
// here before it reaches the GPU.
struct DeviceGeomSoA {
  std::vector<float> tri_vtx;
  std::vector<float> tri_norm;
  std::vector<float> tri_area;
  std::vector<uint16_t> tri_to_poly;
  size_t tri_cnt = 0;
};

DeviceGeomSoA BuildDeviceGeomSoA(const Crystal& crystal) {
  const CrystalGeom& cf = crystal.CfGeom();
  DeviceGeomSoA g;
  g.tri_cnt = detail::CountEntrySubTris(cf);
  std::vector<detail::EntrySubTri> sub(g.tri_cnt);
  detail::BuildEntrySubTris(cf, sub.data());
  g.tri_vtx.resize(9 * g.tri_cnt);
  g.tri_norm.resize(3 * g.tri_cnt);
  g.tri_area.resize(g.tri_cnt);
  g.tri_to_poly.resize(g.tri_cnt);
  for (size_t t = 0; t < g.tri_cnt; t++) {
    std::memcpy(g.tri_vtx.data() + 9 * t, sub[t].v, 9 * sizeof(float));
    std::memcpy(g.tri_norm.data() + 3 * t, sub[t].n, 3 * sizeof(float));
    g.tri_area[t] = sub[t].area;
    // face_id is the compact present-face id (0..PolygonFaceCount()-1), always in
    // uint16 range (kCrystalGeomMaxFaces=20) — never the kInvalidId sentinel the
    // Mesh-only path could emit, so no widening/guard is needed here.
    g.tri_to_poly[t] = static_cast<uint16_t>(sub[t].face_id);
  }
  return g;
}

// Device entry-sampler variants the red states inject. kNone is the kernel as
// shipped; the others each break one thing the comparators must notice.
enum class DeviceFault {
  kNone,
  kUnsignedWeight,  // |d·n|·A instead of max(-d·n·A, 0): the AC1 red state
  kUnitWeight,      // skip the projected-area entry weight: the AC3 red state
};

// Replays the exact device entry-sampler body for ONE ray (cuda_trace_backend.cu
// gen_root_kernel §3–4, transit_root_kernel §3–5, and their lumice_trace.metal
// siblings): per-tri projected weight max(-d·n·A, 0) summed into A alongside
// S = Σ area, one categorical_sample, one sample_triangle in the chosen sub-tri,
// and the entry weight entry_acceptance(entry_area_ratio(A, S), kEntryKeepFloorMetal)
// on a unit birth weight (keep_prob ≡ 1 at that floor, so no draw). Every
// routine is the shared lm_pcg:: one the kernels call, not a re-implementation.
void DeviceEntrySampleOne(const DeviceGeomSoA& g, const float d[3], uint32_t seed, uint32_t global_idx,
                          DeviceFault fault, std::vector<float>* proj, IdType* face, std::array<float, 3>* point,
                          float* weight) {
  lm_pcg::PcgStream s;
  s.seed = seed;
  s.global_idx = global_idx;
  s.slot = 0u;
  float proj_sum = 0.0f;
  float s_total = 0.0f;
  for (size_t t = 0; t < g.tri_cnt; t++) {
    const float dot = d[0] * g.tri_norm[3 * t + 0] + d[1] * g.tri_norm[3 * t + 1] + d[2] * g.tri_norm[3 * t + 2];
    (*proj)[t] =
        fault == DeviceFault::kUnsignedWeight ? std::abs(dot) * g.tri_area[t] : std::max(-dot * g.tri_area[t], 0.0f);
    proj_sum += (*proj)[t];
    s_total += g.tri_area[t];
  }
  const float u_cat = lm_pcg::pcg_uniform(s);
  const uint32_t tri_id = lm_pcg::categorical_sample(proj->data(), static_cast<uint32_t>(g.tri_cnt), u_cat);
  *weight = fault == DeviceFault::kUnitWeight ?
                1.0f :
                lm_pcg::entry_acceptance(lm_pcg::entry_area_ratio(proj_sum, s_total), lm_pcg::kEntryKeepFloorMetal)
                    .weight_mult;
  if (g.tri_cnt == 0) {
    *face = kInvalidId;
    *point = { 0.0f, 0.0f, 0.0f };
    *weight = 0.0f;
    return;
  }
  float p[3];
  lm_pcg::sample_triangle(s, g.tri_vtx.data() + tri_id * 9u, p);
  *point = { p[0], p[1], p[2] };
  *face = static_cast<IdType>(g.tri_to_poly[tri_id]);
}

// n rays along one fixed direction, one independent PCG stream each (distinct
// global_idx), matching the device's per-thread stream construction well
// enough for a statistical test.
EntrySamples DriveEntrySamplingDevice(const Crystal& crystal, const float d[3], size_t n, uint32_t seed,
                                      DeviceFault fault = DeviceFault::kNone) {
  const DeviceGeomSoA g = BuildDeviceGeomSoA(crystal);
  EntrySamples out;
  out.face.resize(n);
  out.point.resize(n);
  out.weight.resize(n);
  std::vector<float> proj(g.tri_cnt);
  for (size_t i = 0; i < n; i++) {
    DeviceEntrySampleOne(g, d, seed, static_cast<uint32_t>(i), fault, &proj, &out.face[i], &out.point[i],
                         &out.weight[i]);
  }
  return out;
}

// One ray per direction (the AC3 shape: many orientations, one ray each).
EntrySamples DriveEntrySamplingDeviceDirs(const Crystal& crystal, const std::vector<std::array<float, 3>>& dirs,
                                          uint32_t seed, DeviceFault fault = DeviceFault::kNone) {
  const DeviceGeomSoA g = BuildDeviceGeomSoA(crystal);
  EntrySamples out;
  out.face.resize(dirs.size());
  out.point.resize(dirs.size());
  out.weight.resize(dirs.size());
  std::vector<float> proj(g.tri_cnt);
  for (size_t i = 0; i < dirs.size(); i++) {
    DeviceEntrySampleOne(g, dirs[i].data(), seed, static_cast<uint32_t>(i), fault, &proj, &out.face[i], &out.point[i],
                         &out.weight[i]);
  }
  return out;
}

// ---- Device sampling math: AC1 (face distribution) -----------------------------
TEST(DeviceSamplingPolygonOracle, DeviceSamplerMathPassesAc1) {
  auto fixtures = MakeFixtures();
  uint32_t seed = 54321;
  for (auto& f : fixtures) {
    const auto dirs = SelectDirections(f.crystal);
    ASSERT_FALSE(dirs.empty()) << f.label;
    for (const auto& d : dirs) {
      EntrySamples s = DriveEntrySamplingDevice(f.crystal, d.data(), kSampleN, seed++);
      Ac1Verdict v = test_support::CheckProjectedAreaDistribution(f.crystal, d.data(), s.face, kAc1KSigma);
      EXPECT_TRUE(v.pass) << f.label << " dir=(" << d[0] << "," << d[1] << "," << d[2] << ")"
                          << " max|z|=" << v.max_abs_z << " worst_face=" << v.worst_face
                          << " zero_leak=" << v.zero_weight_leak;
    }
  }
}

// ---- (b) Device sampling math: AC2 (in-face uniformity) ------------------------
TEST(DeviceSamplingPolygonOracle, DeviceSamplerMathPassesAc2) {
  auto fixtures = MakeFixtures();
  uint32_t seed = 24680;
  for (auto& f : fixtures) {
    const auto dirs = SelectDirections(f.crystal);
    ASSERT_FALSE(dirs.empty()) << f.label;
    for (const auto& d : dirs) {
      EntrySamples s = DriveEntrySamplingDevice(f.crystal, d.data(), kSampleN, seed++);
      Ac2Verdict v = test_support::CheckInFaceUniformity(f.crystal, d.data(), s, kAc2CentroidKSigma, kAc2MomentKSigma);
      EXPECT_TRUE(v.pass) << f.label << " dir=(" << d[0] << "," << d[1] << "," << d[2] << ")"
                          << " max_centroid_sigma=" << v.max_centroid_dev_sigma
                          << " max_moment_sigma=" << v.max_moment_dev_sigma << " worst_face=" << v.worst_face;
    }
  }
}

// ---- (b) Red state: biased device weight must be caught (comparator teeth) ------
TEST(DeviceSamplingPolygonOracle, DeviceSamplerRedStateCatchesBias) {
  Crystal prism = Crystal::CreatePrism(1.2f);
  const auto dirs = SelectDirections(prism);
  ASSERT_FALSE(dirs.empty());
  const std::array<float, 3> d = dirs.front();

  // Green baseline: the unbiased device sampler passes on this same direction.
  {
    EntrySamples good = DriveEntrySamplingDevice(prism, d.data(), kSampleN, 2024);
    Ac1Verdict vg = test_support::CheckProjectedAreaDistribution(prism, d.data(), good.face, kAc1KSigma);
    ASSERT_TRUE(vg.pass) << "unbiased device sampler unexpectedly failed; red-state not isolating the bias. max|z|="
                         << vg.max_abs_z;
  }
  // Biased device sampler (|d·n|·A) — same comparator + oracle → must reject.
  EntrySamples bad = DriveEntrySamplingDevice(prism, d.data(), kSampleN, 2024, DeviceFault::kUnsignedWeight);
  Ac1Verdict vb = test_support::CheckProjectedAreaDistribution(prism, d.data(), bad.face, kAc1KSigma);
  EXPECT_FALSE(vb.pass) << "biased device sampler slipped past AC1 — comparator has no teeth. max|z|=" << vb.max_abs_z;
  EXPECT_GT(vb.max_abs_z, 10.0 * kAc1KSigma)
      << "injected bias barely exceeded threshold — pick a more directional case";
}


// ---- Device sampling math: AC3 (projected-area entry weight) --------------------
// Same draw, bins and comparator as IncidenceSamplingOracle.Ac3* above, with the
// device body in place of InitRay_p_fid: every fixture gets uniformly oriented
// rays, binned by (fixture, polar band), and each ray's entry weight is judged
// against the oracle's A / (S/2).
Ac3Draw DrawAcrossFixturesDevice(const std::vector<CrystalFixture>& fixtures, uint32_t seed, DeviceFault fault) {
  Ac3Draw out;
  out.bin_cnt = fixtures.size() * kAc3PolarBins;
  for (size_t fi = 0; fi < fixtures.size(); fi++) {
    const auto dirs = UniformSphereDirections(kAc3RaysPerShape, seed + static_cast<uint32_t>(fi));
    const EntrySamples s =
        DriveEntrySamplingDeviceDirs(fixtures[fi].crystal, dirs, seed + 1000 + static_cast<uint32_t>(fi), fault);
    AppendFixtureDraw(fixtures[fi], fi, dirs, s, &out);
  }
  return out;
}

TEST(DeviceSamplingPolygonOracle, DeviceSamplerMathPassesAc3) {
  const auto fixtures = MakeFixtures();
  ExpectEntryFamilyPass(fixtures, DrawAcrossFixturesDevice(fixtures, 27182, DeviceFault::kNone),
                        lm_pcg::kEntryKeepFloorMetal);
}

// Cauchy route, sharing nothing with the oracle's geometry: uniformly oriented
// rays enter with mean weight (S/4)/(S/2) = 1/2 on every convex shape.
TEST(DeviceSamplingPolygonOracle, DeviceSamplerUniformOrientationMeanWeightIsHalf) {
  const auto fixtures = MakeFixtures();
  for (size_t fi = 0; fi < fixtures.size(); fi++) {
    const auto dirs = UniformSphereDirections(kAc3RaysPerShape, 1618 + static_cast<uint32_t>(fi));
    const EntrySamples s = DriveEntrySamplingDeviceDirs(fixtures[fi].crystal, dirs, 3141 + static_cast<uint32_t>(fi));
    const double sum_w = std::accumulate(s.weight.begin(), s.weight.end(), 0.0);
    const double n = static_cast<double>(dirs.size());
    EXPECT_NEAR(sum_w / n, 0.5, kAc3KSigma * std::sqrt(0.25 / n)) << fixtures[fi].label;
  }
}

// Red state: the device body without the entry weight (the pre-fix kernels)
// must be rejected by the same comparator on the same dealt rays.
TEST(DeviceSamplingPolygonOracle, DeviceSamplerRedStateCatchesUnitWeight) {
  const auto fixtures = MakeFixtures();
  const Ac3Draw draw = DrawAcrossFixturesDevice(fixtures, 27182, DeviceFault::kUnitWeight);
  const auto v = JudgeFamily(draw, lm_pcg::kEntryKeepFloorMetal);
  EXPECT_FALSE(v.pass);
  EXPECT_GT(v.max_weight_dev, 0.1) << "unit weight should be a gross departure, not a marginal one";
}

// ---- Device sampling math: AC4 (the entry weight itself) ---------------------------
// The kernels accumulate A and S in float over the device triangle SoA and call
// entry_area_ratio; this pins that number against the oracle's double-precision
// A / (S/2) per direction, and that it never reaches the clamp at 1 — including
// on the thin face-on plate, where A comes closest to S/2.
TEST(DeviceSamplingPolygonOracle, DeviceAcceptProbMatchesOracleAndStaysBelowOne) {
  auto fixtures = MakeFixtures();
  fixtures.push_back({ "prism_h0.05", Crystal::CreatePrism(0.05f) });
  fixtures.push_back({ "prism_h10", Crystal::CreatePrism(10.0f) });
  auto dirs = UniformSphereDirections(4000, 1123);
  for (const auto& d : CandidateDirections()) {
    dirs.push_back(d);
  }
  for (const auto& f : fixtures) {
    const DeviceGeomSoA g = BuildDeviceGeomSoA(f.crystal);
    const auto faces = test_support::BuildPresentFaceGeom(f.crystal.CfGeom());
    double max_err = 0.0;
    double max_prob = 0.0;
    for (const auto& dv : dirs) {
      float proj_sum = 0.0f;
      float s_total = 0.0f;
      for (size_t t = 0; t < g.tri_cnt; t++) {
        const float dot = dv[0] * g.tri_norm[3 * t + 0] + dv[1] * g.tri_norm[3 * t + 1] + dv[2] * g.tri_norm[3 * t + 2];
        proj_sum += std::max(-dot * g.tri_area[t], 0.0f);
        s_total += g.tri_area[t];
      }
      const double prob = lm_pcg::entry_area_ratio(proj_sum, s_total);
      const double d[3] = { dv[0], dv[1], dv[2] };
      max_err = std::max(max_err, std::abs(prob - test_support::ComputeEntryAcceptance(faces, d)));
      max_prob = std::max(max_prob, prob);
    }
    // Float accumulation over <= 64 sub-tris of O(1) area: 1e-5 absolute is
    // ~100 ulp of a weight in [0, 1]; kAc3WeightAbsTol is set to it.
    EXPECT_LT(max_err, 1e-5) << f.label;
    EXPECT_LT(max_prob, 1.0) << f.label << ": the clamp bound; entered weight would stop following A";
  }
  // Empty shape: zero weight.
  EXPECT_EQ(lm_pcg::entry_area_ratio(0.0f, 0.0f), 0.0f);
}

// ---- The estimator family's two ends and its empty-shape edge -------------------
// keep_floor 1 (c = 1) is plain accept/reject: keep with probability a, weight
// untouched — exactly 1.0f, so a kept CPU ray carries its birth weight bit for
// bit. keep_floor 0 (c = ∞) keeps every ray at weight exactly a. A zero ratio is
// never kept at any positive floor, and at floor 0 it is kept at weight 0.
TEST(EntryAcceptanceFamily, EndsAreAcceptRejectAndPureWeight) {
  for (const float a : { 0.0f, 0.1f, 0.37f, 0.5f, 0.899f, 1.0f }) {
    const auto c1 = lm_pcg::entry_acceptance(a, 1.0f);
    EXPECT_EQ(c1.keep_prob, a == 1.0f ? 1.0f : a) << a;
    EXPECT_EQ(c1.weight_mult, 1.0f) << a;
    const auto cinf = lm_pcg::entry_acceptance(a, 0.0f);
    EXPECT_EQ(cinf.keep_prob, 1.0f) << a;
    EXPECT_EQ(cinf.weight_mult, a) << a;
    const auto c2 = lm_pcg::entry_acceptance(a, 0.5f);
    EXPECT_FLOAT_EQ(c2.keep_prob * c2.weight_mult, a) << a << ": q·m must be a for every member";
  }
  EXPECT_EQ(lm_pcg::entry_acceptance(0.0f, 0.5f).keep_prob, 0.0f);
}

}  // namespace
}  // namespace lumice
