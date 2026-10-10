// The schema3 serializer (src/raypath/detail/path_feature_report_json.cpp
// PathFeatureReportV3ToJson, 666.3 Step 2): the v3 document's mechanical faces, pinned against
// the authorities they must not fork from —
//   AC2a  the migration ledger's top-level v3 homes reconcile the emitted key set, both ways
//         (a new top-level key must claim a ledger row or join this file's declared new-key
//         set, or the emission is unaccounted for);
//   AC2b  the ledger's 34 record-field rows are the serialization checklist: a maximal record
//         (every field populated) must surface each row's key; the schema3 blocks' struct
//         fields are pinned per block from maximal instances two ways (a dropped field and an
//         unaccounted emission both go red — Schema3BlocksEmitEveryDeclaredField);
//   D2    the three buckets are a derived view: segment(object) == BucketOf(object) per object;
//   D3/D4 the outcome grammar, the unconditional ruling block, the non-finite spellings;
//   AC1   the self-description (conventions, the vocabulary coverage row) and the narrowed
//         observation.
// The document is produced by the real serializer from a directly constructed minimal legal
// PathFeatureReport + AssembledSchema3Report. Assembly wiring has its own composition tests; making
// serializer field tests pay the physical enumeration cost would prove the same chain repeatedly.

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
#include "raypath/detail/schema3/mc_attribution.hpp"
#include "raypath/detail/schema3/mc_evidence.hpp"
#include "raypath/detail/schema3/no_related_feature.hpp"
#include "raypath/detail/schema3/structure_object.hpp"
#include "raypath/detail/schema3/support_block.hpp"

namespace lumice::raypath {
namespace {

struct V3Run {
  PathFeatureReport report;
  schema3::AssembledSchema3Report assembled;
  nlohmann::json document;
};

V3Run MinimalV3Run() {
  V3Run run;
  run.report.snapshot.scene_identity = "serializer-fixture";
  run.report.snapshot.light.spectrum_ = std::vector<WlParam>{ { 550, 1 } };
  PhysicalMemberRequest layer{};
  layer.scene_identity = run.report.snapshot.scene_identity;
  layer.crystal.id_ = 1;
  layer.representative = { 3, 5 };
  layer.semantics = SymmetrySemantics::kPhysical;
  run.report.snapshot.layers = { layer };
  run.report.options.sampling.requested_samples = 64;
  run.report.options.sampling.max_optical_evaluations = 1000;
  run.report.options.bandwidth_rad = 0.01;
  run.report.options.location_resolution_rad = 0.001;
  run.report.requested_outer_samples = 64;
  run.report.requested_path_layers = { { 3, 5 } };
  run.report.budget_ms = 100;
  run.report.spectrum_scope = "explicit wavelengths";

  run.report.representative_input.scene_identity = run.report.snapshot.scene_identity;
  AssembledLayer assembled_layer{};
  assembled_layer.scope.snapshot = layer;
  assembled_layer.scope.members = { { 3, 5 } };
  run.report.representative_input.layers = { assembled_layer };
  SpectralRow spectral{};
  spectral.wavelength_nm = 550;
  spectral.source_weight = 1;
  spectral.measure_mass = 1;
  spectral.refractive_index = 1.31;
  spectral.coefficient = { 0.1, 0.2, 0.3 };
  spectral.provenance = "serializer fixture";
  run.report.representative_input.spectrum.rows = { spectral };

  schema3::StructureObjectRecord object{};
  object.member = { 3, 5 };
  object.visibility.state = VisibilityState::kCertified;
  run.assembled.core.objects = { object };
  schema3::MemberSupport support{};
  support.member = { 3, 5 };
  run.assembled.core.support.members = { support };
  run.assembled.core.coverage.produced = {
    schema3::ObjectKind::kKind1, schema3::ObjectKind::kKind1Restricted, schema3::ObjectKind::kKind2,
    schema3::ObjectKind::kKind3, schema3::ObjectKind::kS6Junction,
  };
  run.assembled.core.coverage.declared_not_produced = {
    schema3::ObjectKind::kCorridorClosed,
    schema3::ObjectKind::kS3SupportBoundary,
    schema3::ObjectKind::kS4BranchBoundary,
    schema3::ObjectKind::kS5DensityFeature,
  };
  run.assembled.core.coverage.declared_reasons = {
    "serializer fixture",
    "serializer fixture",
    "serializer fixture",
    "serializer fixture",
  };
  schema3::CorroborationAnnotation annotation{};
  annotation.state = schema3::CorroborationState::kObserved;
  run.assembled.annotations = { annotation };
  run.assembled.mc.observation_options = run.report.options;
  run.assembled.mc.observation_scope_note = schema3::kMcObservationScopeNote;
  run.assembled.mc.discovery.measure.sources.push_back({ 0, 0, 0 });
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

std::set<std::string> KeysOf(const nlohmann::json& object) {
  std::set<std::string> keys;
  for (auto it = object.begin(); it != object.end(); ++it) {
    keys.insert(it.key());
  }
  return keys;
}

// Two-way block pin: every declared key present, every emitted key declared. `what` names the
// block in the failure output.
void ExpectKeySet(const nlohmann::json& object, std::initializer_list<const char*> expected, const char* what) {
  const std::set<std::string> got = KeysOf(object);
  std::set<std::string> want;
  for (const char* key : expected) {
    want.insert(key);
    EXPECT_NE(got.find(key), got.end()) << what << " dropped its " << key << " key";
  }
  for (const std::string& key : got) {
    EXPECT_NE(want.find(key), want.end()) << what << " emits unaccounted key " << key;
  }
}

}  // namespace

TEST(PathFeatureReportJsonV3, Schema3BlocksEmitEveryDeclaredField) {
  // AC2(b), the block half: each schema3 struct's fields surface in the JSON, pinned per block
  // from a MAXIMAL instance (every field populated; gated emissions pinned in the state that
  // opens the gate). The declared sets are enumerated from the struct headers in the same
  // change as the struct: a new struct field that skips its line here stays unserialized in
  // silence, which is exactly what this test exists to prevent. (The record checklist half of
  // AC2(b) rides the ledger — RecordFieldChecklistRidesTheLedger; the mc_evidence block's
  // discovery payload decomposes across the ledger's top-level homes, which AC2a already
  // reconciles, so no second inventory of it lives here.)
  V3Run run = MinimalV3Run();
  ASSERT_FALSE(run.assembled.core.objects.empty());
  ASSERT_FALSE(run.assembled.core.support.members.empty());

  // -- StructureObjectRecord, every field populated (identity, four machines, geometry,
  //    diagnostics), each gated emission's gate opened.
  schema3::StructureObjectRecord& object = run.assembled.core.objects.front();
  schema3::CorroborationAnnotation& annotation = run.assembled.annotations.front();
  object.kind = schema3::ObjectKind::kKind3;
  object.member = { 3, 5 };
  object.slot = 3;
  object.phi_class_note = "sentinel_phi_note";
  object.existence = ExistenceState::kEscaped;  // opens the escape_regime_slug gate
  object.escape_regime_slug = "slab_crease";
  object.walk_s = 1.5;
  object.visibility = {};
  object.visibility.state = VisibilityState::kCertified;
  object.visibility.lit_fraction = 0.75;
  object.visibility.jets_ok = true;
  object.visibility.saw_zero_area = true;
  object.visibility.saw_zero_transmission = true;
  object.visibility.reason = "sentinel_visibility_reason";
  object.chromatic_assessed = true;
  object.chromatic = {};
  object.chromatic.kind = analytic::ChromaticVerdictKind::kTint;
  object.chromatic.color = analytic::ChromaticColor::kRed;
  object.chromatic.visible = true;
  object.chromatic.coverage_complete = false;
  object.chromatic.has_position = true;
  object.chromatic.position = 0.02;
  analytic::ChromaticFeature feature{};
  feature.kind = analytic::ChromaticFeatureKind::kEdge;
  feature.source = "sentinel_feature_source";
  feature.color = analytic::ChromaticColor::kBlue;
  feature.visible = true;
  feature.positive_fraction = 0.1;
  feature.delta_red = 0.01;
  feature.delta_blue = 0.02;
  feature.shift = 0.03;
  feature.spread = 0.04;
  feature.direction_dispersion = 0.05;
  feature.contrast = 0.06;
  feature.weight = 0.07;
  feature.lit_fraction = 0.08;
  object.chromatic.features = { feature };
  object.chromatic.has_tint = true;
  object.chromatic.tint = { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 };
  object.chromatic.notes = { "sentinel_chromatic_note" };
  annotation = {};
  annotation.state = schema3::CorroborationState::kObserved;
  annotation.presence_ess = 50.0;
  annotation.match_distance = 0.01;
  annotation.tolerance_rad = 0.02;
  annotation.matched_record = 0;
  annotation.matched_record_ess = 40.0;
  annotation.ruler = "sentinel_corroboration_ruler";
  annotation.reason = "sentinel_corroboration_reason";
  object.has_sky_position = true;
  object.sky_position[0] = 0.1;
  object.sky_position[1] = 0.2;
  object.sky_position[2] = 0.3;
  object.u = { 0.1, 0.2, 0.3 };
  object.counterfactual = { true, 2.07, 0.75 };

  const nlohmann::json maximal = nlohmann::json::parse(PathFeatureReportV3ToJson(run.report, run.assembled, "test"));
  const nlohmann::json* serialized = nullptr;
  for (const char* segment : { "actual", "candidate", "unfinished" }) {
    for (const nlohmann::json& entry : maximal["features"][segment]) {
      if (entry["id"] == 0) {
        serialized = &entry;
      }
    }
  }
  ASSERT_NE(serialized, nullptr) << "the mutated object (id 0) must appear in some segment";
  const nlohmann::json& record = *serialized;
  ExpectKeySet(record,
               { "id", "kind", "member", "slot", "phi_class_note", "existence", "visibility", "chromatic",
                 "corroboration", "geometry", "diagnostics" },
               "features[] object");
  ExpectKeySet(record["existence"], { "state", "walk_s", "escape_regime_slug" }, "existence block");
  ExpectKeySet(record["visibility"],
               { "state", "lit_fraction", "evidence", "jets_ok", "saw_zero_area", "saw_zero_transmission", "reason" },
               "visibility block");
  ExpectKeySet(record["chromatic"], { "assessed", "verdict", "thresholds" }, "chromatic block");
  ExpectKeySet(record["chromatic"]["verdict"],
               { "kind", "color", "visible", "coverage_complete", "faces", "n_red", "n_blue", "position_rad",
                 "features", "tint", "notes" },
               "chromatic verdict");
  EXPECT_FALSE(record["chromatic"]["verdict"]["coverage_complete"].get<bool>())
      << "the mutated false must ride through (a line the verdict rests on was not analysed)";
  EXPECT_EQ(record["chromatic"]["verdict"]["tint"]["energy_red"].get<double>(), 1.0);
  ExpectKeySet(record["chromatic"]["verdict"]["features"][0],
               { "kind", "source", "color", "visible", "positive_fraction", "delta_red_rad", "delta_blue_rad",
                 "shift_rad", "spread_rad", "direction_dispersion", "contrast", "weight", "lit_fraction" },
               "chromatic feature row");
  ExpectKeySet(
      record["chromatic"]["verdict"]["tint"],
      { "energy_red", "energy_blue", "ratio", "tir_fraction_red", "tir_fraction_blue", "direction_dispersion" },
      "chromatic tint block");
  ExpectKeySet(record["chromatic"]["thresholds"],
               { "n_red", "n_blue", "edge_min_shift_rad", "edge_spread_per_shift", "calibration_white_max_deviation",
                 "tint_ratio_min" },
               "chromatic thresholds snapshot");
  ExpectKeySet(record["corroboration"],
               { "state", "presence_ess", "match_distance_rad", "tolerance_rad", "matched_record", "matched_record_ess",
                 "ruler", "reason" },
               "corroboration block");
  ExpectKeySet(record["geometry"], { "u", "sky_position" }, "geometry block");
  ExpectKeySet(record["diagnostics"]["counterfactual"], { "available", "with_slot", "without_slot" },
               "counterfactual block");

  // -- UnattributedOutcome, error gate open.
  schema3::UnattributedStructure finding{};
  finding.record_index = 0;
  finding.position = { 1.0, 0.0, 0.0 };
  finding.delta_rad = 0.5;
  finding.record_ess = 100.0;
  finding.min_margin_rad = 0.25;
  finding.ruler = "sentinel_unattributed_ruler";
  run.assembled.unattributed.structures.push_back(finding);
  run.assembled.unattributed.skipped_no_position = 1;
  run.assembled.unattributed.skipped_below_ess = 2;
  run.assembled.unattributed.counts = { 7, 3 };
  run.assembled.unattributed.error = "sentinel_unattributed_error";
  const nlohmann::json attributed = nlohmann::json::parse(PathFeatureReportV3ToJson(run.report, run.assembled, "test"));
  ExpectKeySet(attributed["unattributed_structures"],
               { "structures", "skipped_no_position", "skipped_below_ess", "error" }, "unattributed block");
  ExpectKeySet(attributed["unattributed_structures"]["structures"].back(),
               { "record_index", "position", "delta_rad", "record_ess", "min_margin_rad", "ruler" },
               "unattributed structure row");

  // -- McEvidenceBlock face: the emitted keys; maximal spectral_verification (movement spelled
  //    as a number — the early path's null face is the multicrystal test's).
  run.assembled.mc.spectral_verification.available = true;
  run.assembled.mc.spectral_verification.movement_rad = 0.5;
  const nlohmann::json evidenced = nlohmann::json::parse(PathFeatureReportV3ToJson(run.report, run.assembled, "test"));
  ExpectKeySet(evidenced["mc_evidence"],
               { "records", "observation_options", "spectral_verification", "observation_scope_note", "unfinished" },
               "mc_evidence block");
  ExpectKeySet(evidenced["mc_evidence"]["spectral_verification"],
               { "available", "movement_rad", "optical_evaluations", "field_component_evaluations", "seconds" },
               "spectral_verification block");
  ExpectKeySet(evidenced["mc_evidence"]["observation_options"],
               { "kernel", "bandwidth_rad", "location_resolution_rad", "search_scope" }, "observation_options block");

  // -- NoRelatedRuling: the four conditions and the gate ride always; `basis` joins exactly
  //    when issued (the struct's own gating).
  const nlohmann::json unissued = nlohmann::json::parse(PathFeatureReportV3ToJson(run.report, run.assembled, "test"));
  ExpectKeySet(unissued["no_related_feature"],
               { "issued", "partition_complete_no_escape", "partition_failure", "all_objects_unlit_or_none",
                 "unlit_failure", "no_sufficient_ess_unattributed", "unattributed_failure", "s4_scope_declared",
                 "s4_failure", "two_d_valid_support", "s4_scope_note", "absence_not_proven_note" },
               "unissued ruling block");
  run.assembled.ruling.issued = true;
  run.assembled.ruling.basis = schema3::NoRelatedBasis::kZeroSpectralSignal;
  run.assembled.ruling.partition_failure = "sentinel_partition_failure";
  run.assembled.ruling.unlit_failure = "sentinel_unlit_failure";
  run.assembled.ruling.unattributed_failure = "sentinel_unattributed_failure";
  run.assembled.ruling.s4_failure = "sentinel_s4_failure";
  run.assembled.ruling.s4_scope_note = "sentinel_s4_scope_note";
  const nlohmann::json issued = nlohmann::json::parse(PathFeatureReportV3ToJson(run.report, run.assembled, "test"));
  ExpectKeySet(issued["no_related_feature"],
               { "issued", "basis", "partition_complete_no_escape", "partition_failure", "all_objects_unlit_or_none",
                 "unlit_failure", "no_sufficient_ess_unattributed", "unattributed_failure", "s4_scope_declared",
                 "s4_failure", "two_d_valid_support", "s4_scope_note", "absence_not_proven_note" },
               "issued ruling block");
  run.assembled.ruling.issued = false;

  // -- Support block: one maximal member row (every partition face and both gated arms open)
  //    and both family forms.
  schema3::MemberSupport& row = run.assembled.core.support.members.front();
  row.axis.regime_slug = "slab_crease";                        // opens the slug gate
  row.axis.context.escape_regime = EscapeRegime::kSlabCrease;  // opens the typed-key gate (G3);
                                                               // the kUnset arm is
                                                               // UnsetEscapeRegimeOmitsTheTypedKey's
  row.axis.message = "sentinel_partition_message";
  row.axis.intervals = { analytic::DeviationInterval{ 0.4, 0.5, 2, 1, 1 } };
  analytic::CriticalOnset onset{};
  onset.value = 0.1;
  onset.gradient_norm = 0.6;
  onset.has_measure_limit = true;
  onset.measure_limit = 0.2;
  row.endpoint_onsets = { onset };
  schema3::ConstantDeltaCurve curve{};
  curve.d_p = 0.3;
  curve.weight_step = 2;
  curve.wavelengths_nm = { 550 };
  curve.critical_d_p = { 0.31 };
  row.constant_curves = { curve };
  analytic::DeviationInterval shared_interval{};
  shared_interval.lower = 0.4;
  shared_interval.upper = 0.5;
  shared_interval.n_components = 2;
  shared_interval.n_closed = 1;
  shared_interval.n_open = 1;
  schema3::FamilySupport shared_family{};
  shared_family.shared = true;
  shared_family.intervals = { shared_interval };
  shared_family.members = { { 3, 5 } };
  shared_family.phi_class_note = "sentinel_family_phi";
  schema3::FamilySupport disjoint_family{};
  disjoint_family.shared = false;
  disjoint_family.intervals = { shared_interval };  // carried but never emitted (see below)
  disjoint_family.members = { { 1, 3 } };
  disjoint_family.phi_class_note = "sentinel_other_phi";
  run.assembled.core.support.families = { shared_family, disjoint_family };
  const nlohmann::json supported = nlohmann::json::parse(PathFeatureReportV3ToJson(run.report, run.assembled, "test"));
  ExpectKeySet(supported["support"], { "members", "families" }, "support block");
  const nlohmann::json& member_row = supported["support"]["members"][0];
  ExpectKeySet(member_row, { "member", "partition", "endpoint_onsets", "constant_curves" }, "support member row");
  ExpectKeySet(
      member_row["partition"],
      { "coverage", "walk_status", "walk_closed", "intervals", "escape_regime_slug", "escape_regime", "message" },
      "partition block");
  ExpectKeySet(member_row["partition"]["intervals"][0],
               { "lower_rad", "upper_rad", "n_components", "n_closed", "n_open" }, "partition interval row");
  ExpectKeySet(member_row["endpoint_onsets"][0],
               { "value_rad", "location", "source", "profile", "gradient_norm", "has_measure_limit", "measure_limit",
                 "multiplicity" },
               "endpoint onset row");
  ExpectKeySet(member_row["constant_curves"][0], { "d_p_rad", "weight_step", "wavelengths_nm", "critical_d_p_rad" },
               "constant curve row");
  const nlohmann::json& shared_row = supported["support"]["families"][0];
  const nlohmann::json& disjoint_row = supported["support"]["families"][1];
  ExpectKeySet(shared_row, { "shared", "intervals", "members", "phi_class_note" }, "shared family row");
  EXPECT_EQ(shared_row["intervals"].size(), 1u) << "a shared family reports its intervals once";
  EXPECT_TRUE(disjoint_row["intervals"].empty())
      << "a non-shared family emits an EMPTY intervals list by contract — the struct's carried "
         "intervals are deliberately not reported (no fabricated union; per-member intervals "
         "live on the member rows), not a dropped field";
}

TEST(PathFeatureReportJsonV3, UnsetEscapeRegimeOmitsTheTypedKey) {
  // G3: the typed escape_regime field's default is the kUnset sentinel — a partition that
  // escaped (the slug names the regime) but carries no contract-registered regime value must NOT
  // emit the typed key at all. Serializing the sentinel's own spelling ("unset") would just move
  // the fake-data problem: the old default wrote "slab_crease" here, a regime name the record
  // never escaped with. The slug key is the datum and still rides; the typed key returns only
  // when a producer actually sets a registered regime.
  V3Run run = MinimalV3Run();
  ASSERT_FALSE(run.assembled.core.support.members.empty());
  schema3::MemberSupport& row = run.assembled.core.support.members.front();
  row.axis.regime_slug = "slab_crease_touching";  // the kernel-facing slug, non-empty
  // context.escape_regime stays at its kUnset default: the "no regime was set" state.
  const nlohmann::json document = nlohmann::json::parse(PathFeatureReportV3ToJson(run.report, run.assembled, "test"));
  const nlohmann::json& partition = document["support"]["members"][0]["partition"];
  EXPECT_FALSE(partition.contains("escape_regime"))
      << "the kUnset sentinel must omit the typed key, not serialize the sentinel's own name";
  EXPECT_TRUE(partition.contains("escape_regime_slug"))
      << "the slug is the regime's datum and rides regardless of the sentinel";

  // The explicit arm: a producer that sets a registered regime keeps both keys.
  row.axis.context.escape_regime = EscapeRegime::kSlabCrease;
  const nlohmann::json named = nlohmann::json::parse(PathFeatureReportV3ToJson(run.report, run.assembled, "test"));
  const nlohmann::json& named_partition = named["support"]["members"][0]["partition"];
  EXPECT_EQ(named_partition["escape_regime"], "slab_crease");
  EXPECT_EQ(named_partition["escape_regime_slug"], "slab_crease_touching");
}

TEST(PathFeatureReportJsonV3, LedgerTopLevelHomesReconcileTheEmission) {
  // AC2a, both ways. Every top-level ledger row's v3 home (its FIRST path segment) must exist
  // in the emitted document; every emitted top-level key must be claimed by a ledger home or
  // belong to the v3-new blocks the migration never carried (declared here — a new top-level
  // key that joins neither set is an unaccounted emission and goes red).
  V3Run run = MinimalV3Run();
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
  V3Run run = MinimalV3Run();
  run.assembled.mc.discovery.features = { MaximalRecord(0) };
  const nlohmann::json document = nlohmann::json::parse(PathFeatureReportV3ToJson(run.report, run.assembled, "test"));
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
  V3Run run = MinimalV3Run();
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
  V3Run run = MinimalV3Run();
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
  V3Run run = MinimalV3Run();
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
  V3Run run = MinimalV3Run();
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
  PathFeatureReport report;
  report.unsupported_multicrystal = true;
  report.requested_path_layers = { { 3, 5 }, { 1, 3 } };
  schema3::AssembledSchema3Report empty{};
  const nlohmann::json document = nlohmann::json::parse(PathFeatureReportV3ToJson(report, empty, "test"));
  EXPECT_EQ(document["outcome"], "unsupported_multicrystal");
  // The early path rides the SAME version constant as the main path — a second spelling here
  // would mislabel the document on the next bump while it still names the v3 shape.
  EXPECT_EQ(document["schema_version"], kFeatureReportSchemaVersion);
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
