#ifndef LUMICE_RAYPATH_SURVEY_REUSE_HPP_
#define LUMICE_RAYPATH_SURVEY_REUSE_HPP_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "config/config_manager.hpp"
#include "raypath/domain/error.hpp"
#include "raypath/domain/scope.hpp"
#include "raypath/survey_context.hpp"

namespace lumice::raypath::survey {
class SurveyResult;
struct ProbeLineLayout;
}  // namespace lumice::raypath::survey

namespace lumice::raypath {

// Numeric facts extracted from one survey document. Not an admission token.

// The two document families keep seed sheets distinct from survey indexes.
// An absent survey index remains a named inline-context degradation.
enum class WarmDocumentFamily { kSeedSheet, kSurvey };

struct SurveyWarmSource {
  GeometryScope geometry;
  QuantitySpec quantity;
  // Document family, independent of whether a complete index was present.
  WarmDocumentFamily family = WarmDocumentFamily::kSeedSheet;
  bool index_present = false;
  bool early_stopped = false;  // the identity arm: no delta-axis structure exists to reuse
  bool has_wavelength = false;
  double wavelength_nm = 0.0;
  std::string wavelength_source;
  bool has_sun_altitude = false;
  double sun_altitude_deg = 0.0;
  bool has_sun_azimuth = false;
  double sun_azimuth_deg = 0.0;
  // The support: every support card's delta-axis intervals, with the card's members (the first
  // layer's faces — both harvest arms read that same layer). has_support true exactly when at
  // least one interval was harvested: a support_set with an empty interval list reads as no
  // support (the context answers survey_carries_no_support, never a dereference of nothing).
  struct SupportInterval {
    double lower_deg = 0.0;
    double upper_deg = 0.0;
    std::vector<int> members;
    std::vector<std::vector<int>> source_members;
  };
  bool has_support = false;
  std::vector<SupportInterval> intervals;
  // The caustic edge's certified position (an edge-distance candidate).
  bool has_edge = false;
  double edge_deg = 0.0;
  std::vector<std::vector<int>> edge_members;
  // The canonical probe's radial profile (the band-sum readings).
  struct ProbeSample {
    double delta_deg = 0.0;
    double value = 0.0;
    int k = 0;           // the sample's event count — the dark-region decomposition's classifier
    double k_eff = 0.0;  // the sample's effective count, so a reused profile's rows carry the
                         // warm document's numbers and not a manufactured zero
  };
  bool has_probe = false;
  bool has_probe_azimuth = false;
  double probe_azimuth_deg = 0.0;
  double probe_bin_deg = 0.0;
  int probe_sample_count = 0;  // the warm probe's lattice n — the reuse criterion compares it
                               // (the profile's asked count must be the count the warm values
                               // were computed at, or the values are recomputed)
  std::vector<ProbeSample> probe_samples;
  // The family arm's measure echo, read as the concentration language's numbers (the measure
  // gate's fourth comparison: the caller re-derives its own reading from the query config and
  // compares per dimension; a document without family_routing skips the gate with a declaration).
  struct MeasureDimEcho {
    std::string kind;  // "fixed" | "gauss" | "uniform"
    double center_deg = 0.0;
    double width_deg = 0.0;
  };
  bool has_measure_echo = false;
  MeasureDimEcho measure_zenith;
  MeasureDimEcho measure_azimuth;
  MeasureDimEcho measure_roll;
};


// Decode stages preserve refusal precedence without giving a codec the query.
enum class WarmDecodeStage { kEnvelope, kPath, kMeasure, kPayload };

struct WarmCandidate {
  SurveyWarmSource facts;
  long long crystal_id = 0;
  std::vector<std::vector<int>> path_layers;
  std::optional<ScopeLocation> location;
  std::string config_fingerprint;
  std::vector<double> seeds;
  // A boundary decode failure is carried to the product entry so unknown-crystal
  // validation still precedes warm-document errors for both query faces.
  Error decode_error;
  WarmDecodeStage decode_stage = WarmDecodeStage::kEnvelope;
};

class AdmittedWarmSource {
 public:
  const SurveyWarmSource& Facts() const { return facts_; }
  const std::vector<double>& Seeds() const { return seeds_; }
  const ScopeLocation& Location() const { return location_; }
  MemberKind MembersKind() const { return member_kind_; }

 private:
  AdmittedWarmSource(SurveyWarmSource facts, std::vector<double> seeds)
      : facts_(std::move(facts)), seeds_(std::move(seeds)) {}
  SurveyWarmSource facts_;
  std::vector<double> seeds_;
  ScopeLocation location_;
  MemberKind member_kind_ = MemberKind::kLiteral;
  friend Error ValidateWarmCandidate(const ConfigManager&, const PathSelection&, const WarmCandidate&,
                                     std::optional<AdmittedWarmSource>*);
};

Error ValidateWarmCandidate(const ConfigManager& config, const PathSelection& selection, const WarmCandidate& candidate,
                            std::optional<AdmittedWarmSource>* out);

struct PoseDensityConversion;

struct ProfileReuseScope {
  ObservationScope observation;
  QuantitySpec quantity;
  double azimuth_deg = 0.0;
  double bin_deg = 0.0;
  int sample_count = 0;
  bool radial = false;
};
bool CanReuseSurveyProfile(const AdmittedWarmSource& warm, const ProfileReuseScope& scope,
                           const PoseDensityConversion& density, const survey::ProbeLineLayout& layout);
// The schema-4 value model's harvest, read once off an assembled document (the inline arm's
// producer). The warm reader's JSON arm (DecodeWarmCandidate, single_path_json.hpp) mirrors this
// field-for-field — the dual-arm oracle test pins the two arms together. Shared with the probe
// query, whose inline arm assembles the same context and reads it through the same harvest.
WarmCandidate HarvestSurveyResult(const survey::SurveyResult& doc);

struct SurveyReuseDecision;
class SurveyStructureView {
 public:
  SurveyStructureView() = default;
  static SurveyStructureView Unavailable(const char* reason) { return { nullptr, reason }; }
  explicit operator bool() const { return warm_ != nullptr && reason_ == nullptr; }
  const SurveyWarmSource* Facts() const { return warm_; }
  const char* Reason() const { return reason_; }

 private:
  static SurveyStructureView FromAdmitted(const AdmittedWarmSource& source) { return { &source.Facts(), nullptr }; }
  SurveyStructureView(const SurveyWarmSource* warm, const char* reason) : warm_(warm), reason_(reason) {}
  const SurveyWarmSource* warm_ = nullptr;
  const char* reason_ = nullptr;
  friend SurveyReuseDecision EvaluateSurveyFacts(const AdmittedWarmSource&, double, double, double);
};

struct SurveyReuseDecision {
  SurveyStructureView structure;
  const char* reading_reason = nullptr;
};

SurveyReuseDecision EvaluateSurveyReuse(const AdmittedWarmSource& warm, double wavelength_nm, double sun_altitude_deg,
                                        double sun_azimuth_deg);

// The context's nearest structural anchor, shared by the point and probe queries.
bool NearestStructureAnchor(const SurveyStructureView& structure, double deviation_deg, double* position_deg,
                            ContextEdgeBasis* basis);

// The context's answers, read off a harvest (a warm document's facts or the inline assembly's —
// the same shape either way): the query point's containment and edge distances in the delta-axis
// support, and the band-sum reading under the scope gate's exact-equality comparison of the
// wavelength and the sun. The one context computation both the point query and the probe query
// read; the point's caller passes its meta's values.
void FillSurveyContext(const AdmittedWarmSource& warm, double target_deviation_deg, double wavelength_nm,
                       double sun_altitude_deg, double sun_azimuth_deg, SurveyContextBlock* out);


}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_SURVEY_REUSE_HPP_
