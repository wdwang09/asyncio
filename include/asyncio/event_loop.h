#pragma once

#include <asyncio/handle.h>
#include <asyncio/selector.h>
#include <asyncio/utils/non_copyable.h>

// std
#include <algorithm>
#include <chrono>
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

  // Don't cancel handle immediately.
  void cancel_handle(Handle& handle) {
    handle.set_state(Handle::State::UNSCHEDULED);
    cancelled_.insert(handle.get_handle_id());
  }

  void call_soon(Handle& handle) {
    handle.set_state(Handle::State::SCHEDULED);
    ready_.push(HandleInfo{handle.get_handle_id(), &handle});
  }

  template <typename Rep, typename Period>
  void call_later(std::chrono::duration<Rep, Period> delay, Handle& callback) {
    call_at(time() + std::chrono::duration_cast<MSDuration>(delay), callback);
  }

  void run_until_complete(const Event& event) {
    while (!is_stop()) {
      run_once();
    }
  }

 private:
  void run_once() {
    // deal with ready, scheduled and epoll task

    std::optional<MSDuration> timeout;  // As the epoll_wait() argument.
    if (!ready_.empty()) {
      // If some task are ready
      timeout.emplace(0);
    } else if (!schedule_.empty()) {
      // No task is ready, but some task are scheduled.
      auto&& [when, _] = schedule_[0];
      timeout = std::max(when - time(), MSDuration(0));
    }

    // If no ready or scheduled task, wait infinitely until one event is
    // delivered. If timeout is 0, epoll_wait() with return immediately.
    auto event_list =
        selector_.select(timeout.has_value() ? (int)timeout->count() : -1);
    for (auto&& event : event_list) {
      ready_.push(event.handle_info);
    }

    auto end_time = time();
    // Some scheduled task can be run when epoll_wait() blocks.
    while (!schedule_.empty()) {
      auto&& [when, handle_info] = schedule_[0];
      if (when >= end_time) break;
      ready_.push(handle_info);
      // pop_heap() with "greater{}" moves the smallest to the end. No pop.
      std::ranges::pop_heap(schedule_, std::ranges::greater{},
                            &TimerHandle::first);
      schedule_.pop_back();
    }

    for (size_t n_ready = ready_.size(), i = 0; i < n_ready; ++i) {
      auto [handle_id, handle] = ready_.front();
      ready_.pop();
      if (auto iter = cancelled_.find(handle_id); iter != cancelled_.end()) {
        // handle_id is a cancelled task. Don't cancel it?
        cancelled_.erase(iter);
      } else {
        // When running, the state may be changed. So unschedule it first?
        handle->set_state(Handle::State::UNSCHEDULED);
        handle->run();
      }
    }

    cleanup_delayed_call();
  }

  void cleanup_delayed_call() {
    // Remove delayed calls that were cancelled from head of queue.
    while (!schedule_.empty()) {
      auto&& [when, handle_info] = schedule_[0];
      if (auto it = cancelled_.find(handle_info.id); it != cancelled_.end()) {
        // If the first task in schedule queue has been in cancelled set,
        // cancel it (remove it from schedule queue).
        std::ranges::pop_heap(schedule_, std::ranges::greater{},
                              &TimerHandle::first);
        schedule_.pop_back();
        cancelled_.erase(it);
      } else {
        break;
      }
    }
  }

  bool is_stop() {
    return schedule_.empty() && ready_.empty() && selector_.is_stop();
  }

  MSDuration time() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<MSDuration>(now.time_since_epoch()) -
           start_time_;
  }

  template <typename Rep, typename Period>
  void call_at(std::chrono::duration<Rep, Period> when, Handle& callback) {
    // push the task into schedule queue.
    callback.set_state(Handle::State::SCHEDULED);
    schedule_.emplace_back(std::chrono::duration_cast<MSDuration>(when),
                           HandleInfo{callback.get_handle_id(), &callback});
    std::ranges::push_heap(schedule_, std::ranges::greater{},
                           &TimerHandle::first);
  }

 private:
  MSDuration start_time_{};
  Selector selector_;
  std::queue<HandleInfo> ready_;
  std::vector<TimerHandle> schedule_;  // priority_queue
  std::unordered_set<HandleId> cancelled_;
};

EventLoop& get_event_loop();

}  // namespace asyncio
