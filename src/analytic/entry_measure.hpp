#ifndef LUMICE_ANALYTIC_ENTRY_MEASURE_HPP_
#define LUMICE_ANALYTIC_ENTRY_MEASURE_HPP_

// The finite-crystal entry measure A_P(R) of one face sequence at one pose: the area, perpendicular to
// the incident direction, of the entry points on the entry face whose refracted internal line meets
// every later face of the sequence inside its finite polygon (LI docs/phase1-math-contract.md
// section 7, `entry_measure`). Discovery gates candidates on A > eps (section 9.5.2 / 9.5.4); the
// value itself is kept so a later weight can use it.
//
// A port of LI geometry/entry_measure.py `entry_measure` over the corridor primitives of
// geometry/feasibility.py (corridor_polygons, perp_bases, _ensure_ccw, _clip_halfplane, area_eps),
// step for step and with the same status names in the same gate order, so a disagreement can be
// read against LI line by line. Primitive layer: LI keeps its own copy and the two are checked
// against each other (doc/interfaces/analytic-api.md section 3), which is why this is a second implementation
// by design, not a fork to be merged.
//
// One deliberate difference, documented at its site: the corners are the engine's float closed-form
// polygons promoted to double (LI builds its own polyhedron in double). The exit gate uses the call's
// refractive index, as LI's does.

#include <array>
#include <vector>

#include "analytic/diagnostic_types.hpp"
#include "analytic/path_evaluation.hpp"

namespace lumice::analytic {

// LI feasibility.EPS_REL: the corridor-area threshold is EPS_REL * (shortest crystal edge)^2.
constexpr double kEntryMeasureEpsRel = 1e-6;

// The named outcome of recovering the representative ray from an evaluated corridor
// (RecoverRepresentativePath).
enum class RecoveryStatus {
  kOk,
  kNotEvaluated,  // the diagnostics carry no evaluated corridor geometry: the optical or area
                  // gate refused before the intersection ran, so there is nothing to recover
  kDegenerate,    // the corridor passed its area gate but the recovered points failed an
                  // independent containment invariant on the original crystal — no trajectory
                  // is produced, and none may be approximated
};

const char* RecoveryStatusName(RecoveryStatus status);
// Lower-case reason names of the entry-measure statuses (the refusal vocabulary the corridor's
// consumers spell).
const char* EntryMeasureStatusName(EntryMeasureStatus status);

// The representative ray's collision points in the body frame: contact k sits on the original
// finite face `slots[k]` of the corridor's face sequence — contact 0 is the entry point, the
// last contact the exit point, the ones between the internal reflection points. Segment
// directions and transmittances are EvaluatePath's (path_evaluation.hpp); this type carries the
// geometry only the corridor can produce.
struct RepresentativePath {
  double contacts[kMaxFaceCount][3]{};
  int contact_count = 0;
};


// The unfolded corridor of one face sequence (LI corridor_polygons): the entry face polygon, each
// reflecting face's polygon on the ghost crystal it is met on, and the exit face polygon on the last
// ghost, in the body frame; plus the entry normal and the unfolded exit normal. Built once per
// crystal and path; evaluating a pose allocates nothing once the scratch has grown.
class Corridor {
 public:
  // `slots` as ResolveFaceSequence returns them, 2 <= slot_count <= kMaxFaceCount.
  Corridor(const FaceNormalTable& normals, const FacePolygonTable& polygons, const int* slots, int slot_count);

  // A_P at a pose, given the body-frame incident propagation direction s_body = R^T s (unit) and the
  // refractive index.
  EntryMeasure Evaluate(const double s_body[3], double refractive_index, CorridorDiagnostics* diagnostics = nullptr);

  double Eps() const { return eps_; }

  // Recovers the representative ray of an evaluated pose: the centroid of the corridor
  // intersection (diagnostics from an `Evaluate` call on this corridor that returned kOk)
  // defines, with the internal direction, the one straight line of the unfolded picture; its
  // per-ghost intersections fold back through the construction's own reflection chain to the
  // original finite faces. The recovered points are verified against the ORIGINAL crystal's
  // planes, polygons and half-spaces (independent of the unfolding) — any violation is a named
  // kDegenerate refusal, never an approximate trajectory. Evaluating poses is unchanged: the
  // recovery reads only state recorded at construction and the diagnostics handed in.
  RecoveryStatus RecoverRepresentativePath(const CorridorDiagnostics& diagnostics, RepresentativePath* out) const;

 private:
  std::vector<double> points_;  // every polygon's corners, (x, y, z) packed
  std::vector<int> offsets_;    // polygon k is corners [offsets_[k], offsets_[k + 1])
  double entry_normal_[3];
  double exit_normal_[3];
  double eps_;
  // Clipping scratch, (x, y) packed.
  std::vector<double> poly_;
  std::vector<double> next_;
  std::vector<double> clip_;
  // The recovery's construction-time record, per polygon k: the plane the unfolded line crosses
  // (unit normal and offset, in the unfolded frame — also the k-th fold-back reflection for
  // k >= 1), the original (unplaced) polygon's corners, and the real face's plane. Plus, once
  // per present face, the outward normal and plane offset the whole-crystal containment test
  // walks — all of it the corridor's own authority over its unfolding.
  std::vector<double> plane_normals_;    // 3 per polygon, unfolded frame
  std::vector<double> plane_offsets_;    // 1 per polygon, unfolded frame
  std::vector<double> real_points_;      // every polygon's original corners, (x, y, z) packed
  std::vector<double> real_normals_;     // 3 per polygon, body frame
  std::vector<double> real_offsets_;     // 1 per polygon, body frame
  std::vector<double> crystal_normals_;  // 3 per present face, body frame
  std::vector<double> crystal_offsets_;  // 1 per present face, body frame
  double min_edge_ = 0.0;
};

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_ENTRY_MEASURE_HPP_
