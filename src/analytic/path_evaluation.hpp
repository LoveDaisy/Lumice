#ifndef LUMICE_ANALYTIC_PATH_EVALUATION_HPP_
#define LUMICE_ANALYTIC_PATH_EVALUATION_HPP_

// The single-path evaluator of liblumice_analytic (doc/analytic-api.md section 4.3): one crystal, one
// concrete face sequence, one pose -> outgoing direction, segment directions, interface
// transmittances and a validity flag. LUMICE_ANALYTIC_EvaluatePath is a thin C wrapper over this;
// fiber continuation and seed search call it directly, so there is one evaluator and the panel and
// the fiber cannot disagree about what a point on a fiber is.
//
// Two stages, split by cost and by the kind of failure each can report:
//   1. BuildFaceNormals — once per crystal: validates the crystal, builds it with the engine's own
//      closed-form factory (so "the engine would build an empty crystal" is decided by the engine's
//      gate, and its warnings reach the host's log callback), and records the double-precision
//      outward normal of every present face slot. A crystal the engine rejects is kInvalidConfig.
//   2. EvaluatePath — per pose: no allocation, no crystal construction, no error codes. Its inputs
//      are already validated (ResolveFaceSequence, ValidateUnitVector, ValidateRotation); what it
//      reports is data, `valid`. The ray chain itself is path_chain.hpp's TracePathChain, which the
//      fiber continuation also runs (in double for the domain, in Jet<3> for the derivative).
//
// Precision is double throughout. The engine's float closed-form result decides which faces exist;
// the normals and the Fresnel factor come from the same formulas the simulator uses, instantiated
// in double (geo3d_closedform.hpp ClosedFormHexFacePlane / ClosedFormConeSlopeFromWedgeDeg,
// optics_shared.h GetReflectRatioT) — doc/analytic-api.md section 5.4 has the measurement that ruled
// out promoting the float normals.

#include <string>

#include "core/geo3d_closedform.hpp"
#include "lumice_analytic_core.h"

namespace lumice::analytic {

enum class Status {
  kOk,
  kInvalidValue,   // a value the caller got wrong: non-finite, non-unit, unused crystal field set, ...
  kInvalidConfig,  // a crystal the engine's closed-form validity gate rejects
};

// Upper bound on face slots across the closed-form crystal kinds (the pyramid's 20; the prism's 8
// are its first 8 slots).
constexpr int kMaxFaceSlots = kClosedFormPyramidFaceCnt;
static_assert(kMaxFaceSlots >= kClosedFormPrismFaceCnt, "prism slots must fit the pyramid layout");

// Longest face sequence the library accepts: the simulator's own bound on the hits one crystal can
// record (core/def.hpp kMaxHits). A longer path is one no simulated ray can take, and the bound
// keeps every size derived from face_count far from overflow.
constexpr int kMaxFaceCount = 64;

// Tolerances of the input checks, taken from LI's reference defaults (LI
// docs/phase1-math-contract.md, "Precision and root": unit_tolerance = rotation_tolerance = 1e-10),
// so a direction or pose LI accepts is accepted here.
constexpr double kUnitTolerance = 1e-10;
constexpr double kRotationTolerance = 1e-10;

// Body-frame outward unit normals of one crystal's present faces, by closed-form slot. A fixed-size
// value: copying it is the whole cost of handing a crystal to a per-pose loop.
struct FaceNormalTable {
  int slot_cnt = 0;
  int face_number[kMaxFaceSlots]{};
  bool present[kMaxFaceSlots]{};
  double normal[kMaxFaceSlots][3]{};

  // Slot holding Lumice face number `fn` when that face bounds this crystal; -1 otherwise (an unknown
  // number, or a face the crystal lacks — e.g. 13..18 on a pyramid without an upper cone).
  int SlotOf(int fn) const;
};

// Upper bound on one face polygon's corners (the engine's CrystalGeom layout).
constexpr int kMaxFaceCorners = 12;

// Body-frame corner polygons of one crystal's present faces, by closed-form slot: the engine's float
// closed-form corners, promoted to double. Only the finite-crystal entry measure reads them
// (entry_measure.hpp); the optics read FaceNormalTable. `min_edge_length` is the shortest polygon
// edge over every present face.
struct FacePolygonTable {
  int corner_cnt[kMaxFaceSlots]{};
  double corner[kMaxFaceSlots][kMaxFaceCorners][3]{};
  double min_edge_length = 0.0;
};

// Stage 1. On kOk, `out` holds the crystal's present faces and, when `polygons` is not null, their
// corner polygons. On any other status both are empty. Fields unused by `kind` must be zero
// (doc/analytic-api.md section 4.1); heights fold to their absolute value, as the simulator folds
// them (simulator.cpp SamplePrismShapeScalars / SamplePyramidShapeScalars).
Status BuildFaceNormals(const LUMICE_ANALYTIC_Crystal& crystal, FaceNormalTable* out,
                        FacePolygonTable* polygons = nullptr);

// The crystal as the kernel's own C++ callers pass it: the fields of LUMICE_ANALYTIC_Crystal with the
// same meaning, units and validation (doc/analytic-api.md section 4.1). It exists because outside
// src/analytic/ the published prefix may not be spelled (scripts/check_policies.py,
// analytic-symbol-scope), so an in-tree caller such as the single-path analysis module
// (src/raypath/) cannot fill the C struct; the overload below converts and forwards, so there is
// still one BuildFaceNormals.
enum class CrystalShapeKind { kPrism, kPyramid };

struct CrystalShape {
  CrystalShapeKind kind = CrystalShapeKind::kPrism;
  double height = 0.0;
  double face_distance[6]{};
  double upper_h = 0.0;
  double lower_h = 0.0;
  double upper_wedge_deg = 0.0;
  double lower_wedge_deg = 0.0;
};

Status BuildFaceNormals(const CrystalShape& shape, FaceNormalTable* out, FacePolygonTable* polygons = nullptr);

// The kernel's interface version, for in-tree callers that record which kernel produced a result.
constexpr int kApiVersion = LUMICE_ANALYTIC_API_VERSION;

// Face numbers -> slots. kInvalidValue when face_count < 2 or any number is not a present face of
// `table`. Consecutive repeats are not rejected: LI's evaluator accepts them, and the geometry
// decides — a repeated face fails the "reaches the face from inside" gate, so the pose is simply
// not valid.
Status ResolveFaceSequence(const FaceNormalTable& table, const int* faces, int face_count, int* slots_out);

// General diagnostic-field resolver. A one-face sequence is the external reflection at that face;
// longer sequences retain ResolveFaceSequence's transmitted/internal-reflection/transmitted
// meaning. The older path/fiber/discovery surfaces intentionally keep their 2..64 contract.
Status ResolveDiagnosticFaceSequence(const FaceNormalTable& table, const int* faces, int face_count, int* slots_out);

// Finite and |v| within kUnitTolerance of 1.
bool ValidateUnitVector(const double v[3]);
// Finite, R^T R within kRotationTolerance of I (entrywise) and det(R) > 0. `r` is row-major.
bool ValidateRotation(const double r[9]);

// Stage 2 outputs, caller-owned. `segment_directions` holds (slot_count + 1) * 3 doubles,
// `interface_transmittances` slot_count doubles. When the path is not valid every output is zeroed.
struct PathOutputs {
  double outgoing_direction[3];
  double fresnel_transmission;
  double* segment_directions;
  double* interface_transmittances;
};

// Stage 2. Returns `valid`: the entry incidence cosine and Snell discriminant, each internal
// face's incidence cosine (the ray reaches it from inside) and the exit incidence cosine and Snell
// discriminant are all > 0 — LI's `validity_margin_names`, in that order, with the same margin
// expressions. An internal face's TIR discriminant gates nothing: a partial reflection keeps the
// path valid and lowers its power. Direction-level validity only: whether a ray at this pose
// actually meets these faces' finite polygons is not checked (neither does LI's evaluator).
//
// Frames: `incident_direction` and `outgoing_direction` are world-frame propagation directions,
// `pose` is row-major body -> world, and the segments (incident, each internal leg, outgoing) are
// in the body frame. Transmittances: T at entry and exit, R at each internal face (1 under TIR),
// unpolarised s/p average; `fresnel_transmission` is their product.
bool EvaluatePath(const FaceNormalTable& table, const int* slots, int slot_count, double refractive_index,
                  const double incident_direction[3], const double pose[9], PathOutputs* out);

// The names of a path's validity margins, in the order the chain records them (path_chain.hpp
// ChainDomain): entry_incidence_cosine, entry_snell_discriminant, internal_<k>_incidence_cosine for
// k = 1 .. face_count - 2, exit_incidence_cosine, exit_snell_discriminant — LI's
// validity_margin_names (LI docs/analytic-parity-fixtures.md section 3.1, `branch_margins`). The
// spelling is a cross-repository contract: LI reads these names. BranchMarginCount is face_count + 2.
int BranchMarginCount(int face_count);
std::string BranchMarginName(int index, int face_count);

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_PATH_EVALUATION_HPP_
