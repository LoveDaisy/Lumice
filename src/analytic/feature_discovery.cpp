#include "analytic/feature_discovery.hpp"

#include <cmath>
#include <limits>
#include <set>
#include <string>
#include <utility>

namespace lumice::analytic {

namespace {

bool IsFiniteDirection(const double direction[3]) {
  const double norm2 = direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2];
  return std::isfinite(norm2) && std::fabs(norm2 - 1.0) <= 1e-10;
}

bool FiniteVector(const std::vector<double>& values) {
  for (double value : values) {
    if (!std::isfinite(value)) {
      return false;
    }
  }
  return true;
}

bool Fail(std::string message, std::string* error) {
  if (error != nullptr) {
    *error = std::move(message);
  }
  return false;
}

struct S2Differential {
  int rank = -1;
  double singular_values[2]{};
  double tangent_residual = std::numeric_limits<double>::infinity();
};

void Cross(const double first[3], const double second[3], double out[3]) {
  out[0] = first[1] * second[2] - first[2] * second[1];
  out[1] = first[2] * second[0] - first[0] * second[2];
  out[2] = first[0] * second[1] - first[1] * second[0];
}

double Dot(const double first[3], const double second[3]) {
  return first[0] * second[0] + first[1] * second[1] + first[2] * second[2];
}

S2Differential RestrictedS2Differential(const FeatureSupportSample& sample, int coordinate_dimension,
                                        double relative_tolerance) {
  S2Differential out;
  if (!sample.direction_jacobian_available || sample.support_dimension <= 0) {
    return out;
  }
  int anchor_index = 0;
  for (int component = 1; component < 3; ++component) {
    if (std::fabs(sample.direction[component]) < std::fabs(sample.direction[anchor_index])) {
      anchor_index = component;
    }
  }
  double anchor[3]{};
  anchor[anchor_index] = 1.0;
  double tangent0[3]{};
  Cross(sample.direction, anchor, tangent0);
  const double tangent0_norm = std::sqrt(Dot(tangent0, tangent0));
  for (double& component : tangent0) {
    component /= tangent0_norm;
  }
  double tangent1[3]{};
  Cross(sample.direction, tangent0, tangent1);

  double gram00 = 0.0;
  double gram01 = 0.0;
  double gram11 = 0.0;
  out.tangent_residual = 0.0;
  for (int coordinate : sample.active_coordinates) {
    const double column[3] = {
      sample.direction_jacobian[static_cast<size_t>(0 * coordinate_dimension + coordinate)],
      sample.direction_jacobian[static_cast<size_t>(1 * coordinate_dimension + coordinate)],
      sample.direction_jacobian[static_cast<size_t>(2 * coordinate_dimension + coordinate)],
    };
    const double first = Dot(tangent0, column);
    const double second = Dot(tangent1, column);
    gram00 += first * first;
    gram01 += first * second;
    gram11 += second * second;
    out.tangent_residual = std::max(out.tangent_residual, std::fabs(Dot(sample.direction, column)));
  }
  const double trace = gram00 + gram11;
  const double discriminant = std::sqrt(std::max(0.0, (gram00 - gram11) * (gram00 - gram11) + 4.0 * gram01 * gram01));
  const double eigen0 = std::max(0.0, 0.5 * (trace + discriminant));
  const double eigen1 = std::max(0.0, 0.5 * (trace - discriminant));
  out.singular_values[0] = std::sqrt(eigen0);
  out.singular_values[1] = std::sqrt(eigen1);
  const double threshold = std::max(1e-12, relative_tolerance * out.singular_values[0]);
  out.rank =
      static_cast<int>(out.singular_values[0] > threshold) + static_cast<int>(out.singular_values[1] > threshold);
  return out;
}

bool SameProvenanceBranch(const FeatureProvenance& first, const FeatureProvenance& second) {
  return first.member_index == second.member_index && first.layer_index == second.layer_index &&
         first.interface_index == second.interface_index && first.spectrum_node_id == second.spectrum_node_id &&
         first.source_node_id == second.source_node_id;
}

double DirectionDistance(const double first[3], const double second[3]) {
  const double dot = std::max(-1.0, std::min(1.0, Dot(first, second)));
  return std::acos(dot);
}

void AddCandidate(const FeatureCandidate& candidate, double merge_tolerance, FeatureDiscoveryResult* out) {
  for (FeatureCandidate& existing : out->candidates) {
    if (existing.mechanism == candidate.mechanism && SameProvenanceBranch(existing.provenance, candidate.provenance) &&
        DirectionDistance(existing.direction, candidate.direction) <= merge_tolerance) {
      existing.weighted_mass += candidate.weighted_mass;
      existing.residual = std::max(existing.residual, candidate.residual);
      existing.resolution = std::max(existing.resolution, candidate.resolution);
      if (candidate.status == FeatureEvidenceStatus::kConfirmed) {
        existing.status = candidate.status;
      }
      return;
    }
  }
  out->candidates.push_back(candidate);
}

FeatureCandidate CandidateFromSample(const FeatureSupportSample& sample, FeatureMechanism mechanism,
                                     FeatureEvidenceStatus status, std::string reason) {
  FeatureCandidate candidate;
  candidate.mechanism = mechanism;
  candidate.status = status;
  candidate.provenance = sample.provenance;
  std::copy(sample.direction, sample.direction + 3, candidate.direction);
  candidate.support_dimension = sample.support_dimension;
  candidate.weighted_mass = sample.weight;
  candidate.resolution = sample.direction_jacobian_resolution;
  candidate.reason = std::move(reason);
  return candidate;
}

int MechanismIndex(FeatureMechanism mechanism) {
  return static_cast<int>(mechanism) - static_cast<int>(FeatureMechanism::kInteriorRankLoss);
}

}  // namespace

const char* FeatureEvidenceStatusName(FeatureEvidenceStatus status) {
  switch (status) {
    case FeatureEvidenceStatus::kConfirmed:
      return "confirmed";
    case FeatureEvidenceStatus::kCandidate:
      return "candidate";
    case FeatureEvidenceStatus::kNotDetectedAtResolution:
      return "not_detected_at_resolution";
    case FeatureEvidenceStatus::kNumericalIncomplete:
      return "numerical_incomplete";
    case FeatureEvidenceStatus::kPhysicallyUnreachable:
      return "physically_unreachable";
    case FeatureEvidenceStatus::kNotSupported:
      return "not_supported";
  }
  return "not_supported";
}

const char* FeatureMechanismName(FeatureMechanism mechanism) {
  switch (mechanism) {
    case FeatureMechanism::kInteriorRankLoss:
      return "interior_rank_loss";
    case FeatureMechanism::kSupportBoundary:
      return "support_boundary";
    case FeatureMechanism::kSupportCorner:
      return "support_corner";
    case FeatureMechanism::kOpticalKink:
      return "optical_kink";
    case FeatureMechanism::kFilterBoundary:
      return "filter_boundary";
    case FeatureMechanism::kWeightKink:
      return "weight_kink";
    case FeatureMechanism::kMeasureAtom:
      return "measure_atom";
    case FeatureMechanism::kStrictConfinement:
      return "strict_confinement";
    case FeatureMechanism::kFiniteWidthConcentration:
      return "finite_width_concentration";
    case FeatureMechanism::kBrightnessMaximum:
      return "brightness_maximum";
    case FeatureMechanism::kBrightnessRidge:
      return "brightness_ridge";
  }
  return "interior_rank_loss";
}

const char* SupportMeasureKindName(SupportMeasureKind kind) {
  switch (kind) {
    case SupportMeasureKind::kAtom:
      return "atom";
    case SupportMeasureKind::kContinuous:
      return "continuous";
  }
  return "continuous";
}

const char* ConstraintKindName(ConstraintKind kind) {
  switch (kind) {
    case ConstraintKind::kDomain:
      return "domain";
    case ConstraintKind::kEntry:
      return "entry";
    case ConstraintKind::kTir:
      return "tir";
    case ConstraintKind::kFilter:
      return "filter";
    case ConstraintKind::kWeight:
      return "weight";
  }
  return "domain";
}

bool ValidateFeatureSupportBatch(const FeatureSupportBatch& batch, std::string* error) {
  if (error != nullptr) {
    error->clear();
  }
  if (batch.version != kFeatureSupportBatchVersion) {
    return Fail("unsupported feature support batch version", error);
  }
  if (batch.coordinate_dimension < 0 || batch.coordinate_dimension > kMaxFeatureDiscoveryCoordinateDimension) {
    return Fail("coordinate_dimension is outside the supported range", error);
  }
  if (batch.visited_row_count < batch.samples.size()) {
    return Fail("visited_row_count is smaller than the materialized sample count", error);
  }

  std::set<uint64_t> sample_ids;
  for (const FeatureSupportSample& sample : batch.samples) {
    if (!sample_ids.insert(sample.sample_id).second) {
      return Fail("sample_id values must be unique", error);
    }
    if (sample.support_dimension < 0 || sample.support_dimension > batch.coordinate_dimension) {
      return Fail("sample support_dimension is outside the batch parameterization", error);
    }
    if (sample.measure_kind == SupportMeasureKind::kAtom && sample.support_dimension != 0) {
      return Fail("an atomic sample must have zero-dimensional support", error);
    }
    if (sample.finite_width && sample.support_dimension == 0) {
      return Fail("finite-width support must retain a positive dimension", error);
    }
    if (sample.active_coordinates.size() != static_cast<size_t>(sample.support_dimension)) {
      return Fail("active_coordinates must span the declared support dimension", error);
    }
    std::set<int> active_coordinates;
    for (int coordinate : sample.active_coordinates) {
      if (coordinate < 0 || coordinate >= batch.coordinate_dimension || !active_coordinates.insert(coordinate).second) {
        return Fail("active_coordinates must be unique indices in the batch parameterization", error);
      }
    }
    if (sample.coordinates.size() != static_cast<size_t>(batch.coordinate_dimension) ||
        !FiniteVector(sample.coordinates)) {
      return Fail("sample coordinates do not match coordinate_dimension or are non-finite", error);
    }
    if (!std::isfinite(sample.weight) || sample.weight < 0.0) {
      return Fail("sample weight must be finite and non-negative", error);
    }
    if (sample.numerically_available && !IsFiniteDirection(sample.direction)) {
      return Fail("an available sample direction must be finite and unit length", error);
    }
    if (!sample.direction_jacobian.empty()) {
      const size_t expected = 3u * static_cast<size_t>(batch.coordinate_dimension);
      if (sample.direction_jacobian.size() != expected || !FiniteVector(sample.direction_jacobian)) {
        return Fail("direction_jacobian has the wrong extent or a non-finite value", error);
      }
      if (sample.direction_jacobian_column_available.size() != static_cast<size_t>(batch.coordinate_dimension)) {
        return Fail("direction_jacobian column availability has the wrong extent", error);
      }
    } else if (!sample.direction_jacobian_column_available.empty()) {
      return Fail("direction_jacobian column availability requires a Jacobian", error);
    }
    if (sample.direction_jacobian_available) {
      if (sample.direction_jacobian.empty()) {
        return Fail("an available direction_jacobian must carry values", error);
      }
      for (int coordinate : sample.active_coordinates) {
        if (!sample.direction_jacobian_column_available[static_cast<size_t>(coordinate)]) {
          return Fail("an available direction_jacobian must cover every active coordinate", error);
        }
      }
      if (!std::isfinite(sample.direction_jacobian_error) || sample.direction_jacobian_error < 0.0 ||
          !std::isfinite(sample.direction_jacobian_resolution) || !(sample.direction_jacobian_resolution > 0.0)) {
        return Fail("available direction_jacobian requires finite error and positive resolution", error);
      }
    }
    for (const SupportConstraint& constraint : sample.constraints) {
      if (constraint.name.empty() || (constraint.numerically_available && !std::isfinite(constraint.value))) {
        return Fail("constraints require a name and an available finite value", error);
      }
      if (constraint.gradient_available) {
        if (constraint.gradient.size() != static_cast<size_t>(batch.coordinate_dimension) ||
            !FiniteVector(constraint.gradient)) {
          return Fail("available constraint gradient has the wrong extent or a non-finite value", error);
        }
      } else if (!constraint.gradient.empty()) {
        return Fail("an unavailable constraint gradient must not carry values", error);
      }
    }
  }

  for (const FeatureSupportEdge& edge : batch.edges) {
    if (edge.first < 0 || edge.second < 0 || edge.first == edge.second ||
        edge.first >= static_cast<int>(batch.samples.size()) || edge.second >= static_cast<int>(batch.samples.size())) {
      return Fail("support edge endpoints must name two distinct samples", error);
    }
    if (!std::isfinite(edge.parameter_distance) || !(edge.parameter_distance > 0.0)) {
      return Fail("support edge parameter_distance must be finite and positive", error);
    }
  }
  return true;
}

FeatureDiscoveryResult DiscoverFeatures(const FeatureSupportBatch& batch, const FeatureDiscoveryOptions& options,
                                        const FeatureReevaluateFn&) {
  FeatureDiscoveryResult out;
  out.visited_row_count = batch.visited_row_count;
  out.complete_visit = batch.complete_visit;
  out.materialization_complete = batch.materialization_complete;
  std::string error;
  if (!ValidateFeatureSupportBatch(batch, &error)) {
    for (int mechanism = static_cast<int>(FeatureMechanism::kInteriorRankLoss);
         mechanism <= static_cast<int>(FeatureMechanism::kBrightnessRidge); mechanism++) {
      out.mechanisms.push_back(
          { static_cast<FeatureMechanism>(mechanism), FeatureEvidenceStatus::kNotSupported, 0, error });
    }
    return out;
  }
  out.evaluated_sample_count = static_cast<int>(batch.samples.size());
  for (int mechanism = static_cast<int>(FeatureMechanism::kInteriorRankLoss);
       mechanism <= static_cast<int>(FeatureMechanism::kBrightnessRidge); mechanism++) {
    out.mechanisms.push_back({ static_cast<FeatureMechanism>(mechanism),
                               batch.samples.empty() ? FeatureEvidenceStatus::kPhysicallyUnreachable :
                                                       FeatureEvidenceStatus::kNotDetectedAtResolution,
                               0,
                               batch.samples.empty() ? "the supplied support is empty" :
                                                       "no candidate was detected at the supplied resolution" });
  }
  if (batch.samples.empty()) {
    return out;
  }

  const bool globally_complete = batch.complete_visit && batch.materialization_complete;
  bool rank_numerically_incomplete = false;
  for (const FeatureSupportSample& sample : batch.samples) {
    if (!sample.numerically_available) {
      rank_numerically_incomplete = true;
      continue;
    }
    const FeatureEvidenceStatus direct_status =
        globally_complete ? FeatureEvidenceStatus::kConfirmed : FeatureEvidenceStatus::kCandidate;
    if (sample.measure_kind == SupportMeasureKind::kAtom && sample.weight > 0.0) {
      FeatureCandidate candidate = CandidateFromSample(sample, FeatureMechanism::kMeasureAtom, direct_status,
                                                       "positive input atom contributes mass at this direction");
      candidate.mapping_rank = 0;
      AddCandidate(candidate, options.sky_merge_tolerance, &out);
    }
    if (sample.measure_kind == SupportMeasureKind::kContinuous && sample.support_dimension < 2) {
      AddCandidate(CandidateFromSample(sample, FeatureMechanism::kStrictConfinement, direct_status,
                                       "the actual continuous support has dimension below the sky tangent dimension"),
                   options.sky_merge_tolerance, &out);
    }
    if (sample.finite_width) {
      AddCandidate(
          CandidateFromSample(sample, FeatureMechanism::kFiniteWidthConcentration, FeatureEvidenceStatus::kCandidate,
                              "positive-width support is a resolution-dependent concentration, not an atom"),
          options.sky_merge_tolerance, &out);
    }

    if (sample.support_dimension <= 0) {
      continue;
    }
    if (!sample.direction_jacobian_available) {
      rank_numerically_incomplete = true;
      continue;
    }
    const S2Differential differential =
        RestrictedS2Differential(sample, batch.coordinate_dimension, options.rank_relative_tolerance);
    const int regular_rank = std::min(2, sample.support_dimension);
    if (differential.rank < regular_rank) {
      const double numerical_residual = std::max(differential.tangent_residual, sample.direction_jacobian_error);
      const FeatureEvidenceStatus status =
          globally_complete && numerical_residual <= std::max(1e-8, 10.0 * options.rank_relative_tolerance) ?
              FeatureEvidenceStatus::kConfirmed :
              FeatureEvidenceStatus::kCandidate;
      FeatureCandidate candidate =
          CandidateFromSample(sample, FeatureMechanism::kInteriorRankLoss, status,
                              "the complete-chain differential restricted to the actual support loses S2 tangent rank");
      candidate.mapping_rank = differential.rank;
      candidate.singular_values[0] = differential.singular_values[0];
      candidate.singular_values[1] = differential.singular_values[1];
      candidate.residual = numerical_residual;
      AddCandidate(candidate, options.sky_merge_tolerance, &out);
    }
  }

  for (const FeatureCandidate& candidate : out.candidates) {
    FeatureMechanismRecord& record = out.mechanisms[static_cast<size_t>(MechanismIndex(candidate.mechanism))];
    ++record.candidate_count;
    if (candidate.status == FeatureEvidenceStatus::kConfirmed) {
      record.status = FeatureEvidenceStatus::kConfirmed;
    } else if (record.status != FeatureEvidenceStatus::kConfirmed) {
      record.status = FeatureEvidenceStatus::kCandidate;
    }
    record.reason = "one or more local candidates retain mechanism-specific numerical evidence";
  }
  FeatureMechanismRecord& rank_record =
      out.mechanisms[static_cast<size_t>(MechanismIndex(FeatureMechanism::kInteriorRankLoss))];
  if (rank_numerically_incomplete && rank_record.candidate_count == 0) {
    rank_record.status = FeatureEvidenceStatus::kNumericalIncomplete;
    rank_record.reason = "at least one support row lacks a complete-chain differential";
  }
  if (!globally_complete) {
    for (FeatureMechanismRecord& record : out.mechanisms) {
      if (record.status == FeatureEvidenceStatus::kNotDetectedAtResolution) {
        record.status = FeatureEvidenceStatus::kNumericalIncomplete;
        record.reason = !batch.complete_visit ? "the adapter did not visit the complete input" :
                                                "the support exceeded the materialization budget";
      }
    }
  }
  for (FeatureMechanism mechanism :
       { FeatureMechanism::kSupportBoundary, FeatureMechanism::kSupportCorner, FeatureMechanism::kOpticalKink,
         FeatureMechanism::kFilterBoundary, FeatureMechanism::kWeightKink, FeatureMechanism::kBrightnessMaximum,
         FeatureMechanism::kBrightnessRidge }) {
    FeatureMechanismRecord& record = out.mechanisms[static_cast<size_t>(MechanismIndex(mechanism))];
    record.status = FeatureEvidenceStatus::kNotSupported;
    record.reason = "this mechanism is not evaluated by the differential milestone";
  }
  return out;
}

}  // namespace lumice::analytic
