#include "raypath/detail/measure/fiber_quadrature.hpp"

namespace lumice::raypath {

FiberQuadratureResult QuadratureIntensity(const UMarginal& measure, const FiberSampleStream& stream,
                                          double angular_tol_rad) {
  FiberQuadratureResult out;
  out.total = static_cast<int>(stream.samples.size());
  for (const FiberSample& s : stream.samples) {
    // The SampleEvent positive-part convention: kept iff A*T > 0. A dark sample contributes
    // nothing and is counted out — "all dark" stays distinguishable from "no samples".
    if (!(s.area * s.transmission > 0.0) || !s.valid) {
      continue;
    }
    double mu = 0.0;
    switch (stream.binding) {
      case MeasureBinding::kSolidAngle:
        // dOmega weights need a regular area support; anything else cannot answer this binding.
        if (measure.kind() != USupportKind::kArea) {
          out.binding_mismatch++;
          continue;
        }
        mu = measure.DensitySolidAngle(s.u);
        if (mu <= 0.0) {
          // Off-support for a two-dimensional support: outside the declared measure, the sample
          // is not part of the integral (it does count toward in_support bookkeeping only when
          // the measure actually covers it).
          if (measure.MuPositive(s.u, angular_tol_rad)) {
            out.in_support++;
          }
          continue;
        }
        out.in_support++;
        break;
      case MeasureBinding::kFiberParameter:
        // The stream declares its own fiber parameterization; v1 carries spin orbits, whose
        // density the measure answers directly at the sample's parameter.
        if (measure.kind() != USupportKind::kSpinOrbit) {
          out.binding_mismatch++;  // the binding and the measure disagree: no density, no
                                   // contribution — but the mismatch is reportable, not silent
          continue;
        }
        mu = measure.SpinOrbitThetaDensity(s.parameter);
        if (!(mu > 0.0)) {
          continue;
        }
        out.in_support++;
        break;
      default:
        // An out-of-registry binding value must not read as mu = 0 with the sample silently
        // dropped (indistinguishable from dark): the same reportable path as a kind mismatch.
        // The registered table (RegisteredMeasureBindings) + the coverage walk are the a50
        // half; this default is the other — a future value without its routing arm lands here
        // and is COUNTED, not swallowed.
        out.binding_mismatch++;
        continue;
    }
    out.intensity += mu * s.area * s.transmission * s.weight;
    out.kept++;
  }
  return out;
}

TintQuotient TintRatio(const FiberQuadratureResult& blue, const FiberQuadratureResult& red) {
  TintQuotient q;
  q.numerator = blue.intensity;
  q.denominator = red.intensity;
  if (red.intensity > 0.0) {
    q.ratio = blue.intensity / red.intensity;
    q.defined = true;
  }
  return q;
}

}  // namespace lumice::raypath
