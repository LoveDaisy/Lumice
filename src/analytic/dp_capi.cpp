#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "analytic/dp_boundary.hpp"
#include "analytic/dp_chromatic.hpp"
#include "analytic/dp_contour.hpp"
#include "analytic/dp_field.hpp"
#include "analytic/dp_focus.hpp"
#include "analytic/dp_partition.hpp"
#include "analytic/dp_weight_kink.hpp"
#include "analytic/path_evaluation.hpp"
#include "analytic/pose_density.hpp"
#include "lumice_analytic_core.h"

// The v8 C ABI translation of the u-S^2 field layer (lumice_analytic_core.h's version 8 block).
// Translation only: every value, name string and refusal text comes from the kernels; the
// invariants this layer must keep are the two fail-closed ones (a refused walk delivers an empty
// loop; a refused walk or an escaped partition delivers no intervals), pinned by the ABI tests.

namespace {
namespace an = lumice::analytic;

// Upper bound of DiagnoseClassTint's family sample: each pose is evaluated in full and the call
// cannot be cancelled (the band sum's own reasoning for its bound, section 4.6).
constexpr int kMaxPlateSamples = 10000000;

template <class T>
void ZeroAfterStructSize(T* out) {
  const size_t declared = out->struct_size;
  const size_t known = sizeof(T);
  const size_t end = declared < known ? declared : known;
  constexpr size_t kBegin = offsetof(T, struct_size) + sizeof(out->struct_size);
  if (end > kBegin) {
    std::memset(reinterpret_cast<unsigned char*>(out) + kBegin, 0, end - kBegin);
  }
}

// The result structs of version 8 are first-published whole: no optional group has ever existed,
// so a struct_size below the library's own sizeof has no readable prefix and is refused (the
// PathEvaluation rule, not the DiagnosticResult group rule).
template <class T>
LUMICE_ANALYTIC_ErrorCode CheckOut(T* out) {
  if (out == nullptr) {
    return LUMICE_ANALYTIC_ERR_NULL_ARG;
  }
  ZeroAfterStructSize(out);
  if (out->struct_size < sizeof(T)) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  return LUMICE_ANALYTIC_OK;
}

LUMICE_ANALYTIC_ErrorCode ToErrorCode(an::Status status) {
  switch (status) {
    case an::Status::kOk:
      return LUMICE_ANALYTIC_OK;
    case an::Status::kInvalidValue:
      return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
    case an::Status::kInvalidConfig:
      return LUMICE_ANALYTIC_ERR_INVALID_CONFIG;
  }
  return LUMICE_ANALYTIC_ERR_UNKNOWN;
}

LUMICE_ANALYTIC_ErrorCode ValidateIndex(double refractive_index) {
  if (!std::isfinite(refractive_index) || refractive_index <= 0.0) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  return LUMICE_ANALYTIC_OK;
}

// The crystal + face sequence every call of this layer starts from. Order of checks matches
// EvaluatePath's: NULL arguments, scalar ranges, then the crystal gate, then the face numbers.
LUMICE_ANALYTIC_ErrorCode SetupField(const LUMICE_ANALYTIC_Crystal* crystal, const int* faces, int face_count,
                                     double refractive_index, an::FaceNormalTable* table,
                                     an::FacePolygonTable* polygons, std::vector<int>* slots) {
  if (crystal == nullptr || faces == nullptr) {
    return LUMICE_ANALYTIC_ERR_NULL_ARG;
  }
  if (const LUMICE_ANALYTIC_ErrorCode code = ValidateIndex(refractive_index); code != LUMICE_ANALYTIC_OK) {
    return code;
  }
  if (face_count < 2 || face_count > an::kMaxFaceCount) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  if (auto status = an::BuildFaceNormals(*crystal, table, polygons); status != an::Status::kOk) {
    return ToErrorCode(status);
  }
  slots->resize(static_cast<size_t>(face_count));
  if (an::ResolveFaceSequence(*table, faces, face_count, slots->data()) != an::Status::kOk) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  return LUMICE_ANALYTIC_OK;
}

LUMICE_ANALYTIC_ErrorCode ToSpec(const LUMICE_ANALYTIC_PoseDensity& density, an::PoseDensitySpec* spec) {
  switch (density.family) {
    case LUMICE_ANALYTIC_POSE_RANDOM:
      spec->family = an::PoseFamily::kRandom;
      break;
    case LUMICE_ANALYTIC_POSE_COLUMN:
      spec->family = an::PoseFamily::kColumn;
      break;
    case LUMICE_ANALYTIC_POSE_PLATE:
      spec->family = an::PoseFamily::kPlate;
      break;
    case LUMICE_ANALYTIC_POSE_PARRY:
      spec->family = an::PoseFamily::kParry;
      break;
    case LUMICE_ANALYTIC_POSE_LOWITZ:
      spec->family = an::PoseFamily::kLowitz;
      break;
    default:
      return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  spec->zenith_mean_deg = density.zenith_mean_deg;
  spec->zenith_std_deg = density.zenith_std_deg;
  spec->roll_mean_deg = density.roll_mean_deg;
  spec->roll_std_deg = density.roll_std_deg;
  const char* invalid = an::PoseDensityError(*spec);
  return (invalid != nullptr && *invalid == '\0') ? LUMICE_ANALYTIC_OK : LUMICE_ANALYTIC_ERR_INVALID_VALUE;
}

int WalkStatusOf(an::WalkStatus status) {
  switch (status) {
    case an::WalkStatus::kOk:
      return LUMICE_ANALYTIC_WALK_OK;
    case an::WalkStatus::kStepsExhausted:
      return LUMICE_ANALYTIC_WALK_STEPS_EXHAUSTED;
    case an::WalkStatus::kStartNoPoint:
      return LUMICE_ANALYTIC_WALK_START_NO_POINT;
    case an::WalkStatus::kStartCoversAll:
      return LUMICE_ANALYTIC_WALK_START_COVERS_ALL;
    case an::WalkStatus::kStartNoEdge:
      return LUMICE_ANALYTIC_WALK_START_NO_EDGE;
    case an::WalkStatus::kCornerNotSimple:
      return LUMICE_ANALYTIC_WALK_CORNER_NOT_SIMPLE;
    case an::WalkStatus::kNotClosed:
      return LUMICE_ANALYTIC_WALK_NOT_CLOSED;
    case an::WalkStatus::kNotFinite:
      return LUMICE_ANALYTIC_WALK_NOT_FINITE;
    case an::WalkStatus::kBadOrientation:
      return LUMICE_ANALYTIC_WALK_BAD_ORIENTATION;
  }
  return LUMICE_ANALYTIC_WALK_OK;  // -Wswitch keeps this unreachable as the kernel grows
}

int EscapeRegimeOf(an::EscapeRegime regime) {
  switch (regime) {
    case an::EscapeRegime::kNotDiskUnaudited:
      return LUMICE_ANALYTIC_ESCAPE_NOT_DISK_UNAUDITED;
    case an::EscapeRegime::kNotDiskUnconverged:
      return LUMICE_ANALYTIC_ESCAPE_NOT_DISK_UNCONVERGED;
    case an::EscapeRegime::kNotDiskConfirmed:
      return LUMICE_ANALYTIC_ESCAPE_NOT_DISK_CONFIRMED;
    case an::EscapeRegime::kNotDiskCorrected:
      return LUMICE_ANALYTIC_ESCAPE_NOT_DISK_CORRECTED;
    case an::EscapeRegime::kSlabCreaseContradiction:
      return LUMICE_ANALYTIC_ESCAPE_SLAB_CREASE_CONTRADICTION;
    case an::EscapeRegime::kSlabCreaseNotCarried:
      return LUMICE_ANALYTIC_ESCAPE_SLAB_CREASE_NOT_CARRIED;
    case an::EscapeRegime::kSlabCreaseTouching:
      return LUMICE_ANALYTIC_ESCAPE_SLAB_CREASE_TOUCHING;
    case an::EscapeRegime::kSlabCreaseClosedRidge:
      return LUMICE_ANALYTIC_ESCAPE_SLAB_CREASE_CLOSED_RIDGE;
    case an::EscapeRegime::kMultipleInteriorCriticalPoints:
      return LUMICE_ANALYTIC_ESCAPE_MULTIPLE_INTERIOR_CRITICAL_POINTS;
    case an::EscapeRegime::kLoopExtremaNotAlternating:
      return LUMICE_ANALYTIC_ESCAPE_LOOP_EXTREMA_NOT_ALTERNATING;
    case an::EscapeRegime::kOddBoundaryCrossings:
      return LUMICE_ANALYTIC_ESCAPE_ODD_BOUNDARY_CROSSINGS;
    case an::EscapeRegime::kInteriorCriticalPointNotSimple:
      return LUMICE_ANALYTIC_ESCAPE_INTERIOR_CRITICAL_POINT_NOT_SIMPLE;
    case an::EscapeRegime::kSublevelNotReachingBoundary:
      return LUMICE_ANALYTIC_ESCAPE_SUBLEVEL_NOT_REACHING_BOUNDARY;
  }
  return LUMICE_ANALYTIC_ESCAPE_NOT_DISK_UNAUDITED;
}

// The ABI's copies of the kernels' refusal and verdict texts: the slug tables are static and
// could be pointed into, but the message strings are std::string — and one uniform mechanism
// (intern into the result's storage) is simpler to keep alive than two.
class Strings {
 public:
  const char* Intern(std::string text) {
    strings_.emplace_back(std::move(text));
    return strings_.back().c_str();
  }

 private:
  std::deque<std::string> strings_;  // push_back never invalidates earlier c_str()
};

int CriticalKindOf(an::CriticalKind kind) {
  switch (kind) {
    case an::CriticalKind::kMinimum:
      return LUMICE_ANALYTIC_CRITICAL_MINIMUM;
    case an::CriticalKind::kMaximum:
      return LUMICE_ANALYTIC_CRITICAL_MAXIMUM;
    case an::CriticalKind::kSaddle:
      return LUMICE_ANALYTIC_CRITICAL_SADDLE;
    case an::CriticalKind::kDegenerate:
      return LUMICE_ANALYTIC_CRITICAL_DEGENERATE;
  }
  return LUMICE_ANALYTIC_CRITICAL_DEGENERATE;
}

// ---- TraceBoundaryLoop ------------------------------------------------------------------------------------

struct BoundaryStorage {
  Strings strings;
  std::vector<double> critical_point_positions;
  std::vector<double> critical_point_values;
  std::vector<int> critical_point_kinds;
  std::vector<unsigned char> critical_point_flags;
  std::vector<double> corner_positions;
  std::vector<double> corner_values;
};

int TraceBoundaryLoopImpl(const LUMICE_ANALYTIC_Crystal* crystal, const int* faces, int face_count,
                          double refractive_index, LUMICE_ANALYTIC_BoundaryLoopResult* out) {
  an::FaceNormalTable table;
  an::FacePolygonTable polygons;
  std::vector<int> slots;
  if (const LUMICE_ANALYTIC_ErrorCode code =
          SetupField(crystal, faces, face_count, refractive_index, &table, &polygons, &slots);
      code != LUMICE_ANALYTIC_OK) {
    return code == LUMICE_ANALYTIC_ERR_NULL_ARG ? 3 : code == LUMICE_ANALYTIC_ERR_INVALID_CONFIG ? 4 : 1;
  }
  const an::DeviationField field(table, polygons, slots.data(), face_count, refractive_index);
  const an::WalkResult walk = an::WalkBoundary(field, an::BoundaryWalkOptions{});
  auto storage = std::make_unique<BoundaryStorage>();
  out->status = WalkStatusOf(walk.status);
  out->status_name = storage->strings.Intern(an::WalkStatusName(walk.status));
  if (walk.status != an::WalkStatus::kOk) {
    out->message = storage->strings.Intern(walk.message);
    out->storage = storage.release();
    return 0;
  }
  for (const an::BoundaryCriticalPoint& point : walk.loop.critical_points) {
    storage->critical_point_positions.insert(storage->critical_point_positions.end(), point.position,
                                             point.position + 3);
    storage->critical_point_values.push_back(point.value);
    storage->critical_point_kinds.push_back(CriticalKindOf(point.kind));
    storage->critical_point_flags.push_back(
        static_cast<unsigned char>((point.strict ? 1 : 0) | (point.corner ? 2 : 0)));
  }
  for (const an::LoopCorner& corner : walk.loop.corners) {
    storage->corner_positions.insert(storage->corner_positions.end(), corner.position, corner.position + 3);
    storage->corner_values.push_back(corner.value);
  }
  out->critical_point_count = static_cast<int>(walk.loop.critical_points.size());
  out->critical_point_positions = storage->critical_point_positions.data();
  out->critical_point_values = storage->critical_point_values.data();
  out->critical_point_kinds = storage->critical_point_kinds.data();
  out->critical_point_flags = storage->critical_point_flags.data();
  out->corner_count = static_cast<int>(walk.loop.corners.size());
  out->corner_positions = storage->corner_positions.data();
  out->corner_values = storage->corner_values.data();
  out->has_plateau = walk.loop.has_plateau ? 1 : 0;
  out->plateau_value = walk.loop.plateau_value;
  std::copy_n(walk.loop.first_point, 3, out->first_point);
  out->storage = storage.release();
  return 0;
}

// ---- TraceWeightKinks -------------------------------------------------------------------------------------

struct KinksStorage {
  Strings strings;
  std::vector<LUMICE_ANALYTIC_WeightKinkCurve> curves;
  std::vector<LUMICE_ANALYTIC_WeightKinkArc> arcs;
  std::vector<double> arc_points;
  std::vector<double> arc_values;
};

int CoverageOf(an::KinkCoverage coverage) {
  switch (coverage) {
    case an::KinkCoverage::kClosedFormAuthority:
      return LUMICE_ANALYTIC_KINK_CLOSED_FORM_AUTHORITY;
    case an::KinkCoverage::kMarchedUncertified:
      return LUMICE_ANALYTIC_KINK_MARCHED_UNCERTIFIED;
  }
  return LUMICE_ANALYTIC_KINK_MARCHED_UNCERTIFIED;
}

int TraceWeightKinksImpl(const LUMICE_ANALYTIC_Crystal* crystal, const int* faces, int face_count,
                         double refractive_index, LUMICE_ANALYTIC_WeightKinksResult* out) {
  an::FaceNormalTable table;
  an::FacePolygonTable polygons;
  std::vector<int> slots;
  if (const LUMICE_ANALYTIC_ErrorCode code =
          SetupField(crystal, faces, face_count, refractive_index, &table, &polygons, &slots);
      code != LUMICE_ANALYTIC_OK) {
    return code == LUMICE_ANALYTIC_ERR_NULL_ARG ? 3 : code == LUMICE_ANALYTIC_ERR_INVALID_CONFIG ? 4 : 1;
  }
  const an::DeviationField field(table, polygons, slots.data(), face_count, refractive_index);
  const std::vector<an::KinkCurve> kinks = an::WeightKinks(field, an::KinkOptions{});
  auto storage = std::make_unique<KinksStorage>();
  storage->curves.reserve(kinks.size());
  for (const an::KinkCurve& kink : kinks) {
    LUMICE_ANALYTIC_WeightKinkCurve row{};
    row.step = kink.step;
    row.margin = kink.margin;
    row.index = kink.index;
    row.coverage = CoverageOf(kink.coverage);
    row.has_normal = kink.has_normal ? 1 : 0;
    std::copy_n(kink.normal, 3, row.normal);
    row.failed_seeds = kink.failed_seeds;
    row.status = WalkStatusOf(kink.status);
    row.status_name = storage->strings.Intern(an::WalkStatusName(kink.status));
    row.note = storage->strings.Intern(kink.note);
    row.spread = kink.Spread();
    row.first_arc = static_cast<int>(storage->arcs.size());
    for (const an::KinkArc& arc : kink.arcs) {
      LUMICE_ANALYTIC_WeightKinkArc header{};
      header.first_point = static_cast<int>(storage->arc_values.size());
      header.point_count = static_cast<int>(arc.values.size());
      header.closed = arc.closed ? 1 : 0;
      header.end_gate[0] = arc.end_gates[0];
      header.end_gate[1] = arc.end_gates[1];
      storage->arcs.push_back(header);
      storage->arc_points.insert(storage->arc_points.end(), arc.points.begin(), arc.points.end());
      storage->arc_values.insert(storage->arc_values.end(), arc.values.begin(), arc.values.end());
    }
    row.arc_count = static_cast<int>(kink.arcs.size());
    storage->curves.push_back(row);
  }
  out->curve_count = static_cast<int>(storage->curves.size());
  out->curves = storage->curves.data();
  out->arc_count = static_cast<int>(storage->arcs.size());
  out->arcs = storage->arcs.data();
  out->arc_point_count = static_cast<int>(storage->arc_values.size());
  out->arc_points = storage->arc_points.data();
  out->arc_values = storage->arc_values.data();
  out->storage = storage.release();
  return 0;
}

// ---- ClassifyCriticalStructure ----------------------------------------------------------------------------

int OnsetLocationOf(an::OnsetLocation location) {
  switch (location) {
    case an::OnsetLocation::kInterior:
      return LUMICE_ANALYTIC_ONSET_INTERIOR;
    case an::OnsetLocation::kBoundary:
      return LUMICE_ANALYTIC_ONSET_BOUNDARY;
  }
  return LUMICE_ANALYTIC_ONSET_BOUNDARY;
}

int OnsetSourceOf(an::OnsetSource source) {
  switch (source) {
    case an::OnsetSource::kInteriorMinimum:
      return LUMICE_ANALYTIC_ONSET_INTERIOR_MINIMUM;
    case an::OnsetSource::kInteriorMaximum:
      return LUMICE_ANALYTIC_ONSET_INTERIOR_MAXIMUM;
    case an::OnsetSource::kInteriorSaddle:
      return LUMICE_ANALYTIC_ONSET_INTERIOR_SADDLE;
    case an::OnsetSource::kInteriorDegenerate:
      return LUMICE_ANALYTIC_ONSET_INTERIOR_DEGENERATE;
    case an::OnsetSource::kSlabAxis:
      return LUMICE_ANALYTIC_ONSET_SLAB_AXIS;
    case an::OnsetSource::kSlabCircle:
      return LUMICE_ANALYTIC_ONSET_SLAB_CIRCLE;
    case an::OnsetSource::kBoundaryExtremum:
      return LUMICE_ANALYTIC_ONSET_BOUNDARY_EXTREMUM;
    case an::OnsetSource::kCorner:
      return LUMICE_ANALYTIC_ONSET_CORNER;
  }
  return LUMICE_ANALYTIC_ONSET_CORNER;
}

int OnsetProfileOf(an::OnsetProfile profile) {
  switch (profile) {
    case an::OnsetProfile::kFiniteJump:
      return LUMICE_ANALYTIC_PROFILE_FINITE_JUMP;
    case an::OnsetProfile::kLogDivergence:
      return LUMICE_ANALYTIC_PROFILE_LOG_DIVERGENCE;
    case an::OnsetProfile::kInverseSqrtDivergence:
      return LUMICE_ANALYTIC_PROFILE_INVERSE_SQRT_DIVERGENCE;
    case an::OnsetProfile::kConePoint:
      return LUMICE_ANALYTIC_PROFILE_CONE_POINT;
    case an::OnsetProfile::kCrease:
      return LUMICE_ANALYTIC_PROFILE_CREASE;
    case an::OnsetProfile::kBoundaryOnset:
      return LUMICE_ANALYTIC_PROFILE_BOUNDARY_ONSET;
    case an::OnsetProfile::kDegenerate:
      return LUMICE_ANALYTIC_PROFILE_DEGENERATE;
  }
  return LUMICE_ANALYTIC_PROFILE_DEGENERATE;
}

int EscapeStatusFromMessage(const std::string& message) {
  // The kernel's classify carries the walk refusal only as its composed text ("slug: detail"),
  // built from the same slug table; the typed open-enum status is recovered by prefix match
  // against that table (kOk excluded: a refusal never maps to it).
  for (int v = static_cast<int>(an::WalkStatus::kStepsExhausted);
       v <= static_cast<int>(an::WalkStatus::kBadOrientation); ++v) {
    if (message.rfind(std::string(an::WalkStatusName(static_cast<an::WalkStatus>(v))) + ":", 0) == 0) {
      return WalkStatusOf(static_cast<an::WalkStatus>(v));
    }
  }
  return LUMICE_ANALYTIC_WALK_NOT_FINITE;
}

struct ClassificationStorage {
  Strings strings;
  std::vector<LUMICE_ANALYTIC_CriticalOnset> onsets;
  std::vector<double> widths;
};

int ClassifyCriticalStructureImpl(const LUMICE_ANALYTIC_Crystal* crystal, const int* faces, int face_count,
                                  double refractive_index, LUMICE_ANALYTIC_PoseDensity density,
                                  LUMICE_ANALYTIC_ClassificationResult* out) {
  an::FaceNormalTable table;
  an::FacePolygonTable polygons;
  std::vector<int> slots;
  if (const LUMICE_ANALYTIC_ErrorCode code =
          SetupField(crystal, faces, face_count, refractive_index, &table, &polygons, &slots);
      code != LUMICE_ANALYTIC_OK) {
    return code == LUMICE_ANALYTIC_ERR_NULL_ARG ? 3 : code == LUMICE_ANALYTIC_ERR_INVALID_CONFIG ? 4 : 1;
  }
  an::PoseDensitySpec spec;
  if (const LUMICE_ANALYTIC_ErrorCode code = ToSpec(density, &spec); code != LUMICE_ANALYTIC_OK) {
    return 1;
  }
  const an::FocusingClassification classification =
      an::Classify(table, polygons, slots.data(), face_count, spec, refractive_index);
  auto storage = std::make_unique<ClassificationStorage>();
  out->halo_map_rank = classification.halo_map_rank;
  out->escaped = classification.escaped ? 1 : 0;
  if (classification.escaped) {
    out->escape_status = EscapeStatusFromMessage(classification.escape_message);
    out->escape_status_name = storage->strings.Intern(an::WalkStatusName(static_cast<an::WalkStatus>(
        out->escape_status == LUMICE_ANALYTIC_WALK_OK ? LUMICE_ANALYTIC_WALK_NOT_FINITE : out->escape_status)));
    out->escape_message = storage->strings.Intern(classification.escape_message);
  }
  for (const an::CriticalOnset& onset : classification.onsets) {
    LUMICE_ANALYTIC_CriticalOnset row{};
    row.value = onset.value;
    row.location = OnsetLocationOf(onset.location);
    row.source = OnsetSourceOf(onset.source);
    row.profile = OnsetProfileOf(onset.profile);
    row.gradient_norm = onset.gradient_norm;
    row.has_measure_limit = onset.has_measure_limit ? 1 : 0;
    row.measure_limit = onset.measure_limit;
    row.multiplicity = onset.multiplicity;
    storage->onsets.push_back(row);
  }
  out->onset_count = static_cast<int>(storage->onsets.size());
  out->onsets = storage->onsets.data();
  out->has_gradient_norm_range = classification.has_gradient_norm_range ? 1 : 0;
  out->gradient_norm_range[0] = classification.gradient_norm_range[0];
  out->gradient_norm_range[1] = classification.gradient_norm_range[1];
  out->confined_dimensions = classification.confined.dimensions;
  storage->widths = classification.confined.widths_rad;
  out->confined_width_count = static_cast<int>(storage->widths.size());
  out->confined_widths_rad = storage->widths.data();
  out->family_pinned = classification.family_pinned ? 1 : 0;
  out->mechanism = storage->strings.Intern(classification.Mechanism());
  out->storage = storage.release();
  return 0;
}

// ---- PartitionDeviationAxis -------------------------------------------------------------------------------

struct PartitionStorage {
  Strings strings;
  std::vector<LUMICE_ANALYTIC_DeviationInterval> intervals;
};

int PartitionDeviationAxisImpl(const LUMICE_ANALYTIC_Crystal* crystal, const int* faces, int face_count,
                               double refractive_index, LUMICE_ANALYTIC_PartitionResult* out) {
  an::FaceNormalTable table;
  an::FacePolygonTable polygons;
  std::vector<int> slots;
  if (const LUMICE_ANALYTIC_ErrorCode code =
          SetupField(crystal, faces, face_count, refractive_index, &table, &polygons, &slots);
      code != LUMICE_ANALYTIC_OK) {
    return code == LUMICE_ANALYTIC_ERR_NULL_ARG ? 3 : code == LUMICE_ANALYTIC_ERR_INVALID_CONFIG ? 4 : 1;
  }
  const an::DeviationField field(table, polygons, slots.data(), face_count, refractive_index);
  const an::WalkResult walk = an::WalkBoundary(field, an::BoundaryWalkOptions{});
  auto storage = std::make_unique<PartitionStorage>();
  out->walk_status = WalkStatusOf(walk.status);
  out->walk_status_name = storage->strings.Intern(an::WalkStatusName(walk.status));
  if (walk.status != an::WalkStatus::kOk) {
    // No loop, no partition: the certificate is unavailable at its kind-2 object (fail closed,
    // like every refusal on this layer).
    out->walk_message = storage->strings.Intern(walk.message);
    out->lattice_n = an::DomainTopologyOf(field, an::kTopologyLatticeN, nullptr, 0).lattice_n;
    out->storage = storage.release();
    return 0;
  }
  std::vector<an::InteriorCriticalPoint> interior;
  an::DegenerateFoldSet fold_set;
  const an::DegenerateFoldSet* fold_set_ptr = nullptr;
  if (field.fold().degenerate) {
    fold_set = an::BuildDegenerateFoldSet(field, an::kFoldCircleSamples);
    fold_set_ptr = &fold_set;
    interior = an::SlabInteriorCriticalPoints(field, fold_set);
  } else {
    interior = an::InteriorCriticalPointsOf(field, an::InteriorNewtonOptions{});
  }
  const an::DomainTopology topology = an::DomainTopologyOf(field, an::kTopologyLatticeN, nullptr, 0);
  const an::PartitionResult partition = an::IntervalPartition(field, interior, fold_set_ptr, walk.loop, topology);
  out->escaped = partition.escaped ? 1 : 0;
  if (partition.escaped) {
    out->regime = EscapeRegimeOf(partition.regime);
    out->regime_name = storage->strings.Intern(an::EscapeRegimeName(partition.regime));
    out->message = storage->strings.Intern(partition.message);
  }
  for (const an::DeviationInterval& interval : partition.intervals) {
    LUMICE_ANALYTIC_DeviationInterval row{};
    row.lower = interval.lower;
    row.upper = interval.upper;
    row.n_components = interval.n_components;
    row.n_closed = interval.n_closed;
    row.n_open = interval.n_open;
    storage->intervals.push_back(row);
  }
  out->interval_count = static_cast<int>(storage->intervals.size());
  out->intervals = storage->intervals.data();
  out->lattice_n = topology.lattice_n;
  out->domain_components = topology.domain_components;
  out->complement_components = topology.complement_components;
  out->audit_verdict =
      storage->strings.Intern(topology.has_grid_audit ? an::AuditVerdictName(topology.grid_audit.verdict) : "");
  out->storage = storage.release();
  return 0;
}

// ---- TraceWavelengthCriticalTable -------------------------------------------------------------------------

struct WavelengthStorage {
  Strings strings;
  std::vector<const char*> labels;
  std::vector<double> indices;
  std::vector<LUMICE_ANALYTIC_WavelengthOnsetRow> onsets;
  std::vector<double> values_deg;
};

int TraceWavelengthCriticalTableImpl(const LUMICE_ANALYTIC_Crystal* crystal, const int* faces, int face_count,
                                     LUMICE_ANALYTIC_PoseDensity density, const char* const* labels,
                                     const double* indices, int count, LUMICE_ANALYTIC_WavelengthTableResult* out) {
  an::FaceNormalTable table;
  an::FacePolygonTable polygons;
  std::vector<int> slots;
  // The setup's index gate is nominal here (the face sequence's validity does not depend on n);
  // every per-index value is validated in the loop below.
  if (const LUMICE_ANALYTIC_ErrorCode code = SetupField(crystal, faces, face_count, 1.0, &table, &polygons, &slots);
      code != LUMICE_ANALYTIC_OK) {
    return code == LUMICE_ANALYTIC_ERR_NULL_ARG ? 3 : code == LUMICE_ANALYTIC_ERR_INVALID_CONFIG ? 4 : 1;
  }
  an::PoseDensitySpec spec;
  if (const LUMICE_ANALYTIC_ErrorCode code = ToSpec(density, &spec); code != LUMICE_ANALYTIC_OK) {
    return 1;
  }
  if (count < 0 || (count > 0 && (labels == nullptr || indices == nullptr))) {
    return count < 0 ? 1 : 3;
  }
  for (int k = 0; k < count; ++k) {
    if (!std::isfinite(indices[k]) || indices[k] <= 0.0) {
      return 1;
    }
  }
  std::vector<std::string> label_strings;
  label_strings.reserve(static_cast<size_t>(count));
  for (int k = 0; k < count; ++k) {
    label_strings.emplace_back(labels[k] == nullptr ? "" : labels[k]);
  }
  const std::vector<double> index_values(indices, indices + static_cast<size_t>(count));
  const an::WavelengthCriticalTable result =
      an::WavelengthCriticalTableOf(table, polygons, slots.data(), face_count, spec, label_strings, index_values);
  auto storage = std::make_unique<WavelengthStorage>();
  out->escaped = result.escaped ? 1 : 0;
  out->message = result.escaped ? storage->strings.Intern(result.message) : nullptr;
  for (const std::string& label : result.labels) {
    storage->labels.push_back(storage->strings.Intern(label));
  }
  storage->indices = result.indices;
  out->label_count = static_cast<int>(result.labels.size());
  out->labels = storage->labels.data();
  out->indices = storage->indices.data();
  for (const an::WavelengthOnsetShift& onset : result.onsets) {
    LUMICE_ANALYTIC_WavelengthOnsetRow row{};
    row.location = OnsetLocationOf(onset.location);
    row.source = OnsetSourceOf(onset.source);
    row.profile = OnsetProfileOf(onset.profile);
    row.jacobian_focusing = onset.jacobian_focusing ? 1 : 0;
    row.first_value = static_cast<int>(storage->values_deg.size());
    row.displacement_deg = onset.displacement_deg;
    storage->onsets.push_back(row);
    storage->values_deg.insert(storage->values_deg.end(), onset.values_deg.begin(), onset.values_deg.end());
  }
  out->onset_count = static_cast<int>(storage->onsets.size());
  out->onsets = storage->onsets.data();
  out->values_deg = storage->values_deg.data();
  out->storage = storage.release();
  return 0;
}

// ---- TraceRestrictedFamilyCurve ---------------------------------------------------------------------------

int ExistenceOf(an::CurveExistence existence) {
  switch (existence) {
    case an::CurveExistence::kComputed:
      return LUMICE_ANALYTIC_EXISTENCE_COMPUTED;
    case an::CurveExistence::kEscaped:
      return LUMICE_ANALYTIC_EXISTENCE_ESCAPED;
    case an::CurveExistence::kWalkTruncated:
      return LUMICE_ANALYTIC_EXISTENCE_WALK_TRUNCATED;
    case an::CurveExistence::kS4Declared:
      return LUMICE_ANALYTIC_EXISTENCE_S4_DECLARED;
  }
  return LUMICE_ANALYTIC_EXISTENCE_COMPUTED;
}

struct CurveStorage {
  Strings strings;
  std::vector<double> u;
  std::vector<double> tangent;
  std::vector<double> d_p;
  std::vector<double> wavelengths_nm;
  std::vector<double> indices;
  std::vector<double> critical_d_p;
  std::vector<double> support_param;
};

int TraceRestrictedFamilyCurveImpl(const LUMICE_ANALYTIC_Crystal* crystal, const int* faces, int face_count,
                                   LUMICE_ANALYTIC_PoseDensity density, const double* sun_hat, double base_index,
                                   const double* wavelengths_nm, const double* indices, int wavelength_count, int grid,
                                   LUMICE_ANALYTIC_RestrictedCurveResult* out) {
  if (sun_hat == nullptr) {
    return 3;
  }
  if (grid < 1 || wavelength_count < 0 || (wavelength_count > 0 && (wavelengths_nm == nullptr || indices == nullptr)) ||
      !std::isfinite(base_index) || base_index <= 0.0 || !an::ValidateUnitVector(sun_hat)) {
    return 1;
  }
  for (int k = 0; k < wavelength_count; ++k) {
    if (!std::isfinite(indices[k]) || indices[k] <= 0.0 || !std::isfinite(wavelengths_nm[k])) {
      return 1;
    }
  }
  an::FaceNormalTable table;
  an::FacePolygonTable polygons;
  std::vector<int> slots;
  if (const LUMICE_ANALYTIC_ErrorCode code =
          SetupField(crystal, faces, face_count, base_index, &table, &polygons, &slots);
      code != LUMICE_ANALYTIC_OK) {
    return code == LUMICE_ANALYTIC_ERR_NULL_ARG ? 3 : code == LUMICE_ANALYTIC_ERR_INVALID_CONFIG ? 4 : 1;
  }
  an::PoseDensitySpec spec;
  if (const LUMICE_ANALYTIC_ErrorCode code = ToSpec(density, &spec); code != LUMICE_ANALYTIC_OK) {
    return 1;
  }
  const an::RestrictedFamilyCurve curve =
      an::MakeRestrictedFamilyCurve(table, polygons, slots.data(), face_count, spec, sun_hat, base_index,
                                    wavelengths_nm, indices, wavelength_count, grid);
  auto storage = std::make_unique<CurveStorage>();
  out->closed = curve.closed ? 1 : 0;
  out->existence = ExistenceOf(curve.existence);
  out->note = storage->strings.Intern(curve.note);
  storage->u = curve.u;
  storage->tangent = curve.tangent;
  storage->d_p = curve.d_p;
  storage->wavelengths_nm = curve.wavelengths_nm;
  storage->indices = curve.indices;
  storage->critical_d_p = curve.critical_d_p;
  storage->support_param = curve.support_param;
  out->point_count = static_cast<int>(curve.d_p.size());
  out->u = storage->u.data();
  out->tangent = storage->tangent.data();
  out->d_p = storage->d_p.data();
  out->wavelength_count = static_cast<int>(storage->wavelengths_nm.size());
  out->wavelengths_nm = storage->wavelengths_nm.data();
  out->indices = storage->indices.data();
  out->critical_d_p = storage->critical_d_p.data();
  out->support_param = storage->support_param.data();
  out->routed_nonfinite = curve.routed_nonfinite;
  out->storage = storage.release();
  return 0;
}

// ---- DiagnoseChromatic / DiagnoseClassTint ----------------------------------------------------------------

int FeatureKindOf(an::ChromaticFeatureKind kind) {
  switch (kind) {
    case an::ChromaticFeatureKind::kEdge:
      return LUMICE_ANALYTIC_CHROMATIC_EDGE;
    case an::ChromaticFeatureKind::kGateEdge:
      return LUMICE_ANALYTIC_CHROMATIC_GATE_EDGE;
  }
  return LUMICE_ANALYTIC_CHROMATIC_GATE_EDGE;
}

int ColorOf(an::ChromaticColor color) {
  switch (color) {
    case an::ChromaticColor::kBlue:
      return LUMICE_ANALYTIC_COLOR_BLUE;
    case an::ChromaticColor::kRed:
      return LUMICE_ANALYTIC_COLOR_RED;
    case an::ChromaticColor::kWhite:
      return LUMICE_ANALYTIC_COLOR_WHITE;
    case an::ChromaticColor::kNone:
      return LUMICE_ANALYTIC_COLOR_NONE;
  }
  return LUMICE_ANALYTIC_COLOR_NONE;
}

int VerdictKindOf(an::ChromaticVerdictKind kind) {
  switch (kind) {
    case an::ChromaticVerdictKind::kEdge:
      return LUMICE_ANALYTIC_VERDICT_EDGE;
    case an::ChromaticVerdictKind::kGateEdge:
      return LUMICE_ANALYTIC_VERDICT_GATE_EDGE;
    case an::ChromaticVerdictKind::kTint:
      return LUMICE_ANALYTIC_VERDICT_TINT;
    case an::ChromaticVerdictKind::kUnresolved:
      return LUMICE_ANALYTIC_VERDICT_UNRESOLVED;
    case an::ChromaticVerdictKind::kNone:
      return LUMICE_ANALYTIC_VERDICT_NONE;
  }
  return LUMICE_ANALYTIC_VERDICT_NONE;
}

void FillThresholds(const an::ChromaticThresholds& from, LUMICE_ANALYTIC_ChromaticThresholds* to) {
  to->n_red = from.n_red;
  to->n_blue = from.n_blue;
  to->edge_min_shift_rad = from.edge_min_shift_rad;
  to->edge_spread_per_shift = from.edge_spread_per_shift;
  to->calibration_white_max_deviation = from.calibration_white_max_deviation;
  to->tint_ratio_min = from.tint_ratio_min;
}

class ChromaticStorage {
 public:
  Strings strings;
  std::vector<LUMICE_ANALYTIC_ChromaticFeature> features;
  std::vector<const char*> notes;
  std::vector<int> member_sizes;
  std::vector<int> members;
  std::vector<int> lit_sizes_red;
  std::vector<int> lit_members_red;
  std::vector<int> lit_sizes_blue;
  std::vector<int> lit_members_blue;

  void FillVerdict(const an::ChromaticVerdict& verdict, LUMICE_ANALYTIC_ChromaticResult* out) {
    out->verdict_kind = VerdictKindOf(verdict.kind);
    out->color = ColorOf(verdict.color);
    out->visible = verdict.visible ? 1 : 0;
    out->has_position = verdict.has_position ? 1 : 0;
    out->position = verdict.position;
    out->coverage_complete = verdict.coverage_complete ? 1 : 0;
    for (const an::ChromaticFeature& feature : verdict.features) {
      LUMICE_ANALYTIC_ChromaticFeature row{};
      row.kind = FeatureKindOf(feature.kind);
      row.source = strings.Intern(feature.source);
      row.color = ColorOf(feature.color);
      row.positive_fraction = feature.positive_fraction;
      row.delta_red = feature.delta_red;
      row.delta_blue = feature.delta_blue;
      row.shift = feature.shift;
      row.spread = feature.spread;
      row.direction_dispersion = feature.direction_dispersion;
      row.contrast = feature.contrast;
      row.weight = feature.weight;
      row.lit_fraction = feature.lit_fraction;
      row.visible = feature.visible ? 1 : 0;
      features.push_back(row);
    }
    for (const std::string& note : verdict.notes) {
      notes.push_back(strings.Intern(note));
    }
    FillThresholds(an::ChromaticThresholdsSnapshot(), &out->thresholds);
    out->has_tint = verdict.has_tint ? 1 : 0;
    out->energy_red = verdict.tint.energy_red;
    out->energy_blue = verdict.tint.energy_blue;
    out->ratio = verdict.tint.ratio;
    out->tir_fraction_red = verdict.tint.tir_fraction_red;
    out->tir_fraction_blue = verdict.tint.tir_fraction_blue;
    out->tint_direction_dispersion = verdict.tint.direction_dispersion;
  }

  void FillMembers(const an::ClassVerdict& klass, LUMICE_ANALYTIC_ChromaticResult* out) {
    for (const std::vector<int>& member : klass.members) {
      member_sizes.push_back(static_cast<int>(member.size()));
      members.insert(members.end(), member.begin(), member.end());
    }
    for (const std::vector<int>& member : klass.lit_members_red) {
      lit_sizes_red.push_back(static_cast<int>(member.size()));
      lit_members_red.insert(lit_members_red.end(), member.begin(), member.end());
    }
    for (const std::vector<int>& member : klass.lit_members_blue) {
      lit_sizes_blue.push_back(static_cast<int>(member.size()));
      lit_members_blue.insert(lit_members_blue.end(), member.begin(), member.end());
    }
    out->member_count = static_cast<int>(klass.members.size());
    out->member_sizes = member_sizes.data();
    out->members = members.data();
    out->lit_red_count = static_cast<int>(klass.lit_members_red.size());
    out->lit_sizes_red = lit_sizes_red.data();
    out->lit_members_red = lit_members_red.data();
    out->lit_blue_count = static_cast<int>(klass.lit_members_blue.size());
    out->lit_sizes_blue = lit_sizes_blue.data();
    out->lit_members_blue = lit_members_blue.data();
  }

  void Finish(LUMICE_ANALYTIC_ChromaticResult* out) {
    out->feature_count = static_cast<int>(features.size());
    out->features = features.data();
    out->note_count = static_cast<int>(notes.size());
    out->notes = notes.data();
  }
};

int ValidateChromaticIndices(double n_red, double n_blue) {
  return ValidateIndex(n_red) == LUMICE_ANALYTIC_OK && ValidateIndex(n_blue) == LUMICE_ANALYTIC_OK ? 0 : 1;
}

int DiagnoseChromaticImpl(const LUMICE_ANALYTIC_Crystal* crystal, const int* faces, int face_count, double n_red,
                          double n_blue, LUMICE_ANALYTIC_ChromaticResult* out) {
  an::FaceNormalTable table;
  an::FacePolygonTable polygons;
  std::vector<int> slots;
  if (const LUMICE_ANALYTIC_ErrorCode code = SetupField(crystal, faces, face_count, n_blue, &table, &polygons, &slots);
      code != LUMICE_ANALYTIC_OK) {
    return code == LUMICE_ANALYTIC_ERR_NULL_ARG ? 3 : code == LUMICE_ANALYTIC_ERR_INVALID_CONFIG ? 4 : 1;
  }
  const an::ChromaticVerdict verdict = an::Diagnose(table, polygons, slots.data(), face_count, n_red, n_blue);
  auto storage = std::make_unique<ChromaticStorage>();
  storage->FillVerdict(verdict, out);
  storage->Finish(out);
  out->storage = storage.release();
  return 0;
}

int DiagnoseClassTintImpl(const LUMICE_ANALYTIC_Crystal* crystal, const int* faces, int face_count,
                          const LUMICE_ANALYTIC_PlateFamily& family, double n_red, double n_blue,
                          LUMICE_ANALYTIC_ChromaticResult* out) {
  if (!std::isfinite(family.sun_altitude_deg) || !std::isfinite(family.zenith_std_deg) ||
      family.zenith_std_deg <= 0.0 || family.samples < 1 || family.samples > kMaxPlateSamples) {
    return 1;
  }
  an::FaceNormalTable table;
  an::FacePolygonTable polygons;
  std::vector<int> slots;
  if (const LUMICE_ANALYTIC_ErrorCode code = SetupField(crystal, faces, face_count, n_blue, &table, &polygons, &slots);
      code != LUMICE_ANALYTIC_OK) {
    return code == LUMICE_ANALYTIC_ERR_NULL_ARG ? 3 : code == LUMICE_ANALYTIC_ERR_INVALID_CONFIG ? 4 : 1;
  }
  an::PlateFamilySpec spec;
  spec.sun_altitude_deg = family.sun_altitude_deg;
  spec.zenith_std_deg = family.zenith_std_deg;
  spec.samples = family.samples;
  spec.seed = family.seed;
  // The representative is FACE NUMBERS here — the PBD orbit is a label orbit (the kernel test's
  // own convention); slots would read as labels outside the universe.
  const an::ClassVerdict klass = an::DiagnoseClass(table, polygons, faces, face_count, spec, n_red, n_blue);
  auto storage = std::make_unique<ChromaticStorage>();
  storage->FillVerdict(klass.verdict, out);
  storage->FillMembers(klass, out);
  storage->Finish(out);
  out->storage = storage.release();
  return 0;
}

// The shared wrapper of the eight calls: NULL check, zero-fill, struct_size gate, the try/catch
// that keeps every exception inside (ERR_UNKNOWN, out zero-filled), and the impl-status mapping
// (0 ok / 1 invalid value / 3 null arg / 4 invalid config).
template <class Result, class F>
LUMICE_ANALYTIC_ErrorCode Invoke(Result* out, F&& call) {
  if (const LUMICE_ANALYTIC_ErrorCode gate = CheckOut(out); gate != LUMICE_ANALYTIC_OK) {
    return gate;
  }
  try {
    const int status = call(out);
    if (status != 0) {
      ZeroAfterStructSize(out);
      return status == 3 ? LUMICE_ANALYTIC_ERR_NULL_ARG :
             status == 4 ? LUMICE_ANALYTIC_ERR_INVALID_CONFIG :
                           LUMICE_ANALYTIC_ERR_INVALID_VALUE;
    }
    return LUMICE_ANALYTIC_OK;
  } catch (...) {
    ZeroAfterStructSize(out);
    return LUMICE_ANALYTIC_ERR_UNKNOWN;
  }
}

}  // namespace

LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_TraceBoundaryLoop(const LUMICE_ANALYTIC_Crystal* crystal, const int* faces,
                                                            int face_count, double refractive_index,
                                                            LUMICE_ANALYTIC_BoundaryLoopResult* out) {
  return Invoke(out,
                [&](auto* full) { return TraceBoundaryLoopImpl(crystal, faces, face_count, refractive_index, full); });
}

LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_TraceWeightKinks(const LUMICE_ANALYTIC_Crystal* crystal, const int* faces,
                                                           int face_count, double refractive_index,
                                                           LUMICE_ANALYTIC_WeightKinksResult* out) {
  return Invoke(out,
                [&](auto* full) { return TraceWeightKinksImpl(crystal, faces, face_count, refractive_index, full); });
}

LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_ClassifyCriticalStructure(const LUMICE_ANALYTIC_Crystal* crystal,
                                                                    const int* faces, int face_count,
                                                                    double refractive_index,
                                                                    LUMICE_ANALYTIC_PoseDensity density,
                                                                    LUMICE_ANALYTIC_ClassificationResult* out) {
  return Invoke(out, [&](auto* full) {
    return ClassifyCriticalStructureImpl(crystal, faces, face_count, refractive_index, density, full);
  });
}

LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_PartitionDeviationAxis(const LUMICE_ANALYTIC_Crystal* crystal,
                                                                 const int* faces, int face_count,
                                                                 double refractive_index,
                                                                 LUMICE_ANALYTIC_PartitionResult* out) {
  return Invoke(
      out, [&](auto* full) { return PartitionDeviationAxisImpl(crystal, faces, face_count, refractive_index, full); });
}

LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_TraceWavelengthCriticalTable(
    const LUMICE_ANALYTIC_Crystal* crystal, const int* faces, int face_count, LUMICE_ANALYTIC_PoseDensity density,
    const char* const* labels, const double* indices, int count, LUMICE_ANALYTIC_WavelengthTableResult* out) {
  return Invoke(out, [&](auto* full) {
    return TraceWavelengthCriticalTableImpl(crystal, faces, face_count, density, labels, indices, count, full);
  });
}

LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_TraceRestrictedFamilyCurve(
    const LUMICE_ANALYTIC_Crystal* crystal, const int* faces, int face_count, LUMICE_ANALYTIC_PoseDensity density,
    const double sun_hat[3], double base_index, const double* wavelengths_nm, const double* indices,
    int wavelength_count, int grid, LUMICE_ANALYTIC_RestrictedCurveResult* out) {
  return Invoke(out, [&](auto* full) {
    return TraceRestrictedFamilyCurveImpl(crystal, faces, face_count, density, sun_hat, base_index, wavelengths_nm,
                                          indices, wavelength_count, grid, full);
  });
}

LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_DiagnoseChromatic(const LUMICE_ANALYTIC_Crystal* crystal, const int* faces,
                                                            int face_count, double n_red, double n_blue,
                                                            LUMICE_ANALYTIC_ChromaticResult* out) {
  return Invoke(out,
                [&](auto* full) { return DiagnoseChromaticImpl(crystal, faces, face_count, n_red, n_blue, full); });
}

LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_DiagnoseClassTint(const LUMICE_ANALYTIC_Crystal* crystal, const int* faces,
                                                            int face_count, LUMICE_ANALYTIC_PlateFamily family,
                                                            double n_red, double n_blue,
                                                            LUMICE_ANALYTIC_ChromaticResult* out) {
  return Invoke(
      out, [&](auto* full) { return DiagnoseClassTintImpl(crystal, faces, face_count, family, n_red, n_blue, full); });
}

void LUMICE_ANALYTIC_ReleaseBoundaryLoopResult(LUMICE_ANALYTIC_BoundaryLoopResult* result) {
  if (result == nullptr || result->struct_size < sizeof(LUMICE_ANALYTIC_BoundaryLoopResult)) {
    return;
  }
  std::unique_ptr<BoundaryStorage> storage(static_cast<BoundaryStorage*>(result->storage));
  ZeroAfterStructSize(result);
}

void LUMICE_ANALYTIC_ReleaseWeightKinksResult(LUMICE_ANALYTIC_WeightKinksResult* result) {
  if (result == nullptr || result->struct_size < sizeof(LUMICE_ANALYTIC_WeightKinksResult)) {
    return;
  }
  std::unique_ptr<KinksStorage> storage(static_cast<KinksStorage*>(result->storage));
  ZeroAfterStructSize(result);
}

void LUMICE_ANALYTIC_ReleaseClassificationResult(LUMICE_ANALYTIC_ClassificationResult* result) {
  if (result == nullptr || result->struct_size < sizeof(LUMICE_ANALYTIC_ClassificationResult)) {
    return;
  }
  std::unique_ptr<ClassificationStorage> storage(static_cast<ClassificationStorage*>(result->storage));
  ZeroAfterStructSize(result);
}

void LUMICE_ANALYTIC_ReleasePartitionResult(LUMICE_ANALYTIC_PartitionResult* result) {
  if (result == nullptr || result->struct_size < sizeof(LUMICE_ANALYTIC_PartitionResult)) {
    return;
  }
  std::unique_ptr<PartitionStorage> storage(static_cast<PartitionStorage*>(result->storage));
  ZeroAfterStructSize(result);
}

void LUMICE_ANALYTIC_ReleaseWavelengthTableResult(LUMICE_ANALYTIC_WavelengthTableResult* result) {
  if (result == nullptr || result->struct_size < sizeof(LUMICE_ANALYTIC_WavelengthTableResult)) {
    return;
  }
  std::unique_ptr<WavelengthStorage> storage(static_cast<WavelengthStorage*>(result->storage));
  ZeroAfterStructSize(result);
}

void LUMICE_ANALYTIC_ReleaseRestrictedCurveResult(LUMICE_ANALYTIC_RestrictedCurveResult* result) {
  if (result == nullptr || result->struct_size < sizeof(LUMICE_ANALYTIC_RestrictedCurveResult)) {
    return;
  }
  std::unique_ptr<CurveStorage> storage(static_cast<CurveStorage*>(result->storage));
  ZeroAfterStructSize(result);
}

void LUMICE_ANALYTIC_ReleaseChromaticResult(LUMICE_ANALYTIC_ChromaticResult* result) {
  if (result == nullptr || result->struct_size < sizeof(LUMICE_ANALYTIC_ChromaticResult)) {
    return;
  }
  std::unique_ptr<ChromaticStorage> storage(static_cast<ChromaticStorage*>(result->storage));
  ZeroAfterStructSize(result);
}
