#pragma once
#include <experimental/simd>
#include <vector>
#include <cstddef>
#include <numeric>
#include <algorithm>
#include <ranges>
#include "arrow_proxy.h"

/**
 * @brief Variable resolution vector of SIMD tiles.
 *
 * Acts like `std::vector<simd<T>>` but stores optional higher
 * resolution patches for selected ranges. Low variance patches can
 * be merged into the base layer via `optimize`.
 */
template<typename Simd, std::size_t N>
    requires std::experimental::is_simd_v<Simd>
class vector_mip {
public:
    /// @brief SIMD type used for all operations.
    using simd_type  = Simd;
    /// @brief Scalar type of each SIMD lane.
    using lane_type  = typename simd_type::value_type;
    /// @brief Value type exposed by the container.
    using value_type = simd_type;
    /// @brief Size type of the container.
    using size_type  = std::size_t;

    class reference;
    class const_reference;
    class iterator;
    class const_iterator;

    /// @brief Construct zero initialised container.
    vector_mip() : base_(N) {}

    /// @brief Number of SIMD tiles stored.
    static constexpr size_type size() noexcept { return N; }

    /// @brief Mutable element access without bounds checking.
    reference operator[](size_type idx) { return reference(*this, idx); }
    /// @brief Immutable element access without bounds checking.
    value_type operator[](size_type idx) const { return get(idx); }

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

    /// @brief Current number of high resolution patches.
    size_type patch_count() const noexcept { return patches_.size(); }

    /// @brief Insert patch covering sequential tiles.
    template<std::ranges::input_range R>
        requires std::same_as<std::ranges::range_value_t<R>, simd_type>
    void insert_patch(size_type start, R&& values);

    /// @brief Merge patches until at most max_patches remain.
    void optimize(size_type max_patches);

private:
    struct patch {
        /// @brief True if tile index lies within patch range.
        bool contains(size_type i) const { return i >= start && i < start + tiles; }

        size_type start{}; ///< @brief First tile index covered by the patch.
        size_type tiles{}; ///< @brief Number of SIMD tiles in the patch (power of two).
        std::vector<simd_type> data{}; ///< @brief Values stored relative to the base layer.
    };

    patch*      find_patch(size_type idx);
    const patch* find_patch(size_type idx) const;
    value_type  get(size_type idx) const;
    void        set(size_type idx, const simd_type& v);
    static simd_type patch_mean(const patch& p);
    static double    patch_variance(const patch& p);

    /// @brief Base layer values.
    std::vector<simd_type> base_{};
    /// @brief Additional fine resolution patches.
    std::vector<patch> patches_{};

public:
    /// @brief Proxy reference enabling assignment through operator[].
    class reference {
    public:
        /// @brief Construct proxy from container and index.
        reference(vector_mip& v, size_type i) : vec_(&v), index_(i) {}
        /// @brief Assign SIMD value through proxy.
        reference& operator=(const simd_type& v) { vec_->set(index_, v); return *this; }
        /// @brief Convert to value type.
        operator value_type() const { return vec_->get(index_); }
    private:
        vector_mip* vec_{}; ///< @brief Parent container pointer.
        size_type   index_{}; ///< @brief Index referenced.
    };

    /// @brief Proxy reference to constant element.
    class const_reference {
    public:
        /// @brief Construct proxy from container and index.
        const_reference(const vector_mip& v, size_type i) : vec_(&v), index_(i) {}
        /// @brief Convert to value type.
        operator value_type() const { return vec_->get(index_); }
    private:
        const vector_mip* vec_{}; ///< @brief Parent container pointer.
        size_type         index_{}; ///< @brief Index referenced.
    };

    /// @brief Random access iterator over mutable elements.
    class iterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using iterator_concept  = std::random_access_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = vector_mip::value_type;
        using reference         = vector_mip::reference;
        using pointer           = arrow_proxy<reference>;

        /// @brief Default constructed iterator.
        iterator() = default;
        /// @brief Construct from container and index.
        iterator(vector_mip* v, size_type i) : vec_(v), index_(i) {}
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
        vector_mip* vec_{}; ///< @brief Parent container pointer.
        size_type   index_{}; ///< @brief Current iterator index.
    };

    /// @brief Random access iterator over constant elements.
    class const_iterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using iterator_concept  = std::random_access_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = vector_mip::value_type;
        using reference         = vector_mip::const_reference;
        using pointer           = arrow_proxy<reference>;

        /// @brief Default constructed iterator.
        const_iterator() = default;
        /// @brief Construct from container and index.
        const_iterator(const vector_mip* v, size_type i) : vec_(v), index_(i) {}
        /// @brief Dereference to reference proxy.
        reference operator*() const { return { *vec_, index_ }; }
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
        reference operator[](difference_type n) const { return { *vec_, index_ + n }; }
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
        const vector_mip* vec_{}; ///< @brief Parent container pointer.
        size_type         index_{}; ///< @brief Current iterator index.
    };
};

/// @brief Locate highest resolution patch for tile index.
template<typename Simd, std::size_t N>
    requires std::experimental::is_simd_v<Simd>
auto vector_mip<Simd, N>::find_patch(size_type idx) -> patch* {
    patch* best = nullptr;
    for (auto& p : patches_)
        if (p.contains(idx) && (!best || p.tiles < best->tiles))
            best = &p;
    return best;
}

/// @brief Locate highest resolution patch for tile index (const).
template<typename Simd, std::size_t N>
    requires std::experimental::is_simd_v<Simd>
auto vector_mip<Simd, N>::find_patch(size_type idx) const -> const patch* {
    const patch* best = nullptr;
    for (auto const& p : patches_)
        if (p.contains(idx) && (!best || p.tiles < best->tiles))
            best = &p;
    return best;
}

/// @brief Retrieve value at tile index.
template<typename Simd, std::size_t N>
    requires std::experimental::is_simd_v<Simd>
auto vector_mip<Simd, N>::get(size_type idx) const -> value_type {
    simd_type base = base_[idx];
    if (auto const* p = find_patch(idx)) {
        return base + p->data[idx - p->start];
    }
    return base;
}

/// @brief Store value at tile index.
template<typename Simd, std::size_t N>
    requires std::experimental::is_simd_v<Simd>
void vector_mip<Simd, N>::set(size_type idx, const simd_type& v) {
    if (auto* p = find_patch(idx)) {
        p->data[idx - p->start] = v - base_[idx];
        return;
    }
    patch np;
    np.start = idx;
    np.tiles = 1;
    np.data.resize(1);
    np.data[0] = v - base_[idx];
    patches_.push_back(std::move(np));
}

/// @brief Insert patch from absolute values.
template<typename Simd, std::size_t N>
    requires std::experimental::is_simd_v<Simd>
template<std::ranges::input_range R>
    requires std::same_as<std::ranges::range_value_t<R>, typename vector_mip<Simd, N>::simd_type>
void vector_mip<Simd, N>::insert_patch(size_type start, R&& values) {
    patch p;
    p.start = start;
    p.tiles = std::ranges::distance(values);
    p.data.reserve(p.tiles);
    size_type i = 0;
    for (auto const& v : values) {
        p.data.push_back(v - base_[start + i]);
        ++i;
    }
    patches_.push_back(std::move(p));
}

/// @brief Compute mean of patch data.
template<typename Simd, std::size_t N>
    requires std::experimental::is_simd_v<Simd>
auto vector_mip<Simd, N>::patch_mean(const patch& p) -> simd_type {
    simd_type sum{};
    for (auto const& v : p.data) sum += v;
    return sum / static_cast<typename simd_type::value_type>(p.data.size());
}

/// @brief Compute variance of patch data (zero mean assumed).
template<typename Simd, std::size_t N>
    requires std::experimental::is_simd_v<Simd>
double vector_mip<Simd, N>::patch_variance(const patch& p) {
    using simd_type = typename vector_mip<Simd, N>::simd_type;
    double total = 0.0;
    for (auto const& v : p.data) {
        for (std::size_t i = 0; i < simd_type::size(); ++i) {
            auto d = static_cast<double>(v[i]);
            total += d * d;
        }
    }
    return total / static_cast<double>(p.data.size() * simd_type::size());
}

/// @brief Optimize patch storage keeping at most max_patches.
template<typename Simd, std::size_t N>
    requires std::experimental::is_simd_v<Simd>
void vector_mip<Simd, N>::optimize(size_type max_patches) {
    for (auto& p : patches_) {
        simd_type mean = patch_mean(p);
        for (size_type i = 0; i < p.tiles; ++i) {
            base_[p.start + i] += mean;
            p.data[i] -= mean;
        }
    }
    std::vector<size_type> order(patches_.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](size_type a, size_type b) {
        return patch_variance(patches_[a]) < patch_variance(patches_[b]);
    });
    size_type remove_count = patches_.size() > max_patches ? patches_.size() - max_patches : 0;
    std::vector<bool> remove(order.size(), false);
    for (size_type i = 0; i < remove_count; ++i) remove[order[i]] = true;
    std::vector<patch> keep;
    keep.reserve(patches_.size() - remove_count);
    for (size_type i = 0; i < patches_.size(); ++i) {
        if (remove[i]) {
            patch& p = patches_[i];
            for (size_type t = 0; t < p.tiles; ++t) base_[p.start + t] += p.data[t];
        } else {
            keep.push_back(std::move(patches_[i]));
        }
    }
    patches_.swap(keep);
}
