#ifndef LUMICE_UTIL_CALLBACK_SINK_HPP
#define LUMICE_UTIL_CALLBACK_SINK_HPP

#include <spdlog/sinks/base_sink.h>

#include <mutex>
#include <string>

#include "include/lumice_base.h"

namespace lumice {

// spdlog sink that forwards log messages to a C callback function.
// Used by LUMICE_SetLogCallback() to let external consumers receive Core logs.
// src/analytic/analytic_callback_sink.hpp is a second copy of this class typed on
// lumice_analytic.h's own enum (the two headers share no type, doc/analytic-api.md section 6/7).
// callback-sink-template-threshold: if a third such pair appears, re-weigh a type-agnostic
// template shared by all three over another copy (doc/analytic-api.md section 6).
class CCallbackSink : public spdlog::sinks::base_sink<std::mutex> {
 public:
  using Callback = LUMICE_LogCallback;

  void SetCallback(Callback cb) {
    std::lock_guard<std::mutex> lock(this->mutex_);
    callback_ = cb;
  }

 protected:
  void sink_it_(const spdlog::details::log_msg& msg) override {
    if (!callback_) {
      return;
    }
    spdlog::memory_buf_t formatted;
    this->formatter_->format(msg, formatted);

    auto level = static_cast<LUMICE_LogLevel>(msg.level);
    std::string name(msg.logger_name.data(), msg.logger_name.size());
    std::string text(formatted.data(), formatted.size());

    // Remove trailing newline if present (spdlog default pattern adds one)
    if (!text.empty() && text.back() == '\n') {
      text.pop_back();
    }

    callback_(level, name.c_str(), text.c_str());
  }

  void flush_() override {}

 private:
  Callback callback_ = nullptr;
};

// Global callback sink singleton. Created on first access, added to GetSharedSink().
inline std::shared_ptr<CCallbackSink>& GetCallbackSink() {
  static auto sink = std::make_shared<CCallbackSink>();
  return sink;
}

}  // namespace lumice

#endif  // LUMICE_UTIL_CALLBACK_SINK_HPP
