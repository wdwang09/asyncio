#include <asyncio/asyncio.h>

#include "counted.h"

// 3rd
#include <catch2/catch_test_macros.hpp>

SCENARIO("Test Counted") {
  using TestCounted = Counted<default_counted_policy>;
  TestCounted::reset_count();

  GIVEN("move counted") {
    {
      TestCounted c1;
      TestCounted c2(std::move(c1));
      REQUIRE(TestCounted::construct_counts() == 2);
      REQUIRE(TestCounted::move_construct_counts == 1);
      REQUIRE(TestCounted::alive_counts() == 2);
    }
    REQUIRE(TestCounted::alive_counts() == 0);
  }

  GIVEN("copy counted") {
    {
      TestCounted c1;
      // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
      TestCounted c2(c1);
      REQUIRE(TestCounted::construct_counts() == 2);
      REQUIRE(TestCounted::copy_construct_counts == 1);
      REQUIRE(TestCounted::alive_counts() == 2);
    }
    REQUIRE(TestCounted::alive_counts() == 0);
  }

  GIVEN("move counted II") {
    TestCounted c1;
    {
      TestCounted c2(std::move(c1));
      REQUIRE(TestCounted::construct_counts() == 2);
      REQUIRE(TestCounted::move_construct_counts == 1);
      REQUIRE(TestCounted::alive_counts() == 2);
    }
    REQUIRE(TestCounted::alive_counts() == 1);
    REQUIRE(c1.id_ == -1);  // NOLINT(bugprone-use-after-move)
  }
}

SCENARIO("Test Result") {
  using TestCounted = Counted<not_assignable_counted_policy>;
  TestCounted::reset_count();

  GIVEN("result set lvalue") {
    asyncio::Result<TestCounted> res;
    REQUIRE(!res.has_value());
    {
      TestCounted c;
      REQUIRE(TestCounted::construct_counts() == 1);
      REQUIRE(TestCounted::copy_construct_counts == 0);
      res.set_value(c);
      REQUIRE(TestCounted::construct_counts() == 2);
      REQUIRE(TestCounted::copy_construct_counts == 1);
    }
    REQUIRE(TestCounted::alive_counts() == 1);
    REQUIRE(res.has_value());
  }

  GIVEN("result set rvalue") {
    asyncio::Result<TestCounted> res;
    REQUIRE(!res.has_value());
    {
      TestCounted c;
      REQUIRE(TestCounted::construct_counts() == 1);
      REQUIRE(TestCounted::move_construct_counts == 0);
      res.set_value(std::move(c));
      REQUIRE(TestCounted::construct_counts() == 2);
      REQUIRE(TestCounted::move_construct_counts == 1);
    }
    REQUIRE(TestCounted::alive_counts() == 1);
    REQUIRE(res.has_value());
  }

  GIVEN("lvalue result") {
    asyncio::Result<TestCounted> res;
    res.set_value(TestCounted{});
    REQUIRE(res.has_value());
    REQUIRE(TestCounted::default_construct_counts == 1);
    REQUIRE(TestCounted::move_construct_counts == 1);
    REQUIRE(TestCounted::copy_construct_counts == 0);
    {
      {
        auto&& r = res.result();
        REQUIRE(TestCounted::default_construct_counts == 1);
        REQUIRE(TestCounted::move_construct_counts == 1);
        REQUIRE(TestCounted::copy_construct_counts == 1);
      }

      {
        auto r = res.result();
        REQUIRE(TestCounted::default_construct_counts == 1);
        REQUIRE(TestCounted::copy_construct_counts == 2);
      }
    }
    REQUIRE(TestCounted::alive_counts() == 1);
  }

  GIVEN("rvalue result") {
    asyncio::Result<TestCounted> res;
    res.set_value(TestCounted{});
    REQUIRE(res.has_value());
    REQUIRE(TestCounted::default_construct_counts == 1);
    REQUIRE(TestCounted::move_construct_counts == 1);
    {
      auto r = std::move(res).result();
      REQUIRE(TestCounted::move_construct_counts == 2);
      REQUIRE(TestCounted::alive_counts() == 2);
    }
    REQUIRE(TestCounted::alive_counts() == 1);
  }
}

SCENARIO("Test Counted for Task") {
  using TestCounted = Counted<default_counted_policy>;
  TestCounted::reset_count();

  auto build_count = []() -> asyncio::Task<TestCounted> {
    co_return TestCounted{};
  };
  bool called = false;

  GIVEN("return a counted") {
    asyncio::run([&]() -> asyncio::Task<> {
      auto c = co_await build_count();
      REQUIRE(TestCounted::alive_counts() == 1);
      REQUIRE(TestCounted::move_construct_counts == 2);
      REQUIRE(TestCounted::default_construct_counts == 1);
      REQUIRE(TestCounted::copy_construct_counts == 0);
      called = true;
    }());
    REQUIRE(called);
  }

  GIVEN("return a lvalue counted") {
    asyncio::run([&]() -> asyncio::Task<> {
      auto t = build_count();
      {
        auto c = co_await t;
        REQUIRE(TestCounted::alive_counts() == 2);
        REQUIRE(TestCounted::move_construct_counts == 1);
        REQUIRE(TestCounted::default_construct_counts == 1);
        REQUIRE(TestCounted::copy_construct_counts == 1);
      }

      {
        auto c = co_await std::move(t);
        REQUIRE(TestCounted::alive_counts() == 2);
        REQUIRE(TestCounted::move_construct_counts == 2);
        REQUIRE(TestCounted::default_construct_counts == 1);
        REQUIRE(TestCounted::copy_construct_counts == 1);
      }

      called = true;
    }());
    REQUIRE(called);
  }

  GIVEN("rvalue task: get_result") {
    auto c = asyncio::run(build_count());
    REQUIRE(TestCounted::alive_counts() == 1);
    REQUIRE(TestCounted::move_construct_counts == 2);
    REQUIRE(TestCounted::default_construct_counts == 1);
    REQUIRE(TestCounted::copy_construct_counts == 0);
  }

  GIVEN("lvalue task: get_result") {
    auto t = build_count();
    auto c = asyncio::run(t);
    REQUIRE(TestCounted::alive_counts() == 2);
    REQUIRE(TestCounted::move_construct_counts == 1);
    REQUIRE(TestCounted::default_construct_counts == 1);
    REQUIRE(TestCounted::copy_construct_counts == 1);
  }
}

SCENARIO("Test Pass Parameters to the Coroutine Frame") {
  using TestCounted = Counted<not_assignable_counted_policy>;
  TestCounted::reset_count();

  GIVEN("pass by rvalue") {
    auto task = [](TestCounted count) -> asyncio::Task<> {
      REQUIRE(count.alive_counts() == 2);
      co_return;
    };
    asyncio::run(task(TestCounted{}));
    REQUIRE(TestCounted::default_construct_counts == 1);
    REQUIRE(TestCounted::move_construct_counts == 1);
    REQUIRE(TestCounted::alive_counts() == 0);
  }

  GIVEN("pass by lvalue") {
    auto task = [](TestCounted count) -> asyncio::Task<> {
      REQUIRE(TestCounted::copy_construct_counts == 1);
      REQUIRE(TestCounted::move_construct_counts == 1);
      REQUIRE(count.alive_counts() == 3);
      co_return;
    };
    TestCounted count;
    asyncio::run(task(count));

    REQUIRE(TestCounted::default_construct_counts == 1);
    REQUIRE(TestCounted::copy_construct_counts == 1);
    REQUIRE(TestCounted::move_construct_counts == 1);
    REQUIRE(TestCounted::alive_counts() == 1);
    REQUIRE(count.id_ != -1);
  }

  GIVEN("pass by xvalue") {
    auto task = [](TestCounted count) -> asyncio::Task<> {
      REQUIRE(TestCounted::copy_construct_counts == 0);
      REQUIRE(TestCounted::move_construct_counts == 2);
      REQUIRE(count.alive_counts() == 3);
      REQUIRE(count.id_ != -1);
      co_return;
    };
    TestCounted count;
    asyncio::run(task(std::move(count)));

    REQUIRE(TestCounted::default_construct_counts == 1);
    REQUIRE(TestCounted::copy_construct_counts == 0);
    REQUIRE(TestCounted::move_construct_counts == 2);
    REQUIRE(TestCounted::alive_counts() == 1);
    REQUIRE(count.id_ == -1);  // NOLINT(bugprone-use-after-move)
  }

  GIVEN("pass by lvalue ref") {
    TestCounted count;
    auto task = [&](TestCounted& cnt) -> asyncio::Task<> {
      REQUIRE(cnt.alive_counts() == 1);
      REQUIRE(&cnt == &count);
      co_return;
    };
    asyncio::run(task(count));
    REQUIRE(TestCounted::default_construct_counts == 1);
    REQUIRE(TestCounted::construct_counts() == 1);
    REQUIRE(TestCounted::alive_counts() == 1);
  }

  GIVEN("pass by rvalue ref") {
    auto task = [](TestCounted&& count) -> asyncio::Task<> {
      REQUIRE(count.alive_counts() == 1);
      co_return;
    };
    asyncio::run(task(TestCounted{}));
    REQUIRE(TestCounted::default_construct_counts == 1);
    REQUIRE(TestCounted::construct_counts() == 1);
    REQUIRE(TestCounted::alive_counts() == 0);
  }
}
