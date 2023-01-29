#pragma once

#include <asyncio/event_loop.h>
#include <asyncio/io/addr_info_guard.h>
#include <asyncio/io/io_event.h>
#include <asyncio/io/stream.h>
#include <asyncio/task.h>

// std
#include <ios>
#include <string>
#include <string_view>
#include <system_error>

// sys
#include <netdb.h>
#include <sys/socket.h>

namespace asyncio {

namespace detail {

Task<bool> connect(int fd, const sockaddr* addr, socklen_t len) {
  /// https://man7.org/linux/man-pages/man2/connect.2.html
  /// connect - initiate a connection on a socket
  int rc = ::connect(fd, addr, len);
  if (rc == 0) {
    co_return true;
  }
  if (rc < 0 && errno != EINPROGRESS) {
    throw std::system_error(
        std::make_error_code(static_cast<std::errc>(errno)));
  }
  /// https://man7.org/linux/man-pages/man2/epoll_ctl.2.html
  IoEvent epoll_out_ev{.fd = fd, .event_type = EPOLLOUT};

  co_await get_event_loop().wait_io_event(epoll_out_ev);

  int result = 0;
  socklen_t result_len = sizeof result;
  /// https://man7.org/linux/man-pages/man2/getsockopt.2.html
  if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &result, &result_len) < 0) {
    co_return false;
  }

  co_return (result == 0);
}

}  // namespace detail

Task<Stream> open_connection(std::string_view ip, uint16_t port) {
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
  std::string service = std::to_string(port);
  // getaddrinfo() returns 0 if it succeeds
  if (getaddrinfo(ip.data(), service.c_str(), &hints, &server_info) != 0) {
    throw std::system_error(
        std::make_error_code(std::errc::address_not_available));
  }
  AddrInfoGuard _guard(server_info);

  int sock_fd = -1;
  for (auto p = server_info; p != nullptr; p = p->ai_next) {  // linklist
    /// https://man7.org/linux/man-pages/man2/socket.2.html
    /// socket() creates an endpoint for communication and returns a file
    /// descriptor that refers to that endpoint.
    if ((sock_fd = socket(p->ai_family, p->ai_socktype | SOCK_NONBLOCK,
                          p->ai_protocol)) == -1) {
      continue;
    }
    if (co_await detail::connect(sock_fd, p->ai_addr, p->ai_addrlen)) {
      break;
    }
    close(sock_fd);
    sock_fd = -1;
  }

  if (sock_fd == -1) {
    throw std::system_error(
        std::make_error_code(std::errc::address_not_available));
  }

  co_return Stream(sock_fd);
}

}  // namespace asyncio
