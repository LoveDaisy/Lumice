#ifndef LUMICE_RAYPATH_PREPARED_GEOMETRY_HPP_
#define LUMICE_RAYPATH_PREPARED_GEOMETRY_HPP_

// The lazily prepared geometry one query needs: the selected crystal's conversion and its face
// tables — what every face builds internally today (the same ConvertCrystal + BuildFaceNormals
// pattern at one site per face). A workspace holds at most one (the current selection's),
// builds it on first query and reuses it; a face handed a prepared geometry skips its own
// build, so requery does not rebuild and switching selection builds only the new crystal.
// The tables are the analytic kernel's own value types (fixed arrays): owned by value, safe to
// share across queries, and never referencing the caller's config.

#include "analytic/path_evaluation.hpp"  // BuildFaceNormals, Status, the face tables
#include "raypath/domain/error.hpp"
#include "raypath/scene_to_analytic.hpp"

namespace lumice::raypath {

struct PreparedGeometry {
  CrystalConversion conversion;
  analytic::FaceNormalTable table;
  analytic::FacePolygonTable polygons;
};

// Builds the prepared geometry for one crystal entry; the errors are the faces' own refusal
// vocabulary (crystal_rejected with the kernel's own reason), so a caller passes them through.
// (config/config_manager.hpp — the crystal entry — arrives via scene_to_analytic.hpp.)
Error PrepareGeometry(const CrystalConfig& crystal, PreparedGeometry* out);

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_PREPARED_GEOMETRY_HPP_
