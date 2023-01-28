#pragma once

#include <asyncio/event_loop.h>
#include <asyncio/io/addr_info_guard.h>
#include <asyncio/io/io_event.h>
#include <asyncio/io/stream.h>
#include <asyncio/scheduled_task.h>
#include <asyncio/task.h>

// std
#include <ios>
#include <list>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

// sys
#include <netdb.h>
#include <sys/socket.h>

namespace asyncio {

namespace concepts {

template <typename STREAM_HANDLER>
concept StreamHandler = requires(STREAM_HANDLER cb) {
                          { cb(std::declval<Stream>()) } -> concepts::Awaitable;
                        };

}

constexpr static size_t kMaxConnectCount = 16;

// Use start_server() to create Server.
template <concepts::StreamHandler STREAM_HANDLER>
struct Server : NonCopyable {
  Server(STREAM_HANDLER cb, int fd) : stream_handler_(cb), fd_(fd) {}
  Server(Server&& other) noexcept
      : stream_handler_(other.stream_handler_),
        fd_(std::exchange(other.fd_, -1)) {}
  ~Server() { close(); }

  Task<void> serve_forever() {
    IoEvent epoll_in_ev{.fd = fd_, .events = EPOLLIN};
    std::list<ScheduledTask<Task<>>> connected;
    while (true) {
      co_await get_event_loop().wait_io_event(epoll_in_ev);
      sockaddr_storage remote_addr{};
      socklen_t addr_len = sizeof remote_addr;
      /// https://man7.org/linux/man-pages/man2/accept.2.html
      /// accept a connection on a socket
      int client_fd =
          ::accept(fd_, reinterpret_cast<sockaddr*>(&remote_addr), &addr_len);
      if (client_fd == -1) {
        continue;
      }
      connected.emplace_back(create_scheduled_task(
          stream_handler_(Stream(client_fd, remote_addr))));
      clean_up_connected(connected);
    }
  }

 private:
  void clean_up_connected(std::list<ScheduledTask<Task<>>>& connected) {
    if (connected.size() < 100) [[likely]] {
      return;
    }
    for (auto iter = connected.begin(); iter != connected.end();) {
      if (iter->done()) {
        iter = connected.erase(iter);
      } else {
        ++iter;
      }
    }
  }

  void close() {
    if (fd_ > 0) {
      ::close(fd_);
    }
    fd_ = -1;
  }

 private:
  /// https://en.cppreference.com/w/cpp/language/attributes/no_unique_address
  [[no_unique_address]] STREAM_HANDLER stream_handler_;
  int fd_ = -1;
};

template <concepts::StreamHandler STREAM_HANDLER>
Task<Server<STREAM_HANDLER>> start_server(STREAM_HANDLER cb,
                                          std::string_view ip, uint16_t port) {
  /// https://man7.org/linux/man-pages/man3/getaddrinfo.3.html
  /// int getaddrinfo(const char *restrict node,
  ///                       const char *restrict service,
  ///                       const struct addrinfo *restrict hints,
  ///                       struct addrinfo **restrict res);
  /// Given node and service, which identify an Internet host and a service,
  /// getaddrinfo() returns one or more addrinfo structures, each of which
  /// contains an Internet address that can be specified in a call to bind(2)
  /// or connect(2).
  ///
  /// ai_family
  ///   This field specifies the desired address family for the
  ///   returned addresses.  Valid values for this field include
  ///   AF_INET and AF_INET6.  The value AF_UNSPEC indicates that
  ///   getaddrinfo() should return socket addresses for any
  ///   address family (either IPv4 or IPv6, for example) that can
  ///   be used with node and service.
  ///
  /// ai_socktype
  ///   This field specifies the preferred socket type, for
  ///   example SOCK_STREAM or SOCK_DGRAM.  Specifying 0 in this
  ///   field indicates that socket addresses of any type can be
  ///   returned by getaddrinfo().
  addrinfo hints{.ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM};
  addrinfo* server_info = nullptr;
  std::string service_port = std::to_string(port);
  // getaddrinfo() returns 0 if it succeeds
  if (getaddrinfo(ip.data(), service_port.c_str(), &hints, &server_info) != 0) {
    throw std::system_error(
        std::make_error_code(std::errc::address_not_available));
  }
  AddrInfoGuard _guard(server_info);

  int server_fd = -1;
  for (auto p = server_info; p != nullptr; p = p->ai_next) {  // linklist
    /// https://man7.org/linux/man-pages/man2/socket.2.html
    /// socket() creates an endpoint for communication and returns a file
    /// descriptor that refers to that endpoint.
    if ((server_fd = socket(p->ai_family, p->ai_socktype | SOCK_NONBLOCK,
                            p->ai_protocol)) == -1) {
      continue;
    }
    /// https://man7.org/linux/man-pages/man2/setsockopt.2.html
    /// https://man7.org/linux/man-pages/man7/socket.7.html
    /// getsockopt() and setsockopt() manipulate options for the socket
    /// referred to by the file descriptor sockfd. To manipulate options at
    /// the sockets API level, level is specified as SOL_SOCKET.
    ///
    /// SO_REUSEADDR indicates that the rules used in validating addresses
    /// supplied in a bind(2) call should allow reuse of local addresses.
    /// For AF_INET sockets this means that a socket may bind, except when
    /// there is an active listening socket bound to the address. When the
    /// listening socket is bound to INADDR_ANY with a specific port then it
    /// is not possible to bind to this port for any local address.
    int yes = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    /// https://man7.org/linux/man-pages/man2/bind.2.html
    /// When a socket is created with socket(2), it exists in a name space
    /// (address family) but has no address assigned to it.  bind() assigns the
    /// address specified by addr to the socket referred to by the file
    /// descriptor sockfd.
    /// addr -> fd
    if (bind(server_fd, p->ai_addr, p->ai_addrlen) == 0) {
      // Success
      break;
    } else {
      // Unsuccessful
      close(server_fd);
      server_fd = -1;
    }
  }

  if (server_fd == -1) {
    throw std::system_error(
        std::make_error_code(std::errc::address_not_available));
  }

  /// https://man7.org/linux/man-pages/man2/listen.2.html
  /// listen() marks the socket referred to by sockfd as a passive socket, that
  /// is, as a socket that will be used to accept incoming connection requests
  /// using accept(2).
  if (listen(server_fd, kMaxConnectCount) == -1) {
    throw std::system_error(
        std::make_error_code(static_cast<std::errc>(errno)));
  }

  co_return Server(cb, server_fd);
}

}  // namespace asyncio
