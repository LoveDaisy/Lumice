#include "analytic/dp_chromatic.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <numeric>
#include <random>
#include <sstream>
#include <utility>

#include "analytic/discovery.hpp"
#include "analytic/path_chain.hpp"
#include "analytic/so3.hpp"

namespace lumice::analytic {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();
// The probe sun of the random-orientation weights: A T of a body direction u is taken at a pose
// with R u = this sun (A is twist invariant; LI _PROBE_SUN).
constexpr double kProbeSun[3] = { 0.0, 0.0, 1.0 };

double Dot3(const double a[3], const double b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

double Norm3(const double v[3]) {
  return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

double AngleBetween(const double a[3], const double b[3]) {
  const double cross[3] = { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };
  return std::atan2(Norm3(cross), Dot3(a, b));
}

void FibonacciPoint(int n, int i, double u[3]) {
  double f[3];
  LatticePoint(n, i, f);
  u[0] = -f[0];
  u[1] = -f[1];
  u[2] = -f[2];
}

// Median of a sample (LI np.median: the average of the two middle entries on an even count).
double Median(std::vector<double> values) {
  if (values.empty()) {
    return kNaN;
  }
  const size_t mid = values.size() / 2;
  std::nth_element(values.begin(), values.begin() + static_cast<long>(mid), values.end());
  const double upper = values[mid];
  if (values.size() % 2 == 1) {
    return upper;
  }
  const double lower = *std::max_element(values.begin(), values.begin() + static_cast<long>(mid));
  return 0.5 * (lower + upper);
}

// Median of the finite entries: dD_P/dn is infinite on the exit TIR curve itself (D_P is
// Holder-1/2 there), a gate's value comes from its sampling off the curve (LI _finite_median).
double FiniteMedian(const std::vector<double>& values) {
  std::vector<double> finite;
  for (double value : values) {
    if (std::isfinite(value)) {
      finite.push_back(value);
    }
  }
  return Median(std::move(finite));
}

double Ptp(const std::vector<double>& values) {
  const auto pair = std::minmax_element(values.begin(), values.end());
  return *pair.second - *pair.first;
}

// w = A_P T_P of body directions `points` (3N), the u-S^2 image of LI's weighted_power at the
// poses aligning each u to the probe sun: both weights are functions of u alone, and the field
// layer's Sample(u) is that function (dp_field's module contract).
std::vector<double> BodyWeight(const DeviationField& field, const std::vector<double>& points) {
  std::vector<double> weight(points.size() / 3);
  for (size_t k = 0; k + 2 < points.size(); k += 3) {
    const FieldSample sample = field.Sample(&points[k]);
    weight[k / 3] = sample.a_p * sample.t_p;
  }
  return weight;
}

// The median A T of the lit lattice points inside U_P, 0 when none is lit (LI _median_weight_inside).
double MedianWeightInside(const DeviationField& field, int lattice_n) {
  std::vector<double> lit;
  for (int i = 0; i < lattice_n; i++) {
    double u[3];
    FibonacciPoint(lattice_n, i, u);
    if (!InsideUp(field, u)) {
      continue;
    }
    const FieldSample sample = field.Sample(u);
    const double w = sample.a_p * sample.t_p;
    if (w > 0.0) {
      lit.push_back(w);
    }
  }
  return lit.empty() ? 0.0 : Median(std::move(lit));
}

// Median over the blue line of D_blue(u) - D_red(nearest red point) (LI _shift): nearest by the
// largest dot, the monotone twin of the smallest angle for unit vectors.
double ShiftOf(const std::vector<double>& red_points, const std::vector<double>& red_values,
               const std::vector<double>& blue_points, const std::vector<double>& blue_values) {
  const size_t red_count = red_values.size();
  std::vector<double> differences;
  differences.reserve(blue_values.size());
  for (size_t b = 0; b + 2 < blue_points.size(); b += 3) {
    size_t nearest = 0;
    double best_dot = -kInf;
    for (size_t r = 0; r < red_count; r++) {
      const double dot = Dot3(&blue_points[b], &red_points[3 * r]);
      if (dot > best_dot) {
        best_dot = dot;
        nearest = r;
      }
    }
    differences.push_back(blue_values[b / 3] - red_values[nearest]);
  }
  return Median(std::move(differences));
}

bool IsVisible(double shift, double spread, double lit_fraction) {
  return std::fabs(shift) >= kEdgeMinShiftRad && spread <= kEdgeSpreadPerShift * std::fabs(shift) && lit_fraction > 0.0;
}

std::string Number(double value) {
  std::ostringstream out;
  out << value;
  return out.str();
}

// Unpolarized reflectance of one internal face at LI's margin values (LI internal_reflectance =
// the engine's GetReflectRatio in these variables): R = 1 under TIR (the chain's Reflectance
// clamps its discriminant at 0), else 1 - T(n -> 1) with cos_t = sqrt(-disc). The discriminant
// sign flip is the one LI->chain convention (dp_field's DomainMargins writes LI's sign).
double InternalReflectance(double index, double incidence_cosine, double tir_discriminant) {
  return chain_detail::Reflectance(incidence_cosine, index, -tir_discriminant);
}

// The points and D values of one curve (a TIR onset at both indices, or one margin's boundary
// pieces), finite entries only where the caller filters them (LI _pieces_by_margin's shape).
struct GateCurve {
  std::vector<double> points;
  std::vector<double> values;
};

// LI KinkCurve's points / values members: the concatenation over the arcs (an empty curve is an
// empty list). 660.3's struct keeps the arcs; the concatenation lives here, at its consumer.
GateCurve CurveOf(const KinkCurve& kink) {
  GateCurve curve;
  for (const KinkArc& arc : kink.arcs) {
    curve.points.insert(curve.points.end(), arc.points.begin(), arc.points.end());
    curve.values.insert(curve.values.end(), arc.values.begin(), arc.values.end());
  }
  return curve;
}

// The kink feature of one TIR onset at both indices (LI _kink_feature).
ChromaticFeature KinkFeature(const DeviationField& red, const DeviationField& blue, const KinkCurve& kink_red,
                             const KinkCurve& kink_blue, int slot_count) {
  const int k = kink_red.margin;
  const int cosine = k - 1;  // the same step's incidence cosine
  const GateCurve curve_red = CurveOf(kink_red);
  const GateCurve curve_blue = CurveOf(kink_blue);
  std::vector<double> d_dn;
  for (size_t p = 0; p + 2 < curve_red.points.size(); p += 3) {
    const FieldJet jet = red.Differentiate(&curve_red.points[p]);
    d_dn.push_back(jet.margins_dn[k]);
  }
  const size_t count = curve_red.values.size();
  const double positive =
      static_cast<double>(std::count_if(d_dn.begin(), d_dn.end(), [](double v) { return v > 0.0; })) /
      static_cast<double>(count);
  const ChromaticColor color = positive >= 0.5 ? ChromaticColor::kBlue : ChromaticColor::kRed;
  const DeviationField& favoured = color == ChromaticColor::kBlue ? blue : red;
  const DeviationField& disfavoured = color == ChromaticColor::kBlue ? red : blue;
  const GateCurve& favoured_curve = color == ChromaticColor::kBlue ? curve_blue : curve_red;

  // The disfavoured colour's reflectance on the favoured colour's kink line: the fringe depth.
  std::vector<double> reflectance;
  for (size_t p = 0; p + 2 < favoured_curve.points.size(); p += 3) {
    double margins[2 * kMaxFaceCount];
    disfavoured.DomainMarginsAt(&favoured_curve.points[p], margins);
    reflectance.push_back(InternalReflectance(disfavoured.RefractiveIndex(), margins[cosine], margins[k]));
  }
  const std::vector<double> weight = BodyWeight(favoured, favoured_curve.points);
  const double shift = ShiftOf(curve_red.points, curve_red.values, curve_blue.points, curve_blue.values);
  const double spread = std::max(kink_red.Spread(), kink_blue.Spread());
  const double lit =
      static_cast<double>(std::count_if(weight.begin(), weight.end(), [](double v) { return v > 0.0; })) /
      static_cast<double>(weight.size());

  std::vector<double> dispersion;
  for (size_t p = 0; p + 2 < curve_red.points.size(); p += 3) {
    dispersion.push_back(std::fabs(red.Differentiate(&curve_red.points[p]).d_p_dn));
  }
  ChromaticFeature feature;
  feature.kind = ChromaticFeatureKind::kEdge;
  feature.source = MarginName(slot_count, k);
  feature.color = color;
  feature.positive_fraction = positive;
  feature.delta_red = Median(curve_red.values);
  feature.delta_blue = Median(curve_blue.values);
  feature.shift = shift;
  feature.spread = spread;
  feature.direction_dispersion =
      Median(std::move(dispersion)) * std::fabs(blue.RefractiveIndex() - red.RefractiveIndex());
  feature.contrast = 1.0 - Median(std::move(reflectance));
  feature.weight = Median(weight);
  feature.lit_fraction = lit;
  feature.visible = IsVisible(shift, spread, lit);
  return feature;
}

// The points and D values of one margin's boundary pieces, finite entries only (LI _pieces_by_margin).
GateCurve PiecesByMargin(const BoundaryWalkRecord& record, int margin) {
  GateCurve curve;
  for (const BoundaryPiece& piece : record.pieces) {
    if (piece.margin != margin) {
      continue;
    }
    for (size_t k = 0; k < piece.values.size(); k++) {
      if (std::isfinite(piece.values[k])) {
        curve.points.push_back(piece.points[3 * k]);
        curve.points.push_back(piece.points[3 * k + 1]);
        curve.points.push_back(piece.points[3 * k + 2]);
        curve.values.push_back(piece.values[k]);
      }
    }
  }
  return curve;
}

// Every gate that bounds U_P at both indices and moves with n (LI _gate_features); margins that
// bound at one index only land in `unresolved`.
std::vector<ChromaticFeature> GateFeatures(const DeviationField& red, const DeviationField& blue,
                                           const BoundaryWalkRecord& red_record, const BoundaryWalkRecord& blue_record,
                                           int slot_count, std::vector<std::string>* unresolved) {
  const int margin_count = 2 * slot_count;
  auto has_curve = [&](const BoundaryWalkRecord& record, int margin) {
    return !PiecesByMargin(record, margin).values.empty();
  };
  std::vector<ChromaticFeature> features;
  for (int margin = 0; margin < margin_count; margin++) {
    const bool in_red = has_curve(red_record, margin);
    const bool in_blue = has_curve(blue_record, margin);
    if (in_red != in_blue) {
      const double index = in_red ? red.RefractiveIndex() : blue.RefractiveIndex();
      unresolved->push_back(MarginName(slot_count, margin) + ": bounds U_P at n = " + Number(index) +
                            " only (not assessed)");
      continue;
    }
    if (!in_red) {
      continue;
    }
    const GateCurve curve_red = PiecesByMargin(red_record, margin);
    const GateCurve curve_blue = PiecesByMargin(blue_record, margin);
    // A gate that does not move with n is no colour source (its margin vanishes on the other
    // index's curve to kGateStaticAtol).
    double largest = 0.0;
    for (size_t p = 0; p + 2 < curve_red.points.size(); p += 3) {
      double margins[2 * kMaxFaceCount];
      blue.DomainMarginsAt(&curve_red.points[p], margins);
      largest = std::max(largest, std::fabs(margins[margin]));
    }
    if (largest <= kGateStaticAtol) {
      continue;
    }

    std::vector<double> d_dn;
    for (size_t p = 0; p + 2 < curve_red.points.size(); p += 3) {
      const FieldJet jet = red.Differentiate(&curve_red.points[p]);
      d_dn.push_back(jet.margins_dn[margin]);
    }
    const double positive =
        static_cast<double>(std::count_if(d_dn.begin(), d_dn.end(), [](double v) { return v > 0.0; })) /
        static_cast<double>(d_dn.size());
    const ChromaticColor color = positive >= 0.5 ? ChromaticColor::kBlue : ChromaticColor::kRed;
    const DeviationField& favoured = color == ChromaticColor::kBlue ? blue : red;
    const GateCurve& disfavoured_curve = color == ChromaticColor::kBlue ? curve_red : curve_blue;
    const GateCurve& favoured_curve = color == ChromaticColor::kBlue ? curve_blue : curve_red;

    const double reference = MedianWeightInside(favoured, kContrastLatticeN);
    const std::vector<double> across = BodyWeight(favoured, disfavoured_curve.points);
    const std::vector<double> weight = BodyWeight(favoured, favoured_curve.points);
    const double shift = ShiftOf(curve_red.points, curve_red.values, curve_blue.points, curve_blue.values);
    const double spread = std::max(Ptp(curve_red.values), Ptp(curve_blue.values));
    const double lit =
        static_cast<double>(std::count_if(weight.begin(), weight.end(), [](double v) { return v > 0.0; })) /
        static_cast<double>(weight.size());

    std::vector<double> dispersion;
    for (size_t p = 0; p + 2 < curve_red.points.size(); p += 3) {
      dispersion.push_back(std::fabs(red.Differentiate(&curve_red.points[p]).d_p_dn));
    }
    ChromaticFeature feature;
    feature.kind = ChromaticFeatureKind::kGateEdge;
    feature.source = MarginName(slot_count, margin);
    feature.color = color;
    feature.positive_fraction = positive;
    feature.delta_red = Median(curve_red.values);
    feature.delta_blue = Median(curve_blue.values);
    feature.shift = shift;
    feature.spread = spread;
    feature.direction_dispersion = FiniteMedian(dispersion) * std::fabs(blue.RefractiveIndex() - red.RefractiveIndex());
    feature.contrast = reference > 0.0 ? Median(across) / reference : 0.0;
    feature.weight = Median(weight);
    feature.lit_fraction = lit;
    feature.visible = IsVisible(shift, spread, lit);
    features.push_back(feature);
  }
  return features;
}

// The dominant feature: visible first, then the largest score (LI _dominant; first maximal wins).
const ChromaticFeature* Dominant(const std::vector<ChromaticFeature>& features) {
  const ChromaticFeature* top = nullptr;
  for (const ChromaticFeature& feature : features) {
    if (top == nullptr || (feature.visible && !top->visible) ||
        (feature.visible == top->visible && feature.Score() > top->Score())) {
      top = &feature;
    }
  }
  return top;
}

}  // namespace

// The verdict roll-up of the features and notes (LI _verdict).
ChromaticVerdict VerdictOf(std::vector<int> faces, const std::vector<ChromaticFeature>& features,
                           std::vector<std::string> notes, double n_red, double n_blue, bool unresolved,
                           bool coverage_complete) {
  ChromaticVerdict verdict;
  verdict.faces = std::move(faces);
  verdict.notes = std::move(notes);
  verdict.n_red = n_red;
  verdict.n_blue = n_blue;
  verdict.coverage_complete = coverage_complete;
  if (features.empty()) {
    verdict.kind = unresolved ? ChromaticVerdictKind::kUnresolved : ChromaticVerdictKind::kNone;
    return verdict;
  }
  const ChromaticFeature* top = Dominant(features);
  verdict.kind =
      top->kind == ChromaticFeatureKind::kEdge ? ChromaticVerdictKind::kEdge : ChromaticVerdictKind::kGateEdge;
  verdict.color = top->color;
  verdict.visible = top->visible;
  verdict.has_position = true;
  verdict.position = top->color == ChromaticColor::kBlue ? top->delta_blue : top->delta_red;
  verdict.features = features;
  return verdict;
}

const char* ChromaticFeatureKindName(ChromaticFeatureKind kind) {
  return kind == ChromaticFeatureKind::kEdge ? "edge" : "gate_edge";
}

const char* ChromaticColorName(ChromaticColor color) {
  switch (color) {
    case ChromaticColor::kBlue:
      return "blue";
    case ChromaticColor::kRed:
      return "red";
    case ChromaticColor::kWhite:
      return "white";
    case ChromaticColor::kNone:
      return "none";
  }
  return "none";
}

const char* ChromaticVerdictKindName(ChromaticVerdictKind kind) {
  switch (kind) {
    case ChromaticVerdictKind::kEdge:
      return "edge";
    case ChromaticVerdictKind::kGateEdge:
      return "gate_edge";
    case ChromaticVerdictKind::kTint:
      return "tint";
    case ChromaticVerdictKind::kUnresolved:
      return "unresolved";
    case ChromaticVerdictKind::kNone:
      return "none";
  }
  return "none";
}

ChromaticThresholds ChromaticThresholdsSnapshot() {
  return ChromaticThresholds{};
}

ChromaticVerdict Diagnose(const FaceNormalTable& normals, const FacePolygonTable& polygons, const int* slots,
                          int slot_count, double n_red, double n_blue, const ChromaticOptions& options) {
  const DeviationField red(normals, polygons, slots, slot_count, n_red);
  const DeviationField blue(normals, polygons, slots, slot_count, n_blue);
  const KinkOptions kink_options{ options.lattice_n, options.step, options.max_walk_steps };

  std::vector<std::string> notes;
  std::vector<std::string> unresolved;
  std::vector<ChromaticFeature> features;
  bool complete = true;

  // One feature per weight kink at both indices; a kink at one index only is the strongest colour
  // shape and is named unresolved, never dropped (LI diagnose's pairing loop).
  const std::vector<KinkCurve> kinks_red = WeightKinks(red, kink_options);
  const std::vector<KinkCurve> kinks_blue = WeightKinks(blue, kink_options);
  for (size_t k = 0; k < kinks_red.size() && k < kinks_blue.size(); k++) {
    const KinkCurve& kink_red = kinks_red[k];
    const KinkCurve& kink_blue = kinks_blue[k];
    for (const KinkCurve* kink : { &kink_red, &kink_blue }) {
      if (!kink->note.empty()) {
        notes.push_back(MarginName(slot_count, kink->margin) + " at n = " + Number(kink->index) + ": " + kink->note);
      }
      complete = complete && kink->Complete();
    }
    const bool arcs_red = !kink_red.arcs.empty();
    const bool arcs_blue = !kink_blue.arcs.empty();
    if (arcs_red && arcs_blue) {
      features.push_back(KinkFeature(red, blue, kink_red, kink_blue, slot_count));
    } else if (arcs_red || arcs_blue) {
      const KinkCurve& present = arcs_red ? kink_red : kink_blue;
      unresolved.push_back(MarginName(slot_count, present.margin) + ": weight kink at n = " + Number(present.index) +
                           " only (not assessed)");
    }
  }

  // The gates: both boundary walks; a walk that refuses reports the gates as not analysed and
  // lowers coverage, the kink features above stay (LI catches the boundary error here).
  BoundaryWalkOptions walk_options;
  walk_options.lattice_n = options.lattice_n;
  walk_options.step = options.step;
  walk_options.max_walk_steps = options.max_walk_steps;
  BoundaryWalkRecord red_record;
  BoundaryWalkRecord blue_record;
  const WalkResult red_walk = WalkBoundary(red, walk_options, &red_record);
  const WalkResult blue_walk = WalkBoundary(blue, walk_options, &blue_record);
  if (red_walk.status != WalkStatus::kOk || blue_walk.status != WalkStatus::kOk) {
    const WalkResult& refused = red_walk.status != WalkStatus::kOk ? red_walk : blue_walk;
    notes.push_back("gates not analysed: " + std::string(WalkStatusName(refused.status)) + ": " + refused.message);
    complete = false;
  } else {
    const std::vector<ChromaticFeature> gates =
        GateFeatures(red, blue, red_record, blue_record, slot_count, &unresolved);
    features.insert(features.end(), gates.begin(), gates.end());
  }

  notes.insert(notes.end(), unresolved.begin(), unresolved.end());
  std::vector<int> faces(slots, slots + slot_count);
  return VerdictOf(std::move(faces), features, std::move(notes), n_red, n_blue, !unresolved.empty(), complete);
}

// ---- the class verdict ------------------------------------------------------------------------------------

namespace {

// The plate family's poses R = tilt . Rz(spin), body to world (LI sample_plate_poses's draw order:
// uniform spin, half-normal |N(0, sigma)| tilt, uniform tilt direction; the stream is this
// implementation's own — the header's seed note).
std::vector<double> SamplePlatePoses(const PlateFamilySpec& family) {
  std::mt19937_64 rng(static_cast<unsigned long long>(family.seed));
  std::uniform_real_distribution<double> azimuth(0.0, 2.0 * kPi);
  std::normal_distribution<double> normal(0.0, family.zenith_std_deg * kPi / 180.0);
  std::vector<double> poses(static_cast<size_t>(family.samples) * 9);
  for (int i = 0; i < family.samples; i++) {
    const double spin = azimuth(rng);
    const double tilt = std::fabs(normal(rng));
    const double toward = azimuth(rng);
    const double tilt_vector[3] = { -std::sin(toward) * tilt, std::cos(toward) * tilt, 0.0 };
    double tilt_rotation[9];
    double spin_rotation[9];
    so3::Exp(tilt_vector, tilt_rotation);
    const double spin_vector[3] = { 0.0, 0.0, spin };
    so3::Exp(spin_vector, spin_rotation);
    double rotation[9];
    for (int r = 0; r < 3; r++) {
      for (int c = 0; c < 3; c++) {
        rotation[r * 3 + c] = tilt_rotation[r * 3 + 0] * spin_rotation[0 * 3 + c] +
                              tilt_rotation[r * 3 + 1] * spin_rotation[1 * 3 + c] +
                              tilt_rotation[r * 3 + 2] * spin_rotation[2 * 3 + c];
      }
    }
    for (int e = 0; e < 9; e++) {
      poses[9 * static_cast<size_t>(i) + e] = rotation[e];
    }
  }
  return poses;
}

// s_hat toward the sun at (altitude, azimuth 180) — LI camera.sun_direction.
void SunDirection(double altitude_deg, double sun[3]) {
  const double altitude = altitude_deg * kPi / 180.0;
  const double azimuth = 180.0 * kPi / 180.0;
  sun[0] = std::cos(altitude) * std::cos(azimuth);
  sun[1] = std::cos(altitude) * std::sin(azimuth);
  sun[2] = std::sin(altitude);
}

// The chain's body-frame outgoing direction at the identity pose for body sun u and index n
// (TracePathChain in kEvaluateAll; the world-frame outgoing of a pose realizing u is R times it,
// so the red-blue angle cancels R and reads here).
bool OutgoingAt(const FaceNormalTable& table, const int* slots, int slot_count, double index, const double u[3],
                double out[3]) {
  const double identity[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
  const double incident[3] = { -u[0], -u[1], -u[2] };
  return TracePathChain<double, ChainEvaluation::kEvaluateAll>(table, slots, slot_count, index, incident, identity, out,
                                                               nullptr, nullptr);
}

}  // namespace

ClassVerdict DiagnoseClass(const FaceNormalTable& normals, const FacePolygonTable& polygons, const int* representative,
                           int representative_count, const PlateFamilySpec& family, double n_red, double n_blue) {
  ClassVerdict out;
  out.representative.assign(representative, representative + representative_count);
  if (!PbdOrbit(representative, representative_count, &out.members)) {
    // Face numbers outside the analytic layer's universe: no class, no verdict — fail closed with
    // the empty members list rather than a partial orbit.
    out.verdict.kind = ChromaticVerdictKind::kNone;
    out.verdict.notes.push_back("representative faces are outside the label universe");
    out.verdict.coverage_complete = false;
    return out;
  }

  // The family's u values: u_i = R_i^T s_hat (the u-S^2 reduction carries A and T).
  const std::vector<double> poses = SamplePlatePoses(family);
  double sun[3];
  SunDirection(family.sun_altitude_deg, sun);
  const int samples = family.samples;
  std::vector<double> u(static_cast<size_t>(samples) * 3);
  for (int i = 0; i < samples; i++) {
    const double* r = &poses[9 * static_cast<size_t>(i)];
    for (int c = 0; c < 3; c++) {
      u[3 * static_cast<size_t>(i) + c] = r[0 * 3 + c] * sun[0] + r[1 * 3 + c] * sun[1] + r[2 * 3 + c] * sun[2];
    }
  }

  struct MemberWeights {
    std::vector<double> red;
    std::vector<double> blue;
    int slots[kMaxFaceCount];
    int count = 0;
  };
  std::map<std::vector<int>, MemberWeights> weights;  // keyed by face numbers, sorted by the orbit
  for (const std::vector<int>& member : out.members) {
    MemberWeights entry;
    if (ResolveFaceSequence(normals, member.data(), static_cast<int>(member.size()), entry.slots) != Status::kOk) {
      continue;  // the literal member is not a face set of this crystal: it cannot be lit
    }
    entry.count = static_cast<int>(member.size());
    const DeviationField red(normals, polygons, entry.slots, entry.count, n_red);
    const DeviationField blue(normals, polygons, entry.slots, entry.count, n_blue);
    entry.red = BodyWeight(red, u);
    entry.blue = BodyWeight(blue, u);
    if (std::any_of(entry.red.begin(), entry.red.end(), [](double v) { return v > 0.0; }) ||
        std::any_of(entry.blue.begin(), entry.blue.end(), [](double v) { return v > 0.0; })) {
      weights.emplace(member, std::move(entry));
    }
  }

  double energy_red = 0.0;
  double energy_blue = 0.0;
  double tir_red = 0.0;
  double tir_red_total = 0.0;
  double tir_blue = 0.0;
  double tir_blue_total = 0.0;
  double spread_sum = 0.0;
  double spread_weight = 0.0;
  for (const auto& [member, entry] : weights) {
    const bool lit_red = std::any_of(entry.red.begin(), entry.red.end(), [](double v) { return v > 0.0; });
    const bool lit_blue = std::any_of(entry.blue.begin(), entry.blue.end(), [](double v) { return v > 0.0; });
    if (lit_red) {
      out.lit_members_red.push_back(member);
      energy_red += std::accumulate(entry.red.begin(), entry.red.end(), 0.0) / static_cast<double>(samples);
    }
    if (lit_blue) {
      out.lit_members_blue.push_back(member);
      energy_blue += std::accumulate(entry.blue.begin(), entry.blue.end(), 0.0) / static_cast<double>(samples);
    }
    // The TIR fractions: A T-weighted share of internal reflections that are total, per index,
    // over the lit members' full domain margin vectors (the member's field answers them; each
    // index weights by its own w over its own lit poses).
    for (int label = 0; label < 2; label++) {
      const bool lit = label == 0 ? lit_red : lit_blue;
      if (!lit) {
        continue;
      }
      const double index = label == 0 ? n_red : n_blue;
      const std::vector<double>& w = label == 0 ? entry.red : entry.blue;
      const DeviationField field(normals, polygons, entry.slots, entry.count, index);
      const int margin_count = 2 * entry.count;
      double margins[2 * kMaxFaceCount];
      for (int i = 0; i < samples; i++) {
        if (!(w[static_cast<size_t>(i)] > 0.0)) {
          continue;
        }
        field.DomainMarginsAt(&u[3 * static_cast<size_t>(i)], margins);
        const double weight = w[static_cast<size_t>(i)];
        for (int m = 3; m < margin_count - 2; m += 2) {  // internal TIR discriminants: odd positions
          const double total = weight * (margins[m] > 0.0 ? 1.0 : 0.0);
          if (label == 0) {
            tir_red_total += weight;
            tir_red += total;
          } else {
            tir_blue_total += weight;
            tir_blue += total;
          }
        }
      }
    }
    // The direction dispersion: poses lit at both indices, weighted by the red weight.
    if (lit_red && lit_blue) {
      double outgoing_red[3];
      double outgoing_blue[3];
      for (int i = 0; i < samples; i++) {
        if (!(entry.red[static_cast<size_t>(i)] > 0.0 && entry.blue[static_cast<size_t>(i)] > 0.0)) {
          continue;
        }
        const double* ui = &u[3 * static_cast<size_t>(i)];
        if (!OutgoingAt(normals, entry.slots, entry.count, n_red, ui, outgoing_red) ||
            !OutgoingAt(normals, entry.slots, entry.count, n_blue, ui, outgoing_blue)) {
          continue;
        }
        const double weight = entry.red[static_cast<size_t>(i)];
        spread_sum += weight * AngleBetween(outgoing_red, outgoing_blue);
        spread_weight += weight;
      }
    }
  }

  out.verdict.faces = out.representative;
  out.verdict.n_red = n_red;
  out.verdict.n_blue = n_blue;
  out.verdict.has_tint = true;
  out.verdict.tint.energy_red = energy_red;
  out.verdict.tint.energy_blue = energy_blue;
  out.verdict.tint.ratio = energy_red > 0.0 ? energy_blue / energy_red : kNaN;
  out.verdict.tint.tir_fraction_red = tir_red_total > 0.0 ? tir_red / tir_red_total : kNaN;
  out.verdict.tint.tir_fraction_blue = tir_blue_total > 0.0 ? tir_blue / tir_blue_total : kNaN;
  out.verdict.tint.direction_dispersion = spread_weight > 0.0 ? spread_sum / spread_weight : 0.0;

  // The tint verdict (LI _tint_verdict): not lit at all, lit at one index only (the extreme tint,
  // never "no colour"), dispersing (no single tint), or the ratio's colour.
  ChromaticVerdict& verdict = out.verdict;
  if (energy_red <= 0.0 && energy_blue <= 0.0) {
    verdict.notes.push_back("class not lit at either index");
    return out;
  }
  if (energy_red <= 0.0 || energy_blue <= 0.0) {
    verdict.kind = ChromaticVerdictKind::kTint;
    verdict.color = energy_red <= 0.0 ? ChromaticColor::kBlue : ChromaticColor::kRed;
    verdict.visible = true;
    verdict.notes.push_back("class lit at n = " + Number(energy_red <= 0.0 ? n_blue : n_red) + " only");
    return out;
  }
  if (verdict.tint.direction_dispersion >= kEdgeMinShiftRad) {
    std::ostringstream note;
    note << std::fixed << std::setprecision(2) << "red and blue land "
         << verdict.tint.direction_dispersion * 180.0 / kPi
         << " deg apart: the ordinary dispersion of the direction map spreads the colours, the spot has "
            "no single tint (outside this criterion)";
    verdict.notes.push_back(note.str());
    return out;
  }
  if (verdict.tint.ratio >= kTintRatioMin) {
    verdict.kind = ChromaticVerdictKind::kTint;
    verdict.color = ChromaticColor::kBlue;
    verdict.visible = true;
    return out;
  }
  if (verdict.tint.ratio <= 1.0 / kTintRatioMin) {
    verdict.kind = ChromaticVerdictKind::kTint;
    verdict.color = ChromaticColor::kRed;
    verdict.visible = true;
    return out;
  }
  verdict.color = ChromaticColor::kWhite;
  return out;
}

}  // namespace lumice::analytic
