#include "raypath/detail/quantity/probe_line.hpp"

#include <cmath>
#include <limits>

#include "analytic/pose_density.hpp"
#include "core/math.hpp"

namespace lumice::raypath::survey {

ProbeFrame MakeProbeFrame(const double incident[3]) {
  ProbeFrame frame;
  for (int k = 0; k < 3; k++) {
    frame.incident[k] = incident[k];
  }
  // e1/e2 orthonormal, both normal to the incident.
  double dot = 0.0;
  for (int k = 0; k < 3; k++) {
    dot += frame.e1[k] * incident[k];
  }
  if (std::fabs(dot) > 0.9) {  // the incident nearly parallel the fallback axis: pick another
    frame.e1[0] = 1.0;
    frame.e1[1] = 0.0;
    frame.e1[2] = 0.0;
    dot = incident[0];
  }
  double norm = 0.0;
  for (int k = 0; k < 3; k++) {
    frame.e1[k] -= dot * incident[k];
    norm += frame.e1[k] * frame.e1[k];
  }
  norm = std::sqrt(norm);
  for (int k = 0; k < 3; k++) {
    frame.e1[k] /= norm;
  }
  frame.e2[0] = incident[1] * frame.e1[2] - incident[2] * frame.e1[1];
  frame.e2[1] = incident[2] * frame.e1[0] - incident[0] * frame.e1[2];
  frame.e2[2] = incident[0] * frame.e1[1] - incident[1] * frame.e1[0];
  return frame;
}

void ProbeDirection(const ProbeFrame& frame, double delta_rad, double az_rad, double out[3]) {
  const double sin_d = std::sin(delta_rad);
  const double cos_d = std::cos(delta_rad);
  for (int k = 0; k < 3; k++) {
    out[k] = cos_d * frame.incident[k] + sin_d * (std::cos(az_rad) * frame.e1[k] + std::sin(az_rad) * frame.e2[k]);
  }
}

ProbeReadings ReadProbeProfile(const std::vector<double>& values_deg, const std::vector<double>& values,
                               const std::vector<int>& counts) {
  ProbeReadings readings;
  for (size_t k = 0; k < counts.size() && k < values_deg.size() && k < values.size(); k++) {
    if (counts[k] <= 0) {
      continue;
    }
    if (!readings.has_edge) {
      readings.has_edge = true;
      readings.edge_bin = static_cast<int>(k);
      readings.edge_delta_deg = values_deg[k];
    }
    if (!readings.has_peak || values[k] > readings.peak_value) {
      readings.has_peak = true;
      readings.peak_delta_deg = values_deg[k];
      readings.peak_value = values[k];
    }
  }
  if (readings.has_edge && readings.has_peak) {
    readings.separation_deg = readings.peak_delta_deg - readings.edge_delta_deg;
  }
  return readings;
}

ProbeLineProfile EvaluateProbeLine(const analytic::FaceNormalTable& table, const analytic::FacePolygonTable& polygons,
                                   const std::vector<int>& representative, const PoseDensityConversion& density,
                                   const ProbeFrame& frame, const ProbeLineLayout& layout, double wavelength_nm,
                                   double refractive_index, const double incident[3], int sample_count) {
  const size_t bins = layout.bins.size();
  std::vector<double> centre_dirs(3 * bins);
  std::vector<double> corner_dirs(12 * bins);
  std::vector<double> solid_angles(bins);
  for (size_t k = 0; k < bins; k++) {
    const ProbeLineBin& bin = layout.bins[k];
    ProbeDirection(frame, bin.delta_centre_deg * math::kDegreeToRadD, bin.az_centre_deg * math::kDegreeToRadD,
                   centre_dirs.data() + 3 * k);
    double* corners = corner_dirs.data() + 12 * k;
    ProbeDirection(frame, bin.delta_lo_deg * math::kDegreeToRadD, bin.az_lo_deg * math::kDegreeToRadD, corners);
    ProbeDirection(frame, bin.delta_lo_deg * math::kDegreeToRadD, bin.az_hi_deg * math::kDegreeToRadD, corners + 3);
    ProbeDirection(frame, bin.delta_hi_deg * math::kDegreeToRadD, bin.az_hi_deg * math::kDegreeToRadD, corners + 6);
    ProbeDirection(frame, bin.delta_hi_deg * math::kDegreeToRadD, bin.az_lo_deg * math::kDegreeToRadD, corners + 9);
    solid_angles[k] =
        layout.delta_width_rad * layout.az_width_rad * std::sin(bin.delta_centre_deg * math::kDegreeToRadD);
  }

  std::vector<int> slots;
  // The representative's slots resolve whenever the assembly's did (the T1 arm already ran it).
  slots.reserve(representative.size());
  for (size_t i = 0; i < representative.size(); i++) {
    slots.push_back(table.SlotOf(representative[i]));
  }
  analytic::PixelTable pixels;
  pixels.count = static_cast<int>(bins);
  pixels.centre = centre_dirs.data();
  pixels.corners = corner_dirs.data();
  pixels.solid_angle = solid_angles.data();

  const analytic::PoseDensity pose_density(density.spec);  // the caller's gated conversion —
                                                           // the probe builds no spec of its own
  ProbeLineProfile profile;
  profile.wavelength_nm = wavelength_nm;
  profile.refractive_index = refractive_index;
  const analytic::BandSumOutput band =
      analytic::BandSum(table, polygons, slots.data(), static_cast<int>(slots.size()), refractive_index, incident,
                        sample_count, pixels, pose_density);
  {
    profile.kept_count = band.kept_count;
    profile.samples.resize(bins);
    for (size_t k = 0; k < bins; k++) {
      ProbeLineSample& sample = profile.samples[k];
      sample.delta_deg = layout.bins[k].delta_centre_deg;
      sample.azimuth_deg = layout.bins[k].az_centre_deg;
      sample.value = band.pixels[k].value;
      sample.k = band.pixels[k].k;
      sample.k_eff = band.pixels[k].k_eff;
    }
  }
  return profile;
}

ProbeLineLayout AxisLineLayout(double delta_min_deg, double delta_max_deg, double azimuth_deg) {
  ProbeLineLayout layout;
  const int bins = static_cast<int>(std::lround((delta_max_deg - delta_min_deg) / kProbeBinDeg));
  const double half_wedge = 0.5 * kProbeWedgeHalfWidthDeg * math::kDegreeToRadD;
  const double half_wedge_deg = 0.5 * kProbeWedgeHalfWidthDeg;
  layout.bins.resize(static_cast<size_t>(bins));
  layout.delta_width_rad = kProbeBinDeg * math::kDegreeToRadD;
  layout.az_width_rad = 2.0 * half_wedge;
  for (int k = 0; k < bins; k++) {
    const double lo = delta_min_deg + static_cast<double>(k) * kProbeBinDeg;
    ProbeLineBin& bin = layout.bins[static_cast<size_t>(k)];
    bin.delta_centre_deg = lo + 0.5 * kProbeBinDeg;
    bin.delta_lo_deg = lo;
    bin.delta_hi_deg = lo + kProbeBinDeg;
    bin.az_centre_deg = azimuth_deg;
    bin.az_lo_deg = azimuth_deg - half_wedge_deg;
    bin.az_hi_deg = azimuth_deg + half_wedge_deg;
  }
  return layout;
}

}  // namespace lumice::raypath::survey
