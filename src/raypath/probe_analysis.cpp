#include "raypath/probe_analysis.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <string>
#include <utility>

#include "analytic/pose_density.hpp"
#include "core/math.hpp"
#include "raypath/detail/quantity/probe_line.hpp"
#include "raypath/detail/survey/config_fingerprint.hpp"
#include "raypath/detail/survey/survey_assembly.hpp"
#include "raypath/prepared_geometry.hpp"
#include "raypath/query_control.hpp"
#include "raypath/scene_to_analytic.hpp"
#include "raypath/single_path_analysis.hpp"  // kMaxSampleCount — the sample bound the probe request echoes
#include "raypath/survey_result.hpp"
#include "util/sky_direction.hpp"

namespace lumice::raypath {

const char* ProbeAlongName(ProbeAlong along) {
  switch (along) {
    case ProbeAlong::kRadial:
      return "radial";
    case ProbeAlong::kEdgeNormal:
      return "edge-normal";
    case ProbeAlong::kArc:
      return "arc";
    case ProbeAlong::kTangential:
      return "tangential";
  }
  return "unknown";
}

namespace {

// The forms' declared semantics, spelled once each (the document states them, never implies).
const char* RadialSemantics() {
  return "the delta axis at the user's azimuth about the sun; under a uniform pose density the "
         "band-sum value is azimuth-free; otherwise warm values require the same line azimuth";
}

const char* EdgeNormalSemantics() {
  return "the radial geometry windowed at the context's nearest edge: the anchor is the closest of "
         "the support intervals' endpoints and the certified edge position to the user's "
         "deviation, the window anchor +/- the window half width; a context that carries no edge "
         "is a named refusal (no_edge_in_context), never a guessed anchor";
}

const char* ArcSemantics() {
  return "the constant-delta arc through the user's point: the current context carries only "
         "delta-axis facts, so this is the object curve's realization in the delta-axis "
         "vocabulary, swept over azimuth about the sun; a true curve-following profile needs the "
         "curve context and is not claimed here";
}

const char* TangentialSemantics() {
  return "the transverse crossing at the user's point: the arc geometry read as the line crossing "
         "the object sideways, swept over azimuth about the sun";
}

// The frame azimuth of a unit direction normal to the incident, in the probe frame.
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

// The arc forms' layout: the fixed-delta arc swept over azimuth about the sun, the wedge reading
// the same constant as a delta extent (the same ruler, roles swapped).
survey::ProbeLineLayout ArcLayout(double delta_deg, double azimuth_centre_deg) {
  survey::ProbeLineLayout layout;
  const double lo = azimuth_centre_deg - kProbeWindowHalfWidthDeg;
  const int bins = static_cast<int>(std::lround((2.0 * kProbeWindowHalfWidthDeg) / survey::kProbeBinDeg));
  const double half_wedge = 0.5 * survey::kProbeWedgeHalfWidthDeg * math::kDegreeToRadD;
  const double half_wedge_deg = 0.5 * survey::kProbeWedgeHalfWidthDeg;
  layout.bins.resize(static_cast<size_t>(bins));
  layout.delta_width_rad = 2.0 * half_wedge;
  layout.az_width_rad = survey::kProbeBinDeg * math::kDegreeToRadD;
  for (int k = 0; k < bins; k++) {
    const double az_lo = lo + static_cast<double>(k) * survey::kProbeBinDeg;
    survey::ProbeLineBin& bin = layout.bins[static_cast<size_t>(k)];
    bin.delta_centre_deg = delta_deg;
    bin.delta_lo_deg = delta_deg - half_wedge_deg;
    bin.delta_hi_deg = delta_deg + half_wedge_deg;
    bin.az_centre_deg = az_lo + 0.5 * survey::kProbeBinDeg;
    bin.az_lo_deg = az_lo;
    bin.az_hi_deg = az_lo + survey::kProbeBinDeg;
  }
  return layout;
}

// Fills the spec shell every profile carries, whatever the form's fate (a refusal still declares
// what the request fixed).
void FillProfileSpec(ProbeProfile* profile, const ProbeRequest& request, const ProbeMetadata& meta,
                     const std::vector<int>& representative, int member_count) {
  profile->along = request.along;
  profile->azimuth_deg = meta.at_azimuth_about_sun_deg;
  profile->bin_deg = survey::kProbeBinDeg;
  profile->wedge_half_width_deg = survey::kProbeWedgeHalfWidthDeg;
  profile->lattice = "antipodal_fibonacci";
  profile->deterministic = true;
  profile->representative = representative;
  profile->member_count = member_count;
  profile->semantics = request.along == ProbeAlong::kRadial     ? RadialSemantics() :
                       request.along == ProbeAlong::kEdgeNormal ? EdgeNormalSemantics() :
                       request.along == ProbeAlong::kArc        ? ArcSemantics() :
                                                                  TangentialSemantics();
}

// The arc form's degenerate band: an arc through the sun or its antipode is not a circle there
// (the named refusal's threshold, spelled once).
constexpr double kArcDegenerateNearDeg = 0.05;
constexpr double kArcDegenerateFarDeg = 179.95;

// The dark-region decomposition uses support validated for this profile's wavelength.
// Inside the support, the weak tail (k = 0) stays its own finite-zero class — never clamped into
// a written zero; outside is the geometrically dark side; a context that did not certify the
// support propagates one "unknown" run with its named cause. The segment bounds read the line's
// sweep coordinate: the delta axis for the axis forms, the azimuth for the arc forms.
std::vector<ProbeDarkSegment> DecomposeDark(const survey::ProbeLineLayout& layout, const SurveyStructureView& structure,
                                            const std::vector<survey::ProbeLineSample>& samples, bool arc_form) {
  std::vector<ProbeDarkSegment> segments;
  if (samples.empty()) {
    return segments;
  }
  const auto classify = [&](size_t k) {
    if (!structure) {
      return std::pair<ProbeDarkKind, std::string>{ ProbeDarkKind::kUnknown, structure.Reason() };
    }
    const double pos = layout.bins[k].delta_centre_deg;
    for (const SurveyWarmSource::SupportInterval& interval : structure.Facts()->intervals) {
      if (pos >= interval.lower_deg && pos <= interval.upper_deg) {
        return std::pair<ProbeDarkKind, std::string>{ samples[k].k > 0 ? ProbeDarkKind::kInsideSupportPositive :
                                                                         ProbeDarkKind::kInsideSupportZero,
                                                      std::string() };
      }
    }
    return std::pair<ProbeDarkKind, std::string>{ ProbeDarkKind::kOutsideSupport, std::string() };
  };
  const auto lo_of = [&](size_t k) { return arc_form ? layout.bins[k].az_lo_deg : layout.bins[k].delta_lo_deg; };
  const auto hi_of = [&](size_t k) { return arc_form ? layout.bins[k].az_hi_deg : layout.bins[k].delta_hi_deg; };
  ProbeDarkKind current_kind = ProbeDarkKind::kUnknown;
  std::string current_reason;
  double lo = 0.0, hi = 0.0;
  int count = 0;
  const auto flush = [&]() {
    ProbeDarkSegment segment;
    segment.kind = current_kind;
    segment.reason = current_reason;
    segment.lo_deg = lo;
    segment.hi_deg = hi;
    segment.bin_count = count;
    segments.push_back(std::move(segment));
  };
  for (size_t k = 0; k < samples.size(); k++) {
    const auto [kind, reason] = classify(k);
    if (count > 0 && (kind != current_kind || reason != current_reason)) {
      flush();
      count = 0;
    }
    if (count == 0) {
      current_kind = kind;
      current_reason = reason;
      lo = lo_of(k);
    }
    hi = hi_of(k);
    count++;
  }
  if (count > 0) {
    flush();
  }
  return segments;
}

ProbeProfile CompleteProbeProfile(ProbeProfile profile, const MemberScope& members,
                                  const std::shared_ptr<const QuantityContext>& input) {
  ClaimScope scope;
  scope.input = input;
  scope.members = members;
  scope.members.members = { profile.representative };
  scope.members.member_kind = MemberKind::kLiteral;
  BindQuantityScope(&scope, profile.quantity);
  profile.BindScope(scope);
  return profile;
}

}  // namespace

Error AnalyzePathProbeData(const ConfigManager& config, const ProbeRequest& request, ProbeData* out,
                           const PreparedGeometry* prepared) {
  *out = ProbeData{};
  std::optional<AdmittedWarmSource> admitted;
  if (request.warm_candidate) {
    if (Error e = ValidateWarmCandidate(config, request, *request.warm_candidate, &admitted); !e.Ok())
      return e;
  }
  const AdmittedWarmSource* source =
      admitted && admitted->Facts().family == WarmDocumentFamily::kSurvey ? &*admitted : nullptr;

  // ---- inputs ----
  const auto crystal_it = config.crystals_.find(request.crystal_id);
  if (crystal_it == config.crystals_.end()) {
    return { ErrorCode::kUnknownCrystalId,
             "no crystal entry with id " + std::to_string(request.crystal_id) + " in the config" };
  }
  if (request.sample_count < 0 || request.sample_count > kMaxSampleCount) {
    return { ErrorCode::kInvalidArgument, "sample count must be 0 (the survey probe's count) or in [1, " +
                                              std::to_string(kMaxSampleCount) + "]; got " +
                                              std::to_string(request.sample_count) };
  }
  if (!std::isfinite(request.at_altitude_deg) || !std::isfinite(request.at_azimuth_deg) ||
      request.at_altitude_deg < -90.0 || request.at_altitude_deg > 90.0) {
    return { ErrorCode::kInvalidTarget, "the at point's altitude must be finite and in [-90, 90], the azimuth finite" };
  }
  for (const double nm : request.wavelengths_nm) {
    if (!std::isfinite(nm) || nm <= 0.0) {
      return { ErrorCode::kWavelengthOutOfRange, "wavelengths must be positive numbers of nanometres" };
    }
  }
  if (static_cast<int>(request.wavelengths_nm.size()) > kMaxProbeWavelengthCount) {
    return { ErrorCode::kInvalidArgument,
             "at most " + std::to_string(kMaxProbeWavelengthCount) + " wavelengths may be requested" };
  }
  if (request.budget_ms < 0 || request.budget_ms > kMaxProbeBudgetMs) {
    return { ErrorCode::kInvalidArgument,
             "budget_ms must be 0 (the default) or at most " + std::to_string(kMaxProbeBudgetMs) };
  }

  ProbeData result;
  ProbeMetadata& meta = result.meta;
  meta.analytic_api_version = analytic::kApiVersion;
  meta.crystal_id = request.crystal_id;

  const LightSourceConfig& light = config.scene_.light_source_;

  // The wavelengths: each entry its own resolution (the online lambda grid); empty = the point
  // resolution's single default.
  std::vector<WavelengthChoice> choices;
  if (request.wavelengths_nm.empty()) {
    WavelengthChoice choice;
    if (Error e = ResolveWavelength(light, std::nullopt, &choice); !e.Ok()) {
      return e;
    }
    choices.push_back(std::move(choice));
  } else {
    for (const double nm : request.wavelengths_nm) {
      WavelengthChoice choice;
      if (Error e = ResolveWavelength(light, nm, &choice); !e.Ok()) {
        return e;
      }
      choices.push_back(std::move(choice));
    }
  }
  for (const WavelengthChoice& choice : choices) {
    meta.wavelengths.push_back({ choice.wavelength_nm, choice.source, choice.refractive_index });
  }
  const double reference_nm = meta.wavelengths.front().nm;
  result.context.reference_wavelength_nm = reference_nm;

  // The geometry: prepared by a workspace when it already built the selection's tables, else
  // this query's own one build (the workspace's reuse read counts the builds).
  PreparedGeometry built;
  const PreparedGeometry* geometry = prepared != nullptr ? prepared : &built;
  if (prepared == nullptr) {
    if (Error e = PrepareGeometry(crystal_it->second, &built); !e.Ok()) {
      return e;
    }
  }
  const CrystalConversion& crystal = geometry->conversion;
  const analytic::FaceNormalTable& table = geometry->table;
  const analytic::FacePolygonTable& polygons = geometry->polygons;

  std::vector<int> slots;
  if (Error e = ResolveSingleLayerPath(request.path_layers, table, &slots); !e.Ok()) {
    return e;
  }

  meta.crystal_kind = crystal.kind;
  meta.shape = crystal.scalars;
  meta.upper_wedge_deg = crystal.upper_wedge_deg;
  meta.lower_wedge_deg = crystal.lower_wedge_deg;
  meta.shape_is_nominal = crystal.shape_is_nominal;
  meta.faces = request.path_layers.front();
  meta.sun_altitude_deg = light.param_.altitude_;
  meta.sun_azimuth_deg = light.param_.azimuth_;
  SunIncidentDirection(light.param_, meta.incident_direction);
  meta.at_altitude_deg = request.at_altitude_deg;
  meta.at_azimuth_deg = request.at_azimuth_deg;
  AltAzToDir(request.at_altitude_deg, request.at_azimuth_deg, meta.at_direction);
  meta.at_deviation_deg = analytic::TargetDeviation(meta.incident_direction, meta.at_direction) * math::kRadToDegreeD;

  const survey::ProbeFrame frame = survey::MakeProbeFrame(meta.incident_direction);
  meta.at_azimuth_about_sun_deg = FrameAzimuthOf(frame, meta.at_direction);

  // The budget and its deadline, resolved once at entry by the one owner (query_control.hpp —
  // 0 = the documented default at this layer too: the bridge resolves the same constant before
  // the call, a direct caller passes 0 and gets the identical semantics, and meta echoes the
  // resolved value). Polled at the per-lambda BandSum boundaries; a stopped run serializes the
  // profiles it has — meta.interrupted, an honest early exit, never a failure.
  BudgetRequest budget_request;
  budget_request.budget_ms = request.budget_ms;
  budget_request.interrupted = request.interrupted;
  const LegControl budget = ResolveLeg(BudgetWindow::kProbeLambdas, budget_request);
  meta.budget_ms = budget.resolved_ms;
  // The lambda boundaries' one stop: the resolved window with the query's cancel folded in.
  const analytic::DiscoveryStop stop{ budget.stop.deadline, request.interrupted, request.cancel };
  const auto stopped = [&stop]() {
    if (stop.token != nullptr) {
      stop.token->NotePoll(kPollProbeLambdas);
    }
    return stop.Stopped();
  };
  // The context's own stop is CANCEL-ONLY (ruling 4): the context cost is stoppable by an
  // explicit cancel, never by the lambda window — a leg cut does not interrupt it.
  const analytic::DiscoveryStop context_stop{ std::chrono::steady_clock::time_point::max(), request.interrupted,
                                              request.cancel };

  // ---- the context (the point query's block, read at the at point) ----
  SurveyContextBlock& context = result.context;
  const AdmittedWarmSource* warm = nullptr;
  std::optional<AdmittedWarmSource> inline_source;
  bool inline_refused = false;
  if (source != nullptr && source->Facts().index_present) {
    context.source = ContextSource::kWarm;
    warm = source;
  } else {
    context.source = source != nullptr ? ContextSource::kWarm : ContextSource::kInline;
    context.warm_index_absent = source != nullptr && !source->Facts().index_present;
    SurveyRequest survey_request;
    static_cast<PathSelection&>(survey_request) = request;
    survey_request.wavelength_nm = reference_nm;
    survey::SurveyResult doc;
    survey::SurveyAssemblyOptions context_options;
    context_options.context_only = true;
    context_options.stop = &context_stop;
    Error e = survey::AssembleSurvey(config, survey_request, &doc, context_options);
    if (context_stop.Stopped()) {
      inline_refused = true;
      context.cut_by_stop = true;
      context.object.state = survey::EvidenceState::kUnevaluated;
      context.object.reason = "stop_requested";
      context.edge.state = survey::EvidenceState::kUnevaluated;
      context.edge.reason = "stop_requested";
      context.band_sum.state = survey::EvidenceState::kUnevaluated;
      context.band_sum.reason = "stop_requested";
    } else if (e.Ok())
      e = ValidateWarmCandidate(config, request, HarvestSurveyResult(doc), &inline_source);
    if (!e.Ok()) {
      inline_refused = true;
      context.object.state = survey::EvidenceState::kUnevaluated;
      context.object.reason = "inline_survey_refused";
      context.edge.state = survey::EvidenceState::kUnevaluated;
      context.edge.reason = "inline_survey_refused";
      context.band_sum.state = survey::EvidenceState::kUnevaluated;
      context.band_sum.reason = "inline_survey_refused";
    } else {
      warm = &*inline_source;
    }
  }
  SurveyStructureView reference_structure = SurveyStructureView::Unavailable("inline_survey_refused");
  if (warm != nullptr) {
    FillSurveyContext(*warm, meta.at_deviation_deg, reference_nm, meta.sun_altitude_deg, meta.sun_azimuth_deg,
                      &context);
    reference_structure =
        EvaluateSurveyReuse(*warm, reference_nm, meta.sun_altitude_deg, meta.sun_azimuth_deg).structure;
  }

  // ---- the class scope and the density conversion (the survey assembly's own authority) ----
  survey::CrystalSceneScope class_scope;
  if (Error e = survey::ResolveCrystalSceneScope(config, request, &class_scope); !e.Ok()) {
    return e;
  }
  result.members = static_cast<const MemberScope&>(class_scope);
  QuantityContext quantity_context;
  quantity_context.members = result.members;
  quantity_context.crystal = crystal_it->second;
  quantity_context.light = light;
  quantity_context.observation = { reference_nm, light.param_.altitude_, light.param_.azimuth_ };
  const auto quantity_input = std::make_shared<const QuantityContext>(std::move(quantity_context));
  const int member_count = static_cast<int>(class_scope.members.size());
  const PoseDensityConversion& density = class_scope.density;
  const bool density_ok = density.Ok();

  // ---- the line ----
  // The radial warm sample is the producer's declared line. Only a uniform pose density
  // makes its value independent of the line's azimuth.
  const bool radial = request.along == ProbeAlong::kRadial;
  const bool windowed = request.along == ProbeAlong::kEdgeNormal;
  const bool arc_form = request.along == ProbeAlong::kArc || request.along == ProbeAlong::kTangential;

  // The edge-normal anchor: the closest of the support endpoints and the certified edge position
  // to the user's deviation — the context's own shared walk (the same winner the context's edge
  // block names); a context with neither is the named refusal.
  bool has_anchor = false;
  double anchor = 0.0;
  ContextEdgeBasis anchor_basis = ContextEdgeBasis::kSupportIntervalEndpoint;
  if (reference_structure) {
    has_anchor = NearestStructureAnchor(reference_structure, meta.at_deviation_deg, &anchor, &anchor_basis);
  }

  survey::ProbeLineLayout layout;
  if (arc_form) {
    layout = ArcLayout(meta.at_deviation_deg, meta.at_azimuth_about_sun_deg);
  } else if (windowed) {
    if (has_anchor) {
      const double lo = std::max(0.0, anchor - kProbeWindowHalfWidthDeg);
      const double hi = std::min(180.0, anchor + kProbeWindowHalfWidthDeg);
      layout = survey::AxisLineLayout(lo, hi, meta.at_azimuth_about_sun_deg);
    }
  } else {
    layout =
        survey::AxisLineLayout(survey::kProbeDeltaMinDeg, survey::kProbeDeltaMaxDeg, meta.at_azimuth_about_sun_deg);
  }

  for (size_t w = 0; w < meta.wavelengths.size(); w++) {
    if (stopped()) {
      meta.interrupted = true;
      break;
    }
    ProbeProfile profile;
    FillProfileSpec(&profile, request, meta, request.path_layers[0], member_count);
    // An undetermined extent (the edge-normal refusal with no window to lay out) is NaN — the
    // serializer's null convention spells it; a written 0.0 would read as a degenerate line.
    profile.delta_min_deg =
        layout.bins.empty() ? std::numeric_limits<double>::quiet_NaN() : layout.bins.front().delta_lo_deg;
    profile.delta_max_deg =
        layout.bins.empty() ? std::numeric_limits<double>::quiet_NaN() : layout.bins.back().delta_hi_deg;
    // The arc forms sweep an azimuth window of the same half width (ArcLayout's own constant),
    // so they echo it too; "0 = the full axis" stays the axis forms' reading alone.
    profile.window_half_width_deg = (windowed || arc_form) ? kProbeWindowHalfWidthDeg : 0.0;
    profile.wavelength_nm = meta.wavelengths[w].nm;
    profile.quantity.context = quantity_input;
    profile.quantity.aggregation = MemberAggregation::kRepresentative;
    profile.quantity.observation = { profile.wavelength_nm, meta.sun_altitude_deg, meta.sun_azimuth_deg };
    profile.quantity.wavelengths_nm = { profile.wavelength_nm };
    profile.refractive_index = meta.wavelengths[w].refractive_index;
    profile.sample_count = request.sample_count > 0 ? request.sample_count : survey::kSurveyProbeSampleCount;
    if (windowed && has_anchor) {
      profile.anchor_basis = anchor_basis;
      profile.anchor_deg = anchor;
      profile.anchor_wavelength_nm = reference_nm;
    }

    const bool edge_refused = windowed && !has_anchor;
    const bool arc_refused =
        arc_form && (meta.at_deviation_deg <= kArcDegenerateNearDeg || meta.at_deviation_deg >= kArcDegenerateFarDeg);
    if (edge_refused) {
      // The refusal's cause: a context that names no edge is the form's own refusal; an inline
      // assembly that never produced a context is the infrastructure's, named for what failed.
      profile.Refuse(inline_refused ? "inline_survey_refused" : "no_edge_in_context");
      // No dark_regions on a refused profile: with no samples the decomposition is empty by
      // construction, and doc §10.2 marks the field Finite only.
      result.profiles.push_back(CompleteProbeProfile(std::move(profile), class_scope, quantity_input));
      continue;
    }
    if (arc_refused) {
      profile.Refuse("degenerate_arc_at_pole");
      result.profiles.push_back(CompleteProbeProfile(std::move(profile), class_scope, quantity_input));
      continue;
    }
    if (!density_ok) {
      profile.Refuse("measure_not_expressible");
      result.profiles.push_back(CompleteProbeProfile(std::move(profile), class_scope, quantity_input));
      continue;
    }

    // Reuse or recompute. Radial only: the warm map's slice is the radial line; the windowed and
    // arc geometries are this query's own, always recomputed (lattice-deterministic, no seed).
    const SurveyReuseDecision reuse =
        warm != nullptr ?
            EvaluateSurveyReuse(*warm, profile.wavelength_nm, meta.sun_altitude_deg, meta.sun_azimuth_deg) :
            SurveyReuseDecision{};
    const ProfileReuseScope profile_scope{ { profile.wavelength_nm, meta.sun_altitude_deg, meta.sun_azimuth_deg },
                                           {},
                                           meta.at_azimuth_about_sun_deg,
                                           survey::kProbeBinDeg,
                                           profile.sample_count,
                                           radial };
    const bool reused = warm != nullptr && CanReuseSurveyProfile(*warm, profile_scope, density, layout);
    ProbeFiniteReading& reading = profile.BeginFinite();
    if (reused) {
      reading.source = ProbeProfileSource::kWarmReused;
      reading.members_evaluated = 1;
      reading.samples.resize(layout.bins.size());
      for (size_t k = 0; k < layout.bins.size(); k++) {
        survey::ProbeLineSample& sample = reading.samples[k];
        sample.delta_deg = layout.bins[k].delta_centre_deg;
        sample.azimuth_deg = layout.bins[k].az_centre_deg;
        const auto& samples = warm->Facts().probe_samples;
        sample.value = samples[k].value;
        sample.k = samples[k].k;
        sample.k_eff = samples[k].k_eff;
      }
    } else {
      reading.source = ProbeProfileSource::kRecomputed;
      survey::ProbeLineProfile line = survey::EvaluateProbeLine(
          table, polygons, request.path_layers[0], density, frame, layout, meta.wavelengths[w].nm,
          meta.wavelengths[w].refractive_index, meta.incident_direction, profile.sample_count, &stop);
      if (line.truncated) {
        // The cut line's prefix bins are not the full lattice: the profile refuses with the
        // stop's name and claims no readings it did not run.
        profile.Refuse("stop_requested");
        result.profiles.push_back(CompleteProbeProfile(std::move(profile), class_scope, quantity_input));
        meta.interrupted = true;
        continue;
      }
      reading.members_evaluated = 1;
      reading.samples = std::move(line.samples);
    }

    // The readings: the edge (first non-empty bin) and the peak (the argmax), two things, on the
    // line's sweep coordinate — the deviation for the axis forms, the azimuth for the arc forms
    // (the rule the dark decomposition's bounds already follow; on a constant-delta arc a delta
    // reading would be a constant echo, a position of nothing).
    std::vector<double> centres(reading.samples.size());
    std::vector<double> values(reading.samples.size());
    std::vector<int> counts(reading.samples.size());
    for (size_t k = 0; k < reading.samples.size(); k++) {
      centres[k] = arc_form ? reading.samples[k].azimuth_deg : reading.samples[k].delta_deg;
      values[k] = reading.samples[k].value;
      counts[k] = reading.samples[k].k;
    }
    const survey::ProbeReadings read = survey::ReadProbeProfile(centres, values, counts);
    reading.readings.has_edge = read.has_edge;
    reading.readings.edge_deg = read.edge_delta_deg;
    reading.readings.has_peak = read.has_peak;
    reading.readings.peak_deg = read.peak_delta_deg;
    reading.readings.peak_value = read.peak_value;
    reading.readings.separation_deg = read.separation_deg;

    const SurveyStructureView profile_structure =
        warm != nullptr ? reuse.structure : SurveyStructureView::Unavailable("inline_survey_refused");
    reading.dark_regions = DecomposeDark(layout, profile_structure, reading.samples, arc_form);
    result.profiles.push_back(CompleteProbeProfile(std::move(profile), class_scope, quantity_input));
  }
  if (stopped()) {
    meta.interrupted = true;
  }
  meta.execution = OutcomeOf(stop);

  // The red-edge width: the 650 nm profile's edge minus the reference (first) profile's edge,
  // when both exist and differ as profiles. An axis-form quantity — the edges it subtracts sit
  // on the deviation axis (the C09 anchor's delta-axis quantity), and an arc form's edges would
  // be azimuth positions with no width meaning.
  const double kRedNm = kChromaticRedNm;
  if (result.profiles.size() >= 2 && !arc_form) {
    const ProbeProfile* red = nullptr;
    for (const ProbeProfile& p : result.profiles) {
      if (p.State() == survey::EvidenceState::kFinite && p.wavelength_nm == kRedNm) {
        red = &p;
        break;
      }
    }
    const ProbeProfile& reference = result.profiles.front();
    if (red != nullptr && &reference != red && reference.State() == survey::EvidenceState::kFinite &&
        reference.Reading().readings.has_edge && red->Reading().readings.has_edge) {
      result.has_red_edge_width = true;
      result.red_edge_width_deg = red->Reading().readings.edge_deg - reference.Reading().readings.edge_deg;
    }
  }

  *out = FinalizeProbeData(std::move(result));
  return {};
}

Error AnalyzePathProbe(const ConfigManager& config, const ProbeRequest& request, ProbeResult* out,
                       const PreparedGeometry* geometry) {
  if (!out) {
    return { ErrorCode::kInvalidArgument, "null probe output" };
  }
  ProbeData data;
  if (const Error error = AnalyzePathProbeData(config, request, &data, geometry); !error.Ok()) {
    return error;
  }
  *out = ProbeResult(std::move(data));
  return {};
}

}  // namespace lumice::raypath
