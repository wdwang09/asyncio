#pragma once

#include <asyncio/utils/awaitable.h>
#include <asyncio/utils/future.h>

// std
#include <concepts>

namespace asyncio {

namespace concepts {

template <typename P>
concept Promise = requires(P p) {
                    { p.get_return_object() } -> Future;
                    { p.initial_suspend() } -> Awaitable;
                    { p.final_suspend() } noexcept -> Awaitable;
                    p.unhandled_exception();
                    requires(
                        requires(int v) { p.return_value(v); } ||
                        requires { p.return_void(); });
                  };

}

}  // namespace asyncio
