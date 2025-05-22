#include "magica_voxel_io.h"
#include "flyweight_block_map.h"
#include <iostream>

/// @brief Entry point generating a VOX file.
int main() {
    using block_t = flyweight_block_map<std::size_t, int>;

    block_t frame;
    frame.set(0, 1);
    frame.set(2, 3);
    frame.set(4, 5);

    magica_voxel_writer<block_t> writer("examples/simple_model.vox");
    writer.write({frame});

    std::cout << "Wrote examples/simple_model.vox\n";
}
