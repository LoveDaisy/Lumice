#include "raypath/workspace.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "core/crystal.hpp"
#include "core/crystal_kind.hpp"
#include "raypath/detail/local_class.hpp"
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
  // An automatic candidate is an optimization, not caller input: a failed admission is a miss.
  // Use the same authority as explicit warm input; never duplicate its fingerprint/location gates.
  // The face still owns admission of explicit input and qualification of each numeric fact.
  if (warm_slot_.crystal_id != selection.crystal_id || warm_slot_.path_layers != selection.path_layers) {
    return false;
  }
  std::optional<AdmittedWarmSource> admitted;
  if (!ValidateWarmCandidate(snapshot_, selection, warm_slot_.candidate, &admitted).Ok()) {
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
  // A caller that resolved the query's deadline itself (the interpretive chain's shared window)
  // pins it here; the resolution stays this header's owner either way — this is only "don't
  // re-resolve an already-resolved window", never a second policy.
  if (!effective.query_deadline) {
    BudgetRequest deadline_request;
    deadline_request.budget_ms = request.budget_ms;
    deadline_request.interrupted = request.interrupted;
    effective.query_deadline = ResolveQueryDeadline(deadline_request);
  }
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

Error AnalysisWorkspace::QueryMap(const MapRequest& request, MapResult* out, const analytic::StopToken* cancel) {
  MapRequest effective = request;
  effective.cancel = cancel;
  BudgetRequest deadline_request;
  deadline_request.budget_ms = request.budget_ms;
  deadline_request.interrupted = request.interrupted;
  effective.query_deadline = ResolveQueryDeadline(deadline_request);
  const PreparedGeometry* geometry = GeometryFor(request);
  return AnalyzePathMap(snapshot_, effective, out, geometry);
}

Error AnalysisWorkspace::QueryRepresentativeRay(const RepresentativeRayRequest& request, RepresentativeRayResult* out) {
  const PreparedGeometry* geometry = GeometryFor(request);
  return AnalyzeRepresentativeRay(snapshot_, request, out, geometry);
}

Error AnalysisWorkspace::QueryLocalClasses(const LocalClassQuery& query, std::vector<LocalClass>* out) {
  const auto configured = snapshot_.crystals_.find(query.crystal_id);
  if (configured == snapshot_.crystals_.end()) {
    return { ErrorCode::kInvalidArgument, "local classes: unknown crystal id" };
  }
  if (query.representative.empty()) {
    return { ErrorCode::kInvalidPath, "local classes: empty representative" };
  }
  // Caller input is refused before any element algebra runs: ApplyToFace answers illegal face
  // numbers with an unrecoverable invariant (an abort, not an error), so the faces are checked
  // against the crystal kind's own legal set here, and the sky point against the query's
  // coordinate contract.
  const CrystalKind kind = std::holds_alternative<PrismCrystalParam>(configured->second.param_) ? CrystalKind::kPrism :
                                                                                                  CrystalKind::kPyramid;
  for (const int face : query.representative) {
    if (!IsLegalFace(kind, face)) {
      return { ErrorCode::kFaceNotInCrystal, "local classes: representative face " + std::to_string(face) +
                                                 " is not a legal face number of this crystal" };
    }
  }
  if (!std::isfinite(query.target_altitude_deg) || !std::isfinite(query.target_azimuth_deg) ||
      query.target_altitude_deg < -90.0 || query.target_altitude_deg > 90.0) {
    return { ErrorCode::kInvalidTarget,
             "local classes: the target altitude must be finite and in [-90, 90], the azimuth finite" };
  }
  // The row scope through the same resolution the member lists elsewhere carry: the expansion
  // runs under the physical gating derived from the snapshot's own crystal config, so the
  // members here and the members every other face sees cannot drift.
  PhysicalMemberRequest request;
  request.scene_identity = "workspace local classes";
  request.layer_index = 0;
  request.crystal = configured->second;
  request.representative = query.representative;
  request.symmetry_bits = query.symmetry_bits;
  request.semantics = SymmetrySemantics::kPhysical;
  PhysicalMemberScope row;
  if (Error error = ResolvePhysicalMemberScope(request, &row); !error.Ok()) {
    return error;
  }
  LocalClassRequest resolved;
  resolved.target_altitude_deg = query.target_altitude_deg;
  resolved.target_azimuth_deg = query.target_azimuth_deg;
  // The scene sun's azimuth is the snapshot's own fact (the same field the sun direction
  // consumers read); the caller restating it would be a second copy waiting to drift.
  resolved.sun_azimuth_deg = static_cast<double>(snapshot_.scene_.light_source_.param_.azimuth_);
  return ResolveLocalClasses(row, resolved, out);
}

namespace {

// One member's point evaluation on the existing point-intent chain: the deadline is preset
// (the chain resolved it once at entry — QueryPoint never re-resolves an already-resolved
// window), the cancel token rides through, and the workspace's automatic warm admission and
// the geometry slot are QueryPoint's own reads. The explicit candidate is routed: only the
// member it names receives it (the existing single admission authority keeps the strict
// contract at that member's query), so a candidate for a later class never blocks the
// automatic chain at an earlier one.
Error EvaluateMember(AnalysisWorkspace& workspace, const InterpretivePointQuery& query,
                     const std::vector<int>& member_faces, const QueryDeadline& deadline,
                     const analytic::StopToken* cancel, bool include_context, PointResult* out) {
  SinglePathRequest req;
  req.crystal_id = query.crystal_id;
  req.path_layers.emplace_back(member_faces.begin(), member_faces.end());
  req.target_altitude_deg = query.target_altitude_deg;
  req.target_azimuth_deg = query.target_azimuth_deg;
  req.budget_ms = query.budget_ms;
  req.interrupted = query.interrupted;
  req.sample_count = query.sample_count;
  req.sun_grid_lat_count = query.sun_grid_lat_count;
  req.query_deadline = deadline;
  if (query.warm_candidate.has_value() && query.warm_candidate->path_layers.size() == 1 &&
      std::equal(member_faces.begin(), member_faces.end(), query.warm_candidate->path_layers[0].begin(),
                 query.warm_candidate->path_layers[0].end())) {
    req.warm_candidate = query.warm_candidate;
  }
  if (!query.ratio_reference_faces.empty()) {
    req.ratio_reference_faces = query.ratio_reference_faces;
  }
  return workspace.QueryPoint(req, include_context, out, cancel);
}

// The carrying statement: the interpretive pose's declared-measure log density at or above
// kCarriesLightLogDensityFloor (local_class.hpp — the issue's sanctioned statement convention).
// It rules only the chain's selection; the class facts and the reported values never clamp.
bool IsCarryingCandidate(const PointResult& point) {
  const PointData& data = point.Data();
  if (data.OutcomeValue() != Outcome::kDiscovered || !data.Discovered().interpretive.available) {
    return false;
  }
  return data.Discovered().interpretive.log_density >= kCarriesLightLogDensityFloor;
}

}  // namespace

Error AnalysisWorkspace::QueryInterpretivePoint(const InterpretivePointQuery& query, bool include_context,
                                                InterpretivePointAnswer* out, const analytic::StopToken* cancel) {
  // The ONE window resolves at chain entry — before the class resolution — so the algebra's
  // own time spends the same deadline the member evaluations spend (a preset window is honored
  // verbatim; the resolution stays query_control's).
  BudgetRequest deadline_request;
  deadline_request.budget_ms = query.budget_ms;
  deadline_request.interrupted = query.interrupted;
  const QueryDeadline deadline = query.query_deadline ? *query.query_deadline : ResolveQueryDeadline(deadline_request);
  // The class facts — the same face, the same validation and the same resolver: the algebra is
  // not the member search's to invalidate.
  LocalClassQuery class_query;
  class_query.crystal_id = query.crystal_id;
  class_query.representative = query.representative;
  class_query.symmetry_bits = query.symmetry_bits;
  class_query.target_altitude_deg = query.target_altitude_deg;
  class_query.target_azimuth_deg = query.target_azimuth_deg;
  if (Error error = QueryLocalClasses(class_query, &out->classes); !error.Ok()) {
    return error;
  }
  // The explicit warm candidate names ONE member; this row must contain it, or the input is an
  // error (a member-addressed hint for a member the row does not carry — refused, never
  // silently ignored). Routing happens per evaluation inside EvaluateMember.
  if (query.warm_candidate.has_value()) {
    const auto& warm_layers = query.warm_candidate->path_layers;
    const bool names_row_member =
        warm_layers.size() == 1 &&
        std::any_of(out->classes.begin(), out->classes.end(), [&warm_layers](const LocalClass& klass) {
          return std::any_of(klass.member_sequences.begin(), klass.member_sequences.end(),
                             [&warm_layers](const std::vector<int>& member) { return member == warm_layers[0]; });
        });
    if (!names_row_member) {
      return { ErrorCode::kInvalidPath, "interpretive point: the warm candidate names a member outside this row" };
    }
  }

  if (out->classes.empty()) {
    // Defensive: a legal representative always yields at least itself in the scope; an empty
    // partition has nothing to resolve or evaluate, and an answer with no frozen payload would
    // break the answer's own self-containment.
    return { ErrorCode::kInvalidPath, "interpretive point: the row scope resolved to no members" };
  }

  if (!query.fixed_member.empty()) {
    // The fixed-member contract: the member must be one of this row's (the classes partition
    // it, so exactly one class holds it), evaluated verbatim — never auto-swapped.
    for (size_t c = 0; c < out->classes.size(); ++c) {
      for (size_t m = 0; m < out->classes[c].member_sequences.size(); ++m) {
        if (out->classes[c].member_sequences[m] != query.fixed_member) {
          continue;
        }
        PointResult point;
        if (Error error = EvaluateMember(*this, query, query.fixed_member, deadline, cancel, include_context, &point);
            !error.Ok()) {
          return error;
        }
        out->selected_class = static_cast<int>(c);
        out->selected_member = static_cast<int>(m);
        out->member_fixed = true;
        out->point = std::move(point);
        return {};
      }
    }
    return { ErrorCode::kInvalidPath, "interpretive point: the fixed member is not a member of this row" };
  }

  // Automatic selection: class representatives in the resolver's fixed order; the first whose
  // interpretive pose — the point face's own closed-form density argmax over the recovered
  // branches — reaches the carrying statement (log density at or above
  // kCarriesLightLogDensityFloor, the issue's sanctioned −300 nat statement convention) wins.
  // The line is how small "does not carry light here" is, spelled explicitly instead of the
  // machine's underflow deciding (a deep tail keeps its finite log; the convention only rules
  // the SELECTION, never the class facts and never the reported values). Classes after the
  // winner are not searched this run; a cancelled or deadline-stopped evaluation stops the
  // iteration with the classes standing, the anchor payload, and the honest search outcome.
  out->search_outcome = ExecutionOutcome::kCompleted;
  PointResult anchor;
  bool anchor_valid = false;
  for (size_t c = 0; c < out->classes.size(); ++c) {
    const std::vector<int>& representative = out->classes[c].representative;
    PointResult candidate;
    const Error error = EvaluateMember(*this, query, representative, deadline, cancel, include_context, &candidate);
    if (!error.Ok()) {
      return error;
    }
    if (c == 0) {
      anchor = std::move(candidate);
      anchor_valid = true;
    }
    const PointResult& evaluated = c == 0 ? anchor : candidate;
    const bool selected_here = IsCarryingCandidate(evaluated);
    if (cancel != nullptr && cancel->CancelRequested()) {
      out->search_outcome = ExecutionOutcome::kCancelled;
      out->point = std::move(anchor);
      return {};  // cancelled: the classes stand, the anchor's own identity, the honest state
    }
    if (evaluated.Data().meta.execution != ExecutionOutcome::kCompleted) {
      out->search_outcome = evaluated.Data().meta.execution;
      out->point = std::move(anchor);
      return {};  // the window (or a stop) ended the search: a later member never gains budget
    }
    if (selected_here) {
      out->point = std::move(c == 0 ? anchor : candidate);
      out->selected_class = static_cast<int>(c);
      // The representative carries itself: its member index inside the class.
      for (size_t m = 0; m < out->classes[c].member_indices.size(); ++m) {
        if (out->classes[c].member_sequences[m] == representative) {
          out->selected_member = static_cast<int>(m);
          break;
        }
      }
      return {};
    }
  }
  out->point = std::move(anchor);  // nothing selected: the FIRST class's anchor, honestly -1
  return {};
}

}  // namespace lumice::raypath
