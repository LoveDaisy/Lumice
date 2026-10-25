#ifndef LUMICE_RAYPATH_DETAIL_DIAGNOSTIC_SAMPLER_HPP_
#define LUMICE_RAYPATH_DETAIL_DIAGNOSTIC_SAMPLER_HPP_

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "analytic/path_feature_discovery.hpp"
#include "raypath/detail/input_assembly.hpp"
#include "raypath/results/report_records.hpp"

namespace lumice::analytic {
struct DiscoveryStop;  // the shared stop judgment (analytic/stop.hpp, via discovery.hpp)
}

#include "raypath/query_control.hpp"  // kPollSamplerDraws

namespace lumice::raypath {

// One explicit joint table. Offsets name independent uniform counters, not
// independent statistical observations; Gaussian latents consume two counters.
struct DiagnosticDrawDimension {
  std::string name;
  uint32_t offset = 0;
  uint32_t width = 0;
};
class DiagnosticSampler {
 public:
  DiagnosticSampler(InputSnapshot snapshot, uint32_t seed, SpectrumRequest spectrum);
  const std::vector<DiagnosticDrawDimension>& Dimensions() const { return dimensions_; }
  const InputSnapshot& Snapshot() const { return snapshot_; }
  // Prefix replay: identical snapshot/seed/index produces identical draws,
  // regardless of call order or the eventual number of samples requested.
  Error Draw(uint64_t sample_index, AssembledInput* out) const;
  Error Reassemble(const std::vector<LayerSample>& samples, const SourceSample& source, AssembledInput* out) const {
    return AssembleInput(snapshot_, samples, source, spectrum_, out);
  }
  DiagnosticSampler IndependentReplicate() const {
    return DiagnosticSampler(snapshot_, seed_ ^ 0x9e3779b9u, spectrum_);
  }

 private:
  InputSnapshot snapshot_;
  uint32_t seed_;
  SpectrumRequest spectrum_;
  std::vector<DiagnosticDrawDimension> dimensions_;
  std::vector<ShapeDrawPlan> shape_plans_;
};


struct DiagnosticMeasure : DiagnosticMeasureFacts {
  std::shared_ptr<const DiagnosticSampler> run;
};
struct SamplingBudget {
  uint64_t requested_samples = 0;
  uint64_t max_optical_evaluations = 0;
  std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::time_point::max();
  // When set, this IS the draw loops' judgment: the deadline above and any cancel token fold
  // into one stop (the single-authority rule) and the loops poll it instead of the raw clock.
  // Null: exactly today's deadline-only checks.
  const analytic::DiscoveryStop* stop = nullptr;
};
// The draw/iteration loops' one judgment (the single-authority rule): a stop present folds its
// deadline and cancel token into one verdict; absent, the raw deadline-only check runs exactly
// as before. The evaluation cap rides alongside in both shapes.
inline bool SamplingBudgetExceeded(const SamplingBudget& budget, uint64_t optical_evaluations) {
  if (optical_evaluations >= budget.max_optical_evaluations) {
    return true;
  }
  if (budget.stop != nullptr) {
    if (budget.stop->token != nullptr) {
      budget.stop->token->NotePoll(kPollSamplerDraws);
    }
    return budget.stop->Stopped();
  }
  return std::chrono::steady_clock::now() >= budget.deadline;
}

// One concrete single-crystal chain, all its physical members and spectral rows.
// Only complete outer draws enter the measure. A partial member/spectrum group
// is discarded on deadline, while its spent evaluations remain in the ledger.
Error BuildDiagnosticMeasure(const DiagnosticSampler& sampler, const SamplingBudget& budget, DiagnosticMeasure* out);
Error ReplayDiagnosticSource(const DiagnosticMeasure& measure, uint64_t source_token, AssembledInput* out);

}  // namespace lumice::raypath
#endif  // LUMICE_RAYPATH_DETAIL_DIAGNOSTIC_SAMPLER_HPP_
