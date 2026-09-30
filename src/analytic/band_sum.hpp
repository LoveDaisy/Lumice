#ifndef LUMICE_ANALYTIC_BAND_SUM_HPP_
#define LUMICE_ANALYTIC_BAND_SUM_HPP_

// Module B of liblumice_analytic: the single-path brightness map by the band sum (LI
// docs/band-sum-contract.md, li_rev fa8dadd). One concrete face sequence, one refractive index, one
// pose density, a table of pixel directions. The sample is discovery's (section 3: the antipodal
// Fibonacci lattice of u = R^-1 s_hat, kept iff w = A T > 0, IceDiscovery::BuildEvents); each pixel
// sums the kept events whose deviation lies in the pixel's corner band, each posed at the pixel's
// azimuth and weighted by the density there (section 4). A rank-0 path (path_rank.hpp) is a point
// mass in the sun direction instead (section 5).
//
// Two entries, the contract's two conformance layers (section 7): BandSumOnEvents is the estimator
// on given events (layer 1: LI's events), BandSum regenerates the sample and runs the whole call
// (layer 2). A red in layer 2 alone is the sampler or the fields; in both, the estimator.
//
// Units: a value is LI's Phase I pixel value — power per steradian sent along the path by one
// crystal of the ensemble, per unit incident irradiance, with the crystal at hexagon edge a = 1
// (section 6). Everything is double with gradual underflow (K_rho_pos counts subnormal
// contributions; a caller running on a thread with flush-to-zero set gets smaller counts).

#include <vector>

#include "analytic/discovery.hpp"
#include "analytic/pose_density.hpp"

namespace lumice::analytic {

// The pixel table of section 2.3: per pixel its centre direction, four corner directions in cyclic
// order (either orientation) and its solid angle (read by the point mass only). World unit
// propagation directions; pointers to `count` * 3, `count` * 12 and `count` doubles.
struct PixelTable {
  int count = 0;
  const double* centre = nullptr;
  const double* corners = nullptr;
  const double* solid_angle = nullptr;
};

enum class PixelStatus { kOk, kSingular, kPointMass };

// One pixel of the result (section 6). For a singular pixel `value`, `k_eff` are NaN and the counts
// 0; for a rank-0 call the band fields (delta, delta_lo, delta_hi) are NaN and the counts 0.
struct PixelValue {
  PixelStatus status = PixelStatus::kOk;
  double value = 0.0;
  double delta = 0.0;
  double delta_lo = 0.0;
  double delta_hi = 0.0;
  int k = 0;
  int k_rho_pos = 0;
  double k_eff = 0.0;
};

// How a rank-0 call's mass was computed (section 5): the lattice mean of w under the random
// density (deterministic, as parity requires), or this library's deterministic psi average under
// any other density, with the change of the last doubling of its quadrature as `m_error`.
enum class PointMassMethod { kLatticeMean, kPsiAverage };

struct BandSumOutput {
  bool rank_zero = false;
  std::vector<PixelValue> pixels;
  // Rank 0 only.
  double m = 0.0;
  double m_error = 0.0;
  PointMassMethod method = PointMassMethod::kLatticeMean;
  int point_mass_pixel = -1;  // the first pixel in table order that contains s; -1 if none does
  // The number of kept events (w > 0) of the sample, both ranks.
  int kept_count = 0;
};

// Section 4.1: x lies in the spherical quadrilateral of cyclic `corners` (12 doubles) — its dot with
// the corners' sum is positive and det(c_k, c_(k+1), x) has one sign over k (zero allowed).
bool PixelContains(const double corners[12], const double x[3]);

// Layer 1: section 4 on `events` (kept, sorted by increasing deviation; the sum runs in that order),
// `n` the lattice size, `incident` s. Appends one PixelValue per pixel.
void BandSumOnEvents(const std::vector<SampleEvent>& events, int n, const double incident[3], const PixelTable& pixels,
                     const PoseDensity& density, std::vector<PixelValue>* out);

// Section 5 on kept weights (and, for a non-random density, their u): m, its error and method.
// `u` may be null under the random density.
struct PointMass {
  double m = 0.0;
  double error = 0.0;
  PointMassMethod method = PointMassMethod::kLatticeMean;
};
PointMass RankZeroMass(const std::vector<SampleEvent>& events, int n, const double incident[3],
                       const PoseDensity& density);

// Section 5's per-pixel output: `m / Omega_p` on the first pixel containing s, 0 elsewhere.
void PlacePointMass(double m, const double incident[3], const PixelTable& pixels, std::vector<PixelValue>* out,
                    int* point_mass_pixel);

// Layer 2: the whole call. `slots` resolved (2..kMaxFaceCount), `incident` a world unit vector, `n`
// the lattice size (>= 1), the pixel directions unit. A path with no kept event is not an error: every
// pixel is 0 (or, for rank 0, m = 0).
BandSumOutput BandSum(const FaceNormalTable& table, const FacePolygonTable& polygons, const int* slots, int slot_count,
                      double refractive_index, const double incident[3], int n, const PixelTable& pixels,
                      const PoseDensity& density);

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_BAND_SUM_HPP_
