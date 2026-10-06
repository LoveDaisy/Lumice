#include "optics.hpp"

#include <algorithm>
#include <cmath>

#include "core/math.hpp"
#include "core/shared/optics_shared.h"
#include "core/shared/traversal_shared.h"


namespace lumice {

float GetReflectRatio(float delta, float rr) {
  return lm_optics::GetReflectRatio(delta, rr);
}

// NOLINTNEXTLINE(readability-function-size)
void HitSurface(const Crystal& crystal, float n, size_t num,                             // input
                const float_bf_t d_in, const float_bf_t w_in, const id_bf_t to_face_in,  // input
                float_bf_t d_out, float_bf_t w_out) {                                    // output
  // Polygon-face normals are equivalent to per-triangle normals (Build matches
  // by dot > 1 - 1e-3) but indexed by polygon face — matches RaySeg::to_face_.
  const auto* poly_norm = crystal.GetPolygonFaceNormal();

  for (size_t i = 0; i < num; i++) {
    if (to_face_in[i] == kInvalidId) {
      // No valid hit face — zero both output weights (and leave directions unchanged).
      w_out[2 * i + 0] = 0.0f;
      w_out[2 * i + 1] = 0.0f;
      continue;
    }
    const float* tmp_dir = d_in.Ptr(i);
    const float* tmp_norm = poly_norm + to_face_in[i] * 3;

    float cos_theta = Dot3(tmp_dir, tmp_norm);
    float rr = cos_theta > 0 ? n : 1.0f / n;
    float d = (1.0f - rr * rr) / (cos_theta * cos_theta) + rr * rr;

    bool is_total_reflected = d <= 0.0f;

    w_out[2 * i + 0] = GetReflectRatio(std::max(d, 0.0f), rr) * w_in[i];
    w_out[2 * i + 1] = is_total_reflected ? -1 : w_in[i] - w_out[2 * i + 0];

    float* tmp_dir_reflection = d_out.Ptr(i * 2 + 0);
    float* tmp_dir_refraction = d_out.Ptr(i * 2 + 1);
    for (int j = 0; j < 3; j++) {
      tmp_dir_reflection[j] = tmp_dir[j] - 2 * cos_theta * tmp_norm[j];  // Reflection
      tmp_dir_refraction[j] = is_total_reflected ?
                                  tmp_dir_reflection[j] :
                                  rr * tmp_dir[j] - (rr - std::sqrt(d)) * cos_theta * tmp_norm[j];  // Refraction
    }
  }
}


constexpr size_t kMaxSlabRays = 128;
// Chunk boundary in Propagate() must be a multiple of the position-sharing
// stride (step=1 or step=2 in practice). Keeping kMaxSlabRays even guarantees
// `offset / step` stays integer-aligned across chunks.
static_assert(kMaxSlabRays % 2 == 0, "kMaxSlabRays must be even for step=2 chunk alignment");

// Per-polygon-face half-space interval method with face-outer/ray-inner SoA loop.
// NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity)
static void PropagateSlab(const Crystal& crystal, size_t num, size_t step, const float_bf_t d_in, const float_bf_t p_in,
                          const float_bf_t w_in, const id_bf_t from_face_in, float_bf_t p_out, id_bf_t to_face_out) {
  auto poly_cnt = crystal.PolygonFaceCount();
  const auto* pn = crystal.GetPolygonFaceNormal();
  const auto* pd = crystal.GetPolygonFaceDist();

  // Gather strided BufferWrapper data into contiguous SoA arrays
  alignas(64) float dx[kMaxSlabRays], dy[kMaxSlabRays], dz[kMaxSlabRays];
  alignas(64) float px[kMaxSlabRays], py[kMaxSlabRays], pz[kMaxSlabRays];
  alignas(64) float t_far[kMaxSlabRays];
  int far_face[kMaxSlabRays];
  int src_poly[kMaxSlabRays];
  bool outward[kMaxSlabRays];

  for (size_t i = 0; i < num; i++) {
    const float* d = d_in.Ptr(i);
    const float* p = p_in.Ptr(i / step);
    dx[i] = d[0];
    dy[i] = d[1];
    dz[i] = d[2];
    px[i] = p[0];
    py[i] = p[1];
    pz[i] = p[2];
    t_far[i] = 1e30f;
    far_face[i] = -1;
    IdType src_id = from_face_in[i / step];
    src_poly[i] = (src_id != kInvalidId) ? static_cast<int>(src_id) : -1;
    // Birth classification: a child born on its source face with
    // denom_src = d·n_src > 0 has LEFT the convex crystal — d·n_src > 0 means
    // the signed distance to the source plane strictly increases along the
    // ray, so no re-hit is possible. Pure sign test, no epsilon, identical in
    // shape to CUDA's exit gates (entry external-reflect exits
    // unconditionally; refracted exits gate on cos_exit > 0.0f,
    // cuda_trace_backend.cu) — see traversal_shared.h. The scatter-back loop
    // turns this flag into an exit. Rays with no source face (src_poly < 0,
    // initial entry rays) are not classified and keep the plain slab path.
    outward[i] = false;
    if (src_poly[i] >= 0) {
      const float* n_src = pn + src_poly[i] * 3;
      outward[i] = dx[i] * n_src[0] + dy[i] * n_src[1] + dz[i] * n_src[2] > 0.0f;
    }
  }

  // Face-outer, ray-inner: find minimum t among exit faces (denom > eps)
  //
  // CONVEXITY ASSUMPTION (latent — keep in mind before adding crystal types):
  // This "nearest plane the ray is exiting" next-face search is only correct for a
  // CONVEX crystal whose every polygon plane bounds a real (non-degenerate) face,
  // and its "min-t = exit face" invariant presupposes a ray that STARTS INSIDE the
  // crystal. Outward-going children therefore never consume the search result:
  // they are classified as exits at birth (denom_src > 0.0f, see the gather loop)
  // and dropped at scatter-back. Feeding them to this search was the source of
  // phantom re-hits: a real birth point sits a few ulp INSIDE the source plane
  // (δ < 0), and a grazing outward denominator inflates t = |δ|/denom past the
  // +kFloatEps self-hit guard (a guard with the wrong dimensions — it compares a
  // length against 1e-5 while the failing quantity is the ~1 ulp residual), while
  // the convex half-space property hands positive t to other faces as well —
  // either way the child "hit" a plane it cannot reach, chains grew adjacent
  // duplicate faces (impossible inside a convex crystal), and the grazing re-hit
  // forced TIR back inside, producing non-physical exits (stray sky dots).
  // The convexity argument leans on two further invariants:
  //   (1) all configured crystals are convex (CreateConcavePyramidMesh exists in
  //       geo3d but is NOT wired to the config/Crystal factory; if a concave
  //       type is ever wired through here, outward rays CAN legitimately re-hit
  //       and the birth classification must be revisited along with this search);
  //   (2) no degenerate / zero-area plane leaks into the polygon-face set —
  //       ensured by the numerical-robustness rework (#133), which kills the
  //       fake-basal face at extreme wedge that previously made outward
  //       first-bounce reflections select a phantom plane (the B-ring bug).
  // (An earlier defensive `d·n_src > 0` outward-skip prototype (#132,
  // PropagateSlab) was not merged at the time; the birth classification that
  // now stands is its constructive landing, aligned with CUDA's semantics.)
  for (size_t fi = 0; fi < poly_cnt; fi++) {
    float nx = pn[fi * 3 + 0];
    float ny = pn[fi * 3 + 1];
    float nz = pn[fi * 3 + 2];
    float fd = pd[fi];

    // Per-face intersect via cross-backend single-source helper
    // (lm_traversal::SlabFaceT, see core/shared/traversal_shared.h). Non-
    // candidate faces (denom ≤ kSlabEps) return 1e30f and lose the `t < t_far[i]`
    // comparison because t_far[i] is initialised to 1e30f — equivalent to the
    // prior explicit `denom > kFloatEps` gate. SoA face-outer/ray-inner shape
    // preserved for the inner loop to remain vectorisable.
    for (size_t i = 0; i < num; i++) {
      float t = lm_traversal::SlabFaceT(dx[i], dy[i], dz[i], px[i], py[i], pz[i], nx, ny, nz, fd);
      if (t < t_far[i]) {
        t_far[i] = t;
        far_face[i] = static_cast<int>(fi);
      }
    }
  }

  // Scatter results back to strided BufferWrapper
  for (size_t i = 0; i < num; i++) {
    if (w_in[i] < 0) {
      continue;
    }

    float* out_pt = p_out.Ptr(i);
    IdType* out_face = to_face_out.Ptr(i);

    // Outward child (birth classification, see gather loop): exits right here
    // with its position unchanged. The slab walk above still ran for this ray
    // ON PURPOSE — keeping the face loop face-outer/ray-inner over the whole
    // batch preserves the vectorisable SoA shape, and a per-ray early skip
    // would break it. Only this scatter point discards the outward ray's slab
    // result; do not "simplify" it into a per-ray skip inside the face loop.
    if (outward[i]) {
      out_pt[0] = px[i];
      out_pt[1] = py[i];
      out_pt[2] = pz[i];
      *out_face = kInvalidId;
      continue;
    }

    // Use relaxed threshold for non-source faces to accept t≈0 TIR-edge hits.
    // Source face keeps +kFloatEps to prevent self-selection. With the birth
    // classification above, an inward child (denom_src ≤ 0 < kSlabEps) never
    // offers its source plane as a slab candidate — SlabFaceT's denominator
    // gate excludes it — so the source-face +eps branch below is structurally
    // dead for src_poly >= 0 and survives only as the defensive path for
    // src_poly == -1 (no source face to classify against).
    float eps_thr = (src_poly[i] >= 0 && far_face[i] != src_poly[i]) ? -math::kFloatEps : math::kFloatEps;
    if (far_face[i] >= 0 && t_far[i] > eps_thr) {
      float t = t_far[i];
      out_pt[0] = px[i] + t * dx[i];
      out_pt[1] = py[i] + t * dy[i];
      out_pt[2] = pz[i] + t * dz[i];
      *out_face = static_cast<IdType>(far_face[i]);
    } else {
      out_pt[0] = px[i];
      out_pt[1] = py[i];
      out_pt[2] = pz[i];
      *out_face = kInvalidId;
    }
  }
}

// Polygon-only tracing dispatcher. Chunks the ray batch so each PropagateSlab
// invocation stays within its fixed-size scratch arrays (kMaxSlabRays).
// NOLINTNEXTLINE(readability-function-size)
void Propagate(const Crystal& crystal, size_t num, size_t step,                      // input
               const float_bf_t d_in, const float_bf_t p_in, const float_bf_t w_in,  // input, d, p, w
               const id_bf_t from_face_in,                                           // source polygon face ids
               float_bf_t p_out, id_bf_t to_face_out) {                              // output, p, to_face
  for (size_t offset = 0; offset < num; offset += kMaxSlabRays) {
    size_t chunk = std::min(num - offset, kMaxSlabRays);
    PropagateSlab(crystal, chunk, step,                                          //
                  float_bf_t(d_in.Ptr(offset), d_in.step_),                      //
                  float_bf_t(p_in.Ptr(offset / step), p_in.step_),               //
                  float_bf_t(w_in.Ptr(offset), w_in.step_),                      //
                  id_bf_t(from_face_in.Ptr(offset / step), from_face_in.step_),  //
                  float_bf_t(p_out.Ptr(offset), p_out.step_),                    //
                  id_bf_t(to_face_out.Ptr(offset), to_face_out.step_));
  }
}


double IceRefractiveIndex::Get(double wave_length) {
  /* Shellmeier's equation:
   *
   *   n^2 = 1 + B1 * lambda^2 / (lambda^2 - C1)
   *           + B2 * lambda^2 / (lambda^2 - C2)
   *   lambda in micrometer, B * 1e-2, C * 1e2
   */
  if (wave_length < kMinWaveLength || wave_length > kMaxWaveLength) {
    return 1.0f;
  }

  wave_length /= 1e3;

  double n = 1.0;
  n += kCoefAvr[0] / (1 - kCoefAvr[2] * 1e-2f / wave_length / wave_length);
  n += kCoefAvr[1] / (1 - kCoefAvr[3] * 1e2f / wave_length / wave_length);

  return std::sqrt(n);
}

}  // namespace lumice
