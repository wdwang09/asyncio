#pragma once

// sys
#include <netdb.h>

namespace asyncio {

// RAII for addrinfo
struct AddrInfoGuard {
  explicit AddrInfoGuard(addrinfo* info) : info_(info) {}
  ~AddrInfoGuard() { freeaddrinfo(info_); }

 private:
  addrinfo* info_ = nullptr;
};

}  // namespace asyncio
