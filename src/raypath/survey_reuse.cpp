#include "raypath/survey_reuse.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "raypath/detail/input_scope.hpp"
#include "raypath/detail/measure_scope.hpp"
#include "raypath/detail/quantity/probe_line.hpp"
#include "raypath/detail/survey/config_fingerprint.hpp"
#include "raypath/survey_result.hpp"

namespace lumice::raypath {

// ---- the survey context ---------------------------------------------------------------------------

// The schema-4 value model's harvest: the facts the context reads, read once off the assembled
// document (the warm reader's JSON arm mirrors this — the two must read the same blocks, the
// dual-arm oracle test pins them field-for-field). Members are the card's first layer: the
// support partition ran on the entry faces, and both arms read that same layer, so a future
// multi-layer document changes the answer on both sides together or not at all. A support_set
// with zero intervals harvests nothing — has_support stays false and the context answers
// survey_carries_no_support, the same shape as a document without support at all.
WarmCandidate HarvestSurveyResult(const survey::SurveyResult& result) {
  const survey::SurveyData& doc = result.Data();
  WarmCandidate candidate;
  candidate.crystal_id = doc.request.crystal_id;
  candidate.path_layers = doc.request.path_layers;
  candidate.location = doc.scene_layer ?
                           ScopeLocation{ SelectionLocation::kSceneLayer, static_cast<size_t>(*doc.scene_layer) } :
                           ScopeLocation{ SelectionLocation::kStandalone, 0 };
  candidate.config_fingerprint = doc.warm_index.config_fingerprint;
  candidate.decode_stage = WarmDecodeStage::kPayload;
  for (const auto& seed : doc.warm_index.seeds)
    candidate.seeds.insert(candidate.seeds.end(), seed.begin(), seed.end());
  SurveyWarmSource& out = candidate.facts;
  out.family = WarmDocumentFamily::kSurvey;
  // The typed document, like its serializer, always carries a warm_index block.
  out.index_present = true;
  if (doc.warm_index.seeds.size() != doc.warm_index.seed_sources.size()) {
    candidate.decode_error = { ErrorCode::kInvalidArgument,
                               "warm index: the document's warm_index seeds and seed_sources disagree in length" };
  }
  out.geometry = doc.geometry;
  out.early_stopped = doc.early_stopped;
  out.has_wavelength = std::isfinite(doc.wavelength_nm) && doc.wavelength_nm > 0.0;
  out.wavelength_nm = doc.wavelength_nm;
  out.wavelength_source = doc.wavelength_source;
  out.has_sun_altitude = doc.sun_altitude_deg.has_value() && std::isfinite(*doc.sun_altitude_deg);
  out.sun_altitude_deg = doc.sun_altitude_deg.value_or(0.0);
  out.has_sun_azimuth = doc.sun_azimuth_deg.has_value() && std::isfinite(*doc.sun_azimuth_deg);
  out.sun_azimuth_deg = doc.sun_azimuth_deg.value_or(0.0);
  for (const survey::SurveyObject& card : doc.objects) {
    if (card.support_set.has_value()) {
      for (const survey::SurveyObject::Interval& interval : card.support_set->intervals) {
        SurveyWarmSource::SupportInterval out_interval;
        out_interval.lower_deg = interval.lower_deg;
        out_interval.upper_deg = interval.upper_deg;
        out_interval.members = card.members.empty() ? std::vector<int>{} : card.members.front();
        out_interval.source_members = card.members;
        out.intervals.push_back(std::move(out_interval));
      }
    }
    if (card.caustic_edge.has_value()) {
      out.has_edge = true;
      out.edge_deg = card.caustic_edge->position_deg;
      out.edge_members = card.members;
    }
  }
  out.has_support = !out.intervals.empty();
  if (!doc.probes.empty()) {
    const survey::SurveyProbe& probe = doc.probes.front();
    out.quantity = probe.spec.quantity;
    out.has_probe = true;
    out.has_probe_azimuth = probe.spec.azimuth_deg.has_value() && std::isfinite(*probe.spec.azimuth_deg);
    out.probe_azimuth_deg = probe.spec.azimuth_deg.value_or(0.0);
    out.probe_bin_deg = probe.spec.bin_deg;
    out.probe_sample_count = static_cast<int>(probe.spec.sample_count);
    for (const survey::SurveyProbeSample& sample : probe.samples) {
      out.probe_samples.push_back({ sample.delta_deg, sample.value, sample.k, sample.k_eff });
    }
  }
  out.has_measure_echo = doc.family_routing.has_value();
  if (out.has_measure_echo) {
    if (doc.family_routing->measure.kind != MeasureKind::kActualDeclared)
      out.quantity.measure = doc.family_routing->measure.kind;
    const auto read_dim = [](const survey::FamilyDimMeasure& dim, SurveyWarmSource::MeasureDimEcho* e) {
      e->kind = survey::FamilyDimKindName(dim.kind);
      e->center_deg = dim.center_deg;
      e->width_deg = dim.width_deg;
    };
    read_dim(doc.family_routing->measure.zenith, &out.measure_zenith);
    read_dim(doc.family_routing->measure.azimuth, &out.measure_azimuth);
    read_dim(doc.family_routing->measure.roll, &out.measure_roll);
  }
  return candidate;
}

SurveyReuseDecision EvaluateSurveyFacts(const AdmittedWarmSource& admitted, double wavelength_nm,
                                        double sun_altitude_deg, double sun_azimuth_deg) {
  const SurveyWarmSource& warm = admitted.Facts();
  SurveyReuseDecision decision;
  const ObservationScope source{ warm.has_wavelength ? std::optional<double>(warm.wavelength_nm) : std::nullopt,
                                 warm.has_sun_altitude ? std::optional<double>(warm.sun_altitude_deg) : std::nullopt,
                                 warm.has_sun_azimuth ? std::optional<double>(warm.sun_azimuth_deg) : std::nullopt };
  const ObservationScope target{ wavelength_nm, sun_altitude_deg, sun_azimuth_deg };
  if (warm.early_stopped) {
    decision.structure = SurveyStructureView::Unavailable("survey_early_stopped_no_delta_axis");
  } else if (!warm.has_support || warm.intervals.empty()) {
    decision.structure = SurveyStructureView::Unavailable("survey_carries_no_support");
  } else if (warm.geometry.kind != GeometryKind::kNominal) {
    decision.structure = SurveyStructureView::Unavailable("scope_value_mismatch");
  } else if (!source.wavelength_nm || !std::isfinite(*source.wavelength_nm) || *source.wavelength_nm <= 0.0) {
    decision.structure = SurveyStructureView::Unavailable("scope_field_missing");
  } else if (source.wavelength_nm != target.wavelength_nm) {
    decision.structure = SurveyStructureView::Unavailable("scope_value_mismatch");
  } else {
    decision.structure = SurveyStructureView::FromAdmitted(admitted);
  }
  if (!source.wavelength_nm || !source.sun_altitude_deg || !source.sun_azimuth_deg ||
      !std::isfinite(*source.wavelength_nm) || *source.wavelength_nm <= 0.0 ||
      !std::isfinite(*source.sun_altitude_deg) || !std::isfinite(*source.sun_azimuth_deg)) {
    decision.reading_reason = "scope_field_missing";
  } else if (source.wavelength_nm != target.wavelength_nm || source.sun_altitude_deg != target.sun_altitude_deg ||
             source.sun_azimuth_deg != target.sun_azimuth_deg) {
    decision.reading_reason = "scope_value_mismatch";
  }
  if (decision.reading_reason == nullptr &&
      (warm.quantity.kind != QuantityKind::kBandSum || warm.quantity.unit != QuantityUnit::kPerSteradian ||
       warm.quantity.aggregation != MemberAggregation::kRepresentative ||
       warm.quantity.measure != MeasureKind::kActualDeclared || warm.geometry.kind != GeometryKind::kNominal)) {
    decision.reading_reason = "scope_value_mismatch";
  }
  return decision;
}

SurveyReuseDecision EvaluateSurveyReuse(const AdmittedWarmSource& warm, double wavelength_nm, double sun_altitude_deg,
                                        double sun_azimuth_deg) {
  return EvaluateSurveyFacts(warm, wavelength_nm, sun_altitude_deg, sun_azimuth_deg);
}

bool NearestStructureAnchor(const SurveyStructureView& structure, double deviation_deg, double* position_deg,
                            const char** basis) {
  if (!structure) {
    return false;
  }
  const SurveyWarmSource& warm = *structure.Facts();
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
    *position_deg = best;
    *basis = best_basis;
  }
  return has;
}

// The context's answers, read off a harvest (warm document or inline assembly — the same shape).
static void FillSurveyFacts(const AdmittedWarmSource& admitted, double target_deviation_deg, double wavelength_nm,
                            double sun_altitude_deg, double sun_azimuth_deg, SurveyContextBlock* out) {
  out->structure.reset();
  out->object_ref.reset();
  out->edge_ref.reset();
  const SurveyWarmSource& warm = admitted.Facts();
  const SurveyReuseDecision reuse = EvaluateSurveyFacts(admitted, wavelength_nm, sun_altitude_deg, sun_azimuth_deg);
  const double delta = target_deviation_deg;
  const SurveyWarmSource::SupportInterval* containing = nullptr;
  const SurveyWarmSource::SupportInterval* closest = nullptr;
  double closest_distance = std::numeric_limits<double>::infinity();
  if (reuse.structure) {
    for (const SurveyWarmSource::SupportInterval& interval : warm.intervals) {
      if (delta >= interval.lower_deg && delta <= interval.upper_deg) {
        containing = &interval;
        break;
      }
      const double d = delta < interval.lower_deg ? interval.lower_deg - delta : delta - interval.upper_deg;
      if (d < closest_distance) {
        closest_distance = d;
        closest = &interval;
      }
    }
  }
  if (!reuse.structure) {
    out->object.state = "unevaluated";
    out->object.reason = reuse.structure.Reason();
    out->edge.state = "unevaluated";
    out->edge.reason = reuse.structure.Reason();
  } else {
    // The partition containment is a certified structural fact; the nearest reading inherits it.
    const SurveyWarmSource::SupportInterval& hit = containing != nullptr ? *containing : *closest;
    out->object.state = "certified";
    out->target_inside_support = containing != nullptr;
    out->object_kind = "support_set";
    out->object_members = hit.members;
    out->object_distance_deg = containing != nullptr ? 0.0 : closest_distance;
    // A certified support carries endpoints, so the shared walk always names an anchor here.
    double edge_position = 0.0;
    const char* edge_basis = "";
    NearestStructureAnchor(reuse.structure, delta, &edge_position, &edge_basis);
    out->edge.state = "finite";
    out->edge_distance_deg = std::fabs(delta - edge_position);
    out->edge_basis = edge_basis;
    MemberScope members;
    static_cast<ScopeLocation&>(members) = admitted.Location();
    members.member_kind = admitted.MembersKind();
    std::vector<ContextStructureObject> objects;
    std::vector<std::vector<MemberId>> object_members;
    const auto append = [&](ContextStructureObject object, const std::vector<std::vector<int>>& paths) {
      std::vector<MemberId> ids;
      for (const auto& path : paths) {
        auto found = std::find(members.members.begin(), members.members.end(), path);
        if (found == members.members.end()) {
          members.members.push_back(path);
          ids.push_back({ members.members.size() - 1 });
        } else {
          ids.push_back({ static_cast<size_t>(found - members.members.begin()) });
        }
      }
      objects.push_back(object);
      object_members.push_back(std::move(ids));
    };
    size_t object_index = 0;
    size_t edge_index = 0;
    bool edge_matched = false;
    for (size_t i = 0; i < warm.intervals.size(); ++i) {
      const auto& interval = warm.intervals[i];
      const auto paths = interval.source_members.empty() ?
                             (interval.members.empty() ? std::vector<std::vector<int>>{} :
                                                         std::vector<std::vector<int>>{ interval.members }) :
                             interval.source_members;
      append({ ContextStructureObject::Kind::kSupportInterval, interval.lower_deg, interval.upper_deg }, paths);
      if (&interval == &hit) {
        object_index = i;
      }
      if (!edge_matched && (edge_position == interval.lower_deg || edge_position == interval.upper_deg)) {
        edge_index = i;
        edge_matched = true;
      }
    }
    if (warm.has_edge) {
      if (out->edge_basis == "caustic_edge_position") {
        edge_index = objects.size();
      }
      append({ ContextStructureObject::Kind::kCausticEdge, warm.edge_deg, warm.edge_deg }, warm.edge_members);
    }
    out->structure = ContextStructure::Finalize(std::move(members), std::move(objects), std::move(object_members));
    if (out->structure) {
      out->object_ref = out->structure->Reference(ObjectId{ object_index });
      out->edge_ref = out->structure->Reference(ObjectId{ edge_index });
    }
  }

  // The band-sum reading: warm documents only, under the scope gate's exact-equality comparison
  // (both sides resolved the same way, so equal configs give bit-equal doubles here).
  if (out->source == "inline") {
    out->band_sum.state = "unevaluated";
    out->band_sum.reason = "inline_context_runs_no_probe";
  } else if (out->warm_index_absent) {
    out->band_sum.state = "unevaluated";
    out->band_sum.reason = "warm_index_absent";
  } else if (!warm.has_probe) {
    out->band_sum.state = "unevaluated";
    out->band_sum.reason = "warm_document_has_no_probe";
  } else if (reuse.reading_reason != nullptr) {
    out->band_sum.state = "unevaluated";
    out->band_sum.reason = reuse.reading_reason;
  } else {
    const SurveyWarmSource::ProbeSample* best = nullptr;
    double best_distance = std::numeric_limits<double>::infinity();
    for (const SurveyWarmSource::ProbeSample& sample : warm.probe_samples) {
      if (std::isnan(sample.value)) {
        continue;  // an undefined reading is no reading: the pick spans the defined ones
      }
      const double d = std::fabs(delta - sample.delta_deg);
      if (d < best_distance) {
        best_distance = d;
        best = &sample;
      }
    }
    if (best == nullptr) {
      out->band_sum.state = "unevaluated";
      out->band_sum.reason = "warm_document_has_no_probe";
    } else {
      out->band_sum.state = "finite";
      out->band_sum_value = best->value;
      out->band_sum_delta_deg = best->delta_deg;
      out->band_sum_bin_deg = warm.probe_bin_deg;
      out->band_sum_caliber = "the warm document's solar-radial band-sum profile at wavelength " +
                              std::to_string(warm.wavelength_nm) + " nm (" + warm.wavelength_source + "), sun at " +
                              std::to_string(warm.sun_altitude_deg) + "/" + std::to_string(warm.sun_azimuth_deg) +
                              ", survey line azimuth " +
                              (warm.has_probe_azimuth ? std::to_string(warm.probe_azimuth_deg) : "unknown") +
                              " — reference-line reading, not target-sky intensity";
    }
  }
}

void FillSurveyContext(const AdmittedWarmSource& warm, double target_deviation_deg, double wavelength_nm,
                       double sun_altitude_deg, double sun_azimuth_deg, SurveyContextBlock* out) {
  FillSurveyFacts(warm, target_deviation_deg, wavelength_nm, sun_altitude_deg, sun_azimuth_deg, out);
}

Error ValidateWarmCandidate(const ConfigManager& config, const PathSelection& selection, const WarmCandidate& candidate,
                            std::optional<AdmittedWarmSource>* out) {
  out->reset();
  const auto configured = config.crystals_.find(selection.crystal_id);
  if (configured == config.crystals_.end()) {
    return { ErrorCode::kUnknownCrystalId, "no crystal entry with id " + std::to_string(selection.crystal_id) };
  }
  if (!candidate.decode_error.Ok() && candidate.decode_stage == WarmDecodeStage::kEnvelope)
    return candidate.decode_error;
  const auto refuse = [](const std::string& why) { return Error{ ErrorCode::kInvalidArgument, "warm index: " + why }; };
  SurveyWarmSource facts = candidate.facts;
  if (facts.family != WarmDocumentFamily::kSeedSheet && facts.family != WarmDocumentFamily::kSurvey)
    return refuse("unknown warm document family");
  if (facts.family == WarmDocumentFamily::kSeedSheet) {
    if (facts.index_present || facts.has_support || facts.has_edge || facts.has_probe || facts.has_measure_echo) {
      return refuse("a seed sheet cannot carry survey facts");
    }
    *out = AdmittedWarmSource({}, candidate.seeds);
    return {};
  }
  if (candidate.crystal_id != selection.crystal_id) {
    return refuse("crystal gate: the document's request.crystal_id does not match this query");
  }
  if (!candidate.decode_error.Ok() && candidate.decode_stage == WarmDecodeStage::kPath)
    return candidate.decode_error;
  if (candidate.path_layers != selection.path_layers) {
    return refuse("path gate: the document's request.path_layers does not match this query");
  }
  if (!candidate.decode_error.Ok() && candidate.decode_stage == WarmDecodeStage::kMeasure)
    return candidate.decode_error;
  if (facts.index_present && candidate.config_fingerprint !=
                                 survey::RoutingConfigFingerprint(config, selection.crystal_id, selection.path_layers,
                                                                  selection.symmetry_bits)) {
    return refuse(
        "config fingerprint gate: the document's warm_index.config_fingerprint does not "
        "match this query's config (a config field in the routing projection changed)");
  }
  if (!candidate.decode_error.Ok())
    return candidate.decode_error;
  if (facts.has_measure_echo) {
    const survey::FamilyMeasureReading mine = survey::ReadFamilyMeasure(configured->second.axis_);
    const auto same = [](const SurveyWarmSource::MeasureDimEcho& echo, const survey::FamilyDimMeasure& dim) {
      return echo.kind == survey::FamilyDimKindName(dim.kind) && echo.center_deg == dim.center_deg &&
             echo.width_deg == dim.width_deg;
    };
    if (!mine.ok || mine.measure.kind != MeasureKind::kActualDeclared ||
        !same(facts.measure_zenith, mine.measure.zenith) || !same(facts.measure_azimuth, mine.measure.azimuth) ||
        !same(facts.measure_roll, mine.measure.roll)) {
      return refuse("measure gate: the document's family measure echo does not match this query's config axis");
    }
  }
  if (candidate.location) {
    const ScopeLocation current = survey::ResolveSelectionLocation(config, selection);
    if (candidate.location->location != current.location || (current.location == SelectionLocation::kSceneLayer &&
                                                             candidate.location->layer_index != current.layer_index)) {
      return refuse("scene layer gate: the document's scope.scene_layer does not match this query");
    }
  }
  if (!facts.index_present) {
    SurveyWarmSource absent;
    absent.family = WarmDocumentFamily::kSurvey;
    *out = AdmittedWarmSource(std::move(absent), {});
    return {};
  }
  // The same presence rule applies to typed producers and JSON numbers. In particular,
  // default zero is not a source receipt and a NaN cannot enter an equality gate.
  facts.has_wavelength = facts.has_wavelength && std::isfinite(facts.wavelength_nm) && facts.wavelength_nm > 0.0;
  facts.has_sun_altitude = facts.has_sun_altitude && std::isfinite(facts.sun_altitude_deg);
  facts.has_sun_azimuth = facts.has_sun_azimuth && std::isfinite(facts.sun_azimuth_deg);
  facts.has_probe_azimuth = facts.has_probe_azimuth && std::isfinite(facts.probe_azimuth_deg);
  for (const auto& interval : facts.intervals) {
    if (!std::isfinite(interval.lower_deg) || !std::isfinite(interval.upper_deg) ||
        interval.lower_deg > interval.upper_deg) {
      return refuse("corrupt warm document: invalid support interval");
    }
  }
  facts.has_support = !facts.intervals.empty();
  if (facts.has_edge && !std::isfinite(facts.edge_deg))
    return refuse("corrupt warm document: invalid edge");
  if (facts.has_probe) {
    if (!std::isfinite(facts.probe_bin_deg) || facts.probe_bin_deg < 0.0 || facts.probe_sample_count < 0) {
      return refuse("corrupt warm document: invalid probe scope");
    }
    for (const auto& sample : facts.probe_samples) {
      // NaN value is the wire's null (undefined bin); infinity is never a reading.
      if (!std::isfinite(sample.delta_deg) || std::isinf(sample.value) || sample.k < 0 ||
          !std::isfinite(sample.k_eff) || sample.k_eff < 0.0) {
        return refuse("corrupt warm document: invalid probe sample");
      }
    }
  }
  if (candidate.seeds.size() % 9 != 0 ||
      !std::all_of(candidate.seeds.begin(), candidate.seeds.end(), [](double x) { return std::isfinite(x); })) {
    return refuse("seeds must be finite row-major matrices of nine numbers");
  }
  if (!candidate.location) {
    SurveyWarmSource absent;
    absent.family = WarmDocumentFamily::kSurvey;
    *out = AdmittedWarmSource(std::move(absent), candidate.seeds);
    return {};
  }
  // A missing index is still a survey provenance marker, but carries neither seeds
  // nor numeric facts. The query assembles its own context under the existing rule.
  *out = AdmittedWarmSource(std::move(facts), candidate.facts.index_present ? candidate.seeds : std::vector<double>{});
  (*out)->location_ = survey::ResolveSelectionLocation(config, selection);
  (*out)->member_kind_ = selection.symmetry_bits == 0 ? MemberKind::kLiteral : MemberKind::kPhysicalL2;
  return {};
}

bool CanReuseSurveyProfile(const AdmittedWarmSource& admitted, const ProfileReuseScope& scope,
                           const PoseDensityConversion& density, const survey::ProbeLineLayout& layout) {
  const SurveyWarmSource& warm = admitted.Facts();
  if (!scope.observation.wavelength_nm || !scope.observation.sun_altitude_deg || !scope.observation.sun_azimuth_deg)
    return false;
  const SurveyReuseDecision reuse =
      EvaluateSurveyFacts(admitted, *scope.observation.wavelength_nm, *scope.observation.sun_altitude_deg,
                          *scope.observation.sun_azimuth_deg);
  const bool same_line = warm.has_probe_azimuth && std::isfinite(warm.probe_azimuth_deg) &&
                         std::fabs(std::remainder(scope.azimuth_deg - warm.probe_azimuth_deg, 360.0)) < 1e-9;
  if (!scope.radial || !warm.has_probe || reuse.reading_reason != nullptr ||
      density.measure.kind != MeasureKind::kActualDeclared || density.measure.reference != ReferenceMeasure::kHaar ||
      warm.quantity.kind != scope.quantity.kind || warm.quantity.unit != scope.quantity.unit ||
      warm.quantity.aggregation != scope.quantity.aggregation || warm.quantity.measure != scope.quantity.measure ||
      (density.spec.family != analytic::PoseFamily::kRandom && !same_line) || warm.probe_bin_deg != scope.bin_deg ||
      warm.probe_sample_count != scope.sample_count || warm.probe_samples.size() != layout.bins.size())
    return false;
  // Different FP contraction can move a serialized centre by an ulp, not a bin.
  constexpr double kGridTolDeg = 1e-9;
  for (size_t k = 0; k < layout.bins.size(); ++k) {
    if (!std::isfinite(warm.probe_samples[k].delta_deg) ||
        std::fabs(warm.probe_samples[k].delta_deg - layout.bins[k].delta_centre_deg) > kGridTolDeg)
      return false;
  }
  return true;
}

}  // namespace lumice::raypath
