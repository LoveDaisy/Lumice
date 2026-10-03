#include "analytic/feature_discovery.hpp"

#include <cmath>
#include <set>
#include <string>

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
    if (sample.direction_jacobian_available) {
      const size_t expected = 3u * static_cast<size_t>(batch.coordinate_dimension);
      if (sample.direction_jacobian.size() != expected || !FiniteVector(sample.direction_jacobian)) {
        return Fail("available direction_jacobian has the wrong extent or a non-finite value", error);
      }
    } else if (!sample.direction_jacobian.empty()) {
      return Fail("an unavailable direction_jacobian must not carry values", error);
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

FeatureDiscoveryResult DiscoverFeatures(const FeatureSupportBatch& batch, const FeatureDiscoveryOptions&,
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
  const FeatureEvidenceStatus empty_status = !batch.complete_visit || !batch.materialization_complete ?
                                                 FeatureEvidenceStatus::kNumericalIncomplete :
                                             batch.samples.empty() ? FeatureEvidenceStatus::kPhysicallyUnreachable :
                                                                     FeatureEvidenceStatus::kNotDetectedAtResolution;
  const std::string reason = !batch.complete_visit           ? "the adapter did not visit the complete input" :
                             !batch.materialization_complete ? "the support exceeded the materialization budget" :
                             batch.samples.empty()           ? "the supplied support is empty" :
                                                               "no candidate was classified by the data-model milestone";
  for (int mechanism = static_cast<int>(FeatureMechanism::kInteriorRankLoss);
       mechanism <= static_cast<int>(FeatureMechanism::kBrightnessRidge); mechanism++) {
    out.mechanisms.push_back({ static_cast<FeatureMechanism>(mechanism), empty_status, 0, reason });
  }
  return out;
}

}  // namespace lumice::analytic
