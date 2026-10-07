// The weight kinks (src/analytic/dp_weight_kink.hpp, LI dp_field.weight_kink ported for scrum
// 660.3) against the LI anchors of the task's dump (LI commit 4d0184c6): the single-mirror slab
// closed form over three indices, the arc ends on their named gates, the slab index derivatives,
// the marched cross-check, the Liljequist onset maximum, the exit-Snell-coincident arc's closure
// values, and the completeness-declaration fields (AC2) including the injected failing seed.
// Every tolerance is an assertion constant; the implementation carries none.
//
// symmetry_semantics: none — no symmetry reduction is involved.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "analytic/dp_boundary.hpp"
#include "analytic/dp_weight_kink.hpp"
#include "analytic/path_evaluation.hpp"

namespace lumice::analytic {
namespace {

constexpr double kPi = 3.14159265358979323846;

double Deg(double rad) {
  return rad * 180.0 / kPi;
}

double Dot3(const double a[3], const double b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
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

// The gates of `field` at u, by validity-subset position (the arc-end assertions read the gate a
// name maps to; the domain indices are the walk's currency).
std::vector<double> GatesAt(const DeviationField& field, const double u[3]) {
  double margins[kMaxFaceCount + 2];
  const int count = field.ValidityMarginsAt(u, margins);
  return std::vector<double>(margins, margins + count);
}

// The internal TIR discriminant of `step` of `field` at u (the domain margin vector's 2*step+1).
double DiscAt(const DeviationField& field, int step, const double u[3]) {
  double margins[2 * kMaxFaceCount];
  field.DomainMarginsAt(u, margins);
  return margins[2 * step + 1];
}

// The points of every arc of `curve`, concatenated (LI KinkCurve.points).
std::vector<double> AllPoints(const KinkCurve& curve) {
  std::vector<double> out;
  for (const KinkArc& arc : curve.arcs) {
    out.insert(out.end(), arc.points.begin(), arc.points.end());
  }
  return out;
}

// ---------------------------------------------------------------------------------------------
// The closed form on single-mirror slabs
// ---------------------------------------------------------------------------------------------

// LI test_single_mirror_slab_kink_is_one_level_of_d: C_1 is the small circle m . u = -sqrt(n^2-1)
// and D_P = 2 arcsin sqrt(n^2 - 1) all along it; the antisolar dark hole's radius is
// 180 deg minus that (probe_hole.py's table).
TEST(DPWeightKink, SingleMirrorSlabKinkIsOneLevelOfD) {
  const Fixture f(Prism());
  const int slabs[2][3] = { { 3, 1, 6 }, { 1, 3, 2 } };
  const double indices[3] = { 1.307, 1.31, 1.317 };
  const double hole_radius_deg[3] = { 65.4, 64.4, 62.0 };
  for (const auto& faces : slabs) {
    for (int k = 0; k < 3; k++) {
      const double index = indices[k];
      const DeviationField field = f.Field(faces, 3, index);
      const std::vector<KinkCurve> curves = WeightKinks(field, KinkOptions{});
      if (curves.size() != 1u) {
        ADD_FAILURE() << "expected one kink curve";
        continue;
      }
      const KinkCurve& curve = curves[0];
      // The value loop below indexes arcs[0] against AllPoints' full-arc walk: one arc is the
      // fixture's own premise, not an implementation accident.
      if (curve.arcs.size() != 1u) {
        ADD_FAILURE() << "expected one arc";
        continue;
      }
      // AC2: the closed form is the declared authority.
      EXPECT_EQ(curve.coverage, KinkCoverage::kClosedFormAuthority);
      EXPECT_EQ(MarginName(3, curve.margin), "internal_1_tir_discriminant");
      const std::vector<double> points = AllPoints(curve);
      if (points.size() / 3 <= 1000u) {
        ADD_FAILURE() << "expected a densely sampled circle";
        continue;
      }
      const double expected = 2.0 * std::asin(std::sqrt(index * index - 1.0));
      double values_min = 1e9;
      double values_max = -1e9;
      double disc_worst = 0.0;
      double gate_min = 1e9;
      double dot_worst = 0.0;
      for (size_t j = 0; j + 2 < points.size(); j += 3) {
        values_min = std::min(values_min, curve.arcs[0].values[j / 3]);
        values_max = std::max(values_max, curve.arcs[0].values[j / 3]);
        disc_worst = std::max(disc_worst, std::fabs(DiscAt(field, 1, &points[j])));
        for (double gate : GatesAt(field, &points[j])) {
          gate_min = std::min(gate_min, gate);
        }
        dot_worst =
            std::max(dot_worst, std::fabs(std::fabs(Dot3(&points[j], curve.normal)) - std::sqrt(index * index - 1.0)));
      }
      EXPECT_LT(std::fabs(values_max - expected), 1e-12);
      EXPECT_LT(values_max - values_min, 1e-12);  // spread
      EXPECT_LT(disc_worst, 1e-14);
      EXPECT_GT(gate_min, -1e-12);  // in the closure of U_P
      EXPECT_NEAR(180.0 - Deg(expected), hole_radius_deg[k], 0.05);
      EXPECT_LT(dot_worst, 1e-14);  // |p . m| = sqrt(n^2 - 1) on the circle
    }
  }
}

// LI test_kink_arc_ends_on_the_gate_it_names: an open arc's first and last point sit on the gate
// the corresponding end names (within 1e-9 of zero).
TEST(DPWeightKink, KinkArcEndsOnTheGateItNames) {
  const Fixture f(Prism());
  const int slabs[2][3] = { { 3, 1, 6 }, { 1, 3, 2 } };
  for (const auto& faces : slabs) {
    const DeviationField field = f.Field(faces, 3, 1.307);
    const std::vector<KinkCurve> curves = WeightKinks(field, KinkOptions{});
    const KinkCurve& curve = curves[0];
    for (const KinkArc& arc : curve.arcs) {
      EXPECT_FALSE(arc.closed);
      const double* ends[2] = { &arc.points[0], &arc.points[arc.points.size() - 3] };
      for (int e = 0; e < 2; e++) {
        if (arc.end_gates[e] < 0) {
          ADD_FAILURE() << "an open arc end names no gate";
          continue;
        }
        // The named gate is a domain margin index; its value at the end point is (nearly) zero.
        double margins[2 * kMaxFaceCount];
        field.DomainMarginsAt(ends[e], margins);
        EXPECT_LT(std::fabs(margins[arc.end_gates[e]]), 1e-9);
      }
    }
  }
}

// LI test_slab_index_derivatives: a slab's direction does not disperse (dD/dn exactly 0); on its
// kink d disc/dn = 2n > 0 (the blue side reflects totally).
TEST(DPWeightKink, SlabIndexDerivatives) {
  const Fixture f(Prism());
  const int faces[3] = { 3, 1, 6 };
  const double index = 1.31;
  const DeviationField field = f.Field(faces, 3, index);
  const std::vector<KinkCurve> curves = WeightKinks(field, KinkOptions{});
  const KinkCurve& curve = curves[0];
  const std::vector<double> points = AllPoints(curve);
  for (size_t j = 0; j + 2 < points.size(); j += 150) {  // every 50th point
    const FieldJet jet = field.Differentiate(&points[j]);
    EXPECT_EQ(jet.d_p_dn, 0.0);
    EXPECT_NEAR(jet.margins_dn[curve.margin], 2.0 * index, 1e-12 * 2.0 * index);
  }
}

// ---------------------------------------------------------------------------------------------
// The marched way
// ---------------------------------------------------------------------------------------------

// LI test_non_orthogonal_normal_is_marched: 3-7-5's m_1 . n_a = -1/2 — not a circle of the closed
// form; the walk stays on the zero set and ends on gates. AC2: the march is uncertified.
TEST(DPWeightKink, NonOrthogonalNormalIsMarched) {
  const Fixture f(Prism());
  const int faces[3] = { 3, 7, 5 };
  const DeviationField field = f.Field(faces, 3, 1.31);
  const std::vector<KinkCurve> curves = WeightKinks(field, KinkOptions{});
  ASSERT_EQ(curves.size(), 1u);
  const KinkCurve& curve = curves[0];
  EXPECT_EQ(curve.coverage, KinkCoverage::kMarchedUncertified);
  EXPECT_TRUE(curve.note.empty());
  EXPECT_FALSE(curve.arcs.empty());
  EXPECT_TRUE(curve.Complete());
  const std::vector<double> points = AllPoints(curve);
  double disc_worst = 0.0;
  double gate_min = 1e9;
  for (size_t j = 0; j + 2 < points.size(); j += 3) {
    disc_worst = std::max(disc_worst, std::fabs(DiscAt(field, 1, &points[j])));
    for (double gate : GatesAt(field, &points[j])) {
      gate_min = std::min(gate_min, gate);
    }
  }
  EXPECT_LT(disc_worst, 1e-12);
  EXPECT_GT(gate_min, -1e-12);
  for (const KinkArc& arc : curve.arcs) {
    const double* ends[2] = { &arc.points[0], &arc.points[arc.points.size() - 3] };
    for (int e = 0; e < 2; e++) {
      if (arc.end_gates[e] < 0) {
        ADD_FAILURE() << "an open arc end names no gate";
        continue;
      }
      double margins[2 * kMaxFaceCount];
      field.DomainMarginsAt(ends[e], margins);
      EXPECT_LT(std::fabs(margins[arc.end_gates[e]]), 1e-9);
    }
  }
}

// LI test_walk_agrees_with_the_closed_form_on_a_slab: the marched walk forced onto 3-1-6
// reproduces the closed-form circle (consistency; the closed form is the authority).
TEST(DPWeightKink, WalkAgreesWithTheClosedFormOnASlab) {
  const Fixture f(Prism());
  const int faces[3] = { 3, 1, 6 };
  const double index = 1.31;
  const DeviationField field = f.Field(faces, 3, index);
  const std::vector<KinkCurve> closed_curves = WeightKinks(field, KinkOptions{});
  const KinkCurve& closed_form = closed_curves[0];
  const KinkCurve marched = MarchedKink(field, 1, KinkOptions{});
  EXPECT_EQ(marched.coverage, KinkCoverage::kMarchedUncertified);
  ASSERT_FALSE(marched.arcs.empty());
  const double expected = 2.0 * std::asin(std::sqrt(index * index - 1.0));
  for (const KinkArc& arc : marched.arcs) {
    for (size_t j = 0; j + 2 < arc.points.size(); j += 3) {
      EXPECT_NEAR(Dot3(&arc.points[j], closed_form.normal), -std::sqrt(index * index - 1.0), 1e-12);
    }
    for (double value : arc.values) {
      EXPECT_NEAR(value, expected, 1e-12);
    }
  }
  // The walk covers the same arc: its ends are the closed form's ends.
  const KinkArc& reference = closed_form.arcs[0];
  const double ends[2][3] = { { reference.points[0], reference.points[1], reference.points[2] },
                              { reference.points[reference.points.size() - 3],
                                reference.points[reference.points.size() - 2],
                                reference.points[reference.points.size() - 1] } };
  const KinkArc& walked = marched.arcs[0];
  const double* walked_ends[2] = { &walked.points[0], &walked.points[walked.points.size() - 3] };
  for (int e = 0; e < 2; e++) {
    double best = 1e9;
    for (const auto& end : ends) {
      const double d[3] = { walked_ends[e][0] - end[0], walked_ends[e][1] - end[1], walked_ends[e][2] - end[2] };
      best = std::min(best, std::sqrt(Dot3(d, d)));
    }
    EXPECT_LT(best, 1e-6);
  }
}

// LI test_liljequist_onset_maximum_is_on_the_marched_kinks: 3-5-6-7-3 at h/a 2 — the largest D_P
// on each onset is the optimizer record's 153.0697 deg (the walk samples at 0.25 deg steps, so
// its maximum is below the optimizer's by at most the curvature over half a step).
TEST(DPWeightKink, LiljequistOnsetMaximumIsOnTheMarchedKinks) {
  const Fixture f(Prism());
  const int faces[5] = { 3, 5, 6, 7, 3 };
  const DeviationField field = f.Field(faces, 5, 1.31);
  const std::vector<KinkCurve> curves = WeightKinks(field, KinkOptions{});
  ASSERT_EQ(curves.size(), 3u);
  for (const KinkCurve& curve : curves) {
    EXPECT_EQ(curve.coverage, KinkCoverage::kMarchedUncertified);
    const std::vector<double> points = AllPoints(curve);
    double disc_worst = 0.0;
    for (size_t j = 0; j + 2 < points.size(); j += 3) {
      disc_worst = std::max(disc_worst, std::fabs(DiscAt(field, curve.step, &points[j])));
    }
    EXPECT_LT(disc_worst, 1e-12);
    double top = -1e9;
    for (const KinkArc& arc : curve.arcs) {
      for (double value : arc.values) {
        top = std::max(top, value);
      }
    }
    EXPECT_GT(Deg(top), 153.0697 - 5e-3);
    EXPECT_LE(Deg(top), 153.0697 + 1e-4);
  }
}

// LI test_marched_kink_values_on_the_exit_snell_coincident_arc: 3-5-6-7 at Lumice's n(550) — the
// internal-1 onset coincides with the exit-Snell boundary piece; every value is the closure
// limit, the arc carries the boundary walk's own exit-Snell piece's range, and the internal-2
// onset never enters U_P.
TEST(DPWeightKink, MarchedKinkValuesOnTheExitSnellCoincidentArc) {
  const Fixture f(Prism());
  const int faces[4] = { 3, 5, 6, 7 };
  const DeviationField field = f.Field(faces, 4, 1.3110129);
  KinkOptions options;
  options.lattice_n = 50000;
  const std::vector<KinkCurve> curves = WeightKinks(field, options);
  ASSERT_EQ(curves.size(), 2u);
  const KinkCurve& curve1 = curves[0];
  EXPECT_EQ(MarginName(4, curve1.margin), "internal_1_tir_discriminant");
  EXPECT_EQ(curve1.coverage, KinkCoverage::kMarchedUncertified);
  ASSERT_EQ(curve1.arcs.size(), 1u);
  EXPECT_TRUE(curve1.Complete());
  EXPECT_TRUE(curve1.note.empty());
  const KinkArc& arc = curve1.arcs[0];
  double minimum = 1e9;
  double maximum = -1e9;
  for (double value : arc.values) {
    EXPECT_TRUE(std::isfinite(value));
    minimum = std::min(minimum, value);
    maximum = std::max(maximum, value);
  }
  EXPECT_NEAR(Deg(minimum), 50.16174, 2e-5);
  EXPECT_GE(Deg(maximum), 162.3667);
  EXPECT_FALSE(arc.closed);
  EXPECT_EQ(arc.end_gates[0], kEntryIncidence);
  EXPECT_EQ(arc.end_gates[1], kEntryIncidence);
  EXPECT_TRUE(curves[1].arcs.empty());

  // The same values the boundary walk reports on its own exit-Snell piece (0.25 deg samples).
  BoundaryWalkRecord record;
  const WalkResult walk = WalkBoundary(field, BoundaryWalkOptions{}, &record);
  ASSERT_EQ(walk.status, WalkStatus::kOk) << walk.message;
  const BoundaryPiece* piece = nullptr;
  for (const BoundaryPiece& candidate : record.pieces) {
    if (candidate.margin == ExitSnellOf(4)) {
      piece = &candidate;
    }
  }
  ASSERT_NE(piece, nullptr);
  double piece_max = -1e9;
  for (double value : piece->values) {
    piece_max = std::max(piece_max, value);
  }
  EXPECT_NEAR(Deg(maximum), Deg(piece_max), 2e-3);
}

// LI test_a_b_path_has_no_kink and test_kinks_do_not_change_the_boundary.
TEST(DPWeightKink, AbPathHasNoKinkAndKinksStayOffTheBoundary) {
  const Fixture f(Prism());
  const int ab[2] = { 3, 5 };
  EXPECT_TRUE(WeightKinks(f.Field(ab, 2, 1.31), KinkOptions{}).empty());

  const int faces[3] = { 3, 1, 5 };
  const DeviationField field = f.Field(faces, 3, 1.31);
  const std::vector<KinkCurve> curves = WeightKinks(field, KinkOptions{});
  EXPECT_FALSE(curves.empty());
  BoundaryWalkRecord record;
  ASSERT_EQ(WalkBoundary(field, BoundaryWalkOptions{}, &record).status, WalkStatus::kOk);
  for (const BoundaryPiece& piece : record.pieces) {
    EXPECT_EQ(MarginName(3, piece.margin).find("_tir_discriminant"), std::string::npos);
  }
}

// LI test_index_above_sqrt_2_has_no_onset_on_the_sphere: n^2 - 1 >= 1 makes the closed-form
// circle empty by construction (said in the note), no NaN comparison anywhere.
TEST(DPWeightKink, IndexAboveSqrt2HasNoOnsetOnTheSphere) {
  const Fixture f(Prism());
  const int faces[3] = { 3, 1, 6 };
  const std::vector<KinkCurve> curves = WeightKinks(f.Field(faces, 3, 1.5), KinkOptions{});
  ASSERT_EQ(curves.size(), 1u);
  const KinkCurve& curve = curves[0];
  EXPECT_EQ(curve.coverage, KinkCoverage::kClosedFormAuthority);
  EXPECT_TRUE(curve.arcs.empty());
  EXPECT_NE(curve.note.find("n^2 - 1 >= 1"), std::string::npos);
  EXPECT_TRUE(curve.Complete());
  EXPECT_TRUE(std::isnan(curve.Spread()));
}

// ---------------------------------------------------------------------------------------------
// The three-wavelength anchors (the task's dump, LI commit 4d0184c6): the n literals are LI's
// float64 dispersion authority (refractive_index of spectrum/dispersion.py) — Lumice's own
// IceRefractiveIndex holds the coefficients as float and differs below 1e-8, so the tests pin
// the dump's values instead of re-implementing the formula.
// ---------------------------------------------------------------------------------------------

// C06: the beta crystal's 4-8-1-7-5 — the basal reflection's TIR onset is the closed-form small
// circle, D_P constant on it per wavelength (150.496 / 149.247 / 148.645 deg), the 400-700
// dispersion of the hole edge +1.851 deg (blue deeper inside the antisolar hole).
TEST(DPWeightKink, BasalTirOnsetConstantPerWavelength) {
  const Fixture f(Beta());
  const int faces[5] = { 4, 8, 1, 7, 5 };
  const double n[3] = { 1.3193340315881368, 1.3110129170742788, 1.3068763664637266 };  // 400 / 550 / 700 nm
  const double constant_deg[3] = { 150.496, 149.247, 148.645 };
  double onset[3] = {};
  for (int k = 0; k < 3; k++) {
    const DeviationField field = f.Field(faces, 5, n[k]);
    const std::vector<KinkCurve> curves = WeightKinks(field, KinkOptions{});
    if (curves.size() != 3u) {
      ADD_FAILURE() << "expected three kink curves";
      continue;
    }
    // The basal face is internal step 2 (face 1 of the sequence).
    const KinkCurve* basal = nullptr;
    for (const KinkCurve& curve : curves) {
      if (curve.step == 2) {
        basal = &curve;
      }
    }
    if (basal == nullptr) {
      ADD_FAILURE() << "no basal step found";
      continue;
    }
    EXPECT_EQ(basal->coverage, KinkCoverage::kClosedFormAuthority);
    if (basal->arcs.size() != 1u) {
      ADD_FAILURE() << "expected one basal arc";
      continue;
    }
    double lo = 1e9;
    double hi = -1e9;
    for (double value : basal->arcs[0].values) {
      lo = std::min(lo, value);
      hi = std::max(hi, value);
    }
    EXPECT_NEAR(Deg(hi), constant_deg[k], 1e-3);
    EXPECT_LT(Deg(hi - lo), 1e-9);  // constant along the circle
    onset[k] = Deg(hi);
  }
  EXPECT_NEAR(onset[0] - onset[2], 1.851, 2e-3);
}

// C02: the h/a-2 prism's 3-1-5 — the onset's D_P range per wavelength (the walk's own samples;
// the band the blue edge of the antisolar feature rides on), moving outward as n drops with
// wavelength (the dump's monotonicity check).
TEST(DPWeightKink, KinkSpanMovesWithWavelength) {
  const Fixture f(Prism());
  const int faces[3] = { 3, 1, 5 };
  const double n[3] = { 1.3193340315881368, 1.3110129170742788, 1.3068763664637266 };
  const double span_deg[3][2] = { { 141.003938, 143.652221 }, { 132.458136, 136.842378 }, { 129.364361, 134.255299 } };
  double measured_lo[3] = {};
  for (int k = 0; k < 3; k++) {
    const DeviationField field = f.Field(faces, 3, n[k]);
    const std::vector<KinkCurve> curves = WeightKinks(field, KinkOptions{});
    if (curves.size() != 1u || curves[0].arcs.empty()) {
      ADD_FAILURE() << "expected one non-empty kink curve";
      continue;
    }
    const KinkCurve& curve = curves[0];
    double lo = 1e9;
    double hi = -1e9;
    for (const KinkArc& arc : curve.arcs) {
      for (double value : arc.values) {
        lo = std::min(lo, value);
        hi = std::max(hi, value);
      }
    }
    EXPECT_NEAR(Deg(lo), span_deg[k][0], 2e-3);
    EXPECT_NEAR(Deg(hi), span_deg[k][1], 2e-3);
    measured_lo[k] = Deg(lo);
  }
  // The dump's monotonicity check, on the implementation's own output (the literal table above is
  // the anchor, not the subject — the NEAR bounds already carry it transitively).
  EXPECT_GT(measured_lo[0], measured_lo[1]);
  EXPECT_GT(measured_lo[1], measured_lo[2]);
}

// // ---------------------------------------------------------------------------------------------
// The failing-seam (AC2's truncation half): one seed's refusal is counted, reported, and does
// not stop the others (LI test_a_failed_seed_walk_does_not_stop_the_others, whose monkeypatch is
// the injected KinkBothWaysFn here).
// ---------------------------------------------------------------------------------------------

namespace {

struct InjectionState {
  int calls = 0;
};

KinkSeedArc FirstSeedFails(const BoundaryWalker& walker, const double start[3], int margin, const KinkOptions& options,
                           void* user) {
  auto* state = static_cast<InjectionState*>(user);
  state->calls++;
  if (state->calls == 1) {
    KinkSeedArc refusal;
    refusal.status = WalkStatus::kNotFinite;
    refusal.message = "injected walk failure";
    return refusal;
  }
  return KinkWalkBothWays(walker, start, margin, options, nullptr);
}

double MinChordToPoints(const double u[3], const std::vector<double>& points, size_t stride) {
  double best2 = 1e9;
  for (size_t k = 0; k + 2 < points.size(); k += 3 * stride) {
    const double dx = u[0] - points[k];
    const double dy = u[1] - points[k + 1];
    const double dz = u[2] - points[k + 2];
    best2 = std::min(best2, dx * dx + dy * dy + dz * dz);
  }
  return std::sqrt(best2);
}

}  // namespace

TEST(DPWeightKink, AFailedSeedWalkDoesNotStopTheOthers) {
  const Fixture f(Prism());
  const int faces[5] = { 3, 5, 6, 7, 3 };
  const DeviationField field = f.Field(faces, 5, 1.31);
  const KinkCurve reference = MarchedKink(field, 1, KinkOptions{});
  EXPECT_TRUE(reference.Complete());
  EXPECT_TRUE(reference.note.empty());
  EXPECT_FALSE(reference.arcs.empty());

  InjectionState state;
  const KinkCurve curve = MarchedKink(field, 1, KinkOptions{}, FirstSeedFails, &state);
  EXPECT_EQ(curve.failed_seeds, 1);
  EXPECT_FALSE(curve.Complete());
  EXPECT_NE(curve.note.find("1 seed walk(s) failed"), std::string::npos);
  EXPECT_NE(curve.note.find("injected walk failure"), std::string::npos);
  EXPECT_FALSE(curve.arcs.empty());
  EXPECT_GT(state.calls, 1);

  // The seeds after the failure recover most of the onset (not all: the failed seed's own stretch
  // may be lost) — the reference sampled every 20th point against the curve's every 5th.
  const std::vector<double> reference_points = AllPoints(reference);
  const std::vector<double> curve_points = AllPoints(curve);
  int recovered = 0;
  const int samples = static_cast<int>(reference_points.size() / 3 / 20);
  for (int k = 0; k < samples; k++) {
    if (MinChordToPoints(&reference_points[3 * 20 * static_cast<size_t>(k)], curve_points, 5) < 0.01) {
      recovered++;
    }
  }
  EXPECT_GT(static_cast<double>(recovered) / samples, 0.9);
}

}  // namespace
}  // namespace lumice::analytic
