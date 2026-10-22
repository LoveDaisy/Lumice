#ifndef LUMICE_RAYPATH_SINGLE_PATH_ANALYSIS_HPP_
#define LUMICE_RAYPATH_SINGLE_PATH_ANALYSIS_HPP_

// Single-path analysis: everything the Analyze workspace computes once ONE single-layer raypath of
// ONE crystal entry has been chosen (doc/raypath/raypath-analysis.md section 5.1.8, phase one) — the
// components of the fiber over a sky point, each component's poses with per-pose detail, and the
// deviation over the whole sun-direction sphere. It strings liblumice_analytic's kernel
// (src/analytic/: EvaluatePath, the fiber continuation, component discovery) to a scene config,
// and it knows nothing of a user interface: the result is a plain value, with no pointers and no
// global state, so a command-line shell and a GUI shell can both render it.
//
// Every conversion from the scene to the kernel's inputs is decided here once and recorded in the
// result (SinglePathMetadata): the crystal's shape (a distribution in the config, a nominal value
// here), the sun direction, the wavelength and refractive index. A request this module cannot
// answer truthfully — a multi-layer chain, a face the crystal lacks, a wavelength outside the
// refractive-index table — is an error with a code and a message, never a result.
//
// Frames and angles are doc/geometry/coordinate-convention.md's: a pose is the row-major body -> world
// rotation R = Rz(azimuth - 180) Ry(-zenith) Rz(roll); world directions are propagation directions
// (sky_direction.hpp: the sky point a direction comes from sits at altitude asin(-z)).

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "analytic/discovery.hpp"
#include "config/config_manager.hpp"
#include "core/def.hpp"
#include "core/math.hpp"

namespace lumice::raypath {

// Bumped whenever a field changes meaning or is removed. Fields are only ever appended, so a reader
// written against version N reads every later version's fields it knows about unchanged.
constexpr int kSchemaVersion = 1;

// The factor decomposition's declared epsilon: K = 1/(J_perp + epsilon) never divides by an exact
// zero at a caustic pose. The value is data in the document (each factor block echoes it), never
// a hidden knob.
constexpr double kFactorsEpsilon = 1e-12;

// The budget's default and bound: a 0 budget_ms request resolves to the default, an out-of-range
// one is refused. Enforcement is boundary-checked (loop boundaries only), and a stopped run
// serializes its partial document with meta.interrupted — an honest early exit, not a failure.
constexpr int kDefaultPointBudgetMs = 15000;
constexpr int kMaxPointBudgetMs = 120000;

// The orbit factor integral's grid (midpoint samples over the measure's spin orbit, the
// fiber_quadrature kFiberParameter convention). Part of the document's declared caliber: the
// block echoes the count.
constexpr int kOrbitFactorSampleCount = 1024;

constexpr double kDefaultWavelengthNm = 550.0;
// The kernel's reference default (LI section 9.5.9) and its hard upper bound; the call cannot be
// cancelled, so a larger request is refused.
constexpr int kDefaultSampleCount = 1000000;
constexpr int kMaxSampleCount = 100000000;
constexpr int kDefaultSunGridLatCount = 90;
constexpr int kMaxSunGridLatCount = 1800;

enum class ErrorCode {
  kOk,
  kUnknownCrystalId,       // no crystal entry with this id in the config
  kMultiLayerUnsupported,  // more than one scattering layer: v1 traces single-layer paths only
  kInvalidPath,            // no layer, or a face sequence shorter than 2 or longer than the kernel's bound
  kFaceNotInCrystal,       // a face number this crystal (at its nominal shape) does not have
  kWavelengthOutOfRange,   // outside the refractive-index table [350, 900] nm
  kInvalidTarget,          // non-finite, or at the sun / its antipode (no fiber is defined there)
  kInvalidArgument,        // an option out of range (sample count, grid size, a warm seed that is no rotation)
  kCrystalRejected,        // the engine's closed-form gate rejects the nominal shape
  kPathInfeasible,         // no pose at all realises this face sequence
};

struct Error {
  ErrorCode code = ErrorCode::kOk;
  std::string message;
  bool Ok() const { return code == ErrorCode::kOk; }
};

// Stable lower-case name of an error code (for a shell's output).
const char* ErrorCodeName(ErrorCode code);

// ---- the survey context (--warm consumption) ---------------------------------------------------

// A schema-4 warm document's harvest: the seeds ride the request's warm_seeds; the facts the
// survey context reads ride this. Filled by ParseWarmStart (single_path_json.hpp) after the
// consistency gates; a document that carried no (complete) warm_index leaves index_present false
// — the warm_index_absent degradation, declared downstream, never an error here.
struct SurveyWarmSource {
  // True only when the input was a schema-4 survey document (the family discriminator): the
  // bridge hands the query only such a harvest, so "not a survey document" (schema-1, inline
  // context) and "a schema-4 document without its warm_index block" (the warm_index_absent
  // degradation) stay two different absences.
  bool from_survey_document = false;
  bool index_present = false;
  bool early_stopped = false;  // the identity arm: no delta-axis structure exists to reuse
  // The scope receipt (the band-sum consumption gate's comparison side). has_scope false = the
  // receipt (or a field of it) was missing — that gate answers scope_field_missing.
  bool has_scope = false;
  double wavelength_nm = 0.0;
  std::string wavelength_source;  // carried for the reading's caliber declaration, never compared
  double sun_altitude_deg = 0.0;
  double sun_azimuth_deg = 0.0;
  // The support: every support card's delta-axis intervals, with the card's members (the first
  // layer's faces — both harvest arms read that same layer). has_support true exactly when at
  // least one interval was harvested: a support_set with an empty interval list reads as no
  // support (the context answers survey_carries_no_support, never a dereference of nothing).
  struct SupportInterval {
    double lower_deg = 0.0;
    double upper_deg = 0.0;
    std::vector<int> members;
  };
  bool has_support = false;
  std::vector<SupportInterval> intervals;
  // The caustic edge's certified position (an edge-distance candidate).
  bool has_edge = false;
  double edge_deg = 0.0;
  // The canonical probe's radial profile (the band-sum readings).
  struct ProbeSample {
    double delta_deg = 0.0;
    double value = 0.0;
  };
  bool has_probe = false;
  double probe_bin_deg = 0.0;
  std::vector<ProbeSample> probe_samples;
  // The family arm's measure echo, read as the concentration language's numbers (the measure
  // gate's fourth comparison: the caller re-derives its own reading from the query config and
  // compares per dimension; a document without family_routing skips the gate with a declaration).
  struct MeasureDimEcho {
    std::string kind;  // "fixed" | "gauss" | "uniform"
    double center_deg = 0.0;
    double width_deg = 0.0;
  };
  bool has_measure_echo = false;
  MeasureDimEcho measure_zenith;
  MeasureDimEcho measure_azimuth;
  MeasureDimEcho measure_roll;
};

// One field of the survey context, in the four-state vocabulary (the evidence layer's; schema1
// spells the states as strings). `state` is always serialized; the value fields exist exactly
// when the state says something was read.
struct SurveyContextField {
  std::string state;   // "certified" | "finite" | "unevaluated"
  std::string reason;  // the unevaluated cause, named (never empty when unevaluated)
};

// The survey context block (schema1's top-level append): where the target sits in the survey's
// structure. "Survey is the query's index" made concrete — with warm, the producing document's
// facts; without, the same facts from a context-only (T0–T2) assembly run here. Every field
// carries its own state; a band-sum reading only ever appears under the scope gate's pass.
struct SurveyContextBlock {
  std::string source;  // "warm" | "inline"
  // True: a --warm schema-4 document was given but carried no (complete) warm_index — the
  // documented degradation (no structural reuse, no seed consumption, inline context).
  bool warm_index_absent = false;
  // The target's deviation vs the delta-axis support: inside an interval, or the nearest card.
  SurveyContextField object;
  bool target_inside_support = false;
  std::string object_kind;           // the owning/nearest card's kind (e.g. "support_set")
  std::vector<int> object_members;   // the card's members (empty when the document carried none)
  double object_distance_deg = 0.0;  // 0 when inside
  // The angular distance to the nearest edge (a support endpoint or the certified edge position).
  SurveyContextField edge;
  double edge_distance_deg = 0.0;
  std::string edge_basis;  // "support_interval_endpoint" | "caustic_edge_position"
  // The band-sum reading at the nearest probe sample (warm documents only; the scope gate's
  // exact-equality comparison on lambda and sun decides, with the two named failure causes).
  SurveyContextField band_sum;
  double band_sum_value = 0.0;
  double band_sum_delta_deg = 0.0;  // the nearest sample's own deviation
  double band_sum_bin_deg = 0.0;
  std::string band_sum_caliber;  // both sides' lambda/sun identity, for the reading's declaration
};

struct SinglePathRequest {
  IdType crystal_id = 0;
  // One face sequence per scattering layer, in Lumice face numbers (entry, internal reflections,
  // exit). v1 takes exactly one layer; the list shape exists so a shell can hand over whatever the
  // user wrote and this module alone decides what is refused.
  std::vector<std::vector<int>> path_layers;
  // The sky point, azimuth measured as the sun's (`analyze --center` convention).
  double target_altitude_deg = 0.0;
  double target_azimuth_deg = 0.0;
  // Unset: the config's wavelength if it names exactly one, else kDefaultWavelengthNm.
  std::optional<double> wavelength_nm;
  int sample_count = kDefaultSampleCount;
  // Latitude rows of the sun-direction grid; 0 skips the grid.
  int sun_grid_lat_count = kDefaultSunGridLatCount;
  // Warm starts, typically the component seeds of an earlier call (row-major body -> world, 9 per
  // pose). Densifying is not monotone (LI section 9.5.7); feeding back seeds is the only way to keep
  // what a sparser call found.
  std::vector<double> warm_seeds;
  // A schema-4 warm document's harvest (the survey facts the context reads), when --warm named a
  // survey document that passed the consistency gates. Unset: the context is computed inline
  // (a context-only T0-T2 assembly on this request's own config).
  std::optional<SurveyWarmSource> warm_survey;
  // The budget and the caller's one-way stop flag (see lumice_raypath.h): the deadline resolves
  // once at entry (0 = none for an internal caller; the bridge resolves the documented default),
  // the flag is polled at loop boundaries only. A stopped run reports meta.interrupted.
  int budget_ms = 0;
  const volatile int* interrupted = nullptr;
};

// ---- metadata: every conversion, as applied ------------------------------------------------------

enum class WavelengthSource { kDefault, kConfigSingle, kUser };
const char* WavelengthSourceName(WavelengthSource source);

// One crystal shape scalar as the kernel received it: `value` is the distribution's centre slot,
// which is the value itself (fixed), the interval midpoint (uniform), the mean (gauss) or the
// location (laplacian).
struct NominalShapeScalar {
  std::string name;  // the config key: height, prism_h, upper_h, lower_h, face_distance[i]
  double value = 0.0;
  DistributionType distribution = DistributionType::kNoRandom;
  double spread = 0.0;  // the distribution's second slot (0 for a fixed value)
};

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

struct SinglePathResult {
  SinglePathMetadata meta;
  Outcome outcome = Outcome::kDiscovered;
  PointMass point_mass;
  std::vector<FiberComponent> components;
  std::vector<IncompleteComponent> incomplete;
  DiscoverySummary discovery;
  ReachSummary reach;  // kDiscovered only
  SunSphereGrid sun_grid;
  // The declared measure's registration (kDiscovered only; the point-mass branch carries its own
  // face inside OrbitFactors). A rho consumer reads this before treating a rho of 0 as dark.
  MeasureRegistration measure_registration;
  // Where the target sits in the survey's structure: from the warm document's facts, or from a
  // context-only T0-T2 assembly on this request's config. Computed on demand (SurveyContextOf):
  // the inline branch runs the delta partition — the analysis's dominant cost — and consumers
  // that never read the context (the report chain, discovery internals) must not pay it.
  SurveyContextBlock survey_context;
  // The deferral inputs, retained until the context is first computed; dropped afterwards.
  bool survey_context_ready = false;
  std::shared_ptr<const ConfigManager> context_config;
  SinglePathRequest context_request;
};

// The whole analysis. On error `out` is left default-constructed.
Error AnalyzeSinglePath(const ConfigManager& config, const SinglePathRequest& request, SinglePathResult* out);

// The survey context on demand: the stored block when already computed, else built from the
// retained deferral inputs. NOT a cheap accessor — on a deferred result this runs the
// context-only T0-T2 assembly (the delta partition: seconds, ~13 s on the random full-dim
// scene) unless a warm document's facts fill the context directly. Pure — the result is not
// modified, so a second call on an unmaterialized result pays again; EnsureSurveyContext is
// the materialize-once entry the C API bridge runs before its one-shot serialization.
SurveyContextBlock SurveyContextOf(const SinglePathResult& result);

// Computes the context once and stores it (idempotent; drops the retained inputs). The C API
// bridge calls this before its one-shot serialization so a result's json computes it exactly once.
void EnsureSurveyContext(SinglePathResult* result);

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_SINGLE_PATH_ANALYSIS_HPP_
