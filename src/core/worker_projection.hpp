#ifndef CORE_WORKER_PROJECTION_H_
#define CORE_WORKER_PROJECTION_H_

#include <vector>

#include "config/render_config.hpp"
#include "config/sim_data.hpp"

namespace lumice {

// Legacy-CPU worker-side projection. Fills the two sidecars a legacy-CPU batch carries so the
// consumers can skip their own per-ray projection loops:
//
//   * SimData::projected_ — one ProjectedRayList per entry of `renders`, in that order (the
//     SessionSpec::renders order RenderConsumer::renderer_index_ indexes), each ray classified
//     by ProjectAndClassifyRay — the same function RenderConsumer::Consume's own loop runs —
//     into main (bump_landed, counted in landed_weight_) vs dual-fisheye overlap hits.
//   * SimData::anchor_projected_pixel_ / anchor_projected_y_ — the flat full-sky list
//     AnchorConsumer::Consume accumulates instead of running AccumulateOutgoing's loop.
//     Built with the batch wavelength (sim_data.curr_wl_), which is what AnchorConsumer's own
//     loop falls back to on this route (the legacy CPU path never sets outgoing_wl_).
//
// A no-op when `renders` is empty — an analysis session's batches carry no renderer — which
// leaves both sidecars empty and both consumers on their own loops. Requires the batch's
// outgoing_d_ / outgoing_w_ to be the lock-step parallel arrays the simulator's collection
// loop produces; outgoing_component_ is either lock-step too or absent, and the per-ring
// component lists are filled only in the former case, so the consumer's "no components on
// this batch" branch keeps its trigger.
//
// Called by Simulator::SimulateOneWavelength on the worker thread right after the outgoing
// arrays are published into the batch; a free function so a test can hand it a hand-built
// batch and compare the consumers' short-circuit output against their own loops directly.
void BuildWorkerProjectionSidecars(SimData& sim_data, const std::vector<RenderConfig>& renders);

}  // namespace lumice

#endif  // CORE_WORKER_PROJECTION_H_
