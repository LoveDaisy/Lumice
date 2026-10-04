#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <memory>
#include <vector>

#include "analytic/diagnostic_batch.hpp"
#include "analytic/path_feature_discovery.hpp"
#include "lumice_analytic_core.h"

namespace {
namespace a = lumice::analytic;
struct Storage {
  std::vector<LUMICE_ANALYTIC_DiagnosticOptics> optical;
  std::vector<LUMICE_ANALYTIC_SkyFieldPoint> field;
  std::vector<std::vector<LUMICE_ANALYTIC_DiagnosticInterface>> interfaces;
  std::vector<LUMICE_ANALYTIC_SourceEventRange> events;
  std::vector<std::vector<double>> vertices;
  std::vector<std::vector<int>> edges;
};
a::DiagnosticInputRow Source(const LUMICE_ANALYTIC_DiagnosticSource& row) {
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
LUMICE_ANALYTIC_DiagnosticOptics Optics(const a::DiagnosticInputRow& source, const a::DiagnosticOutputRow& value,
                                        a::InterfaceSolveStatus status, Storage* storage) {
  LUMICE_ANALYTIC_DiagnosticOptics result{};
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
  storage->interfaces.emplace_back(value.interfaces.size());
  result.interfaces = storage->interfaces.back().data();
  for (size_t j = 0; j < value.interfaces.size(); ++j) {
    const auto& from = value.interfaces[j];
    auto& to = storage->interfaces.back()[j];
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
  storage->vertices.emplace_back();
  storage->edges.emplace_back();
  for (const auto& point : value.corridor.vertices) {
    storage->vertices.back().insert(storage->vertices.back().end(), point.begin(), point.end());
  }
  for (const auto& edge : value.corridor.edge_sources) {
    storage->edges.back().push_back(edge.path_index);
    storage->edges.back().push_back(edge.edge_index);
  }
  result.corridor_vertex_count = value.corridor.vertices.size();
  result.corridor_vertices = storage->vertices.back().data();
  result.corridor_edge_sources = storage->edges.back().data();
  for (int j = 0; j < 2; ++j) {
    std::copy_n(value.corridor.projection_basis[j].begin(), 3, result.corridor_basis + 3 * j);
  }
  return result;
}
void Finish(std::unique_ptr<Storage> storage, LUMICE_ANALYTIC_DiagnosticResult* out) {
  out->optical_count = storage->optical.size();
  out->field_count = storage->field.size();
  out->optical = storage->optical.data();
  out->field = storage->field.data();
  out->source_event_count = storage->events.size();
  out->source_events = storage->events.data();
  out->storage = storage.release();
}
}  // namespace

static int EvaluateDiagnosticBatchImpl(const int* faces, int face_count, const LUMICE_ANALYTIC_DiagnosticSource* rows,
                                       size_t row_count, LUMICE_ANALYTIC_DiagnosticResult* out) {
  if (!out) {
    return 1;
  }
  *out = {};
  if (!faces || (row_count && !rows)) {
    return 3;
  }
  if (!a::ValidateDiagnosticPath(faces, face_count) || row_count > 1000000) {
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
      storage->optical.push_back(Optics(inputs[i], values[i], a::InterfaceSolveStatus::kUnavailable, storage.get()));
    }
    Finish(std::move(storage), out);
    return 0;
  } catch (...) {
    *out = {};
    return 2;
  }
}
static int TraceDiagnosticInterfaceImpl(const int* faces, int face_count,
                                        const LUMICE_ANALYTIC_DiagnosticSource* source, int slot, int reverse,
                                        int max_points, uint64_t max_evaluations, int budget_ms,
                                        LUMICE_ANALYTIC_DiagnosticResult* out) {
  if (!out) {
    return 1;
  }
  *out = {};
  if (!faces || !source) {
    return 3;
  }
  if (!a::ValidateDiagnosticPath(faces, face_count) || slot <= 0 || slot >= face_count - 1 || max_points < 2 ||
      max_points > 4096 || budget_ms <= 0 || budget_ms > 120000) {
    return 1;
  }
  try {
    const auto curve = a::TraceInterfaceCurve({ faces, faces + face_count }, Source(*source), slot, .01, 1e-7,
                                              max_points, reverse != 0, max_evaluations,
                                              std::chrono::steady_clock::now() + std::chrono::milliseconds(budget_ms));
    auto storage = std::make_unique<Storage>();
    for (const auto& point : curve.points) {
      storage->optical.push_back(Optics(point.source, point.value, point.status, storage.get()));
    }
    out->curve_point_count = storage->optical.size();
    for (const auto& bracket : curve.events) {
      const size_t first = storage->optical.size();
      storage->events.push_back({ static_cast<int>(bracket.kind), first, first + 1, bracket.source_width_rad });
      storage->optical.push_back(
          Optics(bracket.positive.source, bracket.positive.value, bracket.positive.status, storage.get()));
      storage->optical.push_back(
          Optics(bracket.nonpositive.source, bracket.nonpositive.value, bracket.nonpositive.status, storage.get()));
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
static int TraceWeightedSkyFieldImpl(const LUMICE_ANALYTIC_WeightedSkySample* samples, size_t count,
                                     const double seed[3], int equation, double level, double bandwidth_rad,
                                     int max_points, uint64_t max_evaluations, int budget_ms,
                                     LUMICE_ANALYTIC_DiagnosticResult* out) {
  if (!out) {
    return 1;
  }
  *out = {};
  if ((count && !samples) || !seed) {
    return 3;
  }
  if (count > 4000000 || equation < 0 || equation > 3 || budget_ms <= 0 || budget_ms > 120000 || max_points <= 0 ||
      max_points > 4096 || !(bandwidth_rad > 0) || !std::isfinite(bandwidth_rad) || !std::isfinite(level) ||
      !a::ValidateUnitVector(seed)) {
    return 1;
  }
  try {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(budget_ms);
    std::vector<a::WeightedSkySample> input;
    input.reserve(count);
    for (size_t i = 0; i < count; ++i) {
      if ((i % 1024) == 0 && std::chrono::steady_clock::now() >= deadline) {
        out->termination = 5;
        return 0;
      }
      if (!a::ValidateUnitVector(samples[i].direction) ||
          (i > 0 && samples[i].sample_index < samples[i - 1].sample_index) ||
          (samples[i].has_orbit && !a::ValidateUnitVector(samples[i].orbit_axis)) ||
          !std::all_of(samples[i].xyz_weight, samples[i].xyz_weight + 3,
                       [](double x) { return std::isfinite(x) && x >= 0; })) {
        return 1;
      }
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
    a::FieldWorkBudget budget{ max_evaluations, 0, deadline };
    const a::FieldSolveOptions solve{
      static_cast<a::FieldEquation>(equation), level, bandwidth_rad, 1e-8, .5 * bandwidth_rad, 32
    };
    a::FieldCurve curve;
    if (equation == 0) {
      auto point = a::CorrectSphericalField(input, { seed[0], seed[1], seed[2] }, solve, &budget);
      curve.stop = point.status == a::FieldSolveStatus::kConverged ? a::FieldWalkStop::kPointLimit :
                                                                     a::FieldWalkStop::kCorrectorFailed;
      if (point.status == a::FieldSolveStatus::kConverged) {
        curve.points.push_back(std::move(point));
      }
    } else {
      curve = a::TraceSphericalField(input, { seed[0], seed[1], seed[2] }, { solve, .5 * bandwidth_rad, 0, max_points },
                                     false, &budget);
    }
    auto storage = std::make_unique<Storage>();
    for (const auto& point : curve.points) {
      LUMICE_ANALYTIC_SkyFieldPoint row{};
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
    out->termination = budget.exhausted ? 5 : static_cast<int>(curve.stop);
    Finish(std::move(storage), out);
    return 0;
  } catch (...) {
    *out = {};
    return 2;
  }
}

namespace {
constexpr size_t kDiagnosticBaseSize = offsetof(LUMICE_ANALYTIC_DiagnosticResult, curve_point_count);
void Clear(LUMICE_ANALYTIC_DiagnosticResult* out) {
  const auto end = std::min<size_t>(out->struct_size, sizeof(*out));
  if (end > sizeof(out->struct_size)) {
    std::memset(reinterpret_cast<char*>(out) + sizeof(out->struct_size), 0, end - sizeof(out->struct_size));
  }
}
template <class F>
LUMICE_ANALYTIC_ErrorCode Invoke(LUMICE_ANALYTIC_DiagnosticResult* out, F call) {
  if (!out) {
    return LUMICE_ANALYTIC_ERR_NULL_ARG;
  }
  const auto size = out->struct_size;
  Clear(out);
  if (size < kDiagnosticBaseSize) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  LUMICE_ANALYTIC_DiagnosticResult full{};
  const int status = call(&full);
  if (status) {
    return status == 3 ? LUMICE_ANALYTIC_ERR_NULL_ARG :
           status == 1 ? LUMICE_ANALYTIC_ERR_INVALID_VALUE :
                         LUMICE_ANALYTIC_ERR_UNKNOWN;
  }
  const size_t copied = size >= sizeof(full) ? sizeof(full) : kDiagnosticBaseSize;
  std::memcpy(reinterpret_cast<char*>(out) + sizeof(size), reinterpret_cast<const char*>(&full) + sizeof(size),
              copied - sizeof(size));
  return LUMICE_ANALYTIC_OK;
}
}  // namespace
LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_EvaluateDiagnosticBatch(const int* faces, int face_count,
                                                                  const LUMICE_ANALYTIC_DiagnosticSource* rows,
                                                                  size_t count, LUMICE_ANALYTIC_DiagnosticResult* out) {
  return Invoke(out, [&](auto* full) { return EvaluateDiagnosticBatchImpl(faces, face_count, rows, count, full); });
}
LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_TraceDiagnosticInterface(const int* faces, int face_count,
                                                                   const LUMICE_ANALYTIC_DiagnosticSource* source,
                                                                   int slot, int reverse, int max_points,
                                                                   uint64_t max_evaluations, int budget_ms,
                                                                   LUMICE_ANALYTIC_DiagnosticResult* out) {
  return Invoke(out, [&](auto* full) {
    return TraceDiagnosticInterfaceImpl(faces, face_count, source, slot, reverse, max_points, max_evaluations,
                                        budget_ms, full);
  });
}
LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_TraceWeightedSkyField(const LUMICE_ANALYTIC_WeightedSkySample* samples,
                                                                size_t count, const double seed[3], int equation,
                                                                double level, double bandwidth_rad, int max_points,
                                                                uint64_t max_evaluations, int budget_ms,
                                                                LUMICE_ANALYTIC_DiagnosticResult* out) {
  return Invoke(out, [&](auto* full) {
    return TraceWeightedSkyFieldImpl(samples, count, seed, equation, level, bandwidth_rad, max_points, max_evaluations,
                                     budget_ms, full);
  });
}
void LUMICE_ANALYTIC_ReleaseDiagnosticResult(LUMICE_ANALYTIC_DiagnosticResult* result) {
  if (!result || result->struct_size < kDiagnosticBaseSize) {
    return;
  }
  std::unique_ptr<Storage> storage(static_cast<Storage*>(result->storage));
  Clear(result);
}
LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_CorrectDeviationBatch(const int* faces, int face_count,
                                                                const LUMICE_ANALYTIC_DiagnosticSource* rows,
                                                                size_t count, uint64_t max_evaluations, int budget_ms,
                                                                LUMICE_ANALYTIC_DiagnosticResult* out) {
  return Invoke(out, [&](auto* full) {
    if (!a::ValidateDiagnosticPath(faces, face_count) || (count && !rows) || count > 1000000 || budget_ms <= 0 ||
        budget_ms > 120000) {
      return 1;
    }
    try {
      auto storage = std::make_unique<Storage>();
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(budget_ms);
      const std::vector<int> path(faces, faces + face_count);
      for (size_t i = 0; i < count; ++i) {
        if (full->path_evaluations >= max_evaluations || std::chrono::steady_clock::now() >= deadline) {
          full->termination = 6;
          break;
        }
        const auto point = a::CorrectDeviationMinimum(
            path, Source(rows[i]), {}, max_evaluations - std::min(max_evaluations, full->path_evaluations), deadline);
        full->path_evaluations += point.path_evaluations;
        auto row = Optics(point.source, point.value, point.status, storage.get());
        row.deviation_available = point.deviation_available;
        row.deviation_rad = point.deviation_rad;
        row.correction_rad = point.correction_rad;
        std::copy_n(point.objective_curvatures.begin(), 2, row.objective_curvatures);
        row.hessian_error = point.hessian_error;
        storage->optical.push_back(row);
        if (point.status == a::InterfaceSolveStatus::kBudgetExceeded) {
          full->termination = 6;
          break;
        }
      }
      Finish(std::move(storage), full);
      return 0;
    } catch (...) {
      *full = {};
      return 2;
    }
  });
}
