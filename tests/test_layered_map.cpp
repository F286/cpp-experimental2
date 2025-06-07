#include "doctest.h"
#include "layered_map.h"
#include "positions.h"
#include "layered_map_algo.h"
#include "aabb.h"
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

/// @brief Create axis aligned box filled with a value.
static layered_map<int> make_box(GlobalPosition min_corner,
                                 GlobalPosition max_corner,
                                 int value = 1)
{
    layered_map<int> map;
    for (GlobalPosition p : GlobalAabb{min_corner, max_corner})
        map[p] = value;
    return map;
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

TEST_CASE("layered_map csg operations") {
    auto lhs = make_box(GlobalPosition{0,0,0}, GlobalPosition{3,3,3});
    auto rhs = make_box(GlobalPosition{2,2,2}, GlobalPosition{5,5,5});

    layered_map<int> manual_union = lhs;
    for (auto const& [gp, v] : rhs)
        manual_union.insert({gp, v});

    auto uop = lhs | rhs;
    CHECK(uop.size() == manual_union.size());

    layered_map<int> manual_inter;
    set_intersection(lhs, rhs,
                    std::inserter(manual_inter, manual_inter.end()));

    auto oop = lhs & rhs;
    CHECK(oop.size() == manual_inter.size());

    auto diff = lhs - rhs;
    for (auto const& [gp, v] : diff)
        CHECK(rhs.find(gp) == rhs.end());

    auto tmp = lhs;
    tmp |= rhs;
    CHECK(tmp.size() == manual_union.size());

    tmp = lhs;
    tmp &= rhs;
    CHECK(tmp.size() == manual_inter.size());

    tmp = lhs;
    tmp -= rhs;
    for (auto const& [gp, v] : tmp)
        CHECK(rhs.find(gp) == rhs.end());
}

