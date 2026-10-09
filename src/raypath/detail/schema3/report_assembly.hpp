#ifndef LUMICE_RAYPATH_DETAIL_SCHEMA3_REPORT_ASSEMBLY_HPP_
#define LUMICE_RAYPATH_DETAIL_SCHEMA3_REPORT_ASSEMBLY_HPP_

// schema3 report assembly (scrum 666.3, plan D1): wires the schema3 modules into the engine's
// production report path. Takes the assembled v2 product (the real discovery run over the real
// input) and produces the schema3 report's whole model side: the structural enumeration over
// every physical member, the demoted MC evidence, the two-way attribution (forward
// unattributed + backward corroboration) and the no_related_feature ruling — with the
// enumeration/attribution legs timed for the report's timing block.
//
// The declared-measure side follows the plan's wiring: the measure is built from the input
// snapshot's crystal axis against the sun at the scene's solar center; the density spec from
// the SAME axis. Two declared skips (coverage rows, not errors — fail-closed direction):
//   - MakeUMarginal's kDegenerateSunGeometry (sun at a pole): the measure side is skipped
//     whole — no restricted/orbit legs, kind-1 visibility unproven;
//   - ConvertAxisToPoseDensity's unsupported verdict: the density leg is skipped (no orbit
//     stream -> no restricted leg; kind-1 visibility falls back to the stream-less unproven).
//
// Precondition: `report` is a completed single-crystal assembly (!unsupported_multicrystal —
// that early path never ran the discovery this module reads; the serializer owns its v3 shape)
// with a non-empty representative spectrum and at least one assembled layer. The discovery is
// MOVED into the evidence carry: the v2 document path is gone, so nothing reads
// report.discovery past this call.

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "raypath/detail/path_feature_report.hpp"
#include "raypath/detail/schema3/mc_attribution.hpp"
#include "raypath/detail/schema3/mc_evidence.hpp"
#include "raypath/detail/schema3/no_related_feature.hpp"
#include "raypath/detail/schema3/structure_enumeration.hpp"

namespace lumice::raypath::schema3 {

// The enumeration leg's anti-hang cap (the one ENFORCED bound in the budget vocabulary). The
// quality budgets around it are report-only by A4's ruling; this one is a robustness floor
// against pathological inputs, not a quality knob, and is deliberately NOT coupled to the
// request's `--budget-ms` (which bounds the discovery legs' quality/cost trade). Measured
// basis: the heaviest legal corpus leg runs ~12 s; the C10 degenerate family, once its
// quadratic shift statistic is refused by the kernel's declared pair bound, enumerates its
// 12-member PBD orbit in ~60-90 s locally; CI runners measured up to 1.9x slower, so 300 s
// keeps >=1.6x headroom over that worst case and >=25x over any legal leg.
constexpr long long kEnumerationHangCapMs = 300000;

struct AssembledSchema3Report {
  Schema3DiscoveryCore core;                         // the corroboration-written core (DeriveCorroboration's copy)
  std::vector<CorroborationAnnotation> annotations;  // parallel to core.objects
  McEvidenceBlock mc;
  UnattributedOutcome unattributed;
  McAttributionCounts corroboration_counts;  // the backward pass's own attribution counts
  NoRelatedRuling ruling;
  double enumeration_seconds = 0;
  double attribution_seconds = 0;
  std::string measure_skip_note;  // non-empty = the measure side was declared skipped
  std::string density_skip_note;  // non-empty = the density leg was declared skipped
};

AssembledSchema3Report AssembleSchema3Report(
    PathFeatureReport&& report, uint64_t max_optical_evaluations,
    std::chrono::steady_clock::time_point enumeration_deadline = std::chrono::steady_clock::time_point::max());

}  // namespace lumice::raypath::schema3
#endif  // LUMICE_RAYPATH_DETAIL_SCHEMA3_REPORT_ASSEMBLY_HPP_
