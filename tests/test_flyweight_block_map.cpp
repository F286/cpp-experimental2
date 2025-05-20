#include "doctest.h"
#include "flyweight_block_map.h"
#include <vector>
#include <algorithm>
#include <ranges>

namespace checks {
    using block_t  = flyweight_block_map<std::size_t, int>;
    using citer_t  = block_t::const_iterator;

    static_assert(std::forward_iterator<citer_t>);
    static_assert(std::sentinel_for<citer_t, citer_t>);
    static_assert(std::ranges::forward_range<block_t>);
    static_assert(std::ranges::common_range<block_t>);
    static_assert(std::same_as<std::ranges::range_value_t<block_t>,
                               std::pair<const typename block_t::key_type, int>>);
}

TEST_CASE("default initialized zeros") {
    flyweight_block_map<std::size_t, int> b;
    for(std::size_t i = 0; i < b.size(); ++i) {
        CHECK(b[i] == 0);
    }
}

TEST_CASE("set and retrieve values") {
    flyweight_block_map<std::size_t, int> b;
    b.set(3, 42);
    CHECK(b[3] == 42);
    b.set(3, 7);
    CHECK(b[3] == 7);
}

TEST_CASE("deduplication reacts to mutations") {
    flyweight_block_map<std::size_t, int> a;
    flyweight_block_map<std::size_t, int> b;
    a.set(2, 10);
    b.set(2, 10);
    CHECK(a.key() == b.key());
    b.set(1, 5);
    CHECK(a.key() != b.key());
    a.set(1, 5);
    CHECK(a.key() == b.key());
}

TEST_CASE("iterate indices") {
    flyweight_block_map<std::size_t, int> b;
    b.set(1, 3);
    std::vector<int> vals;
    for(auto const& p : b) {
        vals.push_back(p.second);
    }
    std::vector<int> expect{0,3,0,0,0,0,0,0};
    CHECK(vals == expect);
}

