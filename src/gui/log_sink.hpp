#ifndef LUMICE_GUI_LOG_SINK_HPP
#define LUMICE_GUI_LOG_SINK_HPP

#include <spdlog/sinks/base_sink.h>

#include <deque>
#include <functional>
#include <mutex>
#include <string>

namespace lumice::gui {

struct LogEntry {
  spdlog::level::level_enum level;
  std::string message;
};

// Custom spdlog sink that stores log entries in a ring buffer for ImGui display.
// Thread-safe: spdlog worker threads write via sink_it_(), GUI main thread reads via ForEachEntry().
class ImGuiLogSink : public spdlog::sinks::base_sink<std::mutex> {
 public:
  static constexpr size_t kMaxEntries = 4096;

  // Iterate over all entries under lock. Callback receives (index, entry).
  // This avoids copying the entire deque on every frame.
  void ForEachEntry(const std::function<void(size_t, const LogEntry&)>& callback) {
    std::lock_guard<std::mutex> lock(this->mutex_);
    for (size_t i = 0; i < entries_.size(); i++) {
      callback(i, entries_[i]);
    }
  }

  size_t Size() {
    std::lock_guard<std::mutex> lock(this->mutex_);
    return entries_.size();
  }

  void Clear() {
    std::lock_guard<std::mutex> lock(this->mutex_);
    entries_.clear();
  }

  // Inject an external log entry (e.g., from C API callback).
  // Thread-safe: can be called from any thread.
  void ReceiveExternal(spdlog::level::level_enum level, const char* message) {
    std::lock_guard<std::mutex> lock(this->mutex_);
    Push(level, std::string(message));
  }

 protected:
  void sink_it_(const spdlog::details::log_msg& msg) override {
    spdlog::memory_buf_t formatted;
    this->formatter_->format(msg, formatted);
    Push(msg.level, std::string(formatted.data(), formatted.size()));
  }

  void flush_() override {}

 private:
  // Both writers land here, under the caller's lock. A stored message carries NO line ending:
  // spdlog's formatter appends the platform eol to every line and a C API callback may too, and
  // the one place that would show is the panel's "Copy" — a line joined with '\n' by the panel
  // must not bring its own, or the copied block reads double-spaced.
  void Push(spdlog::level::level_enum level, std::string message) {
    while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) {
      message.pop_back();
    }
    entries_.push_back({ level, std::move(message) });
    if (entries_.size() > kMaxEntries) {
      entries_.pop_front();
    }
  }

  std::deque<LogEntry> entries_;
};

}  // namespace lumice::gui

#endif  // LUMICE_GUI_LOG_SINK_HPP
