#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"
#include "flat_tree_map.h"
#include <vector>
#include <algorithm>
#include <ranges>

namespace checks {
    using test_map = flat_tree_map<std::size_t, bool>;
    using iter_t   = test_map::iterator;
    using citer_t  = test_map::const_iterator;

    static_assert(std::forward_iterator<iter_t>);
    static_assert(std::forward_iterator<citer_t>);
    static_assert(std::sentinel_for<iter_t, iter_t>);
    static_assert(std::sentinel_for<citer_t, citer_t>);
    static_assert(std::ranges::forward_range<test_map>);
    static_assert(std::ranges::common_range<test_map>);
    static_assert(std::same_as<std::ranges::range_value_t<test_map>, std::pair<std::size_t, bool>>);
}

TEST_CASE("initial state empty") {
    flat_tree_map<std::size_t, bool> s;
    CHECK(s.empty());
    CHECK(s.size() == 0);
}

TEST_CASE("set bits and query") {
    flat_tree_map<std::size_t, bool> s;
    s.set(0);
    s.set(7);
    s.set(15);
    CHECK(!s.empty());
    CHECK(s.size() == 3);
    CHECK(s[7]);
}

TEST_CASE("count via ranges") {
    flat_tree_map<std::size_t, bool> s;
    s.set(0);
    s.set(7);
    s.set(15);
    auto const& cs = s;
    std::size_t cnt = std::ranges::count_if(cs, [](auto const& p) { return p.second; });
    CHECK(cnt == 3);
}

TEST_CASE("iterator ordering") {
    flat_tree_map<std::size_t, bool> s;
    s.set(0);
    s.set(7);
    s.set(15);
    auto const& cs = s;
    std::vector<std::size_t> keys;
    std::ranges::transform(cs, std::back_inserter(keys), [](auto const& p) { return p.first; });
    std::vector<std::size_t> expected{0, 7, 15};
    CHECK(keys == expected);
}

TEST_CASE("flip and reset bits") {
    flat_tree_map<std::size_t, bool> s;
    s.set(0);
    s.set(7);
    s.set(15);
    s.flip(1);
    s.reset(7);
    CHECK(s.size() == 3);
    CHECK(s[1]);
    CHECK(!s[7]);
}

TEST_CASE("set intersection example") {
    using map_t = flat_tree_map<std::size_t, bool>;
    map_t s;
    s.set(0);
    s.set(7);
    s.set(15);
    auto const& cs = s;
    map_t t;
    t.set(0);
    t.set(3);
    t.set(15);

    std::vector<std::pair<std::size_t, bool>> out;
    std::ranges::set_intersection(cs, std::as_const(t), std::back_inserter(out),
                                  [](auto const& a, auto const& b) { return a.first < b.first; });

    REQUIRE(out.size() == 2);
    CHECK(out[0].first == 0);
    CHECK(out[1].first == 15);
}

int main(int argc, char** argv) {
    doctest::Context context;
    context.applyCommandLine(argc, argv);
    return context.run();
}
