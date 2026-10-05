#include "raypath/detail/diagnostic_sampler.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "core/shared/lat_path_selection.hpp"
#include "core/shared/pcg_shared.h"

namespace lumice::raypath {
namespace {

// Random-access nested-scrambled Halton coordinates of ONE joint sequence.
// Every prime digit uses a prefix-specific affine permutation. This preserves
// each base's elementary intervals while decorrelating dimensions/scrambles;
// hashing each completed coordinate instead would destroy low discrepancy.
// An independent high-index epoch avoids float-precision prefix repetition.
double Uniform(uint32_t seed, uint64_t sample_index, uint32_t dimension) {
  constexpr uint32_t kPrimes[] = { 2,   3,   5,   7,   11,  13,  17,  19,  23,  29,  31,  37,  41,  43,  47,  53,
                                   59,  61,  67,  71,  73,  79,  83,  89,  97,  101, 103, 107, 109, 113, 127, 131,
                                   137, 139, 149, 151, 157, 163, 167, 173, 179, 181, 191, 193, 197, 199, 211, 223,
                                   227, 229, 233, 239, 241, 251, 257, 263, 269, 271, 277, 281, 283, 293, 307, 311 };
  const uint32_t base = kPrimes[dimension % std::size(kPrimes)];
  uint32_t prefix =
      lm_pcg::pcg_seed_with_high(seed, static_cast<uint32_t>(sample_index >> 32)) ^ lm_pcg::pcg_hash(dimension);
  uint64_t index = static_cast<uint32_t>(sample_index);
  double weight = 1;
  double value = 0;
  while (weight > 0x1p-54) {
    weight /= base;
    const uint32_t digit = index % base;
    index /= base;
    const uint32_t hash = lm_pcg::pcg_hash(prefix);
    const uint32_t multiplier = 1 + hash % (base - 1);
    const uint32_t shift = lm_pcg::pcg_hash(hash) % base;
    value += ((multiplier * digit + shift) % base) * weight;
    prefix = lm_pcg::pcg_hash(prefix ^ (digit + 1) * 1000003u);
  }
  return std::clamp(value, 0x1p-54, 1 - 0x1p-53);
}

float Unit(uint32_t seed, uint64_t sample_index, uint32_t dimension) {
  return std::min(static_cast<float>(Uniform(seed, sample_index, dimension)), std::nextafter(1.f, 0.f));
}

DistributionLatentDraw Latent(const Distribution& distribution, uint32_t seed, uint64_t index, uint32_t offset) {
  const auto plan = BuildDistributionDrawPlan(distribution);
  std::array<double, 2> uniforms{};
  for (uint32_t j = 0; j < plan.uniform_count; ++j) {
    uniforms[j] = Uniform(seed, index, offset + j);
  }
  return TransformDistributionUniforms(plan, uniforms);
}

}  // namespace

DiagnosticSampler::DiagnosticSampler(InputSnapshot snapshot, uint32_t seed, SpectrumRequest spectrum)
    : snapshot_(std::move(snapshot)), seed_(seed), spectrum_(std::move(spectrum)) {
  uint32_t offset = 0;
  const auto add = [&](const std::string& name, uint32_t width) {
    dimensions_.push_back({ name, offset, width });
    offset += width;
  };
  add("source.radial", 1);
  add("source.azimuth", 1);
  for (size_t i = 0; i < snapshot_.layers.size(); ++i) {
    const auto& crystal = snapshot_.layers[i].crystal;
    const auto& axis = crystal.axis_;
    const auto prefix = "layer." + std::to_string(i) + ".";
    const auto path = lat_path::SelectLatPath(axis).kind;
    add(prefix + "latitude",
        path == lat_path::LatPathKind::kFullSphere    ? 1 :
        path == lat_path::LatPathKind::kLutInverseCdf ? 2 :
                                                        BuildDistributionDrawPlan(axis.latitude_dist).uniform_count);
    add(prefix + "azimuth",
        path == lat_path::LatPathKind::kFullSphere ? 1 : BuildDistributionDrawPlan(axis.azimuth_dist).uniform_count);
    add(prefix + "roll", BuildDistributionDrawPlan(axis.roll_dist).uniform_count);
    shape_plans_.push_back(std::visit([](const auto& param) { return BuildShapeDrawPlan(param); }, crystal.param_));
    const auto& plan = shape_plans_.back();
    for (int slot = 0; slot < kShapeScalarCount; ++slot) {
      if (plan[slot].applicable && plan[slot].leader_slot == slot) {
        add(prefix + "shape." + std::to_string(slot), BuildDistributionDrawPlan(plan[slot].distribution).uniform_count);
      }
    }
  }
}

Error DiagnosticSampler::Draw(uint64_t sample_index, AssembledInput* out) const {
  if (!out) {
    return { ErrorCode::kInvalidArgument, "null assembled input output" };
  }
  const SourceSample source{ { Unit(seed_, sample_index, dimensions_[0].offset),
                               Unit(seed_, sample_index, dimensions_[1].offset) },
                             "diagnostic joint source" };
  std::vector<LayerSample> samples;
  size_t cursor = 2;
  for (size_t i = 0; i < snapshot_.layers.size(); ++i) {
    const auto& layer = snapshot_.layers[i];
    const auto& axis = layer.crystal.axis_;
    const auto latitude = dimensions_[cursor++].offset;
    const auto azimuth = dimensions_[cursor++].offset;
    const auto roll = dimensions_[cursor++].offset;
    LayerSample sample;
    sample.identity = { snapshot_.scene_identity, layer.layer_index, layer.crystal.id_ };
    sample.provenance = "joint counter " + std::to_string(sample_index);
    if (axis.IsFullSphereUniform()) {
      sample.axis = FullSphereAxisDraw{ Unit(seed_, sample_index, latitude), Unit(seed_, sample_index, azimuth),
                                        Latent(axis.roll_dist, seed_, sample_index, roll) };
    } else {
      DistributedAxisDraw draw;
      if (lat_path::SelectLatPath(axis).kind == lat_path::LatPathKind::kLutInverseCdf) {
        draw.latitude = LatitudeLutDraw{ Unit(seed_, sample_index, latitude), Unit(seed_, sample_index, latitude + 1) };
      } else {
        draw.latitude = Latent(axis.latitude_dist, seed_, sample_index, latitude);
      }
      draw.azimuth = Latent(axis.azimuth_dist, seed_, sample_index, azimuth);
      draw.roll = Latent(axis.roll_dist, seed_, sample_index, roll);
      sample.axis = draw;
    }
    const auto& plan = shape_plans_[i];
    for (int slot = 0; slot < kShapeScalarCount; ++slot) {
      if (plan[slot].applicable && plan[slot].leader_slot == slot) {
        const auto& distribution = plan[slot].distribution;
        const auto offset = dimensions_[cursor++].offset;
        sample.shape.values[sample.shape.size++] = {
          slot, TransformDistribution(distribution, Latent(distribution, seed_, sample_index, offset))
        };
      }
    }
    samples.push_back(std::move(sample));
  }
  return AssembleInput(snapshot_, samples, source, spectrum_, out);
}

Error BuildDiagnosticMeasure(const DiagnosticSampler& sampler, const SamplingBudget& budget, DiagnosticMeasure* out) {
  if (!out) {
    return { ErrorCode::kInvalidArgument, "null diagnostic measure output" };
  }
  *out = {};
  if (sampler.Snapshot().layers.size() != 1) {
    return { ErrorCode::kMultiLayerUnsupported, "diagnostic measure requires one concrete crystal chain" };
  }
  if (budget.requested_samples == 0) {
    return { ErrorCode::kInvalidArgument, "positive diagnostic sample count required" };
  }
  DiagnosticMeasure result;
  result.run = std::make_shared<const DiagnosticSampler>(sampler);
  for (uint64_t i = 0; i < budget.requested_samples; ++i) {
    if (std::chrono::steady_clock::now() >= budget.deadline ||
        result.optical_evaluations >= budget.max_optical_evaluations) {
      result.budget_exhausted = true;
      break;
    }
    AssembledInput input;
    const auto error = sampler.Draw(i, &input);
    if (!error.Ok()) {
      return error;
    }
    const auto orbit = SingleCrystalIncidentOrbit(input);
    const auto component_begin = result.components.size();
    const auto source_begin = result.sources.size();
    for (size_t member = 0; member < input.layers[0].scope.members.size() && !result.budget_exhausted; ++member) {
      for (size_t spectral = 0; spectral < input.spectrum.rows.size(); ++spectral) {
        if (std::chrono::steady_clock::now() >= budget.deadline ||
            result.optical_evaluations >= budget.max_optical_evaluations) {
          result.budget_exhausted = true;
          break;
        }
        ChainEvaluation value;
        const auto evaluation_error = EvaluateChain(input, { member }, spectral, &value);
        result.optical_evaluations += value.optical_evaluations;
        if (!evaluation_error.Ok()) {
          return evaluation_error;
        }
        if (!(value.optical_weight > 0)) {
          continue;
        }
        analytic::WeightedSkySample component;
        component.sample_index = i;
        component.source_token = result.sources.size();
        component.uniform_orbit_axis = orbit;
        component.xyz_weight = value.xyz;
        for (int j = 0; j < 3; ++j) {
          component.direction[j] = -value.layers[0].outgoing[j];
        }
        result.sources.push_back({ i, member, spectral });
        result.components.push_back(component);
      }
    }
    if (result.budget_exhausted) {
      result.components.resize(component_begin);
      result.sources.resize(source_begin);
      break;
    }
    ++result.completed_samples;
  }
  if (result.completed_samples > 0) {
    for (auto& component : result.components) {
      for (auto& weight : component.xyz_weight) {
        weight /= result.completed_samples;
      }
    }
  }
  *out = std::move(result);
  return {};
}

Error ReplayDiagnosticSource(const DiagnosticMeasure& measure, uint64_t source_token, AssembledInput* out) {
  if (!measure.run || source_token >= measure.sources.size()) {
    return { ErrorCode::kInvalidArgument, "invalid diagnostic source token or missing run" };
  }
  return measure.run->Draw(measure.sources[source_token].sample_index, out);
}

}  // namespace lumice::raypath
