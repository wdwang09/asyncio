#pragma once

#include <asyncio/handle.h>

namespace asyncio {

struct IoEvent {
  int fd;
  uint32_t event_type;
  HandleInfo handle_info;
};

}  // namespace asyncio
