// Golden-document comparison for the survey stop pins.
//
// What the pins may assert depends on which invariance actually holds, and two CI runs on
// PR #557 measured it directly:
//
// - Same machine: two unstopped runs serialize byte-identically (the run-twice assertions in
//   the tests; held everywhere including Ubuntu ARM64). Per-machine determinism is real.
// - Across machines/ISAs the full-precision doubles drift, in two distinct classes observed
//   on the Ubuntu ARM64 legs (runs 41206282493 and 41207661328): the witness document's
//   transcendental-chain fields drifted 1-2 ULP (digits 16-17), and the TIR interface shares
//   — ratios of weight sums over kept events — drifted further, up to ~1.5e-6 absolute on an
//   O(0.65) share and ~4e-10 absolute on a 2.8e-6 share (FP accumulation/reassociation
//   differences). kept counts, sample counts, and every non-numeric byte stayed identical in
//   both runs.
//
// So a golden pin here asserts two things with two mechanisms. Structure and non-numeric
// bytes must match exactly: DocumentsMatchWithinTolerance walks both documents in lockstep
// and compares string-literal bodies, field names and punctuation byte-for-byte. Numeric
// tokens are compared pairwise under |run - golden| <= atol + rtol * |golden| with
// atol = 1e-8 and rtol = 1e-5 — the observed cross-ISA drift sits 6-25x inside that
// envelope, while any real behavior change (a different argmax pose, a swapped red/blue
// share, a wrong face, a shifted representative) moves values far outside it.
#pragma once

#include <cctype>
#include <cstdlib>
#include <string>

namespace survey_golden_compare {

inline bool IsDigit(char c) {
  return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

// Compare one JSON string literal pair byte-for-byte (bodies and escapes). Both `i` and `j`
// must sit on an opening quote; both advance past their closing quotes. False on any byte
// or length mismatch, including a literal that only closes on one side.
inline bool CompareStringLiterals(const std::string& run, std::size_t& i, const std::string& golden, std::size_t& j) {
  const std::size_t n = run.size();
  const std::size_t m = golden.size();
  std::size_t a = i + 1;
  std::size_t b = j + 1;
  while (true) {
    const bool a_closed = a >= n || run[a] == '"';
    const bool b_closed = b >= m || golden[b] == '"';
    if (a_closed != b_closed) {
      return false;
    }
    if (a_closed) {
      i = a < n ? a + 1 : a;
      j = b < m ? b + 1 : b;
      return true;
    }
    if (run[a] != golden[b]) {
      return false;
    }
    if (run[a] == '\\' && a + 1 < n) {
      if (b + 1 >= m || golden[b + 1] != run[a + 1]) {
        return false;
      }
      a += 2;
      b += 2;
      continue;
    }
    ++a;
    ++b;
  }
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

// True when both documents carry the identical structure — identical string bodies, field
// names and punctuation, the same number of numeric tokens — and every numeric token pair
// satisfies |run - golden| <= atol + rtol * |golden|.
inline bool DocumentsMatchWithinTolerance(const std::string& run, const std::string& golden, double atol = 1e-8,
                                          double rtol = 1e-5) {
  std::size_t i = 0;
  std::size_t j = 0;
  while (i < run.size() && j < golden.size()) {
    const char rc = run[i];
    const char gc = golden[j];
    if (rc == '"' && gc == '"') {
      if (!CompareStringLiterals(run, i, golden, j)) {
        return false;
      }
      continue;
    }
    const bool r_numeric = IsDigit(rc) || (rc == '-' && i + 1 < run.size() && IsDigit(run[i + 1]));
    const bool g_numeric = IsDigit(gc) || (gc == '-' && j + 1 < golden.size() && IsDigit(golden[j + 1]));
    if (r_numeric != g_numeric) {
      return false;
    }
    if (r_numeric) {
      const std::size_t r_end = NumberTokenEnd(run, i);
      const std::size_t g_end = NumberTokenEnd(golden, j);
      const double rv = std::strtod(run.substr(i, r_end - i).c_str(), nullptr);
      const double gv = std::strtod(golden.substr(j, g_end - j).c_str(), nullptr);
      const double diff = rv - gv < 0 ? gv - rv : rv - gv;
      if (!(diff <= atol + rtol * (gv < 0 ? -gv : gv))) {
        return false;
      }
      i = r_end;
      j = g_end;
      continue;
    }
    if (rc != gc) {
      return false;
    }
    ++i;
    ++j;
  }
  return i >= run.size() && j >= golden.size();
}

}  // namespace survey_golden_compare
