#include "raypath/detail/measure/visibility_certificate.hpp"

#include <cmath>
#include <vector>

namespace lumice::raypath {

const char* VisibilityStateName(VisibilityState state) {
  switch (state) {
    case VisibilityState::kCertified:
      return "certified";
    case VisibilityState::kPartial:
      return "partial";
    case VisibilityState::kUnlit:
      return "unlit";
    case VisibilityState::kUnproven:
      return "unproven";
  }
  return "unknown";
}

const std::vector<VisibilityState>& RegisteredVisibilityStates() {
  static const std::vector<VisibilityState> kStates = { VisibilityState::kCertified, VisibilityState::kPartial,
                                                        VisibilityState::kUnlit, VisibilityState::kUnproven };
  return kStates;
}

namespace {

// The stable unproven reasons (the report echoes them; a new reason is a new string, not a
// reworded one).
constexpr const char* kReasonEscape = "partition_escape";
constexpr const char* kReasonTruncated = "kind1_walk_truncated";
constexpr const char* kReasonKind1Escaped = "kind1_escaped";
constexpr const char* kReasonNoSupportSamples = "no_in_support_samples";
constexpr const char* kReasonPartialEvidence = "sampled_partial_evidence";
constexpr const char* kReasonNoKind1 = "no_kind1_curve";
constexpr const char* kReasonDegenerateMeasure = "degenerate_measure";
constexpr const char* kReasonDegenerateJets = "degenerate_jets";
constexpr const char* kReasonKind1MeasureZero = "kind1_curve_measure_zero";

}  // namespace

VisibilityCertificate CertifyVisibility(const UMarginal& measure, const FiberSampleStream& stream,
                                        const CriticalSetCurve* kind1, const PartitionContext* partition,
                                        double angular_tol_rad) {
  VisibilityCertificate out;
  out.evidence = stream.evidence;

  // Fail-closed routing first (the header's ordered list).
  if (measure.kind() == USupportKind::kDegenerateSunGeometry ||
      measure.kind() == USupportKind::kDegenerateAxisGeometry) {
    out.state = VisibilityState::kUnproven;
    out.reason = kReasonDegenerateMeasure;
    return out;
  }
  if (partition != nullptr && partition->coverage == PartitionContext::Coverage::kIncomplete) {
    out.state = VisibilityState::kUnproven;
    out.reason = kReasonEscape;
    return out;
  }
  if (kind1 != nullptr && kind1->existence == ExistenceState::kWalkTruncated) {
    out.state = VisibilityState::kUnproven;
    out.reason = kReasonTruncated;
    return out;
  }
  if (kind1 != nullptr && kind1->existence != ExistenceState::kComputed) {
    out.state = VisibilityState::kUnproven;
    out.reason = kReasonKind1Escaped;
    return out;
  }

  // Sweep the stream: classify each sample against the declared measure's support.
  double w_lit = 0.0;
  double w_support = 0.0;
  bool any_mu_positive = false;
  bool all_lit = true;
  bool all_dark = true;  // every in-support sample has A*T == 0
  bool saw_lit = false;
  for (const FiberSample& s : stream.samples) {
    if (!measure.MuPositive(s.u, angular_tol_rad)) {
      continue;  // outside the declared support: not evidence about this object
    }
    any_mu_positive = true;
    const double at = s.area * s.transmission;
    w_support += s.weight;
    if (s.jet_degenerate) {
      out.jets_ok = false;
    }
    if (at > 0.0 && s.valid) {
      w_lit += s.weight;
      saw_lit = true;
      all_dark = false;
    } else {
      all_lit = false;
      if (s.area == 0.0 && s.transmission > 0.0) {
        out.saw_zero_area = true;  // corridor closed
      }
      if (s.transmission == 0.0 && s.area > 0.0) {
        out.saw_zero_transmission = true;  // transmission gate (e.g. TIR at entry/exit)
      }
    }
  }

  if (!any_mu_positive) {
    out.state = VisibilityState::kUnproven;
    out.reason = kReasonNoSupportSamples;
    out.lit_fraction = std::nan("");
    return out;
  }
  out.lit_fraction = w_support > 0.0 ? w_lit / w_support : std::nan("");

  const bool exhaustive = stream.evidence == FiberSampleStream::EvidenceForm::kStructural ||
                          stream.evidence == FiberSampleStream::EvidenceForm::kSampledExhaustive;

  if (all_lit && saw_lit && exhaustive) {
    // The certified conjunction keeps its jets leg (the frozen header's and the plan's definition):
    // a degenerate jet means the producer could not certify the local normal Jacobian, so the
    // pointwise statement is refused the lift to the whole object — fail-closed to unproven.
    if (!out.jets_ok) {
      out.state = VisibilityState::kUnproven;
      out.reason = kReasonDegenerateJets;
      return out;
    }
    // A per-point statement under declared coverage; partition kUnknown does not block it.
    out.state = VisibilityState::kCertified;
    return out;
  }
  if (saw_lit && !all_lit) {
    out.state = VisibilityState::kPartial;
    return out;
  }
  if (all_dark && exhaustive) {
    // C09's precise form: a computed, NON-EMPTY kind-1 contour exists (the geometric object is
    // present), the declared measure is positive SOMEWHERE ON THE CURVE (the frozen header's
    // second leg — a contour outside the ensemble's declared orientations is not evidence the
    // object is reachable), the stream's points sit in the declared support, and no candidate
    // passes.
    if (kind1 != nullptr && !kind1->u.empty()) {
      bool curve_mu_positive = false;
      for (size_t i = 0; i + 2 < kind1->u.size(); i += 3) {
        if (measure.MuPositive(&kind1->u[i], angular_tol_rad)) {
          curve_mu_positive = true;
          break;
        }
      }
      if (curve_mu_positive) {
        out.state = VisibilityState::kUnlit;
        return out;
      }
      // The contour lies entirely in the declared measure's zero set: the dark in-support samples
      // say nothing about it — not unlit (the spec's measure leg fails), and nothing is lit.
      out.state = VisibilityState::kUnproven;
      out.reason = kReasonKind1MeasureZero;
      return out;
    }
    // All dark without the contour: not unlit (nothing vouches the object's presence), and not
    // certified/partial (nothing is lit) — unproven, with the reason naming which leg is missing.
    out.state = VisibilityState::kUnproven;
    out.reason = kReasonNoKind1;
    return out;
  }
  out.state = VisibilityState::kUnproven;
  out.reason = kReasonPartialEvidence;
  return out;
}

}  // namespace lumice::raypath
