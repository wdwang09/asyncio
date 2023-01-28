#pragma once

#include <asyncio/task.h>
#include <asyncio/utils/non_copyable.h>

// std
#include <ios>
#include <system_error>

// sys
#include <sys/socket.h>

namespace asyncio {

struct Stream : NonCopyable {
  using Buffer = std::vector<char>;

  explicit Stream(int fd) : fd_(fd) {
    if (fd_ > 0) {
      socklen_t addr_len = sizeof sock_info_;
      /// https://man7.org/linux/man-pages/man2/getsockname.2.html
      /// int getsockname(int sockfd, struct sockaddr *restrict addr,
      /// socklen_t *restrict addrlen);
      ///
      /// getsockname() returns the current address to which the socket sockfd
      /// is bound, in the buffer pointed to by addr.  The addrlen argument
      /// should be initialized to indicate the amount of space (in bytes)
      /// pointed to by addr.  On return it contains the actual size of the
      /// socket address.
      /// sockfd -> addr
      getsockname(fd_, reinterpret_cast<sockaddr*>(&sock_info_), &addr_len);
    }
  }

  Stream(int fd, const sockaddr_storage& sock_info)
      : fd_(fd), sock_info_(sock_info) {}

  Stream(Stream&& other) noexcept
      : fd_{std::exchange(other.fd_, -1)}, sock_info_(other.sock_info_) {}

  ~Stream() { close(); }

  void close() {
    if (fd_ > 0) {
      ::close(fd_);
    }
    fd_ = -1;
  }

  Task<Buffer> read(ssize_t sz = -1) {
    if (sz < 0) {
      // Read until EOF
      co_return co_await read_until_eof();
    }

    Buffer result(sz, 0);
    /// EPOLLIN: The associated file is available for read(2) operations.
    /// https://man7.org/linux/man-pages/man2/epoll_ctl.2.html
    IoEvent epoll_in_ev{.fd = fd_, .event_type = EPOLLIN};
    co_await get_event_loop().wait_io_event(epoll_in_ev);
    sz = ::read(fd_, result.data(), result.size());
    if (sz == -1) {
      throw std::system_error(
          std::make_error_code(static_cast<std::errc>(errno)));
    }
    result.resize(sz);
    co_return result;
  }

  Task<> write(const Buffer& buf) {
    IoEvent epoll_out_ev{.fd = fd_, .event_type = EPOLLOUT};
    ssize_t total_write = 0;
    while (total_write < buf.size()) {
      co_await get_event_loop().wait_io_event(epoll_out_ev);
      ssize_t sz =
          ::write(fd_, buf.data() + total_write, buf.size() - total_write);
      if (sz == -1) {
        throw std::system_error(
            std::make_error_code(static_cast<std::errc>(errno)));
      }
      total_write += sz;
    }
  }

  const sockaddr_storage& get_sock_info() const { return sock_info_; }

 private:
  Task<Buffer> read_until_eof() {
    Buffer result(kChunkSize, 0);
    IoEvent epoll_in_ev{.fd = fd_, .event_type = EPOLLIN};
    ssize_t current_read = 0;
    int has_read = 0;
    do {
      co_await get_event_loop().wait_io_event(epoll_in_ev);
      /// https://man7.org/linux/man-pages/man2/read.2.html
      /// Return value: -1: error, 0: EOF, positive: num of bytes read
      current_read = ::read(fd_, result.data() + has_read, kChunkSize);
      if (current_read == -1) {
        throw std::system_error(
            std::make_error_code(static_cast<std::errc>(errno)));
      }
      if (current_read < kChunkSize) {
        result.resize(has_read + current_read);
      }
      has_read += (int)current_read;
      result.resize(has_read + kChunkSize);
    } while (current_read > 0);

    co_return result;
  }

 private:
  int fd_ = -1;
  /// https://illumos.org/man/3SOCKET/sockaddr_storage
  /// The sockaddr_storage structure is a sockaddr that is not associated with
  /// an address family.  Instead, it is large enough to hold the contents of
  /// any of the other sockaddr structures.  It can be used to embed sufficient
  /// storage for a sockaddr of any type within a larger structure.
  sockaddr_storage sock_info_{};
  constexpr static size_t kChunkSize = 4096;
};

inline const void* get_in_addr(const sockaddr* sa) {
  if (sa->sa_family == AF_INET) {
    return &reinterpret_cast<const sockaddr_in*>(sa)->sin_addr;
  }
  return &reinterpret_cast<const sockaddr_in6*>(sa)->sin6_addr;
}

uint16_t get_in_port(const sockaddr* sa) {
  /// https://linux.die.net/man/3/ntohs
  /// uint16_t ntohs(uint16_t netshort);
  /// The ntohs() function converts the unsigned short integer netshort from
  /// network byte order to host byte order.
  if (sa->sa_family == AF_INET) {
    return ntohs(reinterpret_cast<const sockaddr_in*>(sa)->sin_port);
  }
  return ntohs(reinterpret_cast<const sockaddr_in6*>(sa)->sin6_port);
}

}  // namespace asyncio
