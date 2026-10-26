#include "raypath/domain/error.hpp"

namespace lumice::raypath {

const char* ErrorCodeName(ErrorCode code) {
  switch (code) {
    case ErrorCode::kOk:
      return "ok";
    case ErrorCode::kUnknownCrystalId:
      return "unknown_crystal_id";
    case ErrorCode::kMultiLayerUnsupported:
      return "multi_layer_unsupported";
    case ErrorCode::kInvalidPath:
      return "invalid_path";
    case ErrorCode::kFaceNotInCrystal:
      return "face_not_in_crystal";
    case ErrorCode::kWavelengthOutOfRange:
      return "wavelength_out_of_range";
    case ErrorCode::kInvalidTarget:
      return "invalid_target";
    case ErrorCode::kInvalidArgument:
      return "invalid_argument";
    case ErrorCode::kCrystalRejected:
      return "crystal_rejected";
    case ErrorCode::kPathInfeasible:
      return "path_infeasible";
  }
  return "unknown";
}

}  // namespace lumice::raypath
