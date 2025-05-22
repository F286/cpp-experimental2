#pragma once

#include <filesystem>
#include <string>
#include <system_error>

namespace voxels {

/// @brief Return path in the temporary voxels directory.
inline std::filesystem::path make_path(const std::string& name) {
    auto dir = std::filesystem::temp_directory_path() / "voxels";
    std::filesystem::create_directories(dir);
    return dir / name;
}

/// @brief RAII helper that deletes the file on destruction.
class temp_file {
public:
    /// @brief Construct with file name.
    explicit temp_file(const std::string& name)
        : path_(make_path(name)) {}

    /// @brief Remove file when the object goes out of scope.
    ~temp_file() { std::error_code ec; std::filesystem::remove(path_, ec); }

    /// @brief Access the full path.
    const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_{};
};

} // namespace voxels
