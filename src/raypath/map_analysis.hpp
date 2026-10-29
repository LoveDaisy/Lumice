#ifndef LUMICE_RAYPATH_MAP_ANALYSIS_HPP_
#define LUMICE_RAYPATH_MAP_ANALYSIS_HPP_

// The map query (the reference-map face): the whole physical class's single-reference-lambda
// band-sum map over an explicit view. The line probe IS a slice of this evaluation — the same
// detail/quantity kernel, one implementation, three triggers (the survey's built-in radial
// line, the probe face's lines, this map). The map differs from the line in its aggregation:
// it evaluates the CLASS (every resolved member, band values summed member-by-member; rank-0
// members into the point-mass arm), where the line evaluates the representative.
//
// Caliber (schema, echoed, never implied): single reference wavelength (the probe resolution's
// rule — the scene's single discrete wavelength, else 550; per-lambda maps are a later wave),
// the nominal geometry, the point sun, the scene's declared pose density or the named refusal.
// Every view request rebuilds the PixelTable and re-runs every member: no warm reuse, no
// upscaling of an older grid (the cross-wavelength warm lesson: view/lambda-dependent values
// are always recomputed; the workspace's geometry slot stays the only reuse).
//
// Synchronous and deterministic on the calling thread (a caller's background thread is its own
// decision); the budget/cancel polls sit at the per-member boundaries — a stopped map keeps
// its completed member prefix and marks truncated, never presenting partial sums as the class.

#include <optional>
#include <string>
#include <vector>

#include "analytic/stop.hpp"
#include "config/config_manager.hpp"
#include "core/def.hpp"
#include "raypath/domain/input_metadata.hpp"
#include "raypath/domain/scope.hpp"
#include "raypath/map_result.hpp"
#include "raypath/prepared_geometry.hpp"
#include "raypath/query_control.hpp"

namespace lumice::raypath {

// The budget's default and bound (the probe face's published pair, mirrored like the survey's);
// enforcement sits at the per-member boundaries.
constexpr int kDefaultMapBudgetMs = 15000;
constexpr int kMaxMapBudgetMs = 120000;

// The view's cell bound. Schema, not tuning — the measured per-cell cost is second-order (the
// M1 probe: 4x the cells cost +15% at a fixed domain), so the bound caps the typed result's
// memory, not speed.
inline constexpr long long kMaxMapPixels = 262144;  // 512 x 512

// The default lattice (the survey probe's count) and the full-sky view's default grid — the
// M1 probe's constants: measured full-sky class maps land ~3.4 s at these values, well inside
// the default budget; the cell bound kMaxMapPixels sits beside the budget pair above.
constexpr int kDefaultMapSampleCount = 2000000;
constexpr int kMapFullSkyRows = 180;
constexpr int kMapFullSkyCols = 360;

struct MapRequest : PathSelection {
  // The view: the named all-sky mode (delta [0, 180], azimuth [0, 360)) or an explicit window
  // centred on the sky point (the probe's at-point convention for centre), spans in degrees.
  bool full_sky = false;
  double centre_altitude_deg = 0.0;
  double centre_azimuth_deg = 0.0;
  double delta_span_deg = 0.0;
  double azimuth_span_deg = 0.0;
  // The sampling scale: the grid's rows (delta) and cols (azimuth); cells/row-major.
  int view_rows = 0;
  int view_cols = 0;
  // Empty: the scene's single discrete wavelength, else 550 (the point resolution). At most
  // one entry — the per-lambda map family is a later face.
  std::vector<double> wavelengths_nm;
  int sample_count = 0;  // 0 = kDefaultMapSampleCount; else [1, kMaxSampleCount]
  int budget_ms = 0;     // 0 = the documented default; polled at the per-member boundaries
  const volatile int* interrupted = nullptr;
  const analytic::StopToken* cancel = nullptr;
  std::optional<QueryDeadline> query_deadline;
};

// The whole analysis. On error `out` is left default-constructed. `geometry` (optional): a
// workspace's prepared tables for the selection's crystal — the query then builds no tables
// of its own (the workspace's reuse read).
Error AnalyzePathMap(const ConfigManager& config, const MapRequest& request, MapResult* out,
                     const PreparedGeometry* geometry = nullptr);

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_MAP_ANALYSIS_HPP_
