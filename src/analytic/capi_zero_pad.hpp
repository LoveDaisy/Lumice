#ifndef LUMICE_ANALYTIC_CAPI_ZERO_PAD_HPP_
#define LUMICE_ANALYTIC_CAPI_ZERO_PAD_HPP_

// Shared by the two analytic C API bridges (analytic_capi.cpp / dp_capi.cpp): the
// caller-sized-result contract of doc/interfaces/analytic-api.md §8.2. Single authority —
// both bridges carried byte-identical anonymous-namespace copies before consolidation.
#include <cstddef>
#include <cstring>

#include "analytic/path_evaluation.hpp"  // lumice::analytic::Status
#include "lumice_analytic_core.h"

namespace lumice::analytic::capi {

// Zero-fills a caller-allocated result struct after its leading struct_size field, within the
// bytes the caller declared (doc/interfaces/analytic-api.md §8.2) — so a caller older than the
// library keeps the memory it did not size, and struct_size itself is never touched.
template <typename T>
void ZeroAfterStructSize(T* out) {
  const size_t declared = out->struct_size;
  const size_t known = sizeof(T);
  const size_t end = declared < known ? declared : known;
  constexpr size_t kBegin = offsetof(T, struct_size) + sizeof(out->struct_size);
  if (end > kBegin) {
    std::memset(reinterpret_cast<unsigned char*>(out) + kBegin, 0, end - kBegin);
  }
}

// Status -> C error code: the single mapping behind every bridge entry point's catch/translate.
inline LUMICE_ANALYTIC_ErrorCode ToErrorCode(Status status) {
  switch (status) {
    case Status::kOk:
      return LUMICE_ANALYTIC_OK;
    case Status::kInvalidValue:
      return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
    case Status::kInvalidConfig:
      return LUMICE_ANALYTIC_ERR_INVALID_CONFIG;
  }
  return LUMICE_ANALYTIC_ERR_UNKNOWN;
}

}  // namespace lumice::analytic::capi

#endif  // LUMICE_ANALYTIC_CAPI_ZERO_PAD_HPP_
