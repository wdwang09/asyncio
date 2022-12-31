#pragma once

#include <asyncio/utils/future.h>
#include <asyncio/utils/non_copyable.h>

// std
#include <coroutine>

namespace asyncio {

// TODO: Why not use Task directly? Show less code?
template <concepts::Future Task>
struct ScheduledTask : private NonCopyable {
  // template <concepts::Future Fut>  // Compilation Error in Clang 15.0.6
  explicit ScheduledTask(Task&& task) : task_(std::forward<Task>(task)) {
    // In member initializer list, save task_ to avoid the lifecycle problem.
    if (task_.valid() && !task_.done()) {
      // from UNSCHEDULED to SCHEDULED, send into ready queue
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
  Task task_;
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
