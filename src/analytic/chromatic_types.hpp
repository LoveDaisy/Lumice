#ifndef LUMICE_ANALYTIC_CHROMATIC_TYPES_HPP_
#define LUMICE_ANALYTIC_CHROMATIC_TYPES_HPP_

#include <cmath>
#include <limits>
#include <string>
#include <vector>
namespace lumice::analytic {


// ---- constants (LI chromatic.py, verbatim) ----------------------------------------------------------------

// Red and blue ends of Lumice's wavelength pool, the index pair of every verdict (parameters,
// not constants of the criterion).
constexpr double kNRed = 1.307;
constexpr double kNBlue = 1.317;
// A colour edge must be displaced by at least the sun's angular diameter: the solar disc smears
// every sky feature over ~0.5 deg, so a smaller red/blue shift is blurred into its own edge.
constexpr double kEdgeMinShiftRad = 0.5 * 3.14159265358979323846 / 180.0;
// ... and the line's own spread in D must not exceed that shift: with sigma > |Delta| the red and
// blue images of the line overlap more than they separate and the step becomes a gradual slope.
constexpr double kEdgeSpreadPerShift = 1.0;
// A plate tint is reported when blue / red weighted power leaves [1 / kTintRatioMin,
// kTintRatioMin]; frozen on calibration classes whose internal reflections do not switch between
// total and partial across the index pair (they stay within this deviation of 1 — plain Fresnel
// dispersion); the threshold is twice that.
constexpr double kCalibrationWhiteMaxDeviation = 0.05;
constexpr double kTintRatioMin = 1.0 + 2.0 * kCalibrationWhiteMaxDeviation;
// A gate that does not move with n (its margin at the other index vanishes on the curve to this)
// is no colour source.
constexpr double kGateStaticAtol = 1e-9;
// Fibonacci lattice of the gate-contrast weight reference inside U_P.
constexpr int kContrastLatticeN = 20000;
// A feature's red-blue shift statistic pairs every red curve point with its nearest blue point
// (ShiftOf, O(red_points * blue_points)). A curve pair denser than this many point products is
// DECLARED not assessed — note + coverage_complete false, the "gates not analysed" honesty
// shape — never compared in a truncated way (a half-computed median is a fabricated statistic).
// Execution boundary, not a numeric-semantic change: no walk, threshold or verdict constant is
// touched. The default sits ~8x above the largest measured legal corpus pair (closed-form
// circles, 3600 x 3600) and ~6000x below the degenerate-family pathology that motivated it
// (duplicated marched arcs, corpus C10: 780825 x 789472 measured hours per member).
constexpr long long kMaxShiftComparisonPairs = 100000000;

struct ChromaticThresholds {
  double n_red = kNRed;
  double n_blue = kNBlue;
  double edge_min_shift_rad = kEdgeMinShiftRad;
  double edge_spread_per_shift = kEdgeSpreadPerShift;
  double calibration_white_max_deviation = kCalibrationWhiteMaxDeviation;
  double tint_ratio_min = kTintRatioMin;
};

enum class ChromaticFeatureKind { kEdge, kGateEdge };

enum class ChromaticColor { kBlue, kRed, kWhite, kNone };

enum class ChromaticVerdictKind { kEdge, kGateEdge, kTint, kUnresolved, kNone };

struct ChromaticFeature {
  ChromaticFeatureKind kind = ChromaticFeatureKind::kEdge;
  std::string source;
  ChromaticColor color = ChromaticColor::kBlue;
  double positive_fraction = 0.0;
  double delta_red = 0.0;
  double delta_blue = 0.0;
  double shift = 0.0;
  double spread = 0.0;
  double direction_dispersion = 0.0;
  double contrast = 0.0;
  double weight = 0.0;
  double lit_fraction = 0.0;
  bool visible = false;

  // |Delta| / sigma: how far the line is from the edge threshold (infinity at sigma = 0).
  double Score() const { return spread > 0.0 ? std::fabs(shift) / spread : std::numeric_limits<double>::infinity(); }
};

struct TintMetrics {
  double energy_red = 0.0;
  double energy_blue = 0.0;
  double ratio = 0.0;
  double tir_fraction_red = 0.0;
  double tir_fraction_blue = 0.0;
  double direction_dispersion = 0.0;
};

struct ChromaticVerdict {
  std::vector<int> faces;
  ChromaticVerdictKind kind = ChromaticVerdictKind::kNone;
  ChromaticColor color = ChromaticColor::kNone;
  bool visible = false;
  bool has_position = false;
  double position = 0.0;
  std::vector<ChromaticFeature> features;
  bool has_tint = false;
  TintMetrics tint;
  std::vector<std::string> notes;
  double n_red = kNRed;
  double n_blue = kNBlue;
  bool coverage_complete = true;
};

ChromaticThresholds ChromaticThresholdsSnapshot();

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_CHROMATIC_TYPES_HPP_
