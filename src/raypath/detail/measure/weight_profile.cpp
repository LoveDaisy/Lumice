#include "raypath/detail/measure/weight_profile.hpp"

#include <cmath>

namespace lumice::raypath {

WeightProfile SampleWeightProfile(const UMarginal& measure, const CriticalSetCurve& curve) {
  WeightProfile profile;
  profile.existence = curve.existence;
  const size_t n = curve.u.size() / 3;
  if (n == 0) {
    return profile;  // a computed empty set is data; the report's empty-state vocabulary owns it
  }
  profile.points.resize(n);
  double s = 0.0;
  double prev[3] = { curve.u[0], curve.u[1], curve.u[2] };
  for (size_t i = 0; i < n; i++) {
    const double* u = &curve.u[3 * i];
    WeightProfileSample& p = profile.points[i];
    p.u[0] = u[0];
    p.u[1] = u[1];
    p.u[2] = u[2];
    p.rho_u = measure.DensitySolidAngle(u);
    if (p.rho_u > 0.0) {
      profile.in_support++;
    }
    if (i > 0) {
      const double dx = u[0] - prev[0], dy = u[1] - prev[1], dz = u[2] - prev[2];
      s += std::sqrt(dx * dx + dy * dy + dz * dz);  // chord arclength of the polyline
      prev[0] = u[0];
      prev[1] = u[1];
      prev[2] = u[2];
    }
    p.s = s;
  }
  // The declared quadrature (header spec): trapezoid of rho_u over the polyline's chord spacing,
  // closed curves wrap to the first point.
  double total = 0.0;
  for (size_t i = 0; i + 1 < n; i++) {
    const double ds = profile.points[i + 1].s - profile.points[i].s;
    total += 0.5 * (profile.points[i].rho_u + profile.points[i + 1].rho_u) * ds;
  }
  if (curve.closed && n > 2) {
    const double* first = curve.u.data();
    const double* last = &curve.u[3 * (n - 1)];
    const double dx = first[0] - last[0], dy = first[1] - last[1], dz = first[2] - last[2];
    const double ds = std::sqrt(dx * dx + dy * dy + dz * dz);
    total += 0.5 * (profile.points[n - 1].rho_u + profile.points[0].rho_u) * ds;
  }
  profile.total = total;
  return profile;
}

}  // namespace lumice::raypath
