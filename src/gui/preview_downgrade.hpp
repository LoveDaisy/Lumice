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
//    attempts came before. DowngradeXyzPlane takes `steps` and always reads the caller's original
//    plane.
//
// The dual-disc layout survives untouched: each 2x2 step halves both axes, so the two discs stay
// proportional, the packing's corner padding stays padding, and the shader's dual-fisheye inverse
// needs no knowledge that it is sampling a smaller disc pair. The step floor exists so the ladder
// terminates: below kMinDowngradedWidth x kMinDowngradedHeight a dual-disc pair no longer carries
// a meaningful projection, and a candidate that small has stopped being a recovery.

#include <cstddef>
#include <cstdint>

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

// One 2x2 area-average step over a linear-XYZ plane (row-major, 3 floats per texel). `src` and
// `dst` must not alias; dst holds (src_w/2)*(src_h/2)*3 floats. Both dimensions must be even
// (CanDowngradeOnce); a mixed odd edge would have to invent texels, and inventing radiance is
// exactly what a recovery path may not do.
inline void DowngradeXyzStep2x2(const float* src, int src_w, int src_h, float* dst) {
  const int dst_w = src_w / 2;
  const int dst_h = src_h / 2;
  for (int y = 0; y < dst_h; ++y) {
    const float* row0 = src + static_cast<size_t>(y * 2) * src_w * 3;
    const float* row1 = row0 + static_cast<size_t>(src_w) * 3;
    float* out = dst + static_cast<size_t>(y) * dst_w * 3;
    for (int x = 0; x < dst_w; ++x) {
      const size_t a = static_cast<size_t>(x * 2) * 3;
      const size_t b = a + 3;
      for (int c = 0; c < 3; ++c) {
        // Average, not sum: the texel keeps its per-source-texel radiance unit (see the header
        // comment, law 1). Plain quarter-sum in float; no reweighting, no gamma, no clamp —
        // radiance is linear and unbounded.
        out[x * 3 + c] = 0.25f * (row0[a + c] + row0[b + c] + row1[a + c] + row1[b + c]);
      }
    }
  }
}

// Apply `steps` sequential 2x2 steps, ALWAYS starting from `src` (law 2). Returns the number of
// steps actually applied — fewer than asked only when the floor stops the ladder. `scratch` must
// hold one INTERMEDIATE plane ((w/2)*(h/2)*3 — the largest the loop ever writes); `dst` must hold
// only the FINAL plane and is written once, at the end (steps == 0 leaves it untouched: the
// original is the candidate). Every intermediate lands in scratch — ping-ponging into dst would
// overflow it, because dst is sized for the result, not for the walk.
inline int DowngradeXyzPlane(const float* src, int w, int h, int steps, float* scratch, float* dst, int* out_w,
                             int* out_h) {
  int applied = 0;
  int cw = w;
  int ch = h;
  const float* cur = src;
  for (int i = 0; i < steps && CanDowngradeOnce(cw, ch); ++i) {
    DowngradeXyzStep2x2(cur, cw, ch, scratch);
    cur = scratch;
    cw /= 2;
    ch /= 2;
    ++applied;
  }
  if (applied > 0) {
    const size_t floats = static_cast<size_t>(cw) * ch * 3;
    for (size_t i = 0; i < floats; ++i) {
      dst[i] = cur[i];
    }
  }
  *out_w = cw;
  *out_h = ch;
  return applied;
}

}  // namespace lumice::gui

#endif  // LUMICE_GUI_PREVIEW_DOWNGRADE_HPP
