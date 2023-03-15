#pragma once

#include <asyncio/utils/future.h>
#include <asyncio/utils/non_copyable.h>

// std
#include <coroutine>

namespace asyncio {

// Hide details for coroutines in struct Task.
template <concepts::Future TaskT>
struct ScheduledTask : private NonCopyable {
  explicit ScheduledTask(TaskT&& task) : task_(std::forward<TaskT>(task)) {
    // Save task in task_ to avoid a temporary Task (in args) being destructed.

    if (task_.valid() && !task_.done()) {  // standard coroutine is valid
      // In CoHandleManager::schedule(), because Task::promise_type inherit it.
      // from UNSCHEDULED to SCHEDULED, send into ready queue. (Don't run it.)
      task_.std_h_.promise().schedule();
    }
  }

  void cancel() { task_.destroy(); }

  decltype(auto) operator co_await() const& noexcept {
    return task_.operator co_await();
  }

  auto operator co_await() const&& noexcept {
    return task_.operator co_await();
  }

  decltype(auto) get_result() & { return task_.get_result(); }

  decltype(auto) get_result() && { return std::move(task_).get_result(); }

  bool valid() const { return task_.valid(); }
  bool done() const { return task_.done(); }

 private:
  TaskT task_;
};

template <concepts::Future Fut>
ScheduledTask(Fut&&) -> ScheduledTask<Fut>;

template <concepts::Future Fut>
[[nodiscard(
    "Discard(detached) a task will not schedule to run.")]] ScheduledTask<Fut>
create_scheduled_task(Fut&& task) {
  return ScheduledTask{std::forward<Fut>(task)};
}

}  // namespace asyncio
