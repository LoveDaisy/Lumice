#pragma once

#include <algorithm>

namespace lumice {

// =================================================================================================
// The "channel B - R" display mode's formula (RenderConfig::kDisplayChannelBr), shared by both
// renderers.
//
// A diagnostic of "is this spot bluer or redder": the same arithmetic a user does by hand in an
// image editor, subtracting the red channel from the blue one of the finished sRGB image, and
// showing the signed result as a grey offset:
//
//   gray = clamp(0.5 + 0.5 * (B_srgb - R_srgb), 0, 1)
//
// Mid grey is zero, bluer is lighter, redder is darker; R == B (any neutral pixel, black included)
// is exactly mid grey. The inputs are POST-GAMMA sRGB values in [0, 1], i.e. the channels of the
// picture the Normal mode would display — exposure, gamut clip, XYZ -> linear RGB, background, then
// the sRGB transfer curve. Producing those two values is the caller's job, not this function's,
// for the same reason InkOpticalDensity (ink_transfer.hpp) does not know where its `e` came from.
// Two consequences worth knowing, both of the formula rather than of any implementation: the value
// moves with EV (B and R are exposed values), and once B or R clips at 1 the difference is
// flattened, so two spots are only comparable at the same EV.
//
// ⚠️ This formula exists TWICE — here, and hand-transcribed as `channelMathBrGray()` in
// preview_renderer.cpp's GLSL — for the constrained reason ink_transfer.hpp spells out: a shader
// cannot include a C++ header. That is the exception, not this repository's pattern. The pair is
// held together by the "MUST equal" note at the shader copy and by the preview/export/CLI parity
// tests (test/gui/parity/). Everything that CAN link this header does.
//
// It lives in src/util/ for the exemption AGENTS.md names: a pure, stateless function with no
// simulation or configuration semantics, which the GUI may include without crossing the C API
// boundary.
// =================================================================================================
inline float ChannelMathBrGray(float r_srgb, float b_srgb) {
  return std::clamp(0.5f + 0.5f * (b_srgb - r_srgb), 0.0f, 1.0f);
}

}  // namespace lumice
