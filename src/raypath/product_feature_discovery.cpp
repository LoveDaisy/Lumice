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
  if (snapshot.light.param_.diameter_ != 0 || snapshot.layers.size() != 1) {
    return false;
  }
  const auto& crystal = snapshot.layers[0].crystal;
  const auto& axis = crystal.axis_;
  for (const auto& dist : { axis.latitude_dist, axis.azimuth_dist, axis.roll_dist }) {
    if (BuildDistributionDrawPlan(dist).uniform_count != 0) {
      return false;
    }
  }
  const auto plan = std::visit([](const auto& param) { return BuildShapeDrawPlan(param); }, crystal.param_);
  return std::none_of(plan.begin(), plan.end(), [](const auto& slot) {
    return slot.applicable && BuildDistributionDrawPlan(slot.distribution).uniform_count != 0;
  });
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
                            DiagnosticFeatureRecord* record) {
  record->minimum_effective_samples =
      std::min({ fine.field.effective_samples_y, coarse.field.effective_samples_y, scale.field.effective_samples_y });
  if (fine.status != a::FieldSolveStatus::kConverged || coarse.status != a::FieldSolveStatus::kConverged ||
      scale.status != a::FieldSolveStatus::kConverged) {
    record->reason = "local equation not converged at every prefix and observation scale";
    return DiagnosticEvidence::kUnfinished;
  }
  record->prefix_movement_rad = Distance(fine.query.direction, coarse.query.direction);
  record->scale_movement_rad = Distance(fine.query.direction, scale.query.direction);
  record->transverse_contrast = contrast;
  if (record->prefix_movement_rad > resolution || record->scale_movement_rad > resolution ||
      record->minimum_effective_samples < 32 || !(contrast > 1e-3)) {
    record->reason = "position stability or positive transverse contrast not established at the declared resolution";
    return DiagnosticEvidence::kUnfinished;
  }
  record->reason = "positive field structure stable under the recorded prefix and scale checks; bounded local search";
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
      const auto spent = result->measure.optical_evaluations + result->event_path_evaluations;
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
      record.interface_event = std::move(event);
      result->features.push_back(std::move(record));
      ++found;
    }
  }
  result->unfinished.push_back("bounded interface seed subset is not a complete event/topology search");
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
      options.max_interface_candidates > 256) {
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
  if (fixed && result.measure.completed_samples == 1) {
    for (const auto& row : result.measure.components) {
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
  const auto event_start = Clock::now();
  FindEvents(options, &result);
  result.event_seconds = std::chrono::duration<double>(Clock::now() - event_start).count();
  const auto field_start = Clock::now();
  a::FieldWorkBudget budget{ options.max_field_evaluations, 0, options.sampling.deadline };
  const auto seeds = Seeds(result.measure, options.max_seeds);
  const uint64_t coarse_count = result.measure.completed_samples / 4;
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
        record.evidence = Classify(fine, previous, scale, options.location_resolution_rad, contrast, &record);
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
                         Contrast(result.measure.components, point, equation, &budget),
                         &evidence) != DiagnosticEvidence::kActual) {
              record.walk_stop = a::FieldWalkStop::kCorrectorFailed;
              break;
            }
            record.prefix_movement_rad = std::max(record.prefix_movement_rad, evidence.prefix_movement_rad);
            record.scale_movement_rad = std::max(record.scale_movement_rad, evidence.scale_movement_rad);
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
      result.features.push_back(std::move(record));
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
  result.unfinished.push_back("bounded seed search, not exhaustive feature or source topology coverage");
  if (result.budget_exhausted) {
    result.unfinished.push_back(
        "deadline or evaluation budget exhausted; previously established local records retained");
  }
  *out = std::move(result);
  return {};
}

}  // namespace lumice::raypath
