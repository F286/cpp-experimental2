#include "doctest.h"
#include "magica_voxel_io.h"
#include <filesystem>

TEST_CASE("magica_voxel round trip") {
    using block_t = flyweight_block_map<std::size_t, int>;
    block_t map; map.set(2,42); map.set(5,7);

    auto path = std::filesystem::path(PROJECT_SOURCE_DIR)/"vox_output"/"mv_round.vox";
    {
        magica_voxel_writer w("mv_round");
        w.add_frame(map);
    }
    CHECK(std::filesystem::file_size(path) > 0);

    {
        magica_voxel_reader<block_t> r(path.string());
        auto loaded = r.read();
        REQUIRE(loaded.size() == 1);
        CHECK(loaded[0] == map);
    }

    std::filesystem::remove(path);
}
