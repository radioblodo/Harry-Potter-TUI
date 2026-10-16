#include "common/map.hpp"

#include <fstream>
#include <stdexcept>
#include <string>

namespace hogwarts {

char Map::at(int x, int y) const {
  if (!in_bounds(x, y)) {
    return '#';
  }
  return cells[static_cast<std::size_t>(y * width + x)];
}

bool Map::in_bounds(int x, int y) const {
  return x >= 0 && y >= 0 && x < width && y < height;
}

bool Map::blocked(int x, int y) const {
  const char tile = at(x, y);
  return tile == '#' || tile == ' ';
}

std::optional<Vec2i> Map::find(char needle) const {
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      if (at(x, y) == needle) {
        return Vec2i{x, y};
      }
    }
  }
  return std::nullopt;
}

Vec2i Map::spawn() const {
  if (const auto at_mark = find('@')) {
    return *at_mark;
  }
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      if (!blocked(x, y)) {
        return {x, y};
      }
    }
  }
  return {1, 1};
}

Map load_map(const std::filesystem::path& path) {
  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("cannot open map: " + path.string());
  }

  Map map;
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty()) {
      continue;
    }
    // "# comment" — real walls are "##..." or "#." with no space after '#'.
    if (line.size() >= 2 && line[0] == '#' && line[1] == ' ') {
      continue;
    }
    if (map.width == 0) {
      map.width = static_cast<int>(line.size());
    } else if (static_cast<int>(line.size()) < map.width) {
      line.append(static_cast<std::size_t>(map.width) - line.size(), ' ');
    } else if (static_cast<int>(line.size()) > map.width) {
      line.resize(static_cast<std::size_t>(map.width));
    }
    map.cells.insert(map.cells.end(), line.begin(), line.end());
    ++map.height;
  }

  if (map.width == 0 || map.height == 0) {
    throw std::runtime_error("empty map: " + path.string());
  }
  return map;
}

}  // namespace hogwarts
