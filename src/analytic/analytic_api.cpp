#include <memory>

#include "analytic/analytic_callback_sink.hpp"
#include "lumice_analytic.h"
#include "util/logger.hpp"

namespace {

// This library's own sink singleton — a separate object from liblumice's GetCallbackSink(), as
// every static here is (each shared library carries its own copy of the engine).
std::shared_ptr<lumice::analytic::AnalyticCallbackSink>& GetAnalyticCallbackSink() {
  static auto sink = std::make_shared<lumice::analytic::AnalyticCallbackSink>();
  return sink;
}

// Removes the default console sink from this library's copy of GetSharedSink(). Nothing references
// this object, and that is the point: its constructor is the one place the library goes silent, and
// deleting it makes every engine warning (crystal.cpp, geo3d_closedform.cpp, ...) print to the
// host's stderr again. It runs during dynamic initialisation of the library, which completes before
// dlopen / LoadLibrary returns, so no LUMICE_ANALYTIC_* call can precede it; the linker keeps it
// because initialiser tables are GC roots under dead-code stripping. On Windows it runs inside
// DllMain under the loader lock; it only allocates and edits a sink list, which is safe there. The
// engine logs nothing at static-initialisation time, so no message can slip out before it runs.
struct SilenceDefaultConsoleSink {
  SilenceDefaultConsoleSink() { lumice::GetSharedSink()->remove_sink(lumice::GetDefaultConsoleSink()); }
};
const SilenceDefaultConsoleSink kSilenceDefaultConsoleSink;

}  // namespace

extern "C" {

int LUMICE_ANALYTIC_GetApiVersion(void) {
  return LUMICE_ANALYTIC_API_VERSION;
}


void LUMICE_ANALYTIC_SetLogCallback(LUMICE_ANALYTIC_LogCallback callback) {
  auto& sink = GetAnalyticCallbackSink();
  sink->SetCallback(callback);

  // Attach the sink on first call. A sink added after Logger::set_formatter does not inherit the
  // logger's formatter, so it gets the engine's pattern here.
  static const bool kRegistered = [&sink] {
    sink->set_formatter(lumice::CreateLumiceFormatter(lumice::kLogPattern));
    lumice::GetSharedSink()->add_sink(sink);
    return true;
  }();
  (void)kRegistered;
}

}  // extern "C"
