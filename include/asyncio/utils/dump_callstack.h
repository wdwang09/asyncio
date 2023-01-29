#pragma once

// std
#include <coroutine>

namespace asyncio {

namespace detail {

struct CallStackAwaiter {
  // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
  constexpr bool await_ready() noexcept { return false; }

  constexpr void await_resume() const noexcept {}

  template <typename Promise>
  bool await_suspend(std::coroutine_handle<Promise> caller) const noexcept {
    caller.promise().dump_backtrace(0);
    return false;
  }
};

}  // namespace detail

[[nodiscard("should use co_await")]] auto dump_callstack() {
  return detail::CallStackAwaiter{};
}

}  // namespace asyncio
