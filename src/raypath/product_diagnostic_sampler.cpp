#include "raypath/product_diagnostic_sampler.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "core/shared/lat_path_selection.hpp"
#include "core/shared/pcg_shared.h"

namespace lumice::raypath {
namespace {

uint32_t DrawWidth(const Distribution& distribution) {
  if (distribution.type == DistributionType::kNoRandom || distribution.spread == 0) {
    return 0;
  }
  return distribution.type == DistributionType::kGaussian || distribution.type == DistributionType::kGaussianLegacy ?
             2 :
             1;
}

// Open double uniform from a full-width hash, before conversion to the product's
// float latents. No artificial Gaussian tail clamp. The finite PRNG resolution
// remains explicit, as with every product random stream.
double Uniform(uint32_t seed, uint64_t sample_index, uint32_t dimension) {
  const auto mixed = lm_pcg::pcg_seed_with_high(seed, static_cast<uint32_t>(sample_index >> 32));
  const auto hash =
      lm_pcg::pcg_hash(mixed ^ lm_pcg::pcg_hash(static_cast<uint32_t>(sample_index) * 1000003u + dimension));
  return (static_cast<double>(hash) + .5) / 4294967296.0;
}

float Unit(uint32_t seed, uint64_t sample_index, uint32_t dimension) {
  return std::min(static_cast<float>(Uniform(seed, sample_index, dimension)), std::nextafter(1.f, 0.f));
}

DistributionLatentDraw Latent(const Distribution& distribution, uint32_t seed, uint64_t index, uint32_t offset) {
  if (DrawWidth(distribution) == 0) {
    return { distribution.type == DistributionType::kGaussian ||
                     distribution.type == DistributionType::kGaussianLegacy ?
                 0.f :
                 .5f };
  }
  if (DrawWidth(distribution) == 2) {
    return { static_cast<float>(std::sqrt(-2 * std::log(Uniform(seed, index, offset))) *
                                std::cos(2 * 3.14159265358979323846 * Uniform(seed, index, offset + 1))) };
  }
  return { Unit(seed, index, offset) };
}

}  // namespace

ProductDiagnosticSampler::ProductDiagnosticSampler(ProductInputSnapshot snapshot, uint32_t seed)
    : snapshot_(std::move(snapshot)), seed_(seed) {
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
    add(prefix + "latitude", path == lat_path::LatPathKind::kFullSphere    ? 1 :
                             path == lat_path::LatPathKind::kLutInverseCdf ? 2 :
                                                                             DrawWidth(axis.latitude_dist));
    add(prefix + "azimuth", path == lat_path::LatPathKind::kFullSphere ? 1 : DrawWidth(axis.azimuth_dist));
    add(prefix + "roll", DrawWidth(axis.roll_dist));
    shape_plans_.push_back(std::visit([](const auto& param) { return BuildShapeDrawPlan(param); }, crystal.param_));
    const auto& plan = shape_plans_.back();
    for (int slot = 0; slot < kShapeScalarCount; ++slot) {
      if (plan[slot].applicable && plan[slot].leader_slot == slot) {
        add(prefix + "shape." + std::to_string(slot), DrawWidth(plan[slot].distribution));
      }
    }
  }
}

Error ProductDiagnosticSampler::Draw(uint64_t sample_index, const ProductSpectrumRequest& spectrum,
                                     ProductInput* out) const {
  if (!out) {
    return { ErrorCode::kInvalidArgument, "null product sample output" };
  }
  const ProductSourceSample source{ { Unit(seed_, sample_index, dimensions_[0].offset),
                                      Unit(seed_, sample_index, dimensions_[1].offset) },
                                    "diagnostic joint source" };
  std::vector<ProductLayerSample> samples;
  size_t cursor = 2;
  for (size_t i = 0; i < snapshot_.layers.size(); ++i) {
    const auto& layer = snapshot_.layers[i];
    const auto& axis = layer.crystal.axis_;
    const auto latitude = dimensions_[cursor++].offset;
    const auto azimuth = dimensions_[cursor++].offset;
    const auto roll = dimensions_[cursor++].offset;
    ProductLayerSample sample;
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
  return AssembleProductInput(snapshot_, samples, source, spectrum, out);
}

}  // namespace lumice::raypath
