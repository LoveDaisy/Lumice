#ifndef LUMICE_RAYPATH_PROBE_ANALYSIS_HPP_
#define LUMICE_RAYPATH_PROBE_ANALYSIS_HPP_

// The probe query (routing.md's L2 draw-a-line face): the canonical probe library's profile at a
// user-chosen location. The canonical probe IS a slice of the L1 synchronous reference-lambda
// band-sum map (evidence.md section 4): every line here is a PixelTable fed to the same
// analytic::BandSum core the survey's built-in radial probe runs (detail/survey/survey_probe.hpp —
// one implementation, two triggers), never a second evaluation path. The chromatic lambda grid
// lives on the line only: each requested wavelength is one BandSum on the same layout, never a
// multi-wavelength map.
//
// The four --along forms (routing.md's probe subdivision) are two line geometries:
//   radial / edge-normal — a delta-axis line at the user's azimuth about the sun; edge-normal
//     anchors the same geometry's window at the context's nearest edge (a support endpoint or the
//     certified edge position, whichever sits closest to the user's deviation);
//   arc / tangential — a constant-delta arc through the user's point. The current context carries
//     only delta-axis facts (support intervals, the certified edge position), so the arc form
//     declares its constant-delta reading of "along the arc" in its spec; a true curve-following
//     profile needs the curve context and belongs to a later wave.
//
// The ruler constants are schema, not knobs (the survey probe's precedent): the bin is the survey
// probe's 0.1 deg, the wedge the survey's, the window forms' half width kProbeWindowHalfWidthDeg,
// the lattice the survey probe's antipodal-Fibonacci count. Every profile carries its evidence
// state and a coverage declaration; the dark-region decomposition classifies each wavelength's
// line against support valid at that wavelength, never the reference context by default.

#include <optional>
#include <string>
#include <vector>

#include "analytic/stop.hpp"
#include "config/config_manager.hpp"
#include "core/def.hpp"
#include "raypath/domain/input_metadata.hpp"
#include "raypath/domain/scope.hpp"
#include "raypath/prepared_geometry.hpp"
#include "raypath/probe_result.hpp"
#include "raypath/query_control.hpp"
#include "raypath/survey_reuse.hpp"

namespace lumice::raypath {

// The window forms' half width (edge-normal and the arc forms), degrees — schema, echoed in every
// profile's spec; the survey probe's wedge/bin constants are shared by include. The reuse
// criterion's premise lives on these constants being schema: warm values are reused only because
// the layout they were computed on is the layout being asked for; if a constant ever becomes a
// knob, the criterion must grow the comparison.
constexpr double kProbeWindowHalfWidthDeg = 10.0;
constexpr int kMaxProbeWavelengthCount = 32;

// The budget's default and bound (the point query's shape); enforcement sits at the per-lambda
// BandSum boundaries, so a stopped run serializes the profiles it has with meta.interrupted.
constexpr int kDefaultProbeBudgetMs = 15000;
constexpr int kMaxProbeBudgetMs = 120000;

struct ProbeRequest : PathSelection {
  // One face sequence per scattering layer, as the point request spells; v1 takes exactly one
  // layer and refuses more.
  ProbeAlong along = ProbeAlong::kRadial;
  // The user's sky point (`analyze --center` convention). The radial/edge-normal forms read its
  // azimuth about the sun; arc/tangential read its deviation. Uniform densities are
  // azimuth-free, while non-uniform warm readings require the survey probe's line azimuth.
  double at_altitude_deg = 0.0;
  double at_azimuth_deg = 0.0;
  // Empty: the config's single wavelength or 550 (the point resolution), one profile. Non-empty:
  // the online lambda grid — one profile per entry, in order, each with its own refractive index.
  std::vector<double> wavelengths_nm;
  int sample_count = 0;  // the band-sum lattice n; 0 = the survey probe's count
  // Untrusted warm candidate, decoded from JSON or harvested from a typed survey.
  // The query admits it through survey_reuse before consuming seeds or facts.
  std::optional<WarmCandidate> warm_candidate;
  int budget_ms = 0;  // 0 = the documented default; polled at the per-lambda boundaries
  const volatile int* interrupted = nullptr;
  // The query's one-way cancel token (the new contract's cancel core): folded into the same
  // single stop the per-lambda boundaries poll, and into the context's cancel-only stop.
  const analytic::StopToken* cancel = nullptr;
  std::optional<QueryDeadline> query_deadline;
};

// The whole analysis. On error `out` is left default-constructed.
// `geometry` (optional): a workspace's prepared tables for the selection's crystal — the query
// then builds no tables of its own (the workspace's reuse read).
Error AnalyzePathProbe(const ConfigManager& config, const ProbeRequest& request, ProbeResult* out,
                       const PreparedGeometry* geometry = nullptr);

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_PROBE_ANALYSIS_HPP_
