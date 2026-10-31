#include "gui/analyze_point_view.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <utility>

#include "app.hpp"                  // g_survey_executor — the cancel button reaches the one executor
#include "gui/crystal_preview.hpp"  // CrystalParamHash — the mesh cache's change key
#include "gui/gui_constants.hpp"
#include "gui/gui_logger.hpp"
#include "gui/gui_state.hpp"
#include "gui/semantic_colors.hpp"
#include "gui/theme.hpp"
#include "imgui.h"
#include "include/lumice_editor.h"  // LUMICE_GetCrystalMesh — the crystal's true-scale wireframe
#include "util/sky_direction.hpp"

namespace lumice::gui {
namespace {

std::string PathTextToString(const LUMICE_PathText& text) {
  if (text.data == nullptr || text.length == 0) {
    return std::string();
  }
  return std::string(text.data, text.length);
}

// A trace end's display words: the status verbatim, the reason in parentheses when the engine
// names one (an open arc's ends are boundary events — the reason IS the fact, R5's honesty).
std::string TraceEndText(const LUMICE_PathText& status, const LUMICE_PathText& reason) {
  std::string text = PathTextToString(status);
  if (!text.empty() && reason.data != nullptr && reason.length > 0) {
    text += " (";
    text.append(reason.data, reason.length);
    text += ")";
  }
  return text;
}

// The execution tag's display words (LUMICE_PATH_OUTCOME_*; anything else stays unnamed-honest).
const char* ExecutionText(int execution) {
  switch (execution) {
    case LUMICE_PATH_OUTCOME_COMPLETED:
      return "completed";
    case LUMICE_PATH_OUTCOME_CANCELLED:
      return "cancelled (partial)";
    case LUMICE_PATH_OUTCOME_DEADLINE_EXCEEDED:
      return "deadline exceeded (partial)";
    default:
      return "unknown";
  }
}

constexpr int kMaxRenderComponents = 64;

}  // namespace

// ---- Display state ---------------------------------------------------------------------------------

void ResetPointViewStateForNewAdoption(PointViewState* view) {
  view->component_index = 0;
  view->sample_index = 0;
  view->playing = false;
  view->last_play_step_s = 0.0;
  // The toggles and the camera survive: they are the user's display preferences for THIS
  // session, not per-document state.
}

bool AdvancePointPlayback(PointViewState* view, int valid_count, double now_s) {
  if (!view->playing || valid_count <= 0) {
    return false;
  }
  if (view->sample_index < 0 || view->sample_index >= valid_count) {
    view->sample_index = 0;  // a stale cursor restarts the walk instead of reading out of range
    view->last_play_step_s = now_s;
    return true;
  }
  if (now_s - view->last_play_step_s < kPointPlaybackStepSeconds) {
    return false;
  }
  view->sample_index = (view->sample_index + 1) % valid_count;
  view->last_play_step_s = now_s;
  return true;
}

// ---- Request derivation -----------------------------------------------------------------------------

LUMICE_SinglePathRequest MakePointRequestForKey(const PointIntentKey& key, std::vector<int>* faces,
                                                std::vector<int>* counts) {
  *faces = key.selection.faces;
  *counts = key.selection.layer_face_counts;
  LUMICE_SinglePathRequest request{};
  request.struct_size = sizeof(request);
  request.crystal_id = key.selection.crystal_id;
  request.faces = faces->data();
  request.face_count = static_cast<int>(faces->size());
  request.layer_face_counts = counts->data();
  request.layer_count = static_cast<int>(counts->size());
  // The sky point's labels, through the ONE named conversion (util/sky_direction.hpp): the
  // intent carries the light-travel direction, the engine target fields carry the (altitude,
  // azimuth) labels of the sky point it comes from — the map coordinate input's inverse trip.
  float alt = 0.0f;
  float az = 0.0f;
  DirToAltAz(key.target_dir, &alt, &az);
  request.target_altitude_deg = alt;
  request.target_azimuth_deg = az;
  request.wavelength_nm = key.wavelength_nm;  // 0.0 = the engine's reference resolution
  request.sample_count = kPointQuerySampleCount;
  request.sun_grid_lat_count = kPointSunGridLatCount;
  request.warm_json = nullptr;
  request.warm_json_len = 0;
  request.deadline_ms = kPointQueryBudgetMs;
  request.interrupted = nullptr;  // the executor's cancel token is the one stop channel
  return request;
}

LUMICE_RepresentativeRayRequest MakeRayRequestForSample(const SelectedClassIdentity& identity, const double pose[9],
                                                        double wavelength_nm, std::vector<int>* faces,
                                                        std::vector<int>* counts, std::vector<double>* wavelengths) {
  *faces = identity.faces;
  *counts = identity.layer_face_counts;
  if (wavelength_nm > 0.0) {
    *wavelengths = { wavelength_nm };
  } else {
    wavelengths->clear();
  }
  LUMICE_RepresentativeRayRequest request{};
  request.struct_size = sizeof(request);
  request.crystal_id = identity.core_crystal_id;
  request.faces = faces->data();
  request.face_count = static_cast<int>(faces->size());
  request.layer_face_counts = counts->data();
  request.layer_count = static_cast<int>(counts->size());
  request.pose = pose;  // rebound to the job's own buffer by the executor
  request.wavelengths_nm = wavelengths->empty() ? nullptr : wavelengths->data();
  request.wavelength_count = static_cast<int>(wavelengths->size());
  return request;
}

// ---- Reading the adopted point document -------------------------------------------------------------

int PointComponentRows(const LUMICE_SinglePathResult& result, std::vector<PointComponentRow>* rows) {
  rows->clear();
  LUMICE_PathPointInfo info{};
  info.struct_size = sizeof(info);
  if (LUMICE_PathPointGetInfo(&result, &info) != LUMICE_PATH_READ_OK) {
    return 0;
  }
  if (info.outcome != LUMICE_PATH_POINT_DISCOVERED) {
    return 0;  // the mass arm renders its own block
  }
  const int count = static_cast<int>(std::min<size_t>(info.component_count, kMaxRenderComponents));
  for (int i = 0; i < count; ++i) {
    LUMICE_PathPointComponent component{};
    component.struct_size = sizeof(component);
    if (LUMICE_PathPointGetComponent(&result, static_cast<size_t>(i), &component) != LUMICE_PATH_READ_OK) {
      continue;
    }
    PointComponentRow row;
    row.kind = PathTextToString(component.kind);
    row.forward_text = TraceEndText(component.forward_status, component.forward_reason);
    row.backward_text = TraceEndText(component.backward_status, component.backward_reason);
    row.point_count = static_cast<int>(component.point_count);
    row.integral_available = component.integral_available != 0;
    row.available_points = static_cast<int>(component.available_points);
    row.total_points = static_cast<int>(component.total_points);
    row.fiber_integral = component.fiber_integral;
    rows->push_back(std::move(row));
  }
  return static_cast<int>(rows->size());
}

std::vector<int> CollectValidSampleIndices(const LUMICE_SinglePathResult& result, int component);

// The two point queries above the one map build: the count and the cursor mapping are the
// valid-index map's own size and lookup (ONE authority for the valid->stored walk — the
// renderers and the pump hold the map itself; these wrappers are the tests' and the
// 742.7 handover's per-call shape).
int PointValidSampleCount(const LUMICE_SinglePathResult& result, int component) {
  return static_cast<int>(CollectValidSampleIndices(result, component).size());
}

int PointValidSampleIndexAt(const LUMICE_SinglePathResult& result, int component, int valid_position) {
  if (valid_position < 0) {
    return -1;
  }
  const std::vector<int> indices = CollectValidSampleIndices(result, component);
  return valid_position < static_cast<int>(indices.size()) ? indices[static_cast<size_t>(valid_position)] : -1;
}

std::vector<int> CollectValidSampleIndices(const LUMICE_SinglePathResult& result, int component) {
  std::vector<int> indices;
  LUMICE_PathPointInfo info{};
  info.struct_size = sizeof(info);
  if (LUMICE_PathPointGetInfo(&result, &info) != LUMICE_PATH_READ_OK || info.outcome != LUMICE_PATH_POINT_DISCOVERED ||
      component < 0 || component >= static_cast<int>(info.component_count)) {
    return indices;
  }
  LUMICE_PathPointComponent head{};
  head.struct_size = sizeof(head);
  if (LUMICE_PathPointGetComponent(&result, static_cast<size_t>(component), &head) != LUMICE_PATH_READ_OK) {
    return indices;
  }
  indices.reserve(head.point_count);
  LUMICE_PathPointSample sample{};
  sample.struct_size = sizeof(sample);
  for (size_t i = 0; i < head.point_count; ++i) {
    if (LUMICE_PathPointGetSample(&result, static_cast<size_t>(component), i, &sample) == LUMICE_PATH_READ_OK &&
        sample.valid != 0) {
      indices.push_back(static_cast<int>(i));
    }
  }
  return indices;
}

std::vector<std::string> PointFactorLines(const LUMICE_PathPointSample& sample) {
  std::vector<std::string> lines;
  char buf[192];
  // Orientation: the pose's own angle words, the degeneracy named (R4 — a spin-degenerate pose
  // says so; the angles do not pretend to locate it).
  std::snprintf(buf, sizeof(buf), "pose: zenith %.2f deg, azimuth %.2f deg, roll %.2f deg%s", sample.zenith_deg,
                sample.azimuth_deg, sample.roll_deg, sample.angles_degenerate != 0 ? " (degenerate)" : "");
  lines.emplace_back(buf);
  std::snprintf(buf, sizeof(buf), "rho = %.6g rad^-3", sample.rho);
  lines.emplace_back(buf);
  std::snprintf(buf, sizeof(buf), "A_P = %.6g, A/a^2 = %.6g", sample.entry_measure, sample.entry_measure_a2);
  lines.emplace_back(buf);
  std::snprintf(buf, sizeof(buf), "T (total transmission) = %.6g", sample.total_transmission);
  lines.emplace_back(buf);
  if (sample.jacobian_available != 0) {
    std::snprintf(buf, sizeof(buf), "J (jacobian) = %.6g (sigma %.4g, %.4g)", sample.jacobian,
                  sample.singular_values[0], sample.singular_values[1]);
    lines.emplace_back(buf);
    std::snprintf(buf, sizeof(buf), "K = 1/(J+eps) = %.6g", sample.k_carea);
    lines.emplace_back(buf);
    std::snprintf(buf, sizeof(buf), "rho*A/a^2*T*K = %.6g", sample.factor_product);
    lines.emplace_back(buf);
  } else {
    // The named-unavailable states (R4): no zero stands in for a quantity the engine did not
    // produce, and the product that would use it is unavailable with it.
    lines.emplace_back("J (jacobian): unavailable");
    lines.emplace_back("K = 1/(J+eps): unavailable");
    lines.emplace_back("rho*A/a^2*T*K: unavailable");
  }
  const int interfaces = static_cast<int>(sample.interface_count);
  for (int i = 0; i < interfaces; ++i) {
    std::snprintf(buf, sizeof(buf), "interface %d/%d transmittance = %.6g (conditional on this path)", i + 1,
                  interfaces, sample.interface_transmittances[i]);
    lines.emplace_back(buf);
  }
  return lines;
}

std::vector<std::string> PointCaliberLines(const LUMICE_PathPointInfo& info) {
  std::vector<std::string> lines;
  char buf[256];
  std::snprintf(buf, sizeof(buf), "target: alt %.3f deg, az %.3f deg (deviation %.3f deg)", info.target_altitude_deg,
                info.target_azimuth_deg, info.target_deviation_deg);
  lines.emplace_back(buf);
  std::string faces_text;
  for (size_t i = 0; i < info.face_count; ++i) {
    if (!faces_text.empty()) {
      faces_text += "-";
    }
    faces_text += std::to_string(info.faces[i]);
  }
  std::snprintf(buf, sizeof(buf), "member faces %s, crystal %d, %s", faces_text.c_str(), info.crystal_id,
                PathTextToString(info.crystal_kind).c_str());
  lines.emplace_back(buf);
  const std::string source = PathTextToString(info.wavelength_source);
  std::snprintf(buf, sizeof(buf), "wavelength %.2f nm (%s), n = %.6f", info.wavelength_nm,
                source.empty() ? "source unknown" : source.c_str(), info.refractive_index);
  lines.emplace_back(buf);
  std::snprintf(buf, sizeof(buf), "discovery sample_count %d, budget %d ms, execution: %s", info.sample_count,
                info.budget_ms, ExecutionText(info.execution));
  lines.emplace_back(buf);
  return lines;
}

std::vector<std::string> PointMassLines(const LUMICE_PathPointMass& mass) {
  std::vector<std::string> lines;
  char buf[256];
  std::snprintf(buf, sizeof(buf), "point mass at alt %.3f deg, az %.3f deg (target separation %.3e deg)",
                mass.altitude_deg, mass.azimuth_deg, mass.target_separation_deg);
  lines.emplace_back(buf);
  if (mass.orbit_available != 0) {
    std::snprintf(buf, sizeof(buf), "orbit integral over %s: %.6g (%d/%d samples kept)",
                  PathTextToString(mass.measure_support).c_str(), mass.orbit_integral, mass.orbit_kept_count,
                  mass.orbit_sample_count);
    lines.emplace_back(buf);
  } else {
    std::snprintf(buf, sizeof(buf), "orbit integral: unavailable (%s)", PathTextToString(mass.orbit_reason).c_str());
    lines.emplace_back(buf);
  }
  return lines;
}

// ---- The u-globe -------------------------------------------------------------------------------------

bool MakeSunGlobeProjector(const LUMICE_SinglePathResult& result, int component, SunGlobeProjector* out) {
  const int stored = PointValidSampleIndexAt(result, component, 0);
  if (stored < 0) {
    return false;
  }
  LUMICE_PathPointSample sample{};
  sample.struct_size = sizeof(sample);
  if (LUMICE_PathPointGetSample(&result, static_cast<size_t>(component), static_cast<size_t>(stored), &sample) !=
      LUMICE_PATH_READ_OK) {
    return false;
  }
  // The projector's basis: the centre is the component's first valid u; the reference up picks the
  // world +z when the centre is not parallel to it (the map's own fallback rule), else +x. The
  // centre/up/right triple is orthonormal by construction (Gram-Schmidt, then a re-cross).
  for (int k = 0; k < 3; ++k) {
    out->centre[k] = sample.sun_in_crystal[k];
  }
  static constexpr double kRefZ[3] = { 0.0, 0.0, 1.0 };
  static constexpr double kRefX[3] = { 1.0, 0.0, 0.0 };
  const double* reference = std::abs(out->centre[2]) < 0.999 ? kRefZ : kRefX;
  double up[3];
  const double dot = out->centre[0] * reference[0] + out->centre[1] * reference[1] + out->centre[2] * reference[2];
  for (int k = 0; k < 3; ++k) {
    up[k] = reference[k] - dot * out->centre[k];
  }
  const double up_norm = std::sqrt(up[0] * up[0] + up[1] * up[1] + up[2] * up[2]);
  for (int k = 0; k < 3; ++k) {
    up[k] /= up_norm;
    out->up[k] = up[k];
  }
  out->right[0] = up[1] * out->centre[2] - up[2] * out->centre[1];
  out->right[1] = up[2] * out->centre[0] - up[0] * out->centre[2];
  out->right[2] = up[0] * out->centre[1] - up[1] * out->centre[0];
  return true;
}

void SunGlobeProject(const SunGlobeProjector& projector, const double u[3], double* out_x, double* out_y) {
  // Lambert azimuthal equal-area about the centre, the map's rho = 2 sin(theta/2) family,
  // normalized so the disk's rim is the antipode: rho in [0,1] over theta in [0, pi]. Screen up is
  // the projector's up, screen right the projector's right (the map's own handedness). The
  // antipode's azimuth is undefined (every lateral component vanishes) — atan2's (0,0) = 0 fixes
  // the deterministic rim point, so the projection is a total function of u.
  const double dot = u[0] * projector.centre[0] + u[1] * projector.centre[1] + u[2] * projector.centre[2];
  const double cos_theta = std::max(-1.0, std::min(1.0, dot));
  const double theta = std::acos(cos_theta);
  const double rho = 0.5 * 2.0 * std::sin(theta / 2.0);  // in [0,1]; 1 = the antipode (the rim)
  if (rho < 1e-12) {
    *out_x = 0.5;
    *out_y = 0.5;
    return;
  }
  const double x = u[0] * projector.right[0] + u[1] * projector.right[1] + u[2] * projector.right[2];
  const double y = u[0] * projector.up[0] + u[1] * projector.up[1] + u[2] * projector.up[2];
  const double azimuth = std::atan2(y, x);
  // Normalized disk coordinates: (0.5, 0.5) is the centre, the rim 0.5 from it; y down.
  *out_x = 0.5 + 0.5 * rho * std::cos(azimuth);
  *out_y = 0.5 - 0.5 * rho * std::sin(azimuth);
}

uint32_t SunGridCellColorRgba(double deviation_rad, uint8_t valid) {
  if (valid == 0 || !(deviation_rad >= 0.0) || !(deviation_rad <= 3.15)) {
    return IM_COL32(0, 0, 0, 0);  // invalid (or non-finite) cells paint nothing
  }
  const double t = std::min(1.0, deviation_rad / 3.14159265358979323846);
  // The map's dark-to-light ramp family, re-anchored on angles: dark at delta 0, light at pi.
  const int v = static_cast<int>(38.0 + 200.0 * t);
  return IM_COL32(v, v, std::min(255, v + 12), 235);
}

// ---- The representative-ray display -----------------------------------------------------------------

bool BuildRayTracePolyline(const LUMICE_RepresentativeRay& ray, float extension, RayTracePolyline* out) {
  out->points.clear();
  out->seg_kinds.clear();
  LUMICE_PathRayContacts contacts{};
  contacts.struct_size = sizeof(contacts);
  LUMICE_PathRayLegs legs{};
  legs.struct_size = sizeof(legs);
  if (LUMICE_RepresentativeRayGetContacts(&ray, &contacts) != LUMICE_PATH_READ_OK ||
      LUMICE_RepresentativeRayGetLegs(&ray, &legs) != LUMICE_PATH_READ_OK || contacts.contact_count == 0 ||
      legs.leg_count != contacts.contact_count + 1) {
    return false;
  }
  // The contacts' own spread sets the display scale (the extension is relative to it). The
  // readers hand back doubles; the display polyline is float — one explicit narrowing each.
  float spread = 0.0f;
  for (size_t i = 0; i < contacts.contact_count; ++i) {
    for (size_t j = i + 1; j < contacts.contact_count; ++j) {
      const float dx = static_cast<float>(contacts.contacts[j].point[0] - contacts.contacts[i].point[0]);
      const float dy = static_cast<float>(contacts.contacts[j].point[1] - contacts.contacts[i].point[1]);
      const float dz = static_cast<float>(contacts.contacts[j].point[2] - contacts.contacts[i].point[2]);
      spread = std::max(spread, std::sqrt(dx * dx + dy * dy + dz * dz));
    }
  }
  if (spread <= 0.0f) {
    spread = 1.0f;
  }
  const float ext = std::max(0.1f, extension) * spread;
  auto push_point = [&out](const float p[3]) {
    out->points.push_back(p[0]);
    out->points.push_back(p[1]);
    out->points.push_back(p[2]);
  };
  auto to_float = [](const double v[3]) {
    return std::array<float, 3>{ static_cast<float>(v[0]), static_cast<float>(v[1]), static_cast<float>(v[2]) };
  };
  // Entry leg: from outside along the incident direction (negative travel), to the first contact.
  const std::array<float, 3> c0 = to_float(contacts.contacts[0].point);
  const std::array<float, 3> leg0 = to_float(legs.legs[0].direction);
  const float entry_start[3] = { c0[0] - leg0[0] * ext, c0[1] - leg0[1] * ext, c0[2] - leg0[2] * ext };
  push_point(entry_start);
  push_point(c0.data());
  out->seg_kinds.push_back(0);
  // Internal legs: contact k to contact k+1.
  for (size_t k = 0; k + 1 < contacts.contact_count; ++k) {
    const std::array<float, 3> a = to_float(contacts.contacts[k].point);
    const std::array<float, 3> b = to_float(contacts.contacts[k + 1].point);
    push_point(a.data());
    push_point(b.data());
    out->seg_kinds.push_back(1);
  }
  // Exit leg: last contact onwards along the outgoing direction.
  const std::array<float, 3> cl = to_float(contacts.contacts[contacts.contact_count - 1].point);
  const std::array<float, 3> legl = to_float(legs.legs[legs.leg_count - 1].direction);
  const float exit_end[3] = { cl[0] + legl[0] * ext, cl[1] + legl[1] * ext, cl[2] + legl[2] * ext };
  push_point(cl.data());
  push_point(exit_end);
  out->seg_kinds.push_back(2);
  return true;
}

void RayTracePointToCrystalFrame(const double pose[9], float point[3]) {
  // World -> body: the transpose of the row-major body->world rotation.
  const float x = point[0];
  const float y = point[1];
  const float z = point[2];
  point[0] = static_cast<float>(pose[0] * x + pose[3] * y + pose[6] * z);
  point[1] = static_cast<float>(pose[1] * x + pose[4] * y + pose[7] * z);
  point[2] = static_cast<float>(pose[2] * x + pose[5] * y + pose[8] * z);
}

RayTraceDisplayState RayTraceDisplayStateFor(const RayTraceSession& trace, const RayTraceKey& current_key) {
  if (!trace.intent_valid) {
    return RayTraceDisplayState::kHidden;
  }
  if (trace.cancelled_for_key) {
    return RayTraceDisplayState::kHidden;
  }
  if (trace.adopted_result != nullptr && trace.key == current_key) {
    LUMICE_RepresentativeRayInfo info{};
    info.struct_size = sizeof(info);
    if (LUMICE_RepresentativeRayGetInfo(trace.adopted_result.get(), &info) != LUMICE_PATH_READ_OK) {
      return RayTraceDisplayState::kError;
    }
    return info.produced != 0 ? RayTraceDisplayState::kDrawn : RayTraceDisplayState::kRefused;
  }
  if (!trace.adopted_error.empty() && trace.key == current_key) {
    return RayTraceDisplayState::kError;
  }
  // The current key's answer has not arrived (or an older one is on hand) — the named computing
  // state, never an older pose's polyline (AC4).
  return RayTraceDisplayState::kComputing;
}

// ---- The pumps ---------------------------------------------------------------------------------------

namespace {

// The point face's issuing half (the map pump's shape): build the request for `key` and hand it
// to the executor with the id the pump already assigned. Buffers move into the executor's job.
void IssuePointIntent(SurveyExecutor& executor, const SurveyQueryId& id, const PointIntentKey& key) {
  std::vector<int> faces;
  std::vector<int> counts;
  const LUMICE_SinglePathRequest request = MakePointRequestForKey(key, &faces, &counts);
  executor.IssuePointQuery(id, request, std::move(faces), std::move(counts));
}

}  // namespace

void PumpPointQuery(GuiState& state, SurveyExecutor& executor, double now_s) {
  PointQuerySession& point = state.analysis.point;
  PointViewState& view = state.analysis.point_view;

  // 1. Drain: adopt the finished point iff it still answers the CURRENT intent and the intent
  //    stands (the survey gate's own rule — a cancelled intent accepts no outcome). A refusal is
  //    an answer for a standing intent (a multi-layer selection's named refusal IS the answer).
  //    A completed document's ADOPTION resets the display cursor: the component/sample selection
  //    belonged to the document it read.
  while (std::optional<SurveyExecutor::Finished> finished = executor.TakeFinished(QueryFace::kPoint)) {
    if (point.intent_valid && !point.cancelled_for_key && finished->id == point.intent_id) {
      const bool adoption_changed = finished->ok && finished->point_result != point.adopted_result;
      point.adopted_result = finished->ok ? finished->point_result : nullptr;
      point.adopted_error = finished->ok ? std::string() : std::move(finished->error);
      if (adoption_changed) {
        ResetPointViewStateForNewAdoption(&view);
      }
    }
  }

  // 2. The live selection through the ONE identity chain (a56 — the same derivation the survey
  //    and map pumps consume), plus the sky-point intent and the shared wavelength knob. A point
  //    intent exists only when all three do.
  const std::optional<SelectedClassIdentity> identity = SelectedClassIdentityFor(state);
  if (!identity.has_value() || !state.analysis_result.payload || !state.analysis.sky_point.valid) {
    if (point.intent_valid) {
      // The selection or the sky point died: the intent dies with it, adopted outcome included —
      // the point block's answer must not linger without its question (AC5).
      executor.CancelInFlight(QueryFace::kPoint);
      point = PointQuerySession{};
      ResetPointViewStateForNewAdoption(&view);
    }
    return;
  }
  PointIntentKey key;
  key.selection = SurveySelectionKeyFor(*identity, state.analysis_result.payload->snapshot_generation);
  for (int k = 0; k < 3; ++k) {
    key.target_dir[k] = state.analysis.sky_point.dir[k];
  }
  key.wavelength_nm = state.analysis.map_wavelength_nm;

  // 3. Issue or supersede. A key change DROPS the adopted outcome — a newer sky point's answer
  //    must not linger under the new target (AC5's display-side elimination; unlike the map
  //    there is no honest "older picture anchored on the sky" claim here). The cancel latch
  //    keeps this branch from re-issuing while the same key stands (the button's meaning).
  if (!point.intent_valid || !(point.key == key)) {
    executor.CancelInFlight(QueryFace::kPoint);
    point = PointQuerySession{};
    point.intent_valid = true;
    point.key = key;
    point.intent_id = executor.NextId(key.selection.generation);
    point.intent_start_s = now_s;  // the wait display's epoch — a moved point restarts it (plan §3.1)
    IssuePointIntent(executor, point.intent_id, key);
  }
}

void PumpRayTrace(GuiState& state, SurveyExecutor& executor) {
  RayTraceSession& trace = state.analysis.trace;

  // 1. Drain: adopt the finished ray iff it still answers the CURRENT intent and the intent
  //    stands (the same gate every face runs). A refusal rides INSIDE the handle (Info.produced
  //    0 + the named reason) — an adopted handle either way.
  while (std::optional<SurveyExecutor::Finished> finished = executor.TakeFinished(QueryFace::kRay)) {
    if (trace.intent_valid && !trace.cancelled_for_key && finished->id == trace.intent_id) {
      trace.adopted_result = finished->ok ? finished->ray_result : nullptr;
      trace.adopted_error = finished->ok ? std::string() : std::move(finished->error);
    }
  }

  // 2. The desired key: the CURRENT point adoption + the selected component + the selected
  //    valid sample. No point adoption (or the mass arm, or no valid sample) means no ray
  //    intent — the session is dropped, the 3D view shows its honest state.
  const PointQuerySession& point = state.analysis.point;
  const PointViewState& view = state.analysis.point_view;
  const std::optional<SelectedClassIdentity> identity = SelectedClassIdentityFor(state);
  if (!identity.has_value() || !point.adopted_result) {
    if (trace.intent_valid) {
      executor.CancelInFlight(QueryFace::kRay);
      trace = RayTraceSession{};
    }
    return;
  }
  LUMICE_PathPointInfo info{};
  info.struct_size = sizeof(info);
  if (LUMICE_PathPointGetInfo(point.adopted_result.get(), &info) != LUMICE_PATH_READ_OK ||
      info.outcome != LUMICE_PATH_POINT_DISCOVERED || info.component_count == 0) {
    if (trace.intent_valid) {
      executor.CancelInFlight(QueryFace::kRay);
      trace = RayTraceSession{};
    }
    return;
  }
  const int component = std::clamp(view.component_index, 0, static_cast<int>(info.component_count) - 1);
  // One scan for both the count and the cursor's stored row (the map the renderers share too).
  const std::vector<int> valid_indices = CollectValidSampleIndices(*point.adopted_result, component);
  if (valid_indices.empty()) {
    if (trace.intent_valid) {
      executor.CancelInFlight(QueryFace::kRay);
      trace = RayTraceSession{};
    }
    return;
  }
  const int stored =
      valid_indices[static_cast<size_t>(std::clamp(view.sample_index, 0, static_cast<int>(valid_indices.size()) - 1))];
  LUMICE_PathPointSample sample{};
  sample.struct_size = sizeof(sample);
  if (LUMICE_PathPointGetSample(point.adopted_result.get(), static_cast<size_t>(component), static_cast<size_t>(stored),
                                &sample) != LUMICE_PATH_READ_OK) {
    if (trace.intent_valid) {
      executor.CancelInFlight(QueryFace::kRay);
      trace = RayTraceSession{};
    }
    return;
  }
  RayTraceKey key;
  key.point_id = point.intent_id;
  key.component = component;
  key.sample = stored;

  // 3. Issue or supersede (the ray face's queue-replacement rule — the QueryFace comment).
  if (!trace.intent_valid || !(trace.key == key)) {
    executor.CancelInFlight(QueryFace::kRay);
    trace = RayTraceSession{};
    trace.intent_valid = true;
    trace.key = key;
    trace.intent_id = executor.NextId(point.key.selection.generation);
    std::vector<int> faces;
    std::vector<int> counts;
    std::vector<double> wavelengths;
    const LUMICE_RepresentativeRayRequest request = MakeRayRequestForSample(
        *identity, sample.pose, state.analysis.map_wavelength_nm, &faces, &counts, &wavelengths);
    executor.IssueRayQuery(trace.intent_id, request, std::move(faces), std::move(counts));
  }
}

// ---- Renderer-shared helpers -------------------------------------------------------------------------

namespace {

std::optional<LUMICE_PathPointInfo> ReadPointInfo(const LUMICE_SinglePathResult& result) {
  LUMICE_PathPointInfo info{};
  info.struct_size = sizeof(info);
  if (LUMICE_PathPointGetInfo(&result, &info) != LUMICE_PATH_READ_OK) {
    return std::nullopt;
  }
  return info;
}

// The sample the display currently points at: (component, stored index, the sample itself, the
// component's valid count). nullopt outside the discovered arm with a usable selection.
struct CurrentSample {
  int component = 0;
  int stored = 0;
  int valid_count = 0;
  LUMICE_PathPointSample sample{};
};

// The sample the display currently points at: (component, stored index, the sample itself, the
// component's valid count). nullopt outside the discovered arm with a usable selection. Takes
// the component's valid-index map (CollectValidSampleIndices) — built once per renderer frame.
std::optional<CurrentSample> CurrentSampleOf(const LUMICE_SinglePathResult& result, const PointViewState& view,
                                             const std::vector<int>& valid_indices) {
  const std::optional<LUMICE_PathPointInfo> info = ReadPointInfo(result);
  if (!info.has_value() || info->outcome != LUMICE_PATH_POINT_DISCOVERED || info->component_count == 0 ||
      valid_indices.empty()) {
    return std::nullopt;
  }
  CurrentSample out;
  out.component = std::clamp(view.component_index, 0, static_cast<int>(info->component_count) - 1);
  out.valid_count = static_cast<int>(valid_indices.size());
  out.stored = valid_indices[static_cast<size_t>(std::clamp(view.sample_index, 0, out.valid_count - 1))];
  out.sample.struct_size = sizeof(out.sample);
  if (LUMICE_PathPointGetSample(&result, static_cast<size_t>(out.component), static_cast<size_t>(out.stored),
                                &out.sample) != LUMICE_PATH_READ_OK) {
    return std::nullopt;
  }
  return out;
}

// The mesh cache: one crystal's wireframe, rebuilt when the pool slot or the config's param hash
// moves. LUMICE_CrystalMesh owns its storage by value (lumice_editor.h: fixed-size caller
// arrays, no destroy) — the by-value cache is the whole ownership story; overwriting sheds the
// old copy with the struct. The mesh is TRUE-SCALE crystal frame — no AABB normalization, no
// axis swap: the trace's contacts are true-scale world/crystal quantities and the two must
// overlay (AC4).
struct CachedCrystalMesh {
  int pool = -1;
  int hash = 0;
  bool valid = false;
  LUMICE_CrystalMesh mesh{};
};

CachedCrystalMesh& PointMeshCache() {
  static CachedCrystalMesh cache;
  return cache;
}

// The crystal param in the analysis geometry's OWN caliber (scene_to_analytic's Nominal rule):
// every shape distribution flattened to its centre — the mesh on show is the mesh the queries ran
// against, not a fresh sample of the document's randomized shape.
LUMICE_Distribution NominalDistribution(const ShapeDist& d) {
  return LUMICE_Distribution{ static_cast<int>(d.type), d.center, 0.0f };
}

bool BuildPointCrystalMesh(const CrystalConfig& cr, LUMICE_CrystalMesh* out) {
  LUMICE_CrystalParam param{};
  param.type = (cr.type == CrystalType::kPrism) ? 0 : 1;
  if (cr.type == CrystalType::kPrism) {
    param.height = NominalDistribution(cr.height);
  } else {
    param.prism_h = NominalDistribution(cr.prism_h);
    param.upper_h = NominalDistribution(cr.upper_h);
    param.lower_h = NominalDistribution(cr.lower_h);
    param.upper_wedge_angle = cr.upper_alpha;
    param.lower_wedge_angle = cr.lower_alpha;
  }
  for (int i = 0; i < 6; i++) {
    param.face_distance[i] = NominalDistribution(cr.face_distance[i]);
  }
  FillSyncGroupArray(cr, param.sync_group);
  *out = {};
  return LUMICE_GetCrystalMesh(&param, /*sample_seed=*/0, out) == LUMICE_OK;
}

void RotateBodyToWorld(const double pose[9], const float v[3], float out[3]) {
  out[0] = static_cast<float>(pose[0] * v[0] + pose[1] * v[1] + pose[2] * v[2]);
  out[1] = static_cast<float>(pose[3] * v[0] + pose[4] * v[1] + pose[5] * v[2]);
  out[2] = static_cast<float>(pose[6] * v[0] + pose[7] * v[1] + pose[8] * v[2]);
}

ImU32 SegmentColor(int kind, float alpha) {
  switch (kind) {
    case 0:
      return IM_COL32(90, 160, 255, static_cast<int>(255 * alpha));  // entry: blue
    case 1:
      return IM_COL32(255, 165, 60, static_cast<int>(255 * alpha));  // internal: orange
    default:
      return IM_COL32(90, 220, 120, static_cast<int>(255 * alpha));  // exit: green
  }
}

}  // namespace

// The 3D view's orthographic orbit basis in the display frame (declared in the hpp for the
// gui_test pin to consume the same basis the renderer draws with; the convention is doc'd in
// doc/geometry/coordinate-convention.md §9.8).
void OrbitBasis(float yaw_deg, float pitch_deg, double fwd[3], double right[3], double up[3]) {
  const double yaw = static_cast<double>(yaw_deg) * (3.14159265358979323846 / 180.0);
  const double pitch = std::clamp(static_cast<double>(pitch_deg), -88.0, 88.0) * (3.14159265358979323846 / 180.0);
  // z-up spherical coordinates about the scene's display convention (the crystal's c-axis, +z,
  // is up on screen): yaw sweeps the horizontal plane, pitch rises from it. `fwd` is the view
  // direction, from the viewer INTO the scene — the eye and the visible near face are on the
  // −fwd side (with up = right x fwd below, the basis satisfies right x up = −fwd).
  fwd[0] = std::cos(pitch) * std::cos(yaw);
  fwd[1] = std::cos(pitch) * std::sin(yaw);
  fwd[2] = std::sin(pitch);
  // right = fwd x worldUp, up = right x fwd. With |pitch| <= 88° the clamp keeps cos(pitch)
  // >= 0.035, so `right` normalizes to exactly (sin yaw, −cos yaw, 0) — a function of the yaw
  // alone, which is what makes a pure pitch drag unable to move anything on screen sideways
  // (the gui_test pin asserts that as an exact invariant). The pitch clamp is also what
  // prevents this cross product's one real singularity (fwd parallel to +z at pitch ±90°); the
  // `n < 1e-12` branch below is an unreachable defense against the double-rounded cosine,
  // kept so the normalizer never divides by zero even if the clamp were ever widened.
  right[0] = fwd[1] * 1.0 - fwd[2] * 0.0;
  right[1] = fwd[2] * 0.0 - fwd[0] * 1.0;
  right[2] = 0.0;
  const double n = std::sqrt(right[0] * right[0] + right[1] * right[1]);
  if (n < 1e-12) {
    right[0] = 1.0;
    right[1] = 0.0;
  } else {
    right[0] /= n;
    right[1] /= n;
  }
  up[0] = right[1] * fwd[2] - right[2] * fwd[1];
  up[1] = right[2] * fwd[0] - right[0] * fwd[2];
  up[2] = right[0] * fwd[1] - right[1] * fwd[0];
}

const CrystalConfig* FrozenCrystalForPool(const GuiState& state, int pool) {
  if (pool < 0 || pool >= static_cast<int>(state.analysis.frozen_crystals.size())) {
    return nullptr;
  }
  return &state.analysis.frozen_crystals[static_cast<size_t>(pool)];
}

// ---- The sun-direction globe -------------------------------------------------------------------------

void RenderAnalyzeSunGlobeRegion(GuiState& state, const ImVec2& size) {
  ImGui::BeginChild("##AnalyzeSunGlobe", size, ImGuiChildFlags_Borders);
  PointQuerySession& point = state.analysis.point;
  PointViewState& view = state.analysis.point_view;

  ImGui::TextUnformatted("Sun direction globe");
  ImGui::SameLine();
  ImGui::Checkbox("Deviation grid##globe", &view.show_deviation_grid);
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
  ImGui::TextWrapped("Crystal-frame sun directions — a pose projection, not the full SO(3).");
  ImGui::PopStyleColor();

  const PointQueryState st = PointQueryStateFor(point);
  if (st == PointQueryState::kNoIntent) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    if (!state.analysis.sky_point.valid) {
      ImGui::TextWrapped(
          "Pick a sky point on the raypath view (or type coordinates there); the fiber of poses "
          "that light it takes this region.");
    } else {
      ImGui::TextWrapped("The fiber takes this region once a class is selected.");
    }
    ImGui::PopStyleColor();
    ImGui::EndChild();
    return;
  }
  if (st == PointQueryState::kWaiting || st == PointQueryState::kCancelled || st == PointQueryState::kRefused) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    switch (st) {
      case PointQueryState::kWaiting: {
        const std::string wait_line =
            PointWaitLineFor(point, ImGui::GetTime(), g_survey_executor.InflightPollSnapshot());
        state.analysis.wait_feedback_texts.point_wait_line_fiber = wait_line;
        ImGui::TextWrapped("%s", wait_line.c_str());
        break;
      }
      case PointQueryState::kCancelled:
        ImGui::TextWrapped(
            "Point cancelled for this selection. Moving the sky point — or re-picking the class — "
            "asks anew.");
        break;
      default:
        ImGui::TextWrapped("Point refused: %s", point.adopted_error.c_str());
        break;
    }
    ImGui::PopStyleColor();
    ImGui::EndChild();
    return;
  }

  // Completed. Either arm reads through the document's own Info.
  const std::optional<LUMICE_PathPointInfo> info = ReadPointInfo(*point.adopted_result);
  if (!info.has_value()) {
    ImGui::TextDisabled("The adopted point document did not answer its Info reader.");
    ImGui::EndChild();
    return;
  }
  if (info->outcome == LUMICE_PATH_POINT_MASS) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextWrapped("A rank-0 point mass has no fiber to project — the details carry the mass numbers.");
    ImGui::PopStyleColor();
    ImGui::EndChild();
    return;
  }

  // The partial-document note, wherever the document is drawn: a deadline-stopped point keeps
  // what arrived, and the regions that draw it say so (AC5's 如实呈现, the details banner's
  // siblings).
  if (info->execution != LUMICE_PATH_OUTCOME_COMPLETED) {
    ImGui::PushStyleColor(ImGuiCol_Text, WarningTextColor());
    ImGui::TextWrapped("Partial document (%s) — the fiber on show is what arrived.", ExecutionText(info->execution));
    ImGui::PopStyleColor();
  }
  // The discovered arm: the selected component's fiber, the current sample marker, the weight
  // bar, the slider and playback.
  std::vector<PointComponentRow> rows;
  const int component_total = PointComponentRows(*point.adopted_result, &rows);
  if (component_total == 0) {
    LUMICE_PathPointReach reach{};
    reach.struct_size = sizeof(reach);
    const bool have_reach = LUMICE_PathPointGetReach(point.adopted_result.get(), &reach) == LUMICE_PATH_READ_OK;
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    if (have_reach && !reach.target_in_range) {
      ImGui::TextWrapped(
          "No fiber here: the target's deviation is outside the range of this path's deflection field "
          "(the reach probe excludes it at its resolution).");
    } else {
      ImGui::TextWrapped(
          "No fiber component at this point: the finite crystal passes no pose along the queried "
          "deflection.");
    }
    ImGui::PopStyleColor();
    ImGui::EndChild();
    return;
  }
  if (component_total > 1) {
    // The component selector (AC2: the user picks a component; separate components are never
    // joined into one curve — this selector is the only thing that moves between them).
    int selected = std::clamp(view.component_index, 0, component_total - 1);
    std::string labels;
    for (int i = 0; i < component_total; ++i) {
      char item[64];
      std::snprintf(item, sizeof(item), "component %d: %s, %d pts%s", i + 1, rows[static_cast<size_t>(i)].kind.c_str(),
                    rows[static_cast<size_t>(i)].point_count,
                    rows[static_cast<size_t>(i)].integral_available ? "" : ", integral unavailable");
      labels += item;
      labels += '\0';
    }
    labels += '\0';
    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo("##globe_component", &selected, labels.c_str())) {
      view.component_index = selected;
      view.sample_index = 0;
      view.playing = false;
    }
  }
  const int component = std::clamp(view.component_index, 0, component_total - 1);
  // The frame's one valid-index scan: the fiber, the weight bar and the cursor reads all share it.
  const std::vector<int> valid_indices = CollectValidSampleIndices(*point.adopted_result, component);
  const int valid_count = static_cast<int>(valid_indices.size());
  if (valid_count == 0) {
    ImGui::TextDisabled("This component carries no valid sample — nothing to project.");
    ImGui::EndChild();
    return;
  }
  // The playback tick: advances the index while playing (the pump's pure step, driven here by the
  // frame clock — the only place playback touches state).
  AdvancePointPlayback(&view, valid_count, ImGui::GetTime());

  SunGlobeProjector projector;
  const bool have_projector = MakeSunGlobeProjector(*point.adopted_result, component, &projector);

  // The canvas: a square disk sized to the child's remaining height above the control rows
  // (weight bar + its label + the slider/playback row).
  const float controls_px = UiPx(96.0f);
  const ImVec2 avail = ImGui::GetContentRegionAvail();
  const float side = std::max(UiPx(60.0f), std::min(avail.x, avail.y - controls_px));
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 canvas_min = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton("##globe_canvas", ImVec2(side, side));
  const ImVec2 centre_px(canvas_min.x + side * 0.5f, canvas_min.y + side * 0.5f);
  const float radius_px = side * 0.5f - UiPx(4.0f);
  auto to_screen = [&](double x, double y) {
    return ImVec2(static_cast<float>(centre_px.x + (x - 0.5) * 2.0 * radius_px),
                  static_cast<float>(centre_px.y + (y - 0.5) * 2.0 * radius_px));
  };
  // The disk: rim + interior, dark — the map's own picture convention (the cells carry the data).
  dl->AddCircleFilled(centre_px, radius_px, IM_COL32(16, 18, 22, 255));
  dl->AddCircle(centre_px, radius_px, IM_COL32(70, 74, 82, 200), 0, 1.0f);

  // The deviation backdrop: only when the document carries a grid (AC2's named-absence rule) and
  // the toggle stands.
  LUMICE_PathSunGrid grid{};
  grid.struct_size = sizeof(grid);
  const bool have_grid = view.show_deviation_grid &&
                         LUMICE_PathPointGetSunGrid(point.adopted_result.get(), &grid) == LUMICE_PATH_READ_OK &&
                         grid.cell_count > 0;
  if (have_grid) {
    const double deg_to_rad = 3.14159265358979323846 / 180.0;
    for (int i = 0; i < grid.lat_count; ++i) {
      const double lat = (-90.0 + (i + 0.5) * 180.0 / grid.lat_count) * deg_to_rad;
      for (int j = 0; j < grid.lon_count; ++j) {
        const double lon = (-180.0 + (j + 0.5) * 360.0 / grid.lon_count) * deg_to_rad;
        const size_t cell = static_cast<size_t>(i) * static_cast<size_t>(grid.lon_count) + static_cast<size_t>(j);
        if (cell >= grid.cell_count || grid.valid[cell] == 0) {
          continue;
        }
        const double u[3] = { std::cos(lat) * std::cos(lon), std::cos(lat) * std::sin(lon), std::sin(lat) };
        double gx = 0.0;
        double gy = 0.0;
        SunGlobeProject(projector, u, &gx, &gy);
        const ImVec2 p = to_screen(gx, gy);
        const float half = std::max(1.5f, radius_px / 48.0f);
        dl->AddRectFilled(ImVec2(p.x - half, p.y - half), ImVec2(p.x + half, p.y + half),
                          SunGridCellColorRgba(grid.deviation_rad[cell], grid.valid[cell]));
      }
    }
  }

  // The fiber: the selected component's VALID samples, in arclength order, as contiguous runs —
  // broken where an invalid sample sits (AC2: this component's own valid samples, nothing joined).
  if (have_projector) {
    std::vector<ImVec2> run;
    auto flush_run = [&]() {
      if (run.size() >= 2) {
        dl->AddPolyline(run.data(), static_cast<int>(run.size()), ImGui::ColorConvertFloat4ToU32(AccentColor()), 0,
                        2.0f);
      }
      run.clear();
    };
    LUMICE_PathPointSample probe{};
    probe.struct_size = sizeof(probe);
    for (size_t v = 0; v < valid_indices.size(); ++v) {
      // A gap in stored indices means an invalid sample sat between: the run breaks there.
      if (v > 0 && valid_indices[v] != valid_indices[v - 1] + 1) {
        flush_run();
      }
      if (LUMICE_PathPointGetSample(point.adopted_result.get(), static_cast<size_t>(component),
                                    static_cast<size_t>(valid_indices[v]), &probe) != LUMICE_PATH_READ_OK) {
        flush_run();
        continue;
      }
      double gx = 0.0;
      double gy = 0.0;
      SunGlobeProject(projector, probe.sun_in_crystal, &gx, &gy);
      run.push_back(to_screen(gx, gy));
    }
    flush_run();
  }

  // The current sample marker (inside the disk; outside means a stale projector — the marker
  // simply sits at the rim, still honest about the direction it names).
  if (have_projector) {
    if (const std::optional<CurrentSample> current = CurrentSampleOf(*point.adopted_result, view, valid_indices);
        current.has_value()) {
      double gx = 0.0;
      double gy = 0.0;
      SunGlobeProject(projector, current->sample.sun_in_crystal, &gx, &gy);
      const ImVec2 p = to_screen(gx, gy);
      dl->AddCircleFilled(p, UiPx(4.0f), ImGui::ColorConvertFloat4ToU32(AccentColor()));
      dl->AddCircle(p, UiPx(6.5f), IM_COL32(255, 255, 255, 210), 0, 1.5f);
    }
  }
  if (!have_grid && view.show_deviation_grid) {
    ImGui::TextDisabled("(no sun grid in this document — the backdrop is absent, not zero)");
  }

  // The weight bar: the component's per-sample integrand (rho*A/a^2*T*K — the quantity the
  // component's fiber_integral integrates over arclength), per valid sample.
  const float bar_h = UiPx(10.0f);
  const ImVec2 bar_min(canvas_min.x, canvas_min.y + side + UiPx(4.0f));
  const ImVec2 bar_max(canvas_min.x + side, bar_min.y + bar_h);
  {
    std::vector<double> weights(static_cast<size_t>(valid_count), 0.0);
    double vmax = 0.0;
    LUMICE_PathPointSample probe{};
    probe.struct_size = sizeof(probe);
    for (int v = 0; v < valid_count; ++v) {
      const int stored = valid_indices[static_cast<size_t>(v)];
      if (LUMICE_PathPointGetSample(point.adopted_result.get(), static_cast<size_t>(component),
                                    static_cast<size_t>(stored), &probe) != LUMICE_PATH_READ_OK) {
        continue;
      }
      const double w = probe.factor_product;
      weights[static_cast<size_t>(v)] = w;
      if (w > vmax && std::isfinite(w)) {
        vmax = w;
      }
    }
    dl->AddRectFilled(bar_min, bar_max, IM_COL32(16, 18, 22, 160));
    if (vmax > 0.0) {
      const double step = static_cast<double>(side) / valid_count;
      const int current_pos = std::clamp(view.sample_index, 0, valid_count - 1);
      for (int v = 0; v < valid_count; ++v) {
        const double w = weights[static_cast<size_t>(v)];
        if (!(w > 0.0) || !std::isfinite(w)) {
          continue;
        }
        const float x0 = static_cast<float>(bar_min.x + v * step);
        const float x1 = static_cast<float>(bar_min.x + (v + 1) * step) - 1.0f;
        const float h = static_cast<float>((bar_max.y - bar_min.y) * (w / vmax));
        const bool is_current = v == current_pos;
        dl->AddRectFilled(ImVec2(x0, bar_max.y - h), ImVec2(std::max(x0 + 1.0f, x1), bar_max.y),
                          is_current ? ImGui::ColorConvertFloat4ToU32(AccentColor()) : IM_COL32(120, 126, 138, 190));
      }
    }
    // The bar renders at absolute coordinates in the strip right below the canvas; the cursor
    // has not crossed that strip yet, so cross it first — the label and the slider row each own
    // their line.
    ImGui::Dummy(ImVec2(side, bar_h + UiPx(4.0f)));
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextWrapped("weight per sample: rho*A/a^2*T*K (the integrand of this component's fiber integral)%s",
                       vmax > 0.0 ? "" : " — all zero here");
    ImGui::PopStyleColor();
  }

  // The slider + playback row: the slider indexes the component's VALID samples (AC3); touching
  // it stops the playback (a hand on the slider owns the cursor).
  int pos = std::clamp(view.sample_index, 0, valid_count - 1);
  ImGui::SetNextItemWidth(-UiPx(120.0f));
  if (ImGui::SliderInt("##globe_sample", &pos, 0, valid_count - 1)) {
    view.sample_index = pos;
    view.playing = false;
  }
  ImGui::SameLine();
  if (ImGui::SmallButton(view.playing ? "Stop##globe_play" : "Play##globe_play")) {
    view.playing = !view.playing;
    view.last_play_step_s = ImGui::GetTime();
  }
  if (const std::optional<CurrentSample> current = CurrentSampleOf(*point.adopted_result, view, valid_indices);
      current.has_value()) {
    ImGui::SameLine();
    ImGui::TextDisabled("sample %d/%d — zenith %.1f, az %.1f, roll %.1f%s",
                        std::clamp(view.sample_index, 0, valid_count - 1) + 1, valid_count, current->sample.zenith_deg,
                        current->sample.azimuth_deg, current->sample.roll_deg,
                        current->sample.angles_degenerate != 0 ? " (degenerate)" : "");
  }
  ImGui::EndChild();
}

// ---- The raypath 3D view -----------------------------------------------------------------------------

void RenderAnalyzeRay3DRegion(GuiState& state, const ImVec2& size) {
  ImGui::BeginChild("##AnalyzeRaypath3D", size, ImGuiChildFlags_Borders,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  PointQuerySession& point = state.analysis.point;
  RayTraceSession& trace = state.analysis.trace;
  PointViewState& view = state.analysis.point_view;

  ImGui::TextUnformatted("Raypath 3D");
  ImGui::SameLine();
  int frame = view.crystal_frame ? 1 : 0;
  ImGui::RadioButton("World##r3d", &frame, 0);
  ImGui::SameLine();
  ImGui::RadioButton("Crystal##r3d", &frame, 1);
  view.crystal_frame = frame == 1;
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
  ImGui::TextWrapped(
      "One pose's real path in the crystal: drag orbits, wheel zooms. Same physical state, two "
      "views.");
  ImGui::PopStyleColor();

  const PointQueryState st = PointQueryStateFor(point);
  if (st == PointQueryState::kNoIntent) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextWrapped("Pick a class and a sky point; the current pose's crystal-interior path takes this region.");
    ImGui::PopStyleColor();
    ImGui::EndChild();
    return;
  }
  if (st == PointQueryState::kWaiting || st == PointQueryState::kCancelled || st == PointQueryState::kRefused) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    switch (st) {
      case PointQueryState::kWaiting:
        ImGui::TextWrapped("The point is computing — the path follows it.");
        break;
      case PointQueryState::kCancelled:
        ImGui::TextWrapped("Point cancelled — no path on show.");
        break;
      default:
        ImGui::TextWrapped("Point refused — no path on show.");
        break;
    }
    ImGui::PopStyleColor();
    ImGui::EndChild();
    return;
  }
  const std::optional<LUMICE_PathPointInfo> info = ReadPointInfo(*point.adopted_result);
  if (!info.has_value()) {
    ImGui::TextDisabled("The adopted point document did not answer its Info reader.");
    ImGui::EndChild();
    return;
  }
  if (info->outcome == LUMICE_PATH_POINT_MASS) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextWrapped(
        "A rank-0 point mass has no crystal-interior path — nothing to draw here (the details carry "
        "the mass numbers).");
    ImGui::PopStyleColor();
    ImGui::EndChild();
    return;
  }

  // The partial-document note, the globe's sibling: the path on show is what a stopped run
  // delivered.
  if (info->execution != LUMICE_PATH_OUTCOME_COMPLETED) {
    ImGui::PushStyleColor(ImGuiCol_Text, WarningTextColor());
    ImGui::TextWrapped("Partial document (%s) — the path on show is what arrived.", ExecutionText(info->execution));
    ImGui::PopStyleColor();
  }
  const std::vector<int> valid_indices = CollectValidSampleIndices(
      *point.adopted_result, std::clamp(view.component_index, 0, static_cast<int>(info->component_count) - 1));
  const std::optional<CurrentSample> current = CurrentSampleOf(*point.adopted_result, view, valid_indices);
  if (!current.has_value()) {
    ImGui::TextDisabled("No valid sample on show — nothing to draw.");
    ImGui::EndChild();
    return;
  }
  RayTraceKey current_key;
  current_key.point_id = point.intent_id;
  current_key.component = current->component;
  current_key.sample = current->stored;
  switch (RayTraceDisplayStateFor(trace, current_key)) {
    case RayTraceDisplayState::kHidden:
      ImGui::TextDisabled("No representative path asked for yet.");
      ImGui::EndChild();
      return;
    case RayTraceDisplayState::kComputing:
      ImGui::TextDisabled("Computing the representative path for this sample…");
      ImGui::EndChild();
      return;
    case RayTraceDisplayState::kError:
      ImGui::TextUnformatted("Path query failed:");
      ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
      ImGui::TextWrapped("%s", trace.adopted_error.c_str());
      ImGui::PopStyleColor();
      ImGui::EndChild();
      return;
    case RayTraceDisplayState::kRefused: {
      LUMICE_RepresentativeRayInfo rinfo{};
      rinfo.struct_size = sizeof(rinfo);
      LUMICE_RepresentativeRayGetInfo(trace.adopted_result.get(), &rinfo);
      ImGui::TextUnformatted("No legal path for this pose:");
      ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
      ImGui::TextWrapped("reason: %s | entry status: %s%s%s | corridor area %.3e (threshold %.3e)",
                         PathTextToString(rinfo.reason).c_str(), PathTextToString(rinfo.entry_status).c_str(),
                         rinfo.chain_failure.length > 0 ? " | chain: " : "",
                         rinfo.chain_failure.length > 0 ? PathTextToString(rinfo.chain_failure).c_str() : "",
                         rinfo.area_perp_internal, rinfo.area_threshold);
      ImGui::PopStyleColor();
      ImGui::EndChild();
      return;
    }
    case RayTraceDisplayState::kDrawn:
      break;
  }

  // The crystal's mesh: the FROZEN binding's config (captured at DoAnalyze), in the analysis
  // geometry's nominal caliber, true scale. The pool slot comes from the snapshot's own
  // core->pool map; the mesh source is the frozen copy, so an edit of the live document after
  // Analyze cannot rebuild the mesh into a shape the trace no longer lies inside (AC4).
  const std::map<int, int> core_to_pool = AnalysisCoreToPoolMap(state);
  const auto pool_it = core_to_pool.find(info->crystal_id);
  if (pool_it == core_to_pool.end() || pool_it->second < 0) {
    ImGui::TextDisabled("The analyzed crystal is not in the snapshot's pool map — no mesh to draw it with.");
    ImGui::EndChild();
    return;
  }
  const int pool = pool_it->second;
  const CrystalConfig* frozen = FrozenCrystalForPool(state, pool);
  if (frozen == nullptr) {
    ImGui::TextDisabled("The frozen binding carries no crystal for this pool slot — no mesh to draw it with.");
    ImGui::EndChild();
    return;
  }
  CachedCrystalMesh& cache = PointMeshCache();
  const int hash = CrystalParamHash(*frozen);
  // The key moves even on a failed build: a persistently-unbuildable config is one failed
  // attempt per key change, not one LUMICE_GetCrystalMesh call per frame.
  if (cache.pool != pool || cache.hash != hash) {
    cache.pool = pool;
    cache.hash = hash;
    cache.valid = BuildPointCrystalMesh(*frozen, &cache.mesh);
  }
  if (!cache.valid || cache.mesh.vertex_count <= 0) {
    ImGui::TextDisabled("The crystal mesh came back empty for this configuration.");
    ImGui::EndChild();
    return;
  }
  RayTracePolyline line;
  if (!BuildRayTracePolyline(*trace.adopted_result, /*extension=*/1.1f, &line)) {
    ImGui::TextDisabled("The path's readers answered empty — nothing to draw.");
    ImGui::EndChild();
    return;
  }

  // The frame switch is a display transform (AC4): world view rotates the body-frame mesh to the
  // pose's world; crystal view rotates the world-frame path back. One physical state, two views.
  const double* pose = current->sample.pose;
  auto to_display = [&](const float v[3], float out[3]) {
    if (view.crystal_frame) {
      out[0] = v[0];
      out[1] = v[1];
      out[2] = v[2];
    } else {
      RotateBodyToWorld(pose, v, out);
    }
  };
  auto point_to_display = [&](const float p[3], float out[3]) {
    if (view.crystal_frame) {
      float tmp[3] = { p[0], p[1], p[2] };
      RayTracePointToCrystalFrame(pose, tmp);
      out[0] = tmp[0];
      out[1] = tmp[1];
      out[2] = tmp[2];
    } else {
      out[0] = p[0];
      out[1] = p[1];
      out[2] = p[2];
    }
  };

  // Content bounds over everything on show (mesh + trace), for the fit and the orbit centre.
  float lo[3] = { 0.0f, 0.0f, 0.0f };
  float hi[3] = { 0.0f, 0.0f, 0.0f };
  bool have_bounds = false;
  auto accumulate = [&](const float p[3]) {
    if (!have_bounds) {
      for (int k = 0; k < 3; ++k) {
        lo[k] = hi[k] = p[k];
      }
      have_bounds = true;
      return;
    }
    for (int k = 0; k < 3; ++k) {
      lo[k] = std::min(lo[k], p[k]);
      hi[k] = std::max(hi[k], p[k]);
    }
  };
  for (int v = 0; v < cache.mesh.vertex_count; ++v) {
    float d[3];
    to_display(&cache.mesh.vertices[v * 3], d);
    accumulate(d);
  }
  // The crystal's own centre, in the same display frame: the sun arrow's anchor (the crystal-frame
  // view's u direction radiates from the CRYSTAL, not from a content box the display extensions
  // inflate).
  float mesh_lo[3] = { 0.0f, 0.0f, 0.0f };
  float mesh_hi[3] = { 0.0f, 0.0f, 0.0f };
  for (int k = 0; k < 3; ++k) {
    mesh_lo[k] = mesh_hi[k] = lo[k];
  }
  {
    bool have_mesh_bounds = false;
    for (int v = 0; v < cache.mesh.vertex_count; ++v) {
      float d[3];
      to_display(&cache.mesh.vertices[v * 3], d);
      if (!have_mesh_bounds) {
        for (int k = 0; k < 3; ++k) {
          mesh_lo[k] = mesh_hi[k] = d[k];
        }
        have_mesh_bounds = true;
        continue;
      }
      for (int k = 0; k < 3; ++k) {
        mesh_lo[k] = std::min(mesh_lo[k], d[k]);
        mesh_hi[k] = std::max(mesh_hi[k], d[k]);
      }
    }
  }
  const float mesh_centre[3] = { (mesh_lo[0] + mesh_hi[0]) * 0.5f, (mesh_lo[1] + mesh_hi[1]) * 0.5f,
                                 (mesh_lo[2] + mesh_hi[2]) * 0.5f };
  for (size_t i = 0; i + 2 < line.points.size(); i += 3) {
    const float p[3] = { line.points[i], line.points[i + 1], line.points[i + 2] };
    float d[3];
    point_to_display(p, d);
    accumulate(d);
  }
  if (!have_bounds) {
    ImGui::TextDisabled("Nothing on show.");
    ImGui::EndChild();
    return;
  }
  const float centre[3] = { (lo[0] + hi[0]) * 0.5f, (lo[1] + hi[1]) * 0.5f, (lo[2] + hi[2]) * 0.5f };
  float fit = 0.0f;
  auto accumulate_radius = [&](const float p[3]) {
    const float dx = p[0] - centre[0];
    const float dy = p[1] - centre[1];
    const float dz = p[2] - centre[2];
    fit = std::max(fit, std::sqrt(dx * dx + dy * dy + dz * dz));
  };
  for (int v = 0; v < cache.mesh.vertex_count; ++v) {
    float d[3];
    to_display(&cache.mesh.vertices[v * 3], d);
    accumulate_radius(d);
  }
  for (size_t i = 0; i + 2 < line.points.size(); i += 3) {
    const float p[3] = { line.points[i], line.points[i + 1], line.points[i + 2] };
    float d[3];
    point_to_display(p, d);
    accumulate_radius(d);
  }
  fit = std::max(fit, 1e-4f);

  // The canvas + gestures: drag orbits (yaw/pitch), wheel zooms. Pure display state.
  const ImVec2 avail = ImGui::GetContentRegionAvail();
  const ImVec2 canvas_min = ImGui::GetCursorScreenPos();
  // The canvas gives back exactly what the legend's own line needs — a scrolled-out legend is
  // an unreachable one (the map region's rule), and the legend is AC4's colour key. No floor:
  // the canvas absorbs what is left and, at squeezed rows, yields to zero rather than push the
  // fixed stack above out of the region (an invisible canvas beats a scrolled-out title).
  const float canvas_h = avail.y - ImGui::GetStyle().ItemSpacing.y - ImGui::GetTextLineHeight();
  if (canvas_h > 0.0f) {
    const ImVec2 canvas_max(canvas_min.x + avail.x, canvas_min.y + canvas_h);
    ImGui::InvisibleButton("##r3d_canvas", ImVec2(canvas_max.x - canvas_min.x, canvas_max.y - canvas_min.y),
                           ImGuiButtonFlags_MouseButtonLeft);
    const bool active = ImGui::IsItemActive();
    if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
      const ImVec2 delta = ImGui::GetIO().MouseDelta;
      // Grab convention — the project's one drag metaphor (Render main view inside-out and Globe
      // alike, the modal preview trackball, this orbit camera; doc/geometry/coordinate-convention.md
      // §9.8): the content's near face follows the hand. Right drag → yaw decreases; down drag →
      // pitch decreases.
      view.orbit_yaw_deg -= delta.x * 0.5f;
      view.orbit_pitch_deg = std::clamp(view.orbit_pitch_deg - delta.y * 0.5f, -88.0f, 88.0f);
    }
    if (ImGui::IsItemHovered() && std::abs(ImGui::GetIO().MouseWheel) > 0.0f) {
      view.orbit_zoom =
          std::clamp(view.orbit_zoom * (ImGui::GetIO().MouseWheel > 0.0f ? 1.12f : 1.0f / 1.12f), 0.25f, 8.0f);
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(canvas_min, canvas_max, IM_COL32(16, 18, 22, 255));
    const float side = std::min(canvas_max.x - canvas_min.x, canvas_max.y - canvas_min.y);
    const float scale = (side * 0.42f / fit) * std::clamp(view.orbit_zoom, 0.25f, 8.0f);
    const ImVec2 mid((canvas_min.x + canvas_max.x) * 0.5f, (canvas_min.y + canvas_max.y) * 0.5f);
    double fwd[3];
    double right[3];
    double up[3];
    OrbitBasis(view.orbit_yaw_deg, view.orbit_pitch_deg, fwd, right, up);
    auto project = [&](const float p[3]) {
      const double dx = static_cast<double>(p[0]) - centre[0];
      const double dy = static_cast<double>(p[1]) - centre[1];
      const double dz = static_cast<double>(p[2]) - centre[2];
      const double sx = dx * right[0] + dy * right[1] + dz * right[2];
      const double sy = dx * up[0] + dy * up[1] + dz * up[2];
      return ImVec2(static_cast<float>(mid.x + sx * scale), static_cast<float>(mid.y - sy * scale));
    };

    // The mesh wireframe (dim), then the trace (full alpha on top).
    for (int e = 0; e < cache.mesh.edge_count; ++e) {
      float a[3];
      float b[3];
      to_display(&cache.mesh.vertices[cache.mesh.edges[e * 2] * 3], a);
      to_display(&cache.mesh.vertices[cache.mesh.edges[e * 2 + 1] * 3], b);
      dl->AddLine(project(a), project(b), IM_COL32(105, 110, 120, 120), 1.0f);
    }
    for (size_t s = 0; s < line.seg_kinds.size(); ++s) {
      const float* p0 = &line.points[s * 6];
      const float* p1 = p0 + 3;
      float d0[3];
      float d1[3];
      point_to_display(p0, d0);
      point_to_display(p1, d1);
      dl->AddLine(project(d0), project(d1), SegmentColor(line.seg_kinds[s], 1.0f), 2.0f);
    }
    // The contacts: one marker per REAL contact of the recovery. The polyline's point layout is
    // [entry_start, c0, c0, c1, ..., cl, exit_end] — odd indices 1..2N-1 are the N contacts, index
    // 0 the entry extension's start and the LAST index the exit extension's end, both this TU's
    // display synthesis and marked by nothing.
    for (size_t idx = 1; idx + 1 < line.points.size() / 3; idx += 2) {
      const float p[3] = { line.points[idx], line.points[idx + 1], line.points[idx + 2] };
      float d[3];
      point_to_display(p, d);
      dl->AddCircle(project(d), UiPx(2.5f), IM_COL32(240, 240, 240, 190), 0, 1.0f);
    }
    // The crystal-frame sun direction: one short arrow along u radiating from the crystal's own
    // centre (the crystal view's own anchor; the world view's entry leg already carries the
    // incident). The tip rides the content fit's scale, the anchor does not.
    if (view.crystal_frame) {
      const float u[3] = { static_cast<float>(current->sample.sun_in_crystal[0]),
                           static_cast<float>(current->sample.sun_in_crystal[1]),
                           static_cast<float>(current->sample.sun_in_crystal[2]) };
      const float tip[3] = { mesh_centre[0] + u[0] * fit * 0.6f, mesh_centre[1] + u[1] * fit * 0.6f,
                             mesh_centre[2] + u[2] * fit * 0.6f };
      dl->AddLine(project(mesh_centre), project(tip), ImGui::ColorConvertFloat4ToU32(AccentColor()), 1.5f);
    }
  }
  // The legend.
  ImGui::TextDisabled("entry");
  ImGui::SameLine();
  ImGui::GetWindowDrawList()->AddLine(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), SegmentColor(0, 1.0f), 2.0f);
  ImGui::SameLine();
  ImGui::TextDisabled("internal");
  ImGui::SameLine();
  ImGui::GetWindowDrawList()->AddLine(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), SegmentColor(1, 1.0f), 2.0f);
  ImGui::SameLine();
  ImGui::TextDisabled("exit");
  ImGui::SameLine();
  ImGui::GetWindowDrawList()->AddLine(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), SegmentColor(2, 1.0f), 2.0f);
  if (current->sample.interface_count >= 2) {
    ImGui::SameLine();
    ImGui::TextDisabled("face transmittance: entry %.3f, exit %.3f", current->sample.interface_transmittances[0],
                        current->sample.interface_transmittances[current->sample.interface_count - 1]);
  }
  ImGui::EndChild();
}

// ---- The point-level details block -------------------------------------------------------------------

void RenderPointDetailsBlock(GuiState& state, const SelectedClassIdentity& identity) {
  PointQuerySession& point = state.analysis.point;
  PointViewState& view = state.analysis.point_view;

  ImGui::Separator();
  ImGui::TextUnformatted("Point detail");
  if (identity.layer_face_counts.size() > 1) {
    // The request-level refusal the point face will name for this selection, spelled before it
    // is asked for (the survey block states the same fact for its own face).
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextWrapped(
        "Multi-layer chains are refused by the point face (multi_layer_unsupported); the existing "
        "retreat — keep only this chain in the filters and re-run — stays the way through.");
    ImGui::PopStyleColor();
    return;
  }
  const PointQueryState st = PointQueryStateFor(point);
  if (st == PointQueryState::kNoIntent) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextWrapped(
        "Pick a sky point on the raypath view — the point's components, factors and path appear "
        "here.");
    ImGui::PopStyleColor();
    return;
  }
  if (st == PointQueryState::kWaiting) {
    const std::string wait_line = PointWaitLineFor(point, ImGui::GetTime(), g_survey_executor.InflightPollSnapshot());
    state.analysis.wait_feedback_texts.point_wait_line_details = wait_line;
    ImGui::TextWrapped("%s", wait_line.c_str());
    if (ImGui::SmallButton("Cancel point")) {
      // R6's cancel row, the survey's shape: withdraw the intent, latch it against this key, and
      // stop the playback a withdrawn query could keep feeding.
      g_survey_executor.CancelInFlight(QueryFace::kPoint);
      point.cancelled_for_key = true;
      point.adopted_result.reset();
      point.adopted_error.clear();
      view.playing = false;
    }
    return;
  }
  if (st == PointQueryState::kCancelled) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextWrapped(
        "Point cancelled for this selection. Moving the sky point — or re-picking the class — asks "
        "anew.");
    ImGui::PopStyleColor();
    return;
  }
  if (st == PointQueryState::kRefused) {
    ImGui::TextUnformatted("Point refused:");
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextWrapped("%s", point.adopted_error.c_str());
    if (point.adopted_error.find("multi_layer") != std::string::npos) {
      ImGui::TextWrapped(
          "Multi-layer chains are not supported by the point face. The existing retreat stands: keep "
          "only this chain in the filters and re-run the analysis.");
    }
    ImGui::PopStyleColor();
    return;
  }

  // Completed: the document's own echo first (the engine's words, AC1), then the arm.
  const std::optional<LUMICE_PathPointInfo> info = ReadPointInfo(*point.adopted_result);
  if (!info.has_value()) {
    ImGui::TextDisabled("The adopted point document did not answer its Info reader.");
    return;
  }
  if (info->execution != LUMICE_PATH_OUTCOME_COMPLETED) {
    ImGui::PushStyleColor(ImGuiCol_Text, WarningTextColor());
    ImGui::TextWrapped("Partial document: %s — what arrived is on show, honestly incomplete.",
                       ExecutionText(info->execution));
    ImGui::PopStyleColor();
  }
  for (const std::string& line_text : PointCaliberLines(*info)) {
    ImGui::BulletText("%s", line_text.c_str());
  }

  if (info->outcome == LUMICE_PATH_POINT_MASS) {
    LUMICE_PathPointMass mass{};
    mass.struct_size = sizeof(mass);
    if (LUMICE_PathPointGetMass(point.adopted_result.get(), &mass) == LUMICE_PATH_READ_OK) {
      ImGui::Spacing();
      ImGui::TextUnformatted("Rank-0 point mass (no fiber, no playback — the mass is the whole answer):");
      for (const std::string& line_text : PointMassLines(mass)) {
        ImGui::BulletText("%s", line_text.c_str());
      }
    } else {
      ImGui::TextDisabled("The mass block did not answer its reader.");
    }
    return;
  }

  std::vector<PointComponentRow> rows;
  const int component_total = PointComponentRows(*point.adopted_result, &rows);
  ImGui::Spacing();
  if (component_total == 0) {
    LUMICE_PathPointReach reach{};
    reach.struct_size = sizeof(reach);
    const bool have_reach = LUMICE_PathPointGetReach(point.adopted_result.get(), &reach) == LUMICE_PATH_READ_OK;
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    if (have_reach && !reach.target_in_range) {
      ImGui::TextWrapped(
          "No component: the target's deviation lies outside this path's deflection range (reach "
          "probe, tolerance %.2e rad).",
          reach.tolerance_rad);
    } else {
      ImGui::TextWrapped(
          "No component at this point (the finite crystal passes no pose; the discovery's own "
          "completeness note is below).");
    }
    ImGui::PopStyleColor();
    LUMICE_PathPointDiscovery discovery{};
    discovery.struct_size = sizeof(discovery);
    if (LUMICE_PathPointGetDiscovery(point.adopted_result.get(), &discovery) == LUMICE_PATH_READ_OK &&
        discovery.completeness_note.length > 0) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
      ImGui::TextWrapped("%s", PathTextToString(discovery.completeness_note).c_str());
      ImGui::PopStyleColor();
    }
    return;
  }

  // The component list (AC2: rows with kind, ends, coverage, integral; selectable). The render
  // cap names itself when a document carries more components than the list shows.
  LUMICE_PathPointInfo count_info{};
  count_info.struct_size = sizeof(count_info);
  const size_t true_component_count =
      LUMICE_PathPointGetInfo(point.adopted_result.get(), &count_info) == LUMICE_PATH_READ_OK ?
          count_info.component_count :
          component_total;
  ImGui::TextUnformatted("Components (select one):");
  for (int i = 0; i < component_total; ++i) {
    const PointComponentRow& row = rows[static_cast<size_t>(i)];
    char label[192];
    std::snprintf(label, sizeof(label), "%d: %s, %d pts, ends %s / %s%s##pcomp%d", i + 1, row.kind.c_str(),
                  row.point_count, row.forward_text.c_str(), row.backward_text.c_str(),
                  row.integral_available ? "" : ", integral unavailable", i);
    const bool selected = std::clamp(view.component_index, 0, component_total - 1) == i;
    if (ImGui::Selectable(label, selected)) {
      view.component_index = i;
      view.sample_index = 0;
      view.playing = false;
    }
  }
  const int component = std::clamp(view.component_index, 0, component_total - 1);
  {
    const PointComponentRow& row = rows[static_cast<size_t>(component)];
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    if (row.integral_available) {
      ImGui::TextWrapped("fiber integral %.6g (covers %d/%d points' arclength)", row.fiber_integral,
                         row.available_points, row.total_points);
    } else {
      ImGui::TextWrapped("fiber integral: unavailable (%d/%d points usable)", row.available_points, row.total_points);
    }
    if (static_cast<int>(true_component_count) > component_total) {
      ImGui::TextWrapped("showing %d of %zu components (the list's render cap).", component_total,
                         true_component_count);
    }
    ImGui::PopStyleColor();
  }

  // The current sample's factors (AC3's vocabulary, the engine's names and units).
  const std::vector<int> detail_valid_indices = CollectValidSampleIndices(*point.adopted_result, component);
  const std::optional<CurrentSample> current = CurrentSampleOf(*point.adopted_result, view, detail_valid_indices);
  if (!current.has_value()) {
    ImGui::TextDisabled("No valid sample on show for this component.");
    return;
  }
  ImGui::SeparatorText("Current sample factors");
  for (const std::string& line_text : PointFactorLines(current->sample)) {
    ImGui::BulletText("%s", line_text.c_str());
  }
}

}  // namespace lumice::gui
