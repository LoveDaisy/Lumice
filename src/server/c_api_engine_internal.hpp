#ifndef SERVER_C_API_ENGINE_INTERNAL_H_
#define SERVER_C_API_ENGINE_INTERNAL_H_

// Engine-layer internals of the C API bridges: the server's Error vocabulary as a C return code,
// and the test door into the result-frame handle. NOT part of the public C API: do not include it
// from src/gui/ or ship it to consumers.

#include <memory>

#include "include/lumice_engine.h"
#include "server/result_types.hpp"  // Error, ErrorCode, ResultFrame

namespace lumice::capi {

// The server's Error vocabulary as a C API return code: the one mapping, shared by every bridge
// that reports a lumice::Error — the single-path bridge among them, which parses a scene through
// the same lumice::ParseConfigManager. Inline in an internal namespace: it is no global symbol of
// the engine, and no export list can pick it up.
inline LUMICE_ErrorCode ToCApiErrorCode(ErrorCode code) {
  switch (code) {
    case ErrorCode::kSuccess:
      return LUMICE_OK;
    case ErrorCode::kInvalidJson:
      return LUMICE_ERR_INVALID_JSON;
    case ErrorCode::kInvalidConfig:
      return LUMICE_ERR_INVALID_CONFIG;
    case ErrorCode::kMissingField:
      return LUMICE_ERR_MISSING_FIELD;
    case ErrorCode::kInvalidValue:
      return LUMICE_ERR_INVALID_VALUE;
    case ErrorCode::kServerNotReady:
    case ErrorCode::kServerError:
    default:
      return LUMICE_ERR_SERVER;
  }
}

}  // namespace lumice::capi

// Test-only: wrap a C++ result frame in the handle the LUMICE_FrameGet* family reads, so a test can
// feed those getters a frame with contents no real run produces (a chain past the C struct's layer
// / segment caps, for the truncation branch). Same ownership as LUMICE_AcquireResultFrame's handle:
// release with LUMICE_ReleaseResultFrame. NOT part of the public C API.
LUMICE_ResultFrame* WrapResultFrameForTest(std::shared_ptr<const lumice::ResultFrame> frame);

#endif  // SERVER_C_API_ENGINE_INTERNAL_H_
