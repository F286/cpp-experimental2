#include "doctest.h"
#include "chunk_map.h"
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

