#ifndef LUMICE_RAYPATH_RESULTS_SURVEY_RECORDS_HPP_
#define LUMICE_RAYPATH_RESULTS_SURVEY_RECORDS_HPP_

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include "analytic/quantity_types.hpp"
#include "raypath/domain/claim.hpp"
#include "raypath/domain/evidence.hpp"
#include "raypath/domain/measure.hpp"
#include "raypath/domain/object_kind.hpp"
#include "raypath/results/quantity.hpp"
#include "raypath/results/structure.hpp"


namespace lumice::raypath::survey {

// Evidence/coverage and the four-arm claim have their sole definitions in domain/.

// The rank-0 identity certificate's declared tolerances: the kernel's own constants
// (analytic/path_rank.hpp), restated on the card so a reader needs no kernel header. The
// certificate itself lives in the kernel (IsRankZeroPath); nothing here re-measures it.
struct SurveyCertificate {
  double fold_tolerance = analytic::kRankZeroFoldTolerance;
  double wedge_tolerance_deg = analytic::kRankZeroWedgeToleranceDeg;
};

// ---- the family routing block (the measure-to-family arm's own record) ----------------
//
// gap1-family-routing.md sections 2-3: the router reads each orientation dimension in the
// concentration language ({fixed | gauss | uniform_full} x (center, width)), never the
// distribution-type language; the ladder freezes the concentrated (gauss) zenith/roll
// dimensions tightest-first, asks the restricted-family rank at each level, and the survival
// filter (structure proposes, weight disposes) decides per object whether a frozen width's
// image blur stays inside the object's own angular scale. These structs are the schema-4
// APPEND that carries it; the block appears on exactly the documents the family arm produced
// (the probes key's own conditional-presence discipline — an early stop, a full-dimensional
// route and an outside-domain refusal carry none).

using structure::DimName;
using structure::FamilyBandVerdict;
using structure::FamilyFrozenDim;
using structure::FamilyKilledCandidate;
using structure::FamilyLadderLevel;

inline SurveyObjectKind SurveyKindOf(structure::FamilyImageKind kind) {
  switch (kind) {
    case structure::FamilyImageKind::kCollapsePoint:
      return SurveyObjectKind::kFamilyCollapsePoint;
    case structure::FamilyImageKind::kCausticEdge:
      return SurveyObjectKind::kCausticEdge;
    case structure::FamilyImageKind::kRegularImage:
      return SurveyObjectKind::kRegularImage;
    case structure::FamilyImageKind::kBrightDomain:
      return SurveyObjectKind::kBrightDomain;
  }
  return SurveyObjectKind::kFamilyCollapsePoint;
}

// The density-support tail rows on the family arm's support_set card (AC5's boundedness-role
// separation): the width entered the ladder, the support's own tail bounds are reported here.
// The v1 domain has no bounded uniform, so the only rows are a gauss zenith's PoseDensity
// window (the +-12 sigma floor of the band sum's own normalization integral).
struct FamilySupportTail {
  std::string dim;
  double lower_deg = 0.0;
  double upper_deg = 0.0;
};

// The whole block; carried by exactly the family arm's documents.
struct FamilyRoutingBlock {
  FamilyRoutingMeasure measure;
  std::vector<FamilyLadderLevel> ladder;
  SurveyEvidence evidence;
  // The level-0 delta partition's refusal mode, echoed at block level when the partition
  // refused (the same string the refused support card carries in evidence.mode — one producer,
  // two readers). Absent when the partition succeeded; a block-level reader need not walk
  // objects to learn it.
  std::optional<std::string> level0_refusal_mode;
};

// One object card. For the v1 source_image: `members` are the class members the card collapses
// (the card is the sky-side object; the members are its preimage decomposition), `position` is
// the sun's altitude/azimuth, `evidence` is certified with the method named, and `certificate`
// carries the declared tolerances. The payload variant determines the kind; each arm owns its own claim. A family
// representative and a delta edge remain different payloads.
struct SurveyObject {
  std::vector<std::vector<int>> members;
  bool has_position = false;
  double position_altitude_deg = 0.0;
  double position_azimuth_deg = 0.0;

  // --- kSupportSet payload: the class's delta-axis support -------------------------------
  // One partition interval (the kernel's DeviationInterval, degrees on the wire) and the
  // mechanism labels of the onsets sitting at an interval endpoint (kernel source/profile
  // slugs passed through — the vocabulary is the kernel's, not re-spelled).
  struct Interval {
    double lower_deg = 0.0;
    double upper_deg = 0.0;
    int n_components = 0;
    int n_closed = 0;
    int n_open = 0;
  };
  struct EndpointLabel {
    double delta_deg = 0.0;
    std::string source;   // OnsetSourceName slug (e.g. "interior_minimum")
    std::string profile;  // OnsetProfileName slug (e.g. "finite_jump")
    int multiplicity = 1;
  };
  struct SupportSetPayload {
    std::vector<Interval> intervals;
    std::vector<EndpointLabel> endpoints;
    // The caliber: the scope the delta machine ran on (result-objects.md: a support set must
    // name its caliber — literal members / L1 / L2 / filter union is the family of axes).
    unsigned symmetry_bits = 0;
    int member_count = 0;  // class members the partition ran on
    // The density-support tail rows (the family arm's append; empty on the T1 arm's cards).
    std::vector<FamilySupportTail> density_tails;
  };

  // --- kCausticEdge payload --------------------------------------------------------------
  // The min-deviation fold: position (certified when the two-face-chain closed form applies),
  // profile, the measure limit the kernel computed for a finite jump, and whether the fold sits
  // on the support boundary (the marker that predicts a sharp edge vs a soft cutoff).
  struct EdgeCertificate {
    std::string formula;           // the cross-checked closed form, as data
    double apex_angle_deg = 0.0;   // A, the angle between the two face normals
    double cross_check_deg = 0.0;  // |kernel position - closed form| at the reference index
    double tolerance_deg = 0.0;    // the acceptance tolerance the cross-check ran at
  };
  struct CausticEdgePayload {
    double position_deg = 0.0;
    std::string profile;  // OnsetProfileName slug
    bool has_measure_limit = false;
    double measure_limit = 0.0;
    bool coincident_with_support = false;
    std::optional<EdgeCertificate> certificate;  // present = the position is certified
  };

  // --- kBrightDomain payload -------------------------------------------------------------
  // The support's interior delta domain and (once the probe leg runs) the profile readings:
  // the sharp edge (certified, on the caustic_edge card) and the brightness peak (finite, this
  // card) are different quantities on different cards with different evidence states.
  struct BrightDomainPayload {
    double domain_lower_deg = 0.0;
    double domain_upper_deg = 0.0;
    bool has_peak = false;
    double peak_delta_deg = 0.0;
    double peak_value = 0.0;
    bool has_edge_reading = false;
    double edge_delta_deg = 0.0;  // the probe's own (finite) reading of the edge position
    long long kept_count = 0;     // T2 existence witness: > 0 = the class carries finite
                                  // positive weight somewhere (an existence statement, never a
                                  // distribution statistic)
    int probe_index = -1;         // the probes[] entry the readings came from
  };


  // --- the family routing's payloads (only the family arm's cards carry them) ------------
  // The band label: the ladder level the card survived at, the frozen dimensions with their
  // narrow/wide verdicts, and the survival filter's own two numbers. A killed candidate never
  // carries a band — it lives in family_routing.ladder[].killed instead.
  struct BandPayload {
    int level = 0;
    std::vector<FamilyBandVerdict> frozen;
    double blur_deg = 0.0;
    double scale_deg = 0.0;
    bool clamped = false;
  };
  // Per-kind coverage counts: a family_collapse_point carries its collapse component's sample
  // count, a family caustic_edge its fold component's crossing count; a regular_image /
  // bright_domain card's sample_count is the consumed-side extent probe's anchor count (the
  // EvaluateFamilyPoint probes the assembly ran, not the rank primitive's own output), and its
  // crossing_count stays 0.
  struct FamilyOriginPayload {
    int sample_count = 0;
    int crossing_count = 0;
  };

  std::optional<BandPayload> band;
  std::optional<FamilyOriginPayload> family_origin;

  // --- the witness payload (the refused-support arm's positive leg) ----------------------
  // A partition refusal says the support is unknown, not that the class carries no light; this
  // payload hangs the positive witness on the SAME refused support card — the card's own
  // evidence stays refused, and the witness sub-block carries its own finite+coverage (the two
  // states coexist; neither overrides the other). The witness is the max-weight event of the
  // declared-source lattice (the antipodal Fibonacci lattice over SO(3), the declared Haar
  // measure's own sampler) re-evaluated by the diagnostic batch at its pose: an existence
  // statement, never a distribution statistic. Absent from a card no leg produced.
  using WitnessPayload = WitnessResult;

  WitnessPayload* Witness() { return witness_ ? &*witness_ : nullptr; }
  const WitnessPayload* Witness() const { return witness_ ? &*witness_ : nullptr; }
  bool SetWitness(WitnessPayload value) {
    if (Kind() != SurveyObjectKind::kSupportSet)
      return false;
    witness_ = std::move(value);
    return true;
  }

  struct SourceImagePayload {
    std::optional<SurveyCertificate> certificate;
  };
  struct FamilyCollapsePayload {};
  struct RegularImagePayload {};
  struct CausticPayload {
    std::optional<CausticEdgePayload> delta;
  };
  struct BrightPayload {
    std::optional<BrightDomainPayload> domain;
  };
  using Payload = std::variant<ClaimedValue<SourceImagePayload>, ClaimedValue<FamilyCollapsePayload>,
                               ClaimedValue<RegularImagePayload>, ClaimedValue<CausticPayload>,
                               ClaimedValue<BrightPayload>, ClaimedValue<SupportSetPayload>>;

  SurveyObjectKind Kind() const { return static_cast<SurveyObjectKind>(payload_.index()); }
  bool IsFamilyKind() const {
    return Kind() == SurveyObjectKind::kFamilyCollapsePoint || Kind() == SurveyObjectKind::kCausticEdge ||
           Kind() == SurveyObjectKind::kRegularImage || Kind() == SurveyObjectKind::kBrightDomain;
  }
  bool AdjunctsValid() const {
    const bool successful = State() == EvidenceState::kFinite || State() == EvidenceState::kCertified;
    return (successful || (!band && !family_origin && !has_position)) && (IsFamilyKind() || (!band && !family_origin));
  }
  bool SetKind(SurveyObjectKind kind) {
    switch (kind) {
      case SurveyObjectKind::kSourceImage:
        payload_ = ClaimedValue<SourceImagePayload>{};
        ClearAdjuncts();
        return true;
      case SurveyObjectKind::kFamilyCollapsePoint:
        payload_ = ClaimedValue<FamilyCollapsePayload>{};
        ClearAdjuncts();
        return true;
      case SurveyObjectKind::kRegularImage:
        payload_ = ClaimedValue<RegularImagePayload>{};
        ClearAdjuncts();
        return true;
      case SurveyObjectKind::kCausticEdge:
        payload_ = ClaimedValue<CausticPayload>{};
        ClearAdjuncts();
        return true;
      case SurveyObjectKind::kBrightDomain:
        payload_ = ClaimedValue<BrightPayload>{};
        ClearAdjuncts();
        return true;
      case SurveyObjectKind::kSupportSet:
        payload_ = ClaimedValue<SupportSetPayload>{};
        ClearAdjuncts();
        return true;
    }
    return false;
  }
  EvidenceState State() const {
    return std::visit([](const auto& c) { return c.State(); }, payload_);
  }
  bool SetState(EvidenceState state) {
    const bool changed = std::visit([state](auto& c) { return c.SetState(state); }, payload_);
    if (changed && (state == EvidenceState::kRefused || state == EvidenceState::kUnevaluated)) {
      band.reset();
      family_origin.reset();
      has_position = false;
      position_altitude_deg = 0.0;
      position_azimuth_deg = 0.0;
    }
    return changed;
  }
  SurveyCoverage& Coverage() {
    return std::visit([](auto& c) -> SurveyCoverage& { return c.Coverage(); }, payload_);
  }
  const SurveyCoverage& Coverage() const {
    return std::visit([](const auto& c) -> const SurveyCoverage& { return c.Coverage(); }, payload_);
  }
  std::string& Method() {
    return std::visit([](auto& c) -> std::string& { return c.Method(); }, payload_);
  }
  const std::string& Method() const {
    return std::visit([](const auto& c) -> const std::string& { return c.Method(); }, payload_);
  }
  std::string& Mode() {
    return std::visit([](auto& c) -> std::string& { return c.Mode(); }, payload_);
  }
  const std::string& Mode() const {
    return std::visit([](const auto& c) -> const std::string& { return c.Mode(); }, payload_);
  }
  std::string& Reason() {
    return std::visit([](auto& c) -> std::string& { return c.Reason(); }, payload_);
  }
  const std::string& Reason() const {
    return std::visit([](const auto& c) -> const std::string& { return c.Reason(); }, payload_);
  }
  SurveyEvidence Evidence() const {
    return std::visit([](const auto& c) { return c.Evidence(); }, payload_);
  }
  const Payload& Declaration() const { return payload_; }
  void BindScope(const ClaimScope& scope) {
    std::visit([&scope](auto& c) { c.BindScope(scope); }, payload_);
  }
  const SupportSetPayload* Support() const { return Value<SupportSetPayload>(); }
  SupportSetPayload* Support() { return Value<SupportSetPayload>(); }
  const CausticEdgePayload* Edge() const {
    const auto* value = Value<CausticPayload>();
    return value && value->delta ? &*value->delta : nullptr;
  }
  CausticEdgePayload* Edge() {
    auto* value = Value<CausticPayload>();
    return value && value->delta ? &*value->delta : nullptr;
  }
  const BrightDomainPayload* Bright() const {
    const auto* value = Value<BrightPayload>();
    return value && value->domain ? &*value->domain : nullptr;
  }
  BrightDomainPayload* Bright() {
    auto* value = Value<BrightPayload>();
    return value && value->domain ? &*value->domain : nullptr;
  }
  const SurveyCertificate* Certificate() const {
    const auto* value = Value<SourceImagePayload>();
    return value && value->certificate ? &*value->certificate : nullptr;
  }
  bool SetSupport(SupportSetPayload value) {
    auto* target = Value<SupportSetPayload>();
    if (!target)
      return false;
    *target = std::move(value);
    return true;
  }
  bool SetEdge(CausticEdgePayload value) {
    auto* target = Value<CausticPayload>();
    if (!target)
      return false;
    target->delta = std::move(value);
    return true;
  }
  bool SetBright(BrightDomainPayload value) {
    auto* target = Value<BrightPayload>();
    if (!target)
      return false;
    target->domain = std::move(value);
    return true;
  }
  bool SetCertificate(SurveyCertificate value) {
    auto* target = Value<SourceImagePayload>();
    if (!target)
      return false;
    target->certificate = std::move(value);
    return true;
  }

 private:
  void ClearAdjuncts() {
    witness_.reset();
    band.reset();
    family_origin.reset();
    has_position = false;
    position_altitude_deg = 0.0;
    position_azimuth_deg = 0.0;
  }
  template <class T>
  const T* Value() const {
    const auto* claim = std::get_if<ClaimedValue<T>>(&payload_);
    return claim ? claim->ValueOrNull() : nullptr;
  }
  template <class T>
  T* Value() {
    auto* claim = std::get_if<ClaimedValue<T>>(&payload_);
    return claim ? claim->ValueOrNull() : nullptr;
  }
  Payload payload_;
  std::optional<WitnessPayload> witness_;
};

// The T0 leg's per-member record: the routing fact the certificate (or its absence) states for
// one class member. `mechanism` is the kernel's own roll-up slug ("point_mass", ...); it is
// empty only when the mechanism was not computable, and `mechanism_unavailable` then says why —
// a missing mechanism is never silent.
struct SurveyMemberVerdict {
  std::vector<int> member;
  bool rank_zero = false;
  std::string mechanism;
  std::string mechanism_unavailable;
  SurveyEvidence evidence;
};

// One unevaluated declaration: the object kinds a routing branch would have produced, and why
// they were not computed. The first real consumer of the unevaluated state.
struct SurveyUnevaluated {
  std::vector<SurveyObjectKind> kinds;
  std::string reason;
};

// The caliber of one canonical probe (the solar-radial band-sum slice). The five coverage
// facts the evidence.md seed-coverage ruling names, as fields: sample_count, the lattice
// identifier, the determinism statement, the bin/domain, the reference wavelength/index (the
// document's own, so the probe's edge reading and the support's lower end share one ruler),
// and the representative member the probe ran on with the class size it stands for.
struct SurveyProbeSpec {
  QuantitySpec quantity;
  double delta_min_deg = 15.0;
  double delta_max_deg = 60.0;
  double bin_deg = 0.1;
  double wedge_half_width_deg = 0.0;
  std::optional<double> azimuth_deg;
  double wavelength_nm = 0.0;
  double refractive_index = 0.0;
  long long sample_count = 0;
  std::string lattice = "antipodal_fibonacci";  // the sampler's own lattice kind (band_sum.hpp)
  bool deterministic = true;                    // no RNG anywhere in the sample
  std::vector<int> representative;              // the literal face sequence probed
  int member_count = 0;                         // the class size the representative stands for (relabeled equivalence)
};

// One radial sample row: the band's centre deviation, its per-sr value (the band-sum value
// formula makes a uniform density's value the full-annulus mean, azimuth-independent), the raw
// event count and the effective count.
struct SurveyProbeSample {
  double delta_deg = 0.0;
  double value = 0.0;
  int k = 0;
  double k_eff = 0.0;
};

// One canonical probe. The derived readings split the two quantities the corpus lesson keeps
// apart: `edge_*` = where the support starts (the first non-empty bin contains the minimum-
// deviation edge, geometrically no event lies below it), `peak_*` = where the profile is
// brightest (the value argmax) — finite readings, never the certified edge position.
enum class SurveyProbeKind { kSolarRadial };

struct SurveyProbeReading {
  std::vector<SurveyProbeSample> samples;
  bool has_edge = false;
  int edge_bin = -1;
  double edge_delta_deg = 0.0;
  bool has_peak = false;
  double peak_delta_deg = 0.0;
  double peak_value = 0.0;
  double separation_deg = 0.0;
  long long kept_count = 0;
};

struct SurveyProbe : ClaimedValue<SurveyProbeReading> {
  SurveyProbeKind kind = SurveyProbeKind::kSolarRadial;
  SurveyProbeSpec spec;
};

// One onset row of the positional-dispersion table, followed across the fixed index pair: the
// kernel's (source, profile) slugs passed through, the pair's values in label order, the
// max-min displacement and the row's own stacking verdict (the same direction vocabulary as
// the min-deviation row's). A class without a min-deviation fold (a corner-bounded support) has
// its mounted-point rows here and nowhere else — the row sequence is the leg's data, not a
// derived view of the min-row fields.
struct SurveyFollowedOnset {
  std::string source;              // OnsetSourceName slug (e.g. "interior_maximum" / "corner")
  std::string profile;             // OnsetProfileName slug (e.g. "finite_jump" / "boundary_onset")
  std::vector<double> values_deg;  // parallel to the labels (red first)
  double displacement_deg = 0.0;   // max - min across the pair, label-order independent
  std::string direction;           // "red_inner_blue_outer" ⇔ values_deg[0] < values_deg[1]
};

// The chromatic layer's positional-dispersion leg: the critical onsets followed across the
// fixed index pair (labels as the kernel table carries them, indices as the engine's table
// echoes them). `has_min_deviation_row` says the min-deviation row exists — the scalar
// min-row fields below it are that row's and serialize only when it does (keys exist exactly
// when the readings they describe do, the probe's own discipline); `followed_onsets` is every
// row the table carried, in table order. `direction` is the min row's stacking verdict;
// `red_inner_blue_outer` ⇔ value_red < value_blue (red edge closer to the sun).
struct SurveyPositionalDispersion {
  std::string red_label = kChromaticRedLabel;
  std::string blue_label = kChromaticBlueLabel;
  double n_red = 0.0;
  double n_blue = 0.0;
  bool has_min_deviation_row = false;
  double value_red_deg = 0.0;
  double value_blue_deg = 0.0;
  double displacement_deg = 0.0;
  std::string direction = "red_inner_blue_outer";
  std::vector<SurveyFollowedOnset> followed_onsets;
  SurveyEvidence evidence;
};

// The chromatic layer's diagnose leg (the kernel's Diagnose): the weight-kink and gate-edge
// features as its own sub-block — a different kernel path than the dispersion table, carrying
// its own thresholds and coverage statement, never merged into the dispersion fields.
struct SurveyDiagnoseFeature {
  std::string kind;  // ChromaticFeatureKindName ("edge" / "gate_edge")
  std::string source;
  std::string color;  // ChromaticColorName
  bool visible = false;
  double delta_red_deg = 0.0;
  double delta_blue_deg = 0.0;
  double shift_deg = 0.0;
};

struct SurveyDiagnoseBlock {
  std::string verdict_kind;  // ChromaticVerdictKindName
  std::string color;         // ChromaticColorName
  bool visible = false;
  bool has_position = false;
  double position_deg = 0.0;
  bool coverage_complete = true;
  std::vector<std::string> notes;
  double n_red = 0.0;  // the snapshot's declared parameter pair
  double n_blue = 0.0;
  std::vector<SurveyDiagnoseFeature> features;
  SurveyEvidence evidence;
};

// The request as the document answered it (the echo).
struct SurveyRequestEcho {
  long long crystal_id = 0;
  std::vector<std::vector<int>> path_layers;
  unsigned symmetry_bits = kDefaultSymmetryBits;  // P|B|D; 0 = the literal path (the report request's encoding)
  bool wavelength_given = false;
  double wavelength_nm = 0.0;  // as given, 0 when unset
  int budget_ms = 0;           // as resolved (default substituted); out-of-range values are
                               // rejected by the engine, never clamped
};

// The warm index block (the --warm contract's survey side): the poses the assembly actually held,
// each named by the leg that held it, plus the reuse-scope declaration, the producer receipt and
// the config fingerprint (the routing-axis projection's SHA256, config_fingerprint.hpp's single
// implementation). A pure append on the schema-4 document — a pre-warm reader sees no key change,
// and a survey whose arms held no pose carries `seeds: []` with the same declaration fields.
struct WarmIndexBlock {
  std::vector<std::array<double, 9>> seeds;
  std::vector<std::string> seed_sources;  // parallel to seeds: the leg that held the pose
  std::string reuse_scope;
  std::string produced_for;
  std::string config_fingerprint;
};


}  // namespace lumice::raypath::survey

#endif  // LUMICE_RAYPATH_RESULTS_SURVEY_RECORDS_HPP_
