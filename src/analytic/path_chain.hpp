#ifndef LUMICE_ANALYTIC_PATH_CHAIN_HPP_
#define LUMICE_ANALYTIC_PATH_CHAIN_HPP_

// The ray chain behind the single-path evaluator: entry refraction, internal reflections, exit
// refraction, with LI's margin expressions and gate order (LI optics.py _path_domain /
// refract_smooth / reflect_internal). It is the one implementation of "a point on a path":
// EvaluatePath (path_evaluation.cpp) runs it in double and asks for the per-segment detail; the
// fiber adapter (path_fiber.hpp) runs it in double for the domain and event classification, and in
// Jet<3> for the direction's derivative. Internal header: path_evaluation.hpp keeps the kernel's
// public two-stage interface, and nothing here is part of the C ABI.

#include <cmath>
#include <type_traits>

#include "analytic/jet.hpp"
#include "analytic/path_evaluation.hpp"
#include "core/shared/optics_shared.h"

namespace lumice::analytic {

// Which validity gate failed first, in LI's gate order — the event kind the continuation reports
// (LI TerminationReason values of the same names).
enum class ChainFailure {
  kNone,
  kPathInfeasible,  // an incidence cosine <= 0: the ray does not enter / reach / leave the face
  kTirBoundary,     // an entry or exit Snell discriminant <= 0
  kNonFinite,       // a margin is not finite
};

// Validity margins and first failure, as LI's _path_domain reports them. `margins` must hold
// slot_count + 2 doubles; it is filled in LI's validity_margin_names order (entry incidence cosine,
// entry Snell discriminant, each internal face's incidence cosine, exit incidence cosine, exit Snell
// discriminant) up to and including the gate that failed, and `margin_count` says how many.
// `failure_margin` is the failing margin (NaN for kNonFinite), as LI's event_margin.
struct ChainDomain {
  double* margins = nullptr;
  int margin_count = 0;
  ChainFailure failure = ChainFailure::kNone;
  double failure_margin = 0.0;
};

namespace chain_detail {

template <class S, class T>
S Dot3(const S a[3], const T b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

// v_W = R v_B with R row-major.
template <class S, class T>
void BodyToWorld(const S r[9], const T v[3], S out[3]) {
  for (int i = 0; i < 3; i++) {
    out[i] = r[i * 3 + 0] * v[0] + r[i * 3 + 1] * v[1] + r[i * 3 + 2] * v[2];
  }
}

// v_B = R^T v_W.
inline void WorldToBody(const double r[9], const double v[3], double out[3]) {
  for (int i = 0; i < 3; i++) {
    out[i] = r[0 * 3 + i] * v[0] + r[1 * 3 + i] * v[1] + r[2 * 3 + i] * v[2];
  }
}

// Unpolarised reflectance of an interface in HitSurface's variables: `cos_i` the incidence cosine
// (> 0), `rr` the relative index along the ray (n_incident / n_transmitted) and `discriminant` =
// 1 - rr^2 (1 - cos_i^2) = cos_t^2. GetReflectRatio's `delta` is discriminant / cos_i^2
// (optics.cpp HitSurface), clamped at 0 as HitSurface clamps it, which makes a total reflection
// R = 1 exactly.
inline double Reflectance(double cos_i, double rr, double discriminant) {
  const double delta = discriminant / (cos_i * cos_i);
  return lm_optics::GetReflectRatioT<double>(delta > 0.0 ? delta : 0.0, rr);
}

inline void Record(ChainDomain* domain, double value) {
  if (domain != nullptr) {
    domain->margins[domain->margin_count++] = value;
  }
}

inline bool Fail(ChainDomain* domain, ChainFailure failure, double margin) {
  if (domain != nullptr) {
    domain->failure = failure;
    domain->failure_margin = margin;
  }
  return false;
}

}  // namespace chain_detail

// Traces `slots` through the crystal at `pose`. Returns `valid` (every validity margin > 0); on
// true `outgoing` holds the world-frame outgoing direction. Stops at the first failed gate; on false
// `outgoing` is not set. `detail` (double only) receives the body-frame segments, the interface
// transmittances and their product, as PathOutputs documents; `domain` receives the margins and
// the first failure. Either may be null. Gates look at the value part only, so a Jet evaluation
// takes the branch the double one takes.
template <class S>
bool TracePathChain(const FaceNormalTable& table, const int* slots, int slot_count, double refractive_index,
                    const double incident_direction[3], const S pose[9], S outgoing[3], PathOutputs* detail,
                    ChainDomain* domain) {
  using chain_detail::BodyToWorld;
  using chain_detail::Dot3;
  using chain_detail::Fail;
  using chain_detail::Record;
  constexpr bool kDetail = std::is_same_v<S, double>;
  if constexpr (!kDetail) {
    (void)detail;
  }
  const double n = refractive_index;
  const int last = slot_count - 1;
  double fresnel = 1.0;

  // Entry: refraction into the crystal, normal toward the incident medium is the outward normal.
  S normal[3];
  BodyToWorld(pose, table.normal[slots[0]], normal);
  const double entry_rr = 1.0 / n;
  const S entry_cos = -Dot3(normal, incident_direction);
  const S entry_disc = 1.0 - entry_rr * entry_rr * (1.0 - entry_cos * entry_cos);
  Record(domain, ValueOf(entry_cos));
  Record(domain, ValueOf(entry_disc));
  if (!std::isfinite(ValueOf(entry_cos)) || !std::isfinite(ValueOf(entry_disc))) {
    return Fail(domain, ChainFailure::kNonFinite, std::nan(""));
  }
  if (!(ValueOf(entry_cos) > 0.0)) {
    return Fail(domain, ChainFailure::kPathInfeasible, ValueOf(entry_cos));
  }
  if (!(ValueOf(entry_disc) > 0.0)) {
    return Fail(domain, ChainFailure::kTirBoundary, ValueOf(entry_disc));
  }

  S dir[3];
  {
    const S k = entry_rr * entry_cos - Sqrt(entry_disc);
    for (int i = 0; i < 3; i++) {
      dir[i] = entry_rr * incident_direction[i] + k * normal[i];
    }
  }
  if constexpr (kDetail) {
    if (detail != nullptr) {
      chain_detail::WorldToBody(pose, incident_direction, detail->segment_directions);
      chain_detail::WorldToBody(pose, dir, detail->segment_directions + 3);
      detail->interface_transmittances[0] = 1.0 - chain_detail::Reflectance(entry_cos, entry_rr, entry_disc);
      fresnel *= detail->interface_transmittances[0];
    }
  }

  // Internal reflections: the ray must reach each face from inside; TIR or not, it reflects.
  for (int k = 1; k < last; k++) {
    BodyToWorld(pose, table.normal[slots[k]], normal);
    const S cos_i = Dot3(normal, dir);
    // = -(LI's internal TIR discriminant); a diagnostic, not a gate, but a non-finite one fails.
    const double disc = 1.0 - n * n * (1.0 - ValueOf(cos_i) * ValueOf(cos_i));
    Record(domain, ValueOf(cos_i));
    if (!std::isfinite(ValueOf(cos_i)) || !std::isfinite(disc)) {
      return Fail(domain, ChainFailure::kNonFinite, std::nan(""));
    }
    if (!(ValueOf(cos_i) > 0.0)) {
      return Fail(domain, ChainFailure::kPathInfeasible, ValueOf(cos_i));
    }
    for (int i = 0; i < 3; i++) {
      dir[i] -= 2.0 * cos_i * normal[i];
    }
    if constexpr (kDetail) {
      if (detail != nullptr) {
        chain_detail::WorldToBody(pose, dir, detail->segment_directions + 3 * (k + 1));
        detail->interface_transmittances[k] = chain_detail::Reflectance(cos_i, n, disc);
        fresnel *= detail->interface_transmittances[k];
      }
    }
  }

  // Exit: refraction out of the crystal, normal toward the incident (inner) medium is -outward.
  BodyToWorld(pose, table.normal[slots[last]], normal);
  const S exit_cos = Dot3(normal, dir);
  const S exit_disc = 1.0 - n * n * (1.0 - exit_cos * exit_cos);
  Record(domain, ValueOf(exit_cos));
  Record(domain, ValueOf(exit_disc));
  if (!std::isfinite(ValueOf(exit_cos)) || !std::isfinite(ValueOf(exit_disc))) {
    return Fail(domain, ChainFailure::kNonFinite, std::nan(""));
  }
  if (!(ValueOf(exit_cos) > 0.0)) {
    return Fail(domain, ChainFailure::kPathInfeasible, ValueOf(exit_cos));
  }
  if (!(ValueOf(exit_disc) > 0.0)) {
    return Fail(domain, ChainFailure::kTirBoundary, ValueOf(exit_disc));
  }
  {
    const S k = n * exit_cos - Sqrt(exit_disc);
    for (int i = 0; i < 3; i++) {
      outgoing[i] = n * dir[i] - k * normal[i];
    }
  }
  if constexpr (kDetail) {
    if (detail != nullptr) {
      chain_detail::WorldToBody(pose, outgoing, detail->segment_directions + 3 * (last + 1));
      detail->interface_transmittances[last] = 1.0 - chain_detail::Reflectance(exit_cos, n, exit_disc);
      fresnel *= detail->interface_transmittances[last];
      detail->fresnel_transmission = fresnel;
    }
  }
  if (domain != nullptr) {
    domain->failure = ChainFailure::kNone;
  }
  return true;
}

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_PATH_CHAIN_HPP_
