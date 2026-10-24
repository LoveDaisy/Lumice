#ifndef LUMICE_RAYPATH_RESULTS_PROBE_LINE_HPP_
#define LUMICE_RAYPATH_RESULTS_PROBE_LINE_HPP_

#include <vector>
namespace lumice::raypath::survey {

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


}  // namespace lumice::raypath::survey

#endif  // LUMICE_RAYPATH_RESULTS_PROBE_LINE_HPP_
