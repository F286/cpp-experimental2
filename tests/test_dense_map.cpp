#include "doctest.h"
#include "dense_map.h"
#include <string>
#include <vector>
#include <ranges>

namespace checks {
    using map_t  = dense_map<std::size_t, std::string, 64>;
    using iter_t = map_t::const_iterator;

    static_assert(std::forward_iterator<iter_t>);
    static_assert(std::sentinel_for<iter_t, iter_t>);
    static_assert(std::ranges::forward_range<map_t>);
    static_assert(std::ranges::common_range<map_t>);
    static_assert(std::same_as<std::ranges::range_value_t<map_t>, std::pair<const std::size_t, const std::string&>>);
}

TEST_CASE("insert and size") {
    dense_map<std::size_t, std::string, 64> m;
    m.insert_or_assign(3, "a");
    CHECK(m.size() == 1);
    CHECK(m.contains(3));
    CHECK(m.at(3) == "a");
}

TEST_CASE("erase elements") {
    dense_map<std::size_t, std::string, 64> m;
    m.insert_or_assign(1, "x");
    m.insert_or_assign(2, "y");
    CHECK(m.erase(1) == 1);
    CHECK(m.size() == 1);
    CHECK(!m.contains(1));
    CHECK(m.contains(2));
}

TEST_CASE("iteration order") {
    dense_map<std::size_t, std::string, 64> m;
    m.insert_or_assign(5, "a");
    m.insert_or_assign(1, "b");
    m.insert_or_assign(3, "c");
    std::vector<std::size_t> keys;
    for (auto const& [k,v] : m) keys.push_back(k);
    std::vector<std::size_t> expected{1,3,5};
    CHECK(keys == expected);
}

TEST_CASE("operator square brackets") {
    dense_map<std::size_t, int, 64> m;
    m[2] = 7;
    CHECK(m[2] == 7);
    CHECK(m.size() == 1);
}

