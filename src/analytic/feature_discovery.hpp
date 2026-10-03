#ifndef LUMICE_ANALYTIC_FEATURE_DISCOVERY_HPP_
#define LUMICE_ANALYTIC_FEATURE_DISCOVERY_HPP_

// General feature discovery over a caller-supplied, finite description of an actual support.
// Unlike discovery.hpp, this interface has no target direction and no ice-crystal names: the
// caller supplies concrete support samples, topology, weights and named constraints.  The kernel
// owns candidate classification and sky-space aggregation; the product adapter owns only scene
// enumeration and the optional controlled re-evaluation callback.

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace lumice::analytic {

constexpr int kMaxFeatureDiscoveryCoordinateDimension = 16;
constexpr uint32_t kFeatureSupportBatchVersion = 1;

enum class FeatureEvidenceStatus {
  kConfirmed,
  kCandidate,
  kNotDetectedAtResolution,
  kNumericalIncomplete,
  kPhysicallyUnreachable,
  kNotSupported,
};

enum class FeatureMechanism {
  kInteriorRankLoss,
  kSupportBoundary,
  kSupportCorner,
  kOpticalKink,
  kFilterBoundary,
  kWeightKink,
  kMeasureAtom,
  kStrictConfinement,
  kFiniteWidthConcentration,
  kBrightnessMaximum,
  kBrightnessRidge,
};

enum class SupportMeasureKind {
  kAtom,
  kContinuous,
};

enum class ConstraintKind {
  kDomain,
  kEntry,
  kTir,
  kFilter,
  kWeight,
};

const char* FeatureEvidenceStatusName(FeatureEvidenceStatus status);
const char* FeatureMechanismName(FeatureMechanism mechanism);
const char* SupportMeasureKindName(SupportMeasureKind kind);
const char* ConstraintKindName(ConstraintKind kind);

struct FeatureProvenance {
  int member_index = -1;
  int layer_index = -1;
  int interface_index = -1;
  int spectrum_node_id = -1;
  int source_node_id = -1;
  int sample_index = -1;
};

struct SupportConstraint {
  std::string name;
  ConstraintKind kind = ConstraintKind::kDomain;
  int layer_index = -1;
  int interface_index = -1;
  double value = 0.0;
  bool numerically_available = true;
  bool gradient_available = false;
  std::vector<double> gradient;
};

struct FeatureSupportSample {
  uint64_t sample_id = 0;
  FeatureProvenance provenance;
  SupportMeasureKind measure_kind = SupportMeasureKind::kContinuous;
  // Dimension of the measure support, not the embedding parameter vector.  A finite-width
  // distribution keeps positive support_dimension; only a true atom has dimension zero.
  int support_dimension = 0;
  bool finite_width = false;
  std::vector<double> coordinates;
  // Embedding-coordinate columns that span the actual continuous support tangent. Its extent is
  // support_dimension; discrete provenance coordinates are deliberately absent.
  std::vector<int> active_coordinates;
  double direction[3]{};  // world propagation direction; the displayed sky point is its negative
  double weight = 0.0;
  bool direction_jacobian_available = false;
  // Row-major [world component][coordinate].  Discovery projects this to an S2 tangent basis;
  // the embedding 3xN matrix is never ranked directly.
  std::vector<double> direction_jacobian;
  std::vector<uint8_t> direction_jacobian_column_available;
  double direction_jacobian_error = 0.0;
  double direction_jacobian_resolution = 0.0;
  std::vector<SupportConstraint> constraints;
  bool numerically_available = true;
};

struct FeatureSupportEdge {
  int first = -1;
  int second = -1;
  double parameter_distance = 0.0;
};

struct FeatureSupportBatch {
  uint32_t version = kFeatureSupportBatchVersion;
  int coordinate_dimension = 0;
  // The adapter increments visited_row_count before any bounded representative store.  A product
  // batch must set complete_visit; synthetic independent consumers may do the same explicitly.
  uint64_t visited_row_count = 0;
  bool complete_visit = false;
  // False means every row was observed but the adapter's explicit materialization budget omitted
  // some rows. Discovery then reports numerical_incomplete rather than treating the subset as a
  // global no-feature certificate.
  bool materialization_complete = true;
  std::vector<FeatureSupportSample> samples;
  std::vector<FeatureSupportEdge> edges;
};

// A re-evaluation request stays in the caller's parameterization and identifies the concrete
// provenance branch.  Returning false is a numerical outcome, not an exception or a proof that the
// requested state is physically impossible.
struct FeatureReevaluationRequest {
  FeatureProvenance provenance;
  std::vector<double> coordinates;
};

using FeatureReevaluateFn = std::function<bool(const FeatureReevaluationRequest&, FeatureSupportSample*, std::string*)>;

struct FeatureCandidate {
  FeatureMechanism mechanism = FeatureMechanism::kInteriorRankLoss;
  FeatureEvidenceStatus status = FeatureEvidenceStatus::kCandidate;
  FeatureProvenance provenance;
  double direction[3]{};
  int support_dimension = 0;
  int mapping_rank = -1;
  double singular_values[2]{};
  double weighted_mass = 0.0;
  bool has_weight_sides = false;
  double weight_sides[2]{};
  double residual = 0.0;
  double resolution = 0.0;
  std::vector<std::string> active_constraints;
  std::string reason;
};

struct FeatureMechanismRecord {
  FeatureMechanism mechanism = FeatureMechanism::kInteriorRankLoss;
  FeatureEvidenceStatus status = FeatureEvidenceStatus::kNotSupported;
  int candidate_count = 0;
  std::string reason;
};

struct FeatureDiscoveryOptions {
  double margin_tolerance = 1e-8;
  double rank_relative_tolerance = 1e-6;
  double sky_merge_tolerance = 1e-5;
  int maximum_refinement_steps = 24;
  int sky_z_bins = 8;
  int sky_azimuth_bins = 16;
};

struct SkyFieldNode {
  double direction[3]{};
  double value = 0.0;
  double normalized_value = 0.0;
  double gradient_norm = 0.0;
  double hessian_eigenvalues[2]{};
  double error = 0.0;
  double resolution = 0.0;
  int sample_count = 0;
  FeatureEvidenceStatus status = FeatureEvidenceStatus::kNotDetectedAtResolution;
};

struct FeatureDiscoveryResult {
  uint64_t visited_row_count = 0;
  int evaluated_sample_count = 0;
  bool complete_visit = false;
  bool materialization_complete = false;
  std::vector<FeatureCandidate> candidates;
  std::vector<FeatureMechanismRecord> mechanisms;
  std::vector<SkyFieldNode> sky_field;
};

// Returns false only for a malformed call contract.  A valid empty or numerically unavailable
// support is accepted and represented by per-mechanism result states in DiscoverFeatures.
bool ValidateFeatureSupportBatch(const FeatureSupportBatch& batch, std::string* error);

FeatureDiscoveryResult DiscoverFeatures(const FeatureSupportBatch& batch, const FeatureDiscoveryOptions& options,
                                        const FeatureReevaluateFn& reevaluate = {});

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_FEATURE_DISCOVERY_HPP_
