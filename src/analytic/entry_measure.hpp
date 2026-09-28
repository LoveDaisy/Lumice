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
// against each other (doc/analytic-api.md section 3), which is why this is a second implementation
// by design, not a fork to be merged.
//
// Two deliberate differences, both documented at their site: the corners are the engine's float
// closed-form polygons promoted to double (LI builds its own polyhedron in double); and the exit gate
// uses the call's refractive index, where LI's uses its package constant N_ICE = 1.31 (the two agree
// at 1.31, the index of every LI fixture).

#include <vector>

#include "analytic/path_evaluation.hpp"

namespace lumice::analytic {

// LI feasibility.EPS_REL: the corridor-area threshold is EPS_REL * (shortest crystal edge)^2.
constexpr double kEntryMeasureEpsRel = 1e-6;

enum class EntryMeasureStatus {
  kOk,
  kEntryBackface,      // the incident ray does not enter the entry face
  kExitCriticalAngle,  // the internal ray cannot leave through the exit face
  kCorridorEmpty,      // no line of that internal direction crosses every face polygon
};

struct EntryMeasure {
  double value = 0.0;  // A_P, absolute area in the crystal's length unit squared; 0 unless kOk
  EntryMeasureStatus status = EntryMeasureStatus::kEntryBackface;
  double area_perp_internal = 0.0;  // the corridor footprint perpendicular to the internal direction
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
  EntryMeasure Evaluate(const double s_body[3], double refractive_index);

  double Eps() const { return eps_; }

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
};

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_ENTRY_MEASURE_HPP_
