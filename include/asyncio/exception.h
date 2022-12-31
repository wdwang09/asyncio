#pragma once

// std
#include <exception>

namespace asyncio {

struct TimeoutError : std::exception {
  [[nodiscard]] const char* what() const noexcept override {
    return "TimeoutError";
  }
};

struct NoResultError : std::exception {
  [[nodiscard]] const char* what() const noexcept override {
    return "Result is unset.";
  }
};

struct InvalidFuture : std::exception {
  [[nodiscard]] const char* what() const noexcept override {
    return "Future is invalid.";
  }
};

}  // namespace asyncio
