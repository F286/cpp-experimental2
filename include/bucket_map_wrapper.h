#pragma once

#include "bucket_map.h"

/// @brief Adapter exposing mutable map interface over bucket_map.
template<typename Key, typename T, typename Bucket = std::uint64_t>
class bucket_map_wrapper {
public:
    /// @brief Iterator type alias.
    using iterator = typename bucket_map<Key,T,Bucket>::iterator;
    /// @brief Const iterator type alias.
    using const_iterator = typename bucket_map<Key,T,Bucket>::const_iterator;

    /// @brief True if container has no elements.
    bool empty() const noexcept { return map_.empty(); }
    /// @brief Number of stored elements.
    std::size_t size() const noexcept { return map_.size(); }

    /// @brief Access value inserting default if missing.
    T& operator[](const Key& k) {
        if(!map_.contains(k)) map_.insert_or_assign(k, T{});
        return const_cast<T&>(map_.at(k));
    }

    /// @brief Access existing value.
    const T& at(const Key& k) const { return map_.at(k); }

    /// @brief Find key returning end if not found.
    iterator find(const Key& k) {
        if(!map_.contains(k)) return map_.end();
        auto it = map_.begin();
        for(; it != map_.end(); ++it) if(it->first == k) break;
        return it;
    }
    /// @brief Const find key.
    const_iterator find(const Key& k) const {
        if(!map_.contains(k)) return map_.end();
        auto it = map_.begin();
        for(; it != map_.end(); ++it) if(it->first == k) break;
        return it;
    }

    /// @brief Remove key and return count.
    std::size_t erase(const Key& k) { return map_.erase(k); }

    /// @brief Begin iterator.
    iterator begin() { return map_.begin(); }
    /// @brief End iterator.
    iterator end() { return map_.end(); }
    /// @brief Const begin iterator.
    const_iterator begin() const { return map_.begin(); }
    /// @brief Const end iterator.
    const_iterator end() const { return map_.end(); }

private:
    /// @brief Underlying bucket_map.
    bucket_map<Key,T,Bucket> map_{};
};

