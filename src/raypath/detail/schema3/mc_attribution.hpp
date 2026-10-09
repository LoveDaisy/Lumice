#ifndef LUMICE_RAYPATH_DETAIL_SCHEMA3_MC_ATTRIBUTION_HPP_
#define LUMICE_RAYPATH_DETAIL_SCHEMA3_MC_ATTRIBUTION_HPP_

// schema3 report core, the MC attribution core (scrum 666.2): the TWO-WAY detector between the
// structural objects and the demoted MC evidence. Owner ruling 2026-10-07 (conclusions section 1
// rulings 3/4): the MC side is corroboration + a two-way difference detector, never an oracle —
//   forward  significant MC structure -> attribution check (distance to the objects' traced
//            images, tolerance max(h, declared width)) -> `unattributed_structures`. The ESS
//            gate guards against false alarms: a starved MC has no standing to witness.
//   backward every object derives its corroboration state (the judgment table below), filling
//            the `unchecked` constant 666.1 shipped.
//
// The matching ruler has two declared layers (plan A2):
//   base    the deviation axis (delta = angle(sun, viewing direction)) — EXACT for an
//           axisymmetric measure (the critical set's sky image of a Haar/azimuth-uniform density
//           is the delta family of the critical values); an approximation for a general measure
//           (azimuth not discriminated — the limitation rides every annotation's ruler string).
//           D_P evaluation has ONE entry: DeviationField::SampleOptical -> RoutedDeviation (the
//           closure routing; values are not re-derived anywhere here).
//   enhanced the orbit-image layer — for a restricted object of a member whose orbit stream is
//           non-empty, the all-sky angular distance against the stream's valid outgoing
//           directions (world frame). It can only ADD a match (the layers combine by min), never
//           remove one.
//
// The ESS gate is two-caliber (plan A4 as sharpened by implementation evidence): the FORWARD
// gate and the `observed` arm read the RECORD's own minimum_effective_samples (v2's
// significance standing, Y-ESS); the backward not-observed arms read the REGIONAL PRESENCE
// ESS — the kernel-windowed Kish ESS ((sum k)^2 / sum k^2, k = exp(kappa (cos delta_distance -
// 1)), kappa = 1/h^2) of the MC components at the object's delta image, grouped by outer draw
// (one draw, one sampling act — the same grouping discipline as the kernel's own EffectiveCount).
// Presence deliberately ignores the row weights: the MC's qualification to witness a window
// comes from what it RECORDED there, not from how bright the rows are — the Y-ESS of a dark
// region is ~0 and would structurally kill the unlit arm. Honest boundary (measured, not
// assumed): the v2 measure builder drops zero-weight draws, so the components are the
// LIGHT-BEARING rows — presence is the standing of the MC's recorded observation, not of its
// raw sampling; a chain that never transmits into the window reads zero and lands the object
// in not_observed_insufficient_ess (the C-arm's measured shape), which is the fail-closed
// direction. (Any common scale cancels in the Kish ratio; the 2D kernel's normalization
// authority stays in analytic/path_feature_discovery.hpp — this is the delta-marginal form the
// base ruler itself declares.)
//
// The judgment table (plan A1 as refined by its plan review — the two Minor refinements are
// IN: `observed` requires the matched record's OWN ESS at/above the floor, and a match that
// falls below the floor is not silently dropped but falls through to the presence rows with a
// matched-but-below-threshold note; the unlit/unproven low-presence arm KEEPS
// not_observed_insufficient_ess — the living hunger case (666.2 Step 4-B) needs it — with the
// reason contract spelling the reading: the absence claim is corroborated only to the MC's
// sampled standing, and an insufficient standing leaves the corroboration itself undeclared):
//
//   1. existence != computed            -> unchecked        (object_not_computed)
//   2. no delta image                   -> unchecked        (the image's own reason)
//   3. match within tolerance AND the
//      matched record's ESS >= floor    -> observed
//   4. lit (certified | partial):
//        presence < floor               -> not_observed_insufficient_ess
//        presence >= floor              -> not_observed_despite_sufficient_ess  (the red flag)
//   5. unlit | unproven:
//        presence >= floor              -> consistent  (unproven: nothing to contradict)
//        presence < floor               -> not_observed_insufficient_ess
//
// Every `presence_ess` consulted lands in the annotation (the weight information survives the
// label choice). The declared parameters (min_ess, h, tolerance policy) travel with every
// result — a declared ruler, not a hidden constant.

#include <array>
#include <string>
#include <vector>

#include "raypath/detail/schema3/mc_evidence.hpp"
#include "raypath/detail/schema3/structure_enumeration.hpp"

namespace lumice::raypath::schema3 {

// The declared corroboration parameters (the chromatic-thresholds discipline: declared, not
// universal; they travel with the results that used them).
struct CorroborationThresholds {
  double min_ess = 32.0;  // the standing floor; the v2 required_ess starting point
};

// Cost accounting where the cost happens (the 666.1 discipline). The kernel-internal walk and
// Newton counts stay a registered gap (the G4 family).
struct McAttributionCounts {
  long long dp_evaluations = 0;        // SampleOptical chain evaluations for the delta images
  long long presence_ess_queries = 0;  // one per object whose presence ESS was consulted
};

// The geometry the delta evaluation runs against. `sun_dir` points AT the sun (world frame) —
// the delta axis's pole; the caller derives it from the evidence's own light configuration
// (the negation of the sun's incident propagation direction).
struct McAttributionContext {
  const analytic::FaceNormalTable* normals = nullptr;
  const analytic::FacePolygonTable* polygons = nullptr;
  double base_index = 0.0;
  double sun_dir[3] = { 0.0, 0.0, 1.0 };
};

// The whole derivation input. `h_rad` MUST equal the evidence's own observation bandwidth
// (mc->observation_options.bandwidth_rad) — the matching ruler is the observation the evidence
// was taken under; a mismatch is a fail-visible refusal, never a silent re-interpretation.
struct McAttributionInput {
  const Schema3DiscoveryCore* core = nullptr;
  const McEvidenceBlock* mc = nullptr;
  CorroborationThresholds thresholds;
  McAttributionContext geometry;
  double h_rad = 0.0;
};

// The object's delta image: the critical values the object claims on the deviation axis (the
// base matching ruler's support). kind-1 reads the member's support row (interval endpoints +
// endpoint onsets + constant curves — the critical STRUCTURE, not the smooth inside of a
// support interval); every other computed object with a u preimage samples RoutedDeviation per
// point. Anything else is unavailable (the reason rides).
struct ObjectDeltaImage {
  bool available = false;
  std::string unavailable_reason;
  std::vector<double> delta_rad;  // radians, ascending, deduped at the partition's kExtremumAtol
};

ObjectDeltaImage ObjectDeltaImageOf(const StructureObjectRecord& object, const Schema3DiscoveryCore& core,
                                    const McAttributionContext& context, McAttributionCounts* counts);

// The support-row / orbit-slot index of a member on the core (the parallel-invariant read
// entry): the first row whose face sequence equals `member`, or -1 when absent.
long long MemberIndexOf(const Schema3DiscoveryCore& core, const std::vector<int>& member);

// The object's declared width (plan A3): the dominant chromatic feature's spread when the
// chromatic leg ran, else 0. The dominant selection re-states the kernel's own rule (visible
// first, then the largest score, first maximal wins — dp_chromatic's Dominant) because the
// analytic header does not export it; REGISTERED GAP: the kernel should export the selection
// now that a second consumer exists — escalation recorded in the task progress, not patched in
// src/analytic here.
double DeclaredWidthOf(const StructureObjectRecord& object);

// The regional presence ESS of the evidence's components at the object's delta image (the A4
// caliber; see the module docstring). Empty image -> 0 (nothing to be present at).
double PresenceEss(const std::vector<analytic::WeightedSkySample>& components,
                   const std::vector<double>& image_delta_rad, const double sun_dir[3], double h_rad,
                   McAttributionCounts* counts);

// The per-object corroboration verdict, the judgment table's annotation (parallel to
// core.objects in DeriveCorroboration's outcome). NaN = the quantity was not consulted.
struct CorroborationAnnotation {
  CorroborationState state = CorroborationState::kUnchecked;
  double presence_ess = std::nan("");
  double match_distance = std::nan("");
  double tolerance_rad = std::nan("");
  long long matched_record = -1;  // index into the evidence's features; -1 = none
  double matched_record_ess = std::nan("");
  std::string ruler;   // the declared ruler + parameter snapshot
  std::string reason;  // the table arm's reading (the contract the review pinned)
};

// The backward pass: a COPY of the core with every object's `corroboration` field written, plus
// the per-object annotations. `error` non-empty = the fail-visible refusal (bad pointers, an h
// mismatch); the outputs are then empty and nothing was derived.
struct CorroborationOutcome {
  Schema3DiscoveryCore core;
  std::vector<CorroborationAnnotation> annotations;
  McAttributionCounts counts;
  std::string error;
};
CorroborationOutcome DeriveCorroboration(const McAttributionInput& input);

// The forward pass: significant MC structures (kActual records with a position and their own
// ESS at/above the floor — the A5 qualification and the AC2 gate) that NO object's image
// reaches within tolerance. `min_margin_rad` = min over objects of (distance - tolerance),
// positive by construction. The skipped counters keep the silences visible: a kActual record
// without sky points and one below the floor are NOT findings, and their counts say so.
struct UnattributedStructure {
  size_t record_index = 0;
  std::array<double, 3> position{};  // the record's sky point of closest approach
  double delta_rad = 0.0;            // that point's deviation
  double record_ess = 0.0;
  double min_margin_rad = 0.0;
  std::string ruler;
};
struct UnattributedOutcome {
  std::vector<UnattributedStructure> structures;
  long long skipped_no_position = 0;
  long long skipped_below_ess = 0;
  McAttributionCounts counts;
  std::string error;
};
UnattributedOutcome DeriveUnattributed(const McAttributionInput& input);

}  // namespace lumice::raypath::schema3

#endif  // LUMICE_RAYPATH_DETAIL_SCHEMA3_MC_ATTRIBUTION_HPP_
