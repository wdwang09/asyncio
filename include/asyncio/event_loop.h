#pragma once

#include <asyncio/handle.h>
#include <asyncio/utils/non_copyable.h>

#ifndef NO_IO
#include <asyncio/io/selector.h>
#endif

// std
#include <algorithm>
#include <chrono>
#include <coroutine>
#include <optional>
#include <queue>
#include <ranges>
#include <unordered_set>

namespace asyncio {

class EventLoop : private NonCopyable {
  using MSDuration = std::chrono::milliseconds;
  using TimerHandle = std::pair<MSDuration, HandleInfo>;

 public:
  EventLoop() {
    auto now = std::chrono::steady_clock::now();
    // https://en.cppreference.com/w/cpp/chrono/time_point/time_since_epoch
    start_time_ =
        std::chrono::duration_cast<MSDuration>(now.time_since_epoch());
  }

  MSDuration time() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<MSDuration>(now.time_since_epoch()) -
           start_time_;
  }

  // Don't cancel handle immediately.
  void cancel_handle(HandleIdAndState& handle) {
    handle.set_state(HandleIdAndState::State::UNSCHEDULED);
    cancelled_set_.insert(handle.get_handle_id());
  }

  void call_soon(HandleIdAndState& handle) {
    handle.set_state(HandleIdAndState::State::SCHEDULED);
    ready_q_.push(HandleInfo{handle.get_handle_id(), &handle});
  }

  template <typename Rep, typename Period>
  void call_later(std::chrono::duration<Rep, Period> delay,
                  HandleIdAndState& callback) {
    call_at(time() + std::chrono::duration_cast<MSDuration>(delay), callback);
  }

  void run_until_complete() {
    while (!is_stop()) {
      run_once();
    }
  }

#ifndef NO_IO

  struct WaitEventAwaiter {
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    constexpr bool await_ready() const noexcept { return false; }

    template <typename Promise>
    constexpr void await_suspend(
        std::coroutine_handle<Promise> handle) noexcept {
      handle.promise().set_state(HandleIdAndState::State::SUSPEND);
      event_.handle_info = {.id = handle.promise().get_handle_id(),
                            .handle = &handle.promise()};
      selector_.register_event(event_);
    }

    void await_resume() noexcept {}

    ~WaitEventAwaiter() { selector_.remove_event(event_); }

    Selector& selector_;
    IoEvent event_;
  };

  [[nodiscard]] auto wait_io_event(const IoEvent& event) {
    return WaitEventAwaiter{selector_, event};
  }

#endif

 private:
  // deal with ready, scheduled and epoll task
  void run_once() {
    std::optional<MSDuration> timeout;  // As the epoll_wait() argument.
    if (!ready_q_.empty()) {
      // If some task are ready
      timeout.emplace(0);
    } else if (!schedule_pq_.empty()) {
      // No task is ready, but some task are slept.
      auto&& [when, _] = schedule_pq_[0];
      timeout = std::max(when - time(), MSDuration(0));
    }

    check_io_ready(timeout);

    auto end_time = time();
    // Some scheduled task can wake up when epoll_wait() blocks.
    while (!schedule_pq_.empty()) {
      auto&& [when, handle_info] = schedule_pq_[0];
      if (when >= end_time) break;
      ready_q_.push(handle_info);
      // pop_heap() with "greater{}" moves the smallest to the end. No pop.
      std::ranges::pop_heap(schedule_pq_, std::ranges::greater{},
                            &TimerHandle::first);
      schedule_pq_.pop_back();
    }

    for (size_t i = 0, n_ready = ready_q_.size(); i < n_ready; ++i) {
      auto [handle_id, handle_manager] = ready_q_.front();
      ready_q_.pop();
      if (auto iter = cancelled_set_.find(handle_id);
          iter != cancelled_set_.end()) {
        // If handle_id is a cancelled task. Don't run this task and remove it
        // from cancelled task.
        cancelled_set_.erase(iter);
      } else {
        // TODO: When running, the state may be changed. So unschedule it first?
        handle_manager->set_state(HandleIdAndState::State::UNSCHEDULED);
        handle_manager->run();
      }
    }

    // If the first task in scheduled queue has been in cancelled set,
    // cancel it (remove it from schedule queue).
    while (!schedule_pq_.empty()) {
      auto&& [_, handle_info] = schedule_pq_[0];
      if (auto it = cancelled_set_.find(handle_info.id);
          it != cancelled_set_.end()) {
        std::ranges::pop_heap(schedule_pq_, std::ranges::greater{},
                              &TimerHandle::first);
        schedule_pq_.pop_back();
        cancelled_set_.erase(it);
      } else {
        break;
      }
    }
  }

  void check_io_ready(std::optional<MSDuration> timeout) {
#ifndef NO_IO
    // Wait for some selector event with specified timeout.
    // If no ready or scheduled task, wait infinitely until one event is
    // delivered. If timeout is 0, epoll_wait() with return immediately.
    auto event_list =
        selector_.select(timeout.has_value() ? (int)timeout->count() : -1);
    for (auto&& event : event_list) {
      // send selector event into ready queue.
      ready_q_.push(event.handle_info);
    }
#endif
  }

  bool is_stop() {
    bool is_selector_empty = true;
#ifndef NO_IO
    is_selector_empty = selector_.is_stop();
#endif
    return schedule_pq_.empty() && ready_q_.empty() && is_selector_empty;
  }

  template <typename Rep, typename Period>
  void call_at(std::chrono::duration<Rep, Period> when,
               HandleIdAndState& callback) {
    // push the task into schedule queue.
    callback.set_state(HandleIdAndState::State::SCHEDULED);
    schedule_pq_.emplace_back(std::chrono::duration_cast<MSDuration>(when),
                              HandleInfo{callback.get_handle_id(), &callback});
    std::ranges::push_heap(schedule_pq_, std::ranges::greater{},
                           &TimerHandle::first);
  }

 private:
  MSDuration start_time_{};
  std::queue<HandleInfo> ready_q_;
  std::vector<TimerHandle> schedule_pq_;  // priority_queue
  std::unordered_set<HandleId> cancelled_set_;
#ifndef NO_IO
  Selector selector_;
#endif
};

EventLoop& get_event_loop();

}  // namespace asyncio
