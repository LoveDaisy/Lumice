#include "raypath/map_analysis.hpp"

#include <cmath>
#include <memory>
#include <string>
#include <utility>

#include "core/math.hpp"
#include "raypath/detail/input_scope.hpp"
#include "raypath/detail/quantity/probe_line.hpp"
#include "raypath/detail/quantity/reference_map.hpp"
#include "raypath/scene_to_analytic.hpp"
#include "raypath/single_path_analysis.hpp"  // kMaxSampleCount — the sample bound the map request echoes
#include "util/sky_direction.hpp"

namespace lumice::raypath {

namespace {

// The probe's frame-azimuth read (the at point's azimuth about the sun) — one statement here
// so the map's view centre reads the same convention the probe face spells.
double FrameAzimuthOf(const survey::ProbeFrame& frame, const double direction[3]) {
  double dot = 0.0;
  for (int k = 0; k < 3; k++) {
    dot += direction[k] * frame.incident[k];
  }
  double v[3];
  for (int k = 0; k < 3; k++) {
    v[k] = direction[k] - dot * frame.incident[k];
  }
  const double e1 = v[0] * frame.e1[0] + v[1] * frame.e1[1] + v[2] * frame.e1[2];
  const double e2 = v[0] * frame.e2[0] + v[1] * frame.e2[1] + v[2] * frame.e2[2];
  return std::atan2(e2, e1) * math::kRadToDegreeD;
}

survey::MapViewDescription BuildView(const MapRequest& request, const survey::ProbeFrame& frame,
                                     double at_deviation_deg, double at_azimuth_about_sun_deg) {
  survey::MapViewDescription view;
  view.rows = request.view_rows;
  view.cols = request.view_cols;
  for (int k = 0; k < 3; k++) {
    view.incident[k] = frame.incident[k];
    view.e1[k] = frame.e1[k];
    view.e2[k] = frame.e2[k];
  }
  if (request.full_sky) {
    view.full_sky = true;
    view.delta_min_deg = 0.0;
    view.delta_max_deg = 180.0;
    view.az_min_deg = 0.0;
    view.az_max_deg = 360.0;
    return view;
  }
  const double half_delta = 0.5 * request.delta_span_deg;
  const double half_az = 0.5 * request.azimuth_span_deg;
  view.delta_min_deg = std::max(0.0, at_deviation_deg - half_delta);
  view.delta_max_deg = std::min(180.0, at_deviation_deg + half_delta);
  view.az_min_deg = at_azimuth_about_sun_deg - half_az;
  view.az_max_deg = at_azimuth_about_sun_deg + half_az;
  return view;
}

std::shared_ptr<const QuantityContext> MakeQuantityContext(const ConfigManager& config, IdType crystal_id,
                                                           const MemberScope& members, const MapMetadata& meta) {
  QuantityContext context;
  context.members = members;
  context.crystal = config.crystals_.find(crystal_id)->second;
  context.light = config.scene_.light_source_;
  context.observation = { meta.wavelength.nm, meta.sun_altitude_deg, meta.sun_azimuth_deg };
  return std::make_shared<const QuantityContext>(std::move(context));
}

}  // namespace

Error AnalyzePathMapData(const ConfigManager& config, const MapRequest& request, MapData* out,
                         const PreparedGeometry* prepared) {
  *out = MapData{};
  const auto crystal_it = config.crystals_.find(request.crystal_id);
  if (crystal_it == config.crystals_.end()) {
    return { ErrorCode::kUnknownCrystalId,
             "no crystal entry with id " + std::to_string(request.crystal_id) + " in the config" };
  }
  // The geometry: prepared by a workspace when it already built the selection's tables, else
  // this query's own one build. The single-layer path check reads the same tables.
  PreparedGeometry built;
  const PreparedGeometry* geometry = prepared;
  if (prepared == nullptr) {
    if (Error e = PrepareGeometry(crystal_it->second, &built); !e.Ok()) {
      return e;
    }
    geometry = &built;
  }
  {
    std::vector<int> slots;
    if (Error e = ResolveSingleLayerPath(request.path_layers, geometry->table, &slots); !e.Ok()) {
      return e;
    }
  }
  if (request.sample_count < 0 || request.sample_count > kMaxSampleCount) {
    return { ErrorCode::kInvalidArgument, "sample count must be 0 (the map default) or in [1, " +
                                              std::to_string(kMaxSampleCount) + "]; got " +
                                              std::to_string(request.sample_count) };
  }
  if (request.budget_ms < 0 || request.budget_ms > kMaxMapBudgetMs) {
    return { ErrorCode::kInvalidArgument,
             "budget_ms must be 0 (the default) or at most " + std::to_string(kMaxMapBudgetMs) };
  }
  if (request.view_rows < 1 || request.view_cols < 1) {
    return { ErrorCode::kInvalidArgument, "the view grid must be at least 1 x 1 cells" };
  }
  if (static_cast<long long>(request.view_rows) * static_cast<long long>(request.view_cols) > kMaxMapPixels) {
    return { ErrorCode::kInvalidArgument,
             "the view grid may carry at most " + std::to_string(kMaxMapPixels) + " cells" };
  }
  if (!request.full_sky) {
    if (!std::isfinite(request.centre_altitude_deg) || !std::isfinite(request.centre_azimuth_deg) ||
        request.centre_altitude_deg < -90.0 || request.centre_altitude_deg > 90.0) {
      return { ErrorCode::kInvalidTarget,
               "the view centre's altitude must be finite and in [-90, 90], the azimuth finite" };
    }
    if (!std::isfinite(request.delta_span_deg) || !std::isfinite(request.azimuth_span_deg) ||
        request.delta_span_deg <= 0.0 || request.azimuth_span_deg <= 0.0) {
      return { ErrorCode::kInvalidArgument, "the view spans must be finite positive degrees" };
    }
  }
  if (request.wavelengths_nm.size() > 1) {
    return { ErrorCode::kInvalidArgument,
             "at most one reference wavelength may be requested; the per-lambda map family is a later face" };
  }
  for (const double nm : request.wavelengths_nm) {
    if (!std::isfinite(nm) || nm <= 0.0) {
      return { ErrorCode::kWavelengthOutOfRange, "wavelengths must be positive numbers of nanometres" };
    }
  }

  MapData result;
  MapMetadata& meta = result.meta;
  meta.analytic_api_version = analytic::kApiVersion;
  meta.crystal_id = request.crystal_id;

  const LightSourceConfig& light = config.scene_.light_source_;

  WavelengthChoice choice;
  const std::optional<double> requested_nm =
      request.wavelengths_nm.empty() ? std::nullopt : std::optional<double>(request.wavelengths_nm.front());
  if (Error e = ResolveWavelength(light, requested_nm, &choice); !e.Ok()) {
    return e;
  }
  meta.wavelength = { choice.wavelength_nm, choice.source, choice.refractive_index };

  const CrystalConversion& crystal = geometry->conversion;
  const analytic::FaceNormalTable& table = geometry->table;
  const analytic::FacePolygonTable& polygons = geometry->polygons;

  meta.crystal_kind = crystal.kind;
  meta.shape = crystal.scalars;
  meta.shape_is_nominal = crystal.shape_is_nominal;
  meta.faces = request.path_layers.front();
  meta.sun_altitude_deg = light.param_.altitude_;
  meta.sun_azimuth_deg = light.param_.azimuth_;
  SunIncidentDirection(light.param_, meta.incident_direction);
  meta.sample_count = request.sample_count > 0 ? request.sample_count : kDefaultMapSampleCount;

  // The budget, resolved once at entry by the one owner: the unified query carries its own
  // deadline; a direct caller's 0 resolves to the documented default at this layer too.
  LegControl budget;
  if (request.query_deadline) {
    budget.stop = request.query_deadline->LegStop();
    budget.resolved_ms = request.query_deadline->resolved_ms;
  } else {
    BudgetRequest budget_request;
    budget_request.budget_ms = request.budget_ms;
    budget_request.interrupted = request.interrupted;
    budget = ResolveLeg(BudgetWindow::kMapMembers, budget_request);
  }
  meta.budget_ms = budget.resolved_ms;
  const analytic::DiscoveryStop stop{ budget.stop.deadline, request.interrupted, request.cancel };

  // The class scope: members (the expansion authority) and the density conversion. The
  // measure's expressibility is a document-level refusal, never a zero map.
  survey::CrystalSceneScope class_scope;
  if (Error e = survey::ResolveCrystalSceneScope(config, request, &class_scope); !e.Ok()) {
    return e;
  }
  result.members = static_cast<const MemberScope&>(class_scope);
  const auto quantity_input = MakeQuantityContext(config, request.crystal_id, result.members, meta);

  if (!class_scope.density.Ok()) {
    // The named refusal: the scene's axis distribution is not expressible in the map's pose
    // families. No silent substitution, never a zero texture.
    Refused refusal;
    refusal.reason = "measure_not_expressible: " + class_scope.density.unsupported;
    ClaimScope scope;
    scope.input = quantity_input;
    scope.members = result.members;
    refusal.asked_scope = scope;
    meta.execution = OutcomeOf(stop);
    result.claim = std::move(refusal);
    *out = FinalizeMapData(std::move(result));
    return {};
  }

  // The view: the named all-sky mode or the explicit window (the centre's probe-frame reading).
  const survey::ProbeFrame frame = survey::MakeProbeFrame(meta.incident_direction);
  double at_direction[3];
  AltAzToDir(request.centre_altitude_deg, request.centre_azimuth_deg, at_direction);
  const double at_deviation_deg =
      analytic::TargetDeviation(meta.incident_direction, at_direction) * math::kRadToDegreeD;
  const double at_azimuth_about_sun_deg = FrameAzimuthOf(frame, at_direction);
  const survey::MapViewDescription view = BuildView(request, frame, at_deviation_deg, at_azimuth_about_sun_deg);
  if (view.delta_max_deg - view.delta_min_deg <= 0.0) {
    return { ErrorCode::kInvalidArgument, "the view's delta window is empty after clamping to [0, 180]" };
  }

  survey::ReferenceMapData map = survey::EvaluateReferenceMap(
      table, polygons, class_scope.members, class_scope.density, frame, view, meta.wavelength.nm,
      meta.wavelength.refractive_index, meta.incident_direction, meta.sample_count, &stop);
  if (map.truncated) {
    // The cut map keeps its completed member prefix and names the stop; an honest early exit.
    meta.interrupted = true;
  }
  meta.execution = OutcomeOf(stop);

  MapValues values;
  values.quantity =
      DescribeQuantity(QuantityKind::kBandSum, QuantityUnit::kPerSteradian, QuantityMethod::kClassLatticePowerSum,
                       ReferenceMeasure::kHaar, DensityWeighting::kDeclaredPoseDensity);
  values.quantity.aggregation = MemberAggregation::kPhysicalClass;
  values.quantity.observation = { meta.wavelength.nm, meta.sun_altitude_deg, meta.sun_azimuth_deg };
  values.quantity.wavelengths_nm = { meta.wavelength.nm };
  values.map = std::move(map);

  Finite<MapValues> finite;
  finite.value = std::move(values);
  finite.coverage.members_evaluated = finite.value.map.members_evaluated;
  finite.coverage.sample_count = finite.value.map.kept_count;
  finite.coverage.wavelength_nm = meta.wavelength.nm;
  finite.coverage.geometry_basis = "nominal crystal shape at the distribution centres";
  ClaimScope scope;
  scope.input = quantity_input;
  scope.members = result.members;
  BindQuantityScope(&scope, finite.value.quantity);
  finite.scope = scope;
  result.claim = std::move(finite);
  *out = FinalizeMapData(std::move(result));
  return {};
}

Error AnalyzePathMap(const ConfigManager& config, const MapRequest& request, MapResult* out,
                     const PreparedGeometry* geometry) {
  if (out == nullptr) {
    return { ErrorCode::kInvalidArgument, "null map output" };
  }
  MapData data;
  if (const Error error = AnalyzePathMapData(config, request, &data, geometry); !error.Ok()) {
    return error;
  }
  *out = MapResult(std::move(data));
  return {};
}

}  // namespace lumice::raypath
