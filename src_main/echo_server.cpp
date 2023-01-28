#include <asyncio/asyncio.h>

// 3rd
#include <fmt/core.h>

// std
#include <iostream>
#include <string>

// sys
#include <arpa/inet.h>

asyncio::Task<> handle_echo(asyncio::Stream stream) {
  auto data = co_await stream.read(100);

  auto& sock_info = stream.get_sock_info();
  auto sa = reinterpret_cast<const sockaddr*>(&sock_info);
  char addr[INET6_ADDRSTRLEN]{};
  /// https://man7.org/linux/man-pages/man3/inet_ntop.3.html
  /// inet_ntop - convert IPv4 and IPv6 addresses from binary to text form
  fmt::print("Received: '{}' from '{}:{}'\n", data.data(),
             inet_ntop(sock_info.ss_family, asyncio::get_in_addr(sa), addr,
                       sizeof addr),
             asyncio::get_in_port(sa));
  fmt::print("Send: '{}'\n", data.data());
  co_await stream.write(data);
  std::cout << "Close the connection with client." << std::endl;
  stream.close();
}

asyncio::Task<> start_server(int port) {
  auto server = co_await asyncio::start_server(handle_echo, "127.0.0.1", port);
  std::cout << "Serving on port " << std::to_string(port) << "..." << std::endl;
  co_await server.serve_forever();
}

int main() {
  asyncio::run(start_server(8888));
  return 0;
}
