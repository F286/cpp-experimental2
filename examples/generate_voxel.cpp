#include "magica_voxel_io.h"
#include <vector>

/// @brief Example program generating a MagicaVoxel file.
int main() {
    using block_t = flyweight_block_map<std::size_t, int>;
    block_t a; a.set(1, 42);
    block_t b; b.set(2, 7);
    std::vector<block_t> frames{a, b, a};
    magica_voxel_writer<block_t> writer("examples/sample.vox");
    writer.write(frames);
    return 0;
}
