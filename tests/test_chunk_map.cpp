#include "doctest.h"
#include "chunk_map.h"
#include "bucket_map.h"
#include <vector>
#include <algorithm>
#include <string>
#include <ranges>

namespace checks {
    using cm      = chunk_map<std::string>;
    using iter_t  = cm::iterator;
    using citer_t = cm::const_iterator;

    static_assert(std::forward_iterator<iter_t>);
    static_assert(std::forward_iterator<citer_t>);
    static_assert(std::sentinel_for<iter_t, iter_t>);
    static_assert(std::sentinel_for<citer_t, citer_t>);
    static_assert(std::ranges::forward_range<cm>);
    static_assert(std::ranges::common_range<cm>);
    static_assert(std::same_as<std::ranges::range_value_t<cm>,
                               std::pair<GlobalPosition, std::string>>);
}

TEST_CASE("iterate & collect keys") {
    using chunk_map_test = chunk_map<std::string>;
    chunk_map_test lhs;
    lhs[GlobalPosition{1}] = "lhs-only-1";
    lhs[GlobalPosition{2}] = "both-A";
    lhs[GlobalPosition{45}] = "both-B";

    std::vector<GlobalPosition> keys;
    for (auto const& [pos, val] : lhs) {
        keys.push_back(pos);
    }
    std::vector<GlobalPosition> expect{GlobalPosition{1}, GlobalPosition{2}, GlobalPosition{45}};
    CHECK(keys == expect);
}

TEST_CASE("ranges::find with projection") {
    using chunk_map_test = chunk_map<std::string>;
    chunk_map_test lhs;
    lhs[GlobalPosition{1}] = "lhs-only-1";
    lhs[GlobalPosition{2}] = "both-A";
    lhs[GlobalPosition{45}] = "both-B";

    auto it = std::ranges::find(lhs, GlobalPosition{2}, [](auto const& p) { return p.first; });
    CHECK(it != lhs.end());
    CHECK(it->second == "both-A");
}

TEST_CASE("count_if non-empty strings") {
    using chunk_map_test = chunk_map<std::string>;
    chunk_map_test lhs;
    lhs[GlobalPosition{1}] = "lhs-only-1";
    lhs[GlobalPosition{2}] = "both-A";
    lhs[GlobalPosition{45}] = "both-B";

    std::size_t cnt = std::ranges::count_if(lhs, [](auto const& p) { return !p.second.empty(); });
    CHECK(cnt == 3);
}

TEST_CASE("set_intersection on full pairs") {
    using chunk_map_test = chunk_map<std::string>;
    using value_t = std::pair<GlobalPosition, std::string>;

    chunk_map_test lhs;
    lhs[GlobalPosition{1}] = "lhs-only-1";
    lhs[GlobalPosition{2}] = "both-A";
    lhs[GlobalPosition{45}] = "both-B";

    chunk_map_test rhs;
    rhs[GlobalPosition{0}] = "rhs-only-0";
    rhs[GlobalPosition{2}] = "both-A2";
    rhs[GlobalPosition{45}] = "both-B2";

    std::vector<value_t> inter;
    std::ranges::set_intersection(lhs, rhs, std::back_inserter(inter),
                                  [](auto const& a, auto const& b) { return a.first < b.first; });

    REQUIRE(inter.size() == 2);
    CHECK(inter[0].first == GlobalPosition{2});
    CHECK(inter[1].first == GlobalPosition{45});
}

TEST_CASE("optimized chunk_map subtract view") {
    chunk_map<int> lhs;
    lhs[GlobalPosition{1,0,0}] = 1;
    lhs[GlobalPosition{5,0,0}] = 2;
    chunk_map<int> rhs;
    rhs[GlobalPosition{5,0,0}] = 8;
    auto diff = std::ranges::to<std::vector<std::pair<GlobalPosition, int>>>(
        lhs | chunk_map<int>::subtract(rhs));
    std::vector<std::pair<GlobalPosition, int>> expected{{GlobalPosition{1,0,0},1}};
    CHECK(diff == expected);
}

TEST_CASE("optimized chunk_map merge view") {
    chunk_map<int> lhs;
    lhs[GlobalPosition{1,0,0}] = 1;
    chunk_map<int> rhs;
    rhs[GlobalPosition{2,0,0}] = 3;
    rhs[GlobalPosition{1,0,0}] = 4;
    auto uni = std::ranges::to<std::vector<std::pair<GlobalPosition, int>>>(
        lhs | chunk_map<int>::merge(rhs));
    std::vector<std::pair<GlobalPosition, int>> expected{
        {GlobalPosition{1,0,0},1}, {GlobalPosition{2,0,0},3}
    };
    CHECK(uni == expected);
}

TEST_CASE("optimized chunk_map exclusive view") {
    chunk_map<int> lhs;
    lhs[GlobalPosition{1,0,0}] = 1;
    lhs[GlobalPosition{4,0,0}] = 2;
    chunk_map<int> rhs;
    rhs[GlobalPosition{4,0,0}] = 5;
    rhs[GlobalPosition{8,0,0}] = 6;
    auto sym = std::ranges::to<std::vector<std::pair<GlobalPosition, int>>>(
        lhs | chunk_map<int>::exclusive(rhs));
    std::vector<std::pair<GlobalPosition, int>> expected{
        {GlobalPosition{1,0,0},1}, {GlobalPosition{8,0,0},6}
    };
    CHECK(sym == expected);
}

TEST_CASE("ranges to from vector for chunk_map") {
    std::vector<std::pair<GlobalPosition, int>> vals{
        {GlobalPosition{1,0,0}, 1}, {GlobalPosition{2,0,0}, 2}
    };
    auto cm = std::ranges::to<chunk_map<int>>(vals);
    CHECK(cm.size() == 2);
    CHECK(cm.at(GlobalPosition{2,0,0}) == 2);
}

TEST_CASE("ranges to move chunk_map") {
    chunk_map<int> src;
    src[GlobalPosition{3,0,0}] = 5;
    auto dst = std::ranges::to<chunk_map<int>>(std::move(src));
    CHECK(dst.find(GlobalPosition{3,0,0}) != dst.end());
    CHECK(src.empty());
}

