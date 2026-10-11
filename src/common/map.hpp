#pragma once

#include "common/types.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace hogwarts {

struct Map {
  int width = 0;
  int height = 0;
  std::vector<char> cells;  // row-major, size == width * height

  [[nodiscard]] char at(int x, int y) const;
  [[nodiscard]] bool in_bounds(int x, int y) const;
  [[nodiscard]] bool blocked(int x, int y) const;
  [[nodiscard]] std::optional<Vec2i> find(char needle) const;
  [[nodiscard]] Vec2i spawn() const;  // '@' if present, else first open tile
};

// Throws std::runtime_error if the file cannot be read.
Map load_map(const std::filesystem::path& path);

}  // namespace hogwarts
