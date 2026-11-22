#ifndef LUMICE_GUI_PREVIEW_DOWNGRADE_HPP
#define LUMICE_GUI_PREVIEW_DOWNGRADE_HPP

// The prefiltered downgrade arithmetic behind the 814 recovery ladder — deliberately free of GL,
// GuiState and ImGui so the filter itself is assertable headlessly (the ladder's DECISION side is
// app.cpp's; this header owns the pure pixel math every attempt executes).
//
// What a downgrade IS, and the two laws it must obey (plan 814 §3.2/§3.3):
//
// 1. LOW-PASS BEFORE RESAMPLE, IN THE LINEAR SOURCE DOMAIN. A prefiltered all-sky frame answers
//    "what radiance did this coarser texel integrate", and the honest answer is the average of the
//    finer texels it covers — computed on the frame's own linear-XYZ floats, BEFORE the float16
//    quantization, not on quantized halves and not in a display space. Averaging (not summing)
//    keeps every texel's unit: a constant-radiance frame downgrades to the same constant, so the
//    relative-mode picture is gain-free, and the absolute mode — which divides by the ORIGINAL
//    source pixel count, not the display's — stays correct exactly because the texels still carry
//    per-original-texel radiance. (Summing here would present a 4x brighter frame; dividing by the
//    displayed pixel count there would present the absolute EV 2 stops dark. Both wrong answers are
//    pinned against by unit test.)
//
// 2. EVERY CANDIDATE IS DERIVED FROM THE ORIGINAL FRAME, BY STEP COUNT, NOT BY REPEATED
//    DOWNGRADING OF PREVIOUS CANDIDATES AS THE USER RETRIES. Sequential 2x2 steps applied k times
//    to the source produce exactly the k-step box average; re-running a step over an already
//    downgraded buffer would compound blur and, worse, make the result depend on how many failed
//    attempts came before. DowngradeXyzPlaneBounded takes `steps` and always reads the caller's
//    original plane.
//
// The dual-disc layout survives untouched: each 2x2 step halves both axes, so the two discs stay
// proportional, the packing's corner padding stays padding, and the shader's dual-fisheye inverse
// needs no knowledge that it is sampling a smaller disc pair. The step floor exists so the ladder
// terminates: below kMinDowngradedWidth x kMinDowngradedHeight a dual-disc pair no longer carries
// a meaningful projection, and a candidate that small has stopped being a recovery.

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <vector>

#include "gui/xyz_half_codec.hpp"

namespace lumice::gui {

// The smallest dual-disc frame the ladder still offers as a candidate. Two discs of 64x64 each
// keep every lens projection a resolvable image (the status-quo shader projects down to ~1 px per
// degree at that size) while bounding the ladder at 7 steps from an 8192x4096 source: finite by
// construction, per plan 814 §3.2.
inline constexpr int kMinDowngradedWidth = 128;
inline constexpr int kMinDowngradedHeight = 64;

// Whether one more 2x2 step keeps both axes at or above the floor. The ladder stops here; a frame
// that cannot take another step is the last candidate, and exhausting it ends the cycle.
inline bool CanDowngradeOnce(int w, int h) {
  return w % 2 == 0 && h % 2 == 0 && w / 2 >= kMinDowngradedWidth && h / 2 >= kMinDowngradedHeight;
}

// Downgraded size after `steps` 2x2 steps from a (w, h) frame; false once any step would cross
// the floor. steps == 0 is the frame itself (a candidate, not a downgrade).
inline bool DowngradedSize(int w, int h, int steps, int* out_w, int* out_h) {
  if (steps < 0 || out_w == nullptr || out_h == nullptr) {
    return false;
  }
  int cw = w;
  int ch = h;
  for (int i = 0; i < steps; ++i) {
    if (!CanDowngradeOnce(cw, ch)) {
      return false;
    }
    cw /= 2;
    ch /= 2;
  }
  *out_w = cw;
  *out_h = ch;
  return true;
}

// Test-only allocation-fault injection (gui/unit tests; never set in production): while the
// counter is above zero, DowngradeXyzPlaneBounded throws std::bad_alloc from its own buffer
// allocations, so a witness can drive the ladder's CPU-allocation failure contract without
// exhausting real memory. Each throw consumes one count. An inline variable + setter: the
// header-inline function consults it directly, no second definition site.
inline std::atomic<int> g_downgrade_alloc_fault_for_test{ 0 };
inline void SetDowngradeAllocFaultForTest(int remaining_failures) {
  g_downgrade_alloc_fault_for_test.store(remaining_failures);
}

// ONE candidate's 2^steps box average over the CANONICAL-half input, computed directly from the
// original plane. The working set is the destination plane plus ONE source row (canonical float +
// half bits) — bounded no matter how large the source, which is the point: the stack this
// replaces (a full-size canonical float plane, a full-size half plane inside the canonicalizer,
// and a quarter-size scratch) peaked around 768 MiB of simultaneous allocations on an 8192x4096
// source, on exactly the constrained machines the ladder serves, and every one of those
// allocations threw std::bad_alloc straight past the failure contract.
//
// Same canonical values as the old canonicalize-then-step formulation: the plane-wide scale scan
// is a read-only pass (ComputeXyzHalfScale), and each element is quantize-then-dequantize
// through THE one codec the .lmc writer shares, applied to one row slice at a time — identical
// per-element results. Same average-not-sum semantics. The block is accumulated in one pass, so
// the float association differs from the nested (a+b+c+d)/4 form; the laws pin the value class
// (constant in, constant out; average; canonical inputs; deterministic per step count), not the
// association, and the unit tests hold both formulations' propositions against THIS function.
//
// Returns false only when `steps` crosses the floor (the caller's cycle already probes this —
// the false return is a defensive same-shape answer, not a second termination authority).
// Allocates its three buffers itself and lets std::bad_alloc propagate: the CALLER owns the
// failure contract (app.cpp's ladder converts it into a reported upload failure), which is what
// keeps an exhausted machine inside the latch-and-unavailable state instead of past it.
inline bool DowngradeXyzPlaneBounded(const float* src, int w, int h, int steps, int* out_w, int* out_h,
                                     std::vector<float>& dst) {
  if (steps < 0 || out_w == nullptr || out_h == nullptr) {
    return false;
  }
  int dw = 0;
  int dh = 0;
  if (!DowngradedSize(w, h, steps, &dw, &dh)) {
    return false;
  }
  const int block = 1 << steps;
  const size_t row_floats = static_cast<size_t>(w) * 3;
  const float scale = ComputeXyzHalfScale(src, static_cast<size_t>(w) * h * 3);
  if (g_downgrade_alloc_fault_for_test.fetch_sub(1) > 0) {
    throw std::bad_alloc();
  }
  dst.assign(static_cast<size_t>(dw) * dh * 3, 0.0f);
  std::vector<float> canon_row(row_floats);
  std::vector<uint16_t> half_row(row_floats);
  const float inv = 1.0f / static_cast<float>(block * block);
  for (int oy = 0; oy < dh; ++oy) {
    float* acc = dst.data() + static_cast<size_t>(oy) * dw * 3;
    for (int by = 0; by < block; ++by) {
      const float* srow = src + (static_cast<size_t>(oy) * block + by) * row_floats;
      QuantizeXyzToHalf(srow, row_floats, scale, half_row.data());
      DequantizeHalfToXyz(half_row.data(), row_floats, scale, canon_row.data());
      for (int ox = 0; ox < dw; ++ox) {
        const float* first = canon_row.data() + static_cast<size_t>(ox) * block * 3;
        for (int bx = 0; bx < block; ++bx) {
          for (int c = 0; c < 3; ++c) {
            acc[ox * 3 + c] += first[bx * 3 + c];
          }
        }
      }
    }
    for (size_t i = 0; i < static_cast<size_t>(dw) * 3; ++i) {
      acc[i] *= inv;
    }
  }
  *out_w = dw;
  *out_h = dh;
  return true;
}

}  // namespace lumice::gui

#endif  // LUMICE_GUI_PREVIEW_DOWNGRADE_HPP
