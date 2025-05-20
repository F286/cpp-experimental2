#include "doctest.h"
#include "flyweight_mirror_block_map.h"
#include <algorithm>
#include <vector>

TEST_CASE("mirror deduplication") {
    flyweight_mirror_block_map<std::size_t, int> a;
    flyweight_mirror_block_map<std::size_t, int> b;
    for(std::size_t i=0;i<8;++i) a.set(i, static_cast<int>(i+1));
    for(std::size_t i=0;i<8;++i) b.set(7-i, static_cast<int>(i+1));
    CHECK(a.key() == b.key());
    for(std::size_t i=0;i<8;++i) CHECK(a[i] == static_cast<int>(i+1));
    for(std::size_t i=0;i<8;++i) CHECK(b[7-i] == static_cast<int>(i+1));
}

TEST_CASE("iteration order") {
    flyweight_mirror_block_map<std::size_t, int> m;
    m.set(1, 3);
    std::vector<int> vals;
    for(auto const& p : m) vals.push_back(p.second);
    std::vector<int> expect{0,3,0,0,0,0,0,0};
    CHECK(vals == expect);
}
