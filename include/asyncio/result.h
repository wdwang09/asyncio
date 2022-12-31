#pragma once

#include <asyncio/exception.h>

// std
#include <exception>
#include <optional>
#include <utility>
#include <variant>

namespace asyncio {

template <typename T>
struct Result {
  constexpr bool has_value() const noexcept {
    // https://en.cppreference.com/w/cpp/utility/variant/get_if
    return std::get_if<std::monostate>(&result_) == nullptr;
  }

  template <typename R>
  constexpr void set_value(R&& value) noexcept {
    // https://en.cppreference.com/w/cpp/utility/variant/emplace
    // Use 'template' keyword to treat 'emplace' as a dependent template name
    result_.template emplace<T>(std::forward<R>(value));
  }

  template <typename R>  // for promise_type
  constexpr void return_value(R&& value) noexcept {
    return set_value(std::forward<R>(value));
  }

  constexpr T result() & {
    if (auto exception = std::get_if<std::exception_ptr>(&result_)) {
      std::rethrow_exception(*exception);
    }
    if (auto res = std::get_if<T>(&result_)) {
      return *res;
    }
    throw NoResultError{};
  }

  constexpr T result() && {
    if (auto exception = std::get_if<std::exception_ptr>(&result_)) {
      std::rethrow_exception(*exception);
    }
    if (auto res = std::get_if<T>(&result_)) {
      return std::move(*res);
    }
    throw NoResultError{};
  }

  void set_exception(const std::exception_ptr& exception) noexcept {
    result_ = exception;
  }

  void unhandled_exception() noexcept { result_ = std::current_exception(); }

 private:
  // https://en.cppreference.com/w/cpp/utility/variant/monostate
  std::variant<std::monostate, T, std::exception_ptr> result_;
};

template <>
struct Result<void> {
  constexpr bool has_value() const noexcept { return result_.has_value(); }

  void return_void() noexcept { result_.emplace(nullptr); }

  void result() {
    if (result_.has_value() && *result_ != nullptr) {
      std::rethrow_exception(*result_);
    }
  }

  void set_exception(const std::exception_ptr& exception) noexcept {
    result_ = exception;
  }

  void unhandled_exception() noexcept { result_ = std::current_exception(); }

 private:
  std::optional<std::exception_ptr> result_;
};

}  // namespace asyncio
