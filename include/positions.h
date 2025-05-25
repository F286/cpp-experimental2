#pragma once

#include <cstdint>
#include <compare>
#include <array>
#include <cstddef>

/// @brief Morton encode three 10-bit coordinates.
constexpr std::uint32_t morton_encode(std::uint32_t x, std::uint32_t y, std::uint32_t z)
{
    std::uint32_t out = 0;
    for (unsigned bit = 0; bit < 10; ++bit) {
        out |= ((x >> bit) & 1u) << (3 * bit);
        out |= ((y >> bit) & 1u) << (3 * bit + 1);
        out |= ((z >> bit) & 1u) << (3 * bit + 2);
    }
    return out;
}

/// @brief Decode Morton code into coordinates.
constexpr std::array<std::uint32_t, 3> morton_decode(std::uint32_t code)
{
    std::array<std::uint32_t, 3> p{0,0,0};
    for (unsigned bit = 0; bit < 10; ++bit) {
        p[0] |= ((code >> (3 * bit)) & 1u) << bit;
        p[1] |= ((code >> (3 * bit + 1)) & 1u) << bit;
        p[2] |= ((code >> (3 * bit + 2)) & 1u) << bit;
    }
    return p;
}

struct GlobalPosition;
struct ChunkPosition;
struct LocalPosition;

/// @brief Chunk coordinate of the world.
struct ChunkPosition {
    /// @brief X chunk coordinate.
    std::uint32_t x{};
    /// @brief Y chunk coordinate.
    std::uint32_t y{};
    /// @brief Z chunk coordinate.
    std::uint32_t z{};

    constexpr ChunkPosition() = default;
    constexpr ChunkPosition(std::uint32_t xi, std::uint32_t yi, std::uint32_t zi)
        : x(xi), y(yi), z(zi) {}
    constexpr explicit ChunkPosition(GlobalPosition gp);

    constexpr auto operator<=>(const ChunkPosition& other) const
    {
        return morton_encode(x, y, z) <=> morton_encode(other.x, other.y, other.z);
    }
    constexpr bool operator==(const ChunkPosition& other) const = default;
    /// @brief Convert to size type using Morton code.
    constexpr operator std::size_t() const { return morton_encode(x, y, z); }
};

/// @brief Local coordinate within a chunk.
struct LocalPosition {
    /// @brief X local coordinate.
    std::uint32_t x{};
    /// @brief Y local coordinate.
    std::uint32_t y{};
    /// @brief Z local coordinate.
    std::uint32_t z{};

    constexpr LocalPosition() = default;
    constexpr explicit LocalPosition(std::uint32_t code)
    {
        auto p = morton_decode(code);
        x = p[0];
        y = p[1];
        z = p[2];
    }
    constexpr LocalPosition(std::uint32_t xi, std::uint32_t yi, std::uint32_t zi)
        : x(xi), y(yi), z(zi) {}
    constexpr explicit LocalPosition(GlobalPosition gp);

    constexpr auto operator<=>(const LocalPosition& other) const
    {
        return morton_encode(x, y, z) <=> morton_encode(other.x, other.y, other.z);
    }
    constexpr bool operator==(const LocalPosition& other) const = default;
    /// @brief Convert to size type using Morton code.
    constexpr operator std::size_t() const { return morton_encode(x, y, z); }
};

/// @brief Global coordinate in the world.
struct GlobalPosition {
    /// @brief X global coordinate.
    std::uint32_t x{};
    /// @brief Y global coordinate.
    std::uint32_t y{};
    /// @brief Z global coordinate.
    std::uint32_t z{};

    constexpr GlobalPosition() = default;
    constexpr explicit GlobalPosition(std::uint32_t code)
    {
        auto p = morton_decode(code);
        x = p[0];
        y = p[1];
        z = p[2];
    }
    constexpr GlobalPosition(std::uint32_t xi, std::uint32_t yi, std::uint32_t zi)
        : x(xi), y(yi), z(zi) {}
    constexpr explicit GlobalPosition(ChunkPosition c);
    constexpr explicit GlobalPosition(LocalPosition l);

    constexpr auto operator<=>(const GlobalPosition& other) const
    {
        return morton_encode(x, y, z) <=> morton_encode(other.x, other.y, other.z);
    }
    constexpr bool operator==(const GlobalPosition& other) const = default;

    /// @brief Add coordinates component-wise.
    constexpr GlobalPosition operator+(GlobalPosition other) const
    {
        return GlobalPosition{x + other.x, y + other.y, z + other.z};
    }

    /// @brief Morton encoded representation.
    constexpr std::uint32_t encode() const { return morton_encode(x, y, z); }
};

// ──────── out-of-class definitions ─────────
constexpr ChunkPosition::ChunkPosition(GlobalPosition gp)
    : x(gp.x >> 5), y(gp.y >> 5), z(gp.z >> 5) {}

constexpr LocalPosition::LocalPosition(GlobalPosition gp)
    : x(gp.x & 31), y(gp.y & 31), z(gp.z & 31) {}

constexpr GlobalPosition::GlobalPosition(ChunkPosition c)
    : x(c.x << 5), y(c.y << 5), z(c.z << 5) {}

constexpr GlobalPosition::GlobalPosition(LocalPosition l)
    : x(l.x), y(l.y), z(l.z) {}

