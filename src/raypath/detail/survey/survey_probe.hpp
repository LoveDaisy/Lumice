#ifndef RAYPATH_DETAIL_SURVEY_PROBE_H_
#define RAYPATH_DETAIL_SURVEY_PROBE_H_

// The canonical solar-radial probe (routing.md: a full-dimensional class's固有方向 is the
// radial one — the delta axis IS the radial profile about the sun). The band-sum map's radial
// slice: 1800 narrow azimuthal wedges, one per 0.1-degree deviation bin over the whole delta
// axis [0, 180], fed to analytic::BandSum at the document's reference wavelength and index —
// the same ruler the support's lower end and the closed-form edge anchor read, so the probe's
// edge reading and the certified edge position are directly comparable. Under the uniform
// (random) density the band-sum value formula makes each wedge's value the full annulus's
// per-sr mean, independent of the wedge's azimuth, so a single representative wedge carries
// the radial profile.
//
// The two readings stay two things (the corpus lesson): the edge reading is where events start
// (geometrically no event lies below the minimum-deviation edge, so the first non-empty bin
// contains it — a finite reading good to half a bin), the peak reading is the value argmax.
// The certified edge position lives on the caustic_edge card; both probe readings are finite.

#include <vector>

#include "analytic/band_sum.hpp"
#include "analytic/pose_density.hpp"
#include "raypath/detail/survey/survey_document.hpp"
#include "raypath/scene_to_analytic.hpp"

namespace lumice::raypath::survey {

// The probe's caliber constants (plan A3; they are schema, not tuning: the serialized spec
// echoes them and the assertion bench reads them back). The window is the whole delta axis:
// the corpus's C06/C09 supports sit outside the original [15, 60] slice, and a support-derived
// window would make the bin layout a per-class branch — one ruler for every class instead.
inline constexpr double kProbeDeltaMinDeg = 0.0;
inline constexpr double kProbeDeltaMaxDeg = 180.0;
inline constexpr double kProbeBinDeg = 0.1;
inline constexpr double kProbeWedgeHalfWidthDeg = 1.0;
inline constexpr int kSurveyProbeSampleCount = 2000000;  // the band_sum lattice size (its `n`)

struct ProbeInput {
  const analytic::FaceNormalTable* table = nullptr;
  const analytic::FacePolygonTable* polygons = nullptr;
  const std::vector<int>* representative = nullptr;  // the literal face sequence probed
  const PoseDensityConversion* density = nullptr;    // the caller's gated conversion (the same
                                                     // one the routing arm checked Ok(); the
                                                     // probe never builds a spec of its own)
  double refractive_index = 0.0;
  double wavelength_nm = 0.0;
  double incident[3] = { 0.0, 0.0, -1.0 };  // the sun's propagation direction (SunIncidentDirection)
  // The representative wedge's azimuth, degrees, in the probe frame (MakeProbeFrame). The band-sum
  // value formula makes the profile azimuth-free under the uniform density, so the built-in's 0 is
  // a representative choice, spelled here so the query face's user-chosen azimuth reads as the
  // same layout with another value (not a parallel implementation).
  double azimuth_deg = 0.0;
  int member_count = 0;  // the class size the representative stands for
  int sample_count = kSurveyProbeSampleCount;
  // When set, only the bright_domain card whose members are exactly this one member receives
  // the readings (the family arm produces one bright_domain card per class member; the probe
  // ran on the representative, and a relabeled sibling's card must not carry its numbers).
  // Null = the T1 arm's shape: every bright_domain card (one) reads the profile.
  const std::vector<int>* fill_member = nullptr;
};

// The profile reader, factored out for the synthetic-profile tests: `values_deg[k]` is bin k's
// centre deviation, `counts[k]` its event count. The edge is the first non-empty bin's centre
// (± half a bin); the peak is the argmax over non-empty bins; separation = peak - edge.
struct ProbeReadings {
  bool has_edge = false;
  int edge_bin = -1;
  double edge_delta_deg = 0.0;
  bool has_peak = false;
  double peak_delta_deg = 0.0;
  double peak_value = 0.0;
  double separation_deg = 0.0;
};

ProbeReadings ReadProbeProfile(const std::vector<double>& values_deg, const std::vector<double>& values,
                               const std::vector<int>& counts);

// ---- the shared line-probe core (the query face consumes the same implementation) --------------
//
// routing.md: the probe is a shared library — one implementation, two triggers (the survey runs the
// object's own radial line automatically, the query face lets the user place a line). The core is
// "line geometry + analytic::BandSum + the readings"; a layout is a per-bin rectangle list in the
// (deviation, azimuth-about-sun) plane. RunSolarRadialProbe below is the fixed-layout wrapper.

// The probe's frame: (e1, e2) orthonormal, both normal to the incident (the band-sum pixel
// convention: directions measured about the incident propagation vector), with the fallback axis
// and cross-product order fixed here so every probe line reads in one frame.
struct ProbeFrame {
  double incident[3] = { 0.0, 0.0, -1.0 };
  double e1[3] = { 0.0, 0.0, 1.0 };
  double e2[3] = { 0.0, 0.0, 0.0 };
};

ProbeFrame MakeProbeFrame(const double incident[3]);

// Unit direction at deviation `delta_rad` from the frame's incident and azimuth `az_rad` about it.
void ProbeDirection(const ProbeFrame& frame, double delta_rad, double az_rad, double out[3]);

// One evaluated bin of a probe line, both coordinates spelled (the profile document's row).
struct ProbeLineSample {
  double delta_deg = 0.0;    // the bin centre's deviation from the sun
  double azimuth_deg = 0.0;  // the bin centre's azimuth about the sun, in the probe frame
  double value = 0.0;        // per sr (the band-sum value convention)
  int k = 0;
  double k_eff = 0.0;
};

// One wavelength's evaluated profile.
struct ProbeLineProfile {
  double wavelength_nm = 0.0;
  double refractive_index = 0.0;
  long long kept_count = 0;
  std::vector<ProbeLineSample> samples;
};

// One bin's rectangle in the (deviation, azimuth) plane, degrees. The corners are read lo-to-hi
// in the fixed order (delta_lo, az_lo) -> (delta_lo, az_hi) -> (delta_hi, az_hi) -> (delta_hi,
// az_lo), the winding the built-in layout has always written.
struct ProbeLineBin {
  double delta_centre_deg = 0.0;
  double delta_lo_deg = 0.0;
  double delta_hi_deg = 0.0;
  double az_centre_deg = 0.0;
  double az_lo_deg = 0.0;
  double az_hi_deg = 0.0;
};

// A line's layout: the bins plus the two uniform widths in radians, precomputed by the layout
// constructor with its own expression (the solid angle is delta_width_rad * az_width_rad *
// sin(centre deviation) — the band-sum pixel convention; the widths are per-line constants, so
// the radians conversion happens once, with the same grouping the built-in layout has always used).
struct ProbeLineLayout {
  std::vector<ProbeLineBin> bins;
  double delta_width_rad = 0.0;
  double az_width_rad = 0.0;
};

// The axis forms' layout constructor, the one statement of the delta-axis bin layout: the ruler
// over [delta_min_deg, delta_max_deg] at the line's azimuth. The built-in radial line and the
// query's radial / edge-normal forms all read this one constructor, so a warm document's grid and
// the asked grid match bit for bit by construction, not by a runtime check.
ProbeLineLayout AxisLineLayout(double delta_min_deg, double delta_max_deg, double azimuth_deg);

// The shared core: resolves the representative's slots, materializes each bin's PixelTable cell
// in the frame, and evaluates analytic::BandSum once for the one wavelength/index pair (the
// chromatic lambda grid lives on the caller's loop, never a multi-wavelength map). Never
// refuses: a path with no kept event yields the honest all-zero profile.
ProbeLineProfile EvaluateProbeLine(const analytic::FaceNormalTable& table, const analytic::FacePolygonTable& polygons,
                                   const std::vector<int>& representative, const PoseDensityConversion& density,
                                   const ProbeFrame& frame, const ProbeLineLayout& layout, double wavelength_nm,
                                   double refractive_index, const double incident[3], int sample_count);

// Runs the band-sum probe and appends it to out->probes; fills the profile fields of the
// bright_domain card when the document carries one. Never refuses: a path with no kept event
// yields all-zero pixels and the honest empty readings.
void RunSolarRadialProbe(const ProbeInput& input, SurveyDocument* out);

}  // namespace lumice::raypath::survey

#endif  // RAYPATH_DETAIL_SURVEY_PROBE_H_
