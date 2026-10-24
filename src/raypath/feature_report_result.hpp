#ifndef LUMICE_RAYPATH_FEATURE_REPORT_RESULT_HPP_
#define LUMICE_RAYPATH_FEATURE_REPORT_RESULT_HPP_

#include <algorithm>

#include "raypath/domain/result_identity.hpp"
#include "raypath/results/report_attribution.hpp"
#include "raypath/results/report_enumeration.hpp"
#include "raypath/results/report_records.hpp"
#include "raypath/results/report_scope.hpp"

namespace lumice::raypath {

enum class ReportOutcome { kCompleted, kPartial, kNoRelatedFeature, kUnsupportedMulticrystal };
enum class CoverageStatus { kSupported, kNumericalIncomplete, kNotSupported };
struct ReportCoverage {
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
  ReportOutcome outcome = ReportOutcome::kCompleted;
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
  using FrozenValue<FeatureReportData>::FrozenValue;
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
};

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_FEATURE_REPORT_RESULT_HPP_
