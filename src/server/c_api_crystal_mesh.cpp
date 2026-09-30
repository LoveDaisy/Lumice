// C API bridge for LUMICE_GetCrystalMesh (declared in lumice_editor.h). Split from the rest of the
// editor bridge because it builds its preview through core's crystal sampler, ns::MakeCrystal,
// which sits at the sim layer: registering this one function there keeps every other editor
// function at the scene layer, where the header is.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <nlohmann/json.hpp>
#include <utility>

#include "config/crystal_config.hpp"  // ns::PrismCrystalParam / PyramidCrystalParam
#include "core/crystal.hpp"
#include "core/trace_ops.hpp"  // ns::MakeCrystal (core single-source crystal sampler)
#include "include/lumice_editor.h"
#include "server/c_api_scene_internal.hpp"  // CrystalShapeToJson

namespace ns = lumice;

// =============== Crystal Mesh ===============

// Reroutes preview-only geometry through the closed-form Crystal factories so
// the LUMICE_CrystalMesh output is built from the parametric face_number /
// face_present / face_vtx tables directly. Replaces the historical pipeline of
//   CreatePrismMesh/CreatePyramidMesh → FillPerFaceTopology (argmax reversal on
//   triangle normals to reconstruct face groups) → FillHexFnMap (argmax again
//   for per-tri face numbers) → triangle-adjacency dihedral edge filter.
// All three reversals are gone: face_numbers per triangle come from the
// Crystal's fn_map_ (parametric, populated from cf_geom_.face_number in
// PopulateFromCfGeom), and face_vtx_pool / face_normals come straight from
// cf_geom_. See doc/crystal-geometry-representation.md §1 for the wider
// "delete the reversal, read the constant" story.
// Fold a 64-bit sample seed into the 32-bit seed RandomNumberGenerator accepts.
// XOR-fold (both halves participate) rather than truncate: truncation would make any
// two seeds that differ only in the high 32 bits collide deterministically; XOR-fold
// reduces that to a ~2^-32 uniform collision probability. This is NOT a "distinct
// sample_seed => distinct mesh" guarantee, only a removal of the deterministic-collision
// class of false negatives.
static uint32_t FoldSampleSeed64(unsigned long long seed) {
  return static_cast<uint32_t>(seed) ^ static_cast<uint32_t>(seed >> 32);
}

LUMICE_ErrorCode LUMICE_GetCrystalMesh(const LUMICE_CrystalParam* crystal, unsigned long long sample_seed,
                                       LUMICE_CrystalMesh* out) {
  if (!crystal || !out) {
    return LUMICE_ERR_NULL_ARG;
  }
  if (crystal->type != 0 && crystal->type != 1) {
    return LUMICE_ERR_INVALID_VALUE;
  }

  // Single mapping table: translate the shape into the core crystal JSON schema, then
  // let the core from_json (already validated by ConfigToJson round-trips) build the
  // CrystalParam variant. This reuses the existing translation instead of maintaining
  // a second, parallel struct-to-struct field map.
  ns::CrystalParam param;
  try {
    const nlohmann::json shape = CrystalShapeToJson(*crystal).at("shape");
    if (crystal->type == 0) {
      param = shape.get<ns::PrismCrystalParam>();
    } else {
      param = shape.get<ns::PyramidCrystalParam>();
    }
  } catch (...) {
    return LUMICE_ERR_INVALID_CONFIG;
  }

  // Sample one concrete shape through the core single-source sampler. A LOCAL RNG
  // instance (not the process singleton) is what makes the determinism contract hold:
  // identical seed + identical param => identical MakeCrystal draw sequence, with no
  // cross-call state carried in a shared generator. For a fully NO_RANDOM param,
  // MakeCrystal never touches the RNG, so sample_seed is a no-op (contract).
  ns::RandomNumberGenerator rng(FoldSampleSeed64(sample_seed));
  const ns::Crystal crystal_obj = ns::MakeCrystal(rng, param);

  // On-demand triangulation: the Crystal no longer stores a triangle mesh
  // (entry-point sampling consumes cf_geom_ corners directly). Geometry export
  // is a cold path (GUI preview, gated by a param hash) so building the mesh
  // here — instead of eagerly in every MakeCrystal — costs nothing on the hot
  // path. A degenerate sample (validation gate rejected) yields a default-constructed
  // Crystal with face_cnt==0; BuildMeshFromCfGeom and the export loops below are all
  // safe (zero-iteration) on that, producing an empty-but-valid mesh and LUMICE_OK.
  const ns::CrystalGeom& g = crystal_obj.CfGeom();
  const ns::detail::BuiltMesh built = ns::detail::BuildMeshFromCfGeom(g);
  const ns::Mesh& mesh = built.mesh;

  auto vtx_cnt = static_cast<int>(mesh.GetVtxCnt());
  if (vtx_cnt > LUMICE_MAX_CRYSTAL_VERTICES) {
    return LUMICE_ERR_INVALID_VALUE;
  }
  out->vertex_count = vtx_cnt;
  if (vtx_cnt > 0) {
    std::memcpy(out->vertices, mesh.GetVtxPtr(0), vtx_cnt * 3 * sizeof(float));
  }

  auto tri_cnt = mesh.GetTriangleCnt();
  if (static_cast<int>(tri_cnt) > LUMICE_MAX_CRYSTAL_TRIANGLES) {
    return LUMICE_ERR_INVALID_VALUE;
  }
  out->triangle_count = static_cast<int>(tri_cnt);
  if (tri_cnt > 0) {
    std::memcpy(out->triangles, mesh.GetTrianglePtr(0), tri_cnt * 3 * sizeof(int));
  }

  // Per-triangle face_number, read straight from cf_geom_. Each triangle's
  // originating face slot (built.tri_face_slot[i]) is, by construction, a
  // present face with >= 3 corners (an absent or sub-triangle face emits no
  // triangle), so cf_geom_.face_number[slot] is always a legal fn — there is
  // no kInvalidId / -1 sentinel case to map anymore.
  for (size_t i = 0; i < tri_cnt; ++i) {
    const int slot = built.tri_face_slot[i];
    out->face_numbers[i] = g.face_number[slot];
  }

  // Per-face polygon topology: walk present slots in cf_geom_, map each face's
  // CCW (x,y,z) vertex to its index in the deduped mesh vertex pool.
  // BuildMeshFromCfGeom used the same coordinates when it built the pool, so a
  // simple linear search with the same 1e-6f tolerance is guaranteed to hit.
  const float* vtx = (vtx_cnt > 0) ? mesh.GetVtxPtr(0) : nullptr;
  constexpr float kDedupTol = 1e-6f;
  auto find_vtx_idx = [&](float x, float y, float z) -> int {
    for (int i = 0; i < vtx_cnt; ++i) {
      float dx = vtx[i * 3 + 0] - x;
      float dy = vtx[i * 3 + 1] - y;
      float dz = vtx[i * 3 + 2] - z;
      if (std::sqrt(dx * dx + dy * dy + dz * dz) < kDedupTol) {
        return i;
      }
    }
    return -1;
  };

  int fi = 0;
  int pool_offset = 0;
  // Track (v_min, v_max) edge → (slot_a, slot_b). Second slot may stay -1 for
  // boundary edges (only in degenerate geometries; well-formed prism/pyramid
  // yields a closed 2-manifold so every polygon edge is shared by exactly two
  // present slots).
  struct EdgeSlotPair {
    int slot_a;
    int slot_b;
  };
  std::map<std::pair<int, int>, EdgeSlotPair> edge_slots;
  int slot_to_fi[ns::kCrystalGeomMaxFaces];
  for (int i = 0; i < ns::kCrystalGeomMaxFaces; ++i) {
    slot_to_fi[i] = -1;
  }

  for (int slot = 0; slot < g.face_cnt; ++slot) {
    if (!g.face_present[slot]) {
      continue;
    }
    int fn = g.face_vtx_cnt[slot];
    if (fn < 3) {
      continue;
    }
    if (fi >= LUMICE_MAX_CRYSTAL_FACES) {
      break;
    }
    if (pool_offset + fn > LUMICE_MAX_CRYSTAL_FACE_VTXPOOL) {
      break;  // pool exhausted
    }

    const float* face_v = g.face_vtx + slot * ns::kCrystalGeomMaxVtxPerFace * 3;
    // Resolve pool indices for this face's CCW vertex list.
    int local_indices[ns::kCrystalGeomMaxVtxPerFace];
    for (int k = 0; k < fn; ++k) {
      int idx = find_vtx_idx(face_v[k * 3 + 0], face_v[k * 3 + 1], face_v[k * 3 + 2]);
      if (idx < 0) {
        return LUMICE_ERR_INVALID_CONFIG;  // should be impossible: BuildMeshFromCfGeom deduped these coords
      }
      local_indices[k] = idx;
      out->face_vtx_pool[pool_offset + k] = idx;
    }

    out->face_numbers_by_face[fi] = g.face_number[slot];
    out->face_vtx_offsets[fi] = pool_offset;
    out->face_vtx_counts[fi] = fn;
    // Face normal from cf_geom_ is already unit outward (populated by the
    // closed-form evaluator + AdaptClosedFormXxxToCrystalGeom).
    out->face_normals[fi * 3 + 0] = g.face_normal[slot * 3 + 0];
    out->face_normals[fi * 3 + 1] = g.face_normal[slot * 3 + 1];
    out->face_normals[fi * 3 + 2] = g.face_normal[slot * 3 + 2];
    slot_to_fi[slot] = fi;
    pool_offset += fn;
    ++fi;

    // Register polygon-boundary edges for this slot.
    for (int k = 0; k < fn; ++k) {
      int a = local_indices[k];
      int b = local_indices[(k + 1) % fn];
      auto key = std::make_pair(std::min(a, b), std::max(a, b));
      auto [it, inserted] = edge_slots.try_emplace(key, EdgeSlotPair{ slot, -1 });
      if (!inserted) {
        if (it->second.slot_b < 0 && it->second.slot_a != slot) {
          it->second.slot_b = slot;
        }
      }
    }
  }
  out->face_count = fi;

  // Emit edges as polygon boundaries (no triangle-adjacency + dihedral-angle
  // threshold — that was a numerical stand-in for "shared by two polygons of
  // different faces", which cf_geom_ tells us directly).
  int edge_cnt = 0;
  for (const auto& [edge, slots] : edge_slots) {
    if (edge_cnt >= LUMICE_MAX_CRYSTAL_EDGES) {
      break;
    }
    out->edges[edge_cnt * 2 + 0] = edge.first;
    out->edges[edge_cnt * 2 + 1] = edge.second;
    // n0 = normal of first adjacent face slot; n1 = normal of second (or same
    // as n0 for boundary edges — only possible in degenerate geometries).
    const float* n0 = g.face_normal + slots.slot_a * 3;
    const float* n1 = (slots.slot_b >= 0) ? (g.face_normal + slots.slot_b * 3) : n0;
    std::memcpy(&out->edge_face_normals[edge_cnt * 6 + 0], n0, 3 * sizeof(float));
    std::memcpy(&out->edge_face_normals[edge_cnt * 6 + 3], n1, 3 * sizeof(float));
    ++edge_cnt;
  }
  out->edge_count = edge_cnt;

  return LUMICE_OK;
}
