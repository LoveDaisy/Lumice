#include "raypath/representative_ray.hpp"

#include <cmath>
#include <string>
#include <vector>

#include "analytic/entry_measure.hpp"
#include "analytic/path_chain.hpp"
#include "raypath/scene_to_analytic.hpp"

namespace lumice::raypath {

Error AnalyzeRepresentativeRay(const ConfigManager& config, const RepresentativeRayRequest& request,
                               RepresentativeRayResult* out, const PreparedGeometry* prepared) {
  if (!out) {
    return { ErrorCode::kInvalidArgument, "null representative-ray output" };
  }
  *out = RepresentativeRayResult{};  // on error the caller's slot is left empty
  const auto crystal_it = config.crystals_.find(request.crystal_id);
  if (crystal_it == config.crystals_.end()) {
    return { ErrorCode::kUnknownCrystalId,
             "no crystal entry with id " + std::to_string(request.crystal_id) + " in the config" };
  }
  if (!analytic::ValidateRotation(request.pose)) {
    return { ErrorCode::kInvalidArgument, "pose is not a rotation" };
  }
  // The geometry: prepared by a workspace when it already built the selection's tables, else
  // this query's own one build. The single-layer path check reads the same tables.
  PreparedGeometry built;
  const PreparedGeometry* geometry = prepared;
  if (prepared == nullptr) {
    if (Error e = PrepareGeometry(crystal_it->second, &built); !e.Ok()) {
      return e;
    }
    geometry = &built;
  }
  std::vector<int> slots;
  if (Error e = ResolveSingleLayerPath(request.path_layers, geometry->table, &slots); !e.Ok()) {
    return e;
  }

  RepresentativeRayData data;
  RepresentativeRayMetadata& meta = data.meta;
  meta.analytic_api_version = analytic::kApiVersion;
  meta.crystal_id = request.crystal_id;
  const LightSourceConfig& light = config.scene_.light_source_;
  WavelengthChoice choice;
  if (Error e = ResolveWavelength(light, request.wavelength_nm, &choice); !e.Ok()) {
    return e;
  }
  meta.wavelength_nm = choice.wavelength_nm;
  meta.wavelength_source = choice.source;
  meta.refractive_index = choice.refractive_index;
  const CrystalConversion& crystal = geometry->conversion;
  meta.crystal_kind = crystal.kind;
  meta.shape = crystal.scalars;
  meta.shape_is_nominal = crystal.shape_is_nominal;
  meta.faces = request.path_layers.front();
  meta.sun_altitude_deg = light.param_.altitude_;
  meta.sun_azimuth_deg = light.param_.azimuth_;
  SunIncidentDirection(light.param_, meta.incident_direction);
  for (int i = 0; i < 9; i++) {
    meta.pose[i] = request.pose[i];
  }

  const int face_count = static_cast<int>(slots.size());
  const double refractive_index = choice.refractive_index;
  // The incident propagation direction in the body frame: s_body = R^T s (row-major pose).
  double s_body[3];
  for (int i = 0; i < 3; i++) {
    s_body[i] = meta.pose[0 * 3 + i] * meta.incident_direction[0] + meta.pose[1 * 3 + i] * meta.incident_direction[1] +
                meta.pose[2 * 3 + i] * meta.incident_direction[2];
  }

  // The chain: values, world outgoing and the failure's name, from the one implementation.
  std::vector<double> segments(3 * (face_count + 1));
  std::vector<double> transmittances(face_count);
  analytic::PathOutputs outputs{};
  outputs.segment_directions = segments.data();
  outputs.interface_transmittances = transmittances.data();
  double outgoing[3];
  analytic::ChainDomain domain;
  double margins[analytic::kMaxFaceCount + 2];
  domain.margins = margins;
  const bool path_valid = analytic::TracePathChain(geometry->table, slots.data(), face_count, refractive_index,
                                                   meta.incident_direction, meta.pose, outgoing, &outputs, &domain);

  // The corridor: the area gates and the intersection the recovery reads.
  analytic::Corridor corridor(geometry->table, geometry->polygons, slots.data(), face_count);
  analytic::CorridorDiagnostics diagnostics;
  const analytic::EntryMeasure measure = corridor.Evaluate(s_body, refractive_index, &diagnostics);

  // The legal domain is the conjunction: the corridor's gates, the chain's validity, the
  // recovery's containment. Whichever spoke names the refusal; the measured corridor quantities
  // ride along as evidence.
  auto finish = [&](RepresentativeRayData&& assembled) {
    *out = RepresentativeRayResult(FinalizeRepresentativeRayData(std::move(assembled)));
    return Error{};
  };
  auto refusal = [&](const std::string& reason, const std::string& chain_failure) {
    RepresentativeRayRefusal refused;
    refused.reason = reason;
    refused.entry_status = analytic::EntryMeasureStatusName(measure.status);
    refused.chain_failure = chain_failure;
    refused.area_perp_internal = measure.area_perp_internal;
    refused.area_threshold = corridor.Eps();
    refused.geometry_evaluated = diagnostics.geometry_evaluated;
    data.payload = std::move(refused);
    return finish(std::move(data));
  };

  if (measure.status != analytic::EntryMeasureStatus::kOk) {
    // The corridor's own gate order spoke: backface, critical exit, or empty.
    return refusal(analytic::EntryMeasureStatusName(measure.status), "");
  }
  if (!path_valid) {
    return refusal(analytic::ChainFailureName(domain.failure), analytic::ChainFailureName(domain.failure));
  }
  analytic::RepresentativePath contacts;
  const analytic::RecoveryStatus recovery = corridor.RecoverRepresentativePath(diagnostics, &contacts);
  if (recovery != analytic::RecoveryStatus::kOk) {
    return refusal(analytic::RecoveryStatusName(recovery), "");
  }

  RepresentativeRayValues values;
  values.entry_measure = measure.value;
  values.area_perp_internal = measure.area_perp_internal;
  values.total_transmission = outputs.fresnel_transmission;
  values.contacts.resize(face_count);
  for (int k = 0; k < face_count; k++) {
    // Body -> world: the recovered contact sits on the original finite face.
    for (int i = 0; i < 3; i++) {
      values.contacts[k].point[i] = meta.pose[0 * 3 + i] * contacts.contacts[k][0] +
                                    meta.pose[1 * 3 + i] * contacts.contacts[k][1] +
                                    meta.pose[2 * 3 + i] * contacts.contacts[k][2];
    }
    values.contacts[k].transmittance = transmittances[k];
  }
  values.legs.resize(face_count + 1);
  for (int i = 0; i < 3; i++) {
    values.legs[0].direction[i] = meta.incident_direction[i];
  }
  for (int k = 1; k < face_count; k++) {
    // The internal legs, body -> world from the chain's own body-frame segments.
    for (int i = 0; i < 3; i++) {
      values.legs[k].direction[i] = meta.pose[0 * 3 + i] * segments[3 * k + 0] +
                                    meta.pose[1 * 3 + i] * segments[3 * k + 1] +
                                    meta.pose[2 * 3 + i] * segments[3 * k + 2];
    }
  }
  for (int i = 0; i < 3; i++) {
    values.legs[face_count].direction[i] = outgoing[i];
  }
  data.payload = std::move(values);
  return finish(std::move(data));
}

}  // namespace lumice::raypath
