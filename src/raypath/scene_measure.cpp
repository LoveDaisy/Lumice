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
  const CrystalKind kind = KindOf(crystal.param_);
  const int* sync = ShapeSyncGroups(crystal.param_);
  std::vector<std::pair<int, int>> group_latents;
  for (int slot = 0; slot < kShapeScalarCount; slot++) {
    const Distribution* distribution = ShapeDistribution(crystal.param_, slot);
    if (distribution == nullptr) {
      continue;
    }
    int latent = -1;
    if (distribution->type != DistributionType::kNoRandom) {
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
                         distribution->type == DistributionType::kNoRandom ? 0 : 1, distribution->center,
                         distribution->spread, "product shape scalar", DistributionMeasure(*distribution, false),
                         "unit probability mass" });
  }

  const Distribution axis[3] = { crystal.axis_.latitude_dist, crystal.axis_.azimuth_dist, crystal.axis_.roll_dist };
  constexpr const char* kNames[3] = { "pose.latitude", "pose.azimuth", "pose.roll" };
  for (int i = 0; i < 3; i++) {
    const int latent = axis[i].type == DistributionType::kNoRandom ? -1 : (*next_latent)++;
    factors->push_back({ layer_index, kNames[i], axis[i].type, latent,
                         axis[i].type == DistributionType::kNoRandom ? 0 : 1, axis[i].center, axis[i].spread,
                         i == 0 ? "spherical latitude" : "angle", DistributionMeasure(axis[i], i == 0),
                         "unit probability mass" });
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
  std::vector<std::vector<std::vector<int>>> per_layer;
  for (size_t i = 0; i < layers.size(); i++) {
    if (request.member_selection == SceneMemberSelection::kConcrete) {
      per_layer.push_back({ request.path_layers[i] });
    } else {
      per_layer.push_back(ExpandPhysicalMembers(layers[i].setting->crystal_, request.path_layers[i]));
      if (request.member_selection == SceneMemberSelection::kPhysicalMask) {
        auto& members = per_layer.back();
        members.erase(std::remove_if(members.begin(), members.end(),
                                     [&](const std::vector<int>& member) {
                                       if (member.empty() || member.front() < 0 || member.front() >= 64) {
                                         return true;
                                       }
                                       return (request.physical_member_mask & (uint64_t{ 1 } << member.front())) == 0;
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
  const double radius_rad = static_cast<double>(sun.diameter_) * M_PI / 360.0;
  const double solid_angle = 2.0 * M_PI * (1.0 - std::cos(radius_rad));
  out->resize(static_cast<size_t>(request.sun_node_count));
  for (int i = 0; i < request.sun_node_count; i++) {
    float direction[3]{};
    SampleSphCapPointWithRng(rng, sun.azimuth_ + 180.0f, -sun.altitude_, sun.diameter_ / 2.0f, direction);
    SunMeasureNode& node = (*out)[static_cast<size_t>(i)];
    node.node_id = i;
    node.mass = solid_angle / request.sun_node_count;
    std::copy(direction, direction + 3, node.incident_direction);
  }
  return {};
}

analytic::CrystalShape SampleShape(RandomNumberGenerator& rng, const CrystalParam& param, const LayerInput& layer,
                                   std::vector<ShapeScalarSample>* samples) {
  analytic::CrystalShape shape;
  const CrystalKind kind = KindOf(param);
  auto add = [&](int slot, double value) {
    samples->push_back({ ShapeName(kind, slot), value, layer.shape_latent[slot] });
  };
  std::visit(
      [&](const auto& p) {
        using T = std::decay_t<decltype(p)>;
        float distances[6]{};
        if constexpr (std::is_same_v<T, PrismCrystalParam>) {
          shape.kind = analytic::CrystalShapeKind::kPrism;
          shape.height = SamplePrismShapeScalars(rng, p, distances);
          add(kShapeScalarHeight, shape.height);
        } else {
          shape.kind = analytic::CrystalShapeKind::kPyramid;
          float upper = 0.0f;
          float prism = 0.0f;
          float lower = 0.0f;
          SamplePyramidShapeScalars(rng, p, upper, prism, lower, distances);
          shape.height = prism;
          shape.upper_h = upper;
          shape.lower_h = lower;
          shape.upper_wedge_deg = p.wedge_angle_u_;
          shape.lower_wedge_deg = p.wedge_angle_l_;
          add(kShapeScalarUpperH, upper);
          add(kShapeScalarPrismH, prism);
          add(kShapeScalarLowerH, lower);
        }
        for (int i = 0; i < 6; i++) {
          shape.face_distance[i] = distances[i];
          add(kShapeScalarFace0 + i, distances[i]);
        }
      },
      param);
  return shape;
}

SceneMeasureStatus FieldStatus(const analytic::DiagnosticFieldResult& field, std::string* reason) {
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
                                   bool include_derivatives) {
  SceneMeasureLayerRow out;
  out.layer_index = layer_index;
  out.crystal_id = input.setting->crystal_.id_;
  out.faces = faces;
  out.crystal_share = input.crystal_share;
  out.continuation_mass = input.continuation_mass;
  std::copy(incident, incident + 3, out.incident_direction);

  const analytic::CrystalShape shape = SampleShape(rng, input.setting->crystal_.param_, input, &out.shape);
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

  float pose_values[3]{};
  RandomSampler::SampleAxisPose(rng, input.setting->crystal_.axis_, pose_values);
  std::copy(pose_values, pose_values + 3, out.pose_lon_lat_roll_rad);
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

}  // namespace

Error BuildSceneMeasure(const ConfigManager& config, const SceneMeasureRequest& request, SceneMeasureResult* out) {
  *out = SceneMeasureResult{};
  if (request.sample_count < 2 || request.sample_count > kMaxSceneMeasureSampleCount || request.sample_count % 2 != 0) {
    return { ErrorCode::kInvalidArgument, "sample_count must be an even integer in [2, 1000000]" };
  }

  SceneMeasureResult result;
  result.seed = request.seed;
  result.requested_sample_count = request.sample_count;
  result.units = "LI a=1 finite-crystal area times scene spectral weight times solar solid angle (m^2 sr)";
  result.normalization =
      "shape and pose are unit probability measures; finite solar discs retain solid angle, while spectrum and "
      "physical members retain raw weights";

  std::vector<LayerInput> layers;
  if (const Error error = ResolveLayers(config, request, &layers, &result.factors); !error.Ok()) {
    return error;
  }
  result.factors.insert(
      result.factors.begin(),
      { -1, "sun_disc", DistributionType::kNoRandom, -1, config.scene_.light_source_.param_.diameter_ == 0.0f ? 0 : 2,
        config.scene_.light_source_.param_.diameter_, 0.0, "spherical cap diameter (degrees)",
        config.scene_.light_source_.param_.diameter_ == 0.0f ? "atom" : "spherical cap with solid-angle mass",
        "unit probability mass" });
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
  for (const SpectrumMeasureNode& spectrum : result.spectrum_nodes) {
    for (const SunMeasureNode& sun : result.sun_nodes) {
      for (size_t member_index = 0; member_index < result.member_chains.size(); member_index++) {
        for (int sample = 0; sample < request.sample_count; sample++) {
          SceneMeasureRow row;
          row.spectrum_node_id = spectrum.node_id;
          row.sun_node_id = sun.node_id;
          row.member_chain_index = static_cast<int>(member_index);
          row.sample_index = sample;
          row.wavelength_nm = spectrum.wavelength_nm;
          row.spectrum_weight = spectrum.weight;
          row.sun_mass = sun.mass;
          row.joint_sample_mass = 1.0 / request.sample_count;
          row.joint_proposal_density = row.joint_sample_mass;
          row.joint_importance_weight = 1.0;
          row.global_weight = spectrum.weight * sun.mass;
          row.status = spectrum.weight == 0.0 ? SceneMeasureStatus::kZeroWeight : SceneMeasureStatus::kConfirmed;
          any_nonzero_source = any_nonzero_source || spectrum.weight > 0.0;

          RandomNumberGenerator rng(
              RowSeed(request.seed, spectrum.node_id, sun.node_id, static_cast<int>(member_index), sample));
          double incident[3] = { sun.incident_direction[0], sun.incident_direction[1], sun.incident_direction[2] };
          double conditional_weight = 1.0;
          double conditional_measure_mass = 1.0;
          for (size_t layer_index = 0; layer_index < layers.size(); layer_index++) {
            SceneMeasureLayerRow layer =
                EvaluateLayer(rng, layers[layer_index], static_cast<int>(layer_index),
                              result.member_chains[member_index][layer_index], spectrum.refractive_index, incident,
                              request.include_derivatives);
            conditional_weight *= layer.crystal_share * layer.continuation_mass;
            conditional_measure_mass *= layer.crystal_share * layer.continuation_mass;
            if (layer.status == SceneMeasureStatus::kConfirmed) {
              conditional_weight *= analytic::kLiAreaPerEngineArea * layer.entry_measure * layer.fresnel_weight;
              std::copy(layer.outgoing_direction, layer.outgoing_direction + 3, incident);
            } else {
              conditional_weight = 0.0;
              row.status = layer.status;
              row.reason = "layer " + std::to_string(layer_index) + ": " + layer.reason;
              any_unreachable = true;
            }
            row.layers.push_back(std::move(layer));
            if (conditional_weight == 0.0) {
              break;
            }
          }
          row.contribution = row.global_weight * row.joint_sample_mass * conditional_weight;
          result.sampled_measure_mass += row.global_weight * row.joint_sample_mass * conditional_measure_mass;
          result.total_contribution += row.contribution;
          if (sample < coarse_count) {
            result.coarse_contribution += 2.0 * row.contribution;
          }
          any_confirmed = any_confirmed || row.status == SceneMeasureStatus::kConfirmed;
          result.evaluated_row_count++;
          if (result.rows.size() < kMaxStoredSceneMeasureRows) {
            result.rows.push_back(std::move(row));
          } else {
            result.rows_truncated = true;
          }
        }
      }
    }
  }

  result.stored_row_count = static_cast<int>(result.rows.size());
  result.absolute_error_estimate = std::fabs(result.total_contribution - result.coarse_contribution);
  const double scale = std::max(std::fabs(result.total_contribution), std::fabs(result.coarse_contribution));
  if (!any_nonzero_source) {
    result.status = SceneMeasureStatus::kZeroWeight;
    result.reason = "every spectrum node has zero weight";
  } else if (!any_confirmed && any_unreachable) {
    result.status = SceneMeasureStatus::kPhysicallyUnreachable;
    result.reason = "no requested member chain had a positive complete-layer contribution";
  } else if (scale > 0.0 && result.absolute_error_estimate > 0.25 * scale) {
    result.status = SceneMeasureStatus::kNumericalIncomplete;
    result.reason = "coarse and fine joint-sample estimates differ by more than 25 percent";
  } else {
    result.status = SceneMeasureStatus::kConfirmed;
  }
  *out = std::move(result);
  return {};
}

}  // namespace lumice::raypath
