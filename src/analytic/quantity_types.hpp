#ifndef LUMICE_ANALYTIC_QUANTITY_TYPES_HPP_
#define LUMICE_ANALYTIC_QUANTITY_TYPES_HPP_


namespace lumice::analytic {

constexpr double kRankZeroFoldTolerance = 1e-12;
constexpr double kRankZeroWedgeToleranceDeg = 1e-9;

enum class PointMassMethod { kLatticeMean, kPsiAverage };

enum class FamilyDimension {
  kZenith,
  kAzimuth,
  kRoll,
};

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_QUANTITY_TYPES_HPP_
