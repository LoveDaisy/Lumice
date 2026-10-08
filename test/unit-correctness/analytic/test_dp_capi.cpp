// The version-8 C ABI translation of the u-S^2 field layer (src/analytic/dp_capi.cpp): return
// codes, the zero-fill after struct_size on every error, library-owned storage and its release,
// the open-enum slug fields against the kernels' own name tables, the fail-closed result shapes
// (a refused walk delivers an empty loop; the partition delivers no intervals without a walked
// loop or with an escape), and the anchor numbers of the scrum's verified fixtures replayed
// through the ABI (beta 4-8-7-5's partition, 3-1-6's dark-hole rim and its zero-spread kink, the
// rhombic plate's blue tint, the rank-0 point mass).
//
// The kernels' red states (synthetic escape regimes, refused walks) are construction seams of the
// kernel layer and cannot cross this surface by design; their invariants are asserted there
// (test_dp_partition.cpp) and their translation shape here on the green arms.
//
// symmetry_semantics: none — every call evaluates one concrete face sequence.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <future>
#include <string>
#include <vector>

#include "lumice_analytic_core.h"

namespace {

constexpr double kNBlue = 1.317;
constexpr double kNRed = 1.307;
constexpr double kN550 = 1.3110129;
constexpr double kPi = 3.14159265358979323846;

const int kFaces35[2] = { 3, 5 };

LUMICE_ANALYTIC_Crystal Prism(double height = 1.0) {
  LUMICE_ANALYTIC_Crystal c{};
  c.kind = LUMICE_ANALYTIC_CRYSTAL_PRISM;
  c.height = height;
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

// The rhombic plate of the class cells: fd = [1.5, 1, 1, 1.5, 1, 1] at h = 0.5.
LUMICE_ANALYTIC_Crystal RhombicPlate() {
  LUMICE_ANALYTIC_Crystal c{};
  c.kind = LUMICE_ANALYTIC_CRYSTAL_PRISM;
  c.height = 0.5;
  const double fd[6] = { 1.5, 1.0, 1.0, 1.5, 1.0, 1.0 };
  for (int i = 0; i < 6; i++) {
    c.face_distance[i] = fd[i];
  }
  return c;
}

LUMICE_ANALYTIC_PoseDensity RandomDensity() {
  LUMICE_ANALYTIC_PoseDensity density{};
  density.family = LUMICE_ANALYTIC_POSE_RANDOM;
  return density;
}

LUMICE_ANALYTIC_PoseDensity PlateDensityAtPole() {
  LUMICE_ANALYTIC_PoseDensity density{};
  density.family = LUMICE_ANALYTIC_POSE_PLATE;
  density.zenith_mean_deg = 0.0;
  density.zenith_std_deg = 1.0;
  return density;
}

double Deg(double rad) {
  return rad * 180.0 / kPi;
}

// ---------------------------------------------------------------------------------------------
// The error table: every call refuses the same inputs the same way, and every refusal leaves the
// result zero-filled after struct_size so Release is safe (twice) and the pointers are null.
// ---------------------------------------------------------------------------------------------

TEST(DpCapi, ErrorTableRefusesAndZeroFillsEveryCall) {
  LUMICE_ANALYTIC_Crystal crystal = Prism();
  LUMICE_ANALYTIC_Crystal collapsed = Prism(0.0);  // zero volume: the validity gate rejects it
  const int bad_faces[2] = { 3, 99 };
  const double bad_index = 0.0;
  LUMICE_ANALYTIC_PoseDensity bad_density{};
  bad_density.family = static_cast<LUMICE_ANALYTIC_PoseFamily>(99);

  {
    LUMICE_ANALYTIC_BoundaryLoopResult out{};
    out.struct_size = sizeof(uint32_t);
    EXPECT_EQ(LUMICE_ANALYTIC_TraceBoundaryLoop(&crystal, kFaces35, 2, 1.31, &out), LUMICE_ANALYTIC_ERR_INVALID_VALUE)
        << "a struct_size without a published prefix is refused whole";
    out.struct_size = sizeof(out);
    EXPECT_EQ(LUMICE_ANALYTIC_TraceBoundaryLoop(nullptr, kFaces35, 2, 1.31, &out), LUMICE_ANALYTIC_ERR_NULL_ARG);
    EXPECT_EQ(LUMICE_ANALYTIC_TraceBoundaryLoop(&crystal, nullptr, 2, 1.31, &out), LUMICE_ANALYTIC_ERR_NULL_ARG);
    EXPECT_EQ(LUMICE_ANALYTIC_TraceBoundaryLoop(&crystal, bad_faces, 2, 1.31, &out), LUMICE_ANALYTIC_ERR_INVALID_VALUE);
    EXPECT_EQ(LUMICE_ANALYTIC_TraceBoundaryLoop(&crystal, kFaces35, 1, 1.31, &out), LUMICE_ANALYTIC_ERR_INVALID_VALUE);
    EXPECT_EQ(LUMICE_ANALYTIC_TraceBoundaryLoop(&crystal, kFaces35, 2, bad_index, &out),
              LUMICE_ANALYTIC_ERR_INVALID_VALUE);
    EXPECT_EQ(LUMICE_ANALYTIC_TraceBoundaryLoop(&collapsed, kFaces35, 2, 1.31, &out),
              LUMICE_ANALYTIC_ERR_INVALID_CONFIG);
    EXPECT_EQ(out.status, 0);
    EXPECT_EQ(out.critical_point_count, 0);
    EXPECT_EQ(out.storage, nullptr);
    LUMICE_ANALYTIC_ReleaseBoundaryLoopResult(&out);
    LUMICE_ANALYTIC_ReleaseBoundaryLoopResult(&out);
    LUMICE_ANALYTIC_ReleaseBoundaryLoopResult(nullptr);
    EXPECT_EQ(out.critical_point_positions, nullptr);
  }
  {
    LUMICE_ANALYTIC_WeightKinksResult out{};
    out.struct_size = sizeof(out);
    EXPECT_EQ(LUMICE_ANALYTIC_TraceWeightKinks(nullptr, kFaces35, 2, 1.31, &out), LUMICE_ANALYTIC_ERR_NULL_ARG);
    EXPECT_EQ(LUMICE_ANALYTIC_TraceWeightKinks(&crystal, nullptr, 2, 1.31, &out), LUMICE_ANALYTIC_ERR_NULL_ARG);
    EXPECT_EQ(LUMICE_ANALYTIC_TraceWeightKinks(&crystal, bad_faces, 2, 1.31, &out), LUMICE_ANALYTIC_ERR_INVALID_VALUE);
    EXPECT_EQ(LUMICE_ANALYTIC_TraceWeightKinks(&crystal, kFaces35, 1, 1.31, &out), LUMICE_ANALYTIC_ERR_INVALID_VALUE);
    EXPECT_EQ(LUMICE_ANALYTIC_TraceWeightKinks(&crystal, kFaces35, 2, bad_index, &out),
              LUMICE_ANALYTIC_ERR_INVALID_VALUE);
    EXPECT_EQ(LUMICE_ANALYTIC_TraceWeightKinks(&collapsed, kFaces35, 2, 1.31, &out),
              LUMICE_ANALYTIC_ERR_INVALID_CONFIG);
    EXPECT_EQ(out.curve_count, 0);
    EXPECT_EQ(out.storage, nullptr);
    LUMICE_ANALYTIC_ReleaseWeightKinksResult(&out);
    LUMICE_ANALYTIC_ReleaseWeightKinksResult(&out);
  }
  {
    LUMICE_ANALYTIC_ClassificationResult out{};
    out.struct_size = sizeof(out);
    EXPECT_EQ(LUMICE_ANALYTIC_ClassifyCriticalStructure(nullptr, kFaces35, 2, 1.31, RandomDensity(), &out),
              LUMICE_ANALYTIC_ERR_NULL_ARG);
    EXPECT_EQ(LUMICE_ANALYTIC_ClassifyCriticalStructure(&crystal, kFaces35, 2, 1.31, bad_density, &out),
              LUMICE_ANALYTIC_ERR_INVALID_VALUE);
    EXPECT_EQ(LUMICE_ANALYTIC_ClassifyCriticalStructure(&crystal, kFaces35, 2, bad_index, RandomDensity(), &out),
              LUMICE_ANALYTIC_ERR_INVALID_VALUE);
    EXPECT_EQ(LUMICE_ANALYTIC_ClassifyCriticalStructure(&collapsed, kFaces35, 2, 1.31, RandomDensity(), &out),
              LUMICE_ANALYTIC_ERR_INVALID_CONFIG);
    EXPECT_EQ(out.storage, nullptr);
    EXPECT_EQ(out.onset_count, 0);
    LUMICE_ANALYTIC_ReleaseClassificationResult(&out);
    LUMICE_ANALYTIC_ReleaseClassificationResult(&out);
  }
  {
    LUMICE_ANALYTIC_PartitionResult out{};
    out.struct_size = sizeof(out);
    EXPECT_EQ(LUMICE_ANALYTIC_PartitionDeviationAxis(nullptr, kFaces35, 2, 1.31, &out), LUMICE_ANALYTIC_ERR_NULL_ARG);
    EXPECT_EQ(LUMICE_ANALYTIC_PartitionDeviationAxis(&crystal, bad_faces, 2, 1.31, &out),
              LUMICE_ANALYTIC_ERR_INVALID_VALUE);
    EXPECT_EQ(out.storage, nullptr);
    LUMICE_ANALYTIC_ReleasePartitionResult(&out);
    LUMICE_ANALYTIC_ReleasePartitionResult(&out);
  }
  {
    LUMICE_ANALYTIC_WavelengthTableResult out{};
    out.struct_size = sizeof(out);
    const double indices[2] = { kNRed, kNBlue };
    const double bad_indices[2] = { kNRed, 0.0 };
    const char* labels[2] = { "red", "blue" };
    EXPECT_EQ(
        LUMICE_ANALYTIC_TraceWavelengthCriticalTable(&crystal, kFaces35, 2, RandomDensity(), nullptr, indices, 2, &out),
        LUMICE_ANALYTIC_ERR_NULL_ARG);
    EXPECT_EQ(
        LUMICE_ANALYTIC_TraceWavelengthCriticalTable(&crystal, kFaces35, 2, RandomDensity(), labels, nullptr, 2, &out),
        LUMICE_ANALYTIC_ERR_NULL_ARG);
    EXPECT_EQ(
        LUMICE_ANALYTIC_TraceWavelengthCriticalTable(&crystal, kFaces35, 2, RandomDensity(), labels, indices, -1, &out),
        LUMICE_ANALYTIC_ERR_INVALID_VALUE);
    EXPECT_EQ(LUMICE_ANALYTIC_TraceWavelengthCriticalTable(&crystal, kFaces35, 2, RandomDensity(), labels, indices,
                                                           10000001, &out),
              LUMICE_ANALYTIC_ERR_INVALID_VALUE);
    EXPECT_EQ(LUMICE_ANALYTIC_TraceWavelengthCriticalTable(&crystal, kFaces35, 2, RandomDensity(), labels, bad_indices,
                                                           2, &out),
              LUMICE_ANALYTIC_ERR_INVALID_VALUE);
    EXPECT_EQ(
        LUMICE_ANALYTIC_TraceWavelengthCriticalTable(&crystal, kFaces35, 2, bad_density, labels, indices, 2, &out),
        LUMICE_ANALYTIC_ERR_INVALID_VALUE);
    EXPECT_EQ(out.storage, nullptr);
    LUMICE_ANALYTIC_ReleaseWavelengthTableResult(&out);
    LUMICE_ANALYTIC_ReleaseWavelengthTableResult(&out);
  }
  {
    LUMICE_ANALYTIC_RestrictedCurveResult out{};
    out.struct_size = sizeof(out);
    const double sun[3] = { 0.0, 0.0, 1.0 };
    const double wavelengths[1] = { 550.0 };
    const double indices[1] = { kN550 };
    const double bad_indices[1] = { 0.0 };
    EXPECT_EQ(LUMICE_ANALYTIC_TraceRestrictedFamilyCurve(&crystal, kFaces35, 2, PlateDensityAtPole(), nullptr, 1.31,
                                                         wavelengths, indices, 1, 8, &out),
              LUMICE_ANALYTIC_ERR_NULL_ARG);
    EXPECT_EQ(LUMICE_ANALYTIC_TraceRestrictedFamilyCurve(&crystal, kFaces35, 2, PlateDensityAtPole(), sun, 1.31,
                                                         nullptr, nullptr, 1, 8, &out),
              LUMICE_ANALYTIC_ERR_NULL_ARG);
    EXPECT_EQ(LUMICE_ANALYTIC_TraceRestrictedFamilyCurve(&crystal, kFaces35, 2, PlateDensityAtPole(), sun, 1.31,
                                                         wavelengths, indices, 1, 0, &out),
              LUMICE_ANALYTIC_ERR_INVALID_VALUE);
    EXPECT_EQ(LUMICE_ANALYTIC_TraceRestrictedFamilyCurve(&crystal, kFaces35, 2, PlateDensityAtPole(), sun, 1.31,
                                                         wavelengths, indices, 1, 10000001, &out),
              LUMICE_ANALYTIC_ERR_INVALID_VALUE);
    EXPECT_EQ(LUMICE_ANALYTIC_TraceRestrictedFamilyCurve(&crystal, kFaces35, 2, PlateDensityAtPole(), sun, -1.0,
                                                         wavelengths, indices, 1, 8, &out),
              LUMICE_ANALYTIC_ERR_INVALID_VALUE);
    EXPECT_EQ(LUMICE_ANALYTIC_TraceRestrictedFamilyCurve(&crystal, kFaces35, 2, PlateDensityAtPole(), sun, 1.31,
                                                         wavelengths, bad_indices, 1, 8, &out),
              LUMICE_ANALYTIC_ERR_INVALID_VALUE);
    EXPECT_EQ(LUMICE_ANALYTIC_TraceRestrictedFamilyCurve(&crystal, kFaces35, 2, bad_density, sun, 1.31, nullptr,
                                                         nullptr, 0, 8, &out),
              LUMICE_ANALYTIC_ERR_INVALID_VALUE);
    EXPECT_EQ(out.storage, nullptr);
    LUMICE_ANALYTIC_ReleaseRestrictedCurveResult(&out);
    LUMICE_ANALYTIC_ReleaseRestrictedCurveResult(&out);
  }
  {
    LUMICE_ANALYTIC_ChromaticResult out{};
    out.struct_size = sizeof(out);
    EXPECT_EQ(LUMICE_ANALYTIC_DiagnoseChromatic(nullptr, kFaces35, 2, 1.31, 1.32, &out), LUMICE_ANALYTIC_ERR_NULL_ARG);
    EXPECT_EQ(LUMICE_ANALYTIC_DiagnoseChromatic(&crystal, kFaces35, 2, 1.31, 0.0, &out),
              LUMICE_ANALYTIC_ERR_INVALID_VALUE);
    EXPECT_EQ(LUMICE_ANALYTIC_DiagnoseChromatic(&crystal, kFaces35, 2, 0.0, 1.32, &out),
              LUMICE_ANALYTIC_ERR_INVALID_VALUE);
    LUMICE_ANALYTIC_PlateFamily family{};
    family.sun_altitude_deg = 9.0;
    family.zenith_std_deg = 1.0;
    family.samples = 0;  // outside 1..10000000
    family.seed = 3;
    EXPECT_EQ(LUMICE_ANALYTIC_DiagnoseClassTint(&crystal, kFaces35, 2, family, kNRed, kNBlue, &out),
              LUMICE_ANALYTIC_ERR_INVALID_VALUE);
    family.samples = 10000001;
    EXPECT_EQ(LUMICE_ANALYTIC_DiagnoseClassTint(&crystal, kFaces35, 2, family, kNRed, kNBlue, &out),
              LUMICE_ANALYTIC_ERR_INVALID_VALUE);
    family.samples = 1000;
    family.zenith_std_deg = 0.0;
    EXPECT_EQ(LUMICE_ANALYTIC_DiagnoseClassTint(&crystal, kFaces35, 2, family, kNRed, kNBlue, &out),
              LUMICE_ANALYTIC_ERR_INVALID_VALUE);
    family.zenith_std_deg = 1.0;
    EXPECT_EQ(LUMICE_ANALYTIC_DiagnoseClassTint(&crystal, kFaces35, 2, family, 0.0, kNBlue, &out),
              LUMICE_ANALYTIC_ERR_INVALID_VALUE);
    EXPECT_EQ(out.storage, nullptr);
    LUMICE_ANALYTIC_ReleaseChromaticResult(&out);
    LUMICE_ANALYTIC_ReleaseChromaticResult(&out);
  }
}

// ---------------------------------------------------------------------------------------------
// The green arms: the anchors of the verified fixtures through the ABI.
// ---------------------------------------------------------------------------------------------

TEST(DpCapi, BoundaryLoopWalksThePrismLoopWithItsSlugs) {
  LUMICE_ANALYTIC_Crystal crystal = Prism();
  LUMICE_ANALYTIC_BoundaryLoopResult out{};
  out.struct_size = sizeof(out);
  ASSERT_EQ(LUMICE_ANALYTIC_TraceBoundaryLoop(&crystal, kFaces35, 2, 1.31, &out), LUMICE_ANALYTIC_OK);
  EXPECT_EQ(out.status, LUMICE_ANALYTIC_WALK_OK);
  EXPECT_STREQ(out.status_name, "ok");
  EXPECT_EQ(out.message, nullptr);
  ASSERT_GT(out.critical_point_count, 0);
  ASSERT_GT(out.corner_count, 0);
  EXPECT_EQ(out.has_plateau, 0);
  double length2 = 0.0;
  for (int j = 0; j < 3; ++j) {
    length2 += out.first_point[j] * out.first_point[j];
  }
  EXPECT_NEAR(length2, 1.0, 1e-12);
  for (int i = 0; i < out.critical_point_count; ++i) {
    const double* u = out.critical_point_positions + 3 * i;
    double u2 = 0.0;
    for (int j = 0; j < 3; ++j) {
      u2 += u[j] * u[j];
    }
    EXPECT_NEAR(u2, 1.0, 1e-12);
    EXPECT_GE(out.critical_point_values[i], 0.0);
    EXPECT_LT(out.critical_point_values[i], kPi);
  }
  LUMICE_ANALYTIC_ReleaseBoundaryLoopResult(&out);
  LUMICE_ANALYTIC_ReleaseBoundaryLoopResult(&out);
  EXPECT_EQ(out.critical_point_positions, nullptr);
  EXPECT_EQ(out.corner_positions, nullptr);
  EXPECT_EQ(out.storage, nullptr);
  EXPECT_EQ(out.struct_size, sizeof(out));
}

TEST(DpCapi, PartitionReplaysTheBetaAnchorAndTheMechanicalInvariants) {
  LUMICE_ANALYTIC_Crystal crystal = Beta();
  const int faces[4] = { 4, 8, 7, 5 };
  LUMICE_ANALYTIC_PartitionResult out{};
  out.struct_size = sizeof(out);
  ASSERT_EQ(LUMICE_ANALYTIC_PartitionDeviationAxis(&crystal, faces, 4, kN550, &out), LUMICE_ANALYTIC_OK);
  EXPECT_EQ(out.walk_status, LUMICE_ANALYTIC_WALK_OK);
  EXPECT_STREQ(out.walk_status_name, "ok");
  EXPECT_EQ(out.walk_message, nullptr);
  EXPECT_EQ(out.escaped, 0);
  ASSERT_EQ(out.interval_count, 2);
  EXPECT_NEAR(Deg(out.intervals[0].lower), 0.0, 1e-9);
  EXPECT_NEAR(Deg(out.intervals[0].upper), 50.161740000307894, 1e-9);
  EXPECT_NEAR(Deg(out.intervals[1].upper), 120.00000000000001, 1e-9);
  for (int i = 0; i < out.interval_count; ++i) {
    EXPECT_EQ(out.intervals[i].n_components, out.intervals[i].n_closed + out.intervals[i].n_open);
    if (i + 1 < out.interval_count) {
      EXPECT_EQ(out.intervals[i].upper, out.intervals[i + 1].lower);
    }
  }
  EXPECT_GT(out.lattice_n, 0);
  EXPECT_EQ(out.domain_components, 1);
  EXPECT_EQ(out.complement_components, 1);
  EXPECT_EQ(out.audit_verdict, nullptr) << "a disk pays no audit";
  LUMICE_ANALYTIC_ReleasePartitionResult(&out);
  LUMICE_ANALYTIC_ReleasePartitionResult(&out);
  EXPECT_EQ(out.intervals, nullptr);
}

TEST(DpCapi, WeightKinksReportTheClosedFormAndZeroSpreadOnTheMirrorSlab) {
  LUMICE_ANALYTIC_Crystal crystal = Prism(0.5);
  const int faces[3] = { 3, 1, 6 };
  LUMICE_ANALYTIC_WeightKinksResult out{};
  out.struct_size = sizeof(out);
  ASSERT_EQ(LUMICE_ANALYTIC_TraceWeightKinks(&crystal, faces, 3, kNBlue, &out), LUMICE_ANALYTIC_OK);
  ASSERT_EQ(out.curve_count, 1);
  const LUMICE_ANALYTIC_WeightKinkCurve& curve = out.curves[0];
  EXPECT_EQ(curve.step, 1);
  EXPECT_EQ(curve.margin, 3);  // internal step 1's TIR discriminant: position 2 * 1 + 1
  EXPECT_DOUBLE_EQ(curve.index, kNBlue);
  EXPECT_EQ(curve.coverage, LUMICE_ANALYTIC_KINK_CLOSED_FORM_AUTHORITY);
  EXPECT_EQ(curve.has_normal, 1);
  EXPECT_EQ(curve.failed_seeds, 0);
  EXPECT_EQ(curve.status, LUMICE_ANALYTIC_WALK_OK);
  EXPECT_STREQ(curve.status_name, "ok");
  EXPECT_LE(curve.spread, 1e-15) << "a single-mirror slab's onset is one level set (to rounding)";
  ASSERT_EQ(curve.arc_count, 1);
  const LUMICE_ANALYTIC_WeightKinkArc& arc = out.arcs[curve.first_arc];
  EXPECT_GT(arc.point_count, 0);
  double length2 = 0.0;
  for (int j = 0; j < 3; ++j) {
    const double u = out.arc_points[3 * arc.first_point + j];
    length2 += u * u;
  }
  EXPECT_NEAR(length2, 1.0, 1e-12);
  LUMICE_ANALYTIC_ReleaseWeightKinksResult(&out);
  LUMICE_ANALYTIC_ReleaseWeightKinksResult(&out);
  EXPECT_EQ(out.curves, nullptr);
  EXPECT_EQ(out.arc_points, nullptr);

  // An entry-exit path has no internal reflection: a successful empty answer.
  LUMICE_ANALYTIC_WeightKinksResult empty{};
  empty.struct_size = sizeof(empty);
  EXPECT_EQ(LUMICE_ANALYTIC_TraceWeightKinks(&crystal, kFaces35, 2, 1.31, &empty), LUMICE_ANALYTIC_OK);
  EXPECT_EQ(empty.curve_count, 0);
  LUMICE_ANALYTIC_ReleaseWeightKinksResult(&empty);
}

TEST(DpCapi, ClassifyLabelsThePointMassAndTheJacobianMechanism) {
  LUMICE_ANALYTIC_Crystal crystal = Prism();
  LUMICE_ANALYTIC_ClassificationResult out{};
  out.struct_size = sizeof(out);
  // {3, 6}: straight through two opposite parallel faces, M = I and the unfolded exit normal
  // parallel to the entry one — a point mass at the sun, no field, no onsets.
  const int straight[2] = { 3, 6 };
  ASSERT_EQ(LUMICE_ANALYTIC_ClassifyCriticalStructure(&crystal, straight, 2, 1.31, RandomDensity(), &out),
            LUMICE_ANALYTIC_OK);
  EXPECT_EQ(out.halo_map_rank, 0);
  EXPECT_EQ(out.escaped, 0);
  EXPECT_EQ(out.onset_count, 0);
  EXPECT_EQ(out.family_pinned, 0);
  EXPECT_STREQ(out.mechanism, "point_mass");
  LUMICE_ANALYTIC_ReleaseClassificationResult(&out);

  // 3-5 at random orientation: the minimum-deviation onsets are finite jumps — bright edges,
  // not divergent measure — so the mechanism roll-up is "none".
  out.struct_size = sizeof(out);
  ASSERT_EQ(LUMICE_ANALYTIC_ClassifyCriticalStructure(&crystal, kFaces35, 2, 1.31, RandomDensity(), &out),
            LUMICE_ANALYTIC_OK);
  EXPECT_EQ(out.halo_map_rank, 2);
  ASSERT_GE(out.onset_count, 1);
  bool finite_jump = false;
  for (int i = 0; i < out.onset_count; ++i) {
    if (out.onsets[i].profile == LUMICE_ANALYTIC_PROFILE_FINITE_JUMP) {
      finite_jump = true;
      EXPECT_EQ(out.onsets[i].has_measure_limit, 1);
      EXPECT_GT(out.onsets[i].measure_limit, 0.0);
      EXPECT_GE(out.onsets[i].multiplicity, 1);
    }
    EXPECT_GE(out.onsets[i].location, LUMICE_ANALYTIC_ONSET_INTERIOR);
    EXPECT_LE(out.onsets[i].location, LUMICE_ANALYTIC_ONSET_BOUNDARY);
  }
  EXPECT_TRUE(finite_jump);
  EXPECT_EQ(out.has_gradient_norm_range, 1);
  EXPECT_EQ(out.confined_dimensions, 0);
  EXPECT_EQ(out.confined_width_count, 0);
  EXPECT_EQ(out.family_pinned, 0);
  EXPECT_STREQ(out.mechanism, "none");
  LUMICE_ANALYTIC_ReleaseClassificationResult(&out);

  // A plate density pinned at the pole confines one dimension; on 3-5 that collapse is the whole
  // mechanism (a finite jump is not a Jacobian profile).
  out.struct_size = sizeof(out);
  ASSERT_EQ(LUMICE_ANALYTIC_ClassifyCriticalStructure(&crystal, kFaces35, 2, 1.31, PlateDensityAtPole(), &out),
            LUMICE_ANALYTIC_OK);
  EXPECT_EQ(out.confined_dimensions, 1);
  ASSERT_EQ(out.confined_width_count, 1);
  EXPECT_GT(out.confined_widths_rad[0], 0.0);
  EXPECT_STREQ(out.mechanism, "dimension_collapse");
  LUMICE_ANALYTIC_ReleaseClassificationResult(&out);

  // The rotation-fold plate row of LI's truth table: a slab rotation circle is an
  // inverse-square-root divergence on top of the dimension collapse.
  const int rotation_pinned[4] = { 3, 6, 4, 8 };
  out.struct_size = sizeof(out);
  ASSERT_EQ(LUMICE_ANALYTIC_ClassifyCriticalStructure(&crystal, rotation_pinned, 4, 1.31, PlateDensityAtPole(), &out),
            LUMICE_ANALYTIC_OK);
  EXPECT_EQ(out.family_pinned, 1);
  EXPECT_STREQ(out.mechanism, "jacobian+dimension_collapse");
  LUMICE_ANALYTIC_ReleaseClassificationResult(&out);
  LUMICE_ANALYTIC_ReleaseClassificationResult(&out);
}

TEST(DpCapi, WavelengthTableFollowsTheOnsetAcrossIndices) {
  LUMICE_ANALYTIC_Crystal crystal = Prism();
  const double indices[2] = { kNRed, kNBlue };
  const char* labels[2] = { "red", "blue" };
  LUMICE_ANALYTIC_WavelengthTableResult out{};
  out.struct_size = sizeof(out);
  ASSERT_EQ(
      LUMICE_ANALYTIC_TraceWavelengthCriticalTable(&crystal, kFaces35, 2, RandomDensity(), labels, indices, 2, &out),
      LUMICE_ANALYTIC_OK);
  EXPECT_EQ(out.escaped, 0);
  ASSERT_EQ(out.label_count, 2);
  EXPECT_STREQ(out.labels[0], "red");
  EXPECT_STREQ(out.labels[1], "blue");
  EXPECT_DOUBLE_EQ(out.indices[0], kNRed);
  ASSERT_EQ(out.onset_count, 4)
      << "3-5: the interior minimum and maximum plus the boundary pair, paired across indices";
  const int interior_min = [&] {
    for (int i = 0; i < out.onset_count; ++i) {
      if (out.onsets[i].location == LUMICE_ANALYTIC_ONSET_INTERIOR &&
          out.onsets[i].source == LUMICE_ANALYTIC_ONSET_INTERIOR_MINIMUM) {
        return i;
      }
    }
    return -1;
  }();
  ASSERT_GE(interior_min, 0);
  EXPECT_EQ(out.onsets[interior_min].profile, LUMICE_ANALYTIC_PROFILE_FINITE_JUMP);
  EXPECT_GT(out.onsets[interior_min].displacement_deg, 0.0) << "the critical value moves with n";
  EXPECT_NEAR(out.values_deg[out.onsets[interior_min].first_value], Deg(2.0 * std::asin(kNRed / 2.0) - kPi / 3.0), 1e-6)
      << "the minimum deviation of the 60-degree prism at the red index";
  EXPECT_NEAR(out.values_deg[out.onsets[interior_min].first_value + 1], Deg(2.0 * std::asin(kNBlue / 2.0) - kPi / 3.0),
              1e-6);
  LUMICE_ANALYTIC_ReleaseWavelengthTableResult(&out);
  LUMICE_ANALYTIC_ReleaseWavelengthTableResult(&out);
  EXPECT_EQ(out.onsets, nullptr);

  // An empty index set is the kernel's own escape, not a call error.
  out.struct_size = sizeof(out);
  ASSERT_EQ(
      LUMICE_ANALYTIC_TraceWavelengthCriticalTable(&crystal, kFaces35, 2, RandomDensity(), nullptr, nullptr, 0, &out),
      LUMICE_ANALYTIC_OK);
  EXPECT_EQ(out.escaped, 1);
  EXPECT_NE(std::strstr(out.message, "indices is empty"), nullptr);
  LUMICE_ANALYTIC_ReleaseWavelengthTableResult(&out);
}

TEST(DpCapi, RestrictedCurveSamplesTheLatitudeCircleAndSaysWhenThereIsNone) {
  LUMICE_ANALYTIC_Crystal crystal = Prism();
  // The sun at altitude 20 deg; the plate family pinned at the pole has one circle of u.
  const double altitude = 20.0 * kPi / 180.0;
  const double sun[3] = { std::cos(altitude), 0.0, std::sin(altitude) };
  const double wavelengths[2] = { 550.0, 440.0 };
  const double indices[2] = { kN550, 1.3175 };
  LUMICE_ANALYTIC_RestrictedCurveResult out{};
  out.struct_size = sizeof(out);
  ASSERT_EQ(LUMICE_ANALYTIC_TraceRestrictedFamilyCurve(&crystal, kFaces35, 2, PlateDensityAtPole(), sun, kN550,
                                                       wavelengths, indices, 2, 16, &out),
            LUMICE_ANALYTIC_OK);
  EXPECT_EQ(out.closed, 1);
  EXPECT_EQ(out.existence, LUMICE_ANALYTIC_EXISTENCE_COMPUTED);
  ASSERT_EQ(out.point_count, 16);
  ASSERT_EQ(out.wavelength_count, 2);
  for (int i = 0; i < out.point_count; ++i) {
    double u2 = 0.0;
    double t2 = 0.0;
    for (int j = 0; j < 3; ++j) {
      u2 += out.u[3 * i + j] * out.u[3 * i + j];
      t2 += out.tangent[3 * i + j] * out.tangent[3 * i + j];
    }
    EXPECT_NEAR(u2, 1.0, 1e-12);
    EXPECT_NEAR(t2, 1.0, 1e-12);
    EXPECT_NEAR(out.support_param[i], 2.0 * kPi * (i + 0.5) / 16.0, 1e-12);
  }
  // The latitude circle about the family axis e3 always crosses the entry gate's hemisphere
  // (a side face's normal is horizontal): part of the circle lies outside U_P, and the field
  // says so — NaN entries counted by routed_nonfinite across all columns, never a silent zero.
  int nan_count = 0;
  for (int i = 0; i < out.point_count; ++i) {
    if (std::isnan(out.d_p[i])) {
      ++nan_count;
    }
  }
  for (int i = 0; i < out.point_count * out.wavelength_count; ++i) {
    if (std::isnan(out.critical_d_p[i])) {
      ++nan_count;
    }
  }
  EXPECT_GT(out.routed_nonfinite, 0);
  EXPECT_EQ(nan_count, out.routed_nonfinite) << "the count and the NaN entries tell the same story";
  LUMICE_ANALYTIC_ReleaseRestrictedCurveResult(&out);
  LUMICE_ANALYTIC_ReleaseRestrictedCurveResult(&out);
  EXPECT_EQ(out.u, nullptr);

  // A random density has no family axis: an empty curve with the reason, a success.
  out.struct_size = sizeof(out);
  ASSERT_EQ(LUMICE_ANALYTIC_TraceRestrictedFamilyCurve(&crystal, kFaces35, 2, RandomDensity(), sun, kN550, nullptr,
                                                       nullptr, 0, 8, &out),
            LUMICE_ANALYTIC_OK);
  EXPECT_EQ(out.point_count, 0);
  EXPECT_NE(out.note, nullptr);
  EXPECT_NE(std::strstr(out.note, "no family axis"), nullptr);
  LUMICE_ANALYTIC_ReleaseRestrictedCurveResult(&out);
}

TEST(DpCapi, ChromaticVerdictCarriesTheDarkHoleRimAndTheThresholds) {
  LUMICE_ANALYTIC_Crystal crystal = Prism(0.5);
  const int faces[3] = { 3, 1, 6 };
  LUMICE_ANALYTIC_ChromaticResult out{};
  out.struct_size = sizeof(out);
  ASSERT_EQ(LUMICE_ANALYTIC_DiagnoseChromatic(&crystal, faces, 3, kNRed, kNBlue, &out), LUMICE_ANALYTIC_OK);
  EXPECT_EQ(out.verdict_kind, LUMICE_ANALYTIC_VERDICT_EDGE);
  EXPECT_EQ(out.color, LUMICE_ANALYTIC_COLOR_BLUE);
  EXPECT_EQ(out.visible, 1);
  EXPECT_EQ(out.coverage_complete, 1);
  EXPECT_EQ(out.note_count, 0);
  ASSERT_EQ(out.feature_count, 1);
  const LUMICE_ANALYTIC_ChromaticFeature& edge = out.features[0];
  EXPECT_EQ(edge.kind, LUMICE_ANALYTIC_CHROMATIC_EDGE);
  EXPECT_STREQ(edge.source, "internal_1_tir_discriminant");
  EXPECT_EQ(edge.color, LUMICE_ANALYTIC_COLOR_BLUE);
  EXPECT_DOUBLE_EQ(edge.positive_fraction, 1.0);
  const double closed_form = 2.0 * std::asin(std::sqrt(kNBlue * kNBlue - 1.0));
  EXPECT_NEAR(edge.delta_blue, closed_form, 2e-3);
  EXPECT_LE(edge.spread, 1e-9);
  EXPECT_EQ(out.has_position, 1);
  EXPECT_NEAR(out.position, closed_form, 2e-3);
  // The class suffix is empty on the random call; the threshold snapshot is the declared pair.
  EXPECT_EQ(out.member_count, 0);
  EXPECT_EQ(out.has_tint, 0);
  EXPECT_DOUBLE_EQ(out.thresholds.n_red, kNRed);
  EXPECT_DOUBLE_EQ(out.thresholds.n_blue, kNBlue);
  EXPECT_DOUBLE_EQ(out.thresholds.edge_spread_per_shift, 1.0);
  LUMICE_ANALYTIC_ReleaseChromaticResult(&out);
  LUMICE_ANALYTIC_ReleaseChromaticResult(&out);
  EXPECT_EQ(out.features, nullptr);
  EXPECT_EQ(out.notes, nullptr);

  // A non-frozen pair comes back as the call's own pair; the four frozen constants stay LI's.
  LUMICE_ANALYTIC_ChromaticResult shifted{};
  shifted.struct_size = sizeof(shifted);
  ASSERT_EQ(LUMICE_ANALYTIC_DiagnoseChromatic(&crystal, faces, 3, 1.45, 1.50, &shifted), LUMICE_ANALYTIC_OK);
  EXPECT_DOUBLE_EQ(shifted.thresholds.n_red, 1.45);
  EXPECT_DOUBLE_EQ(shifted.thresholds.n_blue, 1.50);
  EXPECT_DOUBLE_EQ(shifted.thresholds.edge_spread_per_shift, 1.0);
  LUMICE_ANALYTIC_ReleaseChromaticResult(&shifted);
  LUMICE_ANALYTIC_ReleaseChromaticResult(&shifted);
}

TEST(DpCapi, ClassTintReplaysTheRhombicPlateAnchor) {
  LUMICE_ANALYTIC_Crystal crystal = RhombicPlate();
  const int representative[4] = { 1, 3, 5, 2 };
  LUMICE_ANALYTIC_PlateFamily family{};
  family.sun_altitude_deg = 9.0;
  family.zenith_std_deg = 1.0;
  family.samples = 100000;
  family.seed = 3;
  LUMICE_ANALYTIC_ChromaticResult out{};
  out.struct_size = sizeof(out);
  ASSERT_EQ(LUMICE_ANALYTIC_DiagnoseClassTint(&crystal, representative, 4, family, kNRed, kNBlue, &out),
            LUMICE_ANALYTIC_OK);
  ASSERT_EQ(out.member_count, 24);
  int total = 0;
  for (int i = 0; i < out.member_count; ++i) {
    EXPECT_EQ(out.member_sizes[i], 4) << "every orbit member is a 4-face sequence";
    total += out.member_sizes[i];
  }
  EXPECT_EQ(total, 96);
  EXPECT_EQ(out.verdict_kind, LUMICE_ANALYTIC_VERDICT_TINT);
  EXPECT_EQ(out.color, LUMICE_ANALYTIC_COLOR_BLUE);
  EXPECT_EQ(out.visible, 1);
  EXPECT_EQ(out.has_tint, 1);
  EXPECT_NEAR(out.ratio, 1.492, 5e-2);
  EXPECT_LE(out.tint_direction_dispersion, 1e-9);
  ASSERT_EQ(out.lit_red_count, 4);
  ASSERT_EQ(out.lit_blue_count, 4);
  for (int i = 0; i < out.lit_red_count; ++i) {
    EXPECT_EQ(out.lit_sizes_red[i], 4);
    EXPECT_GE(out.lit_members_red[i], 0);
    EXPECT_LT(out.lit_members_red[i], out.member_count);
  }
  LUMICE_ANALYTIC_ReleaseChromaticResult(&out);
  LUMICE_ANALYTIC_ReleaseChromaticResult(&out);
  EXPECT_EQ(out.members, nullptr);
}

// ---------------------------------------------------------------------------------------------
// Re-entrancy: the calls hold no state between calls, eight threads' results are bit-identical to
// the serial run.
// ---------------------------------------------------------------------------------------------

std::string SerialBytes() {
  LUMICE_ANALYTIC_Crystal crystal = Prism();
  LUMICE_ANALYTIC_BoundaryLoopResult loop{};
  loop.struct_size = sizeof(loop);
  EXPECT_EQ(LUMICE_ANALYTIC_TraceBoundaryLoop(&crystal, kFaces35, 2, 1.31, &loop), LUMICE_ANALYTIC_OK);
  LUMICE_ANALYTIC_WeightKinksResult kinks{};
  kinks.struct_size = sizeof(kinks);
  const int faces[3] = { 3, 1, 6 };
  EXPECT_EQ(LUMICE_ANALYTIC_TraceWeightKinks(&crystal, faces, 3, kNBlue, &kinks), LUMICE_ANALYTIC_OK);
  std::string bytes(reinterpret_cast<const char*>(loop.critical_point_positions),
                    sizeof(double) * 3 * static_cast<size_t>(loop.critical_point_count));
  bytes.append(reinterpret_cast<const char*>(kinks.arc_values),
               sizeof(double) * static_cast<size_t>(kinks.arc_point_count));
  LUMICE_ANALYTIC_ReleaseBoundaryLoopResult(&loop);
  LUMICE_ANALYTIC_ReleaseWeightKinksResult(&kinks);
  return bytes;
}

TEST(DpCapi, EightConcurrentCallsMatchTheSerialRunBitForBit) {
  const std::string serial = SerialBytes();
  std::vector<std::future<std::string>> threads;
  for (int i = 0; i < 8; ++i) {
    threads.emplace_back(std::async(std::launch::async, SerialBytes));
  }
  for (auto& thread : threads) {
    EXPECT_EQ(thread.get(), serial);
  }
}

}  // namespace
