#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <ranges>
#include <algorithm>
#include <utility>

/// @brief Mirror strategy reversing array elements.
template <std::size_t BlockSize>
struct reverse_mirror {
    /// @brief orientation mask bits.
    using orientation_type = std::byte;

    template <typename Array>
    static Array apply(const Array& arr, orientation_type mask) {
        if ((mask & std::byte{1}) != std::byte{0}) {
            Array out = arr;
            std::ranges::reverse(out);
            return out;
        }
        return arr;
    }

    template <typename Array>
    static std::pair<Array, orientation_type> canonicalize(const Array& arr) {
        Array normal = arr;
        Array mirrored = apply(arr, std::byte{1});
        std::hash<Array> h;
        auto h1 = h(normal);
        auto h2 = h(mirrored);
        if (h1 <= h2)
            return {normal, orientation_type{0}};
        return {mirrored, orientation_type{1}};
    }

    static constexpr std::size_t map_index(std::size_t idx, orientation_type mask) {
        if ((mask & std::byte{1}) != std::byte{0})
            return BlockSize - 1 - idx;
        return idx;
    }
};
