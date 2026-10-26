#ifndef RAYPATH_DETAIL_JSON_VALUES_HPP_
#define RAYPATH_DETAIL_JSON_VALUES_HPP_

// The shared JSON number encoding for the four raypath result families. Non-finite doubles
// become null, and the sun-grid arrays are rounded before writing.

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <vector>

#include "raypath/single_path_json.hpp"
#include "raypath/survey_context.hpp"

namespace lumice::raypath {
nlohmann::ordered_json SurveyContextJson(const SurveyContextBlock& context);
}

namespace lumice::raypath::detail {

inline std::atomic<long> g_json_encode_calls{ 0 };
inline std::atomic<long> g_json_decode_calls{ 0 };

inline long JsonEncodeCalls() {
  return g_json_encode_calls.load(std::memory_order_relaxed);
}
inline long JsonDecodeCalls() {
  return g_json_decode_calls.load(std::memory_order_relaxed);
}

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
