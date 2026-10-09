// The MC attribution core (src/raypath/detail/schema3/mc_attribution.{hpp,cpp}): the two-way
// detector between the structural objects and the demoted MC evidence. Pinned here, per the
// plan's red-first discipline: the judgment table CELL BY CELL (observed / the red flag /
// consistent / not_observed_insufficient_ess on both visibility arms / unchecked on both of its
// arms), the forward pass with its ESS gate and margin (the AC2 negative control included), the
// two review refinements (a match below the ESS floor is not `observed` and is not silently
// dropped — it falls to the presence rows with a matched-but-below-threshold note; `observed`
// reads the matched record's OWN ESS), the declared-width tolerance (the dominant-spread rule),
// the fail-visible h mismatch, the delta-image branches (support-row criticals for kind-1, the
// u-preimage sampling for the rest), and the presence-ESS statistic itself.
//
// symmetry_semantics: label — the plate members are the L1/PBD vocabulary (the C11/C12 corpus
// configs). The sun sits at the zenith in the synthetic records: delta = polar angle, so the
// record placement arithmetic is exact by construction.
//
// Provenance: the object images come from the REAL producers (the beta 3-5 support row, the
// beta kind-3 u points, the C12 restricted stream) — the table cells are synthetic, the
// geometry under them is not.

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "core/optics.hpp"
#include "raypath/detail/input_assembly.hpp"
#include "raypath/detail/path_feature_report.hpp"
#include "raypath/detail/schema3/mc_attribution.hpp"
#include "raypath/detail/schema3/mc_evidence.hpp"
#include "raypath/path_feature_report.hpp"
#include "raypath/scene_to_analytic.hpp"

namespace lumice::raypath::schema3 {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;
constexpr double kH = 1.0 * kDeg;  // the synthetic observation bandwidth (v2's default)
constexpr size_t kNoIndex = std::numeric_limits<size_t>::max();

struct Tables {
  analytic::FaceNormalTable normals;
  analytic::FacePolygonTable polys;
};

// test_dp_capi.cpp's Beta (the C05/C06 crystal).
Tables Beta() {
  Tables t;
  analytic::CrystalShape shape;
  shape.kind = analytic::CrystalShapeKind::kPrism;
  shape.height = 3.0;
  const double fd[6] = { 2.0, 1.0, 1.0, 2.0, 1.0, 1.0 };
  for (int i = 0; i < 6; i++) {
    shape.face_distance[i] = fd[i];
  }
  EXPECT_EQ(analytic::BuildFaceNormals(shape, &t.normals, &t.polys), analytic::Status::kOk);
  return t;
}

// test_fiber_quadrature.cpp's RhombicPlate (the C12 crystal).
Tables RhombicPlate() {
  Tables t;
  analytic::CrystalShape shape;
  shape.kind = analytic::CrystalShapeKind::kPrism;
  shape.height = 1.0;
  const double fd[6] = { 1.5, 1, 1, 1.5, 1, 1 };
  for (int i = 0; i < 6; i++) {
    shape.face_distance[i] = fd[i];
  }
  EXPECT_EQ(analytic::BuildFaceNormals(shape, &t.normals, &t.polys), analytic::Status::kOk);
  return t;
}

// The sky point at polar angle `delta` from the +z sun (the synthetic placement arithmetic).
std::array<double, 3> SkyAt(double delta) {
  return { std::sin(delta), 0.0, std::cos(delta) };
}

DiagnosticFeatureRecord ActualAt(const std::vector<std::array<double, 3>>& sky_points, double ess) {
  DiagnosticFeatureRecord record;
  record.evidence = DiagnosticEvidence::kActual;
  record.kind = "synthetic_actual";
  record.sky_points = sky_points;
  record.minimum_effective_samples = ess;
  record.bandwidth_rad = kH;
  return record;
}

analytic::WeightedSkySample ComponentAt(double delta, uint64_t sample_index) {
  analytic::WeightedSkySample sample;
  sample.sample_index = sample_index;  // one row per draw: the draw-grouped presence reads 1
                                       // Kish unit per component
  sample.direction = SkyAt(delta);
  return sample;
}

// The evidence block: `records` + `components` (each a delta in radians) + the declared ruler.
McEvidenceBlock EvidenceOf(const std::vector<DiagnosticFeatureRecord>& records,
                           const std::vector<double>& component_deltas) {
  DiscoveryResult result;
  result.features = records;
  for (size_t i = 0; i < component_deltas.size(); i++) {
    result.measure.components.push_back(ComponentAt(component_deltas[i], i));
  }
  DiscoveryOptions options;
  options.bandwidth_rad = kH;
  return McEvidenceOf(std::move(result), options);
}

McAttributionInput InputOf(const Schema3DiscoveryCore& core, const McEvidenceBlock& mc, const Tables& tables) {
  McAttributionInput input;
  input.core = &core;
  input.mc = &mc;
  input.geometry.normals = &tables.normals;
  input.geometry.polygons = &tables.polys;
  input.geometry.base_index = 1.3110129;
  input.geometry.sun_dir[0] = 0.0;
  input.geometry.sun_dir[1] = 0.0;
  input.geometry.sun_dir[2] = 1.0;
  input.h_rad = kH;
  return input;
}

// The C05 (4-8-7-5) kind-1 core (no measure side: the visibility defaults to unproven, the
// tests set the arms they need by hand — the certificate's production is the enumeration
// layer's subject). The corpus's "3-5" is the CONFIG-prism chain; this analytic prism table
// does not host that face literal (probed: SlotOf rejects it), and the fixture needs no
// specific member — only a real kind-1 over a real support row.
struct Kind1Core {
  Schema3DiscoveryCore core;
  StructureObjectRecord* kind1 = nullptr;
  MemberSupport* row = nullptr;
};

Kind1Core Kind1OnC05(const Tables& tables) {
  EnumerationInput in;
  in.normals = &tables.normals;
  in.polygons = &tables.polys;
  in.base_index = 1.3110129;
  Kind1Core out;
  out.core = EnumerateLayer(in, { { 4, 8, 7, 5 } });
  for (StructureObjectRecord& object : out.core.objects) {
    if (object.kind == ObjectKind::kKind1) {
      out.kind1 = &object;
    }
  }
  EXPECT_NE(out.kind1, nullptr) << "4-8-7-5 must produce its kind-1 object";
  EXPECT_EQ(out.kind1->existence, ExistenceState::kComputed);
  // The table cells run at tolerance = h: the real chromatic leg's dominant spread on this
  // member is tens of degrees and would swallow every no-match arm. Zero it here; the declared
  // width's behavior is pinned by its own test, which sets the features it needs.
  out.kind1->chromatic_assessed = false;
  out.kind1->chromatic.features.clear();
  // ...and the cells run against THIS object only: the member's S2 chains and kinks have dense
  // delta images that would attribute the very records the no-match arms need. The multi-object
  // attribution behavior is the integration test's subject (below).
  std::vector<StructureObjectRecord> only_kind1;
  for (StructureObjectRecord& object : out.core.objects) {
    if (object.kind == ObjectKind::kKind1) {
      only_kind1.push_back(std::move(object));
    }
  }
  out.core.objects = std::move(only_kind1);
  out.kind1 = &out.core.objects[0];
  EXPECT_FALSE(out.core.support.members.empty());
  out.row = &out.core.support.members[0];
  return out;
}

const StructureObjectRecord* FindKind(const std::vector<StructureObjectRecord>& objects, ObjectKind kind) {
  for (const StructureObjectRecord& object : objects) {
    if (object.kind == kind) {
      return &object;
    }
  }
  return nullptr;
}

// The expectation arithmetic the unattributed margin test reads off the same image: the margin
// is distance - tolerance, both declared.
double ExpectedMargin(const std::vector<double>& image, double record_delta) {
  double distance = std::numeric_limits<double>::infinity();
  for (const double value : image) {
    distance = std::min(distance, std::fabs(record_delta - value));
  }
  return distance - kH;
}

// ---- the forward pass: the unattributed positive and the ESS gate ------------------------------

TEST(McAttribution, UnattributedPositiveCarriesMarginAndRuler) {
  const Tables t = Beta();
  Kind1Core fixture = Kind1OnC05(t);
  const ObjectDeltaImage image =
      ObjectDeltaImageOf(*fixture.kind1, fixture.core, InputOf(fixture.core, McEvidenceBlock{}, t).geometry, nullptr);
  ASSERT_TRUE(image.available);
  // A significant MC peak far from every critical value (90 deg): unattributed, margin > 0.
  const double peak = 90.0 * kDeg;
  McEvidenceBlock mc = EvidenceOf({ ActualAt({ SkyAt(peak), SkyAt(peak + 0.1 * kDeg) }, 1000.0) }, {});
  const McAttributionInput input = InputOf(fixture.core, mc, t);
  const UnattributedOutcome outcome = DeriveUnattributed(input);
  EXPECT_TRUE(outcome.error.empty());
  ASSERT_EQ(outcome.structures.size(), 1u);
  EXPECT_EQ(outcome.structures[0].record_index, 0u);
  EXPECT_DOUBLE_EQ(outcome.structures[0].record_ess, 1000.0);
  // The reported position is the record's sky point of closest approach: of the two points
  // (90, 90.1 deg), the 90.1 one sits closer to the 120-deg critical, so it is the reported one
  // and the margin is its gap minus the tolerance.
  EXPECT_NEAR(outcome.structures[0].delta_rad / kDeg, 90.1, 1e-9);
  const double expected =
      std::min(ExpectedMargin(image.delta_rad, peak), ExpectedMargin(image.delta_rad, peak + 0.1 * kDeg));
  EXPECT_NEAR(outcome.structures[0].min_margin_rad, expected, 1e-9);
  EXPECT_GT(outcome.structures[0].min_margin_rad, 0.0);
  EXPECT_FALSE(outcome.structures[0].ruler.empty());
  // The kind-1 image reads the support row: no chain evaluation happened on this fixture.
  EXPECT_EQ(outcome.counts.dp_evaluations, 0);
}

TEST(McAttribution, StarvedRecordIsSilentButCounted) {
  // The AC2 negative control: a kActual record BELOW the ESS floor produces no finding — and
  // the silence is counted, not hidden.
  const Tables t = Beta();
  Kind1Core fixture = Kind1OnC05(t);
  McEvidenceBlock mc = EvidenceOf({ ActualAt({ SkyAt(90.0 * kDeg) }, 5.0) }, {});
  const UnattributedOutcome outcome = DeriveUnattributed(InputOf(fixture.core, mc, t));
  EXPECT_TRUE(outcome.error.empty());
  EXPECT_TRUE(outcome.structures.empty());
  EXPECT_EQ(outcome.skipped_below_ess, 1);
  EXPECT_EQ(outcome.skipped_no_position, 0);
}

TEST(McAttribution, RecordWithoutPositionIsSkippedAndCounted) {
  const Tables t = Beta();
  Kind1Core fixture = Kind1OnC05(t);
  McEvidenceBlock mc = EvidenceOf({ ActualAt({}, 1000.0) }, {});
  const UnattributedOutcome outcome = DeriveUnattributed(InputOf(fixture.core, mc, t));
  EXPECT_TRUE(outcome.structures.empty());
  EXPECT_EQ(outcome.skipped_no_position, 1);
}

// ---- the judgment table, cell by cell -----------------------------------------------------------

TEST(McAttribution, ObservedArmReadsTheMatchedRecordsOwnEss) {
  const Tables t = Beta();
  Kind1Core fixture = Kind1OnC05(t);
  const ObjectDeltaImage image =
      ObjectDeltaImageOf(*fixture.kind1, fixture.core, InputOf(fixture.core, McEvidenceBlock{}, t).geometry, nullptr);
  ASSERT_TRUE(image.available);
  // A record AT a critical value with its OWN ESS at the floor: observed.
  McEvidenceBlock mc = EvidenceOf({ ActualAt({ SkyAt(image.delta_rad.front()) }, 32.0) }, {});
  const CorroborationOutcome outcome = DeriveCorroboration(InputOf(fixture.core, mc, t));
  EXPECT_TRUE(outcome.error.empty());
  const size_t kind1_index = static_cast<size_t>(fixture.kind1 - fixture.core.objects.data());
  ASSERT_GT(outcome.annotations.size(), kind1_index);
  EXPECT_EQ(outcome.annotations[kind1_index].state, CorroborationState::kObserved);
  EXPECT_EQ(outcome.annotations[kind1_index].matched_record, 0);
  EXPECT_EQ(outcome.core.objects[kind1_index].corroboration, CorroborationState::kObserved);
  // The same match with the record's ESS one notch below the floor is NOT observed: the review's
  // refinement — the fall-through arm takes over (the next test pins where it lands).
  McEvidenceBlock starved = EvidenceOf({ ActualAt({ SkyAt(image.delta_rad.front()) }, 31.9) }, {});
  const CorroborationOutcome starved_outcome = DeriveCorroboration(InputOf(fixture.core, starved, t));
  EXPECT_EQ(starved_outcome.annotations[kind1_index].state, CorroborationState::kNotObservedInsufficientEss);
  EXPECT_EQ(starved_outcome.annotations[kind1_index].matched_record, 0);
  EXPECT_NE(starved_outcome.annotations[kind1_index].reason.find("matched-but-below-threshold"), std::string::npos);
}

TEST(McAttribution, LitObjectNoMatchSufficientPresenceIsTheRedFlag) {
  const Tables t = Beta();
  Kind1Core fixture = Kind1OnC05(t);
  const ObjectDeltaImage image =
      ObjectDeltaImageOf(*fixture.kind1, fixture.core, InputOf(fixture.core, McEvidenceBlock{}, t).geometry, nullptr);
  ASSERT_TRUE(image.available);
  fixture.kind1->visibility.state = VisibilityState::kCertified;
  // MC sampled the object's window with standing (64 draws at the image), yet its one
  // significant record sits far away at 90 deg: the divergence red flag.
  std::vector<double> components;
  for (int i = 0; i < 64; i++) {
    components.push_back(image.delta_rad.front() + (i % 2) * 0.1 * kDeg);
  }
  McEvidenceBlock mc = EvidenceOf({ ActualAt({ SkyAt(90.0 * kDeg) }, 1000.0) }, components);
  const CorroborationOutcome outcome = DeriveCorroboration(InputOf(fixture.core, mc, t));
  const size_t kind1_index = static_cast<size_t>(fixture.kind1 - fixture.core.objects.data());
  EXPECT_EQ(outcome.annotations[kind1_index].state, CorroborationState::kNotObservedDespiteSufficientEss);
  EXPECT_GE(outcome.annotations[kind1_index].presence_ess, 32.0);
  EXPECT_EQ(outcome.annotations[kind1_index].matched_record, 0);  // the nearest record, not a match
  EXPECT_GT(outcome.annotations[kind1_index].match_distance, outcome.annotations[kind1_index].tolerance_rad);
}

TEST(McAttribution, UnlitObjectNoMatchSufficientPresenceIsConsistent) {
  const Tables t = Beta();
  Kind1Core fixture = Kind1OnC05(t);
  const ObjectDeltaImage image =
      ObjectDeltaImageOf(*fixture.kind1, fixture.core, InputOf(fixture.core, McEvidenceBlock{}, t).geometry, nullptr);
  ASSERT_TRUE(image.available);
  fixture.kind1->visibility.state = VisibilityState::kUnlit;
  std::vector<double> components;
  for (int i = 0; i < 64; i++) {
    components.push_back(image.delta_rad.front() + (i % 2) * 0.1 * kDeg);
  }
  McEvidenceBlock mc = EvidenceOf({ ActualAt({ SkyAt(90.0 * kDeg) }, 1000.0) }, components);
  const CorroborationOutcome outcome = DeriveCorroboration(InputOf(fixture.core, mc, t));
  const size_t kind1_index = static_cast<size_t>(fixture.kind1 - fixture.core.objects.data());
  EXPECT_EQ(outcome.annotations[kind1_index].state, CorroborationState::kConsistent);
  // The weight information survives the label: the annotation carries the standing.
  EXPECT_GE(outcome.annotations[kind1_index].presence_ess, 32.0);
  EXPECT_NE(outcome.annotations[kind1_index].reason.find("unlit"), std::string::npos);
}

TEST(McAttribution, UnprovenObjectNoMatchSufficientPresenceIsConsistentWithReason) {
  const Tables t = Beta();
  Kind1Core fixture = Kind1OnC05(t);
  const ObjectDeltaImage image =
      ObjectDeltaImageOf(*fixture.kind1, fixture.core, InputOf(fixture.core, McEvidenceBlock{}, t).geometry, nullptr);
  ASSERT_TRUE(image.available);
  // The enumeration default (no measure side): unproven, nothing to contradict.
  EXPECT_EQ(fixture.kind1->visibility.state, VisibilityState::kUnproven);
  std::vector<double> components;
  for (int i = 0; i < 64; i++) {
    components.push_back(image.delta_rad.front());
  }
  McEvidenceBlock mc = EvidenceOf({ ActualAt({ SkyAt(90.0 * kDeg) }, 1000.0) }, components);
  const CorroborationOutcome outcome = DeriveCorroboration(InputOf(fixture.core, mc, t));
  const size_t kind1_index = static_cast<size_t>(fixture.kind1 - fixture.core.objects.data());
  EXPECT_EQ(outcome.annotations[kind1_index].state, CorroborationState::kConsistent);
  EXPECT_NE(outcome.annotations[kind1_index].reason.find("nothing the MC could contradict"), std::string::npos);
}

TEST(McAttribution, LowPresenceRoutesBothArmsToNotObservedInsufficientEss) {
  const Tables t = Beta();
  Kind1Core fixture = Kind1OnC05(t);
  const ObjectDeltaImage image =
      ObjectDeltaImageOf(*fixture.kind1, fixture.core, InputOf(fixture.core, McEvidenceBlock{}, t).geometry, nullptr);
  ASSERT_TRUE(image.available);
  // Lit arm: 8 components at the image (standing below the floor).
  fixture.kind1->visibility.state = VisibilityState::kPartial;
  fixture.kind1->visibility.lit_fraction = 0.5;
  McEvidenceBlock lit =
      EvidenceOf({ ActualAt({ SkyAt(90.0 * kDeg) }, 1000.0) }, std::vector<double>(8, image.delta_rad.front()));
  const CorroborationOutcome lit_outcome = DeriveCorroboration(InputOf(fixture.core, lit, t));
  const size_t kind1_index = static_cast<size_t>(fixture.kind1 - fixture.core.objects.data());
  EXPECT_EQ(lit_outcome.annotations[kind1_index].state, CorroborationState::kNotObservedInsufficientEss);
  EXPECT_LT(lit_outcome.annotations[kind1_index].presence_ess, 32.0);
  // Unlit arm: the same standing — the state is the same and the reason contract spells the
  // reading (the review's option (b): the label is shared, the reading is not left to guesswork).
  fixture.kind1->visibility.state = VisibilityState::kUnlit;
  fixture.kind1->visibility.lit_fraction = 0.0;
  McEvidenceBlock unlit =
      EvidenceOf({ ActualAt({ SkyAt(90.0 * kDeg) }, 1000.0) }, std::vector<double>(8, image.delta_rad.front()));
  const CorroborationOutcome unlit_outcome = DeriveCorroboration(InputOf(fixture.core, unlit, t));
  EXPECT_EQ(unlit_outcome.annotations[kind1_index].state, CorroborationState::kNotObservedInsufficientEss);
  EXPECT_NE(unlit_outcome.annotations[kind1_index].reason.find("leaves the corroboration itself undeclared"),
            std::string::npos);
}

TEST(McAttribution, NonComputedAndImagelessObjectsStayUnchecked) {
  const Tables t = Beta();
  Kind1Core fixture = Kind1OnC05(t);
  fixture.kind1->existence = ExistenceState::kWalkTruncated;
  // A computed object of a kind with neither u nor a support row: the imageless unchecked arm.
  StructureObjectRecord orphan;
  orphan.kind = ObjectKind::kKind2;
  orphan.member = { 3, 5 };
  orphan.existence = ExistenceState::kComputed;  // no u points -> no_delta_image
  fixture.core.objects.push_back(orphan);
  // A kind-1 whose member has no support row at all.
  StructureObjectRecord rowless;
  rowless.kind = ObjectKind::kKind1;
  rowless.member = { 1, 3 };
  rowless.existence = ExistenceState::kComputed;
  fixture.core.objects.push_back(rowless);
  McEvidenceBlock mc = EvidenceOf({ ActualAt({ SkyAt(90.0 * kDeg) }, 1000.0) }, {});
  const CorroborationOutcome outcome = DeriveCorroboration(InputOf(fixture.core, mc, t));
  ASSERT_EQ(outcome.annotations.size(), fixture.core.objects.size());
  EXPECT_EQ(outcome.annotations[0].state, CorroborationState::kUnchecked);
  EXPECT_EQ(outcome.annotations[0].reason, "object_not_computed");
  EXPECT_EQ(outcome.annotations[1].state, CorroborationState::kUnchecked);
  EXPECT_EQ(outcome.annotations[1].reason, "no_delta_image");
  EXPECT_EQ(outcome.annotations[2].state, CorroborationState::kUnchecked);
  EXPECT_EQ(outcome.annotations[2].reason, "support_row_missing");
}

TEST(McAttribution, HMismatchRefusesFailVisible) {
  const Tables t = Beta();
  Kind1Core fixture = Kind1OnC05(t);
  McEvidenceBlock mc = EvidenceOf({ ActualAt({ SkyAt(90.0 * kDeg) }, 1000.0) }, {});
  McAttributionInput input = InputOf(fixture.core, mc, t);
  input.h_rad = 2.0 * kDeg;  // the evidence's declared bandwidth is 1 deg
  const UnattributedOutcome forward = DeriveUnattributed(input);
  EXPECT_FALSE(forward.error.empty());
  EXPECT_TRUE(forward.structures.empty());
  const CorroborationOutcome backward = DeriveCorroboration(input);
  EXPECT_FALSE(backward.error.empty());
  EXPECT_TRUE(backward.annotations.empty());
}

// ---- the declared width (A3) --------------------------------------------------------------------

TEST(McAttribution, DeclaredWidthFollowsTheDominantRule) {
  StructureObjectRecord object;
  object.chromatic_assessed = true;
  // An invisible feature with a huge spread and a higher score must NOT win: the kernel's own
  // dominant rule is visible first, then score.
  analytic::ChromaticFeature invisible_wide;
  invisible_wide.visible = false;
  invisible_wide.spread = 50.0 * kDeg;
  invisible_wide.shift = 10.0 * kDeg;  // score 0.2
  analytic::ChromaticFeature visible_narrow;
  visible_narrow.visible = true;
  visible_narrow.spread = 5.0 * kDeg;
  visible_narrow.shift = 10.0 * kDeg;  // score 2
  object.chromatic.features = { invisible_wide, visible_narrow };
  EXPECT_NEAR(DeclaredWidthOf(object) / kDeg, 5.0, 1e-12);
  // Unassessed chromatic: zero width (the tolerance is then h alone).
  StructureObjectRecord unassessed;
  unassessed.chromatic_assessed = false;
  EXPECT_EQ(DeclaredWidthOf(unassessed), 0.0);
}

TEST(McAttribution, ToleranceUsesTheDeclaredWidthWhenWider) {
  const Tables t = Beta();
  Kind1Core fixture = Kind1OnC05(t);
  const ObjectDeltaImage image =
      ObjectDeltaImageOf(*fixture.kind1, fixture.core, InputOf(fixture.core, McEvidenceBlock{}, t).geometry, nullptr);
  ASSERT_TRUE(image.available);
  fixture.kind1->chromatic_assessed = true;
  analytic::ChromaticFeature wide;
  wide.visible = true;
  wide.spread = 5.0 * kDeg;  // the declared width exceeds h = 1 deg
  fixture.kind1->chromatic.features = { wide };
  // A record 3 deg off the critical value: outside h, inside the declared width -> observed.
  McEvidenceBlock mc = EvidenceOf({ ActualAt({ SkyAt(image.delta_rad.front() + 3.0 * kDeg) }, 1000.0) }, {});
  const CorroborationOutcome outcome = DeriveCorroboration(InputOf(fixture.core, mc, t));
  const size_t kind1_index = static_cast<size_t>(fixture.kind1 - fixture.core.objects.data());
  EXPECT_EQ(outcome.annotations[kind1_index].state, CorroborationState::kObserved);
  EXPECT_NEAR(outcome.annotations[kind1_index].tolerance_rad / kDeg, 5.0, 1e-9);
}

// ---- the delta-image branches -------------------------------------------------------------------

TEST(McAttribution, Kind1ImageIsTheSupportRowsCriticalStructure) {
  const Tables t = Beta();
  Kind1Core fixture = Kind1OnC05(t);
  McAttributionCounts counts;
  const ObjectDeltaImage image =
      ObjectDeltaImageOf(*fixture.kind1, fixture.core, InputOf(fixture.core, McEvidenceBlock{}, t).geometry, &counts);
  ASSERT_TRUE(image.available);
  // The image is the row's critical values (endpoints, onsets, constant circles) — ascending,
  // deduped, and NOT the whole support interval's interior.
  ASSERT_FALSE(image.delta_rad.empty());
  for (size_t i = 1; i < image.delta_rad.size(); i++) {
    EXPECT_GT(image.delta_rad[i], image.delta_rad[i - 1]);
    EXPECT_GE(image.delta_rad[i] - image.delta_rad[i - 1], analytic::kExtremumAtol);
  }
  for (const analytic::DeviationInterval& interval : fixture.row->axis.intervals) {
    // Coverage at the partition's OWN merge constant, not bitwise equality: the image is the
    // row's critical set deduped at kExtremumAtol, and an interval endpoint within that
    // constant of an onset value (this row's lower endpoint sits ~4e-16 from its onset's exact
    // zero) merges into the onset's entry — the row's own "same value" ruler at work.
    bool lower_covered = false;
    bool upper_covered = false;
    for (const double value : image.delta_rad) {
      lower_covered |= std::fabs(value - interval.lower) <= analytic::kExtremumAtol;
      upper_covered |= std::fabs(value - interval.upper) <= analytic::kExtremumAtol;
    }
    EXPECT_TRUE(lower_covered);
    EXPECT_TRUE(upper_covered);
  }
  EXPECT_EQ(counts.dp_evaluations, 0);  // the support-row branch evaluates no chain
}

TEST(McAttribution, UPointImageSamplesThroughTheClosureRouting) {
  const Tables t = Beta();
  EnumerationInput in;
  in.normals = &t.normals;
  in.polygons = &t.polys;
  in.base_index = 1.3110129;
  const Schema3DiscoveryCore core = EnumerateLayer(in, { { 4, 8, 1, 7, 5 } });
  const StructureObjectRecord* kink = FindKind(core.objects, ObjectKind::kKind3);
  ASSERT_NE(kink, nullptr);
  ASSERT_FALSE(kink->u.empty());
  McAttributionCounts counts;
  const ObjectDeltaImage image = ObjectDeltaImageOf(*kink, core, InputOf(core, McEvidenceBlock{}, t).geometry, &counts);
  ASSERT_TRUE(image.available);
  EXPECT_FALSE(image.delta_rad.empty());
  EXPECT_GT(counts.dp_evaluations, 0);
  for (size_t i = 1; i < image.delta_rad.size(); i++) {
    EXPECT_GE(image.delta_rad[i], image.delta_rad[i - 1]);
  }
}

// ---- the presence ESS statistic -----------------------------------------------------------------

TEST(McAttribution, PresenceEssIsTheKernelWindowedKishCount) {
  const double sun[3] = { 0.0, 0.0, 1.0 };
  std::vector<analytic::WeightedSkySample> components;
  for (int i = 0; i < 64; i++) {
    components.push_back(ComponentAt(22.0 * kDeg, static_cast<uint64_t>(i)));
  }
  McAttributionCounts counts;
  // Every component at the image: the Kish count IS the draw count.
  EXPECT_NEAR(PresenceEss(components, { 22.0 * kDeg }, sun, kH, &counts), 64.0, 1e-6);
  // Four draws: four, not the Y-ESS zero a dark region would read.
  EXPECT_NEAR(PresenceEss(std::vector<analytic::WeightedSkySample>(components.begin(), components.begin() + 4),
                          { 22.0 * kDeg }, sun, kH, &counts),
              4.0, 1e-6);
  // Components far outside every image value: no presence contribution.
  EXPECT_LT(PresenceEss(components, { 150.0 * kDeg }, sun, kH, &counts), 1e-6);
  // Degenerate inputs are a zero, not an error: an empty image or an invalid bandwidth claims
  // no standing.
  EXPECT_EQ(PresenceEss(components, {}, sun, kH, &counts), 0.0);
  EXPECT_EQ(PresenceEss(components, { 22.0 * kDeg }, sun, 0.0, &counts), 0.0);
  // A refused call (degenerate inputs) is not a query: only the four real evaluations counted.
  EXPECT_EQ(counts.presence_ess_queries, 3);
}

// ---- the living-shape integration (the C12 restricted object, real producers) --------------------

TEST(McAttribution, RestrictedObjectIntegratesEndToEndOnRealProducers) {
  // The C11 plate (h = 0.3, all face distances 1) at the 20-degree sun: member 1-4-5-2 is
  // family-pinned AND lit on its orbit circle — the enhanced layer's live shape. (The C09 dark
  // member 2-4-5-1 is the zero-stream shape by construction: "no passage" means the producer
  // evaluated no exit direction, so the orbit-image layer has nothing to match against — its
  // corroboration runs on the base delta layer alone.)
  Tables t;
  analytic::CrystalShape shape;
  shape.kind = analytic::CrystalShapeKind::kPrism;
  shape.height = 0.3;
  for (int i = 0; i < 6; i++) {
    shape.face_distance[i] = 1.0;
  }
  ASSERT_EQ(analytic::BuildFaceNormals(shape, &t.normals, &t.polys), analytic::Status::kOk);
  const double alt = 20.0 * kDeg;
  static const UMarginal measure = [] {
    const double a = 20.0 * kDeg;
    const double s[3] = { std::cos(a), 0.0, std::sin(a) };
    AxisDistribution axis;
    axis.azimuth_dist = { DistributionType::kUniform, 0.0f, 360.0f };
    axis.latitude_dist = { DistributionType::kNoRandom, 90.0f, 0.0f };
    axis.roll_dist = { DistributionType::kNoRandom, 0.0f, 0.0f };
    return MakeUMarginal(axis, s);
  }();
  static const analytic::PoseDensitySpec spec = [] {
    analytic::PoseDensitySpec spec;
    spec.family = analytic::PoseFamily::kPlate;
    spec.zenith_mean_deg = 0.0;
    spec.zenith_std_deg = 1.0;
    return spec;
  }();
  EnumerationInput in;
  in.normals = &t.normals;
  in.polygons = &t.polys;
  in.base_index = 1.3110129;
  in.measure = &measure;
  in.density = &spec;
  in.sun_dir[0] = std::cos(alt);
  in.sun_dir[1] = 0.0;
  in.sun_dir[2] = std::sin(alt);
  in.grid = 256;
  const Schema3DiscoveryCore core = EnumerateLayer(in, { { 1, 4, 5, 2 } });
  const StructureObjectRecord* restricted = FindKind(core.objects, ObjectKind::kKind1Restricted);
  ASSERT_NE(restricted, nullptr);
  ASSERT_EQ(core.orbits.size(), 1u);
  // The enhanced layer's live-ability pin: a lit member's stream carries evaluated exits.
  const analytic::OrbitFiberPoint* point = nullptr;
  for (const analytic::OrbitFiberPoint& candidate : core.orbits[0].samples) {
    if (candidate.outgoing[0] != 0.0 || candidate.outgoing[1] != 0.0 || candidate.outgoing[2] != 0.0) {
      point = &candidate;
      break;
    }
  }
  ASSERT_NE(point, nullptr) << "a lit member's stream must carry evaluated exit directions";
  // The MC record sits AT that exit's displayed direction (negative propagation): the enhanced
  // orbit-image layer and the base delta layer both fire.
  McEvidenceBlock mc =
      EvidenceOf({ ActualAt({ { -point->outgoing[0], -point->outgoing[1], -point->outgoing[2] } }, 1000.0) }, {});
  McAttributionInput input = InputOf(core, mc, t);
  input.geometry.base_index = 1.3110129;
  input.geometry.sun_dir[0] = in.sun_dir[0];
  input.geometry.sun_dir[1] = in.sun_dir[1];
  input.geometry.sun_dir[2] = in.sun_dir[2];
  const CorroborationOutcome outcome = DeriveCorroboration(input);
  EXPECT_TRUE(outcome.error.empty());
  const size_t restricted_index = static_cast<size_t>(restricted - core.objects.data());
  ASSERT_GT(outcome.annotations.size(), restricted_index);
  EXPECT_EQ(outcome.annotations[restricted_index].state, CorroborationState::kObserved);
  EXPECT_NEAR(outcome.annotations[restricted_index].match_distance, 0.0, 1e-6);
  EXPECT_FALSE(outcome.annotations[restricted_index].ruler.empty());
  EXPECT_GT(outcome.counts.dp_evaluations, 0);  // the restricted image went through the routing
  // The forward side of the same evidence: nothing unattributed (the record was attributed).
  const UnattributedOutcome forward = DeriveUnattributed(input);
  EXPECT_TRUE(forward.structures.empty());
}

// ---- the living cases: real v2 evidence over real producers (Step 4) -----------------------------

namespace {

// The Haar prism scene (the v2 report tests' shape): crystal 1 (h = 1, all face distances 1),
// sun 20 deg, full-sphere Haar axis. The spectrum request shape is the CALLER's: A runs the
// explicit single-wavelength form (cheap rows, the peak clears the ESS floor); B runs the
// continuous illuminant with the auto budget — the budget allocator spreads the auto count
// over member x spectral-row expansion, so a full-band quadrature measure gets only a few
// hundred outer draws: the weighted field starves (Y-ESS ~0, actual = 0) deterministically.
// That premise is B's subject; if the budget allocator changes, re-measure it first.
ConfigManager HaarPrismScene(bool continuous_spectrum) {
  PrismCrystalParam prism;
  prism.h_ = { DistributionType::kNoRandom, 1, 0 };
  for (auto& distance : prism.d_) {
    distance = { DistributionType::kNoRandom, 1, 0 };
  }
  CrystalConfig crystal{};
  crystal.id_ = 1;
  crystal.param_ = prism;
  crystal.axis_.latitude_dist = { DistributionType::kUniform, 90, 360 };
  crystal.axis_.azimuth_dist = crystal.axis_.roll_dist = { DistributionType::kUniform, 0, 360 };
  ConfigManager config;
  config.crystals_.emplace(1, crystal);
  config.scene_.light_source_.param_ = { 20, 0, 0 };
  config.scene_.light_source_.spectrum_ =
      continuous_spectrum ? SpectrumConfig(IlluminantType::kD65) : SpectrumConfig(std::vector<WlParam>{ { 550, 1 } });
  ScatteringSetting entry{};
  entry.crystal_ = crystal;
  entry.crystal_proportion_ = 1;
  config.scene_.ms_.push_back({ 0, { entry } });
  return config;
}

// The plate scene (the C12 crystal's shape: h = 1, fd = [1.5, 1, 1, 1.5, 1, 1]), plate axis,
// sun 9 deg — the C-arm's configuration.
ConfigManager PlateScene() {
  PrismCrystalParam prism;
  prism.h_ = { DistributionType::kNoRandom, 1, 0 };
  const double fd[6] = { 1.5, 1, 1, 1.5, 1, 1 };
  for (int i = 0; i < 6; i++) {
    prism.d_[i] = { DistributionType::kNoRandom, static_cast<float>(fd[i]), 0 };
  }
  CrystalConfig crystal{};
  crystal.id_ = 1;
  crystal.param_ = prism;
  // The plate family: azimuth uniform, latitude and roll locked.
  crystal.axis_.latitude_dist = { DistributionType::kNoRandom, 90, 0 };
  crystal.axis_.azimuth_dist = { DistributionType::kUniform, 0, 360 };
  crystal.axis_.roll_dist = { DistributionType::kNoRandom, 0, 0 };
  ConfigManager config;
  config.crystals_.emplace(1, crystal);
  config.scene_.light_source_.param_ = { 9, 0, 0 };
  config.scene_.light_source_.spectrum_ = SpectrumConfig(std::vector<WlParam>{ { 550, 1 } });
  ScatteringSetting entry{};
  entry.crystal_ = crystal;
  entry.crystal_proportion_ = 1;
  config.scene_.ms_.push_back({ 0, { entry } });
  return config;
}

// One living run: the real v2 report, the schema3 enumeration over the SAME crystal tables and
// base index, the sun at the scene's solar center, and the evidence block moved together.
struct LivingRun {
  PathFeatureReport report;
  Schema3DiscoveryCore core;
  McAttributionInput input;
};

LivingRun RunLiving(const ConfigManager& config, const PathFeatureReportRequest& request,
                    const std::vector<std::vector<int>>& members, const UMarginal* measure,
                    const analytic::PoseDensitySpec* density, int grid) {
  LivingRun run;
  const Error error = AssemblePathFeatureReport(config, request, &run.report);
  if (!error.Ok()) {
    EXPECT_TRUE(error.Ok()) << error.message;
    return run;  // ASSERT semantics are unavailable in a value-returning helper
  }
  EnumerationInput in;
  in.normals = &run.report.representative_input.layers[0].normals;
  in.polygons = &run.report.representative_input.layers[0].polygons;
  if (run.report.representative_input.spectrum.rows.empty()) {
    ADD_FAILURE() << "the assembled input carries no spectral rows";
    return run;
  }
  in.base_index = run.report.representative_input.spectrum.rows[0].refractive_index;
  // The scene's solar center, AT the sun: SunIncidentDirection names the propagation
  // (sun -> crystal), so the at-sun direction is its negation.
  double sun_hat[3];
  SunIncidentDirection(config.scene_.light_source_.param_, sun_hat);
  for (int i = 0; i < 3; i++) {
    in.sun_dir[i] = -sun_hat[i];
  }
  in.measure = measure;
  in.density = density;
  in.grid = grid;
  run.core = EnumerateLayer(in, members);
  run.input.core = &run.core;
  run.input.mc = nullptr;  // set after the move, below
  run.input.geometry.normals = in.normals;
  run.input.geometry.polygons = in.polygons;
  run.input.geometry.base_index = in.base_index;
  for (int i = 0; i < 3; i++) {
    run.input.geometry.sun_dir[i] = in.sun_dir[i];
  }
  run.input.h_rad = request.bandwidth_rad;
  return run;
}

}  // namespace

TEST(McAttribution, LivingExplicitEventsAttributeTheRealPeak) {
  // A (the two-way living pair, forward and backward): Haar prism, 550 nm, 16384 explicit
  // events. The MC finds the minimum-deviation peak (kActual), the forward pass attributes it
  // to the kind-1 critical delta, and the same object derives `observed`. The explicit events
  // are the premise: at the auto budget this measure starves (see the B arm below).
  ConfigManager config = HaarPrismScene(false);
  PathFeatureReportRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 3, 5 } };
  request.sample_count = 16384;
  request.wavelengths_nm = { 550 };
  // The pipeline is replayable (fixed seed) but bounded by wall-clock; the max budget keeps
  // the deadline from ever biting, so the living case stays byte-reproducible across machines.
  request.budget_ms = 120000;
  LivingRun run = RunLiving(config, request, { { 3, 5 } }, nullptr, nullptr, 0);
  ASSERT_FALSE(run.report.discovery.features.empty());
  size_t actuals = 0;
  for (const DiagnosticFeatureRecord& record : run.report.discovery.features) {
    actuals += record.evidence == DiagnosticEvidence::kActual ? 1 : 0;
  }
  // The premise (the explicit-events rescue of the starved auto budget): the peak clears the
  // significance bar.
  ASSERT_GT(actuals, 0u) << "explicit events must surface at least one kActual on the 3-5 control cell";
  schema3::McEvidenceBlock* mc =
      new schema3::McEvidenceBlock(schema3::McEvidenceOf(std::move(run.report.discovery), run.report.options));
  run.input.mc = mc;
  const CorroborationOutcome outcome = DeriveCorroboration(run.input);
  EXPECT_TRUE(outcome.error.empty()) << outcome.error;
  const UnattributedOutcome forward = DeriveUnattributed(run.input);
  EXPECT_TRUE(forward.error.empty()) << forward.error;
  // The kind-1 object of 3-5: the MC peak sits at its minimum-deviation endpoint -> observed,
  // and that record is not in the unattributed list.
  size_t kind1_index = kNoIndex;
  for (size_t i = 0; i < run.core.objects.size(); i++) {
    if (run.core.objects[i].kind == ObjectKind::kKind1) {
      kind1_index = i;
    }
  }
  ASSERT_NE(kind1_index, kNoIndex);
  EXPECT_EQ(outcome.annotations[kind1_index].state, CorroborationState::kObserved)
      << outcome.annotations[kind1_index].reason;
  const long long matched = outcome.annotations[kind1_index].matched_record;
  ASSERT_GE(matched, 0);
  for (const UnattributedStructure& structure : forward.structures) {
    EXPECT_NE(static_cast<long long>(structure.record_index), matched)
        << "the record the kind-1 observed must not also be unattributed";
  }
  delete mc;
}

TEST(McAttribution, LivingStarvedAutoBudgetStaysSilent) {
  // B (the AC2 real shape): the SAME scene, continuous illuminant, auto budget. The auto
  // budget's starvation is deterministic (a few hundred outer draws over the full-band
  // quadrature, Y-ESS ~0, actual = 0) — and the hunger is in the WEIGHTED field, not in the
  // sampling: the record still carries plenty of light-bearing rows in the object's delta
  // window (measured 15.5k row-level before the draw grouping). So the AC2 content lands on
  // the forward side — no kActual, no
  // unattributed output at all — while the kind-1 (unproven: no measure side on this core)
  // follows the judgment table's unproven arm: nothing to contradict at sufficient recorded
  // presence -> consistent. (The plan's B narrative expected not_observed_insufficient_ess;
  // the measured premise refines which arm the table yields — the table stands, the case is
  // written to what the producers actually do.)
  ConfigManager config = HaarPrismScene(true);
  PathFeatureReportRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 3, 5 } };
  request.sample_count = 0;    // auto
  request.budget_ms = 120000;  // the evaluations cap, not the clock, must end this run
  LivingRun run = RunLiving(config, request, { { 3, 5 } }, nullptr, nullptr, 0);
  size_t actuals = 0;
  for (const DiagnosticFeatureRecord& record : run.report.discovery.features) {
    actuals += record.evidence == DiagnosticEvidence::kActual ? 1 : 0;
  }
  ASSERT_EQ(actuals, 0u) << "the auto x quadrature starvation must hold: it is this case's measured premise";
  schema3::McEvidenceBlock* mc =
      new schema3::McEvidenceBlock(schema3::McEvidenceOf(std::move(run.report.discovery), run.report.options));
  run.input.mc = mc;
  const UnattributedOutcome forward = DeriveUnattributed(run.input);
  EXPECT_TRUE(forward.error.empty());
  EXPECT_TRUE(forward.structures.empty());
  EXPECT_EQ(forward.skipped_below_ess, 0);  // nothing reached the gate at all
  const CorroborationOutcome outcome = DeriveCorroboration(run.input);
  EXPECT_TRUE(outcome.error.empty());
  for (size_t i = 0; i < run.core.objects.size(); i++) {
    if (run.core.objects[i].kind != ObjectKind::kKind1) {
      continue;
    }
    EXPECT_EQ(run.core.objects[i].visibility.state, VisibilityState::kUnproven);
    EXPECT_EQ(outcome.annotations[i].state, CorroborationState::kConsistent) << outcome.annotations[i].reason;
    EXPECT_GE(outcome.annotations[i].presence_ess, 32.0);
    EXPECT_NE(outcome.annotations[i].reason.find("nothing the MC could contradict"), std::string::npos);
  }
  delete mc;
}

TEST(McAttribution, LivingUnlitRestrictedReadsInsufficientOnZeroRecordedPresence) {
  // C: the C12 plate's fully-dark restricted member (the C09 shape), real MC at standing.
  // MEASURED SHAPE (differs from the plan's C narrative, which expected `consistent`): the
  // chain never transmits into the object's delta window, and the v2 measure records
  // light-bearing rows only — so the window's recorded presence is exactly zero and the
  // unlit arm lands in not_observed_insufficient_ess (the fail-closed reading: the absence
  // claim is corroborated only to the recorded standing, and there is none). The
  // consistent-on-unlit arm with real light near the window stays pinned synthetically; a
  // living config with light beside a dark curve is a registered follow-up, not a silent
  // downgrade. (A4's point survives the refinement: the backward standing is not the record's
  // own Y-ESS — that is ~0 for a dark region BY CONSTRUCTION and would kill the arm regardless
  // of what the MC recorded.)
  ConfigManager config = PlateScene();
  PathFeatureReportRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 2, 4, 5, 1 } };
  request.sample_count = 65536;
  request.wavelengths_nm = { 550 };
  request.budget_ms = 120000;  // same replayability guard as the A arm
  // The enumeration's measure side: the plate UMarginal over the scene's own sun.
  const double alt = 9.0 * kDeg;
  const double s[3] = { std::cos(alt), 0.0, std::sin(alt) };
  static const UMarginal plate = [s] {
    AxisDistribution axis;
    axis.azimuth_dist = { DistributionType::kUniform, 0.0f, 360.0f };
    axis.latitude_dist = { DistributionType::kNoRandom, 90.0f, 0.0f };
    axis.roll_dist = { DistributionType::kNoRandom, 0.0f, 0.0f };
    return MakeUMarginal(axis, s);
  }();
  static const analytic::PoseDensitySpec spec = [] {
    analytic::PoseDensitySpec spec;
    spec.family = analytic::PoseFamily::kPlate;
    spec.zenith_mean_deg = 0.0;
    spec.zenith_std_deg = 1.0;
    return spec;
  }();
  LivingRun run = RunLiving(config, request, { { 2, 4, 5, 1 } }, &plate, &spec, 256);
  const StructureObjectRecord* restricted = nullptr;
  size_t restricted_index = kNoIndex;
  for (size_t i = 0; i < run.core.objects.size(); i++) {
    if (run.core.objects[i].kind == ObjectKind::kKind1Restricted) {
      restricted = &run.core.objects[i];
      restricted_index = i;
    }
  }
  ASSERT_NE(restricted, nullptr) << "2-4-5-1 must be family-pinned on the plate";
  ASSERT_EQ(restricted->visibility.state, VisibilityState::kUnlit)
      << "the C09 premise: the pinned member is fully dark on its declared orbit";
  schema3::McEvidenceBlock* mc =
      new schema3::McEvidenceBlock(schema3::McEvidenceOf(std::move(run.report.discovery), run.report.options));
  run.input.mc = mc;
  const CorroborationOutcome outcome = DeriveCorroboration(run.input);
  EXPECT_TRUE(outcome.error.empty()) << outcome.error;
  EXPECT_EQ(outcome.annotations[restricted_index].state, CorroborationState::kNotObservedInsufficientEss)
      << outcome.annotations[restricted_index].reason;
  // The weight information survives: the annotation carries the standing the arm leaned on
  // (zero — the recorded observation has no rows in this window at all).
  EXPECT_DOUBLE_EQ(outcome.annotations[restricted_index].presence_ess, 0.0);
  EXPECT_NE(outcome.annotations[restricted_index].reason.find("leaves the corroboration itself undeclared"),
            std::string::npos);
  delete mc;
}

}  // namespace
}  // namespace lumice::raypath::schema3