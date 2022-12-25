#pragma once

namespace asyncio {

struct NonCopyable {
 protected:
  NonCopyable() = default;
  ~NonCopyable() = default;
  NonCopyable(NonCopyable&&) = default;
  NonCopyable& operator=(NonCopyable&&) = default;

 public:
  NonCopyable(const NonCopyable&) = delete;
  NonCopyable& operator=(NonCopyable&) = delete;
};

}  // namespace asyncio
