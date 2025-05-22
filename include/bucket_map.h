#pragma once

#include <map>
#include <memory>
#include <limits>
#include <type_traits>
#include "arrow_proxy.h"

/// @brief Map storing values in deduplicated buckets.
template<typename Key, typename T, typename Bucket = std::uint64_t>
class bucket_map {
    static_assert(std::is_unsigned_v<Bucket>, "Bucket must be unsigned integral");
    static constexpr std::size_t bits_ = std::numeric_limits<Bucket>::digits;
public:
    /// @brief Key type used for lookup.
    using key_type    = Key;
    /// @brief Value type stored in the map.
    using mapped_type = T;
    /// @brief Size type of the container.
    using size_type   = std::size_t;

    /// @brief Node storing a value and bit mask within a bucket.
    struct node {
        /// @brief Construct node with bucket index and value.
        node(std::size_t idx, Bucket m, const T& v) : bucket_index(idx), mask(m), value(v) {}
        std::size_t bucket_index{};
        Bucket mask{};
        T value{};
    };

    /// @brief Forward iterator over constant elements.
    class const_iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using iterator_concept  = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = std::pair<const key_type, const T&>;
        using reference         = value_type;
        using pointer           = arrow_proxy<reference>;

        /// @brief Default constructed iterator.
        const_iterator() = default;
        /// @brief Construct from underlying iterator.
        explicit const_iterator(typename std::map<key_type, node*>::const_iterator it) : it_(it) {}

        /// @brief Dereference to key/value pair.
        reference operator*() const { return {it_->first, it_->second->value}; }
        /// @brief Arrow operator for structured bindings.
        pointer operator->() const { return pointer{**this}; }
        /// @brief Pre-increment iterator.
        const_iterator& operator++() { ++it_; return *this; }
        /// @brief Post-increment iterator.
        const_iterator operator++(int) { auto tmp = *this; ++(*this); return tmp; }
        /// @brief Equality comparison.
        bool operator==(const const_iterator& o) const { return it_ == o.it_; }
        /// @brief Inequality comparison.
        bool operator!=(const const_iterator& o) const { return it_ != o.it_; }
    private:
        typename std::map<key_type, node*>::const_iterator it_{};
    };

    /// @brief Container view exposing stored nodes.
    class nodes_view {
    public:
        using map_type       = std::map<std::pair<std::size_t, T>, std::unique_ptr<node>>;
        using iterator       = typename map_type::iterator;
        using const_iterator = typename map_type::const_iterator;

        /// @brief Begin iterator over nodes.
        iterator begin() noexcept { return map_->begin(); }
        /// @brief Begin iterator over nodes (const).
        const_iterator begin() const noexcept { return map_->begin(); }
        /// @brief End iterator over nodes.
        iterator end() noexcept { return map_->end(); }
        /// @brief End iterator over nodes (const).
        const_iterator end() const noexcept { return map_->end(); }

    private:
        explicit nodes_view(map_type& m) : map_(&m) {}
        friend class bucket_map;
        map_type* map_{};
    };

    /// @brief Constant view of stored nodes.
    class const_nodes_view {
    public:
        using map_type       = std::map<std::pair<std::size_t, T>, std::unique_ptr<node>>;
        using const_iterator = typename map_type::const_iterator;

        /// @brief Begin iterator over nodes.
        const_iterator begin() const noexcept { return map_->begin(); }
        /// @brief End iterator over nodes.
        const_iterator end() const noexcept { return map_->end(); }

    private:
        explicit const_nodes_view(const map_type& m) : map_(&m) {}
        friend class bucket_map;
        const map_type* map_{};
    };

    /// @brief Default constructor.
    bucket_map() = default;

    /// @brief True if container has no elements.
    bool empty() const noexcept { return key_map_.empty(); }

    /// @brief Number of stored elements.
    size_type size() const noexcept { return key_map_.size(); }

    /// @brief Remove all elements.
    void clear() noexcept { key_map_.clear(); node_map_.clear(); }

    /// @brief Find value at key or throw std::out_of_range.
    const T& at(const key_type& key) const {
        auto it = key_map_.find(key);
        if (it == key_map_.end()) throw std::out_of_range("bucket_map::at");
        return it->second->value;
    }

    /// @brief Check if key exists.
    bool contains(const key_type& key) const noexcept { return key_map_.contains(key); }

    /// @brief Insert or assign a value at key.
    void insert_or_assign(const key_type& key, const T& val) {
        auto idx = static_cast<std::size_t>(key) / bits_;
        auto bit = static_cast<std::size_t>(key) % bits_;
        auto pair_key = std::make_pair(idx, val);
        auto [it, inserted] = node_map_.try_emplace(pair_key, nullptr);
        if (inserted) it->second = std::make_unique<node>(idx, Bucket{}, val);
        node* n = it->second.get();
        Bucket mask_bit = Bucket{1} << bit;
        if (auto k = key_map_.find(key); k != key_map_.end()) {
            if (k->second == n) {
                return; // same value already
            }
            erase(key);
        }
        n->mask |= mask_bit;
        key_map_[key] = n;
    }

    /// @brief Erase key and return count removed.
    size_type erase(const key_type& key) {
        auto it = key_map_.find(key);
        if (it == key_map_.end()) return 0;
        node* n = it->second;
        auto bit = static_cast<std::size_t>(key) % bits_;
        n->mask &= ~(Bucket{1} << bit);
        key_map_.erase(it);
        if (n->mask == Bucket{}) {
            auto k = std::make_pair(n->bucket_index, n->value);
            node_map_.erase(k);
        }
        return 1;
    }

    /// @brief Iterator to first element.
    const_iterator begin() const noexcept { return const_iterator(key_map_.cbegin()); }
    /// @brief Iterator past last element.
    const_iterator end() const noexcept { return const_iterator(key_map_.cend()); }
    /// @brief Begin iterator ADL helper.
    friend const_iterator begin(const bucket_map& m) noexcept { return m.begin(); }
    /// @brief End iterator ADL helper.
    friend const_iterator end(const bucket_map& m) noexcept { return m.end(); }

    /// @brief View of internal nodes.
    nodes_view nodes() noexcept { return nodes_view(node_map_); }
    /// @brief Const view of internal nodes.
    const_nodes_view nodes() const noexcept { return const_nodes_view(node_map_); }

private:
    std::map<key_type, node*> key_map_{};
    std::map<std::pair<std::size_t, T>, std::unique_ptr<node>> node_map_{};
};

