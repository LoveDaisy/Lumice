#include "raypath/product_feature_discovery.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "analytic/so3.hpp"

namespace lumice::raypath {
namespace {
namespace a = analytic;
using Clock = std::chrono::steady_clock;

bool FixedInput(const ProductInputSnapshot& snapshot) {
  const auto support = DescribeProductSupport(snapshot);
  return snapshot.layers.size() == 1 && support.pose_support_dimension == 0 && support.shape_parameter_dimension == 0 &&
         support.source_direction_dimension == 0;
}

double Distance(const std::array<double, 3>& x, const std::array<double, 3>& y) {
  double cross[3];
  a::so3::Cross3(x.data(), y.data(), cross);
  return std::atan2(a::so3::Norm3(cross), a::so3::Dot3(x.data(), y.data()));
}

a::SphericalFieldQuery Query(const std::array<double, 3>& direction, double bandwidth) {
  a::SphericalFieldQuery query;
  query.direction = direction;
  query.bandwidth_rad = bandwidth;
  double basis[2][3];
  a::so3::TangentBasis(direction.data(), basis);
  for (int j = 0; j < 2; ++j) {
    std::copy_n(basis[j], 3, query.basis[j].begin());
  }
  return query;
}

std::vector<std::array<double, 3>> Seeds(const ProductDiagnosticMeasure& measure, int count) {
  std::vector<std::array<double, 3>> seeds;
  std::array<double, 3> mean{};
  double total = 0;
  for (const auto& row : measure.components) {
    total += row.xyz_weight[1];
    for (int j = 0; j < 3; ++j) {
      mean[j] += row.xyz_weight[1] * row.direction[j];
    }
  }
  if (!(total > 0)) {
    return seeds;
  }
  const double norm = a::so3::Norm3(mean.data());
  if (norm > total * 1e-12) {
    for (auto& value : mean) {
      value /= norm;
    }
    seeds.push_back(mean);
  }
  // Weighted source quantiles seed a bounded search; they do not assert that a
  // cloud row is a feature. Source identities are never merged by sky proximity.
  double cumulative = 0;
  size_t cursor = 0;
  for (int i = 0; static_cast<int>(seeds.size()) < count && i < count; ++i) {
    const double wanted = total * (i + .5) / count;
    while (cursor < measure.components.size() && cumulative < wanted) {
      cumulative += measure.components[cursor++].xyz_weight[1];
    }
    seeds.push_back(measure.components[std::max<size_t>(cursor, 1) - 1].direction);
  }
  return seeds;
}

DiagnosticEvidence Classify(const a::FieldStationaryPoint& fine, const a::FieldStationaryPoint& coarse,
                            const a::FieldStationaryPoint& scale, double resolution, double contrast,
                            DiagnosticFeatureRecord* record, double required_ess = 32) {
  record->minimum_effective_samples = std::min(fine.field.effective_samples_y, coarse.field.effective_samples_y);
  record->scale_status = scale.status;
  if (fine.status == a::FieldSolveStatus::kConverged && scale.status == a::FieldSolveStatus::kConverged) {
    record->scale_movement_rad = Distance(fine.query.direction, scale.query.direction);
  }
  if (fine.status != a::FieldSolveStatus::kConverged || coarse.status != a::FieldSolveStatus::kConverged) {
    record->reason = "local equation not converged at both prefixes of the fixed observation";
    return DiagnosticEvidence::kUnfinished;
  }
  record->prefix_movement_rad = Distance(fine.query.direction, coarse.query.direction);
  record->transverse_contrast = contrast;
  if (record->prefix_movement_rad > resolution || record->minimum_effective_samples < required_ess ||
      !(contrast > 1e-3)) {
    record->reason = "position stability or positive transverse contrast not established at the declared resolution";
    return DiagnosticEvidence::kUnfinished;
  }
  record->reason =
      "positive field structure stable at fixed kernel/width/level under the recorded prefix check; scale response is "
      "separate";
  return DiagnosticEvidence::kActual;
}

double Contrast(const std::vector<a::WeightedSkySample>& measure, const a::FieldStationaryPoint& point,
                a::FieldEquation equation, a::FieldWorkBudget* budget) {
  const bool colour = equation == a::FieldEquation::kChromaticityX || equation == a::FieldEquation::kChromaticityY;
  std::array<double, 2> normal = point.normal;
  if (colour) {
    const auto& jet = point.field.xy[equation == a::FieldEquation::kChromaticityX ? 0 : 1];
    const double norm = std::hypot(jet.gradient[0], jet.gradient[1]);
    if (!(norm > 0)) {
      return 0;
    }
    normal = { jet.gradient[0] / norm, jet.gradient[1] / norm };
  }
  a::SphericalFieldValue side[2];
  for (int i = 0; i < 2; ++i) {
    std::array<double, 3> direction;
    const double step = (2 * i - 1) * point.query.bandwidth_rad;
    for (int j = 0; j < 3; ++j) {
      direction[j] = std::cos(step) * point.query.direction[j] +
                     std::sin(step) * (normal[0] * point.query.basis[0][j] + normal[1] * point.query.basis[1][j]);
    }
    if (!a::EvaluateSphericalField(measure, Query(direction, point.query.bandwidth_rad), &side[i], budget)) {
      return 0;
    }
  }
  if (colour) {
    if (!side[0].chromaticity_available || !side[1].chromaticity_available) {
      return 0;
    }
    return std::hypot(side[0].xy[0].value - side[1].xy[0].value, side[0].xy[1].value - side[1].xy[1].value);
  }
  const double y = point.field.xyz[1].value;
  return y > 0 ? 1 - std::max(side[0].xyz[1].value, side[1].xyz[1].value) / y : 0;
}

void FindSourceBoundaries(const ProductDiscoveryOptions& options, ProductDiscoveryResult* result) {
  if (result->measure.sources.empty() || options.max_source_boundaries == 0) {
    return;
  }
  ProductInput base;
  if (!ReplayDiagnosticSource(result->measure, 0, &base).Ok()) {
    return;
  }
  const auto& original = base.layers[0];
  const auto& identity = result->measure.sources[0];
  struct Coordinate {
    int kind;
    size_t index;
    std::string name;
    std::vector<float> ends;
  };
  std::vector<Coordinate> coordinates;
  if (base.source.domain.diameter_ > 0) {
    coordinates.push_back({ 0, 0, "sun.cap_radial", { 1 } });
  }
  const auto bounded = [](const Distribution& d) {
    return d.type == DistributionType::kUniform && d.spread > 0 && d.spread < 360;
  };
  if (std::holds_alternative<DistributedAxisDraw>(original.sample.axis)) {
    const auto& axis = original.scope.snapshot.crystal.axis_;
    if (bounded(axis.azimuth_dist)) {
      coordinates.push_back({ 1, 0, "axis.azimuth_uniform", { 0, 1 } });
    }
    if (bounded(axis.roll_dist)) {
      coordinates.push_back({ 2, 0, "axis.roll_uniform", { 0, 1 } });
    }
    if (bounded(axis.latitude_dist) && std::abs(axis.latitude_dist.center) + axis.latitude_dist.spread * .5f < 90 &&
        std::holds_alternative<LatitudeLutDraw>(std::get<DistributedAxisDraw>(original.sample.axis).latitude)) {
      coordinates.push_back({ 4, 0, "axis.latitude_cdf", { 0, 1 } });
    }
  }
  const auto plan =
      std::visit([](const auto& param) { return BuildShapeDrawPlan(param); }, original.scope.snapshot.crystal.param_);
  for (size_t j = 0; j < original.sample.shape.size; ++j) {
    const int slot = original.sample.shape.values[j].slot;
    if (plan[slot].distribution.type == DistributionType::kUniform && plan[slot].distribution.spread > 0) {
      coordinates.push_back({ 3, j, "shape.leader." + std::to_string(slot), { 0, 1 } });
    }
  }
  int found = 0;
  auto evaluate = [&](const std::vector<std::pair<size_t, float>>& endpoints) {
    if (found >= options.max_source_boundaries) {
      return;
    }
    const auto spent =
        result->measure.optical_evaluations + result->replicate_path_evaluations + result->event_path_evaluations;
    if (spent >= options.sampling.max_optical_evaluations || Clock::now() >= options.sampling.deadline) {
      result->budget_exhausted = true;
      return;
    }
    auto sample = original.sample;
    auto source = base.source.sample;
    DiagnosticFeatureRecord record;
    for (const auto& endpoint : endpoints) {
      const auto& coordinate = coordinates[endpoint.first];
      record.source_parameters.push_back({ coordinate.name, endpoint.second });
      if (coordinate.kind == 0) {
        source.draw.radial_uniform = endpoint.second;
      } else if (coordinate.kind == 1) {
        std::get<DistributedAxisDraw>(sample.axis).azimuth.value = endpoint.second;
      } else if (coordinate.kind == 2) {
        std::get<DistributedAxisDraw>(sample.axis).roll.value = endpoint.second;
      } else if (coordinate.kind == 4) {
        std::get<LatitudeLutDraw>(std::get<DistributedAxisDraw>(sample.axis).latitude).quantile = endpoint.second;
      } else {
        const int slot = sample.shape.values[coordinate.index].slot;
        sample.shape.values[coordinate.index].value =
            TransformDistribution(plan[slot].distribution, { endpoint.second });
      }
    }
    ProductInput input;
    if (!result->measure.run->Reassemble({ sample }, source, &input).Ok()) {
      return;
    }
    const auto& layer = input.layers[0];
    const auto& faces = layer.scope.members[identity.member_index];
    a::DiagnosticInputRow row{ layer.shape, layer.analytic_pose, input.source.incident_direction,
                               input.spectrum.rows[identity.spectral_row].refractive_index, 0 };
    std::vector<a::DiagnosticOutputRow> values;
    if (!a::EvaluateDiagnosticBatch(faces, { row }, { false, true }, &values)) {
      return;
    }
    result->event_path_evaluations += values[0].path_evaluations;
    if (!values[0].path_valid || !(values[0].entry.value > 0) || !(values[0].interface_product > 0)) {
      return;
    }
    record.kind = endpoints.size() == 1 ? "declared_source_boundary" : "declared_source_corner";
    record.evidence = DiagnosticEvidence::kCandidate;
    record.reason =
        "closure of declared product uniform/cap coordinates, other draws fixed; positive finite optical support; not "
        "an outer sky edge or unique cause";
    record.source_token = 0;
    record.boundary_source = row;
    record.boundary_value = values[0];
    const auto& v = values[0].outgoing;
    record.sky_points.push_back({ -v[0], -v[1], -v[2] });
    result->features.push_back(std::move(record));
    ++found;
  };
  // Two declared faces share a corner by their parameter values, never by sky
  // proximity. Other coordinates retain the same replayable parent draw.
  if (coordinates.size() >= 2) {
    for (float x : coordinates[0].ends) {
      for (float y : coordinates[1].ends) {
        evaluate({ { 0, x }, { 1, y } });
      }
    }
  }
  for (size_t i = 0; i < coordinates.size(); ++i) {
    for (float end : coordinates[i].ends) {
      evaluate({ { i, end } });
    }
  }
  if (!coordinates.empty()) {
    result->limitations.push_back(
        "bounded declared-coordinate boundary samples, not complete source-boundary topology");
  }
}

void FindDeviationEdges(const ProductDiscoveryOptions& options, ProductDiscoveryResult* result,
                        a::FieldWorkBudget* budget) {
  const auto& measure = result->measure;
  int found = 0;
  ProductInput input;
  for (size_t index = 0; index < measure.sources.size() && index < 64 && found < options.max_deviation_candidates;
       ++index) {
    if (!ReplayDiagnosticSource(measure, index, &input).Ok() || !SingleCrystalIncidentOrbit(input)) {
      result->unfinished.push_back("minimum-deviation chart is unavailable on restricted orientation support");
      return;
    }
    const auto& identity = measure.sources[index];
    const auto& layer = input.layers[0];
    const auto& faces = layer.scope.members[identity.member_index];
    const auto spent =
        measure.optical_evaluations + result->replicate_path_evaluations + result->event_path_evaluations;
    const uint64_t available =
        options.sampling.max_optical_evaluations - std::min(options.sampling.max_optical_evaluations, spent);
    a::DiagnosticInputRow row{ layer.shape, layer.analytic_pose, input.source.incident_direction,
                               input.spectrum.rows[identity.spectral_row].refractive_index, index };
    auto minimum = a::CorrectDeviationMinimum(faces, row, {}, available, options.sampling.deadline);
    result->event_path_evaluations += minimum.path_evaluations;
    if (minimum.status == a::InterfaceSolveStatus::kBudgetExceeded) {
      result->budget_exhausted = true;
      result->unfinished.push_back("minimum-deviation source correction budget exhausted");
      return;
    }
    if (minimum.status != a::InterfaceSolveStatus::kConverged) {
      continue;
    }
    ++found;
    DiagnosticFeatureRecord record;
    record.kind = "local_minimum_deviation_edge";
    record.evidence = DiagnosticEvidence::kCandidate;
    record.reason =
        "positive local direction-map minimum in the declared Haar quotient; not a global support certificate";
    record.source_token = index;
    record.deviation_minimum = minimum;
    const auto& s = row.incident;
    const std::array<double, 3> sun{ -s[0], -s[1], -s[2] };
    const auto& v = minimum.value.outgoing;
    const std::array<double, 3> q{ -v[0], -v[1], -v[2] };
    record.sky_points.push_back(q);
    record.orbit_axis = sun;
    // A fixed shape and point source make the corrected locus a member/spectrum
    // support feature. For distributed shape/source it remains conditional.
    const auto& snapshot = measure.run->Snapshot();
    const auto plan =
        std::visit([](const auto& p) { return BuildShapeDrawPlan(p); }, snapshot.layers[0].crystal.param_);
    const bool fixed_shape = std::none_of(plan.begin(), plan.end(), [](const auto& slot) {
      return slot.applicable && BuildDistributionDrawPlan(slot.distribution).uniform_count != 0;
    });
    if (fixed_shape && snapshot.light.param_.diameter_ == 0) {
      std::vector<a::WeightedSkySample> spectral, coarse;
      for (const auto& component : measure.components) {
        const auto& source = measure.sources[component.source_token];
        if (source.member_index == identity.member_index && source.spectral_row == identity.spectral_row) {
          spectral.push_back(component);
          if (component.sample_index < measure.completed_samples / 4) {
            coarse.push_back(component);
          }
        }
      }
      // Compare the same member/wavelength observation on either side of the
      // physical radius. The observation peak is deliberately not the radius.
      double normal[3];
      const double cosine = a::so3::Dot3(q.data(), sun.data());
      for (int j = 0; j < 3; ++j) {
        normal[j] = cosine * q[j] - sun[j];
      }
      const double norm = a::so3::Norm3(normal);
      bool observed = norm > 0;
      a::SphericalFieldValue fields[2][2];
      for (int prefix = 0; prefix < 2 && observed; ++prefix) {
        for (int side = 0; side < 2 && observed; ++side) {
          std::array<double, 3> direction;
          const double step = (2 * side - 1) * options.bandwidth_rad;
          for (int j = 0; j < 3; ++j) {
            direction[j] = std::cos(step) * q[j] + std::sin(step) * normal[j] / norm;
          }
          observed = a::EvaluateSphericalField(prefix ? coarse : spectral, Query(direction, options.bandwidth_rad),
                                               &fields[prefix][side], budget);
        }
      }
      if (observed) {
        double contrast[2];
        record.minimum_effective_samples = std::numeric_limits<double>::infinity();
        for (int prefix = 0; prefix < 2; ++prefix) {
          const double inside = fields[prefix][0].xyz[1].value;
          const double outside = fields[prefix][1].xyz[1].value;
          contrast[prefix] = outside > 0 ? 1 - inside / outside : 0;
          for (const auto& field : fields[prefix]) {
            record.minimum_effective_samples = std::min(record.minimum_effective_samples, field.effective_samples_y);
          }
        }
        record.bandwidth_rad = options.bandwidth_rad;
        record.transverse_contrast = contrast[0];
        record.observation_contrast_error = std::abs(contrast[0] - contrast[1]);
        if (contrast[0] > .1 && record.observation_contrast_error < .05 * contrast[0] &&
            record.minimum_effective_samples >= 32 && minimum.correction_rad <= options.location_resolution_rad) {
          record.evidence = DiagnosticEvidence::kActual;
          record.reason =
              "positive local scattering edge for this fixed member/wavelength and Haar point source; fixed-h "
              "inside/outside contrast stable across prefixes; global minimum unproved";
        }
      }
    }
    // The orbit is a real source symmetry, not a circle fitted to sky samples.
    // Preserve its source range even if the point cap truncates drawing geometry.
    const double sine = std::sin(minimum.deviation_rad);
    if (sine > 0) {
      const double step = std::min(.5 * options.bandwidth_rad / sine, .1);
      for (int j = 1; j < options.max_curve_points; ++j) {
        const double delta[]{ j * step * sun[0], j * step * sun[1], j * step * sun[2] };
        double rotation[9];
        std::array<double, 3> point;
        a::so3::Exp(delta, rotation);
        a::chain_detail::BodyToWorld(rotation, q.data(), point.data());
        record.sky_points.push_back(point);
        record.orbit_end_rad = j * step;
      }
    }
    if (record.sky_points.size() > 1) {
      record.geometry = DiagnosticGeometry::kPolyline;
    }
    record.walk_stop = a::FieldWalkStop::kPointLimit;
    result->features.push_back(std::move(record));
    if (budget->exhausted) {
      return;
    }
  }
}

void FindEvents(const ProductDiscoveryOptions& options, ProductDiscoveryResult* result) {
  const auto& measure = result->measure;
  if (measure.components.empty()) {
    return;
  }
  ProductInput input;
  int found = 0;
  // One bounded source subset, all internal slots, no preferred hemisphere or
  // path. This chart is admissible only for the already-proved Haar measure.
  for (size_t index = 0; index < measure.sources.size() && index < 64 && found < options.max_interface_candidates;
       ++index) {
    const auto error = ReplayDiagnosticSource(measure, index, &input);
    if (!error.Ok() || !SingleCrystalIncidentOrbit(input)) {
      result->unfinished.push_back("event SO(3) chart not established for this non-Haar source support");
      return;
    }
    const auto& source = measure.sources[index];
    const auto& layer = input.layers[0];
    const auto& faces = layer.scope.members[source.member_index];
    a::DiagnosticInputRow row{ layer.shape, layer.analytic_pose, input.source.incident_direction,
                               input.spectrum.rows[source.spectral_row].refractive_index, index };
    for (int slot = 1; slot + 1 < static_cast<int>(faces.size()) && found < options.max_interface_candidates; ++slot) {
      const auto spent =
          result->measure.optical_evaluations + result->replicate_path_evaluations + result->event_path_evaluations;
      const auto available =
          options.sampling.max_optical_evaluations - std::min(options.sampling.max_optical_evaluations, spent);
      auto event = a::CorrectInterfaceEvent(faces, row, { slot }, available, options.sampling.deadline);
      result->event_path_evaluations += event.path_evaluations;
      if (event.status == a::InterfaceSolveStatus::kBudgetExceeded) {
        result->budget_exhausted = true;
        result->unfinished.push_back("interface source continuation budget exhausted");
        return;
      }
      if (event.status != a::InterfaceSolveStatus::kConverged) {
        continue;
      }
      DiagnosticFeatureRecord record;
      record.evidence = DiagnosticEvidence::kCandidate;
      record.kind = "conditional_internal_tir";
      record.reason =
          "positive finite support and zero interface discriminant in a declared Haar chart; observed colour effect "
          "and complete source connectivity unproved";
      record.source_token = index;
      record.internal_slot = slot;
      const auto& v = event.value.outgoing;
      record.sky_points.push_back({ -v[0], -v[1], -v[2] });
      record.interface_event = event;
      PairedInterfaceEvidence paired;
      paired.complete = true;
      for (size_t spectral = 0; spectral < input.spectrum.rows.size(); ++spectral) {
        const auto spent =
            result->measure.optical_evaluations + result->replicate_path_evaluations + result->event_path_evaluations;
        if (spent >= options.sampling.max_optical_evaluations || Clock::now() >= options.sampling.deadline) {
          paired.complete = false;
          result->budget_exhausted = true;
          break;
        }
        ProductChainEvaluation value;
        const auto paired_error = EvaluateProductChain(input, { source.member_index }, spectral, &value);
        result->event_path_evaluations += value.optical_evaluations;
        if (!paired_error.Ok() || value.layers.size() != 1) {
          paired.complete = false;
          break;
        }
        const auto& layer_value = value.layers[0];
        // Removing a weight never relaxes the shape, entry or optical domain.
        if (layer_value.status != ProductContributionStatus::kPositive) {
          continue;
        }
        double without = layer_value.entry_weight;
        for (size_t j = 0; j < layer_value.interface_transmittances.size(); ++j) {
          if (j != static_cast<size_t>(slot)) {
            without *= layer_value.interface_transmittances[j];
          }
        }
        for (int j = 0; j < 3; ++j) {
          paired.actual_xyz[j] += value.xyz[j];
          paired.without_slot_xyz[j] += without * input.spectrum.rows[spectral].coefficient[j];
        }
      }
      const double actual_sum = paired.actual_xyz[0] + paired.actual_xyz[1] + paired.actual_xyz[2];
      const double without_sum = paired.without_slot_xyz[0] + paired.without_slot_xyz[1] + paired.without_slot_xyz[2];
      paired.chromaticity_available = paired.complete && actual_sum > 0 && without_sum > 0;
      if (paired.chromaticity_available) {
        for (int j = 0; j < 2; ++j) {
          paired.xy_difference[j] = paired.actual_xyz[j] / actual_sum - paired.without_slot_xyz[j] / without_sum;
        }
      }
      record.paired_interface = paired;
      for (const bool reverse : { false, true }) {
        const auto used =
            result->measure.optical_evaluations + result->replicate_path_evaluations + result->event_path_evaluations;
        const uint64_t remaining =
            options.sampling.max_optical_evaluations - std::min(options.sampling.max_optical_evaluations, used);
        auto curve = a::TraceInterfaceCurve(faces, event.source, slot, .01, 1e-7, options.max_source_curve_points,
                                            reverse, remaining, options.sampling.deadline);
        result->event_path_evaluations += curve.path_evaluations;
        result->budget_exhausted |= curve.stop == a::InterfaceWalkStop::kBudgetExceeded;
        record.interface_curves.push_back(std::move(curve));
      }
      const size_t parent = result->features.size();
      std::vector<DiagnosticFeatureRecord> endpoints;
      for (const auto& curve : record.interface_curves) {
        for (const auto& bracket : curve.events) {
          DiagnosticFeatureRecord endpoint;
          endpoint.evidence = DiagnosticEvidence::kCandidate;
          endpoint.geometry = DiagnosticGeometry::kSourceRange;
          endpoint.kind = bracket.kind == a::InterfaceWalkStop::kGeometricContact ? "geometric_contact_bracket" :
                          bracket.kind == a::InterfaceWalkStop::kOpticalGate      ? "optical_domain_gate" :
                                                                                    "product_area_threshold";
          endpoint.reason =
              "same-interface source continuation brackets this predicate; no global topology or observed colour claim";
          endpoint.source_token = index;
          endpoint.internal_slot = slot;
          endpoint.source_event = bracket;
          endpoint.source_connected_feature = parent;
          for (const auto* bound : { &bracket.positive, &bracket.nonpositive }) {
            if (!bound->value.path_valid) {
              continue;
            }
            const auto& outgoing = bound->value.outgoing;
            endpoint.sky_points.push_back({ -outgoing[0], -outgoing[1], -outgoing[2] });
          }
          endpoints.push_back(std::move(endpoint));
        }
      }
      result->features.push_back(std::move(record));
      for (auto& endpoint : endpoints) {
        result->features.push_back(std::move(endpoint));
      }
      ++found;
    }
  }
  result->limitations.push_back("bounded interface seed subset is not a complete event/topology search");
}

}  // namespace

Error DiscoverProductFeatures(const ProductDiagnosticSampler& sampler, const ProductDiscoveryOptions& options,
                              ProductDiscoveryResult* out) {
  if (!out) {
    return { ErrorCode::kInvalidArgument, "null product discovery output" };
  }
  *out = {};
  if (!(options.bandwidth_rad > 0) || options.bandwidth_rad >= 1 || !(options.location_resolution_rad > 0) ||
      !std::isfinite(options.location_resolution_rad) || options.max_seeds <= 0 || options.max_seeds > 64 ||
      options.max_curve_points <= 0 || options.max_curve_points > 256 || options.max_interface_candidates < 0 ||
      options.max_interface_candidates > 256 || options.max_deviation_candidates < 0 ||
      options.max_deviation_candidates > 256 || options.max_source_curve_points < 2 ||
      options.max_source_curve_points > 4096 || options.max_source_boundaries < 0 ||
      options.max_source_boundaries > 256) {
    return { ErrorCode::kInvalidArgument, "invalid explicit discovery scale or bounded search size" };
  }
  const auto begin = Clock::now();
  ProductDiscoveryResult result;
  auto sampling = options.sampling;
  const bool fixed = FixedInput(sampler.Snapshot());
  if (fixed) {
    sampling.requested_samples = 1;
  }
  const auto error = BuildProductDiagnosticMeasure(sampler, sampling, &result.measure);
  if (!error.Ok()) {
    return error;
  }
  result.assembly_seconds = std::chrono::duration<double>(Clock::now() - begin).count();
  result.budget_exhausted = result.measure.budget_exhausted;
  if (fixed && std::holds_alternative<std::vector<WlParam>>(sampler.Snapshot().light.spectrum_) &&
      result.measure.completed_samples == 1) {
    for (const auto& row : result.measure.components) {
      if (!(row.xyz_weight[1] > 0)) {
        continue;
      }
      DiagnosticFeatureRecord record;
      record.kind = "positive_fixed_source_atom";
      record.evidence = DiagnosticEvidence::kActual;
      record.geometry = DiagnosticGeometry::kAtom;
      record.reason = "zero-dimensional declared shape/pose/point-source support with positive finite entry measure";
      record.source_token = row.source_token;
      record.sky_points.push_back(row.direction);
      record.atom_xyz_mass = row.xyz_weight;
      result.features.push_back(std::move(record));
    }
    if (result.features.empty()) {
      result.unfinished.push_back("fixed source has no positive weighted contribution; no sampled rank-zero inference");
    }
    *out = std::move(result);
    return {};
  }
  ProductDiagnosticMeasure replicate;
  auto replicate_budget = options.sampling;
  replicate_budget.requested_samples = result.measure.completed_samples;
  replicate_budget.max_optical_evaluations -=
      std::min(replicate_budget.max_optical_evaluations, result.measure.optical_evaluations);
  if (replicate_budget.requested_samples > 0) {
    const auto replicate_error =
        BuildProductDiagnosticMeasure(sampler.IndependentReplicate(), replicate_budget, &replicate);
    if (!replicate_error.Ok()) {
      return replicate_error;
    }
  }
  result.replicate_path_evaluations = replicate.optical_evaluations;
  result.replicate_samples = replicate.completed_samples;
  result.assembly_seconds = std::chrono::duration<double>(Clock::now() - begin).count();
  result.budget_exhausted |= replicate.budget_exhausted;
  const auto event_start = Clock::now();
  a::FieldWorkBudget budget{ options.max_field_evaluations, 0, options.sampling.deadline };
  FindSourceBoundaries(options, &result);
  FindDeviationEdges(options, &result, &budget);
  FindEvents(options, &result);
  result.event_seconds = std::chrono::duration<double>(Clock::now() - event_start).count();
  const auto field_start = Clock::now();
  const double required_ess = fixed ? 1 : 32;
  auto verify_replicate = [&](const a::FieldStationaryPoint& point, const a::FieldSolveOptions& solve,
                              DiagnosticFeatureRecord* record) {
    const auto check = a::CorrectSphericalField(replicate.components, point.query.direction, solve, &budget);
    if (check.status != a::FieldSolveStatus::kConverged) {
      record->reason = "independent same-observation replicate not converged";
      return false;
    }
    const double movement = Distance(point.query.direction, check.query.direction);
    record->replicate_movement_rad = std::max(record->replicate_movement_rad.value_or(0), movement);
    if (movement > options.location_resolution_rad || check.field.effective_samples_y < required_ess) {
      record->reason = "independent same-observation replicate disagrees at the declared location resolution";
      return false;
    }
    return true;
  };
  const auto seeds = Seeds(result.measure, options.max_seeds);
  const uint64_t coarse_count = fixed ? result.measure.completed_samples : result.measure.completed_samples / 4;
  std::vector<a::WeightedSkySample> coarse;
  for (const auto& row : result.measure.components) {
    if (row.sample_index < coarse_count) {
      coarse.push_back(row);
    }
  }
  std::vector<a::SphericalFieldValue> seed_fields;
  double min_xy[2]{ 1, 1 };
  double max_xy[2]{};
  for (const auto& seed : seeds) {
    a::SphericalFieldValue field;
    if (!a::EvaluateSphericalField(result.measure.components, Query(seed, options.bandwidth_rad), &field, &budget)) {
      break;
    }
    if (field.chromaticity_available && field.effective_samples_y >= 32) {
      for (int j = 0; j < 2; ++j) {
        min_xy[j] = std::min(min_xy[j], field.xy[j].value);
        max_xy[j] = std::max(max_xy[j], field.xy[j].value);
      }
    }
    seed_fields.push_back(field);
  }
  for (size_t i = 0; i < seed_fields.size() && !budget.exhausted; ++i) {
    for (const auto equation : { a::FieldEquation::kLogYPeak, a::FieldEquation::kLogYRidge,
                                 a::FieldEquation::kChromaticityX, a::FieldEquation::kChromaticityY }) {
      const bool colour = equation == a::FieldEquation::kChromaticityX || equation == a::FieldEquation::kChromaticityY;
      const int channel = equation == a::FieldEquation::kChromaticityX ? 0 : 1;
      if (colour && max_xy[channel] - min_xy[channel] <= 1e-3) {
        continue;
      }
      const double level = colour ? .5 * (max_xy[channel] + min_xy[channel]) : 0;
      a::FieldSolveOptions solve{ equation, level, options.bandwidth_rad, 1e-8, options.bandwidth_rad * .5, 32 };
      auto fine = a::CorrectSphericalField(result.measure.components, seeds[i], solve, &budget);
      DiagnosticFeatureRecord record;
      record.equation = equation;
      record.level = level;
      record.bandwidth_rad = options.bandwidth_rad;
      record.kind = colour ? (channel == 0 ? "chromaticity_x_contour" : "chromaticity_y_contour") :
                    equation == a::FieldEquation::kLogYPeak ? "intensity_peak" :
                                                              "intensity_ridge";
      if (fine.status == a::FieldSolveStatus::kConverged) {
        auto previous = a::CorrectSphericalField(coarse, fine.query.direction, solve, &budget);
        solve.bandwidth_rad /= std::sqrt(2.0);
        auto scale = a::CorrectSphericalField(result.measure.components, fine.query.direction, solve, &budget);
        const double contrast = Contrast(result.measure.components, fine, equation, &budget);
        record.evidence =
            Classify(fine, previous, scale, options.location_resolution_rad, contrast, &record, required_ess);
        solve.bandwidth_rad = options.bandwidth_rad;
        if (record.evidence == DiagnosticEvidence::kActual && !verify_replicate(fine, solve, &record)) {
          record.evidence = DiagnosticEvidence::kUnfinished;
        }
        record.sky_points.push_back(fine.query.direction);
        record.field_points.push_back(fine);
        if (equation != a::FieldEquation::kLogYPeak && record.evidence == DiagnosticEvidence::kActual) {
          solve.bandwidth_rad = options.bandwidth_rad;
          // Keep a bounded forward segment. A point cap is numerical termination,
          // not a physical endpoint; every vertex must pass the same evidence gate.
          const auto curve = a::TraceSphericalField(
              result.measure.components, fine.query.direction,
              { solve, .5 * options.bandwidth_rad, .01 * fine.field.xyz[1].value, options.max_curve_points }, false,
              &budget);
          record.walk_stop = curve.stop;
          for (size_t j = 1; j < curve.points.size(); ++j) {
            const auto& point = curve.points[j];
            previous = a::CorrectSphericalField(coarse, point.query.direction, solve, &budget);
            auto smaller = solve;
            smaller.bandwidth_rad /= std::sqrt(2.0);
            scale = a::CorrectSphericalField(result.measure.components, point.query.direction, smaller, &budget);
            DiagnosticFeatureRecord evidence;
            if (Classify(point, previous, scale, options.location_resolution_rad,
                         Contrast(result.measure.components, point, equation, &budget), &evidence,
                         required_ess) != DiagnosticEvidence::kActual) {
              record.walk_stop = a::FieldWalkStop::kCorrectorFailed;
              break;
            }
            if (!verify_replicate(point, solve, &record)) {
              record.walk_stop = a::FieldWalkStop::kCorrectorFailed;
              break;
            }
            record.prefix_movement_rad = std::max(record.prefix_movement_rad, evidence.prefix_movement_rad);
            if (record.scale_movement_rad && evidence.scale_movement_rad) {
              record.scale_movement_rad = std::max(*record.scale_movement_rad, *evidence.scale_movement_rad);
            } else {
              record.scale_movement_rad.reset();
            }
            if (evidence.scale_status != a::FieldSolveStatus::kConverged) {
              record.scale_status = evidence.scale_status;
            }
            record.minimum_effective_samples =
                std::min(record.minimum_effective_samples, evidence.minimum_effective_samples);
            record.sky_points.push_back(point.query.direction);
            record.field_points.push_back(point);
          }
          if (record.sky_points.size() > 1) {
            record.geometry = DiagnosticGeometry::kPolyline;
          }
        }
      } else {
        record.reason = "seed did not solve the local field equation; not evidence of physical absence";
        record.field_points.push_back(fine);
      }
      if (record.evidence == DiagnosticEvidence::kActual && !record.field_points.empty()) {
        double strongest_y = 0;
        const auto& point = record.field_points.front();
        for (size_t token = 0; token < std::min<size_t>(64, result.measure.components.size()); ++token) {
          a::SphericalFieldValue contribution;
          if (!a::EvaluateSphericalField({ result.measure.components[token] }, point.query, &contribution, &budget)) {
            break;
          }
          if (contribution.xyz[1].value > strongest_y) {
            strongest_y = contribution.xyz[1].value;
            record.source_token = result.measure.components[token].source_token;
          }
        }
        if (record.source_token && point.field.xyz[1].value > 0) {
          record.contributor_fraction_of_estimated_y = strongest_y / point.field.xyz[1].value;
          for (size_t index = 0; index < result.features.size(); ++index) {
            const auto& candidate = result.features[index];
            if (candidate.interface_event && candidate.source_token == record.source_token) {
              record.source_connected_feature = index;
              break;
            }
          }
        }
      }
      if (colour && record.evidence == DiagnosticEvidence::kActual && record.field_points.size() > 1) {
        const auto& jet = record.field_points[0].field.xy[channel];
        const double half_range = .25 * options.bandwidth_rad * std::hypot(jet.gradient[0], jet.gradient[1]);
        DiagnosticFeatureRecord band_record = record;
        band_record.kind = channel == 0 ? "chromaticity_x_band" : "chromaticity_y_band";
        band_record.geometry = DiagnosticGeometry::kBand;
        band_record.scale_movement_rad.reset();
        band_record.scale_status = a::FieldSolveStatus::kInvalidInput;
        solve.bandwidth_rad = options.bandwidth_rad;
        band_record.band = a::CorrectSphericalFieldBand(result.measure.components, record.field_points,
                                                        { level - half_range, level + half_range }, solve, &budget);
        bool stable = band_record.band->status == a::FieldSolveStatus::kConverged;
        for (int side = 0; side < 2 && stable; ++side) {
          solve.level = band_record.band->levels[side];
          for (const auto& point : band_record.band->boundaries[side]) {
            const auto previous = a::CorrectSphericalField(coarse, point.query.direction, solve, &budget);
            DiagnosticFeatureRecord evidence;
            if (Classify(point, previous, point, options.location_resolution_rad,
                         Contrast(result.measure.components, point, equation, &budget), &evidence,
                         required_ess) != DiagnosticEvidence::kActual) {
              stable = false;
              break;
            }
            if (!verify_replicate(point, solve, &band_record)) {
              stable = false;
              break;
            }
            band_record.prefix_movement_rad = std::max(band_record.prefix_movement_rad, evidence.prefix_movement_rad);
          }
        }
        band_record.evidence = stable ? DiagnosticEvidence::kActual : DiagnosticEvidence::kUnfinished;
        band_record.reason = stable ? "local field-value range between two solved levels of one fixed xy observation; "
                                      "transverse window caps, not physical or uncertainty bounds" :
                                      "fixed-observation band boundaries not stable at every vertex";
        result.features.push_back(std::move(record));
        result.features.push_back(std::move(band_record));
      } else {
        result.features.push_back(std::move(record));
      }
      if (budget.exhausted) {
        break;
      }
    }
  }
  result.field_component_evaluations = budget.component_evaluations;
  result.field_seconds = std::chrono::duration<double>(Clock::now() - field_start).count();
  result.budget_exhausted |= budget.exhausted;
  if (seeds.empty()) {
    result.unfinished.push_back("no positive sampling seeds; finite sampling cannot prove empty support");
  }
  result.limitations.push_back("bounded seed search, not exhaustive feature or source topology coverage");
  if (result.budget_exhausted) {
    result.unfinished.push_back(
        "deadline or evaluation budget exhausted; previously established local records retained");
  }
  *out = std::move(result);
  return {};
}

}  // namespace lumice::raypath
