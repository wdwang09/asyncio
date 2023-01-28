#include <asyncio/asyncio.h>

// 3rd
#include <fmt/core.h>

// std
#include <chrono>
#include <iostream>
#include <string_view>

asyncio::Task<> tcp_echo_client(int port, std::string_view message) {
  auto stream = co_await asyncio::open_connection("127.0.0.1", port);

  fmt::print("Send '{}' to port {}.\n", message, port);
  co_await stream.write(asyncio::Stream::Buffer(
      message.begin(), message.end() + 1 /* with \0 */));

  auto timeout = std::chrono::milliseconds(300);
  auto data = co_await asyncio::wait_for(stream.read(100), timeout);
  fmt::print("Received: '{}'\n", data.data());

  std::cout << "Close the connection with server." << std::endl;
  stream.close();
}

int main() {
  asyncio::run(tcp_echo_client(8888, "Hello!"));
  return 0;
}
