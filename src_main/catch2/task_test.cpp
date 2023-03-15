#include <asyncio/asyncio.h>

// 3rd
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

// std
#include <chrono>
#include <functional>
#include <vector>

using namespace asyncio;
using namespace std::chrono_literals;

template <size_t N>
Task<> co_depth_n(std::vector<int>& result) {
  result.push_back(N);
  if constexpr (N > 0) {
    co_await co_depth_n<N - 1>(result);
    result.push_back(N * 10);
  }
}

SCENARIO("test Task await") {
  std::vector<int> result;
  GIVEN("simple await") {
    asyncio::run(co_depth_n<0>(result));
    std::vector<int> expected{0};
    REQUIRE(result == expected);
  }

  GIVEN("nest await") {
    asyncio::run(co_depth_n<1>(result));
    std::vector<int> expected{1, 0, 10};
    REQUIRE(result == expected);
  }

  GIVEN("3 depth await") {
    asyncio::run(co_depth_n<2>(result));
    std::vector<int> expected{2, 1, 0, 10, 20};
    REQUIRE(result == expected);
  }

  GIVEN("4 depth await") {
    asyncio::run(co_depth_n<3>(result));
    std::vector<int> expected{3, 2, 1, 0, 10, 20, 30};
    REQUIRE(result == expected);
  }

  GIVEN("5 depth await") {
    asyncio::run(co_depth_n<4>(result));
    std::vector<int> expected{4, 3, 2, 1, 0, 10, 20, 30, 40};
    REQUIRE(result == expected);
  }
}

Task<int64_t> square(int64_t x) { co_return x* x; }

SCENARIO("Task<> test") {
  GIVEN("co_await empty task<>") {
    bool called{false};
    asyncio::run([&]() -> Task<> {
      auto t = square(5);
      auto tt = std::move(t);
      REQUIRE(!t.valid());  // NOLINT(bugprone-use-after-move)
      REQUIRE(tt.valid());
      REQUIRE_THROWS_AS(co_await t, InvalidFuture);
      called = true;
    }());

    REQUIRE(called);
  }
}

SCENARIO("test Task await result value") {
  GIVEN("square_sum 3, 4") {
    auto square_sum = [&](int x, int y) -> Task<int> {
      auto tx = square(x);
      auto x2 = co_await tx;
      auto y2 = co_await square(y);
      co_return x2 + y2;
    };
    REQUIRE(asyncio::run(square_sum(3, 4)) == 25);
  }

  GIVEN("fibonacci") {
    std::function<auto(size_t)->Task<size_t>> fib =
        [&](size_t n) -> Task<size_t> {
      if (n <= 1) {
        co_return n;
      }
      co_return co_await fib(n - 1) + co_await fib(n - 2);
    };
    REQUIRE(asyncio::run(fib(0)) == 0);
    REQUIRE(asyncio::run(fib(1)) == 1);
    REQUIRE(asyncio::run(fib(2)) == 1);
    REQUIRE(asyncio::run(fib(12)) == 144);
  }
}

SCENARIO("test Task for loop") {
  auto sequence = [](int64_t n) -> Task<int64_t> {
    int64_t result = 1;
    int64_t sign = -1;
    for (size_t i = 2; i <= n; ++i) {
      result += (co_await square((long)i)) * sign;
      sign *= -1;
    }
    co_return result;
  };

  REQUIRE(asyncio::run(sequence(1)) == 1);
  REQUIRE(asyncio::run(sequence(10)) == -55);
  REQUIRE(asyncio::run(sequence(100)) == -5050);
  REQUIRE(asyncio::run(sequence(100000)) == -5000050000);
}

SCENARIO("test schedule_task") {
  bool called{false};
  auto f = [&]() -> Task<int> {
    called = true;
    co_return 0xababcaab;
  };

  GIVEN("run and detach created task") {
    asyncio::run([&]() -> Task<> {
      auto handle = asyncio::create_scheduled_task(f());
      co_return;
    }());
    REQUIRE(!called);
  }

  GIVEN("run and await created task") {
    asyncio::run([&]() -> Task<> {
      auto handle = asyncio::create_scheduled_task(f());
      REQUIRE(co_await handle == 0xababcaab);
      REQUIRE(co_await handle == 0xababcaab);
    }());
    REQUIRE(called);
  }

  GIVEN("cancel and await created task") {
    asyncio::run([&]() -> Task<> {
      auto handle = asyncio::create_scheduled_task(f());
      handle.cancel();
      REQUIRE_THROWS_AS(co_await handle, InvalidFuture);
    }());
  }
}

auto int_div(int a, int b) -> Task<double> {
  if (b == 0) {
    throw std::overflow_error("b is 0!");
  }
  co_return a / b;
};

SCENARIO("test exception") {
  REQUIRE(asyncio::run(int_div(4, 2)) == Catch::Approx(2));
  REQUIRE_THROWS_AS(asyncio::run(int_div(4, 0)), std::overflow_error);
}

SCENARIO("test sleep") {
  size_t call_time = 0;
  auto say_after = [&](auto delay, std::string_view what) -> Task<> {
    co_await asyncio::sleep(delay);
    fmt::print("{}\n", what);
    ++call_time;
  };

  GIVEN("schedule sleep and await") {
    auto async_main = [&]() -> Task<> {
      auto task1 = create_scheduled_task(say_after(100ms, "hello"));
      auto task2 = create_scheduled_task(say_after(200ms, "world"));

      co_await task1;
      co_await task2;
    };
    auto before_wait = get_event_loop().time();
    asyncio::run(async_main());
    auto after_wait = get_event_loop().time();
    auto diff = after_wait - before_wait;
    REQUIRE(diff >= 200ms);
    REQUIRE(diff < 300ms);
    REQUIRE(call_time == 2);
  }

  GIVEN("schedule sleep and cancel") {
    auto async_main = [&]() -> Task<> {
      auto task1 = create_scheduled_task(say_after(100ms, "hello"));
      auto task2 = create_scheduled_task(say_after(200ms, "world"));

      co_await task1;
      task2.cancel();
    };
    auto before_wait = get_event_loop().time();
    asyncio::run(async_main());
    auto after_wait = get_event_loop().time();
    auto diff = after_wait - before_wait;
    REQUIRE(diff >= 100ms);
    REQUIRE(diff < 200ms);
    REQUIRE(call_time == 1);
  }

  GIVEN("schedule sleep and cancel, delay exit") {
    auto async_main = [&]() -> Task<> {
      auto task1 = create_scheduled_task(say_after(100ms, "hello"));
      auto task2 = create_scheduled_task(say_after(200ms, "world"));

      co_await task1;
      task2.cancel();
      // delay 300ms to exit
      co_await asyncio::sleep(200ms);
    };
    auto before_wait = get_event_loop().time();
    asyncio::run(async_main());
    auto after_wait = get_event_loop().time();
    auto diff = after_wait - before_wait;
    REQUIRE(diff >= 300ms);
    REQUIRE(diff < 400ms);
    REQUIRE(call_time == 1);
  }
}

SCENARIO("cancel a infinite loop coroutine") {
  int count = 0;
  asyncio::run([&]() -> Task<> {
    auto inf_loop = [&]() -> Task<> {
      while (true) {
        ++count;
        co_await asyncio::sleep(1ms);
      }
    };
    auto task = create_scheduled_task(inf_loop());
    co_await asyncio::sleep(10ms);
    task.cancel();
  }());
  REQUIRE(count > 0);
  REQUIRE(count < 10);
}

SCENARIO("test timeout") {
  bool is_called = false;
  auto wait_duration = [&](auto duration) -> Task<int> {
    co_await sleep(duration);
    fmt::print("wait_duration finished\n");
    is_called = true;
    co_return 0xbabababc;
  };

  auto wait_for_test = [&](auto duration, auto timeout) -> Task<int> {
    co_return co_await wait_for(wait_duration(duration), timeout);
  };

  SECTION("no timeout") {
    REQUIRE(!is_called);
    REQUIRE(asyncio::run(wait_for_test(12ms, 120ms)) == 0xbabababc);
    REQUIRE(is_called);
  }

  SECTION("wait_for with sleep") {
    REQUIRE(!is_called);
    auto wait_for_rvalue = wait_for(sleep(30ms), 50ms);
    asyncio::run([&]() -> Task<> {
      REQUIRE_NOTHROW(co_await std::move(wait_for_rvalue));
      REQUIRE_THROWS_AS(co_await wait_for(sleep(50ms), 30ms), TimeoutError);
      is_called = true;
    }());
    REQUIRE(is_called);
  }

  SECTION("no time out with exception") {
    REQUIRE_THROWS_AS(asyncio::run([]() -> Task<> {
                        auto v = co_await wait_for(int_div(5, 0), 100ms);
                      }()),
                      std::overflow_error);
  }

  SECTION("timeout error") {
    REQUIRE(!is_called);
    REQUIRE_THROWS_AS(asyncio::run(wait_for_test(200ms, 100ms)), TimeoutError);
    REQUIRE(!is_called);
  }

  SECTION("wait for awaitable") {
    asyncio::run([]() -> Task<> {
      co_await wait_for(std::suspend_always{}, 1s);
      co_await wait_for(std::suspend_never{}, 1s);
    }());
  }
}

SCENARIO("test gather") {
  bool is_called = false;
  auto factorial = [&](std::string_view name, int number) -> Task<int> {
    int r = 1;
    for (int i = 2; i <= number; ++i) {
      fmt::print("Task {}: Compute factorial({}), currently i={}...\n", name,
                 number, i);
      co_await asyncio::sleep(0.1s);
      r *= i;
    }
    fmt::print("Task {}: factorial({}) = {}\n", name, number, r);
    co_return r;
  };
  auto test_void_func = []() -> Task<> {
    fmt::print("this is a void value\n");
    co_return;
  };

  SECTION("test lvalue & rvalue gather") {
    REQUIRE(!is_called);
    asyncio::run([&]() -> Task<> {
      auto fac_lvalue = factorial("A", 2);
      auto fac_xvalue = factorial("B", 3);
      auto&& fac_rvalue = factorial("C", 4);
      {
        auto&& [a, b, c, _void] = co_await asyncio::gather(
            fac_lvalue, static_cast<Task<int>&&>(fac_xvalue),
            std::move(fac_rvalue), test_void_func());
        REQUIRE(a == 2);
        REQUIRE(b == 6);
        REQUIRE(c == 24);
      }
      REQUIRE((co_await fac_lvalue) == 2);
      REQUIRE(!fac_xvalue.valid());  // be moved
      REQUIRE(!fac_rvalue.valid());  // NOLINT(bugprone-use-after-move)
      is_called = true;
    }());
    REQUIRE(is_called);
  }

  SECTION("test gather of gather") {
    REQUIRE(!is_called);
    asyncio::run([&]() -> Task<> {
      auto&& [ab, c, _void] =
          co_await asyncio::gather(gather(factorial("A", 2), factorial("B", 3)),
                                   factorial("C", 4), test_void_func());
      auto&& [a, b] = ab;
      REQUIRE(a == 2);
      REQUIRE(b == 6);
      REQUIRE(c == 24);
      is_called = true;
    }());
    REQUIRE(is_called);
  }

  SECTION("test detach gather") {
    REQUIRE(!is_called);
    auto res = asyncio::gather(factorial("A", 2), factorial("B", 3));
    asyncio::run([&]() -> Task<> {
      auto&& [a, b] = co_await std::move(res);
      REQUIRE(a == 2);
      REQUIRE(b == 6);
      is_called = true;
    }());
    REQUIRE(is_called);
  }

  SECTION("test exception gather") {
    REQUIRE(!is_called);
    REQUIRE_THROWS_AS(asyncio::run([&]() -> Task<std::tuple<double, int>> {
                        is_called = true;
                        co_return co_await asyncio::gather(int_div(4, 0),
                                                           factorial("B", 3));
                      }()),
                      std::overflow_error);
    REQUIRE(is_called);
  }

  SECTION("wait_for with gather") {
    REQUIRE(!is_called);
    asyncio::run([&]() -> Task<> {
      REQUIRE_NOTHROW(co_await wait_for(
          gather(sleep(10ms), sleep(20ms), sleep(30ms)), 50ms));
      REQUIRE_THROWS_AS(
          co_await wait_for(gather(sleep(10ms), sleep(80ms), sleep(30ms)),
                            50ms),
          TimeoutError);
      is_called = true;
    }());
    REQUIRE(is_called);
  }
}

#ifndef NO_IO

SCENARIO("echo server & client") {
  bool is_called = false;
  constexpr std::string_view message = "hello world!";

  asyncio::run([&]() -> Task<> {
    auto handle_echo = [&](Stream stream) -> Task<> {
      auto& sock_info = stream.get_sock_info();
      auto data = co_await stream.read(100);
      REQUIRE(std::string_view{data.data()} == message);
      co_await stream.write(data);
    };

    auto echo_server = [&]() -> Task<> {
      auto server =
          co_await asyncio::start_server(handle_echo, "127.0.0.1", 8888);
      co_await server.serve_forever();
    };

    auto echo_client = [&]() -> Task<> {
      auto stream = co_await asyncio::open_connection("127.0.0.1", 8888);

      co_await stream.write(Stream::Buffer(message.begin(), message.end()));

      auto data = co_await stream.read(100);
      REQUIRE(std::string_view{data.data()} == message);
      is_called = true;
    };

    auto srv = create_scheduled_task(echo_server());
    co_await echo_client();
    srv.cancel();
  }());

  REQUIRE(is_called);
}

#endif
