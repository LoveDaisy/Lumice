#ifndef RAYPATH_DETAIL_SURVEY_PROBE_H_
#define RAYPATH_DETAIL_SURVEY_PROBE_H_

#include "raypath/detail/quantity/probe_line.hpp"
#include "raypath/survey_result.hpp"

namespace lumice::raypath::survey {

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

void RunSolarRadialProbe(const ProbeInput& input, SurveyResult* out);

}  // namespace lumice::raypath::survey
#endif
