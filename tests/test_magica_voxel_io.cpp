#include "doctest.h"
#include "magica_voxel_io.h"
#include <filesystem>

TEST_CASE("magica_voxel round trip") {
    using block_t = flyweight_block_map<std::size_t, int>;
    block_t a; a.set(2,42);
    block_t b; b.set(3,7);
    std::vector<block_t> frames{a,b,a};
    auto path = std::filesystem::temp_directory_path()/"mv_round.vox";
    {
        magica_voxel_writer<block_t> w(path.string());
        w.write(frames);
    }
    magica_voxel_reader<block_t> r(path.string());
    auto loaded = r.read();
    CHECK(loaded.size() == frames.size());
    for(std::size_t i=0;i<frames.size();++i) {
        CHECK(loaded[i].key() == frames[i].key());
    }
    std::filesystem::remove(path);
}

TEST_CASE("writer deduplicates blocks") {
    using block_t = flyweight_block_map<std::size_t, int>;
    block_t a; a.set(1,1);
    block_t b = a;
    std::vector<block_t> frames{a,b};
    auto path = std::filesystem::temp_directory_path()/"mv_dedupe.vox";
    {
        magica_voxel_writer<block_t> w(path.string());
        w.write(frames);
    }
    auto size = std::filesystem::file_size(path);
    std::size_t expected = sizeof(magvox_header) + block_t::block_size*sizeof(int) + frames.size()*sizeof(uint32_t);
    CHECK(size == expected);
    std::filesystem::remove(path);
}
