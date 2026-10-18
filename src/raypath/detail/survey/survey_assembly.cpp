#include "raypath/detail/survey/survey_assembly.hpp"

#include <algorithm>
#include <chrono>

#include "analytic/dp_boundary.hpp"
#include "analytic/dp_focus.hpp"
#include "analytic/path_rank.hpp"
#include "analytic/pose_density.hpp"
#include "raypath/detail/input_assembly.hpp"
#include "raypath/detail/physical_member_scope.hpp"
#include "raypath/detail/survey/survey_probe.hpp"
#include "raypath/detail/survey/survey_t1.hpp"
#include "raypath/scene_to_analytic.hpp"
#include "util/logger.hpp"

namespace lumice::raypath::survey {
namespace {

// The unevaluated skeleton's reasons, word for word what the schema documents: the legs named
// are not wired for that input class, and the record says so instead of reading as an empty
// result. The family-restricted one is the precise shape of gap 1 (routing.md: the T1 rank
// must be taken on the limit family, and the measure-to-family mapping is not wired) — a
// non-random class is not silently routed as if it were random.
constexpr const char* kNotWiredReason = "survey v1: T1/T2 not wired";
constexpr const char* kFamilyRestrictedReason =
    "family-restricted T1 routing not wired (gap: measure-to-family mapping)";

bool ValidPathLayers(const std::vector<std::vector<int>>& layers) {
  return !layers.empty() && std::all_of(layers.begin(), layers.end(), [](const auto& layer) {
    return layer.size() >= 2 && layer.size() <= 64 &&
           std::all_of(layer.begin(), layer.end(), [](int face) { return face > 0; });
  });
}

// The T0 verdict for one member: the kernel's classification, recorded with its own evidence
// state. `density` may be inexpressible — the rank-0 check is pure geometry and does not consume
// it, so the certificate arm runs either way and only the mechanism roll-up depends on it.
SurveyMemberVerdict MemberVerdictOf(const analytic::FaceNormalTable& table, const analytic::FacePolygonTable& polygons,
                                    const std::vector<int>& member, const PoseDensityConversion& density,
                                    double refractive_index, bool* any_escaped, std::string* escape_mode) {
  SurveyMemberVerdict verdict;
  verdict.member = member;
  std::vector<int> slots;
  // A class member is a relabeling of the representative under the same crystal, so its slots
  // resolve whenever the representative's do. A failure here is therefore an internal
  // inconsistency rather than a member property; the member is refused as such (mode
  // "member_resolution") — never an empty or defaulted verdict.
  const Error slot_error = ResolveSingleLayerPath({ member }, table, &slots);
  if (!slot_error.Ok()) {
    verdict.rank_zero = false;
    verdict.mechanism_unavailable = "member face sequence did not resolve: " + slot_error.message;
    verdict.evidence.state = EvidenceState::kRefused;
    verdict.evidence.mode = "member_resolution";
    *any_escaped = true;
    *escape_mode = "member_resolution";
    return verdict;
  }
  if (density.Ok()) {
    const analytic::FocusingClassification classification = analytic::Classify(
        table, polygons, slots.data(), static_cast<int>(slots.size()), density.spec, refractive_index);
    verdict.rank_zero = !classification.escaped && classification.halo_map_rank == 0;
    if (verdict.rank_zero) {
      verdict.mechanism = classification.Mechanism();
      verdict.evidence.state = EvidenceState::kCertified;
      verdict.evidence.method = verdict.mechanism;
      return verdict;
    }
    if (classification.escaped) {
      verdict.evidence.state = EvidenceState::kRefused;
      verdict.evidence.mode = analytic::WalkStatusName(classification.escape_status);
      *any_escaped = true;
      *escape_mode = verdict.evidence.mode;
      return verdict;
    }
    verdict.mechanism = classification.Mechanism();
    verdict.evidence.state = EvidenceState::kFinite;
    verdict.evidence.coverage.members_evaluated = 1;
    return verdict;
  }
  // The axis is not expressible as a pose density: the rank-0 certificate still runs (pure
  // geometry), the mechanism roll-up does not, and the card says why rather than staying silent.
  verdict.rank_zero = analytic::IsRankZeroPath(table, slots.data(), static_cast<int>(slots.size()));
  if (verdict.rank_zero) {
    verdict.mechanism = kCertifiedMethodPointMass;
    verdict.evidence.state = EvidenceState::kCertified;
    verdict.evidence.method = kCertifiedMethodPointMass;
    return verdict;
  }
  verdict.mechanism_unavailable = "axis distribution not expressible as a pose density: " + density.unsupported;
  verdict.evidence.state = EvidenceState::kFinite;
  verdict.evidence.coverage.members_evaluated = 1;
  return verdict;
}

}  // namespace

Error AssembleSurvey(const ConfigManager& config, const SurveyRequest& request, SurveyDocument* out) {
  if (out == nullptr) {
    return { ErrorCode::kInvalidArgument, "null document output" };
  }
  *out = {};
  const auto begin = std::chrono::steady_clock::now();

  if (!ValidPathLayers(request.path_layers)) {
    return { ErrorCode::kInvalidPath, "non-empty layers with 2..64 positive face numbers are required" };
  }
  if (request.symmetry_bits > 7 || request.budget_ms <= 0 || request.budget_ms > kMaxSurveyBudgetMs) {
    return { ErrorCode::kInvalidArgument, "invalid survey symmetry bits or budget request" };
  }

  const auto configured = config.crystals_.find(request.crystal_id);
  if (configured == config.crystals_.end()) {
    return { ErrorCode::kUnknownCrystalId, "unknown crystal id" };
  }

  // The wavelength: the user's if given, else the scene's single discrete wavelength, else the
  // default — the same resolution the target-fiber request spells, with its error.
  WavelengthChoice wavelength;
  if (Error error = ResolveWavelength(config.scene_.light_source_, request.wavelength_nm, &wavelength); !error.Ok()) {
    return error;
  }

  // The nominal crystal's geometry: the rank-0 certificate is algebraic on this shape.
  const CrystalConversion crystal = ConvertCrystal(configured->second.param_);
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
  // The request's own path: single layer, faces the crystal has (the report's error semantics).
  std::vector<int> slots;
  if (Error error = ResolveSingleLayerPath(request.path_layers, table, &slots); !error.Ok()) {
    return error;
  }

  // The class: physical P|B|D by default, the literal sequence at bits 0. A configured but
  // unused crystal stays a valid standalone diagnostic object (the report's layer rule).
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
    setting.crystal_ = configured->second;
    setting.crystal_proportion_ = 1;
    scene.ms_.push_back({ 0, { setting } });
  }
  const std::string identity = layer_index >= config.scene_.ms_.size() ?
                                   "standalone configured crystal (not a scene allocation)" :
                                   "selected scene entry snapshot";
  const std::vector<LayerSelection> selection{ { layer_index, request.crystal_id, request.path_layers[0],
                                                 request.symmetry_bits } };
  InputSnapshot snapshot;
  if (Error error = CaptureInput(scene, identity, selection, &snapshot); !error.Ok()) {
    return error;
  }
  PhysicalMemberScope scope;
  if (Error error = ResolvePhysicalMemberScope(snapshot.layers.front(), &scope); !error.Ok()) {
    return error;
  }
  const PoseDensityConversion density = ConvertAxisToPoseDensity(configured->second.axis_);

  // Scope and echo.
  out->schema_version = kSurveySchemaVersion;
  out->request.crystal_id = static_cast<long long>(request.crystal_id);
  out->request.path_layers = request.path_layers;
  out->request.symmetry_bits = request.symmetry_bits;
  out->request.wavelength_given = request.wavelength_nm.has_value();
  out->request.wavelength_nm = request.wavelength_nm.value_or(0.0);
  out->request.budget_ms = request.budget_ms;
  out->scene_identity = identity;
  if (layer_index < config.scene_.ms_.size()) {
    out->scene_layer = static_cast<long long>(layer_index);
  }
  out->members = scope.members;
  out->wavelength_nm = wavelength.wavelength_nm;
  out->wavelength_source = WavelengthSourceName(wavelength.source);
  out->refractive_index = wavelength.refractive_index;
  out->sun_altitude_deg = config.scene_.light_source_.param_.altitude_;
  out->sun_azimuth_deg = config.scene_.light_source_.param_.azimuth_;
  out->chromatic.state = EvidenceState::kUnevaluated;
  out->chromatic.reason = "survey v1: chromatic layer not wired";

  // The T0 leg, one verdict per member. The source_image position is the sun itself: a rank-0
  // path maps every pose to the incident direction, so the class's sky object IS the sun.
  bool all_rank_zero = true;
  bool any_escaped = false;
  std::string escape_mode;
  for (const auto& member : scope.members) {
    SurveyMemberVerdict verdict =
        MemberVerdictOf(table, polygons, member, density, wavelength.refractive_index, &any_escaped, &escape_mode);
    all_rank_zero = all_rank_zero && verdict.rank_zero;
    if (verdict.evidence.state == EvidenceState::kFinite) {
      verdict.evidence.coverage.members_evaluated = 1;
      verdict.evidence.coverage.wavelength_nm = wavelength.wavelength_nm;
    }
    out->member_verdicts.push_back(std::move(verdict));
  }

  if (all_rank_zero) {
    // The identity certificate closes the branch: one card for the whole class (the sky-side
    // object; the members are its preimage decomposition), then the early stop — no boundary
    // walk beyond the certificate's own, no delta axis, no fiber, no field work.
    out->early_stopped = true;
    SurveyObject card;
    card.kind = SurveyObjectKind::kSourceImage;
    card.members = scope.members;
    card.has_position = true;
    card.position_altitude_deg = out->sun_altitude_deg;
    card.position_azimuth_deg = out->sun_azimuth_deg;
    card.evidence.state = EvidenceState::kCertified;
    card.evidence.method = kCertifiedMethodPointMass;
    card.certificate = SurveyCertificate{};
    out->objects.push_back(std::move(card));
    out->evidence.state = EvidenceState::kCertified;
    out->evidence.method = kCertifiedMethodPointMass;
  } else {
    // No early stop: the T0 facts stand (finite — a not-rank-0 verdict is as algebraic as a
    // rank-0 one, and an escaped walk refuses loudly). A random-family class with an
    // expressible density routes into the T1 full-dimensional arm — the delta machine (the
    // T1 rank must be taken on the family, and full-dimensional IS the family here). Anything
    // else stays unevaluated with the reason naming exactly which leg is not wired.
    out->early_stopped = false;
    const bool full_dimensional = density.Ok() && density.spec.family == analytic::PoseFamily::kRandom;
    if (full_dimensional) {
      const T1Outcome outcome = AssembleT1Objects(table, polygons, scope.members, request.path_layers[0],
                                                  wavelength.refractive_index, wavelength.wavelength_nm, out);
      if (!outcome.ok) {
        // Fail closed downstream of the support: the cards after a refused support are not
        // produced, and the document carries the refusal.
        SurveyObject refused_support;
        refused_support.kind = SurveyObjectKind::kSupportSet;
        refused_support.members = scope.members;
        refused_support.evidence.state = EvidenceState::kRefused;
        refused_support.evidence.mode = outcome.refusal_mode;
        out->objects.clear();
        out->objects.push_back(std::move(refused_support));
        any_escaped = true;
        escape_mode = outcome.refusal_mode;
      } else {
        // The canonical probe: the class's radial profile at the document's reference
        // wavelength (the same ruler the certified edge reads). Never refuses — an empty
        // event set serializes as the honest zero profile.
        ProbeInput probe_input;
        probe_input.table = &table;
        probe_input.polygons = &polygons;
        probe_input.representative = &request.path_layers[0];
        probe_input.refractive_index = wavelength.refractive_index;
        probe_input.wavelength_nm = wavelength.wavelength_nm;
        SunIncidentDirection(config.scene_.light_source_.param_, probe_input.incident);
        probe_input.member_count = static_cast<int>(scope.members.size());
        RunSolarRadialProbe(probe_input, out);
        // The chromatic layer's v1 legs on the representative (orthogonal to the position
        // objects; the dispersion and diagnose legs are separate kernel paths, separate
        // sub-blocks).
        AssembleChromaticLayer(table, polygons, request.path_layers[0], density.spec,
                               static_cast<int>(scope.members.size()), out);
      }
    } else {
      SurveyUnevaluated unevaluated;
      for (const SurveyObjectKind kind : AllSurveyObjectKinds()) {
        if (kind != SurveyObjectKind::kSourceImage) {
          unevaluated.kinds.push_back(kind);
        }
      }
      unevaluated.reason = density.Ok() ? kFamilyRestrictedReason : kNotWiredReason;
      out->unevaluated.push_back(std::move(unevaluated));
    }
    if (any_escaped) {
      out->evidence.state = EvidenceState::kRefused;
      out->evidence.mode = escape_mode;
    } else {
      out->evidence.state = EvidenceState::kFinite;
      out->evidence.coverage.members_evaluated = static_cast<int>(scope.members.size());
      out->evidence.coverage.wavelength_nm = wavelength.wavelength_nm;
    }
  }

  out->budget_ms = request.budget_ms;
  out->total_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
  ILOG_INFO(GetGlobalLogger(), "[raypath survey] crystal {}, path class routed: {} member(s), early stop {}",
            request.crystal_id, scope.members.size(), out->early_stopped ? "taken" : "not taken");
  return {};
}

}  // namespace lumice::raypath::survey
