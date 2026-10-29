#ifndef LUMICE_RAYPATH_REPRESENTATIVE_RAY_RESULT_HPP_
#define LUMICE_RAYPATH_REPRESENTATIVE_RAY_RESULT_HPP_

#include <cstdint>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "core/def.hpp"
#include "raypath/domain/input_metadata.hpp"
#include "raypath/domain/result_identity.hpp"

namespace lumice::raypath {

// ---- metadata: the request's identity, as applied ------------------------------------------------

// One concrete member (crystal + face sequence), one explicit pose, one wavelength: the identity
// the path is bound to (never a class-total path). Every conversion the query applied is echoed.
struct RepresentativeRayMetadata {
  int analytic_api_version = 0;
  IdType crystal_id = 0;
  std::string crystal_kind;  // "prism" or "pyramid"
  std::vector<NominalShapeScalar> shape;
  bool shape_is_nominal = false;
  std::vector<int> faces;
  double pose[9]{};  // the request's own input, row-major body -> world
  double sun_altitude_deg = 0.0;
  double sun_azimuth_deg = 0.0;
  double incident_direction[3]{};  // world, propagation sun -> crystal
  double wavelength_nm = 0.0;
  WavelengthSource wavelength_source = WavelengthSource::kDefault;
  double refractive_index = 0.0;
};

// One collision point: the world-frame point on its original finite face and the Fresnel factor
// of the interface it sits on (T at entry and exit, R at internal faces, unpolarised s/p average).
// One row type, so the points and their per-interface quantities cannot disagree.
struct RayContact {
  double point[3]{};
  double transmittance = 0.0;
};

// One leg's world-frame propagation direction: row k runs from contact k to contact k + 1
// (k in [0, face_count - 1]); row 0 is the incident direction, row face_count the outgoing one.
struct RayLeg {
  double direction[3]{};
};

// The produced path. World frame throughout (the pose rides in the metadata, so nothing here
// needs recomputing to draw). Carries physical quantities only — points, unit directions,
// transmittances; display lengths are a consumer's convention.
struct RepresentativeRayValues {
  double entry_measure = 0.0;        // A_P at this pose, the corridor's own value
  double area_perp_internal = 0.0;   // the raw corridor footprint the gate judged
  double total_transmission = 0.0;   // the product of the interface transmittances
  std::vector<RayContact> contacts;  // face_count rows: entry, internal..., exit
  std::vector<RayLeg> legs;          // face_count + 1 rows: incident, after each interface, outgoing
};

// The named refusal, with the measured quantities as its evidence — trajectory numbers never
// travel in it. The vocabulary is the corridor's and the chain's own (`entry_backface`,
// `exit_critical_angle`, `corridor_empty`, `degenerate_corridor`, and the chain's
// `path_infeasible` / `tir_boundary` / `non_finite`).
struct RepresentativeRayRefusal {
  std::string reason;
  std::string entry_status;         // the corridor gate's status name, always spelled
  std::string chain_failure;        // the chain's failure name when the chain spoke; empty otherwise
  double area_perp_internal = 0.0;  // the measured corridor area when geometry was evaluated
  double area_threshold = 0.0;      // the corridor's own area gate
  double entry_measure = 0.0;       // 0: no path, no area claim
  bool geometry_evaluated = false;  // whether the corridor cut an intersection at all
};

struct RepresentativeRayData;
RepresentativeRayData FinalizeRepresentativeRayData(RepresentativeRayData data);

struct RepresentativeRayData {
  RepresentativeRayMetadata meta;
  std::variant<RepresentativeRayValues, RepresentativeRayRefusal> payload;
  bool Produced() const { return std::holds_alternative<RepresentativeRayValues>(payload); }
  const RepresentativeRayValues& Path() const { return std::get<RepresentativeRayValues>(payload); }
  const RepresentativeRayRefusal& Refusal() const { return std::get<RepresentativeRayRefusal>(payload); }

  bool Completed() const { return completion_ == CompletionState::kCompleted; }

 private:
  friend RepresentativeRayData FinalizeRepresentativeRayData(RepresentativeRayData data);
  CompletionState completion_ = CompletionState::kNotCompleted;
};

inline RepresentativeRayData FinalizeRepresentativeRayData(RepresentativeRayData data) {
  const size_t face_count = data.meta.faces.size();
  if (face_count < 2) {
    throw std::invalid_argument("representative ray: the face sequence must be at least two faces");
  }
  if (const auto* path = std::get_if<RepresentativeRayValues>(&data.payload)) {
    if (path->contacts.size() != face_count) {
      throw std::invalid_argument("a representative path's contacts disagree with its own face count");
    }
    if (path->legs.size() != face_count + 1) {
      throw std::invalid_argument("a representative path's legs disagree with its own face count");
    }
    // The total is the product of the contact rows, in interface order — the chain's own running
    // product, recomputed identically.
    double product = 1.0;
    for (const RayContact& contact : path->contacts) {
      product *= contact.transmittance;
    }
    if (product != path->total_transmission) {
      throw std::invalid_argument("a representative path's total transmission disagrees with its own interfaces");
    }
  } else if (const auto* refusal = std::get_if<RepresentativeRayRefusal>(&data.payload)) {
    if (refusal->reason.empty()) {
      throw std::invalid_argument("a representative-ray refusal must name its cause");
    }
    if (refusal->entry_measure != 0.0) {
      throw std::invalid_argument("a representative-ray refusal carries no area claim");
    }
  } else {
    throw std::invalid_argument("representative ray: neither a path nor a named refusal");
  }
  data.completion_ = CompletionState::kCompleted;
  return data;
}

// A completed product owns one immutable value. Copies preserve identity and storage;
// construction from revised assembly data creates a different snapshot.
class RepresentativeRayResult : public FrozenValue<RepresentativeRayData> {
 public:
  // The output slot an entry writes into: constructible, not publishable.
  RepresentativeRayResult() : FrozenValue<RepresentativeRayData>(RepresentativeRayData{}) {}
  explicit RepresentativeRayResult(RepresentativeRayData data)
      : FrozenValue<RepresentativeRayData>(Validated(std::move(data))) {}

 private:
  static RepresentativeRayData Validated(RepresentativeRayData data) {
    if (!data.Completed()) {
      throw std::invalid_argument("representative ray: the construction slot was frozen without a completion");
    }
    return data;
  }
};

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_REPRESENTATIVE_RAY_RESULT_HPP_
