#include "doctest.h"
#include "layered_map.h"
#include "positions.h"
#include <string>
#include <vector>
#include <ranges>

namespace checks {
    using map_t   = layered_map<std::string>;
    using iter_t  = map_t::iterator;
    using citer_t = map_t::const_iterator;

    static_assert(std::forward_iterator<iter_t>);
    static_assert(std::forward_iterator<citer_t>);
    static_assert(std::sentinel_for<iter_t, iter_t>);
    static_assert(std::sentinel_for<citer_t, citer_t>);
    static_assert(std::ranges::forward_range<map_t>);
    static_assert(std::ranges::common_range<map_t>);
    static_assert(std::same_as<std::ranges::range_value_t<map_t>,
                               std::pair<GlobalPosition, std::string>>);
}

TEST_CASE("layered_map basic insertion") {
    layered_map<int> lm;
    lm[GlobalPosition{1,2,3}] = 10;
    lm[GlobalPosition{33,2,3}] = 20;

    CHECK(lm.size() == 2);
    CHECK(lm.at(GlobalPosition{1,2,3}) == 10);
    CHECK(lm.at(GlobalPosition{33,2,3}) == 20);
}

TEST_CASE("layered_map iteration order") {
    layered_map<int> lm;
    lm[GlobalPosition{1,0,0}] = 1;
    lm[GlobalPosition{0,0,0}] = 0;
    lm[GlobalPosition{32,0,0}] = 2;
    std::vector<GlobalPosition> keys;
    for (auto const& [gp, v] : lm) keys.push_back(gp);
    CHECK(keys[0] == GlobalPosition{0,0,0});
    CHECK(keys[1] == GlobalPosition{1,0,0});
    CHECK(keys[2] == GlobalPosition{32,0,0});
}

TEST_CASE("ranges to layered_map from vector") {
    std::vector<std::pair<GlobalPosition, int>> vals{{GlobalPosition{0,0,0}, 1}};
    auto lm = std::ranges::to<layered_map<int>>(vals);
    CHECK(lm.size() == 1);
    CHECK(lm.at(GlobalPosition{0,0,0}) == 1);
}

TEST_CASE("ranges to move layered_map") {
    layered_map<int> src;
    src[GlobalPosition{1,0,0}] = 4;
    auto dst = std::ranges::to<layered_map<int>>(std::move(src));
    CHECK(dst.find(GlobalPosition{1,0,0}) != dst.end());
    CHECK(src.empty());
}

TEST_CASE("layered_map insert method") {
    layered_map<int> lm;
    auto [it1, ok1] = lm.insert({GlobalPosition{2,2,2}, 7});
    CHECK(ok1);
    CHECK(it1->second == 7);

    auto [it2, ok2] = lm.insert({GlobalPosition{2,2,2}, 9});
    CHECK(!ok2);
    CHECK(it2 == it1);
    CHECK(it2->second == 7);
}

