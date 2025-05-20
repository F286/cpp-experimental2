#include "doctest.h"
#include "flyweight_map.h"
#include <string>
#include <vector>
#include <algorithm>
#include <ranges>

namespace checks {
    using map_t   = flyweight_map<std::string>;
    using iter_t  = map_t::const_iterator;

    static_assert(std::forward_iterator<iter_t>);
    static_assert(std::sentinel_for<iter_t, iter_t>);
    static_assert(std::ranges::forward_range<map_t>);
    static_assert(std::ranges::common_range<map_t>);
    static_assert(std::same_as<std::ranges::range_value_t<map_t>, std::pair<const typename map_t::key_type, const std::string>>);
}

TEST_CASE("deduplicate values and retrieve") {
    flyweight_map<std::string> map;
    auto k1 = map.insert("apple");
    auto k2 = map.insert("banana");
    auto k3 = map.insert("apple");
    CHECK(k1 == k3);
    CHECK(map.size() == 2);
    CHECK(map.contains(k2));
    REQUIRE(map.find(k2) != nullptr);
    CHECK(*map.find(k2) == "banana");
}

TEST_CASE("iterate items") {
    flyweight_map<std::string> map;
    auto k1 = map.insert("a");
    auto k2 = map.insert("b");
    std::vector<std::pair<flyweight_map<std::string>::key_type, std::string>> items;
    for (auto const& [key, val] : map) {
        items.emplace_back(key, val);
    }
    std::vector<std::pair<flyweight_map<std::string>::key_type, std::string>> expected{{k1, "a"}, {k2, "b"}};
    CHECK(items == expected);
}

TEST_CASE("ranges views") {
    flyweight_map<std::string> map;
    map.insert("x");
    map.insert("y");
    std::vector<std::string> vals;
    std::ranges::copy(map.values(), std::back_inserter(vals));
    std::vector<std::string> expected{"x", "y"};
    CHECK(vals == expected);
}

