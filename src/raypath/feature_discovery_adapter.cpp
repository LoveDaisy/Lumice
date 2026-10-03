#include "raypath/feature_discovery_adapter.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "core/crystal_param.hpp"

namespace lumice::raypath {

namespace {

using analytic::ConstraintKind;
using analytic::FeatureSupportSample;
using analytic::SupportConstraint;

constexpr double kPi = 3.14159265358979323846;
using BranchKey = std::tuple<int, int, int, int>;

BranchKey Key(const analytic::FeatureProvenance& provenance) {
  return { provenance.member_index, provenance.spectrum_node_id, provenance.source_node_id, provenance.sample_index };
}

BranchKey Key(const SceneMeasureRow& row) {
  return { row.member_chain_index, row.spectrum_node_id, row.sun_node_id, row.sample_index };
}

struct ReplayContext {
  ConfigManager config;
  SceneMeasureRequest request;
  std::vector<MeasureFactorDescriptor> factors;
  std::map<BranchKey, SceneMeasureRow> rows;
  int coordinate_dimension = 0;
  std::vector<int> active_coordinates;
  size_t spectrum_node_count = 0;
  size_t sun_node_count = 0;
};

bool HasUsableDirection(const SceneMeasureRow& row) {
  if (row.layers.empty()) {
    return false;
  }
  const double* direction = row.layers.back().outgoing_direction;
  const double norm2 = direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2];
  return std::isfinite(norm2) && norm2 > 0.0 && std::fabs(norm2 - 1.0) <= 1e-5;
}

void AppendMargin(const analytic::DiagnosticMargin& margin, ConstraintKind kind, int layer_index,
                  std::vector<SupportConstraint>* constraints) {
  SupportConstraint out;
  out.name = "layer[" + std::to_string(layer_index) + "]." + margin.name;
  out.kind = kind;
  out.layer_index = layer_index;
  out.interface_index = margin.interface_index;
  out.numerically_available = std::isfinite(margin.value);
  out.value = out.numerically_available ? margin.value : 0.0;
  constraints->push_back(std::move(out));
}

void AppendWeight(const std::string& name, double value, int layer_index, int interface_index,
                  std::vector<SupportConstraint>* constraints) {
  SupportConstraint out;
  out.name = name;
  out.kind = ConstraintKind::kWeight;
  out.layer_index = layer_index;
  out.interface_index = interface_index;
  out.numerically_available = std::isfinite(value) && value >= 0.0;
  out.value = out.numerically_available ? value : 0.0;
  constraints->push_back(std::move(out));
}

void AppendLayerConstraints(const SceneMeasureLayerRow& layer, std::vector<SupportConstraint>* constraints) {
  const std::string prefix = "layer[" + std::to_string(layer.layer_index) + "].";
  SupportConstraint entry;
  entry.name = prefix + "entry_measure";
  entry.kind = ConstraintKind::kEntry;
  entry.layer_index = layer.layer_index;
  entry.value = layer.entry_measure;
  entry.numerically_available = std::isfinite(entry.value);
  constraints->push_back(std::move(entry));
  for (const analytic::DiagnosticMargin& margin : layer.field.domain_margins) {
    AppendMargin(margin, ConstraintKind::kDomain, layer.layer_index, constraints);
  }
  for (const analytic::DiagnosticMargin& margin : layer.field.tir_margins) {
    AppendMargin(margin, ConstraintKind::kTir, layer.layer_index, constraints);
  }
  for (const SceneMeasureEntryRow& scene_entry : layer.entries) {
    if (!scene_entry.filter_evaluated) {
      continue;
    }
    SupportConstraint filter;
    filter.name = prefix + "filter[" + std::to_string(scene_entry.entry_index) + "]";
    filter.kind = ConstraintKind::kFilter;
    filter.layer_index = layer.layer_index;
    filter.value = scene_entry.accepted ? 1.0 : -1.0;
    constraints->push_back(std::move(filter));
  }
  AppendWeight(prefix + "normalized_entry_weight", layer.normalized_entry_factor, layer.layer_index, -1, constraints);
  AppendWeight(prefix + "physical_filter_share", layer.crystal_share, layer.layer_index, -1, constraints);
  for (size_t interface_index = 0; interface_index < layer.field.interfaces.size(); ++interface_index) {
    AppendWeight(prefix + "interface[" + std::to_string(interface_index) + "].fresnel",
                 layer.field.interfaces[interface_index].coefficient, layer.layer_index,
                 static_cast<int>(interface_index), constraints);
  }
}

uint64_t BaseSampleId(const SceneMeasureRow& row) {
  return (static_cast<uint64_t>(static_cast<uint32_t>(row.spectrum_node_id)) << 48u) ^
         (static_cast<uint64_t>(static_cast<uint32_t>(row.sun_node_id)) << 32u) ^
         (static_cast<uint64_t>(static_cast<uint32_t>(row.member_chain_index)) << 20u) ^
         static_cast<uint32_t>(row.sample_index);
}

const MeasureFactorDescriptor* FindFactor(const std::vector<MeasureFactorDescriptor>& factors, int latent_id) {
  const auto found = std::find_if(factors.begin(), factors.end(), [latent_id](const auto& factor) {
    return factor.latent_id == latent_id && factor.support_dimension > 0;
  });
  return found == factors.end() ? nullptr : &*found;
}

double PhysicalCoordinate(const SceneMeasureRow& row, const MeasureFactorDescriptor& factor) {
  if (factor.layer_index >= 0 && factor.layer_index < static_cast<int>(row.layers.size())) {
    const SceneMeasureLayerRow& layer = row.layers[static_cast<size_t>(factor.layer_index)];
    if (factor.name.rfind("shape.", 0) == 0) {
      const auto leader = std::find_if(layer.shape.begin(), layer.shape.end(), [&](const auto& scalar) {
        return scalar.latent_id == factor.latent_id && scalar.leader_slot == scalar.slot;
      });
      if (leader != layer.shape.end()) {
        return leader->raw_value;
      }
      const auto any = std::find_if(layer.shape.begin(), layer.shape.end(),
                                    [&](const auto& scalar) { return scalar.latent_id == factor.latent_id; });
      if (any != layer.shape.end()) {
        return any->raw_value;
      }
    }
    if (factor.name == "pose.azimuth") {
      return layer.pose_lon_lat_roll_rad[0];
    }
    if (factor.name == "pose.latitude") {
      return layer.pose_lon_lat_roll_rad[1];
    }
    if (factor.name == "pose.roll") {
      return layer.pose_lon_lat_roll_rad[2];
    }
  }
  const auto latent = std::find_if(row.latents.begin(), row.latents.end(),
                                   [&](const auto& item) { return item.latent_id == factor.latent_id; });
  return latent == row.latents.end() ? 0.0 : latent->coordinate;
}

int CoordinateIndex(const MeasureFactorDescriptor& factor) {
  if (factor.name == "spectrum") {
    return 0;
  }
  if (factor.name == "sun_disc") {
    return 1;
  }
  return factor.latent_id < 0 ? -1 : 4 + factor.latent_id;
}

std::vector<int> ActiveCoordinates(const SceneMeasureResult& measure) {
  std::vector<int> coordinates;
  for (const MeasureFactorDescriptor& factor : measure.factors) {
    if (factor.support_dimension <= 0) {
      continue;
    }
    if (factor.name == "sun_disc") {
      coordinates.push_back(1);
      coordinates.push_back(2);
    } else {
      const int coordinate = CoordinateIndex(factor);
      if (coordinate >= 0) {
        coordinates.push_back(coordinate);
      }
    }
  }
  std::sort(coordinates.begin(), coordinates.end());
  coordinates.erase(std::unique(coordinates.begin(), coordinates.end()), coordinates.end());
  return coordinates;
}

int CoordinateDimension(const std::vector<int>& active_coordinates) {
  return active_coordinates.empty() ? 4 : std::max(4, active_coordinates.back() + 1);
}

std::vector<double> Coordinates(const SceneMeasureRow& row, const ReplayContext& context) {
  std::vector<double> coordinates(static_cast<size_t>(context.coordinate_dimension), 0.0);
  coordinates[0] = row.wavelength_nm;
  for (int coordinate : context.active_coordinates) {
    if (coordinate < 4) {
      continue;
    }
    const MeasureFactorDescriptor* factor = FindFactor(context.factors, coordinate - 4);
    if (factor != nullptr) {
      coordinates[static_cast<size_t>(coordinate)] = PhysicalCoordinate(row, *factor);
    }
  }
  return coordinates;
}

bool HasExactShapeOnlyConstantDirectionProof(const SceneMeasureRow& row, const ReplayContext& context) {
  if (context.active_coordinates.empty()) {
    return false;
  }
  for (int coordinate : context.active_coordinates) {
    if (coordinate < 4) {
      return false;
    }
    const MeasureFactorDescriptor* factor = FindFactor(context.factors, coordinate - 4);
    if (factor == nullptr || factor->name.rfind("shape.", 0) != 0 || factor->layer_index < 0 ||
        factor->layer_index >= static_cast<int>(row.layers.size()) ||
        row.layers[static_cast<size_t>(factor->layer_index)].analytic_shape.kind !=
            analytic::CrystalShapeKind::kPrism) {
      return false;
    }
  }
  return true;
}

FeatureSupportSample ConvertRow(const SceneMeasureRow& row, const ReplayContext& context,
                                const std::vector<double>& coordinates, bool accumulates_measure) {
  FeatureSupportSample out;
  out.sample_id = BaseSampleId(row);
  out.provenance.member_index = row.member_chain_index;
  out.provenance.spectrum_node_id = row.spectrum_node_id;
  out.provenance.source_node_id = row.sun_node_id;
  out.provenance.sample_index = row.sample_index;
  out.coordinates = coordinates;
  out.active_coordinates = context.active_coordinates;
  out.support_dimension = static_cast<int>(out.active_coordinates.size());
  out.measure_kind =
      out.support_dimension == 0 ? analytic::SupportMeasureKind::kAtom : analytic::SupportMeasureKind::kContinuous;
  out.finite_width = out.support_dimension > 0;
  out.accumulates_measure = accumulates_measure;
  out.weight = row.contribution_status == SceneMeasureNumericStatus::kAvailable ||
                       row.contribution_status == SceneMeasureNumericStatus::kExactZero ?
                   row.contribution :
                   0.0;
  out.numerically_available = HasUsableDirection(row);
  if (out.numerically_available) {
    const double* direction = row.layers.back().outgoing_direction;
    const double norm =
        std::sqrt(direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2]);
    for (int component = 0; component < 3; ++component) {
      out.direction[component] = direction[component] / norm;
    }
  }
  for (const SceneMeasureLayerRow& layer : row.layers) {
    AppendLayerConstraints(layer, &out.constraints);
  }
  AppendWeight("joint.contribution", out.weight, -1, -1, &out.constraints);
  return out;
}

void SetShapeScalar(int slot, double raw_value, bool absolute_value_fold, analytic::CrystalShape* shape) {
  const double value = absolute_value_fold ? std::fabs(raw_value) : raw_value;
  if (shape->kind == analytic::CrystalShapeKind::kPrism) {
    if (slot == kShapeScalarHeight) {
      shape->height = value;
    } else if (slot >= kShapeScalarFace0 && slot < kShapeScalarCount) {
      shape->face_distance[slot - kShapeScalarFace0] = value;
    }
    return;
  }
  if (slot == kShapeScalarUpperH) {
    shape->upper_h = value;
  } else if (slot == kShapeScalarPrismH) {
    shape->height = value;
  } else if (slot == kShapeScalarLowerH) {
    shape->lower_h = value;
  } else if (slot >= kShapeScalarFace0 && slot < kShapeScalarCount) {
    shape->face_distance[slot - kShapeScalarFace0] = value;
  }
}

void TangentBasis(const double direction[3], double first[3], double second[3]) {
  int anchor = 0;
  for (int component = 1; component < 3; ++component) {
    if (std::fabs(direction[component]) < std::fabs(direction[anchor])) {
      anchor = component;
    }
  }
  double unit[3]{};
  unit[anchor] = 1.0;
  first[0] = direction[1] * unit[2] - direction[2] * unit[1];
  first[1] = direction[2] * unit[0] - direction[0] * unit[2];
  first[2] = direction[0] * unit[1] - direction[1] * unit[0];
  const double norm = std::hypot(first[0], std::hypot(first[1], first[2]));
  for (int component = 0; component < 3; ++component) {
    first[component] /= norm;
  }
  second[0] = direction[1] * first[2] - direction[2] * first[1];
  second[1] = direction[2] * first[0] - direction[0] * first[2];
  second[2] = direction[0] * first[1] - direction[1] * first[0];
}

bool BuildReplayState(const SceneMeasureRow& base, const ReplayContext& context, const std::vector<double>& coordinates,
                      SceneMeasureReplayState* state, std::string* error) {
  if (coordinates.size() != static_cast<size_t>(context.coordinate_dimension)) {
    if (error != nullptr) {
      *error = "feature re-evaluation coordinates have the wrong extent";
    }
    return false;
  }
  if (base.layers.empty()) {
    if (error != nullptr) {
      *error = "feature re-evaluation base row has no evaluated layer state";
    }
    return false;
  }
  state->wavelength_nm = coordinates[0];
  state->shapes.reserve(base.layers.size());
  state->poses.reserve(base.layers.size());
  for (const SceneMeasureLayerRow& layer : base.layers) {
    state->shapes.push_back(layer.analytic_shape);
    state->poses.push_back(
        { layer.pose_lon_lat_roll_rad[0], layer.pose_lon_lat_roll_rad[1], layer.pose_lon_lat_roll_rad[2] });
  }
  const double* base_incident = base.layers.front().incident_direction;
  double tangent0[3]{};
  double tangent1[3]{};
  TangentBasis(base_incident, tangent0, tangent1);
  double incident_norm2 = 0.0;
  for (int component = 0; component < 3; ++component) {
    state->incident_direction[component] =
        base_incident[component] + coordinates[1] * tangent0[component] + coordinates[2] * tangent1[component];
    incident_norm2 += state->incident_direction[component] * state->incident_direction[component];
  }
  const double incident_norm = std::sqrt(incident_norm2);
  if (!(incident_norm > 0.0) || !std::isfinite(incident_norm)) {
    if (error != nullptr) {
      *error = "feature re-evaluation produced an invalid incident direction";
    }
    return false;
  }
  for (double& component : state->incident_direction) {
    component /= incident_norm;
  }

  for (const MeasureFactorDescriptor& factor : context.factors) {
    if (factor.support_dimension <= 0 || factor.layer_index < 0 || factor.latent_id < 0) {
      continue;
    }
    const size_t layer_index = static_cast<size_t>(factor.layer_index);
    const int coordinate_index = 4 + factor.latent_id;
    if (layer_index >= base.layers.size() || coordinate_index >= context.coordinate_dimension) {
      continue;
    }
    const double value = coordinates[static_cast<size_t>(coordinate_index)];
    if (factor.name.rfind("shape.", 0) == 0) {
      for (const ShapeScalarSample& scalar : base.layers[layer_index].shape) {
        if (scalar.latent_id == factor.latent_id) {
          SetShapeScalar(scalar.slot, value, scalar.absolute_value_fold, &state->shapes[layer_index]);
        }
      }
    } else if (factor.name == "pose.azimuth") {
      state->poses[layer_index][0] = value;
    } else if (factor.name == "pose.latitude") {
      state->poses[layer_index][1] = value;
    } else if (factor.name == "pose.roll") {
      state->poses[layer_index][2] = value;
    }
  }
  return true;
}

bool Reevaluate(const std::shared_ptr<const ReplayContext>& context,
                const analytic::FeatureReevaluationRequest& request, FeatureSupportSample* sample, std::string* error) {
  const auto found = context->rows.find(Key(request.provenance));
  if (found == context->rows.end()) {
    if (error != nullptr) {
      *error = "feature re-evaluation provenance does not name a materialized product branch";
    }
    return false;
  }
  SceneMeasureReplayState state;
  if (!BuildReplayState(found->second, *context, request.coordinates, &state, error)) {
    return false;
  }
  SceneMeasureRow row;
  const Error replay_error = ReevaluateSceneMeasureRow(context->config, context->request, found->second, state, &row);
  if (!replay_error.Ok()) {
    if (error != nullptr) {
      *error = replay_error.message;
    }
    return false;
  }
  *sample = ConvertRow(row, *context, request.coordinates, false);
  sample->provenance = request.provenance;
  return true;
}

double CoordinateStep(int coordinate, const std::vector<double>& coordinates, const ReplayContext& context) {
  if (coordinate == 0) {
    return std::max(1e-3, 100.0 / std::max<size_t>(1, context.spectrum_node_count));
  }
  if (coordinate == 1 || coordinate == 2) {
    const double radius = context.config.scene_.light_source_.param_.diameter_ * kPi / 360.0;
    return std::max(1e-6, radius / std::max(4.0, std::sqrt(static_cast<double>(context.sun_node_count))));
  }
  const MeasureFactorDescriptor* factor = FindFactor(context.factors, coordinate - 4);
  if (factor == nullptr) {
    return 0.0;
  }
  const double center = coordinates[static_cast<size_t>(coordinate)];
  const bool pose = factor->name.rfind("pose.", 0) == 0;
  const double unit = pose ? kPi / 180.0 : 1.0;
  double step = std::fabs(factor->spread) * unit * 0.025;
  step = std::max(step, (std::fabs(center) + 1.0) * 1e-5);
  if (factor->distribution == DistributionType::kUniform) {
    const double midpoint = factor->center * unit;
    const double half_range = std::fabs(factor->spread * unit) * 0.5;
    const double boundary_distance = half_range - std::fabs(center - midpoint);
    step = std::min(step, 0.5 * std::max(0.0, boundary_distance));
  }
  return std::isfinite(step) ? step : 0.0;
}

bool ReevaluateWithJacobian(const std::shared_ptr<const ReplayContext>& context,
                            const analytic::FeatureReevaluationRequest& request, FeatureSupportSample* sample,
                            std::string* error) {
  if (!Reevaluate(context, request, sample, error)) {
    return false;
  }
  sample->direction_jacobian.assign(3u * static_cast<size_t>(context->coordinate_dimension), 0.0);
  sample->direction_jacobian_column_available.assign(static_cast<size_t>(context->coordinate_dimension), 0);
  const auto row = context->rows.find(Key(request.provenance));
  if (row == context->rows.end()) {
    return false;
  }
  double maximum_error = 0.0;
  double maximum_resolution = 0.0;
  for (int coordinate : sample->active_coordinates) {
    const double step = CoordinateStep(coordinate, request.coordinates, *context);
    if (!(step > 0.0)) {
      if (error != nullptr) {
        *error = "feature re-evaluation cannot construct a finite-difference stencil inside the support";
      }
      return false;
    }
    std::vector<double> lower_coordinates = request.coordinates;
    std::vector<double> upper_coordinates = request.coordinates;
    std::vector<double> lower_fine_coordinates = request.coordinates;
    std::vector<double> upper_fine_coordinates = request.coordinates;
    lower_coordinates[static_cast<size_t>(coordinate)] -= step;
    upper_coordinates[static_cast<size_t>(coordinate)] += step;
    lower_fine_coordinates[static_cast<size_t>(coordinate)] -= 0.5 * step;
    upper_fine_coordinates[static_cast<size_t>(coordinate)] += 0.5 * step;
    FeatureSupportSample lower;
    FeatureSupportSample upper;
    FeatureSupportSample lower_fine;
    FeatureSupportSample upper_fine;
    analytic::FeatureReevaluationRequest probe = request;
    probe.coordinates = lower_coordinates;
    if (!Reevaluate(context, probe, &lower, error)) {
      return false;
    }
    probe.coordinates = upper_coordinates;
    if (!Reevaluate(context, probe, &upper, error)) {
      return false;
    }
    probe.coordinates = lower_fine_coordinates;
    if (!Reevaluate(context, probe, &lower_fine, error)) {
      return false;
    }
    probe.coordinates = upper_fine_coordinates;
    if (!Reevaluate(context, probe, &upper_fine, error) || !sample->numerically_available ||
        !lower.numerically_available || !upper.numerically_available || !lower_fine.numerically_available ||
        !upper_fine.numerically_available) {
      return false;
    }
    double column_error = 0.0;
    for (int component = 0; component < 3; ++component) {
      const double coarse = (upper.direction[component] - lower.direction[component]) / (2.0 * step);
      const double fine = (upper_fine.direction[component] - lower_fine.direction[component]) / step;
      const double correction = (fine - coarse) / 3.0;
      sample->direction_jacobian[static_cast<size_t>(component * context->coordinate_dimension + coordinate)] =
          fine + correction;
      column_error = std::max(column_error, std::fabs(correction));
    }
    sample->direction_jacobian_column_available[static_cast<size_t>(coordinate)] = 1;
    maximum_error = std::max(maximum_error, column_error);
    maximum_resolution = std::max(maximum_resolution, 0.5 * step);
  }
  sample->direction_jacobian_error = maximum_error;
  sample->direction_jacobian_resolution = maximum_resolution;
  sample->direction_jacobian_available =
      sample->numerically_available && !sample->active_coordinates.empty() && maximum_resolution > 0.0;
  return sample->direction_jacobian_available;
}

bool EvaluateAt(const analytic::FeatureReevaluateFn& reevaluate, const FeatureSupportSample& base,
                const std::vector<double>& coordinates, FeatureSupportSample* out) {
  analytic::FeatureReevaluationRequest request;
  request.provenance = base.provenance;
  request.coordinates = coordinates;
  std::string error;
  return reevaluate(request, out, &error);
}

void FillLocalCells(const std::map<BranchKey, SceneMeasureRow>& rows, const ReplayContext& context,
                    const analytic::FeatureReevaluateFn& reevaluate, analytic::FeatureSupportBatch* batch) {
  const size_t original_count = batch->samples.size();
  uint64_t synthetic_id = uint64_t{ 1 } << 63u;
  for (size_t base_index = 0; base_index < original_count; ++base_index) {
    FeatureSupportSample& base = batch->samples[base_index];
    const auto row = rows.find(Key(base.provenance));
    if (row == rows.end()) {
      batch->materialization_complete = false;
      continue;
    }
    base.direction_jacobian.assign(3u * static_cast<size_t>(batch->coordinate_dimension), 0.0);
    base.direction_jacobian_column_available.assign(static_cast<size_t>(batch->coordinate_dimension), 0);
    double maximum_error = 0.0;
    double maximum_resolution = 0.0;
    for (int coordinate : base.active_coordinates) {
      const double step = CoordinateStep(coordinate, base.coordinates, context);
      if (!(step > 0.0)) {
        batch->materialization_complete = false;
        continue;
      }
      std::vector<double> lower_coordinates = base.coordinates;
      std::vector<double> upper_coordinates = base.coordinates;
      lower_coordinates[static_cast<size_t>(coordinate)] -= step;
      upper_coordinates[static_cast<size_t>(coordinate)] += step;
      FeatureSupportSample lower;
      FeatureSupportSample upper;
      if (!EvaluateAt(reevaluate, base, lower_coordinates, &lower) ||
          !EvaluateAt(reevaluate, base, upper_coordinates, &upper)) {
        batch->materialization_complete = false;
        continue;
      }
      lower.sample_id = synthetic_id++;
      upper.sample_id = synthetic_id++;
      const int lower_index = static_cast<int>(batch->samples.size());
      batch->samples.push_back(std::move(lower));
      const int upper_index = static_cast<int>(batch->samples.size());
      batch->samples.push_back(std::move(upper));
      batch->edges.push_back({ lower_index, static_cast<int>(base_index), step });
      batch->edges.push_back({ static_cast<int>(base_index), upper_index, step });
      batch->cell_axes.push_back({ static_cast<int>(base_index), coordinate, lower_index, static_cast<int>(base_index),
                                   upper_index, 2.0 * step });

      std::vector<double> lower_fine_coordinates = base.coordinates;
      std::vector<double> upper_fine_coordinates = base.coordinates;
      lower_fine_coordinates[static_cast<size_t>(coordinate)] -= 0.5 * step;
      upper_fine_coordinates[static_cast<size_t>(coordinate)] += 0.5 * step;
      FeatureSupportSample lower_fine;
      FeatureSupportSample upper_fine;
      if (!EvaluateAt(reevaluate, base, lower_fine_coordinates, &lower_fine) ||
          !EvaluateAt(reevaluate, base, upper_fine_coordinates, &upper_fine) || !base.numerically_available ||
          !batch->samples[static_cast<size_t>(lower_index)].numerically_available ||
          !batch->samples[static_cast<size_t>(upper_index)].numerically_available ||
          !lower_fine.numerically_available || !upper_fine.numerically_available) {
        continue;
      }
      double column_error = 0.0;
      for (int component = 0; component < 3; ++component) {
        const double coarse = (batch->samples[static_cast<size_t>(upper_index)].direction[component] -
                               batch->samples[static_cast<size_t>(lower_index)].direction[component]) /
                              (2.0 * step);
        const double fine = (upper_fine.direction[component] - lower_fine.direction[component]) / step;
        const double correction = (fine - coarse) / 3.0;
        base.direction_jacobian[static_cast<size_t>(component * batch->coordinate_dimension + coordinate)] =
            fine + correction;
        FeatureSupportSample& lower_sample = batch->samples[static_cast<size_t>(lower_index)];
        FeatureSupportSample& upper_sample = batch->samples[static_cast<size_t>(upper_index)];
        if (lower_sample.direction_jacobian.empty()) {
          lower_sample.direction_jacobian.assign(3u * static_cast<size_t>(batch->coordinate_dimension), 0.0);
          lower_sample.direction_jacobian_column_available.assign(static_cast<size_t>(batch->coordinate_dimension), 0);
        }
        if (upper_sample.direction_jacobian.empty()) {
          upper_sample.direction_jacobian.assign(3u * static_cast<size_t>(batch->coordinate_dimension), 0.0);
          upper_sample.direction_jacobian_column_available.assign(static_cast<size_t>(batch->coordinate_dimension), 0);
        }
        lower_sample.direction_jacobian[static_cast<size_t>(component * batch->coordinate_dimension + coordinate)] =
            (-3.0 * lower_sample.direction[component] + 4.0 * lower_fine.direction[component] -
             base.direction[component]) /
            step;
        upper_sample.direction_jacobian[static_cast<size_t>(component * batch->coordinate_dimension + coordinate)] =
            (3.0 * upper_sample.direction[component] - 4.0 * upper_fine.direction[component] +
             base.direction[component]) /
            step;
        column_error = std::max(column_error, std::fabs(correction));
      }
      batch->samples[static_cast<size_t>(lower_index)]
          .direction_jacobian_column_available[static_cast<size_t>(coordinate)] = 1;
      batch->samples[static_cast<size_t>(upper_index)]
          .direction_jacobian_column_available[static_cast<size_t>(coordinate)] = 1;
      base.direction_jacobian_column_available[static_cast<size_t>(coordinate)] = 1;
      maximum_error = std::max(maximum_error, column_error);
      maximum_resolution = std::max(maximum_resolution, 0.5 * step);
    }
    base.direction_jacobian_error = maximum_error;
    base.direction_jacobian_resolution = maximum_resolution;
    base.direction_jacobian_available =
        base.numerically_available && !base.active_coordinates.empty() && maximum_resolution > 0.0 &&
        std::all_of(base.active_coordinates.begin(), base.active_coordinates.end(), [&](int coordinate) {
          return base.direction_jacobian_column_available[static_cast<size_t>(coordinate)] != 0;
        });
  }
}

}  // namespace

Error BuildFeatureSupportBatch(const ConfigManager& config, const SceneMeasureRequest& request,
                               analytic::FeatureSupportBatch* batch, SceneMeasureResult* measure,
                               analytic::FeatureReevaluateFn* reevaluate) {
  *batch = analytic::FeatureSupportBatch{};
  *measure = SceneMeasureResult{};
  std::vector<SceneMeasureRow> rows;
  uint64_t visited = 0;
  size_t evaluation_cost = 0;
  const Error error = BuildSceneMeasure(
      config, request,
      [&](const SceneMeasureRow& row) {
        ++visited;
        const bool continuous_spectrum =
            request.spectrum_source == SceneSpectrumSource::kScene &&
            !std::holds_alternative<std::vector<WlParam>>(config.scene_.light_source_.spectrum_);
        const size_t active_dimension = row.latents.size() +
                                        (config.scene_.light_source_.param_.diameter_ > 0.0f ? 2u : 0u) +
                                        (continuous_spectrum ? 1u : 0u);
        const size_t row_cost = 1u + 4u * active_dimension;
        if (row_cost <= kMaxMaterializedFeatureSupportRows &&
            evaluation_cost <= kMaxMaterializedFeatureSupportRows - row_cost) {
          rows.push_back(row);
          evaluation_cost += row_cost;
        } else {
          batch->materialization_complete = false;
        }
      },
      measure);
  if (!error.Ok()) {
    *batch = analytic::FeatureSupportBatch{};
    return error;
  }

  auto context = std::make_shared<ReplayContext>();
  context->config = config;
  context->request = request;
  context->factors = measure->factors;
  context->active_coordinates = ActiveCoordinates(*measure);
  context->coordinate_dimension = CoordinateDimension(context->active_coordinates);
  context->spectrum_node_count = measure->spectrum_nodes.size();
  context->sun_node_count = measure->sun_nodes.size();
  batch->version = analytic::kFeatureSupportBatchVersion;
  batch->coordinate_dimension = context->coordinate_dimension;
  batch->visited_row_count = visited;
  batch->complete_visit = visited == static_cast<uint64_t>(measure->evaluated_row_count);

  const size_t materialized_per_row = 1u + 2u * context->active_coordinates.size();
  batch->samples.reserve(rows.size() * materialized_per_row);
  for (const SceneMeasureRow& row : rows) {
    context->rows.emplace(Key(row), row);
    const std::vector<double> coordinates = Coordinates(row, *context);
    batch->samples.push_back(ConvertRow(row, *context, coordinates, true));
  }

  const analytic::FeatureReevaluateFn raw_callback =
      [context](const analytic::FeatureReevaluationRequest& request_value, FeatureSupportSample* sample,
                std::string* callback_error) { return Reevaluate(context, request_value, sample, callback_error); };
  FillLocalCells(context->rows, *context, raw_callback, batch);
  std::map<int, std::set<int>> cell_coordinates;
  for (const analytic::FeatureSupportCellAxis& axis : batch->cell_axes) {
    cell_coordinates[axis.center].insert(axis.coordinate_index);
  }
  for (size_t sample_index = 0; sample_index < rows.size(); ++sample_index) {
    FeatureSupportSample& sample = batch->samples[sample_index];
    const std::set<int> active(sample.active_coordinates.begin(), sample.active_coordinates.end());
    const auto axes = cell_coordinates.find(static_cast<int>(sample_index));
    if (axes != cell_coordinates.end() && axes->second == active &&
        HasExactShapeOnlyConstantDirectionProof(rows[sample_index], *context)) {
      sample.mapping_evidence_kind = analytic::MappingEvidenceKind::kExactImageDimensionUpperBound;
      sample.image_dimension_upper_bound = 0;
      sample.mapping_error_bound = 0.0;
    }
  }
  if (reevaluate != nullptr) {
    *reevaluate = [context](const analytic::FeatureReevaluationRequest& request_value, FeatureSupportSample* sample,
                            std::string* callback_error) {
      return ReevaluateWithJacobian(context, request_value, sample, callback_error);
    };
  }
  return {};
}

}  // namespace lumice::raypath
