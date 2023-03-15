#include <asyncio/asyncio.h>

// std
#include <iostream>

using namespace std::chrono_literals;

asyncio::Task<std::string> hello() {
  co_await asyncio::dump_callstack();
  co_return "hello";
}

asyncio::Task<std::string> hello_world() {
  co_return (co_await hello() + " world");
}

asyncio::Task<void> tick(int& count) {
  while (++count) {
    co_await asyncio::sleep(10ms);
    if (count == 10) {
      break;
    }
  }
}

asyncio::Task<void> wait_for_test(decltype(1ms) timeout) {
  int count = 0;
  try {
    co_await asyncio::wait_for(tick(count), timeout);
  } catch (asyncio::TimeoutError&) {
    std::cout << "TimeoutError, count: " << count << std::endl;
  }
}

int main() {
  asyncio::run(wait_for_test(10ms));
  asyncio::run(wait_for_test(200ms));
  std::cout << "Sleeping..." << std::endl;
  asyncio::run(asyncio::sleep(500ms));
  std::cout << "Wake up!" << std::endl;
  std::cout << asyncio::run(hello_world()) << std::endl;
  return 0;
}
