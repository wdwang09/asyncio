#pragma once

#include <asyncio/event_loop.h>
#include <asyncio/exception.h>
#include <asyncio/result.h>
#include <asyncio/scheduled_task.h>
#include <asyncio/task.h>
#include <asyncio/utils/awaitable.h>
#include <asyncio/utils/future.h>

// std
#include <chrono>

namespace asyncio {

namespace detail {

template <typename R, typename Duration>
struct WaitForAwaiter : NonCopyable {
  template <concepts::Awaitable Fut>
  WaitForAwaiter(Fut&& task, Duration timeout)
      : timeout_handle_(this, timeout),
        wait_for_task_(
            create_scheduled_task(wait_for_task({}, std::forward<Fut>(task)))) {
  }

  constexpr bool await_ready() noexcept { return result_.has_value(); }

  // result_ has value
  constexpr decltype(auto) await_resume() {
    return std::move(result_).result();
  }

  // result_ doesn't have value
  template <typename Promise>
  void await_suspend(std::coroutine_handle<Promise> parent) noexcept {
    parent_co_manager_ptr_ = &(parent.promise());
    // set parent_co_manager_ptr_ to SUSPEND
    parent_co_manager_ptr_->set_state(HandleIdAndState::State::SUSPEND);
  }

 private:
  template <concepts::Awaitable Fut>
  Task<> wait_for_task(NoWaitAtInitialSuspend, Fut&& task) {
    try {
      if constexpr (std::is_void_v<R>) {
        co_await std::forward<Fut>(task);
      } else {
        result_.set_value(co_await std::forward<Fut>(task));
      }
    } catch (...) {
      result_.unhandled_exception();
    }
    // If done, cancel the timeout event.
    get_event_loop().set_handle_cancelled(timeout_handle_);
    // If done, continue parent coroutine
    if (parent_co_manager_ptr_) {
      get_event_loop().set_handle_will_be_called_soon(*parent_co_manager_ptr_);
    }
  }

 private:
  Result<R> result_;
  CoHandleManager* parent_co_manager_ptr_ = nullptr;

 private:
  struct TimeoutHandleManager : HandleIdAndState {
    TimeoutHandleManager(WaitForAwaiter* awaiter_ptr, Duration timeout)
        : awaiter_ptr_(awaiter_ptr) {
      // As a delay event into event loop.
      get_event_loop().call_later(timeout, *this);
      // If timeout, cancel the task and set error
    }

    void run() final {  // timeout
      awaiter_ptr_->wait_for_task_.cancel();
      awaiter_ptr_->result_.set_exception(
          std::make_exception_ptr(TimeoutError{}));

      // If timeout, continue parent coroutine
      get_event_loop().set_handle_will_be_called_soon(
          *(awaiter_ptr_->parent_co_manager_ptr_));
    }

    WaitForAwaiter* awaiter_ptr_;
  };

  TimeoutHandleManager timeout_handle_;
  ScheduledTask<Task<>> wait_for_task_;
};

template <concepts::Awaitable Fut, typename Duration>
WaitForAwaiter(Fut&&, Duration) -> WaitForAwaiter<AwaitResult<Fut>, Duration>;

template <concepts::Awaitable Fut, typename Duration>
struct WaitForAwaiterRegistry {
  WaitForAwaiterRegistry(Fut&& task, Duration duration)
      : task_(std::forward<Fut>(task)), duration_(duration) {}

  auto operator co_await() && {
    return WaitForAwaiter(std::forward<Fut>(task_), duration_);
  }

 private:
  Fut task_;
  Duration duration_;
};

template <concepts::Awaitable Fut, typename Duration>
WaitForAwaiterRegistry(Fut&&, Duration)
    -> WaitForAwaiterRegistry<Fut, Duration>;

template <concepts::Awaitable Fut, typename Rep, typename Period>
auto wait_for(NoWaitAtInitialSuspend, Fut&& task,
              std::chrono::duration<Rep, Period> timeout)
    -> Task<AwaitResult<Fut>> {
  co_return co_await WaitForAwaiterRegistry{std::forward<Fut>(task), timeout};
}

}  // namespace detail

template <concepts::Awaitable Fut, typename Rep, typename Period>
[[nodiscard("should use co_await")]] Task<AwaitResult<Fut>> wait_for(
    Fut&& task, std::chrono::duration<Rep, Period> timeout) {
  return detail::wait_for({}, std::forward<Fut>(task), timeout);
}

}  // namespace asyncio
