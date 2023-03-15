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
    get_event_loop().set_handle_will_be_called_soon(*this);
  }
}

void CoHandleManager::set_cancelled() {
  if (state_ == HandleIdAndState::State::SCHEDULED) {
    get_event_loop().set_handle_cancelled(*this);
  }
}

}  // namespace asyncio
