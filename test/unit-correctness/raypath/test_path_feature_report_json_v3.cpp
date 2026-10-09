// The schema3 serializer (src/raypath/detail/path_feature_report_json.cpp
// PathFeatureReportV3ToJson, 666.3 Step 2): the v3 document's mechanical faces, pinned against
// the authorities they must not fork from —
//   AC2a  the migration ledger's top-level v3 homes reconcile the emitted key set, both ways
//         (a new top-level key must claim a ledger row or join this file's declared new-key
//         set, or the emission is unaccounted for);
//   AC2b  the ledger's 34 record-field rows are the serialization checklist: a maximal record
//         (every field populated) must surface each row's key;
//   D2    the three buckets are a derived view: segment(object) == BucketOf(object) per object;
//   D3/D4 the outcome grammar, the unconditional ruling block, the non-finite spellings;
//   AC1   the self-description (conventions, the vocabulary coverage row) and the narrowed
//         observation.
// The document is produced through the real assembly (AssembleSchema3Report) over a real v2
// report on the standing Haar-prism scene.

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <vector>

#include "raypath/detail/path_feature_report.hpp"
#include "raypath/detail/path_feature_report_json.hpp"
#include "raypath/detail/schema3/mc_evidence.hpp"
#include "raypath/detail/schema3/structure_object.hpp"

namespace lumice::raypath {
namespace {

ConfigManager ReportScene() {
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
  config.scene_.light_source_.spectrum_ = SpectrumConfig(std::vector<WlParam>{ { 550, 1 } });
  ScatteringSetting entry{};
  entry.crystal_ = crystal;
  entry.crystal_proportion_ = 1;
  config.scene_.ms_.push_back({ 0, { entry } });
  return config;
}

struct V3Run {
  PathFeatureReport report;
  schema3::AssembledSchema3Report assembled;
  nlohmann::json document;
};

// One real report through the real assembly and the v3 serializer. The caller mutates
// `run.report` before assembling when it needs injected content.
V3Run RunV3OrDie(const ConfigManager& config, const PathFeatureReportRequest& request) {
  V3Run run;
  const Error error = AssemblePathFeatureReport(config, request, &run.report);
  EXPECT_TRUE(error.Ok()) << error.message;
  if (!error.Ok()) {
    return run;
  }
  run.assembled = schema3::AssembleSchema3Report(std::move(run.report), request.max_optical_evaluations);
  run.document = nlohmann::json::parse(PathFeatureReportV3ToJson(run.report, run.assembled, "test"));
  return run;
}

std::array<double, 9> IdentityPose() {
  return { 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };
}

// A record with every DiagnosticFeatureRecord field populated (the same sentinel shape the
// carry's round-trip test uses). `source_token` must name an existing entry of the report's own
// measure — the serializer re-derives the sources projection from it.
DiagnosticFeatureRecord MaximalRecord(size_t valid_source_token) {
  DiagnosticFeatureRecord in;
  in.evidence = DiagnosticEvidence::kActual;
  in.geometry = DiagnosticGeometry::kAtom;  // fires the XYZ_mass emission gate too
  in.kind = "sentinel_kind";
  in.reason = "sentinel_reason";
  in.sky_points = { { 1.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 } };
  analytic::FieldStationaryPoint point;
  point.status = analytic::FieldSolveStatus::kConverged;
  point.query.direction = { 0.0, 0.0, 1.0 };
  point.query.bandwidth_rad = 0.75;
  point.correction_rad = 0.125;
  point.field.effective_samples_y = 99.5;
  in.field_points = { point };
  in.band = analytic::FieldBand{};
  in.band->levels = { 0.3, 0.7 };
  in.band->status = analytic::FieldSolveStatus::kConverged;
  in.band->boundaries[0].push_back(point);
  in.equation = analytic::FieldEquation::kChromaticityX;
  in.level = 0.4;
  in.bandwidth_rad = 0.75;
  in.prefix_movement_rad = 0.01;
  in.replicate_movement_rad = 0.02;
  in.scale_movement_rad = 0.03;
  in.scale_status = analytic::FieldSolveStatus::kDegenerate;
  in.minimum_effective_samples = 1234.5;
  in.transverse_contrast = 0.9;
  in.walk_stop = analytic::FieldWalkStop::kObservationCensored;
  in.source_token = valid_source_token;
  in.contributor_fraction_of_estimated_y = 0.55;
  in.internal_slot = 3;
  in.source_parameters = { { "latent_a", 0.25 }, { "latent_b", 0.75 } };
  in.boundary_source = analytic::DiagnosticInputRow{};
  in.boundary_source->pose = IdentityPose();
  in.boundary_source->incident = { 0.0, 0.0, -1.0 };
  in.boundary_source->refractive_index = 1.3110129;
  in.boundary_source->source_token = valid_source_token;
  in.boundary_value = analytic::DiagnosticOutputRow{};
  in.boundary_value->path_valid = true;
  in.boundary_value->outgoing = { 0.6, 0.0, 0.8 };
  in.boundary_value->path_evaluations = 5;
  in.boundary_value->interface_product = 0.8;
  in.interface_event = analytic::InterfaceStationaryPoint{};
  in.interface_event->travelled_rad = 0.4;
  in.interface_event->accepted_poses = { IdentityPose() };
  in.paired_interface = PairedInterfaceEvidence{};
  in.paired_interface->actual_xyz = { 0.1, 0.2, 0.3 };
  in.paired_interface->without_slot_xyz = { 0.15, 0.25, 0.35 };
  in.paired_interface->xy_difference = { 0.01, 0.02 };
  in.paired_interface->chromaticity_available = true;
  in.paired_interface->complete = true;
  in.interface_curves = { analytic::InterfaceCurve{} };
  in.interface_curves[0].stop = analytic::InterfaceWalkStop::kOpticalGate;
  in.source_event = analytic::InterfaceEventBracket{};
  in.source_event->source_width_rad = 0.2;
  in.source_connected_feature = size_t{ 9 };
  in.deviation_minimum = analytic::DeviationStationaryPoint{};
  in.deviation_minimum->deviation_available = true;
  in.deviation_minimum->deviation_rad = 0.3852;
  in.deviation_minimum->correction_rad = 0.001;
  in.orbit_axis = std::array<double, 3>{ 0.0, 0.0, 1.0 };
  in.orbit_begin_rad = 0.5;
  in.orbit_end_rad = 6.0;
  in.observation_contrast_error = 0.04;
  in.atom_xyz_mass = { 0.7, 0.8, 0.9 };
  return in;
}

PathFeatureReportRequest SmallRequest() {
  PathFeatureReportRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 3, 5 } };
  request.sample_count = 64;
  request.wavelengths_nm = { 550 };
  request.budget_ms = 120000;
  return request;
}

}  // namespace

TEST(PathFeatureReportJsonV3, LedgerTopLevelHomesReconcileTheEmission) {
  // AC2a, both ways. Every top-level ledger row's v3 home (its FIRST path segment) must exist
  // in the emitted document; every emitted top-level key must be claimed by a ledger home or
  // belong to the v3-new blocks the migration never carried (declared here — a new top-level
  // key that joins neither set is an unaccounted emission and goes red).
  V3Run run = RunV3OrDie(ReportScene(), SmallRequest());
  std::set<std::string> emitted;
  for (auto it = run.document.begin(); it != run.document.end(); ++it) {
    emitted.insert(it.key());
  }
  std::set<std::string> homes;
  for (const schema3::McMigrationRow& row : schema3::kMcMigrationLedger) {
    if (row.section != schema3::MigrationSection::kTopLevel) {
      continue;
    }
    const std::string home = row.v3_home;
    const size_t stop = home.find_first_of(".[");
    EXPECT_NE(emitted.find(home.substr(0, stop)), emitted.end())
        << "ledger row " << row.v2_key << " names home " << home << " but the document does not emit it";
    homes.insert(home.substr(0, stop));
  }
  // The v3-new top-level keys: blocks the schema3 shape adds (no v2 key migrated into them).
  const std::set<std::string> v3_new = { "support",     "features",           "unattributed_structures",
                                         "observation", "no_related_feature", "basis" };
  // The one claimed-but-not-first-segment key: the sources row's home names the model path
  // ("...+ discovery.measure.sources") the emission re-derives the top-level `sources` view
  // from, so no ledger home's first segment spells the key itself.
  const std::set<std::string> ledger_claimed = { "sources" };
  for (const std::string& key : emitted) {
    EXPECT_TRUE(homes.count(key) > 0 || v3_new.count(key) > 0 || ledger_claimed.count(key) > 0)
        << "emitted top-level key " << key << " is claimed by no ledger row and no declared v3-new block";
  }
  EXPECT_EQ(run.document["schema"], "lumice.path-feature-report");
  EXPECT_EQ(run.document["schema_version"], 3);
}

TEST(PathFeatureReportJsonV3, RecordFieldChecklistRidesTheLedger) {
  // AC2b: the 34 record-field rows are the per-field serialization checklist. A maximal record
  // (every field populated, source_token naming a real measure entry) must surface every row's
  // key in mc_evidence.records[0]; a row whose key never appears is a field the serializer
  // dropped.
  PathFeatureReportRequest request = SmallRequest();
  request.sample_count = 64;
  PathFeatureReport report;
  ASSERT_TRUE(AssemblePathFeatureReport(ReportScene(), request, &report).Ok());
  ASSERT_FALSE(report.discovery.measure.sources.empty()) << "the scene must carry at least one witness source";
  report.discovery.features = { MaximalRecord(report.discovery.measure.sources.size() - 1) };
  schema3::AssembledSchema3Report assembled =
      schema3::AssembleSchema3Report(std::move(report), request.max_optical_evaluations);
  const nlohmann::json document = nlohmann::json::parse(PathFeatureReportV3ToJson(report, assembled, "test"));
  const nlohmann::json& record = document["mc_evidence"]["records"][0];
  EXPECT_EQ(record["id"], 0);
  EXPECT_EQ(record["kind"], "sentinel_kind");
  for (const schema3::McMigrationRow& row : schema3::kMcMigrationLedger) {
    if (row.section != schema3::MigrationSection::kRecordField) {
      continue;
    }
    const std::string home = row.v3_home;
    const std::string prefix = "mc_evidence.records[].";
    if (home.find(prefix) != 0u) {
      ADD_FAILURE() << "record-field row " << row.v2_key << " names home " << home;
      continue;
    }
    // The ledger's record-field homes are LOGICAL (the record carries the field); the record's
    // internal JSON face is the v2 emission shape, frozen and owned once by
    // FeatureRecordJson — the v3 record and the v2 record buckets share it. This map is the
    // declared translation from a row's field name to that face's JSON pointer; a row not
    // listed here must surface under its own name at the record's top level, so a new field
    // whose emission nests or renames goes red until its line is added consciously.
    static const std::map<std::string, std::string> emission = {
      { "geometry", "/geometry" },
      { "sky_points", "/geometry/sky_points" },
      { "equation", "/fixed_observation/equation" },
      { "level", "/fixed_observation/level" },
      { "bandwidth_rad", "/fixed_observation/bandwidth_rad" },
      { "prefix_movement_rad", "/fixed_observation/prefix_movement_rad" },
      { "replicate_movement_rad", "/fixed_observation/replicate_movement_rad" },
      { "minimum_effective_samples", "/fixed_observation/minimum_ESS" },
      { "transverse_contrast", "/fixed_observation/contrast" },
      { "observation_contrast_error", "/fixed_observation/contrast_prefix_error" },
      { "scale_movement_rad", "/scale_response/movement_rad" },
      { "scale_status", "/scale_response/status" },
      { "source_parameters", "/declared_source_event/coordinates" },
      { "boundary_source", "/declared_source_event/source" },
      { "boundary_value", "/declared_source_event/value" },
      { "interface_curves", "/source_curves" },
      { "deviation_minimum", "/physical_position" },
      { "orbit_axis", "/source_orbit/world_axis" },
      { "orbit_begin_rad", "/source_orbit/begin_rad" },
      { "orbit_end_rad", "/source_orbit/end_rad" },
      { "contributor_fraction_of_estimated_y", "/contributor_fraction_of_estimated_Y" },
      { "atom_xyz_mass", "/XYZ_mass" },
    };
    const std::string key = home.substr(prefix.size());
    const auto found = emission.find(key);
    const nlohmann::json::json_pointer pointer(found != emission.end() ? found->second : "/" + key);
    EXPECT_TRUE(record.contains(pointer)) << "ledger field row " << row.v2_key << " (v3 home " << home << " -> "
                                          << pointer.to_string() << ") is missing from the serialized maximal record";
  }
}

TEST(PathFeatureReportJsonV3, SegmentsMatchTheDerivedBucketsPerObject) {
  // D2: the keyed segments are a DERIVED view — segment(object) == BucketOf(object) for every
  // object, and the three id sets partition the records.
  V3Run run = RunV3OrDie(ReportScene(), SmallRequest());
  std::vector<int> segment_of(run.assembled.core.objects.size(), -1);
  const std::pair<const char*, schema3::FeatureBucket> segments[3] = {
    { "actual", schema3::FeatureBucket::kActual },
    { "candidate", schema3::FeatureBucket::kCandidate },
    { "unfinished", schema3::FeatureBucket::kUnfinished },
  };
  for (const auto& [name, bucket] : segments) {
    for (const nlohmann::json& object : run.document["features"][name]) {
      const int id = object["id"];
      if (id < 0 || static_cast<size_t>(id) >= segment_of.size()) {
        ADD_FAILURE() << "feature id " << id << " is out of the record range";
        continue;
      }
      EXPECT_EQ(segment_of[static_cast<size_t>(id)], -1) << "object " << id << " appeared in two segments";
      segment_of[static_cast<size_t>(id)] = static_cast<int>(bucket);
    }
  }
  bool any_actual = false;
  for (size_t i = 0; i < run.assembled.core.objects.size(); i++) {
    EXPECT_NE(segment_of[i], -1) << "object " << i << " is in no segment";
    EXPECT_EQ(segment_of[i], static_cast<int>(schema3::BucketOf(run.assembled.core.objects[i])))
        << "object " << i << " sits in the wrong segment";
    any_actual |= segment_of[i] == static_cast<int>(schema3::FeatureBucket::kActual);
  }
  EXPECT_FALSE(run.assembled.core.objects.empty());
  (void)any_actual;
}

TEST(PathFeatureReportJsonV3, SelfDescriptionCoversTheSchema3Semantics) {
  V3Run run = RunV3OrDie(ReportScene(), SmallRequest());
  const nlohmann::json& conventions = run.document["conventions"];
  for (const char* key : { "sky", "pose", "angles", "field", "weight", "uncertainty", "walk_s", "null",
                           "min_margin_rad", "features_buckets", "chromatic_thresholds" }) {
    EXPECT_TRUE(conventions.contains(key)) << "conventions must declare " << key;
  }
  // The vocabulary coverage row: every produced kind, every declared kind with its reason.
  const nlohmann::json* vocabulary = nullptr;
  for (const nlohmann::json& row : run.document["coverage"]) {
    if (row["subject"] == "object-kind vocabulary") {
      vocabulary = &row;
    }
  }
  ASSERT_NE(vocabulary, nullptr) << "coverage must carry the object-kind vocabulary row";
  const nlohmann::json& vocabulary_row = *vocabulary;
  ASSERT_EQ(vocabulary_row["produced"].size(), 5u);
  ASSERT_EQ(vocabulary_row["declared_not_produced"].size(), 4u);
  std::set<std::string> declared_kinds;
  for (const nlohmann::json& entry : vocabulary_row["declared_not_produced"]) {
    EXPECT_FALSE(std::string(entry["reason"]).empty()) << "a declared-not-produced kind must name its reason";
    declared_kinds.insert(std::string(entry["kind"]));
  }
  std::set<std::string> registered;
  for (schema3::ObjectKind kind : schema3::RegisteredObjectKinds()) {
    registered.insert(schema3::ObjectKindName(kind));
  }
  for (const nlohmann::json& name : vocabulary_row["produced"]) {
    EXPECT_NE(registered.count(std::string(name)), 0u) << "produced kind " << name << " is not a registered kind";
    EXPECT_EQ(declared_kinds.count(std::string(name)), 0u) << "kind " << name << " is both produced and declared";
  }
  // The narrowed observation declares its role and points at the ruler's single home.
  EXPECT_EQ(run.document["observation"]["subject"], "mc_corroboration_observation");
  EXPECT_EQ(run.document["observation"]["ruler"], "mc_evidence.observation_options");
  EXPECT_EQ(run.document["mc_evidence"]["observation_scope_note"], schema3::kMcObservationScopeNote);
  EXPECT_DOUBLE_EQ(run.document["mc_evidence"]["observation_options"]["bandwidth_rad"].get<double>(),
                   run.report.options.bandwidth_rad);
}

TEST(PathFeatureReportJsonV3, OutcomeGrammarAndUnconditionalRuling) {
  // D3: the ruling block is ALWAYS present with its full condition face; the outcome reads
  // no_related_feature exactly when issued, with the basis beside it; the completed/partial
  // arms follow the v2 criteria.
  PathFeatureReportRequest request = SmallRequest();
  V3Run run = RunV3OrDie(ReportScene(), request);
  const nlohmann::json& ruling = run.document["no_related_feature"];
  for (const char* key :
       { "issued", "partition_complete_no_escape", "partition_failure", "all_objects_unlit_or_none", "unlit_failure",
         "no_sufficient_ess_unattributed", "unattributed_failure", "s4_scope_declared", "s4_failure",
         "two_d_valid_support", "s4_scope_note", "absence_not_proven_note" }) {
    EXPECT_TRUE(ruling.contains(key)) << "the ruling block must always carry " << key;
  }
  EXPECT_FALSE(ruling["issued"]) << "the witnessed scene cannot certify an absence";
  EXPECT_EQ(run.document["outcome"], "completed") << "the small bounded run is expected to complete cleanly";
  EXPECT_FALSE(run.document.contains("basis")) << "no basis key may exist when the ruling did not issue";

  // The issued shape, driven directly on the assembled model: outcome flips, the basis names
  // the rule that issued, and the outcome value outranks the partial arm.
  run.assembled.ruling.issued = true;
  run.assembled.ruling.basis = schema3::NoRelatedBasis::kCertifiedSmoothRadiance;
  const nlohmann::json issued = nlohmann::json::parse(PathFeatureReportV3ToJson(run.report, run.assembled, "test"));
  EXPECT_EQ(issued["outcome"], "no_related_feature");
  EXPECT_EQ(issued["basis"], "certified_smooth_radiance");
  EXPECT_EQ(issued["no_related_feature"]["basis"], "certified_smooth_radiance");
  run.assembled.ruling.basis = schema3::NoRelatedBasis::kZeroSpectralSignal;
  const nlohmann::json zero = nlohmann::json::parse(PathFeatureReportV3ToJson(run.report, run.assembled, "test"));
  EXPECT_EQ(zero["basis"], "zero_spectral_signal");
}

TEST(PathFeatureReportJsonV3, NonFiniteSpellingsFollowTheDeclaredConventions) {
  // D4: walk_s NaN -> null, walk_s 0.0 stays 0.0; min_margin_rad inf -> null (the empty-object
  // minimum), a finite margin stays a number.
  PathFeatureReportRequest request = SmallRequest();
  V3Run run = RunV3OrDie(ReportScene(), request);
  bool saw_null_walk = false;
  for (const char* segment : { "actual", "candidate", "unfinished" }) {
    for (const nlohmann::json& object : run.document["features"][segment]) {
      if (object["existence"]["walk_s"].is_null()) {
        saw_null_walk = true;
      }
    }
  }
  EXPECT_TRUE(saw_null_walk) << "a closed walk's arclength does not apply: the null spelling";
  // The 0.0 spelling, driven on the model: a truncated-before-the-record walk is a MEASURED
  // zero? No — it is the declared unknown, spelled 0.0 in the JSON (the convention's second
  // form) and must not come out as null.
  run.assembled.core.objects.front().walk_s = 0.0;
  const nlohmann::json zeroed = nlohmann::json::parse(PathFeatureReportV3ToJson(run.report, run.assembled, "test"));
  bool saw_zero = false;
  for (const char* segment : { "actual", "candidate", "unfinished" }) {
    for (const nlohmann::json& object : zeroed["features"][segment]) {
      if (object["id"] == 0) {
        EXPECT_EQ(object["existence"]["walk_s"], 0.0) << "the truncated-before-record spelling is 0.0, not null";
        saw_zero = true;
      }
    }
  }
  EXPECT_TRUE(saw_zero);

  // The infinite margin: a finding with no object image to bound it.
  schema3::UnattributedStructure finding{};
  finding.record_index = 0;
  finding.position = { 1.0, 0.0, 0.0 };
  finding.delta_rad = 0.5;
  finding.record_ess = 100.0;
  finding.min_margin_rad = std::numeric_limits<double>::infinity();
  finding.ruler = "test ruler";
  run.assembled.unattributed.structures.push_back(finding);
  const nlohmann::json infinite = nlohmann::json::parse(PathFeatureReportV3ToJson(run.report, run.assembled, "test"));
  EXPECT_TRUE(infinite["unattributed_structures"]["structures"][0]["min_margin_rad"].is_null());
  run.assembled.unattributed.structures.back().min_margin_rad = 0.25;
  const nlohmann::json finite = nlohmann::json::parse(PathFeatureReportV3ToJson(run.report, run.assembled, "test"));
  EXPECT_DOUBLE_EQ(finite["unattributed_structures"]["structures"][0]["min_margin_rad"].get<double>(), 0.25);
}

TEST(PathFeatureReportJsonV3, UnsupportedMulticrystalTakesTheV3EarlyShape) {
  ConfigManager config = ReportScene();
  PathFeatureReportRequest request = SmallRequest();
  request.path_layers = { { 3, 5 }, { 1, 3 } };
  PathFeatureReport report;
  ASSERT_TRUE(AssemblePathFeatureReport(config, request, &report).Ok());
  ASSERT_TRUE(report.unsupported_multicrystal);
  schema3::AssembledSchema3Report empty{};
  const nlohmann::json document = nlohmann::json::parse(PathFeatureReportV3ToJson(report, empty, "test"));
  EXPECT_EQ(document["outcome"], "unsupported_multicrystal");
  EXPECT_EQ(document["requested_path_layers"], nlohmann::json({ { 3, 5 }, { 1, 3 } }));
  EXPECT_TRUE(document["features"]["actual"].empty());
  EXPECT_TRUE(document["features"]["candidate"].empty());
  EXPECT_TRUE(document["features"]["unfinished"].empty());
  EXPECT_TRUE(document["mc_evidence"]["records"].empty());
  EXPECT_FALSE(document["mc_evidence"]["observation_scope_note"].empty());
  EXPECT_EQ(document["budgets"]["optical_evaluations"], 0);
  EXPECT_EQ(document["coverage"][0]["status"], "not_supported");
}

}  // namespace lumice::raypath
