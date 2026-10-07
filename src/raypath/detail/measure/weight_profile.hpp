#ifndef LUMICE_RAYPATH_DETAIL_WEIGHT_PROFILE_HPP_
#define LUMICE_RAYPATH_DETAIL_WEIGHT_PROFILE_HPP_

// SPECIFICATION — the M2 measure-side primitive: the weight profile of a kind-1 critical-set
// curve, w(s) = rho_u(u(s)) along the curve, and its declared quadrature. The curve comes from
// the geometry layer (scrum 660); this module owns only the density reading and the rule.
//
// DECLARED QUADRATURE RULE (frozen): the primitive evaluates rho_u(u_i) at every curve point and
// reports
//   total = trapezoid_i ( rho_u(u_i) ) over the curve parameter spacing,
// i.e. the trapezoid line integral of rho_u over the curve's OWN polyline (chord lengths between
// consecutive u points, closed curves wrap — a line integral of rho_u w.r.t. chord arclength). This is a PROFILE, not a
// brightness: it integrates the density along the curve (a line integral of rho_u w.r.t. arclength), the quantity M2 of
// the schema3 vocabulary — the brightness of a feature involves A·T on top and belongs to the fiber quadrature, not
// here. The A·T-enriched variant (optional enrichment when fiber data is present at the curve's points) is deliberately
// NOT in v1: no producer exists yet, and the enrichment interface is pre-registered as an integration-time addition
// (adding a function is not a semantic change).
//
// NUMERIC ANCHOR (pre-declared suspension, see fiber_quadrature.hpp's spec block): no LI fixture
// carries kind-1 curve data yet; v1 pins the primitive on mock curves with analytic densities
// (a latitude circle under a zonal measure, a meridian arc under a band measure) and the real
// anchor waits for the 660 port's critical sets.

#include <vector>

#include "raypath/detail/measure/declared_density.hpp"
#include "raypath/detail/measure/measure_geometry_contract.hpp"

namespace lumice::raypath {

struct WeightProfileSample {
  double s = 0.0;  // cumulative arclength at the point (chord sum; closed curves wrap)
  double u[3] = { 0.0, 0.0, 0.0 };
  double rho_u = 0.0;  // w.r.t. dOmega (the curve is a line; the value is the area density
                       // read at the curve point — positive iff the curve enters the support)
};

struct WeightProfile {
  std::vector<WeightProfileSample> points;
  double total = 0.0;  // the declared trapezoid line integral of rho_u over the polyline
  int in_support = 0;  // points with rho_u > 0
  // The curve's existence state passthrough: an escaped/truncated curve yields the points the
  // walk produced and this flag mirrors CriticalSetCurve::existence for the report's bucket.
  ExistenceState existence = ExistenceState::kComputed;
};

// Reads rho_u along a kind-1 curve. An empty or escaped curve returns its existence with no
// points and total 0 (a computed empty set is data — the report's empty-state vocabulary owns the
// wording, not this primitive).
WeightProfile SampleWeightProfile(const UMarginal& measure, const CriticalSetCurve& curve);

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_DETAIL_WEIGHT_PROFILE_HPP_
