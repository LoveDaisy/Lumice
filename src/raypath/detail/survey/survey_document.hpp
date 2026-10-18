#ifndef RAYPATH_DETAIL_SURVEY_DOCUMENT_H_
#define RAYPATH_DETAIL_SURVEY_DOCUMENT_H_

// The survey document's value model: the six registered object kinds (the design's 5+1 object
// taxonomy; the first entry split into source_image / family_collapse_point by routing origin),
// the object card, and the per-block evidence state. This is the single in-memory shape the
// assembly fills and the serializer reads; it carries no pointers, so a shell and a test can
// both build one.

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "analytic/band_sum.hpp"   // PointMassMethodName's enum (the kernel's own)
#include "analytic/path_rank.hpp"  // the certificate's tolerances are the kernel's own constants
#include "raypath/detail/survey/evidence.hpp"

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
  double delta_min_deg = 15.0;
  double delta_max_deg = 60.0;
  double bin_deg = 0.1;
  double wedge_half_width_deg = 0.0;
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

// The chromatic layer's positional-dispersion leg: the min-deviation edge followed across the
// fixed index pair (labels as the kernel table carries them, indices as the engine's table
// echoes them). `direction` is the stacking verdict; `red_inner_blue_outer` ⇔ value_red <
// value_blue (red edge closer to the sun).
struct SurveyPositionalDispersion {
  std::string red_label = kChromaticRedLabel;
  std::string blue_label = kChromaticBlueLabel;
  double n_red = 0.0;
  double n_blue = 0.0;
  double value_red_deg = 0.0;
  double value_blue_deg = 0.0;
  double displacement_deg = 0.0;
  std::string direction = "red_inner_blue_outer";
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
// The direction vocabulary is the sign of the ratio itself (the issue's caliber: blue-tinted is
// B/R > 1). No neutral band lives in the model — a band is an acceptance notion, and each
// consumer pins its own separation threshold (the C15 bench requires ratio > 1.2 to separate
// from the corpus's no-internal-reflection counterfactual at ~0.97; the C14 bench pins a
// |ratio - 1| < 0.05 band with the same first-run provenance discipline). `kNeutral` is the
// degenerate exact-1.0 point.
enum class SurveyTintDirection { kBlueTinted, kRedTinted, kNeutral };

inline constexpr std::array<std::pair<std::string_view, SurveyTintDirection>, 3> kSurveyTintDirectionNames{ {
    { "blue_tinted", SurveyTintDirection::kBlueTinted },
    { "red_tinted", SurveyTintDirection::kRedTinted },
    { "neutral", SurveyTintDirection::kNeutral },
} };

inline const char* SurveyTintDirectionName(SurveyTintDirection direction) {
  for (const auto& entry : kSurveyTintDirectionNames) {
    if (entry.second == direction) {
      return entry.first.data();
    }
  }
  return "neutral";
}

// The point-mass method names (band_sum.hpp's PointMassMethod) as the document spells them —
// the survey-side single authority over the slug, so a kernel-side spelling change is one edit.
// The switch carries no default: a new kernel PointMassMethod value fails -Wswitch at the
// switch itself; the trailing return only satisfies control-reaches-end for today's
// fully-covered enum.
inline const char* PointMassMethodName(analytic::PointMassMethod method) {
  switch (method) {
    case analytic::PointMassMethod::kPsiAverage:
      return "psi_average";
    case analytic::PointMassMethod::kLatticeMean:
      return "lattice_mean";
  }
  return "lattice_mean";
}

// One weight-tint evaluation. `m_*` are the per-lambda masses and their last-doubling quadrature
// changes (provenance, never asserted against); `ratio` is m_blue / m_red. The tint runs on the
// class representative, with `member_count` declaring the class size it stands for (the probe's
// relabeled-equivalence discipline).
struct SurveyWeightTint {
  std::string red_label = kChromaticRedLabel;
  std::string blue_label = kChromaticBlueLabel;
  double n_red = 0.0;
  double n_blue = 0.0;
  double m_red = 0.0;
  double m_blue = 0.0;
  double m_red_error = 0.0;
  double m_blue_error = 0.0;
  std::string method;  // PointMassMethodName: the mass's own computation path
  long long sample_count = 0;
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

// The request as the document answered it (the echo).
struct SurveyRequestEcho {
  long long crystal_id = 0;
  std::vector<std::vector<int>> path_layers;
  unsigned symmetry_bits = 7;  // P|B|D; 0 = the literal path (the report request's encoding)
  bool wavelength_given = false;
  double wavelength_nm = 0.0;  // as given, 0 when unset
  int budget_ms = 0;           // as resolved (default substituted); out-of-range values are
                               // rejected by the engine, never clamped
};

// The whole document, pre-serialization.
struct SurveyDocument {
  int schema_version = 4;

  SurveyRequestEcho request;

  // Scope: the input the routing ran on.
  std::string scene_identity;
  std::optional<long long> scene_layer;   // absent = a standalone configured crystal
  std::vector<std::vector<int>> members;  // the resolved class members
  double wavelength_nm = 0.0;
  std::string wavelength_source;  // WavelengthSourceName
  double refractive_index = 0.0;
  double sun_altitude_deg = 0.0;
  double sun_azimuth_deg = 0.0;

  // Routing: the T0 leg.
  bool early_stopped = false;
  std::vector<SurveyMemberVerdict> member_verdicts;
  std::vector<SurveyObject> objects;
  std::vector<SurveyUnevaluated> unevaluated;

  // The canonical probes (the survey's quantification layer): filled only by a routing arm
  // that ran them; an early-stop document carries none.
  std::vector<SurveyProbe> probes;

  // The chromatic layer: the block-level evidence plus the optional legs. A leg absent from
  // the document is one the routing arm did not run — the sub-block keys never appear without
  // their content. Block-state rules by arm: the T1 arm's chromatic legs turn the block finite
  // (or refused on a leg's hard failure); the identity arm's only leg is the tint, so the block
  // mirrors the tint's own state — finite with coverage when it computed, refused with the tint's
  // mode when it refused (the block-level state and the sub-block can never disagree).
  SurveyEvidence chromatic;
  std::optional<SurveyPositionalDispersion> positional_dispersion;
  std::optional<SurveyDiagnoseBlock> diagnose;
  std::optional<SurveyWeightTint> weight_tint;

  // The document-level evidence: the T0 routing verdict's own state.
  SurveyEvidence evidence;

  // Meta.
  int budget_ms = 0;  // as resolved
  double total_seconds = 0.0;
  // The budget note text, owned by the arm that produced the document: the default is the v1
  // early-stop wording (whose claim is true of that arm), and the T1 arm states its own legs.
  std::string budget_note =
      "the v1 survey's only computation is the millisecond T0 routing leg, which is no budget "
      "consumer; budget enforcement lands with the band-sum/T3 quantification legs";
};

}  // namespace lumice::raypath::survey

#endif  // RAYPATH_DETAIL_SURVEY_DOCUMENT_H_
