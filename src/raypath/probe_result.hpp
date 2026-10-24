#ifndef LUMICE_RAYPATH_PROBE_RESULT_HPP_
#define LUMICE_RAYPATH_PROBE_RESULT_HPP_

#include <string>
#include <vector>

#include "core/def.hpp"
#include "raypath/domain/input_metadata.hpp"
#include "raypath/results/probe_line.hpp"
#include "raypath/survey_context.hpp"
namespace lumice::raypath {

// ---- metadata: every conversion, as applied ------------------------------------------------------

struct ProbeWavelengthEntry {
  double nm = 0.0;
  WavelengthSource source = WavelengthSource::kDefault;
  double refractive_index = 0.0;
};

struct ProbeMetadata {
  int analytic_api_version = 0;
  IdType crystal_id = 0;
  std::string crystal_kind;
  std::vector<NominalShapeScalar> shape;
  bool shape_is_nominal = false;
  double upper_wedge_deg = 0.0;  // pyramid only
  double lower_wedge_deg = 0.0;
  std::vector<int> faces;
  double sun_altitude_deg = 0.0;
  double sun_azimuth_deg = 0.0;
  double incident_direction[3]{};  // world, propagation sun -> crystal
  double at_altitude_deg = 0.0;
  double at_azimuth_deg = 0.0;
  double at_direction[3]{};               // world, light-travel direction arriving from the point
  double at_deviation_deg = 0.0;          // the point's deviation from the sun
  double at_azimuth_about_sun_deg = 0.0;  // its azimuth in the probe frame (the axis forms' line azimuth)
  std::vector<ProbeWavelengthEntry> wavelengths;
  int budget_ms = 0;
  bool interrupted = false;
};

// ---- profiles ------------------------------------------------------------------------------------

// Consecutive bin runs classified against support certified for this profile's wavelength.
struct ProbeDarkSegment {
  std::string kind;     // "inside_support_positive" | "inside_support_zero" | "outside_support" | "unknown"
  double lo_deg = 0.0;  // along the line's sweep coordinate (deviation for the axis forms,
  double hi_deg = 0.0;  // azimuth for the arc forms), the run's first/last bin bounds
  int bin_count = 0;
  std::string reason;  // unknown only: the per-wavelength support refusal
};

struct ProbeProfileReadings {
  bool has_edge = false;
  double edge_deg = 0.0;  // the first non-empty bin's centre (finite to half a bin)
  bool has_peak = false;
  double peak_deg = 0.0;  // the value argmax — a different thing from the edge (the corpus lesson)
  double peak_value = 0.0;
  double separation_deg = 0.0;  // peak - edge, signed
};

// One line's profile: the spec echoed, the reading's provenance named, the samples, the two
// readings, and the dark-region decomposition. A refused profile carries the spec shell the
// request already fixes and an evidence state of refused with the named mode — never silence.
struct ProbeProfile {
  std::string along;  // ProbeAlongName's spelling, echoed
  // The line's spec — every number a reader needs to reproduce the line (the caliber discipline:
  // schema, echoed, never a knob). The extent is the bins' actual span: the axis forms' window
  // ends at the last bin's hi edge, and the arc forms carry an extent of wedge_half_width_deg
  // total width, centred on the constant delta (delta ± half of it) — the constant deviation
  // itself lives in each sample's delta_deg.
  double delta_min_deg = 0.0;          // the line's delta extent; the arc forms carry the wedge-wide
  double delta_max_deg = 0.0;          // extent, not the bare constant (see above)
  double azimuth_deg = 0.0;            // the line's azimuth (axis forms) or centre azimuth (arc forms)
  double window_half_width_deg = 0.0;  // the axis forms' delta-window half width (0 = the full axis)
                                       // or the arc forms' azimuth-sweep half width
  double bin_deg = 0.0;
  double wedge_half_width_deg = 0.0;
  std::string anchor_basis;  // edge-normal only: "support_interval_endpoint" | "caustic_edge_position"
  double anchor_deg = 0.0;
  double anchor_wavelength_nm = 0.0;
  // The radial form is azimuth-free under uniform density; other densities require a matching
  // producer line for warm reuse. The other forms describe their own line geometries.
  std::string semantics;
  double wavelength_nm = 0.0;
  double refractive_index = 0.0;
  int sample_count = 0;
  std::string lattice = "antipodal_fibonacci";
  bool deterministic = true;
  std::vector<int> representative;
  int member_count = 0;
  // The reading's provenance, mechanically named (the review's zero-recompute ruling):
  // "warm_reused" — the values are the warm document's own samples, checked against the
  //   requested scope, line orientation (if non-uniform), and bin layout; no BandSum here;
  // "recomputed" — BandSum ran here (lattice-deterministic, no seed dependence).
  std::string source;
  // Evidence: "finite" with the coverage declaration (the spec above is the coverage), or
  // "refused" with the named mode: no_edge_in_context, measure_not_expressible,
  // degenerate_arc_at_pole, inline_survey_refused.
  std::string evidence_state;
  std::string refused_mode;
  int members_evaluated = 0;                     // finite: 1 — the representative; the class reads xN
  std::vector<survey::ProbeLineSample> samples;  // finite only; both line coordinates spelled
  ProbeProfileReadings readings;                 // finite only
  std::vector<ProbeDarkSegment> dark_regions;    // classified at this profile's wavelength
};

// ---- result --------------------------------------------------------------------------------------

struct ProbeResult {
  ProbeMetadata meta;
  // Where the --at point sits in the survey's structure: the point document's context shape, read
  // at the at point (warm document's facts, or the context-only T0-T2 assembly).
  SurveyContextBlock context;
  std::vector<ProbeProfile> profiles;  // one per requested wavelength, in order
  // The red-edge width when a 650 nm profile and the reference (the first requested) profile both
  // carry edge readings: the red edge minus the reference edge — the C09 anchor's quantity. Any
  // other wavelength pairing is the profiles' own readings, unpicked here.
  bool has_red_edge_width = false;
  double red_edge_width_deg = 0.0;
};


}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_PROBE_RESULT_HPP_
