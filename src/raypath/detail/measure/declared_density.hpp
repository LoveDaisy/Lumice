#ifndef LUMICE_RAYPATH_DETAIL_DECLARED_DENSITY_HPP_
#define LUMICE_RAYPATH_DETAIL_DECLARED_DENSITY_HPP_

// SPECIFICATION (frozen before the implementation; every closed form below cites the sampler
// mechanism it is the density OF — the mechanism is the authority, this table is derived):
//
// The declared measure of an AxisDistribution is the distribution of the poses the production
// sampler actually draws (src/core/random.cpp SampleSphericalPointsSph + ComposeAxisAngles). The
// pose is composed as R = Rz(lambda_out - pi) . Ry(phi_out - pi/2) . Rz(roll_out) with the folded
// latitude coupling of ComposeAxisAngles (src/core/sample_transform.cpp:98): on a pole-crossing
// fold BOTH lambda_out and roll_out shift by +pi.
//
// --- Table 1: slot-level declared densities (azimuth and roll slots, any type) -------------
// The az/roll slots are drawn by RandomNumberGenerator::Get (src/core/random.cpp:112) — a plain
// 1D draw in DEGREES, no folding, no area weighting, support on the whole real line. Declared
// density w.r.t. dtheta (radians) of the draw value, per DistributionType (transform authority:
// TransformDistribution, src/core/sample_transform.cpp:9):
//   kNoRandom       Dirac at center                        (Get returns Value(), random.cpp:112)
//   kUniform        1/R on [c - R/2, c + R/2], R = spread   (the FULL range is the spread)
//   kGaussian       N(center, spread^2)
//   kGaussianLegacy N(center, spread^2)   — on the az/roll slots the two Gaussians are the SAME
//                   distribution (the "Jacobian correction" is latitude-path-only; verified
//                   against the sampler: both draw GetGaussian()*spread + center)
//   kLaplacian      Laplace(center, b = spread)             (inverse CDF, sample_transform.cpp:25)
//   kZigzag         law of |A sin(2 pi U) + B|, A = spread, B = center (sample_transform.cpp:20);
//                   compact support, piecewise arcsine-of-arcsine, closed form in the .cpp
// DEGENERATE (all types): spread == 0 makes BuildDistributionDrawPlan answer kConstant
// (src/core/sample_transform.cpp:54), so the declared slot law is a Dirac — at center for every
// type EXCEPT kZigzag, whose constant is |center| (|0*sin + B| = |B|). The degenerate value is
// type-visible and is spelled per type in SlotDegenerateValue.
//
// --- Table 2: latitude-slot declared density, per sampling path -----------------------------
// The path is lat_path::SelectLatPath (src/core/shared/lat_path_selection.hpp:55):
//   kFullSphere     (config-level: az uniform[0,360] centered 0 AND lat uniform[90,360] centered
//                   90 AND roll full-turn uniform — IsFullSphereUniform): latitude = asin(2U - 1)
//                   (TransformFullSphereLatitude), never flips. rho_phi(phi) = cos(phi)/2 w.r.t.
//                   dphi on [-pi/2, pi/2]; the joint pose density is the plain slot product.
//   kNoRandom       latitude = center * deg2rad taken UNFOLDED (src/core/random.cpp:172) — a
//                   Dirac at the raw center, whatever its range (no normalize_latitude here).
//   kGaussLegacy    phi = normalize_latitude(N(center, spread)) (TransformLegacyLatitude): the
//                   folded proposal with NO area Jacobian. rho_phi = p_fold, where
//                   p_fold(phi) = sum_k p(phi + 2 pi k) + p(pi - phi + 2 pi k)  (both preimage
//                   families of the fold, src/core/math.cpp:341 detail::NormalizeLatitude) and
//                   the fold integrates to 1, so no renormalization.
//   kLutInverseCdf  (kUniform / kGaussian / kZigzag / kLaplacian): the unified area-measure
//                   target (src/core/lat_lut.cpp BuildLatLut): the folded proposal weighted by
//                   the spherical-area Jacobian, rho_phi(phi) = p_fold(phi) * cos(phi) / Z,
//                   Z = integral of p_fold * cos over [-pi/2, pi/2]. The comment at
//                   src/core/random.cpp:151 states the Jacobian; lat_lut.hpp:26 states that the
//                   LUT target reproduces the FOLDED semantics exactly — this closed form is the
//                   continuum limit of that target, and the test compares the closed-form CDF
//                   against the built LUT's cdf table at the LUT's own quadrature error.
//                   The Gaussian proposal's +-12 sigma window (lat_lut.cpp:102) and the
//                   U-quadrature families' exact transforms are inherited: the window effect is
//                   below 1e-30 relative wherever the closed form is non-negligible.
// The FLIP PROBABILITY density (needed by the joint pose law): P(flip | phi) =
// p_flip_branch(phi) / p_fold(phi) where p_flip_branch = sum_k p(pi - phi + 2 pi k) is the
// branch-B (reflected) preimage family (lat_lut.cpp flip_mass accumulates exactly this share).
//
// --- Table 3: joint pose density at composed angles -----------------------------------------
// rho_pose(lambda_out, phi, roll_out) = rho_phi(phi) * [ (1 - f(phi)) * rho_az(lambda_out) *
// rho_roll(roll_out) + f(phi) * rho_az(lambda_out - pi) * rho_roll(roll_out - pi) ],
// f = P(flip|phi) from Table 2; the kFullSphere path has f = 0. Reference measure: dlambda dphi
// droll (angle Lebesgue). A Dirac slot makes the joint singular in that slot; the evaluator
// returns 0 density off the Dirac's support slice and the singular mass is carried by the
// u-marginal's support kinds instead (no delta arithmetic in a plain double API).
//
// --- Table 4: the u-marginal rho_u (u = R^T s_hat, the body-frame sun direction) -------------
// The map's closed form: u = Rz(-roll_out) . Ry(pi/2 - phi) . Rz(pi - lambda_out) . s_hat.
// Let sigma be the sun's latitude, lambda_s its longitude. Support classification (number of
// spread slots among {az, lat, roll}, with the latitude Dirac's value phi_0):
//   0 spread          kPoint        u is a single point.
//   az spread only    kSpinOrbit    u(theta) = Rz(-roll_0) . Ry(pi/2 - phi_0) . Rz(pi - theta) .
//                                   s_hat, theta = the az draw; the orbit is a circle (rigid
//                                   image of the sun's latitude circle). Density w.r.t. dtheta =
//                                   rho_az(theta); arc-length density = rho_az / |cos(sigma)|
//                                   (|du/dtheta| = cos(sigma), constant along the orbit — the
//                                   1/(2 pi |cos phi_u|)-shaped latitude-circle density of the
//                                   design discussion). This is the plate-family fiber (C12).
//   roll spread only  kRollOrbit    u(r) = Rz(-r) . v_0, a circle about the body z axis through
//                                   v_0 = Ry(pi/2 - phi_0) Rz(pi - lambda_0) s_hat; density
//                                   w.r.t. dr = rho_roll(r), arc density = rho_roll / cos(lat_u).
//   lat spread only   kLatitudeOrbit  u(phi) = Ry(pi/2 - phi) . w_0, w_0 = Rz(pi - lambda_0)
//                                   s_hat; density w.r.t. dphi = rho_phi(phi) (the folded
//                                   latitude law), arc density = rho_phi / |cross(y_hat, u)|.
//   >= 2 spread       kArea         rho_u(u) w.r.t. dOmega exists as a function. FAST PATH (the
//                   existing predicates, NOT new ones — IsAzRotationallySymmetric() &&
//                   IsRollRotationallySymmetric(), src/core/random.hpp:236/254): rho_u is zonal
//                   about the body z axis (the I_0 spin average; full-circle continuation of the
//                   azimuth), rho_u(u) = m(u_z)/(2 pi) with m the density of u_z = cos(angle
//                   between the axis direction and the sun) = cos(gamma). m(c) = integral of
//                   rho_phi(phi) * ArcsineKernel(c; phi, sigma) dphi, the arcsine law of
//                   cos(gamma) = sin(phi) sin(sigma) + cos(phi) cos(sigma) cos(lambda - lambda_s)
//                   at fixed phi under uniform lambda. Equivalence conditions of the fast path:
//                   azimuth full-turn uniform AND roll full-turn uniform AND slot independence
//                   (the product law) — exactly the two predicates; a config that fails them
//                   takes the general path. The fast path is exact for any latitude slot (its
//                   rho_phi may itself be folded, legacy, or the full-sphere law).
//                   GENERAL PATH (slow): rho_u(u_0) = integral d(roll_out) of the roll law times
//                   the sum over the (lambda*, phi*) preimages of Rz(-roll_out) Ry(pi/2 - phi*)
//                   Rz(pi - lambda*) s_hat = u_0 of rho_pose(lambda*, phi*, roll_out) /
//                   |cos(sigma) cos(psi(w*))|, where the preimages are the intersections of the
//                   sun's latitude circle {w = Rz(pi - lambda) s_hat} with the y-rotation orbit
//                   of Rz(roll_out) u_0 (zero or two points), psi(w) is w's longitude and the
//                   Jacobian factor is the area Jacobian of (lambda, phi) -> u at the preimage.
//                   On the u-torus the azimuth and roll laws enter WRAPPED (rho~(x) = sum over k
//                   of rho(x + 2 pi k)): the pose (lambda, phi, roll) and its 2 pi-shifted copy
//                   are the same pose, so the marginal sums every shift's density. A Dirac roll
//                   closes the roll integral. DEGENERATE: sigma = +-pi/2 (sun at the pole)
//                   collapses the lambda degree of freedom (cos(sigma) = 0) — reported as
//                   kDegenerateSunGeometry, no density is invented.
//
// v1 surface gaps (pre-registered, fill when a consumer needs them): MuPositive's parameter
// inversion is wired for kSpinOrbit only (kRollOrbit / kLatitudeOrbit answer membership by
// distance-to-orbit); kDegenerateSunGeometry answers nothing; the general path does not solve
// the Dirac-latitude preimage (a fixed-zenith family with NON-uniform azimuth and spread roll —
// the symmetric members of that family take the fast zonal path, which carries the Dirac mass
// exactly as the arcsine ring). Adding any of these is a new function or a wider branch, not a
// semantic change to what is already answered.
//
// --- Relation to analytic::PoseDensity (deliberate DOUBLE implementation, not a fork) -------
// pose_density.hpp is the LI band-sum contract's density: Haar reference, Gaussian zenith only,
// 1e-12 contract. This module is the engine's declared measure: angle-Lebesgue slots, the full
// DistributionType set, fold semantics. The two agree ON THE OVERLAP FAMILY (zenith Gaussian,
// az and roll full-turn uniform) up to the reference-measure Jacobian (Haar = angle-Lebesgue
// scaled by 8 pi^2 / cos(phi) on these coordinates); the cross-table test pins that conversion.
// The implementations are not shared on purpose: each serves a different contract, and welding
// them would push the LI contract's semantics into the product's declared measure.

#include <array>

#include "core/random.hpp"

namespace lumice::raypath {

// ---------------------------------------------------------------------------
// Slot-level densities (Table 1). All angles radians; densities per radian.
// ---------------------------------------------------------------------------

// True when the slot's declared law is a point mass; `at_rad` receives the point (the per-type
// degenerate value — note kZigzag's |center|). kNoRandom is always a Dirac.
bool SlotIsDirac(const Distribution& slot, double* at_rad);

// The declared density of a NON-Dirac slot at x (0 outside the support). Calling it on a Dirac
// slot returns 0 — a point mass has no density; use SlotIsDirac.
double SlotDensityValue(const Distribution& slot, double x_rad);

// The Dirac value of a slot that IS a point mass (per-type: center, except kZigzag's |center|).
double SlotDegenerateValue(const Distribution& slot);

// The density of the kZigzag law |A sin(2 pi U) + B| at y (A = spread, B = center, radians):
// a finite sum over the up-to-four preimages t of +-y on one sine period,
// sum 1/(2 pi A |cos t|), empty outside the support. Exposed for its own unit test; callers use
// SlotDensityValue.
double ZigzagDensityValue(double amplitude_rad, double tilt_rad, double y_rad);

// ---------------------------------------------------------------------------
// Latitude-slot density (Table 2): the folded law per sampling path.
// ---------------------------------------------------------------------------

enum class LatitudeLawKind {
  kFullSphere,     // cos(phi)/2, never flips
  kFoldedArea,     // LUT families: p_fold * cos(phi) / Z
  kFoldedNoArea,   // kGaussianLegacy: p_fold (no Jacobian, already normalized)
  kDiracUnfolded,  // kNoRandom: point mass at the raw center (no fold)
  kDiracFolded,    // degenerate non-noRandom latitude: point mass at the folded center
  // (the zero-mass guard of BuildLatLut pins the LUT families' Dirac there; the legacy path
  // folds explicitly — both land at the folded degenerate value)
};

class LatitudeDensity {
 public:
  LatitudeLawKind kind() const { return kind_; }
  // rho_phi(phi) w.r.t. dphi on [-pi/2, pi/2]; 0 outside; the kDiracUnfolded law returns 0
  // (use point()).
  double Evaluate(double phi_rad) const;
  // P(flip | phi), the branch-B share of the folded law (0 for kFullSphere/kDiracUnfolded).
  double FlipProbability(double phi_rad) const;
  // The Dirac point of a kDiracUnfolded law (radians).
  double point() const { return point_; }
  // Z of the kFoldedArea law (integral of p_fold * cos); 1 for the others.
  double norm() const { return norm_; }
  // The normalization integral in double Gauss-Legendre (the test's ruler reuses it).
  double TotalMass() const;

 private:
  friend LatitudeDensity MakeLatitudeDensity(const Distribution& lat_slot);
  friend LatitudeDensity MakeFullSphereLatitude();
  LatitudeLawKind kind_ = LatitudeLawKind::kDiracUnfolded;
  Distribution proposal_{};  // the slot's own law, degrees (kFolded* paths)
  double point_ = 0.0;
  double norm_ = 1.0;
};

LatitudeDensity MakeLatitudeDensity(const Distribution& lat_slot);

// The latitude law of the FULL-SPHERE sampler branch (axis.IsFullSphereUniform(): az uniform
// [0, 360] at 0, lat uniform [90, 360] at 90, roll full-turn symmetric) — cos(phi)/2, never
// flipping. The branch is a CONFIG-level fact (SelectLatPath reads all three slots), so the
// joint/u-marginal evaluators call this when IsFullSphereUniform() holds; the slot-level
// MakeLatitudeDensity of the same latitude slot would give the same rho_phi with the LUT fold
// mixture, which is the wrong JOINT for that config (same u-marginal, different composed angles).
LatitudeDensity MakeFullSphereLatitude();

// ---------------------------------------------------------------------------
// Joint pose density (Table 3), w.r.t. dlambda dphi droll at COMPOSED angles (the sampler's
// ComposeAxisAngles outputs: shift-aware lambda_out / roll_out). 0 off a Dirac slot's slice.
// ---------------------------------------------------------------------------

double DeclaredPoseDensity(const AxisDistribution& axis, double lambda_out_rad, double phi_rad, double roll_out_rad);

// The same evaluation on a prebuilt latitude law (MakeLatitudeDensity / MakeFullSphereLatitude):
// building the law costs its normalization quadrature, so a loop over poses must not rebuild it
// per call. The axis supplies the azimuth and roll laws; the latitude law must be the axis's own
// (the caller that special-cases IsFullSphereUniform passes MakeFullSphereLatitude()).
double DeclaredPoseDensity(const LatitudeDensity& lat, const AxisDistribution& axis, double lambda_out_rad,
                           double phi_rad, double roll_out_rad);

// ---------------------------------------------------------------------------
// The u-marginal (Table 4). One instance answers both the certificate's pointwise mu questions
// and the quadrature's density questions, each with the reference measure the caller must name.
// ---------------------------------------------------------------------------

enum class USupportKind {
  kPoint,                   // all slots Dirac
  kSpinOrbit,               // az spread only: the tilted-circle orbit, parameter = the az draw
  kRollOrbit,               // roll spread only
  kLatitudeOrbit,           // latitude spread only
  kArea,                    // >= 2 effectively-spread slots
  kDegenerateSunGeometry,   // sun at a pole: the lambda degree of freedom collapses
  kDegenerateAxisGeometry,  // axis Dirac at a pole AND az and roll both spread: they act about
                            // the SAME body axis, so the support is one circle whose density
                            // is the az-and-roll convolution — a v2 addition, not answered
};

class UMarginal {
 public:
  USupportKind kind() const { return kind_; }
  // True when the fast (zonal) path is in force: az and roll full-turn uniform (the existing
  // AxisDistribution predicates; the equivalence conditions are Table 4's).
  bool fast_path() const { return fast_; }

  // Certificate side: is the declared measure positive at u (within angular_tol_rad of the
  // support)? For kArea this is rho_u(u) > 0; for the orbit kinds it is on-curve membership with
  // a positive parameter density; for kPoint, proximity to the point.
  bool MuPositive(const double u[3], double angular_tol_rad) const;

  // rho_u w.r.t. dOmega. kArea only (0 with kind() reporting anything else — the caller checked).
  // NaN when kind() == kDegenerateSunGeometry.
  double DensitySolidAngle(const double u[3]) const;

  // The GENERAL path's value at u, computed even where DensitySolidAngle would take the fast
  // zonal path. Exposed because the fast path's equivalence claim (Table 4) is checkable only
  // against this; production callers use DensitySolidAngle.
  double DensitySolidAngleGeneral(const double u[3]) const;

  // Spin-orbit accessors (kSpinOrbit only; the plate-family fiber):
  //   u(theta) = Rz(-roll_0) . Ry(pi/2 - phi_0) . Rz(pi - theta) . s_hat
  //   density w.r.t. dtheta at theta = the az slot law there.
  void SpinOrbitPoint(double theta_rad, double u_out[3]) const;
  double SpinOrbitThetaDensity(double theta_rad) const;

  // The latitude/roll orbit point+parameter density, for the other one-dimensional kinds
  // (parameter conventions in Table 4). u_out receives the orbit point when non-null.
  double OrbitDensity(double parameter_rad, double u_out[3]) const;

  // The single support point (kPoint only).
  void Point(double u_out[3]) const;

  const double* sun_dir() const { return sun_; }
  const AxisDistribution& axis() const { return axis_; }

 private:
  friend UMarginal MakeUMarginal(const AxisDistribution&, const double sun_dir[3]);
  USupportKind kind_ = USupportKind::kPoint;
  bool fast_ = false;
  AxisDistribution axis_{};
  LatitudeDensity lat_{};
  double sun_[3] = { 0.0, 0.0, 1.0 };
  double sigma_ = 0.0;     // sun latitude, radians
  double lambda_s_ = 0.0;  // sun longitude, radians

  // File-local helpers of the .cpp (declared here so the definitions bind to the class):
  void DiracLatRoll(double* phi0, double* roll0) const;
  void DiracAz(double* az0) const;
  double SpinOrbitParameter(const double u[3]) const;
  double OrbitAnchor(double u_out[3]) const;
  double PreimageSum(const double u0[3], double roll_out, bool flip_branch) const;
};

// Builds the u-marginal of `axis` under the sun at `sun_dir` (a unit vector pointing AT the sun;
// the propagation direction of incident light is -sun_dir). sun_dir must be unit to ~1e-10 and
// not at a pole (|z| < 1 - 1e-12), else the result reports kDegenerateSunGeometry.
UMarginal MakeUMarginal(const AxisDistribution& axis, const double sun_dir[3]);

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_DETAIL_DECLARED_DENSITY_HPP_
