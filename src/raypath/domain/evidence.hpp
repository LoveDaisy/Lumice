#ifndef LUMICE_RAYPATH_DOMAIN_EVIDENCE_HPP_
#define LUMICE_RAYPATH_DOMAIN_EVIDENCE_HPP_

#include <array>
#include <string>
#include <string_view>
#include <utility>
namespace lumice::raypath::survey {

enum class EvidenceState {
  kCertified,    // closed-form / algebraic / exhaustive proof, with the method named
  kFinite,       // a finite computation; carries a coverage declaration
  kRefused,      // a refusal; the open `mode` string names it
  kUnevaluated,  // not computed; carries the reason
};

struct SurveyCoverage {
  int members_evaluated = 0;
  long long sample_count = 0;
  std::string geometry_basis = "nominal crystal shape at the distribution centres";
  double wavelength_nm = 0.0;
};

struct SurveyEvidence {
  EvidenceState state = EvidenceState::kUnevaluated;
  std::string method;                    // certified
  std::string mode;                      // refused
  std::string reason = "not_requested";  // unevaluated
  SurveyCoverage coverage;               // finite
};

// The name table is the one authority: the writer and the parser below both read it, so a new
// state is one new row — never a second switch arm to keep in step.
inline constexpr std::array<std::pair<std::string_view, EvidenceState>, 4> kEvidenceStateNames{ {
    { "certified", EvidenceState::kCertified },
    { "finite", EvidenceState::kFinite },
    { "refused", EvidenceState::kRefused },
    { "unevaluated", EvidenceState::kUnevaluated },
} };

inline const char* EvidenceStateName(EvidenceState state) {
  for (const auto& entry : kEvidenceStateNames) {
    if (entry.second == state) {
      return entry.first.data();
    }
  }
  return "unevaluated";
}

// False for anything outside the closed vocabulary.
inline bool EvidenceStateFromName(std::string_view name, EvidenceState* out) {
  for (const auto& entry : kEvidenceStateNames) {
    if (entry.first == name) {
      *out = entry.second;
      return true;
    }
  }
  return false;
}

// The certified-method vocabulary: the kernel's rank-0 mechanism slug (the same word the
// analytic kernel rolls up for a point-mass path, FocusingClassification::Mechanism) and the
// survey-side closed-form min-deviation edge (the two-face-chain slab branch's interior minimum,
// which the kernel computes closed-form; the survey restates the formula and cross-checks). The
// registry grows append-only as later legs certify by other means.
constexpr const char* kCertifiedMethodPointMass = "point_mass";
constexpr const char* kCertifiedMethodSlabMinDeviationClosedForm = "slab_min_deviation_closed_form";

inline constexpr std::array<std::string_view, 2> kCertifiedMethods{
  kCertifiedMethodPointMass,
  kCertifiedMethodSlabMinDeviationClosedForm,
};

inline bool CertifiedMethodRegistered(std::string_view method) {
  for (const std::string_view registered : kCertifiedMethods) {
    if (registered == method) {
      return true;
    }
  }
  return false;
}


}  // namespace lumice::raypath::survey

#endif  // LUMICE_RAYPATH_DOMAIN_EVIDENCE_HPP_
