#ifndef LUMICE_GUI_DIAGNOSTICS_PLATFORM_HPP
#define LUMICE_GUI_DIAGNOSTICS_PLATFORM_HPP

#include <optional>
#include <string>
#include <vector>

namespace lumice::gui {
namespace diagnostics {

// Export-time environment readings. Every field is independently optional:
// a platform source that fails degrades that one field (with a note naming
// it) and never takes the report down (plan §3.3 / AC2). Collectors never
// guess, never fill a placeholder, and never shell out — system APIs and
// documented read-only sources only.
struct PlatformInfo {
  std::optional<std::string> os_name;     // "macOS" / "Windows" / "Linux"
  std::optional<std::string> os_version;  // product version / build / kernel release
  std::optional<std::string> os_variant;  // distro identity, when the OS exposes one
  std::optional<std::string> cpu_model;
  std::optional<unsigned long long> total_physical_memory_bytes;
  // Free-text per-field failure reasons, prefixed by the field they explain
  // ("total_physical_memory_bytes: sysctl hw.memsize failed"). Serialized
  // verbatim alongside the fields.
  std::vector<std::string> notes;
};

// The live GL context's identity, read while the exporting thread owns it.
// These strings describe the DISPLAY adapter — never the simulation device
// (the route lives in SimRouteInfo); the report says so beside them.
struct GlInfo {
  std::optional<std::string> vendor;
  std::optional<std::string> renderer;
  std::optional<std::string> version;  // typically embeds the driver build where the driver exposes one
  std::optional<std::string> shading_language_version;
  std::optional<int> max_texture_size;
  std::vector<std::string> notes;
};

// OS / CPU / RAM readings. Callable from any thread with a GL context absent;
// no GL calls inside.
PlatformInfo CollectPlatformInfo();

// GL identity of the CURRENT context. Must be called on the main thread with
// a bound context; queries used are OpenGL 3.3-core legal, so they neither
// raise nor clear the error flag (the upload chain's recorded errors are not
// touched by an export).
GlInfo CollectGlInfo();

}  // namespace diagnostics
}  // namespace lumice::gui

#endif  // LUMICE_GUI_DIAGNOSTICS_PLATFORM_HPP
