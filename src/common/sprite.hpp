#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace hogwarts {

enum class TileKind { Floor, Actor, Building, Ui };

// A reusable stamp: authored as '#' / '.' dots (or raw terminal cells),
// then blitted as UTF-8 rows (Braille for dots mode).
struct Sprite {
  std::string name;
  TileKind kind = TileKind::Floor;

  int dot_w = 0;
  int dot_h = 0;
  std::vector<std::uint8_t> dots;  // 0/1, row-major, size dot_w * dot_h

  int cell_w = 0;  // terminal columns
  int cell_h = 0;  // terminal rows
  std::vector<std::string> rows;   // UTF-8, one line per terminal row
};

// Count terminal columns (code points; Braille and ASCII are width 1).
int display_cols(std::string_view utf8);

// Throws std::runtime_error on bad files.
Sprite load_sprite(const std::filesystem::path& path);

}  // namespace hogwarts
