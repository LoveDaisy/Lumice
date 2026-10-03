#include "raypath/feature_discovery_adapter.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

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
  return std::isfinite(norm2) && std::fabs(norm2 - 1.0) <= 1e-10;
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
    std::copy(row.layers.back().outgoing_direction, row.layers.back().outgoing_direction + 3, out.direction);
  }
  for (const SceneMeasureLayerRow& layer : row.layers) {
    AppendLayerConstraints(layer, &out.constraints);
  }
  return out;
}

int SupportDimension(const SceneMeasureResult& measure) {
  int dimension = 0;
  for (const MeasureFactorDescriptor& factor : measure.factors) {
    dimension += factor.support_dimension;
  }
  return dimension;
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
  const int support_dimension = SupportDimension(*measure);
  const bool finite_width = HasFiniteWidth(*measure);
  for (FeatureSupportSample& sample : batch->samples) {
    sample.coordinates.resize(coordinate_dimension);
    sample.support_dimension = support_dimension;
    sample.measure_kind =
        support_dimension == 0 ? analytic::SupportMeasureKind::kAtom : analytic::SupportMeasureKind::kContinuous;
    sample.finite_width = finite_width;
  }
  BuildTraversalEdges(batch);
  return {};
}

}  // namespace lumice::raypath
