#include "raypath/prepared_geometry.hpp"

#include <string>

namespace lumice::raypath {

Error PrepareGeometry(const CrystalConfig& crystal, PreparedGeometry* out) {
  out->conversion = ConvertCrystal(crystal.param_);
  switch (analytic::BuildFaceNormals(out->conversion.shape, &out->table, &out->polygons)) {
    case analytic::Status::kOk:
      return {};
    case analytic::Status::kInvalidConfig:
      return { ErrorCode::kCrystalRejected,
               "the engine builds no crystal from entry " + std::to_string(crystal.id_) + "'s nominal shape" };
    case analytic::Status::kInvalidValue:
      return { ErrorCode::kCrystalRejected,
               "crystal entry " + std::to_string(crystal.id_) + "'s nominal shape has a value the kernel cannot take" };
  }
  return { ErrorCode::kCrystalRejected, "unknown face-normal build status" };
}

}  // namespace lumice::raypath
