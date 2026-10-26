#ifndef LUMICE_RAYPATH_PROBE_RESULT_HPP_
#define LUMICE_RAYPATH_PROBE_RESULT_HPP_

#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/def.hpp"
#include "raypath/domain/claim.hpp"
#include "raypath/domain/input_metadata.hpp"
#include "raypath/domain/outcome.hpp"  // ExecutionOutcome — the outcome axis's vocabulary
#include "raypath/domain/result_identity.hpp"
#include "raypath/results/probe_line.hpp"
#include "raypath/results/quantity.hpp"
#include "raypath/survey_context.hpp"
namespace lumice::raypath {

enum class ProbeAlong {
  kRadial,      // the full delta axis at the user's azimuth about the sun
  kEdgeNormal,  // the same delta-axis geometry, windowed at the context's nearest edge
  kArc,         // a constant-delta arc through the user's point
  kTangential,  // the arc geometry, spelled as the transverse crossing
};
const char* ProbeAlongName(ProbeAlong along);

enum class ProbeProfileSource { kWarmReused, kRecomputed };
enum class ProbeDarkKind { kInsideSupportPositive, kInsideSupportZero, kOutsideSupport, kUnknown };
inline const char* ProbeDarkKindName(ProbeDarkKind kind) {
  switch (kind) {
    case ProbeDarkKind::kInsideSupportPositive:
      return "inside_support_positive";
    case ProbeDarkKind::kInsideSupportZero:
      return "inside_support_zero";
    case ProbeDarkKind::kOutsideSupport:
      return "outside_support";
    case ProbeDarkKind::kUnknown:
      return "unknown";
  }
  return "unknown";
}

// ---- metadata: every conversion, as applied ------------------------------------------------------

struct ProbeWavelengthEntry {
  double nm = 0.0;
  WavelengthSource source = WavelengthSource::kDefault;
  double refractive_index = 0.0;
};

struct ProbeMetadata {
  int analytic_api_version = 0;
  IdType crystal_id = 0;
  std::string crystal_kind;
  std::vector<NominalShapeScalar> shape;
  bool shape_is_nominal = false;
  double upper_wedge_deg = 0.0;  // pyramid only
  double lower_wedge_deg = 0.0;
  std::vector<int> faces;
  double sun_altitude_deg = 0.0;
  double sun_azimuth_deg = 0.0;
  double incident_direction[3]{};  // world, propagation sun -> crystal
  double at_altitude_deg = 0.0;
  double at_azimuth_deg = 0.0;
  double at_direction[3]{};               // world, light-travel direction arriving from the point
  double at_deviation_deg = 0.0;          // the point's deviation from the sun
  double at_azimuth_about_sun_deg = 0.0;  // its azimuth in the probe frame (the axis forms' line azimuth)
  std::vector<ProbeWavelengthEntry> wavelengths;
  int budget_ms = 0;
  bool interrupted = false;
  // The execution outcome axis (internal typed read; the wire keeps its interrupted convention).
  ExecutionOutcome execution = ExecutionOutcome::kCompleted;
};

// ---- profiles ------------------------------------------------------------------------------------

// Consecutive bin runs classified against support certified for this profile's wavelength.
struct ProbeDarkSegment {
  ProbeDarkKind kind = ProbeDarkKind::kUnknown;
  double lo_deg = 0.0;  // along the line's sweep coordinate (deviation for the axis forms,
  double hi_deg = 0.0;  // azimuth for the arc forms), the run's first/last bin bounds
  int bin_count = 0;
  std::string reason;  // unknown only: the per-wavelength support refusal
};

struct ProbeProfileReadings {
  bool has_edge = false;
  double edge_deg = 0.0;  // the first non-empty bin's centre (finite to half a bin)
  bool has_peak = false;
  double peak_deg = 0.0;  // the value argmax — a different thing from the edge (the corpus lesson)
  double peak_value = 0.0;
  double separation_deg = 0.0;  // peak - edge, signed
};

struct ProbeFiniteReading {
  ProbeProfileSource source = ProbeProfileSource::kRecomputed;
  int members_evaluated = 0;
  std::vector<survey::ProbeLineSample> samples;
  ProbeProfileReadings readings;
  std::vector<ProbeDarkSegment> dark_regions;
};

// One line's profile: the spec echoed, the reading's provenance named, the samples, the two
// readings, and the dark-region decomposition. A refused profile carries the spec shell the
// request already fixes and an evidence state of refused with the named mode — never silence.
struct ProbeProfile {
  QuantitySpec quantity;
  ProbeAlong along = ProbeAlong::kRadial;
  // The line's spec — every number a reader needs to reproduce the line (the caliber discipline:
  // schema, echoed, never a knob). The extent is the bins' actual span: the axis forms' window
  // ends at the last bin's hi edge, and the arc forms carry an extent of wedge_half_width_deg
  // total width, centred on the constant delta (delta ± half of it) — the constant deviation
  // itself lives in each sample's delta_deg.
  double delta_min_deg = 0.0;          // the line's delta extent; the arc forms carry the wedge-wide
  double delta_max_deg = 0.0;          // extent, not the bare constant (see above)
  double azimuth_deg = 0.0;            // the line's azimuth (axis forms) or centre azimuth (arc forms)
  double window_half_width_deg = 0.0;  // the axis forms' delta-window half width (0 = the full axis)
                                       // or the arc forms' azimuth-sweep half width
  double bin_deg = 0.0;
  double wedge_half_width_deg = 0.0;
  std::optional<ContextEdgeBasis> anchor_basis;
  double anchor_deg = 0.0;
  double anchor_wavelength_nm = 0.0;
  // The radial form is azimuth-free under uniform density; other densities require a matching
  // producer line for warm reuse. The other forms describe their own line geometries.
  std::string semantics;
  double wavelength_nm = 0.0;
  double refractive_index = 0.0;
  int sample_count = 0;
  std::string lattice = "antipodal_fibonacci";
  bool deterministic = true;
  std::vector<int> representative;
  int member_count = 0;
  // The arm is private: a writable reading exists only as the return of BeginFinite, a certified
  // arm only through BeginCertified, and a refusal's mode is set together with its arm by Refuse.
 private:
  lumice::raypath::Claim<ProbeFiniteReading> claim_;

 public:
  // The explicit transitions: a writable reading exists only as the result of BeginFinite, and a
  // refusal's mode is set together with its arm. No accessor can write across arms.
  ProbeFiniteReading& BeginFinite() {
    if (!std::holds_alternative<Finite<ProbeFiniteReading>>(claim_)) {
      claim_ = Finite<ProbeFiniteReading>{};
    }
    return std::get<Finite<ProbeFiniteReading>>(claim_).value;
  }
  Certified<ProbeFiniteReading>& BeginCertified(std::string method) {
    claim_ = Certified<ProbeFiniteReading>{};
    Certified<ProbeFiniteReading>& arm = std::get<Certified<ProbeFiniteReading>>(claim_);
    arm.method = std::move(method);
    return arm;
  }
  void Refuse(std::string reason) { claim_ = Refused{ std::move(reason), {} }; }
  const ProbeFiniteReading& Reading() const { return std::get<Finite<ProbeFiniteReading>>(claim_).value; }
  const std::string& RefusalReason() const { return std::get<Refused>(claim_).reason; }
  const lumice::raypath::Claim<ProbeFiniteReading>& Claim() const { return claim_; }
  // The one scope-binding entry: the arm that holds the declaration is chosen by its kind.
  void BindScope(const ClaimScope& scope) {
    std::visit(
        [&scope](auto& arm) {
          using A = std::decay_t<decltype(arm)>;
          if constexpr (std::is_same_v<A, Refused> || std::is_same_v<A, Unevaluated>) {
            arm.asked_scope = scope;
          } else {
            arm.scope = scope;
          }
        },
        claim_);
  }
  survey::EvidenceState State() const { return ClaimState(claim_); }
  // The claim's declared scope, whichever arm holds it (the asked scope on a non-success arm).
  const ClaimScope& Scope() const {
    if (const auto* finite = std::get_if<Finite<ProbeFiniteReading>>(&claim_)) {
      return finite->scope;
    }
    if (const auto* certified = std::get_if<Certified<ProbeFiniteReading>>(&claim_)) {
      return certified->scope;
    }
    if (const auto* refused = std::get_if<Refused>(&claim_)) {
      return refused->asked_scope;
    }
    return std::get<Unevaluated>(claim_).asked_scope;
  }
};

// ---- result --------------------------------------------------------------------------------------

struct ProbeData;

// The single completion entry for the value layer: marks a finished analysis. The marker is
// private, so a bare ProbeData cannot be published as a completed result.
ProbeData FinalizeProbeData(ProbeData data);

inline constexpr char kRedEdgeWidthRule[] =
    "the 650 nm profile's edge reading minus the reference (the first requested "
    "wavelength) profile's edge reading, both present";

struct ProbeData {
  MemberScope members;
  ProbeMetadata meta;
  // Where the --at point sits in the survey's structure: the point document's context shape, read
  // at the at point (warm document's facts, or the context-only T0-T2 assembly).
  SurveyContextBlock context;
  std::vector<ProbeProfile> profiles;  // one per requested wavelength, in order
  // The red-edge width when a 650 nm profile and the reference (the first requested) profile both
  // carry edge readings: the red edge minus the reference edge — the C09 anchor's quantity. Any
  // other wavelength pairing is the profiles' own readings, unpicked here.
  bool has_red_edge_width = false;
  double red_edge_width_deg = 0.0;

  bool Completed() const { return completion_ == CompletionState::kCompleted; }

 private:
  friend ProbeData FinalizeProbeData(ProbeData data);
  CompletionState completion_ = CompletionState::kNotCompleted;
};

inline ProbeData FinalizeProbeData(ProbeData data) {
  data.completion_ = CompletionState::kCompleted;
  return data;
}


// A completed product owns one immutable value. Copies preserve identity and storage;
// construction from revised assembly data creates a different snapshot.
class ProbeResult : public FrozenValue<ProbeData> {
 public:
  // The output slot an entry writes into: constructible, not publishable (ProbeToJson throws).
  ProbeResult() : FrozenValue<ProbeData>(ProbeData{}) {}
  explicit ProbeResult(ProbeData data) : FrozenValue<ProbeData>(Validated(std::move(data))) {}
  std::optional<MemberRef> Reference(MemberId id) const {
    return id.ordinal < Data().members.members.size() ? std::optional<MemberRef>{ { Identity(), id } } : std::nullopt;
  }
  const std::vector<int>* Resolve(MemberRef ref) const {
    return ref.result == Identity() && ref.member.ordinal < Data().members.members.size() ?
               &Data().members.members[ref.member.ordinal] :
               nullptr;
  }

 private:
  static ProbeData Validated(ProbeData data) {
    if (!data.Completed()) {
      throw std::invalid_argument("probe: the construction slot was frozen without a completion");
    }
    for (const ProbeProfile& profile : data.profiles) {
      const survey::EvidenceState state = profile.State();
      if (state != survey::EvidenceState::kFinite && state != survey::EvidenceState::kCertified) {
        continue;
      }
      // The scope must name THIS profile's wavelength and representative, not merely be bound.
      if (!profile.Scope().quantity || profile.Scope().quantity->wavelengths_nm.empty() ||
          profile.Scope().quantity->wavelengths_nm.front() != profile.wavelength_nm ||
          profile.Scope().members.members != std::vector<std::vector<int>>{ profile.representative }) {
        throw std::invalid_argument("a probe profile's scope disagrees with its own declaration");
      }
    }
    // The red-edge width exists only when the red profile and the reference (first) profile both
    // carry an edge reading; a flag without its two readings is a contradictory value.
    if (data.has_red_edge_width) {
      bool red_finite = false;
      for (const ProbeProfile& profile : data.profiles) {
        if (profile.State() == survey::EvidenceState::kFinite && profile.wavelength_nm == kChromaticRedNm) {
          red_finite = profile.Reading().readings.has_edge;
          break;
        }
      }
      if (data.profiles.size() < 2 || !red_finite || data.profiles.front().State() != survey::EvidenceState::kFinite ||
          !data.profiles.front().Reading().readings.has_edge) {
        throw std::invalid_argument("a red-edge width without its two edge readings is not a value");
      }
    }
    return data;
  }
};


}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_PROBE_RESULT_HPP_
