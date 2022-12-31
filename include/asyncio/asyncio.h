#pragma once

#include <asyncio/event_loop.h>
#include <asyncio/scheduled_task.h>
#include <asyncio/task.h>
#include <asyncio/utils/dump_callstack.h>
#include <asyncio/utils/future.h>

// std
#include <type_traits>

namespace asyncio {

template <concepts::Future Fut>
decltype(auto) run(Fut&& main_task) {
  auto t = create_scheduled_task(std::forward<Fut>(main_task));
  // t is in EventLoop's ready queue.
  get_event_loop().run_until_complete();
  if constexpr (std::is_lvalue_reference_v<Fut&&>) {
    return t.get_result();
  } else {
    return std::move(t).get_result();
  }
}

}  // namespace asyncio
