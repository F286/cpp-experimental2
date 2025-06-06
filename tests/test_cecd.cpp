#include "doctest.h"
#include "layered_map.h"
#include "layered_map_algo.h"
#include "aabb.h"
#include <random>
#include <vector>

/// @brief Create an axis aligned box of voxels.
static layered_map<int> make_box(GlobalPosition min, GlobalPosition max) {
    layered_map<int> map;
    for (GlobalPosition p : GlobalAabb{min, max}) map[p] = 1;
    return map;
}

/// @brief Generate synthetic shape from random spheres.
static layered_map<int> make_random_shape(std::size_t seed) {
    std::mt19937 gen(seed);
    std::uniform_int_distribution<int> coord(5, 40);
    std::uniform_int_distribution<int> radius(2, 6);
    layered_map<int> map;
    for (int i = 0; i < 5; ++i) {
        int cx = coord(gen);
        int cy = coord(gen);
        int cz = coord(gen);
        int r  = radius(gen);
        int r2 = r * r;
        for (int dx = -r; dx <= r; ++dx)
            for (int dy = -r; dy <= r; ++dy)
                for (int dz = -r; dz <= r; ++dz) {
                    if (dx*dx + dy*dy + dz*dz <= r2) {
                        int x = cx + dx;
                        int y = cy + dy;
                        int z = cz + dz;
                        if (x >= 0 && y >= 0 && z >= 0) {
                            map[GlobalPosition{static_cast<std::uint32_t>(x),
                                                static_cast<std::uint32_t>(y),
                                                static_cast<std::uint32_t>(z)}] = 1;
                        }
                    }
                }
    }
    return map;
}

/// @brief Count how many elements of lhs exist in rhs.
static std::size_t intersection_size(layered_map<int> const& lhs,
                                     layered_map<int> const& rhs) {
    std::size_t count = 0;
    for (auto it = lhs.cbegin(); it != lhs.cend(); ++it)
        if (rhs.find(it->first) != rhs.cend()) ++count;
    return count;
}

TEST_CASE("extrude and inset operations") {
    auto box = make_box(GlobalPosition{0,0,0}, GlobalPosition{3,3,3});
    auto inner = inset(box);
    CHECK(inner.size() == 1);
    CHECK(inner.find(GlobalPosition{1,1,1}) != inner.end());
    auto grown = extrude(inner);
    for (auto const& [gp, v] : inner)
        CHECK(grown.find(gp) != grown.end());
    CHECK(grown.size() > inner.size());
}

TEST_CASE("CECD accuracy synthetic") {
    for (int seed = 0; seed < 3; ++seed) {
        auto shape = make_random_shape(seed);
        auto layers = core_expanding_convex_decomposition(shape);
        layered_map<int> merged;
        for (auto const& layer : layers)
            for (auto it = layer.cbegin(); it != layer.cend(); ++it)
                merged.insert({it->first, it->second});
        double acc = static_cast<double>(intersection_size(shape, merged)) /
                     static_cast<double>(shape.size());
        CHECK(acc >= 0.95);
    }
}

