// liblumice_analytic's single-path evaluator (src/analytic/path_evaluation.hpp), checked against
// physics restated here rather than against the kernel's own helpers: face normals from the
// wedge-angle geometry, Snell's law as a sine ratio, the Fresnel factor in its r_s / r_p form,
// reciprocity of a reversed path, and — as the cross-check with the simulator — the float
// HitSurface chain on the same crystal.
//
// symmetry_semantics: none — every case evaluates one concrete face sequence (doc/analytic-api.md
// section 3).

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <random>
#include <thread>
#include <vector>

#include "analytic/jet.hpp"
#include "analytic/path_chain.hpp"
#include "analytic/path_evaluation.hpp"
#include "core/crystal.hpp"
#include "core/optics.hpp"

namespace lumice::analytic {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kN = 1.31;

LUMICE_ANALYTIC_Crystal Prism(double height) {
  LUMICE_ANALYTIC_Crystal c{};
  c.kind = LUMICE_ANALYTIC_CRYSTAL_PRISM;
  c.height = height;
  for (double& d : c.face_distance) {
    d = 1.0;
  }
  return c;
}

LUMICE_ANALYTIC_Crystal Pyramid(double prism_h, double upper_h, double lower_h, double upper_wedge,
                                double lower_wedge) {
  LUMICE_ANALYTIC_Crystal c = Prism(prism_h);
  c.kind = LUMICE_ANALYTIC_CRYSTAL_PYRAMID;
  c.upper_h = upper_h;
  c.lower_h = lower_h;
  c.upper_wedge_deg = upper_wedge;
  c.lower_wedge_deg = lower_wedge;
  return c;
}

// Outward normal of Lumice face number `fn` restated from the shape description, not from the
// closed-form plane table: basal ±c; prism face 3+i at azimuth i·60°; a pyramidal face at the same
// azimuth as its prism face, tilted so that it makes `wedge` degrees with the c axis — its normal
// makes 90° − wedge with c (LI's pyramid_face_angle, doc/configuration.md).
std::array<double, 3> ExpectedNormal(int fn, double upper_wedge, double lower_wedge) {
  if (fn == 1) {
    return { 0, 0, 1 };
  }
  if (fn == 2) {
    return { 0, 0, -1 };
  }
  const int i = (fn % 10) - 3;
  const double az = i * kPi / 3.0;
  if (fn < 10) {
    return { std::cos(az), std::sin(az), 0 };
  }
  const double w = (fn < 20 ? upper_wedge : lower_wedge) * kPi / 180.0;
  const double z = fn < 20 ? std::sin(w) : -std::sin(w);
  return { std::cos(w) * std::cos(az), std::cos(w) * std::sin(az), z };
}

std::array<double, 3> Cross(const double* a, const double* b) {
  return { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };
}
double Dot(const double* a, const double* b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
double Norm(const std::array<double, 3>& v) {
  return std::sqrt(Dot(v.data(), v.data()));
}

// Unpolarised transmittance of an interface n1 -> n2 in the textbook r_s / r_p form.
double TextbookTransmittance(double n1, double cos_i, double n2, double cos_t) {
  const double rs = (n1 * cos_i - n2 * cos_t) / (n1 * cos_i + n2 * cos_t);
  const double rp = (n2 * cos_i - n1 * cos_t) / (n2 * cos_i + n1 * cos_t);
  return 1.0 - 0.5 * (rs * rs + rp * rp);
}

// Uniform random rotation (row-major) from a unit quaternion.
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

// A 3-4-5 direction: unit length to the last bit, so the checks below measure the kernel, not the input.
const double kSun[3] = { 0.36, -0.48, -0.8 };

struct Evaluation {
  bool valid = false;
  int face_count = 0;
  std::array<double, 3> out{};
  double fresnel = 0;
  std::vector<double> seg;
  std::vector<double> trans;
};

class PathEvaluationTest : public ::testing::Test {
 protected:
  void Use(const LUMICE_ANALYTIC_Crystal& crystal) { ASSERT_EQ(BuildFaceNormals(crystal, &table_), Status::kOk); }

  Evaluation Eval(const std::vector<int>& faces, const double* incident, const double* pose) const {
    Evaluation e;
    e.face_count = static_cast<int>(faces.size());
    std::vector<int> slots(faces.size());
    EXPECT_EQ(ResolveFaceSequence(table_, faces.data(), e.face_count, slots.data()), Status::kOk);
    e.seg.assign(3 * (faces.size() + 1), -7.0);
    e.trans.assign(faces.size(), -7.0);
    PathOutputs o{};
    o.segment_directions = e.seg.data();
    o.interface_transmittances = e.trans.data();
    e.valid = EvaluatePath(table_, slots.data(), e.face_count, kN, incident, pose, &o);
    std::memcpy(e.out.data(), o.outgoing_direction, sizeof(o.outgoing_direction));
    e.fresnel = o.fresnel_transmission;
    return e;
  }

  // First pose (fixed-seed search) at which `faces` is valid, or at which `accept` holds.
  template <typename Pred>
  std::array<double, 9> FindPose(const std::vector<int>& faces, Pred accept, int tries = 200000) const {
    std::mt19937_64 rng(20260929);
    for (int t = 0; t < tries; t++) {
      auto r = RandomRotation(rng);
      if (accept(Eval(faces, kSun, r.data()), r)) {
        return r;
      }
    }
    ADD_FAILURE() << "no pose found";
    return {};
  }
  std::array<double, 9> FindValidPose(const std::vector<int>& faces) const {
    return FindPose(faces, [](const Evaluation& e, const std::array<double, 9>&) { return e.valid; });
  }

  // Body-frame normal the kernel uses for face `fn`.
  const double* BodyNormal(int fn) const { return table_.normal[table_.SlotOf(fn)]; }

  FaceNormalTable table_;
};

// ---------------------------------------------------------------------------------------------
// Face normals
// ---------------------------------------------------------------------------------------------

TEST_F(PathEvaluationTest, FaceNormalsMatchTheShapeDescription) {
  for (const auto& [crystal, faces] :
       { std::pair{ Prism(1.0), std::vector<int>{ 1, 2, 3, 4, 5, 6, 7, 8 } },
         std::pair{ Pyramid(0.5, 0.5, 0.5, 28.0, 61.7), std::vector<int>{ 1, 2, 3, 5, 8, 13, 16, 18, 23, 25, 28 } } }) {
    Use(crystal);
    for (int fn : faces) {
      if (table_.SlotOf(fn) < 0) {
        ADD_FAILURE() << "face " << fn << " missing";
        continue;
      }
      const auto want = ExpectedNormal(fn, crystal.upper_wedge_deg, crystal.lower_wedge_deg);
      for (int k = 0; k < 3; k++) {
        EXPECT_NEAR(BodyNormal(fn)[k], want[k], 1e-15) << "face " << fn << " component " << k;
      }
    }
  }
}

TEST_F(PathEvaluationTest, AbsentFacesAreNotInTheTable) {
  Use(Pyramid(0.5, 0.0, 0.5, 28.0, 28.0));  // upper_h = 0: no upper cone, basal 1 closes it
  EXPECT_GE(table_.SlotOf(1), 0);
  EXPECT_LT(table_.SlotOf(13), 0);
  EXPECT_GE(table_.SlotOf(23), 0);
  Use(Prism(1.0));
  EXPECT_LT(table_.SlotOf(13), 0);
  EXPECT_LT(table_.SlotOf(0), 0);
  EXPECT_LT(table_.SlotOf(9), 0);
}

// ---------------------------------------------------------------------------------------------
// Physics of a valid path
// ---------------------------------------------------------------------------------------------

struct PathCase {
  const char* name;
  LUMICE_ANALYTIC_Crystal crystal;
  std::vector<int> faces;
};

std::vector<PathCase> Cases() {
  return { { "prism 3-5", Prism(1.0), { 3, 5 } },
           { "prism 3-5-6-7", Prism(0.3), { 3, 5, 6, 7 } },
           { "prism 1-3-2", Prism(0.6), { 1, 3, 2 } },
           { "pyramid 13-15-26-28", Pyramid(0.5, 0.5, 0.5, 28.0, 28.0), { 13, 15, 26, 28 } } };
}

TEST_F(PathEvaluationTest, SnellAndReflectionHoldAtEveryInterface) {
  for (const auto& c : Cases()) {
    SCOPED_TRACE(c.name);
    Use(c.crystal);
    for (int trial = 0; trial < 5; trial++) {
      std::mt19937_64 rng(1000 + trial);
      std::array<double, 9> pose{};
      Evaluation e;
      do {
        pose = RandomRotation(rng);
        e = Eval(c.faces, kSun, pose.data());
      } while (!e.valid);
      EXPECT_NEAR(Norm(e.out), 1.0, 1e-14);
      const int m = e.face_count;
      for (int k = 0; k < m; k++) {
        const double* in = &e.seg[3 * k];
        const double* out = &e.seg[3 * (k + 1)];
        const auto nrm = ExpectedNormal(c.faces[k], c.crystal.upper_wedge_deg, c.crystal.lower_wedge_deg);
        EXPECT_NEAR(Norm({ out[0], out[1], out[2] }), 1.0, 1e-14);
        // Coplanar with the normal: (in x N) and (out x N) are parallel.
        const auto sin_in = Cross(in, nrm.data());
        const auto sin_out = Cross(out, nrm.data());
        EXPECT_NEAR(Norm(Cross(sin_in.data(), sin_out.data())), 0.0, 1e-14);
        if (k == 0 || k == m - 1) {
          const double n_in = k == 0 ? 1.0 : kN;
          const double n_out = k == 0 ? kN : 1.0;
          EXPECT_NEAR(n_in * Norm(sin_in), n_out * Norm(sin_out), 1e-14) << "Snell at interface " << k;
          // Refraction keeps the side of travel relative to the face.
          EXPECT_GT(Dot(in, nrm.data()) * Dot(out, nrm.data()), 0.0);
          const double cos_i = std::fabs(Dot(in, nrm.data()));
          const double cos_t = std::fabs(Dot(out, nrm.data()));
          EXPECT_NEAR(e.trans[k], TextbookTransmittance(n_in, cos_i, n_out, cos_t), 1e-14);
        } else {
          EXPECT_NEAR(Norm(sin_in), Norm(sin_out), 1e-14) << "reflection at interface " << k;
          EXPECT_NEAR(Dot(in, nrm.data()), -Dot(out, nrm.data()), 1e-14);
          const double cos_i = Dot(in, nrm.data());
          const double sin2_t = kN * kN * (1.0 - cos_i * cos_i);
          const double r_expected =
              sin2_t >= 1.0 ? 1.0 : 1.0 - TextbookTransmittance(kN, cos_i, 1.0, std::sqrt(1.0 - sin2_t));
          EXPECT_NEAR(e.trans[k], r_expected, 1e-14);
        }
      }
      double product = 1.0;
      for (double t : e.trans) {
        product *= t;
      }
      EXPECT_NEAR(e.fresnel, product, 1e-15);
      // The first segment is the incident direction in the body frame.
      for (int i = 0; i < 3; i++) {
        const double body = pose[0 * 3 + i] * kSun[0] + pose[1 * 3 + i] * kSun[1] + pose[2 * 3 + i] * kSun[2];
        EXPECT_NEAR(e.seg[i], body, 1e-15);
      }
    }
  }
}

TEST_F(PathEvaluationTest, TwoFacePathEnergyIsEntryTimesExitTransmittance) {
  Use(Prism(1.0));
  const auto pose = FindValidPose({ 3, 5 });
  const auto e = Eval({ 3, 5 }, kSun, pose.data());
  ASSERT_TRUE(e.valid);
  const auto n3 = ExpectedNormal(3, 0, 0);
  const auto n5 = ExpectedNormal(5, 0, 0);
  const double c_in = -Dot(&e.seg[0], n3.data());
  const double c_mid = Dot(&e.seg[3], n3.data());
  const double c_x = Dot(&e.seg[3], n5.data());
  const double c_out = Dot(&e.seg[6], n5.data());
  EXPECT_NEAR(e.fresnel, TextbookTransmittance(1.0, c_in, kN, -c_mid) * TextbookTransmittance(kN, c_x, 1.0, c_out),
              1e-14);
}

TEST_F(PathEvaluationTest, ReversedPathRetracesTheRay) {
  for (const auto& c : Cases()) {
    SCOPED_TRACE(c.name);
    Use(c.crystal);
    const auto pose = FindValidPose(c.faces);
    const auto fwd = Eval(c.faces, kSun, pose.data());
    if (!fwd.valid) {
      ADD_FAILURE() << "forward path not valid";
      continue;
    }
    const double back_in[3] = { -fwd.out[0], -fwd.out[1], -fwd.out[2] };
    const std::vector<int> reversed(c.faces.rbegin(), c.faces.rend());
    const auto back = Eval(reversed, back_in, pose.data());
    if (!back.valid) {
      ADD_FAILURE() << "reversed path not valid";
      continue;
    }
    for (int i = 0; i < 3; i++) {
      EXPECT_NEAR(back.out[i], -kSun[i], 1e-13);
    }
    const int s = fwd.face_count + 1;
    for (int k = 0; k < s; k++) {
      for (int i = 0; i < 3; i++) {
        EXPECT_NEAR(back.seg[3 * k + i], -fwd.seg[3 * (s - 1 - k) + i], 1e-13);
      }
    }
    // Reciprocity: the same interfaces in the other order transmit the same power.
    EXPECT_NEAR(back.fresnel, fwd.fresnel, 1e-13);
  }
}

// ---------------------------------------------------------------------------------------------
// Validity gates
// ---------------------------------------------------------------------------------------------

bool AllZeroAndFinite(const Evaluation& e) {
  bool ok = e.fresnel == 0.0;
  for (double v : e.out) {
    ok = ok && v == 0.0;
  }
  for (double v : e.seg) {
    ok = ok && v == 0.0;
  }
  for (double v : e.trans) {
    ok = ok && v == 0.0;
  }
  return ok;
}

TEST_F(PathEvaluationTest, ExitTotalInternalReflectionIsInvalidWithZeroOutputs) {
  Use(Prism(1.0));
  // Exit refraction impossible: the ray reaches face 5 from inside but beyond the critical angle.
  const auto pose = FindPose({ 3, 5 }, [this](const Evaluation& e, const std::array<double, 9>& r) {
    if (e.valid) {
      return false;
    }
    double n3[3];
    double n5[3];
    for (int i = 0; i < 3; i++) {
      n3[i] = r[i * 3 + 0] * BodyNormal(3)[0] + r[i * 3 + 1] * BodyNormal(3)[1] + r[i * 3 + 2] * BodyNormal(3)[2];
      n5[i] = r[i * 3 + 0] * BodyNormal(5)[0] + r[i * 3 + 1] * BodyNormal(5)[1] + r[i * 3 + 2] * BodyNormal(5)[2];
    }
    const double c = -Dot(n3, kSun);
    if (c <= 0) {
      return false;
    }
    const double rr = 1.0 / kN;
    const double k = rr * c - std::sqrt(1.0 - rr * rr * (1.0 - c * c));
    double d[3];
    for (int i = 0; i < 3; i++) {
      d[i] = rr * kSun[i] + k * n3[i];
    }
    const double cx = Dot(n5, d);
    return cx > 0 && 1.0 - kN * kN * (1.0 - cx * cx) <= 0.0;
  });
  const auto e = Eval({ 3, 5 }, kSun, pose.data());
  EXPECT_FALSE(e.valid);
  EXPECT_TRUE(AllZeroAndFinite(e));
}

TEST_F(PathEvaluationTest, InternalTotalReflectionKeepsThePathValidWithUnitReflectance) {
  Use(Prism(0.3));
  const std::vector<int> faces{ 3, 5, 6, 7 };
  const auto pose = FindPose(faces, [](const Evaluation& e, const std::array<double, 9>&) {
    return e.valid && (e.trans[1] == 1.0 || e.trans[2] == 1.0);
  });
  const auto e = Eval(faces, kSun, pose.data());
  ASSERT_TRUE(e.valid);
  bool saw_total = false;
  for (int k = 1; k < 3; k++) {
    const auto nrm = ExpectedNormal(faces[k], 0, 0);
    const double c = Dot(&e.seg[3 * k], nrm.data());
    if (kN * kN * (1.0 - c * c) > 1.0) {
      EXPECT_EQ(e.trans[k], 1.0) << "interface " << k;
      saw_total = true;
    }
  }
  EXPECT_TRUE(saw_total);
}

TEST_F(PathEvaluationTest, InternalPartialReflectionKeepsThePathValid) {
  Use(Prism(0.3));
  const std::vector<int> faces{ 3, 5, 6, 7 };
  const auto pose = FindPose(faces, [](const Evaluation& e, const std::array<double, 9>&) {
    return e.valid && (e.trans[1] < 1.0 || e.trans[2] < 1.0);
  });
  const auto e = Eval(faces, kSun, pose.data());
  ASSERT_TRUE(e.valid);
  EXPECT_LT(e.fresnel, e.trans[0] * e.trans[3]);
}

TEST_F(PathEvaluationTest, BackFaceEntryAndUnreachableFacesAreInvalidWithoutNan) {
  Use(Prism(1.0));
  int invalid = 0;
  std::mt19937_64 rng(7);
  for (int t = 0; t < 2000; t++) {
    const auto r = RandomRotation(rng);
    for (const auto& faces : { std::vector<int>{ 3, 5 }, std::vector<int>{ 3, 3 }, std::vector<int>{ 3, 6, 5 } }) {
      const auto e = Eval(faces, kSun, r.data());
      if (!e.valid) {
        invalid++;
        EXPECT_TRUE(AllZeroAndFinite(e));
      }
    }
  }
  EXPECT_GT(invalid, 1000);
}

TEST_F(PathEvaluationTest, InternalFaceMustBeReachedFromInside) {
  // 3-1-5 through a prism: whenever the refracted ray heads away from basal face 1 (its incidence
  // cosine there is not positive), the path is invalid even if entry and exit alone would pass.
  Use(Prism(0.6));
  std::mt19937_64 rng(13);
  int away = 0;
  for (int t = 0; t < 4000; t++) {
    const auto r = RandomRotation(rng);
    double n3[3];
    double n1[3];
    for (int i = 0; i < 3; i++) {
      n3[i] = r[i * 3 + 0] * BodyNormal(3)[0] + r[i * 3 + 1] * BodyNormal(3)[1] + r[i * 3 + 2] * BodyNormal(3)[2];
      n1[i] = r[i * 3 + 0] * BodyNormal(1)[0] + r[i * 3 + 1] * BodyNormal(1)[1] + r[i * 3 + 2] * BodyNormal(1)[2];
    }
    const double c = -Dot(n3, kSun);
    if (c <= 0) {
      continue;
    }
    const double rr = 1.0 / kN;
    const double k = rr * c - std::sqrt(1.0 - rr * rr * (1.0 - c * c));
    double d[3];
    for (int i = 0; i < 3; i++) {
      d[i] = rr * kSun[i] + k * n3[i];
    }
    if (Dot(n1, d) <= 0) {
      away++;
      EXPECT_FALSE(Eval({ 3, 1, 5 }, kSun, r.data()).valid);
    }
  }
  EXPECT_GT(away, 500);
}

TEST_F(PathEvaluationTest, RepeatedFaceIsNeverValid) {
  // A face cannot be reached from inside right after the ray entered or reflected off it.
  Use(Prism(1.0));
  std::mt19937_64 rng(11);
  for (int t = 0; t < 2000; t++) {
    const auto r = RandomRotation(rng);
    EXPECT_FALSE(Eval({ 3, 3 }, kSun, r.data()).valid);
    EXPECT_FALSE(Eval({ 3, 5, 5 }, kSun, r.data()).valid);
  }
}

// ---------------------------------------------------------------------------------------------
// Cross-check with the simulator's float HitSurface on the same crystal
// ---------------------------------------------------------------------------------------------

TEST_F(PathEvaluationTest, AgreesWithTheSimulatorsHitSurfaceChain) {
  struct Engine {
    Crystal crystal;
    std::vector<int> faces;
  };
  const Engine engines[] = {
    { Crystal::CreatePrism(0.3f), { 3, 5, 6, 7 } },
    { Crystal::CreatePrism(0.6f), { 1, 3, 2 } },
  };
  const LUMICE_ANALYTIC_Crystal analytic[] = { Prism(0.3), Prism(0.6) };
  for (int c = 0; c < 2; c++) {
    Use(analytic[c]);
    const auto& eng = engines[c];
    auto poly_of = [&eng](int fn) {
      for (size_t p = 0; p < eng.crystal.PolygonFaceCount(); p++) {
        if (static_cast<int>(eng.crystal.GetFn(static_cast<IdType>(p))) == fn) {
          return static_cast<IdType>(p);
        }
      }
      return kInvalidId;
    };
    const auto pose = FindValidPose(eng.faces);
    const auto e = Eval(eng.faces, kSun, pose.data());
    if (!e.valid) {
      ADD_FAILURE() << "path not valid";
      continue;
    }

    float dir[3] = { static_cast<float>(e.seg[0]), static_cast<float>(e.seg[1]), static_cast<float>(e.seg[2]) };
    float weight = 1.0f;
    const int m = static_cast<int>(eng.faces.size());
    for (int k = 0; k < m; k++) {
      float w_in[1] = { weight };
      IdType to_face[1] = { poly_of(eng.faces[k]) };
      if (to_face[0] == kInvalidId) {
        ADD_FAILURE() << "face " << eng.faces[k] << " has no polygon";
        break;
      }
      float d_out[6] = {};
      float w_out[2] = {};
      HitSurface(eng.crystal, static_cast<float>(kN), 1, float_bf_t(dir, 3 * sizeof(float)),
                 float_bf_t(w_in, sizeof(float)), id_bf_t(to_face, sizeof(IdType)),
                 float_bf_t(d_out, 3 * sizeof(float)), float_bf_t(w_out, sizeof(float)));
      const bool reflect = k > 0 && k < m - 1;
      const float* next = reflect ? d_out : d_out + 3;
      weight = reflect ? w_out[0] : w_out[1];
      std::memcpy(dir, next, sizeof(dir));
      for (int i = 0; i < 3; i++) {
        EXPECT_NEAR(dir[i], e.seg[3 * (k + 1) + i], 2e-6) << "interface " << k;
      }
    }
    EXPECT_NEAR(weight, e.fresnel, 2e-6);
  }
}

// ---------------------------------------------------------------------------------------------
// Regression pins from LI's float64 evaluator (Lumice Integral rev bfbd042,
// parity_export.evaluate_path, incident (0.36, -0.48, -0.8), n = 1.31), taken once while this
// evaluator was written. Not the parity suite — LI's exported fixtures are, and they supersede
// these — but a fixed point that keeps a later edit from drifting away from LI unnoticed. At the time
// they were taken, 24000 random poses over six (crystal, path) pairs agreed with LI to within 0.054
// of LI's per-pose kinematic_atol (1e-12, widened near a Snell boundary), with no validity mismatch.
// ---------------------------------------------------------------------------------------------

struct LiPin {
  const char* name;
  LUMICE_ANALYTIC_Crystal crystal;
  std::vector<int> faces;
  std::array<double, 9> pose;
  std::array<double, 3> outgoing;
  double fresnel;
};

TEST_F(PathEvaluationTest, MatchesLiReferenceValues) {
  auto irregular = Pyramid(0.4, 0.3, 0.7, 20.0, 61.7);
  const double fd[6] = { 1.2, 0.7, 1.0, 0.4, 1.5, 0.9 };
  std::memcpy(irregular.face_distance, fd, sizeof(fd));
  const LiPin pins[] = {
    { "prism 3-5",
      Prism(1.0),
      { 3, 5 },
      { 0.0663720309507386, 0.9215718938337404, -0.38249182736781095, 0.5651461671302535, 0.2812000596610774,
        0.775587735994188, 0.8223165833700897, -0.2676411234139845, -0.5021590044719326 },
      { -0.03718867491292033, -0.6495809591238338, -0.759382367455277 },
      0.9231796045844576 },
    { "prism 3-5-6-7",
      Prism(0.3),
      { 3, 5, 6, 7 },
      { -0.5408000095092916, -0.7297493844277124, -0.41833143037802545, 0.6020974578775274, 0.011451332692545946,
        -0.7983404776140213, 0.5873789244875653, -0.6836188286661942, 0.4331873684225933 },
      { 0.1617996779166292, 0.5147776677536923, 0.8419173457104585 },
      0.175319657258382 },
    { "pyramid 13-15-26-28",
      Pyramid(0.5, 0.5, 0.5, 28.0, 28.0),
      { 13, 15, 26, 28 },
      { -0.23820952940492185, -0.15637893774929476, 0.9585415212337375, 0.09694674647588361, -0.9858514037998921,
        -0.1367418662060837, 0.9663630520919279, 0.06035426624675297, 0.24999962819450278 },
      { 0.08242049880135076, 0.6747540528360012, 0.7334260900450198 },
      0.6771144887635017 },
    { "irregular pyramid 3-1-26",
      irregular,
      { 3, 1, 26 },
      { 0.664724037991075, -0.009743898649344607, 0.7470254411704804, 0.7212284735792927, -0.25243746206766693,
        -0.6450618703995133, 0.19486262395678483, 0.9675641499063233, -0.1607736719762602 },
      { -0.6891715858716958, -0.1151825467081706, -0.7153848657617179 },
      0.7723306755566923 },
  };
  for (const auto& pin : pins) {
    SCOPED_TRACE(pin.name);
    Use(pin.crystal);
    const auto e = Eval(pin.faces, kSun, pin.pose.data());
    EXPECT_TRUE(e.valid);
    for (int i = 0; i < 3; i++) {
      EXPECT_NEAR(e.out[i], pin.outgoing[i], 1e-12);
    }
    EXPECT_NEAR(e.fresnel, pin.fresnel, 1e-12);
  }
}

// ---------------------------------------------------------------------------------------------
// The chain's domain report (path_chain.hpp): what the fiber continuation reads as the event kind.
// ---------------------------------------------------------------------------------------------

// LI's _path_domain gate walk restated from the shape description (ExpectedNormal) rather than from
// the kernel's table: margins in validity_margin_names order up to the failing gate, and the first
// failure — non-finite before cosine before Snell discriminant at each interface.
struct OracleDomain {
  std::vector<double> margins;
  ChainFailure failure = ChainFailure::kNone;
};

OracleDomain OracleGateWalk(const std::vector<int>& faces, const double* incident, const double* r) {
  auto world = [r](int fn) {
    const auto b = ExpectedNormal(fn, 0, 0);
    std::array<double, 3> w{};
    for (int i = 0; i < 3; i++) {
      w[i] = r[i * 3 + 0] * b[0] + r[i * 3 + 1] * b[1] + r[i * 3 + 2] * b[2];
    }
    return w;
  };
  OracleDomain o;
  auto fail = [&o](ChainFailure f) {
    o.failure = f;
    return o;
  };
  auto n = world(faces.front());
  const double rr = 1.0 / kN;
  const double c = -Dot(n.data(), incident);
  const double disc = 1.0 - rr * rr * (1.0 - c * c);
  o.margins = { c, disc };
  if (!std::isfinite(c) || !std::isfinite(disc)) {
    return fail(ChainFailure::kNonFinite);
  }
  if (c <= 0) {
    return fail(ChainFailure::kPathInfeasible);
  }
  if (disc <= 0) {
    return fail(ChainFailure::kTirBoundary);
  }
  double d[3];
  for (int i = 0; i < 3; i++) {
    d[i] = rr * incident[i] + (rr * c - std::sqrt(disc)) * n[i];
  }
  for (size_t k = 1; k + 1 < faces.size(); k++) {
    n = world(faces[k]);
    const double ci = Dot(n.data(), d);
    o.margins.push_back(ci);
    if (!std::isfinite(ci)) {
      return fail(ChainFailure::kNonFinite);
    }
    if (ci <= 0) {
      return fail(ChainFailure::kPathInfeasible);
    }
    for (int i = 0; i < 3; i++) {
      d[i] -= 2 * ci * n[i];
    }
  }
  n = world(faces.back());
  const double ce = Dot(n.data(), d);
  const double de = 1.0 - kN * kN * (1.0 - ce * ce);
  o.margins.push_back(ce);
  o.margins.push_back(de);
  if (!std::isfinite(ce) || !std::isfinite(de)) {
    return fail(ChainFailure::kNonFinite);
  }
  if (ce <= 0) {
    return fail(ChainFailure::kPathInfeasible);
  }
  if (de <= 0) {
    return fail(ChainFailure::kTirBoundary);
  }
  return o;
}

TEST_F(PathEvaluationTest, ChainReportsLiGateOrderMarginsAndFirstFailure) {
  Use(Prism(0.6));
  const std::vector<std::vector<int>> paths{ { 3, 5 }, { 3, 1, 5 }, { 3, 5, 6, 7 } };
  std::mt19937_64 rng(29);
  int seen[4] = { 0, 0, 0, 0 };
  for (int t = 0; t < 6000; t++) {
    const auto r = RandomRotation(rng);
    for (const auto& faces : paths) {
      const auto oracle = OracleGateWalk(faces, kSun, r.data());
      bool near_zero = false;
      for (double m : oracle.margins) {
        near_zero = near_zero || std::fabs(m) < 1e-12;
      }
      if (near_zero) {
        continue;  // a knife-edge gate may round either way between the two normal derivations
      }
      const int fc = static_cast<int>(faces.size());
      std::vector<int> slots(faces.size());
      if (ResolveFaceSequence(table_, faces.data(), fc, slots.data()) != Status::kOk) {
        ADD_FAILURE() << "unresolved path";
        continue;
      }
      std::vector<double> margins(faces.size() + 2, -99.0);
      ChainDomain domain;
      domain.margins = margins.data();
      double out[3];
      const bool valid = TracePathChain<double>(table_, slots.data(), fc, kN, kSun, r.data(), out, nullptr, &domain);
      EXPECT_EQ(valid, oracle.failure == ChainFailure::kNone);
      if (domain.failure != oracle.failure || domain.margin_count != static_cast<int>(oracle.margins.size())) {
        ADD_FAILURE() << "failure " << static_cast<int>(domain.failure) << " vs oracle "
                      << static_cast<int>(oracle.failure) << ", margins " << domain.margin_count << " vs "
                      << oracle.margins.size();
        continue;
      }
      for (int i = 0; i < domain.margin_count; i++) {
        EXPECT_NEAR(margins[i], oracle.margins[i], 1e-13);
      }
      if (!valid) {
        // The failing margin is one of those recorded (an interface records its cosine and Snell
        // discriminant together, so it is not always the last), and it is not positive.
        EXPECT_LE(domain.failure_margin, 0.0);
        EXPECT_NE(std::find(margins.begin(), margins.begin() + domain.margin_count, domain.failure_margin),
                  margins.begin() + domain.margin_count);
      }
      seen[static_cast<int>(oracle.failure)]++;
    }
  }
  // Every class but non-finite occurs on random poses; each has enough samples to mean something.
  EXPECT_GT(seen[static_cast<int>(ChainFailure::kNone)], 100);
  EXPECT_GT(seen[static_cast<int>(ChainFailure::kPathInfeasible)], 100);
  EXPECT_GT(seen[static_cast<int>(ChainFailure::kTirBoundary)], 100);
}

TEST_F(PathEvaluationTest, ChainReportsNonFiniteBeforeAnyGate) {
  Use(Prism(1.0));
  const int slots[2] = { table_.SlotOf(3), table_.SlotOf(5) };
  double pose[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
  pose[0] = std::nan("");
  double margins[4];
  ChainDomain domain;
  domain.margins = margins;
  double out[3];
  EXPECT_FALSE(TracePathChain<double>(table_, slots, 2, kN, kSun, pose, out, nullptr, &domain));
  EXPECT_EQ(domain.failure, ChainFailure::kNonFinite);
  EXPECT_EQ(domain.margin_count, 2);
  EXPECT_TRUE(std::isnan(domain.failure_margin));
}

// EvaluatePath does not ask for the domain report; asking for it changes nothing it returns, and a
// Jet<3> run of the same chain has the double run's values (to rounding: the double one may fuse
// a multiply-add, see jet.hpp) — so the continuation's direction and its derivative are the
// direction EvaluatePath reports.
TEST_F(PathEvaluationTest, DomainReportAndJetRunLeaveTheDirectionUnchanged) {
  Use(Pyramid(1.0, 0.3, 0.3, 28.0, 28.0));
  const std::vector<std::vector<int>> paths{ { 3, 5 }, { 13, 15, 26, 28 }, { 3, 1, 5 } };
  std::mt19937_64 rng(31);
  int valid_count = 0;
  for (int t = 0; t < 3000; t++) {
    const auto r = RandomRotation(rng);
    for (const auto& faces : paths) {
      const auto e = Eval(faces, kSun, r.data());
      const int fc = static_cast<int>(faces.size());
      std::vector<int> slots(faces.size());
      if (ResolveFaceSequence(table_, faces.data(), fc, slots.data()) != Status::kOk) {
        ADD_FAILURE() << "unresolved path";
        continue;
      }
      std::vector<double> margins(faces.size() + 2);
      ChainDomain domain;
      domain.margins = margins.data();
      double out[3];
      const bool valid = TracePathChain<double>(table_, slots.data(), fc, kN, kSun, r.data(), out, nullptr, &domain);
      EXPECT_EQ(valid, e.valid);
      if (!valid || !e.valid) {
        continue;
      }
      valid_count++;
      Jet<3> pose_jet[9];
      for (int i = 0; i < 9; i++) {
        pose_jet[i] = Jet<3>(r[i]);
      }
      Jet<3> out_jet[3];
      if (!TracePathChain<Jet<3>>(table_, slots.data(), fc, kN, kSun, pose_jet, out_jet, nullptr, nullptr)) {
        ADD_FAILURE() << "the Jet run took another branch";
        continue;
      }
      for (int i = 0; i < 3; i++) {
        EXPECT_EQ(out[i], e.out[i]);
        // Measured up to ~5e-15 on this sample (FMA in the double build, amplified near a Snell
        // boundary where the square root is steep); LI's kinematic_atol is 1e-12.
        EXPECT_NEAR(out_jet[i].a, e.out[i], 1e-13);
      }
    }
  }
  EXPECT_GT(valid_count, 300);
}

// ---------------------------------------------------------------------------------------------
// Input validation
// ---------------------------------------------------------------------------------------------

TEST(PathEvaluationInput, CrystalFieldsAreChecked) {
  FaceNormalTable t;
  auto prism = Prism(1.0);
  prism.upper_h = 0.5;  // unused by a prism: must be zero
  EXPECT_EQ(BuildFaceNormals(prism, &t), Status::kInvalidValue);
  prism = Prism(1.0);
  prism.lower_wedge_deg = 28.0;
  EXPECT_EQ(BuildFaceNormals(prism, &t), Status::kInvalidValue);
  prism = Prism(1.0);
  prism.kind = 7;
  EXPECT_EQ(BuildFaceNormals(prism, &t), Status::kInvalidValue);
  prism = Prism(std::nan(""));
  EXPECT_EQ(BuildFaceNormals(prism, &t), Status::kInvalidValue);
  prism = Prism(1e300);  // finite, not representable in the engine's float factory
  EXPECT_EQ(BuildFaceNormals(prism, &t), Status::kInvalidValue);
  EXPECT_EQ(t.slot_cnt, 0);
}

TEST(PathEvaluationInput, CrystalsTheEngineRejectsAreInvalidConfig) {
  FaceNormalTable t;
  EXPECT_EQ(BuildFaceNormals(Prism(0.0), &t), Status::kInvalidConfig);  // zero volume
  auto collapsed = Prism(1.0);
  collapsed.face_distance[3] = -1.0;  // opposite faces 3 and 6 meet: empty cross section
  EXPECT_EQ(BuildFaceNormals(collapsed, &t), Status::kInvalidConfig);
  // Pyramid with no cone on either side and no prism band.
  EXPECT_EQ(BuildFaceNormals(Pyramid(0.0, 0.0, 0.0, 28.0, 28.0), &t), Status::kInvalidConfig);
  EXPECT_EQ(t.slot_cnt, 0);
}

TEST(PathEvaluationInput, FaceSequenceIsChecked) {
  FaceNormalTable t;
  ASSERT_EQ(BuildFaceNormals(Prism(1.0), &t), Status::kOk);
  int slots[4];
  const int one[] = { 3 };
  EXPECT_EQ(ResolveFaceSequence(t, one, 1, slots), Status::kInvalidValue);
  const int unknown[] = { 3, 13 };
  EXPECT_EQ(ResolveFaceSequence(t, unknown, 2, slots), Status::kInvalidValue);
  const int zero[] = { 0, 3 };
  EXPECT_EQ(ResolveFaceSequence(t, zero, 2, slots), Status::kInvalidValue);
  const int ok[] = { 3, 1, 5 };
  EXPECT_EQ(ResolveFaceSequence(t, ok, 3, slots), Status::kOk);
  EXPECT_EQ(t.face_number[slots[0]], 3);
  EXPECT_EQ(t.face_number[slots[1]], 1);
  EXPECT_EQ(t.face_number[slots[2]], 5);
}

TEST(PathEvaluationInput, UnitVectorAndRotationChecks) {
  const double unit[3] = { 0.6, 0.8, 0.0 };
  const double off[3] = { 0.6, 0.8, 1e-4 };  // |v| - 1 = 5e-9, above the 1e-10 tolerance
  const double nan3[3] = { std::nan(""), 0, 1 };
  EXPECT_TRUE(ValidateUnitVector(unit));
  EXPECT_FALSE(ValidateUnitVector(off));
  EXPECT_FALSE(ValidateUnitVector(nan3));

  const double identity[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
  const double mirror[9] = { 1, 0, 0, 0, 1, 0, 0, 0, -1 };  // orthogonal, det -1
  const double skewed[9] = { 1, 1e-6, 0, 0, 1, 0, 0, 0, 1 };
  EXPECT_TRUE(ValidateRotation(identity));
  EXPECT_FALSE(ValidateRotation(mirror));
  EXPECT_FALSE(ValidateRotation(skewed));
  std::mt19937_64 rng(3);
  for (int t = 0; t < 100; t++) {
    EXPECT_TRUE(ValidateRotation(RandomRotation(rng).data()));
  }
}

// ---------------------------------------------------------------------------------------------
// Re-entrancy (doc/analytic-api.md section 5.3): concurrent crystal builds and evaluations agree
// with a serial run bit for bit.
// ---------------------------------------------------------------------------------------------

TEST(PathEvaluationConcurrency, ConcurrentCallsMatchSerialResults) {
  const auto crystal = Pyramid(0.5, 0.5, 0.5, 28.0, 28.0);
  const std::vector<int> faces{ 13, 15, 26, 28 };
  constexpr int kPoses = 400;
  std::vector<std::array<double, 9>> poses;
  std::mt19937_64 rng(5);
  for (int i = 0; i < kPoses; i++) {
    poses.push_back(RandomRotation(rng));
  }
  auto run = [&](int begin, int end, std::vector<double>* out) {
    for (int i = begin; i < end; i++) {
      FaceNormalTable t;
      if (BuildFaceNormals(crystal, &t) != Status::kOk) {  // the engine's factory, every call
        ADD_FAILURE();
        continue;
      }
      int slots[4];
      if (ResolveFaceSequence(t, faces.data(), 4, slots) != Status::kOk) {
        ADD_FAILURE();
        continue;
      }
      double seg[15];
      double trans[4];
      PathOutputs o{};
      o.segment_directions = seg;
      o.interface_transmittances = trans;
      EvaluatePath(t, slots, 4, kN, kSun, poses[i].data(), &o);
      (*out)[i * 4 + 0] = o.outgoing_direction[0];
      (*out)[i * 4 + 1] = o.outgoing_direction[1];
      (*out)[i * 4 + 2] = o.outgoing_direction[2];
      (*out)[i * 4 + 3] = o.fresnel_transmission;
    }
  };
  std::vector<double> serial(kPoses * 4);
  run(0, kPoses, &serial);
  std::vector<double> parallel(kPoses * 4);
  std::vector<std::thread> threads;
  constexpr int kThreads = 8;
  for (int k = 0; k < kThreads; k++) {
    threads.emplace_back(run, k * kPoses / kThreads, (k + 1) * kPoses / kThreads, &parallel);
  }
  for (auto& th : threads) {
    th.join();
  }
  EXPECT_EQ(std::memcmp(serial.data(), parallel.data(), serial.size() * sizeof(double)), 0);
}

}  // namespace
}  // namespace lumice::analytic
