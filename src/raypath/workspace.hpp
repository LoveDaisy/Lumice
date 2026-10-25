#ifndef LUMICE_RAYPATH_WORKSPACE_HPP_
#define LUMICE_RAYPATH_WORKSPACE_HPP_

// AnalysisWorkspace (the typed-architecture design's data-and-reuse owner): an exclusive typed
// scene snapshot plus the lazy, bounded reuse state the four query faces read. Creation does no
// structure work and builds no face tables; the first query touching a selection builds that
// selection's geometry once, and a requery reuses it. The reuse slots are bounded BY
// CONSTRUCTION to the current selection/reference structure: exactly one geometry slot, one
// warm slot and one cached survey result, each REPLACED on a scope switch — a workspace never
// accumulates the mouse history of past selections.
//
// Ownership and lifetime: the snapshot is written exactly once, at creation, by the workspace
// itself (the single writer — the write-point census lives in workspace.cpp); queries read it
// by const reference and never see a live caller scene. Results are the faces' frozen values —
// they own their storage, and a returned result stays readable and serializable after the
// caller's config, the scene and this workspace are all gone. The warm slot holds the typed
// harvest by value (HarvestSurveyResult — no JSON anywhere on this chain), and a consumer can
// re-extract warm from any returned survey result at any time.
//
// Concurrency: a workspace instance is NOT thread-safe — its slots and counters are plain
// members. One instance serves one calling thread at a time (the issue's boundary: queries run
// on the caller's thread); cross-workspace parallelism and read-only sharing of returned
// results are the supported shapes. The cancel token is the one deliberately thread-safe piece.
//
// The reuse criterion is not the workspace's to invent: the slot is addressed by the request's
// selection (crystal + path class — storage addressing, not a judgment), and every reuse
// decision runs through the existing single authority (ValidateWarmCandidate's admission, then
// EvaluateSurveyReuse's per-fact dependency evaluation — 738/739.1's criterion). A cancelled or
// cut survey never fills the slot: only a run the workspace saw complete does.

#include <memory>

#include "analytic/stop.hpp"
#include "config/config_manager.hpp"
#include "raypath/path_feature_report.hpp"
#include "raypath/prepared_geometry.hpp"
#include "raypath/probe_analysis.hpp"
#include "raypath/single_path_analysis.hpp"
#include "raypath/survey_report.hpp"

namespace lumice::raypath {

class AnalysisWorkspace {
 public:
  // Captures the snapshot (one copy; the caller's config is not referenced afterwards) and
  // nothing else — no structure runs, no tables build.
  static Error Create(const ConfigManager& config, std::unique_ptr<AnalysisWorkspace>* out);

  const ConfigManager& Scene() const { return snapshot_; }

  // The four faces, on the snapshot. A point/probe query whose selection matches the warm
  // slot's carries the slot as its warm candidate (the typed chain — no JSON); the faces' own
  // admission and per-fact evaluation stay the reuse authority. `cancel` (optional, every
  // face) is the query's one-way token — the same single stop authority the faces poll.
  Error QueryPoint(const SinglePathRequest& request, bool include_context, PointResult* out,
                   const analytic::StopToken* cancel = nullptr);
  Error QueryProbe(const ProbeRequest& request, ProbeResult* out, const analytic::StopToken* cancel = nullptr);
  Error QuerySurvey(const SurveyRequest& request, survey::SurveyResult* out,
                    const analytic::StopToken* cancel = nullptr);
  Error QueryReport(const PathFeatureReportRequest& request, FeatureReportResult* out);

  // ---- the reuse reads (the workspace's own decision ledger, for the acceptance tests) ----
  long GeometryBuildCount() const { return geometry_builds_; }  // face-table builds performed
  long StructureRunCount() const { return structure_runs_; }    // structure-bearing queries that ran one
  long OccupiedSlotCount() const;                               // filled slots (bounded: ≤ 3 by construction)

 private:
  AnalysisWorkspace() = default;
  const PreparedGeometry* GeometryFor(const PathSelection& selection);  // builds/replaces the slot
  bool WarmFor(const PathSelection& selection, WarmCandidate* out) const;

  ConfigManager snapshot_;  // written once, in Create (the single writer)

  // The bounded reuse slots — each holds at most the CURRENT selection's entry.
  struct GeometrySlot {
    long long crystal_id = -1;
    PreparedGeometry geometry;
  } geometry_slot_;
  struct WarmSlot {
    long long crystal_id = -1;
    std::vector<std::vector<int>> path_layers;
    WarmCandidate candidate;
  } warm_slot_;
  struct SurveySlot {
    long long crystal_id = -1;
    std::vector<std::vector<int>> path_layers;
    unsigned symmetry_bits = 0;
    double wavelength_nm = 0.0;
    // The request fields OUTSIDE the selection that change the document's content: the ESS
    // evidence block exists only when asked (and its guardrail reads the budget), the budget
    // is echoed verbatim, and the scene layer decides the scope the document carries. A key
    // without them would silently serve one request's document to a differently-shaped one.
    int mc_ess_draws = 0;
    int budget_ms = 0;
    long long scene_layer = -2;  // -2 = unset (the request's nullopt), -1 = standalone
    survey::SurveyResult result;
  } survey_slot_;

  long geometry_builds_ = 0;
  long structure_runs_ = 0;
};

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_WORKSPACE_HPP_
