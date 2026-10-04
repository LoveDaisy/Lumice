#include "diagnostic_prototype.h"

#include <algorithm>
#include <chrono>
#include <memory>
#include <vector>

#include "analytic/diagnostic_batch.hpp"
#include "analytic/path_feature_discovery.hpp"

namespace {
namespace a = lumice::analytic;
struct Storage {
  std::vector<DiagnosticPrototypeOptics> optical;
  std::vector<DiagnosticPrototypeFieldPoint> field;
};
a::DiagnosticInputRow Source(const DiagnosticPrototypeSource& row) {
  a::DiagnosticInputRow result;
  result.crystal.kind = static_cast<a::CrystalShapeKind>(row.crystal.kind);
  result.crystal.height = row.crystal.height;
  std::copy_n(row.crystal.face_distance, 6, result.crystal.face_distance);
  result.crystal.upper_h = row.crystal.upper_h;
  result.crystal.lower_h = row.crystal.lower_h;
  result.crystal.upper_wedge_deg = row.crystal.upper_wedge_deg;
  result.crystal.lower_wedge_deg = row.crystal.lower_wedge_deg;
  std::copy_n(row.pose, 9, result.pose.begin());
  std::copy_n(row.incident, 3, result.incident.begin());
  result.refractive_index = row.refractive_index;
  result.source_token = row.token;
  return result;
}
DiagnosticPrototypeOptics Optics(const a::DiagnosticInputRow& source, const a::DiagnosticOutputRow& value,
                                 a::InterfaceSolveStatus status) {
  DiagnosticPrototypeOptics result{};
  result.source.crystal.kind = static_cast<int>(source.crystal.kind);
  result.source.crystal.height = source.crystal.height;
  std::copy_n(source.crystal.face_distance, 6, result.source.crystal.face_distance);
  result.source.crystal.upper_h = source.crystal.upper_h;
  result.source.crystal.lower_h = source.crystal.lower_h;
  result.source.crystal.upper_wedge_deg = source.crystal.upper_wedge_deg;
  result.source.crystal.lower_wedge_deg = source.crystal.lower_wedge_deg;
  std::copy_n(source.pose.begin(), 9, result.source.pose);
  std::copy_n(source.incident.begin(), 3, result.source.incident);
  result.source.refractive_index = source.refractive_index;
  result.source.token = source.source_token;
  result.solve_status = static_cast<int>(status);
  result.input_status = static_cast<int>(value.input_status);
  result.path_valid = value.path_valid;
  result.optical_failure = static_cast<int>(value.optical_failure);
  result.entry_available = value.entry_available;
  result.geometry_evaluated = value.corridor.geometry_evaluated;
  std::copy_n(value.outgoing.begin(), 3, result.outgoing);
  result.area = value.entry.value;
  result.raw_area = value.corridor.raw_area;
  result.area_threshold = value.corridor.area_threshold;
  result.interface_product = value.interface_product;
  for (int j = 0; j < 3; ++j) {
    std::copy_n(value.direction_pose_jacobian[j].begin(), 3, result.direction_pose_jacobian + 3 * j);
  }
  std::copy_n(value.direction_index_derivative.begin(), 3, result.direction_index_derivative);
  result.direction_index_error = value.direction_index_error;
  result.direction_pose_available = value.direction_jacobian_available;
  result.direction_index_available = value.direction_index_available;
  result.interface_count = static_cast<int>(value.interfaces.size());
  for (size_t j = 0; j < value.interfaces.size(); ++j) {
    const auto& from = value.interfaces[j];
    auto& to = result.interfaces[j];
    to.reached = from.reached;
    to.factor_available = from.factor_available;
    to.pose_derivative_available = from.pose_gradient_available;
    to.index_derivative_available = from.index_derivative_available;
    to.incidence = from.incidence;
    to.discriminant = from.discriminant;
    to.factor = from.factor;
    std::copy_n(from.discriminant_pose_gradient.begin(), 3, to.pose_gradient);
    to.index_derivative = from.discriminant_index_derivative;
    to.index_error = from.index_derivative_error;
  }
  return result;
}
void Finish(std::unique_ptr<Storage> storage, DiagnosticPrototypeResult* out) {
  out->optical_count = storage->optical.size();
  out->field_count = storage->field.size();
  out->optical = storage->optical.data();
  out->field = storage->field.data();
  out->storage = storage.release();
}
}  // namespace

int DiagnosticPrototypeEvaluate(const int* faces, int face_count, const DiagnosticPrototypeSource* rows,
                                size_t row_count, DiagnosticPrototypeResult* out) {
  if (!out) {
    return 1;
  }
  *out = {};
  if (!faces || face_count < 2 || face_count > 64 || (row_count && !rows) || row_count > 1000000) {
    return 1;
  }
  try {
    std::vector<a::DiagnosticInputRow> inputs;
    for (size_t i = 0; i < row_count; ++i) {
      inputs.push_back(Source(rows[i]));
    }
    std::vector<a::DiagnosticOutputRow> values;
    if (!a::EvaluateDiagnosticBatch({ faces, faces + face_count }, inputs, { true, true }, &values)) {
      return 1;
    }
    auto storage = std::make_unique<Storage>();
    for (size_t i = 0; i < values.size(); ++i) {
      out->path_evaluations += values[i].path_evaluations;
      storage->optical.push_back(Optics(inputs[i], values[i], a::InterfaceSolveStatus::kUnavailable));
    }
    Finish(std::move(storage), out);
    return 0;
  } catch (...) {
    *out = {};
    return 2;
  }
}
int DiagnosticPrototypeWalkEvent(const int* faces, int face_count, const DiagnosticPrototypeSource* source, int slot,
                                 int reverse, int max_points, uint64_t max_evaluations, int budget_ms,
                                 DiagnosticPrototypeResult* out) {
  if (!out) {
    return 1;
  }
  *out = {};
  if (!faces || face_count < 2 || face_count > 64 || !source || budget_ms <= 0 || budget_ms > 120000) {
    return 1;
  }
  try {
    const auto curve = a::TraceInterfaceCurve({ faces, faces + face_count }, Source(*source), slot, .01, 1e-7,
                                              max_points, reverse != 0, max_evaluations,
                                              std::chrono::steady_clock::now() + std::chrono::milliseconds(budget_ms));
    auto storage = std::make_unique<Storage>();
    for (const auto& point : curve.points) {
      storage->optical.push_back(Optics(point.source, point.value, point.status));
    }
    for (const auto& bracket : curve.events) {
      storage->optical.push_back(Optics(bracket.positive.source, bracket.positive.value, bracket.positive.status));
      storage->optical.push_back(
          Optics(bracket.nonpositive.source, bracket.nonpositive.value, bracket.nonpositive.status));
    }
    out->path_evaluations = curve.path_evaluations;
    out->termination = static_cast<int>(curve.stop);
    Finish(std::move(storage), out);
    return 0;
  } catch (...) {
    *out = {};
    return 2;
  }
}
int DiagnosticPrototypeWalkField(const DiagnosticPrototypeSample* samples, size_t count, const double seed[3],
                                 int equation, double level, double bandwidth_rad, int max_points,
                                 uint64_t max_evaluations, int budget_ms, DiagnosticPrototypeResult* out) {
  if (!out) {
    return 1;
  }
  *out = {};
  if ((count && !samples) || count > 4000000 || !seed || equation < 0 || equation > 3 || budget_ms <= 0 ||
      budget_ms > 120000) {
    return 1;
  }
  try {
    std::vector<a::WeightedSkySample> input;
    for (size_t i = 0; i < count; ++i) {
      a::WeightedSkySample row;
      row.sample_index = samples[i].sample_index;
      row.source_token = samples[i].source_token;
      std::copy_n(samples[i].direction, 3, row.direction.begin());
      std::copy_n(samples[i].xyz_weight, 3, row.xyz_weight.begin());
      if (samples[i].has_orbit) {
        row.uniform_orbit_axis = { samples[i].orbit_axis[0], samples[i].orbit_axis[1], samples[i].orbit_axis[2] };
      }
      input.push_back(row);
    }
    a::FieldWorkBudget budget{ max_evaluations, 0,
                               std::chrono::steady_clock::now() + std::chrono::milliseconds(budget_ms) };
    const auto curve = a::TraceSphericalField(
        input, { seed[0], seed[1], seed[2] },
        { { static_cast<a::FieldEquation>(equation), level, bandwidth_rad, 1e-8, .5 * bandwidth_rad, 32 },
          .5 * bandwidth_rad,
          0,
          max_points },
        false, &budget);
    auto storage = std::make_unique<Storage>();
    for (const auto& point : curve.points) {
      DiagnosticPrototypeFieldPoint row{};
      row.status = static_cast<int>(point.status);
      std::copy_n(point.query.direction.begin(), 3, row.direction);
      for (int j = 0; j < 2; ++j) {
        std::copy_n(point.query.basis[j].begin(), 3, row.tangent_basis + 3 * j);
      }
      row.bandwidth_rad = point.query.bandwidth_rad;
      for (int j = 0; j < 5; ++j) {
        const auto& jet = j < 3 ? point.field.xyz[j] : point.field.xy[j - 3];
        row.jets[6 * j] = jet.value;
        std::copy_n(jet.gradient.begin(), 2, row.jets + 6 * j + 1);
        std::copy_n(jet.hessian.begin(), 3, row.jets + 6 * j + 3);
      }
      row.xy_available = point.field.chromaticity_available;
      row.effective_samples_y = point.field.effective_samples_y;
      row.correction_rad = point.correction_rad;
      std::copy_n(point.log_y_curvatures.begin(), 2, row.log_y_curvatures);
      storage->field.push_back(row);
    }
    out->component_evaluations = budget.component_evaluations;
    out->termination = static_cast<int>(curve.stop);
    Finish(std::move(storage), out);
    return 0;
  } catch (...) {
    *out = {};
    return 2;
  }
}
void DiagnosticPrototypeRelease(DiagnosticPrototypeResult* result) {
  if (!result) {
    return;
  }
  std::unique_ptr<Storage> storage(static_cast<Storage*>(result->storage));
  *result = {};
}
