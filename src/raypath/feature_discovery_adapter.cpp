#include "raypath/feature_discovery_adapter.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "core/trace_ops.hpp"
#include "raypath/scene_to_analytic.hpp"

namespace lumice::raypath {

namespace {

using analytic::ConstraintKind;
using analytic::FeatureSupportSample;
using analytic::SupportConstraint;

bool HasUsableDirection(const SceneMeasureRow& row) {
  if (row.layers.empty()) {
    return false;
  }
  const double* direction = row.layers.back().outgoing_direction;
  const double norm2 = direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2];
  return std::isfinite(norm2) && norm2 > 0.0 && std::fabs(norm2 - 1.0) <= 1e-5;
}

bool EvaluateFullChainDirection(const SceneMeasureRow& row, int perturbed_layer, int pose_coordinate, double offset,
                                double direction[3]) {
  if (row.layers.empty()) {
    return false;
  }
  double incident[3] = { row.layers.front().incident_direction[0], row.layers.front().incident_direction[1],
                         row.layers.front().incident_direction[2] };
  for (const SceneMeasureLayerRow& layer : row.layers) {
    analytic::FaceNormalTable normals;
    analytic::FacePolygonTable polygons;
    if (analytic::BuildFaceNormals(layer.analytic_shape, &normals, &polygons) != analytic::Status::kOk) {
      return false;
    }
    std::vector<int> slots;
    if (!ResolveDiagnosticLayerPath(layer.faces, normals, &slots).Ok()) {
      return false;
    }
    double pose[9];
    std::copy(layer.pose, layer.pose + 9, pose);
    if (layer.layer_index == perturbed_layer) {
      float coordinates[3] = { static_cast<float>(layer.pose_lon_lat_roll_rad[0]),
                               static_cast<float>(layer.pose_lon_lat_roll_rad[1]),
                               static_cast<float>(layer.pose_lon_lat_roll_rad[2]) };
      coordinates[pose_coordinate] += static_cast<float>(offset);
      const Rotation rotation = BuildCrystalRotation(coordinates[0], coordinates[1], coordinates[2]);
      std::copy(rotation.GetMat(), rotation.GetMat() + 9, pose);
    }
    analytic::DiagnosticRowInput input;
    input.refractive_index = layer.refractive_index;
    std::copy(incident, incident + 3, input.incident_direction);
    std::copy(pose, pose + 9, input.pose);
    analytic::DiagnosticField field(normals, polygons, layer.faces.data(), slots.data(),
                                    static_cast<int>(layer.faces.size()));
    const analytic::DiagnosticFieldResult result = field.EvaluateWithoutDerivatives(input);
    if (result.path_status != analytic::DiagnosticPathStatus::kOk ||
        result.entry_status != analytic::DiagnosticEntryStatus::kOk) {
      return false;
    }
    std::copy(result.outgoing_direction, result.outgoing_direction + 3, incident);
  }
  const double norm = std::sqrt(incident[0] * incident[0] + incident[1] * incident[1] + incident[2] * incident[2]);
  if (!std::isfinite(norm) || !(norm > 0.0)) {
    return false;
  }
  for (int component = 0; component < 3; ++component) {
    direction[component] = incident[component] / norm;
  }
  return true;
}

int PoseCoordinate(const std::string& name) {
  if (name == "pose.azimuth") {
    return 0;
  }
  if (name == "pose.latitude") {
    return 1;
  }
  if (name == "pose.roll") {
    return 2;
  }
  return -1;
}

void FillFullChainPoseJacobian(const SceneMeasureRow& row, FeatureSupportSample* sample) {
  constexpr double kCoarseStep = 2e-3;
  const size_t dimension = sample->coordinates.size();
  sample->direction_jacobian.assign(3u * dimension, 0.0);
  sample->direction_jacobian_column_available.assign(dimension, 0);
  double maximum_error = 0.0;
  for (const LatentMeasureSample& latent : row.latents) {
    const int pose_coordinate = PoseCoordinate(latent.name);
    if (latent.latent_id < 0 || pose_coordinate < 0 || latent.layer_index < 0 ||
        latent.layer_index >= static_cast<int>(row.layers.size()) ||
        row.layers[static_cast<size_t>(latent.layer_index)].pose_support_rank < 0) {
      continue;
    }
    const size_t coordinate = 4u + static_cast<size_t>(latent.latent_id);
    if (coordinate >= dimension) {
      continue;
    }
    double plus_coarse[3]{};
    double minus_coarse[3]{};
    double plus_fine[3]{};
    double minus_fine[3]{};
    if (!EvaluateFullChainDirection(row, latent.layer_index, pose_coordinate, kCoarseStep, plus_coarse) ||
        !EvaluateFullChainDirection(row, latent.layer_index, pose_coordinate, -kCoarseStep, minus_coarse) ||
        !EvaluateFullChainDirection(row, latent.layer_index, pose_coordinate, 0.5 * kCoarseStep, plus_fine) ||
        !EvaluateFullChainDirection(row, latent.layer_index, pose_coordinate, -0.5 * kCoarseStep, minus_fine)) {
      continue;
    }
    double column_error = 0.0;
    for (int component = 0; component < 3; ++component) {
      const double coarse = (plus_coarse[component] - minus_coarse[component]) / (2.0 * kCoarseStep);
      const double fine = (plus_fine[component] - minus_fine[component]) / kCoarseStep;
      const double correction = (fine - coarse) / 3.0;
      sample->direction_jacobian[static_cast<size_t>(component) * dimension + coordinate] = fine + correction;
      column_error = std::max(column_error, std::fabs(correction));
    }
    maximum_error = std::max(maximum_error, column_error);
    sample->direction_jacobian_column_available[coordinate] = 1;
  }
  sample->direction_jacobian_error = maximum_error;
  sample->direction_jacobian_resolution = 0.5 * kCoarseStep;
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

void AppendLayerConstraints(const SceneMeasureLayerRow& layer, std::vector<SupportConstraint>* constraints) {
  SupportConstraint entry;
  entry.name = "layer[" + std::to_string(layer.layer_index) + "].entry_measure";
  entry.kind = ConstraintKind::kEntry;
  entry.layer_index = layer.layer_index;
  entry.value = layer.entry_measure;
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
    filter.name =
        "layer[" + std::to_string(layer.layer_index) + "].filter[" + std::to_string(scene_entry.entry_index) + "]";
    filter.kind = ConstraintKind::kFilter;
    filter.layer_index = layer.layer_index;
    filter.value = scene_entry.accepted ? 1.0 : -1.0;
    constraints->push_back(std::move(filter));
  }
}

FeatureSupportSample ConvertRow(const SceneMeasureRow& row) {
  FeatureSupportSample out;
  out.sample_id = (static_cast<uint64_t>(static_cast<uint32_t>(row.spectrum_node_id)) << 48u) ^
                  (static_cast<uint64_t>(static_cast<uint32_t>(row.sun_node_id)) << 32u) ^
                  (static_cast<uint64_t>(static_cast<uint32_t>(row.member_chain_index)) << 20u) ^
                  static_cast<uint32_t>(row.sample_index);
  out.provenance.member_index = row.member_chain_index;
  out.provenance.spectrum_node_id = row.spectrum_node_id;
  out.provenance.source_node_id = row.sun_node_id;
  out.provenance.sample_index = row.sample_index;
  out.coordinates.resize(4);
  out.coordinates[0] = row.wavelength_nm;
  if (!row.layers.empty()) {
    const double* incident = row.layers.front().incident_direction;
    std::copy(incident, incident + 3, out.coordinates.begin() + 1);
  }
  for (const LatentMeasureSample& latent : row.latents) {
    if (latent.latent_id < 0) {
      continue;
    }
    const size_t coordinate = 4u + static_cast<size_t>(latent.latent_id);
    if (out.coordinates.size() <= coordinate) {
      out.coordinates.resize(coordinate + 1u);
    }
    out.coordinates[coordinate] = latent.coordinate;
  }
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
  if (out.numerically_available) {
    FillFullChainPoseJacobian(row, &out);
  }
  return out;
}

std::vector<int> ActiveCoordinates(const SceneMeasureResult& measure) {
  std::vector<int> coordinates;
  for (const MeasureFactorDescriptor& factor : measure.factors) {
    if (factor.support_dimension <= 0) {
      continue;
    }
    if (factor.name == "spectrum") {
      coordinates.push_back(0);
    } else if (factor.name == "sun_disc") {
      coordinates.push_back(1);
      coordinates.push_back(2);
    } else if (factor.latent_id >= 0) {
      coordinates.push_back(4 + factor.latent_id);
    }
  }
  std::sort(coordinates.begin(), coordinates.end());
  coordinates.erase(std::unique(coordinates.begin(), coordinates.end()), coordinates.end());
  return coordinates;
}

bool HasFiniteWidth(const SceneMeasureResult& measure) {
  return std::any_of(measure.factors.begin(), measure.factors.end(), [](const MeasureFactorDescriptor& factor) {
    return factor.support_dimension > 0 && factor.spread > 0.0;
  });
}

void BuildTraversalEdges(analytic::FeatureSupportBatch* batch) {
  using Branch = std::tuple<int, int, int>;
  std::map<Branch, std::vector<int>> branches;
  for (size_t index = 0; index < batch->samples.size(); ++index) {
    const auto& provenance = batch->samples[index].provenance;
    branches[{ provenance.member_index, provenance.spectrum_node_id, provenance.source_node_id }].push_back(
        static_cast<int>(index));
  }
  for (auto& [branch, indices] : branches) {
    (void)branch;
    std::sort(indices.begin(), indices.end(), [&](int first, int second) {
      return batch->samples[first].provenance.sample_index < batch->samples[second].provenance.sample_index;
    });
    for (size_t index = 1; index < indices.size(); ++index) {
      const auto& first = batch->samples[indices[index - 1]].coordinates;
      const auto& second = batch->samples[indices[index]].coordinates;
      double distance2 = 0.0;
      for (size_t coordinate = 0; coordinate < first.size(); ++coordinate) {
        const double delta = first[coordinate] - second[coordinate];
        distance2 += delta * delta;
      }
      const double distance = std::sqrt(distance2);
      if (distance > 0.0 && std::isfinite(distance)) {
        batch->edges.push_back({ indices[index - 1], indices[index], distance });
      }
    }
  }
}

}  // namespace

Error BuildFeatureSupportBatch(const ConfigManager& config, const SceneMeasureRequest& request,
                               analytic::FeatureSupportBatch* batch, SceneMeasureResult* measure) {
  *batch = analytic::FeatureSupportBatch{};
  *measure = SceneMeasureResult{};
  uint64_t visited = 0;
  const Error error = BuildSceneMeasure(
      config, request,
      [&](const SceneMeasureRow& row) {
        ++visited;
        if (batch->samples.size() < kMaxMaterializedFeatureSupportRows) {
          batch->samples.push_back(ConvertRow(row));
        } else {
          batch->materialization_complete = false;
        }
      },
      measure);
  if (!error.Ok()) {
    *batch = analytic::FeatureSupportBatch{};
    return error;
  }
  batch->visited_row_count = visited;
  batch->complete_visit = visited == static_cast<uint64_t>(measure->evaluated_row_count);
  size_t coordinate_dimension = 4;
  for (const FeatureSupportSample& sample : batch->samples) {
    coordinate_dimension = std::max(coordinate_dimension, sample.coordinates.size());
  }
  batch->coordinate_dimension = static_cast<int>(coordinate_dimension);
  const std::vector<int> active_coordinates = ActiveCoordinates(*measure);
  const int support_dimension = static_cast<int>(active_coordinates.size());
  const bool finite_width = HasFiniteWidth(*measure);
  for (FeatureSupportSample& sample : batch->samples) {
    sample.coordinates.resize(coordinate_dimension);
    if (!sample.direction_jacobian.empty()) {
      const size_t previous_dimension = sample.direction_jacobian_column_available.size();
      std::vector<double> resized(3u * coordinate_dimension, 0.0);
      for (int component = 0; component < 3; ++component) {
        std::copy_n(sample.direction_jacobian.begin() + static_cast<std::ptrdiff_t>(component * previous_dimension),
                    previous_dimension,
                    resized.begin() + static_cast<std::ptrdiff_t>(component * coordinate_dimension));
      }
      sample.direction_jacobian = std::move(resized);
      sample.direction_jacobian_column_available.resize(coordinate_dimension);
    }
    sample.support_dimension = support_dimension;
    sample.active_coordinates = active_coordinates;
    sample.measure_kind =
        support_dimension == 0 ? analytic::SupportMeasureKind::kAtom : analytic::SupportMeasureKind::kContinuous;
    sample.finite_width = finite_width;
    sample.direction_jacobian_available =
        sample.numerically_available && !sample.direction_jacobian.empty() &&
        std::all_of(active_coordinates.begin(), active_coordinates.end(), [&](int coordinate) {
          return sample.direction_jacobian_column_available[static_cast<size_t>(coordinate)] != 0;
        });
  }
  BuildTraversalEdges(batch);
  return {};
}

}  // namespace lumice::raypath
