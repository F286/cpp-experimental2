#pragma once

// ──────────────────────────────────────────────────────────────────────────
//  flat_tree_map<size_t,bool>  – sparse bit-set presented as an ordered map
//  (single specialisation of a conceptual flat_tree_map template family)
// ──────────────────────────────────────────────────────────────────────────
#include <cstddef>
#include <cstdint>
#include <limits>
#include <bit>
#include <map>
#include <utility>
#include <iterator>
#include <concepts>
#include <ranges>
#include <algorithm>
#include <cassert>
#include <vector>

#include "arrow_proxy.h"

// ──────────────────────────────────────────────────────────────────────────
//  Primary template (empty – we only implement the requested specialisation)
// ──────────────────────────────────────────────────────────────────────────
template<class Key,class T>
class flat_tree_map;      // primary – left deliberately undefined

// ──────────────────────────────────────────────────────────────────────────
//  Specialisation for <std::size_t,bool>
// ──────────────────────────────────────────────────────────────────────────
template<>
class flat_tree_map<std::size_t, bool> {
public:
    // Reference proxy for operator[]
    class reference {
    public:
        reference(flat_tree_map &m, std::size_t idx) : map_(m), index_(idx) {}
        // conversion to bool
        operator bool() const {
            return map_.test(index_);
        }
        // assignment from bool
        reference &operator=(bool val) {
            if(val) map_.set(index_);
            else       map_.reset(index_);
            return *this;
        }
        // assignment from another reference
        reference &operator=(const reference &other) {
            return *this = static_cast<bool>(other);
        }
    private:
        flat_tree_map &map_;
        std::size_t index_;
    };

    class const_iterator;

    // Iterator yielding (key, value) for each set bit (value is always true)
    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = std::pair<std::size_t, bool>;
        using difference_type   = std::ptrdiff_t;
        using reference         = value_type;
        using pointer           = arrow_proxy<reference>;

        iterator() : map_(nullptr), index_(0) {}
        iterator(const flat_tree_map *m, std::size_t idx) : map_(m), index_(idx) {}

        // Dereference yields (key, true)
        value_type operator*() const {
            return {index_, true};
        }
        pointer operator->() const { return pointer{**this}; }
        // Prefix increment: advance to next set bit
        iterator &operator++() {
            if(map_ && index_ < map_->m_capacity) {
                std::size_t next_idx;
                if(map_->find_next(index_, next_idx)) {
                    index_ = next_idx;
                } else {
                    index_ = map_->m_capacity; // end sentinel
                }
            }
            return *this;
        }
        // Postfix increment
        iterator operator++(int) {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        bool operator==(const iterator &other) const {
            return map_ == other.map_ && index_ == other.index_;
        }
        bool operator!=(const iterator &other) const {
            return !(*this == other);
        }
    private:
        const flat_tree_map *map_;
        std::size_t index_;

        friend class const_iterator;
    };

    // ── NEW: read-only iterator over set bits ────────────────────────────
    class const_iterator {
        friend class flat_tree_map;
        const flat_tree_map *map_  = nullptr;
        std::size_t       index_  = 0;
        const_iterator(const flat_tree_map* m, std::size_t i) : map_(m), index_(i) {}
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = std::pair<std::size_t,bool>;
        using difference_type   = std::ptrdiff_t;
        using reference         = value_type;
        using pointer           = arrow_proxy<reference>;

        const_iterator() = default;
        // implicit conversion from mutable iterator
        const_iterator(const iterator& it) : map_(it.map_), index_(it.index_) {}

        reference operator*()  const { return { index_, true }; }
        pointer   operator->() const { return pointer{**this}; }

        const_iterator& operator++() {
            std::size_t next;
            if(!map_->find_next(index_,next)) index_=map_->m_capacity; else index_=next;
            return *this;
        }
        const_iterator operator++(int){ const_iterator tmp=*this; ++(*this); return tmp; }

        bool operator==(const const_iterator& rhs) const { return map_==rhs.map_ && index_==rhs.index_; }
        bool operator!=(const const_iterator& rhs) const { return !(*this==rhs); }
    };

    // Default constructor: start with capacity=1 (one leaf & root)
    flat_tree_map() : m_size(0), m_capacity(1), m_tree(1, false) {}

    // Check if bit at key is set
    bool test(std::size_t key) const {
        if(key >= m_capacity) return false;
        std::size_t offset = m_capacity - 1;
        return m_tree[offset + key];
    }

    // Set bit at key to true. Return true if it was changed.
    bool set(std::size_t key) {
        ensure_capacity(key);
        std::size_t leafIndex = (m_capacity - 1) + key;
        if(m_tree[leafIndex]) {
            return false; // already true
        }
        // Set leaf bit and update count
        m_tree[leafIndex] = true;
        ++m_size;
        // Propagate up: set each parent to true until already true
        while(leafIndex > 0) {
            std::size_t parent = (leafIndex - 1) / 2;
            if(m_tree[parent]) break;
            m_tree[parent] = true;
            leafIndex = parent;
        }
        return true;
    }

    // Reset bit at key to false. Return true if it was changed.
    bool reset(std::size_t key) {
        if(key >= m_capacity) return false;
        std::size_t leafIndex = (m_capacity - 1) + key;
        if(!m_tree[leafIndex]) {
            return false; // already false
        }
        // Clear leaf bit and update count
        m_tree[leafIndex] = false;
        --m_size;
        // Propagate up: clear parents only if both children are false
        while(leafIndex > 0) {
            std::size_t parent = (leafIndex - 1) / 2;
            std::size_t left   = 2 * parent + 1;
            std::size_t right  = left + 1;
            if(m_tree[left] || m_tree[right]) {
                break; // sibling is true, stop
            }
            m_tree[parent] = false;
            leafIndex = parent;
        }
        return true;
    }

    // Flip bit at key; return new value (true if now set, false if reset)
    bool flip(std::size_t key) {
        if(test(key)) {
            reset(key);
            return false;
        } else {
            set(key);
            return true;
        }
    }

    // Number of bits set (true)
    std::size_t size() const {
        return m_size;
    }
    // True if no bits are set
    bool empty() const {
        return (m_size == 0);
    }

    // Clear all bits (preserve capacity)
    void clear() {
        std::fill(m_tree.begin(), m_tree.end(), false);
        m_size = 0;
    }

    // Const [] returns bool
    bool operator[](std::size_t key) const {
        return test(key);
    }
    // Non-const [] returns proxy reference
    reference operator[](std::size_t key) {
        ensure_capacity(key);
        return reference(*this, key);
    }

    // ── iterator boiler-plate ────────────────────────────────────────────
    iterator       begin()       { if(empty()) return end(); return iterator(this,  find_first()); }
    iterator       end()         { return iterator(this, m_capacity); }

    const_iterator begin() const { if(empty()) return end(); return const_iterator(this,find_first()); }
    const_iterator end()   const { return const_iterator(this,m_capacity); }

    const_iterator cbegin()const { return begin(); }
    const_iterator cend()  const { return end();   }

    // ── ADL helpers for ranges ───────────────────────────────────────────
    friend iterator       begin(flat_tree_map& m)             { return m.begin(); }
    friend iterator       end  (flat_tree_map& m)             { return m.end();   }
    friend const_iterator begin(const flat_tree_map& m)       { return m.begin(); }
    friend const_iterator end  (const flat_tree_map& m)       { return m.end();   }

private:
    std::vector<bool> m_tree;  // Binary tree bits
    std::size_t m_size;        // Count of true bits
    std::size_t m_capacity;    // Number of leaf bits (power of 2)

    // Ensure the tree can handle index 'key', doubling as needed
    void ensure_capacity(std::size_t key) {
        if(key < m_capacity) return;
        std::size_t newCap = m_capacity;
        while(key >= newCap) {
            newCap = (newCap == 0 ? 1 : newCap * 2);
        }
        std::size_t oldCap    = m_capacity;
        std::size_t oldOffset = (oldCap > 0 ? oldCap - 1 : 0);
        m_capacity = newCap;
        std::vector<bool> newTree(2 * m_capacity - 1, false);

        // Copy old leaf bits
        if(oldCap > 0) {
            for(std::size_t i = 0; i < oldCap; ++i) {
                if(m_tree[oldOffset + i]) {
                    newTree[(m_capacity - 1) + i] = true;
                }
            }
        }
        // Build all internal bits from bottom up
        for(std::size_t i = m_capacity - 2; i < (std::size_t)-1; --i) {
            std::size_t left  = 2 * i + 1;
            std::size_t right = left + 1;
            newTree[i] = newTree[left] || newTree[right];
            if(i == 0) break;
        }
        m_tree.swap(newTree);
    }

    // Find the first set bit (returns m_capacity if none)
    std::size_t find_first() const {
        if(m_size == 0 || m_tree.empty() || !m_tree[0]) {
            return m_capacity;
        }
        std::size_t idx    = 0;
        std::size_t offset = m_capacity - 1;
        while(idx < offset) {
            std::size_t left = 2 * idx + 1;
            if(m_tree[left]) {
                idx = left;
            } else {
                idx = left + 1;
            }
        }
        return idx - offset;
    }

    // Find next set bit after 'current'. 
    // Returns true and sets 'next'; or false if none.
    bool find_next(std::size_t current, std::size_t &next) const {
        if(current >= m_capacity) return false;
        std::size_t offset    = m_capacity - 1;
        std::size_t leafIndex = offset + current + 1;
        if(leafIndex >= m_tree.size()) {
            return false;
        }
        // If immediate next leaf is set, take it
        if(m_tree[leafIndex]) {
            next = current + 1;
            return true;
        }
        // Otherwise, climb the tree to find a parent where we can go right
        std::size_t idx = leafIndex;
        while(idx > 0) {
            std::size_t parent = (idx - 1) / 2;
            std::size_t left   = 2 * parent + 1;
            std::size_t right  = left + 1;
            if(idx == left && m_tree[right]) {
                // Found a set bit in the right sibling subtree
                idx = right;
                while(idx < offset) {
                    std::size_t left2 = 2 * idx + 1;
                    if(m_tree[left2]) {
                        idx = left2;
                    } else {
                        idx = left2 + 1;
                    }
                }
                next = idx - offset;
                return true;
            }
            idx = parent;
        }
        return false;
    }
};


// ──────────────────────────────────────────────────────────────────────────
//  Compile-time conformance checks (C++20 concepts / ranges)
// ──────────────────────────────────────────────────────────────────────────
using test_map  = flat_tree_map<std::size_t,bool>;
using iter_t    = test_map::iterator;
using citer_t   = test_map::const_iterator;

static_assert(std::forward_iterator<iter_t>);
static_assert(std::forward_iterator<citer_t>);
static_assert(std::sentinel_for<iter_t,iter_t>);
static_assert(std::sentinel_for<citer_t,citer_t>);
static_assert(std::ranges::forward_range<test_map>);
static_assert(std::ranges::common_range<test_map>);
static_assert(std::same_as<std::ranges::range_value_t<test_map>, std::pair<std::size_t,bool>>);

