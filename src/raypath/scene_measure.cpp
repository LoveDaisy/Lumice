#include "raypath/scene_measure.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

#include "analytic/entry_measure.hpp"
#include "analytic/path_evaluation.hpp"
#include "core/crystal.hpp"
#include "core/geo3d.hpp"
#include "core/optics.hpp"
#include "core/shared/lat_path_selection.hpp"
#include "core/trace_ops.hpp"
#include "raypath/scene_to_analytic.hpp"
#include "util/illuminant.hpp"

namespace lumice::raypath {

const char* SceneMeasureStatusName(SceneMeasureStatus status) {
  switch (status) {
    case SceneMeasureStatus::kConfirmed:
      return "confirmed";
    case SceneMeasureStatus::kZeroWeight:
      return "zero_weight";
    case SceneMeasureStatus::kPhysicallyUnreachable:
      return "physically_unreachable";
    case SceneMeasureStatus::kNumericalIncomplete:
      return "numerical_incomplete";
    case SceneMeasureStatus::kNotSupported:
      return "not_supported";
  }
  return "not_supported";
}

const char* LatentBaseMeasureName(LatentBaseMeasure measure) {
  switch (measure) {
    case LatentBaseMeasure::kAtomCounting:
      return "atom_counting";
    case LatentBaseMeasure::kLebesgue:
      return "lebesgue";
    case LatentBaseMeasure::kUnitInterval:
      return "unit_interval_lebesgue";
    case LatentBaseMeasure::kBernoulliCounting:
      return "bernoulli_counting";
  }
  return "atom_counting";
}

namespace {

constexpr double kReferenceRedNm = 694.3628981235904;
constexpr double kReferenceBlueNm = 430.0197374077313;
constexpr int kMaxSceneMeasureSampleCount = 1000000;
constexpr int kMaxSourceNodeCount = 256;
constexpr uint64_t kMaxSceneMeasureRows = 16777216;
constexpr size_t kMaxStoredSceneMeasureRows = 64;
constexpr size_t kMaxMemberChains = 4096;

struct LayerInput {
  const MsInfo* ms = nullptr;
  const ScatteringSetting* setting = nullptr;
  double crystal_share = 0.0;
  double continuation_mass = 0.0;
  int shape_latent[kShapeScalarCount]{};
  int pose_latent[3]{};  // latitude, azimuth, roll
  int pose_flip_latent = -1;
};

uint32_t MixSeed(uint32_t seed, uint32_t value) {
  uint32_t x = seed ^ (value + 0x9e3779b9u + (seed << 6u) + (seed >> 2u));
  x ^= x >> 16u;
  x *= 0x7feb352du;
  x ^= x >> 15u;
  x *= 0x846ca68bu;
  return x ^ (x >> 16u);
}

uint32_t RowSeed(uint32_t seed, int spectrum, int sun, int member, int sample) {
  seed = MixSeed(seed, static_cast<uint32_t>(spectrum));
  seed = MixSeed(seed, static_cast<uint32_t>(sun));
  seed = MixSeed(seed, static_cast<uint32_t>(member));
  return MixSeed(seed, static_cast<uint32_t>(sample));
}

std::string DistributionMeasure(const Distribution& distribution, bool spherical_latitude) {
  if (distribution.type == DistributionType::kNoRandom) {
    return "atom";
  }
  if (spherical_latitude && distribution.type != DistributionType::kGaussianLegacy) {
    return "product distribution multiplied by the spherical-area Jacobian and normalized";
  }
  switch (distribution.type) {
    case DistributionType::kUniform:
      return "normalized uniform interval";
    case DistributionType::kGaussian:
      return "normalized Gaussian";
    case DistributionType::kZigzag:
      return "normalized rectified-arcsine pushforward";
    case DistributionType::kLaplacian:
      return "normalized Laplace distribution";
    case DistributionType::kGaussianLegacy:
      return "normalized legacy Gaussian without spherical-area correction";
    case DistributionType::kNoRandom:
      return "atom";
  }
  return "unknown";
}

bool DistributionHasPositiveWidth(const Distribution& distribution) {
  switch (distribution.type) {
    case DistributionType::kNoRandom:
      return false;
    case DistributionType::kUniform:
      return distribution.UniformFullRange() != 0.0f;
    case DistributionType::kGaussian:
    case DistributionType::kGaussianLegacy:
      return distribution.Std() != 0.0f;
    case DistributionType::kZigzag:
      return distribution.Amplitude() != 0.0f;
    case DistributionType::kLaplacian:
      return distribution.Scale() != 0.0f;
  }
  return false;
}

const Distribution* ShapeDistribution(const CrystalParam& param, int slot) {
  return std::visit(
      [slot](const auto& p) -> const Distribution* {
        using T = std::decay_t<decltype(p)>;
        if constexpr (std::is_same_v<T, PrismCrystalParam>) {
          if (slot == kShapeScalarHeight) {
            return &p.h_;
          }
        } else {
          if (slot == kShapeScalarUpperH) {
            return &p.h_pyr_u_;
          }
          if (slot == kShapeScalarPrismH) {
            return &p.h_prs_;
          }
          if (slot == kShapeScalarLowerH) {
            return &p.h_pyr_l_;
          }
        }
        if (slot >= kShapeScalarFace0 && slot < kShapeScalarCount) {
          return &p.d_[slot - kShapeScalarFace0];
        }
        return nullptr;
      },
      param);
}

const int* ShapeSyncGroups(const CrystalParam& param) {
  return std::visit([](const auto& p) { return p.sync_group_; }, param);
}

CrystalKind KindOf(const CrystalParam& param) {
  return std::holds_alternative<PrismCrystalParam>(param) ? CrystalKind::kPrism : CrystalKind::kPyramid;
}

std::string ShapeName(CrystalKind kind, int slot) {
  const char* key = ShapeScalarSyncKeyName(kind, slot);
  std::string name = key != nullptr ? key : "unknown";
  if (slot >= kShapeScalarFace0) {
    name += "[" + std::to_string(slot - kShapeScalarFace0) + "]";
  }
  return name;
}

void AddFactorDescriptors(int layer_index, const CrystalConfig& crystal, LayerInput* layer, int* next_latent,
                          std::vector<MeasureFactorDescriptor>* factors) {
  std::fill(std::begin(layer->shape_latent), std::end(layer->shape_latent), -1);
  std::fill(std::begin(layer->pose_latent), std::end(layer->pose_latent), -1);
  const CrystalKind kind = KindOf(crystal.param_);
  const int* sync = ShapeSyncGroups(crystal.param_);
  std::vector<std::pair<int, int>> group_latents;
  for (int slot = 0; slot < kShapeScalarCount; slot++) {
    const Distribution* distribution = ShapeDistribution(crystal.param_, slot);
    if (distribution == nullptr) {
      continue;
    }
    int latent = -1;
    if (DistributionHasPositiveWidth(*distribution)) {
      if (sync[slot] != 0) {
        const auto found = std::find_if(group_latents.begin(), group_latents.end(),
                                        [&](const auto& item) { return item.first == sync[slot]; });
        if (found != group_latents.end()) {
          latent = found->second;
        } else {
          latent = (*next_latent)++;
          group_latents.emplace_back(sync[slot], latent);
        }
      } else {
        latent = (*next_latent)++;
      }
    }
    layer->shape_latent[slot] = latent;
    factors->push_back({ layer_index, "shape." + ShapeName(kind, slot), distribution->type, latent,
                         DistributionHasPositiveWidth(*distribution) ? 1 : 0, distribution->center,
                         distribution->spread, "product shape scalar", DistributionMeasure(*distribution, false),
                         "unit probability mass" });
  }

  const Distribution axis[3] = { crystal.axis_.latitude_dist, crystal.axis_.azimuth_dist, crystal.axis_.roll_dist };
  constexpr const char* kNames[3] = { "pose.latitude", "pose.azimuth", "pose.roll" };
  for (int i = 0; i < 3; i++) {
    const int latent = DistributionHasPositiveWidth(axis[i]) ? (*next_latent)++ : -1;
    layer->pose_latent[i] = latent;
    factors->push_back({ layer_index, kNames[i], axis[i].type, latent, DistributionHasPositiveWidth(axis[i]) ? 1 : 0,
                         axis[i].center, axis[i].spread, i == 0 ? "spherical latitude" : "angle",
                         DistributionMeasure(axis[i], i == 0), "unit probability mass" });
  }
  if (lat_path::SelectLatPath(crystal.axis_).kind == lat_path::LatPathKind::kLutInverseCdf) {
    layer->pose_flip_latent = (*next_latent)++;
    factors->push_back({ layer_index, "pose.latitude_fold_branch", DistributionType::kNoRandom, layer->pose_flip_latent,
                         0, 0.0, 0.0, "Bernoulli branch",
                         "conditional pole-fold branch mass from the product latitude LUT", "unit probability mass" });
  }
}

Error ResolveLayers(const ConfigManager& config, const SceneMeasureRequest& request, std::vector<LayerInput>* layers,
                    std::vector<MeasureFactorDescriptor>* factors) {
  if (request.path_layers.empty() || request.path_layers.size() != request.layer_crystal_ids.size()) {
    return { ErrorCode::kInvalidPath, "path_layers and layer_crystal_ids must have the same non-zero length" };
  }
  if (request.path_layers.size() > config.scene_.ms_.size()) {
    return { ErrorCode::kInvalidPath, "the requested path has more layers than the scene" };
  }
  int next_latent = 0;
  for (size_t i = 0; i < request.path_layers.size(); i++) {
    const MsInfo& ms = config.scene_.ms_[i];
    const IdType crystal_id = request.layer_crystal_ids[i];
    const auto found = std::find_if(ms.setting_.begin(), ms.setting_.end(),
                                    [crystal_id](const ScatteringSetting& s) { return s.crystal_.id_ == crystal_id; });
    if (found == ms.setting_.end()) {
      return { ErrorCode::kUnknownCrystalId,
               "layer " + std::to_string(i) + " has no crystal entry with id " + std::to_string(crystal_id) };
    }
    const double total =
        std::accumulate(ms.setting_.begin(), ms.setting_.end(), 0.0, [](double sum, const ScatteringSetting& s) {
          return sum + std::max(0.0, static_cast<double>(s.crystal_proportion_));
        });
    if (!(total > 0.0)) {
      return { ErrorCode::kInvalidArgument, "layer " + std::to_string(i) + " has zero total crystal proportion" };
    }
    if (!std::isfinite(ms.prob_) || ms.prob_ < 0.0f || ms.prob_ > 1.0f) {
      return { ErrorCode::kInvalidArgument, "layer " + std::to_string(i) + " has an invalid continuation probability" };
    }
    LayerInput layer;
    layer.ms = &ms;
    layer.setting = &*found;
    layer.crystal_share = std::max(0.0, static_cast<double>(found->crystal_proportion_)) / total;
    layer.continuation_mass = i + 1 < request.path_layers.size() ? ms.prob_ : 1.0 - ms.prob_;
    AddFactorDescriptors(static_cast<int>(i), found->crystal_, &layer, &next_latent, factors);
    layers->push_back(layer);
  }
  return {};
}

std::vector<std::vector<int>> ExpandPhysicalMembers(const CrystalConfig& crystal, const std::vector<int>& faces) {
  const GeometricSymmetry shape =
      std::visit([](const auto& param) { return DeriveGeometricSymmetry(param); }, crystal.param_);
  const SymmetryGating gating = DeriveSymmetryGating(SymmetrySemantics::kPhysical, shape, crystal.axis_);
  const auto d = detail::DeriveDSymmetryParams(crystal.axis_);
  std::vector<IdType> path(faces.begin(), faces.end());
  std::vector<std::vector<int>> out;
  for (const auto& member :
       ExpandRaypathByPeriod(path, kSymmetryPrism | kSymmetryBasal | kSymmetryDirection, d.sigma_a, d.d_applicable,
                             gating.p_applicable, gating.b_applicable, kHexagonalFnPeriod, gating.geom)) {
    std::vector<int> converted(member.begin(), member.end());
    if (std::find(out.begin(), out.end(), converted) == out.end()) {
      out.push_back(std::move(converted));
    }
  }
  if (out.empty()) {
    out.push_back(faces);
  }
  return out;
}

Error BuildMemberChains(const SceneMeasureRequest& request, const std::vector<LayerInput>& layers,
                        std::vector<std::vector<std::vector<int>>>* out) {
  if (request.member_selection == SceneMemberSelection::kExplicitChains) {
    if (request.explicit_member_chains.empty()) {
      return { ErrorCode::kInvalidArgument, "explicit member selection requires at least one member chain" };
    }
    if (request.explicit_member_chains.size() > kMaxMemberChains) {
      return { ErrorCode::kInvalidArgument, "explicit member selection exceeds the 4096-chain bound" };
    }
    for (size_t chain_index = 0; chain_index < request.explicit_member_chains.size(); chain_index++) {
      const auto& chain = request.explicit_member_chains[chain_index];
      if (chain.size() != layers.size()) {
        return { ErrorCode::kInvalidPath, "explicit member chain " + std::to_string(chain_index) +
                                              " must contain exactly one face sequence per layer" };
      }
      if (std::any_of(chain.begin(), chain.end(), [](const auto& faces) { return faces.empty(); })) {
        return { ErrorCode::kInvalidPath, "explicit member chains cannot contain an empty layer face sequence" };
      }
    }
    *out = request.explicit_member_chains;
    return {};
  }
  if (!request.physical_member_masks.empty() && request.physical_member_masks.size() != layers.size()) {
    return { ErrorCode::kInvalidArgument, "physical_member_masks must be empty or contain one mask per layer" };
  }
  std::vector<std::vector<std::vector<int>>> per_layer;
  for (size_t i = 0; i < layers.size(); i++) {
    if (request.member_selection == SceneMemberSelection::kConcrete) {
      per_layer.push_back({ request.path_layers[i] });
    } else {
      per_layer.push_back(ExpandPhysicalMembers(layers[i].setting->crystal_, request.path_layers[i]));
      if (request.member_selection == SceneMemberSelection::kPhysicalMask) {
        const uint64_t mask =
            request.physical_member_masks.empty() ? request.physical_member_mask : request.physical_member_masks[i];
        auto& members = per_layer.back();
        members.erase(std::remove_if(members.begin(), members.end(),
                                     [&](const std::vector<int>& member) {
                                       if (member.empty() || member.front() < 0 || member.front() >= 64) {
                                         return true;
                                       }
                                       return (mask & (uint64_t{ 1 } << member.front())) == 0;
                                     }),
                      members.end());
        if (members.empty()) {
          return { ErrorCode::kInvalidArgument,
                   "physical_member_mask selects no physical L2 entry-face member in layer " + std::to_string(i) };
        }
      }
    }
  }
  out->push_back({});
  for (const auto& members : per_layer) {
    std::vector<std::vector<std::vector<int>>> next;
    if (members.size() > kMaxMemberChains || out->size() > kMaxMemberChains / members.size()) {
      return { ErrorCode::kInvalidArgument, "physical member-chain expansion exceeds the 4096-chain bound" };
    }
    for (const auto& prefix : *out) {
      for (const auto& member : members) {
        auto chain = prefix;
        chain.push_back(member);
        next.push_back(std::move(chain));
      }
    }
    *out = std::move(next);
  }
  if (out->empty()) {
    return { ErrorCode::kInvalidArgument, "member selection contains no physical member chain" };
  }
  return {};
}

Error ValidateWavelength(double wavelength_nm, double weight, SpectrumMeasureNode* out) {
  if (!std::isfinite(weight) || weight < 0.0) {
    return { ErrorCode::kInvalidArgument, "spectrum weights must be finite and non-negative" };
  }
  if (!std::isfinite(wavelength_nm) || wavelength_nm < IceRefractiveIndex::kMinWaveLength ||
      wavelength_nm > IceRefractiveIndex::kMaxWaveLength) {
    return { ErrorCode::kWavelengthOutOfRange, "spectrum wavelength is outside [350, 900] nm" };
  }
  out->wavelength_nm = wavelength_nm;
  out->weight = weight;
  out->refractive_index = IceRefractiveIndex::Get(wavelength_nm);
  return {};
}

Error BuildSpectrumNodes(const LightSourceConfig& light, const SceneMeasureRequest& request,
                         std::vector<SpectrumMeasureNode>* out) {
  if (request.spectrum_source == SceneSpectrumSource::kLegacyReferenceEndpoints) {
    out->resize(2);
    for (int i = 0; i < 2; i++) {
      (*out)[i].node_id = i;
      (*out)[i].source = "legacy_reference_endpoint";
      if (const Error error = ValidateWavelength(i == 0 ? kReferenceRedNm : kReferenceBlueNm, 1.0, &(*out)[i]);
          !error.Ok()) {
        return error;
      }
    }
    return {};
  }
  if (request.spectrum_source == SceneSpectrumSource::kDiagnostic) {
    if (request.diagnostic_wavelengths_nm.empty()) {
      return { ErrorCode::kInvalidArgument, "diagnostic spectrum requires at least one wavelength" };
    }
    if (!request.diagnostic_wavelength_weights.empty() &&
        request.diagnostic_wavelength_weights.size() != request.diagnostic_wavelengths_nm.size()) {
      return { ErrorCode::kInvalidArgument, "diagnostic wavelength weights must be empty or match wavelengths" };
    }
    out->resize(request.diagnostic_wavelengths_nm.size());
    for (size_t i = 0; i < out->size(); i++) {
      (*out)[i].node_id = static_cast<int>(i);
      (*out)[i].source = "diagnostic";
      const double weight =
          request.diagnostic_wavelength_weights.empty() ? 1.0 : request.diagnostic_wavelength_weights[i];
      if (const Error error = ValidateWavelength(request.diagnostic_wavelengths_nm[i], weight, &(*out)[i]);
          !error.Ok()) {
        return error;
      }
    }
    return {};
  }
  if (const auto* discrete = std::get_if<std::vector<WlParam>>(&light.spectrum_); discrete != nullptr) {
    if (discrete->empty()) {
      return { ErrorCode::kInvalidArgument, "scene spectrum has no wavelength nodes" };
    }
    out->resize(discrete->size());
    for (size_t i = 0; i < discrete->size(); i++) {
      (*out)[i].node_id = static_cast<int>(i);
      (*out)[i].source = "scene_discrete";
      if (const Error error = ValidateWavelength((*discrete)[i].wl_, (*discrete)[i].weight_, &(*out)[i]); !error.Ok()) {
        return error;
      }
    }
    return {};
  }
  const int count = request.illuminant_node_count;
  if (count < 1 || count > kMaxSourceNodeCount) {
    return { ErrorCode::kInvalidArgument, "illuminant_node_count must be in [1, 256]" };
  }
  const IlluminantType illuminant = std::get<IlluminantType>(light.spectrum_);
  out->resize(static_cast<size_t>(count));
  for (int i = 0; i < count; i++) {
    const double wavelength = 380.0 + (static_cast<double>(i) + 0.5) * 400.0 / static_cast<double>(count);
    SpectrumMeasureNode& node = (*out)[static_cast<size_t>(i)];
    node.node_id = i;
    node.source = "scene_illuminant_uniform_380_780";
    if (const Error error =
            ValidateWavelength(wavelength, GetIlluminantSpd(illuminant, static_cast<float>(wavelength)) / count, &node);
        !error.Ok()) {
      return error;
    }
  }
  return {};
}

Error BuildSunNodes(const SunParam& sun, const SceneMeasureRequest& request, std::vector<SunMeasureNode>* out) {
  if (!request.source_sun_nodes.empty()) {
    if (request.source_sun_nodes.size() > static_cast<size_t>(kMaxSourceNodeCount)) {
      return { ErrorCode::kInvalidArgument, "source_sun_nodes may contain at most 256 nodes" };
    }
    *out = request.source_sun_nodes;
    for (size_t i = 0; i < out->size(); i++) {
      SunMeasureNode& node = (*out)[i];
      if (!std::isfinite(node.mass) || node.mass < 0.0 ||
          !std::all_of(std::begin(node.incident_direction), std::end(node.incident_direction),
                       [](double value) { return std::isfinite(value); })) {
        return { ErrorCode::kInvalidArgument, "source_sun_nodes require finite directions and non-negative masses" };
      }
      const double norm =
          std::sqrt(std::inner_product(std::begin(node.incident_direction), std::end(node.incident_direction),
                                       std::begin(node.incident_direction), 0.0));
      if (!(norm > 0.0)) {
        return { ErrorCode::kInvalidArgument, "source_sun_nodes require non-zero directions" };
      }
      for (double& component : node.incident_direction) {
        component /= norm;
      }
      node.node_id = static_cast<int>(i);
    }
    return {};
  }
  if (!std::isfinite(sun.altitude_) || !std::isfinite(sun.azimuth_) || !std::isfinite(sun.diameter_) ||
      sun.diameter_ < 0.0f || sun.diameter_ > 360.0f) {
    return { ErrorCode::kInvalidArgument, "sun altitude, azimuth and diameter must define a finite spherical cap" };
  }
  if (sun.diameter_ == 0.0f) {
    SunMeasureNode node;
    node.mass = 1.0;
    SunIncidentDirection(sun, node.incident_direction);
    out->push_back(node);
    return {};
  }
  if (request.sun_node_count < 1 || request.sun_node_count > kMaxSourceNodeCount) {
    return { ErrorCode::kInvalidArgument, "sun_node_count must be in [1, 256] for a finite solar disc" };
  }
  RandomNumberGenerator rng(MixSeed(request.seed, 0x53554eu));
  out->resize(static_cast<size_t>(request.sun_node_count));
  for (int i = 0; i < request.sun_node_count; i++) {
    float direction[3]{};
    SampleSphCapPointWithRng(rng, sun.azimuth_ + 180.0f, -sun.altitude_, sun.diameter_ / 2.0f, direction);
    SunMeasureNode& node = (*out)[static_cast<size_t>(i)];
    node.node_id = i;
    node.mass = 1.0 / request.sun_node_count;
    std::copy(direction, direction + 3, node.incident_direction);
  }
  return {};
}

LatentBaseMeasure DistributionBaseMeasure(const Distribution& distribution,
                                          const RandomNumberGenerator::DistributionSample& sample,
                                          bool forced_unit_interval = false) {
  if (sample.atom) {
    return LatentBaseMeasure::kAtomCounting;
  }
  if (forced_unit_interval || distribution.type == DistributionType::kUniform ||
      distribution.type == DistributionType::kZigzag || distribution.type == DistributionType::kLaplacian) {
    return LatentBaseMeasure::kUnitInterval;
  }
  return LatentBaseMeasure::kLebesgue;
}

void AppendLatent(int latent_id, int layer_index, const std::string& name, LatentBaseMeasure base_measure,
                  const RandomNumberGenerator::DistributionSample& sample, const std::string& mapping,
                  std::vector<LatentMeasureSample>* latents) {
  if (latent_id >= 0 && std::any_of(latents->begin(), latents->end(),
                                    [latent_id](const auto& item) { return item.latent_id == latent_id; })) {
    return;
  }
  LatentMeasureSample latent;
  latent.latent_id = latent_id;
  latent.layer_index = layer_index;
  latent.name = name;
  latent.base_measure = base_measure;
  latent.coordinate = sample.latent_coordinate;
  latent.proposal_density_or_mass = sample.latent_proposal_density;
  latent.target_density_or_mass = sample.latent_target_density;
  latent.mapping_jacobian = sample.mapping_jacobian;
  latent.mapping = mapping;
  if (!std::isfinite(latent.coordinate) || !std::isfinite(latent.proposal_density_or_mass) ||
      !std::isfinite(latent.target_density_or_mass) || !std::isfinite(latent.mapping_jacobian) ||
      latent.proposal_density_or_mass < 0.0 || latent.target_density_or_mass < 0.0) {
    latent.status = SceneMeasureStatus::kNumericalIncomplete;
  }
  latents->push_back(std::move(latent));
}

analytic::CrystalShape SampleShape(RandomNumberGenerator& rng, const CrystalParam& param, const LayerInput& layer,
                                   int layer_index, std::vector<ShapeScalarSample>* samples,
                                   std::vector<LatentMeasureSample>* latents) {
  analytic::CrystalShape shape;
  const CrystalKind kind = KindOf(param);
  std::vector<ShapeScalarTrace> traces;
  std::visit(
      [&](const auto& p) {
        using T = std::decay_t<decltype(p)>;
        float distances[6]{};
        if constexpr (std::is_same_v<T, PrismCrystalParam>) {
          shape.kind = analytic::CrystalShapeKind::kPrism;
          shape.height = SamplePrismShapeScalarsWithTrace(rng, p, distances, &traces);
        } else {
          shape.kind = analytic::CrystalShapeKind::kPyramid;
          float upper = 0.0f;
          float prism = 0.0f;
          float lower = 0.0f;
          SamplePyramidShapeScalarsWithTrace(rng, p, upper, prism, lower, distances, &traces);
          shape.height = prism;
          shape.upper_h = upper;
          shape.lower_h = lower;
          shape.upper_wedge_deg = p.wedge_angle_u_;
          shape.lower_wedge_deg = p.wedge_angle_l_;
        }
        for (int i = 0; i < 6; i++) {
          shape.face_distance[i] = distances[i];
        }
      },
      param);
  for (const ShapeScalarTrace& trace : traces) {
    const Distribution* distribution = ShapeDistribution(param, trace.slot);
    if (distribution == nullptr) {
      continue;
    }
    const int latent_id = layer.shape_latent[trace.slot];
    const std::string name = ShapeName(kind, trace.slot);
    samples->push_back({ name, trace.mapped_value, latent_id, trace.sync_group, trace.leader_slot, trace.raw_value,
                         trace.absolute_value_fold, trace.mapping_jacobian });
    AppendLatent(latent_id, layer_index, "shape." + ShapeName(kind, trace.leader_slot),
                 DistributionBaseMeasure(*distribution, trace.draw), trace.draw,
                 trace.sync_group == 0 ? "distribution latent -> scalar" :
                                         "leader distribution latent -> synchronized shape scalars",
                 latents);
  }
  return shape;
}

int PoseSupportRank(const float pose[3], const bool active[3], double derivatives[27]) {
  constexpr double kStep = 1e-4;
  std::vector<std::vector<double>> basis;
  for (int coordinate = 0; coordinate < 3; coordinate++) {
    float plus[3] = { pose[0], pose[1], pose[2] };
    float minus[3] = { pose[0], pose[1], pose[2] };
    plus[coordinate] += static_cast<float>(kStep);
    minus[coordinate] -= static_cast<float>(kStep);
    const Rotation plus_rotation = BuildCrystalRotation(plus[0], plus[1], plus[2]);
    const Rotation minus_rotation = BuildCrystalRotation(minus[0], minus[1], minus[2]);
    std::vector<double> tangent(9);
    for (int element = 0; element < 9; element++) {
      tangent[element] = (plus_rotation.GetMat()[element] - minus_rotation.GetMat()[element]) / (2.0 * kStep);
      derivatives[coordinate * 9 + element] = tangent[element];
    }
    if (!active[coordinate]) {
      continue;
    }
    for (const auto& previous : basis) {
      const double projection = std::inner_product(tangent.begin(), tangent.end(), previous.begin(), 0.0);
      for (int element = 0; element < 9; element++) {
        tangent[element] -= projection * previous[element];
      }
    }
    const double norm = std::sqrt(std::inner_product(tangent.begin(), tangent.end(), tangent.begin(), 0.0));
    if (norm > 1e-5) {
      for (double& value : tangent) {
        value /= norm;
      }
      basis.push_back(std::move(tangent));
    }
  }
  return static_cast<int>(basis.size());
}

void AppendPoseLatents(const AxisDistribution& axis, const LayerInput& input, int layer_index,
                       const AxisPoseSampleTrace& trace, std::vector<LatentMeasureSample>* latents) {
  const Distribution distributions[3] = { axis.latitude_dist, axis.azimuth_dist, axis.roll_dist };
  const RandomNumberGenerator::DistributionSample samples[3] = { trace.latitude, trace.longitude, trace.roll };
  constexpr const char* kNames[3] = { "pose.latitude", "pose.azimuth", "pose.roll" };
  for (int i = 0; i < 3; i++) {
    const bool generated_from_unit_interval =
        (i == 0 && (trace.full_sphere || trace.latitude_lut)) || (i == 1 && trace.full_sphere);
    AppendLatent(input.pose_latent[i], layer_index, kNames[i],
                 DistributionBaseMeasure(distributions[i], samples[i], generated_from_unit_interval), samples[i],
                 i == 0 ? "generator coordinate -> folded spherical latitude" :
                          "generator coordinate -> rotation angle (radians)",
                 latents);
  }
  if (trace.latitude_lut) {
    RandomNumberGenerator::DistributionSample branch;
    branch.value = trace.latitude_flipped ? 1.0f : 0.0f;
    branch.latent_coordinate = branch.value;
    branch.latent_proposal_density = trace.latitude_flip_mass;
    branch.latent_target_density = trace.latitude_flip_mass;
    AppendLatent(input.pose_flip_latent, layer_index, "pose.latitude_fold_branch",
                 LatentBaseMeasure::kBernoulliCounting, branch,
                 "conditional branch -> azimuth and roll plus pi when flipped", latents);
  }
}

SceneMeasureStatus FieldStatus(const analytic::DiagnosticFieldResult& field, std::string* reason) {
  if (field.path_status == analytic::DiagnosticPathStatus::kNonFinite) {
    *reason = "analytic field produced a non-finite numerical result";
    return SceneMeasureStatus::kNumericalIncomplete;
  }
  if (field.path_status != analytic::DiagnosticPathStatus::kOk) {
    *reason = "analytic path is infeasible or at a refraction critical state";
    return SceneMeasureStatus::kPhysicallyUnreachable;
  }
  if (field.entry_status != analytic::DiagnosticEntryStatus::kOk || !(field.entry_measure > 0.0) ||
      !(field.fresnel_weight > 0.0)) {
    *reason = "finite-crystal entry support is empty or has zero optical weight";
    return SceneMeasureStatus::kPhysicallyUnreachable;
  }
  return SceneMeasureStatus::kConfirmed;
}

SceneMeasureLayerRow EvaluateLayer(RandomNumberGenerator& rng, const LayerInput& input, int layer_index,
                                   const std::vector<int>& faces, double refractive_index, const double incident[3],
                                   bool include_derivatives, std::vector<LatentMeasureSample>* latents) {
  SceneMeasureLayerRow out;
  out.layer_index = layer_index;
  out.crystal_id = input.setting->crystal_.id_;
  out.faces = faces;
  out.crystal_share = input.crystal_share;
  out.continuation_mass = input.continuation_mass;
  std::copy(incident, incident + 3, out.incident_direction);

  const analytic::CrystalShape shape =
      SampleShape(rng, input.setting->crystal_.param_, input, layer_index, &out.shape, latents);
  float pose_values[3]{};
  const AxisPoseSampleTrace pose_trace =
      RandomSampler::SampleAxisPoseWithTrace(rng, input.setting->crystal_.axis_, pose_values);
  AppendPoseLatents(input.setting->crystal_.axis_, input, layer_index, pose_trace, latents);
  std::copy(pose_values, pose_values + 3, out.pose_lon_lat_roll_rad);
  const bool active_pose_coordinates[3] = {
    !pose_trace.longitude.atom && pose_trace.longitude.mapping_jacobian > 0.0,
    !pose_trace.latitude.atom && pose_trace.latitude.mapping_jacobian > 0.0,
    !pose_trace.roll.atom && pose_trace.roll.mapping_jacobian > 0.0,
  };
  out.pose_support_rank = PoseSupportRank(pose_values, active_pose_coordinates, out.pose_tangent_drotation);

  analytic::FaceNormalTable normals;
  analytic::FacePolygonTable polygons;
  if (analytic::BuildFaceNormals(shape, &normals, &polygons) != analytic::Status::kOk) {
    out.status = SceneMeasureStatus::kPhysicallyUnreachable;
    out.reason = "sampled crystal is rejected by the closed-form geometry gate";
    return out;
  }
  std::vector<int> slots;
  if (const Error error = ResolveSingleLayerPath({ faces }, normals, &slots); !error.Ok()) {
    out.status = SceneMeasureStatus::kPhysicallyUnreachable;
    out.reason = error.message;
    return out;
  }

  const Rotation rotation = BuildCrystalRotation(pose_values[0], pose_values[1], pose_values[2]);
  analytic::DiagnosticRowInput row_input;
  row_input.refractive_index = refractive_index;
  std::copy(incident, incident + 3, row_input.incident_direction);
  const float* matrix = rotation.GetMat();
  std::copy(matrix, matrix + 9, row_input.pose);

  analytic::DiagnosticField field(normals, polygons, faces.data(), slots.data(), static_cast<int>(faces.size()));
  out.field = include_derivatives ? field.Evaluate(row_input) : field.EvaluateWithoutDerivatives(row_input);
  out.entry_measure = out.field.entry_measure;
  out.fresnel_weight = out.field.fresnel_weight;
  std::copy(out.field.outgoing_direction, out.field.outgoing_direction + 3, out.outgoing_direction);
  out.status = FieldStatus(out.field, &out.reason);
  return out;
}

bool ExceedsRowBudget(size_t spectrum_count, size_t sun_count, size_t member_count, int sample_count) {
  uint64_t count = static_cast<uint64_t>(spectrum_count);
  for (const size_t factor : { sun_count, member_count, static_cast<size_t>(sample_count) }) {
    if (factor != 0 && count > kMaxSceneMeasureRows / factor) {
      return true;
    }
    count *= factor;
  }
  return count > kMaxSceneMeasureRows;
}

void CountStatus(SceneMeasureStatus status, SceneMeasureStatusCounts* counts) {
  switch (status) {
    case SceneMeasureStatus::kConfirmed:
      counts->confirmed++;
      break;
    case SceneMeasureStatus::kZeroWeight:
      counts->zero_weight++;
      break;
    case SceneMeasureStatus::kPhysicallyUnreachable:
      counts->physically_unreachable++;
      break;
    case SceneMeasureStatus::kNumericalIncomplete:
      counts->numerical_incomplete++;
      break;
    case SceneMeasureStatus::kNotSupported:
      counts->not_supported++;
      break;
  }
}

void StoreRepresentativeRow(const SceneMeasureRow& row, uint32_t priority, std::vector<uint32_t>* priorities,
                            SceneMeasureResult* result) {
  if (result->rows.size() < kMaxStoredSceneMeasureRows) {
    result->rows.push_back(row);
    priorities->push_back(priority);
    return;
  }
  const auto worst = std::max_element(priorities->begin(), priorities->end());
  if (priority < *worst) {
    const size_t index = static_cast<size_t>(std::distance(priorities->begin(), worst));
    result->rows[index] = row;
    (*priorities)[index] = priority;
  }
  result->rows_truncated = true;
}

double SplitError(const double contribution[2], const double mass[2]) {
  if (!(mass[0] > 0.0) || !(mass[1] > 0.0)) {
    return 0.0;
  }
  const double total_mass = mass[0] + mass[1];
  const double estimate0 = contribution[0] * total_mass / mass[0];
  const double estimate1 = contribution[1] * total_mass / mass[1];
  return 0.5 * std::fabs(estimate0 - estimate1);
}

}  // namespace

Error BuildSceneMeasure(const ConfigManager& config, const SceneMeasureRequest& request, SceneMeasureResult* out) {
  return BuildSceneMeasure(config, request, SceneMeasureRowVisitor{}, out);
}

Error BuildSceneMeasure(const ConfigManager& config, const SceneMeasureRequest& request,
                        const SceneMeasureRowVisitor& visitor, SceneMeasureResult* out) {
  *out = SceneMeasureResult{};
  if (request.sample_count < 2 || request.sample_count > kMaxSceneMeasureSampleCount || request.sample_count % 2 != 0) {
    return { ErrorCode::kInvalidArgument, "sample_count must be an even integer in [2, 1000000]" };
  }

  SceneMeasureResult result;
  result.seed = request.seed;
  result.requested_sample_count = request.sample_count;
  result.stored_row_selection =
      "deterministic bottom-k hash reservoir over all evaluated source/member/sample row identities";
  result.units = "LI a=1 relative finite-crystal area times raw spectral weight";
  result.normalization =
      "shape, pose and the product finite solar source are unit probability measures; spectrum and physical "
      "members retain their explicit weights; atoms carry counting mass rather than continuous density";

  std::vector<LayerInput> layers;
  if (const Error error = ResolveLayers(config, request, &layers, &result.factors); !error.Ok()) {
    return error;
  }
  result.factors.insert(
      result.factors.begin(),
      { -1, "sun_disc", DistributionType::kNoRandom, -1, config.scene_.light_source_.param_.diameter_ == 0.0f ? 0 : 2,
        config.scene_.light_source_.param_.diameter_, 0.0, "spherical cap diameter (degrees)",
        config.scene_.light_source_.param_.diameter_ == 0.0f ? "atom" : "normalized spherical-cap source",
        "unit source probability mass" });
  result.factors.insert(
      result.factors.begin(),
      { -1, "spectrum", DistributionType::kNoRandom, -1, 1, 0.0, 0.0, "wavelength node",
        request.spectrum_source == SceneSpectrumSource::kScene ? "scene spectrum" : "explicit diagnostic spectrum",
        "raw spectral weight" });
  if (const Error error = BuildSpectrumNodes(config.scene_.light_source_, request, &result.spectrum_nodes);
      !error.Ok()) {
    return error;
  }
  if (const Error error = BuildSunNodes(config.scene_.light_source_.param_, request, &result.sun_nodes); !error.Ok()) {
    return error;
  }
  if (const Error error = BuildMemberChains(request, layers, &result.member_chains); !error.Ok()) {
    return error;
  }
  if (ExceedsRowBudget(result.spectrum_nodes.size(), result.sun_nodes.size(), result.member_chains.size(),
                       request.sample_count)) {
    return { ErrorCode::kInvalidArgument, "spectrum × sun × member chains × samples exceeds the 16777216-row budget" };
  }

  const int coarse_count = request.sample_count / 2;
  bool any_confirmed = false;
  bool any_nonzero_source = false;
  bool any_unreachable = false;
  bool any_numerical = false;
  const bool has_continuous_factor = std::any_of(
      result.factors.begin(), result.factors.end(),
      [](const MeasureFactorDescriptor& factor) { return factor.layer_index >= 0 && factor.support_dimension > 0; });
  const bool approximate_sun = request.source_sun_nodes.empty() && config.scene_.light_source_.param_.diameter_ > 0.0f;
  const bool approximate_spectrum =
      !result.spectrum_nodes.empty() && result.spectrum_nodes.front().source == "scene_illuminant_uniform_380_780";
  double sun_partition_contribution[2]{};
  double sun_partition_mass[2]{};
  for (const SunMeasureNode& node : result.sun_nodes) {
    sun_partition_mass[node.node_id & 1] += node.mass;
  }
  double spectrum_partition_contribution[2]{};
  double spectrum_partition_mass[2]{};
  for (const SpectrumMeasureNode& node : result.spectrum_nodes) {
    spectrum_partition_mass[node.node_id & 1] += node.weight;
  }
  std::vector<uint32_t> stored_priorities;
  for (const SpectrumMeasureNode& spectrum : result.spectrum_nodes) {
    for (const SunMeasureNode& sun : result.sun_nodes) {
      for (size_t member_index = 0; member_index < result.member_chains.size(); member_index++) {
        for (int sample = 0; sample < request.sample_count; sample++) {
          SceneMeasureRow row;
          row.spectrum_node_id = spectrum.node_id;
          row.sun_node_id = sun.node_id;
          row.member_chain_index = static_cast<int>(member_index);
          row.sample_index = sample;
          row.replay_seed =
              RowSeed(request.seed, spectrum.node_id, sun.node_id, static_cast<int>(member_index), sample);
          row.wavelength_nm = spectrum.wavelength_nm;
          row.spectrum_weight = spectrum.weight;
          row.sun_mass = sun.mass;
          row.joint_sample_mass = 1.0 / request.sample_count;
          row.joint_proposal_density = 1.0;
          row.joint_importance_weight = 1.0;
          row.global_weight = spectrum.weight * sun.mass;
          row.status = spectrum.weight == 0.0 || sun.mass == 0.0 ? SceneMeasureStatus::kZeroWeight :
                                                                   SceneMeasureStatus::kConfirmed;
          row.evaluation_status = SceneMeasureStatus::kConfirmed;
          if (row.status == SceneMeasureStatus::kZeroWeight) {
            row.reason = "the global spectrum or sun source node has zero weight";
          }
          any_nonzero_source = any_nonzero_source || (spectrum.weight > 0.0 && sun.mass > 0.0);

          RandomNumberGenerator rng(row.replay_seed);
          double incident[3] = { sun.incident_direction[0], sun.incident_direction[1], sun.incident_direction[2] };
          double conditional_weight = 1.0;
          double conditional_measure_mass = 1.0;
          for (size_t layer_index = 0; layer_index < layers.size(); layer_index++) {
            SceneMeasureLayerRow layer =
                EvaluateLayer(rng, layers[layer_index], static_cast<int>(layer_index),
                              result.member_chains[member_index][layer_index], spectrum.refractive_index, incident,
                              request.include_derivatives, &row.latents);
            layer.source_sun_node_id = sun.node_id;
            layer.source_spectrum_node_id = spectrum.node_id;
            layer.source_wavelength_nm = spectrum.wavelength_nm;
            conditional_weight *= layer.crystal_share * layer.continuation_mass;
            conditional_measure_mass *= layer.crystal_share * layer.continuation_mass;
            if (layer.status == SceneMeasureStatus::kConfirmed) {
              conditional_weight *= analytic::kLiAreaPerEngineArea * layer.entry_measure * layer.fresnel_weight;
              std::copy(layer.outgoing_direction, layer.outgoing_direction + 3, incident);
            } else {
              conditional_weight = 0.0;
              row.evaluation_status = layer.status;
              row.evaluation_reason = "layer " + std::to_string(layer_index) + ": " + layer.reason;
              if (row.status != SceneMeasureStatus::kZeroWeight) {
                row.status = layer.status;
                row.reason = row.evaluation_reason;
              }
              any_unreachable = any_unreachable || layer.status == SceneMeasureStatus::kPhysicallyUnreachable;
              any_numerical = any_numerical || layer.status == SceneMeasureStatus::kNumericalIncomplete;
            }
            row.layers.push_back(std::move(layer));
            if (conditional_weight == 0.0) {
              break;
            }
          }
          double target_density = 1.0;
          for (const LatentMeasureSample& latent : row.latents) {
            row.joint_proposal_density *= latent.proposal_density_or_mass;
            target_density *= latent.target_density_or_mass;
            if (latent.status == SceneMeasureStatus::kNumericalIncomplete) {
              row.evaluation_status = SceneMeasureStatus::kNumericalIncomplete;
              row.evaluation_reason = "a generator latent has a non-finite density or mapping Jacobian";
              if (row.status != SceneMeasureStatus::kZeroWeight) {
                row.status = row.evaluation_status;
                row.reason = row.evaluation_reason;
              }
              conditional_weight = 0.0;
              any_numerical = true;
            }
          }
          row.joint_importance_weight =
              row.joint_proposal_density > 0.0 ? target_density / row.joint_proposal_density : 0.0;
          row.contribution = row.global_weight * row.joint_sample_mass * conditional_weight;
          result.sampled_measure_mass += row.global_weight * row.joint_sample_mass * conditional_measure_mass;
          result.total_contribution += row.contribution;
          if (sample < coarse_count) {
            result.coarse_contribution += 2.0 * row.contribution;
          }
          sun_partition_contribution[sun.node_id & 1] += row.contribution;
          spectrum_partition_contribution[spectrum.node_id & 1] += row.contribution;
          any_confirmed = any_confirmed || row.status == SceneMeasureStatus::kConfirmed;
          CountStatus(row.status, &result.status_counts);
          if (row.status == SceneMeasureStatus::kZeroWeight &&
              row.evaluation_status != SceneMeasureStatus::kConfirmed) {
            CountStatus(row.evaluation_status, &result.status_counts);
          }
          result.evaluated_row_count++;
          if (visitor) {
            visitor(row);
          }
          const uint32_t priority = MixSeed(row.replay_seed, 0x524f5753u);
          StoreRepresentativeRow(row, priority, &stored_priorities, &result);
        }
      }
    }
  }

  result.stored_row_count = static_cast<int>(result.rows.size());
  result.rows_truncated = result.evaluated_row_count > result.stored_row_count;
  result.joint_sampling_error_estimate = std::fabs(result.total_contribution - result.coarse_contribution);
  result.sun_node_error_estimate = approximate_sun ? SplitError(sun_partition_contribution, sun_partition_mass) : 0.0;
  result.spectrum_node_error_estimate =
      approximate_spectrum ? SplitError(spectrum_partition_contribution, spectrum_partition_mass) : 0.0;
  result.absolute_error_estimate = std::max(
      { result.joint_sampling_error_estimate, result.sun_node_error_estimate, result.spectrum_node_error_estimate });
  const double scale = std::max(std::fabs(result.total_contribution), std::fabs(result.coarse_contribution));
  const bool source_resolution_incomplete =
      (approximate_sun && result.sun_nodes.size() < 2) || (approximate_spectrum && result.spectrum_nodes.size() < 2) ||
      (approximate_sun && (!(sun_partition_mass[0] > 0.0) || !(sun_partition_mass[1] > 0.0))) ||
      (approximate_spectrum && (!(spectrum_partition_mass[0] > 0.0) || !(spectrum_partition_mass[1] > 0.0)));
  if (!any_nonzero_source) {
    result.status = SceneMeasureStatus::kZeroWeight;
    result.reason = "every spectrum node has zero weight";
  } else if (any_numerical) {
    result.status = SceneMeasureStatus::kNumericalIncomplete;
    result.reason = "at least one evaluated row ended in a numerical-incomplete state";
  } else if (!any_confirmed && any_unreachable && has_continuous_factor) {
    result.status = SceneMeasureStatus::kNumericalIncomplete;
    result.reason =
        "finite random sampling found no positive row; continuous support was not exhausted, so physical "
        "unreachability is not certified";
  } else if (!any_confirmed && any_unreachable) {
    result.status = SceneMeasureStatus::kPhysicallyUnreachable;
    result.reason = "the fully enumerated atomic member/source support had no positive complete-layer contribution";
  } else if (source_resolution_incomplete) {
    result.status = SceneMeasureStatus::kNumericalIncomplete;
    result.reason = "finite sun or illuminant support has fewer than two positive-mass resolution partitions";
  } else if (scale > 0.0 && result.absolute_error_estimate > 0.25 * scale) {
    result.status = SceneMeasureStatus::kNumericalIncomplete;
    result.reason =
        "at least one joint-sample, sun-node or spectrum-node split estimate differs by more than 25 percent";
  } else {
    result.status = SceneMeasureStatus::kConfirmed;
  }
  *out = std::move(result);
  return {};
}

}  // namespace lumice::raypath
