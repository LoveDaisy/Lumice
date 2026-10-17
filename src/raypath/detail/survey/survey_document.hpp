#ifndef RAYPATH_DETAIL_SURVEY_DOCUMENT_H_
#define RAYPATH_DETAIL_SURVEY_DOCUMENT_H_

// The survey document's value model: the six registered object kinds (the design's 5+1 object
// taxonomy; the first entry split into source_image / family_collapse_point by routing origin),
// the object card, and the per-block evidence state. This is the single in-memory shape the
// assembly fills and the serializer reads; it carries no pointers, so a shell and a test can
// both build one.

#include <optional>
#include <string>
#include <string_view>
#include <vector>

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

// The registry itself, registration order. The serializer's coverage row and the assembly's
// unevaluated entries read this rather than keeping their own copies.
inline const std::vector<SurveyObjectKind>& AllSurveyObjectKinds() {
  static const std::vector<SurveyObjectKind> kKinds = {
    SurveyObjectKind::kSourceImage, SurveyObjectKind::kFamilyCollapsePoint, SurveyObjectKind::kRegularImage,
    SurveyObjectKind::kCausticEdge, SurveyObjectKind::kBrightDomain,        SurveyObjectKind::kSupportSet,
  };
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
// carries the declared tolerances.
struct SurveyObject {
  SurveyObjectKind kind = SurveyObjectKind::kSourceImage;
  std::vector<std::vector<int>> members;
  bool has_position = false;
  double position_altitude_deg = 0.0;
  double position_azimuth_deg = 0.0;
  SurveyEvidence evidence;
  std::optional<SurveyCertificate> certificate;
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

// The request as the document answered it (the echo).
struct SurveyRequestEcho {
  long long crystal_id = 0;
  std::vector<std::vector<int>> path_layers;
  unsigned symmetry_bits = 7;  // P|B|D; 0 = the literal path (the report request's encoding)
  bool wavelength_given = false;
  double wavelength_nm = 0.0;  // as given, 0 when unset
  int budget_ms = 0;           // as resolved (default substituted), before the engine's clamp
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
  SurveyEvidence chromatic;  // v1: unevaluated with the not-wired reason (the chromatic layer
                             // does not follow a T0 early stop; wiring lands in a later slice)

  // The document-level evidence: the T0 routing verdict's own state.
  SurveyEvidence evidence;

  // Meta.
  int budget_ms = 0;  // as resolved
  double total_seconds = 0.0;
};

}  // namespace lumice::raypath::survey

#endif  // RAYPATH_DETAIL_SURVEY_DOCUMENT_H_
