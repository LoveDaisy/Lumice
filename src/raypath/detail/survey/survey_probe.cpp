#include "raypath/detail/survey/survey_probe.hpp"

namespace lumice::raypath::survey {

void RunSolarRadialProbe(const ProbeInput& input, SurveyData* out) {
  const ProbeFrame frame = MakeProbeFrame(input.incident);

  const ProbeLineLayout layout = AxisLineLayout(kProbeDeltaMinDeg, kProbeDeltaMaxDeg, input.azimuth_deg);

  const ProbeLineProfile line =
      EvaluateProbeLine(*input.table, *input.polygons, *input.representative, *input.density, frame, layout,
                        input.wavelength_nm, input.refractive_index, input.incident, input.sample_count);

  SurveyProbe probe;
  probe.spec.delta_min_deg = kProbeDeltaMinDeg;
  probe.spec.delta_max_deg = kProbeDeltaMaxDeg;
  probe.spec.bin_deg = kProbeBinDeg;
  probe.spec.wedge_half_width_deg = kProbeWedgeHalfWidthDeg;
  probe.spec.azimuth_deg = input.azimuth_deg;
  probe.spec.wavelength_nm = input.wavelength_nm;
  probe.spec.refractive_index = input.refractive_index;
  probe.spec.sample_count = input.sample_count;
  probe.spec.lattice = "antipodal_fibonacci";
  probe.spec.deterministic = true;
  probe.spec.representative = *input.representative;
  probe.spec.member_count = input.member_count;
  probe.SetState(EvidenceState::kFinite);
  probe.Coverage().members_evaluated = 1;  // the representative; the class reads xN
  probe.Coverage().wavelength_nm = input.wavelength_nm;
  SurveyProbeReading& result = probe.Value();
  result.samples.resize(line.samples.size());
  std::vector<double> sample_values(line.samples.size(), 0.0);
  std::vector<int> sample_counts(line.samples.size(), 0);
  for (size_t k = 0; k < line.samples.size(); k++) {
    SurveyProbeSample& sample = result.samples[k];
    sample.delta_deg = line.samples[k].delta_deg;
    sample.value = line.samples[k].value;
    sample.k = line.samples[k].k;
    sample.k_eff = line.samples[k].k_eff;
    sample_values[k] = sample.value;
    sample_counts[k] = sample.k;
  }
  result.kept_count = line.kept_count;
  std::vector<double> centres(result.samples.size());
  for (size_t k = 0; k < result.samples.size(); k++) {
    centres[k] = result.samples[k].delta_deg;
  }
  const ProbeReadings readings = ReadProbeProfile(centres, sample_values, sample_counts);
  result.has_edge = readings.has_edge;
  result.edge_bin = readings.edge_bin;
  result.edge_delta_deg = readings.edge_delta_deg;
  result.has_peak = readings.has_peak;
  result.peak_delta_deg = readings.peak_delta_deg;
  result.peak_value = readings.peak_value;
  result.separation_deg = readings.separation_deg;
  out->probes.push_back(std::move(probe));

  for (SurveyObject& card : out->objects) {
    if (card.Kind() != SurveyObjectKind::kBrightDomain || card.Bright() == nullptr) {
      continue;
    }
    if (input.fill_member != nullptr && (card.members.size() != 1u || card.members.front() != *input.fill_member)) {
      continue;
    }
    card.Bright()->has_peak = readings.has_peak;
    card.Bright()->peak_delta_deg = readings.peak_delta_deg;
    card.Bright()->peak_value = readings.peak_value;
    card.Bright()->has_edge_reading = readings.has_edge;
    card.Bright()->edge_delta_deg = readings.edge_delta_deg;
    card.Bright()->kept_count = line.kept_count;
    card.Bright()->probe_index = static_cast<int>(out->probes.size()) - 1;
  }
}

}  // namespace lumice::raypath::survey
