#ifndef LUMICE_RAYPATH_RESULTS_SURVEY_RECORDS_HPP_
#define LUMICE_RAYPATH_RESULTS_SURVEY_RECORDS_HPP_

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "analytic/band_sum.hpp"     // PointMassMethodName's enum (the kernel's own)
#include "analytic/family_rank.hpp"  // FamilyDimension: the frozen_dims rows' own enum
#include "analytic/path_rank.hpp"    // the certificate's tolerances are the kernel's own constants
#include "raypath/domain/evidence.hpp"
#include "raypath/domain/measure.hpp"


namespace lumice::raypath::survey {

// The object-kind registry. Registration order is the document order of any listing of the
// vocabulary; the registry grows append-only (a new kind is a new enum value plus its name —
// never a re-spelling of an old one).
enum class SurveyObjectKind {
  kSourceImage,          // T0 identity certificate (M = I and zero wedge): the sun's preimage
  kFamilyCollapsePoint,  // T1 rank 0 on the limit family
  kRegularImage,         // T1 rank 1: a one-dimensional regular image
  kCausticEdge,          // T1 rank 2: the critical image of the relevant projection
  kBrightDomain,         // a two-dimensional bright domain (weight profiles live here)
  kSupportSet,           // the support set: boundary curves with mechanism labels, holes
};

struct SurveyObjectKindNameEntry {
  SurveyObjectKind kind;
  const char* name;
};

inline const std::vector<SurveyObjectKindNameEntry>& SurveyObjectKindTable() {
  static const std::vector<SurveyObjectKindNameEntry> kTable = {
    { SurveyObjectKind::kSourceImage, "source_image" },
    { SurveyObjectKind::kFamilyCollapsePoint, "family_collapse_point" },
    { SurveyObjectKind::kRegularImage, "regular_image" },
    { SurveyObjectKind::kCausticEdge, "caustic_edge" },
    { SurveyObjectKind::kBrightDomain, "bright_domain" },
    { SurveyObjectKind::kSupportSet, "support_set" },
  };
  return kTable;
}

inline const char* SurveyObjectKindName(SurveyObjectKind kind) {
  for (const auto& entry : SurveyObjectKindTable()) {
    if (entry.kind == kind) {
      return entry.name;
    }
  }
  return "source_image";
}

// False for anything outside the registered vocabulary.
inline bool SurveyObjectKindFromName(std::string_view name, SurveyObjectKind* out) {
  for (const auto& entry : SurveyObjectKindTable()) {
    if (name == entry.name) {
      *out = entry.kind;
      return true;
    }
  }
  return false;
}

// The registry itself, registration order — projected from the name table above, of which it is
// not a second copy. The serializer's coverage row and the assembly's unevaluated entries read
// this rather than keeping their own copies.
inline const std::vector<SurveyObjectKind>& AllSurveyObjectKinds() {
  static const std::vector<SurveyObjectKind> kKinds = [] {
    std::vector<SurveyObjectKind> kinds;
    kinds.reserve(SurveyObjectKindTable().size());
    for (const auto& entry : SurveyObjectKindTable()) {
      kinds.push_back(entry.kind);
    }
    return kinds;
  }();
  return kKinds;
}

// The coverage declaration a finite state must carry (the design's default: state required,
// defaulting to finite with this block). What "finite" covered THIS run.
struct SurveyCoverage {
  int members_evaluated = 0;  // class members the leg evaluated
  std::string geometry_basis = "nominal crystal shape at the distribution centres";
  double wavelength_nm = 0.0;  // the reference wavelength the leg ran at
};

// One declaration block's evidence. `state` is always serialized; the accompanying field depends
// on it: certified -> method (registered); refused -> mode (open string); unevaluated -> reason;
// finite -> coverage. A default-constructed block is exactly the schema default: finite plus its
// coverage declaration.
struct SurveyEvidence {
  EvidenceState state = EvidenceState::kFinite;
  std::string method;       // certified
  std::string mode;         // refused
  std::string reason;       // unevaluated
  SurveyCoverage coverage;  // finite
};

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

// The dimension names in canonical (zenith, azimuth, roll) order — the kernel enum's own order;
// the family block's dim vocabulary on the wire (frozen_dims rows and free_dims entries),
// spelled once — the structs carry the enum themselves.
inline const char* DimName(analytic::FamilyDimension dim) {
  switch (dim) {
    case analytic::FamilyDimension::kZenith:
      return "zenith";
    case analytic::FamilyDimension::kAzimuth:
      return "azimuth";
    case analytic::FamilyDimension::kRoll:
      return "roll";
  }
  return "roll";
}

// One frozen dimension of a ladder level: the freeze value and the width that was frozen (the
// measure's own sigma — reported, never consumed by the rank question).
struct FamilyFrozenDim {
  analytic::FamilyDimension dim = analytic::FamilyDimension::kRoll;  // the enum; the name is the wire's
  double center_deg = 0.0;
  double width_deg = 0.0;  // 0 for the measure's own fixed dimensions
};

// One narrow/wide verdict of one frozen dimension against one object (AC5: narrow/wide is a
// property of the (dimension, object) pair, never of the dimension alone).
struct FamilyBandVerdict {
  std::string dim;
  std::string verdict;  // "narrow" | "wide"
};

// A candidate the survival filter killed, recorded in the ladder level it was proposed at:
// every level reports its proposals — the survivors as object cards, the killed here.
struct FamilyKilledCandidate {
  std::vector<int> member;
  std::string kind;  // SurveyObjectKindName of the proposed card
  bool has_position = false;
  double position_altitude_deg = 0.0;
  double position_azimuth_deg = 0.0;
  bool blur_measured = false;  // the survival filter ran; a no_valid_image kill carries no numbers
  double blur_deg = 0.0;
  double scale_deg = 0.0;
  bool clamped = false;  // a blur probe clamped into its dimension's legal range (one-sided)
  std::string reason;    // "survival_filter" | "no_valid_image"
  std::vector<FamilyBandVerdict> verdicts;
};

// One ladder level. Level 0 is the actual support (the measure's fixed dimensions already
// frozen): it owns the support_set card, and asks the rank question only when the fixed
// dimensions already leave at most two free. Levels 1..K freeze the k tightest gauss
// candidates. `rank_asked` false means the level's family has three free dimensions (a level 0
// with no fixed dimension — the rank entry's own refusal domain), so the level owns the support
// alone and the rank fields below stay unset.
struct FamilyLadderLevel {
  int level = 0;
  std::vector<FamilyFrozenDim> frozen_dims;
  std::vector<analytic::FamilyDimension> free_dims;  // the enum; the name is the wire's
  bool rank_asked = false;
  int generic_rank = -1;  // the first member's whose rank run returned ok; relabelings share it
                          // (the support's own relabeling logic, one physical class)
  int grid = 0;
  double grid_spacing_deg[2] = { 0.0, 0.0 };
  int samples = 0;
  int valid_samples = 0;
  int rank_refused_members = 0;  // a member the rank entry refused (data, not a verdict)
  int objects_proposed = 0;
  int objects_killed = 0;
  std::vector<FamilyKilledCandidate> killed;
};

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

struct WitnessResult {
  EvidenceState state = EvidenceState::kRefused;
  std::string mode;         // refused
  SurveyCoverage coverage;  // finite
  long long kept_count = 0;
  long long sample_count = 0;
  std::string lattice = "antipodal_fibonacci";  // the sampler's own lattice kind (discovery.hpp)
  bool deterministic = true;                    // no RNG anywhere in the sample
  double deviation_deg = 0.0;                   // the event's own deviation (the discovery's D)
  double weight = 0.0;                          // the event's w = A*T (the lattice's unit)
  double entry = 0.0;                           // the batch row's entry measure (its own read of A)
  double interface_product = 0.0;               // the batch row's interface product
  std::string source_note;                      // the declared source, spelled inside the document
  // The argmax event's candidate pose, retained for the document's warm_index (the seed the
  // query faces reuse). Not serialized on the card itself — it travels in the warm_index block
  // under the leg name "witness_argmax".
  bool has_pose = false;
  double pose[9] = {};
};

// One object card. For the v1 source_image: `members` are the class members the card collapses
// (the card is the sky-side object; the members are its preimage decomposition), `position` is
// the sun's altitude/azimuth, `evidence` is certified with the method named, and `certificate`
// carries the declared tolerances. The T1 payloads are per-kind optionals: exactly the payload
// of the card's kind is filled, and a payload absent from the document is one the routing arm
// never produced (fields only ever appear when the assembly filled them).
struct SurveyObject {
  SurveyObjectKind kind = SurveyObjectKind::kSourceImage;
  std::vector<std::vector<int>> members;
  bool has_position = false;
  double position_altitude_deg = 0.0;
  double position_azimuth_deg = 0.0;
  SurveyEvidence evidence;
  std::optional<SurveyCertificate> certificate;

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

  std::optional<SupportSetPayload> support_set;
  std::optional<CausticEdgePayload> caustic_edge;
  std::optional<BrightDomainPayload> bright_domain;

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

  std::optional<WitnessPayload> witness;
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
struct SurveyProbe {
  std::string kind = "solar_radial";
  SurveyProbeSpec spec;
  SurveyEvidence evidence;  // finite + the coverage declaration
  std::vector<SurveyProbeSample> samples;
  bool has_edge = false;
  int edge_bin = -1;
  double edge_delta_deg = 0.0;
  bool has_peak = false;
  double peak_delta_deg = 0.0;
  double peak_value = 0.0;
  double separation_deg = 0.0;  // peak_delta_deg - edge_delta_deg, signed
  long long kept_count = 0;     // the sample's kept events (the T2 witness count)
};

// The chromatic layer's fixed wavelength pair — the single authority every chromatic-site
// spelling derives from (both legs' labels and indices, the coverage wavelengths, the prose).
// Both legs (the positional-dispersion table and the weight tint) consume this pair, so it
// cannot drift between them; the serialized labels and n values echo exactly these constants.
inline constexpr double kChromaticRedNm = 650.0;
inline constexpr double kChromaticBlueNm = 450.0;
inline constexpr const char* kChromaticRedLabel = "650nm";
inline constexpr const char* kChromaticBlueLabel = "450nm";

// The pair in prose, derived (never a site's own literal). Two orders are live in the document
// and preserved byte-for-byte: the budget note spells the pair blue-first, the coverage basis
// strings red-first (matching the labels vector's red-first order).
inline std::string ChromaticPairProseBlueFirst() {
  return std::to_string(static_cast<long long>(kChromaticBlueNm)) + "/" +
         std::to_string(static_cast<long long>(kChromaticRedNm));
}

inline std::string ChromaticPairProseRedFirst() {
  return std::to_string(static_cast<long long>(kChromaticRedNm)) + "/" +
         std::to_string(static_cast<long long>(kChromaticBlueNm));
}

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

// The chromatic layer's weight-tint leg: the direction the transmission WEIGHT tilts across
// the fixed 650/450 index pair (the positional-dispersion leg's sibling: a different mechanism
// — the same position's weight changes with lambda, driven by near-TIR Fresnel growth — and a
// separate sub-block, never merged into the dispersion fields). The ratio is m(450nm)/m(650nm),
// the point-mass band sum's masses at the sun direction, unweighted by SPD or CMF: the
// lambda-pair transmission-weight caliber, which is a different quantity from a disk-integrated
// D65 B/R and must not be compared against one.
//
// The direction vocabulary is the sign of the ratio itself: blue-tinted is B/R > 1, red-tinted
// is B/R < 1. No neutral band lives in the model — a band is an acceptance notion, and each
// consumer pins its own separation threshold with first-run provenance (the thresholds are the
// consumers', not the schema's; the caliber rule is documented in doc/raypath/raypath-cli-output.md
// section 9.6). `kNeutral` is the degenerate exact-1.0 point.
enum class SurveyTintDirection { kBlueTinted, kRedTinted, kNeutral };

// The same full-coverage-switch form as PointMassMethodName below: a new vocabulary value fails
// -Wswitch at the switch itself; the trailing return only satisfies control-reaches-end for
// today's fully-covered enum.
inline const char* SurveyTintDirectionName(SurveyTintDirection direction) {
  switch (direction) {
    case SurveyTintDirection::kBlueTinted:
      return "blue_tinted";
    case SurveyTintDirection::kRedTinted:
      return "red_tinted";
    case SurveyTintDirection::kNeutral:
      return "neutral";
  }
  return "neutral";
}

// The point-mass method names (band_sum.hpp's PointMassMethod) as the document spells them —
// the survey-side single authority over the slug, so a kernel-side spelling change is one edit.
// The switch carries no default: a new kernel PointMassMethod value fails -Wswitch at the
// switch itself; the trailing return only satisfies control-reaches-end for today's
// fully-covered enum. The class power-sum path is survey-side, not a kernel method: its slug
// is the constant below, and the two families never share a field shape (see SurveyWeightTint).
inline const char* PointMassMethodName(analytic::PointMassMethod method) {
  switch (method) {
    case analytic::PointMassMethod::kPsiAverage:
      return "psi_average";
    case analytic::PointMassMethod::kLatticeMean:
      return "lattice_mean";
  }
  return "lattice_mean";
}

constexpr const char* kClassTintMethodLatticePowerSum = "lattice_power_sum";

// One weight-tint evaluation. The two shapes this struct carries share the ratio/question but
// not the reading, and the field sets never merge:
// - the point-mass shape (the identity arm; `class_power_shape` false): `m_*` are the
//   per-lambda masses and their last-doubling quadrature changes (provenance, never asserted
//   against); `method` names the kernel's PointMassMethod. `ratio` is m_blue / m_red.
// - the class power-sum shape (the T1 arm; `class_power_shape` true): `energy_*` are the
//   per-sample average powers of the declared lattice's kept events (sum of w = A*T over the
//   lattice, divided by the lattice size); `method` is kClassTintMethodLatticePowerSum; the
//   `m_*` family stays zero and does not serialize (a mass is not this shape's quantity).
// Either way the tint runs on the class representative, with `member_count` declaring the
// class size it stands for (the probe's relabeled-equivalence discipline), and `ratio` is the
// blue reading over the red one.
struct SurveyWeightTint {
  std::string red_label = kChromaticRedLabel;
  std::string blue_label = kChromaticBlueLabel;
  bool class_power_shape = false;  // the reading's shape: point-mass masses vs lattice power sum
  double n_red = 0.0;
  double n_blue = 0.0;
  double m_red = 0.0;
  double m_blue = 0.0;
  double m_red_error = 0.0;
  double m_blue_error = 0.0;
  double energy_red = 0.0;   // class shape: the per-sample average power at the red index
  double energy_blue = 0.0;  // class shape: the per-sample average power at the blue index
  std::string method;        // PointMassMethodName or kClassTintMethodLatticePowerSum: the reading's own path
  long long sample_count = 0;
  std::string lattice = "antipodal_fibonacci";  // the sampler's own lattice kind (band_sum.hpp)
  bool deterministic = true;                    // no RNG anywhere in the sample
  std::vector<int> representative;
  int member_count = 0;
  double ratio = 0.0;
  SurveyTintDirection direction = SurveyTintDirection::kNeutral;
  long long kept_count_red = 0;
  long long kept_count_blue = 0;
  std::string caliber = std::string("m(") + kChromaticBlueLabel + ")/m(" + kChromaticRedLabel +
                        "): point-mass band-sum transmission weights at the sun direction; fixed " +
                        ChromaticPairProseRedFirst() +
                        " index pair, unweighted by SPD or CMF — not a disk-integrated D65 B/R";
  SurveyEvidence evidence;
};

// One internal interface's row of the TIR share leg: the face's literal number (the
// representative's own, since slots resolve 1:1) and the per-lambda TIR weight shares.
struct SurveyTirShareInterface {
  int face = 0;
  double share_red = 0.0;
  double share_blue = 0.0;
};

// The TIR share leg (the schema-4 append; the exception evidence leg that decomposes the class
// tint's per-sample average power by internal-reflection mechanism): per lambda, the kept
// events' weight share spent on all-internal-interfaces-TIR poses, plus the per-interface
// decomposition. Declared antipodal Fibonacci lattice, class representative, the fixed
// 450/650 pair — the same caliber the class tint reads, a different quantity (a [0,1] share,
// never a cross-lambda ratio). Present only on a document whose arm ran the leg (the T1 arm
// and the refused-support arm); a path with no internal interface refuses the leg
// (no_internal_interfaces — the question is meaningless for that path shape), never zero.
struct SurveyTirShare {
  double n_red = 0.0;
  double n_blue = 0.0;
  std::vector<SurveyTirShareInterface> interfaces;
  double share_all_tir_red = 0.0;
  double share_all_tir_blue = 0.0;
  long long sample_count = 0;
  std::string lattice = "antipodal_fibonacci";
  bool deterministic = true;  // no RNG anywhere in the sample
  std::vector<int> representative;
  int member_count = 0;
  long long kept_count_red = 0;
  long long kept_count_blue = 0;
  std::string caliber;
  SurveyEvidence evidence;
};

// One delta band's row of the MC region ESS leg: the Kish effective sample size of the band's
// Y-channel draw weights (rows grouped by outer draw — one draw is one sampling act even when
// it contributed several member rows), the contributing draw count and the weight sum.
struct SurveyMcRegionEssBand {
  double delta_min_deg = 0.0;
  double delta_max_deg = 0.0;
  double ess = 0.0;
  long long draw_count = 0;
  double sum_weight = 0.0;
};

// The opt-in MC region ESS leg (the schema-4 append): the MC corroboration sampling's effective
// sample size per solar-radial delta band for THIS raypath class — a sampling-quality
// measurement, never a corroboration verdict (no agrees/silent/conflicts word may appear here;
// the MC annotation vocabulary stays on the schema-3 side). Seeded and prefix-replay
// reproducible, so `deterministic` is false by construction: the field exists to state the
// leg's own shape against the deterministic legs' true — a deterministic quadrature carries no
// ESS semantics and never appears on one. Present only when the caller requested the leg.
struct SurveyMcRegionEss {
  long long seed = 0;
  long long draws = 0;
  double lambda_nm = 0.0;
  double band_deg = 0.0;
  std::vector<SurveyMcRegionEssBand> bands;
  bool deterministic = false;  // seeded random; a given seed reproduces the sample
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
