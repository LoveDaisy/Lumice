#ifndef LUMICE_RAYPATH_SINGLE_PATH_ANALYSIS_HPP_
#define LUMICE_RAYPATH_SINGLE_PATH_ANALYSIS_HPP_

// Single-path analysis: everything the Analyze workspace computes once ONE single-layer raypath of
// ONE crystal entry has been chosen (doc/raypath-analysis.md section 5.1.8, phase one) — the
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
// Frames and angles are doc/coordinate-convention.md's: a pose is the row-major body -> world
// rotation R = Rz(azimuth - 180) Ry(-zenith) Rz(roll); world directions are propagation directions
// (sky_direction.hpp: the sky point a direction comes from sits at altitude asin(-z)).

#include <cstdint>
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
  double target_direction[3]{};       // world, propagation crystal -> observer
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

struct FiberComponent {
  ComponentKind kind = ComponentKind::kClosed;
  double seed[9]{};
  TraceEnd forward;
  TraceEnd backward;  // arc only (pose_count 0 for a closed component)
  // One ordered point list. Closed: the forward trace, seed first, its last pose the corrected
  // closing pose. Arc: the backward trace reversed, then the forward trace — the seed appears once,
  // at `seed_index` — so the list runs from one boundary event to the other.
  std::vector<PointDetail> points;
  int seed_index = 0;
  // The continuation's arclength (rad on SO(3)) between consecutive points: points.size() - 1 entries.
  std::vector<double> arclength_increments;
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
  SunSphereGrid sun_grid;
};

// The whole analysis. On error `out` is left default-constructed.
Error AnalyzeSinglePath(const ConfigManager& config, const SinglePathRequest& request, SinglePathResult* out);

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_SINGLE_PATH_ANALYSIS_HPP_
