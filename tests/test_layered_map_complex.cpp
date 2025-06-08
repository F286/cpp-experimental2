#include "doctest.h"
#include "layered_map.h"
#include "layered_map_algo.h"
#include "aabb.h"
#include "positions.h"
#include <ranges>
#include <algorithm>

/// @brief Create an axis aligned box filled with a value.
static layered_map<int> make_box(GlobalPosition min_corner,
                                 GlobalPosition max_corner,
                                 int value = 1) {
    layered_map<int> map;
    for (GlobalPosition p : GlobalAabb{min_corner, max_corner})
        map[p] = value;
    return map;
}

/// @brief Create a solid sphere filled with a value.
static layered_map<int> make_sphere(GlobalPosition center,
                                    std::uint32_t radius,
                                    int value = 1) {
    layered_map<int> map;
    const int r  = static_cast<int>(radius);
    const int r2 = r * r;
    for (int dx = -r; dx <= r; ++dx)
        for (int dy = -r; dy <= r; ++dy)
            for (int dz = -r; dz <= r; ++dz)
                if (dx * dx + dy * dy + dz * dz <= r2)
                    map[GlobalPosition{center.x + dx, center.y + dy, center.z + dz}] = value;
    return map;
}

/// @brief Compute inclusive bounds of occupied voxels in a layered_map.
static std::pair<GlobalPosition, GlobalPosition>
compute_bounds(layered_map<int> const& map) {
    auto mmx = std::ranges::minmax_element(map, {},
        [](auto const& e){ return e.first.x; });
    auto mmy = std::ranges::minmax_element(map, {},
        [](auto const& e){ return e.first.y; });
    auto mmz = std::ranges::minmax_element(map, {},
        [](auto const& e){ return e.first.z; });
    return {{mmx.min->first.x, mmy.min->first.y, mmz.min->first.z},
            {mmx.max->first.x, mmy.max->first.y, mmz.max->first.z}};
}

TEST_CASE("layered_map complex merge overlap chain") {
    auto boxA = make_box({0,0,0}, {10,10,10}, 1);
    auto boxB = make_box({5,5,5}, {15,15,15}, 2);
    auto sphereC = make_sphere({8,8,8}, 5, 3);
    auto boxD = make_box({7,0,0}, {12,8,8}, 4);

    auto expected = merge(boxA, boxB);
    expected = overlap(expected, sphereC);
    expected = merge(expected, boxD);
    expected = subtract(expected, boxB);

    auto result = (((boxA | boxB) & sphereC) | boxD) - boxB;

    constexpr std::size_t expected_count = 318;
    constexpr GlobalPosition expected_min{3,0,0};
    constexpr GlobalPosition expected_max{11,9,9};

    auto bounds = compute_bounds(result);

    CHECK(result.size() == expected_count);
    CHECK(bounds.first == expected_min);
    CHECK(bounds.second == expected_max);
    CHECK(result.size() == expected.size());
    CHECK(std::ranges::equal(result, expected,
        [](auto const& a, auto const& b){
            return a.first == b.first && a.second == b.second;
        }));
}

TEST_CASE("layered_map algorithm chaining") {
    auto boxA = make_box({0,0,0}, {10,10,10}, 1);
    auto boxB = make_box({2,2,2}, {12,12,12}, 2);
    auto boxC = make_box({4,4,4}, {8,8,8}, 3);
    auto sphereD = make_sphere({6,6,6}, 5, 4);

    auto expected = merge(boxA, boxB);
    expected = subtract(expected, boxC);
    expected = overlap(expected, sphereD);

    auto result = overlap(subtract(merge(boxA, boxB), boxC), sphereD);

    constexpr std::size_t expected_count = 482;
    constexpr GlobalPosition expected_min{1,1,1};
    constexpr GlobalPosition expected_max{11,11,11};

    auto bounds = compute_bounds(result);

    CHECK(result.size() == expected_count);
    CHECK(bounds.first == expected_min);
    CHECK(bounds.second == expected_max);
    CHECK(result.size() == expected.size());
    CHECK(std::ranges::equal(result, expected,
        [](auto const& a, auto const& b){
            return a.first == b.first && a.second == b.second;
        }));
}

