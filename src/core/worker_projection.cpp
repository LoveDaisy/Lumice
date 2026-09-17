#include "core/worker_projection.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>

#include "core/anchor_buffer.hpp"
#include "core/color_util.hpp"
#include "core/lens_proj_build.hpp"
#include "core/scatter_accum.hpp"
#include "core/shared/projection_shared.h"

namespace lumice {

void BuildWorkerProjectionSidecars(SimData& sim_data, const std::vector<RenderConfig>& renders) {
  if (renders.empty()) {
    return;
  }
  const size_t n = sim_data.outgoing_w_.size();
  // The simulator's collection loop pushes d / w / component in lock-step for every outgoing
  // ray. The component list is carried into the sidecar only when it is present, so the
  // consumer's "no outgoing_component_ on this batch" branch keeps the same trigger it has on
  // its own loop (main_component_ empty ⇔ outgoing_component_ empty) rather than seeing a list
  // of zeros.
  assert(sim_data.outgoing_d_.size() == 3 * n);
  const bool has_component = sim_data.outgoing_component_.size() == n;
  assert(has_component || sim_data.outgoing_component_.empty());

  // Render sidecar: one list per renderer. MakeCameraRotation + BuildProjParams are
  // recomputed per batch per renderer, not cached: that is the same frequency the consumer
  // assembles ProjParams at, it is negligible next to the trace itself, and it means a batch
  // is always projected with the renders_ snapshot it was generated under — no cache to
  // invalidate when a commit changes a renderer.
  sim_data.projected_.resize(renders.size());
  for (size_t ri = 0; ri < renders.size(); ++ri) {
    const RenderConfig& cfg = renders[ri];
    const Rotation rot = MakeCameraRotation(cfg);
    const float short_pix = static_cast<float>(std::min(cfg.resolution_[0], cfg.resolution_[1]));
    const auto proj_params = BuildProjParams(cfg, rot, short_pix);
    const int w_res = cfg.resolution_[0];
    const int h_res = cfg.resolution_[1];
    auto& out = sim_data.projected_[ri];
    out.main_pixel_.reserve(n);
    out.main_w_.reserve(n);
    if (has_component) {
      out.main_component_.reserve(n);
    }
    for (size_t i = 0; i < n; ++i) {
      const float w = sim_data.outgoing_w_[i];
      const uint64_t component = has_component ? sim_data.outgoing_component_[i] : 0u;
      ProjectAndClassifyRay(proj_params, w_res, h_res, sim_data.outgoing_d_[i * 3 + 0], sim_data.outgoing_d_[i * 3 + 1],
                            sim_data.outgoing_d_[i * 3 + 2],
                            [&out, w, component, has_component](int pixel, bool is_main) {
                              if (is_main) {
                                out.main_pixel_.push_back(pixel);
                                out.main_w_.push_back(w);
                                out.landed_weight_ += w;
                                if (has_component) {
                                  out.main_component_.push_back(component);
                                }
                              } else {
                                out.overlap_pixel_.push_back(pixel);
                                out.overlap_w_.push_back(w);
                                if (has_component) {
                                  out.overlap_component_.push_back(component);
                                }
                              }
                            });
    }
  }

  // Anchor sidecar: AnchorConsumer::AccumulateOutgoing's own independent per-ray
  // lm_proj::ProjectExitToPixel loop (src/server/anchor_consumer.cpp) — the P99
  // sky-luminance anchor plane build for ev_mode=relative. Flat list (no main/overlap
  // split: that loop has none either — every hit of every ray accumulates into the plane
  // the same way), so ProjectAndClassifyRay's classification has nothing to offer here and
  // lm_proj::ProjectExitToPixel is called directly. Rays whose Y reads exactly 0 are
  // skipped, as that loop skips them.
  static const lm_proj::ProjParams kAnchorProjParams = BuildAnchorProjParams();
  sim_data.anchor_projected_pixel_.reserve(n);
  sim_data.anchor_projected_y_.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    const float y = SpectrumToYSingle(sim_data.curr_wl_, sim_data.outgoing_w_[i]);
    if (y == 0.0f) {
      continue;
    }
    const auto hit = lm_proj::ProjectExitToPixel(kAnchorProjParams, sim_data.outgoing_d_[i * 3 + 0],
                                                 sim_data.outgoing_d_[i * 3 + 1], sim_data.outgoing_d_[i * 3 + 2]);
    for (int k = 0; k < hit.count; ++k) {
      const int px = hit.hits[k].px;
      const int py = hit.hits[k].py;
      if (px < 0 || px >= kAnchorWidth || py < 0 || py >= kAnchorHeight) {
        continue;
      }
      sim_data.anchor_projected_pixel_.push_back(py * kAnchorWidth + px);
      sim_data.anchor_projected_y_.push_back(y);
    }
  }
}

}  // namespace lumice
