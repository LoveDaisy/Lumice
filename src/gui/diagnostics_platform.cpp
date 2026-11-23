#include "gui/diagnostics_platform.hpp"

#include <cstdio>
#include <cstring>
#include <string>

#include "gui/gl_common.h"

#if defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/types.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winreg.h>  // RegGetValueA — not guaranteed through the lean windows.h
#else
#include <sys/utsname.h>

#include <fstream>
#endif

namespace lumice::gui {
namespace diagnostics {

namespace {

std::optional<std::string> GlString(GLenum name) {
  const GLubyte* value = glGetString(name);
  if (value == nullptr) {
    return std::nullopt;
  }
  return std::string(reinterpret_cast<const char*>(value));
}

}  // namespace

PlatformInfo CollectPlatformInfo() {
  PlatformInfo info;
#if defined(__APPLE__)
  info.os_name = "macOS";

  // Product version ("14.6.1"); absent on very old systems, where it degrades
  // alone rather than taking the report down.
  size_t length = 0;
  if (sysctlbyname("kern.osproductversion", nullptr, &length, nullptr, 0) == 0 && length > 0) {
    std::string version(length, '\0');
    if (sysctlbyname("kern.osproductversion", version.data(), &length, nullptr, 0) == 0) {
      version.resize(std::strlen(version.c_str()));  // sysctl writes a NUL-terminated string
      info.os_version = version;
    } else {
      info.notes.push_back("os_version: kern.osproductversion read failed");
    }
  } else {
    info.notes.push_back("os_version: kern.osproductversion not present on this system");
  }

  char cpu_brand[128] = { 0 };
  size_t cpu_brand_size = sizeof(cpu_brand);
  if (sysctlbyname("machdep.cpu.brand_string", cpu_brand, &cpu_brand_size, nullptr, 0) == 0) {
    info.cpu_model = std::string(cpu_brand);
  } else {
    info.notes.push_back("cpu_model: machdep.cpu.brand_string read failed");
  }

  unsigned long long memsize = 0;
  size_t memsize_size = sizeof(memsize);
  if (sysctlbyname("hw.memsize", &memsize, &memsize_size, nullptr, 0) == 0) {
    info.total_physical_memory_bytes = memsize;
  } else {
    info.notes.push_back("total_physical_memory_bytes: sysctl hw.memsize read failed");
  }
#elif defined(_WIN32)
  info.os_name = "Windows";

  // GetVersionEx lies under a manifest that pins compatibility; RtlGetVersion
  // reports the running OS truthfully. It lives in ntdll and has no
  // import-library stub, hence the GetProcAddress.
  using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
  const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  if (ntdll != nullptr) {
    const auto rtl_get_version = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion"));
    if (rtl_get_version != nullptr) {
      RTL_OSVERSIONINFOW osvi{};
      osvi.dwOSVersionInfoSize = sizeof(osvi);
      if (rtl_get_version(&osvi) == 0) {
        info.os_version = std::to_string(osvi.dwMajorVersion) + "." + std::to_string(osvi.dwMinorVersion) + "." +
                          std::to_string(osvi.dwBuildNumber);
      } else {
        info.notes.push_back("os_version: RtlGetVersion failed");
      }
    } else {
      info.notes.push_back("os_version: RtlGetVersion not found in ntdll");
    }
  } else {
    info.notes.push_back("os_version: ntdll module handle unavailable");
  }

  char cpu_name[256] = { 0 };
  DWORD cpu_name_size = sizeof(cpu_name);
  if (RegGetValueA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", "ProcessorNameString",
                   RRF_RT_REG_SZ, nullptr, cpu_name, &cpu_name_size) == ERROR_SUCCESS) {
    info.cpu_model = std::string(cpu_name);
  } else {
    info.notes.push_back("cpu_model: CentralProcessor registry value read failed");
  }

  MEMORYSTATUSEX memstatus{};
  memstatus.dwLength = sizeof(memstatus);
  if (GlobalMemoryStatusEx(&memstatus) != 0) {
    info.total_physical_memory_bytes = static_cast<unsigned long long>(memstatus.ullTotalPhys);
  } else {
    info.notes.push_back("total_physical_memory_bytes: GlobalMemoryStatusEx failed");
  }
#else
  info.os_name = "Linux";

  utsname uts{};
  if (uname(&uts) == 0) {
    info.os_version = uts.release;
  } else {
    info.notes.push_back("os_version: uname failed");
  }

  // Distro identity from the documented read-only interface; absent on
  // minimal containers, which is a fine outcome for this field.
  {
    std::ifstream os_release("/etc/os-release");
    std::string line;
    while (std::getline(os_release, line)) {
      if (line.rfind("PRETTY_NAME=", 0) == 0) {
        std::string pretty = line.substr(12);
        if (pretty.size() >= 2 && pretty.front() == '"' && pretty.back() == '"') {
          pretty = pretty.substr(1, pretty.size() - 2);
        }
        if (!pretty.empty()) {
          info.os_variant = pretty;
        }
        break;
      }
    }
  }

  {
    // First "model name" line of /proc/cpuinfo — the boot CPU's brand string.
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
      if (line.rfind("model name", 0) == 0) {
        const size_t colon = line.find(':');
        if (colon != std::string::npos && colon + 2 < line.size()) {
          info.cpu_model = line.substr(colon + 2);
        }
        break;
      }
    }
    if (!info.cpu_model.has_value()) {
      info.notes.push_back("cpu_model: no 'model name' line in /proc/cpuinfo");
    }
  }

  {
    std::ifstream meminfo("/proc/meminfo");
    std::string line;
    while (std::getline(meminfo, line)) {
      if (line.rfind("MemTotal:", 0) == 0) {
        unsigned long long kb = 0;
        if (std::sscanf(line.c_str(), "MemTotal: %llu kB", &kb) == 1) {
          info.total_physical_memory_bytes = kb * 1024ULL;
        }
        break;
      }
    }
    if (!info.total_physical_memory_bytes.has_value()) {
      info.notes.push_back("total_physical_memory_bytes: MemTotal read failed");
    }
  }
#endif
  return info;
}

GlInfo CollectGlInfo() {
  GlInfo info;
  if ((info.vendor = GlString(GL_VENDOR)) == std::nullopt) {
    info.notes.push_back("gl_vendor: glString returned null (no bound context?)");
  }
  if ((info.renderer = GlString(GL_RENDERER)) == std::nullopt) {
    info.notes.push_back("gl_renderer: glGetString returned null (no bound context?)");
  }
  if ((info.version = GlString(GL_VERSION)) == std::nullopt) {
    info.notes.push_back("gl_version: glGetString returned null (no bound context?)");
  }
  info.shading_language_version = GlString(GL_SHADING_LANGUAGE_VERSION);

  GLint max_texture_size = 0;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
  if (max_texture_size > 0) {
    info.max_texture_size = max_texture_size;
  } else {
    info.notes.push_back("gl_max_texture_size: query returned a non-positive value");
  }
  return info;
}

}  // namespace diagnostics
}  // namespace lumice::gui
