// The rank-0 criterion (src/analytic/path_rank.hpp, LI docs/band-sum-contract.md section 5): the
// fold matrix is the identity and the wedge is zero. Checked against an oracle that does not read
// the geometry — the outgoing direction at every valid pose of a lattice of sun directions is the
// incident one (deviation 0) — over every realisable face sequence of two and three faces on the
// regular prism and the fixtures' asymmetric pyramid, and of four faces on the prism.
//
// symmetry_semantics: none — each sequence is one concrete path (doc/analytic-api.md section 3).

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "analytic/discovery.hpp"
#include "analytic/path_chain.hpp"
#include "analytic/path_rank.hpp"

namespace lumice::analytic {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kOracleSampleCount = 256;

LUMICE_ANALYTIC_Crystal RegularPrism() {
  LUMICE_ANALYTIC_Crystal c{};
  c.kind = LUMICE_ANALYTIC_CRYSTAL_PRISM;
  c.height = 1.0;
  for (double& d : c.face_distance) {
    d = 1.0;
  }
  return c;
}

// The pyramid of the LI fixtures 13-15-26-28__* (both cones partial, uneven face distances).
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

struct OracleResult {
  int valid = 0;
  bool all_forward = true;
};

// Deviation at every valid lattice pose; the path's fields depend on the pose only through u.
OracleResult Oracle(const FaceNormalTable& table, const std::vector<int>& slots) {
  OracleResult r;
  const double identity[9] = { 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };
  for (int i = 0; i < kOracleSampleCount; i++) {
    double u[3];
    LatticePoint(kOracleSampleCount, i, u);
    const double incident[3] = { -u[0], -u[1], -u[2] };
    double out[3];
    if (!TracePathChain<double>(table, slots.data(), static_cast<int>(slots.size()), 1.31, incident, identity, out,
                                nullptr, nullptr)) {
      continue;
    }
    r.valid++;
    const double c = out[0] * incident[0] + out[1] * incident[1] + out[2] * incident[2];
    const double s[3] = { out[1] * incident[2] - out[2] * incident[1], out[2] * incident[0] - out[0] * incident[2],
                          out[0] * incident[1] - out[1] * incident[0] };
    const double deviation = std::atan2(std::sqrt(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]), c);
    r.all_forward = r.all_forward && deviation <= 1e-10;
  }
  return r;
}

std::string Name(const FaceNormalTable& table, const std::vector<int>& slots) {
  std::string s;
  for (int slot : slots) {
    s += (s.empty() ? "" : "-") + std::to_string(table.face_number[slot]);
  }
  return s;
}

// Every sequence of `length` present slots, no face repeated consecutively (a repeat is never valid).
void Enumerate(const FaceNormalTable& table, int length, std::vector<int>* prefix, std::vector<std::vector<int>>* out) {
  if (static_cast<int>(prefix->size()) == length) {
    out->push_back(*prefix);
    return;
  }
  for (int s = 0; s < table.slot_cnt; s++) {
    if (!table.present[s] || (!prefix->empty() && prefix->back() == s)) {
      continue;
    }
    prefix->push_back(s);
    Enumerate(table, length, prefix, out);
    prefix->pop_back();
  }
}

std::vector<std::string> RankZeroAgreesWithOracle(const LUMICE_ANALYTIC_Crystal& crystal, int max_length,
                                                  int* realisable) {
  FaceNormalTable table;
  EXPECT_EQ(BuildFaceNormals(crystal, &table), Status::kOk);
  std::vector<std::string> rank_zero;
  for (int length = 2; length <= max_length; length++) {
    std::vector<std::vector<int>> paths;
    std::vector<int> prefix;
    Enumerate(table, length, &prefix, &paths);
    for (const std::vector<int>& slots : paths) {
      const OracleResult oracle = Oracle(table, slots);
      if (oracle.valid == 0) {
        continue;  // no realisable pose: the question does not arise
      }
      (*realisable)++;
      const bool criterion = IsRankZeroPath(table, slots.data(), static_cast<int>(slots.size()));
      EXPECT_EQ(criterion, oracle.all_forward) << Name(table, slots) << " valid poses " << oracle.valid;
      if (criterion) {
        rank_zero.push_back(Name(table, slots));
      }
    }
  }
  return rank_zero;
}

TEST(PathRank, PrismRankZeroIsExactlyTheParallelSlabs) {
  int realisable = 0;
  const std::vector<std::string> found = RankZeroAgreesWithOracle(RegularPrism(), 4, &realisable);
  EXPECT_GT(realisable, 100);
  // Two faces: the three opposite prism pairs and the basal pair, both ways. Three faces: none (one
  // reflection is never the identity). Four faces: a slab entered and left through a parallel pair,
  // with two reflections off one parallel pair (S_n S_(-n) = I).
  for (const char* p : { "1-2", "2-1", "3-6", "6-3", "4-7", "7-4", "5-8", "8-5" }) {
    EXPECT_NE(std::find(found.begin(), found.end(), p), found.end()) << p;
  }
  for (const std::string& p : found) {
    const int dashes = static_cast<int>(std::count(p.begin(), p.end(), '-'));
    EXPECT_NE(dashes, 2) << p;
  }
  std::cout << "[path-rank] prism rank-0 paths:";
  for (const std::string& p : found) {
    std::cout << ' ' << p;
  }
  std::cout << '\n';
}

TEST(PathRank, AsymmetricPyramidAgreesWithOracle) {
  int realisable = 0;
  const std::vector<std::string> found = RankZeroAgreesWithOracle(AsymmetricPyramid(), 3, &realisable);
  EXPECT_GT(realisable, 100);
  EXPECT_NE(std::find(found.begin(), found.end(), "1-2"), found.end());
  EXPECT_NE(std::find(found.begin(), found.end(), "3-6"), found.end());
}

TEST(PathRank, NonzeroWedgeIsRankTwo) {
  FaceNormalTable table;
  ASSERT_EQ(BuildFaceNormals(RegularPrism(), &table), Status::kOk);
  const int prism_22[2] = { table.SlotOf(3), table.SlotOf(5) };  // 60 degree wedge
  const int prism_46[2] = { table.SlotOf(1), table.SlotOf(3) };  // 90 degree wedge
  EXPECT_FALSE(IsRankZeroPath(table, prism_22, 2));
  EXPECT_FALSE(IsRankZeroPath(table, prism_46, 2));
  (void)kPi;
}

}  // namespace
}  // namespace lumice::analytic
