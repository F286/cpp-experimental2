#include "doctest.h"
#include "array_packed.h"
#include <vector>
#include <ranges>

namespace checks {
    using arr_t = array_packed<64, int>;
    using iter_t = arr_t::iterator;
    using citer_t = arr_t::const_iterator;

    static_assert(std::random_access_iterator<iter_t>);
    static_assert(std::random_access_iterator<citer_t>);
    static_assert(std::sentinel_for<iter_t, iter_t>);
    static_assert(std::sentinel_for<citer_t, citer_t>);
    static_assert(std::ranges::random_access_range<arr_t>);
    static_assert(std::ranges::sized_range<arr_t>);
    static_assert(std::ranges::common_range<arr_t>);
    static_assert(std::same_as<std::ranges::range_value_t<arr_t>, int>);
}

TEST_CASE("set and get values") {
    array_packed<64, int> a;
    a[0] = 1;
    a[1] = 4;
    a[1] = 5;
    a[3] = 42;
    CHECK(a[0] == 1);
    CHECK(a[1] == 5);
    CHECK(a[2] == 0);
    CHECK(a[3] == 42);
}

TEST_CASE("iterate values") {
    array_packed<64, int> a;
    for(int i = 0; i < 4; ++i) a[i] = i * 2;
    std::vector<int> out;
    for(auto const& v : a) out.push_back(v);
    CHECK(out[0] == 0);
    CHECK(out[1] == 2);
    CHECK(out[2] == 4);
    CHECK(out[3] == 6);
}
