#include "raypath/workspace.hpp"

#include <chrono>

#include "raypath/detail/survey/survey_assembly.hpp"
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
  const bool warm_hit = WarmFor(request, &effective.warm_candidate.emplace());
  if (!warm_hit) {
    effective.warm_candidate.reset();
  }
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
  const bool warm_hit = WarmFor(request, &effective.warm_candidate.emplace());
  if (!warm_hit) {
    effective.warm_candidate.reset();
  }
  const PreparedGeometry* geometry = GeometryFor(request);
  if (!warm_hit) {
    ++structure_runs_;
  }
  return AnalyzePathProbe(snapshot_, effective, out, geometry);
}

Error AnalysisWorkspace::QuerySurvey(const SurveyRequest& request, survey::SurveyResult* out,
                                     const analytic::StopToken* cancel) {
  const bool cached = survey_slot_.crystal_id == request.crystal_id &&
                      survey_slot_.path_layers == request.path_layers &&
                      survey_slot_.symmetry_bits == request.symmetry_bits &&
                      survey_slot_.wavelength_nm == request.wavelength_nm.value_or(0.0);
  if (cached) {
    *out = survey_slot_.result;  // the frozen value's shared storage: zero recomputation
    return {};
  }
  const PreparedGeometry* geometry = GeometryFor(request);
  survey::SurveyAssemblyOptions options;
  options.geometry = geometry;
  const analytic::DiscoveryStop assembly_stop{ std::chrono::steady_clock::time_point::max(), nullptr,
                                               cancel };  // cancel-only: the legs stay unenforced
  options.stop = cancel != nullptr ? &assembly_stop : nullptr;
  const Error error = survey::AssembleSurvey(snapshot_, request, out, options);
  if (!error.Ok()) {
    return error;
  }
  ++structure_runs_;
  // Only a run the workspace saw complete fills the slots: a cancelled or cut survey never
  // masquerades as a cached block (its own refusal/truncation markers say what ran).
  const survey::SurveyData& data = out->Data();
  const bool complete = (data.evidence.state == survey::EvidenceState::kCertified ||
                         data.evidence.state == survey::EvidenceState::kFinite) &&
                        !(cancel != nullptr && cancel->CancelRequested());
  if (complete) {
    survey_slot_.crystal_id = request.crystal_id;
    survey_slot_.path_layers = request.path_layers;
    survey_slot_.symmetry_bits = request.symmetry_bits;
    survey_slot_.wavelength_nm = request.wavelength_nm.value_or(0.0);
    survey_slot_.result = *out;
    warm_slot_.crystal_id = request.crystal_id;
    warm_slot_.path_layers = request.path_layers;
    warm_slot_.candidate = HarvestSurveyResult(*out);
  }
  return {};
}

Error AnalysisWorkspace::QueryReport(const PathFeatureReportRequest& request, FeatureReportResult* out) {
  // The report consumes no warm candidate (its evidence is its own); it runs on the snapshot
  // through the same single entry. Its geometry lives in its own input assembly (the diagnostic
  // sampler's capture), which the report face owns.
  return AnalyzePathFeatureReportValue(snapshot_, request, out);
}

}  // namespace lumice::raypath
