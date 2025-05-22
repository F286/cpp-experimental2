#include "doctest.h"
#include "flat_vector.h"
#include "flat_vector_array_packed.h"
#include <vector>
#include <array>
#include <ranges>

namespace checks {
    using vec_t   = flat_vector<array_packed<64, int>>;
    using iter_t  = vec_t::iterator;
    using citer_t = vec_t::const_iterator;

    static_assert(std::forward_iterator<iter_t>);
    static_assert(std::forward_iterator<citer_t>);
    static_assert(std::sentinel_for<iter_t, iter_t>);
    static_assert(std::sentinel_for<citer_t, citer_t>);
    static_assert(std::ranges::forward_range<vec_t>);
    static_assert(std::ranges::common_range<vec_t>);
    static_assert(std::same_as<std::ranges::range_value_t<vec_t>, array_packed<64, int>>);
}

TEST_CASE("basic usage") {
    array_packed<64, int> a;
    a[0] = 1;
    a[1] = 4;
    a[1] = 5;
    a[3] = 42;

    flat_vector<array_packed<64, int>> v;
    v.push_back(a);
    v.resize(3);
    v[2][10] = 100;
    v[2][63] = 200;

    CHECK(v.size() == 3);
    CHECK(v[0][0] == 1);
    CHECK(v[0][1] == 5);
    CHECK(v[0][3] == 42);
    CHECK(v[2][10] == 100);
    CHECK(v[2][63] == 200);
}

TEST_CASE("compare with std::vector") {
    array_packed<64, int> src;
    for(int i=0;i<64;++i) src[i] = i;

    flat_vector<array_packed<64, int>> fv;
    std::vector<std::array<int,64>> sv;
    fv.push_back(src);
    sv.push_back({});
    for(int i=0;i<64;++i) sv[0][i] = i;

    REQUIRE(fv.size() == sv.size());
    for(std::size_t i=0;i<64;++i) {
        CHECK(fv[0][i] == sv[0][i]);
    }
}
