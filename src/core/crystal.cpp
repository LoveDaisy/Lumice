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
#include <numeric>
#include <set>
#include <utility>
#include <vector>

#include "core/crystal_param.hpp"
#include "core/def.hpp"
#include "core/geo3d.hpp"
#include "core/geo3d_closedform.hpp"
#include "core/math.hpp"
#include "core/optics.hpp"
#include "core/shared/lat_path_selection.hpp"
#include "util/fatal.hpp"
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

// Whether each symmetry element takes part, given request, ensemble and geometry: the one place the
// three halves are intersected for Crystal's reduction and expansion. `p_retained` /
// `b_applicable` / `d_applicable` are the orientation ensemble's halves (the P one already carries
// the shape intersection from DeriveSymmetryGating); `geom` (including geom.b_applicable) is the
// shape's.
bool PActive(uint8_t symmetry, const PRotationSet& p_retained) {
  return (symmetry & sym::kSymP) && p_retained.NonTrivial();
}

bool DActive(uint8_t symmetry, int sigma_a, bool d_applicable, const GeometricSymmetry& geom) {
  return (symmetry & sym::kSymD) && DMirrorActive(d_applicable, sigma_a, geom);
}

bool BActive(uint8_t symmetry, bool b_applicable, const GeometricSymmetry& geom) {
  return (symmetry & sym::kSymB) && b_applicable && geom.b_applicable;
}

}  // namespace

// ONE label action on a face sequence: the orbit enumeration's and the expansion's candidate
// generation apply the group element's own ApplyToFace, so the label machinery exists exactly
// once — the element inventory is its authority (MirrorFaceIndex spells the same map
// parameterized by fn_period, which production always passes as the hexagonal 6, the element
// action's own domain).
std::vector<IdType> ApplyLabelAction(const CrystalElement& element, const std::vector<IdType>& sequence) {
  std::vector<IdType> image = sequence;
  for (IdType& face : image) {
    face = element.ApplyToFace(face);
  }
  return image;
}

// The bit-spelled entry of the ONE naming authority: it builds the bit request's generator
// inventory (the same P/D/B activity gates the expansion reads) and delegates to the element-set
// form below — the lexicographic minimum of rp's orbit under the generators they generate. The
// old greedy walk (first-face P-canonicalization, then mirror/B candidate steps) was this same
// orbit minimum in closed four-coset form — its candidates are orbit members, and the first
// differing face between the P-shifted seed and its mirror image sits at a position whose band
// both σ_a and the P shift preserve, so S_0 (which fixes every prism index and flips each ring
// uniformly) cannot reverse that comparison — a fact the three-encoding equivalence sweep in
// test_filter_spec.cpp pins across the widened domain. detail::ReduceBuffer (filter_spec.cpp)
// still spells that closed form for the per-ray matcher's buffer path; the sweep is the standing
// guardrail that the encodings cannot drift. The inventory's domain is the hexagonal 6: every
// caller passes a crystal's FnPeriod(), which is kHexagonalFnPeriod or the no-reduction -1.
std::vector<IdType> ReduceRaypathByPeriod(const std::vector<IdType>& rp, uint8_t symmetry, int sigma_a,
                                          bool d_applicable, PRotationSet p_retained, bool b_applicable, int fn_period,
                                          const GeometricSymmetry& geom) {
  if (symmetry == sym::kSymNone || fn_period < 0) {
    return rp;
  }
  std::vector<CrystalElement> elements;
  if (PActive(symmetry, p_retained)) {
    for (int k = 1; k < kHexagonalFnPeriod; ++k) {
      if (p_retained.Keeps(k)) {
        elements.push_back(CrystalElement{ CrystalElement::Family::kCAxisRotation, k });
      }
    }
  }
  if (DActive(symmetry, sigma_a, d_applicable, geom)) {
    elements.push_back(CrystalElement{ CrystalElement::Family::kVerticalMirror, sigma_a });
  }
  if (BActive(symmetry, b_applicable, geom)) {
    elements.push_back(CrystalElement{ CrystalElement::Family::kRotoMirror, 0 });
  }
  return ReduceRaypathByPeriod(rp, elements, fn_period);
}

std::vector<IdType> Crystal::ReduceRaypath(const std::vector<IdType>& rp, uint8_t symmetry) const {
  return ReduceRaypath(rp, symmetry, 0, false, PRotationSet::Identity(), false);
}

std::vector<IdType> Crystal::ReduceRaypath(const std::vector<IdType>& rp, uint8_t symmetry, int sigma_a,
                                           bool d_applicable, PRotationSet p_retained, bool b_applicable) const {
  return ReduceRaypathByPeriod(rp, symmetry, sigma_a, d_applicable, p_retained, b_applicable, fn_period_,
                               geom_symmetry_);
}

std::vector<std::vector<IdType>> Crystal::ExpandRaypath(const std::vector<IdType>& rp, uint8_t symmetry) const {
  return ExpandRaypath(rp, symmetry, 0, false, PRotationSet::Identity(), false);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
std::vector<std::vector<IdType>> ExpandRaypathByPeriod(const std::vector<IdType>& rp, uint8_t symmetry, int sigma_a,
                                                       bool d_applicable, PRotationSet p_retained, bool b_applicable,
                                                       int fn_period, const GeometricSymmetry& geom) {
  std::vector<std::vector<IdType>> result;
  result.emplace_back(rp);
  if (symmetry == sym::kSymNone || fn_period < 0) {
    return result;
  }

  if (PActive(symmetry, p_retained)) {
    // Only the rotations the retained set keeps — its generating stride already carries the shape
    // intersection (the derivation's), and a stride ≥ fn_period enumerates nothing.
    const int step = p_retained.GeneratorStep();
    for (int i = step; i < fn_period; i += step) {
      std::vector<IdType> curr_rp{ rp };
      bool changed = false;
      for (auto& x : curr_rp) {
        if (x < 3) {
          continue;
        }

        IdType pyr = x / 10;
        IdType pri = x % 10;
        pri += fn_period - 3;
        pri += i;
        pri %= fn_period;
        pri += 3;
        x = pyr * 10 + pri;
        changed = true;
      }
      if (changed) {
        result.emplace_back(curr_rp);
      }
    }
  }

  if (DActive(symmetry, sigma_a, d_applicable, geom)) {
    // σ-clean: each existing variant gains the σ_{sigma_a} mirror's label image — the element
    // inventory's own action, the machinery the reduction reads too.
    auto size = result.size();
    for (size_t i = 0; i < size; i++) {
      const auto image =
          ApplyLabelAction(CrystalElement{ CrystalElement::Family::kVerticalMirror, sigma_a }, result[i]);
      if (image != result[i]) {
        result.emplace_back(std::move(image));
      }
    }
  }

  if (BActive(symmetry, b_applicable, geom)) {
    // B reflection: basal 1↔2, pyramid upper[13..18]↔lower[23..28], prism unchanged — the
    // element action's own image, both swaps applied together.
    auto size = result.size();
    for (size_t i = 0; i < size; i++) {
      const auto image = ApplyLabelAction(CrystalElement{ CrystalElement::Family::kRotoMirror, 0 }, result[i]);
      if (image != result[i]) {
        result.emplace_back(std::move(image));
      }
    }
  }

  return result;
}

std::vector<std::vector<IdType>> Crystal::ExpandRaypath(const std::vector<IdType>& rp, uint8_t symmetry, int sigma_a,
                                                        bool d_applicable, PRotationSet p_retained,
                                                        bool b_applicable) const {
  return ExpandRaypathByPeriod(rp, symmetry, sigma_a, d_applicable, p_retained, b_applicable, fn_period_,
                               geom_symmetry_);
}

std::vector<IdType> LabelEquivalentFaces(IdType face, uint8_t symmetry, int sigma_a, bool d_applicable) {
  const SymmetryGating label =
      DeriveSymmetryGating(SymmetrySemantics::kLabel, kFullHexagonalSymmetry, AxisDistribution{});
  std::vector<IdType> faces;
  for (const auto& rp : ExpandRaypathByPeriod({ face }, symmetry, sigma_a, d_applicable, label.p_retained,
                                              label.b_applicable, kHexagonalFnPeriod, label.geom)) {
    if (std::find(faces.begin(), faces.end(), rp[0]) == faces.end()) {
      faces.push_back(rp[0]);
    }
  }
  return faces;
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

namespace {

// The exact support [lo, hi] of a shape scalar, or false when it is not bounded and exactly known.
// `fold` applies the sampler's |x| (heights fold; face distances are signed, see
// SamplePrismShapeScalars).
bool ShapeScalarSupport(const Distribution& d, bool fold, float& lo, float& hi) {
  switch (d.type) {
    case DistributionType::kNoRandom:
      lo = hi = d.center;
      break;
    case DistributionType::kUniform:
      lo = d.center - d.spread / 2.0f;
      hi = d.center + d.spread / 2.0f;
      break;
    default:
      return false;
  }
  if (fold) {
    const float a = std::fabs(lo);
    const float b = std::fabs(hi);
    lo = (lo <= 0.0f && hi >= 0.0f) ? 0.0f : std::min(a, b);
    hi = std::max(a, b);
  }
  return true;
}

// The six face distances at the corner most favourable to prism slot `favoured` (0..5, or -1 for
// none): that one at its minimum, every other at its maximum. False when any support is unknown.
bool FavourableDistances(const Distribution (&d)[6], int favoured, float out[6]) {
  for (int i = 0; i < 6; i++) {
    float lo = 0.0f;
    float hi = 0.0f;
    if (!ShapeScalarSupport(d[i], false, lo, hi)) {
      return false;
    }
    out[i] = (i == favoured) ? lo : hi;
  }
  return true;
}

}  // namespace

bool CouldFaceExist(const PrismCrystalParam& param, IdType face) {
  if (face < 1 || face > 8) {
    return true;
  }
  float h_lo = 0.0f;
  float h_hi = 0.0f;
  float dist[6]{};
  if (!ShapeScalarSupport(param.h_, true, h_lo, h_hi) ||
      !FavourableDistances(param.d_, face >= 3 ? static_cast<int>(face) - 3 : -1, dist)) {
    return true;
  }
  const ClosedFormPrismResult r = ComputeClosedFormPrism(h_hi, dist);
  for (int slot = 0; slot < kClosedFormPrismFaceCnt; slot++) {
    if (r.face_number[slot] == static_cast<int>(face)) {
      return r.face_present[slot];
    }
  }
  return true;
}

bool CouldFaceExist(const PyramidCrystalParam& param, IdType face) {
  const bool basal = face == 1 || face == 2;
  const bool side = face >= 3 && face <= 8;
  const bool cone = (face >= 13 && face <= 18) || (face >= 23 && face <= 28);
  if (!basal && !side && !cone) {
    return true;
  }
  float h[3][2]{};  // upper cone, prism, lower cone: {lo, hi}
  if (!ShapeScalarSupport(param.h_pyr_u_, true, h[0][0], h[0][1]) ||
      !ShapeScalarSupport(param.h_prs_, true, h[1][0], h[1][1]) ||
      !ShapeScalarSupport(param.h_pyr_l_, true, h[2][0], h[2][1])) {
    return true;
  }
  const int favoured = side ? static_cast<int>(face) - 3 : cone ? static_cast<int>(face % 10) - 3 : -1;
  float dist[6]{};
  if (!FavourableDistances(param.d_, favoured, dist)) {
    return true;
  }
  const float h1 = face == 1 ? h[0][0] : h[0][1];
  const float h3 = face == 2 ? h[2][0] : h[2][1];
  const ClosedFormPyramidResult r =
      ComputeClosedFormPyramid(param.wedge_angle_u_, param.wedge_angle_l_, h1, h[1][1], h3, dist);
  for (int slot = 0; slot < kClosedFormPyramidFaceCnt; slot++) {
    if (r.face_number[slot] == static_cast<int>(face)) {
      return r.face_present[slot];
    }
  }
  return true;
}

namespace {

template <typename Param>
bool CouldFilterMatchFaceImpl(const Param& param, const AxisDistribution& axis, IdType face, uint8_t symmetry) {
  const auto d = detail::DeriveDSymmetryParams(axis);
  for (IdType f : LabelEquivalentFaces(face, symmetry, d.sigma_a, d.d_applicable)) {
    if (CouldFaceExist(param, f)) {
      return true;
    }
  }
  return false;
}

}  // namespace

bool CouldFilterMatchFace(const PrismCrystalParam& param, const AxisDistribution& axis, IdType face, uint8_t symmetry) {
  return CouldFilterMatchFaceImpl(param, axis, face, symmetry);
}

bool CouldFilterMatchFace(const PyramidCrystalParam& param, const AxisDistribution& axis, IdType face,
                          uint8_t symmetry) {
  return CouldFilterMatchFaceImpl(param, axis, face, symmetry);
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

bool GluingRetainsHalfTurn(const AxisDistribution& d) {
  // The latitude centre being a pole, within FloatEqual of the same predicate family.
  auto latitude_centered_at_pole = [](float latitude_center_deg) {
    return FloatEqual(latitude_center_deg, 90.0f) || FloatEqual(latitude_center_deg, -90.0f);
  };
  // Equal azimuth weighting is the same full-turn-uniform primitive the B and D ensemble
  // conditions read: a non-uniform azimuth weights the two pole leaves per sky position.
  if (!IsFullTurnUniform(d.azimuth_dist.type, d.azimuth_dist.spread)) {
    return false;
  }
  // Only a latitude path that actually folds glues the two pole leaves — the fold (LUT flip or
  // kGaussianLegacy's NormalizeLatitude, composed by ComposeAxisAngles' +π) IS the mechanism that
  // mixes roll ψ with ψ+180°. Which paths fold is lat_path::SelectLatPath's statement, the same
  // selector the CPU sampler and both device backends run, so this cannot drift onto a path that
  // never folds: the full-sphere uniform (no fold, no leaf pair) and kNoRandom (a locked zenith
  // feeds one leaf only) answer false here by construction.
  switch (lat_path::SelectLatPath(d).kind) {
    case lat_path::LatPathKind::kLutInverseCdf:
    case lat_path::LatPathKind::kGaussLegacy:
      break;
    default:
      return false;
  }
  // Equal leaves ⟺ the latitude density is symmetric about a pole — core latitude ±90°, the wire
  // zenith 0°/180° pair (the raw half past the pole folds back with exactly the mass that stayed).
  // Zero width is the degenerate exception, the same physical state as kNoRandom's locked zenith:
  // every sample sits on the pole, the raw half past it is empty rather than equal, and nothing
  // folds — so a pole-centred family holds only with non-degenerate support. FloatEqual of the
  // same predicate family; a hair-thin band within its threshold is a missed merge, the safe side.
  // Per-type reading of the centre and width slots, like IsBApplicableParams.
  switch (d.latitude_dist.type) {
    case DistributionType::kUniform:
      return latitude_centered_at_pole(d.latitude_dist.UniformCenter()) && !FloatEqual(d.latitude_dist.spread, 0.0f);
    case DistributionType::kGaussian:
    case DistributionType::kGaussianLegacy:
    case DistributionType::kLaplacian:
      return latitude_centered_at_pole(d.latitude_dist.center) && !FloatEqual(d.latitude_dist.spread, 0.0f);
    case DistributionType::kZigzag:
    case DistributionType::kNoRandom:
      return false;
  }
  return false;
}

}  // namespace detail

int PRotationSet::GeneratorStep() const {
  int step = 0;
  for (int k = 1; k < 6; ++k) {
    if (Keeps(k)) {
      step = std::gcd(step, k);
    }
  }
  return step == 0 ? 6 : step;
}

namespace {

// Source A of the physical P gating: the roll density's own periodicity, asked per element —
// element k survives iff the density is invariant under a k·60° shift. Of this repo's
// distribution families only a full-turn uniform is shift-invariant at all (and then for every k
// at once), so today the loop collapses to Full() or Identity(); the loop is the shape of the
// per-element question, and detail::IsPApplicable is its Z6 extreme (same primitive, one root).
PRotationSet RollRetainedRotations(const Distribution& roll) {
  PRotationSet set{};
  for (int k = 0; k < 6; ++k) {
    if (detail::IsFullTurnUniform(roll.type, roll.spread)) {
      set.Keep(k);
    }
  }
  return set;
}

// The subgroup of Z6 both sources generate together (invariance under either element set implies
// invariance under the group they generate): the multiples of gcd(step_A, step_B).
PRotationSet JoinRetainedSubgroups(const PRotationSet& a, const PRotationSet& b) {
  return PRotationSet::FromStep(std::gcd(a.GeneratorStep(), b.GeneratorStep()));
}

// The shape half of the §2a intersection: a rotation acts on the crystal only if the shape maps
// onto itself under it (multiples of geom.p_step). Intersecting subgroup ⟨s⟩ with the shape's
// multiples of p_step gives the multiples of lcm(s, p_step) — this is what bounces a glued
// half-turn off a face-alternating prism (⟨3⟩ ∩ ⟨2⟩ = the identity: the element survives in the
// measure yet merges no two faces).
PRotationSet RetainedOnShape(const PRotationSet& measure, int shape_p_step) {
  return PRotationSet::FromStep(std::lcm(measure.GeneratorStep(), shape_p_step));
}

}  // namespace

// ---- the element algebra: implementation ----------------------------------------------------
//
// Every formula here is derived once from face 3's normal sitting at crystal-frame azimuth 0
// (the same phase tables the closed-form geometry uses), and the derivation — including the
// improper-conjugation trap (a mirror conjugating a rotation flips the rotation's angle, so
// F_y·Ry(θ)·F_y is Ry(θ), NOT Ry(−θ)) — is recorded in doc/raypath/raypath-elements.md.

namespace {

// cos/sin of a·30° for a = 0..5: the phase of the horizontal half-turn axes and the vertical
// mirror planes (face normals sit on the 60° phase, hexagon vertices on the 30° one; the
// element families need the half-step table the vertex table already carries, spelled flat).
constexpr double kAxis30Cos[6] = { 1.0, kHexSqrt3Half, 0.5, 0.0, -0.5, -kHexSqrt3Half };
constexpr double kAxis30Sin[6] = { 0.0, 0.5, kHexSqrt3Half, 1.0, kHexSqrt3Half, 0.5 };

enum class FaceBand { kBasalUpper, kBasalLower, kPrism, kConeUpper, kConeLower };

FaceBand BandOfFace(IdType face) {
  if (face == 1) {
    return FaceBand::kBasalUpper;
  }
  if (face == 2) {
    return FaceBand::kBasalLower;
  }
  if (face >= 3 && face <= 8) {
    return FaceBand::kPrism;
  }
  if (face >= 13 && face <= 18) {
    return FaceBand::kConeUpper;
  }
  if (face >= 23 && face <= 28) {
    return FaceBand::kConeLower;
  }
  FatalAbort("CrystalElement: face %d is not a legal face number", static_cast<int>(face));
}

// The prism index (0..5) of a face whose band carries one (prism sides, either cone ring).
int PriOfFace(IdType face) {
  const int pri = static_cast<int>(face % 10) - 3;
  if (pri < 0 || pri >= kHexagonalFnPeriod) {
    FatalAbort("CrystalElement: face %d has no prism index", static_cast<int>(face));
  }
  return pri;
}

IdType BandFace(FaceBand band, int pri) {
  switch (band) {
    case FaceBand::kPrism:
      return static_cast<IdType>(pri + 3);
    case FaceBand::kConeUpper:
      return static_cast<IdType>(pri + 13);
    case FaceBand::kConeLower:
      return static_cast<IdType>(pri + 23);
    default:
      FatalAbort("CrystalElement: band %d carries no prism index", static_cast<int>(band));
  }
}

int WrappedMod(int value, int period) {
  return ((value % period) + period) % period;
}

// Rz(60°·k), rows; c/s are the exact 60°-phase table entries (kHexFaceCos/Sin[k]).
void Rz60(int k, double out[9]) {
  const double c = kHexFaceCos[k];
  const double s = kHexFaceSin[k];
  out[0] = c;
  out[1] = -s;
  out[2] = 0.0;
  out[3] = s;
  out[4] = c;
  out[5] = 0.0;
  out[6] = 0.0;
  out[7] = 0.0;
  out[8] = 1.0;
}

void MatMul3(const double a[9], const double b[9], double out[9]) {
  double result[9];
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      result[i * 3 + j] = a[i * 3 + 0] * b[0 * 3 + j] + a[i * 3 + 1] * b[1 * 3 + j] + a[i * 3 + 2] * b[2 * 3 + j];
    }
  }
  std::copy(result, result + 9, out);
}

void Transpose3(const double a[9], double out[9]) {
  out[0] = a[0];
  out[1] = a[3];
  out[2] = a[6];
  out[3] = a[1];
  out[4] = a[4];
  out[5] = a[7];
  out[6] = a[2];
  out[7] = a[5];
  out[8] = a[8];
}

}  // namespace

bool CrystalElement::IsProper() const {
  return family == Family::kIdentity || family == Family::kCAxisRotation || family == Family::kHorizontalHalfTurn;
}

IdType CrystalElement::ApplyToFace(IdType face) const {
  const FaceBand band = BandOfFace(face);
  // The basal faces swap exactly for the elements carrying B (the horizontal mirror's label
  // action); the c-axis rotation and the vertical mirror fix them (their matrices fix ±z).
  if (band == FaceBand::kBasalUpper || band == FaceBand::kBasalLower) {
    const bool swaps = family == Family::kHorizontalHalfTurn || family == Family::kRotoMirror;
    return swaps ? (face == 1 ? IdType{ 2 } : IdType{ 1 }) : face;
  }
  const int pri = PriOfFace(face);
  int new_pri = pri;
  bool ring_swap = false;
  switch (family) {
    case Family::kIdentity:
      break;
    case Family::kCAxisRotation:
      new_pri = WrappedMod(pri + index, kHexagonalFnPeriod);
      break;
    case Family::kHorizontalHalfTurn:
      // B·σ_a: reflect the prism index in the a·30° plane, then swap the cone rings and the
      // basal pair (the prism band has no ring to swap).
      new_pri = WrappedMod(index - pri, kHexagonalFnPeriod);
      ring_swap = true;
      break;
    case Family::kVerticalMirror:
      new_pri = WrappedMod(index - pri, kHexagonalFnPeriod);
      break;
    case Family::kRotoMirror:
      // σ_h·Rz(60k): shift the prism index, then swap the cone rings and the basal pair.
      new_pri = WrappedMod(pri + index, kHexagonalFnPeriod);
      ring_swap = true;
      break;
  }
  FaceBand target = band;
  if (ring_swap) {
    if (band == FaceBand::kConeUpper) {
      target = FaceBand::kConeLower;
    } else if (band == FaceBand::kConeLower) {
      target = FaceBand::kConeUpper;
    }
  }
  return BandFace(target, new_pri);
}

void CrystalElement::BodyMatrix(double out[9]) const {
  switch (family) {
    case Family::kIdentity:
      out[0] = 1.0;
      out[1] = 0.0;
      out[2] = 0.0;
      out[3] = 0.0;
      out[4] = 1.0;
      out[5] = 0.0;
      out[6] = 0.0;
      out[7] = 0.0;
      out[8] = 1.0;
      return;
    case Family::kCAxisRotation:
      Rz60(index, out);
      return;
    case Family::kHorizontalHalfTurn: {
      // The half-turn about the horizontal axis û at azimuth a·30° is 2·ûûᵀ − I (a proper
      // involution). a = 0 spells diag(1, −1, −1), the x-axis half-turn the derivation uses.
      const double ca = kAxis30Cos[index];
      const double sa = kAxis30Sin[index];
      out[0] = 2.0 * ca * ca - 1.0;
      out[1] = 2.0 * ca * sa;
      out[2] = 0.0;
      out[3] = 2.0 * ca * sa;
      out[4] = 2.0 * sa * sa - 1.0;
      out[5] = 0.0;
      out[6] = 0.0;
      out[7] = 0.0;
      out[8] = -1.0;
      return;
    }
    case Family::kVerticalMirror:
      // Rz(60°·a)·F_y: the vertical mirror plane at azimuth a·30°, fixing the basal pair.
      Rz60(index, out);
      out[1] = -out[1];
      out[4] = -out[4];
      out[7] = -out[7];
      return;
    case Family::kRotoMirror:
      // σ_h·Rz(60°·k): the horizontal mirror after the c-axis rotation (σ_h commutes with Rz).
      // k = 3 is the inversion −I.
      Rz60(index, out);
      out[6] = -out[6];
      out[7] = -out[7];
      out[8] = -out[8];
      return;
  }
}

std::vector<CrystalElement> AllCrystalElements() {
  std::vector<CrystalElement> elements;
  elements.reserve(24);
  elements.push_back({ CrystalElement::Family::kIdentity, 0 });
  for (int k = 0; k < kHexagonalFnPeriod; ++k) {
    if (k != 0) {  // P_0 is the identity, already pushed
      elements.push_back({ CrystalElement::Family::kCAxisRotation, k });
    }
    elements.push_back({ CrystalElement::Family::kHorizontalHalfTurn, k });
    elements.push_back({ CrystalElement::Family::kVerticalMirror, k });
    elements.push_back({ CrystalElement::Family::kRotoMirror, k });
  }
  return elements;
}

void CompensatedPose(const CrystalElement& element, const double pose[9], double sun_azimuth_rad, double out[9]) {
  double g[9];
  element.BodyMatrix(g);
  double g_transposed[9];
  Transpose3(g, g_transposed);
  double compensated[9];
  MatMul3(pose, g_transposed, compensated);
  if (element.IsProper()) {
    std::copy(compensated, compensated + 9, out);
    return;
  }
  // M: the world vertical mirror at the sun's azimuth, Rz(2φ)·F_y spelled flat (symmetric, as
  // every mirror is). Only a mirror whose plane contains the sun fixes the incident light, so
  // only such an M turns member gP's mirrored world rays into a scene the same sun lights.
  const double cos2 = std::cos(2.0 * sun_azimuth_rad);
  const double sin2 = std::sin(2.0 * sun_azimuth_rad);
  const double mirror[9] = { cos2, sin2, 0.0, sin2, -cos2, 0.0, 0.0, 0.0, 1.0 };
  MatMul3(mirror, compensated, out);
}

bool ElementFixesSkyDirection(const CrystalElement& element, const double direction[3], double sun_azimuth_rad) {
  if (element.IsProper()) {
    return true;  // σ_g is the identity: every sky direction is fixed
  }
  // Improper: σ_g is the sun's vertical mirror; `direction` is fixed iff it lies in that plane.
  // The plane's normal is the horizontal direction at azimuth φ + 90°, so the test reads
  // cos(altitude)·sin(azimuth − φ): the poles (altitude ±90, cos = 0) are fixed in every
  // vertical plane, and the 1e-12 band absorbs the wrapped azimuths' rounding (see the header
  // comment).
  const double plane_normal[3] = { -std::sin(sun_azimuth_rad), std::cos(sun_azimuth_rad), 0.0 };
  const double off_plane =
      direction[0] * plane_normal[0] + direction[1] * plane_normal[1] + direction[2] * plane_normal[2];
  return std::fabs(off_plane) <= 1e-12;
}

namespace {

// The EXACT layer's anchor-grid membership: the centre must BE the grid point. Any genuine
// offset δ makes the reflection non-preserving (the mirrored pose's density differs by the
// offset's full quadratic — a 5e-6° centre already moves the log density by ~10⁵ nats), so the
// old anchor tolerance is the frozen D FILTER's approximation, never a preservation proof: the
// exact layer checks bit-exact membership and refuses the tolerance band, diverging from the
// per-bit answer by design (documented in raypath-elements.md).
bool RollCentreExactlyOnAnchorGrid(float roll_center_deg) {
  if (!std::isfinite(roll_center_deg)) {
    return false;
  }
  const float grid = std::round(roll_center_deg / 30.0f) * 30.0f;
  return roll_center_deg == grid;
}

// The EXACT layer's equator-grid membership, the zenith-reflection twin of the roll anchor
// rule: a zenith reflection maps the normalized latitude centre c to −c, so the reflection is
// preserving exactly when the centre IS 0 (bit-exact; ±0 compare equal). A genuine offset δ
// moves the reflected band's density by the offset's full quadratic (a 7.6e-6° centre already
// moves the pose log density by ~10⁶ nats), so the old IsBApplicable latitude conjunct —
// FloatEqual to 0, the frozen per-bit B approximation — is never a preservation proof: the
// exact layer refuses the tolerance band, the documented deliberate divergence.
bool LatitudeCentreExactlyOnEquator(float latitude_center_deg) {
  if (!std::isfinite(latitude_center_deg)) {
    return false;
  }
  return latitude_center_deg == 0.0f;
}

// The zenith-reflection half (latitude λ → −λ) of the H_a and S_k families, the exact-layer
// form of IsBApplicable's latitude conjunct: full-turn uniform keeps every reflection; the
// centre-symmetric families keep exactly the equator reflection (bit-exact centre); kZigzag
// has no reflection proof (the roll anchor rule's same conservative refusal).
bool LatitudeLawReflectsAboutEquatorExactly(const AxisDistribution& axis) {
  if (detail::IsFullTurnUniform(axis.latitude_dist.type, axis.latitude_dist.spread)) {
    return true;
  }
  switch (axis.latitude_dist.type) {
    case DistributionType::kNoRandom:
    case DistributionType::kUniform:
    case DistributionType::kGaussian:
    case DistributionType::kGaussianLegacy:
    case DistributionType::kLaplacian:
      return LatitudeCentreExactlyOnEquator(axis.latitude_dist.center);
    case DistributionType::kZigzag:
      return false;
  }
  return false;
}

// The EXACT layer's pole-fold gluing: GluingRetainsHalfTurn with the pole centre bit-exact.
// The fold glues the two pole leaves only for a family whose centre IS the pole — a
// FloatEqual-band centre's mass sits off the pole, the reflection moves it onto an empty
// mirror band, and nothing folds. The old per-bit P answer keeps the tolerance form (the
// frozen approximation); this exact layer refuses what it cannot prove.
bool GluingRetainsHalfTurnExactly(const AxisDistribution& axis) {
  if (!detail::IsFullTurnUniform(axis.azimuth_dist.type, axis.azimuth_dist.spread)) {
    return false;
  }
  switch (lat_path::SelectLatPath(axis).kind) {
    case lat_path::LatPathKind::kLutInverseCdf:
    case lat_path::LatPathKind::kGaussLegacy:
      break;
    default:
      return false;
  }
  const auto centered_exactly_at_pole = [](float latitude_center_deg) {
    return latitude_center_deg == 90.0f || latitude_center_deg == -90.0f;
  };
  switch (axis.latitude_dist.type) {
    case DistributionType::kUniform:
      return centered_exactly_at_pole(axis.latitude_dist.UniformCenter()) && axis.latitude_dist.spread != 0.0f;
    case DistributionType::kGaussian:
    case DistributionType::kGaussianLegacy:
    case DistributionType::kLaplacian:
      return centered_exactly_at_pole(axis.latitude_dist.center) && axis.latitude_dist.spread != 0.0f;
    case DistributionType::kZigzag:
    case DistributionType::kNoRandom:
      return false;
  }
  return false;
}

// The roll reflection half at the canonical centre −ψ_a: full-turn uniform keeps every a; the
// centre-symmetric families (Dirac, uniform band, Gaussian, legacy Gaussian, Laplacian) keep
// exactly the reflection whose fixed point is their centre — c ≡ −30a (mod 180°), the statement
// `ComputeSigmaA(centre) == a` spells — with the centre's grid membership checked EXACTLY (see
// RollCentreExactlyOnAnchorGrid). kZigzag is REFUSED: its declared law |A·sin(t)+B| carries no
// reflection-symmetry proof about any centre (centre 0 with B = 0 even has one-sided support),
// and a missed merge is safe where a wrong one is not. The old per-bit D answer keeps its own
// type-erased tolerance-banded anchor check — the frozen filter approximation; this exact layer
// refuses what it cannot prove, and the deliberate divergence is documented in
// raypath-elements.md.
bool RollLawReflectsAtMinusPsi(const AxisDistribution& axis, int a) {
  if (axis.roll_dist.type == DistributionType::kZigzag) {
    return false;  // the one family without a reflection proof: conservative refusal
  }
  if (detail::IsFullTurnUniform(axis.roll_dist.type, axis.roll_dist.spread)) {
    return true;
  }
  if (!RollCentreExactlyOnAnchorGrid(axis.roll_dist.center)) {
    return false;
  }
  return detail::ComputeSigmaA(axis.roll_dist.center) == a;
}

// The measure half of an element's roll action x → −x − 60a. The action decomposes as
// reflect-about-c after a shift by s for every (s, c) with 2c ≡ s − 60a (mod 360°), so the
// measure keeps the element iff SOME decomposition keeps it on both halves: the shift half
// through the P answer's own invariances (full-turn uniform, or the pole-fold gluing's exact
// 180° — RollLawInvariantUnderShift is that statement), the reflection half through the centre
// families above. The s = 180 branch is what closes the retained set under products: σ_0·P_3 =
// σ_3 keeps a pole-folded axis (shift 180° by the fold, reflect about the anchor 0°) even
// though its CANONICAL reflection centre −90° names nothing — testing only the canonical form
// would drop a provably preserving composite.
bool RollLawSymmetricAboutMinusPsi(const AxisDistribution& axis, int a) {
  if (RollLawReflectsAtMinusPsi(axis, a)) {
    return true;  // the s = 0 decomposition: reflect about −30a directly
  }
  // The s = 180° decomposition: reflect about c = 90° − 30a, kept iff the centre names
  // −30·((a + 3) mod 6) — the same exact-grid statement shifted by the half-turn.
  if (!GluingRetainsHalfTurnExactly(axis) || axis.roll_dist.type == DistributionType::kZigzag) {
    return false;
  }
  if (!RollCentreExactlyOnAnchorGrid(axis.roll_dist.center)) {
    return false;
  }
  return detail::ComputeSigmaA(axis.roll_dist.center) == (a + 3) % 6;
}

// The roll half of P, per element: invariance under the −60k° shift. Full-turn uniform is
// shift-invariant at every k; the pole-fold gluing keeps exactly the 180° shift
// (GluingRetainsHalfTurn); k = 0 shifts nothing.
bool RollLawInvariantUnderShift(const AxisDistribution& axis, int k) {
  if (k == 0) {
    return true;
  }
  if (detail::IsFullTurnUniform(axis.roll_dist.type, axis.roll_dist.spread)) {
    return true;
  }
  return k == 3 && GluingRetainsHalfTurnExactly(axis);
}

// Compose two elements through their body matrices and name the product back in the inventory
// (the matrices are built from the same exact phase tables; the match runs at the 1e-12
// kinematic band the ±√3/2 entries need). Only used inside this file's closure pass.
CrystalElement ComposeElements(const CrystalElement& a, const CrystalElement& b) {
  double ma[9];
  a.BodyMatrix(ma);
  double mb[9];
  b.BodyMatrix(mb);
  double product[9];
  MatMul3(ma, mb, product);
  for (const CrystalElement& candidate : AllCrystalElements()) {
    double mc[9];
    candidate.BodyMatrix(mc);
    bool same = true;
    for (int i = 0; i < 9 && same; ++i) {
      same = std::fabs(product[i] - mc[i]) <= 1e-12;
    }
    if (same) {
      return candidate;
    }
  }
  FatalAbort("CrystalElement: the composition of two D6h members left the inventory");
}

}  // namespace

bool MeasureRetainsElement(const CrystalElement& element, const AxisDistribution& axis_dist) {
  switch (element.family) {
    case CrystalElement::Family::kIdentity:
      return true;
    case CrystalElement::Family::kCAxisRotation:
      // T leaves the azimuth and zenith slots unchanged: the roll shift is the whole condition.
      return RollLawInvariantUnderShift(axis_dist, element.index);
    case CrystalElement::Family::kHorizontalHalfTurn:
      // az shift by 180° + zenith reflection + the roll reflection about −ψ_a (D's anchor
      // half, same index). The zenith half is the EXACT equator-grid statement, not
      // IsBApplicable's FloatEqual latitude conjunct — the frozen per-bit approximation is
      // never a preservation proof (the latitude-centre twin of the roll anchor rule).
      return detail::IsFullTurnUniform(axis_dist.azimuth_dist.type, axis_dist.azimuth_dist.spread) &&
             LatitudeLawReflectsAboutEquatorExactly(axis_dist) &&
             RollLawSymmetricAboutMinusPsi(axis_dist, element.index);
    case CrystalElement::Family::kVerticalMirror:
      // az reflection about the sun's plane (a full-turn uniform is invariant) + the roll
      // reflection; the zenith slot is untouched.
      return detail::IsFullTurnUniform(axis_dist.azimuth_dist.type, axis_dist.azimuth_dist.spread) &&
             RollLawSymmetricAboutMinusPsi(axis_dist, element.index);
    case CrystalElement::Family::kRotoMirror:
      // The zenith-reflection half through the EXACT equator-grid statement (IsBApplicable's
      // tolerance conjunct is the frozen per-bit answer, never a proof) plus the roll shift
      // (none at k = 0, the 180° half-turn at k = 3 through the exact pole-fold gluing).
      return detail::IsFullTurnUniform(axis_dist.azimuth_dist.type, axis_dist.azimuth_dist.spread) &&
             LatitudeLawReflectsAboutEquatorExactly(axis_dist) && RollLawInvariantUnderShift(axis_dist, element.index);
  }
  return false;
}

bool ShapeAdmitsElement(const CrystalElement& element, const GeometricSymmetry& shape) {
  switch (element.family) {
    case CrystalElement::Family::kIdentity:
      return true;
    case CrystalElement::Family::kCAxisRotation:
      return element.index % shape.p_step == 0;
    case CrystalElement::Family::kHorizontalHalfTurn:
      // The composite acts as B and σ_a together: both shape halves must admit it.
      return shape.b_applicable && shape.AllowsMirror(element.index);
    case CrystalElement::Family::kVerticalMirror:
      return shape.AllowsMirror(element.index);
    case CrystalElement::Family::kRotoMirror:
      return shape.b_applicable && element.index % shape.p_step == 0;
  }
  return false;
}

std::vector<CrystalElement> DeriveRetainedElements(const GeometricSymmetry& shape, const AxisDistribution& axis_dist) {
  // The per-family predicates first, then the composition closure: g1 and g2 both preserving
  // means g1·g2 preserves (the measure statement is a group statement), and a composite's
  // canonical form can name no anchor of its own — σ_0·P_3 = σ_3 keeps a pole-folded axis while
  // its canonical reflection centre −90° names nothing. Enumerated to the fixed point over the
  // 24-element inventory the result IS the preserved subgroup's label inventory (both closure
  // factors are shape-admitted, so the product is), what the local-class orbits and their
  // transport facts consume as complete.
  std::vector<CrystalElement> retained;
  for (const CrystalElement& element : AllCrystalElements()) {
    if (MeasureRetainsElement(element, axis_dist) && ShapeAdmitsElement(element, shape)) {
      retained.push_back(element);
    }
  }
  bool grew = true;
  while (grew) {
    grew = false;
    std::vector<CrystalElement> additions;
    for (const CrystalElement& a : retained) {
      for (const CrystalElement& b : retained) {
        const CrystalElement product = ComposeElements(a, b);
        if (std::find(retained.begin(), retained.end(), product) == retained.end() &&
            std::find(additions.begin(), additions.end(), product) == additions.end()) {
          additions.push_back(product);
        }
      }
    }
    if (!additions.empty()) {
      grew = true;
      retained.insert(retained.end(), additions.begin(), additions.end());
    }
  }
  // AllCrystalElements()' inventory order (k-major, per-k P, H, σ, S; identity first): the
  // order the consumers' deterministic tie-breaks read.
  const auto rank = [](const CrystalElement& e) {
    switch (e.family) {
      case CrystalElement::Family::kIdentity:
        return std::pair<int, int>{ 0, 0 };
      case CrystalElement::Family::kCAxisRotation:
        return std::pair<int, int>{ e.index, 0 };
      case CrystalElement::Family::kHorizontalHalfTurn:
        return std::pair<int, int>{ e.index, 1 };
      case CrystalElement::Family::kVerticalMirror:
        return std::pair<int, int>{ e.index, 2 };
      case CrystalElement::Family::kRotoMirror:
        return std::pair<int, int>{ e.index, 3 };
    }
    return std::pair<int, int>{ 6, 4 };
  };
  std::sort(retained.begin(), retained.end(),
            [&rank](const CrystalElement& x, const CrystalElement& y) { return rank(x) < rank(y); });
  return retained;
}

SymmetryGating DeriveSymmetryGating(SymmetrySemantics semantics, const GeometricSymmetry& shape_geom,
                                    const AxisDistribution& axis_dist) {
  if (semantics == SymmetrySemantics::kLabel) {
    return { kFullHexagonalSymmetry, PRotationSet::Full(), true, AllCrystalElements() };
  }
  const PRotationSet measure = JoinRetainedSubgroups(
      RollRetainedRotations(axis_dist.roll_dist),
      detail::GluingRetainsHalfTurn(axis_dist) ? PRotationSet::FromStep(3) : PRotationSet::Identity());
  // One producer, two contracts: the per-bit fields below are the old consumers' frozen answer
  // (their approximation clauses included — never re-derived from the element layer, which
  // would change old outputs); the element inventory is the exact layer beside them.
  return { shape_geom, RetainedOnShape(measure, shape_geom.p_step), detail::IsBApplicable(axis_dist),
           DeriveRetainedElements(shape_geom, axis_dist) };
}

// The element-set forms of the one naming authority (declared beside the per-bit overload in
// the header): the lexicographic minimum of `rp`'s orbit under `elements`' label actions,
// optionally clamped to `row` — a partial-bit row's class lives inside the row's member list
// (the issue's own definition), a complete orbit names the whole class. Plain BFS closure
// terminates at the orbit (row-sized: at most a few dozen face sequences).
std::vector<IdType> ReduceRaypathByPeriod(const std::vector<IdType>& rp, const std::vector<CrystalElement>& elements,
                                          int fn_period, const std::vector<std::vector<IdType>>* row) {
  if (fn_period < 0 || elements.empty()) {
    return rp;
  }
  std::set<std::vector<IdType>> seen;
  std::vector<std::vector<IdType>> queue{ rp };
  seen.insert(rp);
  while (!queue.empty()) {
    const std::vector<IdType> current = std::move(queue.back());
    queue.pop_back();
    for (const CrystalElement& element : elements) {
      const std::vector<IdType> image = ApplyLabelAction(element, current);
      if (row != nullptr && std::find(row->begin(), row->end(), image) == row->end()) {
        continue;  // leaves the row: not a member here (the scope's clamp, not a naming rule)
      }
      if (seen.insert(image).second) {
        queue.push_back(std::move(image));
      }
    }
  }
  return *seen.begin();
}

std::vector<IdType> ReduceRaypathByPeriod(const std::vector<IdType>& rp, const std::vector<CrystalElement>& elements,
                                          int fn_period) {
  return ReduceRaypathByPeriod(rp, elements, fn_period, nullptr);
}

std::vector<IdType> ReduceRaypathByPeriod(const std::vector<IdType>& rp, const std::vector<CrystalElement>& elements,
                                          int fn_period, const std::vector<std::vector<IdType>>& row) {
  return ReduceRaypathByPeriod(rp, elements, fn_period, &row);
}

bool FindLabelMappingElement(const std::vector<IdType>& from, const std::vector<IdType>& to, bool proper_only,
                             CrystalElement* out) {
  if (from.size() != to.size() || out == nullptr) {
    return false;
  }
  for (const CrystalElement& element : AllCrystalElements()) {
    if (proper_only && !element.IsProper()) {
      continue;
    }
    bool maps = true;
    for (size_t i = 0; i < from.size(); ++i) {
      if (element.ApplyToFace(from[i]) != to[i]) {
        maps = false;
        break;
      }
    }
    if (maps) {
      *out = element;
      return true;
    }
  }
  return false;
}


}  // namespace lumice
