#ifndef LUMICE_RAYPATH_DOMAIN_CLAIM_HPP_
#define LUMICE_RAYPATH_DOMAIN_CLAIM_HPP_

#include <string>
#include <variant>

#include "raypath/domain/evidence.hpp"
#include "raypath/domain/scope.hpp"

namespace lumice::raypath {

struct ClaimScope {
  std::optional<QuantitySpec> quantity;
  std::shared_ptr<const QuantityContext> input;
  MemberScope members;
  GeometryScope geometry;
  ObservationScope observation;
  MeasureScope measure;
};
using ClaimCoverage = survey::SurveyCoverage;
struct Refused {
  std::string reason;
  ClaimScope asked_scope;
};
struct Unevaluated {
  std::string reason = "not_requested";
  ClaimScope asked_scope;
};
template <class T>
struct Certified {
  using ValueType = T;
  T value;
  std::string method;
  ClaimScope scope;
};
template <class T>
struct Finite {
  T value;
  ClaimCoverage coverage;
  ClaimScope scope;
};

// Default is explicitly unevaluated. Refusal arms have no T to accidentally read.
template <class T>
using Claim = std::variant<Unevaluated, Refused, Certified<T>, Finite<T>>;

template <class T>
survey::EvidenceState ClaimState(const Claim<T>& claim) {
  if (std::holds_alternative<Certified<T>>(claim))
    return survey::EvidenceState::kCertified;
  if (std::holds_alternative<Finite<T>>(claim))
    return survey::EvidenceState::kFinite;
  if (std::holds_alternative<Refused>(claim))
    return survey::EvidenceState::kRefused;
  return survey::EvidenceState::kUnevaluated;
}

template <class T>
const T* ClaimValue(const Claim<T>& claim) {
  if (const auto* finite = std::get_if<Finite<T>>(&claim)) {
    return &finite->value;
  }
  if (const auto* certified = std::get_if<Certified<T>>(&claim)) {
    return &certified->value;
  }
  return nullptr;
}

// Mutable only while assembling a product; completed roots own this through const storage.
// Every numeric read must select a success arm. Refused/unevaluated have no T at all.
template <class T>
class ClaimedValue {
 public:
  survey::EvidenceState State() const { return ClaimState(claim_); }
  const ClaimScope& Scope() const {
    if (const auto* value = std::get_if<Finite<T>>(&claim_))
      return value->scope;
    if (const auto* value = std::get_if<Certified<T>>(&claim_))
      return value->scope;
    if (const auto* value = std::get_if<Refused>(&claim_))
      return value->asked_scope;
    return std::get<Unevaluated>(claim_).asked_scope;
  }
  bool SetState(survey::EvidenceState state) {
    if (state == State())
      return true;
    const ClaimScope scope = Scope();
    switch (state) {
      case survey::EvidenceState::kCertified:
        claim_ = Certified<T>{};
        break;
      case survey::EvidenceState::kFinite:
        claim_ = Finite<T>{};
        break;
      case survey::EvidenceState::kRefused:
        claim_ = Refused{};
        break;
      case survey::EvidenceState::kUnevaluated:
        claim_ = Unevaluated{};
        break;
      default:
        return false;
    }
    BindScope(scope);
    return true;
  }
  T* ValueOrNull() {
    if (auto* value = std::get_if<Finite<T>>(&claim_))
      return &value->value;
    if (auto* value = std::get_if<Certified<T>>(&claim_))
      return &value->value;
    return nullptr;
  }
  const T* ValueOrNull() const { return ClaimValue(claim_); }
  T& Value() {
    if (auto* value = std::get_if<Finite<T>>(&claim_))
      return value->value;
    return std::get<Certified<T>>(claim_).value;
  }
  const T& Value() const {
    if (const auto* value = std::get_if<Finite<T>>(&claim_))
      return value->value;
    return std::get<Certified<T>>(claim_).value;
  }
  ClaimCoverage& Coverage() { return std::get<Finite<T>>(claim_).coverage; }
  const ClaimCoverage& Coverage() const { return std::get<Finite<T>>(claim_).coverage; }
  std::string& Method() { return std::get<Certified<T>>(claim_).method; }
  const std::string& Method() const { return std::get<Certified<T>>(claim_).method; }
  std::string& Mode() { return std::get<Refused>(claim_).reason; }
  const std::string& Mode() const { return std::get<Refused>(claim_).reason; }
  std::string& Reason() { return std::get<Unevaluated>(claim_).reason; }
  const std::string& Reason() const { return std::get<Unevaluated>(claim_).reason; }
  const Claim<T>& Declaration() const { return claim_; }
  void BindScope(const ClaimScope& scope) {
    if (auto* value = std::get_if<Finite<T>>(&claim_))
      value->scope = scope;
    if (auto* value = std::get_if<Certified<T>>(&claim_))
      value->scope = scope;
    if (auto* value = std::get_if<Refused>(&claim_))
      value->asked_scope = scope;
    if (auto* value = std::get_if<Unevaluated>(&claim_))
      value->asked_scope = scope;
  }
  survey::SurveyEvidence Evidence() const {
    survey::SurveyEvidence evidence;
    evidence.state = State();
    if (const auto* value = std::get_if<Finite<T>>(&claim_))
      evidence.coverage = value->coverage;
    if (const auto* value = std::get_if<Certified<T>>(&claim_))
      evidence.method = value->method;
    if (const auto* value = std::get_if<Refused>(&claim_))
      evidence.mode = value->reason;
    if (const auto* value = std::get_if<Unevaluated>(&claim_))
      evidence.reason = value->reason;
    return evidence;
  }

 private:
  Claim<T> claim_;
};

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_DOMAIN_CLAIM_HPP_
