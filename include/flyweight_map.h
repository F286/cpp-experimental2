#pragma once
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <cassert>
#include <ranges>
#include <type_traits>

// flyweight_map is an immutable, deduplicating associative container inspired by std::unordered_map.
// It stores unique immutable values of type T, assigning each a stable, compact 32-bit key ("handle").
// 
// - Values inserted into flyweight_map are deduplicated: inserting an equal value returns the existing handle.
// - Stored values cannot be modified after insertion, ensuring correctness of deduplication.
// - Only a single instance of flyweight_map<T> is allowed per type T, enabling efficient iteration.
// - Provides lightweight const_iterators (32-bit handles only) that dereference via a static pointer.
// - Fully compliant with C++23 standard ranges and concepts, enabling seamless integration with modern algorithms.
//
// Example usage:
//
//   flyweight_map<std::string> map;
//   auto key = map.insert("example");
//   const std::string* value = map.find(key);
//
// Iteration example:
//
//   for (const auto& [key, value] : map.items()) {
//       // use key and value
//   }
//
// Important notes:
//
// - Enforces single-instance-per-type constraint with runtime assertions.
// - Values are exposed strictly as const references, maintaining immutability.
// - Lightweight iterators improve memory efficiency and iteration performance.
//

template <typename T>
class flyweight_map {
public:
    using key_type = uint32_t;
    using mapped_type = T;
    using value_type = std::pair<const key_type, const T>;
    using size_type = std::size_t;

    flyweight_map() {
        assert(instance_ == nullptr && "Only one flyweight_map instance per type allowed.");
        instance_ = this;
    }

    ~flyweight_map() {
        instance_ = nullptr;
    }

    flyweight_map(const flyweight_map&) = delete;
    flyweight_map& operator=(const flyweight_map&) = delete;

    // Inserts a value if not present; returns existing key otherwise.
    key_type insert(const T& value) {
        auto [it, inserted] = value_to_key_.try_emplace(value, static_cast<key_type>(storage_.size()));
        if (inserted) {
            storage_.emplace_back(value);
        }
        return it->second;
    }

    bool contains(key_type k) const noexcept {
        return k < storage_.size();
    }

    const T* find(key_type k) const noexcept {
        return contains(k) ? &storage_[k] : nullptr;
    }

    size_type size() const noexcept {
        return storage_.size();
    }

    void clear() noexcept {
        value_to_key_.clear();
        storage_.clear();
    }

    // Const_iterator stores only keys
    class const_iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using iterator_concept = std::forward_iterator_tag;
        using value_type = flyweight_map::value_type;
        using difference_type = std::ptrdiff_t;
        using reference = value_type;

        const_iterator() = default;
        explicit const_iterator(key_type k) noexcept : key_(k) {}

        const_iterator(const const_iterator&) = default;
        const_iterator& operator=(const const_iterator&) = default;

        const_iterator& operator++() noexcept { ++key_; return *this; }
        const_iterator operator++(int) noexcept { auto tmp = *this; ++key_; return tmp; }

        bool operator==(const const_iterator&) const noexcept = default;

        value_type operator*() const noexcept {
            assert(instance_);
            return {key_, instance_->storage_[key_]};
        }

    private:
        key_type key_{0};
    };

    const_iterator begin() const noexcept { return const_iterator{0}; }
    const_iterator end() const noexcept { return const_iterator{static_cast<key_type>(storage_.size())}; }

    friend const_iterator begin(const flyweight_map& m) noexcept { return m.begin(); }
    friend const_iterator end(const flyweight_map& m) noexcept { return m.end(); }


    // Ranges-compliant views explicitly protect constness
    auto keys() const noexcept {
        return std::views::iota(key_type{0}, static_cast<key_type>(storage_.size()));
    }

    auto values() const noexcept {
        return std::views::transform(storage_, [](const T& v) -> const T& { return v; });
    }

    auto items() const noexcept {
        return std::views::transform(keys(), [this](key_type k) -> value_type {
            return {k, storage_[k]};
        });
    }

private:
    inline static flyweight_map* instance_{nullptr};
    std::unordered_map<T, key_type> value_to_key_;
    std::vector<T> storage_;

};


