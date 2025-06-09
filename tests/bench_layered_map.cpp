#include "doctest.h"
#include "layered_map.h"
#include "layered_map_algo.h"
#include "aabb.h"
#include "benchmark.h"

/// @brief Create a filled axis aligned box.
static layered_map<int> make_box(GlobalPosition min_corner,
                                 GlobalPosition max_corner,
                                 int value = 1)
{
    layered_map<int> map;
    for (GlobalPosition p : GlobalAabb{min_corner, max_corner})
        map[p] = value;
    return map;
}

TEST_CASE("layered_map csg benchmark") {
    auto lhs = make_box({0,0,0}, {50,50,50});
    auto rhs = make_box({25,25,25}, {75,75,75});

    layered_map<int> uni;
    BENCHMARK("union") {
        uni = lhs | rhs;
    }

    layered_map<int> inter;
    BENCHMARK("intersection") {
        inter = lhs & rhs;
    }

    layered_map<int> diff;
    BENCHMARK("difference") {
        diff = lhs - rhs;
    }

    CHECK(uni.size() > 0);
    CHECK(inter.size() > 0);
    CHECK(diff.size() > 0);
}
