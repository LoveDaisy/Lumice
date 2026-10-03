#include "raypath/scene_measure.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

#include "analytic/path_evaluation.hpp"
#include "core/crystal.hpp"
#include "core/filter_spec.hpp"
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

const char* SceneMeasureNumericStatusName(SceneMeasureNumericStatus status) {
  switch (status) {
    case SceneMeasureNumericStatus::kAvailable:
      return "available";
    case SceneMeasureNumericStatus::kExactZero:
      return "exact_zero";
    case SceneMeasureNumericStatus::kUnderflow:
      return "underflow";
    case SceneMeasureNumericStatus::kOverflow:
      return "overflow";
    case SceneMeasureNumericStatus::kInvalid:
      return "invalid";
  }
  return "invalid";
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

struct LayerInput {
  struct Entry {
    const ScatteringSetting* setting = nullptr;
    int entry_index = -1;
    double scene_share = 0.0;
  };

  const MsInfo* ms = nullptr;
  const ScatteringSetting* setting = nullptr;
  std::vector<Entry> entries;
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

struct NumericValue {
  double value = 0.0;
  SceneMeasureNumericStatus status = SceneMeasureNumericStatus::kExactZero;
};

bool IsRepresentable(const NumericValue& value) {
  return value.status == SceneMeasureNumericStatus::kAvailable || value.status == SceneMeasureNumericStatus::kExactZero;
}

NumericValue CheckedNonnegative(double value) {
  if (!std::isfinite(value) || value < 0.0) {
    return { std::numeric_limits<double>::quiet_NaN(), SceneMeasureNumericStatus::kInvalid };
  }
  return { value, value == 0.0 ? SceneMeasureNumericStatus::kExactZero : SceneMeasureNumericStatus::kAvailable };
}

NumericValue CheckedMultiply(const NumericValue& lhs, const NumericValue& rhs) {
  if (!IsRepresentable(lhs)) {
    return lhs;
  }
  if (!IsRepresentable(rhs)) {
    return rhs;
  }
  if (lhs.value == 0.0 || rhs.value == 0.0) {
    return {};
  }
  if (lhs.value > std::numeric_limits<double>::max() / rhs.value) {
    return { std::numeric_limits<double>::infinity(), SceneMeasureNumericStatus::kOverflow };
  }
  const double product = lhs.value * rhs.value;
  if (product == 0.0) {
    return { 0.0, SceneMeasureNumericStatus::kUnderflow };
  }
  return CheckedNonnegative(product);
}

NumericValue CheckedMultiply(const NumericValue& lhs, double rhs) {
  return CheckedMultiply(lhs, CheckedNonnegative(rhs));
}

struct ConditionalMeasureLedger {
  NumericValue product{ 1.0, SceneMeasureNumericStatus::kAvailable };
  bool strictly_positive = true;
  int first_zero_layer = -1;
  std::string first_zero_factor;
};

ConditionalMeasureLedger BuildConditionalMeasureLedger(const std::vector<LayerInput>& layers) {
  ConditionalMeasureLedger ledger;
  for (size_t layer_index = 0; layer_index < layers.size(); ++layer_index) {
    const NumericValue crystal_share = CheckedNonnegative(layers[layer_index].crystal_share);
    const NumericValue continuation_mass = CheckedNonnegative(layers[layer_index].continuation_mass);
    if (crystal_share.status == SceneMeasureNumericStatus::kExactZero && ledger.first_zero_layer < 0) {
      ledger.first_zero_layer = static_cast<int>(layer_index);
      ledger.first_zero_factor = "crystal share";
    } else if (continuation_mass.status == SceneMeasureNumericStatus::kExactZero && ledger.first_zero_layer < 0) {
      ledger.first_zero_layer = static_cast<int>(layer_index);
      ledger.first_zero_factor = "continuation/exit mass";
    }
    ledger.strictly_positive = ledger.strictly_positive && crystal_share.value > 0.0 && continuation_mass.value > 0.0;
    ledger.product = CheckedMultiply(ledger.product, CheckedMultiply(crystal_share, continuation_mass));
  }
  // An exact zero anywhere in the complete factor ledger is authoritative even if an earlier
  // positive prefix underflowed. This is a statement about the measure, not floating-point
  // representability or how far field evaluation happened to progress.
  if (ledger.first_zero_layer >= 0) {
    ledger.product = {};
  }
  return ledger;
}

NumericValue CheckedAdd(const NumericValue& lhs, const NumericValue& rhs) {
  if (!IsRepresentable(lhs)) {
    return lhs;
  }
  if (!IsRepresentable(rhs)) {
    return rhs;
  }
  if (lhs.value > std::numeric_limits<double>::max() - rhs.value) {
    return { std::numeric_limits<double>::infinity(), SceneMeasureNumericStatus::kOverflow };
  }
  return CheckedNonnegative(lhs.value + rhs.value);
}

NumericValue CheckedDivide(const NumericValue& numerator, const NumericValue& denominator) {
  if (!IsRepresentable(numerator)) {
    return numerator;
  }
  if (!IsRepresentable(denominator) || denominator.value == 0.0) {
    return { std::numeric_limits<double>::quiet_NaN(), SceneMeasureNumericStatus::kInvalid };
  }
  if (numerator.value == 0.0) {
    return {};
  }
  const double quotient = numerator.value / denominator.value;
  if (!std::isfinite(quotient)) {
    return { quotient, SceneMeasureNumericStatus::kOverflow };
  }
  if (quotient == 0.0) {
    return { 0.0, SceneMeasureNumericStatus::kUnderflow };
  }
  return CheckedNonnegative(quotient);
}

NumericValue CheckedAbsoluteDifference(const NumericValue& lhs, const NumericValue& rhs) {
  if (!IsRepresentable(lhs)) {
    return lhs;
  }
  if (!IsRepresentable(rhs)) {
    return rhs;
  }
  return CheckedNonnegative(std::fabs(lhs.value - rhs.value));
}

NumericValue CheckedMax(const NumericValue& first, const NumericValue& second, const NumericValue& third) {
  for (const NumericValue* value : { &first, &second, &third }) {
    if (!IsRepresentable(*value)) {
      return *value;
    }
  }
  return CheckedNonnegative(std::max({ first.value, second.value, third.value }));
}

NumericValue MaterializeLogProduct(double log_value, bool exact_zero) {
  if (exact_zero) {
    return {};
  }
  if (!std::isfinite(log_value)) {
    return { std::numeric_limits<double>::quiet_NaN(), SceneMeasureNumericStatus::kInvalid };
  }
  static const double kLogMax = std::log(std::numeric_limits<double>::max());
  static const double kLogMin = std::log(std::numeric_limits<double>::denorm_min());
  if (log_value > kLogMax) {
    return { std::numeric_limits<double>::infinity(), SceneMeasureNumericStatus::kOverflow };
  }
  if (log_value < kLogMin) {
    return { 0.0, SceneMeasureNumericStatus::kUnderflow };
  }
  const double value = std::exp(log_value);
  if (!std::isfinite(value)) {
    return { value, SceneMeasureNumericStatus::kOverflow };
  }
  if (value == 0.0) {
    return { 0.0, SceneMeasureNumericStatus::kUnderflow };
  }
  return { value, SceneMeasureNumericStatus::kAvailable };
}

void AssignNumeric(const NumericValue& source, double* value, SceneMeasureNumericStatus* status) {
  *value = source.value;
  *status = source.status;
}

bool DistributionHasPositiveWidth(const Distribution& distribution);

std::string DistributionMeasure(const Distribution& distribution, bool spherical_latitude) {
  if (!DistributionHasPositiveWidth(distribution)) {
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
  struct ShapeLatentGroup {
    int sync_group = 0;
    int leader_slot = -1;
    int latent_id = -1;
    const Distribution* distribution = nullptr;
  };
  std::vector<ShapeLatentGroup> groups;
  for (int slot = 0; slot < kShapeScalarCount; slot++) {
    const Distribution* declared_distribution = ShapeDistribution(crystal.param_, slot);
    if (declared_distribution == nullptr) {
      continue;
    }
    const Distribution* generator_distribution = declared_distribution;
    int latent = -1;
    int leader_slot = slot;
    if (sync[slot] != 0) {
      const auto found = std::find_if(groups.begin(), groups.end(),
                                      [&](const ShapeLatentGroup& group) { return group.sync_group == sync[slot]; });
      if (found == groups.end()) {
        if (DistributionHasPositiveWidth(*generator_distribution)) {
          latent = (*next_latent)++;
        }
        groups.push_back({ sync[slot], slot, latent, generator_distribution });
      } else {
        leader_slot = found->leader_slot;
        latent = found->latent_id;
        generator_distribution = found->distribution;
      }
    } else if (DistributionHasPositiveWidth(*generator_distribution)) {
      latent = (*next_latent)++;
    }
    const bool has_positive_width = DistributionHasPositiveWidth(*generator_distribution);
    layer->shape_latent[slot] = latent;
    factors->push_back({ layer_index, "shape." + ShapeName(kind, slot), generator_distribution->type, latent,
                         has_positive_width ? 1 : 0, generator_distribution->center, generator_distribution->spread,
                         leader_slot == slot ?
                             "product shape scalar" :
                             "synchronized shape scalar driven by shape." + ShapeName(kind, leader_slot),
                         DistributionMeasure(*generator_distribution, false), "unit probability mass" });
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
  if (DistributionHasPositiveWidth(crystal.axis_.latitude_dist) &&
      lat_path::SelectLatPath(crystal.axis_).kind == lat_path::LatPathKind::kLutInverseCdf) {
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
    double total = 0.0;
    double selected_total = 0.0;
    LayerInput layer;
    layer.ms = &ms;
    for (size_t entry_index = 0; entry_index < ms.setting_.size(); ++entry_index) {
      const ScatteringSetting& setting = ms.setting_[entry_index];
      if (!std::isfinite(setting.crystal_proportion_)) {
        return { ErrorCode::kInvalidArgument, "layer " + std::to_string(i) + " has a non-finite crystal proportion" };
      }
      const double proportion = std::max(0.0, static_cast<double>(setting.crystal_proportion_));
      total += proportion;
      if (setting.crystal_.id_ == crystal_id) {
        if (layer.setting == nullptr) {
          layer.setting = &setting;
        }
        layer.entries.push_back({ &setting, static_cast<int>(entry_index), proportion });
        selected_total += proportion;
      }
    }
    if (layer.setting == nullptr) {
      return { ErrorCode::kUnknownCrystalId,
               "layer " + std::to_string(i) + " has no crystal entry with id " + std::to_string(crystal_id) };
    }
    if (!std::isfinite(total) || !std::isfinite(selected_total)) {
      return { ErrorCode::kInvalidArgument, "layer " + std::to_string(i) + " has overflowing crystal proportions" };
    }
    if (!std::isfinite(ms.prob_) || ms.prob_ < 0.0f || ms.prob_ > 1.0f) {
      return { ErrorCode::kInvalidArgument, "layer " + std::to_string(i) + " has an invalid continuation probability" };
    }
    layer.crystal_share = total > 0.0 ? selected_total / total : 0.0;
    for (LayerInput::Entry& entry : layer.entries) {
      entry.scene_share = total > 0.0 ? entry.scene_share / total : 0.0;
    }
    layer.continuation_mass = i + 1 < request.path_layers.size() ? ms.prob_ : 1.0 - ms.prob_;
    AddFactorDescriptors(static_cast<int>(i), layer.setting->crystal_, &layer, &next_latent, factors);
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

bool CouldFaceExistInShapeSupport(const CrystalParam& param, int face) {
  return std::visit([face](const auto& shape) { return CouldFaceExist(shape, static_cast<IdType>(face)); }, param);
}

Error ValidateMemberSequence(const LayerInput& layer, const std::vector<int>& faces, const std::string& name) {
  if (faces.empty() || faces.size() > kMaxSceneMeasureFacesPerLayer) {
    return { ErrorCode::kInvalidPath,
             name + " must contain 1 to " + std::to_string(kMaxSceneMeasureFacesPerLayer) + " faces" };
  }
  const CrystalParam& param = layer.setting->crystal_.param_;
  const CrystalKind kind = KindOf(param);
  for (const int face : faces) {
    if (!IsLegalFace(kind, face)) {
      return { ErrorCode::kInvalidPath,
               name + " contains face " + std::to_string(face) + ", which is not legal for this crystal kind" };
    }
    if (!CouldFaceExistInShapeSupport(param, face)) {
      return { ErrorCode::kInvalidPath, name + " contains face " + std::to_string(face) +
                                            ", which cannot exist in this crystal's shape support" };
    }
  }
  return {};
}

Error BuildMemberChains(const SceneMeasureRequest& request, const std::vector<LayerInput>& layers,
                        std::vector<std::vector<std::vector<int>>>* out) {
  for (size_t layer_index = 0; layer_index < layers.size(); ++layer_index) {
    if (const Error error = ValidateMemberSequence(layers[layer_index], request.path_layers[layer_index],
                                                   "requested path layer " + std::to_string(layer_index));
        !error.Ok()) {
      return error;
    }
  }
  if (request.member_selection == SceneMemberSelection::kExplicitChains) {
    if (request.explicit_member_chains.empty()) {
      return { ErrorCode::kInvalidArgument, "explicit member selection requires at least one member chain" };
    }
    if (request.explicit_member_chains.size() > kMaxSceneMeasureMemberChainCount) {
      return { ErrorCode::kInvalidArgument, "explicit member selection exceeds the 4096-chain bound" };
    }
    for (size_t chain_index = 0; chain_index < request.explicit_member_chains.size(); chain_index++) {
      const auto& chain = request.explicit_member_chains[chain_index];
      if (chain.size() != layers.size()) {
        return { ErrorCode::kInvalidPath, "explicit member chain " + std::to_string(chain_index) +
                                              " must contain exactly one face sequence per layer" };
      }
      for (size_t layer_index = 0; layer_index < chain.size(); ++layer_index) {
        if (const Error error = ValidateMemberSequence(
                layers[layer_index], chain[layer_index],
                "explicit member chain " + std::to_string(chain_index) + " layer " + std::to_string(layer_index));
            !error.Ok()) {
          return error;
        }
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
    if (members.size() > kMaxSceneMeasureMemberChainCount ||
        out->size() > kMaxSceneMeasureMemberChainCount / members.size()) {
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
    const Distribution* distribution = ShapeDistribution(param, trace.leader_slot);
    if (distribution == nullptr) {
      continue;
    }
    const int latent_id = layer.shape_latent[trace.slot];
    const std::string name = ShapeName(kind, trace.slot);
    samples->push_back({ trace.slot, name, trace.mapped_value, latent_id, trace.sync_group, trace.leader_slot,
                         trace.raw_value, trace.absolute_value_fold, trace.mapping_jacobian });
    AppendLatent(latent_id, layer_index, "shape." + ShapeName(kind, trace.leader_slot),
                 DistributionBaseMeasure(*distribution, trace.draw), trace.draw,
                 trace.sync_group == 0 ? "distribution latent -> scalar" :
                                         "leader distribution latent -> synchronized shape scalars",
                 latents);
  }
  return shape;
}

void MultiplyMatrix3(const double lhs[9], const double rhs[9], double out[9]) {
  for (int row = 0; row < 3; row++) {
    for (int column = 0; column < 3; column++) {
      out[row * 3 + column] = 0.0;
      for (int inner = 0; inner < 3; inner++) {
        out[row * 3 + column] += lhs[row * 3 + inner] * rhs[inner * 3 + column];
      }
    }
  }
}

void FillPoseTangent(const float pose[3], double derivatives[27]) {
  const double azimuth = static_cast<double>(pose[0]) - math::kPi;
  const double latitude = static_cast<double>(pose[1]) - math::kPi_2;
  const double roll = pose[2];
  const double ca = std::cos(azimuth);
  const double sa = std::sin(azimuth);
  const double cl = std::cos(latitude);
  const double sl = std::sin(latitude);
  const double cr = std::cos(roll);
  const double sr = std::sin(roll);
  const double rz_azimuth[9] = { ca, -sa, 0.0, sa, ca, 0.0, 0.0, 0.0, 1.0 };
  const double ry_latitude[9] = { cl, 0.0, sl, 0.0, 1.0, 0.0, -sl, 0.0, cl };
  const double rz_roll[9] = { cr, -sr, 0.0, sr, cr, 0.0, 0.0, 0.0, 1.0 };
  const double drz_azimuth[9] = { -sa, -ca, 0.0, ca, -sa, 0.0, 0.0, 0.0, 0.0 };
  const double dry_latitude[9] = { -sl, 0.0, cl, 0.0, 0.0, 0.0, -cl, 0.0, -sl };
  const double drz_roll[9] = { -sr, -cr, 0.0, cr, -sr, 0.0, 0.0, 0.0, 0.0 };
  double intermediate[9]{};
  MultiplyMatrix3(drz_azimuth, ry_latitude, intermediate);
  MultiplyMatrix3(intermediate, rz_roll, derivatives);
  MultiplyMatrix3(rz_azimuth, dry_latitude, intermediate);
  MultiplyMatrix3(intermediate, rz_roll, derivatives + 9);
  MultiplyMatrix3(rz_azimuth, ry_latitude, intermediate);
  MultiplyMatrix3(intermediate, drz_roll, derivatives + 18);
}

int PoseSupportRank(const float pose[3], const bool active[3], double derivatives[27]) {
  FillPoseTangent(pose, derivatives);
  const int active_count = static_cast<int>(active[0]) + static_cast<int>(active[1]) + static_cast<int>(active[2]);
  if (active_count < 2) {
    return active_count;
  }

  // In this Z-Y-Z chart, longitude and roll generate the same rotation direction exactly at a
  // pole. Test the chart identity rather than a floating Gram-Schmidt residual. If latitude also
  // has positive width, the sampled float can round onto the singular chart even though nearby
  // generator coordinates span all of SO(3); report that local rank as unavailable instead of
  // misclassifying the continuous support as strictly two-dimensional.
  const float pole_remainder = std::remainder(pose[1] - math::kPi_2, math::kPi);
  const bool at_pole = pole_remainder == 0.0f;
  if (!at_pole || !active[0] || !active[2]) {
    return active_count;
  }
  return active[1] ? -1 : active_count - 1;
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

Crystal ProductCrystal(const analytic::CrystalShape& shape) {
  float distances[6];
  for (int face = 0; face < 6; face++) {
    distances[face] = static_cast<float>(shape.face_distance[face]);
  }
  if (shape.kind == analytic::CrystalShapeKind::kPrism) {
    return Crystal::CreatePrism(static_cast<float>(std::fabs(shape.height)), distances);
  }
  return Crystal::CreatePyramid(static_cast<float>(shape.upper_wedge_deg), static_cast<float>(shape.lower_wedge_deg),
                                static_cast<float>(std::fabs(shape.upper_h)),
                                static_cast<float>(std::fabs(shape.height)),
                                static_cast<float>(std::fabs(shape.lower_h)), distances);
}

const char* SimpleFilterTypeName(const SimpleFilterParam& param) {
  if (std::holds_alternative<NoneFilterParam>(param)) {
    return "none";
  }
  if (std::holds_alternative<RaypathFilterParam>(param)) {
    return "raypath";
  }
  if (std::holds_alternative<EntryExitFilterParam>(param)) {
    return "entry_exit";
  }
  if (std::holds_alternative<DirectionFilterParam>(param)) {
    return "direction";
  }
  return "crystal";
}

const char* FilterTypeName(const FilterParam& param) {
  if (const auto* simple = std::get_if<SimpleFilterParam>(&param)) {
    return SimpleFilterTypeName(*simple);
  }
  return "complex";
}

bool FilterAcceptanceSupportConstant(const FilterConfig& filter) {
  if (filter.symmetry_ != FilterConfig::kSymNone) {
    return false;
  }
  const auto simple_is_constant = [](const SimpleFilterParam& param) {
    return !std::holds_alternative<DirectionFilterParam>(param);
  };
  if (const auto* simple = std::get_if<SimpleFilterParam>(&filter.param_)) {
    return simple_is_constant(*simple);
  }
  const auto& complex = std::get<ComplexFilterParam>(filter.param_);
  return std::all_of(complex.filters_.begin(), complex.filters_.end(), [&](const auto& clause) {
    return std::all_of(clause.begin(), clause.end(), [&](const auto& term) { return simple_is_constant(term.second); });
  });
}

struct FixedFilterEntryDecision {
  bool evaluated = false;
  bool accepted = false;
};

struct FixedFilterLayerLedger {
  std::vector<FixedFilterEntryDecision> entries;
  bool rejection_certified = false;
};

struct FixedFilterLedger {
  std::vector<FixedFilterLayerLedger> layers;
  int first_zero_layer = -1;
};

const uint8_t* PopulateFilterRecorder(const std::vector<int>& faces, RaypathRecorder* recorder,
                                      std::array<uint8_t, kMaxHits>* overflow) {
  recorder->Clear();
  if (faces.size() <= RaypathRecorder::kInlineCap) {
    for (const int face : faces) {
      *recorder << static_cast<IdType>(face);
    }
    return nullptr;
  }
  recorder->size_ = static_cast<uint8_t>(faces.size());
  recorder->overflow_idx_ = 0;
  for (size_t index = 0; index < faces.size(); ++index) {
    (*overflow)[index] = static_cast<uint8_t>(faces[index] & 0xff);
  }
  return overflow->data();
}

bool FitsFilterRecorder(const std::vector<int>& faces) {
  return faces.size() <= kMaxHits && std::all_of(faces.begin(), faces.end(), [](int face) {
           return face >= 0 && face <= static_cast<int>(std::numeric_limits<IdType>::max());
         });
}

RaySeg FilterRay(IdType crystal_id, const double* outgoing_direction) {
  RaySeg ray{};
  if (outgoing_direction != nullptr) {
    for (int coordinate = 0; coordinate < 3; ++coordinate) {
      ray.d_[coordinate] = static_cast<float>(outgoing_direction[coordinate]);
    }
  }
  ray.w_ = 1.0f;
  ray.from_face_ = kInvalidId;
  ray.to_face_ = kInvalidId;
  ray.crystal_idx_ = 0;
  ray.crystal_config_id_ = crystal_id;
  return ray;
}

FixedFilterLayerLedger EvaluateFixedFilterLayer(const LayerInput& input, const std::vector<int>& faces) {
  FixedFilterLayerLedger ledger;
  ledger.entries.resize(input.entries.size());
  // The path resolver validates a member against its sampled crystal later. Until then, do not
  // narrow an unrepresentable public face id into the uint8_t recorder: it could alias a different
  // valid member and incorrectly certify a zero filter weight.
  if (!FitsFilterRecorder(faces)) {
    return ledger;
  }

  // Support-constant filters have neither direction predicates nor label-symmetry expansion, so
  // their Check result depends only on the fixed member recorder and crystal config id. Use an
  // isolated product draw solely to drive the runtime FilterSpec factory; its geometry cannot
  // affect this deliberately conservative subset, and the scene-measure replay RNG is untouched.
  RandomNumberGenerator probe_rng(1);
  const Crystal probe_crystal = MakeCrystal(probe_rng, input.setting->crystal_.param_);
  RaypathRecorder recorder;
  std::array<uint8_t, kMaxHits> overflow{};
  const uint8_t* overflow_ptr = PopulateFilterRecorder(faces, &recorder, &overflow);
  const RaySeg ray = FilterRay(input.setting->crystal_.id_, nullptr);

  bool has_positive_share = false;
  bool every_positive_share_rejected = true;
  for (size_t entry_index = 0; entry_index < input.entries.size(); ++entry_index) {
    const LayerInput::Entry& input_entry = input.entries[entry_index];
    if (!(input_entry.scene_share > 0.0)) {
      continue;
    }
    has_positive_share = true;
    const FilterConfig& filter = input_entry.setting->filter_;
    if (!FilterAcceptanceSupportConstant(filter)) {
      every_positive_share_rejected = false;
      continue;
    }
    const auto spec = FilterSpec::Create(filter, probe_crystal, input_entry.setting->crystal_.axis_);
    const bool accepted = spec->Check(ray, recorder, overflow_ptr);
    ledger.entries[entry_index] = { true, accepted };
    every_positive_share_rejected = every_positive_share_rejected && !accepted;
  }
  ledger.rejection_certified = has_positive_share && every_positive_share_rejected;
  return ledger;
}

FixedFilterLedger BuildFixedFilterLedger(const std::vector<LayerInput>& layers,
                                         const std::vector<std::vector<int>>& member_chain) {
  FixedFilterLedger ledger;
  ledger.layers.reserve(layers.size());
  for (size_t layer_index = 0; layer_index < layers.size(); ++layer_index) {
    ledger.layers.push_back(EvaluateFixedFilterLayer(layers[layer_index], member_chain[layer_index]));
    if (ledger.first_zero_layer < 0 && ledger.layers.back().rejection_certified) {
      ledger.first_zero_layer = static_cast<int>(layer_index);
    }
  }
  return ledger;
}

bool MemberChainExistsInSample(uint32_t replay_seed, const std::vector<LayerInput>& layers,
                               const std::vector<std::vector<int>>& member_chain) {
  RandomNumberGenerator rng(replay_seed);
  for (size_t layer_index = 0; layer_index < layers.size(); ++layer_index) {
    std::vector<ShapeScalarSample> shape_samples;
    std::vector<LatentMeasureSample> latents;
    const analytic::CrystalShape shape =
        SampleShape(rng, layers[layer_index].setting->crystal_.param_, layers[layer_index],
                    static_cast<int>(layer_index), &shape_samples, &latents);
    float pose_values[3]{};
    RandomSampler::SampleAxisPoseWithTrace(rng, layers[layer_index].setting->crystal_.axis_, pose_values);
    analytic::FaceNormalTable normals;
    if (analytic::BuildFaceNormals(shape, &normals) != analytic::Status::kOk) {
      return false;
    }
    std::vector<int> slots;
    if (!ResolveDiagnosticLayerPath(member_chain[layer_index], normals, &slots).Ok()) {
      return false;
    }
  }
  return true;
}

void PopulateEntryRows(const LayerInput& input, SceneMeasureLayerRow* out) {
  out->entries.reserve(input.entries.size());
  for (const LayerInput::Entry& input_entry : input.entries) {
    const FilterConfig& filter = input_entry.setting->filter_;
    out->entries.push_back({ input_entry.entry_index, filter.id_,
                             std::max(0.0, static_cast<double>(input_entry.setting->crystal_proportion_)),
                             input_entry.scene_share, 0.0, false, false, FilterAcceptanceSupportConstant(filter),
                             FilterTypeName(filter.param_),
                             filter.action_ == FilterConfig::kFilterIn ? "filter_in" : "filter_out",
                             FilterSymmetryToString(filter.symmetry_) });
  }
}

void ApplyFixedFilterLedger(const FixedFilterLayerLedger& ledger, SceneMeasureLayerRow* out) {
  for (size_t entry_index = 0; entry_index < ledger.entries.size(); ++entry_index) {
    const FixedFilterEntryDecision& decision = ledger.entries[entry_index];
    if (!decision.evaluated) {
      continue;
    }
    SceneMeasureEntryRow& entry = out->entries[entry_index];
    entry.filter_evaluated = true;
    entry.accepted = decision.accepted;
    entry.accepted_share = decision.accepted ? entry.scene_share : 0.0;
  }
  out->filter_rejection_certified = ledger.rejection_certified;
}

void ApplyPhysicalEntryMixture(const LayerInput& input, const Crystal& crystal, const std::vector<int>& faces,
                               SceneMeasureLayerRow* out) {
  RaypathRecorder recorder;
  std::array<uint8_t, kMaxHits> overflow{};
  const uint8_t* overflow_ptr = PopulateFilterRecorder(faces, &recorder, &overflow);
  const RaySeg ray = FilterRay(input.setting->crystal_.id_, out->outgoing_direction);

  double accepted_share = 0.0;
  for (size_t entry_index = 0; entry_index < input.entries.size(); ++entry_index) {
    const LayerInput::Entry& input_entry = input.entries[entry_index];
    const FilterConfig& filter = input_entry.setting->filter_;
    const auto spec = FilterSpec::Create(filter, crystal, input_entry.setting->crystal_.axis_);
    const bool accepted = spec->Check(ray, recorder, overflow_ptr);
    const double entry_accepted_share = accepted ? input_entry.scene_share : 0.0;
    accepted_share += entry_accepted_share;
    out->entries[entry_index].accepted_share = entry_accepted_share;
    out->entries[entry_index].filter_evaluated = true;
    out->entries[entry_index].accepted = accepted;
  }
  out->crystal_share = accepted_share;
  out->filter_rejection_certified =
      accepted_share == 0.0 &&
      std::all_of(out->entries.begin(), out->entries.end(), [](const SceneMeasureEntryRow& entry) {
        return entry.scene_share == 0.0 ||
               (entry.filter_evaluated && !entry.accepted && entry.acceptance_support_constant);
      });
}

SceneMeasureLayerRow EvaluateLayer(RandomNumberGenerator& rng, const LayerInput& input, int layer_index,
                                   const std::vector<int>& faces, double refractive_index, const double incident[3],
                                   bool include_derivatives, const FixedFilterLayerLedger& fixed_filter_ledger,
                                   std::vector<LatentMeasureSample>* latents) {
  SceneMeasureLayerRow out;
  out.layer_index = layer_index;
  out.crystal_id = input.setting->crystal_.id_;
  out.crystal_kind = KindOf(input.setting->crystal_.param_) == CrystalKind::kPrism ? "prism" : "pyramid";
  out.faces = faces;
  out.selected_crystal_share = input.crystal_share;
  out.crystal_share = input.crystal_share;
  out.continuation_mass = input.continuation_mass;
  PopulateEntryRows(input, &out);
  std::copy(incident, incident + 3, out.incident_direction);

  const analytic::CrystalShape shape =
      SampleShape(rng, input.setting->crystal_.param_, input, layer_index, &out.shape, latents);
  out.analytic_shape = shape;
  out.refractive_index = refractive_index;
  out.upper_wedge_deg = shape.upper_wedge_deg;
  out.lower_wedge_deg = shape.lower_wedge_deg;
  float pose_values[3]{};
  const AxisPoseSampleTrace pose_trace =
      RandomSampler::SampleAxisPoseWithTrace(rng, input.setting->crystal_.axis_, pose_values);
  AppendPoseLatents(input.setting->crystal_.axis_, input, layer_index, pose_trace, latents);
  std::copy(pose_values, pose_values + 3, out.pose_lon_lat_roll_rad);
  const AxisDistribution& axis = input.setting->crystal_.axis_;
  const bool active_pose_coordinates[3] = { DistributionHasPositiveWidth(axis.azimuth_dist),
                                            DistributionHasPositiveWidth(axis.latitude_dist),
                                            DistributionHasPositiveWidth(axis.roll_dist) };
  out.pose_support_rank = PoseSupportRank(pose_values, active_pose_coordinates, out.pose_tangent_drotation);

  analytic::FaceNormalTable normals;
  analytic::FacePolygonTable polygons;
  if (analytic::BuildFaceNormals(shape, &normals, &polygons) != analytic::Status::kOk) {
    out.status = SceneMeasureStatus::kPhysicallyUnreachable;
    out.reason = "sampled crystal is rejected by the closed-form geometry gate";
    return out;
  }
  const Crystal product_crystal = ProductCrystal(shape);
  out.total_surface_area = EntrySamplingSurfaceArea(product_crystal.CfGeom());
  if (!(out.total_surface_area > 0.0) || !std::isfinite(out.total_surface_area)) {
    out.status = SceneMeasureStatus::kNumericalIncomplete;
    out.reason = "sampled crystal surface area is not a positive finite value";
    return out;
  }
  if (faces.size() > kMaxHits) {
    out.status = SceneMeasureStatus::kNotSupported;
    out.reason = "the requested path exceeds the runtime kMaxHits filter domain";
    return out;
  }
  std::vector<int> slots;
  if (const Error error = ResolveDiagnosticLayerPath(faces, normals, &slots); !error.Ok()) {
    out.status = SceneMeasureStatus::kPhysicallyUnreachable;
    out.reason = error.message;
    return out;
  }
  ApplyFixedFilterLedger(fixed_filter_ledger, &out);
  if (out.filter_rejection_certified) {
    out.crystal_share = 0.0;
    out.status = SceneMeasureStatus::kZeroWeight;
    out.reason = "every selected scattering entry has a support-constant rejecting physical filter";
    return out;
  }

  const Rotation rotation = BuildCrystalRotation(pose_values[0], pose_values[1], pose_values[2]);
  analytic::DiagnosticRowInput row_input;
  row_input.refractive_index = refractive_index;
  std::copy(incident, incident + 3, row_input.incident_direction);
  const float* matrix = rotation.GetMat();
  std::copy(matrix, matrix + 9, row_input.pose);
  std::copy(row_input.pose, row_input.pose + 9, out.pose);

  analytic::DiagnosticField field(normals, polygons, faces.data(), slots.data(), static_cast<int>(faces.size()));
  out.field = include_derivatives ? field.Evaluate(row_input) : field.EvaluateWithoutDerivatives(row_input);
  out.entry_measure = out.field.entry_measure;
  out.fresnel_weight = out.field.fresnel_weight;
  std::copy(out.field.outgoing_direction, out.field.outgoing_direction + 3, out.outgoing_direction);
  out.status = FieldStatus(out.field, &out.reason);
  if (out.status == SceneMeasureStatus::kConfirmed) {
    out.normalized_entry_factor = 2.0 * out.entry_measure / out.total_surface_area;
    if (!std::isfinite(out.normalized_entry_factor) || out.normalized_entry_factor < 0.0) {
      out.status = SceneMeasureStatus::kNumericalIncomplete;
      out.reason = "native entry normalization is not a finite non-negative value";
    }
  }
  if (out.status == SceneMeasureStatus::kConfirmed) {
    ApplyPhysicalEntryMixture(input, product_crystal, faces, &out);
    if (out.crystal_share == 0.0) {
      out.status = SceneMeasureStatus::kZeroWeight;
      out.reason = "every selected scattering entry rejected the physical filter";
    }
  }
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

NumericValue SplitError(const NumericValue contribution[2], const NumericValue mass[2]) {
  if (!IsRepresentable(mass[0])) {
    return mass[0];
  }
  if (!IsRepresentable(mass[1])) {
    return mass[1];
  }
  if (!(mass[0].value > 0.0) || !(mass[1].value > 0.0)) {
    return {};
  }
  const NumericValue total_mass = CheckedAdd(mass[0], mass[1]);
  const NumericValue estimate0 = CheckedMultiply(contribution[0], CheckedDivide(total_mass, mass[0]));
  const NumericValue estimate1 = CheckedMultiply(contribution[1], CheckedDivide(total_mass, mass[1]));
  return CheckedMultiply(CheckedAbsoluteDifference(estimate0, estimate1), 0.5);
}

Distribution FixedDistribution(double value) {
  return { DistributionType::kNoRandom, static_cast<float>(value), 0.0f };
}

void SetDeterministicShape(const analytic::CrystalShape& shape, CrystalParam* param) {
  if (auto* prism = std::get_if<PrismCrystalParam>(param); prism != nullptr) {
    prism->h_ = FixedDistribution(shape.height);
    for (int face = 0; face < 6; ++face) {
      prism->d_[face] = FixedDistribution(shape.face_distance[face]);
    }
    std::fill(std::begin(prism->sync_group_), std::end(prism->sync_group_), 0);
    return;
  }
  auto& pyramid = std::get<PyramidCrystalParam>(*param);
  pyramid.h_pyr_u_ = FixedDistribution(shape.upper_h);
  pyramid.h_prs_ = FixedDistribution(shape.height);
  pyramid.h_pyr_l_ = FixedDistribution(shape.lower_h);
  for (int face = 0; face < 6; ++face) {
    pyramid.d_[face] = FixedDistribution(shape.face_distance[face]);
  }
  std::fill(std::begin(pyramid.sync_group_), std::end(pyramid.sync_group_), 0);
  pyramid.wedge_angle_u_ = static_cast<float>(shape.upper_wedge_deg);
  pyramid.wedge_angle_l_ = static_cast<float>(shape.lower_wedge_deg);
}

void SetDeterministicPose(const std::array<double, 3>& pose, AxisDistribution* axis) {
  axis->azimuth_dist = FixedDistribution(pose[0] * math::kRadToDegree);
  axis->latitude_dist = FixedDistribution(pose[1] * math::kRadToDegree);
  axis->roll_dist = FixedDistribution(pose[2] * math::kRadToDegree);
}

bool SameCrystalId(const CrystalConfig& crystal, IdType id) {
  return crystal.id_ == id;
}

void ApplyReplayStateToConfig(const SceneMeasureRequest& request, const SceneMeasureReplayState& state,
                              ConfigManager* config) {
  for (size_t layer_index = 0; layer_index < request.layer_crystal_ids.size(); ++layer_index) {
    const IdType crystal_id = request.layer_crystal_ids[layer_index];
    if (layer_index < config->scene_.ms_.size()) {
      for (ScatteringSetting& setting : config->scene_.ms_[layer_index].setting_) {
        if (!SameCrystalId(setting.crystal_, crystal_id)) {
          continue;
        }
        SetDeterministicShape(state.shapes[layer_index], &setting.crystal_.param_);
        SetDeterministicPose(state.poses[layer_index], &setting.crystal_.axis_);
      }
    }
    const auto crystal = config->crystals_.find(crystal_id);
    if (crystal != config->crystals_.end()) {
      SetDeterministicShape(state.shapes[layer_index], &crystal->second.param_);
      SetDeterministicPose(state.poses[layer_index], &crystal->second.axis_);
    }
  }
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
  result.units = "dimensionless product-native entry and Fresnel measure times raw spectral weight";
  result.normalization =
      "shape, pose and the product finite solar source are unit probability measures; spectrum and physical "
      "members retain their explicit weights; every layer uses 2*A_path/S_total for its sampled shape; atoms "
      "carry counting mass rather than continuous density";

  std::vector<LayerInput> layers;
  if (const Error error = ResolveLayers(config, request, &layers, &result.factors); !error.Ok()) {
    return error;
  }
  const bool continuous_sun = config.scene_.light_source_.param_.diameter_ > 0.0f;
  const bool continuous_spectrum = request.spectrum_source == SceneSpectrumSource::kScene &&
                                   std::holds_alternative<IlluminantType>(config.scene_.light_source_.spectrum_);
  result.factors.insert(result.factors.begin(),
                        { -1, "sun_disc", DistributionType::kNoRandom, -1, continuous_sun ? 2 : 0,
                          config.scene_.light_source_.param_.diameter_, 0.0, "spherical cap diameter (degrees)",
                          continuous_sun ? "normalized spherical-cap source" : "enumerated source atoms",
                          "unit source probability mass" });
  result.factors.insert(
      result.factors.begin(),
      { -1, "spectrum", DistributionType::kNoRandom, -1, continuous_spectrum ? 1 : 0, 0.0, 0.0, "wavelength node",
        continuous_spectrum ? "continuous scene illuminant quadrature" : "enumerated spectrum atoms",
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

  std::vector<FixedFilterLedger> fixed_filter_ledgers;
  fixed_filter_ledgers.reserve(result.member_chains.size());
  for (const auto& member_chain : result.member_chains) {
    fixed_filter_ledgers.push_back(BuildFixedFilterLedger(layers, member_chain));
  }

  const int coarse_count = request.sample_count / 2;
  const ConditionalMeasureLedger conditional_measure = BuildConditionalMeasureLedger(layers);
  bool any_confirmed = false;
  bool any_nonzero_source = false;
  bool any_positive_path_measure = false;
  bool any_filter_rejected = false;
  bool any_uncertified_filter_rejection = false;
  bool any_unreachable = false;
  bool any_numerical = false;
  std::string numerical_reason;
  const auto note_result_numerical = [&](const std::string& reason) {
    any_numerical = true;
    if (numerical_reason.empty()) {
      numerical_reason = reason;
    }
  };
  const auto mark_row_numerical = [&](SceneMeasureRow* row, const std::string& reason) {
    note_result_numerical(reason);
    row->evaluation_status = SceneMeasureStatus::kNumericalIncomplete;
    if (row->evaluation_reason.empty()) {
      row->evaluation_reason = reason;
    } else if (row->evaluation_reason.find(reason) == std::string::npos) {
      row->evaluation_reason += "; " + reason;
    }
    if (row->status != SceneMeasureStatus::kZeroWeight) {
      row->status = SceneMeasureStatus::kNumericalIncomplete;
      row->reason = row->evaluation_reason;
    }
  };
  const bool has_continuous_factor =
      std::any_of(result.factors.begin(), result.factors.end(),
                  [](const MeasureFactorDescriptor& factor) { return factor.support_dimension > 0; });
  const bool approximate_sun = continuous_sun;
  const bool approximate_spectrum = continuous_spectrum;
  NumericValue sun_partition_contribution[2];
  NumericValue sun_partition_mass[2];
  for (const SunMeasureNode& node : result.sun_nodes) {
    NumericValue& partition = sun_partition_mass[node.node_id & 1];
    partition = CheckedAdd(partition, CheckedNonnegative(node.mass));
    if (!IsRepresentable(partition)) {
      note_result_numerical("sun partition mass is not representable as a finite double");
    }
  }
  NumericValue spectrum_partition_contribution[2];
  NumericValue spectrum_partition_mass[2];
  for (const SpectrumMeasureNode& node : result.spectrum_nodes) {
    NumericValue& partition = spectrum_partition_mass[node.node_id & 1];
    partition = CheckedAdd(partition, CheckedNonnegative(node.weight));
    if (!IsRepresentable(partition)) {
      note_result_numerical("spectrum partition mass is not representable as a finite double");
    }
  }
  NumericValue coarse_contribution;
  NumericValue total_contribution;
  NumericValue sampled_measure_mass;
  std::vector<uint32_t> stored_priorities;
  for (const SpectrumMeasureNode& spectrum : result.spectrum_nodes) {
    for (const SunMeasureNode& sun : result.sun_nodes) {
      for (size_t member_index = 0; member_index < result.member_chains.size(); member_index++) {
        const FixedFilterLedger& fixed_filter_ledger = fixed_filter_ledgers[member_index];
        for (int sample = 0; sample < request.sample_count; sample++) {
          SceneMeasureRow row;
          row.spectrum_node_id = spectrum.node_id;
          row.sun_node_id = sun.node_id;
          row.member_chain_index = static_cast<int>(member_index);
          row.sample_index = sample;
          row.replay_seed =
              RowSeed(request.seed, spectrum.node_id, sun.node_id, static_cast<int>(member_index), sample);
          const bool fixed_filter_paths_valid =
              fixed_filter_ledger.first_zero_layer < 0 ||
              MemberChainExistsInSample(row.replay_seed, layers, result.member_chains[member_index]);
          const bool has_validated_fixed_filter_zero =
              fixed_filter_ledger.first_zero_layer >= 0 && fixed_filter_paths_valid;
          row.wavelength_nm = spectrum.wavelength_nm;
          row.spectrum_weight = spectrum.weight;
          row.sun_mass = sun.mass;
          row.joint_sample_mass = 1.0 / request.sample_count;
          row.status = spectrum.weight == 0.0 || sun.mass == 0.0 ? SceneMeasureStatus::kZeroWeight :
                                                                   SceneMeasureStatus::kConfirmed;
          row.evaluation_status = SceneMeasureStatus::kConfirmed;
          if (row.status == SceneMeasureStatus::kZeroWeight) {
            row.reason = "the global spectrum or sun source node has zero weight";
          }
          any_nonzero_source = any_nonzero_source || (spectrum.weight > 0.0 && sun.mass > 0.0);

          const NumericValue global_weight =
              CheckedMultiply(CheckedNonnegative(spectrum.weight), CheckedNonnegative(sun.mass));
          AssignNumeric(global_weight, &row.global_weight, &row.global_weight_status);
          if (!IsRepresentable(global_weight)) {
            mark_row_numerical(&row, "global spectrum and sun weight product is not representable");
          }

          const NumericValue known_conditional_measure_mass = conditional_measure.product;
          if (conditional_measure.first_zero_layer >= 0) {
            row.status = SceneMeasureStatus::kZeroWeight;
            row.reason = "layer " + std::to_string(conditional_measure.first_zero_layer) + " has zero " +
                         conditional_measure.first_zero_factor;
          } else if (!IsRepresentable(known_conditional_measure_mass)) {
            mark_row_numerical(&row, "complete layer conditional mass product is not representable");
          }
          if (has_validated_fixed_filter_zero) {
            row.status = SceneMeasureStatus::kZeroWeight;
            row.reason = "layer " + std::to_string(fixed_filter_ledger.first_zero_layer) +
                         " has a support-constant zero physical-filter acceptance mass";
            any_filter_rejected = true;
          }

          RandomNumberGenerator rng(row.replay_seed);
          double incident[3] = { sun.incident_direction[0], sun.incident_direction[1], sun.incident_direction[2] };
          NumericValue conditional_weight{ 1.0, SceneMeasureNumericStatus::kAvailable };
          NumericValue row_conditional_measure_mass{ 1.0, SceneMeasureNumericStatus::kAvailable };
          const FixedFilterLayerLedger empty_fixed_filter_layer;
          for (size_t layer_index = 0; layer_index < layers.size(); layer_index++) {
            const FixedFilterLayerLedger& validated_fixed_filter_layer =
                fixed_filter_paths_valid ? fixed_filter_ledger.layers[layer_index] : empty_fixed_filter_layer;
            SceneMeasureLayerRow layer =
                EvaluateLayer(rng, layers[layer_index], static_cast<int>(layer_index),
                              result.member_chains[member_index][layer_index], spectrum.refractive_index, incident,
                              request.include_derivatives, validated_fixed_filter_layer, &row.latents);
            layer.source_sun_node_id = sun.node_id;
            layer.source_spectrum_node_id = spectrum.node_id;
            layer.source_wavelength_nm = spectrum.wavelength_nm;
            const NumericValue layer_mass =
                CheckedMultiply(CheckedNonnegative(layer.crystal_share), CheckedNonnegative(layer.continuation_mass));
            row_conditional_measure_mass = CheckedMultiply(row_conditional_measure_mass, layer_mass);
            conditional_weight = CheckedMultiply(conditional_weight, layer_mass);
            if (!IsRepresentable(conditional_weight)) {
              mark_row_numerical(&row, "layer conditional and optical weight product is not representable");
            }
            if (IsRepresentable(layer_mass) && layer_mass.value == 0.0) {
              row.status = SceneMeasureStatus::kZeroWeight;
              row.reason = "layer " + std::to_string(layer_index) + " has zero crystal-share or continuation/exit mass";
            }
            const bool filter_rejected =
                layer.status == SceneMeasureStatus::kZeroWeight &&
                std::any_of(layer.entries.begin(), layer.entries.end(), [](const SceneMeasureEntryRow& entry) {
                  return entry.filter_evaluated && !entry.accepted;
                });
            if (layer.status == SceneMeasureStatus::kConfirmed) {
              const NumericValue optical_weight = CheckedMultiply(CheckedNonnegative(layer.normalized_entry_factor),
                                                                  CheckedNonnegative(layer.fresnel_weight));
              conditional_weight = CheckedMultiply(conditional_weight, optical_weight);
              if (!IsRepresentable(conditional_weight)) {
                mark_row_numerical(&row, "layer optical weight product is not representable");
              }
              std::copy(layer.outgoing_direction, layer.outgoing_direction + 3, incident);
            } else if (filter_rejected) {
              any_filter_rejected = true;
              any_uncertified_filter_rejection = any_uncertified_filter_rejection || !layer.filter_rejection_certified;
              conditional_weight = {};
              row.status = SceneMeasureStatus::kZeroWeight;
              row.reason = "layer " + std::to_string(layer_index) + ": " + layer.reason;
            } else {
              conditional_weight = {};
              row.evaluation_status = layer.status;
              row.evaluation_reason = "layer " + std::to_string(layer_index) + ": " + layer.reason;
              if (row.status != SceneMeasureStatus::kZeroWeight) {
                row.status = layer.status;
                row.reason = row.evaluation_reason;
                any_unreachable = any_unreachable || layer.status == SceneMeasureStatus::kPhysicallyUnreachable;
                any_numerical = any_numerical || layer.status == SceneMeasureStatus::kNumericalIncomplete;
              }
            }
            row.layers.push_back(std::move(layer));
            if (IsRepresentable(conditional_weight) && conditional_weight.value == 0.0) {
              break;
            }
          }
          if (conditional_measure.first_zero_layer >= 0 || has_validated_fixed_filter_zero) {
            row_conditional_measure_mass = {};
          }
          if (spectrum.weight > 0.0 && sun.mass > 0.0 && conditional_measure.strictly_positive) {
            any_positive_path_measure = true;
          }
          double proposal_log_density = 0.0;
          double target_log_density = 0.0;
          double importance_log_weight = 0.0;
          bool proposal_is_zero = false;
          bool target_is_zero = false;
          bool proposal_log_invalid = false;
          bool target_log_invalid = false;
          bool importance_log_invalid = false;
          for (const LatentMeasureSample& latent : row.latents) {
            if (latent.status == SceneMeasureStatus::kNumericalIncomplete) {
              mark_row_numerical(&row, "a generator latent has a non-finite density or mapping Jacobian");
              conditional_weight = { std::numeric_limits<double>::quiet_NaN(), SceneMeasureNumericStatus::kInvalid };
            }
            if (latent.base_measure == LatentBaseMeasure::kAtomCounting) {
              continue;
            }
            const double proposal = latent.proposal_density_or_mass;
            const double target = latent.target_density_or_mass;
            double proposal_log_term = 0.0;
            double target_log_term = 0.0;
            if (!(proposal > 0.0)) {
              proposal_is_zero = proposal_is_zero || proposal == 0.0;
              proposal_log_invalid = proposal_log_invalid || proposal < 0.0 || !std::isfinite(proposal);
            } else {
              proposal_log_term = std::log(proposal);
              proposal_log_density += proposal_log_term;
              proposal_log_invalid = proposal_log_invalid || !std::isfinite(proposal_log_density);
            }
            if (!(target > 0.0)) {
              target_is_zero = target_is_zero || target == 0.0;
              target_log_invalid = target_log_invalid || target < 0.0 || !std::isfinite(target);
            } else {
              target_log_term = std::log(target);
              target_log_density += target_log_term;
              target_log_invalid = target_log_invalid || !std::isfinite(target_log_density);
            }
            if (proposal > 0.0 && target > 0.0) {
              importance_log_weight += target_log_term - proposal_log_term;
              importance_log_invalid = importance_log_invalid || !std::isfinite(importance_log_weight);
            }
          }

          row.joint_log_proposal_density =
              proposal_is_zero ? std::numeric_limits<double>::quiet_NaN() : proposal_log_density;
          row.joint_log_target_density = target_is_zero ? std::numeric_limits<double>::quiet_NaN() : target_log_density;
          NumericValue proposal_density =
              proposal_log_invalid ?
                  NumericValue{ std::numeric_limits<double>::quiet_NaN(), SceneMeasureNumericStatus::kInvalid } :
                  MaterializeLogProduct(proposal_log_density, proposal_is_zero);
          NumericValue target_density = target_log_invalid ? NumericValue{ std::numeric_limits<double>::quiet_NaN(),
                                                                           SceneMeasureNumericStatus::kInvalid } :
                                                             MaterializeLogProduct(target_log_density, target_is_zero);
          NumericValue importance_weight;
          if (proposal_log_invalid || target_log_invalid || importance_log_invalid || proposal_is_zero) {
            importance_weight = { std::numeric_limits<double>::quiet_NaN(), SceneMeasureNumericStatus::kInvalid };
          } else {
            importance_weight = MaterializeLogProduct(importance_log_weight, target_is_zero);
          }
          AssignNumeric(proposal_density, &row.joint_proposal_density, &row.joint_proposal_density_status);
          AssignNumeric(target_density, &row.joint_target_density, &row.joint_target_density_status);
          AssignNumeric(importance_weight, &row.joint_importance_weight, &row.joint_importance_weight_status);
          if (!IsRepresentable(importance_weight)) {
            mark_row_numerical(&row, "joint importance weight is not representable from the generator densities");
            conditional_weight = { std::numeric_limits<double>::quiet_NaN(), SceneMeasureNumericStatus::kInvalid };
          }

          const NumericValue sample_mass = CheckedNonnegative(row.joint_sample_mass);
          const NumericValue contribution = CheckedMultiply(
              CheckedMultiply(CheckedMultiply(global_weight, sample_mass), importance_weight), conditional_weight);
          AssignNumeric(contribution, &row.contribution, &row.contribution_status);
          if (!IsRepresentable(contribution)) {
            mark_row_numerical(&row, "row contribution is not representable as a finite double");
          }
          const NumericValue row_measure_mass =
              CheckedMultiply(CheckedMultiply(global_weight, sample_mass), row_conditional_measure_mass);
          sampled_measure_mass = CheckedAdd(sampled_measure_mass, row_measure_mass);
          if (!IsRepresentable(sampled_measure_mass)) {
            mark_row_numerical(&row, "sampled measure mass accumulation exceeds the finite double range");
          }
          total_contribution = CheckedAdd(total_contribution, contribution);
          if (!IsRepresentable(total_contribution)) {
            mark_row_numerical(&row, "total contribution accumulation exceeds the finite double range");
          }
          if (sample < coarse_count) {
            coarse_contribution = CheckedAdd(coarse_contribution, CheckedMultiply(contribution, 2.0));
            if (!IsRepresentable(coarse_contribution)) {
              mark_row_numerical(&row, "coarse contribution accumulation exceeds the finite double range");
            }
          }
          NumericValue& sun_contribution = sun_partition_contribution[sun.node_id & 1];
          sun_contribution = CheckedAdd(sun_contribution, contribution);
          if (!IsRepresentable(sun_contribution)) {
            mark_row_numerical(&row, "sun partition contribution accumulation exceeds the finite double range");
          }
          NumericValue& spectrum_contribution = spectrum_partition_contribution[spectrum.node_id & 1];
          spectrum_contribution = CheckedAdd(spectrum_contribution, contribution);
          if (!IsRepresentable(spectrum_contribution)) {
            mark_row_numerical(&row, "spectrum partition contribution accumulation exceeds the finite double range");
          }
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
  AssignNumeric(coarse_contribution, &result.coarse_contribution, &result.coarse_contribution_status);
  AssignNumeric(total_contribution, &result.total_contribution, &result.total_contribution_status);
  AssignNumeric(sampled_measure_mass, &result.sampled_measure_mass, &result.sampled_measure_mass_status);
  const NumericValue joint_sampling_error = CheckedAbsoluteDifference(total_contribution, coarse_contribution);
  const NumericValue sun_node_error =
      approximate_sun ? SplitError(sun_partition_contribution, sun_partition_mass) : NumericValue{};
  const NumericValue spectrum_node_error =
      approximate_spectrum ? SplitError(spectrum_partition_contribution, spectrum_partition_mass) : NumericValue{};
  const NumericValue absolute_error = CheckedMax(joint_sampling_error, sun_node_error, spectrum_node_error);
  AssignNumeric(joint_sampling_error, &result.joint_sampling_error_estimate,
                &result.joint_sampling_error_estimate_status);
  AssignNumeric(sun_node_error, &result.sun_node_error_estimate, &result.sun_node_error_estimate_status);
  AssignNumeric(spectrum_node_error, &result.spectrum_node_error_estimate, &result.spectrum_node_error_estimate_status);
  AssignNumeric(absolute_error, &result.absolute_error_estimate, &result.absolute_error_estimate_status);
  if (!IsRepresentable(joint_sampling_error) || !IsRepresentable(sun_node_error) ||
      !IsRepresentable(spectrum_node_error) || !IsRepresentable(absolute_error)) {
    note_result_numerical("one or more integration error estimates are not representable as finite doubles");
  }
  const double scale = IsRepresentable(total_contribution) && IsRepresentable(coarse_contribution) ?
                           std::max(total_contribution.value, coarse_contribution.value) :
                           0.0;
  const bool source_resolution_incomplete =
      (approximate_sun && result.sun_nodes.size() < 2) || (approximate_spectrum && result.spectrum_nodes.size() < 2) ||
      (approximate_sun && (!(sun_partition_mass[0].value > 0.0) || !(sun_partition_mass[1].value > 0.0))) ||
      (approximate_spectrum &&
       (!(spectrum_partition_mass[0].value > 0.0) || !(spectrum_partition_mass[1].value > 0.0)));
  if (!any_nonzero_source) {
    result.status = SceneMeasureStatus::kZeroWeight;
    result.reason = "every spectrum node has zero weight";
  } else if (!any_positive_path_measure) {
    result.status = SceneMeasureStatus::kZeroWeight;
    result.reason = "every positive source row has zero crystal-share or continuation/exit mass";
  } else if (any_numerical) {
    result.status = SceneMeasureStatus::kNumericalIncomplete;
    result.reason = numerical_reason.empty() ? "at least one evaluated row ended in a numerical-incomplete state" :
                                               numerical_reason;
  } else if (!any_confirmed && !any_unreachable && any_filter_rejected && has_continuous_factor &&
             any_uncertified_filter_rejection) {
    result.status = SceneMeasureStatus::kNumericalIncomplete;
    result.reason =
        "finite random sampling found no filter-accepted row; continuous support was not exhausted, so zero "
        "accepted measure is not certified";
  } else if (!any_confirmed && !any_unreachable && any_filter_rejected) {
    result.status = SceneMeasureStatus::kZeroWeight;
    result.reason = "the fully enumerated atomic member/source support was rejected by the physical filters";
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

Error ReevaluateSceneMeasureRow(const ConfigManager& config, const SceneMeasureRequest& request,
                                const SceneMeasureRow& row_template, const SceneMeasureReplayState& state,
                                SceneMeasureRow* out) {
  if (out == nullptr) {
    return { ErrorCode::kInvalidArgument, "scene-measure replay requires an output row" };
  }
  *out = SceneMeasureRow{};
  if (state.shapes.size() != request.layer_crystal_ids.size() ||
      state.poses.size() != request.layer_crystal_ids.size() ||
      row_template.layers.size() != request.layer_crystal_ids.size()) {
    return { ErrorCode::kInvalidArgument, "scene-measure replay state must describe every requested layer" };
  }
  if (!std::isfinite(state.wavelength_nm) ||
      !std::all_of(std::begin(state.incident_direction), std::end(state.incident_direction),
                   [](double value) { return std::isfinite(value); })) {
    return { ErrorCode::kInvalidArgument, "scene-measure replay source state must be finite" };
  }
  const double incident_norm =
      std::sqrt(std::inner_product(std::begin(state.incident_direction), std::end(state.incident_direction),
                                   std::begin(state.incident_direction), 0.0));
  if (!(incident_norm > 0.0)) {
    return { ErrorCode::kInvalidArgument, "scene-measure replay incident direction must be non-zero" };
  }

  ConfigManager replay_config = config;
  ApplyReplayStateToConfig(request, state, &replay_config);
  SceneMeasureRequest replay_request = request;
  replay_request.member_selection = SceneMemberSelection::kExplicitChains;
  replay_request.explicit_member_chains = { {} };
  replay_request.explicit_member_chains.front().reserve(row_template.layers.size());
  for (const SceneMeasureLayerRow& layer : row_template.layers) {
    replay_request.explicit_member_chains.front().push_back(layer.faces);
  }
  replay_request.spectrum_source = SceneSpectrumSource::kDiagnostic;
  replay_request.diagnostic_wavelengths_nm = { state.wavelength_nm };
  replay_request.diagnostic_wavelength_weights = { row_template.spectrum_weight };
  SunMeasureNode sun;
  sun.mass = row_template.sun_mass;
  for (int component = 0; component < 3; ++component) {
    sun.incident_direction[component] = state.incident_direction[component] / incident_norm;
  }
  replay_request.source_sun_nodes = { sun };
  replay_request.sample_count = 2;
  replay_request.include_derivatives = false;

  SceneMeasureRow replayed;
  bool saw_row = false;
  SceneMeasureResult ignored;
  const Error error = BuildSceneMeasure(
      replay_config, replay_request,
      [&](const SceneMeasureRow& row) {
        if (!saw_row) {
          replayed = row;
          saw_row = true;
        }
      },
      &ignored);
  if (!error.Ok()) {
    return error;
  }
  if (!saw_row) {
    return { ErrorCode::kInvalidArgument, "scene-measure replay produced no row" };
  }

  const double sample_mass_scale = 2.0 / static_cast<double>(request.sample_count);
  replayed.spectrum_node_id = row_template.spectrum_node_id;
  replayed.sun_node_id = row_template.sun_node_id;
  replayed.member_chain_index = row_template.member_chain_index;
  replayed.sample_index = row_template.sample_index;
  replayed.replay_seed = row_template.replay_seed;
  replayed.joint_sample_mass = row_template.joint_sample_mass;
  if (replayed.contribution_status == SceneMeasureNumericStatus::kAvailable) {
    replayed.contribution *= sample_mass_scale;
  }
  replayed.latents = row_template.latents;
  for (SceneMeasureLayerRow& layer : replayed.layers) {
    layer.source_sun_node_id = row_template.sun_node_id;
    layer.source_spectrum_node_id = row_template.spectrum_node_id;
  }
  *out = std::move(replayed);
  return {};
}

}  // namespace lumice::raypath
