#include "doctest.h"
#include "bucket_map.h"
#include <string>
#include <vector>
#include <ranges>

namespace checks {
    using map_t   = bucket_map<std::size_t, std::string>;
    using iter_t  = map_t::const_iterator;
    using view_t  = map_t::nodes_view;
    using node_t  = map_t::node;

    static_assert(std::forward_iterator<iter_t>);
    static_assert(std::sentinel_for<iter_t, iter_t>);
    static_assert(std::ranges::forward_range<map_t>);
    static_assert(std::ranges::common_range<map_t>);
    static_assert(std::ranges::forward_range<view_t>);
    static_assert(std::same_as<std::ranges::range_value_t<map_t>,
                               std::pair<const std::size_t, const std::string&>>);
    static_assert(std::same_as<std::ranges::range_value_t<view_t>,
                               std::pair<const std::size_t, node_t>>);
}

TEST_CASE("insert and size") {
    bucket_map<std::size_t, std::string> map;
    map.insert_or_assign(1, "a");
    map.insert_or_assign(2, "a");
    CHECK(map.size() == 2);
    CHECK(!map.empty());
}

TEST_CASE("deduplicate nodes in bucket") {
    bucket_map<std::size_t, std::string> map;
    map.insert_or_assign(1, "x");
    map.insert_or_assign(2, "x");
    auto nodes = map.nodes();
    CHECK(std::distance(nodes.begin(), nodes.end()) == 1);
    CHECK(!nodes.begin()->second.occupied().empty());
}

TEST_CASE("iteration order") {
    bucket_map<std::size_t, std::string> map;
    map.insert_or_assign(5, "a");
    map.insert_or_assign(1, "b");
    map.insert_or_assign(3, "c");
    std::vector<std::size_t> keys;
    for (auto const& [k,v] : map) keys.push_back(k);
    std::vector<std::size_t> expected{1,3,5};
    CHECK(keys == expected);
}

TEST_CASE("erase elements") {
    bucket_map<std::size_t, std::string> map;
    map.insert_or_assign(1, "a");
    map.insert_or_assign(2, "a");
    CHECK(map.erase(1) == 1);
    CHECK(map.size() == 1);
    CHECK(map.contains(2));
}

TEST_CASE("values across bucket boundary") {
    bucket_map<std::size_t, int> map;
    map.insert_or_assign(60, 1);
    map.insert_or_assign(70, 1);
    auto nodes = map.nodes();
    CHECK(std::distance(nodes.begin(), nodes.end()) == 2);
    std::vector<std::size_t> keys;
    for (auto const& [k,v] : map) keys.push_back(k);
    CHECK(keys == std::vector<std::size_t>{60,70});
}

TEST_CASE("range with empty bucket between") {
    bucket_map<std::size_t, int> map;
    for(std::size_t i = 0; i < 64; ++i) map.insert_or_assign(i, 1);
    for(std::size_t i = 128; i < 192; ++i) map.insert_or_assign(i, 2);
    auto nodes = map.nodes();
    CHECK(std::distance(nodes.begin(), nodes.end()) == 2);
    CHECK(std::next(nodes.begin())->first == 2);
    std::vector<std::size_t> keys;
    for(auto const& [k,v] : map) keys.push_back(k);
    CHECK(keys.front() == 0);
    CHECK(keys.back() == 191);
}

TEST_CASE("insert_range from vector") {
    bucket_map<std::size_t, std::string> map;
    std::vector<std::pair<std::size_t, std::string>> vals{{0, "a"}, {1, "b"}, {5, "a"}};
    map.insert_range(vals);
    CHECK(map.size() == 3);
    CHECK(map.at(1) == "b");
    auto nodes = map.nodes();
    CHECK(std::distance(nodes.begin(), nodes.end()) == 2);
}

TEST_CASE("insert_range move map") {
    bucket_map<std::size_t, std::string> src;
    src.insert_or_assign(3, "x");
    src.insert_or_assign(4, "y");
    bucket_map<std::size_t, std::string> dst;
    dst.insert_range(std::move(src));
    CHECK(dst.size() == 2);
    CHECK(dst.contains(3));
    CHECK(src.empty());
}

TEST_CASE("insert_range move with overlap") {
    bucket_map<std::size_t, std::string> src;
    src.insert_or_assign(0, "x");
    src.insert_or_assign(128, "c");
    bucket_map<std::size_t, std::string> dst;
    dst.insert_or_assign(0, "a");
    dst.insert_or_assign(64, "b");
    dst.insert_range(std::move(src));
    CHECK(dst.size() == 3);
    CHECK(dst.at(0) == "x");
    CHECK(dst.at(64) == "b");
    CHECK(dst.at(128) == "c");
    CHECK(src.empty());
}

