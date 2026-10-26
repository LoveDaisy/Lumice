#include "raypath/workspace.hpp"

#include <chrono>

#include "raypath/detail/survey/survey_assembly.hpp"
#include "raypath/query_control.hpp"
#include "raypath/survey_reuse.hpp"

namespace lumice::raypath {

// The snapshot's write-point census (the single-writer proof's ledger): EVERY write to
// snapshot_ in this translation unit is the one copy-assignment in Create. Queries take it by
// const reference (the faces' signatures take const ConfigManager&), and no other path in this
// file names snapshot_ as anything but a const read. One write point, one writer, closed list.

Error AnalysisWorkspace::Create(const ConfigManager& config, std::unique_ptr<AnalysisWorkspace>* out) {
  if (out == nullptr) {
    return { ErrorCode::kInvalidArgument, "null workspace output" };
  }
  // Direct new: the private constructor is this static entry's own to call.
  std::unique_ptr<AnalysisWorkspace> workspace(new AnalysisWorkspace());
  workspace->snapshot_ = config;  // THE single write: the caller's config, copied once
  *out = std::move(workspace);
  return {};
}

const PreparedGeometry* AnalysisWorkspace::GeometryFor(const PathSelection& selection) {
  if (geometry_slot_.crystal_id != selection.crystal_id) {
    const auto configured = snapshot_.crystals_.find(selection.crystal_id);
    if (configured == snapshot_.crystals_.end()) {
      return nullptr;  // the face reports the unknown-crystal refusal in its own vocabulary
    }
    PreparedGeometry built;
    if (!PrepareGeometry(configured->second, &built).Ok()) {
      return nullptr;  // ditto: crystal_rejected with the kernel's own reason
    }
    geometry_slot_.crystal_id = selection.crystal_id;
    geometry_slot_.geometry = std::move(built);  // the slot REPLACES: no per-selection growth
    ++geometry_builds_;
  }
  return &geometry_slot_.geometry;
}

bool AnalysisWorkspace::WarmFor(const PathSelection& selection, WarmCandidate* out) const {
  // Storage addressing only: the slot answers when the request's selection is the one that
  // filled it. Whether any fact is reusable stays the existing criterion's call (admission in
  // the face, then per-fact evaluation) — this file judges nothing.
  if (warm_slot_.crystal_id != selection.crystal_id || warm_slot_.path_layers != selection.path_layers) {
    return false;
  }
  *out = warm_slot_.candidate;
  return true;
}

long AnalysisWorkspace::OccupiedSlotCount() const {
  long occupied = 0;
  occupied += geometry_slot_.crystal_id >= 0 ? 1 : 0;
  occupied += warm_slot_.crystal_id >= 0 ? 1 : 0;
  occupied += survey_slot_.crystal_id >= 0 ? 1 : 0;
  return occupied;
}

Error AnalysisWorkspace::QueryPoint(const SinglePathRequest& request, bool include_context, PointResult* out,
                                    const analytic::StopToken* cancel) {
  SinglePathRequest effective = request;
  effective.cancel = cancel;
  BudgetRequest deadline_request;
  deadline_request.budget_ms = request.budget_ms;
  deadline_request.interrupted = request.interrupted;
  effective.query_deadline = ResolveQueryDeadline(deadline_request);
  if (!effective.warm_candidate) {
    WarmCandidate cached;
    if (WarmFor(request, &cached)) {
      effective.warm_candidate = std::move(cached);
    }
  }
  const bool warm_hit = effective.warm_candidate.has_value();
  const PreparedGeometry* geometry = GeometryFor(request);
  // The ledger counts the workspace's no-warm decisions — the queries that had to run a
  // structure. A fed slot is zero by this read; the behavioral proof that the context really
  // came from the warm facts is the result's own context.source == kWarm.
  if (!warm_hit) {
    ++structure_runs_;
  }
  return AnalyzePathPointValue(snapshot_, effective, include_context, out, geometry);
}

Error AnalysisWorkspace::QueryProbe(const ProbeRequest& request, ProbeResult* out, const analytic::StopToken* cancel) {
  ProbeRequest effective = request;
  effective.cancel = cancel;
  BudgetRequest deadline_request;
  deadline_request.budget_ms = request.budget_ms;
  deadline_request.interrupted = request.interrupted;
  effective.query_deadline = ResolveQueryDeadline(deadline_request);
  if (!effective.warm_candidate) {
    WarmCandidate cached;
    if (WarmFor(request, &cached)) {
      effective.warm_candidate = std::move(cached);
    }
  }
  const bool warm_hit = effective.warm_candidate.has_value();
  const PreparedGeometry* geometry = GeometryFor(request);
  if (!warm_hit) {
    ++structure_runs_;
  }
  return AnalyzePathProbe(snapshot_, effective, out, geometry);
}

Error AnalysisWorkspace::QuerySurvey(const SurveyRequest& request, survey::SurveyResult* out,
                                     const analytic::StopToken* cancel) {
  SurveyRequest effective = request;
  BudgetRequest deadline_request;
  deadline_request.budget_ms = request.budget_ms;
  effective.query_deadline = ResolveQueryDeadline(deadline_request);
  const analytic::DiscoveryStop query_stop{ effective.query_deadline->at, nullptr, cancel };
  const long long layer = request.scene_layer.has_value() ? static_cast<long long>(*request.scene_layer) :
                                                            -2LL;  // unset (the standalone shape writes -1)
  const bool cached = survey_slot_.crystal_id == request.crystal_id &&
                      survey_slot_.path_layers == request.path_layers &&
                      survey_slot_.symmetry_bits == request.symmetry_bits &&
                      survey_slot_.wavelength_nm == request.wavelength_nm.value_or(0.0) &&
                      survey_slot_.mc_ess_draws == request.mc_ess_draws &&
                      survey_slot_.budget_ms == request.budget_ms && survey_slot_.scene_layer == layer;
  if (cached && !query_stop.Stopped()) {
    *out = survey_slot_.result;  // the frozen value's shared storage: zero recomputation
    return {};
  }
  const PreparedGeometry* geometry = GeometryFor(request);
  survey::SurveyAssemblyOptions options;
  options.geometry = geometry;
  options.stop = &query_stop;
  const Error error = survey::AssembleSurvey(snapshot_, effective, out, options);
  if (!error.Ok()) {
    return error;
  }
  ++structure_runs_;
  // Only a run the workspace saw complete fills the slots: a cancelled or cut survey never
  // masquerades as a cached block (its own refusal/truncation markers say what ran).
  const survey::SurveyData& data = out->Data();
  // A requested-but-guardrail-cut ESS leg is a truncated document even when the block-level
  // refusal left the document-level state finite: it never enters the slot (a later, larger
  // request must not reuse the cut block as complete).
  const bool ess_complete =
      request.mc_ess_draws <= 0 ||
      (data.mc_region_ess.has_value() && data.mc_region_ess->State() == survey::EvidenceState::kFinite);
  const bool complete = (data.evidence.state == survey::EvidenceState::kCertified ||
                         data.evidence.state == survey::EvidenceState::kFinite) &&
                        ess_complete && !query_stop.Stopped();
  if (complete) {
    survey_slot_.crystal_id = request.crystal_id;
    survey_slot_.path_layers = request.path_layers;
    survey_slot_.symmetry_bits = request.symmetry_bits;
    survey_slot_.wavelength_nm = request.wavelength_nm.value_or(0.0);
    survey_slot_.mc_ess_draws = request.mc_ess_draws;
    survey_slot_.budget_ms = request.budget_ms;
    survey_slot_.scene_layer = layer;
    survey_slot_.result = *out;
    warm_slot_.crystal_id = request.crystal_id;
    warm_slot_.path_layers = request.path_layers;
    warm_slot_.candidate = HarvestSurveyResult(*out);
  }
  return {};
}

Error AnalysisWorkspace::QueryReport(const PathFeatureReportRequest& request, FeatureReportResult* out,
                                     const analytic::StopToken* cancel) {
  // The report consumes no warm candidate (its evidence is its own); it runs on the snapshot
  // through the same single entry, with the query's token folded into both of its stop
  // authorities (the discovery quality window and the enumeration cap). Its geometry lives in
  // its own input assembly (the diagnostic sampler's capture), which the report face owns.
  PathFeatureReportRequest effective = request;
  effective.cancel = cancel;
  BudgetRequest deadline_request;
  deadline_request.budget_ms = request.budget_ms;
  deadline_request.absolute_deadline = request.deadline;
  effective.query_deadline = ResolveQueryDeadline(deadline_request);
  return AnalyzePathFeatureReportValue(snapshot_, effective, out);
}

}  // namespace lumice::raypath
