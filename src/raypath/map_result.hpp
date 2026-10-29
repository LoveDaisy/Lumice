#ifndef LUMICE_RAYPATH_MAP_RESULT_HPP_
#define LUMICE_RAYPATH_MAP_RESULT_HPP_

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "core/def.hpp"
#include "raypath/domain/claim.hpp"
#include "raypath/domain/input_metadata.hpp"
#include "raypath/domain/outcome.hpp"  // ExecutionOutcome — the outcome axis's vocabulary
#include "raypath/domain/result_identity.hpp"
#include "raypath/results/probe_line.hpp"  // the probe frame the map's view shares
#include "raypath/results/quantity.hpp"
#include "raypath/results/reference_map.hpp"

namespace lumice::raypath {

// ---- metadata: the request's echo, as applied ----------------------------------------------------

struct MapWavelengthEntry {
  double nm = 0.0;
  WavelengthSource source = WavelengthSource::kDefault;
  double refractive_index = 0.0;
};

struct MapMetadata {
  int analytic_api_version = 0;
  IdType crystal_id = 0;
  std::string crystal_kind;
  std::vector<NominalShapeScalar> shape;
  bool shape_is_nominal = false;
  std::vector<int> faces;
  double sun_altitude_deg = 0.0;
  double sun_azimuth_deg = 0.0;
  double incident_direction[3]{};
  MapWavelengthEntry wavelength;
  int sample_count = 0;
  int budget_ms = 0;
  bool interrupted = false;
  ExecutionOutcome execution = ExecutionOutcome::kCompleted;
};

// ---- the claim payload ---------------------------------------------------------------------------

// One evaluated class reference map: the survey-side payload plus its caliber declaration.
struct MapValues {
  QuantitySpec quantity;
  survey::ReferenceMapData map;
};

struct MapData;
MapData FinalizeMapData(MapData data);

struct MapData {
  MemberScope members;
  MapMetadata meta;
  // The claim: the finite map (with its coverage), or the named refusal — a map-level refusal
  // means the request's measure is not expressible (no silent substitution, never a zero map).
  Claim<MapValues> claim;

  bool Completed() const { return completion_ == CompletionState::kCompleted; }

 private:
  friend MapData FinalizeMapData(MapData data);
  CompletionState completion_ = CompletionState::kNotCompleted;
};

inline MapData FinalizeMapData(MapData data) {
  data.completion_ = CompletionState::kCompleted;
  return data;
}

// A completed product owns one immutable value. Copies preserve identity and storage;
// construction from revised assembly data creates a different snapshot.
class MapResult : public FrozenValue<MapData> {
 public:
  // The output slot an entry writes into: constructible, not publishable.
  MapResult() : FrozenValue<MapData>(MapData{}) {}
  explicit MapResult(MapData data) : FrozenValue<MapData>(Validated(std::move(data))) {}

 private:
  static MapData Validated(MapData data) {
    if (!data.Completed()) {
      throw std::invalid_argument("map: the construction slot was frozen without a completion");
    }
    if (const auto* finite = std::get_if<Finite<MapValues>>(&data.claim)) {
      const survey::ReferenceMapData& map = finite->value.map;
      const size_t cells = static_cast<size_t>(map.view.rows) * static_cast<size_t>(map.view.cols);
      if (map.pixels.size() != cells) {
        throw std::invalid_argument("a map's pixel count disagrees with its own view description");
      }
      // The rank-0 arm exists exactly when the coverage says a rank-0 member ran; the two
      // declarations travel together, whichever way they agree.
      const bool members_all_evaluated = map.members_evaluated == map.member_count;
      if (finite->coverage.members_evaluated != map.members_evaluated) {
        throw std::invalid_argument("a map's coverage disagrees with its own member count");
      }
      if (!members_all_evaluated && !map.truncated && finite->coverage.members_evaluated != map.member_count) {
        throw std::invalid_argument("an untruncated map must have evaluated its whole class");
      }
    }
    return data;
  }
};

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_MAP_RESULT_HPP_
