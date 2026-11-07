#ifndef LUMICE_TEST_MAP_STRESS_FIXTURE_HPP
#define LUMICE_TEST_MAP_STRESS_FIXTURE_HPP

#include "include/lumice_raypath.h"

namespace lumice::test {

// C01's public C request at the actual cell limit. The two window slots lie wholly above
// the minimum-deviation edge, so they also exercise positive-value normalization/Fit at
// maximum capacity rather than timing an all-zero maximum-sized array.
inline LUMICE_PathMapRequest MaximumMapRequest(int slot) {
  static constexpr int kFaces[] = { 3, 5 };
  static constexpr int kCounts[] = { 2 };
  LUMICE_PathMapRequest request{};
  request.struct_size = sizeof(request);
  request.crystal_id = 1;
  request.faces = kFaces;
  request.face_count = 2;
  request.layer_face_counts = kCounts;
  request.layer_count = 1;
  request.full_sky = slot == 0 ? 1 : 0;
  request.sample_count = 500000;
  request.view_rows = 512;
  request.view_cols = 512;
  request.centre_altitude_deg = -31.0;
  request.centre_azimuth_deg = slot * 5.0;
  request.delta_span_deg = 18.0;
  request.azimuth_span_deg = 8.0;
  return request;
}

}  // namespace lumice::test

#endif
