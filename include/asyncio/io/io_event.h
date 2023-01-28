#pragma once

#include <asyncio/handle.h>

namespace asyncio {

struct IoEvent {
  int fd;
  uint32_t events;
  HandleInfo handle_info;
};

}  // namespace asyncio
