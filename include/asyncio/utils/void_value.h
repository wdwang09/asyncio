#pragma once

// std
#include <type_traits>

namespace asyncio {

struct VoidValue {};

namespace detail {

template <typename T>
struct GetTypeIfVoid : std::type_identity<T> {};

template <>
struct GetTypeIfVoid<void> : std::type_identity<VoidValue> {};

}  // namespace detail

template <typename T>
using GetTypeIfVoid_t = typename detail::GetTypeIfVoid<T>::type;

}  // namespace asyncio
