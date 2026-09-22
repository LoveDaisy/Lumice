#include "gui/user_defaults.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <system_error>
#include <utility>

#include "gui/edit_modals.hpp"
#include "gui/file_io.hpp"
#include "gui/gui_logger.hpp"
#include "util/path_utils.hpp"

namespace lumice::gui {

namespace {

// Degradation bookkeeping for the current load. Same shape as file_io.cpp's
// g_shape_dist_downgrade_count: a TU-local counter consumed (and zeroed) by its Take* function.
// Override-file loading is single-threaded (startup / New / import), so no synchronization is
// needed and no accumulator has to be threaded through every helper.
int g_downgrade_count = 0;
std::vector<std::string> g_downgrade_notices;

// Loaded preset-library overrides, indexed by AxisPreset. Only the presets whose kAxisPresets row
// has an adjustable face can ever be populated.
//
// Two faces, each with its own presence flag, and each with its own Read/Write/Erase/Adopt/Get
// family below. That is a deliberate parallel design, not an abstraction left on the table: the
// two are different kinds of quantity (std is continuous and an out-of-domain value is CLAMPED to
// the nearest legal one; type is a discrete set and an out-of-set value has no nearest neighbour,
// so it is REFUSED and the factory type stands), and folding them into one interface would either
// give type a clamp it cannot have or take std's away. A third face would get a third family.
struct AxisPresetOverride {
  bool has_zenith_std = false;
  float zenith_std = 0.0f;
  bool has_zenith_type = false;
  AxisDistType zenith_type = AxisDistType::kGauss;
};
constexpr std::size_t kAxisPresetSlotCount = 6;  // AxisPreset has 6 enumerators
static_assert(kAxisPresetSlotCount == static_cast<std::size_t>(AxisPreset::kCustom) + 1,
              "kAxisPresetSlotCount must track AxisPreset's enumerator count (kCustom is its "
              "last member) so the g_axis_overrides index never goes out of bounds silently");

// Value type (not a bare array) so MakeNewDocumentState() can replace the whole thing with one
// unconditional assignment instead of a loop gated behind a branch. code-review round 4: gating
// the reset inside `if (dir)` was the second time a branch wrapped around this reset produced a
// stale-slot leak (round 2's Major, fixed by 806eff19, was the first) — the fix this time is to
// make the assignment itself unconditional rather than adding another conditional reset.
struct AxisPresetOverrides {
  AxisPresetOverride slots[kAxisPresetSlotCount];
};
AxisPresetOverrides g_axis_overrides;

// The user's saved wedge-angle shortcuts, in the order the file lists them. A plain vector rather
// than the fixed slot array above because this list has no built-in rows to index into: every
// element is one the user added. Replaced wholesale by MakeNewDocumentState(), for the same reason
// the struct above is a value type — an unconditional assignment cannot leave a stale element the
// way a loop behind a branch can.
std::vector<WedgeMillerTriple> g_user_wedge_presets;

// Process-wide personal-defaults source, installed once from argv by each binary's main() (see
// SetUserConfigSourceForProcess). kAutoDetect is the unset value, so a binary that never calls
// the setter keeps the pre-switch behavior.
UserConfigSource g_process_user_config_source = UserConfigSource::kAutoDetect;
std::filesystem::path g_process_user_config_explicit_dir;

std::optional<std::string> ReadEnv(const char* key) {
  const char* value = std::getenv(key);
  if (value == nullptr || *value == '\0') {
    return std::nullopt;
  }
  return std::string(value);
}

// Clamp a stored zenith std into the domain ClassifyAxisPreset still recognizes as this preset:
//   Column / Plate / Parry : (0, kColumnPlateParryZenithStdUpperBound)
//   Lowitz                 : (kLowitzZenithStdLowerBound, inf)
// Both domains are OPEN, so the clamp target is the next representable float inside the
// interval rather than the bound itself — landing exactly on the bound would fail the
// classifier's strict inequality and turn the preset into "Custom".
// Returns true when the value was modified.
bool ClampZenithStdToPresetDomain(AxisPreset preset, float& value) {
  constexpr float kInf = std::numeric_limits<float>::infinity();
  const float original = value;
  if (preset == AxisPreset::kLowitz) {
    if (!(value > kLowitzZenithStdLowerBound)) {
      value = std::nextafter(kLowitzZenithStdLowerBound, kInf);
    }
  } else {
    if (!(value > 0.0f)) {
      value = std::nextafter(0.0f, kInf);
    } else if (!(value < kColumnPlateParryZenithStdUpperBound)) {
      value = std::nextafter(kColumnPlateParryZenithStdUpperBound, 0.0f);
    }
  }
  return value != original;
}

// The ONE sentence describing a clamp, shared by the load path and the two edit paths (the panel's
// warning cell and the modal's "Save as <preset>" gesture). Three call sites wording the same
// event differently is how a user ends up believing three different things happened.
//
// COPY CONSTRAINT: it must not present the bound as a physical fact. Plate's dead zone [10, 15) is
// the clearest case — a user asking for std=12 is refused because 12 is where the Lowitz criterion
// starts, i.e. for an implementation reason. Saying "not physically possible" there would be a
// lie, and one the user cannot check.
std::string DescribeAxisPresetClamp(AxisPreset preset, float requested, float stored) {
  const AxisPresetEntry& entry = AxisPresetEntryFor(preset);
  // Both names: the label is what the user pressed, the dotted key is what they would search for
  // if they open the override file to check. Naming only one leaves whichever half of that they
  // are holding unmatched.
  return std::string("Preset '") + entry.label + "' (presets.axis." + entry.override_json_name + "): a zenith std of " +
         FormatAxisPresetStd(requested) + " is outside the range this preset is still recognised in, so " +
         FormatAxisPresetStd(stored) + " was stored instead. Allowed: " + DescribeAxisPresetZenithStdDomain(preset) +
         " — that boundary is where the neighbouring preset's criterion begins, not a physical limit.";
}

// The ONE sentence describing a refused zenith type, shared by the load path and any edit path
// that reports one. Deliberately NOT the clamp sentence with a word swapped: std is clamped ("was
// stored instead"), type is refused outright ("was ignored, the built-in type stands"), and two
// notices that read alike for two different outcomes would teach a user that both do the same
// thing.
std::string DescribeAxisPresetTypeRejection(AxisPreset preset, const std::string& requested_spelling) {
  const AxisPresetEntry& entry = AxisPresetEntryFor(preset);
  return std::string("Preset '") + entry.label + "' (presets.axis." + entry.override_json_name +
         "): a zenith type of '" + requested_spelling +
         "' is not one this preset is still recognised with, so it was ignored and the built-in " +
         AxisDistTypeLabel(entry.zenith.type) +
         " stays in effect. Allowed: " + DescribeAxisPresetZenithTypeDomain(preset) + ".";
}

// Re-assert that the DOCUMENT half of an override file did not decide an app-preference field.
//
// It is the boundary between namespace 1 (the GuiState half, applied by ApplyUserDefaultsOverlay)
// and namespace 3 (the `app` root key), pinned in code. Most fields namespace 1 may not decide are
// structurally unreachable from it anyway — kDisplay and the `kView \ serialized` set have no JSON
// key at all, and the collections are cleared wholesale by the caller. This handles the ones that
// are ordinary serializable scalars, where the file's top level has a well-formed place to put
// them. Today those are `use_gpu_backend` and `worker_count`, both registered
// app_preference_eligible.
//
// It does NOT rest on the claim that the deserializer would read such a key today: for neither of
// them would it — the GuiState serializer is written key by key (file_io.cpp) and has no entry for
// either field, so a top-level `use_gpu_backend` or `worker_count` is inert on the way in. The
// reason to keep the reset anyway is that this is the one place where "namespace 1 cannot decide
// this field" is stated as code, its cost is one assignment per field, and the alternative is that
// the boundary holds only for as long as nobody adds a field to the serializer. An earlier version of this
// comment asserted the deserializer WOULD read it; that was checked and is false, which is the
// second reason the justification is stated as a boundary rather than as a defence against a
// specific reader.
//
// ORDER: this must run BEFORE ApplyAppPreferencesOverride, never after — reversed, it would wipe
// the personal default namespace 3 had just legitimately applied. Both calls are welded into
// ResetIneligibleScalarsThenApplyAppOverride below so no call site has to know that.
//
// Keep kIneligibleScalarResetFieldCount (user_defaults.hpp) in step with this body; the AC1
// test asserts the constant equals the population of the app_preference_eligible predicate, so
// adding another such field turns that test red rather than leaving a silent hole here.
void ResetIneligibleScalarFields(GuiState& state) {
  const GuiState factory{};
  state.use_gpu_backend = factory.use_gpu_backend;
  state.worker_count = factory.worker_count;
}

// "Does this document say anything the user chose?" — the provenance stamp does not count.
//
// The stamp is written by WriteUserDefaultsFile on every save, so a user who reverts every
// personal default back to factory ends up with a file holding exactly one key instead of the
// `{}` that meant "nothing saved" before. Anything answering "are there personal defaults?" with
// a plain emptiness test would flip its answer over a field the user never set and cannot see.
// Only one such test exists today (ApplyUserDefaultsOverlay's early return, below); this states
// the rule where the next one will find it, instead of leaving it to be re-derived.
bool IsOverlayDocEffectivelyEmpty(const nlohmann::json& doc) {
  if (!doc.is_object()) {
    return true;
  }
  for (const auto& item : doc.items()) {
    if (item.key() != kUserDefaultsOverlaySchemaVersionKey) {
      return false;
    }
  }
  return true;
}

// Classify the overlay's provenance stamp and report anything worth telling the user about. It
// returns nothing and gates nothing: the caller proceeds identically whatever this finds, which
// is the mechanical form of "record, never refuse" (see kUserDefaultsOverlaySchemaVersion).
//
//   absent          — every file written before the stamp existed. Silent, NOT a degradation:
//                     that is the same reason ReadOverlayJsonIfPresent short-circuits a missing
//                     file rather than counting it, and folding it in would hand every existing
//                     user a warning the first time they upgraded.
//   malformed       — string / null / negative / fractional. Treated as absent, but noticed: the
//                     file says something about itself that cannot be true, and the legal domain
//                     is positive integers only (0 was never written by any build, the counter
//                     starts at 1).
//   newer than this build — noticed, and that is ALL. The other keys are applied in full; see
//                     AC5 in the issue and the header note for why refusing them would be worse
//                     than the pre-stamp behavior it replaces.
//   at or below this build — the ordinary case (a file this build or an older one wrote). Silent:
//                     reading an older file is the design's daily job, not an anomaly.
void ReportSchemaVersionStampIssues(const nlohmann::json& doc) {
  const auto stamp = doc.find(kUserDefaultsOverlaySchemaVersionKey);
  if (stamp == doc.end()) {
    return;
  }

  // The classification below reads a value from a document that need not have come from a
  // text file this build's own parser validated. `dump()` (used to render the malformed-shape
  // notice) independently re-validates UTF-8 and throws (nlohmann::json type_error 316) on a
  // string built in-memory from raw invalid bytes — e.g. `nlohmann::json{}["k"] =
  // std::string("\xC0\x80")`, which bypasses json::parse()'s own strict UTF-8 checking (that
  // checking is why a hand-edited FILE containing such a string, like an unpaired UTF-16
  // surrogate escape `\uD800`, never reaches here at all: parse() rejects it upstream in
  // ReadOverlayJsonIfPresent, well before this doc exists). This exception would otherwise
  // escape this function and, since it runs before ApplyUserDefaultsOverlay's own try/catch
  // (see the call site), abort loading the ENTIRE overlay — turning "an oddly-shaped version
  // stamp" into exactly the kind of hard failure B3/AC5/AC6 rule out. A malformed stamp must
  // never do worse than being ignored-and-reported.
  try {
    if (!stamp->is_number_integer() || stamp->get<long long>() <= 0) {
      NoteUserDefaultsDowngrade(std::string("'") + kUserDefaultsOverlaySchemaVersionKey +
                                "' is not a positive integer (" + stamp->dump() +
                                "); treating the file as unstamped and loading it anyway");
      return;
    }

    const long long version = stamp->get<long long>();
    if (version > kUserDefaultsOverlaySchemaVersion) {
      // COPY CONSTRAINT, same one DescribeAxisPresetClamp above is written under: the sentence
      // must not assert something the user cannot check and that may well be false. A higher
      // stamp says only which build wrote the file — it does NOT imply the file holds any key
      // this build fails to recognize, and the common case is that it holds none (the stamp
      // rises on every format generation, whether or not the user's own settings changed shape).
      // "settings it does not recognize are ignored" told every such user their settings had
      // been dropped, on a load where nothing was. State what always holds: known keys applied,
      // unknown ones (if any) skipped.
      NoteUserDefaultsDowngrade("personal defaults were written by a newer version of Lumice (format " +
                                std::to_string(version) + ", this build understands " +
                                std::to_string(kUserDefaultsOverlaySchemaVersion) +
                                "); every setting this build recognizes was applied as usual, and any it does "
                                "not recognize were skipped");
    }
  } catch (const std::exception&) {
    NoteUserDefaultsDowngrade(std::string("'") + kUserDefaultsOverlaySchemaVersionKey +
                              "' could not be interpreted; treating the file as unstamped and loading it anyway");
  }
}

}  // namespace

int TakeUserDefaultsDowngradeCount() {
  int n = g_downgrade_count;
  g_downgrade_count = 0;
  return n;
}

std::vector<std::string> TakeUserDefaultsDowngradeNotices() {
  std::vector<std::string> notices;
  notices.swap(g_downgrade_notices);
  return notices;
}

void NoteUserDefaultsDowngrade(std::string notice) {
  ++g_downgrade_count;
  GUI_LOG_WARNING("[GUI] User defaults: {}", notice);
  g_downgrade_notices.push_back(std::move(notice));
}

std::optional<std::filesystem::path> GetUserConfigDir() {
  std::optional<std::filesystem::path> dir;
#if defined(_WIN32)
  dir = ComputeWindowsConfigDir(ReadEnv("APPDATA"));
#elif defined(__APPLE__)
  dir = ComputeMacConfigDir(ReadEnv("HOME"));
#else
  dir = ComputeLinuxConfigDir(ReadEnv("XDG_CONFIG_HOME"), ReadEnv("HOME"));
#endif
  if (!dir) {
    GUI_LOG_WARNING(
        "[GUI] GetUserConfigDir: no per-user config directory available from the environment; "
        "user defaults are disabled for this session");
    return std::nullopt;
  }

  std::error_code ec;
  std::filesystem::create_directories(*dir, ec);
  if (!std::filesystem::is_directory(*dir)) {
    GUI_LOG_WARNING("[GUI] GetUserConfigDir: cannot create '{}' ({}); user defaults are disabled for this session",
                    PathToU8(*dir), ec.message());
    return std::nullopt;
  }
  return dir;
}

std::optional<std::filesystem::path> GetActiveUserConfigDir() {
  switch (g_process_user_config_source) {
    case UserConfigSource::kDisabled:
      return std::nullopt;
    case UserConfigSource::kExplicitDir:
      return g_process_user_config_explicit_dir;
    case UserConfigSource::kAutoDetect:
      return GetUserConfigDir();
  }
  return GetUserConfigDir();  // unreachable; silences -Wreturn-type on some compilers
}

void SetUserConfigSourceForProcess(UserConfigSource source, std::filesystem::path explicit_dir) {
  g_process_user_config_source = source;
  g_process_user_config_explicit_dir = std::move(explicit_dir);
}

nlohmann::json ReadOverlayJsonIfPresent(const std::filesystem::path& dir) {
  const std::filesystem::path file = dir / kUserDefaultsFileName;

  // Short-circuit BEFORE the try/catch: "the user has never saved a default" is the normal
  // first-run state, not a degradation. Folding it into the failure path would make the
  // load-time notice fire for every fresh install.
  std::error_code ec;
  if (!std::filesystem::exists(file, ec) || ec) {
    return nlohmann::json::object();
  }

  std::ifstream in(file);
  if (!in.is_open()) {
    ++g_downgrade_count;
    GUI_LOG_WARNING("[GUI] User defaults: cannot open '{}'; ignoring it", PathToU8(file));
    return nlohmann::json::object();
  }

  try {
    nlohmann::json doc = nlohmann::json::parse(in);
    if (!doc.is_object()) {
      ++g_downgrade_count;
      GUI_LOG_WARNING("[GUI] User defaults: '{}' is not a JSON object; ignoring it", PathToU8(file));
      return nlohmann::json::object();
    }
    return doc;
  } catch (const std::exception& e) {
    ++g_downgrade_count;
    GUI_LOG_WARNING("[GUI] User defaults: '{}' is not valid JSON ({}); ignoring it", PathToU8(file), e.what());
    return nlohmann::json::object();
  }
}

namespace detail {

// Layer the sparse override onto a full factory document rather than handing the fragment
// straight to the deserializer. Both routes end at the same values (the deserializer is
// already "missing key = factory value"), but this one keeps the deserializer's own
// "no renderer key found" diagnostic honest: it is meant to flag a malformed .lmc, and it
// would otherwise fire on every startup for a user whose defaults touch no renderer setting.
nlohmann::json BuildMergedOverlayDocument(const nlohmann::json& doc) {
  nlohmann::json merged = nlohmann::json::parse(SerializeGuiStateJson(GuiState{}));
  merged.merge_patch(doc);

  // Unconditional, after the merge and before anyone reads the result: whatever the overlay said
  // about `schema_version` describes the file it came from, never the document being assembled
  // here. merge_patch has already either overwritten this build's value with the file's, or —
  // for a `null` — deleted the key; one assignment repairs both. See the header for why this
  // cannot be observed through the resulting GuiState, and therefore why this function is
  // separately visible at all.
  merged["schema_version"] = kGuiStateSchemaVersion;
  return merged;
}

}  // namespace detail

void ApplyUserDefaultsOverlay(GuiState& state, const nlohmann::json& doc) {
  if (!doc.is_object()) {
    return;
  }

  // Before the emptiness check, not after: a document whose ONLY key is a malformed stamp is
  // still a file making a false claim about itself, and the user is owed the notice even though
  // there is nothing left to apply.
  ReportSchemaVersionStampIssues(doc);

  if (IsOverlayDocEffectivelyEmpty(doc)) {
    return;
  }

  nlohmann::json merged;
  GuiState overlaid;
  try {
    merged = detail::BuildMergedOverlayDocument(doc);
    if (!DeserializeGuiStateJson(merged.dump(), overlaid)) {
      ++g_downgrade_count;
      GUI_LOG_WARNING("[GUI] User defaults: override document could not be applied; using factory defaults");
      return;
    }
  } catch (const std::exception& e) {
    // A field-level type error (`"bg_alpha": "not a number"`) throws out of the middle of the
    // merge. Discard the whole overlay: a half-applied set of defaults is harder to reason
    // about than none, and `state` is left exactly as the caller had it.
    ++g_downgrade_count;
    GUI_LOG_WARNING("[GUI] User defaults: override document has a malformed value ({}); using factory defaults",
                    e.what());
    return;
  }

  state = std::move(overlaid);
}

// Parse the preset-library half of an override document: root["presets"]["axis"]["<preset>"]
// ["zenith_std"]. Pure function — it neither reads nor writes g_axis_overrides; the caller
// (MakeNewDocumentState()) decides how the result replaces the global, unconditionally on every
// call. A user may retune a built-in preset, but only within the domain ClassifyAxisPreset still
// recognizes as that preset — otherwise the preset library would be able to define a "Column"
// the classifier calls Custom. Values outside the domain are therefore clamped, and each clamp
// is recorded in TakeUserDefaultsDowngradeNotices(): a silent clamp is a silent data loss.
AxisPresetOverrides ParseAxisPresetOverrides(const nlohmann::json& root) {
  AxisPresetOverrides result{};

  if (!root.is_object() || !root.contains("presets")) {
    return result;
  }
  const nlohmann::json& presets = root["presets"];
  if (!presets.is_object() || !presets.contains("axis")) {
    return result;
  }
  const nlohmann::json& axis = presets["axis"];
  if (!axis.is_object()) {
    return result;
  }

  for (const auto& entry : kAxisPresets) {
    if (!entry.has_adjustable_zenith_std) {
      continue;  // no key on disk for this preset — see AxisPresetEntry's field comments
    }
    if (!axis.contains(entry.override_json_name)) {
      continue;
    }
    const nlohmann::json& node = axis[entry.override_json_name];
    if (!node.is_object()) {
      continue;
    }
    // The two faces are parsed independently: a bad zenith_type must not take a good zenith_std
    // down with it, or vice versa. Each lands in `slot` only once its own checks have passed, and
    // the slot is assigned as one whole value at the end.
    AxisPresetOverride slot{};

    if (node.contains("zenith_std")) {
      const nlohmann::json& value_node = node["zenith_std"];
      if (!value_node.is_number()) {
        ++g_downgrade_count;
        GUI_LOG_WARNING("[GUI] User defaults: presets.axis.{}.zenith_std is not a number; ignoring it",
                        entry.override_json_name);
      } else if (float value = value_node.get<float>(); !std::isfinite(value)) {
        ++g_downgrade_count;
        GUI_LOG_WARNING("[GUI] User defaults: presets.axis.{}.zenith_std is not finite; ignoring it",
                        entry.override_json_name);
      } else {
        const float original = value;
        if (ClampZenithStdToPresetDomain(entry.id, value)) {
          // Never silent: at load time the user is not looking at the preset panel, so a clamp
          // with no trace would be indistinguishable from the value having been dropped.
          NoteUserDefaultsDowngrade(DescribeAxisPresetClamp(entry.id, original, value));
        }
        slot.has_zenith_std = true;
        slot.zenith_std = value;
      }
    }

    if (node.contains("zenith_type")) {
      const nlohmann::json& type_node = node["zenith_type"];
      if (!type_node.is_string()) {
        ++g_downgrade_count;
        GUI_LOG_WARNING("[GUI] User defaults: presets.axis.{}.zenith_type is not a string; ignoring it",
                        entry.override_json_name);
      } else {
        // Three outcomes and no clamp, because a discrete set has no "nearest legal value": a
        // recognised type inside this preset's accepted set is adopted; anything else — a spelling
        // the table does not know, or a real type the classifier would not keep this preset under
        // (Column with zigzag) — is refused whole and the factory type stands. Both refusals are
        // reported through the same sentence, since to the user they are the same event.
        const std::string spelled = type_node.get<std::string>();
        const auto parsed = AxisDistTypeFromJsonName(spelled);
        if (parsed && IsAcceptedZenithType(entry.id, *parsed)) {
          slot.has_zenith_type = true;
          slot.zenith_type = *parsed;
        } else {
          NoteUserDefaultsDowngrade(DescribeAxisPresetTypeRejection(entry.id, spelled));
        }
      }
    }

    result.slots[static_cast<std::size_t>(entry.id)] = slot;
  }
  return result;
}

// JSON key names for one stored wedge shortcut. Spelled once so the reader, the writer and the
// tests cannot disagree about the document shape.
constexpr const char* kWedgePresetsKey = "wedge";
constexpr const char* kWedgeIndexKeyH = "h";
constexpr const char* kWedgeIndexKeyK = "k";
constexpr const char* kWedgeIndexKeyL = "l";

// One integer field of a stored triple. Returns false when the key is missing or is not an integer
// — a float 1.0 is refused rather than truncated, because a Miller index that arrived as a
// non-integer is a hand-edit whose intent nobody can recover, and silently rounding it would invent
// one.
bool ReadWedgeIndexField(const nlohmann::json& node, const char* key, int& out) {
  const auto it = node.find(key);
  if (it == node.end() || !it->is_number_integer()) {
    return false;
  }
  out = it->get<int>();
  return true;
}

// Does `list` already hold this triple? The three de-duplication sites in this feature (load-time
// filter below, the merged dropdown table in edit_modals.cpp, the panel's Add button) each answer
// this over a different list with different consequences, so they are not one authority split three
// ways; this helper only spares THIS file from spelling the scan twice.
bool ContainsWedgeTriple(const std::vector<WedgeMillerTriple>& list, const WedgeMillerTriple& candidate) {
  for (const WedgeMillerTriple& existing : list) {
    if (existing == candidate) {
      return true;
    }
  }
  return false;
}

// Parse the wedge half of an override document: root["presets"]["wedge"], an array of {h,k,l}.
// Pure with respect to the cache — it neither reads nor writes g_user_wedge_presets; the caller
// (MakeNewDocumentState()) decides how the result replaces the global, unconditionally on every
// call.
//
// This is the load path, so unlike ReadWedgePresetsFromDoc it DOES judge each triple and DOES leave
// a trace. Two things get dropped, each with one notice: a triple the owner cannot build a cone
// face from (k != 0 is the common hand-edit, but h == 0 and a zero l are refused for their own
// reasons — the verdict is entirely EvaluateCustomWedgeInput's, which is entirely
// LUMICE_ConvertMillerIndexToWedgeAngle's), and a triple that repeats one already accepted. A
// silently shortened list would be indistinguishable from the file never having held those rows.
//
// The refusal wording is EvaluateCustomWedgeInput's own, not a second phrasing written here: the
// dropdown's live feedback and this notice describe the same verdict, and two wordings of one
// verdict drift the first time either is edited.
std::vector<WedgeMillerTriple> ParseWedgePresetOverrides(const nlohmann::json& root) {
  std::vector<WedgeMillerTriple> result;
  for (const WedgeMillerTriple& candidate : ReadWedgePresetsFromDoc(root)) {
    const CustomWedgeInputFeedback fb = EvaluateCustomWedgeInput(candidate.h, candidate.k, candidate.l);
    if (!fb.can_apply) {
      NoteUserDefaultsDowngrade("saved wedge preset {" + std::to_string(candidate.h) + "," +
                                std::to_string(candidate.k) + "," + std::to_string(candidate.l) +
                                "} was dropped: " + fb.message);
      continue;
    }
    if (ContainsWedgeTriple(result, candidate)) {
      NoteUserDefaultsDowngrade("saved wedge preset {" + std::to_string(candidate.h) + "," +
                                std::to_string(candidate.k) + "," + std::to_string(candidate.l) +
                                "} was listed more than once; the duplicate was dropped.");
      continue;
    }
    result.push_back(candidate);
  }
  return result;
}

int RoundTripPrecisionForAxisPresetStd(float value) {
  char buffer[40];
  for (int precision = 6; precision < 9; ++precision) {
    std::snprintf(buffer, sizeof(buffer), "%.*g", precision, static_cast<double>(value));
    if (std::strtof(buffer, nullptr) == value) {
      return precision;
    }
  }
  return 9;
}

std::string FormatAxisPresetStd(float value) {
  // Shortest form that reads back as the same float, not a fixed precision. A fixed %.6g renders
  // the tuned values a user types correctly (0.3 stays "0.3") but collapses the clamp target —
  // nextafter(10, 0) is 9.99999905, which %.6g rounds to "10" and turns the notice into a
  // contradiction: "10 was stored instead. Allowed: less than 10." Escalating only when the short
  // form is lossy keeps the common case short AND the boundary case honest.
  char buffer[40];
  std::snprintf(buffer, sizeof(buffer), "%.*g", RoundTripPrecisionForAxisPresetStd(value), static_cast<double>(value));
  return buffer;
}

std::string DescribeAxisPresetZenithStdDomain(AxisPreset preset) {
  if (preset == AxisPreset::kLowitz) {
    return "greater than " + FormatAxisPresetStd(kLowitzZenithStdLowerBound);
  }
  return "greater than 0 and less than " + FormatAxisPresetStd(kColumnPlateParryZenithStdUpperBound);
}

std::string DescribeAxisPresetZenithTypeDomain(AxisPreset preset) {
  const auto choices = AcceptedZenithTypesForPreset(preset);
  std::string out;
  for (std::size_t i = 0; i < choices.size(); ++i) {
    if (i > 0) {
      out += (i + 1 == choices.size()) ? " or " : ", ";
    }
    out += choices[i].label;
  }
  return out;
}

AxisPresetTypeResult ValidateAxisPresetZenithTypeForSave(AxisPreset preset, AxisDistType requested) {
  AxisPresetTypeResult result;
  const AxisPresetEntry& entry = AxisPresetEntryFor(preset);
  // Same two-clause guard as ClampAxisPresetZenithStdForSave, for the same reason: a nullptr
  // override_json_name is a crash at the write, not a refusal, and the static_assert that rules it
  // out today is not what "refuses cleanly" should rest on.
  if (!entry.has_adjustable_zenith_std || entry.override_json_name == nullptr) {
    GUI_LOG_WARNING("[GUI] User defaults: preset '{}' has no adjustable zenith type; nothing was saved", entry.label);
    result.message = std::string(entry.label) + " has no adjustable value, so nothing was saved.";
    return result;
  }
  if (static_cast<int>(requested) < 0 || requested >= AxisDistType::kCount) {
    GUI_LOG_WARNING("[GUI] User defaults: preset '{}' zenith type {} is not a distribution; nothing was saved",
                    entry.label, static_cast<int>(requested));
    result.message = "That is not a distribution type, so nothing was saved.";
    return result;
  }
  if (!IsAcceptedZenithType(preset, requested)) {
    // The UI's combo never offers a type outside the set, so this is the second defense — the one
    // a hand-edited working copy or a future caller hits. Refused, not clamped: see the note on
    // AxisPresetOverride for why the two faces differ here.
    result.message = DescribeAxisPresetTypeRejection(preset, AxisDistTypeJsonName(requested));
    return result;
  }
  result.accepted = true;
  result.message.clear();
  return result;
}

AxisPresetClampResult ClampAxisPresetZenithStdForSave(AxisPreset preset, float raw_value) {
  AxisPresetClampResult result;

  const AxisPresetEntry& entry = AxisPresetEntryFor(preset);
  // Both halves tested, not just the predicate: override_json_name is what a write indexes the
  // document with, and a nullptr there is not a refusal but a crash. The static_assert in
  // axis_presets.hpp makes the two agree, so this second clause is unreachable today — it is here
  // so that "refuses cleanly" does not depend on that assert still being in place.
  if (!entry.has_adjustable_zenith_std || entry.override_json_name == nullptr) {
    // Second of two defenses (the UI draws no input for these). A warning rather than an assert:
    // an assert is compiled out of the release build, which is the build the requirement — that
    // an unadjustable preset never reaches the override file — is actually about.
    GUI_LOG_WARNING("[GUI] User defaults: preset '{}' has no adjustable zenith std; nothing was saved", entry.label);
    result.message = std::string(entry.label) + " has no adjustable value, so nothing was saved.";
    return result;
  }
  if (!std::isfinite(raw_value)) {
    GUI_LOG_WARNING("[GUI] User defaults: preset '{}' zenith std is not finite; nothing was saved", entry.label);
    result.message = "That value is not a number, so nothing was saved.";
    return result;
  }

  float stored = raw_value;
  result.clamped = ClampZenithStdToPresetDomain(preset, stored);
  result.accepted = true;
  result.stored_value = stored;
  result.message = result.clamped ? DescribeAxisPresetClamp(preset, raw_value, stored) : std::string();
  return result;
}

std::optional<float> ReadAxisPresetZenithStdFromDoc(const nlohmann::json& doc, AxisPreset preset) {
  const AxisPresetEntry& entry = AxisPresetEntryFor(preset);
  if (!entry.has_adjustable_zenith_std || entry.override_json_name == nullptr || !doc.is_object()) {
    return std::nullopt;
  }
  // find() rather than the operator[] chain: the document is user-editable, and operator[] on a
  // non-object throws a type_error that would take the caller down over a hand-edit.
  const auto presets = doc.find("presets");
  if (presets == doc.end() || !presets->is_object()) {
    return std::nullopt;
  }
  const auto axis = presets->find("axis");
  if (axis == presets->end() || !axis->is_object()) {
    return std::nullopt;
  }
  const auto node = axis->find(entry.override_json_name);
  if (node == axis->end() || !node->is_object()) {
    return std::nullopt;
  }
  const auto value = node->find("zenith_std");
  if (value == node->end() || !value->is_number()) {
    return std::nullopt;
  }
  const float stored = value->get<float>();
  if (!std::isfinite(stored)) {
    return std::nullopt;
  }
  return stored;
}

void WriteAxisPresetZenithStdToDoc(nlohmann::json& doc, AxisPreset preset, float stored_value) {
  const AxisPresetEntry& entry = AxisPresetEntryFor(preset);
  if (!entry.has_adjustable_zenith_std || entry.override_json_name == nullptr) {
    return;
  }
  if (!doc.is_object()) {
    doc = nlohmann::json::object();
  }
  // Surgical: ONE key is touched. The GuiState half of the document and every other preset survive
  // by construction rather than by each caller remembering to preserve them.
  doc["presets"]["axis"][entry.override_json_name]["zenith_std"] = stored_value;
}

std::optional<AxisDistType> ReadAxisPresetZenithTypeFromDoc(const nlohmann::json& doc, AxisPreset preset) {
  const AxisPresetEntry& entry = AxisPresetEntryFor(preset);
  if (!entry.has_adjustable_zenith_std || entry.override_json_name == nullptr || !doc.is_object()) {
    return std::nullopt;
  }
  // find() throughout, for the same reason as ReadAxisPresetZenithStdFromDoc: the document is
  // user-editable and operator[] on a non-object throws.
  const auto presets = doc.find("presets");
  if (presets == doc.end() || !presets->is_object()) {
    return std::nullopt;
  }
  const auto axis = presets->find("axis");
  if (axis == presets->end() || !axis->is_object()) {
    return std::nullopt;
  }
  const auto node = axis->find(entry.override_json_name);
  if (node == axis->end() || !node->is_object()) {
    return std::nullopt;
  }
  const auto value = node->find("zenith_type");
  if (value == node->end() || !value->is_string()) {
    return std::nullopt;
  }
  // RAW in the same sense as the std reader: a recognised spelling is returned whether or not
  // this preset accepts it — that judgement is ValidateAxisPresetZenithTypeForSave's. A spelling
  // the shared table does not know has no AxisDistType to return and reads as absent.
  return AxisDistTypeFromJsonName(value->get<std::string>());
}

void WriteAxisPresetZenithTypeToDoc(nlohmann::json& doc, AxisPreset preset, AxisDistType stored_type) {
  const AxisPresetEntry& entry = AxisPresetEntryFor(preset);
  if (!entry.has_adjustable_zenith_std || entry.override_json_name == nullptr) {
    return;
  }
  if (static_cast<int>(stored_type) < 0 || stored_type >= AxisDistType::kCount) {
    return;  // kCount has no spelling; the validator refuses it before any caller gets here
  }
  if (!doc.is_object()) {
    doc = nlohmann::json::object();
  }
  // Surgical, like the std writer: ONE key, spelled with the table the .lmc writer spells its
  // axis types with (AxisDistTypeJsonName), so the two documents cannot disagree on a name.
  doc["presets"]["axis"][entry.override_json_name]["zenith_type"] = AxisDistTypeJsonName(stored_type);
}

std::vector<WedgeMillerTriple> ReadWedgePresetsFromDoc(const nlohmann::json& doc) {
  std::vector<WedgeMillerTriple> result;
  if (!doc.is_object()) {
    return result;
  }
  // find() rather than the operator[] chain, for the same reason as ReadAxisPresetZenithStdFromDoc:
  // the document is user-editable, and operator[] on a non-object throws a type_error that would
  // take the caller down over a hand-edit.
  const auto presets = doc.find("presets");
  if (presets == doc.end() || !presets->is_object()) {
    return result;
  }
  const auto wedge = presets->find(kWedgePresetsKey);
  if (wedge == presets->end() || !wedge->is_array()) {
    return result;
  }
  for (const nlohmann::json& node : *wedge) {
    if (!node.is_object()) {
      continue;  // shape-only rejection; see this function's declaration for why it stays silent
    }
    WedgeMillerTriple triple;
    if (!ReadWedgeIndexField(node, kWedgeIndexKeyH, triple.h) ||
        !ReadWedgeIndexField(node, kWedgeIndexKeyK, triple.k) ||
        !ReadWedgeIndexField(node, kWedgeIndexKeyL, triple.l)) {
      continue;
    }
    result.push_back(triple);
  }
  return result;
}

namespace {

// Drop presets.wedge and prune the parent it empties. Shared by the writer's empty-list path and by
// EraseWedgePresetsFromDoc so "cleared the list" and "erased the key" cannot leave two different
// documents behind.
void ErasePresetsWedgeSubtree(nlohmann::json& doc) {
  const auto presets_it = doc.find("presets");
  if (presets_it == doc.end() || !presets_it->is_object()) {
    return;
  }
  presets_it->erase(kWedgePresetsKey);
  if (presets_it->empty()) {
    doc.erase("presets");
  }
}

}  // namespace

void WriteWedgePresetsToDoc(nlohmann::json& doc, const std::vector<WedgeMillerTriple>& presets) {
  // Same normalization the axis writer owes its callers: a malformed top-level document must become
  // a valid empty object rather than being written back to disk unchanged.
  if (!doc.is_object()) {
    doc = nlohmann::json::object();
  }
  if (presets.empty()) {
    // An empty list is the absence of the key, not a `[]` under it. Writing the empty array would
    // leave a skeleton behind for anyone who opens the file by hand, and would make "the user
    // cleared their list" and "this file predates the feature" two states the reader has to tell
    // apart for no benefit.
    ErasePresetsWedgeSubtree(doc);
    return;
  }
  nlohmann::json array = nlohmann::json::array();
  for (const WedgeMillerTriple& triple : presets) {
    array.push_back(nlohmann::json{
        { kWedgeIndexKeyH, triple.h },
        { kWedgeIndexKeyK, triple.k },
        { kWedgeIndexKeyL, triple.l },
    });
  }
  // Surgical in the sense that matters: ONE key under `presets` is replaced. presets.axis, the app
  // preferences and the GuiState half of the document survive by construction.
  doc["presets"][kWedgePresetsKey] = std::move(array);
}

void EraseWedgePresetsFromDoc(nlohmann::json& doc) {
  if (!doc.is_object()) {
    doc = nlohmann::json::object();
  }
  ErasePresetsWedgeSubtree(doc);
}

const std::vector<WedgeMillerTriple>& GetUserWedgePresets() {
  return g_user_wedge_presets;
}

void AdoptWedgePresetOverridesInMemory(std::vector<WedgeMillerTriple> presets) {
  // Whole-list assignment, never "clear then refill". Same discipline as
  // AdoptAxisPresetZenithStdOverrideInMemory, which this scrum's predecessor spent three
  // code-review rounds arriving at.
  g_user_wedge_presets = std::move(presets);
}

void ResetUserWedgePresets() {
  g_user_wedge_presets.clear();
}

// The one eraser both faces call, keyed by which of the preset node's two keys to drop. Each face
// erases ONLY its own key — Restore to factory calls both, one after the other — and the node is
// pruned when its last key goes, then `axis`, then `presets`, so the two faces never leave each
// other a skeleton and never take each other's value away.
namespace {
void EraseAxisPresetZenithKeyFromDoc(nlohmann::json& doc, AxisPreset preset, const char* key) {
  const AxisPresetEntry& entry = AxisPresetEntryFor(preset);
  if (!entry.has_adjustable_zenith_std || entry.override_json_name == nullptr) {
    return;
  }
  // Same normalization as WriteAxisPresetZenithStdToDoc: a malformed top-level document (hand-edited,
  // truncated, or a bare null/array) must become a valid empty object rather than being written back
  // to disk unchanged by the caller.
  //
  // Reachability, stated rather than implied: no production caller gets here. Every one of them
  // passes the panel's working copy, which is either a document read from the override file or a
  // fresh object, so it is an object by construction. The branch is reached only by the unit test
  // that hands it a json::array() on purpose. It is kept because the normalization is the same
  // shape the axis-preset writer already owes its callers, not because a real path was observed
  // producing a non-object root.
  if (!doc.is_object()) {
    doc = nlohmann::json::object();
  }
  const auto presets_it = doc.find("presets");
  if (presets_it == doc.end() || !presets_it->is_object()) {
    return;
  }
  const auto axis_it = presets_it->find("axis");
  if (axis_it != presets_it->end() && axis_it->is_object()) {
    const auto node_it = axis_it->find(entry.override_json_name);
    if (node_it != axis_it->end()) {
      if (node_it->is_object()) {
        node_it->erase(key);
      }
      // A node that is not an object cannot hold this key and is not this face's to keep: a
      // hand-edit that replaced the node with a bare number is dropped along with the override.
      if (!node_it->is_object() || node_it->empty()) {
        axis_it->erase(entry.override_json_name);
      }
    }
    if (axis_it->empty()) {
      presets_it->erase("axis");
    }
  }
  if (presets_it->empty()) {
    doc.erase("presets");
  }
}
}  // namespace

void EraseAxisPresetZenithStdFromDoc(nlohmann::json& doc, AxisPreset preset) {
  EraseAxisPresetZenithKeyFromDoc(doc, preset, "zenith_std");
}

void EraseAxisPresetZenithTypeFromDoc(nlohmann::json& doc, AxisPreset preset) {
  EraseAxisPresetZenithKeyFromDoc(doc, preset, "zenith_type");
}

// ------------------------------------------------------------------------------------------------
// The `app` root key (namespace 3). See user_defaults.hpp for why it is a third root key rather
// than another GuiState field in the document half.
// ------------------------------------------------------------------------------------------------

namespace {
constexpr const char* kAppRootKey = "app";
constexpr const char* kUseGpuBackendKey = "use_gpu_backend";
constexpr const char* kWorkerCountKey = "worker_count";
constexpr const char* kUiScaleMultiplierKey = "ui_scale_multiplier";
}  // namespace

std::optional<bool> ReadUseGpuBackendFromDoc(const nlohmann::json& doc) {
  if (!doc.is_object()) {
    return std::nullopt;
  }
  // find() rather than the operator[] chain, for the same reason as ReadAxisPresetZenithStdFromDoc:
  // the document is user-editable and operator[] on a non-object throws.
  const auto app = doc.find(kAppRootKey);
  if (app == doc.end() || !app->is_object()) {
    return std::nullopt;
  }
  const auto value = app->find(kUseGpuBackendKey);
  if (value == app->end() || !value->is_boolean()) {
    return std::nullopt;
  }
  return value->get<bool>();
}

void WriteUseGpuBackendToDoc(nlohmann::json& doc, bool value) {
  if (!doc.is_object()) {
    doc = nlohmann::json::object();
  }
  // Surgical: ONE key is touched, so the GuiState half and `presets` survive by construction.
  // operator[] is fine on the write path — it starts from a document this process controls, not
  // from whatever a user may have typed into the file.
  doc[kAppRootKey][kUseGpuBackendKey] = value;
}

void EraseUseGpuBackendFromDoc(nlohmann::json& doc) {
  if (!doc.is_object()) {
    doc = nlohmann::json::object();
  }
  const auto app_it = doc.find(kAppRootKey);
  if (app_it == doc.end() || !app_it->is_object()) {
    return;
  }
  app_it->erase(kUseGpuBackendKey);
  if (app_it->empty()) {
    doc.erase(kAppRootKey);
  }
}

std::optional<int> ReadWorkerCountFromDoc(const nlohmann::json& doc) {
  if (!doc.is_object()) {
    return std::nullopt;
  }
  const auto app = doc.find(kAppRootKey);
  if (app == doc.end() || !app->is_object()) {
    return std::nullopt;
  }
  const auto value = app->find(kWorkerCountKey);
  // is_number_integer() rather than is_number(): a stored 1.5 is not a worker count anyone meant, so
  // it reads as nothing stored instead of being truncated into a number the user never typed. A
  // JSON bool is not a number under either predicate, so `true` is rejected here too.
  if (value == app->end() || !value->is_number_integer()) {
    return std::nullopt;
  }
  // A hand-edited file can carry an integer literal outside int's range (e.g. 99999999999).
  // get<int>() on that is an implementation-defined narrowing conversion — it could come back as a
  // sign-flipped negative or another out-of-range value instead of failing. Read as the widest
  // integer type nlohmann exposes and reject anything that would not round-trip through int.
  const auto wide = value->get<std::int64_t>();
  if (wide < std::numeric_limits<int>::min() || wide > std::numeric_limits<int>::max()) {
    return std::nullopt;
  }
  return static_cast<int>(wide);
}

void WriteWorkerCountToDoc(nlohmann::json& doc, int value) {
  if (!doc.is_object()) {
    doc = nlohmann::json::object();
  }
  doc[kAppRootKey][kWorkerCountKey] = value;
}

void EraseWorkerCountFromDoc(nlohmann::json& doc) {
  if (!doc.is_object()) {
    doc = nlohmann::json::object();
  }
  const auto app_it = doc.find(kAppRootKey);
  if (app_it == doc.end() || !app_it->is_object()) {
    return;
  }
  app_it->erase(kWorkerCountKey);
  if (app_it->empty()) {
    doc.erase(kAppRootKey);
  }
}

std::optional<float> ReadUiScaleMultiplierFromDoc(const nlohmann::json& doc) {
  if (!doc.is_object()) {
    return std::nullopt;
  }
  const auto app = doc.find(kAppRootKey);
  if (app == doc.end() || !app->is_object()) {
    return std::nullopt;
  }
  const auto value = app->find(kUiScaleMultiplierKey);
  // is_number() — integral 1 and 2 are legal spellings of 1.0 and 2.0 — but a bool is not a number
  // under it, so `true` is rejected here like everywhere else in this namespace.
  if (value == app->end() || !value->is_number()) {
    return std::nullopt;
  }
  const auto stored = value->get<double>();
  for (const float allowed : kAllowedUiScaleMultipliers) {
    // A tolerance rather than exact equality: 1.25 round-trips through JSON text exactly, but the
    // file is user-editable and "1.250" or a value typed with a stray digit should still snap to
    // the step it obviously means, while 1.3 must not.
    if (std::fabs(stored - static_cast<double>(allowed)) < 1e-4) {
      return allowed;
    }
  }
  return std::nullopt;
}

void WriteUiScaleMultiplierToDoc(nlohmann::json& doc, float value) {
  if (!doc.is_object()) {
    doc = nlohmann::json::object();
  }
  doc[kAppRootKey][kUiScaleMultiplierKey] = value;
}

void EraseUiScaleMultiplierFromDoc(nlohmann::json& doc) {
  if (!doc.is_object()) {
    doc = nlohmann::json::object();
  }
  const auto app_it = doc.find(kAppRootKey);
  if (app_it == doc.end() || !app_it->is_object()) {
    return;
  }
  app_it->erase(kUiScaleMultiplierKey);
  if (app_it->empty()) {
    doc.erase(kAppRootKey);
  }
}

float LoadUiScaleMultiplierAtStartup(std::optional<std::filesystem::path> override_dir) {
  const std::optional<std::filesystem::path> dir = override_dir ? std::move(override_dir) : GetActiveUserConfigDir();
  if (!dir) {
    return kFactoryUiScaleMultiplier;
  }
  // The second read of the same file in a startup (MakeNewDocumentState reads it again once the
  // GL context exists). An unreadable file is therefore counted twice in g_downgrade_count and
  // warned about twice in the log; the user-facing notice collapses to the one generic sentence
  // either way (SurfaceUserDefaultsDowngrades prints no count), so the cost is one log line.
  return ReadUiScaleMultiplierFromDoc(ReadOverlayJsonIfPresent(*dir)).value_or(kFactoryUiScaleMultiplier);
}

void ApplyAppPreferencesOverride(GuiState& state, const nlohmann::json& doc) {
  if (const std::optional<bool> use_gpu = ReadUseGpuBackendFromDoc(doc); use_gpu.has_value()) {
    state.use_gpu_backend = *use_gpu;
  }
  if (const std::optional<int> workers = ReadWorkerCountFromDoc(doc); workers.has_value()) {
    state.worker_count = *workers;
  }
}

void ResetIneligibleScalarsThenApplyAppOverride(GuiState& state, const nlohmann::json& doc, bool has_dir) {
  // Order is the invariant; see this function's declaration in user_defaults.hpp for why the two
  // calls live in one body and why the reset half is not gated on has_dir.
  ResetIneligibleScalarFields(state);
  if (has_dir) {
    ApplyAppPreferencesOverride(state, doc);
  }
}

void AdoptAxisPresetZenithStdOverrideInMemory(AxisPreset preset, std::optional<float> stored_value) {
  const auto slot = static_cast<std::size_t>(preset);
  if (slot >= kAxisPresetSlotCount) {
    return;
  }
  // Whole-struct assignment, never "clear the fields then refill them". This scrum has already
  // spent three code-review rounds on partial updates to this exact global. The next value is
  // built complete beside the slot — the other face carried over unchanged — and assigned once.
  AxisPresetOverride next = g_axis_overrides.slots[slot];
  next.has_zenith_std = stored_value.has_value();
  next.zenith_std = stored_value.value_or(0.0f);
  g_axis_overrides.slots[slot] = next;
}

void AdoptAxisPresetZenithTypeOverrideInMemory(AxisPreset preset, std::optional<AxisDistType> stored_type) {
  const auto slot = static_cast<std::size_t>(preset);
  if (slot >= kAxisPresetSlotCount) {
    return;
  }
  // Mirrors AdoptAxisPresetZenithStdOverrideInMemory: complete next value, one assignment.
  AxisPresetOverride next = g_axis_overrides.slots[slot];
  next.has_zenith_type = stored_type.has_value();
  next.zenith_type = stored_type.value_or(AxisDistType::kGauss);
  g_axis_overrides.slots[slot] = next;
}

AxisDist EffectiveAxisPresetZenith(const AxisPresetEntry& entry) {
  AxisDist zenith = entry.zenith;
  if (const auto stored = GetUserAxisPresetZenithStdOverride(entry.id)) {
    zenith.std = *stored;
  }
  // Trusted as stored: the cache is only ever filled by ParseAxisPresetOverrides, which refuses
  // an out-of-set type before it gets here, and by the panel's commit, which validates first.
  if (const auto stored = GetUserAxisPresetZenithTypeOverride(entry.id)) {
    zenith.type = *stored;
  }
  return zenith;
}

std::optional<AxisDistType> GetUserAxisPresetZenithTypeOverride(AxisPreset preset) {
  const auto slot = static_cast<std::size_t>(preset);
  if (slot >= kAxisPresetSlotCount || !g_axis_overrides.slots[slot].has_zenith_type) {
    return std::nullopt;
  }
  return g_axis_overrides.slots[slot].zenith_type;
}

std::optional<float> GetUserAxisPresetZenithStdOverride(AxisPreset preset) {
  const auto slot = static_cast<std::size_t>(preset);
  if (slot >= kAxisPresetSlotCount || !g_axis_overrides.slots[slot].has_zenith_std) {
    return std::nullopt;
  }
  return g_axis_overrides.slots[slot].zenith_std;
}

void ResetUserAxisPresetOverrides() {
  g_axis_overrides = AxisPresetOverrides{};
}

bool WriteUserDefaultsFile(const std::filesystem::path& dir, const nlohmann::json& doc) {
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (!std::filesystem::is_directory(dir)) {
    GUI_LOG_WARNING("[GUI] User defaults: cannot create '{}' ({}); nothing was saved", PathToU8(dir), ec.message());
    return false;
  }

  const std::filesystem::path file = dir / kUserDefaultsFileName;
  std::ofstream out(file, std::ios::trunc);
  if (!out.is_open()) {
    GUI_LOG_WARNING("[GUI] User defaults: cannot write '{}'; nothing was saved", PathToU8(file));
    return false;
  }

  // Stamp here, in the single write owner, so every writer is stamped without knowing it exists —
  // and so no caller can choose otherwise. Overwriting any stamp the caller supplied is the point:
  // "this file was written by this build" has no legitimate caller-chosen value. A test that needs
  // a file claiming something else writes it raw, outside this owner (WriteRawOverlay).
  //
  // A non-object `doc` is left exactly as given. The only thing that hands one over is a test
  // staging a deliberately malformed file; stamping it would turn it into a well-formed-but-odd
  // file and quietly retire the case.
  nlohmann::json stamped = doc;
  if (stamped.is_object()) {
    stamped[kUserDefaultsOverlaySchemaVersionKey] = kUserDefaultsOverlaySchemaVersion;
  }
  out << stamped.dump(2) << '\n';
  out.flush();
  if (!out.good()) {
    GUI_LOG_WARNING("[GUI] User defaults: write to '{}' failed; the file may be incomplete", PathToU8(file));
    return false;
  }
  return true;
}

GuiState MakeNewDocumentState(std::optional<std::filesystem::path> override_dir) {
  GuiState state{};

  // An explicit override_dir still outranks everything (tests inject one directly); only the
  // no-arg production path consults the process-wide source installed from argv.
  std::optional<std::filesystem::path> dir = override_dir ? std::move(override_dir) : GetActiveUserConfigDir();
  const nlohmann::json doc = dir ? ReadOverlayJsonIfPresent(*dir) : nlohmann::json{};
  if (dir) {
    ApplyUserDefaultsOverlay(state, doc);
  }
  // Deliberately NOT gated behind the `if (dir)` above (code-review round 4 Major): `state` is a
  // fresh local, so it is zero-residue on every call regardless of `dir`, but g_axis_overrides is
  // a persistent global across MakeNewDocumentState()'s repeated production calls (startup,
  // every DoNew(), every DoOpen() .json import). GetUserConfigDir() can flip from available to
  // unavailable within one process (its own doc comment: the directory can become uncreatable at
  // runtime) — reusing the same `if (dir)` to guard this assignment would leave a prior call's
  // overrides live on exactly that flip, the one case this function exists to degrade from.
  g_axis_overrides = dir ? ParseAxisPresetOverrides(doc) : AxisPresetOverrides{};
  // Same rule, same reason, for the second namespace-2 member: unconditional whole-value assignment
  // rather than a loop gated on `dir`, so a directory that becomes unavailable mid-process cannot
  // leave a prior call's shortcuts live.
  g_user_wedge_presets = dir ? ParseWedgePresetOverrides(doc) : std::vector<WedgeMillerTriple>{};

  // The override file is user-editable, so the read path — not just the write path — has to
  // enforce eligibility. Otherwise "which fields may be defaults" would be advisory metadata
  // that anyone can step around with a text editor. The same call also applies the `app` root
  // key, which is the ONLY channel that may decide those fields — the two are one function
  // because their order is what makes both halves true (see its declaration).
  ResetIneligibleScalarsThenApplyAppOverride(state, doc, dir.has_value());

  // Namespace 4 (collections): a key path into these carries a document-local index, so they
  // are never written as defaults — but clear them anyway so a hand-edited file cannot make a
  // new document start with someone else's crystals/layers/filters/raypath_color. Must track
  // kCollectionFields (user_defaults.hpp) exactly — that list, not this line count, is the
  // single source of truth for which containers are namespace 4.
  state.crystals.clear();
  state.layers.clear();
  state.filters.clear();
  state.raypath_color.clear();

  SeedDefaultDocumentContents(state);
  return state;
}

}  // namespace lumice::gui
