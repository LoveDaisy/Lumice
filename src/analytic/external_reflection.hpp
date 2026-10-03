#ifndef LUMICE_ANALYTIC_EXTERNAL_REFLECTION_HPP_
#define LUMICE_ANALYTIC_EXTERNAL_REFLECTION_HPP_

// One-face external reflection from a finite convex crystal face. This is distinct from the
// transmitted/internal-reflection/transmitted chain in path_chain.hpp: the incident medium is air,
// the reflected branch never enters the crystal, and the finite measure is the projected area of
// the one physical face.

#include <cstdint>

#include "analytic/path_evaluation.hpp"

namespace lumice::analytic {

enum class ExternalReflectionStatus {
  kOk,
  kEntryBackface,
  kDegenerateFace,
  kNonFinite,
};

struct ExternalReflectionResult {
  ExternalReflectionStatus status = ExternalReflectionStatus::kNonFinite;
  double outgoing_direction[3]{};  // body-frame propagation direction
  double entry_measure = 0.0;      // projected finite-face area
  double reflectance = 0.0;        // unpolarised Fresnel reflection coefficient
  double incidence_cosine = 0.0;
  double tir_discriminant = 0.0;
  uint64_t topology_signature = 0;
};

class ExternalReflectionFace {
 public:
  ExternalReflectionFace(const FaceNormalTable& normals, const FacePolygonTable& polygons, int slot);

  ExternalReflectionResult Evaluate(const double incident_body[3], double refractive_index) const;

  double Area() const { return area_; }

 private:
  double normal_[3]{};
  double area_ = 0.0;
  int corner_count_ = 0;
};

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_EXTERNAL_REFLECTION_HPP_
