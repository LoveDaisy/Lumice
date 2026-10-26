#include "raypath/detail/measure_scope.hpp"

#include "core/crystal.hpp"

namespace lumice::raypath::survey {
namespace {

// The out-of-vocabulary distribution types' own names (the refusal modes name them).
const char* DistributionTypeSlug(DistributionType type) {
  switch (type) {
    case DistributionType::kZigzag:
      return "zigzag";
    case DistributionType::kLaplacian:
      return "laplacian";
    case DistributionType::kGaussianLegacy:
      return "gaussian_legacy";
    case DistributionType::kNoRandom:
    case DistributionType::kUniform:
    case DistributionType::kGaussian:
      return "uniform";
  }
  return "uniform";
}

// ---- the measure extraction -------------------------------------------------------------------------

// The routing-vocabulary class of one distribution slot. The two default semantics are the
// config loader's own facts, read not re-derived: kNoRandom is the fixed value, a full-turn
// kUniform (detail::IsFullTurnUniform — the same predicate the axis's own symmetry members
// read) is the free uniform. A zero-sigma gauss is read as fixed (the degenerate limit of the
// concentration language; the density side refuses it independently).
enum class DimClass { kFixed, kGauss, kUniformFull, kUniformBounded, kOutOfVocabulary };

DimClass ClassifyDim(const Distribution& dist) {
  switch (dist.type) {
    case DistributionType::kNoRandom:
      return DimClass::kFixed;
    case DistributionType::kGaussian:
      return dist.spread > 0.0f ? DimClass::kGauss : DimClass::kFixed;
    case DistributionType::kUniform:
      return detail::IsFullTurnUniform(dist.type, dist.spread) ? DimClass::kUniformFull : DimClass::kUniformBounded;
    case DistributionType::kZigzag:
    case DistributionType::kLaplacian:
    case DistributionType::kGaussianLegacy:
      return DimClass::kOutOfVocabulary;
  }
  return DimClass::kOutOfVocabulary;
}

FamilyDimMeasure MeasureOf(DimClass cls, const Distribution& dist, bool latitude_slot) {
  FamilyDimMeasure out;
  const double center = latitude_slot ? 90.0 - static_cast<double>(dist.center) : static_cast<double>(dist.center);
  switch (cls) {
    case DimClass::kFixed:
      out.kind = FamilyDimKind::kFixed;
      out.center_deg = center;
      out.width_deg = 0.0;
      break;
    case DimClass::kGauss:
      out.kind = FamilyDimKind::kGauss;
      out.center_deg = center;
      out.width_deg = static_cast<double>(dist.spread);
      break;
    case DimClass::kUniformFull:
    case DimClass::kUniformBounded:
    case DimClass::kOutOfVocabulary:
      out.kind = FamilyDimKind::kUniformFull;
      out.center_deg = center;                           // the raw midpoint echo
      out.width_deg = static_cast<double>(dist.spread);  // the raw range, display only
      break;
  }
  return out;
}

}  // namespace

FamilyMeasureReading ReadFamilyMeasure(const AxisDistribution& axis) {
  FamilyMeasureReading out;
  const Distribution* dims[3] = { &axis.latitude_dist, &axis.azimuth_dist, &axis.roll_dist };
  const char* names[3] = { "zenith", "azimuth", "roll" };
  DimClass classes[3] = {};
  for (int i = 0; i < 3; i++) {
    classes[i] = ClassifyDim(*dims[i]);
  }

  // The all-fixed axis is the single pose — the one legal shape with a frozen azimuth (the 0D
  // arm; the ladder never freezes azimuth, the rank entry's kRefusedAzimuthFrozen semantics).
  out.single_pose = classes[0] == DimClass::kFixed && classes[1] == DimClass::kFixed && classes[2] == DimClass::kFixed;

  // The v1 domain gate (AC3): azimuth full-uniform, zenith {fixed, gauss}, roll
  // {fixed, gauss, uniform-full}; the first offender in canonical order is named, and nothing
  // is approximated into the domain (a bounded uniform is never swapped for a matching gauss).
  for (int i = 0; i < 3; i++) {
    std::string shape;
    if (classes[i] == DimClass::kOutOfVocabulary) {
      shape = std::string(names[i]) + "_" + DistributionTypeSlug(dims[i]->type);
    } else if (i == 0 /* zenith */) {
      if (classes[i] == DimClass::kUniformBounded) {
        shape = "zenith_bounded_uniform";
      } else if (classes[i] == DimClass::kUniformFull) {
        // A Haar axis routed to the full-dimensional arm before this gate; any other
        // full-uniform zenith has no kernel family to carry it.
        shape = "zenith_full_uniform";
      }
    } else if (i == 1 /* azimuth */) {
      if (out.single_pose) {
        continue;
      }
      if (classes[i] == DimClass::kFixed) {
        shape = "azimuth_fixed";
      } else if (classes[i] == DimClass::kGauss) {
        shape = "azimuth_gaussian";
      } else if (classes[i] == DimClass::kUniformBounded) {
        shape = "azimuth_bounded_uniform";
      }
    } else /* roll */ {
      if (classes[i] == DimClass::kUniformBounded) {
        shape = "roll_bounded_uniform";
      }
    }
    if (!shape.empty()) {
      out.refusal_mode = std::string(kMeasureOutsideDomainPrefix) + shape;
      return out;
    }
  }

  out.measure.zenith = MeasureOf(classes[0], *dims[0], true);
  out.measure.azimuth = MeasureOf(classes[1], *dims[1], false);
  out.measure.roll = MeasureOf(classes[2], *dims[2], false);
  out.ok = true;
  return out;
}

}  // namespace lumice::raypath::survey
