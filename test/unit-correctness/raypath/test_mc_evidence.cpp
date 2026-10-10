// The mc_evidence block (src/raypath/detail/schema3/mc_evidence.{hpp,cpp}): the v2 smooth-field
// observation record demoted wholesale into schema3 (owner ruling 2026-10-07, no kept-out
// exception). Pinned here: the carry invariants (count, order and the records' own `evidence`
// classification survive the move untouched; the options ruler moves field-for-field), the
// migration ledger's mechanical face (every row legal, renamed/deprecated rows carry a note,
// the ledger's top-level v3 homes reconcile against the schema3 serializer's actual emission,
// both ways — the v3 witness is PathFeatureReportJsonV3.LedgerTopLevelHomesReconcileTheEmission,
// which replaced this file's v2-emission reconciliation when the v2 document path was deleted),
// and the maximal-record round-trip (a record with
// every field — all optionals included — filled with sentinel values reads back field-for-field
// through the block: the no-silent-drop mechanical face).
//
// The block is a CARRY, not a mirror: these tests are what makes "全量迁入" structural rather
// than a per-field promise that would fork with the v2 struct.

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <vector>

#include "raypath/detail/path_feature_report.hpp"
#include "raypath/detail/schema3/mc_evidence.hpp"

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
  config.scene_.light_source_.spectrum_ = std::vector<WlParam>{ { 550, 1 } };
  ScatteringSetting entry{};
  entry.crystal_ = crystal;
  entry.crystal_proportion_ = 1;
  config.scene_.ms_.push_back({ 0, { entry } });
  return config;
}

// A synthetic record with the classification and one sentinel scalar per arm.
DiagnosticFeatureRecord RecordOf(DiagnosticEvidence evidence, const std::string& kind, double level_sentinel) {
  DiagnosticFeatureRecord record;
  record.evidence = evidence;
  record.kind = kind;
  record.reason = "synthetic " + kind;
  record.level = level_sentinel;
  return record;
}

std::array<double, 9> IdentityPose() {
  return { 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };
}

// ---- the carry invariants -----------------------------------------------------------------------

TEST(McEvidence, CarryPreservesCountOrderAndClassification) {
  DiscoveryResult result;
  result.features = { RecordOf(DiagnosticEvidence::kActual, "actual_a", 1.0),
                      RecordOf(DiagnosticEvidence::kCandidate, "candidate_b", 2.0),
                      RecordOf(DiagnosticEvidence::kUnfinished, "unfinished_c", 3.0),
                      RecordOf(DiagnosticEvidence::kActual, "actual_d", 4.0) };
  result.unfinished = { "stage one incomplete" };
  result.limitations = { "bounded local windows only" };
  result.event_path_evaluations = 11;
  result.field_component_evaluations = 22;
  // One sentinel component: the measure rides the carry verbatim (the presence side's input).
  analytic::WeightedSkySample sample;
  sample.sample_index = 7;
  sample.direction = { 0.0, 0.6, 0.8 };
  sample.xyz_weight = { 0.1, 0.2, 0.3 };
  result.measure.components = { sample };
  result.measure.completed_samples = 7;

  DiscoveryOptions options;
  options.sampling.requested_samples = 65536;
  options.sampling.max_optical_evaluations = 4000000;
  options.bandwidth_rad = 0.5;
  options.location_resolution_rad = 0.05;
  options.max_field_evaluations = 12345;
  options.max_seeds = 8;
  options.max_curve_points = 8;
  options.max_interface_candidates = 4;
  options.max_deviation_candidates = 6;

  schema3::McSpectralVerification spectral;
  spectral.available = true;
  spectral.movement_rad = 0.001;
  spectral.optical_evaluations = 33;
  spectral.field_component_evaluations = 44;
  spectral.seconds = 0.25;

  schema3::McEvidenceBlock block = schema3::McEvidenceOf(std::move(result), options, spectral);
  // Count, order and classification: the move's own semantics, pinned so no re-bucketing or
  // re-sorting can creep in.
  ASSERT_EQ(block.discovery.features.size(), 4u);
  EXPECT_EQ(block.discovery.features[0].evidence, DiagnosticEvidence::kActual);
  EXPECT_EQ(block.discovery.features[0].kind, "actual_a");
  EXPECT_DOUBLE_EQ(block.discovery.features[0].level, 1.0);
  EXPECT_EQ(block.discovery.features[1].evidence, DiagnosticEvidence::kCandidate);
  EXPECT_EQ(block.discovery.features[2].evidence, DiagnosticEvidence::kUnfinished);
  EXPECT_EQ(block.discovery.features[3].evidence, DiagnosticEvidence::kActual);
  EXPECT_EQ(block.discovery.features[3].kind, "actual_d");
  // The record-level side products ride.
  EXPECT_EQ(block.discovery.unfinished, std::vector<std::string>({ "stage one incomplete" }));
  EXPECT_EQ(block.discovery.limitations, std::vector<std::string>({ "bounded local windows only" }));
  EXPECT_EQ(block.discovery.event_path_evaluations, 11u);
  EXPECT_EQ(block.discovery.field_component_evaluations, 22u);
  ASSERT_EQ(block.discovery.measure.components.size(), 1u);
  EXPECT_EQ(block.discovery.measure.components[0].sample_index, 7u);
  EXPECT_DOUBLE_EQ(block.discovery.measure.components[0].xyz_weight[1], 0.2);
  // The ruler: field-for-field, the declared observation.
  EXPECT_EQ(block.observation_options.sampling.requested_samples, 65536u);
  EXPECT_EQ(block.observation_options.sampling.max_optical_evaluations, 4000000u);
  EXPECT_DOUBLE_EQ(block.observation_options.bandwidth_rad, 0.5);
  EXPECT_DOUBLE_EQ(block.observation_options.location_resolution_rad, 0.05);
  EXPECT_EQ(block.observation_options.max_field_evaluations, 12345u);
  EXPECT_EQ(block.observation_options.max_seeds, 8);
  EXPECT_EQ(block.observation_options.max_curve_points, 8);
  EXPECT_EQ(block.observation_options.max_interface_candidates, 4);
  EXPECT_EQ(block.observation_options.max_deviation_candidates, 6);
  // The spectral check and the narrowing declaration.
  ASSERT_TRUE(block.spectral_verification.available);
  ASSERT_TRUE(block.spectral_verification.movement_rad.has_value());
  EXPECT_DOUBLE_EQ(*block.spectral_verification.movement_rad, 0.001);
  EXPECT_EQ(block.spectral_verification.optical_evaluations, 33u);
  EXPECT_EQ(block.spectral_verification.field_component_evaluations, 44u);
  EXPECT_EQ(block.observation_scope_note, std::string(schema3::kMcObservationScopeNote));
  EXPECT_FALSE(block.observation_scope_note.empty());
}

TEST(McEvidence, DefaultSpectralVerificationIsDeclaredUnavailable) {
  // `available=false` is the declared "nothing to verify" shape (discrete/diagnostic spectrum),
  // never a stability claim — the default rides it.
  schema3::McEvidenceBlock block = schema3::McEvidenceOf(DiscoveryResult{}, DiscoveryOptions{});
  EXPECT_FALSE(block.spectral_verification.available);
  EXPECT_FALSE(block.spectral_verification.movement_rad.has_value());
}

// ---- the migration ledger -----------------------------------------------------------------------

TEST(McEvidence, MigrationLedgerRowsAreWellFormed) {
  ASSERT_FALSE(schema3::kMcMigrationLedger.empty());
  std::set<std::string> seen_keys;
  for (const schema3::McMigrationRow& row : schema3::kMcMigrationLedger) {
    EXPECT_NE(row.v2_key, nullptr) << "row without a v2 spelling";
    EXPECT_NE(row.v3_home, nullptr) << "row without a v3 home";
    EXPECT_STRNE(row.v2_key, "");
    EXPECT_STRNE(row.v3_home, "");
    // Renamed and deprecated rows carry a note (what moved / why gone); kept rows may leave it
    // empty. A deprecated row's note IS its reason — v1 registers the mechanism ahead of need
    // and expects zero deprecated rows.
    switch (row.status) {
      case schema3::MigrationStatus::kKept:
        break;
      case schema3::MigrationStatus::kRenamed:
        EXPECT_NE(row.note, nullptr);
        EXPECT_STRNE(row.note, "") << "renamed row without a note: " << row.v2_key;
        break;
      case schema3::MigrationStatus::kDeprecated:
        EXPECT_NE(row.note, nullptr);
        EXPECT_STRNE(row.note, "") << "deprecated row without a reason: " << row.v2_key;
        break;
    }
    const auto [_, inserted] = seen_keys.insert(
        std::string(row.section == schema3::MigrationSection::kTopLevel ? "top:" : "field:") + row.v2_key);
    EXPECT_TRUE(inserted) << "duplicate ledger key: " << row.v2_key;
  }
}

// ---- the maximal-record round-trip (the no-silent-drop mechanical face) --------------------------

TEST(McEvidence, MaximalRecordSurvivesTheCarryFieldForField) {
  // Every DiagnosticFeatureRecord field — every optional included — carries a distinct
  // sentinel through McEvidenceOf, and each reads back. A field the carry dropped would read
  // as its default and go red here (the ledger's rows are the checklist; THIS test is the proof).
  DiagnosticFeatureRecord in;
  in.evidence = DiagnosticEvidence::kCandidate;
  in.geometry = DiagnosticGeometry::kBand;
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
  in.source_token = 4242;
  in.contributor_fraction_of_estimated_y = 0.55;
  in.internal_slot = 3;
  in.source_parameters = { { "latent_a", 0.25 }, { "latent_b", 0.75 } };
  in.boundary_source = analytic::DiagnosticInputRow{};
  in.boundary_source->pose = IdentityPose();
  in.boundary_source->incident = { 0.0, 0.0, -1.0 };
  in.boundary_source->refractive_index = 1.3110129;
  in.boundary_source->source_token = 4242;
  in.boundary_value = analytic::DiagnosticOutputRow{};
  in.boundary_value->path_valid = true;
  in.boundary_value->outgoing = { 0.6, 0.0, 0.8 };
  in.boundary_value->path_evaluations = 5;
  in.boundary_value->interface_product = 0.8;
  in.interface_event = analytic::InterfaceStationaryPoint{};
  in.interface_event->travelled_rad = 0.4;
  in.interface_event->accepted_poses = { std::array<double, 9>{ 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 } };
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

  schema3::McEvidenceBlock block = schema3::McEvidenceOf(DiscoveryResult{}, DiscoveryOptions{});
  block.discovery.features = { std::move(in) };
  const DiagnosticFeatureRecord& out = block.discovery.features[0];

  EXPECT_EQ(out.evidence, DiagnosticEvidence::kCandidate);
  EXPECT_EQ(out.geometry, DiagnosticGeometry::kBand);
  EXPECT_EQ(out.kind, "sentinel_kind");
  EXPECT_EQ(out.reason, "sentinel_reason");
  ASSERT_EQ(out.sky_points.size(), 2u);
  EXPECT_DOUBLE_EQ(out.sky_points[1][1], 1.0);
  ASSERT_EQ(out.field_points.size(), 1u);
  EXPECT_EQ(out.field_points[0].status, analytic::FieldSolveStatus::kConverged);
  EXPECT_DOUBLE_EQ(out.field_points[0].query.bandwidth_rad, 0.75);
  EXPECT_DOUBLE_EQ(out.field_points[0].correction_rad, 0.125);
  EXPECT_DOUBLE_EQ(out.field_points[0].field.effective_samples_y, 99.5);
  ASSERT_TRUE(out.band.has_value());
  EXPECT_DOUBLE_EQ(out.band->levels[0], 0.3);
  EXPECT_DOUBLE_EQ(out.band->levels[1], 0.7);
  EXPECT_EQ(out.band->status, analytic::FieldSolveStatus::kConverged);
  ASSERT_EQ(out.band->boundaries[0].size(), 1u);
  EXPECT_EQ(out.equation, analytic::FieldEquation::kChromaticityX);
  EXPECT_DOUBLE_EQ(out.level, 0.4);
  EXPECT_DOUBLE_EQ(out.bandwidth_rad, 0.75);
  EXPECT_DOUBLE_EQ(out.prefix_movement_rad, 0.01);
  ASSERT_TRUE(out.replicate_movement_rad.has_value());
  EXPECT_DOUBLE_EQ(*out.replicate_movement_rad, 0.02);
  ASSERT_TRUE(out.scale_movement_rad.has_value());
  EXPECT_DOUBLE_EQ(*out.scale_movement_rad, 0.03);
  EXPECT_EQ(out.scale_status, analytic::FieldSolveStatus::kDegenerate);
  EXPECT_DOUBLE_EQ(out.minimum_effective_samples, 1234.5);
  EXPECT_DOUBLE_EQ(out.transverse_contrast, 0.9);
  EXPECT_EQ(out.walk_stop, analytic::FieldWalkStop::kObservationCensored);
  ASSERT_TRUE(out.source_token.has_value());
  EXPECT_EQ(*out.source_token, 4242u);
  ASSERT_TRUE(out.contributor_fraction_of_estimated_y.has_value());
  EXPECT_DOUBLE_EQ(*out.contributor_fraction_of_estimated_y, 0.55);
  EXPECT_EQ(out.internal_slot, 3);
  ASSERT_EQ(out.source_parameters.size(), 2u);
  EXPECT_DOUBLE_EQ(out.source_parameters[1].second, 0.75);
  ASSERT_TRUE(out.boundary_source.has_value());
  EXPECT_DOUBLE_EQ(out.boundary_source->refractive_index, 1.3110129);
  EXPECT_EQ(out.boundary_source->source_token, 4242u);
  ASSERT_TRUE(out.boundary_value.has_value());
  EXPECT_TRUE(out.boundary_value->path_valid);
  EXPECT_DOUBLE_EQ(out.boundary_value->outgoing[0], 0.6);
  EXPECT_EQ(out.boundary_value->path_evaluations, 5u);
  EXPECT_DOUBLE_EQ(out.boundary_value->interface_product, 0.8);
  ASSERT_TRUE(out.interface_event.has_value());
  EXPECT_DOUBLE_EQ(out.interface_event->travelled_rad, 0.4);
  EXPECT_EQ(out.interface_event->accepted_poses.size(), 1u);
  ASSERT_TRUE(out.paired_interface.has_value());
  EXPECT_TRUE(out.paired_interface->complete);
  EXPECT_TRUE(out.paired_interface->chromaticity_available);
  EXPECT_DOUBLE_EQ(out.paired_interface->xy_difference[1], 0.02);
  ASSERT_EQ(out.interface_curves.size(), 1u);
  EXPECT_EQ(out.interface_curves[0].stop, analytic::InterfaceWalkStop::kOpticalGate);
  ASSERT_TRUE(out.source_event.has_value());
  EXPECT_DOUBLE_EQ(out.source_event->source_width_rad, 0.2);
  ASSERT_TRUE(out.source_connected_feature.has_value());
  EXPECT_EQ(*out.source_connected_feature, 9u);
  ASSERT_TRUE(out.deviation_minimum.has_value());
  EXPECT_TRUE(out.deviation_minimum->deviation_available);
  EXPECT_DOUBLE_EQ(out.deviation_minimum->deviation_rad, 0.3852);
  EXPECT_DOUBLE_EQ(out.deviation_minimum->correction_rad, 0.001);
  ASSERT_TRUE(out.orbit_axis.has_value());
  EXPECT_DOUBLE_EQ((*out.orbit_axis)[2], 1.0);
  EXPECT_DOUBLE_EQ(out.orbit_begin_rad, 0.5);
  EXPECT_DOUBLE_EQ(out.orbit_end_rad, 6.0);
  EXPECT_DOUBLE_EQ(out.observation_contrast_error, 0.04);
  EXPECT_DOUBLE_EQ(out.atom_xyz_mass[0], 0.7);
}

}  // namespace
}  // namespace lumice::raypath
