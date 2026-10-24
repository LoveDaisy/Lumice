#ifndef LUMICE_RAYPATH_FEATURE_REPORT_RESULT_HPP_
#define LUMICE_RAYPATH_FEATURE_REPORT_RESULT_HPP_

#include <algorithm>
#include <stdexcept>

#include "raypath/domain/result_identity.hpp"
#include "raypath/results/report_attribution.hpp"
#include "raypath/results/report_enumeration.hpp"
#include "raypath/results/report_records.hpp"
#include "raypath/results/report_scope.hpp"

namespace lumice::raypath {

enum class ReportOutcome {
  kNotCompleted,  // the construction-slot default: no completion assembly has run yet
  kCompleted,
  kPartial,
  kNoRelatedFeature,
  kUnsupportedMulticrystal,
};
enum class CoverageStatus { kSupported, kNumericalIncomplete, kNotSupported };
// The row's machine role, owned by the completion assembly. The codec partitions by this role;
// display text and array position carry no meaning (a renamed subject keeps its limitations, an
// inserted row keeps the vocabulary ledger in place).
enum class CoverageRole {
  kBaseInput,         // the actual product input the run read
  kBoundedDiscovery,  // the bounded-search honesty row; the only role carrying limitations
  kDeclaredSkip,      // a leg the run declared it did not evaluate
};
struct ReportCoverage {
  CoverageRole role = CoverageRole::kBaseInput;
  std::string subject;
  CoverageStatus status = CoverageStatus::kNotSupported;
  std::string reason;
  std::vector<std::string> limitations;
};
struct ReportObservation {
  double bandwidth_rad = 0;
  double location_resolution_rad = 0;
};
struct ReportMcEvidence {
  CompletedDiscovery discovery;
  ReportObservation observation_options;
  schema3::McSpectralVerification spectral_verification;
  std::string observation_scope_note;
};
struct ReportBudgets {
  uint64_t requested_outer_samples = 0;
  uint64_t completed_outer_samples = 0;
  uint64_t replicate_samples = 0;
  uint32_t replicate_seed = 0;
  uint64_t max_optical_evaluations = 0;
  uint64_t optical_evaluations = 0;
  uint64_t max_field_component_evaluations = 0;
  uint64_t field_component_evaluations = 0;
  bool exhausted = false;
  uint64_t attribution_dp_evaluations = 0;
  uint64_t attribution_presence_ess_queries = 0;
};
struct ReportTiming {
  double spectral_verification = 0;
  double capture = 0;
  double assembly = 0;
  double source_events_and_edge_observations = 0;
  double field_discovery = 0;
  double enumeration = 0;
  double attribution = 0;
};

// One completed value. In particular, MC source witnesses have already been replayed;
// neither a serializer nor a typed reader retains a sampler or an input snapshot.
struct FeatureReportData {
  ReportScope scope;
  // kNotCompleted is the construction slot's value; only the completion assembly assigns a
  // final outcome, and FeatureReportResult refuses to freeze the slot.
  ReportOutcome outcome = ReportOutcome::kNotCompleted;
  std::vector<std::vector<int>> requested_path_layers;
  schema3::Schema3DiscoveryCore core;
  std::vector<schema3::CorroborationAnnotation> annotations;
  ReportMcEvidence mc;
  schema3::UnattributedOutcome unattributed;
  schema3::McAttributionCounts corroboration_counts;
  schema3::NoRelatedRuling ruling;
  std::vector<ReportCoverage> coverage;
  ReportBudgets budgets;
  ReportTiming timing;
};

// A completed product owns one immutable value. Copies preserve identity and storage;
// construction from revised assembly data creates a different snapshot.
class FeatureReportResult : public FrozenValue<FeatureReportData> {
 public:
  // The output slot an entry writes into. No completion has run, so the value is not
  // publishable: the data constructor below rejects it and PathFeatureReportV3ToJson throws on it.
  FeatureReportResult() : FrozenValue<FeatureReportData>(FeatureReportData{}) {}
  explicit FeatureReportResult(FeatureReportData data) : FrozenValue<FeatureReportData>(Validated(std::move(data))) {}
  std::optional<RecordRef> Reference(RecordId id) const {
    return id.ordinal < Data().mc.discovery.features.size() ? std::optional<RecordRef>{ { Identity(), id } } :
                                                              std::nullopt;
  }
  const DiagnosticFeatureRecord* Resolve(RecordRef ref) const {
    return ref.result == Identity() && ref.record.ordinal < Data().mc.discovery.features.size() ?
               &Data().mc.discovery.features[ref.record.ordinal] :
               nullptr;
  }
  std::optional<SourceRef> Reference(SourceId id) const {
    return id.ordinal < Data().mc.discovery.measure.sources.size() ? std::optional<SourceRef>{ { Identity(), id } } :
                                                                     std::nullopt;
  }
  const DiagnosticSourceIdentity* Resolve(SourceRef ref) const {
    return ref.result == Identity() && ref.source.ordinal < Data().mc.discovery.measure.sources.size() ?
               &Data().mc.discovery.measure.sources[ref.source.ordinal] :
               nullptr;
  }
  const schema3::CorroborationAnnotation* Annotation(ObjectRef ref) const {
    return Resolve(ref) && ref.object.ordinal < Data().annotations.size() ? &Data().annotations[ref.object.ordinal] :
                                                                            nullptr;
  }
  std::optional<RecordRef> MatchedRecord(ObjectRef ref) const {
    const auto* annotation = Annotation(ref);
    return annotation && annotation->matched_record >= 0 ?
               Reference(RecordId{ static_cast<size_t>(annotation->matched_record) }) :
               std::nullopt;
  }
  std::optional<SourceRef> SourceOf(RecordRef ref) const {
    const auto* record = Resolve(ref);
    return record && record->source_token ? Reference(SourceId{ static_cast<size_t>(*record->source_token) }) :
                                            std::nullopt;
  }
  std::optional<RecordRef> ConnectedRecord(RecordRef ref) const {
    const auto* record = Resolve(ref);
    return record && record->source_connected_feature ? Reference(RecordId{ *record->source_connected_feature }) :
                                                        std::nullopt;
  }
  std::optional<MemberRef> SourceMember(SourceRef ref) const {
    const auto* source = Resolve(ref);
    return source ? Reference(MemberId{ source->member_index }) : std::nullopt;
  }
  std::optional<RecordRef> UnattributedRecord(size_t index) const {
    return index < Data().unattributed.structures.size() ?
               Reference(RecordId{ Data().unattributed.structures[index].record_index }) :
               std::nullopt;
  }

  std::optional<MemberRef> ObjectMember(ObjectRef ref) const {
    const auto* object = Resolve(ref);
    if (!object)
      return std::nullopt;
    const auto& members = Data().scope.physical_members;
    const auto found = std::find(members.begin(), members.end(), object->member);
    return found == members.end() ? std::nullopt : Reference(MemberId{ static_cast<size_t>(found - members.begin()) });
  }
  std::optional<ObjectRef> Reference(ObjectId id) const {
    return id.ordinal < Data().core.objects.size() ? std::optional<ObjectRef>{ { Identity(), id } } : std::nullopt;
  }
  const schema3::StructureObjectRecord* Resolve(ObjectRef ref) const {
    return ref.result == Identity() && ref.object.ordinal < Data().core.objects.size() ?
               &Data().core.objects[ref.object.ordinal] :
               nullptr;
  }

  std::optional<MemberRef> Reference(MemberId id) const {
    return id.ordinal < Data().scope.physical_members.size() ? std::optional<MemberRef>{ { Identity(), id } } :
                                                               std::nullopt;
  }
  const std::vector<int>* Resolve(MemberRef ref) const {
    return ref.result == Identity() && ref.member.ordinal < Data().scope.physical_members.size() ?
               &Data().scope.physical_members[ref.member.ordinal] :
               nullptr;
  }

 private:
  static FeatureReportData Validated(FeatureReportData data) {
    if (data.outcome == ReportOutcome::kNotCompleted) {
      throw std::invalid_argument("feature report: the construction slot was frozen without a completion");
    }
    size_t base_rows = 0;
    size_t bounded_rows = 0;
    for (const ReportCoverage& row : data.coverage) {
      switch (row.role) {
        case CoverageRole::kBaseInput:
          ++base_rows;
          break;
        case CoverageRole::kBoundedDiscovery:
          ++bounded_rows;
          break;
        case CoverageRole::kDeclaredSkip:
          break;
      }
      if (row.role != CoverageRole::kBoundedDiscovery && !row.limitations.empty()) {
        throw std::invalid_argument("feature report: only the bounded-discovery coverage row carries limitations");
      }
    }
    const bool unsupported = data.outcome == ReportOutcome::kUnsupportedMulticrystal;
    if (unsupported) {
      // The refusal arm publishes exactly its one refusal row; an empty coverage array would
      // silently drop the machine statement the wire contract has always carried.
      if (data.coverage.size() != 1 || data.coverage[0].role != CoverageRole::kBaseInput ||
          data.coverage[0].status != CoverageStatus::kNotSupported) {
        throw std::invalid_argument("feature report: the unsupported arm publishes exactly one refusal row");
      }
    } else if (base_rows != 1 || bounded_rows != 1) {
      throw std::invalid_argument("feature report: the coverage roles do not describe one completed run");
    } else {
      // The outcome and the bounded row's status are one machine statement: completed means the
      // search finished inside its account, partial means it did not.
      const auto bounded = std::find_if(data.coverage.begin(), data.coverage.end(), [](const ReportCoverage& row) {
        return row.role == CoverageRole::kBoundedDiscovery;
      });
      if (data.outcome == ReportOutcome::kCompleted && bounded->status != CoverageStatus::kSupported) {
        throw std::invalid_argument("feature report: a completed outcome carries a partial coverage row");
      }
      if (data.outcome == ReportOutcome::kPartial && bounded->status != CoverageStatus::kNumericalIncomplete) {
        throw std::invalid_argument("feature report: a partial outcome carries a complete coverage row");
      }
    }
    // The published associations are indexed by ordinal: every table the reader walks must agree
    // in length, and every ordinal must be in range, before the value can be frozen.
    if (data.annotations.size() != data.core.objects.size()) {
      throw std::invalid_argument("feature report: the annotations and objects tables disagree in length");
    }
    const std::vector<DiagnosticFeatureRecord>& features = data.mc.discovery.features;
    const std::vector<DiagnosticSourceIdentity>& sources = data.mc.discovery.measure.sources;
    for (const schema3::CorroborationAnnotation& annotation : data.annotations) {
      if (annotation.matched_record >= 0 && static_cast<size_t>(annotation.matched_record) >= features.size()) {
        throw std::invalid_argument("feature report: an annotation names a record outside the evidence");
      }
    }
    for (const DiagnosticFeatureRecord& feature : features) {
      if (feature.source_token && *feature.source_token >= sources.size()) {
        throw std::invalid_argument("feature report: a record names a source outside the measure");
      }
      if (feature.source_connected_feature && *feature.source_connected_feature >= features.size()) {
        throw std::invalid_argument("feature report: a record names a connected record outside the evidence");
      }
    }
    for (const schema3::StructureObjectRecord& object : data.core.objects) {
      if (object.u.size() % 3 != 0) {
        throw std::invalid_argument("feature report: a u preimage is not a whole number of triples");
      }
    }
    for (const schema3::UnattributedStructure& structure : data.unattributed.structures) {
      if (structure.record_index >= features.size()) {
        throw std::invalid_argument("feature report: an unattributed structure names a record outside the evidence");
      }
    }
    return data;
  }
};


}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_FEATURE_REPORT_RESULT_HPP_
