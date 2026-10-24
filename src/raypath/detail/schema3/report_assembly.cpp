#include "raypath/detail/schema3/report_assembly.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <limits>

#include "raypath/scene_to_analytic.hpp"

namespace lumice::raypath::schema3 {
namespace {

// The declared orbit-stream / restricted-curve resolution (EnumerationInput's own default).
constexpr int kEnumerationGrid = 720;

}  // namespace

AssembledSchema3Report AssembleSchema3Report(PathFeatureReport&& report, uint64_t max_optical_evaluations,
                                             std::chrono::steady_clock::time_point enumeration_deadline) {
  // The header's precondition, mechanized: the early path never ran the discovery this module
  // reads, so assembling over it dereferences an empty layer (the shape the production wiring
  // hit once as SIGSEGV before the conditional-assembly discipline existed).
  assert(!report.unsupported_multicrystal);
  AssembledSchema3Report out;
  const AssembledLayer& layer = report.representative_input.layers.front();

  // The sun at the scene's solar center: SunIncidentDirection names the propagation
  // (sun -> crystal), so the at-sun direction is its negation.
  double sun_hat[3];
  SunIncidentDirection(report.snapshot.light.param_, sun_hat);
  const double sun_dir[3] = { -sun_hat[0], -sun_hat[1], -sun_hat[2] };

  // The declared measure and the density spec from the SAME axis (the input snapshot's crystal).
  const UMarginal measure = MakeUMarginal(report.snapshot.layers.front().crystal.axis_, sun_dir);
  if (measure.kind() == USupportKind::kDegenerateSunGeometry) {
    out.measure_skip_note =
        "declared measure skipped: the sun sits at a pole (kDegenerateSunGeometry); the "
        "restricted/orbit legs are absent and kind-1 visibility stays unproven";
  }
  const PoseDensityConversion density = ConvertAxisToPoseDensity(report.snapshot.layers.front().crystal.axis_);
  if (!density.Ok()) {
    out.density_skip_note = "density leg skipped: axis distribution not expressible (" + density.unsupported + ")";
  }

  EnumerationInput in;
  in.normals = &layer.normals;
  in.polygons = &layer.polygons;
  in.base_index = report.representative_input.spectrum.rows.front().refractive_index;
  for (const SpectralRow& row : report.representative_input.spectrum.rows) {
    in.wavelengths_nm.push_back(row.wavelength_nm);
    in.indices.push_back(row.refractive_index);
  }
  for (int i = 0; i < 3; i++) {
    in.sun_dir[i] = sun_dir[i];
  }
  in.measure = out.measure_skip_note.empty() ? &measure : nullptr;
  in.density = density.Ok() ? &density.spec : nullptr;
  in.grid = kEnumerationGrid;

  const auto enumeration_begin = std::chrono::steady_clock::now();
  out.core = EnumerateLayer(in, layer.scope.members, enumeration_deadline);
  out.enumeration_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - enumeration_begin).count();
  // The budget's configured cap rides the report (the request's own vocabulary); the
  // enumeration counts what it built, the kernel-internal counts stay the registered gap.
  out.core.budget.max_optical_evaluations =
      static_cast<long long>(std::min<uint64_t>(max_optical_evaluations, std::numeric_limits<long long>::max()));
  // The anti-hang cap's installed value rides the budget only when a deadline was installed
  // (the default-argument callers — unit tests, future non-CLI consumers — keep hang_cap_ms 0).
  if (enumeration_deadline != std::chrono::steady_clock::time_point::max()) {
    out.core.budget.hang_cap_ms = kEnumerationHangCapMs;
  }

  // The demoted evidence: the spectral-verification record rides (it re-measured these same
  // records under a refined spectrum). A continuous-spectrum quadrature document is the
  // structural in-scope signal; `available=false` is the declared nothing-to-verify shape.
  McSpectralVerification spectral;
  spectral.available = report.representative_input.spectrum.quadrature.has_value();
  spectral.movement_rad = report.spectral_movement_rad;
  spectral.optical_evaluations = report.spectral_optical_evaluations;
  spectral.field_component_evaluations = report.spectral_field_evaluations;
  spectral.seconds = report.spectral_seconds;
  // The production form: the discovery MOVES into the carry (the v2 document path is gone —
  // nothing reads report.discovery past this call).
  McEvidenceBlock mc = McEvidenceOf(std::move(report.discovery), report.options, spectral);

  McAttributionInput input;
  input.core = &out.core;
  input.mc = &mc;
  input.geometry.normals = in.normals;
  input.geometry.polygons = in.polygons;
  input.geometry.base_index = in.base_index;
  for (int i = 0; i < 3; i++) {
    input.geometry.sun_dir[i] = sun_dir[i];
  }
  input.h_rad = report.options.bandwidth_rad;  // the matching ruler is the observation's own bandwidth

  const auto attribution_begin = std::chrono::steady_clock::now();
  out.unattributed = DeriveUnattributed(input);
  CorroborationOutcome corroboration = DeriveCorroboration(input);
  out.attribution_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - attribution_begin).count();
  out.annotations = std::move(corroboration.annotations);
  out.corroboration_counts = corroboration.counts;
  out.core = std::move(corroboration.core);  // the corroboration-written copy is THE core
  out.mc = std::move(mc);
  // The ruling reads the final core; the support kind rides even when the measure side was
  // skipped (a non-kArea kind fails rule B's gate — the honest carrier of "no valid support").
  out.ruling = DeriveNoRelatedFeature(out.core, out.unattributed, measure.kind(), report.no_related_signal);
  return out;
}

FeatureReportData PrepareFeatureReportResult(const PathFeatureReport& report) {
  FeatureReportData result;
  result.requested_path_layers = report.requested_path_layers;
  if (report.unsupported_multicrystal) {
    result.outcome = ReportOutcome::kUnsupportedMulticrystal;
    result.coverage.push_back(
        { CoverageRole::kBaseInput,
          "multi-crystal chain",
          CoverageStatus::kNotSupported,
          "single-crystal internal reflections are supported, but this multi-crystal request was not evaluated",
          {} });
    return result;
  }
  result.scope.scene_identity = report.snapshot.scene_identity;
  result.scope.spectrum_scope = report.spectrum_scope;
  result.scope.seed = report.seed;
  result.scope.light = report.snapshot.light;
  result.scope.support = DescribeSupport(report.snapshot);
  result.scope.requested_outer_samples = report.requested_outer_samples;
  result.scope.budget_ms = report.budget_ms;
  const auto& member_scope = report.representative_input.layers.front().scope;
  result.scope.physical_members = member_scope.members;
  result.scope.member_kind = member_scope.member_kind;
  result.scope.location = static_cast<const ScopeLocation&>(member_scope);
  result.scope.spectrum = report.representative_input.spectrum.rows;
  for (const auto& layer : report.snapshot.layers) {
    ReportLayerScope scope;
    if (!report.standalone_crystal) {
      scope.scene_layer = layer.layer_index;
    }
    scope.crystal = layer.crystal;
    scope.representative_faces = layer.representative;
    scope.symmetry_bits = layer.symmetry_bits;
    result.scope.layers.push_back(std::move(scope));
  }
  result.budgets.requested_outer_samples = report.options.sampling.requested_samples;
  result.budgets.replicate_seed = report.seed ^ 0x9e3779b9u;
  result.budgets.max_optical_evaluations = report.options.sampling.max_optical_evaluations;
  result.budgets.optical_evaluations = report.spectral_optical_evaluations;
  result.budgets.max_field_component_evaluations = report.options.max_field_evaluations;
  result.budgets.field_component_evaluations = report.spectral_field_evaluations;
  result.timing.spectral_verification = report.spectral_seconds;
  result.timing.capture = report.capture_seconds;
  return result;
}

FeatureReportResult CompleteFeatureReportResult(FeatureReportData result, AssembledSchema3Report assembled) {
  if (result.outcome == ReportOutcome::kUnsupportedMulticrystal) {
    result.mc.observation_scope_note = kMcObservationScopeNote;
    return FeatureReportResult(std::move(result));
  }
  auto& discovery = assembled.mc.discovery;
  for (const auto& feature : discovery.features) {
    if (!feature.source_token) {
      continue;
    }
    const uint64_t token = *feature.source_token;
    if (token >= discovery.measure.sources.size()) {
      continue;
    }
    if (std::any_of(result.mc.discovery.positive_sources.begin(), result.mc.discovery.positive_sources.end(),
                    [token](const PositiveSourceWitness& source) { return source.token == token; })) {
      continue;
    }
    PositiveSourceWitness source;
    source.token = token;
    source.source = discovery.measure.sources[token];
    AssembledInput input;
    if (ReplayDiagnosticSource(discovery.measure, token, &input).Ok()) {
      const auto& layer = input.layers.front();
      const auto& spectral = input.spectrum.rows[source.source.spectral_row];
      source.coordinates =
          analytic::DiagnosticInputRow{ layer.shape, layer.analytic_pose, input.source.incident_direction,
                                        spectral.refractive_index, token };
    }
    result.mc.discovery.positive_sources.push_back(std::move(source));
  }
  result.budgets.completed_outer_samples = discovery.measure.completed_samples;
  result.budgets.replicate_samples = discovery.replicate_samples;
  result.budgets.optical_evaluations +=
      discovery.measure.optical_evaluations + discovery.replicate_path_evaluations + discovery.event_path_evaluations;
  result.budgets.field_component_evaluations += discovery.field_component_evaluations;
  result.budgets.exhausted = discovery.budget_exhausted;
  result.budgets.attribution_dp_evaluations = static_cast<uint64_t>(assembled.unattributed.counts.dp_evaluations) +
                                              static_cast<uint64_t>(assembled.corroboration_counts.dp_evaluations);
  result.budgets.attribution_presence_ess_queries =
      static_cast<uint64_t>(assembled.unattributed.counts.presence_ess_queries) +
      static_cast<uint64_t>(assembled.corroboration_counts.presence_ess_queries);
  result.timing.assembly = discovery.assembly_seconds;
  result.timing.source_events_and_edge_observations = discovery.event_seconds;
  result.timing.field_discovery = discovery.field_seconds;
  result.timing.enumeration = assembled.enumeration_seconds;
  result.timing.attribution = assembled.attribution_seconds;
  result.coverage.push_back({ CoverageRole::kBaseInput,
                              "actual product input and physical L2",
                              CoverageStatus::kSupported,
                              "one selected crystal chain; actual distribution/shape/source/spectrum assembly",
                              {} });
  const bool partial = discovery.budget_exhausted || !discovery.unfinished.empty();
  result.coverage.push_back(
      { CoverageRole::kBoundedDiscovery, "bounded MC corroboration discovery",
        partial ? CoverageStatus::kNumericalIncomplete : CoverageStatus::kSupported,
        "only the recorded local windows and source seeds; no all-sky or source-topology completeness claim",
        discovery.limitations });
  if (!assembled.measure_skip_note.empty()) {
    result.coverage.push_back({ CoverageRole::kDeclaredSkip,
                                "declared measure",
                                CoverageStatus::kNotSupported,
                                assembled.measure_skip_note,
                                {} });
  }
  if (!assembled.density_skip_note.empty()) {
    result.coverage.push_back({ CoverageRole::kDeclaredSkip,
                                "pose density conversion",
                                CoverageStatus::kNotSupported,
                                assembled.density_skip_note,
                                {} });
  }
  result.outcome = assembled.ruling.issued ? ReportOutcome::kNoRelatedFeature :
                   partial                 ? ReportOutcome::kPartial :
                                             ReportOutcome::kCompleted;
  result.core = std::move(assembled.core);
  result.annotations = std::move(assembled.annotations);
  result.unattributed = std::move(assembled.unattributed);
  result.corroboration_counts = assembled.corroboration_counts;
  result.ruling = std::move(assembled.ruling);
  result.mc.observation_options = { assembled.mc.observation_options.bandwidth_rad,
                                    assembled.mc.observation_options.location_resolution_rad };
  result.mc.spectral_verification = assembled.mc.spectral_verification;
  result.mc.observation_scope_note = std::move(assembled.mc.observation_scope_note);
  static_cast<DiscoveryRecords&>(result.mc.discovery) = std::move(static_cast<DiscoveryRecords&>(discovery));
  result.mc.discovery.measure = std::move(static_cast<DiagnosticMeasureFacts&>(discovery.measure));
  return FeatureReportResult(std::move(result));
}

}  // namespace lumice::raypath::schema3
