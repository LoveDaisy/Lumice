#ifndef LUMICE_RAYPATH_POINT_RESULT_HPP_
#define LUMICE_RAYPATH_POINT_RESULT_HPP_

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "analytic/fiber_types.hpp"
#include "core/def.hpp"
#include "raypath/domain/input_metadata.hpp"
#include "raypath/domain/outcome.hpp"  // ExecutionOutcome — the outcome axis's vocabulary
#include "raypath/domain/result_identity.hpp"
#include "raypath/results/quantity.hpp"
#include "raypath/survey_context.hpp"
namespace lumice::raypath {

constexpr int kSchemaVersion = 1;
constexpr double kFactorsEpsilon = 1e-12;
constexpr int kOrbitFactorSampleCount = 1024;

// ---- metadata: every conversion, as applied ------------------------------------------------------

struct SinglePathMetadata {
  int schema_version = kSchemaVersion;
  int analytic_api_version = 0;

  IdType crystal_id = 0;
  std::string crystal_kind;  // "prism" or "pyramid"
  std::vector<NominalShapeScalar> shape;
  double upper_wedge_deg = 0.0;  // pyramid only (fixed in the config)
  double lower_wedge_deg = 0.0;
  // True when any shape scalar is a distribution with a non-zero spread: the fiber and the grid
  // belong to the nominal crystal, not to the population the simulation samples.
  bool shape_is_nominal = false;

  std::vector<int> faces;

  double sun_altitude_deg = 0.0;
  double sun_azimuth_deg = 0.0;
  double sun_diameter_deg = 0.0;   // recorded, not used: the sun is a point here
  double incident_direction[3]{};  // world, propagation sun -> crystal

  double target_altitude_deg = 0.0;
  double target_azimuth_deg = 0.0;
  double target_direction[3]{};       // world, light-travel direction arriving from the target sky point
                                      // (doc/geometry/coordinate-convention.md "Direction-Vector Semantics")
  double target_deviation_deg = 0.0;  // angle between the sun and the target sky point

  double wavelength_nm = 0.0;
  WavelengthSource wavelength_source = WavelengthSource::kDefault;
  double refractive_index = 0.0;

  // Discovery settings actually used (kernel reference defaults except the sample count).
  int sample_count = 0;
  double band_half_width_rad = 0.0;
  double cluster_radius_rad = 0.0;
  double distance_threshold_rad = 0.0;
  int warm_seed_count = 0;

  // The budget receipt: the ms the deadline was in force for (0 = unbounded), and whether the run
  // stopped early — a partial document, honestly marked.
  int budget_ms = 0;
  bool interrupted = false;
  // The execution outcome axis (internal typed read; the wire keeps its interrupted convention):
  // completed / cancelled / deadline_exceeded, derived from the query's single stop authority.
  ExecutionOutcome execution = ExecutionOutcome::kCompleted;
};

// ---- per-pose detail --------------------------------------------------------------------------

// A pose as the config's orientation angles, in degrees. zenith in [0, 180]; azimuth and roll in
// (-180, 180]. At zenith 0 or 180 only azimuth +- roll is defined: roll is set to 0, the whole
// angle goes to azimuth, and `degenerate` is set.
struct PoseAngles {
  double zenith_deg = 0.0;
  double azimuth_deg = 0.0;
  double roll_deg = 0.0;
  bool degenerate = false;
};

PoseAngles PoseToAngles(const double pose[9]);

// The declared measure's registration, as a per-pose rho read needs it declared: the u-marginal
// support kind plus the registered read-as-zero gaps (declared_density.hpp's v1 gap list). A
// factor consumer must see this before reading a rho of 0 as "dark" — a registered gap means the
// density layer did not answer, not that the measure is empty there.
struct MeasureRegistration {
  std::string support_kind;  // USupportKind's name (point / spin_orbit / roll_orbit / latitude_orbit /
                             // area / degenerate_sun_geometry / degenerate_axis_geometry)
  bool fast_path = false;    // the zonal fast path was in force (declared_density.hpp Table 4)
  bool registered_gap = false;
  std::string note;  // empty when registered_gap is false; names the gap family otherwise
};

// One pose of the orbit factor integral: the factors along the declared measure's spin orbit (the
// rank-0 branch's fiber substitute — every pose lands on the same sky point, so the point's total
// is the measure-weighted orbit integral, and the tint quotient of two such integrals at the
// wavelength-pool ends is that spot's tint).
struct OrbitFactorPoint {
  double theta_rad = 0.0;  // the spin orbit's parameter (the az slot's draw)
  double rho_theta = 0.0;  // the declared density along the orbit w.r.t. dtheta
  double entry_measure_a2 = 0.0;
  double total_transmission = 0.0;
  bool valid = false;
};

struct OrbitFactors {
  // False: the block carries the honest unavailable state — the measure's support is not a v1
  // spin orbit — with `measure_support` naming what it is and `reason` the named cause. No
  // defaulted numbers reach the document.
  bool available = false;
  std::string reason;           // named cause when unavailable (e.g. "measure_support_not_spin_orbit")
  std::string measure_support;  // the u-marginal support kind, spelled either way
  int sample_count = 0;         // midpoint samples over [0, 2 pi)
  // The quadrature: sum of rho_theta * entry_measure_a2 * total_transmission * dtheta over kept
  // samples (A*T > 0 and valid — fiber_quadrature's positive-part convention, which this branch
  // reuses; the kept count rides along so "all dark" stays distinguishable from "no samples").
  double integral = 0.0;
  int kept_count = 0;
  std::vector<OrbitFactorPoint> points;
};

struct PointDetail {
  double pose[9]{};  // row-major body -> world
  PoseAngles angles;
  double sun_in_crystal[3]{};                    // u = R^T (sun direction): where the sun sits in the crystal frame
  double residual_norm = 0.0;                    // |basis^T (F(R) - d)| as the continuation reports it
  bool valid = false;                            // the path's direction-level validity at this pose
  double outgoing_direction[3]{};                // world
  std::vector<double> segment_directions;        // (faces + 1) * 3, body frame: incident, legs, outgoing
  std::vector<double> interface_transmittances;  // faces: T at entry and exit, R at internal faces
  // Product of the interface factors. NOT a relative intensity on its own: the fraction of the
  // crystal's cross-section that enters this face and meets every later face is entry_measure.
  double total_transmission = 0.0;
  // A_P: the entry cross-section (perpendicular to the incident direction) whose rays follow this
  // exact face sequence through the finite crystal, in the crystal's length unit squared. Discovery
  // only seeds where it is positive.
  double entry_measure = 0.0;

  // The named-factor decomposition at this pose (the L2 query's per-pose diagnostics).
  // Multiplicative factors in their real units — never additive percentages (the corpus
  // discipline the point output inherits):
  double rho = 0.0;               // the declared joint pose density, w.r.t. dlambda dphi droll (angle Lebesgue,
                                  // radians) at the pose's composed angles
  double entry_measure_a2 = 0.0;  // A/a^2: entry_measure over the crystal length unit squared (a = 1 in this
                                  // repo's configs — a declared identity, not a normalization)
  // The normal Jacobian at the pose's own direction (kernel NormalJacobian; NaN-honest: the
  // numbers are NaN, never a plausible 0 or 1, when the Jacobian cannot be formed).
  bool jacobian_available = false;
  double jacobian = 0.0;        // J_perp = sigma1 * sigma2
  double singular_values[2]{};  // NaN when unavailable
  double k_carea = 0.0;         // 1 / (jacobian + kFactorsEpsilon); NaN when the Jacobian is unavailable
  double factor_product = 0.0;  // rho * entry_measure_a2 * total_transmission * k_carea; NaN when the
                                // Jacobian is unavailable or the pose is not valid
};

// ---- components -------------------------------------------------------------------------------

// The kernel's own classification types (src/analytic/discovery.hpp, fiber_continuation.hpp), used
// as they are: one definition of what a trace can end on. The names below are LI's.
using ComponentKind = analytic::ComponentKind;
using TraceStatus = analytic::FiberStatus;
using TraceReason = analytic::FiberReason;
using IncompleteCause = analytic::IncompleteCause;

const char* ComponentKindName(ComponentKind kind);
const char* TraceStatusName(TraceStatus status);
const char* TraceReasonName(TraceReason reason);
const char* IncompleteCauseName(IncompleteCause cause);

struct TraceEnd {
  TraceStatus status = TraceStatus::kNumericalFailure;
  TraceReason reason = TraceReason::kInvalidNumericalInput;
  int pose_count = 0;
};

// The factor decomposition's component-level bookkeeping (the per-pose factor arrays' integral).
struct ComponentFactors {
  // Sum of factor_product * arclength over consecutive point pairs whose first point carries a
  // usable product (valid pose, Jacobian available). NaN-free honest shape: available false when
  // no segment contributed, and the document writes null then.
  bool integral_available = false;
  double fiber_integral = 0.0;
  int available_points = 0;  // points whose factor_product is usable
  int total_points = 0;
};

struct FiberComponent {
  ComponentKind kind = ComponentKind::kClosed;
  double seed[9]{};
  TraceEnd forward;
  // Arc only. A closed component leaves it default-constructed (pose_count 0): its status and
  // reason then mean nothing, and a shell reads `kind` before printing them.
  TraceEnd backward;
  // One ordered point list. Closed: the forward trace, seed first, its last pose the corrected
  // closing pose. Arc: the backward trace reversed, then the forward trace — the seed appears once,
  // at `seed_index` — so the list runs from one boundary event to the other.
  std::vector<PointDetail> points;
  int seed_index = 0;
  // The continuation's arclength (rad on SO(3)) between consecutive points: points.size() - 1 entries.
  std::vector<double> arclength_increments;
  ComponentFactors factors;
};

struct IncompleteComponent {
  IncompleteCause cause = IncompleteCause::kNotConverged;
  double seed[9]{};
  TraceEnd forward;
  TraceEnd backward;  // run only for the two ARC_BACKWARD causes
};

// The discovery funnel and counters (LI section 9.5.6), transcribed.
struct DiscoverySummary {
  // Procedural, not a certificate: true means only that every admissible candidate of this sample
  // closed or became an arc. It never means every component of the fiber was found.
  bool complete = false;
  int pool_count = 0;
  int extra_seed_count = 0;
  int raw_cluster_count = 0;
  int admissible_count = 0;
  int dedup_merged = 0;
  int arc_stitched = 0;
  int arc_backward_failed = 0;
  int arc_backward_closed_anomaly = 0;
  int incomplete_unnamed_event = 0;
  int incomplete_not_converged = 0;
};

// The sentence a shell prints next to `complete`, so no shell paraphrases it into a promise.
extern const char* const kCompletenessNote;

// ---- rank 0: the outgoing direction does not depend on the pose --------------------------------

struct PointMass {
  double direction[3]{};      // world outgoing direction, the same at every pose that realises the path
  double altitude_deg = 0.0;  // the sky point it lands on
  double azimuth_deg = 0.0;
  double target_separation_deg = 0.0;  // angle between that sky point and the requested target
  OrbitFactors orbit_factors;          // the measure-orbit factor integral (the rank-0 branch's
                                       // factor decomposition; honest unavailable state when the
                                       // measure's support is not a v1 spin orbit)
};

// ---- the sun-direction sphere -----------------------------------------------------------------

// D_P(u) over the sun direction u in the crystal frame, on a latitude-longitude grid of cell centres:
//   latitude  lat_i = -90 + (i + 0.5) * 180 / lat_count,   i in [0, lat_count)
//   longitude lon_j = -180 + (j + 0.5) * 360 / lon_count,  j in [0, lon_count), lon_count = 2 lat_count
//   u = (cos lat cos lon, cos lat sin lon, sin lat)  (body frame; +z the c-axis)
// Row-major [i][j]. The grid holds no pole row, so no cell is degenerate; a contour tracer sees the
// longitude seam as periodic. D is the deviation angle between the incident and outgoing
// directions; it depends on the pose only through u (a rotation about the sun direction turns the
// outgoing ray about it too).
struct SunSphereGrid {
  int lat_count = 0;
  int lon_count = 0;
  std::vector<double> deviation_rad;  // NaN where not valid
  std::vector<uint8_t> valid;         // direction-level validity, as PointDetail::valid
  std::vector<double> entry_measure;  // A_P(u), as PointDetail::entry_measure; 0 where not valid
};

void SunSphereGridCellCentre(const SunSphereGrid& grid, int lat_index, int lon_index, double u[3]);

// ---- whether the target's deviation is in the path's range at all ------------------------------

// Separates the two empty outcomes a shell otherwise shows alike ("0 components"): the target's
// deviation delta lies outside the range of D over every valid sun direction (no pose of the
// infinite crystal reaches the target), or inside it (a pose may reach it; an empty component list
// then means the finite crystal passes no ray along the fiber, or the search missed it).
//
// The range is probed on its own fixed grid, never on the request's sun_grid, so the answer does not
// move with --grid. Samples are the valid cell centres (the sun_grid layout at kReachProbeLatCount
// rows) plus, on every edge between a valid and an invalid neighbour cell, the validity boundary
// found by bisection: D is square-root singular at a total-reflection or grazing boundary, where
// cell centres alone converge at half order and no fixed tolerance covers their shortfall. The
// tolerance is derived from the probe's own convergence: if an end's shortfall shrinks as h^p with
// the cell size h, the change c between the half-resolution probe and the full one bounds the full
// one's shortfall by c / (2^p - 1). tolerance_rad takes p = 1/2, the worst order D shows (measured
// against a 2880-row reference on eleven prism and pyramid paths: shortfall / c peaks at 2.04, where
// an extremum sits where a validity boundary meets another; a pole's cone point gives 0.88, a smooth
// extremum less), so tolerance_rad = (1 + sqrt 2) c. A valid region thin enough to hold no cell
// centre is still missed, so target_in_range == false means "not reached at the probe's
// resolution", not a certificate.
// target_in_range == true does not promise a component either: the valid set need not be connected.
constexpr int kReachProbeLatCount = 360;

struct ReachSummary {
  int probe_lat_count = 0;
  // Attained values of D (every sample is an evaluated valid direction); NaN when the probe finds no
  // valid direction at all, in which case target_in_range is true (nothing is excluded).
  double deviation_min_rad = 0.0;
  double deviation_max_rad = 0.0;
  double target_deviation_rad = 0.0;
  double tolerance_rad = 0.0;
  // delta in [deviation_min_rad - tolerance_rad, deviation_max_rad + tolerance_rad].
  bool target_in_range = true;
};

// ---- result -----------------------------------------------------------------------------------

enum class Outcome {
  kPointMass,   // rank 0: no fiber is traced (point_mass is set, components empty)
  kDiscovered,  // components / incomplete / discovery are set (possibly zero components)
};

struct DiscoveredPoint {
  std::vector<FiberComponent> components;
  std::vector<IncompleteComponent> incomplete;
  DiscoverySummary discovery;
  ReachSummary reach;  // kDiscovered only
  MeasureRegistration measure_registration;
};

struct PointData;

// The single completion entry for the value layer: validates the payload's internal consistency
// and marks the value complete. The marker is private, so no construction path can forge a
// completed value by writing a flag.
PointData FinalizePointData(PointData data);

struct PointData {
  PointFactorQuantities quantities;
  MemberScope members;
  SinglePathMetadata meta;
  std::variant<DiscoveredPoint, PointMass> payload;
  Outcome OutcomeValue() const {
    return std::holds_alternative<PointMass>(payload) ? Outcome::kPointMass : Outcome::kDiscovered;
  }
  PointMass& Mass() { return std::get<PointMass>(payload); }
  const PointMass& Mass() const { return std::get<PointMass>(payload); }
  DiscoveredPoint& Discovered() { return std::get<DiscoveredPoint>(payload); }
  const DiscoveredPoint& Discovered() const { return std::get<DiscoveredPoint>(payload); }
  SunSphereGrid sun_grid;
  // The declared measure's registration (kDiscovered only; the point-mass branch carries its own
  // face inside OrbitFactors). A rho consumer reads this before treating a rho of 0 as dark.
  // Explicitly materialized context. Reading a value never starts a computation.
  bool context_requested = false;
  SurveyContextBlock survey_context;

  bool Completed() const { return completion_ == CompletionState::kCompleted; }

 private:
  friend PointData FinalizePointData(PointData data);
  CompletionState completion_ = CompletionState::kNotCompleted;
};

inline PointData FinalizePointData(PointData data) {
  const SunSphereGrid& grid = data.sun_grid;
  if (grid.deviation_rad.size() != grid.valid.size() || grid.deviation_rad.size() != grid.entry_measure.size()) {
    throw std::invalid_argument("point: the sun grid's parallel arrays disagree in length");
  }
  data.completion_ = CompletionState::kCompleted;
  return data;
}


// A completed product owns one immutable value. Copies preserve identity and storage;
// construction from revised assembly data creates a different snapshot.
class PointResult : public FrozenValue<PointData> {
 public:
  // The output slot an entry writes into. No completion has run, so the value is not
  // publishable: the data constructor rejects it and ToJson throws on it.
  PointResult() : FrozenValue<PointData>(PointData{}) {}
  explicit PointResult(PointData data) : FrozenValue<PointData>(Validated(std::move(data))) {}
  std::optional<MemberRef> Reference(MemberId id) const {
    return id.ordinal < Data().members.members.size() ? std::optional<MemberRef>{ { Identity(), id } } : std::nullopt;
  }
  const std::vector<int>* Resolve(MemberRef ref) const {
    return ref.result == Identity() && ref.member.ordinal < Data().members.members.size() ?
               &Data().members.members[ref.member.ordinal] :
               nullptr;
  }

 private:
  static PointData Validated(PointData data) {
    if (!data.Completed()) {
      throw std::invalid_argument("point: the construction slot was frozen without a completion");
    }
    if (data.meta.schema_version != kSchemaVersion) {
      throw std::invalid_argument("point: unsupported schema version");
    }
    return data;
  }
};


}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_POINT_RESULT_HPP_
