#pragma once

#include <limits>
#include <type_traits>
#include <vector>
#include <map>
#include <utility>
#include <bit>
#include "arrow_proxy.h"
#include "flat_vector_array_packed.h"
#include "set_views.h"

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

    /// @brief Node referencing a deduplicated value.
    struct node {
        /// @brief Container storing keys referencing the value.
        using occupied_set = std::vector<key_type>;

        /// @brief Keys referencing the value.
        occupied_set occupied() const {
            occupied_set out;
            for (size_type bit = 0; bit < bits_; ++bit) {
                if (mask_ & (Bucket{1} << bit))
                    out.push_back(static_cast<key_type>(bucket_index_ * bits_ + bit));
            }
            return out;
        }

        /// @brief Referenced value.
        const T& value() const { return *value_ptr_; }

        /// @brief Mask of occupied keys.
        Bucket mask() const { return mask_; }

        /// @brief Index of the containing bucket.
        size_type bucket_index() const { return bucket_index_; }

    private:
        /// @brief Bucket index this node belongs to.
        size_type bucket_index_{};
        /// @brief Bit mask of occupied keys.
        Bucket mask_{};
        /// @brief Pointer to the stored value.
        const T* value_ptr_{};

        friend class bucket_map;
    };

    /// @brief Proxy reference to mutable value access.
    class reference {
    public:
        /// @brief Construct proxy from map and key.
        reference(bucket_map& m, key_type k) : map_(&m), key_(k) {}
        /// @brief Convert to constant reference.
        operator const T&() const { return map_->at(key_); }
        /// @brief Assign through proxy.
        reference& operator=(const T& v) { map_->insert_or_assign(key_, v); return *this; }
        /// @brief Move-assign through proxy.
        reference& operator=(T&& v) { map_->insert_or_assign(key_, std::move(v)); return *this; }

    private:
        bucket_map* map_{}; ///< @brief Parent container pointer.
        key_type    key_{}; ///< @brief Referenced key.
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
        reference operator*() const { return {key_type(key_), map_->values_[map_->value_index_at(key_)]}; }
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

    /// @brief Mutable iterator aliasing const_iterator.
    using iterator = const_iterator;

    /// @brief Lazy view providing nodes per bucket.
    class nodes_view {
    public:
        /// @brief Iterator over lazily computed nodes.
        class const_iterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using iterator_concept  = std::forward_iterator_tag;
            using difference_type   = std::ptrdiff_t;
            using value_type        = std::pair<const size_type, node>;
            using reference         = value_type;
            using pointer           = arrow_proxy<reference>;

            /// @brief Default constructed end iterator.
            const_iterator() = default;
            /// @brief Construct from map and starting bucket.
            const_iterator(const bucket_map* m, size_type b)
                : map_(m), bucket_(b) { load(); }

            /// @brief Dereference to bucket/node pair.
            reference operator*() const {
                auto [vi, mask] = masks_[index_];
                node n{};
                n.bucket_index_ = bucket_;
                n.mask_ = mask;
                n.value_ptr_ = &map_->values_[vi];
                return {bucket_, n};
            }
            /// @brief Arrow operator for structured bindings.
            pointer operator->() const { return pointer{**this}; }
            /// @brief Pre-increment iterator.
            const_iterator& operator++() {
                ++index_;
                if (index_ >= masks_.size()) {
                    ++bucket_;
                    load();
                }
                return *this;
            }
            /// @brief Post-increment iterator.
            const_iterator operator++(int) { auto t = *this; ++(*this); return t; }
            /// @brief Equality comparison.
            bool operator==(const const_iterator& o) const {
                return bucket_ == o.bucket_ && index_ == o.index_;
            }
            /// @brief Inequality comparison.
            bool operator!=(const const_iterator& o) const { return !(*this == o); }

        private:
            /// @brief Load data for current bucket.
            void load() {
                masks_.clear();
                index_ = 0;
                while (bucket_ < map_->buckets_.size()) {
                    std::map<int, Bucket> tmp;
                    auto arr = map_->buckets_[bucket_];
                    for (size_type bit = 0; bit < bits_; ++bit) {
                        int vi = arr[bit];
                        if (vi != 0) tmp[vi] |= Bucket{1} << bit;
                    }
                    for (auto [vi, mask] : tmp) masks_.push_back({vi, mask});
                    if (!masks_.empty()) break;
                    ++bucket_;
                }
            }

            /// @brief Parent container pointer.
            const bucket_map* map_{};
            /// @brief Current bucket index.
            size_type bucket_{};
            /// @brief Masks for the current bucket.
            std::vector<std::pair<int, Bucket>> masks_{};
            /// @brief Index into the current masks.
            size_type index_{0};
        };

        /// @brief Begin iterator over nodes.
        const_iterator begin() const { return const_iterator(map_, 0); }
        /// @brief End iterator over nodes.
        const_iterator end() const { return const_iterator(map_, map_->buckets_.size()); }

    private:
        explicit nodes_view(const bucket_map* m) : map_(m) {}
        friend class bucket_map;
        /// @brief Pointer to parent container.
        const bucket_map* map_{};
    };

    /// @brief Default constructed empty map.
    bucket_map() { values_.push_back(T{}); }

    /// @brief Construct map from input range.
    template<std::ranges::input_range R>
        requires (!std::same_as<std::remove_cvref_t<R>, bucket_map>)
    explicit bucket_map(R&& range) : bucket_map() {
        insert_range(std::forward<R>(range));
    }

    /// @brief Construct map by copying another map.
    bucket_map(const bucket_map& other) : bucket_map() { insert_range(other); }

    /// @brief Construct map by moving another map.
    bucket_map(bucket_map&& other) noexcept : bucket_map() {
        insert_range(std::move(other));
    }

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

    /// @brief Access value inserting default if missing.
    reference operator[](const key_type& key) {
        if (!contains(key)) insert_or_assign(key, T{});
        return reference(*this, key);
    }
    /// @brief Read-only access to existing value.
    const T& operator[](const key_type& key) const { return at(key); }

    /// @brief Const iterator to element if present.
    const_iterator find(const key_type& key) const {
        if (!contains(key)) return end();
        return const_iterator(this, static_cast<size_type>(key));
    }

    /// @brief Insert or assign a value at key.
    void insert_or_assign(const key_type& key, const T& val) {
        insert_or_assign_impl(key, val);
    }

    /// @brief Insert or assign a value at key using move semantics.
    void insert_or_assign(const key_type& key, T&& val) {
        insert_or_assign_impl(key, std::move(val));
    }

    /// @brief Insert a range of key/value pairs.
    template<std::ranges::input_range R>
    void insert_range(R&& range) {
        for (auto&& elem : range) {
            auto&& [k, v] = elem;
            insert_or_assign(k, std::forward<decltype(v)>(v));
        }
    }

    /// @brief Insert all entries from another map.
    void insert_range(const bucket_map& other) {
        if (other.empty()) return;
        if (empty()) {
            buckets_ = other.buckets_;
            values_  = other.values_;
            size_    = other.size_;
            return;
        }
        for (size_type b = 0; b < other.buckets_.size(); ++b) {
            auto arr = other.buckets_[b];
            for (size_type bit = 0; bit < bits_; ++bit) {
                int vi = arr[bit];
                if (vi != 0) insert_or_assign(static_cast<key_type>(b * bits_ + bit),
                                             other.values_[vi]);
            }
        }
    }

    /// @brief Insert all entries from another map by moving.
    void insert_range(bucket_map&& other) {
        if (other.empty()) return;
        if (empty()) {
            buckets_ = std::move(other.buckets_);
            values_  = std::move(other.values_);
            size_    = other.size_;
            other.clear();
            return;
        }
        std::vector<bool> moved(other.values_.size(), false);
        for (size_type b = 0; b < other.buckets_.size(); ++b) {
            auto arr = other.buckets_[b];
            for (size_type bit = 0; bit < bits_; ++bit) {
                int vi = arr[bit];
                if (vi != 0) {
                    if (!moved[vi]) {
                        insert_or_assign(static_cast<key_type>(b * bits_ + bit),
                                         std::move(other.values_[vi]));
                        moved[vi] = true;
                    } else {
                        insert_or_assign(static_cast<key_type>(b * bits_ + bit),
                                         other.values_[vi]);
                    }
                }
            }
        }
        other.clear();
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
    /// @brief Non-const iterator to first element.
    iterator begin() noexcept { return iterator(this, 0); }
    /// @brief Non-const iterator past last element.
    iterator end() noexcept { return iterator(this, capacity()); }
    /// @brief Begin iterator ADL helper.
    friend const_iterator begin(const bucket_map& m) noexcept { return m.begin(); }
    /// @brief End iterator ADL helper.
    friend const_iterator end(const bucket_map& m) noexcept { return m.end(); }

    /// @brief View lazily describing stored nodes.
    nodes_view nodes() const { return nodes_view(this); }


    /// @brief Lazy overlap view of two maps.
    static auto overlap(const bucket_map& lhs, const bucket_map& rhs);
    /// @brief Overlap adaptor for piping.
    static auto overlap(const bucket_map& rhs);

    /// @brief Lazy subtraction view of two maps.
    static auto subtract(const bucket_map& lhs, const bucket_map& rhs);
    /// @brief Subtract adaptor for piping.
    static auto subtract(const bucket_map& rhs);

    /// @brief Lazy merge view of two maps.
    static auto merge(const bucket_map& lhs, const bucket_map& rhs);
    /// @brief Merge adaptor for piping.
    static auto merge(const bucket_map& rhs);

    /// @brief Lazy exclusive view of two maps.
    static auto exclusive(const bucket_map& lhs, const bucket_map& rhs);
    /// @brief Exclusive adaptor for piping.
    static auto exclusive(const bucket_map& rhs);

    /// @brief Bit mask of occupied entries in a bucket array.
    static Bucket mask_of(const bucket_array& arr);

private:
    /// @brief Implementation of insert_or_assign with perfect forwarding.
    template<typename U>
    void insert_or_assign_impl(const key_type& key, U&& val) {
        auto idx = static_cast<size_type>(key) / bits_;
        auto bit = static_cast<size_type>(key) % bits_;
        if (idx >= buckets_.size()) buckets_.resize(idx + 1);
        auto arr = buckets_[idx];

        const T& ref = val;
        int val_idx = 0;
        for (size_type i = 0; i < bits_; ++i) {
            int vi = arr[i];
            if (vi != 0 && values_[vi] == ref) { val_idx = vi; break; }
        }
        if (val_idx == 0) {
            values_.push_back(std::forward<U>(val));
            val_idx = static_cast<int>(values_.size() - 1);
        }
        if (arr[bit] == 0) ++size_;
        arr[bit] = val_idx;
    }
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

template<typename Key, typename T, typename Bucket>
Bucket bucket_map<Key, T, Bucket>::mask_of(const bucket_array& arr)
{
    Bucket mask{};
    for (std::size_t bit = 0; bit < bits_; ++bit)
        if (arr[bit] != 0) mask |= Bucket{1} << bit;
    return mask;
}

/// @brief Invoke function for each set bit in a mask.
template<typename Bucket, typename Fn>
static void for_each_set_bit(Bucket mask, Fn&& fn)
{
    while (mask) {
        auto bit = std::countr_zero(mask);
        fn(static_cast<std::size_t>(bit));
        mask &= mask - 1;
    }
}

template<typename Key, typename T, typename Bucket>
auto bucket_map<Key, T, Bucket>::overlap(const bucket_map& lhs,
                                         const bucket_map& rhs)
{
    return views::overlap(lhs, rhs,
        [](auto const& a, auto const& b) { return a.first < b.first; });
}

template<typename Key, typename T, typename Bucket>
auto bucket_map<Key, T, Bucket>::overlap(const bucket_map& rhs)
{
    return views::overlap(rhs,
        [](auto const& a, auto const& b) { return a.first < b.first; });
}

template<typename Key, typename T, typename Bucket>
auto bucket_map<Key, T, Bucket>::subtract(const bucket_map& lhs,
                                          const bucket_map& rhs)
{
    return views::subtract(lhs, rhs,
        [](auto const& a, auto const& b) { return a.first < b.first; });
}

template<typename Key, typename T, typename Bucket>
auto bucket_map<Key, T, Bucket>::subtract(const bucket_map& rhs)
{
    return views::subtract(rhs,
        [](auto const& a, auto const& b) { return a.first < b.first; });
}

template<typename Key, typename T, typename Bucket>
auto bucket_map<Key, T, Bucket>::merge(const bucket_map& lhs,
                                       const bucket_map& rhs)
{
    return views::merge(lhs, rhs,
        [](auto const& a, auto const& b) { return a.first < b.first; });
}

template<typename Key, typename T, typename Bucket>
auto bucket_map<Key, T, Bucket>::merge(const bucket_map& rhs)
{
    return views::merge(rhs,
        [](auto const& a, auto const& b) { return a.first < b.first; });
}

template<typename Key, typename T, typename Bucket>
auto bucket_map<Key, T, Bucket>::exclusive(const bucket_map& lhs,
                                           const bucket_map& rhs)
{
    return views::exclusive(lhs, rhs,
        [](auto const& a, auto const& b) { return a.first < b.first; });
}

template<typename Key, typename T, typename Bucket>
auto bucket_map<Key, T, Bucket>::exclusive(const bucket_map& rhs)
{
    return views::exclusive(rhs,
        [](auto const& a, auto const& b) { return a.first < b.first; });
}

/// @brief Write entries present in both maps to output iterator.
template<typename Key, typename T, typename Bucket, typename OutIt>
OutIt set_intersection(const bucket_map<Key, T, Bucket>& lhs,
                       const bucket_map<Key, T, Bucket>& rhs,
                       OutIt out)
{
    for (auto&& e : bucket_map<Key, T, Bucket>::overlap(lhs, rhs))
        *out++ = e;
    return out;
}

/// @brief Write entries present in lhs but not rhs to output iterator.
template<typename Key, typename T, typename Bucket, typename OutIt>
OutIt set_difference(const bucket_map<Key, T, Bucket>& lhs,
                     const bucket_map<Key, T, Bucket>& rhs,
                     OutIt out)
{
    for (auto&& e : bucket_map<Key, T, Bucket>::subtract(lhs, rhs))
        *out++ = e;
    return out;
}

/// @brief Write all unique entries from both maps to output iterator.
template<typename Key, typename T, typename Bucket, typename OutIt>
OutIt set_union(const bucket_map<Key, T, Bucket>& lhs,
                const bucket_map<Key, T, Bucket>& rhs,
                OutIt out)
{
    for (auto&& e : bucket_map<Key, T, Bucket>::merge(lhs, rhs))
        *out++ = e;
    return out;
}

/// @brief Write elements present in exactly one of the maps.
template<typename Key, typename T, typename Bucket, typename OutIt>
OutIt set_symmetric_difference(const bucket_map<Key, T, Bucket>& lhs,
                               const bucket_map<Key, T, Bucket>& rhs,
                               OutIt out)
{
    for (auto&& e : bucket_map<Key, T, Bucket>::exclusive(lhs, rhs))
        *out++ = e;
    return out;
}

namespace std::ranges {
    /// @brief Convert a range to a container using insert_range if available.
    template<class C, std::ranges::input_range R>
    C to(R&& r) {
        if constexpr (requires(C c) { c.insert_range(std::declval<R>()); }) {
            C out;
            out.insert_range(std::forward<R>(r));
            return out;
        } else if constexpr (requires { C(std::ranges::begin(r), std::ranges::end(r)); }) {
            return C(std::ranges::begin(r), std::ranges::end(r));
        } else {
            C out;
            for (auto&& e : r) {
                if constexpr (requires { out.push_back(std::forward<decltype(e)>(e)); })
                    out.push_back(std::forward<decltype(e)>(e));
                else
                    out.insert(out.end(), std::forward<decltype(e)>(e));
            }
            return out;
        }
    }
}

