#include "core/crystal.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include "config/crystal_config.hpp"
#include "config/filter_config.hpp"
#include "core/def.hpp"
#include "core/geo3d.hpp"
#include "core/geo3d_closedform.hpp"
#include "core/math.hpp"
#include "core/optics.hpp"
#include "util/logger.hpp"

namespace lumice {

// Bridge Crystal's flat-POD capacity to the closed-form evaluator's per-face
// counts. If the closed-form path ever gains a face-count larger than 20 (a
// new crystal family or a widened cone slot layout), CrystalGeom's fixed arrays
// would silently truncate — catch that at compile time here rather than at a
// runtime read-past-end site.
static_assert(kCrystalGeomMaxFaces >= kClosedFormPrismFaceCnt,
              "CrystalGeom face capacity must accommodate the prism closed-form family");
static_assert(kCrystalGeomMaxFaces >= kClosedFormPyramidFaceCnt,
              "CrystalGeom face capacity must accommodate the pyramid closed-form family");
static_assert(kCrystalGeomMaxVtxPerFace >= kClosedFormPyramidMaxFaceVtx,
              "CrystalGeom per-face vertex capacity must accommodate pyramid cone faces");

// Legal face-number sets for the hexagonal crystal family:
//   basal:         1, 2
//   prism lateral: 3..8
//   upper pyramid: 13..18
//   lower pyramid: 23..28
// See crystal_kind.hpp for rationale on why this GUI-facing coarse enum is
// intentionally distinct from the core CrystalType.
bool IsLegalFace(CrystalKind kind, int face) {
  auto is_basal = [](int f) { return f == 1 || f == 2; };
  auto is_prism_lateral = [](int f) { return f >= 3 && f <= 8; };
  auto is_upper_pyramid = [](int f) { return f >= 13 && f <= 18; };
  auto is_lower_pyramid = [](int f) { return f >= 23 && f <= 28; };

  switch (kind) {
    case CrystalKind::kPrism:
      return is_basal(face) || is_prism_lateral(face);
    case CrystalKind::kPyramid:
      return is_basal(face) || is_prism_lateral(face) || is_upper_pyramid(face) || is_lower_pyramid(face);
  }
  // Unhandled CrystalKind — new enum values must extend the switch above.
  assert(false && "IsLegalFace: unhandled CrystalKind");
  return false;
}

bool IsClosedTriMesh(size_t v, size_t f) {
  if (v == 0 || f == 0 || f % 2 != 0) {
    return false;
  }
  const auto vi = static_cast<int64_t>(v);
  const auto fi = static_cast<int64_t>(f);
  return vi - (3 * fi / 2) + fi == 2;
}

namespace {

// 386.2 validity gate for the prism closed-form path. The malformed families
// the Euler check used to catch (opposite-pair-sum ≤ 0, wedge-collapse
// pyramid-adjacent inputs) collapse the 2D cross-section corner ring to fewer
// than three distinct corners; the closed-form solver produces that fact
// directly (corner_cnt) — no reverse-engineering the topology from a triangle
// mesh required.
bool IsValidClosedFormPrism(const ClosedFormPrismResult& r, float h) {
  return h > math::kFloatEps && r.corner_cnt >= 3;
}

// 386.2 validity gate for the pyramid closed-form path. The closed-form
// evaluator already resolves face-presence per slot (accounting for cone
// dropout at illegal alpha, shoulder-vs-apex layering, and corner-death
// events); a valid solid needs at least a tetrahedron's worth of faces
// (4 present). Zero-volume inputs (h1=h2=h3=0 or all cones dropped with h2=0)
// short-circuit to vtx_cnt == 0 inside the evaluator, which trivially fails
// the count check.
bool IsValidClosedFormPyramid(const ClosedFormPyramidResult& r) {
  int present_cnt = 0;
  for (int i = 0; i < kClosedFormPyramidFaceCnt; i++) {
    if (r.face_present[i]) {
      present_cnt++;
    }
  }
  return present_cnt >= 4;
}

// Convert ClosedFormPrismResult (single CCW 2D corner ring + h) into per-face
// 3D CCW vertex lists in CrystalGeom.
//   - Upper basal (slot 0, normal +z): ring CCW at z = +h/2 (ring is emitted
//     CCW as seen from +z by SolveHexCrossSection; no re-ordering).
//   - Lower basal (slot 1, normal -z): ring reversed at z = -h/2 so viewer at
//     -z sees CCW.
//   - Side face (slot 2+i, present): 4-vert rectangle
//       (c_prev, z_bot), (c_curr, z_bot), (c_curr, z_top), (c_prev, z_top)
//     with c_prev, c_curr the two ring corners the side lies between (walking
//     present sides in i-increasing order). cross(v1-v0, v2-v0) aligns with
//     the outward horizontal normal.
void AdaptClosedFormPrismToCrystalGeom(const ClosedFormPrismResult& r, float h, CrystalGeom& g) {
  g = CrystalGeom{};
  g.face_cnt = kClosedFormPrismFaceCnt;

  std::memcpy(g.plane_coef, r.plane_coef, sizeof(r.plane_coef));
  std::memcpy(g.face_normal, r.face_normal, sizeof(r.face_normal));
  std::memcpy(g.face_number, r.face_number, sizeof(r.face_number));
  for (int i = 0; i < kClosedFormPrismFaceCnt; i++) {
    g.face_present[i] = r.face_present[i];
  }

  if (r.corner_cnt < 3) {
    return;
  }

  const int n = r.corner_cnt;
  const float z_top = 0.5f * h;
  const float z_bot = -0.5f * h;

  if (r.face_present[0]) {
    g.face_vtx_cnt[0] = n;
    float* base = g.face_vtx + 0 * kCrystalGeomMaxVtxPerFace * 3;
    for (int k = 0; k < n; k++) {
      base[k * 3 + 0] = r.corner_x[k];
      base[k * 3 + 1] = r.corner_y[k];
      base[k * 3 + 2] = z_top;
    }
  }
  if (r.face_present[1]) {
    g.face_vtx_cnt[1] = n;
    float* base = g.face_vtx + 1 * kCrystalGeomMaxVtxPerFace * 3;
    for (int k = 0; k < n; k++) {
      const int rev = n - 1 - k;
      base[k * 3 + 0] = r.corner_x[rev];
      base[k * 3 + 1] = r.corner_y[rev];
      base[k * 3 + 2] = z_bot;
    }
  }

  int present_idx[kClosedFormPrismSideCnt];
  int p_n = 0;
  for (int i = 0; i < kClosedFormPrismSideCnt; i++) {
    if (r.face_present[2 + i]) {
      present_idx[p_n++] = i;
    }
  }
  // p_n must equal n: the corner ring is built by walking present sides and
  // emitting one corner per adjacent pair, and a bounded 2D polygon has as
  // many edges as vertices.
  assert(p_n == n);

  for (int k = 0; k < p_n; k++) {
    const int side_i = present_idx[k];
    const int slot = 2 + side_i;
    const int c_prev = (k - 1 + p_n) % p_n;
    const int c_curr = k;

    const float px = r.corner_x[c_prev];
    const float py = r.corner_y[c_prev];
    const float cx = r.corner_x[c_curr];
    const float cy = r.corner_y[c_curr];

    g.face_vtx_cnt[slot] = 4;
    float* base = g.face_vtx + slot * kCrystalGeomMaxVtxPerFace * 3;
    base[0 * 3 + 0] = px;
    base[0 * 3 + 1] = py;
    base[0 * 3 + 2] = z_bot;
    base[1 * 3 + 0] = cx;
    base[1 * 3 + 1] = cy;
    base[1 * 3 + 2] = z_bot;
    base[2 * 3 + 0] = cx;
    base[2 * 3 + 1] = cy;
    base[2 * 3 + 2] = z_top;
    base[3 * 3 + 0] = px;
    base[3 * 3 + 1] = py;
    base[3 * 3 + 2] = z_top;
  }
}

// Convert ClosedFormPyramidResult (per-slot CCW vertex indices into the
// evaluator's global 3D pool) into per-face 3D CCW vertex coord lists in
// CrystalGeom. The evaluator already sorts each face's vertices CCW as seen
// from OUTSIDE the solid (geo3d_closedform.cpp:923-988), so no re-ordering is
// needed here — just an index→coord scatter.
void AdaptClosedFormPyramidToCrystalGeom(const ClosedFormPyramidResult& r, CrystalGeom& g) {
  g = CrystalGeom{};
  g.face_cnt = kClosedFormPyramidFaceCnt;

  std::memcpy(g.plane_coef, r.plane_coef, sizeof(r.plane_coef));
  std::memcpy(g.face_normal, r.face_normal, sizeof(r.face_normal));
  std::memcpy(g.face_number, r.face_number, sizeof(r.face_number));
  for (int i = 0; i < kClosedFormPyramidFaceCnt; i++) {
    g.face_present[i] = r.face_present[i];
  }

  for (int slot = 0; slot < kClosedFormPyramidFaceCnt; slot++) {
    if (!r.face_present[slot]) {
      continue;
    }
    const int fn = r.face_vtx_cnt[slot];
    assert(fn <= kCrystalGeomMaxVtxPerFace);
    g.face_vtx_cnt[slot] = fn;
    float* base = g.face_vtx + slot * kCrystalGeomMaxVtxPerFace * 3;
    for (int k = 0; k < fn; k++) {
      const int vi = r.face_vtx[slot][k];
      base[k * 3 + 0] = r.vtx[vi * 3 + 0];
      base[k * 3 + 1] = r.vtx[vi * 3 + 1];
      base[k * 3 + 2] = r.vtx[vi * 3 + 2];
    }
  }
}

}  // namespace

namespace detail {

// Dedup-linear-search 3D vertex pool + fixed-fan triangulation. Small n
// (≤ 24 verts for prism, ≤ 96 for the pyramid worst case), so O(n²) dedup is
// fine and matches the tolerance choice already used inside
// geo3d_closedform.cpp's 2D corner dedup.
//
// This is the only triangulating path left in the codebase. It is a pure
// function of cf_geom_, invoked on the cold path only (C-API geometry export +
// white-box tests) — the MakeCrystal hot path no longer builds a triangle mesh.
BuiltMesh BuildMeshFromCfGeom(const CrystalGeom& g) {
  constexpr float kDedupTol = 1e-6f;

  std::vector<float> vtx_pool;
  vtx_pool.reserve(static_cast<size_t>(kCrystalGeomMaxFaces) * kCrystalGeomMaxVtxPerFace * 3);
  auto add_or_find = [&](float x, float y, float z) -> int {
    const size_t n = vtx_pool.size() / 3;
    for (size_t i = 0; i < n; i++) {
      const float dx = vtx_pool[i * 3 + 0] - x;
      const float dy = vtx_pool[i * 3 + 1] - y;
      const float dz = vtx_pool[i * 3 + 2] - z;
      if (std::sqrt(dx * dx + dy * dy + dz * dz) < kDedupTol) {
        return static_cast<int>(i);
      }
    }
    vtx_pool.push_back(x);
    vtx_pool.push_back(y);
    vtx_pool.push_back(z);
    return static_cast<int>(n);
  };

  std::vector<int> tri_idx;
  std::vector<int> tri_face_slot;
  int face_to_global[kCrystalGeomMaxVtxPerFace];

  for (int slot = 0; slot < g.face_cnt; slot++) {
    if (!g.face_present[slot]) {
      continue;
    }
    const int fn = g.face_vtx_cnt[slot];
    if (fn < 3) {
      continue;
    }
    const float* base = g.face_vtx + slot * kCrystalGeomMaxVtxPerFace * 3;
    for (int k = 0; k < fn; k++) {
      face_to_global[k] = add_or_find(base[k * 3 + 0], base[k * 3 + 1], base[k * 3 + 2]);
    }
    // Fan (v[0], v[i-1], v[i]) for i = 2..n-1 → n-2 triangles.
    for (int i = 2; i < fn; i++) {
      tri_idx.push_back(face_to_global[0]);
      tri_idx.push_back(face_to_global[i - 1]);
      tri_idx.push_back(face_to_global[i]);
      tri_face_slot.push_back(slot);
    }
  }

  const size_t vtx_cnt = vtx_pool.size() / 3;
  const size_t tri_cnt = tri_face_slot.size();

  auto vtx_buf = std::make_unique<float[]>(vtx_cnt * 3);
  if (vtx_cnt > 0) {
    std::memcpy(vtx_buf.get(), vtx_pool.data(), vtx_cnt * 3 * sizeof(float));
  }
  auto tri_buf = std::make_unique<int[]>(tri_cnt * 3);
  if (tri_cnt > 0) {
    std::memcpy(tri_buf.get(), tri_idx.data(), tri_cnt * 3 * sizeof(int));
  }

  return BuiltMesh{ Mesh(vtx_cnt, std::move(vtx_buf), tri_cnt, std::move(tri_buf)), std::move(tri_face_slot) };
}

}  // namespace detail

// Populate poly_face_data_ (normals + plane distances) and poly_face_fn_
// (per-polygon face-number) directly from the closed-form output. The
// closed-form output already carries the parametric face-number and per-slot
// presence; there is nothing to reverse-engineer. This is the payoff of the
// representation swap: what used to be three separate argmax reversals (CPU /
// Metal / CUDA) collapse into straight assignments from the same source, and
// the fn is now stored at its natural key (per polygon face) rather than
// reconstructed from an argmax-selected representative triangle.
void Crystal::PopulateFromCfGeom() {
  size_t present = 0;
  for (int slot = 0; slot < cf_geom_.face_cnt; slot++) {
    if (cf_geom_.face_present[slot]) {
      present++;
    }
  }
  poly_face_cnt_ = present;

  if (poly_face_cnt_ == 0) {
    poly_face_data_.reset();
    poly_face_n_ = nullptr;
    poly_face_d_ = nullptr;
    poly_face_fn_.reset();
    return;
  }
  // Layout: normals(3*cnt) + dist(cnt) = 4*cnt floats.
  poly_face_data_ = std::make_unique<float[]>(poly_face_cnt_ * 4);
  poly_face_n_ = poly_face_data_.get();
  poly_face_d_ = poly_face_data_.get() + poly_face_cnt_ * 3;
  poly_face_fn_ = std::make_unique<IdType[]>(poly_face_cnt_);

  size_t p = 0;
  for (int slot = 0; slot < cf_geom_.face_cnt; slot++) {
    if (!cf_geom_.face_present[slot]) {
      continue;
    }

    poly_face_n_[p * 3 + 0] = cf_geom_.face_normal[slot * 3 + 0];
    poly_face_n_[p * 3 + 1] = cf_geom_.face_normal[slot * 3 + 1];
    poly_face_n_[p * 3 + 2] = cf_geom_.face_normal[slot * 3 + 2];

    // Normalize the plane's d by |(a, b, c)| so the stored plane keeps the
    // "unit normal" convention. The closed-form plane_coef layout mirrors
    // FillHexCrystalCoef's, whose per-slot norms are {1, 1, 0.5, 0.5, 0.5,
    // 0.5, 0.5, 0.5} for prism (basal unit, side 0.5).
    const float* coef = cf_geom_.plane_coef + slot * 4;
    const float norm = Norm3(coef);
    poly_face_d_[p] = (norm > math::kFloatEps) ? (coef[3] / norm) : 0.0f;

    poly_face_fn_[p] = static_cast<IdType>(cf_geom_.face_number[slot]);
    p++;
  }
}

Crystal Crystal::MakePrismClosedForm(float h, const float dist[6], const GeometricSymmetry* symmetry,
                                     const char* factory) {
  ClosedFormPrismResult r = ComputeClosedFormPrism(h, dist);
  if (!IsValidClosedFormPrism(r, h)) {
    // Silent for the FillHexCrystalCoef-mirroring zero-volume path (h ≤ eps).
    // Warn on the "constructed something but it failed the closed-form gate"
    // case — matches the RejectMalformed semantics for what used to be the
    // "closed-mesh Euler check" family: caller-visible contract is unchanged,
    // downstream sees a zero-triangle Crystal that contributes nothing.
    if (h > math::kFloatEps) {
      LOG_WARNING("{}: failed closed-form validity gate (h={:.4e}, corner_cnt={}); treating as degenerate",  //
                  factory, static_cast<double>(h), r.corner_cnt);
    }
    return Crystal();
  }
  Crystal c;
  AdaptClosedFormPrismToCrystalGeom(r, h, c.cf_geom_);
  c.fn_period_ = kHexagonalFnPeriod;
  if (symmetry != nullptr) {
    c.geom_symmetry_ = *symmetry;
  } else {
    PrismCrystalParam as_param;
    as_param.h_ = Distribution{ DistributionType::kNoRandom, h, 0.0f };
    for (int i = 0; i < kHexagonalFnPeriod; i++) {
      as_param.d_[i] = Distribution{ DistributionType::kNoRandom, dist[i], 0.0f };
    }
    c.geom_symmetry_ = DeriveGeometricSymmetry(as_param);
  }
  c.PopulateFromCfGeom();
  return c;
}

Crystal Crystal::CreatePrism(float h) {
  float dist[6]{ 1, 1, 1, 1, 1, 1 };
  return MakePrismClosedForm(h, dist, nullptr, "CreatePrism(h)");
}

Crystal Crystal::CreatePrism(float h, const float* fd) {
  return MakePrismClosedForm(h, fd, nullptr, "CreatePrism(h, fd)");
}

Crystal Crystal::CreatePrism(float h, const float* fd, const GeometricSymmetry& ensemble_symmetry) {
  return MakePrismClosedForm(h, fd, &ensemble_symmetry, "CreatePrism(h, fd, symmetry)");
}

Crystal Crystal::CreatePyramid(float h1, float h2, float h3) {
  // The no-argument pyramid is the {1,0,-1,1} face, i.e. Miller (i1, i4) = (1, 1).
  float alpha = MillerIndexToWedgeAngleDeg(1, 1);
  return CreatePyramid(alpha, alpha, h1, h2, h3);
}

Crystal Crystal::MakePyramidClosedForm(float upper_alpha, float lower_alpha, float h1, float h2, float h3,
                                       const float dist[6], const GeometricSymmetry* symmetry, const char* factory) {
  ClosedFormPyramidResult r = ComputeClosedFormPyramid(upper_alpha, lower_alpha, h1, h2, h3, dist);
  if (!IsValidClosedFormPyramid(r)) {
    // Silent when the evaluator returned an all-empty result (zero-volume
    // short-circuit — matches FillHexCrystalCoef's own zero-volume path,
    // which emits its own warning). Warn only when it produced *some*
    // vertices but not enough face slots to bound a solid — the RejectMalformed
    // analog for the closed-form path.
    if (r.vtx_cnt > 0) {
      LOG_WARNING("{}: failed closed-form validity gate (present face count < 4, vtx_cnt={})", factory, r.vtx_cnt);
    }
    return Crystal();
  }
  Crystal c;
  AdaptClosedFormPyramidToCrystalGeom(r, c.cf_geom_);
  c.fn_period_ = kHexagonalFnPeriod;
  if (symmetry != nullptr) {
    c.geom_symmetry_ = *symmetry;
  } else {
    PyramidCrystalParam as_param;
    as_param.h_pyr_u_ = Distribution{ DistributionType::kNoRandom, h1, 0.0f };
    as_param.h_prs_ = Distribution{ DistributionType::kNoRandom, h2, 0.0f };
    as_param.h_pyr_l_ = Distribution{ DistributionType::kNoRandom, h3, 0.0f };
    for (int i = 0; i < kHexagonalFnPeriod; i++) {
      as_param.d_[i] = Distribution{ DistributionType::kNoRandom, dist[i], 0.0f };
    }
    as_param.wedge_angle_u_ = upper_alpha;
    as_param.wedge_angle_l_ = lower_alpha;
    c.geom_symmetry_ = DeriveGeometricSymmetry(as_param);
  }
  c.PopulateFromCfGeom();
  return c;
}

Crystal Crystal::CreatePyramid(float upper_alpha, float lower_alpha, float h1, float h2, float h3, const float* dist) {
  return MakePyramidClosedForm(upper_alpha, lower_alpha, h1, h2, h3, dist, nullptr, "CreatePyramid");
}

Crystal Crystal::CreatePyramid(float upper_alpha, float lower_alpha, float h1, float h2, float h3, const float* dist,
                               const GeometricSymmetry& ensemble_symmetry) {
  return MakePyramidClosedForm(upper_alpha, lower_alpha, h1, h2, h3, dist, &ensemble_symmetry, "CreatePyramid");
}

Crystal Crystal::CreatePyramid(float upper_alpha, float lower_alpha, float h1, float h2, float h3) {
  float dist[6]{ 1, 1, 1, 1, 1, 1 };
  return CreatePyramid(upper_alpha, lower_alpha, h1, h2, h3, dist);
}

Crystal Crystal::CreatePyramid(int upper_i1, int upper_i4, int lower_i1, int lower_i4,  // Miller index
                               float h1, float h2, float h3,                            // height
                               const float* dist) {                                     // face distance
  float upper_alpha = MillerIndexToWedgeAngleDeg(upper_i1, upper_i4);
  float lower_alpha = MillerIndexToWedgeAngleDeg(lower_i1, lower_i4);
  return CreatePyramid(upper_alpha, lower_alpha, h1, h2, h3, dist);
}


Crystal::Crystal() {}

Crystal::Crystal(const Crystal& other)
    : config_id_(other.config_id_), fn_period_(other.fn_period_), geom_symmetry_(other.geom_symmetry_),
      poly_face_cnt_(other.poly_face_cnt_), cf_geom_(other.cf_geom_) {
  if (poly_face_cnt_ > 0) {
    poly_face_data_ = std::make_unique<float[]>(poly_face_cnt_ * 4);
    poly_face_n_ = poly_face_data_.get();
    poly_face_d_ = poly_face_data_.get() + poly_face_cnt_ * 3;
    std::memcpy(poly_face_data_.get(), other.poly_face_data_.get(), poly_face_cnt_ * 4 * sizeof(float));
    poly_face_fn_ = std::make_unique<IdType[]>(poly_face_cnt_);
    std::memcpy(poly_face_fn_.get(), other.poly_face_fn_.get(), poly_face_cnt_ * sizeof(IdType));
  }
}

Crystal::Crystal(Crystal&& other) noexcept
    : config_id_(other.config_id_), fn_period_(other.fn_period_), geom_symmetry_(other.geom_symmetry_),
      poly_face_cnt_(other.poly_face_cnt_), poly_face_data_(std::move(other.poly_face_data_)),
      poly_face_fn_(std::move(other.poly_face_fn_)), cf_geom_(other.cf_geom_) {
  if (poly_face_cnt_ > 0) {
    poly_face_n_ = poly_face_data_.get();
    poly_face_d_ = poly_face_data_.get() + poly_face_cnt_ * 3;
  }
  other.poly_face_cnt_ = 0;
  other.poly_face_n_ = nullptr;
  other.poly_face_d_ = nullptr;
}

Crystal& Crystal::operator=(const Crystal& other) {
  if (&other == this) {
    return *this;
  }

  config_id_ = other.config_id_;
  fn_period_ = other.fn_period_;
  geom_symmetry_ = other.geom_symmetry_;

  poly_face_cnt_ = other.poly_face_cnt_;
  if (poly_face_cnt_ > 0) {
    poly_face_data_ = std::make_unique<float[]>(poly_face_cnt_ * 4);
    poly_face_n_ = poly_face_data_.get();
    poly_face_d_ = poly_face_data_.get() + poly_face_cnt_ * 3;
    std::memcpy(poly_face_data_.get(), other.poly_face_data_.get(), poly_face_cnt_ * 4 * sizeof(float));
    poly_face_fn_ = std::make_unique<IdType[]>(poly_face_cnt_);
    std::memcpy(poly_face_fn_.get(), other.poly_face_fn_.get(), poly_face_cnt_ * sizeof(IdType));
  } else {
    poly_face_data_.reset();
    poly_face_n_ = nullptr;
    poly_face_d_ = nullptr;
    poly_face_fn_.reset();
  }
  cf_geom_ = other.cf_geom_;
  return *this;
}

Crystal& Crystal::operator=(Crystal&& other) noexcept {
  if (&other == this) {
    return *this;
  }

  config_id_ = other.config_id_;
  fn_period_ = other.fn_period_;
  geom_symmetry_ = other.geom_symmetry_;

  poly_face_cnt_ = other.poly_face_cnt_;
  poly_face_data_ = std::move(other.poly_face_data_);
  poly_face_fn_ = std::move(other.poly_face_fn_);
  if (poly_face_cnt_ > 0) {
    poly_face_n_ = poly_face_data_.get();
    poly_face_d_ = poly_face_data_.get() + poly_face_cnt_ * 3;
  } else {
    poly_face_n_ = nullptr;
    poly_face_d_ = nullptr;
  }
  other.poly_face_cnt_ = 0;
  other.poly_face_n_ = nullptr;
  other.poly_face_d_ = nullptr;

  cf_geom_ = other.cf_geom_;
  other.cf_geom_ = CrystalGeom{};

  return *this;
}

IdType Crystal::GetFn(IdType poly_idx) const {
  if (poly_idx == kInvalidId || poly_idx >= poly_face_cnt_ || !poly_face_fn_) {
    return kInvalidId;
  }
  return poly_face_fn_[poly_idx];
}

namespace {

// Rotate prism/pyramid faces by the allowed step so the first non-basal face lands on the smallest
// face its orbit can reach (the P-canonical form): with rotations by multiples of p_step, the first
// 0-based pri index f can be sent exactly to f mod p_step. p_step == 1 is the full six-fold case
// (first face -> 0, bit-identical to the rule before geometry was consulted); p_step >= fn_period
// admits no rotation and returns the input. Basal faces (x < 3) pass through unchanged; the input is
// returned verbatim when no non-basal face is present.
std::vector<IdType> PCanonicalShiftByPeriod(const std::vector<IdType>& rp, int fn_period, int p_step) {
  std::vector<IdType> result = rp;
  if (p_step <= 0 || p_step >= fn_period) {
    return result;
  }
  int shift = -1;
  for (auto& x : result) {
    if (x < 3) {
      continue;
    }
    IdType pyr = x / 10;
    int pri0 = static_cast<int>(x % 10) - 3;
    if (shift < 0) {
      shift = pri0 - pri0 % p_step;
    }
    pri0 = (pri0 - shift + fn_period) % fn_period;
    x = pyr * 10 + static_cast<IdType>(pri0 + 3);
  }
  return result;
}

// Whether each symmetry element takes part, given request, ensemble and geometry: the one place the
// three halves are intersected for Crystal's reduction and expansion. `p_applicable` /
// `b_applicable` / `d_applicable` are the orientation ensemble's halves (detail::Is*Applicable);
// `geom` (including geom.b_applicable) is the shape's.
bool PActive(uint8_t symmetry, bool p_applicable) {
  return (symmetry & FilterConfig::kSymP) && p_applicable;
}

bool DActive(uint8_t symmetry, int sigma_a, bool d_applicable, const GeometricSymmetry& geom) {
  return (symmetry & FilterConfig::kSymD) && DMirrorActive(d_applicable, sigma_a, geom);
}

bool BActive(uint8_t symmetry, bool b_applicable, const GeometricSymmetry& geom) {
  return (symmetry & FilterConfig::kSymB) && b_applicable && geom.b_applicable;
}

}  // namespace

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
std::vector<IdType> ReduceRaypathByPeriod(const std::vector<IdType>& rp, uint8_t symmetry, int sigma_a,
                                          bool d_applicable, bool p_applicable, bool b_applicable, int fn_period,
                                          const GeometricSymmetry& geom) {
  if (symmetry == FilterConfig::kSymNone || fn_period < 0) {
    return rp;
  }

  std::vector<IdType> reduced_rp = rp;
  if (PActive(symmetry, p_applicable)) {
    reduced_rp = PCanonicalShiftByPeriod(reduced_rp, fn_period, geom.p_step);
  }

  if (DActive(symmetry, sigma_a, d_applicable, geom)) {
    // σ-clean: reflect every prism face, face_id_new = MirrorFaceIndex(face_id - 3, sigma_a, fn_period) + 3
    std::vector<IdType> rp_reflected = reduced_rp;
    for (auto& x : rp_reflected) {
      if (x < 3) {
        continue;  // skip basal faces; pyramid faces use same pyr/pri decomposition
      }
      IdType pyr = x / 10;
      int pri = static_cast<int>(x % 10) - 3;
      pri = MirrorFaceIndex(pri, sigma_a, fn_period);
      x = pyr * 10 + static_cast<IdType>(pri + 3);
    }
    // When kSymP is also enabled, the D-image may no longer be P-canonical
    // (first pri shifted by sigma_a); re-canonicalize before lex comparison
    // so same orbit always reduces to the same representative.
    if (PActive(symmetry, p_applicable)) {
      rp_reflected = PCanonicalShiftByPeriod(rp_reflected, fn_period, geom.p_step);
    }
    if (rp_reflected < reduced_rp) {
      reduced_rp = rp_reflected;
    }
  }

  if (BActive(symmetry, b_applicable, geom)) {
    // B reflection: basal 1↔2 and pyramid upper[13..18]↔lower[23..28], applied together.
    // Generate the B-reflected candidate and keep the lexicographically smaller one.
    std::vector<IdType> rp_b_reflected = reduced_rp;
    bool changed = false;
    for (auto& x : rp_b_reflected) {
      if (x <= 2) {
        x = 3 - x;
        changed = true;
      } else if (x >= 13 && x <= 18) {
        x += 10;
        changed = true;
      } else if (x >= 23 && x <= 28) {
        x -= 10;
        changed = true;
      }
    }
    if (changed && rp_b_reflected < reduced_rp) {
      reduced_rp = rp_b_reflected;
    }
  }

  return reduced_rp;
}

std::vector<IdType> Crystal::ReduceRaypath(const std::vector<IdType>& rp, uint8_t symmetry) const {
  return ReduceRaypath(rp, symmetry, 0, false, true, true);
}

std::vector<IdType> Crystal::ReduceRaypath(const std::vector<IdType>& rp, uint8_t symmetry, int sigma_a,
                                           bool d_applicable, bool p_applicable, bool b_applicable) const {
  return ReduceRaypathByPeriod(rp, symmetry, sigma_a, d_applicable, p_applicable, b_applicable, fn_period_,
                               geom_symmetry_);
}

std::vector<std::vector<IdType>> Crystal::ExpandRaypath(const std::vector<IdType>& rp, uint8_t symmetry) const {
  return ExpandRaypath(rp, symmetry, 0, false, true, true);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
std::vector<std::vector<IdType>> Crystal::ExpandRaypath(const std::vector<IdType>& rp, uint8_t symmetry, int sigma_a,
                                                        bool d_applicable, bool p_applicable, bool b_applicable) const {
  std::vector<std::vector<IdType>> result;
  result.emplace_back(rp);
  if (symmetry == FilterConfig::kSymNone || fn_period_ < 0) {
    return result;
  }

  if (PActive(symmetry, p_applicable)) {
    // Only the rotations the shape admits: multiples of p_step (none when p_step >= fn_period_).
    const int step = geom_symmetry_.p_step > 0 ? geom_symmetry_.p_step : fn_period_;
    for (int i = step; i < fn_period_; i += step) {
      std::vector<IdType> curr_rp{ rp };
      bool changed = false;
      for (auto& x : curr_rp) {
        if (x < 3) {
          continue;
        }

        IdType pyr = x / 10;
        IdType pri = x % 10;
        pri += fn_period_ - 3;
        pri += i;
        pri %= fn_period_;
        pri += 3;
        x = pyr * 10 + pri;
        changed = true;
      }
      if (changed) {
        result.emplace_back(curr_rp);
      }
    }
  }

  if (DActive(symmetry, sigma_a, d_applicable, geom_symmetry_)) {
    // σ-clean: for each existing variant, generate σ-reflected copy
    auto size = result.size();
    for (size_t i = 0; i < size; i++) {
      auto curr_rp = result[i];
      bool changed = false;
      for (auto& x : curr_rp) {
        if (x < 3) {
          continue;  // skip basal faces; pyramid faces use same pyr/pri decomposition
        }
        IdType pyr = x / 10;
        int pri = static_cast<int>(x % 10) - 3;
        IdType pri_new = static_cast<IdType>(MirrorFaceIndex(pri, sigma_a, fn_period_));
        IdType x_new = pyr * 10 + pri_new + 3;
        if (x_new != x) {
          x = x_new;
          changed = true;
        }
      }
      if (changed) {
        result.emplace_back(curr_rp);
      }
    }
  }

  if (BActive(symmetry, b_applicable, geom_symmetry_)) {
    // B reflection: basal 1↔2, pyramid upper[13..18]↔lower[23..28], prism unchanged.
    // Both swaps are part of the same transformation and applied together.
    auto size = result.size();
    for (size_t i = 0; i < size; i++) {
      auto curr_rp = result[i];
      bool changed = false;
      for (auto& x : curr_rp) {
        if (x <= 2) {
          x = 3 - x;
          changed = true;
        } else if (x >= 13 && x <= 18) {
          x += 10;
          changed = true;
        } else if (x >= 23 && x <= 28) {
          x -= 10;
          changed = true;
        }
      }
      if (changed) {
        result.emplace_back(curr_rp);
      }
    }
  }

  return result;
}

size_t Crystal::PolygonFaceCount() const {
  return poly_face_cnt_;
}

const float* Crystal::GetPolygonFaceNormal() const {
  return poly_face_n_;
}

const float* Crystal::GetPolygonFaceDist() const {
  return poly_face_d_;
}

float Crystal::GetRefractiveIndex(float wl) const {
  return IceRefractiveIndex::Get(wl);
}


// ---- Geometric symmetry of a crystal ensemble -------------------------------------------------

namespace {

// One shape scalar as SyncGroupSampler (simulator.cpp) realizes it: which distribution is drawn,
// and by whom. A slot in sync group g takes the draw of g's leader — its lowest applicable slot in
// ShapeScalar order, which is also the sampler's draw order — and ignores its own distribution, so
// two slots with the same `token` always hold the same value and slots with different tokens are
// independent.
struct ShapeScalarDraw {
  bool present = false;
  Distribution dist{ DistributionType::kNoRandom, 0.0f, 0.0f };
  int token = -1;
};

// "The same shape scalar" at the closed-form geometry's own tolerance (geo3d_closedform.hpp,
// kClosedFormGapToleranceCoefficient): relative to the larger magnitude, never clamped to an
// absolute floor, so it scales with the crystal.
bool SameShapeScalar(float a, float b) {
  const double da = static_cast<double>(a);
  const double db = static_cast<double>(b);
  const double scale = std::max(std::fabs(da), std::fabs(db));
  return std::fabs(da - db) <= kClosedFormGapToleranceCoefficient * static_cast<double>(math::kFloatEps) * scale;
}

// The two slots draw from the same distribution. Fields are read type-erased (raw center/spread):
// a kNoRandom slot has only its constant, every other type both positional parameters.
bool SameDraw(const Distribution& a, const Distribution& b) {
  if (a.type != b.type) {
    return false;
  }
  if (a.type == DistributionType::kNoRandom) {
    return SameShapeScalar(a.center, b.center);
  }
  return SameShapeScalar(a.center, b.center) && SameShapeScalar(a.spread, b.spread);
}

// The resolved draws plus what every permutation test needs, computed once: `cls[s]` names the
// equal-distribution class of slot s among the slots a symmetry can exchange it with (faces with
// faces, the two cone heights with each other), and `any_shared` says whether some random slot
// shares its draw with another — the only case the draw-pairing test has anything to check.
// MakeCrystal derives this for every drawn crystal, so the common no-sync case costs a handful of
// comparisons, not a pass over every slot pair per permutation.
struct ShapeScalarDraws {
  std::array<ShapeScalarDraw, kShapeScalarCount> slot{};
  std::array<int, kShapeScalarCount> cls{};
  bool any_shared = false;
};
using ShapeScalarPerm = std::array<int, kShapeScalarCount>;

bool IsRandomDraw(const ShapeScalarDraw& d) {
  return d.present && d.dist.type != DistributionType::kNoRandom;
}

ShapeScalarDraws ResolveShapeScalarDraws(const std::array<const Distribution*, kShapeScalarCount>& slots,
                                         const int sync_group[kShapeScalarCount]) {
  ShapeScalarDraws draws;
  for (int i = 0; i < kShapeScalarCount; i++) {
    auto& d = draws.slot[i];
    draws.cls[i] = i;
    if (slots[i] == nullptr) {
      continue;
    }
    d.present = true;
    d.token = i;
    d.dist = *slots[i];
    if (sync_group[i] == 0) {
      continue;
    }
    for (int k = 0; k < i; k++) {
      if (slots[k] != nullptr && sync_group[k] == sync_group[i]) {
        d.token = k;
        d.dist = *slots[k];
        break;
      }
    }
  }
  for (int i = 0; i < kShapeScalarCount; i++) {
    const auto& d = draws.slot[i];
    if (IsRandomDraw(d) && d.token != i) {
      draws.any_shared = true;  // i takes an earlier slot's draw
    }
  }
  // Classes among the faces, and between the two cone heights.
  for (int i = kShapeScalarFace0 + 1; i < kShapeScalarCount; i++) {
    for (int k = kShapeScalarFace0; k < i; k++) {
      if (draws.slot[k].present && draws.slot[i].present && SameDraw(draws.slot[k].dist, draws.slot[i].dist)) {
        draws.cls[i] = draws.cls[k];
        break;
      }
    }
  }
  const auto& up = draws.slot[kShapeScalarUpperH];
  const auto& lo = draws.slot[kShapeScalarLowerH];
  if (up.present && lo.present && SameDraw(up.dist, lo.dist)) {
    draws.cls[kShapeScalarLowerH] = draws.cls[kShapeScalarUpperH];
  }
  return draws;
}

// Whether the joint distribution of the shape scalars is unchanged when slot s's value is moved to
// slot perm[s] (perm only ever exchanges faces with faces and cone height with cone height): each
// moved slot's distribution must match its image's, and random slots that share a draw must map to
// slots that share a draw (and vice versa). Constant slots need no pairing — equal constants are
// interchangeable whoever "draws" them.
bool InvariantUnder(const ShapeScalarDraws& draws, const ShapeScalarPerm& perm) {
  for (int s = 0; s < kShapeScalarCount; s++) {
    const int t = perm[s];
    if (t == s) {
      continue;
    }
    if (draws.slot[s].present != draws.slot[t].present || (draws.slot[s].present && draws.cls[s] != draws.cls[t])) {
      return false;
    }
  }
  if (!draws.any_shared) {
    return true;
  }
  for (int s = 0; s < kShapeScalarCount; s++) {
    if (!IsRandomDraw(draws.slot[s])) {
      continue;
    }
    for (int t = s + 1; t < kShapeScalarCount; t++) {
      if (!IsRandomDraw(draws.slot[t])) {
        continue;
      }
      const bool shared = draws.slot[s].token == draws.slot[t].token;
      const bool shared_image = draws.slot[perm[s]].token == draws.slot[perm[t]].token;
      if (shared != shared_image) {
        return false;
      }
    }
  }
  return true;
}

// A permutation of the six face-distance slots (face i -> face_map(i)), identity elsewhere.
template <class FaceMap>
ShapeScalarPerm FacePerm(FaceMap face_map) {
  ShapeScalarPerm perm{};
  for (int s = 0; s < kShapeScalarCount; s++) {
    perm[s] = s;
  }
  for (int i = 0; i < kHexagonalFnPeriod; i++) {
    perm[kShapeScalarFace0 + i] = kShapeScalarFace0 + face_map(i);
  }
  return perm;
}

// P and D: both act on the face distances only (a pyramid's cone faces sit on the same six
// distances, scaled, so they follow).
void DeriveFaceSymmetry(const ShapeScalarDraws& draws, GeometricSymmetry& out) {
  // The common case — a regular hexagon, or six faces drawn i.i.d. with no shared draw — admits
  // every face permutation; answer it without walking the nine.
  const bool one_face_class = std::all_of(draws.cls.begin() + kShapeScalarFace0, draws.cls.end(),
                                          [&draws](int c) { return c == draws.cls[kShapeScalarFace0]; }) &&
                              std::all_of(draws.slot.begin() + kShapeScalarFace0, draws.slot.end(),
                                          [](const ShapeScalarDraw& d) { return d.present; });
  if (one_face_class && !draws.any_shared) {
    out.p_step = kFullHexagonalSymmetry.p_step;
    out.d_valid_sigma_mask = kFullHexagonalSymmetry.d_valid_sigma_mask;
    return;
  }
  out.p_step = kHexagonalFnPeriod;
  // The rotation subgroups of Z6 are generated by 1, 2, 3 or nothing; the first step that works is
  // the generator (a shape invariant under steps 2 and 3 is invariant under 1, found first).
  for (int k : { 1, 2, 3 }) {
    if (InvariantUnder(draws, FacePerm([k](int i) { return (i + k) % kHexagonalFnPeriod; }))) {
      out.p_step = k;
      break;
    }
  }
  out.d_valid_sigma_mask = 0;
  for (int a = 0; a < kHexagonalFnPeriod; a++) {
    if (InvariantUnder(draws, FacePerm([a](int i) { return MirrorFaceIndex(i, a, kHexagonalFnPeriod); }))) {
      out.d_valid_sigma_mask = static_cast<uint8_t>(out.d_valid_sigma_mask | (1u << a));
    }
  }
}

}  // namespace

GeometricSymmetry DeriveGeometricSymmetry(const PrismCrystalParam& param) {
  std::array<const Distribution*, kShapeScalarCount> slots{
    &param.h_,    nullptr,      nullptr,      nullptr,      &param.d_[0],
    &param.d_[1], &param.d_[2], &param.d_[3], &param.d_[4], &param.d_[5],
  };
  const auto draws = ResolveShapeScalarDraws(slots, param.sync_group_);
  GeometricSymmetry g;
  DeriveFaceSymmetry(draws, g);
  // A prism's cross section does not change along the c-axis: the horizontal mirror is always one
  // of its symmetries.
  g.b_applicable = true;
  return g;
}

GeometricSymmetry DeriveGeometricSymmetry(const PyramidCrystalParam& param) {
  std::array<const Distribution*, kShapeScalarCount> slots{
    nullptr,      &param.h_pyr_u_, &param.h_prs_, &param.h_pyr_l_, &param.d_[0],
    &param.d_[1], &param.d_[2],    &param.d_[3],  &param.d_[4],    &param.d_[5],
  };
  const auto draws = ResolveShapeScalarDraws(slots, param.sync_group_);
  GeometricSymmetry g;
  DeriveFaceSymmetry(draws, g);
  // B swaps the upper and lower cones: their heights must be interchangeable and their wedge angles
  // equal — unless neither cone exists, in which case the angles shape nothing.
  ShapeScalarPerm swap{};
  for (int s = 0; s < kShapeScalarCount; s++) {
    swap[s] = s;
  }
  swap[kShapeScalarUpperH] = kShapeScalarLowerH;
  swap[kShapeScalarLowerH] = kShapeScalarUpperH;
  auto certainly_flat = [](const Distribution& d) {
    return d.type == DistributionType::kNoRandom && std::fabs(d.center) <= math::kFloatEps;
  };
  const bool no_cones =
      certainly_flat(draws.slot[kShapeScalarUpperH].dist) && certainly_flat(draws.slot[kShapeScalarLowerH].dist);
  g.b_applicable =
      InvariantUnder(draws, swap) && (no_cones || SameShapeScalar(param.wedge_angle_u_, param.wedge_angle_l_));
  return g;
}


namespace detail {

bool IsRollAnchorAtMultipleOf30(float roll_anchor_deg) {
  float remainder = std::fmod(std::fmod(roll_anchor_deg, 30.0f) + 30.0f, 30.0f);
  return FloatEqual(remainder, 0.0f) || FloatEqual(remainder, 30.0f);
}


bool IsRollMeanAtMultipleOf30(const AxisDistribution& d) {
  // Deliberately type-erased: the anchor slot is read for every DistributionType, not just the
  // Gaussian family, so the generic `center` member is the correct access (a named accessor would
  // assert on the other types). Do NOT read this as "the statistical mean of the roll angle" —
  // for kZigzag it is a tilt offset, for kUniform an interval midpoint.
  return IsRollAnchorAtMultipleOf30(d.roll_dist.center);
}

int ComputeSigmaA(float roll_mean_deg) {
  if (std::fabs(roll_mean_deg) > 1e6f) {
    return 0;
  }
  int n = (static_cast<int>(std::round(roll_mean_deg / 30.0f)) % 6 + 6) % 6;
  return (6 - n) % 6;
}

bool IsDApplicableParams(DistributionType azimuth_type, float azimuth_full_range_deg, float roll_anchor_deg) {
  return IsFullTurnUniform(azimuth_type, azimuth_full_range_deg) && IsRollAnchorAtMultipleOf30(roll_anchor_deg);
}


bool IsDApplicable(const AxisDistribution& d) {
  // Both fields are read type-erased (raw `spread` / `center`, not the named accessors) for the
  // reason IsRollMeanAtMultipleOf30 gives above: the arguments are evaluated before either
  // conjunct has established a type, and the named accessors assert on it.
  return IsDApplicableParams(d.azimuth_dist.type, d.azimuth_dist.spread, d.roll_dist.center);
}

DSymmetryParams DeriveDSymmetryParams(const AxisDistribution& d) {
  DSymmetryParams params;
  params.d_applicable = IsDApplicable(d);
  params.sigma_a = params.d_applicable ? ComputeSigmaA(d.roll_dist.center) : 0;
  return params;
}

bool IsPApplicableParams(DistributionType roll_type, float roll_full_range_deg) {
  return IsFullTurnUniform(roll_type, roll_full_range_deg);
}

bool IsPApplicable(const AxisDistribution& d) {
  // Raw `spread`, for the reason IsDApplicable gives.
  return IsPApplicableParams(d.roll_dist.type, d.roll_dist.spread);
}

bool IsBApplicableParams(DistributionType azimuth_type, float azimuth_full_range_deg, DistributionType latitude_type,
                         float latitude_center_deg, float latitude_full_range_deg) {
  if (!IsFullTurnUniform(azimuth_type, azimuth_full_range_deg)) {
    return false;
  }
  if (IsFullTurnUniform(latitude_type, latitude_full_range_deg)) {
    return true;
  }
  switch (latitude_type) {
    case DistributionType::kNoRandom:
    case DistributionType::kUniform:
    case DistributionType::kGaussian:
    case DistributionType::kGaussianLegacy:
    case DistributionType::kLaplacian:
      return FloatEqual(latitude_center_deg, 0.0f);
    case DistributionType::kZigzag:
      return false;
  }
  return false;
}

bool IsBApplicable(const AxisDistribution& d) {
  // Raw `spread` / `center`, for the reason IsDApplicable gives.
  return IsBApplicableParams(d.azimuth_dist.type, d.azimuth_dist.spread, d.latitude_dist.type, d.latitude_dist.center,
                             d.latitude_dist.spread);
}

}  // namespace detail


}  // namespace lumice
