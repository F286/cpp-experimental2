#pragma once

#include "flyweight_block_map.h"
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#include <cstdint>
#include <cstring>

/*
    Excerpt from the official MagicaVoxel 0.99.5 VOX specification
    ---------------------------------------------------------------
    File Layout (RIFF style):

        'VOX ' (4 bytes)          // file id
        int32 version            // currently 150

        Chunk 'MAIN' {
            Chunk 'PACK'  : optional, number of models
            repeated {
                Chunk 'SIZE' : model dimensions
                Chunk 'XYZI' : voxel data
            }
            Chunk 'RGBA'  : optional palette
        }

    Chunk structure:
        char[4] id
        int32   content_bytes
        int32   children_bytes
        ...content
        ...children

    Only the basic chunks above are used here.  The format allows many more
    chunk types for scene graphs and materials.
*/

namespace magica_voxel_detail {
    /// @brief Default RGBA palette.
    inline constexpr uint32_t default_palette[256] = {
        0x00000000, 0xffffffff, 0xffccffff, 0xff99ffff, 0xff66ffff, 0xff33ffff, 0xff00ffff, 0xffffccff,
        0xffccccff, 0xff99ccff, 0xff66ccff, 0xff33ccff, 0xff00ccff, 0xffff99ff, 0xffcc99ff, 0xff9999ff,
        0xff6699ff, 0xff3399ff, 0xff0099ff, 0xffff66ff, 0xffcc66ff, 0xff9966ff, 0xff6666ff, 0xff3366ff,
        0xff0066ff, 0xffff33ff, 0xffcc33ff, 0xff9933ff, 0xff6633ff, 0xff3333ff, 0xff0033ff, 0xffff00ff,
        0xffcc00ff, 0xff9900ff, 0xff6600ff, 0xff3300ff, 0xff0000ff, 0xffffffcc, 0xffccffcc, 0xff99ffcc,
        0xff66ffcc, 0xff33ffcc, 0xff00ffcc, 0xffffcccc, 0xffcccccc, 0xff99cccc, 0xff66cccc, 0xff33cccc,
        0xff00cccc, 0xffff99cc, 0xffcc99cc, 0xff9999cc, 0xff6699cc, 0xff3399cc, 0xff0099cc, 0xffff66cc,
        0xffcc66cc, 0xff9966cc, 0xff6666cc, 0xff3366cc, 0xff0066cc, 0xffff33cc, 0xffcc33cc, 0xff9933cc,
        0xff6633cc, 0xff3333cc, 0xff0033cc, 0xffff00cc, 0xffcc00cc, 0xff9900cc, 0xff6600cc, 0xff3300cc,
        0xff0000cc, 0xffffff99, 0xffccff99, 0xff99ff99, 0xff66ff99, 0xff33ff99, 0xff00ff99, 0xffffcc99,
        0xffcccc99, 0xff99cc99, 0xff66cc99, 0xff33cc99, 0xff00cc99, 0xffff9999, 0xffcc9999, 0xff999999,
        0xff669999, 0xff339999, 0xff009999, 0xffff6699, 0xffcc6699, 0xff996699, 0xff666699, 0xff336699,
        0xff006699, 0xffff3399, 0xffcc3399, 0xff993399, 0xff663399, 0xff333399, 0xff003399, 0xffff0099,
        0xffcc0099, 0xff990099, 0xff660099, 0xff330099, 0xff000099, 0xffffff66, 0xffccff66, 0xff99ff66,
        0xff66ff66, 0xff33ff66, 0xff00ff66, 0xffffcc66, 0xffcccc66, 0xff99cc66, 0xff66cc66, 0xff33cc66,
        0xff00cc66, 0xffff9966, 0xffcc9966, 0xff999966, 0xff669966, 0xff339966, 0xff009966, 0xffff6666,
        0xffcc6666, 0xff996666, 0xff666666, 0xff336666, 0xff006666, 0xffff3366, 0xffcc3366, 0xff993366,
        0xff663366, 0xff333366, 0xff003366, 0xffff0066, 0xffcc0066, 0xff990066, 0xff660066, 0xff330066,
        0xff000066, 0xffffff33, 0xffccff33, 0xff99ff33, 0xff66ff33, 0xff33ff33, 0xff00ff33, 0xffffcc33,
        0xffcccc33, 0xff99cc33, 0xff66cc33, 0xff33cc33, 0xff00cc33, 0xffff9933, 0xffcc9933, 0xff999933,
        0xff669933, 0xff339933, 0xff009933, 0xffff6633, 0xffcc6633, 0xff996633, 0xff666633, 0xff336633,
        0xff006633, 0xffff3333, 0xffcc3333, 0xff993333, 0xff663333, 0xff333333, 0xff003333, 0xffff0033,
        0xffcc0033, 0xff990033, 0xff660033, 0xff330033, 0xff000033, 0xffffff00, 0xffccff00, 0xff99ff00,
        0xff66ff00, 0xff33ff00, 0xff00ff00, 0xffffcc00, 0xffcccc00, 0xff99cc00, 0xff66cc00, 0xff33cc00,
        0xff00cc00, 0xffff9900, 0xffcc9900, 0xff999900, 0xff669900, 0xff339900, 0xff009900, 0xffff6600,
        0xffcc6600, 0xff996600, 0xff666600, 0xff336600, 0xff006600, 0xffff3300, 0xffcc3300, 0xff993300,
        0xff663300, 0xff333300, 0xff003300, 0xffff0000, 0xffcc0000, 0xff990000, 0xff660000, 0xff330000,
        0xff0000ee, 0xff0000dd, 0xff0000bb, 0xff0000aa, 0xff000088, 0xff000077, 0xff000055, 0xff000044,
        0xff000022, 0xff000011, 0xff00ee00, 0xff00dd00, 0xff00bb00, 0xff00aa00, 0xff008800, 0xff007700,
        0xff005500, 0xff004400, 0xff002200, 0xff001100, 0xffee0000, 0xffdd0000, 0xffbb0000, 0xffaa0000,
        0xff880000, 0xff770000, 0xff550000, 0xff440000, 0xff220000, 0xff110000, 0xffeeeeee, 0xffdddddd,
        0xffbbbbbb, 0xffaaaaaa, 0xff888888, 0xff777777, 0xff555555, 0xff444444, 0xff222222, 0xff111111
    };
}

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

    /// @brief Write frames to VOX file.
    void write(const std::vector<Map>& frames)
    {
        // total byte count of the children inside MAIN
        uint32_t child_size = frames.size() > 1 ? 12 + 4 : 0; // PACK chunk

        std::vector<std::vector<std::array<uint8_t,4>>> voxel_data;
        voxel_data.reserve(frames.size());

        // convert maps into XYZI tuples and compute sizes
        for (const auto& frame : frames) {
            std::vector<std::array<uint8_t,4>> voxels;
            for (std::size_t i = 0; i < Map::block_size; ++i) {
                auto v = frame.at(static_cast<typename Map::key_type>(i));
                if (v != 0)
                    voxels.push_back({static_cast<uint8_t>(i), 0, 0,
                                      static_cast<uint8_t>(v)});
            }
            child_size += 12 + 12;                   // SIZE chunk
            child_size += 12 + 4 + voxels.size()*4;   // XYZI chunk
            voxel_data.push_back(std::move(voxels));
        }
        child_size += 12 + 1024; // RGBA palette

        // write file header and MAIN chunk
        out_.write("VOX ",4);
        write_u32(150);
        out_.write("MAIN",4);
        write_u32(0);           // no MAIN content
        write_u32(child_size);  // total size of children
        if (frames.size() > 1) {
            // number of models when more than one
            out_.write("PACK",4);
            write_u32(4);
            write_u32(0);
            write_u32(static_cast<uint32_t>(frames.size()));
        }
        for (std::size_t m=0;m<frames.size();++m) {
            // model dimensions (1D grid)
            out_.write("SIZE",4);
            write_u32(12);
            write_u32(0);
            write_u32(static_cast<uint32_t>(Map::block_size));
            write_u32(1);
            write_u32(1);

            const auto& voxels = voxel_data[m];
            out_.write("XYZI",4);
            write_u32(static_cast<uint32_t>(4 + voxels.size()*4));
            write_u32(0);
            write_u32(static_cast<uint32_t>(voxels.size()));
            for (auto const& v : voxels)
                out_.write(reinterpret_cast<const char*>(v.data()),4);
        }
        out_.write("RGBA",4);
        write_u32(1024);
        write_u32(0);
        for(uint32_t c : magica_voxel_detail::default_palette)
            write_u32(c);
    }

    /// @brief Close file on destruction.
    ~magica_voxel_writer() = default;

private:
    /// @brief Write a 32-bit integer.
    void write_u32(uint32_t v) { out_.write(reinterpret_cast<const char*>(&v),4); }

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

    /// @brief Read frames from VOX file.
    std::vector<Map> read()
    {
        char id[4];
        in_.read(id,4);
        if(std::strncmp(id,"VOX ",4)!=0) throw std::runtime_error("invalid file");
        uint32_t version; read_u32(version);
        if(version!=150) throw std::runtime_error("invalid version");

        // MAIN chunk
        in_.read(id,4);
        if(std::strncmp(id,"MAIN",4)!=0) throw std::runtime_error("invalid file");
        uint32_t content, children; read_u32(content); read_u32(children);
        uint32_t num_models = 1;
        std::streampos pos = in_.tellg();
        in_.read(id,4);
        if(std::strncmp(id,"PACK",4)==0) {
            // optional PACK chunk
            read_u32(content); read_u32(children); read_u32(num_models);
        } else {
            in_.seekg(pos);
        }
        std::vector<Map> frames(num_models);
        for(uint32_t m=0;m<num_models;++m) {
            // SIZE chunk
            in_.read(id,4);
            if(std::strncmp(id,"SIZE",4)!=0) throw std::runtime_error("missing SIZE");
            read_u32(content); read_u32(children);
            uint32_t sx,sy,sz; read_u32(sx); read_u32(sy); read_u32(sz);
            (void)sx; (void)sy; (void)sz;

            // XYZI chunk
            in_.read(id,4); if(std::strncmp(id,"XYZI",4)!=0) throw std::runtime_error("missing XYZI");
            read_u32(content); read_u32(children);
            uint32_t num_vox; read_u32(num_vox);
            for(uint32_t i=0;i<num_vox;++i) {
                uint8_t buf[4]; in_.read(reinterpret_cast<char*>(buf),4);
                if(buf[1]||buf[2]) continue;
                if(buf[0] < Map::block_size && buf[3]!=0)
                    frames[m].set(static_cast<typename Map::key_type>(buf[0]), static_cast<typename Map::mapped_type>(buf[3]));
            }
        }
        // optional RGBA palette chunk
        in_.read(id,4);
        if(std::strncmp(id,"RGBA",4)==0) {
            read_u32(content); read_u32(children);
            std::vector<uint32_t> dummy(256);
            for(auto& c : dummy) read_u32(c);
        }
        return frames;
    }

    /// @brief Close file on destruction.
    ~magica_voxel_reader() = default;

private:
    /// @brief Read a 32-bit integer.
    void read_u32(uint32_t& v) { in_.read(reinterpret_cast<char*>(&v),4); }

    std::ifstream in_{};
};
