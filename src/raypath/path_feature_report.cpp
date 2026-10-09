#include "raypath/path_feature_report.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "analytic/so3.hpp"
#include "raypath/detail/path_feature_report.hpp"
#include "raypath/detail/path_feature_report_json.hpp"
#include "raypath/detail/schema3/report_assembly.hpp"
#include "util/logger.hpp"

namespace lumice::raypath {
Error AssemblePathFeatureReport(const ConfigManager& config, const PathFeatureReportRequest& request,
                                PathFeatureReport* out) {
  if (!out) {
    return { ErrorCode::kInvalidArgument, "null report output" };
  }
  *out = {};
  const auto begin = std::chrono::steady_clock::now();
  if (request.path_layers.empty() ||
      !std::all_of(request.path_layers.begin(), request.path_layers.end(), [](const auto& layer) {
        return layer.size() >= 2 && layer.size() <= 64 &&
               std::all_of(layer.begin(), layer.end(), [](int face) { return face > 0; });
      })) {
    return { ErrorCode::kInvalidPath, "non-empty layers with 2..64 positive face numbers are required" };
  }
  if ((request.sample_count != 0 && (request.sample_count < 64 || request.sample_count > kMaxFeatureReportSampleCount ||
                                     request.sample_count % 2)) ||
      request.budget_ms <= 0 || request.budget_ms > kMaxFeatureReportBudgetMs || request.max_optical_evaluations == 0 ||
      request.max_optical_evaluations > kMaxFeatureReportSampleEvaluations || request.max_field_evaluations == 0 ||
      request.max_field_evaluations > 1000000000 || !(request.bandwidth_rad > 0) || request.bandwidth_rad >= 1 ||
      !std::isfinite(request.bandwidth_rad) || !(request.location_resolution_rad > 0) ||
      !std::isfinite(request.location_resolution_rad) || request.symmetry_bits > 7) {
    return { ErrorCode::kInvalidArgument, "invalid report sample count, observation or bounded work/deadline request" };
  }
  if (request.wavelengths_nm.size() > kMaxFeatureReportWavelengthCount ||
      (!request.wavelength_weights.empty() && request.wavelength_weights.size() != request.wavelengths_nm.size())) {
    return { ErrorCode::kInvalidArgument, "diagnostic wavelength weights must match at most 32 wavelengths" };
  }
  SceneConfig scene = config.scene_;
  bool diagnostic_spectrum = !request.wavelengths_nm.empty();
  if (diagnostic_spectrum) {
    std::vector<WlParam> spectrum;
    for (size_t i = 0; i < request.wavelengths_nm.size(); ++i) {
      const double wavelength = request.wavelengths_nm[i];
      const double weight = request.wavelength_weights.empty() ? 1 : request.wavelength_weights[i];
      if (!std::isfinite(wavelength) || wavelength < 350 || wavelength > 900 || !std::isfinite(weight) || weight < 0 ||
          weight > std::numeric_limits<float>::max()) {
        return { ErrorCode::kWavelengthOutOfRange,
                 "diagnostic wavelengths must be in [350,900] nm with finite nonnegative weights" };
      }
      spectrum.push_back({ static_cast<float>(wavelength), static_cast<float>(weight) });
    }
    scene.light_source_.spectrum_ = std::move(spectrum);
  }
  const auto configured = config.crystals_.find(request.crystal_id);
  if (configured == config.crystals_.end()) {
    return { ErrorCode::kUnknownCrystalId, "unknown crystal id" };
  }
  size_t layer_index = request.scene_layer.value_or(scene.ms_.size());
  if (!request.scene_layer) {
    for (size_t i = 0; i < scene.ms_.size(); ++i) {
      if (std::any_of(scene.ms_[i].setting_.begin(), scene.ms_[i].setting_.end(),
                      [&](const auto& s) { return s.crystal_.id_ == request.crystal_id; })) {
        layer_index = i;
        break;
      }
    }
    // A configured but unused crystal is still a valid standalone diagnostic
    // object. Keep this explicit in the snapshot identity, not as a fake layer.
    if (layer_index == scene.ms_.size()) {
      ScatteringSetting setting{};
      setting.crystal_ = configured->second;
      setting.crystal_proportion_ = 1;
      scene.ms_.push_back({ 0, { setting } });
    }
  }
  SpectrumRequest spectrum = DiscreteSpectrumSum{};
  if (std::holds_alternative<IlluminantType>(scene.light_source_.spectrum_)) {
    SpectrumQuadrature quadrature;
    quadrature.rule = "dyadic full-band trapezoid";
    quadrature.evaluation_budget = 33;
    for (int j = 0; j <= 32; ++j) {
      quadrature.nodes.push_back(
          { std::min(380.f + 400.f * j / 32, std::nextafter(780.f, 380.f)), (j == 0 || j == 32 ? .5 : 1.) / 32 });
    }
    spectrum = std::move(quadrature);
  }
  const std::vector<LayerSelection> selection{ { layer_index, request.crystal_id, request.path_layers[0],
                                                 request.symmetry_bits } };
  InputSnapshot snapshot;
  const std::string identity = layer_index >= config.scene_.ms_.size() ?
                                   "standalone configured crystal (not a scene allocation)" :
                                   "selected scene entry snapshot";
  auto error = CaptureInput(scene, identity, selection, &snapshot);
  if (!error.Ok()) {
    return error;
  }
  // Validate the selected input without realizing a path or doing optical work,
  // even when the requested chain is outside the supported single-crystal scope.
  if (request.path_layers.size() > 1) {
    AssembledSpectrum validated_spectrum;
    error = std::holds_alternative<DiscreteSpectrumSum>(spectrum) ?
                AssembleDiscreteSpectrum(scene.light_source_, &validated_spectrum) :
                AssembleSpectrumQuadrature(scene.light_source_, std::get<SpectrumQuadrature>(spectrum),
                                           &validated_spectrum);
    if (!error.Ok()) {
      return error;
    }
    out->unsupported_multicrystal = true;
    out->requested_path_layers = request.path_layers;
    return {};
  }
  ILOG_INFO(GetGlobalLogger(), "[raypath report] input snapshot captured; resolving member/spectrum work");
  DiagnosticSampler sampler(snapshot, 1497, spectrum);
  AssembledInput representative;
  error = sampler.Draw(0, &representative);
  if (!error.Ok()) {
    return error;
  }
  uint64_t samples = request.sample_count;
  if (samples == 0) {
    const uint64_t spectra = representative.spectrum.rows.size();
    const uint64_t members = representative.layers[0].scope.members.size();
    const uint64_t per_outer =
        members * (2 * spectra + (std::holds_alternative<SpectrumQuadrature>(spectrum) ? 2 * spectra - 1 : 0));
    const uint64_t affordable = per_outer ? (request.max_optical_evaluations * 3 / 4) / per_outer : 0;
    samples = 64;
    while (samples < kDefaultFeatureReportSampleCount && samples * 2 <= affordable) {
      samples *= 2;
    }
  }
  DiscoveryOptions options{ { samples, request.max_optical_evaluations,
                              request.deadline.value_or(begin + std::chrono::milliseconds(request.budget_ms)) },
                            request.bandwidth_rad,
                            request.location_resolution_rad,
                            request.max_field_evaluations,
                            8,
                            8,
                            4,
                            6 };
  error = BuildPathFeatureReport(scene, identity, selection, spectrum, 1497, options, out);
  if (!error.Ok()) {
    return error;
  }
  out->standalone_crystal = layer_index >= config.scene_.ms_.size();
  out->requested_outer_samples = request.sample_count;
  out->budget_ms = request.budget_ms;
  out->capture_seconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count() -
                          out->capture_seconds - out->discovery.assembly_seconds - out->discovery.event_seconds -
                          out->discovery.field_seconds - out->spectral_seconds;
  if (diagnostic_spectrum) {
    out->spectrum_scope = "explicit diagnostic spectrum override; not the scene SPD";
  }
  return {};
}

Error AnalyzePathFeatureReport(const ConfigManager& config, const PathFeatureReportRequest& request,
                               const std::string& product_version, std::string* json_out) {
  if (!json_out) {
    return { ErrorCode::kInvalidArgument, "null report output" };
  }
  PathFeatureReport report;
  if (const Error error = AssemblePathFeatureReport(config, request, &report); !error.Ok()) {
    return error;
  }
  // The multicrystal early path never ran the discovery; the assembly's precondition excludes
  // it and the v3 early shape ignores the assembled side. The enumeration leg's anti-hang
  // deadline starts here: the enforced robustness floor (kEnumerationHangCapMs), independent
  // of the request's quality budget (--budget-ms bounds the discovery legs, report-only by
  // A4 for the enumeration's own cost vocabulary).
  const schema3::AssembledSchema3Report assembled =
      report.unsupported_multicrystal ?
          schema3::AssembledSchema3Report{} :
          schema3::AssembleSchema3Report(
              std::move(report), request.max_optical_evaluations,
              std::chrono::steady_clock::now() + std::chrono::milliseconds(schema3::kEnumerationHangCapMs));
  *json_out = PathFeatureReportV3ToJson(report, assembled, product_version.c_str());
  return {};
}

Error BuildPathFeatureReport(const SceneConfig& scene, const std::string& identity,
                             const std::vector<LayerSelection>& selection, const SpectrumRequest& spectrum,
                             uint32_t seed, const DiscoveryOptions& options, PathFeatureReport* out) {
  if (!out) {
    return { ErrorCode::kInvalidArgument, "null report output" };
  }
  *out = {};
  const auto begin = std::chrono::steady_clock::now();
  if (selection.size() != 1) {
    return { ErrorCode::kMultiLayerUnsupported,
             "multi-crystal diagnostic chains are unsupported; select one complete single-crystal path" };
  }
  PathFeatureReport result;
  auto error = CaptureInput(scene, identity, selection, &result.snapshot);
  if (!error.Ok()) {
    return error;
  }
  result.options = options;
  result.seed = seed;
  result.spectrum_scope =
      std::holds_alternative<DiscreteSpectrumSum>(spectrum) ? "scene discrete spectrum, exact sum" :
      std::holds_alternative<SpectrumQuadrature>(spectrum)  ? "scene continuous spectrum, declared quadrature" :
                                                              "explicit diagnostic wavelength";
  const DiagnosticSampler sampler(result.snapshot, seed, spectrum);
  error = sampler.Draw(0, &result.representative_input);
  if (!error.Ok()) {
    return error;
  }
  result.capture_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
  ILOG_INFO(GetGlobalLogger(), "[raypath report] assembling measure and bounded local/source discovery");
  error = DiscoverFeatures(sampler, options, &result.discovery);
  if (!error.Ok()) {
    return error;
  }
  if (std::holds_alternative<SpectrumQuadrature>(spectrum)) {
    // Refine the actual full-band measure, not a diagnostic RGB triple. Keep
    // the same outer draws, kernel and levels when measuring spectral movement.
    // Arbitrary external quadratures have no implicit refinement rule here.
    ILOG_INFO(GetGlobalLogger(), "[raypath report] verifying continuous-spectrum quadrature");
    const auto spectral_start = std::chrono::steady_clock::now();
    const auto& base = std::get<SpectrumQuadrature>(spectrum);
    bool refinement_available = base.rule == "dyadic full-band trapezoid" && base.nodes.size() >= 3;
    if (refinement_available) {
      const size_t intervals = base.nodes.size() - 1;
      refinement_available = (intervals & (intervals - 1)) == 0 && intervals <= 256;
      for (size_t j = 0; j <= intervals; ++j) {
        refinement_available &=
            base.nodes[j].wavelength_nm == std::min(380.f + 400.f * j / intervals, std::nextafter(780.f, 380.f));
        refinement_available &= base.nodes[j].probability_mass == (j == 0 || j == intervals ? .5 : 1.) / intervals;
      }
    }
    DiagnosticMeasure refined;
    if (refinement_available) {
      SpectrumQuadrature next;
      const size_t intervals = 2 * (base.nodes.size() - 1);
      next.rule = base.rule;
      next.evaluation_budget = intervals + 1;
      for (size_t j = 0; j <= intervals; ++j) {
        next.nodes.push_back({ std::min(380.f + 400.f * j / intervals, std::nextafter(780.f, 380.f)),
                               (j == 0 || j == intervals ? .5 : 1.0) / intervals });
      }
      const DiagnosticSampler verifier(result.snapshot, seed, next);
      auto remaining = options.sampling;
      const auto used = result.discovery.measure.optical_evaluations + result.discovery.replicate_path_evaluations +
                        result.discovery.event_path_evaluations;
      remaining.max_optical_evaluations -= std::min(remaining.max_optical_evaluations, used);
      remaining.requested_samples = result.discovery.measure.completed_samples;
      if (remaining.requested_samples > 0) {
        error = BuildDiagnosticMeasure(verifier, remaining, &refined);
        refinement_available = error.Ok() && refined.completed_samples == remaining.requested_samples;
      } else {
        refinement_available = false;
      }
      result.spectral_optical_evaluations = refined.optical_evaluations;
      result.discovery.budget_exhausted |= refined.budget_exhausted;
    }
    analytic::FieldWorkBudget verification_budget{ options.max_field_evaluations -
                                                       std::min(options.max_field_evaluations,
                                                                result.discovery.field_component_evaluations),
                                                   0, options.sampling.deadline };
    if (refinement_available) {
      result.spectral_movement_rad = 0;
    }
    for (auto& feature : result.discovery.features) {
      if (feature.evidence != DiagnosticEvidence::kActual) {
        continue;
      }
      bool stable = refinement_available && !feature.field_points.empty();
      std::vector<std::pair<const analytic::FieldStationaryPoint*, double>> points;
      for (const auto& point : feature.field_points) {
        points.push_back({ &point, feature.level });
      }
      if (feature.band) {
        for (int side = 0; side < 2; ++side) {
          for (const auto& point : feature.band->boundaries[side]) {
            points.push_back({ &point, feature.band->levels[side] });
          }
        }
      }
      for (const auto& entry : points) {
        const auto& point = *entry.first;
        if (!stable) {
          break;
        }
        const auto corrected = analytic::CorrectSphericalField(
            refined.components, point.query.direction,
            { feature.equation, entry.second, feature.bandwidth_rad, 1e-8, feature.bandwidth_rad * .5, 32 },
            &verification_budget);
        if (corrected.status != analytic::FieldSolveStatus::kConverged) {
          stable = false;
          break;
        }
        double cross[3];
        analytic::so3::Cross3(corrected.query.direction.data(), point.query.direction.data(), cross);
        const double movement =
            std::atan2(analytic::so3::Norm3(cross),
                       analytic::so3::Dot3(corrected.query.direction.data(), point.query.direction.data()));
        *result.spectral_movement_rad = std::max(*result.spectral_movement_rad, movement);
        stable &= movement <= options.location_resolution_rad;
      }
      if (!stable) {
        feature.evidence = DiagnosticEvidence::kUnfinished;
        feature.reason += "; continuous spectral quadrature accuracy not established for this geometry";
      }
    }
    result.spectral_field_evaluations = verification_budget.component_evaluations;
    result.spectral_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - spectral_start).count();
    result.discovery.budget_exhausted |= verification_budget.exhausted;
    if (!refinement_available) {
      result.discovery.unfinished.push_back("continuous spectral quadrature refinement incomplete");
    }
  }
  result.no_related_signal = ZeroSpectralSignal(result.snapshot, result.representative_input);
  *out = std::move(result);
  return {};
}

}  // namespace lumice::raypath
