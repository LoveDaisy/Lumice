// Finite-crystal entry measure (src/analytic/entry_measure.hpp).

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "analytic/entry_measure.hpp"
#include "analytic/path_evaluation.hpp"

namespace lumice::analytic {
namespace {

LUMICE_ANALYTIC_Crystal Prism() {
  LUMICE_ANALYTIC_Crystal c{};
  c.kind = LUMICE_ANALYTIC_CRYSTAL_PRISM;
  c.height = 1.0;
  for (double& d : c.face_distance) {
    d = 1.0;
  }
  return c;
}

LUMICE_ANALYTIC_Crystal RhombicPlate() {
  LUMICE_ANALYTIC_Crystal c{};
  c.kind = LUMICE_ANALYTIC_CRYSTAL_PRISM;
  c.height = 1.0;
  const double fd[6] = { 1.5, 1.0, 1.0, 1.5, 1.0, 1.0 };
  for (int i = 0; i < 6; i++) {
    c.face_distance[i] = fd[i];
  }
  return c;
}

LUMICE_ANALYTIC_Crystal AsymmetricPyramid() {
  LUMICE_ANALYTIC_Crystal c{};
  c.kind = LUMICE_ANALYTIC_CRYSTAL_PYRAMID;
  c.height = 0.5;
  const double fd[6] = { 1.0, 1.1, 0.9, 1.0, 1.2, 0.95 };
  for (int i = 0; i < 6; i++) {
    c.face_distance[i] = fd[i];
  }
  c.upper_h = 0.25;
  c.lower_h = 0.6;
  c.upper_wedge_deg = 27.996455531220374;
  c.lower_wedge_deg = 38.5704386184827;
  return c;
}

struct Built {
  FaceNormalTable table;
  FacePolygonTable polygons;
  std::vector<int> slots;
};

Built Build(const LUMICE_ANALYTIC_Crystal& crystal, const std::vector<int>& faces) {
  Built b;
  EXPECT_EQ(BuildFaceNormals(crystal, &b.table, &b.polygons), Status::kOk);
  b.slots.resize(faces.size());
  EXPECT_EQ(ResolveFaceSequence(b.table, faces.data(), static_cast<int>(faces.size()), b.slots.data()), Status::kOk);
  return b;
}

// Area of a planar 3D polygon: half the norm of the summed corner cross products.
double PlanarArea(const FacePolygonTable& p, int slot) {
  double sum[3] = { 0.0, 0.0, 0.0 };
  for (int k = 0; k < p.corner_cnt[slot]; k++) {
    const double* a = p.corner[slot][k];
    const double* b = p.corner[slot][(k + 1) % p.corner_cnt[slot]];
    sum[0] += a[1] * b[2] - a[2] * b[1];
    sum[1] += a[2] * b[0] - a[0] * b[2];
    sum[2] += a[0] * b[1] - a[1] * b[0];
  }
  return 0.5 * std::sqrt(sum[0] * sum[0] + sum[1] * sum[1] + sum[2] * sum[2]);
}

// Normal incidence through two parallel side faces: the internal ray is undeviated and every line
// along it that enters face 3 leaves through face 6, so the measure is face 3's own area.
TEST(EntryMeasure, NormalIncidenceThroughParallelFacesIsTheFaceArea) {
  const Built b = Build(Prism(), { 3, 6 });
  Corridor corridor(b.table, b.polygons, b.slots.data(), 2);
  const double* n = b.table.normal[b.slots[0]];
  const double s[3] = { -n[0], -n[1], -n[2] };
  const EntryMeasure m = corridor.Evaluate(s, 1.31);
  ASSERT_EQ(m.status, EntryMeasureStatus::kOk);
  EXPECT_NEAR(m.value, PlanarArea(b.polygons, b.slots[0]), 1e-6 * m.value);
  EXPECT_NEAR(m.area_perp_internal, m.value, 1e-12);  // cos_i = cos_t = 1
}

TEST(EntryMeasure, OptionalLineageDistinguishesContactAreaGateAndOptics) {
  // Two parallel squares offset transversely. At normal incidence their
  // overlap is a unit square, a thin rectangle, a line, or empty, analytically.
  FaceNormalTable normals;
  normals.slot_cnt = 2;
  normals.present[0] = normals.present[1] = true;
  normals.normal[0][0] = 1;
  normals.normal[1][0] = -1;
  FacePolygonTable polygons;
  polygons.corner_cnt[0] = polygons.corner_cnt[1] = 4;
  polygons.min_edge_length = 1;
  const int slots[] = { 0, 1 };
  const double yz[4][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
  for (double shift : { 0.25, 1 - 1e-8, 1.0, 1.01 }) {
    for (int i = 0; i < 4; ++i) {
      polygons.corner[0][i][0] = .5;
      polygons.corner[0][i][1] = yz[i][0];
      polygons.corner[0][i][2] = yz[i][1];
      polygons.corner[1][i][0] = -.5;
      polygons.corner[1][i][1] = yz[3 - i][0] + shift;
      polygons.corner[1][i][2] = yz[3 - i][1];
    }
    Corridor corridor(normals, polygons, slots, 2);
    const double incident[] = { -1, 0, 0 };
    const auto plain = corridor.Evaluate(incident, 1.31);
    CorridorDiagnostics diagnostic;
    const auto traced = corridor.Evaluate(incident, 1.31, &diagnostic);
    EXPECT_EQ(plain.status, traced.status);
    EXPECT_DOUBLE_EQ(plain.value, traced.value);
    EXPECT_DOUBLE_EQ(plain.area_perp_internal, traced.area_perp_internal);
    EXPECT_TRUE(diagnostic.geometry_evaluated);
    EXPECT_DOUBLE_EQ(diagnostic.area_threshold, corridor.Eps());
    EXPECT_NEAR(diagnostic.raw_area, std::max(0.0, 1 - shift), 1e-15);
    EXPECT_EQ(diagnostic.vertices.size(), diagnostic.edge_sources.size());
    if (shift < 1 && shift > .9) {
      EXPECT_GT(diagnostic.raw_area, 0);
      EXPECT_LT(diagnostic.raw_area, diagnostic.area_threshold);
      EXPECT_EQ(traced.status, EntryMeasureStatus::kCorridorEmpty);
    }
    if (shift == 1) {
      EXPECT_DOUBLE_EQ(diagnostic.raw_area, 0);
      EXPECT_GE(diagnostic.vertices.size(), 2u);
    }
    for (size_t i = 0; i < diagnostic.vertices.size(); ++i) {
      const auto source = diagnostic.edge_sources[i];
      if (source.path_index < 0 || source.path_index >= 2 || source.edge_index < 0 || source.edge_index >= 4) {
        ADD_FAILURE() << "invalid original-edge provenance";
        return;
      }
      const auto* a = polygons.corner[source.path_index][source.edge_index];
      const auto* b = polygons.corner[source.path_index][(source.edge_index + 1) % 4];
      double pa[2]{};
      double pb[2]{};
      for (int k = 0; k < 2; ++k) {
        for (int j = 0; j < 3; ++j) {
          pa[k] += a[j] * diagnostic.projection_basis[k][j];
          pb[k] += b[j] * diagnostic.projection_basis[k][j];
        }
      }
      // Both endpoints of each clipped output edge lie on its declared source
      // line; this checks original edge numbering through orientation reversal.
      for (size_t v : { i, (i + 1) % diagnostic.vertices.size() }) {
        const auto& p = diagnostic.vertices[v];
        EXPECT_NEAR((pb[0] - pa[0]) * (p[1] - pa[1]) - (pb[1] - pa[1]) * (p[0] - pa[0]), 0, 1e-14);
      }
    }
    const double backface[] = { 1, 0, 0 };
    EXPECT_EQ(corridor.Evaluate(backface, 1.31, &diagnostic).status, EntryMeasureStatus::kEntryBackface);
    EXPECT_FALSE(diagnostic.geometry_evaluated);
    EXPECT_TRUE(diagnostic.vertices.empty());
    EXPECT_TRUE(diagnostic.edge_sources.empty());
  }
}

// The threshold is LI's area_eps: 1e-6 times the shortest crystal edge squared.
TEST(EntryMeasure, EpsIsRelativeToTheShortestEdge) {
  const Built b = Build(Prism(), { 3, 5 });
  Corridor corridor(b.table, b.polygons, b.slots.data(), 2);
  EXPECT_GT(b.polygons.min_edge_length, 0.0);
  EXPECT_DOUBLE_EQ(corridor.Eps(), 1e-6 * b.polygons.min_edge_length * b.polygons.min_edge_length);
}

TEST(EntryMeasure, MatchesRhombicPlateDiagnosticSnapshot) {
  const Built b = Build(RhombicPlate(), { 1, 3, 4, 2 });
  Corridor corridor(b.table, b.polygons, b.slots.data(), 4);
  const double incident[3] = { 0.9876883405951378, 0.0, -0.15643446504023087 };
  const double pose[9] = { 0.258942519, 0.965892733, 0.0, -0.965892733, 0.258942519, 0.0, 0.0, 0.0, 1.0 };
  double incident_body[3]{};
  for (int i = 0; i < 3; i++) {
    incident_body[i] = pose[0 * 3 + i] * incident[0] + pose[1 * 3 + i] * incident[1] + pose[2 * 3 + i] * incident[2];
  }
  const EntryMeasure m = corridor.Evaluate(incident_body, 1.307);
  ASSERT_EQ(m.status, EntryMeasureStatus::kOk);
  EXPECT_NEAR(m.value, 0.25 * 0.00620221727, 2e-8);
}

// Pins from LI's geometry.entry_measure at li_rev bfbd042 (the fixture crystals of 3-5__random,
// 3-5-6-7__random and 13-15-26-28__random, those fixtures' incident direction and n = 1.31, uniformly
// random poses): the status of every row, and the value up to the unit — these pins are pre-
// caliber-fix captures, when LI exported at hexagon edge 1 and this library's reference crystal
// (circumscribed diameter 1) was a quarter of its area (value ratio 0.25; LI's module C caliber
// fix later moved LI's export scale to this reference, making the ratio 1 for new captures — the
// frozen literals keep the 0.25). Over 12000 such poses the statuses agreed
// everywhere and the value ratio stayed within 1.4e-5 of 0.25, the spread of the float closed-form
// corners on the pyramid's smallest corridors. Statuses: 0 ok, 1 entry_backface,
// 2 exit_critical_angle, 3 corridor_empty (the 3-5 sample had no corridor_empty pose).
TEST(EntryMeasure, MatchesLiEntryMeasurePins) {
  struct Row {
    const char* path;
    int status;
    double li_value;
    double pose[9];
  };
  const Row rows[] = {
    { "3-5",
      0,
      0.1817512873316552,
      { -0.0214067317528861, -0.6662448270399101, -0.7454257724805471, -0.35598205366005387, 0.7018108832951394,
        -0.6170399189359362, 0.9342475738837273, 0.252149389313273, -0.2521946790900098 } },
    { "3-5",
      0,
      1.239854080558465,
      { 0.6054547955155409, -0.6462888774699322, -0.4644730104601229, 0.5399180577823683, 0.7623040432141128,
        -0.35690480044394357, 0.5847332566801291, -0.034687642728656376, 0.8104836740947836 } },
    { "3-5",
      0,
      0.015308769653460317,
      { -0.19165130732005853, -0.7338866935227326, 0.6516748402944395, -0.5884525860909908, -0.445480093088896,
        -0.6747377568984363, 0.7854892299231973, -0.5127941182694855, -0.34648068047567016 } },
    { "3-5",
      1,
      0.0,
      { -0.5108181656453654, -0.8569178335521683, -0.06896831291950242, 0.6557541851097652, -0.3365085527995817,
        -0.6758316673578998, 0.5559237810687107, -0.39045335244173474, 0.7338221373120865 } },
    { "3-5",
      2,
      0.0,
      { 0.5021590044719326, 0.775587735994188, -0.38249182736781095, -0.2676411234139845, -0.2812000596610773,
        -0.9215718938337404, -0.8223165833700897, 0.5651461671302535, 0.06637203095073863 } },
    { "3-5-6-7",
      0,
      0.36379929350611545,
      { 0.9777657700649336, -0.07101251752593112, 0.19731021576177132, -0.16976474081015924, 0.28430024017676436,
        0.9435853465442825, -0.12310171268908844, -0.9561017706238513, 0.26592362163401295 } },
    { "3-5-6-7",
      0,
      0.0006157353109285528,
      { 0.507176099237148, -0.41948159619954756, -0.7528662529377117, 0.7945215096895646, 0.566039436924794,
        0.21985205590687013, 0.333928098552976, -0.7096721400025099, 0.6203688247333614 } },
    { "3-5-6-7",
      0,
      0.16881385158042136,
      { 0.8855374154946081, -0.4544714637280438, -0.09632847147141496, 0.4605334760502101, 0.831500553812929,
        0.3106698351077923, -0.06109339732115394, -0.3194722486628247, 0.9456241743621394 } },
    { "3-5-6-7",
      1,
      0.0,
      { -0.8296065636112238, -0.28562017956182917, 0.4797645908570627, 0.532076285708425, -0.14393912256859065,
        0.8343718326866688, -0.16925653842585434, 0.9474717104068345, 0.27138456510797493 } },
    { "3-5-6-7",
      2,
      0.0,
      { 0.3126662147508653, 0.16993692350149503, 0.9345380036061738, -0.4366749742857968, 0.899449912388528,
        -0.017459150516793753, -0.8435370797930962, -0.4026304721889743, 0.35543480116474124 } },
    { "3-5-6-7",
      3,
      0.0,
      { 0.3373647742329948, -0.8217959637781687, -0.45916925313508417, -0.6007478036546372, 0.18758651077411415,
        -0.7771186379052573, 0.724766918057695, 0.5380173741558014, -0.4304070394355164 } },
    { "13-15-26-28",
      0,
      0.015243541525689002,
      { 0.38947319055953666, -0.9206440277496026, -0.02692968630273812, -0.41810850781521075, -0.15067503460533604,
        -0.8958137695074904, 0.8206679654573541, 0.3601549779132174, -0.44361298713678066 } },
    { "13-15-26-28",
      0,
      0.0005791450814237324,
      { 0.21445647570750936, -0.9763539104065909, -0.027229793625240584, 0.1482046988512512, 0.06008354019688411,
        -0.9871298473026836, 0.9654241488928711, 0.2076608047542149, 0.15758554154270454 } },
    { "13-15-26-28",
      0,
      0.005585096727643553,
      { 0.5399591921297402, -0.8212506317702406, -0.18436775925193366, 0.8416891483475517, 0.5263616729857026,
        0.12042743360906359, -0.0018569837294391267, -0.22020624202931438, 0.9754515685480011 } },
    { "13-15-26-28",
      1,
      0.0,
      { -0.2499996281945034, -0.13674186620608372, 0.9585415212337379, 0.06035426624675296, 0.9858514037998922,
        0.1563789377492948, -0.9663630520919283, 0.09694674647588364, -0.23820952940492246 } },
    { "13-15-26-28",
      2,
      0.0,
      { 0.3628389462954271, 0.8913242777228337, -0.27182518461888894, 0.916772744286252, -0.39370717486763085,
        -0.0672487605209459, -0.16695997839326285, -0.22480145106208232, -0.9599941006148445 } },
    { "13-15-26-28",
      3,
      0.0,
      { 0.6520773139551843, -0.5328080838581624, -0.5393614024013821, 0.44307766523227876, -0.30946417829811135,
        0.8413763158792789, -0.615205135867139, -0.7876213989764809, 0.034280791662777865 } },
  };
  const double incident[3] = { -0.9659258262890683, -0.0, -0.25881904510252074 };
  for (const Row& row : rows) {
    SCOPED_TRACE(row.path);
    std::vector<int> faces;
    const std::string path = row.path;
    size_t begin = 0;
    while (true) {
      const size_t end = path.find('-', begin);
      faces.push_back(std::stoi(path.substr(begin, end - begin)));
      if (end == std::string::npos) {
        break;
      }
      begin = end + 1;
    }
    const Built b = Build(faces[0] >= 13 ? AsymmetricPyramid() : Prism(), faces);
    Corridor corridor(b.table, b.polygons, b.slots.data(), static_cast<int>(b.slots.size()));
    double s_body[3];
    for (int i = 0; i < 3; i++) {
      s_body[i] =
          row.pose[0 * 3 + i] * incident[0] + row.pose[1 * 3 + i] * incident[1] + row.pose[2 * 3 + i] * incident[2];
    }
    const EntryMeasure m = corridor.Evaluate(s_body, 1.31);
    EXPECT_EQ(static_cast<int>(m.status), row.status);
    if (row.status == 0) {
      EXPECT_NEAR(m.value, 0.25 * row.li_value, 2e-5 * 0.25 * row.li_value);
    } else {
      EXPECT_EQ(m.value, 0.0);
    }
  }
}

// ---- representative-path recovery ------------------------------------------------------------

// The test's own oracles, independent of the corridor's internals (numerical-robustness #6: the
// checks must not share the implementation they police).

// Distance of a point to a face's plane, from the normal table and polygon table's own corners.
double PlaneResidual(const FaceNormalTable& table, const FacePolygonTable& polygons, int slot, const double q[3]) {
  double offset = 0.0;
  for (int v = 0; v < polygons.corner_cnt[slot]; v++) {
    offset += table.normal[slot][0] * polygons.corner[slot][v][0] +
              table.normal[slot][1] * polygons.corner[slot][v][1] + table.normal[slot][2] * polygons.corner[slot][v][2];
  }
  offset /= polygons.corner_cnt[slot];
  const double d = table.normal[slot][0] * q[0] + table.normal[slot][1] * q[1] + table.normal[slot][2] * q[2];
  return std::fabs(d - offset);
}

// Signed distance of an on-plane point to the nearest edge line of a convex face polygon:
// positive inside. The winding reference is the ring's own triple, like the implementation's
// predicate — but spelled out here, so a drifted predicate shows up as a disagreement.
double PolygonMargin(const FacePolygonTable& polygons, int slot, const double q[3]) {
  const double(*pts)[3] = polygons.corner[slot];
  const int count = polygons.corner_cnt[slot];
  const double* n = nullptr;  // set by the caller's table; recomputed here for independence
  // The outward normal from the ring itself: the first triple's cross, normalized.
  double e0[3] = { pts[1][0] - pts[0][0], pts[1][1] - pts[0][1], pts[1][2] - pts[0][2] };
  double v0[3] = { pts[2][0] - pts[0][0], pts[2][1] - pts[0][1], pts[2][2] - pts[0][2] };
  double normal[3] = { e0[1] * v0[2] - e0[2] * v0[1], e0[2] * v0[0] - e0[0] * v0[2], e0[0] * v0[1] - e0[1] * v0[0] };
  const double norm = std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
  for (double& c : normal) {
    c /= norm;
  }
  n = normal;
  // The ring's winding around its own normal decides the inside direction.
  double winding[3] = { e0[1] * v0[2] - e0[2] * v0[1], e0[2] * v0[0] - e0[0] * v0[2], e0[0] * v0[1] - e0[1] * v0[0] };
  const double ref_sign = (winding[0] * n[0] + winding[1] * n[1] + winding[2] * n[2]) >= 0.0 ? 1.0 : -1.0;
  double margin = std::numeric_limits<double>::infinity();
  for (int i = 0; i < count; i++) {
    const double* a = pts[i];
    const double* b = pts[(i + 1) % count];
    const double e[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] };
    const double v[3] = { q[0] - a[0], q[1] - a[1], q[2] - a[2] };
    const double c[3] = { e[1] * v[2] - e[2] * v[1], e[2] * v[0] - e[0] * v[2], e[0] * v[1] - e[1] * v[0] };
    const double edge_length = std::sqrt(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]);
    margin = std::min(margin, ref_sign * (c[0] * n[0] + c[1] * n[1] + c[2] * n[2]) / edge_length);
  }
  return margin;
}

// Whole-crystal containment of a point: the engine's own closed-form half-spaces, every present
// face (the convex-crystal test, independent of the corridor).
double CrystalPenetration(const FaceNormalTable& table, const FacePolygonTable& polygons, const double q[3]) {
  double worst = -std::numeric_limits<double>::infinity();
  for (int slot = 0; slot < table.slot_cnt; slot++) {
    if (!table.present[slot]) {
      continue;
    }
    double offset = 0.0;
    for (int v = 0; v < polygons.corner_cnt[slot]; v++) {
      offset += table.normal[slot][0] * polygons.corner[slot][v][0] +
                table.normal[slot][1] * polygons.corner[slot][v][1] +
                table.normal[slot][2] * polygons.corner[slot][v][2];
    }
    offset /= polygons.corner_cnt[slot];
    const double d = table.normal[slot][0] * q[0] + table.normal[slot][1] * q[1] + table.normal[slot][2] * q[2];
    worst = std::max(worst, d - offset);
  }
  return worst;
}

// The representative ray's independent verification on an evaluated corridor: I0 plane
// membership, I1 finite-face containment, I2 in-crystal propagation and I3 agreement with
// EvaluatePath's own segment directions (two mechanisms: the unfolding geometry vs the chain).
// The contacts pass only if every check holds; the corridor's internal self-checks are not
// consulted.
void VerifyRecoveredPath(const Built& b, double refractive_index, const double incident[3], const double pose[9],
                         const RepresentativePath& path) {
  const int face_count = static_cast<int>(b.slots.size());
  ASSERT_EQ(path.contact_count, face_count);
  const double tol = 1e-6 * b.polygons.min_edge_length;
  for (int k = 0; k < face_count; k++) {
    SCOPED_TRACE(testing::Message() << "contact " << k);
    // I0: on the original finite face's plane — a fold-back that dropped the affine
    // translation would land on the origin-parallel plane and miss by the face's own offset.
    EXPECT_LT(PlaneResidual(b.table, b.polygons, b.slots[k], path.contacts[k]), tol);
    // I1: inside the original polygon.
    EXPECT_GT(PolygonMargin(b.polygons, b.slots[k], path.contacts[k]), -tol);
    // I2: the leg to the next contact stays inside the crystal.
    if (k + 1 < face_count) {
      const double mid[3] = { 0.5 * (path.contacts[k][0] + path.contacts[k + 1][0]),
                              0.5 * (path.contacts[k][1] + path.contacts[k + 1][1]),
                              0.5 * (path.contacts[k][2] + path.contacts[k + 1][2]) };
      EXPECT_LT(CrystalPenetration(b.table, b.polygons, mid), tol);
    }
  }
  // I3: each leg's direction agrees with EvaluatePath's — the unfolding's straight line and the
  // chain's reflections are independent computations of the same optics.
  std::vector<double> segments(3 * (face_count + 1));
  std::vector<double> transmittances(face_count);
  PathOutputs outputs{};
  outputs.segment_directions = segments.data();
  outputs.interface_transmittances = transmittances.data();
  ASSERT_TRUE(EvaluatePath(b.table, b.slots.data(), face_count, refractive_index, incident, pose, &outputs));
  for (int k = 0; k + 1 < face_count; k++) {
    double leg[3] = { path.contacts[k + 1][0] - path.contacts[k][0], path.contacts[k + 1][1] - path.contacts[k][1],
                      path.contacts[k + 1][2] - path.contacts[k][2] };
    const double norm = std::sqrt(leg[0] * leg[0] + leg[1] * leg[1] + leg[2] * leg[2]);
    for (double& c : leg) {
      c /= norm;
    }
    const double* want = segments.data() + 3 * (k + 1);
    EXPECT_NEAR(leg[0] * want[0] + leg[1] * want[1] + leg[2] * want[2], 1.0, 1e-9)
        << "leg " << k << " disagrees with EvaluatePath";
  }
}

// Evaluates at one LI-pin pose and, when the corridor is legal, recovers and verifies the path.
// The pins' statuses are asserted by MatchesLiEntryMeasurePins; here the kOk rows must recover.
void RecoverAtPin(const char* path_text, const double pose[9]) {
  std::vector<int> faces;
  const std::string text = path_text;
  size_t begin = 0;
  while (true) {
    const size_t end = text.find('-', begin);
    faces.push_back(std::stoi(text.substr(begin, end - begin)));
    if (end == std::string::npos) {
      break;
    }
    begin = end + 1;
  }
  const Built b = Build(faces[0] >= 13 ? AsymmetricPyramid() : Prism(), faces);
  Corridor corridor(b.table, b.polygons, b.slots.data(), static_cast<int>(b.slots.size()));
  const double incident[3] = { -0.9659258262890683, -0.0, -0.25881904510252074 };
  double s_body[3];
  for (int i = 0; i < 3; i++) {
    s_body[i] = pose[0 * 3 + i] * incident[0] + pose[1 * 3 + i] * incident[1] + pose[2 * 3 + i] * incident[2];
  }
  CorridorDiagnostics diagnostics;
  const EntryMeasure m = corridor.Evaluate(s_body, 1.31, &diagnostics);
  ASSERT_EQ(m.status, EntryMeasureStatus::kOk) << EntryMeasureStatusName(m.status);
  const double area_before = m.area_perp_internal;
  RepresentativePath path;
  const RecoveryStatus recovery = corridor.RecoverRepresentativePath(diagnostics, &path);
  ASSERT_EQ(recovery, RecoveryStatus::kOk) << RecoveryStatusName(recovery);
  VerifyRecoveredPath(b, 1.31, incident, pose, path);
  // The recovery reads only construction-time state: the corridor's own answer is untouched.
  const EntryMeasure again = corridor.Evaluate(s_body, 1.31);
  EXPECT_DOUBLE_EQ(again.area_perp_internal, area_before);
}

// The corpus's two-face transmission (C01 shape): entry face 3, exit face 5 of the random prism.
TEST(EntryMeasureRecovery, RecoversTwoFaceTransmission) {
  const double pose[9] = { -0.0214067317528861,  -0.6662448270399101, -0.7454257724805471,
                           -0.35598205366005387, 0.7018108832951394,  -0.6170399189359362,
                           0.9342475738837273,   0.252149389313273,   -0.2521946790900098 };
  RecoverAtPin("3-5", pose);
}

// The corpus's internal-reflection path (C03 shape, 3-5-6-7), every legal pin pose.
TEST(EntryMeasureRecovery, RecoversInternalReflectionPaths) {
  const double poses[][9] = {
    { 0.9777657700649336, -0.07101251752593112, 0.19731021576177132, -0.16976474081015924, 0.28430024017676436,
      0.9435853465442825, -0.12310171268908844, -0.9561017706238513, 0.26592362163401295 },
    { 0.507176099237148, -0.41948159619954756, -0.7528662529377117, 0.7945215096895646, 0.566039436924794,
      0.21985205590687013, 0.333928098552976, -0.7096721400025099, 0.6203688247333614 },
    { 0.8855374154946081, -0.4544714637280438, -0.09632847147141496, 0.4605334760502101, 0.831500553812929,
      0.3106698351077923, -0.06109339732115394, -0.3194722486628247, 0.9456241743621394 },
  };
  for (int p = 0; p < 3; p++) {
    SCOPED_TRACE(testing::Message() << "pin pose " << p);
    RecoverAtPin("3-5-6-7", poses[p]);
  }
}

// The asymmetric pyramid's four-face path with internal reflections (the 13-15-26-28 pins): the
// fold-back's affine translations are load-bearing here — a rotation-only fold lands a face's
// offset away from its plane and is refused or misplaced.
TEST(EntryMeasureRecovery, RecoversPyramidPaths) {
  const double poses[][9] = {
    { 0.38947319055953666, -0.9206440277496026, -0.02692968630273812, -0.41810850781521075, -0.15067503460533604,
      -0.8958137695074904, 0.8206679654573541, 0.3601549779132174, -0.44361298713678066 },
    { 0.21445647570750936, -0.9763539104065909, -0.027229793625240584, 0.1482046988512512, 0.06008354019688411,
      -0.9871298473026836, 0.9654241488928711, 0.2076608047542149, 0.15758554154270454 },
    { 0.5399591921297402, -0.8212506317702406, -0.18436775925193366, 0.8416891483475517, 0.5263616729857026,
      0.12042743360906359, -0.0018569837294391267, -0.22020624202931438, 0.9754515685480011 },
  };
  for (int p = 0; p < 3; p++) {
    SCOPED_TRACE(testing::Message() << "pin pose " << p);
    RecoverAtPin("13-15-26-28", poses[p]);
  }
}

// The rhombic plate's thin corridor (the known diagnostic snapshot pose): a legal but narrow
// four-face corridor recovers with honest margins.
TEST(EntryMeasureRecovery, RecoversRhombicPlateCorridor) {
  const Built b = Build(RhombicPlate(), { 1, 3, 4, 2 });
  Corridor corridor(b.table, b.polygons, b.slots.data(), 4);
  const double incident[3] = { 0.9876883405951378, 0.0, -0.15643446504023087 };
  const double pose[9] = { 0.258942519, 0.965892733, 0.0, -0.965892733, 0.258942519, 0.0, 0.0, 0.0, 1.0 };
  double s_body[3];
  for (int i = 0; i < 3; i++) {
    s_body[i] = pose[0 * 3 + i] * incident[0] + pose[1 * 3 + i] * incident[1] + pose[2 * 3 + i] * incident[2];
  }
  CorridorDiagnostics diagnostics;
  ASSERT_EQ(corridor.Evaluate(s_body, 1.307, &diagnostics).status, EntryMeasureStatus::kOk);
  RepresentativePath path;
  ASSERT_EQ(corridor.RecoverRepresentativePath(diagnostics, &path), RecoveryStatus::kOk);
  VerifyRecoveredPath(b, 1.307, incident, pose, path);
}

// One arm of the thin-corridor sweep, per shift: the asserts live here so one arm's early
// failure returns from the arm, not out of the test past the other arm.
void RunThinCorridorArm(double shift, bool above_gate) {
  FaceNormalTable normals;
  normals.slot_cnt = 2;
  normals.present[0] = normals.present[1] = true;
  normals.normal[0][0] = 1;
  normals.normal[1][0] = -1;
  FacePolygonTable polygons;
  polygons.corner_cnt[0] = polygons.corner_cnt[1] = 4;
  polygons.min_edge_length = 1;
  const int slots[] = { 0, 1 };
  const double yz[4][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
  for (int i = 0; i < 4; ++i) {
    polygons.corner[0][i][0] = .5;
    polygons.corner[0][i][1] = yz[i][0];
    polygons.corner[0][i][2] = yz[i][1];
    polygons.corner[1][i][0] = -.5;
    polygons.corner[1][i][1] = yz[3 - i][0] + shift;
    polygons.corner[1][i][2] = yz[3 - i][1];
  }
  Corridor corridor(normals, polygons, slots, 2);
  const double incident[] = { -1, 0, 0 };
  CorridorDiagnostics diagnostic;
  const EntryMeasure m = corridor.Evaluate(incident, 1.31, &diagnostic);
  if (above_gate) {
    // shift 1-2e-6: overlap area 2e-6, above the 1e-6 gate — a legal thin corridor recovers a
    // real path whose entry point sits in the overlap sliver.
    ASSERT_EQ(m.status, EntryMeasureStatus::kOk);
    RepresentativePath path;
    ASSERT_EQ(corridor.RecoverRepresentativePath(diagnostic, &path), RecoveryStatus::kOk)
        << RecoveryStatusName(RecoveryStatus::kDegenerate);
    EXPECT_EQ(path.contact_count, 2);
    EXPECT_NEAR(path.contacts[0][1], 0.5 * (1.0 + shift), 1e-6) << "entry point not inside the overlap sliver";
  } else {
    // shift 1-0.5e-6: overlap area 0.5e-6, below the gate — corridor_empty, no geometry, named
    // refusal.
    ASSERT_EQ(m.status, EntryMeasureStatus::kCorridorEmpty);
    RepresentativePath path;
    EXPECT_EQ(corridor.RecoverRepresentativePath(diagnostic, &path), RecoveryStatus::kNotEvaluated);
  }
}

// A corridor just above the area gate still recovers: the centroid of a thin legal sliver sits
// inside by margins the float corners' own noise cannot overturn. Just below the gate there is
// no geometry and the recovery refuses by name.
TEST(EntryMeasureRecovery, ThinCorridorRecoversAboveTheGateAndRefusesBelow) {
  RunThinCorridorArm(1 - 2e-6, true);
  RunThinCorridorArm(1 - 0.5e-6, false);
}

// The refusal vocabulary and the detection power of the containment guards: a diagnostics whose
// intersection does not belong to this evaluation is refused named, never traced. The parallel
// faces 3-6 keep the happy path inside the optical gates (normal incidence on face 5 would exit
// at 120° incidence, past the critical angle).
TEST(EntryMeasureRecovery, RefusesUnevaluatedAndCorruptedGeometryByName) {
  const Built b = Build(Prism(), { 3, 6 });
  Corridor corridor(b.table, b.polygons, b.slots.data(), 2);
  const double* n = b.table.normal[b.slots[0]];
  const double s[3] = { -n[0], -n[1], -n[2] };
  RepresentativePath path;
  // No diagnostics: the corridor never cut an intersection.
  EXPECT_EQ(corridor.RecoverRepresentativePath(CorridorDiagnostics{}, &path), RecoveryStatus::kNotEvaluated);
  // The optical gates refused: no geometry either.
  CorridorDiagnostics diagnostic;
  const double backface[3] = { n[0], n[1], n[2] };
  ASSERT_EQ(corridor.Evaluate(backface, 1.31, &diagnostic).status, EntryMeasureStatus::kEntryBackface);
  EXPECT_EQ(corridor.RecoverRepresentativePath(diagnostic, &path), RecoveryStatus::kNotEvaluated);
  // A corrupted intersection (the ring pushed off the entry face): the guards must catch it.
  ASSERT_EQ(corridor.Evaluate(s, 1.31, &diagnostic).status, EntryMeasureStatus::kOk);
  for (auto& v : diagnostic.vertices) {
    v[0] += 1.0;  // one full crystal-scale offset along the projection's u
  }
  EXPECT_EQ(corridor.RecoverRepresentativePath(diagnostic, &path), RecoveryStatus::kDegenerate);
  EXPECT_EQ(path.contact_count, 0);
  // The named vocabularies spell themselves.
  EXPECT_STREQ(RecoveryStatusName(RecoveryStatus::kDegenerate), "degenerate_corridor");
  EXPECT_STREQ(EntryMeasureStatusName(EntryMeasureStatus::kCorridorEmpty), "corridor_empty");
  EXPECT_STREQ(EntryMeasureStatusName(EntryMeasureStatus::kExitCriticalAngle), "exit_critical_angle");
}

}  // namespace
}  // namespace lumice::analytic
