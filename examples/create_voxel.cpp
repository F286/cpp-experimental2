#include "create_voxel.h"
#include "magica_voxel_io.h"
#include "flyweight_block_map.h"
#include "temp_voxel_path.h"
#include <iostream>

/// @brief Generate a simple VOX file and save it.
void create_voxel_example() {
    using block_t = flyweight_block_map<std::size_t, int>;

    block_t frame;
    frame.set(0, 1);
    frame.set(2, 3);
    frame.set(4, 5);

    auto path = voxels::make_path("simple_model.vox");
    magica_voxel_writer<block_t> writer(path.string());
    writer.write({frame});

    std::cout << "Wrote " << path << "\n";
}
