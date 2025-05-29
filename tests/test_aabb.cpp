#include "doctest.h"
#include "aabb.h"
#include <vector>
#include <ranges>

namespace checks {
    using box_t  = GlobalAabb;
    using iter_t = box_t::iterator;

    static_assert(std::forward_iterator<iter_t>);
    static_assert(std::sentinel_for<iter_t, iter_t>);
    static_assert(std::ranges::forward_range<box_t>);
    static_assert(std::ranges::common_range<box_t>);
    static_assert(std::same_as<std::ranges::range_value_t<box_t>, GlobalPosition>);
}

TEST_CASE("aabb iteration volume") {
    GlobalAabb box{GlobalPosition{0,0,0}, GlobalPosition{2,2,2}};
    std::vector<GlobalPosition> vals;
    for(auto p : box) vals.push_back(p);
    CHECK(box.volume() == 8);
    CHECK(vals.size() == box.volume());
    CHECK(vals.front() == GlobalPosition{0,0,0});
    CHECK(vals.back() == GlobalPosition{1,1,1});
}

TEST_CASE("aabb contains") {
    GlobalAabb box{GlobalPosition{1,1,1}, GlobalPosition{4,3,2}};
    CHECK(box.contains(GlobalPosition{1,1,1}));
    CHECK(!box.contains(GlobalPosition{4,2,1}));
}

TEST_CASE("aabb iterator explicit") {
    GlobalAabb box{GlobalPosition{0,0,0}, GlobalPosition{2,2,2}};
    std::vector<GlobalPosition> iter_vals;
    for(auto it = box.begin(); it != box.end(); ++it) iter_vals.push_back(*it);
    std::vector<GlobalPosition> range_vals;
    for(auto p : box) range_vals.push_back(p);
    CHECK(iter_vals == range_vals);
}

