#include "raypath/detail/survey/survey_probe.hpp"

#include <cmath>
#include <limits>

#include "analytic/pose_density.hpp"
#include "core/math.hpp"

namespace lumice::raypath::survey {
namespace {

// Unit direction at deviation `delta` from `incident` and azimuth `az` about it, in the frame
// (e1, e2) normal to the incident (the band-sum pixel convention: directions measured about
// the incident propagation vector).
void DirectionAbout(const double incident[3], const double e1[3], const double e2[3], double delta_rad, double az_rad,
                    double out[3]) {
  const double sin_d = std::sin(delta_rad);
  const double cos_d = std::cos(delta_rad);
  for (int k = 0; k < 3; k++) {
    out[k] = cos_d * incident[k] + sin_d * (std::cos(az_rad) * e1[k] + std::sin(az_rad) * e2[k]);
  }
}

}  // namespace

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

void RunSolarRadialProbe(const ProbeInput& input, SurveyDocument* out) {
  // The probe's frame: e1/e2 orthonormal, both normal to the incident.
  double e1[3] = { 0.0, 0.0, 1.0 };
  double dot = 0.0;
  for (int k = 0; k < 3; k++) {
    dot += e1[k] * input.incident[k];
  }
  if (std::fabs(dot) > 0.9) {  // the incident nearly parallel the fallback axis: pick another
    e1[0] = 1.0;
    e1[1] = 0.0;
    e1[2] = 0.0;
    dot = input.incident[0];
  }
  double norm = 0.0;
  for (int k = 0; k < 3; k++) {
    e1[k] -= dot * input.incident[k];
    norm += e1[k] * e1[k];
  }
  norm = std::sqrt(norm);
  for (int k = 0; k < 3; k++) {
    e1[k] /= norm;
  }
  double e2[3];
  e2[0] = input.incident[1] * e1[2] - input.incident[2] * e1[1];
  e2[1] = input.incident[2] * e1[0] - input.incident[0] * e1[2];
  e2[2] = input.incident[0] * e1[1] - input.incident[1] * e1[0];

  const int bins = static_cast<int>((kProbeDeltaMaxDeg - kProbeDeltaMinDeg) / kProbeBinDeg);
  const double azimuth = 0.0;  // the representative wedge's azimuth (the value is azimuth-free)
  const double half_wedge = 0.5 * kProbeWedgeHalfWidthDeg * math::kDegreeToRadD;

  std::vector<double> centres(bins);
  std::vector<double> centre_dirs(3 * static_cast<size_t>(bins));
  std::vector<double> corner_dirs(12 * static_cast<size_t>(bins));
  std::vector<double> solid_angles(static_cast<size_t>(bins));
  for (int k = 0; k < bins; k++) {
    const double lo = kProbeDeltaMinDeg + static_cast<double>(k) * kProbeBinDeg;
    const double centre = lo + 0.5 * kProbeBinDeg;
    centres[k] = centre;
    DirectionAbout(input.incident, e1, e2, centre * math::kDegreeToRadD, azimuth,
                   centre_dirs.data() + 3 * static_cast<size_t>(k));
    double* corners = corner_dirs.data() + 12 * static_cast<size_t>(k);
    DirectionAbout(input.incident, e1, e2, lo * math::kDegreeToRadD, azimuth - half_wedge, corners);
    DirectionAbout(input.incident, e1, e2, lo * math::kDegreeToRadD, azimuth + half_wedge, corners + 3);
    DirectionAbout(input.incident, e1, e2, (lo + kProbeBinDeg) * math::kDegreeToRadD, azimuth + half_wedge,
                   corners + 6);
    DirectionAbout(input.incident, e1, e2, (lo + kProbeBinDeg) * math::kDegreeToRadD, azimuth - half_wedge,
                   corners + 9);
    solid_angles[k] = kProbeBinDeg * math::kDegreeToRadD * 2.0 * half_wedge * std::sin(centre * math::kDegreeToRadD);
  }

  std::vector<int> slots;
  // The representative's slots resolve whenever the assembly's did (the T1 arm already ran it).
  for (size_t i = 0; i < input.representative->size(); i++) {
    slots.push_back(input.table->SlotOf((*input.representative)[i]));
  }
  analytic::PixelTable table;
  table.count = bins;
  table.centre = centre_dirs.data();
  table.corners = corner_dirs.data();
  table.solid_angle = solid_angles.data();

  const analytic::PoseDensity density(input.density->spec);  // the caller's gated conversion —
                                                             // the probe builds no spec of its own
  const analytic::BandSumOutput band =
      analytic::BandSum(*input.table, *input.polygons, slots.data(), static_cast<int>(slots.size()),
                        input.refractive_index, input.incident, input.sample_count, table, density);

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
  probe.samples.resize(static_cast<size_t>(bins));
  std::vector<double> sample_values(static_cast<size_t>(bins), 0.0);
  std::vector<int> sample_counts(static_cast<size_t>(bins), 0);
  for (int k = 0; k < bins; k++) {
    SurveyProbeSample& sample = probe.samples[static_cast<size_t>(k)];
    sample.delta_deg = centres[k];
    sample.value = band.pixels[static_cast<size_t>(k)].value;
    sample.k = band.pixels[static_cast<size_t>(k)].k;
    sample.k_eff = band.pixels[static_cast<size_t>(k)].k_eff;
    sample_values[static_cast<size_t>(k)] = sample.value;
    sample_counts[static_cast<size_t>(k)] = sample.k;
  }
  probe.kept_count = band.kept_count;
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
    card.bright_domain->has_peak = readings.has_peak;
    card.bright_domain->peak_delta_deg = readings.peak_delta_deg;
    card.bright_domain->peak_value = readings.peak_value;
    card.bright_domain->has_edge_reading = readings.has_edge;
    card.bright_domain->edge_delta_deg = readings.edge_delta_deg;
    card.bright_domain->kept_count = band.kept_count;
    card.bright_domain->probe_index = static_cast<int>(out->probes.size()) - 1;
  }
}

}  // namespace lumice::raypath::survey
