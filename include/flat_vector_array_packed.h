#pragma once
#include "flat_vector.h"
#include "array_packed.h"
#include "arrow_proxy.h"
#include <vector>
#include <bit>
#include <concepts>
#include <type_traits>
#include <bitset>
#include <cstdint>

/// @brief Specialisation of flat_vector for array_packed providing contiguous storage.
template<std::size_t N, std::integral T>
class flat_vector<array_packed<N, T>> {
public:
    /// @brief Value type stored in the container.
    using value_type = array_packed<N, T>;
    /// @brief Size type of the container.
    using size_type  = std::size_t;

    class reference;
    class const_reference;
    class iterator;
    class const_iterator;

    /// @brief Construct empty flat_vector.
    flat_vector() = default;

    /// @brief Number of elements stored.
    size_type size() const noexcept { return elementBitCount_.size(); }

    /// @brief Resize container to hold n elements.
    void resize(size_type n) {
        if (n < size()) {
            size_type remove = 0;
            for (size_type i = n; i < elementBitCount_.size(); ++i) remove += elementBitCount_[i];
            bits_.resize(bits_.size() - remove);
        }
        elementBitCount_.resize(n, 0);
    }

    /// @brief Append element to container.
    void push_back(const value_type& v) {
        resize(size() + 1);
        assign(size() - 1, v);
    }
    /// @brief Append element to container (rvalue overload).
    void push_back(value_type&& v) { push_back(static_cast<const value_type&>(v)); }

    /// @brief Access element at index without bounds checking.
    reference operator[](size_type idx) { return reference(*this, idx); }
    /// @brief Access element at index without bounds checking (const).
    const_reference operator[](size_type idx) const { return const_reference(*this, idx); }

    /// @brief Begin iterator over mutable elements.
    iterator begin() noexcept { return iterator(this, 0); }
    /// @brief Begin iterator over constant elements.
    const_iterator begin() const noexcept { return const_iterator(this, 0); }
    /// @brief Begin const iterator.
    const_iterator cbegin() const noexcept { return const_iterator(this, 0); }
    /// @brief End iterator over mutable elements.
    iterator end() noexcept { return iterator(this, size()); }
    /// @brief End iterator over constant elements.
    const_iterator end() const noexcept { return const_iterator(this, size()); }
    /// @brief End const iterator.
    const_iterator cend() const noexcept { return const_iterator(this, size()); }

private:
    using unsigned_t = std::make_unsigned_t<T>;

    /// @brief Compute starting bit-plane index for element.
    size_type plane_offset(size_type elem) const {
        size_type off = 0;
        for (size_type i = 0; i < elem; ++i) off += elementBitCount_[i];
        return off;
    }

    /// @brief Ensure the given element has at least cnt bit planes.
    void ensure_bitplanes_for_element(size_type elem, size_type cnt) {
        size_type start = plane_offset(elem);
        size_type cur   = elementBitCount_[elem];
        if (cnt > cur) {
            bits_.insert(bits_.begin() + start + cur, cnt - cur, {});
            elementBitCount_[elem] = static_cast<std::uint8_t>(cnt);
        }
    }

    /// @brief Set value at element and index.
    void set(size_type elem, size_type idx, T value) {
        unsigned_t u = std::bit_cast<unsigned_t>(value);
        size_type need = u ? std::bit_width(u) : 0u;
        ensure_bitplanes_for_element(elem, need);
        size_type start = plane_offset(elem);
        for (size_type b = 0; b < elementBitCount_[elem]; ++b) {
            bool bit = (u >> b) & 1u;
            bits_[start + b].set(idx, bit);
        }
    }

    /// @brief Get value at element and index.
    T get(size_type elem, size_type idx) const {
        unsigned_t v = 0;
        size_type start = plane_offset(elem);
        for (size_type b = 0; b < elementBitCount_[elem]; ++b) {
            if (bits_[start + b].test(idx)) v |= unsigned_t{1} << b;
        }
        return std::bit_cast<T>(v);
    }

    /// @brief Assign entire element from value.
    void assign(size_type elem, const value_type& val) {
        for (size_type i = 0; i < N; ++i) set(elem, i, val[i]);
    }

    /// @brief Bit planes storing packed element data.
    std::vector<std::bitset<N>> bits_{};
    /// @brief Bit count of each element.
    std::vector<std::uint8_t> elementBitCount_{};

public:
    /// @brief Proxy reference to mutable array element.
    class reference {
    public:
        /// @brief Construct proxy referencing parent and index.
        reference(flat_vector& v, size_type i) : vec_(&v), index_(i) {}
        /// @brief Proxy for individual value in array.
        class value_ref {
        public:
            /// @brief Construct value proxy.
            value_ref(flat_vector& v, size_type e, size_type i) : vec_(&v), elem_(e), idx_(i) {}
            /// @brief Assign from value.
            value_ref& operator=(T val) { vec_->set(elem_, idx_, val); return *this; }
            /// @brief Convert to value.
            operator T() const { return vec_->get(elem_, idx_); }
        private:
            /// @brief Parent container pointer.
            flat_vector* vec_{};
            /// @brief Index of referenced element.
            size_type elem_{};
            /// @brief Index of value inside element.
            size_type idx_{};
        };
        /// @brief Access value at sub-index.
        value_ref operator[](size_type i) { return value_ref(*vec_, index_, i); }
        /// @brief Access value at sub-index (const).
        T operator[](size_type i) const { return vec_->get(index_, i); }
        /// @brief Convert to value type.
        operator value_type() const {
            value_type tmp;
            for (size_type i = 0; i < N; ++i) tmp[i] = vec_->get(index_, i);
            return tmp;
        }
    private:
        /// @brief Parent container pointer.
        flat_vector* vec_{};
        /// @brief Index of referenced element.
        size_type index_{};
    };

    /// @brief Proxy reference to constant array element.
    class const_reference {
    public:
        /// @brief Construct proxy referencing parent and index.
        const_reference(const flat_vector& v, size_type i) : vec_(&v), index_(i) {}
        /// @brief Access value at sub-index.
        T operator[](size_type i) const { return vec_->get(index_, i); }
        /// @brief Convert to value type.
        operator value_type() const {
            value_type tmp;
            for (size_type i = 0; i < N; ++i) tmp[i] = vec_->get(index_, i);
            return tmp;
        }
    private:
        /// @brief Parent container pointer.
        const flat_vector* vec_{};
        /// @brief Index of referenced element.
        size_type index_{};
    };

    /// @brief Random access iterator over mutable elements.
    class iterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using iterator_concept  = std::random_access_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = array_packed<N, T>;
        using reference         = flat_vector::reference;
        using pointer           = arrow_proxy<reference>;

        /// @brief Default constructed iterator.
        iterator() = default;
        /// @brief Construct from parent and index.
        iterator(flat_vector* v, size_type i) : vec_(v), index_(i) {}
        /// @brief Dereference to reference proxy.
        reference operator*() const { return (*vec_)[index_]; }
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
        reference operator[](difference_type n) const { return (*vec_)[index_ + n]; }
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
        flat_vector* vec_{};
        /// @brief Current iterator index.
        size_type index_{};
    };

    /// @brief Random access iterator over constant elements.
    class const_iterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using iterator_concept  = std::random_access_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = array_packed<N, T>;
        using reference         = flat_vector::const_reference;
        using pointer           = arrow_proxy<reference>;

        /// @brief Default constructed iterator.
        const_iterator() = default;
        /// @brief Construct from parent and index.
        const_iterator(const flat_vector* v, size_type i) : vec_(v), index_(i) {}
        /// @brief Dereference to reference proxy.
        reference operator*() const { return const_reference(*vec_, index_); }
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
        reference operator[](difference_type n) const { return const_reference(*vec_, index_ + n); }
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
        const flat_vector* vec_{};
        /// @brief Current iterator index.
        size_type index_{};
    };
};

