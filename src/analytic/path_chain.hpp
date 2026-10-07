#ifndef LUMICE_ANALYTIC_PATH_CHAIN_HPP_
#define LUMICE_ANALYTIC_PATH_CHAIN_HPP_

// The ray chain behind the single-path evaluator: entry refraction, internal reflections, exit
// refraction, with LI's margin expressions and gate order (LI optics.py _path_domain /
// refract_smooth / reflect_internal). It is the one implementation of "a point on a path":
// EvaluatePath (path_evaluation.cpp) runs it in double and asks for the per-segment detail; the
// fiber adapter (path_fiber.hpp) runs it in double for the domain and event classification, and in
// Jet<3> for the direction's derivative; the u-S^2 field layer (dp_field.hpp) runs it at the
// identity pose in double and in Jet2<4> (three u directions plus the refractive index, hence the
// scalar-typed incident direction and index below) in the kEvaluateAll mode. Internal header:
// path_evaluation.hpp keeps the kernel's public two-stage interface, and nothing here is part of
// the C ABI.

#include <cmath>
#include <type_traits>

#include "analytic/jet.hpp"
#include "analytic/path_evaluation.hpp"
#include "core/shared/optics_shared.h"

namespace lumice::analytic {

// Which evaluation the chain runs. The two modes are LI's two readers of one chain body —
// optics.py's _path_domain (gated, first failure) and trace_path (gate-free smooth branch) — and
// must not become two chain implementations.
enum class ChainEvaluation {
  // Stop at the first failed gate (the default; existing callers are unchanged). The margins are
  // recorded up to and including the failed gate.
  kFirstFailure,
  // Keep evaluating after a failed gate: the smooth branch itself has no gates (reflections take
  // no square root; the exit refraction goes NaN beyond its Snell limit by its own sqrt), so every
  // interface's margins and diagnostics are still produced, the outgoing direction keeps its
  // smooth-branch value (NaN included), and the first failure is recorded with the same order and
  // kinds as kFirstFailure. LI field.py's evaluation layer; its root finders re-check membership
  // themselves.
  kEvaluateAll,
};

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
// entry Snell discriminant, each internal face's incidence cosine, exit incidence cosine, exit
// Snell discriminant) up to and including the gate that failed in kFirstFailure mode, or in full
// (no truncation — the chain does not stop) in kEvaluateAll mode; `margin_count` says how many.
// `failure_margin` is the failing margin (NaN for kNonFinite), as LI's event_margin.
//
// The internal TIR discriminant is not in this vector in either mode (it gates nothing; LI keeps
// it as a diagnostic). Its sign convention is an explicit decision, not an oversight: inside the
// chain it is 1 - n^2 (1 - cos^2) (the module A convention this header froze, the negated LI
// value), and the field layer converts to LI's n^2 (1 - cos^2) - 1 at the single documented
// point that builds the LI domain margin vector (dp_field.hpp). Unifying the two signs belongs to
// a later subtask of its own and must not be done here — module A's reported margins are a frozen
// cross-repository contract.
struct ChainDomain {
  double* margins = nullptr;
  int margin_count = 0;
  ChainFailure failure = ChainFailure::kNone;
  double failure_margin = 0.0;
};

// Optional prefix-local values/derivatives. A failed later gate must not erase
// an already-reached interface's diagnostic. Unreached entries stay unavailable (kEvaluateAll
// reaches every interface). `discriminant` is the chain's own convention: the Snell discriminant
// (positive = transmitting) at entry and exit, the negated LI TIR discriminant at internal faces
// (see ChainDomain's sign decision).
template <class S>
struct ChainInterfaceDiagnostics {
  int reached = 0;
  S incidence[kMaxFaceCount]{};
  S discriminant[kMaxFaceCount]{};
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

// One gate's verdict. `condition_failed` is the gate's failure condition. In kFirstFailure every
// failure stops the chain (true); in kEvaluateAll the first failure is recorded — later ones
// change nothing — and the chain keeps evaluating (false). `first_failure_seen` carries the
// evaluate-mode state between gates.
template <ChainEvaluation kMode>
inline bool GateStops(ChainDomain* domain, bool condition_failed, ChainFailure failure, double margin,
                      bool* first_failure_seen) {
  if (!condition_failed) {
    return false;
  }
  if constexpr (kMode == ChainEvaluation::kFirstFailure) {
    Fail(domain, failure, margin);
    return true;
  } else {
    if (!*first_failure_seen) {
      *first_failure_seen = true;
      Fail(domain, failure, margin);
    }
    return false;
  }
}

}  // namespace chain_detail

// Traces `slots` through the crystal at `pose`. Returns `valid` (every validity margin > 0); on
// true `outgoing` holds the world-frame outgoing direction. In kFirstFailure mode the chain stops
// at the first failed gate and `outgoing` is not set; in kEvaluateAll it always is, NaN included
// (the smooth branch's own boundary). `detail` (double only) receives the body-frame segments,
// the interface transmittances and their product, as PathOutputs documents; `domain` receives the
// margins and the first failure. Either may be null. Gates look at the value part only, so a Jet
// or Jet2 evaluation takes the branch the double one takes. `refractive_index` and
// `incident_direction` are scalar-typed so the field layer can seed the index and the incident
// direction as dual variables (Jet2<4>'s fourth and first three slots).
template <class S, ChainEvaluation kMode = ChainEvaluation::kFirstFailure>
bool TracePathChain(const FaceNormalTable& table, const int* slots, int slot_count, S refractive_index,
                    const S incident_direction[3], const S pose[9], S outgoing[3], PathOutputs* detail,
                    ChainDomain* domain, ChainInterfaceDiagnostics<S>* diagnostics = nullptr) {
  using chain_detail::BodyToWorld;
  using chain_detail::Dot3;
  using chain_detail::GateStops;
  using chain_detail::Record;
  constexpr bool kDetail = std::is_same_v<S, double>;
  if constexpr (!kDetail) {
    (void)detail;
  }
  if (diagnostics) {
    *diagnostics = {};
  }
  const S n = refractive_index;
  const int last = slot_count - 1;
  bool failed = false;  // kEvaluateAll only: has the first failure been recorded
  double fresnel = 1.0;

  // Entry: refraction into the crystal, normal toward the incident medium is the outward normal.
  S normal[3];
  BodyToWorld(pose, table.normal[slots[0]], normal);
  const S entry_rr = 1.0 / n;
  const S entry_cos = -Dot3(normal, incident_direction);
  const S entry_disc = 1.0 - entry_rr * entry_rr * (1.0 - entry_cos * entry_cos);
  if (diagnostics) {
    diagnostics->reached = 1;
    diagnostics->incidence[0] = entry_cos;
    diagnostics->discriminant[0] = entry_disc;
  }
  Record(domain, ValueOf(entry_cos));
  Record(domain, ValueOf(entry_disc));
  if (GateStops<kMode>(domain, !std::isfinite(ValueOf(entry_cos)) || !std::isfinite(ValueOf(entry_disc)),
                       ChainFailure::kNonFinite, std::nan(""), &failed)) {
    return false;
  }
  if (GateStops<kMode>(domain, !(ValueOf(entry_cos) > 0.0), ChainFailure::kPathInfeasible, ValueOf(entry_cos),
                       &failed)) {
    return false;
  }
  if (GateStops<kMode>(domain, !(ValueOf(entry_disc) > 0.0), ChainFailure::kTirBoundary, ValueOf(entry_disc),
                       &failed)) {
    return false;
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
    const S disc_jet = 1.0 - n * n * (1.0 - cos_i * cos_i);
    const double disc = ValueOf(disc_jet);
    if (diagnostics) {
      diagnostics->reached = k + 1;
      diagnostics->incidence[k] = cos_i;
      diagnostics->discriminant[k] = disc_jet;
    }
    Record(domain, ValueOf(cos_i));
    if (GateStops<kMode>(domain, !std::isfinite(ValueOf(cos_i)) || !std::isfinite(disc), ChainFailure::kNonFinite,
                         std::nan(""), &failed)) {
      return false;
    }
    if (GateStops<kMode>(domain, !(ValueOf(cos_i) > 0.0), ChainFailure::kPathInfeasible, ValueOf(cos_i), &failed)) {
      return false;
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
  if (diagnostics) {
    diagnostics->reached = slot_count;
    diagnostics->incidence[last] = exit_cos;
    diagnostics->discriminant[last] = exit_disc;
  }
  Record(domain, ValueOf(exit_cos));
  Record(domain, ValueOf(exit_disc));
  if (GateStops<kMode>(domain, !std::isfinite(ValueOf(exit_cos)) || !std::isfinite(ValueOf(exit_disc)),
                       ChainFailure::kNonFinite, std::nan(""), &failed)) {
    return false;
  }
  if (GateStops<kMode>(domain, !(ValueOf(exit_cos) > 0.0), ChainFailure::kPathInfeasible, ValueOf(exit_cos), &failed)) {
    return false;
  }
  if (GateStops<kMode>(domain, !(ValueOf(exit_disc) > 0.0), ChainFailure::kTirBoundary, ValueOf(exit_disc), &failed)) {
    return false;
  }
  {
    // Beyond the exit Snell limit (exit_disc < 0) the root is NaN and so is the outgoing
    // direction — the smooth branch's own boundary, LI trace_path's convention (the field layer's
    // d_p_grazing / d_p_exit_limit exist to read values there).
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
  if (domain != nullptr && !failed) {
    domain->failure = ChainFailure::kNone;
  }
  return !failed;
}

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_PATH_CHAIN_HPP_
