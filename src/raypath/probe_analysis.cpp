#include "raypath/probe_analysis.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <string>
#include <utility>

#include "analytic/pose_density.hpp"
#include "core/math.hpp"
#include "raypath/detail/input_assembly.hpp"
#include "raypath/detail/physical_member_scope.hpp"
#include "raypath/detail/survey/config_fingerprint.hpp"
#include "raypath/detail/survey/survey_assembly.hpp"
#include "raypath/detail/survey/survey_document.hpp"
#include "raypath/scene_to_analytic.hpp"
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
  return "the delta axis at the user's azimuth about the sun; under the uniform density the "
         "band-sum value formula makes the profile azimuth-free, so the value does not depend on "
         "the --at azimuth, and the warm reuse criterion does not compare it either";
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

// The axis forms' layout: the survey probe's ruler over [delta_min, delta_max] at the line's
// azimuth (the built-in's own bin expressions, so a warm document's grid matches bit for bit).
survey::ProbeLineLayout AxisLayout(double delta_min_deg, double delta_max_deg, double azimuth_deg, int* bin_count) {
  survey::ProbeLineLayout layout;
  const int bins = static_cast<int>((delta_max_deg - delta_min_deg) / survey::kProbeBinDeg);
  *bin_count = bins;
  const double half_wedge = 0.5 * survey::kProbeWedgeHalfWidthDeg * math::kDegreeToRadD;
  const double half_wedge_deg = 0.5 * survey::kProbeWedgeHalfWidthDeg;
  layout.bins.resize(static_cast<size_t>(bins));
  layout.delta_width_rad = survey::kProbeBinDeg * math::kDegreeToRadD;
  layout.az_width_rad = 2.0 * half_wedge;
  for (int k = 0; k < bins; k++) {
    const double lo = delta_min_deg + static_cast<double>(k) * survey::kProbeBinDeg;
    survey::ProbeLineBin& bin = layout.bins[static_cast<size_t>(k)];
    bin.delta_centre_deg = lo + 0.5 * survey::kProbeBinDeg;
    bin.delta_lo_deg = lo;
    bin.delta_hi_deg = lo + survey::kProbeBinDeg;
    bin.az_centre_deg = azimuth_deg;
    bin.az_lo_deg = azimuth_deg - half_wedge_deg;
    bin.az_hi_deg = azimuth_deg + half_wedge_deg;
  }
  return layout;
}

// The arc forms' layout: the fixed-delta arc swept over azimuth about the sun, the wedge reading
// the same constant as a delta extent (the same ruler, roles swapped).
survey::ProbeLineLayout ArcLayout(double delta_deg, double azimuth_centre_deg, int* bin_count) {
  survey::ProbeLineLayout layout;
  const double lo = azimuth_centre_deg - kProbeWindowHalfWidthDeg;
  const int bins = static_cast<int>((2.0 * kProbeWindowHalfWidthDeg) / survey::kProbeBinDeg);
  *bin_count = bins;
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
  profile->along = ProbeAlongName(request.along);
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

// The edge-normal anchor: the closest of the support intervals' endpoints and the certified edge
// position to the user's deviation — recomputed from the harvest (the context block carries the
// distance to the edge; the line needs the position and its basis). The strict-lesser walk is the
// context's own: an endpoint wins a tie against the certified edge, the first interval's against
// the next.
bool EdgeAnchor(const SurveyWarmSource& warm, double deviation_deg, double* anchor, const char** basis) {
  bool has = false;
  double best = 0.0;
  const char* best_basis = "";
  for (const SurveyWarmSource::SupportInterval& interval : warm.intervals) {
    for (const double end : { interval.lower_deg, interval.upper_deg }) {
      if (!has || std::fabs(deviation_deg - end) < std::fabs(deviation_deg - best)) {
        best = end;
        best_basis = "support_interval_endpoint";
        has = true;
      }
    }
  }
  if (warm.has_edge && (!has || std::fabs(deviation_deg - warm.edge_deg) < std::fabs(deviation_deg - best))) {
    best = warm.edge_deg;
    best_basis = "caustic_edge_position";
    has = true;
  }
  if (has) {
    *anchor = best;
    *basis = best_basis;
  }
  return has;
}

// The dark-region decomposition: consecutive bin runs classified against the context's support.
// Inside the support, the weak tail (k = 0) stays its own finite-zero class — never clamped into
// a written zero; outside is the geometrically dark side; a context that did not certify the
// support propagates one "unknown" run with its named cause. The segment bounds read the line's
// sweep coordinate: the delta axis for the axis forms, the azimuth for the arc forms.
std::vector<ProbeDarkSegment> DecomposeDark(const survey::ProbeLineLayout& layout, const SurveyContextBlock& context,
                                            const SurveyWarmSource* warm,
                                            const std::vector<survey::ProbeLineSample>& samples, bool arc_form) {
  std::vector<ProbeDarkSegment> segments;
  if (samples.empty()) {
    return segments;
  }
  const bool support_certified = context.object.state == "certified";
  const auto classify = [&](size_t k) {
    if (!support_certified) {
      return std::pair<std::string, std::string>{ "unknown", context.object.reason };
    }
    const double pos = layout.bins[k].delta_centre_deg;
    if (warm != nullptr) {
      for (const SurveyWarmSource::SupportInterval& interval : warm->intervals) {
        if (pos >= interval.lower_deg && pos <= interval.upper_deg) {
          return std::pair<std::string, std::string>{ samples[k].k > 0 ? "inside_support_positive" :
                                                                         "inside_support_zero",
                                                      std::string() };
        }
      }
    }
    return std::pair<std::string, std::string>{ "outside_support", std::string() };
  };
  const auto lo_of = [&](size_t k) { return arc_form ? layout.bins[k].az_lo_deg : layout.bins[k].delta_lo_deg; };
  const auto hi_of = [&](size_t k) { return arc_form ? layout.bins[k].az_hi_deg : layout.bins[k].delta_hi_deg; };
  std::string current_kind, current_reason;
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

}  // namespace

Error AnalyzePathProbe(const ConfigManager& config, const ProbeRequest& request, ProbeResult* out) {
  *out = ProbeResult{};

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

  ProbeResult result;
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

  const CrystalConversion crystal = ConvertCrystal(crystal_it->second.param_);
  analytic::FaceNormalTable table;
  analytic::FacePolygonTable polygons;
  switch (analytic::BuildFaceNormals(crystal.shape, &table, &polygons)) {
    case analytic::Status::kOk:
      break;
    case analytic::Status::kInvalidConfig:
      return { ErrorCode::kCrystalRejected,
               "the engine builds no crystal from entry " + std::to_string(request.crystal_id) + "'s nominal shape" };
    case analytic::Status::kInvalidValue:
      return { ErrorCode::kCrystalRejected, "crystal entry " + std::to_string(request.crystal_id) +
                                                "'s nominal shape has a value the kernel cannot take" };
  }

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

  meta.budget_ms = request.budget_ms > 0 ? request.budget_ms : 0;

  // The deadline (the point query's shape): resolved once at entry, polled at the per-lambda
  // BandSum boundaries; a stopped run serializes the profiles it has — meta.interrupted, an
  // honest early exit, never a failure.
  const analytic::DiscoveryStop stop{ request.budget_ms > 0 ? std::chrono::steady_clock::now() +
                                                                  std::chrono::milliseconds(request.budget_ms) :
                                                              std::chrono::steady_clock::time_point::max(),
                                      request.interrupted };
  const auto stopped = [&stop]() { return stop.Stopped(); };

  // ---- the context (the point query's block, read at the at point) ----
  SurveyContextBlock& context = result.context;
  const SurveyWarmSource* warm = nullptr;
  SurveyWarmSource inline_harvest;
  if (request.warm_survey.has_value() && request.warm_survey->index_present) {
    context.source = "warm";
    warm = &*request.warm_survey;
  } else {
    context.source = request.warm_survey.has_value() ? "warm" : "inline";
    context.warm_index_absent = request.warm_survey.has_value() && !request.warm_survey->index_present;
    SurveyRequest survey_request;
    survey_request.crystal_id = request.crystal_id;
    survey_request.path_layers = request.path_layers;
    survey_request.symmetry_bits = survey::kDefaultSymmetryBits;
    survey_request.wavelength_nm = reference_nm;
    survey::SurveyDocument doc;
    const Error e = survey::AssembleSurvey(config, survey_request, &doc, survey::SurveyAssemblyOptions{ true });
    if (!e.Ok()) {
      context.object.state = "unevaluated";
      context.object.reason = "inline_survey_refused";
      context.edge.state = "unevaluated";
      context.edge.reason = "inline_survey_refused";
      context.band_sum.state = "unevaluated";
      context.band_sum.reason = "inline_survey_refused";
    } else {
      inline_harvest = HarvestSurveyDocument(doc);
      warm = &inline_harvest;
    }
  }
  if (warm != nullptr) {
    FillSurveyContext(*warm, meta.at_deviation_deg, reference_nm, meta.sun_altitude_deg, meta.sun_azimuth_deg,
                      &context);
  }

  // ---- the class scope and the density conversion (the survey assembly's own authorities) ----
  SceneConfig scene = config.scene_;
  size_t layer_index = scene.ms_.size();
  for (size_t i = 0; i < scene.ms_.size(); ++i) {
    if (std::any_of(scene.ms_[i].setting_.begin(), scene.ms_[i].setting_.end(),
                    [&](const auto& setting) { return setting.crystal_.id_ == request.crystal_id; })) {
      layer_index = i;
      break;
    }
  }
  if (layer_index == scene.ms_.size()) {
    ScatteringSetting setting{};
    setting.crystal_ = crystal_it->second;
    setting.crystal_proportion_ = 1;
    scene.ms_.push_back({ 0, { setting } });
  }
  const std::string identity = layer_index >= config.scene_.ms_.size() ?
                                   "standalone configured crystal (not a scene allocation)" :
                                   "selected scene entry snapshot";
  const std::vector<LayerSelection> selection{ { layer_index, request.crystal_id, request.path_layers[0],
                                                 survey::kDefaultSymmetryBits } };
  InputSnapshot snapshot;
  if (Error e = CaptureInput(scene, identity, selection, &snapshot); !e.Ok()) {
    return e;
  }
  PhysicalMemberScope scope;
  if (Error e = ResolvePhysicalMemberScope(snapshot.layers.front(), &scope); !e.Ok()) {
    return e;
  }
  const int member_count = static_cast<int>(scope.members.size());
  const PoseDensityConversion density = ConvertAxisToPoseDensity(crystal_it->second.axis_);
  const bool density_ok = density.Ok();

  // ---- the line ----
  // The radial form's reuse side: the warm document's radial samples, when the scope matches and
  // the grid is this layout's grid bit for bit. The premise is the ruler constants being schema
  // (the profile's spec comment says so); the azimuth is deliberately absent from the comparison —
  // the radial profile's value is azimuth-free, the form's declared semantics.
  const bool radial = request.along == ProbeAlong::kRadial;
  const bool windowed = request.along == ProbeAlong::kEdgeNormal;
  const bool arc_form = request.along == ProbeAlong::kArc || request.along == ProbeAlong::kTangential;

  // The edge-normal anchor: the closest of the support endpoints and the certified edge position
  // to the user's deviation — recomputed from the harvest (the same walk the context's edge block
  // ran); a context with neither is the named refusal.
  bool has_anchor = false;
  double anchor = 0.0;
  const char* anchor_basis = "";
  if (warm != nullptr) {
    has_anchor = EdgeAnchor(*warm, meta.at_deviation_deg, &anchor, &anchor_basis);
  }

  survey::ProbeLineLayout layout;
  int bins = 0;
  if (arc_form) {
    const bool degenerate = meta.at_deviation_deg <= 0.05 || meta.at_deviation_deg >= 179.95;
    layout = ArcLayout(meta.at_deviation_deg, meta.at_azimuth_about_sun_deg, &bins);
  } else if (windowed) {
    if (!has_anchor) {
      // handled per-profile below
    } else {
      const double lo = std::max(0.0, anchor - kProbeWindowHalfWidthDeg);
      const double hi = std::min(180.0, anchor + kProbeWindowHalfWidthDeg);
      layout = AxisLayout(lo, hi, meta.at_azimuth_about_sun_deg, &bins);
    }
  } else {
    layout = AxisLayout(survey::kProbeDeltaMinDeg, survey::kProbeDeltaMaxDeg, meta.at_azimuth_about_sun_deg, &bins);
  }

  for (size_t w = 0; w < meta.wavelengths.size(); w++) {
    if (stopped()) {
      meta.interrupted = true;
      break;
    }
    ProbeProfile profile;
    FillProfileSpec(&profile, request, meta, request.path_layers[0], member_count);
    profile.delta_min_deg = layout.bins.empty() ? 0.0 : layout.bins.front().delta_lo_deg;
    profile.delta_max_deg = layout.bins.empty() ? 0.0 : layout.bins.back().delta_hi_deg;
    profile.window_half_width_deg = windowed ? kProbeWindowHalfWidthDeg : 0.0;
    profile.wavelength_nm = meta.wavelengths[w].nm;
    profile.refractive_index = meta.wavelengths[w].refractive_index;
    profile.sample_count = request.sample_count > 0 ? request.sample_count : survey::kSurveyProbeSampleCount;
    if (windowed && has_anchor) {
      profile.anchor_basis = anchor_basis;
      profile.anchor_deg = anchor;
    }

    const bool edge_refused = windowed && !has_anchor;
    const bool arc_refused = arc_form && (meta.at_deviation_deg <= 0.05 || meta.at_deviation_deg >= 179.95);
    if (edge_refused) {
      profile.evidence_state = "refused";
      profile.refused_mode = "no_edge_in_context";
      profile.dark_regions = DecomposeDark(layout, context, warm, {}, arc_form);
      result.profiles.push_back(std::move(profile));
      continue;
    }
    if (arc_refused) {
      profile.evidence_state = "refused";
      profile.refused_mode = "degenerate_arc_at_pole";
      profile.dark_regions = DecomposeDark(layout, context, warm, {}, arc_form);
      result.profiles.push_back(std::move(profile));
      continue;
    }
    if (!density_ok) {
      profile.evidence_state = "refused";
      profile.refused_mode = "measure_not_expressible";
      profile.dark_regions = DecomposeDark(layout, context, warm, {}, arc_form);
      result.profiles.push_back(std::move(profile));
      continue;
    }

    // Reuse or recompute. Radial only: the warm map's slice is the radial line; the windowed and
    // arc geometries are this query's own, always recomputed (lattice-deterministic, no seed).
    bool reused = false;
    if (radial && warm != nullptr && warm->has_probe && warm->has_scope &&
        meta.wavelengths[w].nm == warm->wavelength_nm && meta.sun_altitude_deg == warm->sun_altitude_deg &&
        meta.sun_azimuth_deg == warm->sun_azimuth_deg && warm->probe_bin_deg == survey::kProbeBinDeg &&
        warm->probe_sample_count == profile.sample_count && warm->probe_samples.size() == layout.bins.size()) {
      reused = true;
      for (size_t k = 0; k < layout.bins.size(); k++) {
        if (warm->probe_samples[k].delta_deg != layout.bins[k].delta_centre_deg) {
          reused = false;
          break;
        }
      }
    }
    if (reused) {
      profile.source = "warm_reused";
      profile.evidence_state = "finite";
      profile.members_evaluated = 1;
      profile.samples.resize(layout.bins.size());
      for (size_t k = 0; k < layout.bins.size(); k++) {
        survey::ProbeLineSample& sample = profile.samples[k];
        sample.delta_deg = layout.bins[k].delta_centre_deg;
        sample.azimuth_deg = layout.bins[k].az_centre_deg;
        sample.value = warm->probe_samples[k].value;
        sample.k = warm->probe_samples[k].k;
        sample.k_eff = warm->probe_samples[k].k_eff;
      }
    } else {
      profile.source = "recomputed";
      std::vector<survey::ProbeLineProfile> profiles = survey::EvaluateProbeLine(
          table, polygons, request.path_layers[0], density, frame, layout, { meta.wavelengths[w].nm },
          { meta.wavelengths[w].refractive_index }, meta.incident_direction, profile.sample_count);
      profile.evidence_state = "finite";
      profile.members_evaluated = 1;
      profile.samples = std::move(profiles.front().samples);
    }

    // The readings: the edge (first non-empty bin) and the peak (the argmax), two things.
    std::vector<double> centres(profile.samples.size());
    std::vector<double> values(profile.samples.size());
    std::vector<int> counts(profile.samples.size());
    for (size_t k = 0; k < profile.samples.size(); k++) {
      centres[k] = profile.samples[k].delta_deg;
      values[k] = profile.samples[k].value;
      counts[k] = profile.samples[k].k;
    }
    const survey::ProbeReadings read = survey::ReadProbeProfile(centres, values, counts);
    profile.readings.has_edge = read.has_edge;
    profile.readings.edge_deg = read.edge_delta_deg;
    profile.readings.has_peak = read.has_peak;
    profile.readings.peak_deg = read.peak_delta_deg;
    profile.readings.peak_value = read.peak_value;
    profile.readings.separation_deg = read.separation_deg;

    profile.dark_regions = DecomposeDark(layout, context, warm, profile.samples, arc_form);
    result.profiles.push_back(std::move(profile));
  }
  if (stopped()) {
    meta.interrupted = true;
  }

  // The red-edge width: the 650 nm profile's edge minus the reference (first) profile's edge,
  // when both exist and differ as profiles.
  const double kRedNm = survey::kChromaticRedNm;
  if (result.profiles.size() >= 2) {
    const ProbeProfile* red = nullptr;
    for (const ProbeProfile& p : result.profiles) {
      if (p.evidence_state == "finite" && p.wavelength_nm == kRedNm) {
        red = &p;
        break;
      }
    }
    const ProbeProfile& reference = result.profiles.front();
    if (red != nullptr && &reference != red && reference.evidence_state == "finite" && reference.readings.has_edge &&
        red->readings.has_edge) {
      result.has_red_edge_width = true;
      result.red_edge_width_deg = red->readings.edge_deg - reference.readings.edge_deg;
    }
  }

  *out = std::move(result);
  return {};
}

}  // namespace lumice::raypath
