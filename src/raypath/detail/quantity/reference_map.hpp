#ifndef LUMICE_RAYPATH_DETAIL_QUANTITY_REFERENCE_MAP_HPP_
#define LUMICE_RAYPATH_DETAIL_QUANTITY_REFERENCE_MAP_HPP_

// The class reference map (routing.md's map trigger — the third trigger of the shared kernel):
// a (deviation x azimuth-about-sun) grid in the probe frame, each cell fed to the SAME
// analytic::BandSum the line probe runs — one implementation, now three triggers (the survey's
// built-in radial line, the query face's lines, this map). The map's unit of evaluation is the
// whole physical class: the resolved member list, each member one BandSum on the same lattice
// and the same PixelTable, band members' per-sr values summed member-by-member, rank-0 members
// (path_rank) summed into the point-mass arm instead — never a representative times a member
// count, and never rank-0 mass smeared into the per-sr pixels.
//
// Determinism: the lattice is the band sum's antipodal Fibonacci sample — identical requests
// (same members, view, lambda, n) reproduce the map bit for bit. A view change re-runs every
// member: no caching, no upscaling of an older grid.
//
// Honesty under a stop: the member loop polls the stop before each member (and the BandSum
// rides it into the lattice pass). A member the stop cut mid-lattice is DISCARDED — its
// partial-lattice sums never mix with the completed prefix's full-lattice values — and the
// map marks truncated with the completed member count.

#include <vector>

#include "analytic/band_sum.hpp"
#include "analytic/pose_density.hpp"
#include "raypath/detail/quantity/probe_line.hpp"
#include "raypath/results/reference_map.hpp"
#include "raypath/scene_to_analytic.hpp"

namespace lumice::raypath::survey {

// Materializes the view's PixelTable in the probe frame (cell centres / four corners in cyclic
// order / solid angles by the band-sum pixel convention — the same statements the line probe's
// cell loop makes, generalized from one row to the grid).
class MapPixelGrid {
 public:
  MapPixelGrid(const ProbeFrame& frame, const MapViewDescription& view);

  const analytic::PixelTable& table() const { return pixels_; }
  const MapViewDescription& view() const { return view_; }

 private:
  MapViewDescription view_;
  std::vector<double> centres_;
  std::vector<double> corners_;
  std::vector<double> solid_angles_;
  analytic::PixelTable pixels_;
};

// The class evaluation. `members` is the resolved physical class (the expansion authority's
// output, in its order); `density` the caller's gated conversion (this core builds no spec of
// its own, the probe's rule). A member with no kept event yields honest zeros — the map never
// refuses on emptiness; emptiness is kept_count == 0 and the caller reads it as finite zero.
ReferenceMapData EvaluateReferenceMap(const analytic::FaceNormalTable& table,
                                      const analytic::FacePolygonTable& polygons,
                                      const std::vector<std::vector<int>>& members,
                                      const PoseDensityConversion& density, const ProbeFrame& frame,
                                      const MapViewDescription& view, double wavelength_nm, double refractive_index,
                                      const double incident[3], int sample_count,
                                      const analytic::DiscoveryStop* stop = nullptr);

}  // namespace lumice::raypath::survey

#endif  // LUMICE_RAYPATH_DETAIL_QUANTITY_REFERENCE_MAP_HPP_
