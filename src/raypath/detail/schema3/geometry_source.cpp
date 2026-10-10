#include "raypath/detail/schema3/geometry_source.hpp"

namespace lumice::raypath::schema3 {

ExistenceState ExistenceOf(analytic::CurveExistence existence) {
  switch (existence) {
    case analytic::CurveExistence::kComputed:
      return ExistenceState::kComputed;
    case analytic::CurveExistence::kEscaped:
      return ExistenceState::kEscaped;
    case analytic::CurveExistence::kWalkTruncated:
      return ExistenceState::kWalkTruncated;
    case analytic::CurveExistence::kS4Declared:
      return ExistenceState::kS4Declared;
  }
  return ExistenceState::kWalkTruncated;  // unreachable (-Wswitch total); fail closed anyway
}

ExistenceState ExistenceOfWalkStatus(analytic::WalkStatus status) {
  // The single ruling is dp_contour's (ChainFromBoundaryPieces'): a walk refusal is a
  // truncation of the walk; the contract's kEscaped is partition vocabulary a walk never emits.
  // Reading it through the authority keeps a future ruling change from forking here.
  return ExistenceOf(analytic::ChainFromBoundaryPieces(analytic::BoundaryWalkRecord{}, status).existence);
}

WeightSingularChain ChainOf(const analytic::ChainCurve& chain) {
  WeightSingularChain out;
  out.is_gate_boundary = chain.is_gate_boundary;
  out.existence = ExistenceOf(chain.existence);
  out.u = chain.u;
  out.param = chain.param;
  out.kink = chain.kink;
  out.event = chain.event;
  out.closed = chain.closed;
  return out;
}

CriticalSetCurve CurveOf(const analytic::RestrictedFamilyCurve& curve) {
  CriticalSetCurve out;
  out.existence = ExistenceOf(curve.existence);
  out.closed = curve.closed;
  out.u = curve.u;
  out.tangent = curve.tangent;
  out.d_p = curve.d_p;
  out.wavelengths_nm = curve.wavelengths_nm;
  out.critical_d_p = curve.critical_d_p;
  out.support_param = curve.support_param;
  return out;
}

FiberSampleStream StreamOf(const analytic::OrbitFiberStream& stream) {
  FiberSampleStream out;
  out.samples.resize(stream.samples.size());
  for (size_t i = 0; i < stream.samples.size(); i++) {
    const analytic::OrbitFiberPoint& p = stream.samples[i];
    FiberSample& s = out.samples[i];
    s.u[0] = p.u[0];
    s.u[1] = p.u[1];
    s.u[2] = p.u[2];
    s.area = p.area;
    s.transmission = p.transmission;
    s.valid = p.valid;
    s.jet_degenerate = p.jet_degenerate;
    s.parameter = p.parameter;
    s.weight = p.weight;
  }
  out.binding = MeasureBinding::kFiberParameter;
  out.evidence = FiberSampleStream::EvidenceForm::kSampledExhaustive;
  out.source_name = stream.source_name;
  return out;
}

AxisAssembly AssembleAxis(const analytic::DeviationField& field) {
  AxisAssembly out;
  analytic::BoundaryWalkRecord record;
  const analytic::WalkResult walk = analytic::WalkBoundary(field, analytic::BoundaryWalkOptions{}, &record);
  out.axis.walk_status = walk.status;
  out.axis.walk_closed = walk.status == analytic::WalkStatus::kOk;
  if (!out.axis.walk_closed) {
    // No loop, no partition (the kernel's own fail-closed shape): the certificate is
    // unavailable at its kind-2 object, and there is no loop data to keep.
    out.axis.context.coverage = PartitionContext::Coverage::kUnknown;
    out.axis.message = walk.message;
    return out;
  }
  out.record = std::move(record);
  std::vector<analytic::InteriorCriticalPoint> interior;
  analytic::DegenerateFoldSet fold_set;
  const analytic::DegenerateFoldSet* fold_set_ptr = nullptr;
  if (field.fold().degenerate) {
    fold_set = analytic::BuildDegenerateFoldSet(field, analytic::kFoldCircleSamples);
    fold_set_ptr = &fold_set;
    interior = analytic::SlabInteriorCriticalPoints(field, fold_set);
  } else {
    interior = analytic::InteriorCriticalPointsOf(field, analytic::InteriorNewtonOptions{});
  }
  const analytic::DomainTopology topology = analytic::DomainTopologyOf(field, analytic::kTopologyLatticeN, nullptr, 0);
  const analytic::PartitionResult partition =
      analytic::IntervalPartition(field, interior, fold_set_ptr, walk.loop, topology);
  if (partition.escaped) {
    // The escape is DATA (PartitionResult's mechanical invariant: intervals empty). The slug is
    // the report-side datum; the contract's typed field gets the default (G3 pending — the
    // module docstring records why that field is not authoritative here).
    out.axis.context.coverage = PartitionContext::Coverage::kIncomplete;
    out.axis.regime_slug = analytic::EscapeRegimeName(partition.regime);
    out.axis.message = partition.message;
    return out;
  }
  out.axis.context.coverage = PartitionContext::Coverage::kComplete;
  out.axis.intervals = partition.intervals;
  out.onsets = analytic::FieldOnsets(field, fold_set_ptr, walk.loop, analytic::InteriorNewtonOptions{});
  return out;
}

PartitionedAxis PartitionAxisOf(const analytic::DeviationField& field) {
  return AssembleAxis(field).axis;
}

bool ContractRegimeOfSlug(const std::string& slug, EscapeRegime* out) {
  for (const EscapeRegime regime : RegisteredEscapeRegimes()) {
    // The sentinel is in the table (the registered string face is complete) but is not a regime
    // name (G3): a name lookup must not accept "unset" as an escape answer, so the walk skips it.
    if (regime == EscapeRegime::kUnset) {
      continue;
    }
    if (slug == EscapeRegimeName(regime)) {
      if (out != nullptr) {
        *out = regime;
      }
      return true;
    }
  }
  return false;
}

}  // namespace lumice::raypath::schema3
