#include "analytic/dp_contour.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace lumice::analytic {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

// The orbit-collapse threshold, algebraically equal to the measure layer's declared form
// (declared_density.hpp's kDegenerateSunGeometry: |s_z| > 1 - 1e-12): that inequality IS
// 1 - s_z^2 < 2e-12 (up to the 1e-24 tail), NOT < 1e-12 — the constant below is the squared
// form's exact value so both layers call the same geometry collapsed. One side moving must
// update the other; the paths in this comment are the retrieval anchor.
constexpr double kSunPoleOneMinus = 2e-12;

void Copy3(const double from[3], double to[3]) {
  to[0] = from[0];
  to[1] = from[1];
  to[2] = from[2];
}

// The minimal rotation taking `axis` to +e3 (Rodrigues about normalize(axis x e3)); the identity
// when the axis already IS +e3 (the plate family — Rz(theta) times the identity is then
// bit-exactly the handwritten grid this producer productizes), and a pi rotation about x at the
// antipode. Row-major, the body -> world convention of every pose here.
void BasePoseAbout(const double axis[3], double r0[9]) {
  if (axis[2] >= 1.0 - 1e-15) {
    const double identity[9] = { 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };
    std::copy(identity, identity + 9, r0);
    return;
  }
  if (axis[2] <= -1.0 + 1e-15) {
    const double flip[9] = { 1.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, 0.0, -1.0 };
    std::copy(flip, flip + 9, r0);
    return;
  }
  const double c = axis[2];                 // cos of the axis/e3 angle
  const double s = std::sqrt(1.0 - c * c);  // its sine, positive on (0, pi)
  const double nx = axis[1] / s;            // n = normalize(axis x e3) = (axis_y, -axis_x, 0) / s
  const double ny = -axis[0] / s;
  const double omc = 1.0 - c;
  r0[0] = c + omc * nx * nx;
  r0[1] = omc * nx * ny;
  r0[2] = s * ny;
  r0[3] = omc * nx * ny;
  r0[4] = c + omc * ny * ny;
  r0[5] = -s * nx;
  r0[6] = -s * ny;
  r0[7] = s * nx;
  r0[8] = c;
}

// pose = Rz(theta) * r0, row-major. The identity-r0 product is bit-exact (each entry is one
// nonzero product plus signed zeros), so the plate family's poses are the handwritten grid's.
void SpinPose(double theta, const double r0[9], double pose[9]) {
  const double c = std::cos(theta);
  const double s = std::sin(theta);
  const double rz[9] = { c, -s, 0.0, s, c, 0.0, 0.0, 0.0, 1.0 };
  for (int r = 0; r < 3; r++) {
    for (int j = 0; j < 3; j++) {
      pose[3 * r + j] = rz[3 * r] * r0[j] + rz[3 * r + 1] * r0[3 + j] + rz[3 * r + 2] * r0[6 + j];
    }
  }
}

// Cumulative chord arclength of a packed 3N polyline (s_0 = 0; no wrap — the WeightProfileSample
// convention: the wrap chord, when a closed rule reads it, belongs to that rule, not to `param`).
void ChordParameters(const std::vector<double>& u, std::vector<double>* param) {
  const size_t count = u.size() / 3;
  param->assign(count, 0.0);
  for (size_t i = 1; i < count; i++) {
    const double dx = u[3 * i] - u[3 * (i - 1)];
    const double dy = u[3 * i + 1] - u[3 * (i - 1) + 1];
    const double dz = u[3 * i + 2] - u[3 * (i - 1) + 2];
    (*param)[i] = (*param)[i - 1] + std::sqrt(dx * dx + dy * dy + dz * dz);
  }
}

// The open trapezoid weights over the whole span, ACCUMULATED (a node shared by two segments
// carries both half-weights — the composite trapezoid; kink cuts between segments are no-ops
// for the weights, which is why the open kink rule is the plain trapezoid).
void OpenTrapezoidWeights(const std::vector<double>& params, std::vector<double>* weights) {
  const size_t count = params.size();
  for (size_t i = 0; i < count; i++) {
    const double lo = (i == 0) ? params[i] : params[i - 1];
    const double hi = (i + 1 < count) ? params[i + 1] : params[i];
    (*weights)[i] += 0.5 * (hi - lo);
  }
}

// The forward parameter step from index i to index next on the 2 pi circle (params ascending
// over one period, any start): the seam step closes the period. The closed path TRUSTS the
// caller's coverage claim (dp_contour.hpp: "the params cover one full 2 pi-period circle"): a
// span falling short of 2 pi passes silently and the weight sum is short by the same amount —
// nothing validates the precondition.
double CircleStep(const std::vector<double>& params, size_t i, size_t next) {
  if (next != 0) {
    return params[next] - params[i];
  }
  return params[0] + kTwoPi - params[params.size() - 1];
}

bool SunAtPole(const double sun_hat[3]) {
  return sun_hat[0] * sun_hat[0] + sun_hat[1] * sun_hat[1] < kSunPoleOneMinus;
}

}  // namespace

// ---- declared quadrature rules ---------------------------------------------------------------------------

const char* CurveRuleName(CurveRule rule) {
  switch (rule) {
    case CurveRule::kPeriodicMidpoint:
      return "periodic-midpoint";
    case CurveRule::kChordTrapezoid:
      return "chord-trapezoid";
    case CurveRule::kKinkSegmented:
      return "kink-segmented";
  }
  return "unknown";
}

int DeclaredCurveRuleOrder(CurveRule rule) {
  // 0 = spectral (the periodic rule on a smooth periodic integrand); the others are for a smooth
  // integrand on each smooth piece.
  switch (rule) {
    case CurveRule::kPeriodicMidpoint:
      return 0;
    case CurveRule::kChordTrapezoid:
    case CurveRule::kKinkSegmented:
      return 2;
  }
  return 2;
}

std::vector<double> SampleCurveWeights(CurveRule rule, const std::vector<double>& params, bool closed,
                                       const std::vector<char>& kink) {
  const size_t count = params.size();
  std::vector<double> weights;
  if (count == 0) {
    return weights;
  }
  if (rule == CurveRule::kPeriodicMidpoint && closed) {
    // The trapezoid over the parameter circle: each in-circle step contributes half to each of
    // its endpoints. On a uniform grid every weight is 2 pi / N — the orbit producer's grid.
    weights.assign(count, 0.0);
    for (size_t i = 0; i < count; i++) {
      const double step = CircleStep(params, i, (i + 1) % count);
      weights[i] += 0.5 * step;
      weights[(i + 1) % count] += 0.5 * step;
    }
    return weights;
  }
  if (rule == CurveRule::kChordTrapezoid && !closed) {
    weights.assign(count, 0.0);
    OpenTrapezoidWeights(params, &weights);
    return weights;
  }
  if (rule == CurveRule::kKinkSegmented) {
    const bool has_kink = kink.size() >= count && std::any_of(kink.begin(), kink.begin() + static_cast<long>(count),
                                                              [](char v) { return v != 0; });
    weights.assign(count, 0.0);
    if (!closed) {
      // The open kink rule IS the open trapezoid (a cut at a node does not move the composite
      // weights); kept as its own rule so the caller's smoothness statement stays explicit.
      OpenTrapezoidWeights(params, &weights);
      return weights;
    }
    if (!has_kink) {
      return weights;  // a closed kink rule with no cut does not apply: the empty answer is
                       // louder than a silent wrap over an uncut kink
    }
    // Cut at every marked point; the arcs between consecutive marks (walking forward, wrapping
    // once from the last mark back to the first) each get the open trapezoid over their own
    // span, measured on the circle.
    std::vector<size_t> cuts;
    for (size_t i = 0; i < count; i++) {
      if (kink[i] != 0) {
        cuts.push_back(i);
      }
    }
    for (size_t c = 0; c < cuts.size(); c++) {
      const size_t from = cuts[c];
      const size_t to = (c + 1 < cuts.size()) ? cuts[c + 1] : cuts[0];
      size_t i = from;
      while (true) {
        const size_t next = (i + 1) % count;
        const double step = CircleStep(params, i, next);
        weights[i] += 0.5 * step;
        weights[next] += 0.5 * step;
        if (next == to) {
          break;
        }
        i = next;
      }
    }
    return weights;
  }
  // A rule/shape pair the function does not declare (the periodic rule on an open curve, the
  // plain trapezoid on a closed one): empty weights — the caller's rule statement was wrong,
  // and an empty answer is louder than a guessed one.
  return weights;
}

double ChordLineIntegral(const std::vector<double>& u, const std::vector<double>& f, bool closed) {
  const size_t count = f.size();
  if (count == 0 || u.size() != 3 * count) {
    return 0.0;
  }
  double total = 0.0;
  for (size_t i = 0; i + 1 < count; i++) {
    const double dx = u[3 * i + 3] - u[3 * i];
    const double dy = u[3 * i + 4] - u[3 * i + 1];
    const double dz = u[3 * i + 5] - u[3 * i + 2];
    total += 0.5 * (f[i] + f[i + 1]) * std::sqrt(dx * dx + dy * dy + dz * dz);
  }
  if (closed) {
    const double dx = u[0] - u[3 * (count - 1)];
    const double dy = u[1] - u[3 * (count - 1) + 1];
    const double dz = u[2] - u[3 * (count - 1) + 2];
    total += 0.5 * (f[count - 1] + f[0]) * std::sqrt(dx * dx + dy * dy + dz * dz);
  }
  return total;
}

double RefinementErrorEstimate(double integral_n, double integral_2n) {
  return std::fabs(integral_n - integral_2n);
}

// ---- the orbit fiber-stream producer ---------------------------------------------------------------------

OrbitFiberStream MakeOrbitFiberStream(const FaceNormalTable& normals, const FacePolygonTable& polygons,
                                      const int* slots, int slot_count, const PoseDensitySpec& spec,
                                      const double sun_hat[3], double refractive_index, int grid) {
  OrbitFiberStream out;
  out.grid = grid;
  double axis[3];
  if (!FamilyAxis(spec, axis)) {
    out.note = "no family axis: the density's sigma->0 support is not one circle (random or off-pole column)";
    return out;
  }
  out.spin_degenerate = SunAtPole(sun_hat);
  if (out.spin_degenerate) {
    out.note = "sun at the family-axis pole: the orbit circle collapses to a point, every sample is jet-degenerate";
  }
  double r0[9];
  BasePoseAbout(axis, r0);
  Corridor corridor(normals, polygons, slots, slot_count);
  const double incident[3] = { -sun_hat[0], -sun_hat[1], -sun_hat[2] };
  std::vector<double> segments(static_cast<size_t>(slot_count + 1) * 3);
  std::vector<double> transmittances(static_cast<size_t>(slot_count));
  out.samples.resize(static_cast<size_t>(grid));
  for (int i = 0; i < grid; i++) {
    const double theta = kTwoPi * (i + 0.5) / grid;
    double pose[9];
    SpinPose(theta, r0, pose);
    PathOutputs outputs;
    outputs.segment_directions = segments.data();
    outputs.interface_transmittances = transmittances.data();
    const bool valid = EvaluatePath(normals, slots, slot_count, refractive_index, incident, pose, &outputs);
    OrbitFiberPoint& point = out.samples[static_cast<size_t>(i)];
    // u = R^T s_hat (the pose's rows dotted with the sun); the corridor wants the body-frame
    // PROPAGATION direction, which is -u.
    for (int r = 0; r < 3; r++) {
      point.u[r] = pose[r] * sun_hat[0] + pose[3 + r] * sun_hat[1] + pose[6 + r] * sun_hat[2];
    }
    const double s_body[3] = { -point.u[0], -point.u[1], -point.u[2] };
    const EntryMeasure entry = corridor.Evaluate(s_body, refractive_index);
    point.area = entry.status == EntryMeasureStatus::kOk ? entry.value : 0.0;
    point.transmission = valid ? outputs.fresnel_transmission : 0.0;
    point.valid = valid && point.area > 0.0 && point.transmission > 0.0;
    point.jet_degenerate = out.spin_degenerate;
    point.parameter = theta;
    point.weight = kTwoPi / static_cast<double>(grid);
    Copy3(outputs.outgoing_direction, point.outgoing);
  }
  return out;
}

// ---- the restricted family curve -------------------------------------------------------------------------

RestrictedFamilyCurve MakeRestrictedFamilyCurve(const FaceNormalTable& normals, const FacePolygonTable& polygons,
                                                const int* slots, int slot_count, const PoseDensitySpec& spec,
                                                const double sun_hat[3], double base_index,
                                                const double* wavelengths_nm, const double* indices,
                                                int wavelength_count, int grid) {
  RestrictedFamilyCurve out;
  double axis[3];
  if (!FamilyAxis(spec, axis)) {
    out.note = "no family axis: the density's sigma->0 support is not one circle (random or off-pole column)";
    return out;
  }
  if (SunAtPole(sun_hat)) {
    out.note = "sun at the family-axis pole: the restricted circle collapses to a point, not a curve";
    return out;
  }
  // The circle of u about `axis` through u_0 = R_0^T s_hat: cos_p = u_0 . axis is the sun's
  // zenith-angle cosine (independent of R_0 — R_0 axis = e3 and axis . R_0^T s_hat = R_0 axis .
  // s_hat), e1 the in-plane unit vector of u_0's perpendicular component, e2 = axis x e1. The
  // grid is the orbit producer's periodic midpoint grid, so the two objects walk the same circle
  // at the same thetas (their u values agree as constructions of the same geometry, not
  // bit-for-bit — one goes through the pose, the other through the closed form).
  double r0[9];
  BasePoseAbout(axis, r0);
  double u0[3];
  for (int j = 0; j < 3; j++) {
    u0[j] = sun_hat[0] * r0[j] + sun_hat[1] * r0[3 + j] + sun_hat[2] * r0[6 + j];
  }
  double cos_p = 0.0;
  for (int j = 0; j < 3; j++) {
    cos_p += u0[j] * axis[j];
  }
  cos_p = std::max(-1.0, std::min(1.0, cos_p));
  const double sin_p = std::sqrt(1.0 - cos_p * cos_p);
  double e1[3];
  for (int j = 0; j < 3; j++) {
    e1[j] = (u0[j] - cos_p * axis[j]) / sin_p;
  }
  const double e2[3] = { axis[1] * e1[2] - axis[2] * e1[1], axis[2] * e1[0] - axis[0] * e1[2],
                         axis[0] * e1[1] - axis[1] * e1[0] };
  out.u.assign(static_cast<size_t>(3 * grid), 0.0);
  out.tangent.assign(static_cast<size_t>(3 * grid), 0.0);
  out.d_p.assign(static_cast<size_t>(grid), 0.0);
  out.support_param.assign(static_cast<size_t>(grid), 0.0);
  for (int i = 0; i < grid; i++) {
    const double theta = kTwoPi * (i + 0.5) / grid;
    const double ct = std::cos(theta);
    const double st = std::sin(theta);
    for (int j = 0; j < 3; j++) {
      out.u[3 * static_cast<size_t>(i) + j] = cos_p * axis[j] + sin_p * (ct * e1[j] + st * e2[j]);
      out.tangent[3 * static_cast<size_t>(i) + j] = -st * e1[j] + ct * e2[j];  // unit: d u / d theta
    }
    out.support_param[static_cast<size_t>(i)] = theta;
  }
  // The routed D_P columns: the base index and every wavelength's, each from its own field
  // (n-continuation; the index table is the caller's, 660.4's convention).
  const int count = std::max(0, wavelength_count);
  out.wavelengths_nm.assign(wavelengths_nm, wavelengths_nm + count);
  out.indices.assign(indices, indices + count);
  out.critical_d_p.assign(static_cast<size_t>(grid) * static_cast<size_t>(count), 0.0);
  for (int k = -1; k < count; k++) {
    const double index = k < 0 ? base_index : indices[k];
    DeviationField field(normals, polygons, slots, slot_count, index);
    const bool slab = field.fold().degenerate;
    for (int i = 0; i < grid; i++) {
      const FieldSample sample = field.SampleOptical(&out.u[3 * static_cast<size_t>(i)]);
      double d = 0.0;
      if (RoutedDeviation(sample, slab, &d) != RoutedDeviationStatus::kOk) {
        d = std::numeric_limits<double>::quiet_NaN();
        out.routed_nonfinite++;
      }
      if (k < 0) {
        out.d_p[static_cast<size_t>(i)] = d;
      } else {
        out.critical_d_p[static_cast<size_t>(i) * static_cast<size_t>(count) + static_cast<size_t>(k)] = d;
      }
    }
  }
  return out;
}

// ---- kind-2 / kind-3 chain mapping -----------------------------------------------------------------------

ChainCurve ChainFromBoundaryPieces(const BoundaryWalkRecord& record, WalkStatus status, const std::string& message) {
  ChainCurve out;
  out.is_gate_boundary = true;
  if (status != WalkStatus::kOk) {
    out.existence = CurveExistence::kWalkTruncated;
    out.note = std::string(WalkStatusName(status)) + (message.empty() ? "" : ": " + message);
    return out;  // trusts the status; a non-empty record here violates WalkBoundary's invariant
  }
  out.existence = CurveExistence::kComputed;
  out.closed = true;
  // Assembly: piece p's first point IS piece p-1's last (the corner they share; the last piece's
  // end is glued to the first corner, dp_boundary), so each piece after the first drops its
  // first point — every source point lands in the chain exactly once, with the glued seam point
  // duplicated at the very end (bit-equal to the first; dropped below).
  std::vector<size_t> run_ends;
  for (size_t p = 0; p < record.pieces.size(); p++) {
    const BoundaryPiece& piece = record.pieces[p];
    const size_t point_count = piece.points.size() / 3;
    for (size_t i = (p == 0 ? 0 : 1); i < point_count; i++) {
      out.u.insert(out.u.end(), piece.points.begin() + 3 * static_cast<long>(i),
                   piece.points.begin() + 3 * static_cast<long>(i + 1));
      out.values.push_back(piece.values[i]);
      out.kink.push_back(0);
      out.event.push_back(-1);
    }
    run_ends.push_back(out.values.size() - 1);
  }
  // The glued seam: bit-equal duplicate of the first point (a corner loop's last piece was glued
  // to the first corner exactly). Drop it so the polyline visits every point once; the closed
  // rule's wrap chord closes the loop.
  if (out.u.size() >= 6 && out.u[0] == out.u[out.u.size() - 3] && out.u[1] == out.u[out.u.size() - 2] &&
      out.u[2] == out.u[out.u.size() - 1]) {
    out.u.resize(out.u.size() - 3);
    out.values.pop_back();
    out.kink.pop_back();
    out.event.pop_back();
  }
  // Mark every corner: corner p sits at the END of piece p's run (the cyclic layout), and the
  // last piece's end — now dropped as the seam duplicate — is the polyline's FIRST point
  // (corner C-1). A smooth loop (no corners) gets no marks.
  for (size_t p = 0; p < run_ends.size() && !record.corners.empty(); p++) {
    if (p + 1 == run_ends.size()) {
      out.kink.front() = 1;  // the seam corner lives at index 0 after the drop
      break;
    }
    if (run_ends[p] < out.kink.size()) {
      out.kink[run_ends[p]] = 1;
    }
  }
  ChordParameters(out.u, &out.param);
  return out;
}

ChainCurve ChainFromKinkArcs(const KinkCurve& curve) {
  ChainCurve out;
  out.is_gate_boundary = false;
  out.existence = curve.status == WalkStatus::kOk ? CurveExistence::kComputed : CurveExistence::kWalkTruncated;
  out.note = curve.note + (curve.failed_seeds > 0 ? " (failed seeds: " + std::to_string(curve.failed_seeds) + ")" : "");
  out.closed = !curve.arcs.empty();
  // The arcs are separate components (each its own zero-set walk); they are concatenated in
  // order with every arc's first point marked — the TIR onset — so a consumer can re-split.
  for (const KinkArc& arc : curve.arcs) {
    out.closed = out.closed && arc.closed;
    const size_t point_count = arc.points.size() / 3;
    for (size_t i = 0; i < point_count; i++) {
      out.u.insert(out.u.end(), arc.points.begin() + 3 * static_cast<long>(i),
                   arc.points.begin() + 3 * static_cast<long>(i + 1));
      out.values.push_back(arc.values[i]);
      out.kink.push_back(i == 0 ? 1 : 0);
      out.event.push_back(-1);
    }
  }
  ChordParameters(out.u, &out.param);
  return out;
}

}  // namespace lumice::analytic
