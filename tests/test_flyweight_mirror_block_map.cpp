#include "doctest.h"
#include "flyweight_mirror_block_map.h"
#include <algorithm>
#include <vector>
#include <ranges>

namespace checks {
    using map_t   = flyweight_mirror_block_map<std::size_t, int>;
    using iter_t  = map_t::iterator;
    using citer_t = map_t::const_iterator;

    static_assert(std::forward_iterator<iter_t>);
    static_assert(std::forward_iterator<citer_t>);
    static_assert(std::sentinel_for<iter_t, iter_t>);
    static_assert(std::sentinel_for<citer_t, citer_t>);
    static_assert(std::ranges::forward_range<map_t>);
    static_assert(std::ranges::common_range<map_t>);
    static_assert(std::same_as<std::ranges::range_value_t<map_t>,
                               std::pair<const typename map_t::key_type, int>>);
}

TEST_CASE("default values") {
    flyweight_mirror_block_map<std::size_t, int> m;
    CHECK(m.empty());
    CHECK(m.size() == 0);
    for(std::size_t i = 0; i < 8; ++i) {
        CHECK(m[i] == 0);
    }
}

TEST_CASE("set and access") {
    flyweight_mirror_block_map<std::size_t, int> m;
    m.set(2, 5);
    m[3] = 7;
    CHECK(m[2] == 5);
    CHECK(m[3] == 7);
    CHECK(m.size() == 2);
    CHECK_FALSE(m.empty());
}

TEST_CASE("mirror deduplication") {
    flyweight_mirror_block_map<std::size_t, int> a;
    flyweight_mirror_block_map<std::size_t, int> b;
    for(std::size_t i=0;i<8;++i) a.set(i, static_cast<int>(i+1));
    for(std::size_t i=0;i<8;++i) b.set(7-i, static_cast<int>(i+1));
    CHECK(a.key() == b.key());
    for(std::size_t i=0;i<8;++i) CHECK(a[i] == static_cast<int>(i+1));
    for(std::size_t i=0;i<8;++i) CHECK(b[7-i] == static_cast<int>(i+1));
}

TEST_CASE("erase and clear") {
    flyweight_mirror_block_map<std::size_t, int> m;
    m.set(1, 10);
    CHECK(m.erase(1) == 1);
    CHECK(m.size() == 0);
    CHECK(m.erase(1) == 0);
    m.set(0, 1);
    m.clear();
    CHECK(m.empty());
    for(std::size_t i = 0; i < 8; ++i) {
        CHECK(m[i] == 0);
    }
}

TEST_CASE("ranges algorithms") {
    flyweight_mirror_block_map<std::size_t, int> m;
    m.set(0, 5);
    m.set(3, 2);
    auto cnt = std::ranges::count_if(m, [](auto const& p){ return p.second != 0; });
    CHECK(cnt == 2);
    std::vector<int> vals;
    std::ranges::transform(m, std::back_inserter(vals), [](auto const& p){ return p.second; });
    std::vector<int> expect{5,0,0,2,0,0,0,0};
    CHECK(vals == expect);
}
