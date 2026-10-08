#ifndef LUMICE_ANALYTIC_DP_CHROMATIC_HPP_
#define LUMICE_ANALYTIC_DP_CHROMATIC_HPP_

// The colour edges and tints of one halo path (LI chromatic.py, ported for scrum 660.4): what
// changes with the refractive index between a red and a blue end of the wavelength pool. Three
// things can — the direction map (dD_P/dn, the ordinary dispersion, reported as
// direction_dispersion and not a mechanism of its own), a weight kink C_k whose TIR onset moves
// with n (d disc_k/dn > 0 means blue reflects totally on a larger set: the kink can only push
// energy toward blue, asserted per point), and a gate of U_P that moves with n (the exit Snell
// discriminant, or an internal incidence cosine off the entry normal: either sign).
//
// Random orientation (the image is a function of delta = D_P alone): a line of S^2 makes a
// colour edge of the delta profile only where its image in D is concentrated. With Delta the
// pointwise shift of the line's D image between the two indices and sigma its width max - min at
// one index, a colour edge is sigma <= EDGE_SPREAD_PER_SHIFT |Delta| and |Delta| >=
// EDGE_MIN_SHIFT_RAD on a line that carries weight (lit_fraction > 0). A single-mirror slab
// (3-1-6) has sigma = 0: the whole kink sits on D = 2 arcsin sqrt(n^2 - 1) and makes the blue rim
// of the dark hole around the antisolar point. A line with sigma > |Delta| still gives a slope
// corner at its D extrema, not a step: its colour is spread and reported not visible.
//
// Oriented crystals (a plate family): when every pose of a class lands near one sky point at
// BOTH indices (the weighted red-blue angle, direction_dispersion, below EDGE_MIN_SHIFT_RAD — a
// slab-like class), the colour is a tint of the whole spot, measured by the ratio of the blue to
// the red weighted power sum A T over the class on a sample of the family (the u-S^2 reduction:
// both weights are functions of u = R^-1 s_hat alone, so the family sample enters through its u
// values). A class whose direction disperses spreads its colours like a spectrum; its ratio is
// reported but gives no tint verdict.
//
// THE THRESHOLDS ARE DECLARED PARAMETERS, NOT CONSTANTS OF NATURE (issue AC3): the index pair,
// the solar-disc edge floor, the spread-per-shift ratio and the tint ratio are provided alongside
// every verdict through ChromaticThresholdsSnapshot — a recorded snapshot of the constants below,
// not a second implementation, paired with the verdict by the consumer (LI puts them on the
// fixture's input side the same way) — so a consumer sees which declared criterion produced a
// label. They are LI's frozen values, copied verbatim (the tint threshold is twice the
// plain-Fresnel dispersion deviation of the calibration classes).
//
// JAX is the authority, this is the derived implementation. LI's raise sites become data: a
// boundary walk that refuses turns into the note "gates not analysed" with coverage_complete
// false (the kink features it already assessed stay), never a hidden partial answer.
//
// Internal header of the analytic kernel: nothing here is part of the C ABI. Dependency
// direction: dp_chromatic -> {dp_field, dp_boundary, dp_weight_kink, path_chain (the reflectance
// kernel of the chain), reflection_group (the class orbit), discovery (the Fibonacci lattice)}.
// The plate family sampler is this layer's own (PlateFamilySpec / SamplePlatePoses below — LI's
// pose_density module was superseded by it at port time). The measure-layer-weighted tint
// ABSOLUTE brightness is out of scope here (B1): this layer reports the ratio and its verdict
// only.

#include <limits>
#include <string>
#include <vector>

#include "analytic/dp_boundary.hpp"
#include "analytic/dp_field.hpp"
#include "analytic/dp_weight_kink.hpp"
#include "analytic/path_evaluation.hpp"
#include "analytic/reflection_group.hpp"

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

// The declared-parameter snapshot that travels with every chromatic verdict (LI
// mc_thresholds_snapshot's field set, minus its bookkeeping strings): the constants above as
// data, so a consumer reads which criterion produced a label. A change to a constant above is a
// change to this record, in the same edit.
struct ChromaticThresholds {
  double n_red = kNRed;
  double n_blue = kNBlue;
  double edge_min_shift_rad = kEdgeMinShiftRad;
  double edge_spread_per_shift = kEdgeSpreadPerShift;
  double calibration_white_max_deviation = kCalibrationWhiteMaxDeviation;
  double tint_ratio_min = kTintRatioMin;
};
ChromaticThresholds ChromaticThresholdsSnapshot();

// ---- random orientation: features of one path --------------------------------------------------------------

enum class ChromaticFeatureKind { kEdge, kGateEdge };
enum class ChromaticColor { kBlue, kRed, kWhite, kNone };
enum class ChromaticVerdictKind { kEdge, kGateEdge, kTint, kUnresolved, kNone };

const char* ChromaticFeatureKindName(ChromaticFeatureKind kind);
const char* ChromaticColorName(ChromaticColor color);
const char* ChromaticVerdictKindName(ChromaticVerdictKind kind);

// One colour source of a path under random orientation. `kind` kEdge = a weight kink C_k,
// kGateEdge = a gate of U_P that moves with n; `source` its margin name. `color` from the sign of
// d margin / dn on the line (positive_fraction of its points positive: blue reflects totally /
// passes the gate on the larger set). Angles in radians: `delta_red` / `delta_blue` the median D
// of the line at each index, `shift` the median pointwise Delta, `spread` sigma (the larger of
// the two indices), `direction_dispersion` the median |dD_P/dn| (n_b - n_r) on the line (finite
// points only: dD_P/dn diverges on the exit TIR curve itself). `contrast`: an edge's 1 - R of the
// disfavoured colour on the favoured colour's kink (the fringe depth), a gate's favoured-colour
// weight on the disfavoured colour's gate over its median weight in U_P. `weight` the median A T
// of the favoured colour on its line, `lit_fraction` the fraction of its points with A T > 0.
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

// The weighted power of a class on a family sample, per index (LI TintMetrics): the mean over the
// sample of sum_members A T for each colour, their ratio, the A T-weighted fraction of internal
// reflections that are total, and the weighted mean angle between the red and blue outgoing
// directions of the same pose over poses lit at both indices (0 for a class whose direction does
// not disperse). ratio / tir fractions are quiet-NaN where their denominators vanish.
struct TintMetrics {
  double energy_red = 0.0;
  double energy_blue = 0.0;
  double ratio = 0.0;
  double tir_fraction_red = 0.0;
  double tir_fraction_blue = 0.0;
  double direction_dispersion = 0.0;
};

// The colour verdict of one face sequence or of a path class. A random-orientation verdict takes
// kind / color / visible from its dominant feature (visible features first, then the largest
// score) and `position` from it (delta of the dominant feature, the blue one's for blue;
// `has_position` false without one). `kUnresolved`: no assessed feature, but a kink or gate
// exists at one index only — an onset between n_red and n_blue, the strongest colour shape,
// which the two-index metrics cannot measure; the notes name it (never a silent kNone). A plate
// verdict has five shapes (LI _tint_verdict, all exhaustive): not lit at all — kNone / none with
// the note; lit at ONE index only — kTint / that index's colour / visible with the note (LI's
// "the extreme tint, not 'no colour'": the single-index power sum is the strongest tint shape
// the two-index criterion can name); dispersing — kNone / none with the note; the ratio inside
// the band ends — kTint / blue or red / visible; the ratio inside the band — kNone / white.
// `coverage_complete` is false when a line the
// verdict rests on was not fully analysed: a weight-kink walk with failed seeds or gates that
// could not be walked; the notes say which.
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

// The knobs of a random-orientation diagnose: the lattice of the kink seeding and the walks (LI
// diagnose's lattice_n).
struct ChromaticOptions {
  int lattice_n = 20000;
  double step = kWalkStepRad;
  int max_walk_steps = kMaxWalkSteps;
};

// The random-orientation colour verdict of one face sequence: every weight kink and every moving
// gate as a feature, the dominant one the verdict (module docstring). A path whose boundary walk
// refuses keeps its kink features and reports the gates as not analysed. Rank-0 paths are not
// accepted here (there is no field to read); the caller labels them point_mass (dp_focus).
ChromaticVerdict Diagnose(const FaceNormalTable& normals, const FacePolygonTable& polygons, const int* slots,
                          int slot_count, double n_red = kNRed, double n_blue = kNBlue,
                          const ChromaticOptions& options = ChromaticOptions());

// The verdict roll-up of features and notes (LI _verdict, exposed as LI's own test does): no
// features makes kUnresolved when a one-sided line was named (never a silent kNone), otherwise
// the dominant feature decides kind / color / visible / position. Diagnose's last step and the
// tests' subject — the one-sided shape has no small real path (the kink onset's presence in U_P
// does not flip across any pair on the prism at these face counts), so like LI the shape is
// asserted at the roll-up itself.
ChromaticVerdict VerdictOf(std::vector<int> faces, const std::vector<ChromaticFeature>& features,
                           std::vector<std::string> notes, double n_red, double n_blue, bool unresolved,
                           bool coverage_complete);

// The plate-class tint verdict of weighted-power metrics (LI _tint_verdict, a standalone function
// there too): the five plate shapes of the ChromaticVerdict doc — not lit (kNone + note), lit at
// one index only (kTint / visible + note, the extreme tint), dispersing (kNone + note), ratio
// tint (kTint / visible) or inside-band white (kNone / white). Sets faces / n_red / n_blue /
// tint / has_tint from the arguments like VerdictOf sets its own.
ChromaticVerdict TintVerdictOf(std::vector<int> faces, const TintMetrics& tint, double n_red, double n_blue);

// ---- oriented crystals: the class verdict (plate family) ----------------------------------------------------

// The plate family of LI chromatic.PlateFamily: c axis tilted from the zenith by |N(0,
// zenith_std_deg)| in a uniform direction, uniform spin. The C++ sampler draws spin, tilt,
// tilt direction per pose from its own stream (std::mt19937_64 seeded by `seed`) — LI's numpy
// PCG64 stream does not cross languages, and the fixture tolerance (5e-2, > 15 sigma of
// LI's split-half error) is the parity caliber, not stream equality.
struct PlateFamilySpec {
  double sun_altitude_deg = 9.0;
  double zenith_std_deg = 1.0;
  int samples = 100000;
  unsigned long long seed = 3;
};

// The PBD class verdict of `representative` under a plate family: the class members
// (reflection_group's orbit, sorted), the members lit at each index (positive weighted power
// somewhere on the sample), the tint metrics over the sample, and the tint verdict of the ratio
// (module docstring). Member feasibility is decided by the kernel itself on the family's sample,
// not by any n-constant gate table: a member is lit when some sampled pose carries A T > 0.
struct ClassVerdict {
  std::vector<int> representative;
  std::vector<std::vector<int>> members;
  std::vector<std::vector<int>> lit_members_red;
  std::vector<std::vector<int>> lit_members_blue;
  ChromaticVerdict verdict;
};

ClassVerdict DiagnoseClass(const FaceNormalTable& normals, const FacePolygonTable& polygons, const int* representative,
                           int representative_count, const PlateFamilySpec& family, double n_red = kNRed,
                           double n_blue = kNBlue);

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_DP_CHROMATIC_HPP_
