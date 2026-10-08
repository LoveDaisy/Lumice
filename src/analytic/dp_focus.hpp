#ifndef LUMICE_ANALYTIC_DP_FOCUS_HPP_
#define LUMICE_ANALYTIC_DP_FOCUS_HPP_

// Kind-1 focusing: the critical values of D_P with the profile each produces for a random
// orientation, the family_pinned label, and the wavelength-critical table (LI focusing.py,
// ported for scrum 660.4; docs/phase2.md section 10). Two mechanisms make a feature bright or
// singular — the Jacobian one (the level-set measure int dl / |grad D| diverges as delta reaches
// a critical value; a property of the path's D_P field alone) and the dimension collapse (a pose
// density confining the pose to a lower-dimensional family; a property of the density alone) —
// and this layer says which, as data, instead of leaving it implicit in a filter.
//
// The onset profiles (LI PROFILES, verbatim): finite_jump (a non-degenerate interior extremum,
// measure limit 2 pi / sqrt(det H)), log_divergence (an interior saddle), inverse_sqrt_divergence
// (a curve of extrema — the slab rotation's great circle), cone_point (a slab's axis, where D
// grows linearly away), crease (a mirror slab's plane, a kink with no focusing),
// boundary_onset (a restricted extremum or corner of dU_P, a square-root cusp) and degenerate
// (anything else with a vanishing gradient). Jacobian focusing = the divergence profiles.
//
// family_pinned is a third, orthogonal label: the density's sigma -> 0 support runs u round one
// circle about a body axis a (a zenith Gaussian at a pole pins e3; a roll-locked density pins
// the body image of the zenith; a zenith Gaussian off a pole pins nothing), and when the path's
// refractions cancel (wedge 0) and its fold matrix commutes with the rotations about a, that
// circle lies in ONE level set — the family's image is a single deviation (Parry's 1-6-2 at body
// x is Lumice corpus C13). A mirror commutes with the rotations about its own normal, so
// mirror-fold paths pin on the axis the family locks; 3-5 collapses too but its fold matrix is I
// with wedge 60 deg — the wedge guard keeps a commuting-with-everything I out.
//
// The interior critical points of a NON-slab path come from a lattice Newton search (LI
// dp_field.field interior_critical_points, the gap 660.2's summary explicitly left here): seeds
// are the U_P points of a Fibonacci lattice, the iteration is a damped tangent-space Newton on
// grad_{S^2} D_P = 0, and converged iterates are re-checked for membership before they count.
// A slab path's interior set stays 660.2's closed form (SlabInteriorCriticalPoints).
//
// JAX is the authority, this is the derived implementation: constants, thresholds, profile
// vocabulary and judgement order are LI's, copied verbatim and marked with their LI names — no
// re-calibration here. LI's ValueError / RuntimeError raise sites (a boundary walk that refuses,
// an onset topology that changes across refractive indices) become status fields — escape-as-data,
// an exception semantics never crosses a future C ABI. The mechanical invariant mirrors
// PartitionResult's: `escaped` implies `onsets` is EMPTY, so an ignored escape fails loudly
// downstream as no onsets at all, never as stale ones.
//
// Internal header of the analytic kernel: nothing here is part of the C ABI (the report surface
// is 660.6's). Dependency direction: dp_focus -> {dp_field, dp_partition, dp_boundary,
// path_rank, pose_density, reflection_group, discovery (the Fibonacci lattice)}.
//
// Migration note (660.3 review): InteriorCriticalPointsOf is a generic field-topology primitive
// that lives here because dp_focus is today its only consumer; a non-focusing consumer (e.g. the
// contour integration of 660.5) moves it down to the dp_field/dp_partition layer and keeps a
// call here.

#include <string>
#include <utility>
#include <vector>

#include "analytic/dp_boundary.hpp"
#include "analytic/dp_field.hpp"
#include "analytic/dp_partition.hpp"
#include "analytic/path_rank.hpp"
#include "analytic/pose_density.hpp"

namespace lumice::analytic {

// ---- constants (LI focusing.py / dp_field.field.py, verbatim) ------------------------------------------

// A full S^2 gradient at or below this at a boundary critical point is a vanishing one — profile
// degenerate rather than boundary_onset (LI BOUNDARY_GRADIENT_ATOL; interior Newton iterates are
// held to 1e-10, boundary points sit on pieces located to ~1e-12, a non-degenerate |grad D| is
// O(1) there).
constexpr double kBoundaryGradientAtol = 1e-6;
// Samples of the slab circle u . n_M = 0 for its location relative to U_P (LI focusing
// CIRCLE_SAMPLES). Numerically equal to dp_partition's kFoldCircleSamples but a different probe:
// that one clusters the fold set's crease for the partition gates, this one decides where the
// circle of the onset itself lies. Two consumers, two constants (a56) — the equality of the
// values is LI's own coincidence, not a coupling.
constexpr int kFocusingCircleSamples = 7200;
// The lattice of the gradient-norm range and the Newton seeding (LI LATTICE_N; equal to the
// field layer's own default lattice, LI DPField.build's).
constexpr int kFocusingLatticeN = 20000;
// Angle (rad) off a slab axis at which the cone slope |grad D| is read (LI CONE_PROBE_RAD).
constexpr double kConeProbeRad = 1e-4;
// Interior Newton: a tangent-gradient norm at or below this is a critical point, a Hessian
// eigenvalue at or below this in size is a degenerate one, two converged points closer than this
// (rad) are one (LI CRITICAL_GRADIENT_TOL / DEGENERATE_EIGENVALUE_TOL /
// CRITICAL_POINT_MERGE_RAD).
constexpr double kCriticalGradientTol = 1e-10;
constexpr double kDegenerateEigenvalueTol = 1e-6;
constexpr double kCriticalPointMergeRad = 1e-6;

// ---- the onset vocabulary -------------------------------------------------------------------------------

enum class OnsetLocation { kInterior, kBoundary };
enum class OnsetSource {
  kInteriorMinimum,
  kInteriorMaximum,
  kInteriorSaddle,
  kInteriorDegenerate,
  kSlabAxis,
  kSlabCircle,
  kBoundaryExtremum,
  kCorner,
};
enum class OnsetProfile {
  kFiniteJump,
  kLogDivergence,
  kInverseSqrtDivergence,
  kConePoint,
  kCrease,
  kBoundaryOnset,
  kDegenerate,
};

// LI's identifier strings (location / source / profile of CriticalOnset.as_json): stable for the
// report side and the parity fixtures; a name, not a description.
const char* OnsetLocationName(OnsetLocation location);
const char* OnsetSourceName(OnsetSource source);
const char* OnsetProfileName(OnsetProfile profile);
// LI JACOBIAN_FOCUSING_PROFILES: the profiles whose level-set measure diverges at the critical
// value (log_divergence, inverse_sqrt_divergence, degenerate).
bool ProfileIsJacobianFocusing(OnsetProfile profile);

// One critical value of D_P and the profile it produces for a random orientation. `value` is in
// radians; `gradient_norm` is |grad_{S^2} D_P| at the point (CONE_PROBE_RAD next to it for a cone
// point; infinity for an exit-TIR end of the boundary, where the gradient is unbounded — the
// fixture records those as null); `measure_limit` (2 pi / sqrt(det H), the limit of int dl /
// |grad D|) exists only for a finite_jump (`has_measure_limit`); `multiplicity` counts the
// critical points merged into this record — same value within kExtremumAtol, same location,
// source and profile (the mirror images of one extremum, the corners where several margins meet)
// — keeping the smallest gradient_norm (LI CriticalOnset).
struct CriticalOnset {
  double value = 0.0;
  OnsetLocation location = OnsetLocation::kInterior;
  OnsetSource source = OnsetSource::kInteriorMinimum;
  OnsetProfile profile = OnsetProfile::kFiniteJump;
  double gradient_norm = 0.0;
  bool has_measure_limit = false;
  double measure_limit = 0.0;
  int multiplicity = 1;

  bool JacobianFocusing() const { return ProfileIsJacobianFocusing(profile); }
};

// ---- the density-side labels (LI confined_dimensions / _family_axis / family_pinned) ---------------------

// The pose dimensions a density confines (one per narrow Gaussian factor) and their widths (rad):
// random none; a zenith Gaussian (column / plate) one; a roll-locked density (parry / lowitz)
// two. Dispatch is on the density's kind, not on a family name, and no width threshold is
// applied — a wide factor still confines one dimension (LI confined_dimensions).
struct ConfinedDimensions {
  int dimensions = 0;
  std::vector<double> widths_rad;
};
ConfinedDimensions ConfinedDimensionsOf(const PoseDensitySpec& spec);

// The body axis the density's sigma -> 0 support runs one circle about, false when it is no
// single circle (LI _family_axis): a zenith Gaussian at a pole (zenith mean exactly 0 or 180 deg)
// leaves only the spin about c — e3; off a pole (column) the c-axis azimuth is free too — none. A
// roll-locked density fixes zenith and roll, so the free azimuth runs one circle about
// a = (sin zeta cos rho, -sin zeta sin rho, cos zeta), the body image of the world zenith on the
// support — e3 at a pole whatever the roll (Lowitz), body x at Parry's (90, 0).
bool FamilyAxis(const PoseDensitySpec& spec, double axis[3]);

// Whether the sigma -> 0 family of `spec` lies in one level set of D_P (module docstring): the
// path is not rank 0, its wedge is 0 within kRankZeroWedgeToleranceDeg (path_rank.hpp — the SAME
// constant LI uses for both its halo_map_rank default and this guard, geometry
// WEDGE_ZERO_TOLERANCE_DEG; two consumers of one source constant, kept one here) and its fold
// matrix (dp_field's FoldMatrixOf, the single authority) commutes with the rotations about the
// family axis (reflection_group.hpp). The density test is on its kind and means, never on a
// family name.
bool FamilyPinned(const FaceNormalTable& table, const int* slots, int slot_count, const PoseDensitySpec& spec);

// ---- the interior Newton search (LI lattice_newton_critical_points) ------------------------------------

struct InteriorNewtonOptions {
  int lattice_n = kFocusingLatticeN;
  int iterations = 40;         // LI's fixed count; the loop is branch-free there
  double max_step_rad = 0.05;  // the damping cap of every Newton step
};

// Every interior critical point of a NON-slab path: seeds are the U_P points of the Fibonacci
// lattice, each walks a damped tangent-space Newton on grad D_P = 0 (a 2x2 solve on the
// Riemannian Hessian in the tangent basis, the step capped at max_step_rad), and a converged
// iterate counts only when it is finite, back inside U_P with every gate above
// kBoundaryMarginAtol (a critical point of the smooth extension that sits on dU_P is the walk's
// loop extremum, not an interior one), with tangent gradient below kCriticalGradientTol;
// survivors merge within kCriticalPointMergeRad. The returned points carry the Hessian
// eigenvalues and the gradient norm (dp_partition's InteriorCriticalPoint extended for this
// layer), sorted by value. The slab branch is 660.2's SlabInteriorCriticalPoints — pass its
// output alongside for a degenerate fold; this function does not handle that case.
std::vector<InteriorCriticalPoint> InteriorCriticalPointsOf(const DeviationField& field,
                                                            const InteriorNewtonOptions& options);

// ---- the onset table ------------------------------------------------------------------------------------

// The knobs of one classification: LI's defaults (the lattice of the gradient range and the
// Newton seeding, the walk's own options).
struct FocusingOptions {
  int lattice_n = kFocusingLatticeN;
  InteriorNewtonOptions newton;
  BoundaryWalkOptions walk;
};

// Every critical value's onset: the interior set (slab onsets for a degenerate fold — the axis
// cone points and the rotation circle, LI _slab_onsets — or the Newton set otherwise), plus every
// restricted extremum and corner of `loop` with its gradient-based profile, merged (LI
// field_onsets + _merged). `fold_set` is null for a non-degenerate path; `newton` carries the
// interior search's knobs (the caller's FocusingOptions.newton; the default is LI's own). Sorted
// by value.
std::vector<CriticalOnset> FieldOnsets(const DeviationField& field, const DegenerateFoldSet* fold_set,
                                       const BoundaryLoopData& loop,
                                       const InteriorNewtonOptions& newton = InteriorNewtonOptions());

// (min, max) of |grad D_P| on the U_P points of a `lattice_n` Fibonacci lattice — a sampled
// bound, not a proof; false when no lattice point is inside or every norm is non-finite (LI
// gradient_norm_range).
bool GradientNormRange(const DeviationField& field, int lattice_n, double out[2]);

// The focusing label of one face sequence under one pose density (LI classify). A rank-0 path is
// the point_mass label alone — no onsets, no gradient range, family_pinned false — without
// touching the field. `escaped` is the fail-closed refusal of the boundary walk (LI raises out
// of classify there); escaped implies empty onsets.
struct FocusingClassification {
  std::string path;
  int halo_map_rank = 2;  // LI halo_map_rank: 0 (a point mass) or 2
  std::vector<CriticalOnset> onsets;
  bool has_gradient_norm_range = false;
  double gradient_norm_range[2] = {};
  ConfinedDimensions confined;
  bool family_pinned = false;
  bool escaped = false;
  std::string escape_message;

  bool JacobianFocusing() const;
  bool DimensionCollapse() const { return halo_map_rank > 0 && confined.dimensions > 0; }
  // "point_mass", "none", "jacobian", "dimension_collapse" or "jacobian+dimension_collapse"
  // (LI FocusingClassification.mechanism's vocabulary, joined with "+" in that order).
  std::string Mechanism() const;
};

FocusingClassification Classify(const FaceNormalTable& normals, const FacePolygonTable& polygons, const int* slots,
                                int slot_count, const PoseDensitySpec& density, double index,
                                const FocusingOptions& options = FocusingOptions());

// ---- the wavelength-critical table (LI wavelength_critical_table) ---------------------------------------

// One onset followed across refractive indices: `values_deg` parallels `labels` (the caller's
// index labels, in the order given); `displacement_deg` is max - min, independent of label order.
struct WavelengthOnsetShift {
  OnsetLocation location = OnsetLocation::kInterior;
  OnsetSource source = OnsetSource::kInteriorMinimum;
  OnsetProfile profile = OnsetProfile::kFiniteJump;
  bool jacobian_focusing = false;
  std::vector<double> values_deg;
  double displacement_deg = 0.0;
};

// The onsets of one path at several refractive indices, aligned by rank (each Classify is a fresh
// field — D_P depends on n). Rank pairing is accepted only when every index carries the same
// onset count and every rank the same (location, source, profile) at every index; anything else
// escapes with LI's message prefix ("onset counts differ across refractive indices" /
// "onset N differs across refractive indices" / "classifications disagree on the path") instead of
// pairing unrelated onsets — following a topology that changes with n is not handled here, and
// n(lambda) itself is the caller's. An empty index set escapes ("indices is empty").
struct WavelengthCriticalTable {
  bool escaped = false;
  std::string message;
  std::string path;
  std::vector<std::string> labels;
  std::vector<double> indices;  // parallel to labels
  std::vector<WavelengthOnsetShift> onsets;
};

WavelengthCriticalTable WavelengthCriticalTableOf(const FaceNormalTable& normals, const FacePolygonTable& polygons,
                                                  const int* slots, int slot_count, const PoseDensitySpec& density,
                                                  const std::vector<std::string>& labels,
                                                  const std::vector<double>& indices,
                                                  const FocusingOptions& options = FocusingOptions());

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_DP_FOCUS_HPP_
