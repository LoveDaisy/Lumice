#include "analytic/feature_discovery.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <string>
#include <tuple>
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

bool NearlyEqual(double first, double second) {
  constexpr double kRelativeTolerance = 1e-12;
  const double scale = std::max({ 1.0, std::fabs(first), std::fabs(second) });
  return std::fabs(first - second) <= kRelativeTolerance * scale;
}

bool SameActiveCoordinates(const FeatureSupportSample& first, const FeatureSupportSample& second) {
  return first.active_coordinates == second.active_coordinates;
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
                                        const std::vector<int>& active_coordinates, double relative_tolerance) {
  S2Differential out;
  if (active_coordinates.empty() ||
      sample.direction_jacobian.size() != 3u * static_cast<size_t>(coordinate_dimension) ||
      sample.direction_jacobian_column_available.size() != static_cast<size_t>(coordinate_dimension) ||
      std::any_of(active_coordinates.begin(), active_coordinates.end(), [&](int coordinate) {
        return coordinate < 0 || coordinate >= coordinate_dimension ||
               sample.direction_jacobian_column_available[static_cast<size_t>(coordinate)] == 0;
      })) {
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
  for (int coordinate : active_coordinates) {
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

S2Differential RestrictedS2Differential(const FeatureSupportSample& sample, int coordinate_dimension,
                                        double relative_tolerance) {
  if (!sample.direction_jacobian_available) {
    return {};
  }
  return RestrictedS2Differential(sample, coordinate_dimension, sample.active_coordinates, relative_tolerance);
}

bool SameProvenanceBranch(const FeatureProvenance& first, const FeatureProvenance& second) {
  return first.member_index == second.member_index && first.layer_index == second.layer_index &&
         first.interface_index == second.interface_index && first.spectrum_node_id == second.spectrum_node_id &&
         first.source_node_id == second.source_node_id;
}

struct TangentColumn {
  bool available = false;
  double value[3]{};
  double norm = 0.0;
  double tangent_residual = 0.0;
};

TangentColumn RestrictedS2Column(const FeatureSupportSample& sample, int coordinate_dimension, int coordinate_index) {
  TangentColumn out;
  if (coordinate_index < 0 || coordinate_index >= coordinate_dimension || sample.direction_jacobian.empty() ||
      sample.direction_jacobian_column_available.size() != static_cast<size_t>(coordinate_dimension) ||
      sample.direction_jacobian_column_available[static_cast<size_t>(coordinate_index)] == 0) {
    return out;
  }
  double column[3]{};
  for (int component = 0; component < 3; ++component) {
    column[component] =
        sample.direction_jacobian[static_cast<size_t>(component * coordinate_dimension + coordinate_index)];
  }
  const double radial = Dot(sample.direction, column);
  for (int component = 0; component < 3; ++component) {
    out.value[component] = column[component] - radial * sample.direction[component];
  }
  out.norm = std::sqrt(Dot(out.value, out.value));
  out.tangent_residual = std::fabs(radial);
  out.available = std::isfinite(out.norm);
  return out;
}

double TransportedDot(const double reference[3], const FeatureSupportSample& sample, const TangentColumn& column) {
  double transported[3]{};
  const double radial = Dot(reference, sample.direction);
  for (int component = 0; component < 3; ++component) {
    transported[component] = reference[component] - radial * sample.direction[component];
  }
  const double norm = std::sqrt(Dot(transported, transported));
  if (!(norm > 0.0) || !std::isfinite(norm)) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  for (double& component : transported) {
    component /= norm;
  }
  return Dot(transported, column.value);
}

bool SameProvenance(const FeatureProvenance& first, const FeatureProvenance& second) {
  return SameProvenanceBranch(first, second) && first.sample_index == second.sample_index;
}

bool ValidCallbackConstraint(const SupportConstraint& constraint, int coordinate_dimension) {
  if (constraint.name.empty() || constraint.kind < ConstraintKind::kDomain ||
      constraint.kind > ConstraintKind::kWeight ||
      (constraint.numerically_available && !std::isfinite(constraint.value))) {
    return false;
  }
  if (constraint.gradient_available) {
    return constraint.gradient.size() == static_cast<size_t>(coordinate_dimension) && FiniteVector(constraint.gradient);
  }
  return constraint.gradient.empty();
}

bool CallbackSampleMatchesRequest(const FeatureSupportSample& sample, const FeatureReevaluationRequest& request,
                                  const FeatureSupportSample& expected, int coordinate_dimension) {
  if (!sample.numerically_available || !IsFiniteDirection(sample.direction) || sample.accumulates_measure ||
      !SameProvenance(sample.provenance, request.provenance) || sample.measure_kind != expected.measure_kind ||
      sample.support_dimension != expected.support_dimension || sample.finite_width != expected.finite_width ||
      sample.active_coordinates != expected.active_coordinates ||
      sample.coordinates.size() != request.coordinates.size() ||
      sample.coordinates.size() != static_cast<size_t>(coordinate_dimension) || !FiniteVector(sample.coordinates) ||
      !std::isfinite(sample.weight) || sample.weight < 0.0 ||
      sample.mapping_evidence_kind != MappingEvidenceKind::kNone || sample.image_dimension_upper_bound != -1 ||
      sample.mapping_error_bound != 0.0) {
    return false;
  }
  for (std::size_t index = 0; index < sample.coordinates.size(); ++index) {
    if (!NearlyEqual(sample.coordinates[index], request.coordinates[index])) {
      return false;
    }
  }
  if (!sample.direction_jacobian.empty()) {
    if (sample.direction_jacobian.size() != 3u * static_cast<size_t>(coordinate_dimension) ||
        sample.direction_jacobian_column_available.size() != static_cast<size_t>(coordinate_dimension) ||
        !FiniteVector(sample.direction_jacobian)) {
      return false;
    }
  } else if (!sample.direction_jacobian_column_available.empty()) {
    return false;
  }
  if (sample.direction_jacobian_available) {
    if (sample.direction_jacobian.empty() || !std::isfinite(sample.direction_jacobian_error) ||
        sample.direction_jacobian_error < 0.0 || !std::isfinite(sample.direction_jacobian_resolution) ||
        !(sample.direction_jacobian_resolution > 0.0)) {
      return false;
    }
    for (int coordinate : sample.active_coordinates) {
      if (coordinate < 0 || coordinate >= coordinate_dimension ||
          sample.direction_jacobian_column_available[static_cast<size_t>(coordinate)] == 0) {
        return false;
      }
    }
  }
  if (!std::all_of(sample.constraints.begin(), sample.constraints.end(), [&](const SupportConstraint& constraint) {
        return ValidCallbackConstraint(constraint, coordinate_dimension);
      })) {
    return false;
  }
  return true;
}

bool EvaluateRankSearchSample(const FeatureReevaluateFn& reevaluate, const FeatureSupportSample& expected,
                              const std::vector<double>& coordinates, int maximum_calls, int* calls,
                              FeatureSupportSample* sample) {
  if (!reevaluate || *calls >= maximum_calls) {
    return false;
  }
  FeatureReevaluationRequest request;
  request.provenance = expected.provenance;
  request.coordinates = coordinates;
  std::string callback_error;
  ++*calls;
  return reevaluate(request, sample, &callback_error) &&
         CallbackSampleMatchesRequest(*sample, request, expected, static_cast<int>(coordinates.size()));
}

bool EvaluateRankSearchColumn(const FeatureReevaluateFn& reevaluate, const FeatureSupportSample& expected,
                              const std::vector<double>& coordinates, int coordinate_dimension, int coordinate_index,
                              double bracket_width, int maximum_calls, int* calls, FeatureSupportSample* sample,
                              TangentColumn* column) {
  if (!EvaluateRankSearchSample(reevaluate, expected, coordinates, maximum_calls, calls, sample)) {
    return false;
  }
  *column = RestrictedS2Column(*sample, coordinate_dimension, coordinate_index);
  if (column->available) {
    return true;
  }
  const double scale = std::max(1.0, std::fabs(coordinates[static_cast<size_t>(coordinate_index)]));
  const double step = std::min(0.25 * bracket_width, std::max(1e-7 * scale, 1e-6 * bracket_width));
  if (!(step > 0.0) || *calls + 2 > maximum_calls) {
    return false;
  }
  std::vector<double> lower_coordinates = coordinates;
  std::vector<double> upper_coordinates = coordinates;
  lower_coordinates[static_cast<size_t>(coordinate_index)] -= step;
  upper_coordinates[static_cast<size_t>(coordinate_index)] += step;
  FeatureSupportSample lower;
  FeatureSupportSample upper;
  if (!EvaluateRankSearchSample(reevaluate, expected, lower_coordinates, maximum_calls, calls, &lower) ||
      !EvaluateRankSearchSample(reevaluate, expected, upper_coordinates, maximum_calls, calls, &upper)) {
    return false;
  }
  double raw[3]{};
  for (int component = 0; component < 3; ++component) {
    raw[component] = (upper.direction[component] - lower.direction[component]) / (2.0 * step);
  }
  const double radial = Dot(sample->direction, raw);
  for (int component = 0; component < 3; ++component) {
    column->value[component] = raw[component] - radial * sample->direction[component];
  }
  column->norm = std::sqrt(Dot(column->value, column->value));
  column->tangent_residual = std::fabs(radial);
  column->available = std::isfinite(column->norm);
  return column->available;
}

bool RefineOneDimensionalRankLoss(const FeatureSupportBatch& batch, const FeatureSupportCellAxis& axis,
                                  const FeatureDiscoveryOptions& options, const FeatureReevaluateFn& reevaluate,
                                  bool globally_complete, FeatureCandidate* candidate, bool* numerical_incomplete) {
  const FeatureSupportSample* nodes[3] = {
    &batch.samples[static_cast<size_t>(axis.lower)],
    &batch.samples[static_cast<size_t>(axis.center)],
    &batch.samples[static_cast<size_t>(axis.upper)],
  };
  TangentColumn columns[3] = {
    RestrictedS2Column(*nodes[0], batch.coordinate_dimension, axis.coordinate_index),
    RestrictedS2Column(*nodes[1], batch.coordinate_dimension, axis.coordinate_index),
    RestrictedS2Column(*nodes[2], batch.coordinate_dimension, axis.coordinate_index),
  };
  for (const TangentColumn& column : columns) {
    if (!column.available) {
      return false;
    }
  }

  int bracket = -1;
  double reference[3]{};
  double bracket_scale = 0.0;
  for (int pair = 0; pair < 2; ++pair) {
    if (!(columns[pair].norm > 0.0) || !(columns[pair + 1].norm > 0.0)) {
      continue;
    }
    std::copy(columns[pair].value, columns[pair].value + 3, reference);
    const double opposite = TransportedDot(reference, *nodes[pair + 1], columns[pair + 1]);
    const double scale = std::max(columns[pair].norm, columns[pair + 1].norm);
    if (std::isfinite(opposite) && opposite < -options.rank_relative_tolerance * scale) {
      bracket = pair;
      bracket_scale = scale;
      break;
    }
  }
  if (bracket < 0) {
    return false;
  }
  if (!reevaluate) {
    *numerical_incomplete = true;
    return false;
  }

  const FeatureSupportSample* lower_node = nodes[bracket];
  const FeatureSupportSample* upper_node = nodes[bracket + 1];
  std::vector<double> lower_coordinates = lower_node->coordinates;
  std::vector<double> upper_coordinates = upper_node->coordinates;
  const TangentColumn lower_column = columns[bracket];
  FeatureSupportSample best = lower_column.norm <= columns[bracket + 1].norm ? *lower_node : *upper_node;
  TangentColumn best_column = lower_column.norm <= columns[bracket + 1].norm ? lower_column : columns[bracket + 1];
  double lower_sign = TransportedDot(reference, *lower_node, lower_column);
  double upper_sign = TransportedDot(reference, *upper_node, columns[bracket + 1]);
  double refinement_resolution = std::fabs(upper_coordinates[static_cast<size_t>(axis.coordinate_index)] -
                                           lower_coordinates[static_cast<size_t>(axis.coordinate_index)]);
  int callback_calls = 0;
  for (int step = 0; step < options.maximum_refinement_steps && callback_calls < options.maximum_refinement_steps;
       ++step) {
    const double interpolation = std::clamp(lower_sign / (lower_sign - upper_sign), 0.1, 0.9);
    std::vector<double> midpoint(lower_coordinates.size());
    for (size_t coordinate = 0; coordinate < midpoint.size(); ++coordinate) {
      midpoint[coordinate] = lower_coordinates[coordinate] +
                             interpolation * (upper_coordinates[coordinate] - lower_coordinates[coordinate]);
    }
    const double width = upper_coordinates[static_cast<size_t>(axis.coordinate_index)] -
                         lower_coordinates[static_cast<size_t>(axis.coordinate_index)];
    refinement_resolution = std::fabs(width);
    FeatureSupportSample evaluated;
    TangentColumn evaluated_column;
    if (!EvaluateRankSearchColumn(reevaluate, *lower_node, midpoint, batch.coordinate_dimension, axis.coordinate_index,
                                  width, options.maximum_refinement_steps, &callback_calls, &evaluated,
                                  &evaluated_column)) {
      *numerical_incomplete = true;
      return false;
    }
    if (evaluated_column.norm < best_column.norm) {
      best = evaluated;
      best_column = evaluated_column;
    }
    if (evaluated_column.norm <= std::max(1e-12, options.rank_relative_tolerance * bracket_scale)) {
      lower_coordinates = midpoint;
      upper_coordinates = midpoint;
      break;
    }
    const double sign = TransportedDot(reference, evaluated, evaluated_column);
    if (!std::isfinite(sign)) {
      return false;
    }
    if (std::signbit(sign) == std::signbit(lower_sign)) {
      lower_coordinates = midpoint;
      lower_sign = sign;
    } else {
      upper_coordinates = midpoint;
      upper_sign = sign;
    }
  }

  candidate->mechanism = FeatureMechanism::kInteriorRankLoss;
  candidate->status = FeatureEvidenceStatus::kCandidate;
  candidate->provenance = best.provenance;
  std::copy(best.direction, best.direction + 3, candidate->direction);
  candidate->support_dimension = best.support_dimension;
  candidate->weighted_mass = best.weight;
  candidate->reason =
      "a signed S2 tangent derivative bracket was refined through the caller's continuous-support callback";
  candidate->mapping_rank = 0;
  candidate->singular_values[0] = best_column.norm;
  candidate->singular_values[1] = 0.0;
  candidate->residual = std::max(best_column.norm, best_column.tangent_residual);
  candidate->resolution = refinement_resolution;
  const double residual_tolerance = std::max(1e-10, 10.0 * options.rank_relative_tolerance * bracket_scale);
  if (globally_complete && candidate->residual <= residual_tolerance) {
    candidate->status = FeatureEvidenceStatus::kConfirmed;
  }
  return true;
}

bool OrientedS2Minor(const FeatureSupportSample& sample, int coordinate_dimension, int first_coordinate,
                     int second_coordinate, double* value) {
  const TangentColumn first = RestrictedS2Column(sample, coordinate_dimension, first_coordinate);
  const TangentColumn second = RestrictedS2Column(sample, coordinate_dimension, second_coordinate);
  if (!first.available || !second.available) {
    return false;
  }
  double cross[3]{};
  Cross(first.value, second.value, cross);
  *value = Dot(sample.direction, cross);
  return std::isfinite(*value);
}

double CoordinateDistance(const std::vector<double>& first, const std::vector<double>& second,
                          const std::vector<int>& active_coordinates) {
  double distance2 = 0.0;
  for (int coordinate : active_coordinates) {
    const double delta = second[static_cast<size_t>(coordinate)] - first[static_cast<size_t>(coordinate)];
    distance2 += delta * delta;
  }
  return std::sqrt(distance2);
}

bool RefineMultidimensionalRankLossLine(const FeatureSupportSample& first, const FeatureSupportSample& second,
                                        int coordinate_dimension, int regular_rank,
                                        const FeatureDiscoveryOptions& options, const FeatureReevaluateFn& reevaluate,
                                        bool globally_complete, int* callback_calls, FeatureCandidate* candidate,
                                        bool* numerical_incomplete) {
  if (regular_rank != 2 || first.active_coordinates.size() < 2u || !reevaluate) {
    return false;
  }
  int determinant_coordinates[2]{ -1, -1 };
  double first_determinant = 0.0;
  double second_determinant = 0.0;
  double determinant_scale = 0.0;
  for (size_t first_index = 0; first_index < first.active_coordinates.size(); ++first_index) {
    for (size_t second_index = first_index + 1; second_index < first.active_coordinates.size(); ++second_index) {
      double first_value = 0.0;
      double second_value = 0.0;
      const int first_coordinate = first.active_coordinates[first_index];
      const int second_coordinate = first.active_coordinates[second_index];
      if (!OrientedS2Minor(first, coordinate_dimension, first_coordinate, second_coordinate, &first_value) ||
          !OrientedS2Minor(second, coordinate_dimension, first_coordinate, second_coordinate, &second_value)) {
        continue;
      }
      const double scale = std::max(std::fabs(first_value), std::fabs(second_value));
      if (scale > determinant_scale) {
        determinant_scale = scale;
        determinant_coordinates[0] = first_coordinate;
        determinant_coordinates[1] = second_coordinate;
        first_determinant = first_value;
        second_determinant = second_value;
      }
    }
  }
  if (determinant_coordinates[0] < 0 || determinant_scale <= 1e-14 ||
      std::signbit(first_determinant) == std::signbit(second_determinant)) {
    return false;
  }

  std::vector<double> lower_coordinates = first.coordinates;
  std::vector<double> upper_coordinates = second.coordinates;
  FeatureSupportSample best;
  S2Differential best_differential;
  bool have_best = false;
  double refinement_resolution = CoordinateDistance(lower_coordinates, upper_coordinates, first.active_coordinates);
  for (int step = 0; step < options.maximum_refinement_steps && *callback_calls < options.maximum_refinement_steps;
       ++step) {
    const double interpolation = std::clamp(first_determinant / (first_determinant - second_determinant), 0.1, 0.9);
    std::vector<double> coordinates(lower_coordinates.size());
    refinement_resolution = CoordinateDistance(lower_coordinates, upper_coordinates, first.active_coordinates);
    for (size_t coordinate = 0; coordinate < coordinates.size(); ++coordinate) {
      coordinates[coordinate] = lower_coordinates[coordinate] +
                                interpolation * (upper_coordinates[coordinate] - lower_coordinates[coordinate]);
    }
    FeatureSupportSample evaluated;
    if (!EvaluateRankSearchSample(reevaluate, first, coordinates, options.maximum_refinement_steps, callback_calls,
                                  &evaluated) ||
        !evaluated.direction_jacobian_available) {
      *numerical_incomplete = true;
      return false;
    }
    const S2Differential differential =
        RestrictedS2Differential(evaluated, coordinate_dimension, options.rank_relative_tolerance);
    if (differential.rank < 0) {
      *numerical_incomplete = true;
      return false;
    }
    if (!have_best || differential.singular_values[1] < best_differential.singular_values[1]) {
      best = evaluated;
      best_differential = differential;
      have_best = true;
    }
    double determinant = 0.0;
    if (!OrientedS2Minor(evaluated, coordinate_dimension, determinant_coordinates[0], determinant_coordinates[1],
                         &determinant)) {
      *numerical_incomplete = true;
      return false;
    }
    if (differential.rank < regular_rank ||
        std::fabs(determinant) <= options.rank_relative_tolerance * determinant_scale) {
      lower_coordinates = coordinates;
      upper_coordinates = coordinates;
      break;
    }
    if (std::signbit(determinant) == std::signbit(first_determinant)) {
      lower_coordinates = coordinates;
      first_determinant = determinant;
    } else {
      upper_coordinates = coordinates;
      second_determinant = determinant;
    }
  }
  if (!have_best || best_differential.rank >= regular_rank) {
    return false;
  }

  candidate->mechanism = FeatureMechanism::kInteriorRankLoss;
  candidate->status = FeatureEvidenceStatus::kCandidate;
  candidate->provenance = best.provenance;
  std::copy(best.direction, best.direction + 3, candidate->direction);
  candidate->support_dimension = best.support_dimension;
  candidate->mapping_rank = best_differential.rank;
  candidate->singular_values[0] = best_differential.singular_values[0];
  candidate->singular_values[1] = best_differential.singular_values[1];
  candidate->weighted_mass = best.weight;
  candidate->residual = std::max(best_differential.tangent_residual, best.direction_jacobian_error);
  candidate->resolution = refinement_resolution;
  candidate->reason = "a signed S2 tangent minor bracket was refined and the complete support differential lost rank";
  const double tolerance = std::max(1e-10, 10.0 * options.rank_relative_tolerance * determinant_scale);
  if (globally_complete && candidate->residual <= tolerance) {
    candidate->status = FeatureEvidenceStatus::kConfirmed;
  }
  return true;
}

double RankLossObjective(const S2Differential& differential, int regular_rank) {
  if (differential.rank < 0 || regular_rank < 1 || regular_rank > 2) {
    return std::numeric_limits<double>::infinity();
  }
  return differential.singular_values[regular_rank - 1];
}

bool SearchCellRankLoss(const FeatureSupportBatch& batch, const std::vector<const FeatureSupportCellAxis*>& axes,
                        const FeatureDiscoveryOptions& options, const FeatureReevaluateFn& reevaluate,
                        bool globally_complete, FeatureCandidate* candidate, bool* numerical_incomplete) {
  if (!reevaluate || axes.empty() || options.maximum_refinement_steps < 3) {
    return false;
  }

  const FeatureSupportSample& center = batch.samples[static_cast<size_t>(axes.front()->center)];
  std::vector<const FeatureSupportCellAxis*> ordered_axes = axes;
  std::sort(ordered_axes.begin(), ordered_axes.end(),
            [](const FeatureSupportCellAxis* first, const FeatureSupportCellAxis* second) {
              return first->coordinate_index < second->coordinate_index;
            });
  std::vector<int> scoped_coordinates;
  std::vector<double> lower;
  std::vector<double> upper;
  scoped_coordinates.reserve(ordered_axes.size());
  lower.reserve(ordered_axes.size());
  upper.reserve(ordered_axes.size());
  for (const FeatureSupportCellAxis* axis : ordered_axes) {
    scoped_coordinates.push_back(axis->coordinate_index);
    lower.push_back(
        batch.samples[static_cast<size_t>(axis->lower)].coordinates[static_cast<size_t>(axis->coordinate_index)]);
    upper.push_back(
        batch.samples[static_cast<size_t>(axis->upper)].coordinates[static_cast<size_t>(axis->coordinate_index)]);
  }

  int regular_rank = -1;
  double regular_scale = 0.0;
  for (const FeatureSupportCellAxis* axis : ordered_axes) {
    for (int sample_index : { axis->lower, axis->center, axis->upper }) {
      const S2Differential differential =
          RestrictedS2Differential(batch.samples[static_cast<size_t>(sample_index)], batch.coordinate_dimension,
                                   scoped_coordinates, options.rank_relative_tolerance);
      regular_rank = std::max(regular_rank, differential.rank);
      regular_scale = std::max(regular_scale, differential.singular_values[std::max(0, differential.rank - 1)]);
    }
  }
  if (regular_rank <= 0) {
    *numerical_incomplete = true;
    return false;
  }

  const int dimension = static_cast<int>(ordered_axes.size());
  int grid_extent = 3;
  if (dimension == 1) {
    grid_extent = std::min(65, options.maximum_refinement_steps + 2);
  } else if (dimension == 2) {
    grid_extent = std::min(21, static_cast<int>(std::sqrt(options.maximum_refinement_steps)));
  } else {
    grid_extent = static_cast<int>(
        std::floor(std::pow(std::max(3, options.maximum_refinement_steps / 2), 1.0 / static_cast<double>(dimension))));
  }
  grid_extent = std::max(3, grid_extent);
  if ((grid_extent & 1) == 0) {
    --grid_extent;
  }

  FeatureSupportSample best;
  S2Differential best_differential;
  std::vector<double> best_coordinates;
  double best_objective = std::numeric_limits<double>::infinity();
  int callback_calls = 0;
  bool callback_failed = false;
  std::vector<int> grid_indices(static_cast<size_t>(dimension), 1);
  while (callback_calls < options.maximum_refinement_steps) {
    std::vector<double> coordinates = center.coordinates;
    for (int axis_index = 0; axis_index < dimension; ++axis_index) {
      const double fraction =
          static_cast<double>(grid_indices[static_cast<size_t>(axis_index)]) / static_cast<double>(grid_extent - 1);
      coordinates[static_cast<size_t>(scoped_coordinates[static_cast<size_t>(axis_index)])] =
          lower[static_cast<size_t>(axis_index)] +
          fraction * (upper[static_cast<size_t>(axis_index)] - lower[static_cast<size_t>(axis_index)]);
    }
    FeatureSupportSample evaluated;
    if (!EvaluateRankSearchSample(reevaluate, center, coordinates, options.maximum_refinement_steps, &callback_calls,
                                  &evaluated) ||
        !evaluated.direction_jacobian_available) {
      callback_failed = true;
      break;
    }
    const S2Differential differential = RestrictedS2Differential(evaluated, batch.coordinate_dimension,
                                                                 scoped_coordinates, options.rank_relative_tolerance);
    const double objective = RankLossObjective(differential, regular_rank);
    if (objective < best_objective) {
      best = std::move(evaluated);
      best_differential = differential;
      best_coordinates = coordinates;
      best_objective = objective;
    }

    int carry_axis = dimension - 1;
    for (; carry_axis >= 0; --carry_axis) {
      int& index = grid_indices[static_cast<size_t>(carry_axis)];
      ++index;
      if (index < grid_extent - 1) {
        break;
      }
      index = 1;
    }
    if (carry_axis < 0) {
      break;
    }
  }
  if (callback_failed) {
    *numerical_incomplete = true;
  }
  if (best_coordinates.empty()) {
    return false;
  }

  std::vector<double> steps(static_cast<size_t>(dimension));
  for (int axis_index = 0; axis_index < dimension; ++axis_index) {
    steps[static_cast<size_t>(axis_index)] =
        (upper[static_cast<size_t>(axis_index)] - lower[static_cast<size_t>(axis_index)]) /
        static_cast<double>(grid_extent - 1);
  }
  for (int sweep = 0; callback_calls < options.maximum_refinement_steps; ++sweep) {
    bool improved = false;
    for (int axis_index = 0; axis_index < dimension && callback_calls < options.maximum_refinement_steps;
         ++axis_index) {
      const int coordinate = scoped_coordinates[static_cast<size_t>(axis_index)];
      for (double sign : { -1.0, 1.0 }) {
        if (callback_calls >= options.maximum_refinement_steps) {
          break;
        }
        std::vector<double> coordinates = best_coordinates;
        coordinates[static_cast<size_t>(coordinate)] += sign * steps[static_cast<size_t>(axis_index)];
        if (!(coordinates[static_cast<size_t>(coordinate)] > lower[static_cast<size_t>(axis_index)]) ||
            !(coordinates[static_cast<size_t>(coordinate)] < upper[static_cast<size_t>(axis_index)])) {
          continue;
        }
        FeatureSupportSample evaluated;
        if (!EvaluateRankSearchSample(reevaluate, center, coordinates, options.maximum_refinement_steps,
                                      &callback_calls, &evaluated) ||
            !evaluated.direction_jacobian_available) {
          *numerical_incomplete = true;
          continue;
        }
        const S2Differential differential = RestrictedS2Differential(
            evaluated, batch.coordinate_dimension, scoped_coordinates, options.rank_relative_tolerance);
        const double objective = RankLossObjective(differential, regular_rank);
        if (objective < best_objective) {
          best = std::move(evaluated);
          best_differential = differential;
          best_coordinates = std::move(coordinates);
          best_objective = objective;
          improved = true;
        }
      }
    }
    if (!improved) {
      for (double& step : steps) {
        step *= 0.5;
      }
    }
    if (sweep > 0 && *std::max_element(steps.begin(), steps.end()) <= 1e-12) {
      break;
    }
  }
  if (best_differential.rank >= regular_rank) {
    return false;
  }

  candidate->mechanism = FeatureMechanism::kInteriorRankLoss;
  candidate->status = FeatureEvidenceStatus::kCandidate;
  candidate->provenance = best.provenance;
  std::copy(best.direction, best.direction + 3, candidate->direction);
  candidate->support_dimension = best.support_dimension;
  candidate->mapping_rank = best_differential.rank;
  candidate->singular_values[0] = best_differential.singular_values[0];
  candidate->singular_values[1] = best_differential.singular_values[1];
  candidate->weighted_mass = best.weight;
  candidate->residual = std::max({ best_objective, best_differential.tangent_residual, best.direction_jacobian_error });
  candidate->resolution = *std::max_element(steps.begin(), steps.end());
  candidate->reason =
      "a bounded joint search of the continuous support cell localized an interior differential rank loss";
  const double tolerance = std::max(1e-10, 10.0 * options.rank_relative_tolerance * regular_scale);
  if (globally_complete && candidate->residual <= tolerance) {
    candidate->status = FeatureEvidenceStatus::kConfirmed;
  }
  return true;
}

double DirectionDistance(const double first[3], const double second[3]) {
  const double dot = std::max(-1.0, std::min(1.0, Dot(first, second)));
  return std::acos(dot);
}

void AddCandidate(const FeatureCandidate& candidate, double merge_tolerance, FeatureDiscoveryResult* out) {
  for (FeatureCandidate& existing : out->candidates) {
    if (existing.mechanism == candidate.mechanism && SameProvenanceBranch(existing.provenance, candidate.provenance) &&
        existing.scope_kind == candidate.scope_kind && existing.scope_id == candidate.scope_id &&
        existing.scope_active_coordinates == candidate.scope_active_coordinates &&
        DirectionDistance(existing.direction, candidate.direction) <= merge_tolerance) {
      existing.weighted_mass += candidate.weighted_mass;
      existing.residual = std::max(existing.residual, candidate.residual);
      existing.resolution = std::max(existing.resolution, candidate.resolution);
      if (candidate.has_weight_sides) {
        existing.has_weight_sides = true;
        existing.weight_sides[0] = candidate.weight_sides[0];
        existing.weight_sides[1] = candidate.weight_sides[1];
      }
      for (const std::string& constraint : candidate.active_constraints) {
        if (std::find(existing.active_constraints.begin(), existing.active_constraints.end(), constraint) ==
            existing.active_constraints.end()) {
          existing.active_constraints.push_back(constraint);
        }
      }
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
  candidate.scope_active_coordinates = sample.active_coordinates;
  candidate.scope_parameters.resize(sample.active_coordinates.size());
  candidate.reason = std::move(reason);
  return candidate;
}

FeatureSupportScope ScopeForCell(const FeatureSupportBatch& batch, int cell_id) {
  const auto found = std::find_if(batch.scopes.begin(), batch.scopes.end(),
                                  [cell_id](const FeatureSupportScope& scope) { return scope.cell_id == cell_id; });
  return found == batch.scopes.end() ? FeatureSupportScope{} : *found;
}

void ApplyCandidateScope(const FeatureSupportBatch& batch, int cell_id, const std::vector<int>& active_coordinates,
                         FeatureCandidate* candidate) {
  const FeatureSupportScope scope = ScopeForCell(batch, cell_id);
  candidate->scope_kind = scope.kind;
  candidate->scope_id = scope.scope_id;
  if (scope.kind == FeatureSupportScopeKind::kConditional) {
    candidate->weighted_mass = 0.0;
  }
  candidate->scope_active_coordinates = active_coordinates;
  candidate->scope_parameters.clear();
  candidate->scope_parameters.reserve(active_coordinates.size());
  for (int coordinate : active_coordinates) {
    candidate->scope_parameters.push_back(batch.parameter_descriptors.empty() ?
                                              FeatureParameterDescriptor{} :
                                              batch.parameter_descriptors[static_cast<size_t>(coordinate)]);
  }
}

int MechanismIndex(FeatureMechanism mechanism) {
  return static_cast<int>(mechanism) - static_cast<int>(FeatureMechanism::kInteriorRankLoss);
}

FeatureMechanism MechanismForConstraint(ConstraintKind kind) {
  switch (kind) {
    case ConstraintKind::kDomain:
    case ConstraintKind::kEntry:
      return FeatureMechanism::kSupportBoundary;
    case ConstraintKind::kTir:
      return FeatureMechanism::kOpticalKink;
    case ConstraintKind::kFilter:
      return FeatureMechanism::kFilterBoundary;
    case ConstraintKind::kWeight:
      return FeatureMechanism::kWeightKink;
  }
  return FeatureMechanism::kSupportBoundary;
}

bool SameConstraint(const SupportConstraint& first, const SupportConstraint& second) {
  return first.name == second.name && first.kind == second.kind && first.layer_index == second.layer_index &&
         first.interface_index == second.interface_index;
}

const SupportConstraint* FindConstraint(const FeatureSupportSample& sample, const SupportConstraint& target) {
  const auto found =
      std::find_if(sample.constraints.begin(), sample.constraints.end(),
                   [&](const SupportConstraint& candidate) { return SameConstraint(candidate, target); });
  return found == sample.constraints.end() ? nullptr : &*found;
}

bool NormalizedInterpolation(const FeatureSupportSample& first, const FeatureSupportSample& second, double t,
                             double direction[3]) {
  for (int component = 0; component < 3; ++component) {
    direction[component] = (1.0 - t) * first.direction[component] + t * second.direction[component];
  }
  const double norm = std::sqrt(Dot(direction, direction));
  if (!std::isfinite(norm) || !(norm > 0.0)) {
    return false;
  }
  for (int component = 0; component < 3; ++component) {
    direction[component] /= norm;
  }
  return true;
}

FeatureCandidate ConstraintCandidate(const FeatureSupportSample& sample, const SupportConstraint& constraint,
                                     FeatureEvidenceStatus status, std::string reason) {
  FeatureCandidate candidate =
      CandidateFromSample(sample, MechanismForConstraint(constraint.kind), status, std::move(reason));
  candidate.provenance.layer_index = constraint.layer_index;
  candidate.provenance.interface_index = constraint.interface_index;
  candidate.residual = std::fabs(constraint.value);
  candidate.active_constraints.push_back(constraint.name);
  return candidate;
}

bool RefineConstraintRoot(const FeatureSupportSample& first, const FeatureSupportSample& second,
                          const SupportConstraint& target, const FeatureDiscoveryOptions& options,
                          const FeatureReevaluateFn& reevaluate, FeatureCandidate* candidate,
                          bool* numerical_incomplete) {
  if (!reevaluate || first.coordinates.size() != second.coordinates.size()) {
    return false;
  }
  std::vector<double> lo = first.coordinates;
  std::vector<double> hi = second.coordinates;
  double initial_resolution2 = 0.0;
  for (size_t coordinate = 0; coordinate < lo.size(); ++coordinate) {
    const double delta = hi[coordinate] - lo[coordinate];
    initial_resolution2 += delta * delta;
  }
  double lo_value = target.value;
  const SupportConstraint* second_constraint = FindConstraint(second, target);
  if (second_constraint == nullptr || !second_constraint->numerically_available) {
    *numerical_incomplete = true;
    candidate->status = FeatureEvidenceStatus::kNumericalIncomplete;
    return false;
  }
  double hi_value = second_constraint->value;
  FeatureSupportSample best;
  double best_residual = std::numeric_limits<double>::infinity();
  std::string callback_error;
  for (int step = 0; step < options.maximum_refinement_steps; ++step) {
    FeatureReevaluationRequest request;
    request.provenance = first.provenance;
    request.coordinates.resize(lo.size());
    for (size_t coordinate = 0; coordinate < lo.size(); ++coordinate) {
      request.coordinates[coordinate] = 0.5 * (lo[coordinate] + hi[coordinate]);
    }
    FeatureSupportSample evaluated;
    if (!reevaluate(request, &evaluated, &callback_error) ||
        !CallbackSampleMatchesRequest(evaluated, request, first, static_cast<int>(request.coordinates.size()))) {
      *numerical_incomplete = true;
      candidate->status = FeatureEvidenceStatus::kNumericalIncomplete;
      return false;
    }
    const SupportConstraint* constraint = FindConstraint(evaluated, target);
    if (constraint == nullptr || !constraint->numerically_available || !evaluated.numerically_available) {
      *numerical_incomplete = true;
      candidate->status = FeatureEvidenceStatus::kNumericalIncomplete;
      return false;
    }
    const double residual = std::fabs(constraint->value);
    if (residual < best_residual) {
      best = evaluated;
      best_residual = residual;
    }
    if (residual <= options.margin_tolerance) {
      break;
    }
    if (std::signbit(constraint->value) == std::signbit(lo_value)) {
      lo = request.coordinates;
      lo_value = constraint->value;
    } else {
      hi = request.coordinates;
      hi_value = constraint->value;
    }
  }
  (void)hi_value;
  if (!std::isfinite(best_residual)) {
    return false;
  }
  std::copy(best.direction, best.direction + 3, candidate->direction);
  candidate->residual = best_residual;
  candidate->resolution = 0.0;
  for (size_t coordinate = 0; coordinate < lo.size(); ++coordinate) {
    const double delta = hi[coordinate] - lo[coordinate];
    candidate->resolution += delta * delta;
  }
  candidate->resolution = std::sqrt(candidate->resolution);
  if (target.kind == ConstraintKind::kFilter) {
    const double attainable = std::sqrt(initial_resolution2) * std::ldexp(1.0, -options.maximum_refinement_steps);
    candidate->residual = 0.0;
    return candidate->resolution <= std::max(options.margin_tolerance, 2.0 * attainable);
  }
  return best_residual <= options.margin_tolerance;
}

bool ValidOptions(const FeatureDiscoveryOptions& options, std::string* error) {
  if (!std::isfinite(options.margin_tolerance) || !(options.margin_tolerance > 0.0) ||
      !std::isfinite(options.rank_relative_tolerance) || !(options.rank_relative_tolerance > 0.0) ||
      !std::isfinite(options.sky_merge_tolerance) || !(options.sky_merge_tolerance > 0.0) ||
      options.maximum_refinement_steps <= 0 || options.sky_z_bins < 4 || options.sky_z_bins % 2 != 0 ||
      options.sky_azimuth_bins < 8 || options.sky_azimuth_bins % 2 != 0) {
    return Fail("feature discovery options require positive tolerances and even sky grids of at least 4x8", error);
  }
  return true;
}

struct SkyGridCell {
  double mass = 0.0;
  int sample_count = 0;
};

struct SkyGrid {
  int z_bins = 0;
  int azimuth_bins = 0;
  double solid_angle = 0.0;
  std::vector<SkyGridCell> cells;
};

constexpr double kPi = 3.14159265358979323846;

int SkyCellIndex(const double direction[3], int z_bins, int azimuth_bins) {
  const double z_position = 0.5 * (std::max(-1.0, std::min(1.0, direction[2])) + 1.0);
  const int z_index = std::min(z_bins - 1, static_cast<int>(z_position * z_bins));
  double azimuth = std::atan2(direction[1], direction[0]);
  if (azimuth < 0.0) {
    azimuth += 2.0 * kPi;
  }
  const int azimuth_index = std::min(azimuth_bins - 1, static_cast<int>(azimuth * azimuth_bins / (2.0 * kPi)));
  return z_index * azimuth_bins + azimuth_index;
}

SkyGrid BuildSkyGrid(const FeatureSupportBatch& batch, int z_bins, int azimuth_bins) {
  SkyGrid grid;
  grid.z_bins = z_bins;
  grid.azimuth_bins = azimuth_bins;
  grid.solid_angle = 4.0 * kPi / static_cast<double>(z_bins * azimuth_bins);
  grid.cells.resize(static_cast<size_t>(z_bins * azimuth_bins));
  std::vector<size_t> order(batch.samples.size());
  std::iota(order.begin(), order.end(), 0u);
  std::sort(order.begin(), order.end(), [&](size_t first, size_t second) {
    return batch.samples[first].sample_id < batch.samples[second].sample_id;
  });
  for (size_t sample_index : order) {
    const FeatureSupportSample& sample = batch.samples[sample_index];
    if (!sample.numerically_available || !sample.accumulates_measure) {
      continue;
    }
    SkyGridCell& cell = grid.cells[static_cast<size_t>(SkyCellIndex(sample.direction, z_bins, azimuth_bins))];
    cell.mass += sample.weight;
    ++cell.sample_count;
  }
  return grid;
}

const SkyGridCell& SkyCell(const SkyGrid& grid, int z_index, int azimuth_index) {
  azimuth_index = (azimuth_index % grid.azimuth_bins + grid.azimuth_bins) % grid.azimuth_bins;
  return grid.cells[static_cast<size_t>(z_index * grid.azimuth_bins + azimuth_index)];
}

double SkyDensity(const SkyGrid& grid, int z_index, int azimuth_index) {
  return SkyCell(grid, z_index, azimuth_index).mass / grid.solid_angle;
}

void SkyCellDirection(const SkyGrid& grid, int z_index, int azimuth_index, double direction[3]) {
  const double z = -1.0 + (static_cast<double>(z_index) + 0.5) * 2.0 / grid.z_bins;
  const double azimuth = (static_cast<double>(azimuth_index) + 0.5) * 2.0 * kPi / grid.azimuth_bins;
  const double radius = std::sqrt(std::max(0.0, 1.0 - z * z));
  direction[0] = radius * std::cos(azimuth);
  direction[1] = radius * std::sin(azimuth);
  direction[2] = z;
}

struct SkyDifferential {
  bool complete_neighborhood = false;
  bool maximum = false;
  bool ridge = false;
  double gradient[2]{};
  double gradient_norm = 0.0;
  double hessian_eigenvalues[2]{};
  double ridge_residual = 0.0;
};

SkyDifferential EvaluateSkyDifferential(const SkyGrid& grid, int z_index, int azimuth_index) {
  SkyDifferential out;
  if (z_index <= 0 || z_index + 1 >= grid.z_bins || SkyCell(grid, z_index, azimuth_index).sample_count == 0) {
    return out;
  }
  for (int dz = -1; dz <= 1; ++dz) {
    for (int da = -1; da <= 1; ++da) {
      if (SkyCell(grid, z_index + dz, azimuth_index + da).sample_count == 0) {
        return out;
      }
    }
  }
  out.complete_neighborhood = true;
  const double center = SkyDensity(grid, z_index, azimuth_index);
  const double left = SkyDensity(grid, z_index, azimuth_index - 1);
  const double right = SkyDensity(grid, z_index, azimuth_index + 1);
  const double down = SkyDensity(grid, z_index - 1, azimuth_index);
  const double up = SkyDensity(grid, z_index + 1, azimuth_index);
  out.maximum = center > left && center > right && center > down && center > up;
  out.gradient[0] = 0.5 * (right - left);
  out.gradient[1] = 0.5 * (up - down);
  out.gradient_norm = std::hypot(out.gradient[0], out.gradient[1]);
  const double h00 = right - 2.0 * center + left;
  const double h11 = up - 2.0 * center + down;
  const double h01 =
      0.25 * (SkyDensity(grid, z_index + 1, azimuth_index + 1) - SkyDensity(grid, z_index + 1, azimuth_index - 1) -
              SkyDensity(grid, z_index - 1, azimuth_index + 1) + SkyDensity(grid, z_index - 1, azimuth_index - 1));
  const double trace = h00 + h11;
  const double discriminant = std::hypot(h00 - h11, 2.0 * h01);
  out.hessian_eigenvalues[0] = 0.5 * (trace - discriminant);
  out.hessian_eigenvalues[1] = 0.5 * (trace + discriminant);
  double eigenvector[2] = { h01, out.hessian_eigenvalues[0] - h00 };
  double eigenvector_norm = std::hypot(eigenvector[0], eigenvector[1]);
  if (!(eigenvector_norm > 1e-15)) {
    eigenvector[0] =
        std::fabs(h00 - out.hessian_eigenvalues[0]) < std::fabs(h11 - out.hessian_eigenvalues[0]) ? 1.0 : 0.0;
    eigenvector[1] = eigenvector[0] == 0.0 ? 1.0 : 0.0;
    eigenvector_norm = 1.0;
  }
  out.ridge_residual =
      std::fabs((out.gradient[0] * eigenvector[0] + out.gradient[1] * eigenvector[1]) / eigenvector_norm);
  const double curvature_scale = std::max(1e-12, std::fabs(out.hessian_eigenvalues[0]));
  out.ridge = out.hessian_eigenvalues[0] < -1e-12 && out.ridge_residual <= 0.25 * curvature_scale;
  return out;
}

void DiscoverSkyFeatures(const FeatureSupportBatch& batch, const FeatureDiscoveryOptions& options,
                         bool globally_complete, FeatureDiscoveryResult* out) {
  const SkyGrid fine = BuildSkyGrid(batch, options.sky_z_bins, options.sky_azimuth_bins);
  const SkyGrid coarse = BuildSkyGrid(batch, options.sky_z_bins / 2, options.sky_azimuth_bins / 2);
  out->sky_field.reserve(fine.cells.size());
  for (int z_index = 0; z_index < fine.z_bins; ++z_index) {
    for (int azimuth_index = 0; azimuth_index < fine.azimuth_bins; ++azimuth_index) {
      const SkyGridCell& cell = SkyCell(fine, z_index, azimuth_index);
      const SkyDifferential differential = EvaluateSkyDifferential(fine, z_index, azimuth_index);
      const int coarse_z = z_index / 2;
      const int coarse_azimuth = azimuth_index / 2;
      const SkyDifferential coarse_differential = EvaluateSkyDifferential(coarse, coarse_z, coarse_azimuth);
      SkyFieldNode node;
      SkyCellDirection(fine, z_index, azimuth_index, node.direction);
      node.value = cell.mass;
      node.normalized_value = cell.mass / fine.solid_angle;
      node.gradient_norm = differential.gradient_norm;
      node.hessian_eigenvalues[0] = differential.hessian_eigenvalues[0];
      node.hessian_eigenvalues[1] = differential.hessian_eigenvalues[1];
      node.error = std::fabs(node.normalized_value - SkyDensity(coarse, coarse_z, coarse_azimuth));
      node.resolution = std::sqrt(fine.solid_angle);
      node.sample_count = cell.sample_count;
      node.status = differential.complete_neighborhood ? FeatureEvidenceStatus::kCandidate :
                                                         FeatureEvidenceStatus::kNumericalIncomplete;
      out->sky_field.push_back(node);

      for (FeatureMechanism mechanism : { FeatureMechanism::kBrightnessMaximum, FeatureMechanism::kBrightnessRidge }) {
        const bool detected =
            mechanism == FeatureMechanism::kBrightnessMaximum ? differential.maximum : differential.ridge;
        if (!detected) {
          continue;
        }
        const bool coarse_detected =
            mechanism == FeatureMechanism::kBrightnessMaximum ? coarse_differential.maximum : coarse_differential.ridge;
        FeatureCandidate candidate;
        candidate.mechanism = mechanism;
        candidate.status = globally_complete && differential.complete_neighborhood && coarse_detected ?
                               FeatureEvidenceStatus::kConfirmed :
                               FeatureEvidenceStatus::kCandidate;
        std::copy(node.direction, node.direction + 3, candidate.direction);
        candidate.weighted_mass = node.value;
        candidate.residual =
            mechanism == FeatureMechanism::kBrightnessMaximum ? node.gradient_norm : differential.ridge_residual;
        candidate.resolution = node.resolution;
        candidate.reason = mechanism == FeatureMechanism::kBrightnessMaximum ?
                               "a complete equal-area sky neighborhood has a local brightness maximum" :
                               "the sky-field Hessian has transverse negative curvature and a small normal gradient";
        AddCandidate(candidate, options.sky_merge_tolerance, out);
      }
    }
  }
}

using ConcentrationBranch = std::tuple<int, int, int>;

void DiscoverFiniteWidthConcentrations(const FeatureSupportBatch& batch, const FeatureDiscoveryOptions& options,
                                       bool globally_complete, FeatureDiscoveryResult* out,
                                       bool* resolution_incomplete) {
  std::map<ConcentrationBranch, std::vector<const FeatureSupportSample*>> branches;
  for (const FeatureSupportSample& sample : batch.samples) {
    if (sample.accumulates_measure && sample.finite_width && sample.numerically_available) {
      branches[{ sample.provenance.member_index, sample.provenance.spectrum_node_id, sample.provenance.source_node_id }]
          .push_back(&sample);
    }
  }
  for (const auto& [branch, samples] : branches) {
    (void)branch;
    if (samples.size() < 4u) {
      *resolution_incomplete = true;
      continue;
    }
    double weighted_direction[3]{};
    double parity_direction[2][3]{};
    double total_weight = 0.0;
    double parity_weight[2]{};
    for (const FeatureSupportSample* sample : samples) {
      if (!(sample->weight > 0.0)) {
        continue;
      }
      const int parity = sample->provenance.sample_index & 1;
      total_weight += sample->weight;
      parity_weight[parity] += sample->weight;
      for (int component = 0; component < 3; ++component) {
        weighted_direction[component] += sample->weight * sample->direction[component];
        parity_direction[parity][component] += sample->weight * sample->direction[component];
      }
    }
    if (!(total_weight > 0.0) || !(parity_weight[0] > 0.0) || !(parity_weight[1] > 0.0)) {
      *resolution_incomplete = true;
      continue;
    }
    const double resultant = std::sqrt(Dot(weighted_direction, weighted_direction));
    const double concentration = resultant / total_weight;
    if (!(resultant > 0.0) || concentration < 0.95) {
      continue;
    }
    for (double& component : weighted_direction) {
      component /= resultant;
    }
    double parity_unit[2][3]{};
    for (int parity = 0; parity < 2; ++parity) {
      const double norm = std::sqrt(Dot(parity_direction[parity], parity_direction[parity]));
      if (!(norm > 0.0)) {
        *resolution_incomplete = true;
        continue;
      }
      for (int component = 0; component < 3; ++component) {
        parity_unit[parity][component] = parity_direction[parity][component] / norm;
      }
    }
    const double split_error = DirectionDistance(parity_unit[0], parity_unit[1]);
    double spread2 = 0.0;
    for (const FeatureSupportSample* sample : samples) {
      if (sample->weight > 0.0) {
        const double angle = DirectionDistance(weighted_direction, sample->direction);
        spread2 += sample->weight * angle * angle;
      }
    }
    const double spread = std::sqrt(spread2 / total_weight);
    const double sky_resolution = std::sqrt(4.0 * kPi / (options.sky_z_bins * options.sky_azimuth_bins));
    if (spread > 2.0 * sky_resolution || split_error > sky_resolution) {
      continue;
    }
    FeatureCandidate candidate =
        CandidateFromSample(*samples.front(), FeatureMechanism::kFiniteWidthConcentration,
                            globally_complete && split_error <= spread + 1e-12 ? FeatureEvidenceStatus::kConfirmed :
                                                                                 FeatureEvidenceStatus::kCandidate,
                            "positive-width input mass is concentrated in a stable local sky neighborhood");
    std::copy(weighted_direction, weighted_direction + 3, candidate.direction);
    candidate.weighted_mass = total_weight;
    candidate.residual = split_error;
    candidate.resolution = spread;
    AddCandidate(candidate, options.sky_merge_tolerance, out);
  }
}

void DiscoverWeightKinks(const FeatureSupportBatch& batch, const FeatureDiscoveryOptions& options,
                         FeatureDiscoveryResult* out, bool* numerical_incomplete) {
  for (const FeatureSupportCellAxis& axis : batch.cell_axes) {
    const FeatureSupportSample& lower = batch.samples[static_cast<size_t>(axis.lower)];
    const FeatureSupportSample& center = batch.samples[static_cast<size_t>(axis.center)];
    const FeatureSupportSample& upper = batch.samples[static_cast<size_t>(axis.upper)];
    if (!lower.numerically_available || !center.numerically_available || !upper.numerically_available) {
      *numerical_incomplete = true;
      continue;
    }
    const double half_span = 0.5 * axis.parameter_span;
    for (const SupportConstraint& center_weight : center.constraints) {
      if (center_weight.kind != ConstraintKind::kWeight || !center_weight.numerically_available) {
        continue;
      }
      const SupportConstraint* lower_weight = FindConstraint(lower, center_weight);
      const SupportConstraint* upper_weight = FindConstraint(upper, center_weight);
      if (lower_weight == nullptr || upper_weight == nullptr || !lower_weight->numerically_available ||
          !upper_weight->numerically_available) {
        *numerical_incomplete = true;
        continue;
      }
      const double left_slope = (center_weight.value - lower_weight->value) / half_span;
      const double right_slope = (upper_weight->value - center_weight.value) / half_span;
      const double jump = std::fabs(right_slope - left_slope);
      const double scale = std::max({ std::fabs(left_slope), std::fabs(right_slope), 1e-12 });
      const double relative_second_difference =
          std::fabs(upper_weight->value - 2.0 * center_weight.value + lower_weight->value) /
          std::max({ std::fabs(lower_weight->value), std::fabs(center_weight.value), std::fabs(upper_weight->value),
                     1e-12 });
      if (jump <= 1e-8 || relative_second_difference <= 0.1 || jump <= 0.25 * scale) {
        continue;
      }
      FeatureCandidate candidate = ConstraintCandidate(
          center, center_weight, FeatureEvidenceStatus::kCandidate,
          "two sides of one real parameter cell show a non-smooth change in an actual weight factor");
      candidate.has_weight_sides = true;
      candidate.weight_sides[0] = lower_weight->value;
      candidate.weight_sides[1] = upper_weight->value;
      candidate.residual = jump;
      candidate.resolution = axis.parameter_span;
      AddCandidate(candidate, options.sky_merge_tolerance, out);
    }
  }
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
  if (batch.version != kFeatureSupportBatchVersionV1 && batch.version != kFeatureSupportBatchVersionV2 &&
      batch.version != kFeatureSupportBatchVersionV3 && batch.version != kFeatureSupportBatchVersion) {
    return Fail("unsupported feature support batch version", error);
  }
  if (batch.coordinate_dimension < 0 || (batch.version == kFeatureSupportBatchVersionV1 &&
                                         batch.coordinate_dimension > kLegacyFeatureDiscoveryCoordinateDimension)) {
    return Fail("coordinate_dimension is outside the supported range", error);
  }
  const size_t measure_sample_count =
      static_cast<size_t>(std::count_if(batch.samples.begin(), batch.samples.end(),
                                        [](const FeatureSupportSample& sample) { return sample.accumulates_measure; }));
  if (batch.visited_row_count < measure_sample_count) {
    return Fail("visited_row_count is smaller than the materialized input-measure sample count", error);
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
    if (!std::isfinite(sample.mapping_error_bound) || sample.mapping_error_bound < 0.0) {
      return Fail("mapping evidence error bounds must be finite and non-negative", error);
    }
    if (sample.mapping_evidence_kind == MappingEvidenceKind::kNone) {
      if (sample.image_dimension_upper_bound != -1 || sample.mapping_error_bound != 0.0) {
        return Fail("samples without mapping evidence must not carry an image-dimension bound", error);
      }
    } else if (sample.mapping_evidence_kind == MappingEvidenceKind::kExactImageDimensionUpperBound) {
      if (batch.version < kFeatureSupportBatchVersionV3 || sample.measure_kind != SupportMeasureKind::kContinuous ||
          sample.support_dimension <= 0 || sample.image_dimension_upper_bound < 0 ||
          sample.image_dimension_upper_bound > std::min(2, sample.support_dimension) ||
          sample.mapping_error_bound != 0.0) {
        return Fail("exact mapping evidence requires a current-version continuous support and an exact valid bound",
                    error);
      }
    } else {
      return Fail("unknown mapping evidence kind", error);
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
    const FeatureSupportSample& first = batch.samples[static_cast<size_t>(edge.first)];
    const FeatureSupportSample& second = batch.samples[static_cast<size_t>(edge.second)];
    if (!SameProvenanceBranch(first.provenance, second.provenance) || !SameActiveCoordinates(first, second)) {
      return Fail("support edge endpoints must belong to one continuous provenance branch", error);
    }
    double coordinate_distance2 = 0.0;
    for (int coordinate : first.active_coordinates) {
      const double delta =
          second.coordinates[static_cast<size_t>(coordinate)] - first.coordinates[static_cast<size_t>(coordinate)];
      coordinate_distance2 += delta * delta;
    }
    const double coordinate_distance = std::sqrt(coordinate_distance2);
    if (!(coordinate_distance > 0.0) || !NearlyEqual(coordinate_distance, edge.parameter_distance)) {
      return Fail("support edge parameter_distance must match its active-coordinate displacement", error);
    }
  }
  std::map<int, std::pair<int, std::set<int>>> cell_topology;
  for (const FeatureSupportCellAxis& axis : batch.cell_axes) {
    if (axis.cell_id < 0 || axis.coordinate_index < 0 || axis.coordinate_index >= batch.coordinate_dimension ||
        axis.lower < 0 || axis.center < 0 || axis.upper < 0 || axis.lower == axis.center || axis.center == axis.upper ||
        axis.lower == axis.upper || axis.lower >= static_cast<int>(batch.samples.size()) ||
        axis.center >= static_cast<int>(batch.samples.size()) || axis.upper >= static_cast<int>(batch.samples.size()) ||
        !std::isfinite(axis.parameter_span) || !(axis.parameter_span > 0.0)) {
      return Fail("support cell axes require three distinct samples and one valid parameter coordinate", error);
    }
    const FeatureSupportSample& lower = batch.samples[static_cast<size_t>(axis.lower)];
    const FeatureSupportSample& center = batch.samples[static_cast<size_t>(axis.center)];
    const FeatureSupportSample& upper = batch.samples[static_cast<size_t>(axis.upper)];
    if (!center.accumulates_measure || lower.accumulates_measure || upper.accumulates_measure ||
        !SameProvenanceBranch(lower.provenance, center.provenance) ||
        !SameProvenanceBranch(center.provenance, upper.provenance) || !SameActiveCoordinates(lower, center) ||
        !SameActiveCoordinates(center, upper)) {
      return Fail("support cell axes must attach two non-measure probes to one input-measure center", error);
    }
    if (std::find(center.active_coordinates.begin(), center.active_coordinates.end(), axis.coordinate_index) ==
        center.active_coordinates.end()) {
      return Fail("support cell coordinate_index must name an active support coordinate", error);
    }
    for (int coordinate = 0; coordinate < batch.coordinate_dimension; ++coordinate) {
      if (coordinate == axis.coordinate_index) {
        continue;
      }
      const size_t offset = static_cast<size_t>(coordinate);
      if (!NearlyEqual(lower.coordinates[offset], center.coordinates[offset]) ||
          !NearlyEqual(center.coordinates[offset], upper.coordinates[offset])) {
        return Fail("support cell probes may differ only along coordinate_index", error);
      }
    }
    const double lower_coordinate = lower.coordinates[static_cast<size_t>(axis.coordinate_index)];
    const double center_coordinate = center.coordinates[static_cast<size_t>(axis.coordinate_index)];
    const double upper_coordinate = upper.coordinates[static_cast<size_t>(axis.coordinate_index)];
    if (!(lower_coordinate < center_coordinate && center_coordinate < upper_coordinate) ||
        !NearlyEqual(upper_coordinate - lower_coordinate, axis.parameter_span)) {
      return Fail("support cell coordinates must be ordered and match parameter_span", error);
    }
    auto& topology = cell_topology[axis.cell_id];
    if (topology.second.empty()) {
      topology.first = axis.center;
    } else if (topology.first != axis.center) {
      return Fail("all axes in one support cell must share the same center", error);
    }
    if (!topology.second.insert(axis.coordinate_index).second) {
      return Fail("a support cell cannot repeat a coordinate axis", error);
    }
  }
  if (!batch.parameter_descriptors.empty()) {
    if (batch.version != kFeatureSupportBatchVersion ||
        batch.parameter_descriptors.size() != static_cast<size_t>(batch.coordinate_dimension)) {
      return Fail("parameter descriptors require a current-version entry for every coordinate", error);
    }
    for (const FeatureParameterDescriptor& parameter : batch.parameter_descriptors) {
      if (parameter.role < FeatureParameterRole::kUnspecified || parameter.role > FeatureParameterRole::kPose) {
        return Fail("parameter descriptor role is outside the published enumeration", error);
      }
      const bool layered =
          parameter.role == FeatureParameterRole::kShape || parameter.role == FeatureParameterRole::kPose;
      if ((layered && parameter.group_id < 0) || (!layered && parameter.group_id != -1)) {
        return Fail("parameter descriptor group does not match its physical role", error);
      }
    }
  }
  if (!batch.scopes.empty()) {
    if (batch.version != kFeatureSupportBatchVersion || batch.parameter_descriptors.empty()) {
      return Fail("explicit support scopes require current-version parameter descriptors", error);
    }
    std::set<int> scope_ids;
    std::set<int> scoped_cells;
    for (const FeatureSupportScope& scope : batch.scopes) {
      if (scope.scope_id < 0 || cell_topology.find(scope.cell_id) == cell_topology.end() ||
          (scope.kind != FeatureSupportScopeKind::kJoint && scope.kind != FeatureSupportScopeKind::kConditional) ||
          !scope_ids.insert(scope.scope_id).second || !scoped_cells.insert(scope.cell_id).second) {
        return Fail("support scopes require unique ids and valid, uniquely owned cells", error);
      }
    }
  }
  for (size_t sample_index = 0; sample_index < batch.samples.size(); ++sample_index) {
    const FeatureSupportSample& sample = batch.samples[sample_index];
    if (sample.mapping_evidence_kind != MappingEvidenceKind::kExactImageDimensionUpperBound) {
      continue;
    }
    const auto topology = std::find_if(cell_topology.begin(), cell_topology.end(), [&](const auto& item) {
      return item.second.first == static_cast<int>(sample_index);
    });
    const std::set<int> active(sample.active_coordinates.begin(), sample.active_coordinates.end());
    if (!sample.accumulates_measure || topology == cell_topology.end() || topology->second.second != active) {
      return Fail("exact mapping evidence must cover every active coordinate of one complete support cell", error);
    }
  }
  return true;
}

FeatureDiscoveryResult DiscoverFeatures(const FeatureSupportBatch& batch, const FeatureDiscoveryOptions& options,
                                        const FeatureReevaluateFn& reevaluate) {
  FeatureDiscoveryResult out;
  out.visited_row_count = batch.visited_row_count;
  out.complete_visit = batch.complete_visit;
  out.materialization_complete = batch.materialization_complete;
  std::string error;
  if (!ValidateFeatureSupportBatch(batch, &error) || !ValidOptions(options, &error)) {
    for (int mechanism = static_cast<int>(FeatureMechanism::kInteriorRankLoss);
         mechanism <= static_cast<int>(FeatureMechanism::kBrightnessRidge); mechanism++) {
      out.mechanisms.push_back(
          { static_cast<FeatureMechanism>(mechanism), FeatureEvidenceStatus::kNotSupported, 0, error });
    }
    return out;
  }
  out.evaluated_sample_count = static_cast<int>(batch.samples.size());
  const bool globally_complete = batch.complete_visit && batch.materialization_complete;
  const FeatureEvidenceStatus empty_status =
      globally_complete ? FeatureEvidenceStatus::kPhysicallyUnreachable : FeatureEvidenceStatus::kNumericalIncomplete;
  const char* empty_reason = globally_complete ?
                                 "the supplied support is empty" :
                                 (!batch.complete_visit ? "the adapter did not visit the complete input" :
                                                          "the support exceeded the materialization budget");
  for (int mechanism = static_cast<int>(FeatureMechanism::kInteriorRankLoss);
       mechanism <= static_cast<int>(FeatureMechanism::kBrightnessRidge); mechanism++) {
    out.mechanisms.push_back(
        { static_cast<FeatureMechanism>(mechanism),
          batch.samples.empty() ? empty_status : FeatureEvidenceStatus::kNotDetectedAtResolution, 0,
          batch.samples.empty() ? empty_reason : "no candidate was detected at the supplied resolution" });
  }
  if (batch.samples.empty()) {
    return out;
  }

  bool rank_numerically_incomplete = false;
  bool constraint_numerically_incomplete = false;
  bool concentration_resolution_incomplete = false;
  std::vector<S2Differential> differentials;
  differentials.reserve(batch.samples.size());
  for (const FeatureSupportSample& sample : batch.samples) {
    differentials.push_back(
        RestrictedS2Differential(sample, batch.coordinate_dimension, options.rank_relative_tolerance));
  }
  std::vector<int> regular_ranks(batch.samples.size(), -1);
  for (const FeatureSupportCellAxis& axis : batch.cell_axes) {
    int local_rank = -1;
    for (int index : { axis.lower, axis.center, axis.upper }) {
      local_rank = std::max(local_rank, differentials[static_cast<size_t>(index)].rank);
    }
    regular_ranks[static_cast<size_t>(axis.center)] =
        std::max(regular_ranks[static_cast<size_t>(axis.center)], local_rank);
  }
  std::map<int, std::vector<const FeatureSupportCellAxis*>> cell_axes;
  for (const FeatureSupportCellAxis& axis : batch.cell_axes) {
    cell_axes[axis.cell_id].push_back(&axis);
  }
  std::set<int> searched_rank_loss_cells;
  for (const auto& [cell_id, axes] : cell_axes) {
    std::vector<int> scoped_coordinates;
    scoped_coordinates.reserve(axes.size());
    for (const FeatureSupportCellAxis* axis : axes) {
      scoped_coordinates.push_back(axis->coordinate_index);
    }
    std::sort(scoped_coordinates.begin(), scoped_coordinates.end());
    FeatureCandidate searched_candidate;
    if (SearchCellRankLoss(batch, axes, options, reevaluate, globally_complete, &searched_candidate,
                           &rank_numerically_incomplete)) {
      ApplyCandidateScope(batch, cell_id, scoped_coordinates, &searched_candidate);
      AddCandidate(searched_candidate, options.sky_merge_tolerance, &out);
      searched_rank_loss_cells.insert(cell_id);
    }
    if (axes.size() < 2u) {
      continue;
    }
    const int center_index = axes.front()->center;
    const FeatureSupportSample& center = batch.samples[static_cast<size_t>(center_index)];
    if (center.support_dimension < 2) {
      continue;
    }
    int callback_calls = 0;
    std::vector<FeatureSupportSample> joint_endpoints;
    if (regular_ranks[static_cast<size_t>(center_index)] < std::min(2, center.support_dimension) &&
        center.mapping_evidence_kind == MappingEvidenceKind::kNone && !reevaluate) {
      rank_numerically_incomplete = true;
    }
    if (regular_ranks[static_cast<size_t>(center_index)] < std::min(2, center.support_dimension) &&
        center.mapping_evidence_kind == MappingEvidenceKind::kNone && reevaluate) {
      std::vector<double> lower_coordinates = center.coordinates;
      std::vector<double> upper_coordinates = center.coordinates;
      for (const FeatureSupportCellAxis* axis : axes) {
        lower_coordinates[static_cast<size_t>(axis->coordinate_index)] =
            batch.samples[static_cast<size_t>(axis->lower)].coordinates[static_cast<size_t>(axis->coordinate_index)];
        upper_coordinates[static_cast<size_t>(axis->coordinate_index)] =
            batch.samples[static_cast<size_t>(axis->upper)].coordinates[static_cast<size_t>(axis->coordinate_index)];
      }
      for (const std::vector<double>* coordinates : { &lower_coordinates, &upper_coordinates }) {
        FeatureSupportSample endpoint;
        if (EvaluateRankSearchSample(reevaluate, center, *coordinates, options.maximum_refinement_steps,
                                     &callback_calls, &endpoint) &&
            endpoint.direction_jacobian_available) {
          regular_ranks[static_cast<size_t>(center_index)] = std::max(
              regular_ranks[static_cast<size_t>(center_index)],
              RestrictedS2Differential(endpoint, batch.coordinate_dimension, options.rank_relative_tolerance).rank);
          joint_endpoints.push_back(std::move(endpoint));
        } else {
          rank_numerically_incomplete = true;
        }
      }
    }
    const int regular_rank = regular_ranks[static_cast<size_t>(center_index)];
    if (searched_rank_loss_cells.find(cell_id) != searched_rank_loss_cells.end()) {
      continue;
    }
    for (const FeatureSupportCellAxis* axis : axes) {
      FeatureCandidate candidate;
      if (RefineMultidimensionalRankLossLine(
              batch.samples[static_cast<size_t>(axis->lower)], batch.samples[static_cast<size_t>(axis->upper)],
              batch.coordinate_dimension, regular_rank, options, reevaluate, globally_complete, &callback_calls,
              &candidate, &rank_numerically_incomplete)) {
        ApplyCandidateScope(batch, cell_id, scoped_coordinates, &candidate);
        AddCandidate(candidate, options.sky_merge_tolerance, &out);
      }
    }
    if (joint_endpoints.size() == 2u) {
      FeatureCandidate candidate;
      if (RefineMultidimensionalRankLossLine(joint_endpoints[0], joint_endpoints[1], batch.coordinate_dimension,
                                             regular_rank, options, reevaluate, globally_complete, &callback_calls,
                                             &candidate, &rank_numerically_incomplete)) {
        ApplyCandidateScope(batch, cell_id, scoped_coordinates, &candidate);
        AddCandidate(candidate, options.sky_merge_tolerance, &out);
      }
    }
  }
  for (const FeatureSupportCellAxis& axis : batch.cell_axes) {
    if (cell_axes[axis.cell_id].size() != 1u ||
        searched_rank_loss_cells.find(axis.cell_id) != searched_rank_loss_cells.end()) {
      continue;
    }
    FeatureCandidate candidate;
    if (RefineOneDimensionalRankLoss(batch, axis, options, reevaluate, globally_complete, &candidate,
                                     &rank_numerically_incomplete)) {
      ApplyCandidateScope(batch, axis.cell_id, { axis.coordinate_index }, &candidate);
      AddCandidate(candidate, options.sky_merge_tolerance, &out);
    }
  }
  for (size_t sample_index = 0; sample_index < batch.samples.size(); ++sample_index) {
    const FeatureSupportSample& sample = batch.samples[sample_index];
    if (!sample.accumulates_measure) {
      continue;
    }
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
    std::vector<const SupportConstraint*> active_constraints;
    for (const SupportConstraint& constraint : sample.constraints) {
      if (!constraint.numerically_available) {
        constraint_numerically_incomplete = true;
        continue;
      }
      if (constraint.kind != ConstraintKind::kWeight && std::fabs(constraint.value) <= options.margin_tolerance) {
        active_constraints.push_back(&constraint);
        AddCandidate(ConstraintCandidate(sample, constraint, FeatureEvidenceStatus::kCandidate,
                                         "a named support margin is active but still requires two-sided evidence"),
                     options.sky_merge_tolerance, &out);
      }
    }
    if (active_constraints.size() >= 2u) {
      FeatureCandidate corner =
          CandidateFromSample(sample, FeatureMechanism::kSupportCorner, FeatureEvidenceStatus::kCandidate,
                              "two or more independent named constraints are active at the same support point");
      for (const SupportConstraint* constraint : active_constraints) {
        corner.active_constraints.push_back(constraint->name);
      }
      AddCandidate(corner, options.sky_merge_tolerance, &out);
    }

    if (sample.support_dimension <= 0) {
      continue;
    }
    if (!sample.direction_jacobian_available) {
      rank_numerically_incomplete = true;
      continue;
    }
    const S2Differential& differential = differentials[sample_index];
    const bool neighborhood_rank_available = regular_ranks[sample_index] >= 0;
    const int regular_rank =
        neighborhood_rank_available ? regular_ranks[sample_index] : std::min(2, sample.support_dimension);
    if (!neighborhood_rank_available) {
      rank_numerically_incomplete = true;
    }
    const bool exact_mapping_bound =
        sample.mapping_evidence_kind == MappingEvidenceKind::kExactImageDimensionUpperBound &&
        sample.mapping_error_bound == 0.0;
    if (sample.measure_kind == SupportMeasureKind::kContinuous) {
      const bool proven_strict_confinement =
          sample.support_dimension == 1 ||
          (exact_mapping_bound && sample.image_dimension_upper_bound >= 0 && sample.image_dimension_upper_bound < 2);
      const bool sampled_strict_confinement = neighborhood_rank_available && regular_rank < 2;
      if (proven_strict_confinement || sampled_strict_confinement) {
        AddCandidate(
            CandidateFromSample(
                sample, FeatureMechanism::kStrictConfinement,
                proven_strict_confinement ? direct_status : FeatureEvidenceStatus::kCandidate,
                proven_strict_confinement ?
                    "the support dimension or exact mapping certificate bounds the sky image below dimension two" :
                    "finite local differentials suggest confinement but do not prove a global image-dimension bound"),
            options.sky_merge_tolerance, &out);
      }
      if (exact_mapping_bound && sample.image_dimension_upper_bound == 0 && sample.weight > 0.0) {
        FeatureCandidate atom =
            CandidateFromSample(sample, FeatureMechanism::kMeasureAtom, direct_status,
                                "an exact complete-cell mapping certificate proves constant sky direction");
        atom.mapping_rank = 0;
        AddCandidate(atom, options.sky_merge_tolerance, &out);
      }
    }
    if (differential.rank < regular_rank) {
      const double numerical_residual = std::max(differential.tangent_residual, sample.direction_jacobian_error);
      const FeatureEvidenceStatus status =
          globally_complete && neighborhood_rank_available &&
                  numerical_residual <= std::max(1e-8, 10.0 * options.rank_relative_tolerance) ?
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

  for (const FeatureSupportEdge& edge : batch.edges) {
    const FeatureSupportSample& first = batch.samples[static_cast<size_t>(edge.first)];
    const FeatureSupportSample& second = batch.samples[static_cast<size_t>(edge.second)];
    if (!first.numerically_available || !second.numerically_available) {
      constraint_numerically_incomplete = true;
      continue;
    }
    std::vector<std::string> crossing_constraints;
    FeatureCandidate first_crossing;
    bool have_first_crossing = false;
    for (const SupportConstraint& first_constraint : first.constraints) {
      if (first_constraint.kind == ConstraintKind::kWeight) {
        continue;
      }
      const SupportConstraint* second_constraint = FindConstraint(second, first_constraint);
      if (!first_constraint.numerically_available || second_constraint == nullptr ||
          !second_constraint->numerically_available) {
        constraint_numerically_incomplete = true;
        continue;
      }
      if (std::fabs(first_constraint.value) <= options.margin_tolerance ||
          std::fabs(second_constraint->value) <= options.margin_tolerance ||
          std::signbit(first_constraint.value) == std::signbit(second_constraint->value)) {
        continue;
      }
      const double denominator = std::fabs(first_constraint.value) + std::fabs(second_constraint->value);
      const double interpolation = std::fabs(first_constraint.value) / denominator;
      FeatureCandidate candidate = ConstraintCandidate(
          first, first_constraint, FeatureEvidenceStatus::kCandidate,
          "opposite constraint-margin signs bracket a support transition; callback refinement preserves the branch");
      if (!NormalizedInterpolation(first, second, interpolation, candidate.direction)) {
        constraint_numerically_incomplete = true;
        continue;
      }
      candidate.has_weight_sides = true;
      candidate.weight_sides[0] = first.weight;
      candidate.weight_sides[1] = second.weight;
      candidate.residual = 0.0;
      candidate.resolution = edge.parameter_distance;
      if (RefineConstraintRoot(first, second, first_constraint, options, reevaluate, &candidate,
                               &constraint_numerically_incomplete) &&
          globally_complete) {
        candidate.status = FeatureEvidenceStatus::kConfirmed;
      }
      AddCandidate(candidate, options.sky_merge_tolerance, &out);
      crossing_constraints.push_back(first_constraint.name);
      if (!have_first_crossing) {
        first_crossing = candidate;
        have_first_crossing = true;
      }
    }
    if (crossing_constraints.size() >= 2u && have_first_crossing) {
      first_crossing.mechanism = FeatureMechanism::kSupportCorner;
      first_crossing.status = FeatureEvidenceStatus::kCandidate;
      first_crossing.active_constraints = crossing_constraints;
      first_crossing.reason =
          "multiple named support margins cross on one resolved edge; joint root refinement is still required";
      AddCandidate(first_crossing, options.sky_merge_tolerance, &out);
    }
  }

  DiscoverFiniteWidthConcentrations(batch, options, globally_complete, &out, &concentration_resolution_incomplete);
  DiscoverWeightKinks(batch, options, &out, &constraint_numerically_incomplete);

  DiscoverSkyFeatures(batch, options, globally_complete, &out);

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
  if (constraint_numerically_incomplete) {
    for (FeatureMechanism mechanism :
         { FeatureMechanism::kSupportBoundary, FeatureMechanism::kSupportCorner, FeatureMechanism::kOpticalKink,
           FeatureMechanism::kFilterBoundary, FeatureMechanism::kWeightKink }) {
      FeatureMechanismRecord& record = out.mechanisms[static_cast<size_t>(MechanismIndex(mechanism))];
      if (record.candidate_count == 0) {
        record.status = FeatureEvidenceStatus::kNumericalIncomplete;
        record.reason = "at least one named constraint margin was unavailable on the supplied support";
      }
    }
  }
  FeatureMechanismRecord& concentration_record =
      out.mechanisms[static_cast<size_t>(MechanismIndex(FeatureMechanism::kFiniteWidthConcentration))];
  if (concentration_resolution_incomplete && concentration_record.candidate_count == 0) {
    concentration_record.status = FeatureEvidenceStatus::kNumericalIncomplete;
    concentration_record.reason = "finite-width concentration needs at least four positive local measure samples";
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
  return out;
}

}  // namespace lumice::analytic
