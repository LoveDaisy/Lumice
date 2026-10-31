// Golden-document comparison for the survey stop pins.
//
// The witness/TIR goldens embed full-precision doubles, and those numbers cross
// transcendental functions whose libm implementations vary at the last 1-2 ULP between ISAs.
// Observed concretely on PR #557's Ubuntu ARM64 leg (Actions run 41206282493): 3 of ~20
// numeric fields in each document differed from the golden by exactly one
// shortest-round-trip digit (e.g. 153.08206187018394 vs 153.0820618701839), while the kept
// counts, the weight's short format, and every non-numeric byte were identical. A raw
// byte-equal comparison against a hard-coded golden is therefore same-machine-only.
//
// The pins compare through CanonicalizeJsonNumbers instead: every floating-point token is
// re-rendered at %.12g — the observed drift lives at digits 16-17, well below that cutoff —
// so a real behavior change (field names, strings, integers, numbers through 12 significant
// digits) still fails the comparison, while cross-ISA last-digit libm variance does not.
// Same-machine determinism is separately pinned byte-exactly by the run-twice assertions in
// the tests themselves.
#pragma once

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace survey_golden_compare {

inline bool IsDigit(char c) {
  return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

// Copy one JSON string literal (from the opening quote through the closing quote) verbatim.
// Returns the index just past the closing quote, or text.size() on an unterminated literal.
inline std::size_t CopyStringLiteral(const std::string& text, std::size_t begin, std::string& out) {
  const std::size_t n = text.size();
  out.push_back(text[begin]);
  std::size_t i = begin + 1;
  while (i < n) {
    out.push_back(text[i]);
    if (text[i] == '\\' && i + 1 < n) {
      out.push_back(text[i + 1]);
      i += 2;
      continue;
    }
    if (text[i] == '"') {
      return i + 1;
    }
    ++i;
  }
  return n;
}

// Scan the JSON number starting at `begin` (the caller has checked it starts with a digit or
// a minus-digit) and return the index just past it.
inline std::size_t NumberTokenEnd(const std::string& text, std::size_t begin) {
  const std::size_t n = text.size();
  std::size_t i = begin + (text[begin] == '-' ? 1 : 0);
  std::size_t digits = 0;
  while (i < n && IsDigit(text[i])) {
    ++i;
    ++digits;
  }
  if (i < n && text[i] == '.') {
    ++i;
    while (i < n && IsDigit(text[i])) {
      ++i;
      ++digits;
    }
  }
  if (digits > 0 && i < n && (text[i] == 'e' || text[i] == 'E')) {
    std::size_t k = i + 1;
    if (k < n && (text[k] == '+' || text[k] == '-')) {
      ++k;
    }
    if (k < n && IsDigit(text[k])) {
      while (k < n && IsDigit(text[k])) {
        ++k;
      }
      i = k;
    }
  }
  return i;
}

// Re-render every floating-point token outside JSON string literals at 12 significant
// digits; every other byte passes through verbatim.
inline std::string CanonicalizeJsonNumbers(const std::string& text) {
  std::string out;
  out.reserve(text.size());
  std::size_t i = 0;
  while (i < text.size()) {
    if (text[i] == '"') {
      i = CopyStringLiteral(text, i, out);
      continue;
    }
    const bool numeric_start = IsDigit(text[i]) || (text[i] == '-' && i + 1 < text.size() && IsDigit(text[i + 1]));
    if (!numeric_start) {
      out.push_back(text[i]);
      ++i;
      continue;
    }
    const std::size_t end = NumberTokenEnd(text, i);
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.12g", std::strtod(text.substr(i, end - i).c_str(), nullptr));
    out += buf;
    i = end;
  }
  return out;
}

}  // namespace survey_golden_compare
