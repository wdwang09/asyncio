#include <asyncio/asyncio.h>

// std
#include <iostream>

asyncio::Task<std::string> hello() {
  co_await asyncio::dump_callstack();
  co_return "hello";
}

asyncio::Task<std::string> hello_world() {
  co_return (co_await hello() + " world");
}

int main() {
  std::cout << asyncio::run(hello_world()) << std::endl;
  return 0;
}
