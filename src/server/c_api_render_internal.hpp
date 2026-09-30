#ifndef SERVER_C_API_RENDER_INTERNAL_H_
#define SERVER_C_API_RENDER_INTERNAL_H_

// Render-layer internals of the C API bridges: the LUMICE_AnnotationView translation and
// validation, shared by the render bridge and the engine bridge's IN_FRAME analysis request. NOT
// part of the public C API: do not include it from src/gui/ or ship it to consumers.

#include "core/annotation_overlay.hpp"
#include "include/lumice_render.h"

// Translate the public LUMICE_AnnotationView into core's internal
// lumice::annotation::ViewSnapshot. This is the single implementation of that field mapping
// (a56: same semantics, one owner) — shared by the render bridge, the engine bridge's IN_FRAME
// analysis request and the test-only lumice_test_api.cpp LUMICE_TEST_ComputeRenderDomainMask hook,
// which otherwise carried a hand-copied second translation of the same struct.
lumice::annotation::ViewSnapshot ToAnnotationViewSnapshot(const LUMICE_AnnotationView& v);

namespace lumice::capi {

// The view validation LUMICE_ComputeAnnotationAnchors applies, for the other takers of a
// LUMICE_AnnotationView (the IN_FRAME analysis request, LUMICE_UnprojectPixel and
// LUMICE_ProjectDirection).
inline bool AnnotationViewEnumsValid(const LUMICE_AnnotationView& v) {
  if (v.lens_type < 0 || v.lens_type > LUMICE_LENS_TYPE_GLOBE) {
    return false;
  }
  return v.visible == LUMICE_VISIBLE_UPPER || v.visible == LUMICE_VISIBLE_LOWER || v.visible == LUMICE_VISIBLE_FULL;
}

}  // namespace lumice::capi

#endif  // SERVER_C_API_RENDER_INTERNAL_H_
