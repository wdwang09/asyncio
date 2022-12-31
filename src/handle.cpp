#include <asyncio/event_loop.h>
#include <asyncio/handle.h>

namespace asyncio {

EventLoop& get_event_loop() {
  static EventLoop loop;
  return loop;
}

HandleId HandleIdAndState::handle_id_generation_ = 0;

void CoHandleManager::schedule() {
  if (state_ == HandleIdAndState::State::UNSCHEDULED) {
    get_event_loop().call_soon(*this);
  }
}

void CoHandleManager::cancel() {
  if (state_ == HandleIdAndState::State::SCHEDULED) {
    get_event_loop().cancel_handle(*this);
  }
}

}  // namespace asyncio
