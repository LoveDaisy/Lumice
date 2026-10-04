#ifndef LUMICE_RAYPATH_PRODUCT_DIAGNOSTIC_SAMPLER_HPP_
#define LUMICE_RAYPATH_PRODUCT_DIAGNOSTIC_SAMPLER_HPP_

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "analytic/path_feature_discovery.hpp"
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
  ProductDiagnosticSampler(ProductInputSnapshot snapshot, uint32_t seed, ProductSpectrumRequest spectrum);
  const std::vector<DiagnosticDrawDimension>& Dimensions() const { return dimensions_; }
  const ProductInputSnapshot& Snapshot() const { return snapshot_; }
  // Prefix replay: identical snapshot/seed/index produces identical draws,
  // regardless of call order or the eventual number of samples requested.
  Error Draw(uint64_t sample_index, ProductInput* out) const;
  ProductDiagnosticSampler IndependentReplicate() const {
    return ProductDiagnosticSampler(snapshot_, seed_ ^ 0x9e3779b9u, spectrum_);
  }

 private:
  ProductInputSnapshot snapshot_;
  uint32_t seed_;
  ProductSpectrumRequest spectrum_;
  std::vector<DiagnosticDrawDimension> dimensions_;
  std::vector<ShapeDrawPlan> shape_plans_;
};

struct DiagnosticSourceIdentity {
  uint64_t sample_index = 0;
  size_t member_index = 0;
  size_t spectral_row = 0;
};
struct ProductDiagnosticMeasure {
  std::vector<analytic::WeightedSkySample> components;
  // components[i].source_token indexes this table. Actual shapes/poses/source
  // are replayed through run, which owns the snapshot, seed AND spectrum request.
  // A source token is local to this measure, not a cross-run identifier.
  std::vector<DiagnosticSourceIdentity> sources;
  std::shared_ptr<const ProductDiagnosticSampler> run;
  uint64_t completed_samples = 0;
  uint64_t optical_evaluations = 0;
  bool budget_exhausted = false;
};
struct ProductSamplingBudget {
  uint64_t requested_samples = 0;
  uint64_t max_optical_evaluations = 0;
  std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::time_point::max();
};
// One concrete single-crystal chain, all its physical members and spectral rows.
// Only complete outer draws enter the measure. A partial member/spectrum group
// is discarded on deadline, while its spent evaluations remain in the ledger.
Error BuildProductDiagnosticMeasure(const ProductDiagnosticSampler& sampler, const ProductSamplingBudget& budget,
                                    ProductDiagnosticMeasure* out);
Error ReplayDiagnosticSource(const ProductDiagnosticMeasure& measure, uint64_t source_token, ProductInput* out);

}  // namespace lumice::raypath
#endif  // LUMICE_RAYPATH_PRODUCT_DIAGNOSTIC_SAMPLER_HPP_
