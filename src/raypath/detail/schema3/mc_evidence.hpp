#ifndef LUMICE_RAYPATH_DETAIL_SCHEMA3_MC_EVIDENCE_HPP_
#define LUMICE_RAYPATH_DETAIL_SCHEMA3_MC_EVIDENCE_HPP_

// schema3 report core, the mc_evidence block (scrum 666.2): the v2 smooth-field observation
// record, DEMOTED wholesale into the schema3 report (owner ruling 2026-10-07, conclusions
// section 1 ruling 4 — "MC 整体降级为佐证 + 双向差异探测器"; the migration has NO kept-out
// exception). The block is a CARRY, not a mirror: `discovery` and `observation_options` are the
// v2 products moved in field-for-field by the move constructor's own semantics, so a field
// added to `DiagnosticFeatureRecord` later cannot be silently dropped by this layer — the
// no-silent-drop face is the maximal-record round-trip test plus the ledger below, not a
// field-by-field copy that would fork with the v2 struct (a56).
//
// The v2 top-level evidence buckets (`actual_features` / `candidates` / `unfinished`) do not
// exist here as arrays: the records ride `discovery.features` in build order, and each record's
// own `evidence` field keeps the v2 classification verbatim — the buckets are a serialization
// view (666.3), derived, never a second store.
//
// `observation` (the v2 top-level block) does not survive as a discovery-parameter carrier:
// observation's v3 semantics is NARROWED to a corroboration-observation declaration (scrum.md
// section 3). What v2's observation block held (kernel, bandwidth, location resolution, search
// scope) rides `observation_options` here as the DECLARED RULER of the corroboration
// observation, and `kMcObservationScopeNote` is the narrowing declaration itself — these
// parameters define the MC corroboration observation ruler; they do not define feature
// identity. The spectral-verification record rides too (it re-measured these same records
// under a refined spectrum and its movement can downgrade them — intrinsic to their meaning).
//
// The migration ledger below is the SINGLE table for the v2 -> v3 field-home accounting
// (666.2's tests walk it; 666.3's serializer tests read the SAME table — the second
// enumeration point the carry form otherwise lacks). v2 is frozen: a new v2 field must add its
// ledger row in the same change, or the round-trip/ledger tests go red.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "raypath/detail/feature_discovery.hpp"

namespace lumice::raypath::schema3 {

// The refinement check's own record (v2 top-level `spectral_verification`): only produced for
// a continuous-spectrum quadrature document, where v2 re-measured every actual feature under a
// doubled quadrature and downgraded the movers. `available=false` = the document had nothing to
// verify (discrete/diagnostic spectrum), never a claim of stability.
struct McSpectralVerification {
  bool available = false;
  std::optional<double> movement_rad;  // null = the check did not run to a number
  uint64_t optical_evaluations = 0;
  uint64_t field_component_evaluations = 0;
  double seconds = 0;
};

// The demoted v2 observation record. Field-group order: the carry (records, ruler, spectral
// check), then the narrowing declaration.
struct McEvidenceBlock {
  DiscoveryResult discovery;             // the records, the measure, unfinished, limitations, counters
  DiscoveryOptions observation_options;  // the corroboration observation's declared ruler
  McSpectralVerification spectral_verification;
  std::string observation_scope_note;  // kMcObservationScopeNote unless a caller overrides
};

// The narrowing declaration, in words (scrum.md section 3's "observation 收窄为佐证观察声明").
extern const char* const kMcObservationScopeNote;

// Moves the v2 products in. Count, order and `evidence` classification are the move's own
// semantics (nothing is re-bucketed, re-sorted or re-classified here — the tests pin that).
McEvidenceBlock McEvidenceOf(DiscoveryResult discovery, DiscoveryOptions observation_options,
                             McSpectralVerification spectral_verification = {});

// ---------------------------------------------------------------------------
// The migration ledger: v2 spelling -> v3 home -> status -> note. ONE table,
// two sections. `kTopLevel` rows are the v2 document's top-level keys (21 —
// reconciled against `PathFeatureReportToJson`'s actual emission by test: same
// set, each exactly once); `kRecordField` rows are `DiagnosticFeatureRecord`'s
// fields (the carry makes them all survive; the rows are 666.3's serialization
// checklist). Status: kept (same name, same side), renamed (the home moved —
// note names what moved and why), deprecated (v3 stops carrying it — note is
// the reason; v1 expects NONE, the mechanism is registered ahead of need).
// ---------------------------------------------------------------------------

enum class MigrationSection { kTopLevel, kRecordField };
enum class MigrationStatus { kKept, kRenamed, kDeprecated };

struct McMigrationRow {
  MigrationSection section;
  const char* v2_key;
  const char* v3_home;
  MigrationStatus status;
  const char* note;
};

extern const std::vector<McMigrationRow> kMcMigrationLedger;

}  // namespace lumice::raypath::schema3

#endif  // LUMICE_RAYPATH_DETAIL_SCHEMA3_MC_EVIDENCE_HPP_
