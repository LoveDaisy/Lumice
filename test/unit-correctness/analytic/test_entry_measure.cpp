// Finite-crystal entry measure (src/analytic/entry_measure.hpp).

#include <gtest/gtest.h>

#include <cmath>
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
// random poses): the status of every row, and the value up to the unit — this library's crystals are
// half the size of LI's in length (value ratio 0.25). Over 12000 such poses the statuses agreed
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

}  // namespace
}  // namespace lumice::analytic
