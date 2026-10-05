#ifndef CORE_SHAPE_SAMPLE_H_
#define CORE_SHAPE_SAMPLE_H_

#include <array>
#include <cstddef>

#include "core/crystal_param.hpp"
#include "core/random.hpp"

namespace lumice {

struct ShapeDrawSlot {
  bool applicable = false;
  int leader_slot = -1;
  int group = 0;
  Distribution distribution;
};
using ShapeDrawPlan = std::array<ShapeDrawSlot, kShapeScalarCount>;
ShapeDrawPlan BuildShapeDrawPlan(const PrismCrystalParam& param);
ShapeDrawPlan BuildShapeDrawPlan(const PyramidCrystalParam& param);

// Already transformed by the LEADER distribution, but not folded by its use
// site. Distinct from DistributionLatentDraw to prevent follower rescaling.
struct ShapeLeaderValue {
  int slot = -1;
  float value = 0.0f;
};
struct ShapeLeaderValues {
  std::array<ShapeLeaderValue, kShapeScalarCount> values{};
  size_t size = 0;
};
struct ShapeSample {
  ShapeDrawPlan plan;
  std::array<float, kShapeScalarCount> raw{};
  std::array<float, kShapeScalarCount> consumed{};
};
enum class ShapeSampleStatus { kOk, kInvalidLeader, kDuplicateLeader, kMissingLeader, kNonFiniteValue };
ShapeSampleStatus RealizeShape(const ShapeDrawPlan& plan, const ShapeLeaderValues& values, ShapeSample* out);
// The only RNG adapter. Iterates leaders in ShapeScalar order, with no heap allocation.
ShapeLeaderValues DrawShapeLeaders(RandomNumberGenerator& rng, const ShapeDrawPlan& plan);

float SamplePrismShapeScalars(RandomNumberGenerator& rng, const PrismCrystalParam& p, float dist_out[6]);
void SamplePyramidShapeScalars(RandomNumberGenerator& rng, const PyramidCrystalParam& p, float& h1, float& h2,
                               float& h3, float dist_out[6]);

}  // namespace lumice
#endif  // CORE_SHAPE_SAMPLE_H_
