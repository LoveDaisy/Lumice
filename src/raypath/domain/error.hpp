#ifndef LUMICE_RAYPATH_DOMAIN_ERROR_HPP_
#define LUMICE_RAYPATH_DOMAIN_ERROR_HPP_

#include <string>

namespace lumice::raypath {

enum class ErrorCode {
  kOk,
  kUnknownCrystalId,       // no crystal entry with this id in the config
  kMultiLayerUnsupported,  // more than one scattering layer: v1 traces single-layer paths only
  kInvalidPath,            // no layer, or a face sequence shorter than 2 or longer than the kernel's bound
  kFaceNotInCrystal,       // a face number this crystal (at its nominal shape) does not have
  kWavelengthOutOfRange,   // outside the refractive-index table [350, 900] nm
  kInvalidTarget,          // non-finite, or at the sun / its antipode (no fiber is defined there)
  kInvalidArgument,        // an option out of range (sample count, grid size, a warm seed that is no rotation)
  kCrystalRejected,        // the engine's closed-form gate rejects the nominal shape
  kPathInfeasible,         // no pose at all realises this face sequence
};

struct Error {
  ErrorCode code = ErrorCode::kOk;
  std::string message;
  bool Ok() const { return code == ErrorCode::kOk; }
};

// Stable lower-case name of an error code (for a shell's output).
const char* ErrorCodeName(ErrorCode code);

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_DOMAIN_ERROR_HPP_
