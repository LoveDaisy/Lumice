#ifndef LUMICE_RAYPATH_PRODUCT_DIAGNOSTIC_SAMPLER_HPP_
#define LUMICE_RAYPATH_PRODUCT_DIAGNOSTIC_SAMPLER_HPP_

#include <cstdint>
#include <string>
#include <vector>

#include "raypath/product_input_assembly.hpp"

namespace lumice::raypath {

// One explicit joint table. Offsets name independent uniform counters, not
// independent statistical observations; Gaussian latents consume two counters.
struct DiagnosticDrawDimension {
  std::string name;
  uint32_t offset = 0;
  uint32_t width = 0;
};
class ProductDiagnosticSampler {
 public:
  explicit ProductDiagnosticSampler(ProductInputSnapshot snapshot, uint32_t seed);
  const std::vector<DiagnosticDrawDimension>& Dimensions() const { return dimensions_; }
  const ProductInputSnapshot& Snapshot() const { return snapshot_; }
  // Prefix replay: identical snapshot/seed/index produces identical draws,
  // regardless of call order or the eventual number of samples requested.
  Error Draw(uint64_t sample_index, const ProductSpectrumRequest& spectrum, ProductInput* out) const;

 private:
  ProductInputSnapshot snapshot_;
  uint32_t seed_;
  std::vector<DiagnosticDrawDimension> dimensions_;
  std::vector<ShapeDrawPlan> shape_plans_;
};

}  // namespace lumice::raypath
#endif  // LUMICE_RAYPATH_PRODUCT_DIAGNOSTIC_SAMPLER_HPP_
