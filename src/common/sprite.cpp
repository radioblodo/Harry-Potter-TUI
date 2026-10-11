#include "common/sprite.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace hogwarts {
namespace {

void append_utf8(std::string& out, char32_t cp) {
  if (cp < 0x80) {
    out.push_back(static_cast<char>(cp));
  } else if (cp < 0x800) {
    out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else if (cp < 0x10000) {
    out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else {
    out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
}

bool is_dot_on(char c) {
  return c == '#' || c == 'X' || c == 'x' || c == '1';
}

TileKind parse_kind(std::string_view s) {
  if (s == "floor") return TileKind::Floor;
  if (s == "actor") return TileKind::Actor;
  if (s == "building") return TileKind::Building;
  if (s == "ui") return TileKind::Ui;
  throw std::runtime_error("unknown kind '" + std::string(s) +
                           "' (use floor, actor, building, ui)");
}

std::string trim(std::string s) {
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
    s.erase(s.begin());
  }
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
    s.pop_back();
  }
  return s;
}

char32_t braille_cell(const Sprite& spr, int origin_x, int origin_y) {
  // Dot layout in U+2800:
  // 1 4
  // 2 5
  // 3 6
  // 7 8
  static constexpr int kBit[4][2] = {
      {0, 3},
      {1, 4},
      {2, 5},
      {6, 7},
  };
  char32_t bits = 0;
  for (int dy = 0; dy < 4; ++dy) {
    for (int dx = 0; dx < 2; ++dx) {
      const int x = origin_x + dx;
      const int y = origin_y + dy;
      if (x < 0 || y < 0 || x >= spr.dot_w || y >= spr.dot_h) {
        continue;
      }
      const auto on = spr.dots[static_cast<std::size_t>(y * spr.dot_w + x)];
      if (on) {
        bits |= (1u << kBit[dy][dx]);
      }
    }
  }
  return 0x2800 + bits;
}

void pack_braille(Sprite& spr) {
  const int cells_x = (spr.dot_w + 1) / 2;
  const int cells_y = (spr.dot_h + 3) / 4;
  spr.cell_w = cells_x;
  spr.cell_h = cells_y;
  spr.rows.assign(static_cast<std::size_t>(cells_y), std::string{});
  for (int cy = 0; cy < cells_y; ++cy) {
    auto& row = spr.rows[static_cast<std::size_t>(cy)];
    row.reserve(static_cast<std::size_t>(cells_x) * 3);
    for (int cx = 0; cx < cells_x; ++cx) {
      append_utf8(row, braille_cell(spr, cx * 2, cy * 4));
    }
  }
}

}  // namespace

int display_cols(std::string_view utf8) {
  int n = 0;
  for (unsigned char c : utf8) {
    if ((c & 0xC0) != 0x80) {
      ++n;
    }
  }
  return n;
}

Sprite load_sprite(const std::filesystem::path& path) {
  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("cannot open sprite: " + path.string());
  }

  Sprite spr;
  spr.name = path.stem().string();

  enum class Mode { Header, Dots, Cells };
  Mode mode = Mode::Header;
  int body_w = 0;
  int body_h = 0;
  int read_h = 0;
  std::string line;
  int lineno = 0;

  auto fail = [&](const std::string& msg) {
    throw std::runtime_error(path.string() + ':' + std::to_string(lineno) +
                             ": " + msg);
  };

  while (std::getline(in, line)) {
    ++lineno;
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }

    if (mode == Mode::Header) {
      const std::string t = trim(line);
      if (t.empty() || t.front() == '#') {
        continue;
      }
      std::istringstream ss(t);
      std::string key;
      ss >> key;
      if (key == "name") {
        ss >> spr.name;
      } else if (key == "kind") {
        std::string k;
        ss >> k;
        spr.kind = parse_kind(k);
      } else if (key == "dots") {
        if (!(ss >> body_w >> body_h) || body_w <= 0 || body_h <= 0) {
          fail("dots needs: dots <width> <height>");
        }
        spr.dot_w = body_w;
        spr.dot_h = body_h;
        spr.dots.assign(static_cast<std::size_t>(body_w * body_h), 0);
        mode = Mode::Dots;
        read_h = 0;
      } else if (key == "cells") {
        if (!(ss >> body_w >> body_h) || body_w <= 0 || body_h <= 0) {
          fail("cells needs: cells <width> <height>");
        }
        spr.cell_w = body_w;
        spr.cell_h = body_h;
        spr.rows.resize(static_cast<std::size_t>(body_h));
        mode = Mode::Cells;
        read_h = 0;
      } else {
        fail("unknown header key '" + key + "' (use name, kind, dots, cells)");
      }
      continue;
    }

    if (read_h >= body_h) {
      break;
    }

    if (mode == Mode::Dots) {
      for (int x = 0; x < body_w; ++x) {
        const char c = (x < static_cast<int>(line.size())) ? line[static_cast<std::size_t>(x)] : '.';
        spr.dots[static_cast<std::size_t>(read_h * body_w + x)] =
            is_dot_on(c) ? 1 : 0;
      }
    } else {
      if (static_cast<int>(line.size()) < body_w) {
        line.append(static_cast<std::size_t>(body_w) - line.size(), ' ');
      }
      spr.rows[static_cast<std::size_t>(read_h)] = line;
    }
    ++read_h;
  }

  if (mode == Mode::Header) {
    fail("missing 'dots W H' or 'cells W H' body");
  }
  if (read_h < body_h) {
    fail("expected " + std::to_string(body_h) + " body lines, got " +
         std::to_string(read_h));
  }

  if (mode == Mode::Dots) {
    pack_braille(spr);
  } else {
    int widest = 0;
    for (const auto& row : spr.rows) {
      widest = std::max(widest, display_cols(row));
    }
    spr.cell_w = std::max(spr.cell_w, widest);
  }
  return spr;
}

}  // namespace hogwarts
