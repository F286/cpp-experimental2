#pragma once

#include "flyweight_map.h"
#include "arrow_proxy.h"
#include <array>
#include <cstddef>
#include <cassert>
#include <ranges>
#include <functional>
#include <concepts>
#include <algorithm>

template<typename Map>
class flyweight_block_map_blocks_view;
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
template <typename Key, typename T, std::size_t BlockSize = 8>
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
    static constexpr size_type block_size = BlockSize;

    template<typename Map>
    friend class flyweight_block_map_blocks_view;

    /// @brief flyweight_map storing individual values.
    using value_pool_t = flyweight_map<T>;
    /// @brief Key type for stored values.
    using value_key    = typename value_pool_t::key_type;
    /// @brief Array of value handles comprising a block.
    using block_array  = std::array<value_key, BlockSize>;
    /// @brief flyweight_map storing unique blocks.
    using block_pool_t = flyweight_map<block_array>;
    /// @brief Key type for stored blocks.
    using block_key    = typename block_pool_t::key_type;

    /// @brief View over deduplicated blocks.
    using blocks_view  = flyweight_block_map_blocks_view<flyweight_block_map>;
    /// @brief Access deduplicated blocks.
    static blocks_view blocks() noexcept { return {}; }

private:

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

    /// @brief Convert key type to index.
    static constexpr std::size_t to_index(const key_type& key) {
        if constexpr(std::integral<key_type>) {
            return static_cast<std::size_t>(key);
        } else {
            return static_cast<std::size_t>(key) & (BlockSize - 1);
        }
    }

public:
    /// @brief Return handle of default value.
    static value_key default_value_key() {
        static const value_key key = value_pool_.insert(T{});
        return key;
    }

    /// @brief Key for the default block of values.
    static block_key default_block_key() {
        static const block_key key = []{
            block_array arr{};
            arr.fill(default_value_key());
            return block_pool_.insert(arr);
        }();
        return key;
    }

    /// @brief Construct default block with all values default-initialized.
    flyweight_block_map() { block_ = default_block_key(); }

    /// @brief Get value at key.
    const T& at(const key_type& key) const {
        auto idx = to_index(key);
        assert(idx < BlockSize);
        const auto& arr = get_block(block_);
        return get_value(arr[idx]);
    }

    /// @brief Proxy reference to a stored value.
    class reference {
    public:
        /// @brief Construct from parent and key.
        reference(flyweight_block_map& parent, key_type key) noexcept
            : parent_(&parent), key_(key) {}

        /// @brief Convert to const reference.
        operator const T&() const { return parent_->at(key_); }

        /// @brief Assign through proxy.
        reference& operator=(const T& val) {
            parent_->set(key_, val);
            return *this;
        }

    private:
        flyweight_block_map* parent_{nullptr};
        key_type key_{};
    };

    /// @brief Indexed mutable access.
    reference operator[](const key_type& key) { return reference(*this, key); }

    /// @brief Indexed read-only access.
    const T& operator[](const key_type& key) const { return at(key); }

    /// @brief Count of non-default values.
    size_type size() const {
        auto def = default_value_key();
        const auto& arr = get_block(block_);
        return std::ranges::count_if(arr, [def](value_key k){ return k != def; });
    }

    /// @brief True if all values are default.
    bool empty() const noexcept { return block_ == default_block_key(); }

    /// @brief Reset to default values.
    void clear() noexcept { block_ = default_block_key(); }

    /// @brief Assign value at key.
    void set(const key_type& key, const T& val) {
        auto idx = to_index(key);
        assert(idx < BlockSize);
        auto arr = get_block(block_);
        arr[idx] = value_pool_.insert(val);
        block_ = block_pool_.insert(arr);
    }

    /// @brief Retrieve internal deduplication key.
    block_key key() const noexcept { return block_; }


    /// @brief Forward iterator over mutable values.
class iterator {
    public:
        friend class flyweight_block_map;
        using iterator_category = std::forward_iterator_tag;
        using iterator_concept  = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = flyweight_block_map::value_type;
        using reference         = std::pair<key_type, flyweight_block_map::reference>;
        using pointer           = arrow_proxy<reference>;

        /// @brief Default constructed iterator.
        iterator() = default;
        /// @brief Construct from parent and starting index.
        iterator(flyweight_block_map* parent, std::size_t idx)
            : parent_(parent), index_(idx) {}

        /// @brief Dereference to key/value pair.
        reference operator*() const {
            return { static_cast<key_type>(index_),
                     flyweight_block_map::reference{*parent_, static_cast<key_type>(index_)} };
        }

        /// @brief Arrow operator for structured bindings.
        pointer operator->() const { return pointer{ **this }; }

        /// @brief Advance to next element.
        iterator& operator++() { ++index_; return *this; }
        /// @brief Post-increment.
        iterator operator++(int) { auto tmp = *this; ++(*this); return tmp; }

        /// @brief Equality comparison.
        bool operator==(const iterator& o) const {
            return parent_ == o.parent_ && index_ == o.index_;
        }
        /// @brief Inequality comparison.
        bool operator!=(const iterator& o) const { return !(*this == o); }

    private:
        flyweight_block_map* parent_{nullptr};
        std::size_t index_{0};
    };

    /// @brief Read-only forward iterator.
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

    /// @brief Iterator to value by key, or end().
    iterator find(const key_type& key) {
        auto idx = to_index(key);
        assert(idx < BlockSize);
        const auto& arr = get_block(block_);
        if (arr[idx] == default_value_key()) return end();
        return iterator(this, idx);
    }

    /// @brief Const iterator to value by key.
    const_iterator find(const key_type& key) const {
        auto idx = to_index(key);
        assert(idx < BlockSize);
        const auto& arr = get_block(block_);
        if (arr[idx] == default_value_key()) return end();
        return const_iterator(this, idx);
    }

    /// @brief Erase value at key if present.
    size_type erase(const key_type& key) {
        auto idx = to_index(key);
        assert(idx < BlockSize);
        auto arr = get_block(block_);
        if (arr[idx] == default_value_key()) return 0;
        arr[idx] = default_value_key();
        block_ = block_pool_.insert(arr);
        return 1;
    }

    /// @brief Erase value at iterator and return next.
    iterator erase(iterator pos) {
        auto idx = pos.index_;
        erase(static_cast<key_type>(idx));
        return iterator(this, idx);
    }

    /// @brief Iterator to first element.
    iterator begin() noexcept { return iterator(this, 0); }
    /// @brief Iterator past the last element.
    iterator end() noexcept { return iterator(this, BlockSize); }

    /// @brief Const iterator to first element.
    const_iterator begin() const noexcept { return const_iterator(this, 0); }
    /// @brief Const iterator past the last element.
    const_iterator end() const noexcept { return const_iterator(this, BlockSize); }

    /// @brief Begin iterator ADL helper.
    friend iterator begin(flyweight_block_map& b) noexcept { return b.begin(); }
    /// @brief End iterator ADL helper.
    friend iterator end(flyweight_block_map& b) noexcept { return b.end(); }
    /// @brief Const begin iterator ADL helper.
    friend const_iterator begin(const flyweight_block_map& b) noexcept { return b.begin(); }
    /// @brief Const end iterator ADL helper.
    friend const_iterator end(const flyweight_block_map& b) noexcept { return b.end(); }

private:
    inline static value_pool_t value_pool_{};
    inline static block_pool_t block_pool_{};
    block_key block_{};
};

/// @brief Compare two maps for equality.
template<typename Key, typename T, std::size_t N>
bool operator==(const flyweight_block_map<Key,T,N>& a,
                const flyweight_block_map<Key,T,N>& b) noexcept
{
    return a.key() == b.key();
}

/// @brief Inequality comparison.
template<typename Key, typename T, std::size_t N>
bool operator!=(const flyweight_block_map<Key,T,N>& a,
                const flyweight_block_map<Key,T,N>& b) noexcept
{
    return !(a == b);
}

/// @brief View object exposing deduplicated blocks for a flyweight_block_map.
template<typename Map>
class flyweight_block_map_blocks_view {
public:
    using map_type    = Map;
    using block_array = typename map_type::block_array;
    using value_key   = typename map_type::value_key;
    using block_key   = typename map_type::block_key;
    using iterator    = typename map_type::block_pool_t::const_iterator;
    using size_type   = typename map_type::block_pool_t::size_type;

    /// @brief First block iterator.
    iterator begin() const noexcept { return map_type::block_pool_.begin(); }
    /// @brief Past-the-end block iterator.
    iterator end() const noexcept { return map_type::block_pool_.end(); }

    /// @brief Begin iterator ADL helper.
    friend iterator begin(const flyweight_block_map_blocks_view& v) noexcept { return v.begin(); }
    /// @brief End iterator ADL helper.
    friend iterator end(const flyweight_block_map_blocks_view& v) noexcept { return v.end(); }

    /// @brief Retrieve block by key.
    const block_array& at(block_key k) const { return map_type::get_block(k); }
    /// @brief Retrieve block for map instance.
    const block_array& at(const map_type& m) const { return map_type::get_block(m.block_); }
    /// @brief Insert a block and return its key.
    block_key insert(const block_array& arr) const { return map_type::block_pool_.insert(arr); }
    /// @brief Insert a value and return its key.
    value_key insert_value(const typename map_type::mapped_type& v) const { return map_type::value_pool_.insert(v); }
    /// @brief Access stored value by handle.
    const typename map_type::mapped_type& value_data(value_key k) const { return map_type::get_value(k); }
    /// @brief Number of unique blocks.
    size_type size() const noexcept { return map_type::block_pool_.size(); }
    /// @brief Check for existence of block key.
    bool contains(block_key k) const noexcept { return map_type::block_pool_.contains(k); }

private:
    flyweight_block_map_blocks_view() = default;
    friend map_type;
};

