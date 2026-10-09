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

AssembledSchema3Report AssembleSchema3Report(PathFeatureReport&& report, uint64_t max_optical_evaluations) {
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
  out.core = EnumerateLayer(in, layer.scope.members);
  out.enumeration_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - enumeration_begin).count();
  // The budget's configured cap rides the report (the request's own vocabulary); the
  // enumeration counts what it built, the kernel-internal counts stay the registered gap.
  out.core.budget.max_optical_evaluations =
      static_cast<long long>(std::min<uint64_t>(max_optical_evaluations, std::numeric_limits<long long>::max()));

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

}  // namespace lumice::raypath::schema3
