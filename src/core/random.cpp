#include "core/random.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

#include "core/lat_lut.hpp"
#include "core/math.hpp"
#include "core/sample_transform.hpp"
#include "core/shared/lat_path_selection.hpp"
#include "util/fatal.hpp"

namespace lumice {

// Named accessors for Distribution. Each one names the row of the per-type table in random.hpp that
// the caller is relying on, and asserts the type actually matches. They are pure forwarding to
// `center` / `spread` — no arithmetic — so the sampling behavior is unchanged by construction.
float Distribution::Value() const {
  assert(type == DistributionType::kNoRandom);
  return center;
}

float Distribution::UniformCenter() const {
  assert(type == DistributionType::kUniform);
  return center;
}

float Distribution::UniformFullRange() const {
  assert(type == DistributionType::kUniform);
  return spread;
}

float Distribution::Mean() const {
  assert(type == DistributionType::kGaussian || type == DistributionType::kGaussianLegacy);
  return center;
}

float Distribution::Std() const {
  assert(type == DistributionType::kGaussian || type == DistributionType::kGaussianLegacy);
  return spread;
}

float Distribution::Tilt() const {
  assert(type == DistributionType::kZigzag);
  return center;
}

float Distribution::Amplitude() const {
  assert(type == DistributionType::kZigzag);
  return spread;
}

float Distribution::Location() const {
  assert(type == DistributionType::kLaplacian);
  return center;
}

float Distribution::Scale() const {
  assert(type == DistributionType::kLaplacian);
  return spread;
}


RandomNumberGenerator::RandomNumberGenerator(uint32_t seed)
    : seed_(seed), generator_{ static_cast<std::mt19937::result_type>(seed) } {}


RandomNumberGenerator& RandomNumberGenerator::GetInstance() {
  static thread_local RandomNumberGenerator instance{ static_cast<uint32_t>(
      std::chrono::system_clock::now().time_since_epoch().count()) };
  return instance;
}


float RandomNumberGenerator::GetGaussian() {
  return gauss_dist_(generator_);
}


float RandomNumberGenerator::GetUniform() {
  return uniform_dist_(generator_);
}


namespace detail {

size_t ClampUniformToIndex(float u, size_t n) {
  if (n == 0) {
    // Deliberately not an assert: assert() is a no-op under NDEBUG, and there the
    // `n - 1` below underflows to SIZE_MAX and is handed back as if it were a
    // valid subscript. There is no index that is correct for an empty range, so
    // returning any value at all only hides the caller's bug — this guard is the
    // whole reason the single owner of this conversion exists.
    FatalAbort("ClampUniformToIndex: n must be > 0 (got n == 0)");
  }
  const size_t j = static_cast<size_t>(u * static_cast<float>(n));
  return j >= n ? n - 1 : j;
}

}  // namespace detail


size_t RandomNumberGenerator::GetUniformIndex(size_t n) {
  return detail::ClampUniformToIndex(GetUniform(), n);
}


float RandomNumberGenerator::Get(Distribution dist) {
  switch (BuildDistributionDrawPlan(dist).kind) {
    case DistributionDrawKind::kUnitUniform:
      return TransformDistribution(dist, { GetUniform() });
    case DistributionDrawKind::kStandardNormal:
      return TransformDistribution(dist, { GetGaussian() });
    default:
      return TransformDistribution(dist, {});
  }
}


void RandomNumberGenerator::Reset() {
  generator_.seed(seed_);
}


void RandomNumberGenerator::SetSeed(uint32_t seed) {
  seed_ = seed;
  generator_.seed(seed_);
}


void RandomSampler::SampleSphericalPointsSph(float* data, size_t num, size_t step) {
  auto& rng = RandomNumberGenerator::GetInstance();
  for (size_t i = 0; i < num; i++) {
    const float latitude_uniform = rng.GetUniform();
    const float longitude_uniform = rng.GetUniform();
    const auto point = TransformFullSpherePoint(latitude_uniform, longitude_uniform);
    data[i * step + 0] = point[0];
    data[i * step + 1] = point[1];
  }
}


void RandomSampler::SampleSphericalPointsSph(const AxisDistribution& axis_dist, float* data, size_t num,
                                             const LatLut* lat_lut) {
  auto& rng = RandomNumberGenerator::GetInstance();

  // Latitude sampling with the spherical-area Jacobian p(phi) ∝ proposal(phi) × cos(phi),
  // where cos(phi) = sin(colatitude) is the area element. Since 330.3 every non-degenerate
  // distribution routes to the unified inverse-CDF area-measure LUT (kLutInverseCdf); the
  // remaining explicit paths are kGaussianLegacy (legacy no-Jacobian Gaussian) and kNoRandom
  // (single deterministic orientation, no Jacobian needed). Path selection is single-sourced
  // with the two GPU backends via lat_path::SelectLatPath.
  auto lat_type = axis_dist.latitude_dist.type;
  auto decision = lat_path::SelectLatPath(axis_dist);

  for (size_t i = 0; i < num; i++) {
    LatitudeSample latitude;
    if (decision.kind == lat_path::LatPathKind::kFullSphere) {
      latitude = TransformFullSphereLatitude(rng.GetUniform());
    } else if (decision.kind == lat_path::LatPathKind::kLutInverseCdf) {
      const LatLut& lut = (lat_lut != nullptr) ? *lat_lut : *GetSharedLatLut(axis_dist.latitude_dist);
      const float xi = rng.GetUniform();
      const float flip_uniform = rng.GetUniform();
      latitude = TransformLatitudeLut(lut, { xi, flip_uniform });
    } else if (lat_type == DistributionType::kGaussianLegacy) {
      latitude = TransformLegacyLatitude(rng.Get(axis_dist.latitude_dist));
    } else {
      latitude = { rng.Get(axis_dist.latitude_dist) * math::kDegreeToRad, false };
    }
    const float azimuth = rng.Get(axis_dist.azimuth_dist);
    const float roll = rng.Get(axis_dist.roll_dist);
    const auto angles = ComposeAxisAngles(latitude, azimuth, roll);
    std::copy(angles.begin(), angles.end(), data + i * 3);
  }
}


AxisDistribution::AxisDistribution()
    : azimuth_dist{ DistributionType::kNoRandom, 0, 0 }, latitude_dist{ DistributionType::kNoRandom, 90.0f, 0 },
      roll_dist{ DistributionType::kNoRandom, 0, 0 } {}

bool AxisDistribution::IsFullSphereUniform() const {
  // The az / lat halves read through UniformCenter() / UniformFullRange() while the roll half
  // (IsRollRotationallySymmetric -> detail::IsFullTurnUniform) reads the raw `spread`. The two are
  // same value: for kUniform, UniformFullRange() returns `spread` outright. The accessors are
  // kept here because they assert the kUniform precondition, which the preceding `type ==` in
  // each conjunct establishes; detail::IsFullTurnUniform cannot use them because it is also the entry
  // point for callers that have not yet checked the type.
  return azimuth_dist.type == DistributionType::kUniform && FloatEqual(azimuth_dist.UniformCenter(), 0.0f) &&
         FloatEqual(azimuth_dist.UniformFullRange(), 360.0f) && latitude_dist.type == DistributionType::kUniform &&
         FloatEqual(latitude_dist.UniformCenter(), 90.0f) && FloatEqual(latitude_dist.UniformFullRange(), 360.0f) &&
         IsRollRotationallySymmetric();
}


bool detail::IsFullTurnUniform(DistributionType type, float full_range_deg) {
  return type == DistributionType::kUniform && FloatEqual(full_range_deg, 360.0f);
}


bool AxisDistribution::IsAzRotationallySymmetric() const {
  return detail::IsFullTurnUniform(azimuth_dist.type, azimuth_dist.spread);
}


bool AxisDistribution::IsRollRotationallySymmetric() const {
  return detail::IsFullTurnUniform(roll_dist.type, roll_dist.spread);
}


bool AxisDistribution::IsAxisDeterministic() const {
  // kNoRandom is the one type that consumes no RNG: RandomNumberGenerator::Get
  // returns dist.Value() outright for it, and SampleSphericalPointsSph's
  // kNoRandom latitude branch yields the single deterministic orientation. So
  // "all three kNoRandom" is literally "zero draws, one fixed rotation", not an
  // approximation of it.
  return azimuth_dist.type == DistributionType::kNoRandom && latitude_dist.type == DistributionType::kNoRandom &&
         roll_dist.type == DistributionType::kNoRandom;
}


// The on-disk JSON keys stay "mean" / "std": that is the published config file format (see
// doc/configuration.md and examples/config_example.json). Only the C++ member names changed, so
// this is the one place where the two vocabularies meet. Serialization is type-erased by nature —
// it must round-trip every DistributionType — hence the generic members rather than the named
// accessors.
void to_json(nlohmann::json& obj, const Distribution& dist) {
  if (dist.type == DistributionType::kNoRandom) {
    obj = dist.center;
  } else {
    obj["type"] = dist.type;
    obj["mean"] = dist.center;
    obj["std"] = dist.spread;
  }
}

void from_json(const nlohmann::json& obj, Distribution& dist) {
  if (obj.is_number()) {
    dist.type = DistributionType::kNoRandom;
    obj.get_to(dist.center);
  } else if (obj.is_object()) {
    // "type" is required. It used to be optional, in which case the object kept whatever type the
    // destination already carried — so the SAME written form meant different things depending on
    // who called in: an axis `zenith` slot stayed kNoRandom and dropped "std" outright, while
    // `azimuth` / `roll` stayed kUniform/360 and dropped what "mean" was asking for. No commit
    // ever chose that; it fell out of "overwrite only the keys that are present". The ruling was
    // to delete the implicit default rather than write it down, so there is nothing left for a
    // downstream parser to mirror differently.
    //
    // The check reaches the shape scalars (height / prism_h / upper_h / lower_h / face_distance[])
    // as well, since they parse through this same function. That is the intended scope, not a
    // miss to be narrowed later: one entry point, one rule. Measured over this repo's corpus
    // before landing — 68 configs, 149 crystals — no object-shaped slot of either kind omits
    // "type", so nothing in tree changes how it loads.
    if (!obj.contains("type")) {
      throw std::invalid_argument(
          "distribution object is missing required key \"type\". Write either a bare number "
          "(e.g. 20) or an object naming the distribution (e.g. {\"type\": \"gauss\", \"mean\": 20, \"std\": 5}).");
    }
    obj.at("type").get_to(dist.type);
    if (obj.contains("mean")) {
      obj.at("mean").get_to(dist.center);
    }
    if (obj.contains("std")) {
      obj.at("std").get_to(dist.spread);
    }
  } else {
    // Rejecting rather than logging: neither branch runs, so `dist` silently keeps whatever the
    // caller handed in — the destination's default, not anything written in the document. Same
    // honest boundary as the missing-"type" branch above: this function serves every distribution
    // slot in the document and cannot name which one it is on.
    throw std::invalid_argument("distribution value is neither a number nor an object: " + obj.dump() +
                                ". Write either a bare number (e.g. 20) or an object naming the distribution "
                                "(e.g. {\"type\": \"gauss\", \"mean\": 20, \"std\": 5}).");
  }
}

namespace {

// The three axis keys, spelled here and nowhere else in the tree. Indexed by
// AxisScalar; every layer that reads or writes them goes through
// AxisScalarKeyName below.
constexpr const char* kAxisScalarKeys[kAxisScalarCount] = { "zenith", "azimuth", "roll" };

}  // namespace

const char* AxisScalarKeyName(int slot) {
  if (slot < 0 || slot >= kAxisScalarCount) {
    return nullptr;
  }
  return kAxisScalarKeys[slot];
}

std::string FormatAxisSlotHint(const std::string& slot_key) {
  return "Write axis." + slot_key + " either as a bare number for a fixed angle (e.g. \"" + slot_key +
         "\": 20) or as an object naming the distribution (e.g. \"" + slot_key +
         "\": {\"type\": \"gauss\", \"mean\": 20, \"std\": 5}).";
}

namespace {

// Parse one slot of a crystal's `axis` object.
//
// The missing-"type" rejection already lives in from_json(Distribution&), but that function serves
// the shape scalars too and therefore cannot know which slot it is on. This narrows a published
// contract, and a hand-written config outside this repo cannot be enumerated — the message is the
// only migration guidance its author will ever get — so the check is repeated here, where the slot
// name is known, to say WHICH slot and how to write it. The one in Distribution::from_json stays:
// it is the backstop for every other caller.
void ParseAxisSlot(const nlohmann::json& axis, const char* slot_key, Distribution& dist) {
  const auto& slot = axis.at(slot_key);
  if (slot.is_object() && !slot.contains("type")) {
    throw std::invalid_argument(std::string("axis.") + slot_key + " is a distribution object with no \"type\". " +
                                FormatAxisSlotHint(slot_key));
  }
  slot.get_to(dist);
}

}  // namespace

void to_json(nlohmann::json& obj, const AxisDistribution& axis) {
  // Zenith: internal latitude → external zenith (zenith center = 90 - latitude center).
  // Must handle kNoRandom (serialized as number) vs others (serialized as object).
  nlohmann::json zenith;
  to_json(zenith, axis.latitude_dist);
  if (zenith.is_number()) {
    zenith = 90.0f - axis.latitude_dist.center;
  } else {
    zenith["mean"] = 90.0f - axis.latitude_dist.center;
  }
  obj[AxisScalarKeyName(kAxisScalarZenith)] = zenith;
  obj[AxisScalarKeyName(kAxisScalarAzimuth)] = axis.azimuth_dist;
  obj[AxisScalarKeyName(kAxisScalarRoll)] = axis.roll_dist;
}

void from_json(const nlohmann::json& obj, AxisDistribution& axis) {
  // A present `axis` requires `zenith`; this has always been so, but it used to be enforced by
  // .at() alone, whose "[json.exception.out_of_range.403] key 'zenith' not found" names neither
  // the crystal nor a way forward. Only the message changes here — the document is rejected
  // exactly as before.
  const char* zenith_key = AxisScalarKeyName(kAxisScalarZenith);
  if (!obj.contains(zenith_key)) {
    throw std::invalid_argument(
        std::string("axis is present but has no \"") + zenith_key +
        "\", which is required whenever `axis` is written at all (omit `axis` entirely to get the "
        "default orientation instead). " +
        FormatAxisSlotHint(zenith_key));
  }
  ParseAxisSlot(obj, zenith_key, axis.latitude_dist);
  axis.latitude_dist.center = 90.0f - axis.latitude_dist.center;

  axis.azimuth_dist.type = DistributionType::kUniform;
  axis.azimuth_dist.center = 0.0f;
  axis.azimuth_dist.spread = 360.0f;
  axis.roll_dist.type = DistributionType::kUniform;
  axis.roll_dist.center = 0.0f;
  axis.roll_dist.spread = 360.0f;

  // These two seeds answer "what if the KEY is absent" — deliberate since 2022, 47 slots in this
  // repo rely on it, and it is untouched here. What is gone is the second, accidental job they
  // used to do: standing in as the `type` of a slot that was written but left typeless.
  const char* azimuth_key = AxisScalarKeyName(kAxisScalarAzimuth);
  if (obj.contains(azimuth_key)) {
    ParseAxisSlot(obj, azimuth_key, axis.azimuth_dist);
  }
  const char* roll_key = AxisScalarKeyName(kAxisScalarRoll);
  if (obj.contains(roll_key)) {
    ParseAxisSlot(obj, roll_key, axis.roll_dist);
  }
}

}  // namespace lumice
