#include "gui/diagnostics_report.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <nlohmann/json.hpp>

#include "gui/gui_state.hpp"

namespace lumice::gui {
namespace diagnostics {

namespace {

// constexpr rather than a magic literal at the schema line: the report's
// first key and this number change together, and tests pin both.
constexpr int kSchemaVersion = 1;

// The lens's user-facing spelling, from the same table the UI draws. An
// out-of-range value (a future lens type read by an older build — not a
// state the report should crash on) degrades to a named unknown instead of
// a bare number.
std::string LensTypeNameOrUnknown(int lens_type) {
  if (lens_type >= 0 && lens_type < kLensTypeCount) {
    return kLensTypeNames[lens_type];
  }
  return "unknown(" + std::to_string(lens_type) + ")";
}

nlohmann::json HexGlEnum(std::optional<unsigned int> code) {
  if (!code.has_value()) {
    return nullptr;
  }
  char buf[16];
  std::snprintf(buf, sizeof(buf), "0x%04X", *code);
  return buf;
}

// Optional text reads as explicit null when absent — never an empty string
// pretending to be a value.
nlohmann::json OptionalText(const std::optional<std::string>& v) {
  return v.has_value() ? nlohmann::json(*v) : nlohmann::json(nullptr);
}

// Non-finite floats are honestly absent, not dumped: nlohmann would emit
// them as null anyway, but the paired note is what stops a reader from
// mistaking that null for "field not collected". fov is the only float in
// the whitelist today; a second float field reuses this.
nlohmann::json FiniteDegrees(float value) {
  if (!std::isfinite(value)) {
    return { { "value", nullptr }, { "state", "non_finite" } };
  }
  return value;
}

nlohmann::json FrozenSettingsToJson(const FrozenViewSettings& s) {
  nlohmann::json j;
  j["resolution_tier"] = s.resolution_tier;
  j["lens_type"] = LensTypeNameOrUnknown(s.lens_type);
  j["fov"] = FiniteDegrees(s.fov);
  j["ray_num_millions"] = FiniteDegrees(s.ray_num_millions);
  j["max_hits"] = s.max_hits;
  j["ray_num_infinite"] = s.ray_num_infinite;
  j["ray_allocation_adaptive"] = s.ray_allocation_adaptive;
  return j;
}

nlohmann::json GlErrorCheckToJson(const UploadFacts& f) {
  nlohmann::json j;
  switch (f.gl_error_state) {
    case GlErrorCheckState::kNone:
      j["state"] = "none";
      break;
    case GlErrorCheckState::kReported:
      j["state"] = "reported";
      j["code_hex"] = HexGlEnum(f.gl_error_code);
      break;
    case GlErrorCheckState::kNotChecked:
      j["state"] = "not_checked";
      break;
  }
  return j;
}

nlohmann::json UploadEventToJson(const UploadEvent& e, unsigned long long current_serial) {
  nlohmann::json j;
  j["occurred_at"] = FormatIso8601Utc(e.occurred_at);
  j["document_serial"] = e.document_serial;
  j["document_is_current"] = e.document_serial == current_serial;

  switch (e.context.provenance) {
    case UploadProvenance::kLive:
      j["provenance"] = "live";
      break;
    case UploadProvenance::kReopen:
      j["provenance"] = "reopen";
      break;
  }
  j["run_epoch"] = e.context.run_epoch.has_value() ? nlohmann::json(*e.context.run_epoch) : nlohmann::json(nullptr);
  j["bundle_epoch"] =
      e.context.bundle_epoch.has_value() ? nlohmann::json(*e.context.bundle_epoch) : nlohmann::json(nullptr);
  j["texture_serial"] =
      e.context.texture_serial.has_value() ? nlohmann::json(*e.context.texture_serial) : nlohmann::json(nullptr);
  j["active_backend_at_upload"] = OptionalText(e.context.active_backend_at_upload);
  j["server_generation"] = e.server_generation;

  j["source"] = { { "width", e.context.source_width }, { "height", e.context.source_height } };

  switch (e.facts.path) {
    case UploadPath::kXyz:
      j["path"] = "xyz";
      break;
    case UploadPath::kSrgbComposited:
      j["path"] = "srgb_composited";
      break;
    case UploadPath::kSrgbRadiance:
      j["path"] = "srgb_radiance";
      break;
  }

  switch (e.facts.outcome) {
    case UploadOutcome::kCompleted:
      j["outcome"] = "completed";
      break;
    case UploadOutcome::kRejectedInvalidInput:
      j["outcome"] = "rejected_invalid_input";
      break;
    case UploadOutcome::kFailedFrameRetained:
      j["outcome"] = "failed_frame_retained";
      break;
    case UploadOutcome::kFailedDisplayRevoked:
      j["outcome"] = "failed_display_revoked";
      break;
  }

  j["gl_error"] = GlErrorCheckToJson(e.facts);
  j["error_stage"] = e.facts.error_stage.has_value() ? nlohmann::json(*e.facts.error_stage) : nlohmann::json(nullptr);
  j["fence_create_failed"] =
      e.facts.fence_create_failed.has_value() ? nlohmann::json(*e.facts.fence_create_failed) : nlohmann::json(nullptr);

  nlohmann::json requested;
  requested["width"] = e.facts.requested_width;
  requested["height"] = e.facts.requested_height;
  requested["pbo_bytes"] = e.facts.pbo_bytes.has_value() ?
                               nlohmann::json(static_cast<unsigned long long>(*e.facts.pbo_bytes)) :
                               nlohmann::json(nullptr);
  j["requested"] = requested;
  if (e.facts.xyz_half_scale.has_value()) {
    j["xyz_half_scale"] = *e.facts.xyz_half_scale;
  }

  if (e.facts.actual_queried) {
    nlohmann::json actual;
    actual["state"] = "queried";
    actual["width"] = e.facts.actual_width;
    actual["height"] = e.facts.actual_height;
    actual["internal_format_hex"] = HexGlEnum(e.facts.actual_internal_format);
    j["actual_texture"] = actual;
  } else {
    // The measured state only exists where the upload ran its level queries;
    // a skipped or rejected attempt has nothing honest to print.
    j["actual_texture"] = { { "state", "not_queried" },
                            { "reason", e.facts.outcome == UploadOutcome::kCompleted ?
                                            "level queries did not run" :
                                            "upload did not reach the texture stage" } };
  }

  j["view_settings_at_upload"] = FrozenSettingsToJson(e.context.settings);

  switch (e.display_commit) {
    case DisplayCommit::kUnknown:
      j["display_commit"] = "unknown";
      break;
    case DisplayCommit::kAdvanced:
      j["display_commit"] = "advanced";
      break;
    case DisplayCommit::kNotAdvanced:
      j["display_commit"] = "not_advanced";
      break;
  }
  return j;
}

nlohmann::json OptionalCount(const std::optional<unsigned long long>& v) {
  return v.has_value() ? nlohmann::json(*v) : nlohmann::json(nullptr);
}

}  // namespace

bool IsUploadFailure(const UploadEvent& event) {
  if (event.facts.outcome != UploadOutcome::kCompleted) {
    return true;
  }
  if (event.facts.gl_error_state == GlErrorCheckState::kReported) {
    return true;
  }
  // A completed frame whose fence creation failed is still an observed
  // failure fact: the frame is displayed, but the slot's next write runs
  // unguarded. Consuming it here is what keeps a later successful upload
  // from erasing the fact from the retention (the completed outcome alone
  // would classify the event as a success).
  if (event.facts.fence_create_failed.value_or(false)) {
    return true;
  }
  return false;
}

unsigned long long DroppedEventCount(const UploadHistory& history) {
  // Distinct attempts among the three slots, identified by the collector's
  // event serial (one attempt copied into first_failure and last_failure
  // counts once). Serials, not timestamps: two uploads inside one clock tick
  // are a real state.
  unsigned long long serials[3] = {};
  size_t distinct = 0;
  const std::optional<UploadEvent>* slots[3] = { &history.first_failure, &history.last_failure, &history.last_upload };
  for (const auto* slot : slots) {
    if (!slot->has_value()) {
      continue;
    }
    bool seen = false;
    for (size_t i = 0; i < distinct; ++i) {
      if (serials[i] == (*slot)->event_serial) {
        seen = true;
        break;
      }
    }
    if (!seen) {
      serials[distinct++] = (*slot)->event_serial;
    }
  }
  return history.total_event_count > distinct ? history.total_event_count - distinct : 0;
}

void DiagnosticsCollector::BeginUpload(const UploadEventContext& context) {
  if (upload_pending_) {
    // Production never stacks slots; a stacked one means a caller began and
    // never drove an upload. The stale slot is discarded without a count: it
    // never became a recorded attempt, so there is no event the report could
    // have represented and lost.
  }
  upload_pending_ = true;
  pending_ = UploadEvent{};
  pending_.context = context;
  pending_.occurred_at = std::chrono::system_clock::now();
  pending_.document_serial = document_serial_;
}

void DiagnosticsCollector::OnServerReconstructed() {
  ++server_generation_;
}

void DiagnosticsCollector::CompleteUpload(const UploadFacts& facts) {
  if (!upload_pending_) {
    // An upload driven without a frozen context produces no event: "no
    // record" stays true rather than growing a half-attributed row.
    return;
  }
  upload_pending_ = false;
  pending_.facts = facts;
  pending_.event_serial = ++next_event_serial_;
  pending_.server_generation = server_generation_;
  last_closed_serial_ = pending_.event_serial;
  Retain(pending_);
  current_document_has_upload_ = true;
}

void DiagnosticsCollector::AttachDisplayCommit(DisplayCommit commit) {
  // The just-closed attempt may be retained in more than one slot (a failure
  // is copied into first_failure and last_failure beside last_upload); the
  // caller's fact belongs to the attempt, so every copy of it gets it.
  const auto apply = [this, commit](std::optional<UploadEvent>& slot) {
    if (slot.has_value() && slot->event_serial == last_closed_serial_) {
      slot->display_commit = commit;
    }
  };
  apply(history_.first_failure);
  apply(history_.last_failure);
  apply(history_.last_upload);
}

void DiagnosticsCollector::OnDocumentReset(DocumentOrigin origin) {
  ++document_serial_;
  current_origin_ = origin;
  current_document_has_upload_ = false;
}

void DiagnosticsCollector::Retain(const UploadEvent& event) {
  ++history_.total_event_count;
  if (IsUploadFailure(event)) {
    if (!history_.first_failure.has_value()) {
      history_.first_failure = event;  // latched: later successes never overwrite
    }
    history_.last_failure = event;
  }
  history_.last_upload = event;
}

void DiagnosticsCollector::ResetForTest() {
  history_ = UploadHistory{};
  document_serial_ = 1;
  current_origin_ = DocumentOrigin::kNew;
  current_document_has_upload_ = false;
  upload_pending_ = false;
  pending_ = UploadEvent{};
  last_closed_serial_ = 0;
  next_event_serial_ = 0;
  server_generation_ = 1;
}

DiagnosticsCollector& Diagnostics() {
  static DiagnosticsCollector collector;
  return collector;
}

FrozenViewSettings SnapshotFrozenSettings(int resolution_tier, int lens_type, float fov, float ray_num_millions,
                                          int max_hits, bool ray_num_infinite, bool ray_allocation_adaptive) {
  FrozenViewSettings s;
  s.resolution_tier = resolution_tier;
  s.lens_type = lens_type;
  s.fov = fov;
  s.ray_num_millions = ray_num_millions;
  s.max_hits = max_hits;
  s.ray_num_infinite = ray_num_infinite;
  s.ray_allocation_adaptive = ray_allocation_adaptive;
  return s;
}

std::string FormatIso8601Utc(std::chrono::system_clock::time_point tp) {
  const std::time_t time = std::chrono::system_clock::to_time_t(tp);
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count() % 1000;
  std::tm tm_utc{};
#ifdef _WIN32
  gmtime_s(&tm_utc, &time);
#else
  gmtime_r(&time, &tm_utc);
#endif
  char buf[40];
  std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03lldZ", tm_utc.tm_year + 1900, tm_utc.tm_mon + 1,
                tm_utc.tm_mday, tm_utc.tm_hour, tm_utc.tm_min, tm_utc.tm_sec, static_cast<long long>(ms));
  return buf;
}

std::string BuildDiagnosticsJson(const DiagnosticsExportInput& input, const std::vector<LogLine>* log_tail) {
  nlohmann::json j;

  j["schema_version"] = kSchemaVersion;
  j["exported_at"] = FormatIso8601Utc(input.exported_at);

  // --- Export-time environment -------------------------------------------------
  nlohmann::json app;
  app["version"] = OptionalText(input.environment.app_version);
  app["engine_isa_level"] = OptionalText(input.environment.engine_isa_level);
  j["application"] = app;

  nlohmann::json graphics;
  graphics["gl_vendor"] = OptionalText(input.gl_info.vendor);
  graphics["gl_renderer"] = OptionalText(input.gl_info.renderer);
  graphics["gl_version"] = OptionalText(input.gl_info.version);
  graphics["gl_shading_language_version"] = OptionalText(input.gl_info.shading_language_version);
  graphics["gl_max_texture_size"] = input.gl_info.max_texture_size.has_value() ?
                                        nlohmann::json(*input.gl_info.max_texture_size) :
                                        nlohmann::json(nullptr);
  graphics["collection_notes"] = input.gl_info.notes;
  graphics["note"] =
      "GL strings describe the display adapter of the context that exported this report; "
      "they do not identify the device the simulation ran on — see simulation_route.";
  j["graphics_context"] = graphics;

  nlohmann::json platform;
  platform["os_name"] = OptionalText(input.platform_info.os_name);
  platform["os_version"] = OptionalText(input.platform_info.os_version);
  platform["os_variant"] = OptionalText(input.platform_info.os_variant);
  platform["cpu_model"] = OptionalText(input.platform_info.cpu_model);
  platform["total_physical_memory_bytes"] = input.platform_info.total_physical_memory_bytes.has_value() ?
                                                nlohmann::json(*input.platform_info.total_physical_memory_bytes) :
                                                nlohmann::json(nullptr);
  platform["collection_notes"] = input.platform_info.notes;
  j["platform"] = platform;

  // --- Route -------------------------------------------------------------------
  nlohmann::json route;
  route["preferred_backend"] = OptionalText(input.route.preferred_backend);
  route["active_backend"] = OptionalText(input.route.active_backend);
  route["backend_fell_back_mid_run"] = input.route.backend_fell_back_mid_run.has_value() ?
                                           nlohmann::json(*input.route.backend_fell_back_mid_run) :
                                           nlohmann::json(nullptr);
  route["note"] =
      "preferred/active/fallback describe the CURRENT server session. A reopened document's picture "
      "was uploaded from its file and may predate this session; such a frame does not certify this route.";
  j["simulation_route"] = route;

  // --- Current document --------------------------------------------------------
  nlohmann::json doc;
  doc["document_serial"] = input.current.document_serial;
  // The collector's generations start at 1, so a 0 here means "not gathered"
  // — it reads as explicit null, never as a fabricated generation.
  doc["server_generation"] =
      input.current.server_generation != 0 ? nlohmann::json(input.current.server_generation) : nlohmann::json(nullptr);
  switch (input.current.origin) {
    case DocumentOrigin::kNew:
      doc["origin"] = "new";
      break;
    case DocumentOrigin::kImported:
      doc["origin"] = "imported";
      break;
    case DocumentOrigin::kReopened:
      doc["origin"] = "reopened";
      break;
    case DocumentOrigin::kReopenedNoTexture:
      doc["origin"] = "reopened_no_texture";
      break;
  }
  if (input.current.has_texture) {
    doc["display"] = { { "has_texture", true },
                       { "texture_mode", input.current.texture_mode },
                       { "width", input.current.display_width },
                       { "height", input.current.display_height } };
  } else {
    doc["display"] = { { "has_texture", false } };
  }
  doc["committed_epoch"] = OptionalCount(input.current.committed_epoch);
  doc["last_uploaded_texture_serial"] = OptionalCount(input.current.last_uploaded_texture_serial);
  doc["texture_upload_count"] = input.current.texture_upload_count;
  doc["view_settings_at_export"] = FrozenSettingsToJson(input.current.settings_at_export);
  doc["current_document_has_upload_attempt"] = input.current_document_has_upload;
  j["current_document"] = doc;

  // --- Upload history (the incident record) ------------------------------------
  nlohmann::json history;
  history["retention_policy"] =
      "This session's first failure (latched; later successes do not overwrite it), the most recent "
      "failure, and the most recent attempt of any outcome. Attempts no longer represented are "
      "counted in dropped_event_count. The history is process-local and leaves with the app. "
      "view_settings_* are the VIEW and simulation settings as of the upload or the export — the "
      "configuration the source run actually used is not captured here.";
  history["server_generation_note"] =
      "server_generation namespaces run epochs: a server reconstruction (backend/worker change) "
      "restarts the epoch authority at 0 inside the same document, so only the pair is unique.";
  history["dropped_event_count"] = DroppedEventCount(input.history);
  history["first_failure"] = input.history.first_failure.has_value() ?
                                 UploadEventToJson(*input.history.first_failure, input.current.document_serial) :
                                 nlohmann::json(nullptr);
  history["last_failure"] = input.history.last_failure.has_value() ?
                                UploadEventToJson(*input.history.last_failure, input.current.document_serial) :
                                nlohmann::json(nullptr);
  history["last_upload"] = input.history.last_upload.has_value() ?
                               UploadEventToJson(*input.history.last_upload, input.current.document_serial) :
                               nlohmann::json(nullptr);
  j["upload_history"] = history;

  // --- Raw log, only on explicit opt-in ----------------------------------------
  if (log_tail != nullptr) {
    nlohmann::json logs = nlohmann::json::array();
    logs.get_ref<nlohmann::json::array_t&>().reserve(log_tail->size());
    for (const LogLine& line : *log_tail) {
      logs.push_back({ { "level", line.level }, { "message", line.message } });
    }
    j["log_tail"] = logs;
  }
  // No opt-in: the key stays absent. An empty ring on an opted-in export is a
  // present-but-empty array — those two mean different things.

  // Non-UTF-8 bytes (a raw log line, a platform string) must degrade to the
  // replacement character, never throw: the export's contract is a readable
  // file or a visible failure, not an escape through an exception.
  return j.dump(2, ' ', false, nlohmann::json::error_handler_t::replace);
}

}  // namespace diagnostics
}  // namespace lumice::gui
