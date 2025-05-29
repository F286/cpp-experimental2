#pragma once

#include <bitset>
#include <vector>
#include <cstddef>
#include <stdexcept>
#include <concepts>

#include "arrow_proxy.h"


/// @brief Key type explicitly convertible to size_t.
template<typename Key>
concept dense_map_key = requires(const Key& k) {
    { static_cast<std::size_t>(k) } -> std::convertible_to<std::size_t>;
};

/// @brief Dense associative container with fixed key range.
template<dense_map_key Key, typename T, std::size_t MaxSize>
class dense_map {
public:
    /// @brief Key type used for lookup.
    using key_type    = Key;
    /// @brief Value type stored in the map.
    using mapped_type = T;
    /// @brief Pair type returned by iteration.
    using value_type  = std::pair<const key_type, const T&>;
    /// @brief Size type of the container.
    using size_type   = std::size_t;

    /// @brief Proxy reference for mutable element access.
    class reference {
    public:
        /// @brief Construct proxy from map and key.
        reference(dense_map& m, key_type k) : map_(&m), key_(k) {}
        /// @brief Convert to constant reference.
        operator const T&() const { return map_->at(key_); }
        /// @brief Assign through proxy.
        reference& operator=(const T& v) { map_->insert_or_assign(key_, v); return *this; }
        /// @brief Move-assign through proxy.
        reference& operator=(T&& v) { map_->insert_or_assign(key_, std::move(v)); return *this; }

    private:
        dense_map* map_{}; ///< @brief Parent container pointer.
        key_type   key_{}; ///< @brief Referenced key.
    };

    /// @brief Forward iterator over constant elements.
    class const_iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using iterator_concept  = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = dense_map::value_type;
        using reference         = value_type;
        using pointer           = arrow_proxy<reference>;

        /// @brief Default constructed iterator.
        const_iterator() = default;
        /// @brief Construct from map and starting key index.
        const_iterator(const dense_map* m, size_type k) : map_(m), key_(k) { advance(); }

        /// @brief Dereference to key/value pair.
        reference operator*() const {
            return { static_cast<key_type>(key_), map_->values_[map_->value_index(key_)] };
        }
        /// @brief Arrow operator for structured bindings.
        pointer operator->() const { return pointer{ **this }; }
        /// @brief Pre-increment iterator.
        const_iterator& operator++() { ++key_; advance(); return *this; }
        /// @brief Post-increment iterator.
        const_iterator operator++(int) { auto tmp = *this; ++(*this); return tmp; }
        /// @brief Equality comparison.
        bool operator==(const const_iterator& o) const { return key_ == o.key_; }
        /// @brief Inequality comparison.
        bool operator!=(const const_iterator& o) const { return key_ != o.key_; }

    private:
        /// @brief Skip to next occupied key.
        void advance() {
            while(key_ < MaxSize && !map_->used_.test(key_)) ++key_;
        }
        /// @brief Pointer to parent container.
        const dense_map* map_{};
        /// @brief Current key index.
        size_type key_{};
    };

    /// @brief Mutable iterator aliasing const_iterator.
    using iterator = const_iterator;

    /// @brief Default constructed empty map.
    dense_map() = default;

    /// @brief True if container has no elements.
    bool empty() const noexcept { return values_.empty(); }

    /// @brief Number of stored elements.
    size_type size() const noexcept { return values_.size(); }

    /// @brief Remove all elements.
    void clear() {
        used_.reset();
        values_.clear();
    }

    /// @brief Access value at key or throw std::out_of_range.
    const T& at(const key_type& key) const {
        size_type idx = index(key);
        if(!used_.test(idx)) throw std::out_of_range("dense_map::at");
        return values_[value_index(idx)];
    }

    /// @brief Check if key exists in map.
    bool contains(const key_type& key) const noexcept {
        size_type idx = index(key);
        return used_.test(idx);
    }

    /// @brief Access value inserting default if missing.
    reference operator[](const key_type& key) {
        if(!contains(key)) insert_or_assign(key, T{});
        return reference(*this, key);
    }
    /// @brief Read-only access to existing value.
    const T& operator[](const key_type& key) const { return at(key); }

    /// @brief Const iterator to element if present.
    const_iterator find(const key_type& key) const {
        size_type idx = index(key);
        if(!used_.test(idx)) return end();
        return const_iterator(this, idx);
    }

    /// @brief Insert or assign a value at key.
    void insert_or_assign(const key_type& key, const T& val) { insert_impl(key, val); }
    /// @brief Insert or assign a value at key using move semantics.
    void insert_or_assign(const key_type& key, T&& val) { insert_impl(key, std::move(val)); }

    /// @brief Erase key and return count removed.
    size_type erase(const key_type& key) {
        size_type idx = index(key);
        if(!used_.test(idx)) return 0;
        size_type pos = value_index(idx);
        used_.reset(idx);
        values_.erase(values_.begin() + pos);
        return 1;
    }

    /// @brief Iterator to first element.
    const_iterator begin() const noexcept { return const_iterator(this, 0); }
    /// @brief Iterator past last element.
    const_iterator end() const noexcept { return const_iterator(this, MaxSize); }
    /// @brief Non-const iterator to first element.
    iterator begin() noexcept { return iterator(this, 0); }
    /// @brief Non-const iterator past last element.
    iterator end() noexcept { return iterator(this, MaxSize); }
    /// @brief Begin iterator ADL helper.
    friend const_iterator begin(const dense_map& m) noexcept { return m.begin(); }
    /// @brief End iterator ADL helper.
    friend const_iterator end(const dense_map& m) noexcept { return m.end(); }

private:
    /// @brief Implementation of insert_or_assign with perfect forwarding.
    template<typename U>
    void insert_impl(const key_type& key, U&& val) {
        size_type idx = index(key);
        size_type pos = value_index(idx);
        if(!used_.test(idx)) {
            used_.set(idx);
            values_.insert(values_.begin() + pos, std::forward<U>(val));
        } else {
            values_[pos] = std::forward<U>(val);
        }
    }

    /// @brief Convert key to index throwing on overflow.
    static constexpr size_type index(const key_type& key) {
        size_type idx = static_cast<size_type>(key);
        if(idx >= MaxSize) throw std::out_of_range("dense_map index");
        return idx;
    }

    /// @brief Compute value index for a given key index.
    size_type value_index(size_type idx) const {
        size_type count = 0;
        for(size_type i = 0; i < idx; ++i)
            if(used_.test(i)) ++count;
        return count;
    }

    std::bitset<MaxSize> used_{}; ///< @brief Bitset marking occupied keys.
    std::vector<T> values_{};     ///< @brief Stored values sorted by key.
}; 

