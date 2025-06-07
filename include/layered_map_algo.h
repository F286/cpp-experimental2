#pragma once
#include "layered_map.h"
#include <map>
#include <array>

/**
 * @brief Core expanding convex decomposition utilities.
 *
 * Provides voxel morphology helpers (extrude/inset) and the main
 * core_expanding_convex_decomposition algorithm using layered_map.
 */

/**
 * @brief Helper offsets for the six cardinal directions.
 */
inline constexpr std::array<std::tuple<int,int,int>,6> cardinal_offsets{
    std::tuple{ 1, 0, 0}, std::tuple{-1, 0, 0}, std::tuple{0, 1, 0},
    std::tuple{ 0,-1, 0}, std::tuple{0, 0, 1}, std::tuple{0, 0,-1}
};

/// @brief Write elements common to both layered_maps to output iterator.
template<typename T, typename OutIt>
OutIt set_intersection(const layered_map<T>& lhs,
                       const layered_map<T>& rhs,
                       OutIt out)
{
    chunk_map<T, bucket_map<LocalPosition, T>>::for_each_intersection(
        lhs, rhs,
        [&](GlobalPosition gp, const T& val)
        {
            *out++ = std::pair{gp, val};
        });
    return out;
}

/// @brief Combine voxels from both maps.
template<typename T>
layered_map<T> merge(layered_map<T> lhs, const layered_map<T>& rhs)
{
    for (auto it = rhs.cbegin(); it != rhs.cend(); ++it)
        lhs.insert({it->first, it->second});
    return lhs;
}

/// @brief Voxels present in both maps.
template<typename T>
layered_map<T> overlap(layered_map<T> const& lhs, layered_map<T> const& rhs)
{
    layered_map<T> out;
    chunk_map<T, bucket_map<LocalPosition, T>>::for_each_intersection(
        lhs, rhs,
        [&](GlobalPosition gp, const T& val)
        {
            out.insert({gp, val});
        });
    return out;
}

/// @brief Remove rhs voxels from lhs.
template<typename T>
layered_map<T> subtract(layered_map<T> lhs, const layered_map<T>& rhs)
{
    for (auto it = rhs.cbegin(); it != rhs.cend(); ++it)
        lhs.erase(it->first);
    return lhs;
}

/// @brief Union operator shorthand.
template<typename T>
layered_map<T> operator|(layered_map<T> lhs, const layered_map<T>& rhs)
{
    return merge(std::move(lhs), rhs);
}

/// @brief In-place union assignment.
template<typename T>
layered_map<T>& operator|=(layered_map<T>& lhs, const layered_map<T>& rhs)
{
    for (auto it = rhs.cbegin(); it != rhs.cend(); ++it)
        lhs.insert({it->first, it->second});
    return lhs;
}

/// @brief Intersection operator shorthand.
template<typename T>
layered_map<T> operator&(layered_map<T> lhs, const layered_map<T>& rhs)
{
    return overlap(lhs, rhs);
}

/// @brief In-place intersection assignment.
template<typename T>
layered_map<T>& operator&=(layered_map<T>& lhs, const layered_map<T>& rhs)
{
    lhs = overlap(lhs, rhs);
    return lhs;
}

/// @brief Difference operator shorthand.
template<typename T>
layered_map<T> operator-(layered_map<T> lhs, const layered_map<T>& rhs)
{
    return subtract(std::move(lhs), rhs);
}

/// @brief In-place difference assignment.
template<typename T>
layered_map<T>& operator-=(layered_map<T>& lhs, const layered_map<T>& rhs)
{
    for (auto it = rhs.cbegin(); it != rhs.cend(); ++it)
        lhs.erase(it->first);
    return lhs;
}

/// @brief Add offset to a position clamping at zero.
inline bool offset(GlobalPosition p, int dx, int dy, int dz, GlobalPosition& out)
{
    auto add = [](std::uint32_t a, int d, std::uint32_t& res) -> bool
    {
        int v = static_cast<int>(a) + d;
        if (v < 0) return false;
        res = static_cast<std::uint32_t>(v);
        return true;
    };
    std::uint32_t nx, ny, nz;
    if (!add(p.x, dx, nx) || !add(p.y, dy, ny) || !add(p.z, dz, nz))
        return false;
    out = GlobalPosition{nx, ny, nz};
    return true;
}

/// @brief Extrude voxels by one unit in all six cardinal directions.
template<typename T>
layered_map<T> extrude(layered_map<T> const& map)
{
    layered_map<T> out = map;
    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        auto const& [gp, val] = *it;
        for (auto [dx, dy, dz] : cardinal_offsets) {
            GlobalPosition n;
            if (offset(gp, dx, dy, dz, n)) out[n] = val;
        }
    }
    return out;
}

/// @brief Remove boundary voxels leaving the interior.
template<typename T>
layered_map<T> inset(layered_map<T> const& map)
{
    layered_map<T> out;
    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        auto const& [gp, val] = *it;
        bool all_neighbors = true;
        for (auto [dx, dy, dz] : cardinal_offsets) {
            GlobalPosition n;
            if (!offset(gp, dx, dy, dz, n) || map.find(n) == map.cend()) {
                all_neighbors = false;
                break;
            }
        }
        if (all_neighbors) out[gp] = val;
    }
    return out;
}

/// @brief Find the innermost core of a voxel map.
template<typename T>
layered_map<T> detect_core(layered_map<T> const& map)
{
    layered_map<T> prev = map;
    layered_map<T> cur  = map;
    while (!cur.empty()) {
        prev = cur;
        cur  = inset(cur);
    }
    return prev;
}

/// @brief Grow a convex layer from a core within the remaining voxels.
template<typename T>
layered_map<T> expand_convex(layered_map<T> const& core,
                             layered_map<T> const& remaining)
{
    layered_map<T> hull = core;
    layered_map<T> front = core;
    while (true) {
        front = extrude(front);
        layered_map<T> added;
        for (auto const& [gp, v] : front) {
            auto it = remaining.find(gp);
            if (it != remaining.cend()) added.insert({gp, it->second});
        }
        std::size_t before = hull.size();
        for (auto const& [gp, v] : added) hull.insert({gp, v});
        if (hull.size() == before) break;
        front = added;
    }
    return hull;
}

/// @brief Core-Expanding Convex Decomposition producing layered hulls.
template<typename T>
std::vector<layered_map<T>> core_expanding_convex_decomposition(
    layered_map<T> const& voxels)
{
    layered_map<T> remaining = voxels;
    std::vector<layered_map<T>> layers;
    while (!remaining.empty()) {
        auto core = detect_core(remaining);
        if (core.empty()) {
            core.insert(*remaining.begin());
        }
        auto hull = expand_convex(core, remaining);
        if (hull.empty()) break;
        layers.push_back(hull);
        layered_map<T> next;
        for (auto it = remaining.cbegin(); it != remaining.cend(); ++it)
            if (hull.find(it->first) == hull.end())
                next.insert({it->first, it->second});
        remaining = std::move(next);
    }
    return layers;
}

