#include "raypath/detail/schema3/mc_evidence.hpp"

namespace lumice::raypath::schema3 {

const char* const kMcObservationScopeNote =
    "these parameters define the MC corroboration observation ruler; they do not define feature "
    "identity (the v2 discovery observation, demoted and narrowed — scrum 666 ruling 2026-10-07)";

McEvidenceBlock McEvidenceOf(DiscoveryResult discovery, DiscoveryOptions observation_options,
                             McSpectralVerification spectral_verification) {
  McEvidenceBlock block;
  block.discovery = std::move(discovery);
  block.observation_options = std::move(observation_options);
  block.spectral_verification = spectral_verification;
  block.observation_scope_note = kMcObservationScopeNote;
  return block;
}

// The ledger. Top-level section: the 21 keys `PathFeatureReportToJson` emits on a normal
// document, in emission order. Renamed rows name the v3 home the plan/scrum section 3 shape
// fixed; kept rows keep name and side. Record-field section: every `DiagnosticFeatureRecord`
// member in declaration order — all kept, because the carry moves the struct itself.
const std::vector<McMigrationRow> kMcMigrationLedger = {
  // ---- top-level keys (the normal document path; the unsupported-chain early path emits a
  // subset of these — schema/schema_version/outcome/requested_path_layers/buckets/coverage/
  // budgets — all rows below cover it).
  { MigrationSection::kTopLevel, "schema", "schema", MigrationStatus::kKept, "" },
  { MigrationSection::kTopLevel, "schema_version", "schema_version", MigrationStatus::kKept,
    "value re-cut to 3 by the schema3 serializer (666.3); the KEY is unchanged" },
  { MigrationSection::kTopLevel, "generator", "generator", MigrationStatus::kKept, "" },
  { MigrationSection::kTopLevel, "scope", "scope", MigrationStatus::kKept,
    "scene identity, spectral scope, seed, member semantics, light, layers all stay" },
  { MigrationSection::kTopLevel, "conventions", "conventions", MigrationStatus::kKept, "" },
  { MigrationSection::kTopLevel, "support", "scope.support_dimensions", MigrationStatus::kRenamed,
    "v2's support block is the input-side pose/shape/source/spectral dimension description; it "
    "moves under scope. The v3 top-level `support` name is REUSED by the new delta-axis support "
    "block (schema3 MemberSupport/FamilySupport) — the rename frees the name instead of "
    "overloading it" },
  { MigrationSection::kTopLevel, "physical_members", "scope.physical_members", MigrationStatus::kRenamed,
    "input identity (the assembled physical-L2 face sequences) rides the scope side" },
  { MigrationSection::kTopLevel, "spectrum", "scope.spectrum", MigrationStatus::kRenamed,
    "the assembled spectral rows (nm/weights/index/XYZ coefficient) are input assembly, not "
    "discovery product — the representative input's spectrum rides scope" },
  { MigrationSection::kTopLevel, "requested_outer_samples", "scope.requested_outer_samples", MigrationStatus::kRenamed,
    "the request's sample_count verbatim (0 = auto) is request identity; the RESOLVED count "
    "stays in budgets.requested_outer_samples" },
  { MigrationSection::kTopLevel, "budget_ms", "scope.budget_ms", MigrationStatus::kRenamed,
    "request echo, rides the request identity with the sample echo" },
  { MigrationSection::kTopLevel, "budgets", "budgets", MigrationStatus::kKept, "" },
  { MigrationSection::kTopLevel, "timing", "timing", MigrationStatus::kKept, "" },
  { MigrationSection::kTopLevel, "observation", "mc_evidence", MigrationStatus::kRenamed,
    "v2's discovery-observation declaration (kernel/bandwidth/location resolution/search scope) "
    "is demoted into the mc_evidence block as observation_options + the narrowing note; the v3 "
    "top-level `observation` is re-issued NARROWED to the corroboration-observation declaration "
    "(scrum 666 section 3), not dropped" },
  { MigrationSection::kTopLevel, "spectral_verification", "mc_evidence.spectral_verification",
    MigrationStatus::kRenamed,
    "the refinement check re-measured these same records and can downgrade them — it rides the "
    "demoted record set (McSpectralVerification)" },
  { MigrationSection::kTopLevel, "actual_features", "mc_evidence.records", MigrationStatus::kRenamed,
    "the v2 evidence buckets are serialization views over the carried records; the record's own "
    "`evidence` field keeps the v2 classification verbatim (actual)" },
  { MigrationSection::kTopLevel, "candidates", "mc_evidence.records", MigrationStatus::kRenamed,
    "same demotion; classification kept on the record (candidate)" },
  { MigrationSection::kTopLevel, "unfinished", "mc_evidence.records", MigrationStatus::kRenamed,
    "same demotion; classification kept on the record (unfinished) — the bucket split no "
    "longer exists as arrays in v3" },
  { MigrationSection::kTopLevel, "sources", "mc_evidence.records[].source_token + discovery.measure.sources",
    MigrationStatus::kRenamed,
    "v2's top-level sources object was a projection of the measure's token table; the "
    "authoritative table rides the carry and 666.3 re-derives the view from it" },
  { MigrationSection::kTopLevel, "unfinished_reasons", "mc_evidence.discovery.unfinished", MigrationStatus::kRenamed,
    "stage-level incompleteness strings ride the DiscoveryResult carry" },
  { MigrationSection::kTopLevel, "coverage", "coverage", MigrationStatus::kKept,
    "v3 re-derives the rows' subjects from the schema3 shape (666.3); the top-level block and "
    "its role (bounded-search honesty) are unchanged" },
  { MigrationSection::kTopLevel, "outcome", "outcome", MigrationStatus::kKept,
    "the v3 grammar extends no_related_feature to the dual-form ruling (schema3 "
    "no_related_feature module); the key and the completed/partial arms are unchanged" },

  // ---- DiagnosticFeatureRecord fields (declaration order; all kept — the carry moves the
  // struct itself, so these rows are 666.3's per-field serialization checklist, not a copy).
  { MigrationSection::kRecordField, "evidence", "mc_evidence.records[].evidence", MigrationStatus::kKept,
    "the v2 classification, preserved verbatim — the v3 bucket derivation reads it" },
  { MigrationSection::kRecordField, "geometry", "mc_evidence.records[].geometry", MigrationStatus::kKept, "" },
  { MigrationSection::kRecordField, "kind", "mc_evidence.records[].kind", MigrationStatus::kKept, "" },
  { MigrationSection::kRecordField, "reason", "mc_evidence.records[].reason", MigrationStatus::kKept, "" },
  { MigrationSection::kRecordField, "sky_points", "mc_evidence.records[].sky_points", MigrationStatus::kKept,
    "unit world viewing directions (negative propagation) — the MC side of the attribution match" },
  { MigrationSection::kRecordField, "field_points", "mc_evidence.records[].field_points", MigrationStatus::kKept, "" },
  { MigrationSection::kRecordField, "band", "mc_evidence.records[].band", MigrationStatus::kKept, "" },
  { MigrationSection::kRecordField, "equation", "mc_evidence.records[].equation", MigrationStatus::kKept, "" },
  { MigrationSection::kRecordField, "level", "mc_evidence.records[].level", MigrationStatus::kKept, "" },
  { MigrationSection::kRecordField, "bandwidth_rad", "mc_evidence.records[].bandwidth_rad", MigrationStatus::kKept,
    "the record's own observation width — the attribution tolerance's declared-width input" },
  { MigrationSection::kRecordField, "prefix_movement_rad", "mc_evidence.records[].prefix_movement_rad",
    MigrationStatus::kKept, "" },
  { MigrationSection::kRecordField, "replicate_movement_rad", "mc_evidence.records[].replicate_movement_rad",
    MigrationStatus::kKept, "" },
  { MigrationSection::kRecordField, "scale_movement_rad", "mc_evidence.records[].scale_movement_rad",
    MigrationStatus::kKept, "" },
  { MigrationSection::kRecordField, "scale_status", "mc_evidence.records[].scale_status", MigrationStatus::kKept, "" },
  { MigrationSection::kRecordField, "minimum_effective_samples", "mc_evidence.records[].minimum_effective_samples",
    MigrationStatus::kKept,
    "the record's Y-ESS — the FORWARD unattributed gate's and `observed`'s significance standing" },
  { MigrationSection::kRecordField, "transverse_contrast", "mc_evidence.records[].transverse_contrast",
    MigrationStatus::kKept, "" },
  { MigrationSection::kRecordField, "walk_stop", "mc_evidence.records[].walk_stop", MigrationStatus::kKept, "" },
  { MigrationSection::kRecordField, "source_token", "mc_evidence.records[].source_token", MigrationStatus::kKept,
    "local to discovery.measure — never a cross-run identifier" },
  { MigrationSection::kRecordField, "contributor_fraction_of_estimated_y",
    "mc_evidence.records[].contributor_fraction_of_estimated_y", MigrationStatus::kKept, "" },
  { MigrationSection::kRecordField, "internal_slot", "mc_evidence.records[].internal_slot", MigrationStatus::kKept,
    "" },
  { MigrationSection::kRecordField, "source_parameters", "mc_evidence.records[].source_parameters",
    MigrationStatus::kKept, "" },
  { MigrationSection::kRecordField, "boundary_source", "mc_evidence.records[].boundary_source", MigrationStatus::kKept,
    "" },
  { MigrationSection::kRecordField, "boundary_value", "mc_evidence.records[].boundary_value", MigrationStatus::kKept,
    "" },
  { MigrationSection::kRecordField, "interface_event", "mc_evidence.records[].interface_event", MigrationStatus::kKept,
    "" },
  { MigrationSection::kRecordField, "paired_interface", "mc_evidence.records[].paired_interface",
    MigrationStatus::kKept, "" },
  { MigrationSection::kRecordField, "interface_curves", "mc_evidence.records[].interface_curves",
    MigrationStatus::kKept, "" },
  { MigrationSection::kRecordField, "source_event", "mc_evidence.records[].source_event", MigrationStatus::kKept, "" },
  { MigrationSection::kRecordField, "source_connected_feature", "mc_evidence.records[].source_connected_feature",
    MigrationStatus::kKept, "" },
  { MigrationSection::kRecordField, "deviation_minimum", "mc_evidence.records[].deviation_minimum",
    MigrationStatus::kKept, "" },
  { MigrationSection::kRecordField, "orbit_axis", "mc_evidence.records[].orbit_axis", MigrationStatus::kKept, "" },
  { MigrationSection::kRecordField, "orbit_begin_rad", "mc_evidence.records[].orbit_begin_rad", MigrationStatus::kKept,
    "" },
  { MigrationSection::kRecordField, "orbit_end_rad", "mc_evidence.records[].orbit_end_rad", MigrationStatus::kKept,
    "" },
  { MigrationSection::kRecordField, "observation_contrast_error", "mc_evidence.records[].observation_contrast_error",
    MigrationStatus::kKept, "" },
  { MigrationSection::kRecordField, "atom_xyz_mass", "mc_evidence.records[].atom_xyz_mass", MigrationStatus::kKept,
    "" },
};

}  // namespace lumice::raypath::schema3
