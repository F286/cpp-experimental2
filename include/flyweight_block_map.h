#pragma once

#include "flyweight_map.h"
#include "arrow_proxy.h"
#include <array>
#include <cstddef>
#include <cassert>
#include <ranges>
#include <functional>
#include <concepts>
namespace std {
    template <class T, size_t N>
    struct hash<array<T, N>> {
        size_t operator()(const array<T,N>& arr) const noexcept {
            size_t h = 0;
            hash<T> hasher;
            for (const auto& v : arr) {
                h ^= hasher(v) + 0x9e3779b9 + (h<<6) + (h>>2);
            }
            return h;
        }
    };
}


// flyweight_block_map provides deduplicated storage for a fixed-size block of
// values. Internally values are stored in a flyweight_map, and entire blocks
// of value handles are also deduplicated via another flyweight_map. This
// enables one level of nested deduplication.

/// @brief Deduplicated fixed-size associative container.
template <std::integral Key, typename T, std::size_t BlockSize = 8>
class flyweight_block_map {
public:
    /// @brief key type used to access elements.
    using key_type     = Key;
    /// @brief mapped value type.
    using mapped_type  = T;
    /// @brief key/value pair type returned by iteration.
    using value_type   = std::pair<const key_type, T>;
    /// @brief size type of the container.
    using size_type    = std::size_t;

private:
    using value_pool_t = flyweight_map<T>;
    using value_key    = typename value_pool_t::key_type;
    using block_array  = std::array<value_key, BlockSize>;
    using block_pool_t = flyweight_map<block_array>;
    using block_key    = typename block_pool_t::key_type;

    /// @brief Retrieve block array for key.
    static const block_array& get_block(block_key k) {
        auto ptr = block_pool_.find(k);
        assert(ptr && "invalid block key");
        return *ptr;
    }

    /// @brief Retrieve value for value handle.
    static const T& get_value(value_key k) {
        auto ptr = value_pool_.find(k);
        assert(ptr && "invalid value key");
        return *ptr;
    }

public:
    /// @brief Construct default block with all values default-initialized.
    flyweight_block_map() {
        block_array arr{};
        value_key def = value_pool_.insert(T{});
        arr.fill(def);
        block_ = block_pool_.insert(arr);
    }

    /// @brief Get value at key.
    const T& at(const key_type& key) const {
        auto idx = static_cast<std::size_t>(key);
        assert(idx < BlockSize);
        const auto& arr = get_block(block_);
        return get_value(arr[idx]);
    }

    /// @brief Indexed read-only access.
    const T& operator[](const key_type& key) const { return at(key); }

    /// @brief Number of elements in the block.
    static constexpr size_type size() noexcept { return BlockSize; }

    /// @brief Assign value at key.
    void set(const key_type& key, const T& val) {
        auto idx = static_cast<std::size_t>(key);
        assert(idx < BlockSize);
        auto arr = get_block(block_);
        arr[idx] = value_pool_.insert(val);
        block_ = block_pool_.insert(arr);
    }

    /// @brief Retrieve internal deduplication key.
    block_key key() const noexcept { return block_; }
    class const_iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using iterator_concept  = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = flyweight_block_map::value_type;
        using reference         = std::pair<key_type, const T&>;
        using pointer           = arrow_proxy<reference>;

        /// @brief Default constructed iterator.
        const_iterator() = default;
        /// @brief Construct from parent and starting index.
        const_iterator(const flyweight_block_map* parent, std::size_t idx)
            : parent_(parent), index_(idx) {}

        /// @brief Dereference to key/value pair.
        reference operator*() const {
            return { static_cast<key_type>(index_), parent_->at(static_cast<key_type>(index_)) };
        }

        /// @brief Arrow operator for structured bindings.
        pointer operator->() const { return pointer{ **this }; }

        /// @brief Advance to next element.
        const_iterator& operator++() { ++index_; return *this; }
        /// @brief Post-increment.
        const_iterator operator++(int) { auto tmp = *this; ++(*this); return tmp; }

        /// @brief Equality comparison.
        bool operator==(const const_iterator& o) const {
            return parent_ == o.parent_ && index_ == o.index_;
        }
        /// @brief Inequality comparison.
        bool operator!=(const const_iterator& o) const { return !(*this == o); }

    private:
        const flyweight_block_map* parent_{nullptr};
        std::size_t index_{0};
    };

    /// @brief Iterator to first element.
    const_iterator begin() const noexcept { return const_iterator(this, 0); }
    /// @brief Iterator past the last element.
    const_iterator end() const noexcept { return const_iterator(this, BlockSize); }

    /// @brief Begin iterator ADL helper.
    friend const_iterator begin(const flyweight_block_map& b) noexcept { return b.begin(); }
    /// @brief End iterator ADL helper.
    friend const_iterator end(const flyweight_block_map& b) noexcept { return b.end(); }

private:
    inline static value_pool_t value_pool_{};
    inline static block_pool_t block_pool_{};
    block_key block_{};
};

