#include "doctest.h"
#include "chunk_map.h"
#include "flyweight_block_map.h"
#include <vector>
#include <algorithm>
#include <ranges>

TEST_CASE("chunk_map with flyweight_block_map inner") {
    using inner_t = flyweight_block_map<LocalPosition, int>;
    using cm_t = chunk_map<int, inner_t>;

    cm_t cm;
    cm[GlobalPosition{1}] = 7;       // chunk 0, local 1
    cm[GlobalPosition{37}] = 3;      // chunk 1, local 5
    CHECK(cm.size() == 2);

    CHECK(cm.at(GlobalPosition{1}) == 7);
    CHECK(cm[GlobalPosition{37}] == 3);

    auto it1 = cm.find(GlobalPosition{1});
    CHECK(it1 != cm.end());
    CHECK(it1->second == 7);
    auto it2 = cm.find(GlobalPosition{37});
    CHECK(it2 != cm.end());
    CHECK(it2->second == 3);

    cm.erase(GlobalPosition{1});
    CHECK(cm.size() == 1);
    CHECK(cm.find(GlobalPosition{1}) == cm.end());
}
