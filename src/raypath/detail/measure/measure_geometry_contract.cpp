#include "raypath/detail/measure/measure_geometry_contract.hpp"

namespace lumice::raypath {

// The registered value tables of the contract's open enums (see the header's registry block).
// Appending a value to an enum without extending its table leaves the coverage test green only
// until a caller walks the new value — the test pins the table-enum pair, not the promise.

const char* ExistenceStateName(ExistenceState state) {
  switch (state) {
    case ExistenceState::kComputed:
      return "computed";
    case ExistenceState::kEscaped:
      return "escaped";
    case ExistenceState::kWalkTruncated:
      return "walk_truncated";
    case ExistenceState::kS4Declared:
      return "s4_declared";
  }
  return "unknown";
}

const std::vector<ExistenceState>& RegisteredExistenceStates() {
  static const std::vector<ExistenceState> kStates = {
    ExistenceState::kComputed,
    ExistenceState::kEscaped,
    ExistenceState::kWalkTruncated,
    ExistenceState::kS4Declared,
  };
  return kStates;
}

const char* EscapeRegimeName(EscapeRegime regime) {
  switch (regime) {
    case EscapeRegime::kSlabCrease:
      return "slab_crease";
  }
  return "unknown";
}

const std::vector<EscapeRegime>& RegisteredEscapeRegimes() {
  static const std::vector<EscapeRegime> kRegimes = { EscapeRegime::kSlabCrease };
  return kRegimes;
}

const char* ChainEventKindName(ChainEventKind kind) {
  switch (kind) {
    case ChainEventKind::kTirBoundary:
      return "tir_boundary";
    case ChainEventKind::kPathInfeasible:
      return "path_infeasible";
    case ChainEventKind::kGatedOut:
      return "gated_out";
    case ChainEventKind::kCorridorClosed:
      return "corridor_closed";
  }
  return "unknown";
}

const std::vector<ChainEventKind>& RegisteredChainEventKinds() {
  static const std::vector<ChainEventKind> kKinds = {
    ChainEventKind::kTirBoundary,
    ChainEventKind::kPathInfeasible,
    ChainEventKind::kGatedOut,
    ChainEventKind::kCorridorClosed,
  };
  return kKinds;
}

const char* MeasureBindingName(MeasureBinding binding) {
  switch (binding) {
    case MeasureBinding::kSolidAngle:
      return "solid_angle";
    case MeasureBinding::kFiberParameter:
      return "fiber_parameter";
  }
  return "unknown";
}

const std::vector<MeasureBinding>& RegisteredMeasureBindings() {
  static const std::vector<MeasureBinding> kBindings = { MeasureBinding::kSolidAngle, MeasureBinding::kFiberParameter };
  return kBindings;
}

const char* EvidenceFormName(FiberSampleStream::EvidenceForm form) {
  switch (form) {
    case FiberSampleStream::EvidenceForm::kStructural:
      return "structural";
    case FiberSampleStream::EvidenceForm::kSampledExhaustive:
      return "sampled_exhaustive";
    case FiberSampleStream::EvidenceForm::kSampledPartial:
      return "sampled_partial";
  }
  return "unknown";
}

const std::vector<FiberSampleStream::EvidenceForm>& RegisteredEvidenceForms() {
  static const std::vector<FiberSampleStream::EvidenceForm> kForms = {
    FiberSampleStream::EvidenceForm::kStructural,
    FiberSampleStream::EvidenceForm::kSampledExhaustive,
    FiberSampleStream::EvidenceForm::kSampledPartial,
  };
  return kForms;
}

}  // namespace lumice::raypath
