#pragma once

#include "flyweight_block_map.h"
#include <fstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <cstdint>

/// @brief Header structure for custom voxel files.
struct magvox_header {
    char     magic[4];
    uint32_t version;
    uint32_t block_size;
    uint32_t value_size;
    uint32_t num_blocks;
    uint32_t num_frames;
};

/// @brief RAII writer for flyweight_block_map frames.
template<typename Map>
class magica_voxel_writer {
public:
    /// @brief Open file for writing.
    explicit magica_voxel_writer(const std::string& path)
        : out_(path, std::ios::binary)
    {
        if (!out_) throw std::runtime_error("cannot open output file");
    }

    /// @brief Write frames to file.
    void write(const std::vector<Map>& frames) {
        auto view = Map::blocks();
        std::unordered_map<typename Map::block_key, uint32_t> index;
        std::vector<typename Map::block_array> unique;
        for (const auto& f : frames) {
            auto k = f.key();
            if (!index.count(k)) {
                index[k] = static_cast<uint32_t>(unique.size());
                unique.push_back(view.at(k));
            }
        }
        magvox_header header{};
        header.magic[0]='F'; header.magic[1]='W'; header.magic[2]='M'; header.magic[3]='V';
        header.version = 1;
        header.block_size = Map::block_size;
        header.value_size = sizeof(typename Map::mapped_type);
        header.num_blocks = static_cast<uint32_t>(unique.size());
        header.num_frames = static_cast<uint32_t>(frames.size());
        out_.write(reinterpret_cast<const char*>(&header), sizeof(header));
        for (const auto& arr : unique) {
            for (auto vk : arr) {
                auto v = view.value_data(vk);
                out_.write(reinterpret_cast<const char*>(&v), sizeof(v));
            }
        }
        for (const auto& f : frames) {
            uint32_t i = index[f.key()];
            out_.write(reinterpret_cast<const char*>(&i), sizeof(i));
        }
    }

    /// @brief Close file on destruction.
    ~magica_voxel_writer() = default;
private:
    std::ofstream out_{};
};

/// @brief RAII reader for flyweight_block_map frames.
template<typename Map>
class magica_voxel_reader {
public:
    /// @brief Open file for reading.
    explicit magica_voxel_reader(const std::string& path)
        : in_(path, std::ios::binary)
    {
        if (!in_) throw std::runtime_error("cannot open input file");
    }

    /// @brief Read frames from file.
    std::vector<Map> read() {
        auto view = Map::blocks();
        magvox_header header{};
        in_.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (header.magic[0] != 'F' || header.magic[1] != 'W' ||
            header.magic[2] != 'M' || header.magic[3] != 'V')
            throw std::runtime_error("invalid file");
        if (header.block_size != Map::block_size ||
            header.value_size != sizeof(typename Map::mapped_type))
            throw std::runtime_error("incompatible map type");
        std::vector<typename Map::block_array> blocks(header.num_blocks);
        for (auto& arr : blocks) {
            for (auto& vk : arr) {
                typename Map::mapped_type v{};
                in_.read(reinterpret_cast<char*>(&v), sizeof(v));
                vk = view.insert_value(v);
            }
        }
        std::vector<Map> frames(header.num_frames);
        for (auto& f : frames) {
            uint32_t idx{};
            in_.read(reinterpret_cast<char*>(&idx), sizeof(idx));
            auto const& arr = blocks.at(idx);
            for (std::size_t i=0;i<Map::block_size;++i) {
                auto val = view.value_data(arr[i]);
                if (val != typename Map::mapped_type{})
                    f.set(static_cast<typename Map::key_type>(i), val);
            }
        }
        return frames;
    }

    /// @brief Close file on destruction.
    ~magica_voxel_reader() = default;

private:
    std::ifstream in_{};
};

