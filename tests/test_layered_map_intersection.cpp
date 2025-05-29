#include "doctest.h"
#include "layered_map.h"
#include "magica_voxel_io.h"
#include "positions.h"
#include <algorithm>
#include <limits>
#include <numbers>
#include <ranges>
#include <vector>

/// @brief Create an axis aligned box filled with a value.
static layered_map<int> make_box(GlobalPosition min_corner,
                                 GlobalPosition max_corner,
                                 int value = 2) {
  layered_map<int> map;
  for (std::uint32_t x = min_corner.x; x < max_corner.x; ++x)
    for (std::uint32_t y = min_corner.y; y < max_corner.y; ++y)
      for (std::uint32_t z = min_corner.z; z < max_corner.z; ++z)
        map[GlobalPosition{x, y, z}] = value;
  return map;
}

/// @brief Create a solid sphere filled with a value.
static layered_map<int> make_sphere(GlobalPosition center,
                                    std::uint32_t radius,
                                    int value = 2) {
  layered_map<int> map;
  const int r = static_cast<int>(radius);
  const int r2 = r * r;
  for (int dx = -r; dx <= r; ++dx)
    for (int dy = -r; dy <= r; ++dy)
      for (int dz = -r; dz <= r; ++dz)
        if (dx * dx + dy * dy + dz * dz <= r2)
          map[GlobalPosition{center.x + dx, center.y + dy, center.z + dz}] =
              value;
  return map;
}

/// @brief Compute bounding box of positions in a layered_map.
static std::pair<GlobalPosition, GlobalPosition>
compute_bounds(layered_map<int> const &map) {
  GlobalPosition min{std::numeric_limits<std::uint32_t>::max(),
                     std::numeric_limits<std::uint32_t>::max(),
                     std::numeric_limits<std::uint32_t>::max()};
  GlobalPosition max{0, 0, 0};
  for (auto it = map.cbegin(); it != map.cend(); ++it) {
    min.x = std::min(min.x, it->first.x);
    min.y = std::min(min.y, it->first.y);
    min.z = std::min(min.z, it->first.z);
    max.x = std::max(max.x, it->first.x);
    max.y = std::max(max.y, it->first.y);
    max.z = std::max(max.z, it->first.z);
  }
  return {min, max};
}

TEST_CASE("layered_map intersection sphere box") {
  auto box = make_box(GlobalPosition{0, 0, 0}, GlobalPosition{20, 20, 20});
  auto sphere = make_sphere(GlobalPosition{15, 10, 10}, 12);

  CHECK(box.size() == 8000);
  double expected_volume = 4.0 / 3.0 * std::numbers::pi * 1728.0;
  CHECK(static_cast<double>(sphere.size()) ==
        doctest::Approx(expected_volume).epsilon(0.15));

  layered_map<int> inter;
  std::ranges::set_intersection(
      box, sphere, std::inserter(inter, inter.end()),
      [](auto const &a, auto const &b) { return a.first < b.first; });

  layered_map<int> manual;
  for (auto const &[pos, val] : sphere)
    if (box.find(pos) != box.end())
      manual[pos] = val;

  CHECK(!manual.empty());
  CHECK(manual.size() < sphere.size());
  CHECK(inter.size() == manual.size());
  CHECK(std::ranges::equal(inter, manual,
                           [](auto const &a, auto const &b) {
                             return a.first == b.first && a.second == b.second;
                           }));

  auto bounds = compute_bounds(inter);
  CHECK(bounds.first < bounds.second);
  CHECK(bounds.second < GlobalPosition{20, 20, 20});

  magica_voxel_writer writer("intersection");
  writer.add_frame(inter);
}
