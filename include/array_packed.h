#pragma once
#include <bitset>
#include <vector>
#include <cstddef>
#include <climits>
#include <bit>
#include <stdexcept>
#include <concepts>
#include <type_traits>
#include "arrow_proxy.h"

/// @brief Packed array storing integral elements across bit planes.
template<std::size_t N, std::integral T>
class array_packed {
public:
    /// @brief Value type stored in the container.
    using value_type = T;
    /// @brief Size type of the container.
    using size_type  = std::size_t;

    /// @brief Proxy reference enabling assignment through operator[].
    class reference {
    public:
        /// @brief Construct proxy referencing parent and index.
        reference(array_packed& a, size_type i) : arr_(&a), index_(i) {}
        /// @brief Assign from value of element type.
        reference& operator=(T v) { arr_->set(index_, v); return *this; }
        /// @brief Convert to element value.
        operator T() const { return arr_->get(index_); }
    private:
        /// @brief Parent container pointer.
        array_packed* arr_{};
        /// @brief Element index referenced.
        size_type index_{};
    };

    /// @brief Random access iterator over mutable elements.
    class iterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using iterator_concept  = std::random_access_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = T;
        using reference         = array_packed::reference;
        using pointer           = arrow_proxy<reference>;

        /// @brief Default constructed iterator.
        iterator() = default;
        /// @brief Construct from parent and index.
        iterator(array_packed* a, size_type i) : arr_(a), index_(i) {}
        /// @brief Dereference to reference proxy.
        reference operator*() const { return (*arr_)[index_]; }
        /// @brief Arrow operator for structured bindings.
        pointer operator->() const { return pointer{**this}; }
        /// @brief Pre-increment iterator.
        iterator& operator++() { ++index_; return *this; }
        /// @brief Post-increment iterator.
        iterator operator++(int) { auto tmp = *this; ++(*this); return tmp; }
        /// @brief Pre-decrement iterator.
        iterator& operator--() { --index_; return *this; }
        /// @brief Post-decrement iterator.
        iterator operator--(int) { auto tmp = *this; --(*this); return tmp; }
        /// @brief Advance by n positions.
        iterator& operator+=(difference_type n) { index_ += n; return *this; }
        /// @brief Rewind by n positions.
        iterator& operator-=(difference_type n) { index_ -= n; return *this; }
        /// @brief Iterator at offset n.
        reference operator[](difference_type n) const { return (*arr_)[index_ + n]; }
        /// @brief Difference between iterators.
        friend difference_type operator-(iterator a, iterator b) { return static_cast<difference_type>(a.index_) - static_cast<difference_type>(b.index_); }
        /// @brief Iterator advanced by n.
        friend iterator operator+(iterator it, difference_type n) { it += n; return it; }
        /// @brief Iterator advanced by n (commutative).
        friend iterator operator+(difference_type n, iterator it) { return it + n; }
        /// @brief Iterator rewound by n.
        friend iterator operator-(iterator it, difference_type n) { it -= n; return it; }
        /// @brief Equality comparison.
        bool operator==(const iterator&) const = default;
        /// @brief Order comparison.
        auto operator<=>(const iterator& o) const { return index_ <=> o.index_; }
    private:
        /// @brief Parent container pointer.
        array_packed* arr_{};
        /// @brief Current iterator index.
        size_type index_{};
    };

    /// @brief Random access iterator over constant elements.
    class const_iterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using iterator_concept  = std::random_access_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = T;
        using reference         = T;
        using pointer           = arrow_proxy<T>;

        /// @brief Default constructed iterator.
        const_iterator() = default;
        /// @brief Construct from parent and index.
        const_iterator(const array_packed* a, size_type i) : arr_(a), index_(i) {}
        /// @brief Dereference to element value.
        T operator*() const { return (*arr_)[index_]; }
        /// @brief Arrow operator for structured bindings.
        pointer operator->() const { return pointer{**this}; }
        /// @brief Pre-increment iterator.
        const_iterator& operator++() { ++index_; return *this; }
        /// @brief Post-increment iterator.
        const_iterator operator++(int) { auto tmp = *this; ++(*this); return tmp; }
        /// @brief Pre-decrement iterator.
        const_iterator& operator--() { --index_; return *this; }
        /// @brief Post-decrement iterator.
        const_iterator operator--(int) { auto tmp = *this; --(*this); return tmp; }
        /// @brief Advance by n positions.
        const_iterator& operator+=(difference_type n) { index_ += n; return *this; }
        /// @brief Rewind by n positions.
        const_iterator& operator-=(difference_type n) { index_ -= n; return *this; }
        /// @brief Iterator at offset n.
        T operator[](difference_type n) const { return (*arr_)[index_ + n]; }
        /// @brief Difference between iterators.
        friend difference_type operator-(const_iterator a, const_iterator b) { return static_cast<difference_type>(a.index_) - static_cast<difference_type>(b.index_); }
        /// @brief Iterator advanced by n.
        friend const_iterator operator+(const_iterator it, difference_type n) { it += n; return it; }
        /// @brief Iterator advanced by n (commutative).
        friend const_iterator operator+(difference_type n, const_iterator it) { return it + n; }
        /// @brief Iterator rewound by n.
        friend const_iterator operator-(const_iterator it, difference_type n) { it -= n; return it; }
        /// @brief Equality comparison.
        bool operator==(const const_iterator&) const = default;
        /// @brief Order comparison.
        auto operator<=>(const const_iterator& o) const { return index_ <=> o.index_; }
    private:
        /// @brief Parent container pointer.
        const array_packed* arr_{};
        /// @brief Current iterator index.
        size_type index_{};
    };

    /// @brief Construct empty packed array.
    array_packed() = default;

    /// @brief Access element at index with bounds checking.
    T at(size_type idx) const { if (idx >= size()) throw std::out_of_range("array_packed::at"); return (*this)[idx]; }

    /// @brief Mutable element access without bounds checking.
    reference operator[](size_type idx) { return reference(*this, idx); }

    /// @brief Immutable element access without bounds checking.
    T operator[](size_type idx) const { return get(idx); }

    /// @brief Beginning iterator over mutable elements.
    iterator begin() noexcept { return iterator(this, 0); }
    /// @brief Beginning iterator over constant elements.
    const_iterator begin() const noexcept { return const_iterator(this, 0); }
    /// @brief Beginning const iterator.
    const_iterator cbegin() const noexcept { return const_iterator(this, 0); }
    /// @brief Ending iterator over mutable elements.
    iterator end() noexcept { return iterator(this, size()); }
    /// @brief Ending iterator over constant elements.
    const_iterator end() const noexcept { return const_iterator(this, size()); }
    /// @brief Ending const iterator.
    const_iterator cend() const noexcept { return const_iterator(this, size()); }

    /// @brief Number of elements stored.
    static constexpr size_type size() noexcept { return N; }

private:
    /// @brief Get value at index.
    T get(size_type idx) const {
        using unsigned_t = std::make_unsigned_t<T>;
        unsigned_t v = 0;
        for (size_type b = 0; b < bits_.size(); ++b) {
            if (bits_[b].test(idx)) v |= 1u << b;
        }
        return std::bit_cast<T>(v);
    }

    /// @brief Set value at index.
    void set(size_type idx, T value) {
        using unsigned_t = std::make_unsigned_t<T>;
        unsigned_t u = std::bit_cast<unsigned_t>(value);
        size_type needed = u ? std::bit_width(u) : 0u;
        if (needed > bits_.size()) bits_.resize(needed);
        for (size_type b = 0; b < bits_.size(); ++b) {
            bool bit = (u >> b) & 1u;
            bits_[b].set(idx, bit);
        }
    }

    /// @brief Bit planes storing packed element data.
    std::vector<std::bitset<N>> bits_{};
};

