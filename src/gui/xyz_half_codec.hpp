#ifndef LUMICE_GUI_XYZ_HALF_CODEC_HPP
#define LUMICE_GUI_XYZ_HALF_CODEC_HPP

// The float16 encoding of the preview's linear-XYZ texture, in one place.
//
// Two consumers, one implementation — and the ONE is load-bearing, not tidy. The live preview
// (PreviewRenderer::UploadXyzTexture) quantizes a result frame's XYZ floats through these functions
// before handing the half bits to a GL_RGB16F texture; the .lmc writer (file_io.cpp's v>=6 texture
// section) quantizes the SAME floats through the SAME functions before deflating them. That is
// what makes "the picture on screen == the picture the reopened file shows" true to the byte
// (test/gui/parity/test_gui_lmc_roundtrip_parity.cpp): the same pure function on the same input
// yields the same bits, so the two chains cannot drift by a rounding mode, a flush-to-zero flag or
// a driver's own float→half conversion. Do NOT let a second float→half conversion into either path —
// not a SIMD variant for the upload side, not a "the driver can convert GL_FLOAT for us" shortcut,
// not a per-call-site tweak. Any optimization must be made HERE, to the one implementation both
// sides call, or the τ=0 identity turns into a threshold.
//
// The encoding. A frame is stored as `scale` (one float32) plus one IEEE 754 binary16 per component
// holding `xyz / scale`, with `scale = max|xyz| / 32768` — so the brightest component lands on
// exactly 2^15, a power of two the half format represents exactly and float division by `scale`
// reproduces exactly. Everything else has half's relative precision (~2^-11), which an 8-bit
// display cannot resolve; components below scale·2^-14 fall into half's subnormal range and lose
// relative precision, but at that point they are more than 29 stops under the brightest one.
// Nothing here is clever: half is the hardware format GL_RGB16F samples, and the scale is a
// uniform the shader multiplies back in.
//
// Why the two-step (dequantize to float, requantize) round trip is exact, which is what the
// reopened path relies on. Dequantize(Quantize(x, s), s) = h·s where h is a half value (≤ 11
// significant bits) and s a float; the product rounds to float with relative error ≤ 2^-24, far
// inside half's rounding half-width of 2^-12, so requantizing it lands back on h. The brightest
// component is 2^15·s = max exactly (scaling by a power of two is exact), so the recomputed scale
// is the same s. Hence quantize ∘ dequantize ∘ quantize == quantize, bit for bit, and a v6 file
// reopened, re-uploaded and re-saved writes the same texture bytes it was read from.
//
// Deliberately free of GL, file I/O and ImGui so the round trip can be asserted in a unit test
// without a window (same shape as mono_exposure_scale.hpp beside it).

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace lumice::gui {

// binary32 -> binary16 bit pattern, round-to-nearest-even, IEEE semantics throughout (subnormals
// kept, overflow to Inf, NaN stays NaN). Bit-level port of the well-known "fast3_rtne" formulation:
// the subnormal arm rides on the FPU's own round-to-nearest-even by adding a magic 0.5f that puts
// the half's 2^-24 quantum exactly at float's last mantissa bit.
inline uint16_t FloatToHalfBits(float f) {
  uint32_t u = 0;
  std::memcpy(&u, &f, sizeof(u));
  const uint32_t sign = u & 0x80000000u;
  u ^= sign;

  constexpr uint32_t kF32Inf = 255u << 23;
  constexpr uint32_t kF16MaxAsF32 = (127u + 16u) << 23;                       // 2^16: at or above, the half is Inf
  constexpr uint32_t kMinNormalAsF32 = 113u << 23;                            // 2^-14: below, the half is subnormal
  constexpr uint32_t kDenormMagic = ((127u - 15u) + (23u - 10u) + 1u) << 23;  // 0.5f

  uint16_t o = 0;
  if (u >= kF16MaxAsF32) {
    o = (u > kF32Inf) ? 0x7E00u : 0x7C00u;  // NaN -> quiet NaN, Inf (and overflow) -> Inf
  } else if (u < kMinNormalAsF32) {
    float magic = 0.0f;
    std::memcpy(&magic, &kDenormMagic, sizeof(magic));
    float sum = 0.0f;
    std::memcpy(&sum, &u, sizeof(sum));
    sum += magic;
    uint32_t su = 0;
    std::memcpy(&su, &sum, sizeof(su));
    o = static_cast<uint16_t>(su - kDenormMagic);
  } else {
    const uint32_t mant_odd = (u >> 13) & 1u;
    u += ((15u - 127u) << 23) + 0xFFFu;  // rebias the exponent, then round half to even
    u += mant_odd;
    o = static_cast<uint16_t>(u >> 13);
  }
  return static_cast<uint16_t>(o | (sign >> 16));
}

// binary16 bit pattern -> binary32. Exact: every half value is a float value.
inline float HalfBitsToFloat(uint16_t h) {
  const uint32_t sign = static_cast<uint32_t>(h & 0x8000u) << 16;
  const uint32_t exp = (h >> 10) & 0x1Fu;
  const uint32_t mant = h & 0x3FFu;
  uint32_t u = 0;
  if (exp == 0) {
    // Zero or subnormal: mant * 2^-24. float(mant) is exact (mant < 2^10) and so is the scaling.
    const float v = static_cast<float>(mant) * 5.9604644775390625e-8f;  // 2^-24
    std::memcpy(&u, &v, sizeof(u));
    u |= sign;
  } else if (exp == 31) {
    u = sign | 0x7F800000u | (mant << 13);
  } else {
    u = sign | ((exp + 112u) << 23) | (mant << 13);  // 112 = 127 - 15
  }
  float f = 0.0f;
  std::memcpy(&f, &u, sizeof(f));
  return f;
}

// The frame's scale: max|xyz| / 32768, so the brightest component quantizes to exactly 2^15. An
// all-zero (or empty) frame returns 1.0f, which quantizes every component to 0 without dividing
// by zero. Non-finite inputs are ignored by std::fmax's NaN handling; an Inf would make every
// finite component 0 — a frame with an Inf in it is already broken upstream.
inline float ComputeXyzHalfScale(const float* xyz, size_t n) {
  float max_abs = 0.0f;
  for (size_t i = 0; i < n; ++i) {
    max_abs = std::fmax(max_abs, std::fabs(xyz[i]));
  }
  if (!(max_abs > 0.0f) || !std::isfinite(max_abs)) {
    return 1.0f;
  }
  return max_abs / 32768.0f;
}

// out[i] = half(xyz[i] / scale). `scale` must come from ComputeXyzHalfScale over the same buffer
// (or be the value a file recorded next to the halves it holds); anything else is a second
// encoding.
inline void QuantizeXyzToHalf(const float* xyz, size_t n, float scale, uint16_t* out) {
  for (size_t i = 0; i < n; ++i) {
    out[i] = FloatToHalfBits(xyz[i] / scale);
  }
}

// out[i] = float(half[i]) * scale — the inverse of QuantizeXyzToHalf up to half's precision, and
// an exact fixed point of it (see the header comment).
inline void DequantizeHalfToXyz(const uint16_t* half, size_t n, float scale, float* out) {
  for (size_t i = 0; i < n; ++i) {
    out[i] = HalfBitsToFloat(half[i]) * scale;
  }
}

}  // namespace lumice::gui

#endif  // LUMICE_GUI_XYZ_HALF_CODEC_HPP
