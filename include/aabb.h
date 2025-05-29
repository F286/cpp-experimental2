#pragma once

#include "positions.h"
#include "arrow_proxy.h"
#include <cstddef>
#include <iterator>

/// @brief Axis aligned bounding box using min inclusive and max exclusive.
template<typename Position>
class aabb {
public:
    using position_type = Position;
    class iterator;
    using const_iterator = iterator;

    /// @brief Construct empty box.
    constexpr aabb() = default;
    /// @brief Construct from min and max corners.
    constexpr aabb(Position mi, Position ma) : min_(mi), max_(ma) {}

    /// @brief Minimum inclusive corner.
    constexpr Position min() const noexcept { return min_; }
    /// @brief Maximum exclusive corner.
    constexpr Position max() const noexcept { return max_; }
    /// @brief Box width.
    constexpr std::uint32_t width() const noexcept { return max_.x - min_.x; }
    /// @brief Box height.
    constexpr std::uint32_t height() const noexcept { return max_.y - min_.y; }
    /// @brief Box depth.
    constexpr std::uint32_t depth() const noexcept { return max_.z - min_.z; }
    /// @brief Number of contained positions.
    constexpr std::size_t volume() const noexcept {
        return static_cast<std::size_t>(width()) * height() * depth();
    }
    /// @brief Check if position inside box.
    constexpr bool contains(Position p) const noexcept {
        return p.x >= min_.x && p.x < max_.x &&
               p.y >= min_.y && p.y < max_.y &&
               p.z >= min_.z && p.z < max_.z;
    }

    /// @brief Begin iterator over contained positions.
    constexpr iterator begin() const noexcept { return iterator(min_, max_, min_); }
    /// @brief End iterator over contained positions.
    constexpr iterator end() const noexcept {
        Position endPos = min_;
        endPos.x = max_.x;
        return iterator(min_, max_, endPos);
    }

public:
    /// @brief Forward iterator over box positions.
    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using iterator_concept  = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = Position;
        using reference         = value_type;
        using pointer           = arrow_proxy<reference>;

        /// @brief Default constructed iterator.
        constexpr iterator() = default;
        /// @brief Construct from bounds and starting position.
        constexpr iterator(Position mi, Position ma, Position p)
            : min_(mi), max_(ma), pos_(p) {}

        /// @brief Dereference to current position.
        constexpr reference operator*() const { return pos_; }
        /// @brief Arrow operator for structured bindings.
        constexpr pointer operator->() const { return pointer{**this}; }

        /// @brief Pre-increment iterator.
        constexpr iterator& operator++() {
            ++pos_.z;
            if (pos_.z >= max_.z) {
                pos_.z = min_.z;
                ++pos_.y;
                if (pos_.y >= max_.y) {
                    pos_.y = min_.y;
                    ++pos_.x;
                }
            }
            return *this;
        }
        /// @brief Post-increment iterator.
        constexpr iterator operator++(int) { auto t = *this; ++(*this); return t; }
        /// @brief Equality comparison.
        constexpr bool operator==(const iterator& o) const {
            return pos_.x == o.pos_.x && pos_.y == o.pos_.y && pos_.z == o.pos_.z;
        }
        /// @brief Inequality comparison.
        constexpr bool operator!=(const iterator& o) const { return !(*this == o); }

    private:
        Position min_{}; ///< @brief Inclusive minimum corner.
        Position max_{}; ///< @brief Exclusive maximum corner.
        Position pos_{}; ///< @brief Current iterator position.
    };

private:
    Position min_{}; ///< @brief Inclusive minimum corner.
    Position max_{}; ///< @brief Exclusive maximum corner.
};

using GlobalAabb = aabb<GlobalPosition>;
using ChunkAabb  = aabb<ChunkPosition>;
using LocalAabb  = aabb<LocalPosition>;

