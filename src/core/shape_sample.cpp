#include "core/shape_sample.hpp"

#include <algorithm>
#include <cmath>

#include "util/fatal.hpp"

namespace lumice {
namespace {

template <typename Param>
ShapeDrawPlan BuildPlan(Param param) {
  CanonicalizeSyncGroups(param);
  const auto slots = GetShapeScalarSlots(param);
  ShapeDrawPlan plan{};
  for (int i = 0; i < kShapeScalarCount; ++i) {
    if (!slots[i])
      continue;
    auto& slot = plan[i];
    slot = { true, i, param.sync_group_[i], *slots[i] };
    if (slot.group == 0)
      continue;
    for (int j = 0; j < i; ++j) {
      if (plan[j].applicable && plan[j].group == slot.group) {
        slot.leader_slot = j;
        break;
      }
    }
  }
  return plan;
}

ShapeSample DrawAndRealize(RandomNumberGenerator& rng, const ShapeDrawPlan& plan) {
  ShapeSample sample;
  if (RealizeShape(plan, DrawShapeLeaders(rng, plan), &sample) != ShapeSampleStatus::kOk) {
    FatalAbort("Shape sampler produced invalid leader values");
  }
  return sample;
}
}  // namespace

ShapeDrawPlan BuildShapeDrawPlan(const PrismCrystalParam& param) {
  return BuildPlan(param);
}
ShapeDrawPlan BuildShapeDrawPlan(const PyramidCrystalParam& param) {
  return BuildPlan(param);
}

ShapeLeaderValues DrawShapeLeaders(RandomNumberGenerator& rng, const ShapeDrawPlan& plan) {
  ShapeLeaderValues values;
  for (int i = 0; i < kShapeScalarCount; ++i) {
    if (plan[i].applicable && plan[i].leader_slot == i) {
      values.values[values.size++] = { i, rng.Get(plan[i].distribution) };
    }
  }
  return values;
}

ShapeSampleStatus RealizeShape(const ShapeDrawPlan& plan, const ShapeLeaderValues& values, ShapeSample* out) {
  *out = {};
  if (values.size > values.values.size())
    return ShapeSampleStatus::kInvalidLeader;
  bool seen[kShapeScalarCount]{};
  float raw[kShapeScalarCount]{};
  for (size_t i = 0; i < values.size; ++i) {
    const auto value = values.values[i];
    if (value.slot < 0 || value.slot >= kShapeScalarCount || !plan[value.slot].applicable ||
        plan[value.slot].leader_slot != value.slot)
      return ShapeSampleStatus::kInvalidLeader;
    if (seen[value.slot])
      return ShapeSampleStatus::kDuplicateLeader;
    if (!std::isfinite(value.value))
      return ShapeSampleStatus::kNonFiniteValue;
    seen[value.slot] = true;
    raw[value.slot] = value.value;
  }
  ShapeSample sample;
  sample.plan = plan;
  for (int i = 0; i < kShapeScalarCount; ++i) {
    if (!plan[i].applicable)
      continue;
    const int leader = plan[i].leader_slot;
    if (leader < 0 || leader > i || !seen[leader])
      return ShapeSampleStatus::kMissingLeader;
    sample.raw[i] = raw[leader];
    sample.consumed[i] = i < kShapeScalarFace0 ? std::abs(raw[leader]) : raw[leader];
  }
  *out = sample;
  return ShapeSampleStatus::kOk;
}

float SamplePrismShapeScalars(RandomNumberGenerator& rng, const PrismCrystalParam& p, float dist_out[6]) {
  const auto sample = DrawAndRealize(rng, BuildShapeDrawPlan(p));
  std::copy_n(sample.consumed.data() + kShapeScalarFace0, 6, dist_out);
  return sample.consumed[kShapeScalarHeight];
}

void SamplePyramidShapeScalars(RandomNumberGenerator& rng, const PyramidCrystalParam& p, float& h1, float& h2,
                               float& h3, float dist_out[6]) {
  const auto sample = DrawAndRealize(rng, BuildShapeDrawPlan(p));
  h1 = sample.consumed[kShapeScalarUpperH];
  h2 = sample.consumed[kShapeScalarPrismH];
  h3 = sample.consumed[kShapeScalarLowerH];
  std::copy_n(sample.consumed.data() + kShapeScalarFace0, 6, dist_out);
}
}  // namespace lumice
