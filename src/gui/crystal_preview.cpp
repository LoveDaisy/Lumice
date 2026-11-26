#include "gui/crystal_preview.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "gui/app.hpp"
#include "gui/face_number_overlay.hpp"
#include "gui/gui_constants.hpp"
#include "gui/gui_logger.hpp"
#include "gui/gui_state.hpp"
#include "gui/theme.hpp"
#include "lumice_base.h"
#include "lumice_editor.h"
#include "lumice_scene.h"

namespace lumice::gui {

// ----- Modal Singleton State Inventory (multi-instance refactor entry point) -----
// All file-scope state below is scoped to the single active edit modal. If the
// project grows to support multi-instance preview, every variable here needs
// to move into a per-instance context struct.
//   g_crystal_rotation / g_crystal_zoom / g_crystal_style  — observation trackball controls (O)
//   g_crystal_mesh_hash                                    — upload-skip hash
//   g_last_mesh / g_last_mesh_valid                        — cached mesh for overlay
//   g_axis_gizmo_snapshot                                  — portrait submission record (test seam)
// The crystal's physical representative pose is NOT stored here: it is derived per frame from the
// live axis buffers (RenderCrystalPreviewPane / DrawCrystalAxisGizmo), so g_crystal_rotation
// carries the observation only.

// Crystal preview trackball state
float g_crystal_rotation[16] = {
  1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,
};
float g_crystal_zoom = kDefaultCrystalZoom;
int g_crystal_style = GuiState{}.entry_edit_preview_style;
int g_crystal_mesh_hash = 0;  // Hash of crystal params for change detection

void InitializeCrystalPreviewSession(const GuiState& startup_state) {
  g_crystal_style = startup_state.entry_edit_preview_style;
}

// Cached mesh copy consumed by face-number overlay.
// vertices/edge_face_normals are already Y-Z swapped + AABB normalized
// (BuildCrystalMeshData modifies them in-place before returning).
static LUMICE_CrystalMesh g_last_mesh{};
static bool g_last_mesh_valid = false;

// Salt bases for CrystalParamHash's ten ShapeDist fields (height/prism_h/upper_h/lower_h then the
// six face_distance entries) and the two single-slot alpha salts. This is the single source both
// the hash_shape() call sites below and HashSaltSlotsAreDistinct's compile-time proof read from —
// two independently hand-typed copies of the same literals could drift out of sync (e.g. a call
// site's salt changed without updating the proof's copy) while the assert keeps compiling green,
// silently certifying a layout that is no longer the one actually hashed.
constexpr int kShapeSaltBaseCount = 10;
constexpr int kShapeSaltBases[kShapeSaltBaseCount] = { 31, 37, 43, 49, 59, 63, 67, 71, 75, 79 };
constexpr int kAlphaSaltCount = 2;
constexpr int kAlphaSalts[kAlphaSaltCount] = { 55, 57 };  // upper_alpha / lower_alpha

int CrystalParamHash(const CrystalConfig& c) {
  // Simple hash to detect parameter changes
  int h = static_cast<int>(c.type);
  auto hash_float = [](float f) {
    int i;
    std::memcpy(&i, &f, sizeof(i));
    return i;
  };
  // Each shape field is a ShapeDist: hash all three components ({type, center, spread}) so a change
  // to the distribution type or spread (not just the center) is detected and re-uploads the mesh.
  // A ShapeDist occupies four consecutive salt slots: type/center/spread plus sync_group. The
  // group MUST be hashed — changing only the grouping changes the geometry (synced scalars share
  // one draw), and a hash blind to it leaves the stale mesh on screen, which reads as "the feature
  // does not work" rather than as a caching bug.
  auto hash_shape = [&](const ShapeDist& d, int salt) {
    int hh = static_cast<int>(d.type) * salt;
    hh ^= hash_float(d.center) * (salt + 1);
    hh ^= hash_float(d.spread) * (salt + 2);
    hh ^= d.sync_group * (salt + 3);
    return hh;
  };
  // Salts are spaced ≥4 apart because each ShapeDist consumes four consecutive slots. They were
  // 31/37/41/43 + 47/53 while a ShapeDist was three slots wide; at four, 41's block would have
  // swallowed lower_h's 43. See HashSaltSlotsAreDistinct below — the assertion, not this comment,
  // is what keeps the property true.
  h ^= hash_shape(c.height, kShapeSaltBases[0]);
  h ^= hash_shape(c.prism_h, kShapeSaltBases[1]);
  h ^= hash_shape(c.upper_h, kShapeSaltBases[2]);
  h ^= hash_shape(c.lower_h, kShapeSaltBases[3]);
  h ^= hash_float(c.upper_alpha) * kAlphaSalts[0];
  h ^= hash_float(c.lower_alpha) * kAlphaSalts[1];
  for (int i = 0; i < 6; i++) {
    // Stride 4, not 3: each ShapeDist now spans salt..salt+3, so a stride of 3 would give face i's
    // sync_group slot the same multiplier as face i+1's type slot, letting two changes cancel.
    h ^= hash_shape(c.face_distance[i], kShapeSaltBases[4 + i]);
  }
  return h;
}

// Compile-time proof of the property CrystalParamHash relies on: no two slots share a multiplier.
// Reads kShapeSaltBases/kAlphaSalts directly (the same constants the hash_shape call sites above
// use) rather than a second hand-typed copy, so the proof cannot silently drift from what is
// actually hashed. The slots in use are each ShapeDist base's base..base+3 (ten of them: the four
// isolated scalars plus the six face_distance blocks) and the two single-slot alpha salts.
constexpr bool HashSaltSlotsAreDistinct() {
  constexpr int kSlotCount = kShapeSaltBaseCount * 4 + kAlphaSaltCount;
  int slots[kSlotCount] = {};
  int n = 0;
  for (int base : kShapeSaltBases) {
    for (int k = 0; k < 4; k++) {
      slots[n++] = base + k;
    }
  }
  for (int salt : kAlphaSalts) {
    slots[n++] = salt;
  }
  for (int i = 0; i < kSlotCount; i++) {
    for (int j = i + 1; j < kSlotCount; j++) {
      if (slots[i] == slots[j]) {
        return false;
      }
    }
  }
  return true;
}
static_assert(HashSaltSlotsAreDistinct(),
              "CrystalParamHash salt slots collide; two fields would share a multiplier and could cancel");

bool HasActiveShapeRandomization(const CrystalConfig& c) {
  auto is_random = [](const ShapeDist& d) { return d.type != ShapeDistType::kNoRandom; };
  // Only the height-family fields the current type actually consumes (mirrors BuildCrystalMeshData's
  // type branch); face_distance is consumed by both types.
  if (c.type == CrystalType::kPrism) {
    if (is_random(c.height)) {
      return true;
    }
  } else {
    if (is_random(c.prism_h) || is_random(c.upper_h) || is_random(c.lower_h)) {
      return true;
    }
  }
  for (const auto& fd : c.face_distance) {
    if (is_random(fd)) {
      return true;
    }
  }
  return false;
}

void ResetCrystalView() {
  // Legacy default observation (+20 deg around X). Kept for callers that have no axis preset in
  // scope (main GUI init and test harness setup); the modal's re-seats use
  // ResetCrystalViewToRepresentative, whose identity observation composes with the per-frame
  // representative pose instead of baking one in.
  constexpr float kAngle = 0.35f;  // ~20 degrees
  float c = std::cos(kAngle);
  float s = std::sin(kAngle);
  // Rotation around X axis
  g_crystal_rotation[0] = 1;
  g_crystal_rotation[1] = 0;
  g_crystal_rotation[2] = 0;
  g_crystal_rotation[3] = 0;
  g_crystal_rotation[4] = 0;
  g_crystal_rotation[5] = c;
  g_crystal_rotation[6] = s;
  g_crystal_rotation[7] = 0;
  g_crystal_rotation[8] = 0;
  g_crystal_rotation[9] = -s;
  g_crystal_rotation[10] = c;
  g_crystal_rotation[11] = 0;
  g_crystal_rotation[12] = 0;
  g_crystal_rotation[13] = 0;
  g_crystal_rotation[14] = 0;
  g_crystal_rotation[15] = 1;
  g_crystal_zoom = kDefaultCrystalZoom;
}

// Writer closure: ResetCrystalViewToRepresentative resets the OBSERVATION only — the physical
// representative pose is derived per frame from the live axis buffers, so an identity observation
// lands the composite view on the classified family's representative pose without anything being
// stored. Writers of the observation: modal open on a different crystal (ResetCrystalView was
// here: modal open, app/test init), a chip switch, Reset View, and app/test init. A chip switch
// re-seats the view because the card thumbnail already does — under Immediate its re-render flips
// on every label change, so a modal that kept the old view would be the divergence. Reset
// (parameter reload / Reset All) still does NOT write the observation, nor do parameter edits:
// driving the view from either would jump it mid-edit. If a reload reclassifies the family, the
// physical pose follows on the next frame while the observation holds still.
// a_label_chip_switch_reseats_the_modal_preview_pose enforces the chip half;
// the_reset_row_leaves_the_modal_preview_pose_alone the Reset half. The entry-card thumbnail is a
// non-writer by design: it keeps its own rotation and never touches g_crystal_rotation.
void ResetCrystalViewToRepresentative() {
  g_crystal_rotation[0] = 1;
  g_crystal_rotation[1] = 0;
  g_crystal_rotation[2] = 0;
  g_crystal_rotation[3] = 0;
  g_crystal_rotation[4] = 0;
  g_crystal_rotation[5] = 1;
  g_crystal_rotation[6] = 0;
  g_crystal_rotation[7] = 0;
  g_crystal_rotation[8] = 0;
  g_crystal_rotation[9] = 0;
  g_crystal_rotation[10] = 1;
  g_crystal_rotation[11] = 0;
  g_crystal_rotation[12] = 0;
  g_crystal_rotation[13] = 0;
  g_crystal_rotation[14] = 0;
  g_crystal_rotation[15] = 1;
  g_crystal_zoom = kDefaultCrystalZoom;
}

// Apply incremental rotation from mouse drag. Rotation is composed in world
// coordinates (observation is left-multiplied by Rodrigues(world_axis, angle)) so the
// camera position stays fixed and the whole physical scene — crystal, body-frame
// portrait primitives AND the world-frame ones (az ring, up reference) — turns
// together relative to the user's mental model of world space. The drag therefore
// changes the on-screen projection of everything while preserving the physical
// relations between the groups (Plate's c axis stays coaxial with the ring's
// vertical, Column's stays horizontal): it mutates the observation O only, never
// the crystal's physical representative pose.
//
// Coordinate convention: the GUI mesh frame has a Y-Z swap relative to core
// (see crystal_preview.cpp::BuildCrystalMeshData), so the user's "world +z up"
// corresponds to mesh +y, and "world -y forward (camera direction)" to mesh +z.
// The trackball axis is therefore expressed in mesh coordinates as:
//   drag right (dx>0) → axis = mesh +y (= world +z) → spin around world up
//   drag down  (dy>0) → axis = mesh +x (= world +x, camera right) → top of crystal toward user
//
// Implementation note: the axis components (dy/mag, dx/mag, 0) are numerically
// IDENTICAL to the pre-refactor code. The semantic "world coordinates"
// improvement is not a math change here — it comes from the explicit V_rot /
// model split inside CrystalRenderer::ComputeMvp combined with the Y-Z swap
// equivalence above. This file's previous notes implied a camera-space frame,
// which was a misreading of the mesh swap.
void ApplyTrackballRotation(float dx, float dy) {
  float mag = std::sqrt(dx * dx + dy * dy);
  if (mag < 1e-4f) {
    return;  // ~ angle < 1e-6f under the 0.01 sensitivity scale
  }
  float ax = dy / mag;  // mesh +x component (= world +x, camera-right)
  float ay = dx / mag;  // mesh +y component (= world +z, vertical spin axis)
  float az = 0.0f;      // mesh +z (= world -y, camera-forward) — no roll
  float angle = mag * 0.01f;

  float ca = std::cos(angle);
  float sa = std::sin(angle);

  // Rodrigues rotation matrix (column-major)
  float r[16] = {};
  r[0] = ca + ax * ax * (1 - ca);
  r[1] = ay * ax * (1 - ca) + az * sa;
  r[2] = az * ax * (1 - ca) - ay * sa;
  r[4] = ax * ay * (1 - ca) - az * sa;
  r[5] = ca + ay * ay * (1 - ca);
  r[6] = az * ay * (1 - ca) + ax * sa;
  r[8] = ax * az * (1 - ca) + ay * sa;
  r[9] = ay * az * (1 - ca) - ax * sa;
  r[10] = ca + az * az * (1 - ca);
  r[15] = 1.0f;

  // g_crystal_rotation = r · g_crystal_rotation. Left-multiply means the new
  // rotation is composed in world coordinates (independent of current model
  // orientation). With CrystalRenderer's view = T·V_rot·model split, this
  // achieves "drag rotates the crystal around world axes; camera stays put".
  float tmp[16];
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      float sum = 0.0f;
      for (int k = 0; k < 4; k++) {
        sum += r[i + k * 4] * g_crystal_rotation[k + j * 4];
      }
      tmp[i + j * 4] = sum;
    }
  }
  std::memcpy(g_crystal_rotation, tmp, sizeof(g_crystal_rotation));
}

bool BuildCrystalMeshData(const CrystalConfig& cr, unsigned long long sample_seed, LUMICE_CrystalMesh* out) {
  // Preview and simulation now share the same LUMICE_CrystalParam: build it straight from the GUI
  // config (no stringify → no %.4f precision divergence). Shape fields are first-class ShapeDist,
  // mapped through ToLumiceDistribution so a randomized shape drives the preview mesh exactly as it
  // drives the simulator (given the same sample_seed). The axis fields are irrelevant to the
  // local-frame mesh and left zero-initialized.
  LUMICE_CrystalParam param{};
  param.type = (cr.type == CrystalType::kPrism) ? 0 : 1;
  if (cr.type == CrystalType::kPrism) {
    param.height = ToLumiceDistribution(cr.height);
  } else {
    param.prism_h = ToLumiceDistribution(cr.prism_h);
    param.upper_h = ToLumiceDistribution(cr.upper_h);
    param.lower_h = ToLumiceDistribution(cr.lower_h);
    param.upper_wedge_angle = cr.upper_alpha;  // bare float, not a distribution
    param.lower_wedge_angle = cr.lower_alpha;
  }
  for (int i = 0; i < 6; i++) {
    param.face_distance[i] = ToLumiceDistribution(cr.face_distance[i]);
  }
  // Sync groups (v4.13) via the same shared translation the commit path uses (file_io.cpp's
  // FillCrystalParam). Filled for both types — the slots the current type does not use are
  // dropped by core's canonicalization, and sharing one function is what guarantees the preview
  // and the simulation see the same crystal.
  FillSyncGroupArray(cr, param.sync_group);

  *out = {};
  if (LUMICE_GetCrystalMesh(&param, sample_seed, out) != LUMICE_OK) {
    return false;
  }

  // Y-Z swap: convert from core coordinate system to OpenGL view
  for (int vi = 0; vi < out->vertex_count; vi++) {
    float y = out->vertices[vi * 3 + 1];
    float z = out->vertices[vi * 3 + 2];
    out->vertices[vi * 3 + 1] = z;
    out->vertices[vi * 3 + 2] = -y;
  }
  for (int ei = 0; ei < out->edge_count; ei++) {
    for (int side = 0; side < 2; side++) {
      float* n = &out->edge_face_normals[ei * 6 + side * 3];
      float ny = n[1];
      float nz = n[2];
      n[1] = nz;
      n[2] = -ny;
    }
  }
  // Same Y-Z swap for per-face unit normals so face_number_overlay.cpp reads
  // them in the same GL frame as `vertices` (both feed into ProjectLabelToScreen
  // together — mixed frames flip front/back on the fixed 90° offset).
  for (int fi = 0; fi < out->face_count; fi++) {
    float* n = &out->face_normals[fi * 3];
    float ny = n[1];
    float nz = n[2];
    n[1] = nz;
    n[2] = -ny;
  }

  // AABB normalization: scale to fit unit cube
  if (out->vertex_count > 0) {
    float min_x = out->vertices[0], max_x = out->vertices[0];
    float min_y = out->vertices[1], max_y = out->vertices[1];
    float min_z = out->vertices[2], max_z = out->vertices[2];
    for (int vi = 1; vi < out->vertex_count; vi++) {
      float x = out->vertices[vi * 3];
      float y = out->vertices[vi * 3 + 1];
      float z = out->vertices[vi * 3 + 2];
      min_x = std::min(min_x, x);
      max_x = std::max(max_x, x);
      min_y = std::min(min_y, y);
      max_y = std::max(max_y, y);
      min_z = std::min(min_z, z);
      max_z = std::max(max_z, z);
    }
    float extent = std::max({ max_x - min_x, max_y - min_y, max_z - min_z });
    if (extent > 1e-6f) {
      float scale = 1.0f / extent;
      for (int vi = 0; vi < out->vertex_count; vi++) {
        out->vertices[vi * 3] *= scale;
        out->vertices[vi * 3 + 1] *= scale;
        out->vertices[vi * 3 + 2] *= scale;
      }
    }
  }

  return true;
}

int BuildAndUploadCrystalMesh(const CrystalConfig& cr, unsigned long long sample_seed) {
  LUMICE_CrystalMesh mesh{};
  if (!BuildCrystalMeshData(cr, sample_seed, &mesh)) {
    return 0;
  }

  g_crystal_renderer.UpdateMesh(mesh.vertices, mesh.vertex_count, mesh.edges, mesh.edge_count, mesh.triangles,
                                mesh.triangle_count, mesh.edge_face_normals);
  g_last_mesh = mesh;
  g_last_mesh_valid = true;
  return CrystalParamHash(cr);
}

const LUMICE_CrystalMesh* GetLastCrystalMesh() {
  return g_last_mesh_valid ? &g_last_mesh : nullptr;
}

void ResetLastCrystalMesh() {
  g_last_mesh_valid = false;
  g_last_mesh = {};
}

// ============================================================
// Axis family-DOF portrait (task 841)
// ============================================================

namespace {

// The body-frame c axis: the body z convention. One constant, two documented readings — as a
// BODY-frame direction it is the c axis the Column/Plate/Parry/Custom portraits draw; read as a
// WORLD-frame direction it is the world vertical the az ring's normal and the up reference express
// (zenith is measured from world +z, so both are "the z convention of their frame"). Both reach
// the mesh frame through CoreDirToMeshDir below; neither is ever spelled as a mesh-frame literal.
constexpr float kBodyZDir[3] = { 0.0f, 0.0f, 1.0f };

// Core→mesh direction mapping — the same Y-Z swap BuildCrystalMeshData applies to vertices and
// normals ((x, y, z) → (x, z, −y)), named here so the portrait's constants cannot drift from the
// mesh they are drawn over.
void CoreDirToMeshDir(const float core[3], float out_mesh[3]) {
  out_mesh[0] = core[0];
  out_mesh[1] = core[2];
  out_mesh[2] = -core[1];
}

void CrossProduct(const float a[3], const float b[3], float out[3]) {
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}

float DotProduct(const float a[3], const float b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

float VectorLength(const float a[3]) {
  return std::sqrt(DotProduct(a, a));
}

// Orthonormal basis {u, w} of the plane perpendicular to `axis`, with u taken from the projection
// of `preferred` onto that plane when the two are not near-parallel (so arcs start from an
// orientation the reader can relate to), and a fallback perpendicular otherwise.
void PerpendicularBasis(const float axis[3], const float preferred[3], float out_u[3], float out_w[3]) {
  CrossProduct(preferred, axis, out_u);
  float len = VectorLength(out_u);
  if (len < 0.1f) {
    // axis ∥ preferred: any perpendicular works; pick one off the least-aligned cardinal axis.
    const float alt[3] = { 1.0f, 0.0f, 0.0f };
    CrossProduct(alt, axis, out_u);
    len = VectorLength(out_u);
    if (len < 1e-6f) {
      const float alt2[3] = { 0.0f, 0.0f, 1.0f };
      CrossProduct(alt2, axis, out_u);
      len = VectorLength(out_u);
    }
  }
  for (int i = 0; i < 3; ++i) {
    out_u[i] /= len;
  }
  CrossProduct(axis, out_u, out_w);
}

struct AxisReferenceGeometry {
  bool attempted = false;
  bool valid = false;
  float h3_core[3] = { 0.0f, 0.0f, 0.0f };  // unit long-diagonal direction, core body frame
};

// The Lowitz characteristic axis, derived once per process from the face convention itself rather
// than spelled as an angle anywhere on the GUI side: build a standard reference hex prism through
// the same C API the mesh comes from (LUMICE_GetCrystalMesh — no randomization, six faces present,
// finite height, seed-independent), read the face-NUMBER 4/5 slot normals, and take the normalized
// sum — the shared edge direction of that adjacent pair, which the face convention (side faces at
// i·60°) places along the H_3 half-turn axis. Faces 7/8 are the antipodal pair and must sum to the
// opposite direction. A reference mesh that fails any structural check leaves `valid` false and
// the Lowitz portrait degrades to azimuth-only; hard-coding a direction to paper over a broken
// chain is exactly what this refuses to do. The user's current mesh is deliberately not consulted:
// a randomized or missing-face shape must not move a named axis.
const AxisReferenceGeometry& AxisReferenceGeometryOnce() {
  static AxisReferenceGeometry ref;
  if (ref.attempted) {
    return ref;
  }
  ref.attempted = true;

  LUMICE_CrystalParam param{};
  param.type = 0;  // prism
  param.height = { LUMICE_DIST_NO_RANDOM, 1.0f, 0.0f };
  for (int i = 0; i < 6; ++i) {
    param.face_distance[i] = { LUMICE_DIST_NO_RANDOM, 1.0f, 0.0f };
  }
  LUMICE_CrystalMesh mesh{};
  if (LUMICE_GetCrystalMesh(&param, kPreviewFixedSampleSeed, &mesh) != LUMICE_OK || mesh.face_count <= 0) {
    GUI_LOG_ERROR("axis gizmo: reference hex prism mesh unavailable (face_count={}); Lowitz axis object disabled",
                  mesh.face_count);
    return ref;
  }

  int slot45[2] = { -1, -1 };
  int slot78[2] = { -1, -1 };
  for (int f = 0; f < mesh.face_count; ++f) {
    const int number = mesh.face_numbers_by_face[f];
    if (number == 4) {
      slot45[0] = f;
    } else if (number == 5) {
      slot45[1] = f;
    } else if (number == 7) {
      slot78[0] = f;
    } else if (number == 8) {
      slot78[1] = f;
    }
  }
  if (slot45[0] < 0 || slot45[1] < 0 || slot78[0] < 0 || slot78[1] < 0) {
    GUI_LOG_ERROR("axis gizmo: reference prism missing face numbers 4/5/7/8; Lowitz axis object disabled");
    return ref;
  }
  const float* n4 = &mesh.face_normals[slot45[0] * 3];
  const float* n5 = &mesh.face_normals[slot45[1] * 3];
  const float* n7 = &mesh.face_normals[slot78[0] * 3];
  const float* n8 = &mesh.face_normals[slot78[1] * 3];
  const float sum[3] = { n4[0] + n5[0], n4[1] + n5[1], n4[2] + n5[2] };
  const float anti[3] = { n7[0] + n8[0], n7[1] + n8[1], n7[2] + n8[2] };
  const float sum_len = VectorLength(sum);
  const float anti_len = VectorLength(anti);
  // Structural checks: the sum of two ADJACENT side-face normals (60° apart) has length 2·cos30°
  // = √3 — any other pairing of the six falls at or below 1.0; the antipodal pair's sum must
  // oppose it; and the axis is horizontal by construction. A violation means the numbering
  // transmission broke somewhere between the closed-form tables and this mesh, which the unit
  // test chain pins separately; production refuses to draw on a broken chain.
  constexpr float kHexAdjacentPairSumLen = 1.73205f;  // √3
  if (sum_len < kHexAdjacentPairSumLen - 0.2f || sum_len > kHexAdjacentPairSumLen + 0.2f || anti_len < 1.0f) {
    GUI_LOG_ERROR("axis gizmo: face-4/5 normal sum length {} not an adjacent pair; Lowitz axis object disabled",
                  sum_len);
    return ref;
  }
  const float dot = DotProduct(sum, anti) / (sum_len * anti_len);
  if (dot > -0.99f) {
    GUI_LOG_ERROR("axis gizmo: face-7/8 sum not antipodal to face-4/5 sum (dot={}); Lowitz axis object disabled", dot);
    return ref;
  }
  float unit[3] = { sum[0] / sum_len, sum[1] / sum_len, sum[2] / sum_len };
  if (std::fabs(unit[2]) > 1e-2f) {
    GUI_LOG_ERROR("axis gizmo: derived Lowitz axis not horizontal (z={}); Lowitz axis object disabled", unit[2]);
    return ref;
  }
  for (int i = 0; i < 3; ++i) {
    ref.h3_core[i] = unit[i];
  }
  ref.valid = true;
  return ref;
}

// ----- portrait geometry constants (mesh units; the mesh is AABB-normalized to extent ≤ 1) -----
constexpr float kGizmoAxisHalfLength = 0.85f;    // axis line half-length, past the crystal's 0.5
constexpr float kGizmoExposedStart = 0.55f;      // where the "outside the crystal" stub begins
constexpr float kGizmoRingRadius = 0.92f;        // az ring radius, outside the unit cube
constexpr float kGizmoUpRefHeight = 0.42f;       // world-up reference segment length
constexpr float kGizmoSmallArcRadius = 0.16f;    // small DOF arrow radius at the axis tip
constexpr float kGizmoRingStartDeg = 125.0f;     // az ring arc placement (gap faces the viewer at
constexpr float kGizmoRingSweepDeg = 290.0f;     // the identity observation; visual, not semantic)
constexpr float kGizmoSpinSweepDeg = 250.0f;     // spin arrow: long arc, one arrowhead
constexpr float kGizmoTiltHalfSweepDeg = 28.0f;  // tilt arrow: short arc, arrowheads both ends

// Canvas content colors (visual-language §7 exemption: drawn over the render image, judged
// against the image — the face-number overlay's fixed colors are the same precedent). Amber
// carries the body-frame group (axis + small DOF arrow), green the world-frame group (az ring +
// up reference); neither borrows the good/warning/destructive semantic hues, and neither tracks
// the palette.
constexpr ImU32 kGizmoAxisColor = IM_COL32(255, 176, 64, 255);
constexpr ImU32 kGizmoAxisWeakColor = IM_COL32(255, 176, 64, 80);
constexpr ImU32 kGizmoWorldColor = IM_COL32(130, 225, 160, 235);
constexpr ImU32 kGizmoWorldWeakColor = IM_COL32(130, 225, 160, 110);
constexpr ImU32 kGizmoUpTextColor = IM_COL32(130, 225, 160, 200);

// Screen-space V arrowhead at `tip_px`, pointing along `dir_px`. `size_px` is the wing length.
void DrawArrowHead(ImDrawList* draw_list, const ImVec2& tip_px, const ImVec2& dir_px, float size_px, ImU32 color,
                   float thickness) {
  const float len = std::sqrt(dir_px.x * dir_px.x + dir_px.y * dir_px.y);
  if (len < 1e-4f) {
    return;
  }
  const float dx = dir_px.x / len;
  const float dy = dir_px.y / len;
  // Wings at ±25° off the reversed direction.
  constexpr float kHalfAngle = 0.4363f;  // radians
  const float back_x = -dx;
  const float back_y = -dy;
  const float c = std::cos(kHalfAngle);
  const float s = std::sin(kHalfAngle);
  const float wing1_x = back_x * c - back_y * s;
  const float wing1_y = back_x * s + back_y * c;
  const float wing2_x = back_x * c + back_y * s;
  const float wing2_y = -back_x * s + back_y * c;
  draw_list->AddLine(tip_px, ImVec2(tip_px.x + wing1_x * size_px, tip_px.y + wing1_y * size_px), color, thickness);
  draw_list->AddLine(tip_px, ImVec2(tip_px.x + wing2_x * size_px, tip_px.y + wing2_y * size_px), color, thickness);
}

// Sample the arc p(t) = center + radius·(cos t · u + sin t · w), project every sample through
// `mvp`, stroke the polyline, and (optionally) report the end tangents for arrowheads. Returns
// false — drawing nothing — when any sample degenerates: a partial arc would read as a wrong
// primitive rather than a clipped one.
bool StrokeArc(ImDrawList* draw_list, const float mvp[16], float image_pos_x, float image_pos_y, float image_px,
               const float center[3], const float u[3], const float w[3], float radius, float a0_rad, float a1_rad,
               int segments, ImU32 color, float thickness, ImVec2* out_start_dir, ImVec2* out_end_dir) {
  ImVec2 pts[33];
  for (int i = 0; i <= segments; ++i) {
    const float t = a0_rad + (a1_rad - a0_rad) * static_cast<float>(i) / static_cast<float>(segments);
    const float ct = std::cos(t);
    const float st = std::sin(t);
    const float p[3] = { center[0] + radius * (ct * u[0] + st * w[0]), center[1] + radius * (ct * u[1] + st * w[1]),
                         center[2] + radius * (ct * u[2] + st * w[2]) };
    if (!ProjectCrystalPointToImage(p, mvp, image_pos_x, image_pos_y, image_px, image_px, &pts[i].x, &pts[i].y)) {
      return false;
    }
  }
  draw_list->AddPolyline(pts, segments + 1, color, 0, thickness);
  if (out_start_dir != nullptr) {
    *out_start_dir = ImVec2(pts[1].x - pts[0].x, pts[1].y - pts[0].y);
  }
  if (out_end_dir != nullptr) {
    *out_end_dir = ImVec2(pts[segments].x - pts[segments - 1].x, pts[segments].y - pts[segments - 1].y);
  }
  return true;
}

CrystalAxisGizmoSnapshot g_axis_gizmo_snapshot{};

}  // namespace

const CrystalAxisGizmoSnapshot& GetLastAxisGizmoSnapshot() {
  return g_axis_gizmo_snapshot;
}

void ResetAxisGizmoSnapshot() {
  g_axis_gizmo_snapshot = {};
}

// The mesh-consumption witness: written by the pane at the Render call site, copied into the
// snapshot at publish. A separate holder because Render runs before the portrait draws within the
// same frame; the pair must stay same-frame coherent (both stamped every submitted frame).
float g_mesh_render_rotation[16]{};
int g_mesh_render_frame = -1;

void RecordMeshRenderRotation(const float m[16]) {
  std::memcpy(g_mesh_render_rotation, m, sizeof(g_mesh_render_rotation));
  g_mesh_render_frame = ImGui::GetFrameCount();
}

void DrawCrystalAxisGizmo(const AxisGizmoPlan& plan, const AxisDist live_axis[3], const float observation[16],
                          const float body_rotation[16], ImVec2 image_pos, float image_px, ImDrawList* draw_list,
                          float zoom) {
  CrystalAxisGizmoSnapshot snap;
  snap.frame_index = ImGui::GetFrameCount();
  snap.family = plan.family;
  // The mesh-consumption record is the pane's same-frame write; carry it through untouched.
  std::memcpy(snap.mesh_render_rotation, g_mesh_render_rotation, sizeof(snap.mesh_render_rotation));

  snap.drawn = plan.draw_body_axis || plan.draw_az_ring || plan.small_arrow != AxisGizmoSmallArrow::kNone;
  if (!snap.drawn || draw_list == nullptr || image_px <= 0.0f) {
    g_axis_gizmo_snapshot = snap;
    return;
  }

  // The characteristic axis object (mesh frame). The c axis comes from the body-z convention via
  // the named swap; the Lowitz diagonal comes from the face-convention derivation, and a broken
  // chain degrades the portrait to azimuth-only (ring + up reference) rather than drawing an
  // axis of unverifiable provenance.
  float axis_dir_mesh[3] = { 0.0f, 0.0f, 0.0f };
  bool axis_object_available = true;
  if (plan.family == AxisPreset::kLowitz) {
    const AxisReferenceGeometry& ref = AxisReferenceGeometryOnce();
    axis_object_available = ref.valid;
    if (ref.valid) {
      CoreDirToMeshDir(ref.h3_core, axis_dir_mesh);
    }
  } else {
    CoreDirToMeshDir(kBodyZDir, axis_dir_mesh);
  }

  const int px = static_cast<int>(image_px);
  // Consumption-site records: each group's ComputeMvp and its record read the ONE shared
  // expression below, so a swap at the source flows into the record the tests contradict.
  const float* const body_pose = body_rotation;
  float body_mvp[16];
  CrystalRenderer::ComputeMvp(body_pose, zoom, px, px, body_mvp);
  std::memcpy(snap.body_rotation, body_pose, sizeof(snap.body_rotation));
  const float* const world_pose = observation;
  float world_mvp[16];
  CrystalRenderer::ComputeMvp(world_pose, zoom, px, px, world_mvp);
  std::memcpy(snap.world_rotation, world_pose, sizeof(snap.world_rotation));

  const float kDeg2Rad = CrystalRenderer::kDeg2Rad;

  draw_list->PushClipRect(image_pos, ImVec2(image_pos.x + image_px, image_pos.y + image_px), true);

  // ---- world-frame group (O only): az ring + world-up reference ----
  float up_mesh[3];
  CoreDirToMeshDir(kBodyZDir, up_mesh);  // the world vertical, in mesh coordinates
  if (plan.draw_az_ring) {
    const float ring_center[3] = { 0.0f, 0.0f, 0.0f };
    const float ring_u[3] = { 1.0f, 0.0f, 0.0f };
    const float ring_w[3] = { 0.0f, 0.0f, 1.0f };
    const float a0 = kGizmoRingStartDeg * kDeg2Rad;
    const float a1 = (kGizmoRingStartDeg + kGizmoRingSweepDeg) * kDeg2Rad;
    ImVec2 end_dir(0.0f, 0.0f);
    ImVec2 end_px(0.0f, 0.0f);
    if (StrokeArc(draw_list, world_mvp, image_pos.x, image_pos.y, image_px, ring_center, ring_u, ring_w,
                  kGizmoRingRadius, a0, a1, 32, kGizmoWorldColor, UiPx(2.0f), nullptr, &end_dir)) {
      // The end pixel for the head: reproject the arc's end point (cheap, and StrokeArc keeps its
      // point array private).
      const float t1 = a1;
      const float end_point[3] = { kGizmoRingRadius * std::cos(t1), 0.0f, kGizmoRingRadius * std::sin(t1) };
      if (ProjectCrystalPointToImage(end_point, world_mvp, image_pos.x, image_pos.y, image_px, image_px, &end_px.x,
                                     &end_px.y)) {
        DrawArrowHead(draw_list, end_px, end_dir, UiPx(9.0f), kGizmoWorldColor, UiPx(2.0f));
        // The anchor the tests read is the arc end's ACTUAL projected pixel, from the same mvp
        // the stroke was drawn with — not a test-side reprojection.
        snap.ring_end_px_valid = true;
        snap.ring_end_px[0] = end_px.x;
        snap.ring_end_px[1] = end_px.y;
      }
      snap.az_ring_drawn = true;
      for (int i = 0; i < 3; ++i) {
        snap.ring_normal_mesh[i] = up_mesh[i];
      }
    }
  }
  if (plan.draw_world_up_reference) {
    // A short weak vertical segment standing on the ring plane, with the "up" word above it: the
    // shared-observation world anchor that keeps the big arrow's plane readable under arbitrary
    // observation drag (plan §3.3's lab-readability ruling).
    constexpr float kUpRefAzimuthDeg = 145.0f;
    const float base[3] = { kGizmoRingRadius * std::cos(kUpRefAzimuthDeg * kDeg2Rad), 0.0f,
                            kGizmoRingRadius * std::sin(kUpRefAzimuthDeg * kDeg2Rad) };
    const float top[3] = { base[0] + up_mesh[0] * kGizmoUpRefHeight, base[1] + up_mesh[1] * kGizmoUpRefHeight,
                           base[2] + up_mesh[2] * kGizmoUpRefHeight };
    ImVec2 base_px(0.0f, 0.0f);
    ImVec2 top_px(0.0f, 0.0f);
    if (ProjectCrystalPointToImage(base, world_mvp, image_pos.x, image_pos.y, image_px, image_px, &base_px.x,
                                   &base_px.y) &&
        ProjectCrystalPointToImage(top, world_mvp, image_pos.x, image_pos.y, image_px, image_px, &top_px.x,
                                   &top_px.y)) {
      draw_list->AddLine(base_px, top_px, kGizmoWorldWeakColor, UiPx(1.0f));
      draw_list->AddText(ImVec2(top_px.x + UiPx(3.0f), top_px.y - UiPx(16.0f)), kGizmoUpTextColor, "up");
      snap.world_up_ref_drawn = true;
      for (int i = 0; i < 3; ++i) {
        snap.up_ref_dir_mesh[i] = up_mesh[i];
      }
    }
  }

  // ---- body-frame group (O·R): characteristic axis + small DOF arrow ----
  if (axis_object_available && plan.draw_body_axis) {
    const float axis_min[3] = { -axis_dir_mesh[0] * kGizmoAxisHalfLength, -axis_dir_mesh[1] * kGizmoAxisHalfLength,
                                -axis_dir_mesh[2] * kGizmoAxisHalfLength };
    const float axis_max[3] = { axis_dir_mesh[0] * kGizmoAxisHalfLength, axis_dir_mesh[1] * kGizmoAxisHalfLength,
                                axis_dir_mesh[2] * kGizmoAxisHalfLength };
    const float stub_pos[3] = { axis_dir_mesh[0] * kGizmoExposedStart, axis_dir_mesh[1] * kGizmoExposedStart,
                                axis_dir_mesh[2] * kGizmoExposedStart };
    const float stub_neg[3] = { -stub_pos[0], -stub_pos[1], -stub_pos[2] };
    ImVec2 a_px(0.0f, 0.0f);
    ImVec2 b_px(0.0f, 0.0f);
    ImVec2 sp_px(0.0f, 0.0f);
    ImVec2 sn_px(0.0f, 0.0f);
    if (ProjectCrystalPointToImage(axis_min, body_mvp, image_pos.x, image_pos.y, image_px, image_px, &a_px.x,
                                   &a_px.y) &&
        ProjectCrystalPointToImage(axis_max, body_mvp, image_pos.x, image_pos.y, image_px, image_px, &b_px.x,
                                   &b_px.y) &&
        ProjectCrystalPointToImage(stub_pos, body_mvp, image_pos.x, image_pos.y, image_px, image_px, &sp_px.x,
                                   &sp_px.y) &&
        ProjectCrystalPointToImage(stub_neg, body_mvp, image_pos.x, image_pos.y, image_px, image_px, &sn_px.x,
                                   &sn_px.y)) {
      // Weak through the body, strong on the two exposed stubs: the line reads as attached to the
      // crystal without fighting the wireframe or the face numbers for attention.
      draw_list->AddLine(a_px, b_px, kGizmoAxisWeakColor, UiPx(1.0f));
      draw_list->AddLine(sp_px, b_px, kGizmoAxisColor, UiPx(2.0f));
      draw_list->AddLine(sn_px, a_px, kGizmoAxisColor, UiPx(2.0f));
      snap.body_axis_drawn = true;
      // The tip anchor: the exposed end's actual projected pixel, from the mvp the strokes were
      // drawn with.
      snap.body_tip_px_valid = true;
      snap.body_tip_px[0] = b_px.x;
      snap.body_tip_px[1] = b_px.y;
      for (int i = 0; i < 3; ++i) {
        snap.axis_dir_mesh[i] = axis_dir_mesh[i];
        snap.axis_tip_mesh[i] = axis_max[i];
      }

      if (plan.small_arrow != AxisGizmoSmallArrow::kNone) {
        // The small arrow's rotation axis: spin turns about the axis object itself (the c axis);
        // tilt turns about the family's swing axis — H_3 for Lowitz, and for Custom the zenith
        // DOF's body-frame axis at the live roll anchor, Rz(−roll)·ŷ (which reduces to the H_3
        // direction at roll = 0 — a coincidence of the anchor, not a reclassification).
        float arc_axis[3] = { 0.0f, 0.0f, 0.0f };
        if (plan.small_arrow == AxisGizmoSmallArrow::kSpin) {
          for (int i = 0; i < 3; ++i) {
            arc_axis[i] = axis_dir_mesh[i];
          }
        } else if (plan.family == AxisPreset::kLowitz) {
          for (int i = 0; i < 3; ++i) {
            arc_axis[i] = axis_dir_mesh[i];
          }
        } else {
          const float roll_rad = live_axis[2].mean * kDeg2Rad;
          const float tilt_core[3] = { std::sin(roll_rad), std::cos(roll_rad), 0.0f };
          CoreDirToMeshDir(tilt_core, arc_axis);
        }

        // Orthonormal in-plane basis at the tip. The tilt arc centers on the projection of the
        // world vertical when that is not parallel to the arc axis (the swing's most readable
        // reference); the spin arc just needs any consistent start.
        float arc_u[3];
        float arc_w[3];
        PerpendicularBasis(arc_axis, up_mesh, arc_u, arc_w);
        const float a0 = 0.0f;
        const float a1 = kGizmoSpinSweepDeg * kDeg2Rad;
        const float tilt_half = kGizmoTiltHalfSweepDeg * kDeg2Rad;
        ImVec2 start_dir(0.0f, 0.0f);
        ImVec2 end_dir(0.0f, 0.0f);
        bool stroked = false;
        if (plan.small_arrow == AxisGizmoSmallArrow::kSpin) {
          stroked = StrokeArc(draw_list, body_mvp, image_pos.x, image_pos.y, image_px, axis_max, arc_u, arc_w,
                              kGizmoSmallArcRadius, a0, a1, 20, kGizmoAxisColor, UiPx(2.0f), nullptr, &end_dir);
        } else {
          // Bidirectional: the arc starts at −tilt_half so its middle sits on the up reference.
          stroked = StrokeArc(draw_list, body_mvp, image_pos.x, image_pos.y, image_px, axis_max, arc_u, arc_w,
                              kGizmoSmallArcRadius, -tilt_half, tilt_half, 12, kGizmoAxisColor, UiPx(2.0f), &start_dir,
                              &end_dir);
        }
        if (stroked) {
          // Head placement: reproject the two arc endpoints (StrokeArc keeps its array private).
          const float ct0 = std::cos(plan.small_arrow == AxisGizmoSmallArrow::kSpin ? a0 : -tilt_half);
          const float st0 = std::sin(plan.small_arrow == AxisGizmoSmallArrow::kSpin ? a0 : -tilt_half);
          const float ct1 = std::cos(plan.small_arrow == AxisGizmoSmallArrow::kSpin ? a1 : tilt_half);
          const float st1 = std::sin(plan.small_arrow == AxisGizmoSmallArrow::kSpin ? a1 : tilt_half);
          const float p0[3] = { axis_max[0] + kGizmoSmallArcRadius * (ct0 * arc_u[0] + st0 * arc_w[0]),
                                axis_max[1] + kGizmoSmallArcRadius * (ct0 * arc_u[1] + st0 * arc_w[1]),
                                axis_max[2] + kGizmoSmallArcRadius * (ct0 * arc_u[2] + st0 * arc_w[2]) };
          const float p1[3] = { axis_max[0] + kGizmoSmallArcRadius * (ct1 * arc_u[0] + st1 * arc_w[0]),
                                axis_max[1] + kGizmoSmallArcRadius * (ct1 * arc_u[1] + st1 * arc_w[1]),
                                axis_max[2] + kGizmoSmallArcRadius * (ct1 * arc_u[2] + st1 * arc_w[2]) };
          ImVec2 p0_px(0.0f, 0.0f);
          ImVec2 p1_px(0.0f, 0.0f);
          if (ProjectCrystalPointToImage(p0, body_mvp, image_pos.x, image_pos.y, image_px, image_px, &p0_px.x,
                                         &p0_px.y) &&
              ProjectCrystalPointToImage(p1, body_mvp, image_pos.x, image_pos.y, image_px, image_px, &p1_px.x,
                                         &p1_px.y)) {
            DrawArrowHead(draw_list, p1_px, end_dir, UiPx(8.0f), kGizmoAxisColor, UiPx(2.0f));
            if (plan.small_arrow == AxisGizmoSmallArrow::kTilt) {
              // The start head points AWAY from the arc: its tangent runs into the stroke, and a
              // head reading along it would sit inside the arc instead of marking the swing's
              // other extreme.
              const ImVec2 start_out(-start_dir.x, -start_dir.y);
              DrawArrowHead(draw_list, p0_px, start_out, UiPx(8.0f), kGizmoAxisColor, UiPx(2.0f));
            }
            snap.small_arrow_drawn = true;
            snap.small_arrow_kind = plan.small_arrow;
            for (int i = 0; i < 3; ++i) {
              snap.small_arc_axis_mesh[i] = arc_axis[i];
            }
          }
        }
      }
    }
  }

  g_axis_gizmo_snapshot = snap;
  draw_list->PopClipRect();
}

}  // namespace lumice::gui
