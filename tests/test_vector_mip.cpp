#include "doctest.h"
#include "vector_mip.h"
#include <ranges>
#include <numeric>

namespace checks {
    using vec_t   = vector_mip<std::experimental::simd<float>, 4>;
    using iter_t  = vec_t::iterator;
    using citer_t = vec_t::const_iterator;

    static_assert(std::random_access_iterator<iter_t>);
    static_assert(std::random_access_iterator<citer_t>);
    static_assert(std::sentinel_for<iter_t, iter_t>);
    static_assert(std::sentinel_for<citer_t, citer_t>);
    static_assert(std::ranges::forward_range<vec_t>);
    static_assert(std::ranges::common_range<vec_t>);
    static_assert(std::same_as<std::ranges::range_value_t<vec_t>, std::experimental::simd<float>>);
}

TEST_CASE("basic usage") {
    vector_mip<std::experimental::simd<float>, 4> v;
    CHECK(v.size() == 4);
    v[0] = std::experimental::simd<float>(1.0f);
    v[1] = std::experimental::simd<float>(2.0f);
    v[2] = std::experimental::simd<float>(3.0f);
    CHECK(static_cast<std::experimental::simd<float>>(v[0])[0] == doctest::Approx(1.0f));
    CHECK(static_cast<std::experimental::simd<float>>(v[1])[0] == doctest::Approx(2.0f));
    CHECK(static_cast<std::experimental::simd<float>>(v[2])[0] == doctest::Approx(3.0f));
}

TEST_CASE("optimize patches") {
    vector_mip<std::experimental::simd<float>, 4> v;
    v[1] = std::experimental::simd<float>(4.0f);
    v[3] = std::experimental::simd<float>(5.0f);
    CHECK(v.patch_count() == 2);
    v.optimize(0);
    CHECK(v.patch_count() == 0);
    CHECK(static_cast<std::experimental::simd<float>>(v[1])[0] == doctest::Approx(4.0f));
    CHECK(static_cast<std::experimental::simd<float>>(v[3])[0] == doctest::Approx(5.0f));
}

TEST_CASE("remove low variance region") {
    using simd_t = std::experimental::simd<float>;
    vector_mip<simd_t, 4> v;
    v.insert_patch(0, std::vector<simd_t>{simd_t(1.0f), simd_t(1.0f)});
    v.insert_patch(2, std::vector<simd_t>{simd_t(1.0f), simd_t(-1.0f)});
    CHECK(v.patch_count() == 2);
    std::array<simd_t,4> before{v[0], v[1], v[2], v[3]};
    v.optimize(1);
    CHECK(v.patch_count() == 1);
    v[0] = simd_t(2.0f);
    CHECK(v.patch_count() == 2);
    CHECK(static_cast<simd_t>(v[0])[0] == doctest::Approx(2.0f));
    for(std::size_t i = 1; i < 4; ++i)
        CHECK(static_cast<simd_t>(v[i])[0] == doctest::Approx(before[i][0]));
}

