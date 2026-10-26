#include "raypath/domain/input_metadata.hpp"

namespace lumice::raypath {

const char* WavelengthSourceName(WavelengthSource source) {
  switch (source) {
    case WavelengthSource::kDefault:
      return "default";
    case WavelengthSource::kConfigSingle:
      return "config";
    case WavelengthSource::kUser:
      return "user";
  }
  return "unknown";
}

}  // namespace lumice::raypath
