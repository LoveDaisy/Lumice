#ifndef LUMICE_ANALYTIC_CURVE_PROFILE_HPP_
#define LUMICE_ANALYTIC_CURVE_PROFILE_HPP_

#include <array>
#include <limits>
#include <vector>

#include "analytic/axis_pose_density.hpp"
#include "analytic/discovery.hpp"
#include "analytic/path_intrinsic.hpp"
#include "analytic/pose_curve.hpp"

namespace lumice::analytic {

// Per-node physical values, not a C ABI. No terrain search, blur or wavelength derivatives.
// Inputs borrow only for the duration of a call; results own their data. PathIntrinsic's mutable
// corridor requires external synchronization when the SAME path value is used concurrently.
//
// Domain: finite unit sun/u/sky and proper body->world poses (path_evaluation.hpp validators),
// a valid manifold of the curve's dimension (1 for family, 2 for caustic/collapsed; unused for
// fibers). Euler parameters must reproduce pose and u = R^T sun to context_tolerance. Sky is
// not required to match an optically invalid node. Wavelengths are PathIntrinsic values with
// the SAME shape and literal faces as main_path, each carrying its discrete n > 1; the caller
// owns shape identity (the public field exposes normals, not finite polygon identity). Null
// wavelength pointers / different literal faces throw invalid_argument. Empty list = main only.
// Density retains AxisPoseDensity's stricter rotation domain and all its flags, including poles.
// Invalid node/context is flagged, never projected/clamped. Upstream curve coverage is retained.
//
// Optical factors use PathIntrinsic's U/area authorities and EvaluatePath's per-face coefficients.
// T_P is 0 off U_P, with validity/refusal retained. Internal TIR uses the chain convention
// d_k = 1-n^2(1-cos_i^2): negative = total, zero = critical. Unreached/nonfinite values are
// unavailable, not partial reflections. Relative Fresnel accuracy is NOT contracted when
// min(entry_disc, exit_disc) < grazing_discriminant (default 1e-6); this attaches a note only,
// never clamps T. Discriminants/angles, not reflectance derivatives, locate TIR events.
//
// Geometric coefficients: fibers use a fresh SO(3) normal Jacobian, never stored node sigmas.
// Caustics use Jet2<2> of sky in the manifold's FREE EULER coordinates q (Euclidean dq measure,
// NOT Haar or surface measure). Let sigma1 be the nonzero singular value, k the unit kernel,
// t the unit image tangent, nu = sky x t. beta = nu . D^2 sky[k,k]/2 (rad/rad^2).
// Two sheets push dq to concentration 1/(sigma1*sqrt(|beta|)*sqrt(epsilon)) in the normal sky
// distance epsilon on the illuminated side. rho/A/T are separate factors, not multiplied here.
// No cap or absolute acceptance threshold on beta, 1/J or the concentration coefficient: near
// cusps they can diverge. Exact zero beta/rank loss is a named degeneracy; nonfinite tensors
// are unavailable. Numerical/integral comparisons must use regularized or cumulative quantities.
// rank_relative_tolerance tests sigma2/sigma1 only; it is not a coefficient precision claim.
// Options domain: 64*eps <= context_tolerance <=1e-3, 0 < grazing_discriminant <=1,
// 64*eps <= rank_relative_tolerance <1; every option finite. Invalid options throw.
struct CurveProfileOptions {
  double context_tolerance = 1e-10;
  double grazing_discriminant = 1e-6;
  double rank_relative_tolerance = 1e-7;
};

enum class ProfileTirState { kUnavailable, kPartial, kCritical, kTotal, kNotInternal };

struct ProfileInterface {
  double incidence = std::numeric_limits<double>::quiet_NaN();
  double discriminant = std::numeric_limits<double>::quiet_NaN();
  double factor = std::numeric_limits<double>::quiet_NaN();
  bool reached = false;
  bool factor_available = false;
  ProfileTirState tir = ProfileTirState::kUnavailable;
};

struct ProfileOptics {
  double refractive_index = 0.0;
  bool valid = false;
  bool grazing_accuracy_note = false;
  double transmission = 0.0;
  EntryMeasure area;
  std::vector<ProfileInterface> interfaces;
};

enum class ProfileGeometryStatus { kNotApplicable, kAvailable, kInvalidNode, kUnavailable, kNotCaustic, kDegenerate };

struct ProfileGeometry {
  ProfileGeometryStatus status = ProfileGeometryStatus::kNotApplicable;
  double inverse_normal_jacobian = std::numeric_limits<double>::quiet_NaN();
  double lateral_beta = std::numeric_limits<double>::quiet_NaN();
  double fold_concentration = std::numeric_limits<double>::quiet_NaN();
  double sigma1 = std::numeric_limits<double>::quiet_NaN();
  double sigma2 = std::numeric_limits<double>::quiet_NaN();
  std::array<double, 2> kernel{};
  std::array<double, 3> sky_normal{};
};

struct CurveProfileNode {
  bool context_valid = false;
  AxisDensityValue rho;
  AxisDensityValue log_rho;
  ProfileOptics main;
  std::vector<ProfileOptics> wavelengths;
  ProfileGeometry geometry;
};

struct CurveProfile {
  PoseCurveKind kind = PoseCurveKind::kFamily1D;
  CurveCoverage coverage;
  std::vector<CurveProfileNode> nodes;
};

CurveProfile EvaluateCurveProfile(const PoseCurve& curve, const PathIntrinsic& main_path,
                                  const AxisPoseDensity& density, const PoseManifold& manifold, const double sun[3],
                                  const std::vector<const PathIntrinsic*>& wavelengths = {},
                                  const CurveProfileOptions& options = {});

struct ProfileExpansion {
  RecoveryStatus status = RecoveryStatus::kNotEvaluated;
  EntryMeasure area;
  RepresentativePath path;
  // Body-frame propagation directions: incident, internal segments, outgoing; interface factors
  // have the same order as the literal faces. Empty on optical/recovery refusal.
  std::vector<std::array<double, 3>> directions;
  std::vector<double> interface_factors;
};
ProfileExpansion ExpandProfileNode(const PathIntrinsic& path, const PoseCurveNode& node);

// Same-seed DISCRETE chromatic correctors. No global branch-discovery claim. Family1D evaluates
// the same free coordinate; Caustic2D solves J_reg=0 on the seed's transverse parameter section.
// TIR additionally solves internal d_k=0 (internal_index is the literal path position, 1..N-2).
// A 1D TIR solve moves the free coordinate. Collapsed/fiber chromatic re-tracing is unsupported,
// not approximated as a regular caustic. Per-node discrete optics still supports every curve kind.
// J_reg = entry_disc*exit_disc*sky.(sky_q0 x sky_q1), differentiated DIRECTLY by Jet2<2>.
// Newton stopping uses the O(1) inferred parameter correction in radians, never raw J or dR.
// Bounds: tolerance >=64*eps, <=1e-3; 1..128 iterations; 0 < max_travel <=pi; all finite.
// max_travel is the local branch association budget from the SAME seed, not a uniqueness proof.
// Singular systems use relative determinant <=64*eps; no absolute tensor/coefficient threshold.
// No parameter clipping/wrapping: leaving the declared nonperiodic manifold is kOutsideDomain.
struct ProfileChromaticOptions {
  double parameter_tolerance = 1e-11;
  double max_travel = 0.25;
  int maximum_iterations = 24;
};

enum class ProfileSolveStatus {
  kConverged,
  kInvalidInput,
  kUnsupported,
  kOpticallyInvalid,
  kOutsideDomain,
  kDegenerate,
  kTravelLimit,
  kIterationLimit
};

struct ProfileChromaticPoint {
  ProfileSolveStatus status = ProfileSolveStatus::kInvalidInput;
  PoseCurveNode node;
  double correction_rad = std::numeric_limits<double>::quiet_NaN();
  double travelled_rad = 0.0;
  int iterations = 0;
};
struct ProfileChromaticDisplacement {
  ProfileChromaticPoint red;
  ProfileChromaticPoint blue;
  bool available = false;
  // blue.sky - red.sky, a Cartesian chord in the world sky sphere (dimensionless).
  std::array<double, 3> dx{};
  double angle_rad = std::numeric_limits<double>::quiet_NaN();
};

ProfileChromaticPoint RetraceProfileSeed(const PathIntrinsic& path, PoseCurveKind kind, const PoseManifold& manifold,
                                         const double sun[3], const PoseCurveNode& seed, int internal_index = -1,
                                         const ProfileChromaticOptions& options = {});
ProfileChromaticDisplacement ProfileChromaticShift(const PathIntrinsic& red, const PathIntrinsic& blue,
                                                   PoseCurveKind kind, const PoseManifold& manifold,
                                                   const double sun[3], const PoseCurveNode& seed,
                                                   int internal_index = -1,
                                                   const ProfileChromaticOptions& options = {});

// On-demand along-arc linkage through the landed IceDiscovery authority (global band PLUS hot
// seed). discovery must have this node's path/index/sun context. All incomplete candidates are
// preserved. Complete() alone does NOT mean the hot seed converged (it may have been rejected).
// Returned hot_component indexes output.components; -1 means no converged hot component. A
// converged component does not imply completeness of global discovery. Stop applies to both
// the global lattice and candidate loop, as in IceDiscovery. Parameters keep that API's domains.
struct ProfileFiberOptions {
  int sample_count = kDefaultDiscoverySampleCount;
  double band_half_width = kDefaultBandHalfWidth;
  double cluster_radius = kDefaultClusterRadius;
  double dedup_distance = ContinuationParams{}.closure_distance;
  ContinuationParams continuation;
};
enum class ProfileFiberStatus { kConverged, kInvalidInput, kSeedRejected, kNotConverged, kStopped };
struct ProfileFiberLink {
  ProfileFiberStatus status = ProfileFiberStatus::kInvalidInput;
  bool hot_seed_admitted = false;
  bool stop_requested = false;  // global discovery may stop even after the hot component converged
  int hot_component = -1;
  DiscoveryOutput output;
};
ProfileFiberLink LinkProfileFiber(IceDiscovery& discovery, const PoseCurveNode& node,
                                  const ProfileFiberOptions& options = {}, const DiscoveryStop* stop = nullptr);

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_CURVE_PROFILE_HPP_
