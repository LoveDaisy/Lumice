// Parity with Lumice Integral (LI): every fixture under test/fixtures/li-parity/, exported by LI's
// scripts/export_analytic_parity.py at the rev pinned in that directory's SOURCE file, is replayed
// against this library's kernels and compared by the recipes of LI docs/analytic-parity-fixtures.md
// section 4. The direction is one way: LI is the reference. A red here is fixed in the C++ or, when
// the disagreement is about a tolerance or a semantic, in LI followed by a re-export — never by
// editing a fixture or widening a bound in this file (doc/analytic-api.md, "Parity with LI").
//
// What is compared and what is not:
//  - Every tolerance is read from the fixture's own `tolerance.<quantity>.value`. The only numbers
//    of the recipe that live here are the geodesic densification spacing (1e-3 rad, part of the
//    curve-distance definition, LI section 4) and the absolute guard LI's own verifier adds to a
//    relative arclength comparison so a zero-length reference is well defined.
//  - The kernels are called directly (EvaluatePath, TraceFiber, IceDiscovery::DiscoverOnBand), not
//    the C ABI: a seed_search fixture is a replay on LI's exported band, and only the kernel takes a
//    band. The ABI's own translation (LUMICE_ANALYTIC_Crystal and the v0 options block into these
//    kernels) is the ctypes e2e tests' subject (test/e2e-correctness/test_analytic_*.py).
//  - The comparison geometry (rotation distance, SO(3) log/exp, densification, Hausdorff) is written
//    out below independently of src/analytic/so3.hpp, so a defect there cannot also blind the ruler.
//
// symmetry_semantics: none — asserted per fixture (doc/analytic-api.md section 3.3 rule 2): a
// fixture that carried a symmetry reduction would not describe one concrete face sequence.

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <vector>

#include "analytic/discovery.hpp"
#include "analytic/fiber_continuation.hpp"
#include "analytic/path_evaluation.hpp"
#include "analytic/path_fiber.hpp"

#ifndef LUMICE_LI_PARITY_FIXTURE_DIR
#error "LUMICE_LI_PARITY_FIXTURE_DIR must name test/fixtures/li-parity (test/CMakeLists.txt)"
#endif

namespace lumice::analytic {
namespace {

using Json = nlohmann::json;
using Pose = std::array<double, 9>;
using Curve = std::vector<Pose>;

constexpr double kPi = 3.14159265358979323846;
constexpr const char* kFormat = "lumice-integral/analytic-parity";
constexpr int kSchemaVersion = 1;
// LI docs/analytic-parity-fixtures.md section 4: polylines are densified along geodesic chords at a
// spacing of at most 1e-3 rad before the Hausdorff distance is taken (parity_export.CURVE_DENSIFY_SPACING).
constexpr double kDensifySpacing = 1e-3;
// LI parity_export._compare_traces: `abs(got - ref) <= rtol * ref + 1e-12`. The absolute term only
// makes a zero-length reference (a one-pose trace) comparable; it is LI's recipe, not a tolerance.
constexpr double kArclengthAbsoluteGuard = 1e-12;

bool ArclengthWithin(double got, double reference, double rtol) {
  return std::fabs(got - reference) <= rtol * reference + kArclengthAbsoluteGuard;
}

// ------------------------------------------------------------------------------------------------
// Fixture access
// ------------------------------------------------------------------------------------------------

std::filesystem::path FixtureDir() {
  return std::filesystem::path(LUMICE_LI_PARITY_FIXTURE_DIR);
}

Json LoadJson(const std::string& name) {
  std::ifstream in(FixtureDir() / name);
  if (!in) {
    ADD_FAILURE() << "cannot open fixture " << (FixtureDir() / name).string();
    return Json::object();
  }
  return Json::parse(in);
}

// `key: value` lines of SOURCE; `#` lines are comments.
std::map<std::string, std::string> ReadSource() {
  std::map<std::string, std::string> out;
  std::ifstream in(FixtureDir() / "SOURCE");
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    const size_t colon = line.find(':');
    if (colon == std::string::npos) {
      continue;
    }
    size_t v = colon + 1;
    while (v < line.size() && line[v] == ' ') {
      v++;
    }
    // A checkout with core.autocrlf leaves a trailing '\r' on a text file; a value never carries one.
    std::string value = line.substr(v);
    while (!value.empty() && (value.back() == '\r' || value.back() == ' ' || value.back() == '\t')) {
      value.pop_back();
    }
    out[line.substr(0, colon)] = value;
  }
  return out;
}

// Every fixture file the manifest lists, in manifest order. Read at registration time so each
// fixture is its own gtest case; a missing manifest yields no cases, which the manifest test below
// then reports as a failure rather than a silent pass.
std::vector<std::string> ManifestFiles() {
  std::ifstream in(FixtureDir() / "manifest.json");
  if (!in) {
    return {};
  }
  const Json manifest = Json::parse(in, nullptr, false);
  std::vector<std::string> files;
  if (!manifest.is_object() || !manifest.contains("cells")) {
    return files;
  }
  for (const Json& cell : manifest["cells"]) {
    if (!cell.is_object() || !cell.contains("files")) {
      continue;  // a malformed cell surfaces in the manifest test, not as a crash at registration
    }
    for (const Json& f : cell.at("files")) {
      files.push_back(f.get<std::string>());
    }
  }
  return files;
}

std::vector<std::string> ManifestFilesOfKind(const std::string& kind) {
  std::vector<std::string> out;
  for (const std::string& f : ManifestFiles()) {
    if (f.find("__" + kind) != std::string::npos) {
      out.push_back(f);
    }
  }
  return out;
}

std::string CaseName(const testing::TestParamInfo<std::string>& info) {
  std::string name = info.param.substr(0, info.param.size() - std::string(".json").size());
  for (char& c : name) {
    if (!std::isalnum(static_cast<unsigned char>(c))) {
      c = '_';
    }
  }
  return name;
}

void Flatten(const Json& j, std::vector<double>* out) {
  if (j.is_array()) {
    for (const Json& x : j) {
      Flatten(x, out);
    }
  } else {
    out->push_back(j.get<double>());
  }
}

std::vector<double> Numbers(const Json& j) {
  std::vector<double> out;
  Flatten(j, &out);
  return out;
}

Curve Poses(const Json& j) {
  const std::vector<double> flat = Numbers(j);
  Curve out(flat.size() / 9);
  for (size_t i = 0; i < out.size(); i++) {
    std::copy(flat.begin() + 9 * i, flat.begin() + 9 * (i + 1), out[i].begin());
  }
  return out;
}

double Tolerance(const Json& fixture, const std::string& quantity) {
  return fixture.at("tolerance").at(quantity).at("value").get<double>();
}

// One line per compared quantity, printed red or green, so a run records how much room each
// tolerance actually leaves (the first cross-backend evidence about them, LI section 5).
void Report(const std::string& fixture, const std::string& quantity, double error, double tolerance) {
  std::cout << "[li-parity] " << fixture << " " << quantity << " error=" << error << " tolerance=" << tolerance << '\n';
}

// ------------------------------------------------------------------------------------------------
// Inputs: crystal, face sequence, continuation options
// ------------------------------------------------------------------------------------------------

LUMICE_ANALYTIC_Crystal CrystalOf(const Json& j) {
  LUMICE_ANALYTIC_Crystal c{};
  const std::string kind = j.at("kind").get<std::string>();
  EXPECT_TRUE(kind == "prism" || kind == "pyramid") << kind;
  c.kind = kind == "pyramid" ? LUMICE_ANALYTIC_CRYSTAL_PYRAMID : LUMICE_ANALYTIC_CRYSTAL_PRISM;
  c.height = j.at("height").get<double>();
  const std::vector<double> fd = Numbers(j.at("face_distance"));
  EXPECT_EQ(fd.size(), 6u);
  for (size_t i = 0; i < 6 && i < fd.size(); i++) {
    c.face_distance[i] = fd[i];
  }
  c.upper_h = j.at("upper_h").get<double>();
  c.lower_h = j.at("lower_h").get<double>();
  c.upper_wedge_deg = j.at("upper_wedge_deg").get<double>();
  c.lower_wedge_deg = j.at("lower_wedge_deg").get<double>();
  return c;
}

// LI's ContinuationOptions (every field) -> ContinuationParams. LI section 2.1: "a backend uses its
// own equivalents and documents the mapping". This is that mapping for the kernel:
//  - kDirect: same name, same meaning, passed through.
//  - kIgnored: LI fields the kernel has no knob for. Each must hold the one value the kernel
//    implicitly implements, or the fixture no longer describes the problem this kernel solves.
//  - Any other key fails the test, so a field LI adds later cannot be dropped without a decision.
ContinuationParams ParamsOf(const Json& j) {
  ContinuationParams p;
  const std::map<std::string, double*> real_fields = {
    { "unit_tolerance", &p.unit_tolerance },
    { "residual_tolerance", &p.residual_tolerance },
    { "relative_residual_tolerance", &p.relative_residual_tolerance },
    { "singular_value_tolerance", &p.singular_value_tolerance },
    { "condition_limit", &p.condition_limit },
    { "initial_step", &p.initial_step },
    { "minimum_step", &p.minimum_step },
    { "maximum_step", &p.maximum_step },
    { "shrink_factor", &p.shrink_factor },
    { "growth_factor", &p.growth_factor },
    { "corrector_phase_tolerance", &p.corrector_phase_tolerance },
    { "corrector_update_tolerance", &p.corrector_update_tolerance },
    { "maximum_correction", &p.maximum_correction },
    { "maximum_advance", &p.maximum_advance },
    { "minimum_tangent_dot", &p.minimum_tangent_dot },
    { "event_slowdown_margin", &p.event_slowdown_margin },
    { "maximum_arclength", &p.maximum_arclength },
    { "closure_minimum_arclength", &p.closure_minimum_arclength },
    { "closure_distance", &p.closure_distance },
    { "closure_tangent_dot", &p.closure_tangent_dot },
    { "closure_section_tolerance", &p.closure_section_tolerance },
  };
  const std::map<std::string, int*> int_fields = {
    { "maximum_retries", &p.maximum_retries },
    { "corrector_maximum_iterations", &p.corrector_maximum_iterations },
    { "maximum_accepted_steps", &p.maximum_accepted_steps },
    { "maximum_evaluations", &p.maximum_evaluations },
    { "closure_minimum_steps", &p.closure_minimum_steps },
    { "closure_maximum_iterations", &p.closure_maximum_iterations },
    { "initial_tangent_sign", &p.initial_tangent_sign },
  };
  for (const auto& [key, value] : j.items()) {
    if (auto it = real_fields.find(key); it != real_fields.end()) {
      *it->second = value.get<double>();
    } else if (auto jt = int_fields.find(key); jt != int_fields.end()) {
      *jt->second = value.get<int>();
    } else if (key == "rotation_tolerance") {
      // The kernel's rotation gate is the constant kRotationTolerance; the exact pin is deliberate, so a
      // constant change has to be re-agreed with LI rather than drift inside a tolerance.
      EXPECT_EQ(value.get<double>(), kRotationTolerance) << key;
    } else if (key == "dtype") {
      EXPECT_EQ(value.get<std::string>(), "float64") << key;  // the kernel computes in double only
    } else if (key == "sample_retention") {
      EXPECT_EQ(value.get<std::string>(), "all") << key;  // TraceResult keeps every accepted sample
    } else if (key == "diagnostic_level") {
      // Diagnostics are wave 2 and not part of v0 fixtures (LI section 8); the level changes what
      // LI records, not the trace. Pinned so a different level forces a look at this mapping.
      EXPECT_EQ(value.get<std::string>(), "full") << key;
    } else {
      ADD_FAILURE() << "continuation field " << key << " has no mapping in this reader";
    }
  }
  EXPECT_TRUE(ValidateParams(p));
  return p;
}

// The crystal and face sequence resolved to the kernels' tables. Owns the tables that IcePathMap
// and IceDiscovery point into.
struct Scene {
  FaceNormalTable table;
  FacePolygonTable polygons;
  std::vector<int> slots;
  double refractive_index = 0.0;
  double incident[3]{};

  bool Init(const Json& input) {
    const LUMICE_ANALYTIC_Crystal crystal = CrystalOf(input.at("crystal"));
    if (BuildFaceNormals(crystal, &table, &polygons) != Status::kOk) {
      ADD_FAILURE() << "BuildFaceNormals rejected the fixture crystal";
      return false;
    }
    const std::vector<int> faces = input.at("faces").get<std::vector<int>>();
    slots.resize(faces.size());
    if (ResolveFaceSequence(table, faces.data(), static_cast<int>(faces.size()), slots.data()) != Status::kOk) {
      ADD_FAILURE() << "ResolveFaceSequence rejected the fixture faces";
      return false;
    }
    refractive_index = input.at("refractive_index").get<double>();
    const std::vector<double> s = Numbers(input.at("incident_direction"));
    std::copy(s.begin(), s.end(), incident);
    return true;
  }
  int Count() const { return static_cast<int>(slots.size()); }
};

// ------------------------------------------------------------------------------------------------
// Comparison geometry, restated from LI parity_export (independent of src/analytic/so3.hpp)
// ------------------------------------------------------------------------------------------------

// M = A^T B.
Pose RelativeRotation(const Pose& a, const Pose& b) {
  Pose m{};
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      for (int k = 0; k < 3; k++) {
        m[i * 3 + j] += a[k * 3 + i] * b[k * 3 + j];
      }
    }
  }
  return m;
}

Pose Product(const Pose& a, const Pose& b) {
  Pose m{};
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      for (int k = 0; k < 3; k++) {
        m[i * 3 + j] += a[i * 3 + k] * b[k * 3 + j];
      }
    }
  }
  return m;
}

// vee((M - M^T)/2) and clip((tr M - 1)/2).
void SkewAndCosine(const Pose& m, double skew[3], double* cosine) {
  skew[0] = 0.5 * (m[7] - m[5]);
  skew[1] = 0.5 * (m[2] - m[6]);
  skew[2] = 0.5 * (m[3] - m[1]);
  *cosine = std::clamp((m[0] + m[4] + m[8] - 1.0) / 2.0, -1.0, 1.0);
}

// angle(A^T B) = atan2(|vee(skew)|, (tr - 1)/2) (LI section 4).
double RotationDistance(const Pose& a, const Pose& b) {
  double skew[3];
  double cosine = 0.0;
  SkewAndCosine(RelativeRotation(a, b), skew, &cosine);
  return std::atan2(std::sqrt(skew[0] * skew[0] + skew[1] * skew[1] + skew[2] * skew[2]), cosine);
}

// parity_export._log_rotations: rotation vector of a rotation below pi.
std::array<double, 3> LogRotation(const Pose& m) {
  double skew[3];
  double cosine = 0.0;
  SkewAndCosine(m, skew, &cosine);
  const double sine = std::sqrt(skew[0] * skew[0] + skew[1] * skew[1] + skew[2] * skew[2]);
  const double scale = sine > 1e-15 ? std::atan2(sine, cosine) / sine : 1.0;
  return { skew[0] * scale, skew[1] * scale, skew[2] * scale };
}

// parity_export._exp_rotations: Rodrigues.
Pose ExpRotation(const std::array<double, 3>& w) {
  const double angle = std::sqrt(w[0] * w[0] + w[1] * w[1] + w[2] * w[2]);
  const double a = angle > 1e-15 ? std::sin(angle) / angle : 1.0;
  const double b = angle > 1e-15 ? (1.0 - std::cos(angle)) / (angle * angle) : 0.5;
  const Pose hat = { 0.0, -w[2], w[1], w[2], 0.0, -w[0], -w[1], w[0], 0.0 };
  const Pose hat2 = Product(hat, hat);
  Pose out{};
  for (int i = 0; i < 9; i++) {
    out[i] = (i % 4 == 0 ? 1.0 : 0.0) + a * hat[i] + b * hat2[i];
  }
  return out;
}

// parity_export.densify_curve: the geodesic polyline through `poses`, sampled at most `spacing`
// apart; a closed curve gets the chord from its last pose back to its first.
Curve Densify(const Curve& poses, bool closed, double spacing) {
  Curve p = poses;
  if (closed && p.size() > 1) {
    p.push_back(p.front());
  }
  if (p.size() < 2) {
    return p;
  }
  Curve out;
  for (size_t i = 0; i + 1 < p.size(); i++) {
    const std::array<double, 3> step = LogRotation(RelativeRotation(p[i], p[i + 1]));
    const double norm = std::sqrt(step[0] * step[0] + step[1] * step[1] + step[2] * step[2]);
    const int count = std::max(1, static_cast<int>(std::ceil(norm / spacing)));
    for (int k = 0; k < count; k++) {
      const double f = static_cast<double>(k) / count;
      out.push_back(Product(p[i], ExpRotation({ f * step[0], f * step[1], f * step[2] })));
    }
  }
  out.push_back(p.back());
  return out;
}

double DistanceToSamples(const Pose& r, const Curve& samples) {
  double best = std::numeric_limits<double>::infinity();
  for (const Pose& q : samples) {
    best = std::min(best, RotationDistance(r, q));
  }
  return best;
}

// parity_export.curve_distance: symmetric Hausdorff distance, each curve's poses against the other
// curve densified.
double CurveDistance(const Curve& a, bool a_closed, const Curve& b, bool b_closed) {
  const Curve dense_a = Densify(a, a_closed, kDensifySpacing);
  const Curve dense_b = Densify(b, b_closed, kDensifySpacing);
  double a_to_b = 0.0;
  for (const Pose& p : a) {
    a_to_b = std::max(a_to_b, DistanceToSamples(p, dense_b));
  }
  double b_to_a = 0.0;
  for (const Pose& q : b) {
    b_to_a = std::max(b_to_a, DistanceToSamples(q, dense_a));
  }
  return std::max(a_to_b, b_to_a);
}

// LI section 3.2: the second trace's poses after the seed, reversed, then the first trace's poses.
Curve Stitch(const Curve& first, const Curve& second) {
  Curve out;
  for (size_t i = second.size(); i-- > 1;) {
    out.push_back(second[i]);
  }
  out.insert(out.end(), first.begin(), first.end());
  return out;
}

// ------------------------------------------------------------------------------------------------
// Names of kernel enums as LI writes them
// ------------------------------------------------------------------------------------------------

std::string StatusName(FiberStatus s) {
  switch (s) {
    case FiberStatus::kClosed:
      return "closed";
    case FiberStatus::kEventTerminated:
      return "event_terminated";
    case FiberStatus::kNumericalFailure:
      return "numerical_failure";
    case FiberStatus::kBudgetExhausted:
      return "budget_exhausted";
  }
  return "?";
}

std::string ReasonName(FiberReason r) {
  switch (r) {
    case FiberReason::kClosedLoop:
      return "closed_loop";
    case FiberReason::kTirBoundary:
      return "tir_boundary";
    case FiberReason::kBranchBoundary:
      return "branch_boundary";
    case FiberReason::kPathInfeasible:
      return "path_infeasible";
    case FiberReason::kVisibilityBoundary:
      return "visibility_boundary";
    case FiberReason::kChartBoundary:
      return "chart_boundary";
    case FiberReason::kRankLoss:
      return "rank_loss";
    case FiberReason::kTopologyAmbiguity:
      return "topology_ambiguity";
    case FiberReason::kCorrectorFailure:
      return "corrector_failure";
    case FiberReason::kLinearSolveFailure:
      return "linear_solve_failure";
    case FiberReason::kNonFinite:
      return "non_finite";
    case FiberReason::kStepUnderflow:
      return "step_underflow";
    case FiberReason::kInvalidNumericalInput:
      return "invalid_numerical_input";
    case FiberReason::kStepBudget:
      return "step_budget";
    case FiberReason::kArclengthBudget:
      return "arclength_budget";
    case FiberReason::kEvaluationBudget:
      return "evaluation_budget";
  }
  return "?";
}

// discovery.DISCOVERY_EVENT_NAMES, the entry that classified an incomplete candidate.
std::string CauseName(IncompleteCause c) {
  switch (c) {
    case IncompleteCause::kArcBackwardFailed:
      return "arc_backward_failed";
    case IncompleteCause::kArcBackwardClosedAnomaly:
      return "arc_backward_closed_anomaly";
    case IncompleteCause::kUnnamedEvent:
      return "incomplete_unnamed_event";
    case IncompleteCause::kNotConverged:
      return "incomplete_not_converged";
  }
  return "?";
}

Curve PosesOf(const TraceResult& t) {
  Curve out(t.PoseCount());
  for (size_t i = 0; i < out.size(); i++) {
    std::copy(t.poses.begin() + 9 * i, t.poses.begin() + 9 * (i + 1), out[i].begin());
  }
  return out;
}

double Length(const TraceResult& t) {
  double sum = 0.0;
  for (double x : t.arclength_increments) {
    sum += x;
  }
  return sum;
}

double MaxAbsDiff(const std::vector<double>& got, const std::vector<double>& expected) {
  if (got.size() != expected.size()) {
    return std::numeric_limits<double>::infinity();
  }
  double e = 0.0;
  for (size_t i = 0; i < got.size(); i++) {
    e = std::max(e, std::fabs(got[i] - expected[i]));
  }
  return e;
}

// ------------------------------------------------------------------------------------------------
// The recipe's own geometry, so a broken ruler cannot pass as a green parity
// ------------------------------------------------------------------------------------------------

TEST(LiParityRecipe, RotationDistanceOfKnownRotations) {
  const Pose identity = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
  const Pose quarter_z = { 0, -1, 0, 1, 0, 0, 0, 0, 1 };
  EXPECT_EQ(RotationDistance(identity, identity), 0.0);
  EXPECT_NEAR(RotationDistance(identity, quarter_z), kPi / 2, 1e-15);
  EXPECT_NEAR(RotationDistance(quarter_z, identity), kPi / 2, 1e-15);
  const Pose r = ExpRotation({ 0.3, -0.2, 0.5 });
  EXPECT_NEAR(RotationDistance(identity, r), std::sqrt(0.09 + 0.04 + 0.25), 1e-14);
  const std::array<double, 3> w = LogRotation(r);
  EXPECT_NEAR(w[0], 0.3, 1e-14);
  EXPECT_NEAR(w[1], -0.2, 1e-14);
  EXPECT_NEAR(w[2], 0.5, 1e-14);
}

// Samples of one great circle about a fixed axis, starting at `phase`, `count` of them over `span`.
Curve Circle(double phase, double span, int count, bool include_end) {
  Curve out;
  const int n = include_end ? count - 1 : count;
  for (int i = 0; i < count; i++) {
    const double t = phase + span * i / n;
    out.push_back(Product(ExpRotation({ 0.1, 0.2, 0.3 }), ExpRotation({ 0.6 * t, 0.0, 0.8 * t })));
  }
  return out;
}

TEST(LiParityRecipe, CurveDistanceIsZeroOnItselfAndSmallAcrossSamplings) {
  const Curve a = Circle(0.0, 2 * kPi, 60, false);
  EXPECT_LT(CurveDistance(a, true, a, true), 1e-12);
  // Two samplings of one closed curve (different phase and count) differ by at most half the
  // densification spacing — LI section 4's own property statement.
  const Curve b = Circle(0.037, 2 * kPi, 97, false);
  EXPECT_LE(CurveDistance(a, true, b, true), kDensifySpacing / 2);
  // Displaced by 0.05 rad perpendicular to the curve's axis (0.6, 0, 0.8), it reads 0.05, not
  // zero: the ruler can go red. (A displacement with a component along the axis would partly slide
  // along the curve and read less.)
  Curve shifted;
  for (const Pose& p : a) {
    shifted.push_back(Product(p, ExpRotation({ 0.05 * 0.8, 0.0, -0.05 * 0.6 })));
  }
  EXPECT_NEAR(CurveDistance(a, true, shifted, true), 0.05, 1e-5);
}

TEST(LiParityRecipe, StitchIsOrientationFree) {
  // One open arc traced from an interior seed both ways (LI section 3.2), stitched in either order,
  // is one curve: the recipe must not care which direction a backend calls +1.
  const Curve arc = Circle(-0.8, 2.0, 41, true);
  const int seed = 16;
  const Curve forward(arc.begin() + seed, arc.end());
  Curve backward;
  for (int i = seed; i >= 0; i--) {
    backward.push_back(arc[i]);
  }
  const Curve one = Stitch(forward, backward);
  const Curve other = Stitch(backward, forward);
  EXPECT_EQ(one.size(), arc.size());
  EXPECT_LT(CurveDistance(one, false, arc, false), 1e-12);
  EXPECT_LT(CurveDistance(other, false, arc, false), 1e-12);
  // A one-pose side contributes nothing beyond the seed.
  EXPECT_EQ(Stitch(arc, Curve{ arc.front() }).size(), arc.size());
}

// ------------------------------------------------------------------------------------------------
// The fixture set: manifest, provenance, symmetry semantics
// ------------------------------------------------------------------------------------------------

TEST(LiParityFixtures, ManifestMatchesDirectoryAndSource) {
  const Json manifest = LoadJson("manifest.json");
  const std::map<std::string, std::string> source = ReadSource();
  ASSERT_TRUE(source.count("li_rev")) << "SOURCE has no li_rev line";
  const std::string li_rev = source.at("li_rev");
  EXPECT_EQ(li_rev.size(), 40u) << "SOURCE li_rev must be a full SHA";

  EXPECT_EQ(manifest.value("format", ""), kFormat);
  EXPECT_EQ(manifest.value("schema_version", 0), kSchemaVersion);
  EXPECT_EQ(manifest.value("symmetry_semantics", ""), "none");
  EXPECT_EQ(manifest.at("provenance").at("li_rev").get<std::string>(), li_rev);
  EXPECT_TRUE(manifest.at("provenance").at("li_tracked_tree_clean").get<bool>());

  // The listed files are exactly the files present: nothing lost or added in a copy.
  std::set<std::string> listed;
  for (const std::string& f : ManifestFiles()) {
    EXPECT_TRUE(listed.insert(f).second) << "listed twice: " << f;
  }
  EXPECT_FALSE(listed.empty());
  // Every listed file must be replayed by one of the suites below. A fixture kind LI adds later would
  // otherwise pass provenance alone and be silently skipped; here it goes red and points at the reader.
  size_t replayed = 0;
  for (const char* kind : { "evaluate_path", "trace_fiber", "seed_search" }) {
    const size_t n = ManifestFilesOfKind(kind).size();
    EXPECT_GT(n, 0u) << "no fixture of kind " << kind;
    replayed += n;
  }
  EXPECT_EQ(replayed, listed.size())
      << "the manifest lists a fixture kind this reader does not replay: extend the reader first "
         "(doc/analytic-api.md, Parity with LI)";
  std::set<std::string> present;
  for (const auto& entry : std::filesystem::directory_iterator(FixtureDir())) {
    const std::string name = entry.path().filename().string();
    if (name != "manifest.json" && name != "SOURCE") {
      present.insert(name);
    }
  }
  EXPECT_EQ(listed, present);

  // A skipped fixture is a legal input, not a missing file: it carries its reason, and its cell
  // lists no file it did not export. On this matrix that is 3-5-6-7__critical (D_P has no interior
  // extremum on the canonical column — a physical fact, LI section 6).
  for (const Json& cell : manifest.at("cells")) {
    for (const Json& s : cell.at("skipped")) {
      EXPECT_FALSE(s.value("reason", "").empty()) << cell.at("name");
      EXPECT_TRUE(s.contains("fixture")) << cell.at("name");
      if (s.value("fixture", "") == "all") {
        EXPECT_TRUE(cell.at("files").empty()) << cell.at("name");
      }
    }
  }
}

class LiParityProvenance : public testing::TestWithParam<std::string> {};

TEST_P(LiParityProvenance, HeaderFieldsAndSymmetrySemantics) {
  const Json f = LoadJson(GetParam());
  const std::string li_rev = ReadSource().at("li_rev");
  EXPECT_EQ(f.value("format", ""), kFormat);
  EXPECT_EQ(f.value("schema_version", 0), kSchemaVersion)
      << "schema changed in LI: update this reader first (doc/analytic-api.md, Parity with LI)";
  // doc/analytic-api.md section 3.3 rule 2: every fixture is one concrete face sequence.
  EXPECT_EQ(f.value("symmetry_semantics", ""), "none");
  EXPECT_EQ(f.at("provenance").at("li_rev").get<std::string>(), li_rev);
  EXPECT_TRUE(f.at("provenance").at("li_tracked_tree_clean").get<bool>());
  const std::string kind = f.value("fixture_kind", "");
  EXPECT_NE(GetParam().find("__" + kind), std::string::npos) << "file name and fixture_kind disagree";
}

INSTANTIATE_TEST_SUITE_P(All, LiParityProvenance, testing::ValuesIn(ManifestFiles()), CaseName);

// ------------------------------------------------------------------------------------------------
// evaluate_path (LI section 3.1)
// ------------------------------------------------------------------------------------------------

class LiParityEvaluatePath : public testing::TestWithParam<std::string> {};

TEST_P(LiParityEvaluatePath, MatchesLi) {
  const Json f = LoadJson(GetParam());
  ASSERT_EQ(f.value("fixture_kind", ""), "evaluate_path");
  const Json& input = f.at("input");
  const Json& expected = f.at("expected");
  Scene scene;
  ASSERT_TRUE(scene.Init(input));
  const std::vector<double> pose = Numbers(input.at("pose"));
  ASSERT_EQ(pose.size(), 9u);

  const int n = scene.Count();
  std::vector<double> segments(3 * (static_cast<size_t>(n) + 1));
  std::vector<double> transmittances(n);
  PathOutputs out{};
  out.segment_directions = segments.data();
  out.interface_transmittances = transmittances.data();
  const bool valid =
      EvaluatePath(scene.table, scene.slots.data(), n, scene.refractive_index, scene.incident, pose.data(), &out);

  const bool expected_valid = expected.at("valid").get<bool>();
  ASSERT_EQ(Tolerance(f, "valid"), 0.0);
  EXPECT_EQ(valid, expected_valid);
  if (valid && expected_valid) {
    const struct {
      const char* key;
      std::vector<double> got;
    } quantities[] = {
      { "outgoing_direction", { out.outgoing_direction, out.outgoing_direction + 3 } },
      { "segment_directions", segments },
      { "interface_transmittances", transmittances },
      { "fresnel_transmission", { out.fresnel_transmission } },
    };
    for (const auto& q : quantities) {
      const double error = MaxAbsDiff(q.got, Numbers(expected.at(q.key)));
      const double tolerance = Tolerance(f, q.key);
      Report(GetParam(), q.key, error, tolerance);
      EXPECT_LE(error, tolerance) << q.key;
    }
  } else {
    // An invalid pose compares only `valid` and the Fresnel factor, exactly (LI verifier atol 0).
    const double error = std::fabs(out.fresnel_transmission - expected.at("fresnel_transmission").get<double>());
    Report(GetParam(), "fresnel_transmission(invalid)", error, 0.0);
    EXPECT_EQ(error, 0.0);
  }
}

INSTANTIATE_TEST_SUITE_P(All, LiParityEvaluatePath, testing::ValuesIn(ManifestFilesOfKind("evaluate_path")), CaseName);

// ------------------------------------------------------------------------------------------------
// trace_fiber (LI section 3.2)
// ------------------------------------------------------------------------------------------------

struct Traced {
  std::vector<TraceResult> traces;
  Curve curve;
  bool closed = false;
};

// LI parity_export.run_traces: the forward trace with the fixture's sign, then the reversed one
// unless the forward trace closed; the curve is the closed trace or the stitched arc.
Traced RunTraces(const IcePathMap& map, const TargetChart& chart, const double seed[9], const ContinuationParams& p) {
  Traced t;
  t.traces.push_back(TraceFiber(map, chart, seed, p));
  if (t.traces[0].status == FiberStatus::kClosed) {
    t.closed = true;
    t.curve = PosesOf(t.traces[0]);
    return t;
  }
  ContinuationParams reversed = p;
  reversed.initial_tangent_sign = -p.initial_tangent_sign;
  t.traces.push_back(TraceFiber(map, chart, seed, reversed));
  t.curve = Stitch(PosesOf(t.traces[0]), PosesOf(t.traces[1]));
  return t;
}

class LiParityTraceFiber : public testing::TestWithParam<std::string> {};

TEST_P(LiParityTraceFiber, MatchesLi) {
  const Json f = LoadJson(GetParam());
  ASSERT_EQ(f.value("fixture_kind", ""), "trace_fiber");
  const Json& input = f.at("input");
  const Json& expected_traces = f.at("expected").at("traces");
  Scene scene;
  ASSERT_TRUE(scene.Init(input));
  const ContinuationParams params = ParamsOf(input.at("continuation"));
  const std::vector<double> seed = Numbers(input.at("seed_pose"));
  const std::vector<double> target = Numbers(input.at("target_direction"));
  ASSERT_EQ(seed.size(), 9u);
  ASSERT_EQ(target.size(), 3u);

  const auto start = std::chrono::steady_clock::now();
  const IcePathMap map(scene.table, scene.slots.data(), scene.Count(), scene.refractive_index, scene.incident);
  const Traced got = RunTraces(map, MakeTargetChart(target.data()), seed.data(), params);

  // Trace count and the (status, reason) multiset.
  EXPECT_EQ(got.traces.size(), expected_traces.size());
  std::vector<std::pair<std::string, std::string>> mine;
  std::vector<std::pair<std::string, std::string>> theirs;
  for (const TraceResult& t : got.traces) {
    mine.emplace_back(StatusName(t.status), ReasonName(t.reason));
  }
  for (const Json& t : expected_traces) {
    theirs.emplace_back(t.at("status").get<std::string>(), t.at("reason").get<std::string>());
  }
  std::sort(mine.begin(), mine.end());
  std::sort(theirs.begin(), theirs.end());
  ASSERT_EQ(Tolerance(f, "status_reason"), 0.0);
  EXPECT_EQ(mine, theirs);

  // The curve, orientation-free.
  Curve reference;
  bool reference_closed = false;
  if (expected_traces.at(0).at("status").get<std::string>() == "closed") {
    reference_closed = true;
    reference = Poses(expected_traces.at(0).at("poses"));
  } else if (expected_traces.size() >= 2) {
    reference = Stitch(Poses(expected_traces.at(0).at("poses")), Poses(expected_traces.at(1).at("poses")));
  } else {
    reference = Poses(expected_traces.at(0).at("poses"));
  }
  const double distance = CurveDistance(got.curve, got.closed, reference, reference_closed);
  Report(GetParam(), "curve_distance_rad", distance, Tolerance(f, "curve_distance_rad"));
  EXPECT_LE(distance, Tolerance(f, "curve_distance_rad"));

  // Summed arclength, relative.
  double length = 0.0;
  for (const TraceResult& t : got.traces) {
    length += Length(t);
  }
  double reference_length = 0.0;
  for (const Json& t : expected_traces) {
    reference_length += t.at("arclength").get<double>();
  }
  const double rtol = Tolerance(f, "arclength_relative");
  Report(GetParam(), "arclength_relative", std::fabs(length - reference_length) / reference_length, rtol);
  EXPECT_TRUE(ArclengthWithin(length, reference_length, rtol)) << "arclength " << length << " vs " << reference_length;

  // Every residual within the bound (a bound on this backend, not an equality with LI).
  double worst = 0.0;
  for (const TraceResult& t : got.traces) {
    for (double r : t.residual_norms) {
      worst = std::max(worst, r);
    }
  }
  Report(GetParam(), "residual_norm_bound", worst, Tolerance(f, "residual_norm_bound"));
  EXPECT_LE(worst, Tolerance(f, "residual_norm_bound"));

  const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  std::cout << "[li-parity] " << GetParam() << " elapsed_ms=" << ms << '\n';
}

INSTANTIATE_TEST_SUITE_P(All, LiParityTraceFiber, testing::ValuesIn(ManifestFilesOfKind("trace_fiber")), CaseName);

// ------------------------------------------------------------------------------------------------
// seed_search (LI section 3.3): contract 9.5.3-9.5.5 replayed on LI's exported band
// ------------------------------------------------------------------------------------------------

class LiParitySeedSearch : public testing::TestWithParam<std::string> {};

TEST_P(LiParitySeedSearch, MatchesLi) {
  const Json f = LoadJson(GetParam());
  ASSERT_EQ(f.value("fixture_kind", ""), "seed_search");
  const Json& input = f.at("input");
  const Json& expected = f.at("expected");
  Scene scene;
  ASSERT_TRUE(scene.Init(input));
  const ContinuationParams params = ParamsOf(input.at("continuation"));
  const std::vector<double> target = Numbers(input.at("target_direction"));
  ASSERT_EQ(target.size(), 3u);
  // v0 fixtures carry no extra seeds (LI section 3.3); a later export that does needs this reader
  // to pass them, so fail rather than drop them.
  ASSERT_TRUE(input.at("extra_seeds").empty()) << "extra_seeds are not replayed by this reader yet";

  // The band in pool order. BandEvent::index only breaks deviation ties when a band is built and
  // sorted (BuildBand); on replay the order is LI's, so the position stands in for it.
  const Json& sample = input.at("sample");
  const std::vector<double> band_u = Numbers(sample.at("band_u"));
  const std::vector<double> band_phi = Numbers(sample.at("band_phi"));
  const std::vector<double> band_d = Numbers(sample.at("band_deviation"));
  ASSERT_EQ(band_u.size(), 3 * band_d.size());
  ASSERT_EQ(band_phi.size(), 3 * band_d.size());
  std::vector<BandEvent> band(band_d.size());
  for (size_t i = 0; i < band.size(); i++) {
    band[i].index = static_cast<int>(i);
    band[i].deviation = band_d[i];
    for (int k = 0; k < 3; k++) {
      band[i].u[k] = band_u[3 * i + k];
      band[i].phi[k] = band_phi[3 * i + k];
    }
  }

  const auto start = std::chrono::steady_clock::now();
  IceDiscovery discovery(scene.table, scene.polygons, scene.slots.data(), scene.Count(), scene.refractive_index,
                         scene.incident);
  const DiscoveryOutput out =
      discovery.DiscoverOnBand(target.data(), band, nullptr, 0, input.at("cluster_radius_rad").get<double>(),
                               input.at("distance_threshold").get<double>(), params);

  // Completeness, the four funnel counts and the six counters: exact.
  ASSERT_EQ(Tolerance(f, "counts_and_events"), 0.0);
  EXPECT_EQ(out.Complete() ? "complete" : "unknown", expected.at("completeness").get<std::string>());
  EXPECT_EQ(out.pool_count, expected.at("pool_count").get<int>());
  EXPECT_EQ(out.extra_seed_count, expected.at("extra_seed_count").get<int>());
  EXPECT_EQ(out.raw_cluster_count, expected.at("raw_cluster_count").get<int>());
  EXPECT_EQ(out.admissible_count, expected.at("admissible_count").get<int>());
  const std::map<std::string, int> events = {
    { "arc_backward_closed_anomaly", out.arc_backward_closed_anomaly },
    { "arc_backward_failed", out.arc_backward_failed },
    { "arc_stitched", out.arc_stitched },
    { "dedup_merged", out.dedup_merged },
    { "incomplete_not_converged", out.incomplete_not_converged },
    { "incomplete_unnamed_event", out.incomplete_unnamed_event },
  };
  using Counters = std::map<std::string, int>;
  EXPECT_EQ(events, expected.at("events").get<Counters>());

  // The incomplete causes, in order.
  std::vector<std::string> causes;
  for (const IncompleteCandidate& c : out.incomplete) {
    causes.push_back(CauseName(c.cause));
  }
  std::vector<std::string> expected_causes;
  for (const Json& c : expected.at("incomplete")) {
    expected_causes.push_back(c.at("cause").get<std::string>());
  }
  EXPECT_EQ(causes, expected_causes);

  // The components, in trace order. An empty list is a legal expectation: the
  // 13-15-26-28__near_boundary target is outside the lit range, and a seed search must not invent
  // a component there (LI section 6).
  const Json& components = expected.at("components");
  ASSERT_EQ(out.components.size(), components.size());
  ASSERT_EQ(Tolerance(f, "kind_status_reasons"), 0.0);
  for (size_t i = 0; i < out.components.size(); i++) {
    SCOPED_TRACE("component " + std::to_string(i));
    const DiscoveredComponent& mine = out.components[i];
    const Json& ref = components.at(i);
    const bool closed = ref.at("kind").get<std::string>() == "closed";
    EXPECT_EQ(mine.kind == ComponentKind::kClosed ? "closed" : "arc", ref.at("kind").get<std::string>());
    EXPECT_EQ(StatusName(mine.forward.status), ref.at("status").get<std::string>());

    std::vector<std::string> ends = { ReasonName(mine.forward.reason) };
    if (mine.kind == ComponentKind::kArc) {
      ends.push_back(ReasonName(mine.backward.reason));
    }
    std::vector<std::string> ref_ends = { ref.at("reason").get<std::string>() };
    if (!ref.at("start_reason").is_null()) {
      ref_ends.push_back(ref.at("start_reason").get<std::string>());
    }
    std::sort(ends.begin(), ends.end());
    std::sort(ref_ends.begin(), ref_ends.end());
    EXPECT_EQ(ends, ref_ends);

    const Curve reference = Poses(ref.at("curve_poses"));
    const std::string tag = "component" + std::to_string(i) + ".";
    Pose seed;
    std::copy(mine.seed, mine.seed + 9, seed.begin());
    const double seed_distance = DistanceToSamples(seed, reference);
    Report(GetParam(), tag + "seed_to_curve_rad", seed_distance, Tolerance(f, "seed_to_curve_rad"));
    EXPECT_LE(seed_distance, Tolerance(f, "seed_to_curve_rad"));

    const Curve curve = closed ? PosesOf(mine.forward) : Stitch(PosesOf(mine.forward), PosesOf(mine.backward));
    const double distance = CurveDistance(curve, closed, reference, closed);
    Report(GetParam(), tag + "curve_distance_rad", distance, Tolerance(f, "curve_distance_rad"));
    EXPECT_LE(distance, Tolerance(f, "curve_distance_rad"));

    // An arc's length depends on its seed (section 9.5.6), so only closed lengths are compared.
    if (closed) {
      const double length = Length(mine.forward);
      const double reference_length = ref.at("arclength").get<double>();
      const double rtol = Tolerance(f, "closed_arclength_relative");
      Report(GetParam(), tag + "closed_arclength_relative", std::fabs(length - reference_length) / reference_length,
             rtol);
      EXPECT_TRUE(ArclengthWithin(length, reference_length, rtol))
          << "closed arclength " << length << " vs " << reference_length;
    }
  }

  const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  std::cout << "[li-parity] " << GetParam() << " elapsed_ms=" << ms << '\n';
}

INSTANTIATE_TEST_SUITE_P(All, LiParitySeedSearch, testing::ValuesIn(ManifestFilesOfKind("seed_search")), CaseName);

}  // namespace
}  // namespace lumice::analytic
