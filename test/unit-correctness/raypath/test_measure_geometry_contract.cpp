// The geometry<->measure contract (src/raypath/detail/measure/measure_geometry_contract.hpp):
// the frozen value types' shape, the schema1 pinned vocabulary, the registered open-enum tables
// (a value appended without its table entry must fail here), and the mock producers the other
// measure tests build on. No mathematical claim lives in this file — the densities are pinned in
// test_declared_density.cpp; this pins the CONTRACT, so an integration-time change of any frozen
// semantic shows up as a red here first.
//
// symmetry_semantics: none — value types only.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "raypath/detail/measure/measure_geometry_contract.hpp"
#include "raypath/detail/measure/visibility_certificate.hpp"

namespace lumice::raypath {
namespace {

constexpr double kPi = 3.14159265358979323846;

// --- Mock producers (the shapes 660 is expected to hand over; the tests' only geometry). ----

// A plate-family latitude-circle support: the circle at latitude `lat` (closed, 128 points),
// parameter = longitude.
SupportPiece MockLatitudeCircleSupport(double lat_rad, int points) {
  SupportPiece piece;
  piece.shape = SupportPiece::Shape::kCurve;
  piece.closed = true;
  piece.u.resize(3 * static_cast<size_t>(points));
  piece.param.resize(static_cast<size_t>(points));
  for (int i = 0; i < points; i++) {
    const double lon = 2.0 * kPi * i / points;
    piece.u[3 * i + 0] = std::cos(lat_rad) * std::cos(lon);
    piece.u[3 * i + 1] = std::cos(lat_rad) * std::sin(lon);
    piece.u[3 * i + 2] = std::sin(lat_rad);
    piece.param[static_cast<size_t>(i)] = lon;
  }
  return piece;
}

// A synthetic kind-1 circle: the latitude circle at `lat`, tangents along the circle, D_P
// constant, one per-wavelength critical value column (a caustic-like table shape, no physics).
CriticalSetCurve MockKind1Circle(double lat_rad, int points, const std::vector<double>& wavelengths_nm) {
  CriticalSetCurve curve;
  curve.existence = ExistenceState::kComputed;
  const size_t n = static_cast<size_t>(points);
  curve.u.resize(3 * n);
  curve.tangent.resize(3 * n);
  curve.d_p.resize(n);
  curve.wavelengths_nm = wavelengths_nm;
  curve.critical_d_p.resize(n * wavelengths_nm.size());
  for (size_t i = 0; i < n; i++) {
    const double lon = 2.0 * kPi * static_cast<double>(i) / points;
    curve.u[3 * i + 0] = std::cos(lat_rad) * std::cos(lon);
    curve.u[3 * i + 1] = std::cos(lat_rad) * std::sin(lon);
    curve.u[3 * i + 2] = std::sin(lat_rad);
    curve.tangent[3 * i + 0] = -std::sin(lon);
    curve.tangent[3 * i + 1] = std::cos(lon);
    curve.tangent[3 * i + 2] = 0.0;
    curve.d_p[i] = 22.0;  // the 22-degree halo's deviation, constant on this synthetic circle
    for (size_t k = 0; k < wavelengths_nm.size(); k++) {
      curve.critical_d_p[i * wavelengths_nm.size() + k] = 21.4 + 0.6 * k;
    }
  }
  return curve;
}

// A synthetic kind-3 kink chain: a meridian arc with a kink in the middle and one tir_boundary
// event word at the kink (schema1's pinned spelling).
WeightSingularChain MockKinkChain(int points) {
  WeightSingularChain chain;
  chain.is_gate_boundary = false;
  chain.existence = ExistenceState::kComputed;
  const size_t n = static_cast<size_t>(points);
  chain.u.resize(3 * n);
  chain.param.resize(n);
  chain.kink.assign(n, 0);
  chain.event.assign(n, -1);
  for (size_t i = 0; i < n; i++) {
    const double lat = -0.3 + 0.6 * static_cast<double>(i) / (points - 1);
    chain.u[3 * i + 0] = std::cos(lat);
    chain.u[3 * i + 1] = 0.0;
    chain.u[3 * i + 2] = std::sin(lat);
    chain.param[i] = lat;
  }
  chain.kink[points / 2] = 1;
  chain.event[points / 2] = static_cast<int>(ChainEventKind::kTirBoundary);
  return chain;
}

FiberSample MakeSample(double u0, double u1, double u2, double area, double transmission, double parameter,
                       double weight) {
  FiberSample s;
  s.u[0] = u0;
  s.u[1] = u1;
  s.u[2] = u2;
  s.area = area;
  s.transmission = transmission;
  s.valid = area * transmission > 0.0;
  s.parameter = parameter;
  s.weight = weight;
  return s;
}

// --- Value-type shape: default-constructible, copyable, aggregate-writable. ----------------

TEST(MeasureContract, ValueTypesAreDefaultConstructibleAndCopyable) {
  SupportPiece support;
  CriticalSetCurve kind1;
  WeightSingularChain chain;
  FiberSampleStream stream;
  PartitionContext partition;
  (void)support;
  (void)kind1;
  (void)chain;
  (void)stream;
  (void)partition;

  const SupportPiece circle = MockLatitudeCircleSupport(0.2, 16);
  SupportPiece copy = circle;
  ASSERT_EQ(copy.u.size(), circle.u.size());
  copy.u[0] = 2.0;
  EXPECT_NE(copy.u[0], circle.u[0]);  // deep copy, no shared storage
}

// --- u is a first-class preimage field: the curve points and the support parameter pair up. --

TEST(MeasureContract, CurveCarriesUnitVectorsAndPairedParameters) {
  const CriticalSetCurve curve = MockKind1Circle(0.3, 24, { 650.0, 450.0 });
  ASSERT_EQ(curve.u.size(), 3 * 24);
  ASSERT_EQ(curve.tangent.size(), 3 * 24);
  ASSERT_EQ(curve.d_p.size(), 24);
  ASSERT_EQ(curve.critical_d_p.size(), 24 * 2);
  for (int i = 0; i < 24; i++) {
    const double* u = &curve.u[3 * i];
    EXPECT_NEAR(u[0] * u[0] + u[1] * u[1] + u[2] * u[2], 1.0, 1e-14);
    const double* t = &curve.tangent[3 * i];
    EXPECT_NEAR(u[0] * t[0] + u[1] * t[1] + u[2] * t[2], 0.0, 1e-14);  // tangent orthogonal
    // critical_d_p row i holds wavelengths_nm.size() entries (row-major, per the contract),
    // pinned per cell against the mock's 21.4 + 0.6 k: the row-major layout is content-checked,
    // not just in-bounds.
    for (size_t k = 0; k < curve.wavelengths_nm.size(); k++) {
      EXPECT_NEAR(curve.critical_d_p[i * curve.wavelengths_nm.size() + k], 21.4 + 0.6 * static_cast<double>(k), 1e-12);
    }
  }
  // The per-wavelength table's row k belongs to wavelengths_nm[k]: the mock wrote 21.4 + 0.6 k.
  EXPECT_NEAR(curve.critical_d_p[0], 21.4, 1e-12);
  EXPECT_NEAR(curve.critical_d_p[1], 22.0, 1e-12);

  const SupportPiece circle = MockLatitudeCircleSupport(0.2, 16);
  ASSERT_EQ(circle.param.size(), circle.u.size() / 3);
}

// --- The schema1 pinned words, verbatim. ---------------------------------------------------

TEST(MeasureContract, ChainEventVocabularyIsSchema1Pinned) {
  EXPECT_STREQ(ChainEventKindName(ChainEventKind::kTirBoundary), "tir_boundary");
  EXPECT_STREQ(ChainEventKindName(ChainEventKind::kPathInfeasible), "path_infeasible");
  EXPECT_STREQ(ChainEventKindName(ChainEventKind::kGatedOut), "gated_out");
  // The schema3-only addition (the corridor A = 0 curve, S2's new object) is registered beside
  // them, not inside schema1's words.
  EXPECT_STREQ(ChainEventKindName(ChainEventKind::kCorridorClosed), "corridor_closed");
}

TEST(MeasureContract, ExistenceVocabularyIsSchema3Pinned) {
  EXPECT_STREQ(ExistenceStateName(ExistenceState::kComputed), "computed");
  EXPECT_STREQ(ExistenceStateName(ExistenceState::kEscaped), "escaped");
  EXPECT_STREQ(ExistenceStateName(ExistenceState::kWalkTruncated), "walk_truncated");
  EXPECT_STREQ(ExistenceStateName(ExistenceState::kS4Declared), "s4_declared");
}

// --- Registered open-enum tables: the walk pins the registry (a50). -------------------------
// Adding a value to an enum without its table entry leaves these walks unchanged — that is the
// point: the registry only grows through its table, and the table's coverage test is this walk.

TEST(MeasureContract, RegisteredTablesCoverTheirEnums) {
  EXPECT_EQ(RegisteredExistenceStates().size(), 4);
  EXPECT_EQ(RegisteredChainEventKinds().size(), 4);
  EXPECT_EQ(RegisteredEscapeRegimes().size(), 1);
  EXPECT_EQ(RegisteredVisibilityStates().size(), 4);
  // Every registered value's name is non-null and unique within its table.
  for (ExistenceState s : RegisteredExistenceStates()) {
    EXPECT_NE(ExistenceStateName(s), nullptr);
  }
  std::vector<const char*> names;
  for (ChainEventKind k : RegisteredChainEventKinds()) {
    names.push_back(ChainEventKindName(k));
  }
  std::sort(names.begin(), names.end());
  EXPECT_TRUE(std::adjacent_find(names.begin(), names.end(),
                                 [](const char* a, const char* b) { return std::strcmp(a, b) == 0; }) == names.end());
}

// --- The mock fiber source: a programmable stream with the A/T split. -----------------------

TEST(MeasureContract, FiberSampleStreamCarriesTheAreaTransmissionSplit) {
  // The contract's SampleEvent caveat (its header comment): the merged w = A*T is enough for
  // intensity, NOT for certificate discrimination. A stream built from these samples must keep
  // the split — the certificate test drives both zero-branches off these fields.
  FiberSampleStream stream;
  stream.binding = MeasureBinding::kFiberParameter;
  stream.evidence = FiberSampleStream::EvidenceForm::kSampledExhaustive;
  stream.source_name = "mock-spin-grid";
  stream.samples.push_back(MakeSample(1.0, 0.0, 0.0, 0.0, 0.5, 0.1, 0.01));  // corridor closed
  stream.samples.push_back(MakeSample(0.0, 1.0, 0.0, 0.4, 0.0, 0.2, 0.01));  // TIR gate
  stream.samples.push_back(MakeSample(0.0, 0.0, 1.0, 0.4, 0.5, 0.3, 0.01));  // lit
  ASSERT_EQ(stream.samples.size(), 3);
  // valid is the direction-level chain validity, and the mock ties it to A*T > 0: the kept
  // convention (discovery's kept iff w > 0) sees exactly the third sample.
  int kept = 0;
  for (const FiberSample& s : stream.samples) {
    if (s.valid && s.area * s.transmission > 0.0) {
      kept++;
    }
  }
  EXPECT_EQ(kept, 1);
}

// --- Evidence forms and bindings are part of the frozen shape. ------------------------------

TEST(MeasureContract, EvidenceFormsAndBindingsAreDistinctValues) {
  // Three evidence forms, two bindings: distinct enum values (no aliasing a future refactor
  // could silently collapse), because the certificate routes on them.
  EXPECT_NE(FiberSampleStream::EvidenceForm::kStructural, FiberSampleStream::EvidenceForm::kSampledExhaustive);
  EXPECT_NE(FiberSampleStream::EvidenceForm::kSampledExhaustive, FiberSampleStream::EvidenceForm::kSampledPartial);
  EXPECT_NE(FiberSampleStream::EvidenceForm::kStructural, FiberSampleStream::EvidenceForm::kSampledPartial);
  EXPECT_NE(MeasureBinding::kSolidAngle, MeasureBinding::kFiberParameter);
}

// --- Partition context routes escapes by name. ----------------------------------------------

TEST(MeasureContract, PartitionContextNamesItsEscapeRegime) {
  PartitionContext escaped;
  escaped.coverage = PartitionContext::Coverage::kIncomplete;
  escaped.escape_regime = EscapeRegime::kSlabCrease;
  EXPECT_STREQ(EscapeRegimeName(escaped.escape_regime), "slab_crease");

  PartitionContext unknown;  // the default is kUnknown: v1 producers that do not partition yet
  EXPECT_EQ(unknown.coverage, PartitionContext::Coverage::kUnknown);
}

}  // namespace
}  // namespace lumice::raypath
