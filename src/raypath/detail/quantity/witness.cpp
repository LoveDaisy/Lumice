#include "raypath/detail/quantity/witness.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "analytic/diagnostic_batch.hpp"
#include "core/math.hpp"  // math::kRadToDegreeD — the deviation's unit conversion
#include "util/logger.hpp"

namespace lumice::raypath::survey {
namespace {

constexpr const char* kWitnessRefusalMode = "no_declared_source_witness";

// The declared source, spelled inside the document (the provenance the reader needs): what was
// sampled, which event became the witness, and what the two positive readings each are.
constexpr const char* kWitnessSourceNote =
    "declared source: the antipodal Fibonacci lattice over SO(3) (the declared measure's own "
    "sampler); witness = the max-weight kept event's pose; kept_count is an existence count, "
    "weight the lattice's w = A*T, entry and interface_product the diagnostic batch's own "
    "re-evaluation at that pose";

}  // namespace

WitnessResult EvaluateWitness(const WitnessInput& input) {
  WitnessResult witness;
  std::vector<int> slots;
  if (const Error slot_error = ResolveSingleLayerPath({ *input.representative }, *input.table, &slots);
      !slot_error.Ok()) {
    // Unreachable past the assembly's request-level resolution, but the refusal stays honest —
    // the same discipline as the sibling legs (the tint's member_resolution mode).
    witness.state = EvidenceState::kRefused;
    witness.mode = "member_resolution";
    return witness;
  }

  // The declared source's own existence sample: every kept event has w = A*T > 0. One
  // streaming pass; the entry measure is evaluated only for kept events.
  analytic::IceDiscovery discovery(*input.table, *input.polygons, slots.data(), static_cast<int>(slots.size()),
                                   input.refractive_index, input.incident);
  const std::vector<analytic::SampleEvent> events = discovery.BuildEvents(input.sample_count);
  if (events.empty()) {
    witness.state = EvidenceState::kRefused;
    witness.mode = kWitnessRefusalMode;
    return witness;
  }
  const analytic::SampleEvent& best = *std::max_element(
      events.begin(), events.end(),
      [](const analytic::SampleEvent& a, const analytic::SampleEvent& b) { return a.weight < b.weight; });

  // The event's candidate pose (the declared source draws this pose), then the independent
  // forward-chain read at that pose: the batch re-derives path validity, the entry measure and
  // the interface product through a different code path than the lattice's own.
  double pose[9];
  analytic::CandidatePose(input.incident, best.event.phi, best.event, pose);
  analytic::DiagnosticInputRow row;
  row.crystal = *input.crystal;
  for (int k = 0; k < 9; k++) {
    row.pose[static_cast<size_t>(k)] = pose[k];
  }
  for (int k = 0; k < 3; k++) {
    row.incident[static_cast<size_t>(k)] = input.incident[k];
  }
  row.refractive_index = input.refractive_index;
  row.source_token = 0;
  std::vector<analytic::DiagnosticOutputRow> values;
  if (!analytic::EvaluateDiagnosticBatch(*input.representative, { row }, {}, &values) || values.size() != 1 ||
      !values[0].path_valid || !values[0].entry_available || !(values[0].entry.value > 0.0) ||
      !(values[0].interface_product > 0.0)) {
    witness.state = EvidenceState::kRefused;
    witness.mode = kWitnessRefusalMode;
    return witness;
  }

  witness.state = EvidenceState::kFinite;
  witness.coverage.members_evaluated = 1;  // the representative; the class reads xN
  witness.coverage.geometry_basis =
      "the declared-source lattice argmax pose, re-evaluated by the diagnostic batch on the "
      "nominal crystal shape";
  witness.coverage.wavelength_nm = input.wavelength_nm;
  witness.kept_count = static_cast<long long>(events.size());
  witness.sample_count = input.sample_count;
  witness.lattice = "antipodal_fibonacci";
  witness.deterministic = true;
  witness.deviation_deg = best.event.deviation * math::kRadToDegreeD;
  witness.weight = best.weight;
  witness.entry = values[0].entry.value;
  witness.interface_product = values[0].interface_product;
  witness.source_note = kWitnessSourceNote;
  // The argmax pose is the one the assembly actually held: retain it for the document's
  // warm_index (the leg name is "witness_argmax"; the collector reads it from here).
  witness.has_pose = true;
  for (int k = 0; k < 9; k++) {
    witness.pose[k] = pose[k];
  }

  ILOG_INFO(GetGlobalLogger(), "[raypath survey] witness: {} kept of {} lattice events, max w {:.6e}, entry {:.6e}",
            events.size(), input.sample_count, best.weight, values[0].entry.value);
  return witness;
}

}  // namespace lumice::raypath::survey
