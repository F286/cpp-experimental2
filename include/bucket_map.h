#pragma once

#include <limits>
#include <type_traits>
#include <vector>
#include <map>
#include "arrow_proxy.h"
#include "flat_vector_array_packed.h"

/// @brief Map storing values in deduplicated buckets.
template<typename Key, typename T, typename Bucket = std::uint64_t>
class bucket_map {
    static_assert(std::is_unsigned_v<Bucket>, "Bucket must be unsigned integral");
    static constexpr std::size_t bits_ = std::numeric_limits<Bucket>::digits;
    using bucket_array = array_packed<bits_, int>;
public:
    /// @brief Key type used for lookup.
    using key_type    = Key;
    /// @brief Value type stored in the map.
    using mapped_type = T;
    /// @brief Size type of the container.
    using size_type   = std::size_t;

    /// @brief Node describing keys with the same value within a bucket.
    struct node {
        /// @brief Bucket index of the node.
        std::size_t bucket_index{};
        /// @brief Bit mask of keys using the value.
        Bucket mask{};
        /// @brief Stored value referenced by the mask.
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
        /// @brief Construct from map and starting key.
        const_iterator(const bucket_map* m, size_type k) : map_(m), key_(k) { advance(); }

        /// @brief Dereference to key/value pair.
        reference operator*() const { return {key_, map_->values_[map_->value_index_at(key_)]}; }
        /// @brief Arrow operator for structured bindings.
        pointer operator->() const { return pointer{**this}; }
        /// @brief Pre-increment iterator.
        const_iterator& operator++() { ++key_; advance(); return *this; }
        /// @brief Post-increment iterator.
        const_iterator operator++(int) { auto tmp = *this; ++(*this); return tmp; }
        /// @brief Equality comparison.
        bool operator==(const const_iterator& o) const { return key_ == o.key_; }
        /// @brief Inequality comparison.
        bool operator!=(const const_iterator& o) const { return key_ != o.key_; }
    private:
        /// @brief Skip to next used key.
        void advance() {
            while (key_ < map_->capacity() && map_->value_index_at(key_) == 0) ++key_;
        }
        /// @brief Parent container pointer.
        const bucket_map* map_{};
        /// @brief Current key index.
        size_type key_{};
    };

    /// @brief View of nodes computed from buckets.
    class nodes_view {
    public:
        using vector_type   = std::vector<node>;
        using const_iterator = typename vector_type::const_iterator;

        /// @brief Begin iterator over nodes.
        const_iterator begin() const noexcept { return nodes_.begin(); }
        /// @brief End iterator over nodes.
        const_iterator end() const noexcept { return nodes_.end(); }
    private:
        explicit nodes_view(vector_type n) : nodes_(std::move(n)) {}
        friend class bucket_map;
        /// @brief Stored collection of nodes.
        vector_type nodes_{};
    };

    /// @brief Default constructed empty map.
    bucket_map() { values_.push_back(T{}); }

    /// @brief True if container has no elements.
    bool empty() const noexcept { return size_ == 0; }

    /// @brief Number of stored elements.
    size_type size() const noexcept { return size_; }

    /// @brief Remove all elements.
    void clear() noexcept {
        buckets_.resize(0);
        values_.clear();
        values_.push_back(T{});
        size_ = 0;
    }

    /// @brief Find value at key or throw std::out_of_range.
    const T& at(const key_type& key) const {
        auto idx = value_index_at(key);
        if (idx == 0) throw std::out_of_range("bucket_map::at");
        return values_[idx];
    }

    /// @brief Check if key exists in map.
    bool contains(const key_type& key) const noexcept { return value_index_at(key) != 0; }

    /// @brief Insert or assign a value at key.
    void insert_or_assign(const key_type& key, const T& val) {
        auto idx = static_cast<size_type>(key) / bits_;
        auto bit = static_cast<size_type>(key) % bits_;
        if (idx >= buckets_.size()) buckets_.resize(idx + 1);
        auto arr = buckets_[idx];

        int val_idx = 0;
        for (size_type i = 0; i < bits_; ++i) {
            int vi = arr[i];
            if (vi != 0 && values_[vi] == val) { val_idx = vi; break; }
        }
        if (val_idx == 0) {
            values_.push_back(val);
            val_idx = static_cast<int>(values_.size() - 1);
        }
        if (arr[bit] == 0) ++size_;
        arr[bit] = val_idx;
    }

    /// @brief Erase key and return count removed.
    size_type erase(const key_type& key) {
        auto idx = static_cast<size_type>(key) / bits_;
        if (idx >= buckets_.size()) return 0;
        auto bit = static_cast<size_type>(key) % bits_;
        auto arr = buckets_[idx];
        if (arr[bit] == 0) return 0;
        arr[bit] = 0;
        --size_;
        return 1;
    }

    /// @brief Iterator to first element.
    const_iterator begin() const noexcept { return const_iterator(this, 0); }
    /// @brief Iterator past last element.
    const_iterator end() const noexcept { return const_iterator(this, capacity()); }
    /// @brief Begin iterator ADL helper.
    friend const_iterator begin(const bucket_map& m) noexcept { return m.begin(); }
    /// @brief End iterator ADL helper.
    friend const_iterator end(const bucket_map& m) noexcept { return m.end(); }

    /// @brief Compute nodes describing stored values.
    nodes_view nodes() const {
        std::vector<node> out;
        for (size_type idx = 0; idx < buckets_.size(); ++idx) {
            auto arr = buckets_[idx];
            std::map<int, Bucket> masks;
            for (size_type bit = 0; bit < bits_; ++bit) {
                int vi = arr[bit];
                if (vi != 0) masks[vi] |= Bucket{1} << bit;
            }
            for (auto [vi, mask] : masks) {
                out.push_back(node{idx, mask, values_[vi]});
            }
        }
        return nodes_view(std::move(out));
    }

private:
    /// @brief Value index at key or 0 if empty.
    int value_index_at(size_type key) const noexcept {
        auto idx = key / bits_;
        auto bit = key % bits_;
        if (idx >= buckets_.size()) return 0;
        return buckets_[idx][bit];
    }

    /// @brief Total key capacity.
    size_type capacity() const noexcept { return buckets_.size() * bits_; }

    /// @brief Packed arrays storing value indices per bucket.
    flat_vector<bucket_array> buckets_{};
    /// @brief Vector storing unique values.
    std::vector<T> values_{};
    /// @brief Current number of stored keys.
    size_type size_{0};
};

