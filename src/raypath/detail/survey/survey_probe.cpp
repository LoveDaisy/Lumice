#include "raypath/detail/survey/survey_probe.hpp"

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

std::vector<ProbeLineProfile> EvaluateProbeLine(
    const analytic::FaceNormalTable& table, const analytic::FacePolygonTable& polygons,
    const std::vector<int>& representative, const PoseDensityConversion& density, const ProbeFrame& frame,
    const ProbeLineLayout& layout, const std::vector<double>& wavelengths_nm,
    const std::vector<double>& refractive_indices, const double incident[3], int sample_count) {
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
  std::vector<ProbeLineProfile> profiles(wavelengths_nm.size());
  for (size_t w = 0; w < wavelengths_nm.size(); w++) {
    ProbeLineProfile& profile = profiles[w];
    profile.wavelength_nm = wavelengths_nm[w];
    profile.refractive_index = refractive_indices[w];
    const analytic::BandSumOutput band =
        analytic::BandSum(table, polygons, slots.data(), static_cast<int>(slots.size()), refractive_indices[w],
                          incident, sample_count, pixels, pose_density);
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
  return profiles;
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

void RunSolarRadialProbe(const ProbeInput& input, SurveyDocument* out) {
  const ProbeFrame frame = MakeProbeFrame(input.incident);

  const ProbeLineLayout layout = AxisLineLayout(kProbeDeltaMinDeg, kProbeDeltaMaxDeg, input.azimuth_deg);

  const std::vector<ProbeLineProfile> profiles =
      EvaluateProbeLine(*input.table, *input.polygons, *input.representative, *input.density, frame, layout,
                        { input.wavelength_nm }, { input.refractive_index }, input.incident, input.sample_count);

  SurveyProbe probe;
  probe.spec.delta_min_deg = kProbeDeltaMinDeg;
  probe.spec.delta_max_deg = kProbeDeltaMaxDeg;
  probe.spec.bin_deg = kProbeBinDeg;
  probe.spec.wedge_half_width_deg = kProbeWedgeHalfWidthDeg;
  probe.spec.wavelength_nm = input.wavelength_nm;
  probe.spec.refractive_index = input.refractive_index;
  probe.spec.sample_count = input.sample_count;
  probe.spec.lattice = "antipodal_fibonacci";
  probe.spec.deterministic = true;
  probe.spec.representative = *input.representative;
  probe.spec.member_count = input.member_count;
  probe.evidence.state = EvidenceState::kFinite;
  probe.evidence.coverage.members_evaluated = 1;  // the representative; the class reads xN
  probe.evidence.coverage.wavelength_nm = input.wavelength_nm;
  probe.samples.resize(profiles.front().samples.size());
  std::vector<double> sample_values(profiles.front().samples.size(), 0.0);
  std::vector<int> sample_counts(profiles.front().samples.size(), 0);
  for (size_t k = 0; k < profiles.front().samples.size(); k++) {
    SurveyProbeSample& sample = probe.samples[k];
    sample.delta_deg = profiles.front().samples[k].delta_deg;
    sample.value = profiles.front().samples[k].value;
    sample.k = profiles.front().samples[k].k;
    sample.k_eff = profiles.front().samples[k].k_eff;
    sample_values[k] = sample.value;
    sample_counts[k] = sample.k;
  }
  probe.kept_count = profiles.front().kept_count;
  std::vector<double> centres(probe.samples.size());
  for (size_t k = 0; k < probe.samples.size(); k++) {
    centres[k] = probe.samples[k].delta_deg;
  }
  const ProbeReadings readings = ReadProbeProfile(centres, sample_values, sample_counts);
  probe.has_edge = readings.has_edge;
  probe.edge_bin = readings.edge_bin;
  probe.edge_delta_deg = readings.edge_delta_deg;
  probe.has_peak = readings.has_peak;
  probe.peak_delta_deg = readings.peak_delta_deg;
  probe.peak_value = readings.peak_value;
  probe.separation_deg = readings.separation_deg;
  out->probes.push_back(std::move(probe));

  for (SurveyObject& card : out->objects) {
    if (card.kind != SurveyObjectKind::kBrightDomain || !card.bright_domain.has_value()) {
      continue;
    }
    if (input.fill_member != nullptr && (card.members.size() != 1u || card.members.front() != *input.fill_member)) {
      continue;
    }
    card.bright_domain->has_peak = readings.has_peak;
    card.bright_domain->peak_delta_deg = readings.peak_delta_deg;
    card.bright_domain->peak_value = readings.peak_value;
    card.bright_domain->has_edge_reading = readings.has_edge;
    card.bright_domain->edge_delta_deg = readings.edge_delta_deg;
    card.bright_domain->kept_count = profiles.front().kept_count;
    card.bright_domain->probe_index = static_cast<int>(out->probes.size()) - 1;
  }
}

}  // namespace lumice::raypath::survey
