#ifndef RAYPATH_DETAIL_JSON_VALUES_HPP_
#define RAYPATH_DETAIL_JSON_VALUES_HPP_

// The single owner of the JSON number encoding shared by both raypath schemas
// (single-path and feature report): a non-finite double (NaN and the
// infinities, which JSON cannot spell) is written as null, and the sun-grid
// arrays are rounded to kSunGridSignificantDigits before writing. The two
// serializers must not hand-roll their own copies — the contract statement
// lives in doc/raypath-cli-output.md.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <vector>

#include "raypath/single_path_json.hpp"

namespace lumice::raypath::detail {

// A double as a JSON value: NaN and the infinities (which JSON cannot spell) become null.
inline nlohmann::ordered_json Num(double x) {
  if (!std::isfinite(x)) {
    return nullptr;
  }
  return x;
}

// Rounded to kSunGridSignificantDigits: the shortest decimal of the rounded double then has at most
// that many digits, which is the whole point (size), and it reads back as the rounded value.
inline nlohmann::ordered_json GridNum(double x) {
  if (!std::isfinite(x)) {
    return nullptr;
  }
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.*g", kSunGridSignificantDigits, x);
  return std::strtod(buf, nullptr);
}

inline nlohmann::ordered_json Array(const double* v, int n) {
  nlohmann::ordered_json a = nlohmann::ordered_json::array();
  for (int i = 0; i < n; i++) {
    a.push_back(Num(v[i]));
  }
  return a;
}

inline nlohmann::ordered_json Array(const std::vector<double>& v) {
  return Array(v.data(), static_cast<int>(v.size()));
}

}  // namespace lumice::raypath::detail
#endif  // RAYPATH_DETAIL_JSON_VALUES_HPP_
