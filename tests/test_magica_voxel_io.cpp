#include "doctest.h"
#include "magica_voxel_io.h"
#include "temp_voxel_path.h"

TEST_CASE("magica_voxel round trip") {
    using block_t = flyweight_block_map<std::size_t, int>;
    block_t map; map.set(2,42); map.set(5,7);

    voxels::temp_file tmp("mv_round.vox");
    {
        magica_voxel_writer<block_t> w(tmp.path().string());
        w.write({map});
    }
    CHECK(std::filesystem::file_size(tmp.path()) > 0);

    {
        magica_voxel_reader<block_t> r(tmp.path().string());
        auto loaded = r.read();
        REQUIRE(loaded.size() == 1);
        CHECK(loaded[0] == map);
    }
    // file removed when tmp goes out of scope
}
