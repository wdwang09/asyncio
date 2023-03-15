#pragma once

#include <asyncio/event_loop.h>
#include <asyncio/exception.h>
#include <asyncio/handle.h>
#include <asyncio/result.h>
// include "scheduled_task.h" before "task.h" to avoid some problem
#include <asyncio/scheduled_task.h>
#include <asyncio/utils/future.h>
#include <asyncio/utils/non_copyable.h>
#include <asyncio/utils/promise.h>

// 3rd
#include <fmt/core.h>

// std
#include <cassert>
#include <coroutine>
#include <iostream>
#include <utility>

namespace asyncio {

struct NoWaitAtInitialSuspend {};
inline constexpr NoWaitAtInitialSuspend
    no_wait_at_initial_suspend;  // use in "gather.h"

template <typename R = void>
struct Task : private NonCopyable {
  struct promise_type;
  using std_co_handle = std::coroutine_handle<promise_type>;

  template <concepts::Future>
  friend struct ScheduledTask;

  explicit Task(std_co_handle h) noexcept : std_h_(h) {}
  // https://en.cppreference.com/w/cpp/utility/exchange
  Task(Task&& t) noexcept : std_h_(std::exchange(t.std_h_, {})) {}

  ~Task() { destroy(); }

  decltype(auto) get_result() & { return std_h_.promise().result(); }

  decltype(auto) get_result() && {
    return std::move(std_h_.promise()).result();
  }

  // ===== operator co_await begin =====

  struct AwaiterBase {
    std_co_handle sub_coroutine_{};

    // if return true, run await_resume()
    // if return false, suspend itself and run await_suspend()
    constexpr bool await_ready() {
      // A co_await B: B is sub_coroutine_, A is await_suspend's arg
      if (sub_coroutine_) [[likely]] {
        // If B isn't done, suspend B; else resume B.
        return sub_coroutine_.done();
      }
      return true;
    }

    // await_resume() is defined in derived class.

    // await_ready is false (B isn't done)
    template <typename Promise>
    void await_suspend(std::coroutine_handle<Promise> parent) const noexcept {
      // CoHandleManager* parent_co_manager_ptr_;
      assert(!sub_coroutine_.promise().parent_co_manager_ptr_);  // nullptr
      // suspend parent
      parent.promise().set_state(HandleIdAndState::State::SUSPEND);
      // record parent
      sub_coroutine_.promise().parent_co_manager_ptr_ = &(parent.promise());
      // send B into ready queue
      sub_coroutine_.promise().schedule();  // SCHEDULED and into ready queue
    }
  };

  auto operator co_await() const& noexcept {
    struct Awaiter : AwaiterBase {
      // co_await's return value
      decltype(auto) await_resume() const {
        if (!AwaiterBase::sub_coroutine_) [[unlikely]] {
          throw InvalidFuture{};
        }
        return AwaiterBase::sub_coroutine_.promise().result();
      }
    };

    return Awaiter{std_h_};
  }

  auto operator co_await() const&& noexcept {
    struct Awaiter : AwaiterBase {
      decltype(auto) await_resume() const {
        if (!AwaiterBase::sub_coroutine_) [[unlikely]] {
          throw InvalidFuture{};
        }
        return std::move(AwaiterBase::sub_coroutine_.promise()).result();
      }
    };

    return Awaiter{std_h_};
  }

  // ===== operator co_await end =====

  bool valid() const { return bool(std_h_); }

  bool done() const { return std_h_.done(); }

 private:
  void destroy() {
    if (auto std_h = std::exchange(std_h_, nullptr)) {
      // after std::exchange, std_h_'s frame pointer will be nullptr
      std_h.promise().set_cancelled();
      std_h.destroy();
    }
  }

 private:
  std_co_handle std_h_;
};

template <typename R>
struct Task<R>::promise_type : CoHandleManager, Result<R> {
  // CoHandleManager inherit HandleIdAndState;
  // Result has two type:
  // Result<R> (return_value) and Result<void> (return_void)

  promise_type() = default;

  // Read function's args, use NoWaitAtInitialSuspend to
  // determine initial_suspend()'s return value.
  template <typename... Args>  // from free function
  explicit promise_type(NoWaitAtInitialSuspend, Args&&...)
      : wait_at_initial_suspend_{false} {}
  template <typename Obj, typename... Args>  // from member function
  promise_type(Obj&&, NoWaitAtInitialSuspend, Args&&...)
      : wait_at_initial_suspend_{false} {}

  Task get_return_object() noexcept {
    return Task{std_co_handle::from_promise(*this)};
  }

  auto initial_suspend() noexcept {
    struct InitialSuspendAwaiter {
      constexpr bool await_ready() const noexcept {
        // if wait == True: suspend (default value)
        // if wait == False: resume (in wait_for, sleep, which need time)
        return !wait_at_initial_suspend_;
      }
      constexpr void await_suspend(std::coroutine_handle<>) const noexcept {}
      constexpr void await_resume() const noexcept {}

      const bool wait_at_initial_suspend_;
    };
    // If true (default), await_suspend(), don't resume coroutine when created.
    // If false, await_resume(), resume coroutine when created.
    return InitialSuspendAwaiter{wait_at_initial_suspend_};
  }

  struct FinalAwaiter {
    constexpr bool await_ready() const noexcept { return false; }

    // Because of template (which should have parent_co_manager_ptr_), cannot
    // write the struct in final_suspend().

    // Don't return coroutine_handle here because tasks are controlled by event
    // loop.
    template <typename Promise>
    constexpr void await_suspend(
        std::coroutine_handle<Promise> h) const noexcept {
      // h is itself rather than parent (because B co_await final_suspend())
      if (CoHandleManager* parent = h.promise().parent_co_manager_ptr_) {
        // send parent into ready queue
        get_event_loop().set_handle_will_be_called_soon(*parent);
      }
    }
    constexpr void await_resume() const noexcept {}
  };

  auto final_suspend() noexcept { return FinalAwaiter{}; }

  // unhandled_exception() is in Result: If the coroutine ends with an uncaught
  // exception, it catches the exception and calls promise.unhandled_exception()
  // from within the catch-block

  // Using this function to save std::source_location. No other usage.
  // GCC (12.2.1) and Clang (15.0.6) show different behaviors in "loc".
  template <concepts::Awaitable A>
  decltype(auto) await_transform(
      A&& awaiter, std::source_location loc = std::source_location::current()) {
    frame_info_ = loc;
    return std::forward<A>(awaiter);
  }

  // Inherit HandleIdAndState
  void run() final { std_co_handle::from_promise(*this).resume(); }

  const std::source_location& get_frame_info() const final {
    return frame_info_;
  }

  void dump_backtrace(size_t depth) const final {
    std::cout << fmt::format("[{}] {}", depth, frame_name()) << std::endl;
    if (parent_co_manager_ptr_) {
      parent_co_manager_ptr_->dump_backtrace(depth + 1);
    } else {
      std::cout << std::endl;
    }
  }

  const bool wait_at_initial_suspend_{true};
  CoHandleManager* parent_co_manager_ptr_ = nullptr;
  std::source_location frame_info_{};
};

static_assert(concepts::Promise<Task<>::promise_type>);
static_assert(concepts::Future<Task<>>);

}  // namespace asyncio
