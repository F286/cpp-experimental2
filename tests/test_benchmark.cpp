#include "doctest.h"
#include "benchmark.h"
#include <thread>
#include <type_traits>

namespace checks {
    static_assert(std::movable<bench::BenchmarkGuard>);
    static_assert(std::is_nothrow_destructible_v<bench::BenchmarkGuard>);
}

TEST_CASE("benchmark guard basic") {
    BENCHMARK("sleep") {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(true);
}
