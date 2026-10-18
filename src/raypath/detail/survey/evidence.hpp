#ifndef RAYPATH_DETAIL_SURVEY_EVIDENCE_H_
#define RAYPATH_DETAIL_SURVEY_EVIDENCE_H_

// The survey's evidence-state vocabulary: the four-value main state of the diagnostic-pipeline
// design (certified / finite / refused / unevaluated), closed like the kernel's own enums, with
// the refusal mode as an open string (the analytic-api precedent: status closed / reason open —
// refusal causes cannot be pre-enumerated, but the state itself must be switchable). Every
// declaration block in a survey document carries one of these states; the serializer never omits
// it, so an absent state can never read as "settled".

#include <array>
#include <string_view>
#include <utility>

namespace lumice::raypath::survey {

enum class EvidenceState {
  kCertified,    // closed-form / algebraic / exhaustive proof, with the method named
  kFinite,       // a finite computation; carries a coverage declaration
  kRefused,      // a refusal; the open `mode` string names it
  kUnevaluated,  // not computed; carries the reason
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
  return "finite";
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

// The certified-method vocabulary, v1: the kernel's rank-0 mechanism slug — the same word the
// analytic kernel rolls up for a point-mass path (FocusingClassification::Mechanism), reused
// rather than re-spelled. The registry grows append-only as later legs certify by other means.
constexpr const char* kCertifiedMethodPointMass = "point_mass";

inline bool CertifiedMethodRegistered(std::string_view method) {
  return method == kCertifiedMethodPointMass;
}

}  // namespace lumice::raypath::survey

#endif  // RAYPATH_DETAIL_SURVEY_EVIDENCE_H_
