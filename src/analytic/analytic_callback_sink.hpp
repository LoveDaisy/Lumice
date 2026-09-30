#ifndef LUMICE_ANALYTIC_CALLBACK_SINK_HPP
#define LUMICE_ANALYTIC_CALLBACK_SINK_HPP

#include <spdlog/sinks/base_sink.h>

#include <mutex>
#include <string>

#include "lumice_analytic.h"

namespace lumice::analytic {

// Forwards spdlog messages to the host's LUMICE_ANALYTIC_LogCallback. The same shape as
// lumice::CCallbackSink (src/util/callback_sink.hpp), deliberately not shared with it: that one is
// typed on lumice.h's LUMICE_LogCallback, and this library's header shares no types with lumice.h
// (doc/analytic-api.md section 7). A header of its own only so a unit test can drive this
// class without linking analytic_lib.cpp, whose load-time silencing would mute the test binary.
// callback-sink-template-threshold: if a third such pair of small, type-bound copies appears,
// re-weigh a type-agnostic template shared by all three over another copy (doc/analytic-api.md
// section 6).
class AnalyticCallbackSink : public spdlog::sinks::base_sink<std::mutex> {
 public:
  void SetCallback(LUMICE_ANALYTIC_LogCallback cb) {
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

    // spdlog's numeric levels are the header's enum values; LOG_WARNING arrives as spdlog::err,
    // i.e. LUMICE_ANALYTIC_LOG_WARNING (Logger::ToSpdLevel in util/logger.hpp).
    auto level = static_cast<LUMICE_ANALYTIC_LogLevel>(msg.level);
    std::string name(msg.logger_name.data(), msg.logger_name.size());
    std::string text(formatted.data(), formatted.size());
    if (!text.empty() && text.back() == '\n') {
      text.pop_back();
    }
    callback_(level, name.c_str(), text.c_str());
  }

  void flush_() override {}

 private:
  LUMICE_ANALYTIC_LogCallback callback_ = nullptr;
};

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_CALLBACK_SINK_HPP
