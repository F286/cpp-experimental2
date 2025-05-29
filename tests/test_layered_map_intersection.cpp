#include "doctest.h"
#include "layered_map.h"
#include "magica_voxel_io.h"
#include "positions.h"
#include <algorithm>
#include <limits>
#include <numbers>
#include <ranges>
#include <vector>

/// @brief Create an axis aligned box of ones.
static layered_map<int> make_box(GlobalPosition origin, std::uint32_t sx,
                                 std::uint32_t sy, std::uint32_t sz) {
  layered_map<int> map;
  for (auto x : std::views::iota(0u, sx))
    for (auto y : std::views::iota(0u, sy))
      for (auto z : std::views::iota(0u, sz))
        map[GlobalPosition{origin.x + x, origin.y + y, origin.z + z}] = 1;
  return map;
}

/// @brief Create a sphere of ones.
static layered_map<int> make_sphere(GlobalPosition center,
                                    std::uint32_t radius) {
  layered_map<int> map;
  int r = static_cast<int>(radius);
  int r2 = r * r;
  for (auto dx : std::views::iota(-r, r + 1))
    for (auto dy : std::views::iota(-r, r + 1))
      for (auto dz : std::views::iota(-r, r + 1))
        if (dx * dx + dy * dy + dz * dz <= r2)
          map[GlobalPosition{center.x + dx, center.y + dy, center.z + dz}] = 1;
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
  auto box = make_box(GlobalPosition{0, 0, 0}, 10, 10, 10);
  auto sphere = make_sphere(GlobalPosition{5, 5, 5}, 3);

  CHECK(box.size() == 1000);
  CHECK(sphere.size() == 123);
  double expected_volume = 4.0 / 3.0 * std::numbers::pi * 27.0;
  CHECK(static_cast<double>(sphere.size()) ==
        doctest::Approx(expected_volume).epsilon(0.15));

  std::vector<std::pair<GlobalPosition, int>> inter_vec;
  std::ranges::set_intersection(
      box, sphere, std::back_inserter(inter_vec),
      [](auto const &a, auto const &b) { return a.first < b.first; });
  auto inter = std::ranges::to<layered_map<int>>(inter_vec);
  CHECK(static_cast<double>(inter.size()) ==
        doctest::Approx(expected_volume).epsilon(0.15));

  auto expected_bounds =
      std::make_pair(GlobalPosition{2, 2, 2}, GlobalPosition{8, 8, 8});
  auto bounds = compute_bounds(inter);
  CHECK(inter.size() == sphere.size());
  CHECK(bounds.first == expected_bounds.first);
  CHECK(bounds.second == expected_bounds.second);

    magica_voxel_writer writer("intersection");
    writer.add_frame(inter);
}
