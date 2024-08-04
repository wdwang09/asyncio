#pragma once

#include <asyncio/event_loop.h>
#include <asyncio/task.h>
#include <asyncio/utils/non_copyable.h>

// std
#include <chrono>

namespace asyncio {

namespace detail {

template <typename Rep, typename Period>
struct SleepAwaiter : private NonCopyable {
  explicit SleepAwaiter(std::chrono::duration<Rep, Period> delay)
      : delay_(delay) {}

  constexpr bool await_ready() noexcept { return false; }

  constexpr void await_resume() const noexcept {}

  template <typename Promise>
  void await_suspend(std::coroutine_handle<Promise> caller) const noexcept {
    // push caller into schedule_pq_
    get_event_loop().call_later(delay_, caller.promise());
  }

 private:
  std::chrono::duration<Rep, Period> delay_;
};

template <typename Rep, typename Period>
Task<> sleep(ResumeAtInitialSuspend, std::chrono::duration<Rep, Period> delay) {
  co_await detail::SleepAwaiter{delay};
}

}  // namespace detail

template <typename Rep, typename Period>
[[nodiscard("should use co_await")]] Task<> sleep(
    std::chrono::duration<Rep, Period> delay) {
  // Delay parent task.
  return detail::sleep(ResumeAtInitialSuspend{}, delay);
}

}  // namespace asyncio
