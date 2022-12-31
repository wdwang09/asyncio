#pragma once

#include <asyncio/utils/awaitable.h>

// std
#include <concepts>

namespace asyncio {

namespace concepts {

template <typename Fut>
concept Future =
    Awaitable<Fut> && requires(Fut fut) {
                        requires !std::default_initializable<Fut>;
                        requires std::move_constructible<Fut>;
                        typename std::remove_cvref_t<Fut>::promise_type;
                        fut.get_result();
                      };

}

}  // namespace asyncio
