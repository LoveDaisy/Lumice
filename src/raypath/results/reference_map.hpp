#ifndef LUMICE_RAYPATH_RESULTS_REFERENCE_MAP_HPP_
#define LUMICE_RAYPATH_RESULTS_REFERENCE_MAP_HPP_

#include <optional>
#include <string>
#include <vector>

namespace lumice::raypath::survey {

// One evaluated pixel of a class reference map, both probe-frame coordinates spelled (the
// document's row). The map is a (deviation x azimuth-about-sun) grid in the probe frame — the
// line probe's own frame and band-sum pixel convention (detail/quantity/probe_line.hpp), one
// implementation family for both triggers. The value is the class sum: the per-sr band-sum
// values of the class's band members summed member-by-member on one shared lattice.
struct MapPixelSample {
  double delta_deg = 0.0;    // the cell centre's deviation from the frame's incident
  double azimuth_deg = 0.0;  // the cell centre's azimuth about the sun, in the probe frame
  double value = 0.0;        // per sr (the band-sum value convention), the class sum
  int k = 0;                 // summed member event counts in the cell
  // ok / singular / point_mass: singular is view-determined (the cell covers the frame's
  // incident or its antipode — NaN value, never a written zero); point_mass marks the cell
  // holding the rank-0 arm's mass (m_total / cell solid angle).
  enum class Status { kOk, kSingular, kPointMass };
  Status status = Status::kOk;
};

// The rank-0 arm's payload: the summed per-member masses (each a RankZeroMass) and the grid
// index of the cell holding m_total / Omega — the first cell containing the frame's incident,
// -1 when the view does not cover it. A rank-0 class's sky image IS this mass, not a per-sr
// texture; a map that carries it never reads its zero pixel arm as "no light".
struct MapPointMass {
  double m = 0.0;
  double m_error = 0.0;  // summed per-member quadrature changes (a conservative sum)
  // The per-member method names, deduplicated in member order (lattice_mean / psi_average);
  // more than one entry names a mixed-class sum honestly.
  std::vector<std::string> methods;
  int pixel = -1;
};

// The view's mapping description: everything a reader needs to map grid indices to sky
// directions and back, without deriving a second convention. Row-major grid: row r covers the
// delta interval [delta_min_deg + r*d_row, ... + d_row], column c the azimuth interval
// [az_min_deg + c*d_az, ... + d_az]; a direction is ProbeDirection(frame, delta, az) about the
// frame's incident, and the inverse reading maps a direction through the frame's (e1, e2).
struct MapViewDescription {
  double delta_min_deg = 0.0;
  double delta_max_deg = 0.0;
  double az_min_deg = 0.0;
  double az_max_deg = 0.0;
  int rows = 0;
  int cols = 0;
  // The probe frame (MakeProbeFrame of the incident): the three axes a GUI needs for the
  // pixel <-> sky-direction mapping, spelled so the convention travels with the result.
  double incident[3] = { 0.0, 0.0, -1.0 };
  double e1[3] = { 0.0, 0.0, 1.0 };
  double e2[3] = { 0.0, 0.0, 0.0 };
  bool full_sky = false;  // the named all-sky mode (delta [0, 180], azimuth [0, 360])
};

// The evaluated class map. Under a cooperative stop the pixels are the completed members'
// full-lattice sums — `members_evaluated`/`truncated` say which member prefix actually ran, so
// a cut map never presents itself as the whole class: a truncated member's partial-lattice
// sums are discarded, never mixed into the completed prefix's full-lattice values.
struct ReferenceMapData {
  double wavelength_nm = 0.0;
  double refractive_index = 0.0;
  int sample_count = 0;       // the lattice n each member ran
  long long kept_count = 0;   // summed kept events (w > 0) over the evaluated members
  int member_count = 0;       // the class's full size
  int members_evaluated = 0;  // the completed member prefix (== member_count when not truncated)
  bool truncated = false;
  long long kept_count_truncated_member = 0;  // the cut member's own kept count, 0 when none
  MapViewDescription view;
  std::vector<MapPixelSample> pixels;         // row-major, rows * cols entries
  std::optional<MapPointMass> point_mass;     // present iff at least one rank-0 member was evaluated
  std::vector<long long> member_kept_counts;  // per evaluated member, in member order (coverage)
};

}  // namespace lumice::raypath::survey

#endif  // LUMICE_RAYPATH_RESULTS_REFERENCE_MAP_HPP_
