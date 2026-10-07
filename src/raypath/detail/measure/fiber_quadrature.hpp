#ifndef LUMICE_RAYPATH_DETAIL_FIBER_QUADRATURE_HPP_
#define LUMICE_RAYPATH_DETAIL_FIBER_QUADRATURE_HPP_

// SPECIFICATION — the M1 fiber quadrature and the tint quotient, with the LI #51 integral
// definition this module's anchor test reproduces (frozen here before implementation; the
// authority is LI docs/raypath-diagnostic-reference.md "Ideal horizontal plates at the two
// 120-degree azimuths" and chromatic-module-c.md's tint kernel — restated so the numbers in the
// test are auditable without the LI repo):
//
// INTENSITY. intensity = sum over kept fiber samples of mu(u_i) * A_i * T_i * w_i, where (mu, w)
// share the stream's MeasureBinding (see measure_geometry_contract.hpp): a kSolidAngle stream
// carries dOmega weights and mu = rho_u w.r.t. dOmega; a kFiberParameter stream carries its own
// parameter element (v1: d_theta of a spin orbit) and mu = the declared density along the orbit
// w.r.t. the same parameter. POSITIVE-PART SEMANTICS are inherited from the analytic SampleEvent
// convention (src/analytic/discovery.hpp: kept iff w = A*T > 0): a sample with A*T <= 0 is not
// merely weighted zero, it is KEPT OUT — the kept count is part of the result, so "every sample
// dark" stays distinguishable from "no samples".
//
// TINT (the LI #51 authority, verbatim semantics). For oriented families whose every pose lands
// on one sky spot, the colour of the spot is the ratio of blue to red weighted power,
//   tint(target) = (sum over members m assigned to target of E_m,blue)
//                / (sum over members m assigned to target of E_m,red),
//   E_m,n = (1/2*pi) * integral_0^{2*pi} A_m,n(theta) * T_m,n(theta) dtheta,
// with per member m of the class's 24-member L1/PBD orbit, per refractive index
//   n_red = 1.307, n_blue = 1.317
// (LI conventions entry 22: the wavelength-pool ends; parameters of every verdict), the poses
// Rz(theta) under dtheta/(2 pi) (the ideal horizontal family), A the entry measure at the same
// n, T the path power with every internal reflectance (entry/exit Fresnel, internal R, 1 under
// TIR — the same product analytic::EvaluatePath reports as fresnel_transmission). Members are
// assigned ONCE to a fixed target (plus / minus / other) by their theta-INDEPENDENT outgoing
// direction (the fold-matrix identity out(Rz(theta)) = M * incident on the physical branch);
// the two targets (relative solar azimuth +-120 degrees at the sun's altitude) are kept SEPARATE
// and the class total is only a partition check. The recorded authority values, "at the recorded
// resolution" (4096/8192 periodic midpoint grids with transition refinement — convergence
// evidence, not exact results): tint ~ 1.010 for the white class (representative 1-3-4-2) and
// ~ 1.525 for the blue class (representative 1-3-5-2) at EACH target.
// The diagnose_class 4096-random-spin ratios (1.528 / 1.632 variants) are a QUALITATIVE
// cross-check under a different member/wavelength aggregation, NOT the anchor — the C12 corpus
// row pins exactly this ambiguity.
//
// M2 WEIGHT-PROFILE ANCHOR — SCOPING DECISION (pre-declared suspension path): the #51 fixture
// carries per-pose snapshots and per-target energy sums, NO kind-1 critical-set curves (those
// are geometry-layer objects, expected with the LI module C port into src/analytic/). The
// weight profile's numeric LI anchor therefore CANNOT be exercised in v1; weight_profile.hpp ships the
// primitive with mock-curve analytic tests only, and the numeric anchor waits for 660's critical
// sets. Recorded in the task progress at spec time so the gap surfaces at Step 2(b), not at
// Step 6 implementation time.

#include "raypath/detail/measure/declared_density.hpp"
#include "raypath/detail/measure/measure_geometry_contract.hpp"

namespace lumice::raypath {

struct FiberQuadratureResult {
  // sum of mu * A * T * w over kept samples (A*T > 0). NaN-free: an empty stream gives 0.
  double intensity = 0.0;
  int kept = 0;        // samples with A * T > 0 (the SampleEvent kept-iff-positive convention)
  int in_support = 0;  // samples whose u the declared measure covers (mu > 0 within tolerance)
  int total = 0;       // all samples
};

// The quadrature of one stream. `angular_tol_rad` is the support-membership tolerance handed to
// UMarginal::MuPositive (orbit-supported measures need it; area supports answer exactly).
FiberQuadratureResult QuadratureIntensity(const UMarginal& measure, const FiberSampleStream& stream,
                                          double angular_tol_rad);

// The tint quotient of two quadratures at the two refractive indices (numerator = blue).
// `defined` is false when the denominator is 0 (nothing lit at red) — the caller reports, not
// guesses, per the "coverage gaps keep their reason" rule.
struct TintQuotient {
  double ratio = 0.0;
  double numerator = 0.0;
  double denominator = 0.0;
  bool defined = false;
};
TintQuotient TintRatio(const FiberQuadratureResult& blue, const FiberQuadratureResult& red);

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_DETAIL_FIBER_QUADRATURE_HPP_
