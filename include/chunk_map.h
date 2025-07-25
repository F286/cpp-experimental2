#pragma once

#include <map>
#include <vector>
#include <initializer_list>
#include <type_traits>
#include <iterator>
#include <algorithm>
#include <numeric>
#include <compare>
#include <stdexcept>
#include <utility>
#include <cstddef>

#include "arrow_proxy.h"
#include "positions.h"
#include "set_views.h"
    

template <
    typename T,
    typename InnerMap = std::map<LocalPosition, T>,
    typename OuterMap = std::map<ChunkPosition, InnerMap>
>
class chunk_map {
public:
    using key_type        = GlobalPosition;
    using mapped_type     = T;
    using value_type      = std::pair<const GlobalPosition, T>;
    using size_type       = std::size_t;

    // Nested iterator types
    class iterator;
    class const_iterator;

    chunk_map() = default;

    /** Returns true if no elements stored. */
    bool empty() const noexcept {
        return chunks_.empty();
    }

    /// @brief Total number of elements across all chunks.
    size_type size() const {
        return std::accumulate(chunks_.begin(), chunks_.end(), size_type{0},
            [](size_type total, const auto& pair) {
                return total + pair.second.size();
            });
    }

    /** Removes all elements (all chunks). */
    void clear() noexcept {
        chunks_.clear();
    }

    /** 
     * Access or create the element at the given GlobalPosition.
     * If the chunk or local entry does not exist, it will be created.
     * (Like std::map::operator[], inserts default T() if missing.)
     */
    /// @brief Access element, creating chunk/local entry if needed.
    decltype(auto) operator[](const GlobalPosition& gp) {
        auto [cp, lp] = split(gp);
        InnerMap& inner = chunks_[cp];
        return inner[lp];
    }

    /**
     * Access element at GlobalPosition, throwing if it does not exist.
     * (Like std::map::at().) 
     */
    /// @brief Access existing element or throw.
    decltype(auto) at(const GlobalPosition& gp) {
        auto [cp, lp] = split(gp);
        auto itChunk = chunks_.find(cp);
        if (itChunk == chunks_.end())
            throw std::out_of_range("chunk_map::at: chunk not found");
        auto itLocal = itChunk->second.find(lp);
        if (itLocal == itChunk->second.end())
            throw std::out_of_range("chunk_map::at: element not found");
        return itLocal->second;
    }
    /** const overload of at() */
    decltype(auto) at(const GlobalPosition& gp) const {
        auto [cp, lp] = split(gp);
        auto itChunk = chunks_.find(cp);
        if (itChunk == chunks_.end())
            throw std::out_of_range("chunk_map::at: chunk not found");
        auto itLocal = itChunk->second.find(lp);
        if (itLocal == itChunk->second.end())
            throw std::out_of_range("chunk_map::at: element not found");
        return itLocal->second;
    }

    /**
     * Returns iterator to the element at GlobalPosition, or end() if not found.
     * (If found, dereferencing the iterator yields a pair<GlobalPosition, T&>.)
     */
    iterator find(const GlobalPosition& gp) {
        auto [cp, lp] = split(gp);
        auto itChunk = chunks_.find(cp);
        if (itChunk == chunks_.end()) return end();
        auto itLocal = itChunk->second.find(lp);
        if (itLocal == itChunk->second.end()) return end();
        return iterator(this, itChunk, itLocal);
    }
    /** const overload of find() */
    const_iterator find(const GlobalPosition& gp) const {
        auto [cp, lp] = split(gp);
        auto itChunk = chunks_.find(cp);
        if (itChunk == chunks_.end()) return cend();
        auto itLocal = itChunk->second.find(lp);
        if (itLocal == itChunk->second.end()) return cend();
        return const_iterator(this, itChunk, itLocal);
    }

    /// @brief Insert value if key not present.
    std::pair<iterator, bool> insert(const value_type& v) {
        auto [cp, lp] = split(v.first);
        auto [outerIt, outerInserted] = chunks_.try_emplace(cp);
        (void)outerInserted;
        bool existed = outerIt->second.contains(lp);
        if (!existed) outerIt->second.insert_or_assign(lp, v.second);
        auto innerIt = outerIt->second.find(lp);
        return { iterator(this, outerIt, innerIt), !existed };
    }

    /// @brief Insert movable value if key not present.
    std::pair<iterator, bool> insert(value_type&& v) {
        auto [cp, lp] = split(v.first);
        auto [outerIt, outerInserted] = chunks_.try_emplace(cp);
        (void)outerInserted;
        bool existed = outerIt->second.contains(lp);
        if (!existed) outerIt->second.insert_or_assign(lp, std::move(v.second));
        auto innerIt = outerIt->second.find(lp);
        return { iterator(this, outerIt, innerIt), !existed };
    }

    /// @brief Insert value with hint.
    iterator insert(const iterator&, const value_type& v) {
        return insert(v).first;
    }

    /// @brief Insert movable value with hint.
    iterator insert(const iterator&, value_type&& v) {
        return insert(std::move(v)).first;
    }

    /** 
     * Erase the element at GlobalPosition. Returns number of elements removed (0 or 1).
     */
    size_type erase(const GlobalPosition& gp) {
        auto [cp, lp] = split(gp);
        auto itChunk = chunks_.find(cp);
        if (itChunk == chunks_.end()) return 0;
        auto itLocal = itChunk->second.find(lp);
        if (itLocal == itChunk->second.end()) return 0;
        itChunk->second.erase(lp);
        if (itChunk->second.empty()) {
            chunks_.erase(itChunk);
        }
        return 1;
    }

    /** Begin iterator (first element in the first non-empty chunk). */
    iterator begin() {
        auto itChunk = chunks_.begin();
        while (itChunk != chunks_.end() && itChunk->second.empty())
            ++itChunk;
        if (itChunk == chunks_.end())
            return end();
        return iterator(this, itChunk, itChunk->second.begin());
    }
    /** End iterator (past-the-end). */
    iterator end() {
        return iterator(this, chunks_.end(), typename InnerMap::iterator());
    }

    /** const begin iterator */
    const_iterator cbegin() const {
        auto itChunk = chunks_.begin();
        while (itChunk != chunks_.end() && itChunk->second.empty())
            ++itChunk;
        if (itChunk == chunks_.end())
            return cend();
        return const_iterator(this, itChunk, itChunk->second.begin());
    }
    /** const end iterator */
    const_iterator cend() const {
        return const_iterator(this, chunks_.end(), typename InnerMap::const_iterator());
    }

    /// @brief Begin iterator for constant map.
    const_iterator begin() const { return cbegin(); }
    /// @brief End iterator for constant map.
    const_iterator end() const { return cend(); }

    /// @brief Lazy overlap view of two maps.
    static auto overlap(const chunk_map& lhs, const chunk_map& rhs);
    /// @brief Overlap adaptor for piping.
    static auto overlap(const chunk_map& rhs);

    /// @brief Lazy subtraction view of two maps.
    static auto subtract(const chunk_map& lhs, const chunk_map& rhs);
    /// @brief Subtract adaptor for piping.
    static auto subtract(const chunk_map& rhs);

    /// @brief Lazy merge view of two maps.
    static auto merge(const chunk_map& lhs, const chunk_map& rhs);
    /// @brief Merge adaptor for piping.
    static auto merge(const chunk_map& rhs);

    /// @brief Lazy exclusive view of two maps.
    static auto exclusive(const chunk_map& lhs, const chunk_map& rhs);
    /// @brief Exclusive adaptor for piping.
    static auto exclusive(const chunk_map& rhs);

private:
    OuterMap chunks_;
        
    auto split(GlobalPosition g) const {
        return std::pair{ ChunkPosition{g}, LocalPosition{g} };
    }

    static GlobalPosition combine(ChunkPosition c, LocalPosition l) {
        return GlobalPosition(c) + GlobalPosition(l);
    }

public:
    /// @brief Forward iterator over all (GlobalPosition, value) pairs.
    class iterator {
    public:
        using difference_type = std::ptrdiff_t;
        using inner_reference = decltype(std::declval<typename InnerMap::iterator::reference>().second);
        using value_type = std::pair<GlobalPosition, std::remove_cvref_t<inner_reference>>;
        using reference  = std::pair<GlobalPosition, inner_reference>;
        using pointer    = arrow_proxy<reference>;
        using iterator_category = std::forward_iterator_tag;

        iterator() = default;
        iterator(chunk_map* parent,
                 typename OuterMap::iterator outerIt,
                 typename InnerMap::iterator innerIt)
            : parent_(parent), outerIt_(outerIt), innerIt_(innerIt) {}

        /// @brief Dereference to key/value pair.
        reference operator*() const {
            GlobalPosition gp = parent_->combine(outerIt_->first, innerIt_->first);
            return { gp, innerIt_->second };
        }
        
        pointer operator->() const { return pointer{**this}; }
        
        // Pre-increment
        iterator& operator++() {
            ++innerIt_;
            // Move to next non-empty chunk if needed
            while (outerIt_ != parent_->chunks_.end() &&
                   innerIt_ == outerIt_->second.end())
            {
                ++outerIt_;
                if (outerIt_ == parent_->chunks_.end()) break;
                innerIt_ = outerIt_->second.begin();
            }
            return *this;
        }
        iterator operator++(int)
        { iterator tmp = *this; ++(*this); return tmp; }
        // Equality / inequality
        bool operator==(iterator const& o) const {
            return parent_ == o.parent_
                && outerIt_ == o.outerIt_
                && (outerIt_ == parent_->chunks_.end() || innerIt_ == o.innerIt_);
        }
        bool operator!=(iterator const& o) const {
            return !(*this == o);
        }
    private:
        chunk_map* parent_{nullptr};
        typename OuterMap::iterator outerIt_;
        typename InnerMap::iterator innerIt_;
    };

    /// @brief const iterator over all (GlobalPosition, value) pairs.
    class const_iterator {
    public:
        using difference_type = std::ptrdiff_t;
        using inner_reference = decltype(std::declval<typename InnerMap::const_iterator::reference>().second);
        using value_type = std::pair<GlobalPosition, std::remove_cvref_t<inner_reference>>;
        using reference  = std::pair<GlobalPosition, inner_reference>;
        using pointer    = arrow_proxy<reference>;
        using iterator_category = std::forward_iterator_tag;

        const_iterator() = default;
        const_iterator(const chunk_map* parent,
                       typename OuterMap::const_iterator outerIt,
                       typename InnerMap::const_iterator innerIt)
            : parent_(parent), outerIt_(outerIt), innerIt_(innerIt) {}

        reference operator*() const {
            GlobalPosition gp = parent_->combine(outerIt_->first, innerIt_->first);
            return { gp, innerIt_->second };
        }
        
        pointer operator->() const { return pointer{**this}; }

        const_iterator& operator++() {
            ++innerIt_;
            while (outerIt_ != parent_->chunks_.end() &&
                   innerIt_ == outerIt_->second.end())
            {
                ++outerIt_;
                if (outerIt_ == parent_->chunks_.end()) break;
                innerIt_ = outerIt_->second.begin();
            }
            return *this;
        }
        const_iterator operator++(int)
        { const_iterator tmp = *this; ++(*this); return tmp; }
        bool operator==(const_iterator const& o) const {
            return parent_ == o.parent_
                && outerIt_ == o.outerIt_
                && (outerIt_ == parent_->chunks_.end() || innerIt_ == o.innerIt_);
        }
        bool operator!=(const_iterator const& o) const {
            return !(*this == o);
        }
    private:
        const chunk_map* parent_{nullptr};
        typename OuterMap::const_iterator outerIt_;
        typename InnerMap::const_iterator innerIt_;
    }; 

};

template<typename T, typename InnerMap, typename OuterMap>
auto chunk_map<T, InnerMap, OuterMap>::overlap(const chunk_map& lhs,
                                               const chunk_map& rhs)
{
    return views::overlap(lhs, rhs,
        [](auto const& a, auto const& b) { return a.first < b.first; });
}

template<typename T, typename InnerMap, typename OuterMap>
auto chunk_map<T, InnerMap, OuterMap>::overlap(const chunk_map& rhs)
{
    return views::overlap(rhs,
        [](auto const& a, auto const& b) { return a.first < b.first; });
}

template<typename T, typename InnerMap, typename OuterMap>
auto chunk_map<T, InnerMap, OuterMap>::subtract(const chunk_map& lhs,
                                                const chunk_map& rhs)
{
    return views::subtract(lhs, rhs,
        [](auto const& a, auto const& b) { return a.first < b.first; });
}

template<typename T, typename InnerMap, typename OuterMap>
auto chunk_map<T, InnerMap, OuterMap>::subtract(const chunk_map& rhs)
{
    return views::subtract(rhs,
        [](auto const& a, auto const& b) { return a.first < b.first; });
}

template<typename T, typename InnerMap, typename OuterMap>
auto chunk_map<T, InnerMap, OuterMap>::merge(const chunk_map& lhs,
                                             const chunk_map& rhs)
{
    return views::merge(lhs, rhs,
        [](auto const& a, auto const& b) { return a.first < b.first; });
}

template<typename T, typename InnerMap, typename OuterMap>
auto chunk_map<T, InnerMap, OuterMap>::merge(const chunk_map& rhs)
{
    return views::merge(rhs,
        [](auto const& a, auto const& b) { return a.first < b.first; });
}

template<typename T, typename InnerMap, typename OuterMap>
auto chunk_map<T, InnerMap, OuterMap>::exclusive(const chunk_map& lhs,
                                                 const chunk_map& rhs)
{
    return views::exclusive(lhs, rhs,
        [](auto const& a, auto const& b) { return a.first < b.first; });
}

template<typename T, typename InnerMap, typename OuterMap>
auto chunk_map<T, InnerMap, OuterMap>::exclusive(const chunk_map& rhs)
{
    return views::exclusive(rhs,
        [](auto const& a, auto const& b) { return a.first < b.first; });
}

/// @brief Write elements present in both chunk_maps to output.
template<typename T, typename InnerMap, typename OuterMap, typename OutIt>
OutIt set_intersection(const chunk_map<T, InnerMap, OuterMap>& lhs,
                       const chunk_map<T, InnerMap, OuterMap>& rhs,
                       OutIt out)
{
    for (auto&& e : chunk_map<T, InnerMap, OuterMap>::overlap(lhs, rhs))
        *out++ = e;
    return out;
}

/// @brief Write elements present in lhs but not rhs to output iterator.
template<typename T, typename InnerMap, typename OuterMap, typename OutIt>
OutIt set_difference(const chunk_map<T, InnerMap, OuterMap>& lhs,
                     const chunk_map<T, InnerMap, OuterMap>& rhs,
                     OutIt out)
{
    for (auto&& e : chunk_map<T, InnerMap, OuterMap>::subtract(lhs, rhs))
        *out++ = e;
    return out;
}

/// @brief Write all unique elements from both maps to output iterator.
template<typename T, typename InnerMap, typename OuterMap, typename OutIt>
OutIt set_union(const chunk_map<T, InnerMap, OuterMap>& lhs,
                const chunk_map<T, InnerMap, OuterMap>& rhs,
                OutIt out)
{
    for (auto&& e : chunk_map<T, InnerMap, OuterMap>::merge(lhs, rhs))
        *out++ = e;
    return out;
}

/// @brief Write elements present in exactly one map to output iterator.
template<typename T, typename InnerMap, typename OuterMap, typename OutIt>
OutIt set_symmetric_difference(const chunk_map<T, InnerMap, OuterMap>& lhs,
                               const chunk_map<T, InnerMap, OuterMap>& rhs,
                               OutIt out)
{
    for (auto&& e : chunk_map<T, InnerMap, OuterMap>::exclusive(lhs, rhs))
        *out++ = e;
    return out;
}

template<class>
struct is_chunk_map : std::false_type {};
template<typename T, typename InnerMap, typename OuterMap>
struct is_chunk_map<chunk_map<T, InnerMap, OuterMap>> : std::true_type {};

namespace std::ranges {
    /// @brief Convert a range to a chunk_map.
    template<class C, std::ranges::input_range R>
        requires is_chunk_map<std::remove_cvref_t<C>>::value
    C to(R&& r)
    {
        if constexpr (std::same_as<std::remove_cvref_t<R>, C>) {
            if constexpr (std::is_lvalue_reference_v<R>) {
                return r;
            } else {
                return std::move(r);
            }
        } else {
            C out;
            for (auto&& elem : r) {
                auto&& [gp, val] = elem;
                out[gp] = std::forward<decltype(val)>(val);
            }
            return out;
        }
    }
}


