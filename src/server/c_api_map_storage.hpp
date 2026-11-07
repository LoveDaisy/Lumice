#ifndef LUMICE_SERVER_C_API_MAP_STORAGE_HPP_
#define LUMICE_SERVER_C_API_MAP_STORAGE_HPP_

#include <memory>
#include <mutex>
#include <vector>

#include "include/lumice_raypath.h"
#include "raypath/map_result.hpp"

// Private C-handle storage, shared only with white-box composition inspection. This is not
// an export surface. Keeping the actual declaration here lets capacity measurements inspect
// the real owners rather than duplicate a layout or infer capacity from a public column size.
struct LUMICE_PathMapPixelColumns {
  std::vector<double> delta_deg;
  std::vector<double> azimuth_deg;
  std::vector<double> values;
  std::vector<int> counts;
  std::vector<uint8_t> status;
};

struct LUMICE_PathMap_ {
  lumice::raypath::MapResult result;
  mutable std::mutex cache_mutex;
  mutable std::unique_ptr<LUMICE_PathMapPixelColumns> pixels;
  mutable std::unique_ptr<std::vector<const char*>> method_pointers;
};

#endif
