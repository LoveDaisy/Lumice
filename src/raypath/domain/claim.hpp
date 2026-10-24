#ifndef LUMICE_RAYPATH_DOMAIN_CLAIM_HPP_
#define LUMICE_RAYPATH_DOMAIN_CLAIM_HPP_

#include <string>
#include <variant>

#include "raypath/domain/scope.hpp"

namespace lumice::raypath {

struct ClaimScope {
  MemberScope members;
  GeometryScope geometry;
  ObservationScope observation;
  MeasureScope measure;
};
struct ClaimCoverage {
  int members_evaluated = 0;
  long long sample_count = 0;
  std::string geometry_basis;
};
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
const T* ClaimValue(const Claim<T>& claim) {
  if (const auto* finite = std::get_if<Finite<T>>(&claim)) {
    return &finite->value;
  }
  if (const auto* certified = std::get_if<Certified<T>>(&claim)) {
    return &certified->value;
  }
  return nullptr;
}

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_DOMAIN_CLAIM_HPP_
