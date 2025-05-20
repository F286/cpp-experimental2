#pragma once

#include <map>
#include <vector>
#include <initializer_list>
#include <type_traits>
#include <iterator>
#include <algorithm>
#include <compare>
#include <stdexcept>
#include <utility>

#include "arrow_proxy.h"

// ── forward declarations so the class bodies can mention each other ──
struct GlobalPosition;
struct ChunkPosition;
struct LocalPosition;

// ───────────────── ChunkPosition & LocalPosition – simple bodies ─────
struct ChunkPosition {
    int value{};
    constexpr explicit ChunkPosition(int chunkIdx) : value(chunkIdx) {}
    constexpr explicit ChunkPosition(GlobalPosition);                 // declared, defined later
    constexpr auto operator<=>(ChunkPosition const&) const = default;
    constexpr bool operator==(ChunkPosition const&) const = default;
};

struct LocalPosition {
    int value{};
    constexpr explicit LocalPosition(int localIdx) : value(localIdx & 31) {}
    constexpr explicit LocalPosition(GlobalPosition);                  // declared, defined later
    constexpr auto operator<=>(LocalPosition const&) const = default;
    constexpr bool operator==(LocalPosition const&) const = default;
};

// ────────────────── GlobalPosition – now can mention C & L ───────────
struct GlobalPosition {
    int value{};
    constexpr explicit GlobalPosition(int v) : value(v) {}
    constexpr explicit GlobalPosition(ChunkPosition);   // declared, defined later
    constexpr explicit GlobalPosition(LocalPosition);   // declared, defined later

    constexpr auto operator<=>(GlobalPosition const&) const = default;
    constexpr bool operator==(GlobalPosition const&) const = default;

    constexpr GlobalPosition operator+(GlobalPosition other) const
    { return GlobalPosition{ value + other.value }; }
};

// ────────────────── out-of-class definitions (all types complete) ────
constexpr ChunkPosition::ChunkPosition(GlobalPosition g)
    : value(g.value >> 5) {}

constexpr LocalPosition::LocalPosition(GlobalPosition g)
    : value(g.value & 31) {}

constexpr GlobalPosition::GlobalPosition(ChunkPosition c)
    : value(c.value << 5) {}
    
constexpr GlobalPosition::GlobalPosition(LocalPosition l)
    : value(l.value) {}
    

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

    /** Total number of elements across all chunks. */
    size_type size() const {
        size_type count = 0;
        for (auto const& [cpos, inner] : chunks_)
            count += inner.size();
        return count;
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
    T& operator[](const GlobalPosition& gp) {
        auto [cp, lp] = split(gp);
        InnerMap& inner = chunks_[cp];  // create chunk if needed
        return inner[lp];              // create local element if needed
    }

    /**
     * Access element at GlobalPosition, throwing if it does not exist.
     * (Like std::map::at().) 
     */
    T& at(const GlobalPosition& gp) {
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
    const T& at(const GlobalPosition& gp) const {
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

    /** 
     * Erase the element at GlobalPosition. Returns number of elements removed (0 or 1).
     */
    size_type erase(const GlobalPosition& gp) {
        auto [cp, lp] = split(gp);
        auto itChunk = chunks_.find(cp);
        if (itChunk == chunks_.end()) return 0;
        auto itLocal = itChunk->second.find(lp);
        if (itLocal == itChunk->second.end()) return 0;
        itChunk->second.erase(itLocal);
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

private:
    OuterMap chunks_;
        
    auto split(GlobalPosition g) const {
        return std::pair{ ChunkPosition{g}, LocalPosition{g} };
    }

    static GlobalPosition combine(ChunkPosition c, LocalPosition l) {
        return GlobalPosition(c) + GlobalPosition(l);
    }

public:
    // Forward iterator over all (GlobalPosition, T) pairs.
    class iterator {
    public:
        using difference_type = std::ptrdiff_t;
        // We yield a pair<GlobalPosition, T&> on dereference
        using value_type = std::pair<GlobalPosition, T>;
        using reference  = std::pair<GlobalPosition, T&>;
        using pointer           = arrow_proxy<reference>;
        using iterator_category = std::forward_iterator_tag;

        iterator() = default;
        iterator(chunk_map* parent,
                 typename OuterMap::iterator outerIt,
                 typename InnerMap::iterator innerIt)
            : parent_(parent), outerIt_(outerIt), innerIt_(innerIt) {}

        // Dereference returns (GlobalPosition, T&) pair by value
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

    // const_iterator (similar to iterator but over const maps)
    class const_iterator {
    public:
        using difference_type = std::ptrdiff_t;
        using value_type = std::pair<GlobalPosition, T>;
        using reference  = std::pair<GlobalPosition, const T&>;
        using pointer           = arrow_proxy<reference>;
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

