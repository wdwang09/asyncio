#pragma once

#include <asyncio/handle.h>

namespace asyncio {

// struct HandleInfo {
//   HandleId id;
//   HandleIdAndState* handle;
// };

struct IoEvent {
  int fd;
  uint32_t event_type;  // EPOLLIN EPOLLOUT
  HandleInfo handle_info;
};

}  // namespace asyncio
