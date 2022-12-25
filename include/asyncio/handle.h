#pragma once

// 3rd
#include <fmt/core.h>

// std
#include <cstdint>
#include <source_location>
#include <string>

namespace asyncio {

using HandleId = uint64_t;

class Handle {
 public:
  enum class State : uint8_t { UNSCHEDULED, SUSPEND, SCHEDULED };

  Handle() noexcept : handle_id_(handle_id_generation_++) {}

  virtual void run() = 0;

  void set_state(State state) { state_ = state; }

  HandleId get_handle_id() const { return handle_id_; }

  virtual ~Handle() = default;

 private:
  HandleId handle_id_;
  static HandleId handle_id_generation_;

 protected:
  State state_{State::UNSCHEDULED};
};

class CoroutineHandle : Handle {
 public:
  std::string frame_name() const {
    const auto& frame_info = get_frame_info();
    return fmt::format("{} at {}:{}", frame_info.function_name(),
                       frame_info.file_name(), frame_info.line());
  }

  virtual void dump_backtrace(size_t depth) const {}

  void schedule();
  void cancel();

 private:
  virtual const std::source_location& get_frame_info() const {
    static const std::source_location frame_info =
        std::source_location::current();
    return frame_info;
  }
};

struct HandleInfo {
  HandleId id;
  Handle* handle;
};

}  // namespace asyncio
