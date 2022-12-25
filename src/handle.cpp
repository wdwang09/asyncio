#include <asyncio/event_loop.h>
#include <asyncio/handle.h>

namespace asyncio {

EventLoop& get_event_loop() {
  static EventLoop loop;
  return loop;
}

HandleId Handle::handle_id_generation_ = 0;

void CoroutineHandle::schedule() {
  if (state_ == Handle::State::UNSCHEDULED) {
    get_event_loop().call_soon(*this);
  }
}

void CoroutineHandle::cancel() {
  if (state_ == Handle::State::SCHEDULED) {
    get_event_loop().cancel_handle(*this);
  }
}

}  // namespace asyncio
