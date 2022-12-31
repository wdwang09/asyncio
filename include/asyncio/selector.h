#pragma once

#include <asyncio/handle.h>

// std
#include <cstdint>
#include <vector>

// sys
// https://man7.org/linux/man-pages/man7/epoll.7.html
#include <sys/epoll.h>
#include <unistd.h>

namespace asyncio {

struct Event {
  int fd;
  uint32_t events;
  HandleInfo handle_info;
};

class Selector {
 public:
  Selector() : epfd_(epoll_create1(0)) {
    /// epoll_create(2) creates a new epoll instance and returns a file
    /// descriptor referring to that instance.  (The more recent
    /// epoll_create1(2) extends the functionality of epoll_create(2).)
    if (epfd_ < 0) {
      perror("epoll_create1");
      throw;
    }
  }

  std::vector<Event> select(int timeout_ms) const {
    errno = 0;
    std::vector<epoll_event> events(register_event_count_);
    /// epoll_wait(2) waits for I/O events, blocking the calling thread if no
    /// events are currently available.  (This system call can be thought of as
    /// fetching items from the ready list of the epoll instance.)

    /// https://man7.org/linux/man-pages/man2/epoll_wait.2.html
    /// The timeout argument specifies the number of milliseconds that
    /// epoll_wait() will block.

    /// A call to epoll_wait() will block until either:
    ///   • a file descriptor delivers an event;
    ///   • the call is interrupted by a signal handler; or
    ///   • the timeout expires.

    /// On success, epoll_wait() returns the number of file descriptors ready
    /// for the requested I/O, or zero if no file descriptor became ready during
    /// the requested timeout milliseconds.  On failure, epoll_wait() returns -1
    /// and errno is set to indicate the error.
    int num_fd =
        epoll_wait(epfd_, events.data(), register_event_count_, timeout_ms);
    std::vector<Event> result;
    for (size_t i = 0; i < num_fd; ++i) {
      result.emplace_back(Event{
          .handle_info = *reinterpret_cast<HandleInfo*>(events[i].data.ptr)});
    }
    return result;
  }

  ~Selector() {
    if (epfd_ > 0) {
      close(epfd_);
    }
  }

  bool is_stop() const { return register_event_count_ == 1; }

  void register_event(const Event& event) {
    epoll_event ev{.events = event.events,
                   .data{.ptr = const_cast<HandleInfo*>(&event.handle_info)}};
    /// Interest in particular file descriptors is then registered via
    /// epoll_ctl(2), which adds items to the interest list of the epoll
    /// instance.
    if (epoll_ctl(epfd_, EPOLL_CTL_ADD, event.fd, &ev) == 0) {
      ++register_event_count_;
    }
  }

  void remove_event(const Event& event) {
    epoll_event ev{.events = event.events};
    if (epoll_ctl(epfd_, EPOLL_CTL_DEL, event.fd, &ev) == 0) {
      --register_event_count_;
    }
  }

 private:
  int epfd_;
  int register_event_count_ = 1;
};

}  // namespace asyncio
