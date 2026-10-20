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
  int member_count = 0;                     // the class size the representative stands for
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

// Runs the band-sum probe and appends it to out->probes; fills the profile fields of the
// bright_domain card when the document carries one. Never refuses: a path with no kept event
// yields all-zero pixels and the honest empty readings.
void RunSolarRadialProbe(const ProbeInput& input, SurveyDocument* out);

}  // namespace lumice::raypath::survey

#endif  // RAYPATH_DETAIL_SURVEY_PROBE_H_
