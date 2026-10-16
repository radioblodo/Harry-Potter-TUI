#include "session/raycast.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hogwarts::session {
namespace {

struct Camera {
  double x = 1.5;
  double y = 1.5;
  double dir_x = 0.0;
  double dir_y = -1.0;
  double plane_x = 0.66;
  double plane_y = 0.0;
};

struct Pix {
  char ch = ' ';
  std::uint8_t fg = 255;
  std::uint8_t bg = 0;
};

bool walkable(const Map& map, double x, double y) {
  // Small body radius keeps 1-tile doors/corridors from feeling blocked.
  const double r = 0.08;
  return !map.blocked(static_cast<int>(x - r), static_cast<int>(y - r)) &&
         !map.blocked(static_cast<int>(x + r), static_cast<int>(y - r)) &&
         !map.blocked(static_cast<int>(x - r), static_cast<int>(y + r)) &&
         !map.blocked(static_cast<int>(x + r), static_cast<int>(y + r));
}

void try_move(const Map& map, Camera& cam, double dx, double dy) {
  if (walkable(map, cam.x + dx, cam.y)) {
    cam.x += dx;
  }
  if (walkable(map, cam.x, cam.y + dy)) {
    cam.y += dy;
  }
}

void face(Camera& cam, double dx, double dy) {
  cam.dir_x = dx;
  cam.dir_y = dy;
  cam.plane_x = -dy * 0.66;
  cam.plane_y = dx * 0.66;
}

void turn_cardinal(Camera& cam, int quarter_turns) {
  // Snap to N/E/S/W and rotate relative to current facing. This makes repeated
  // Right presses go north -> east -> south -> west, like grid movement.
  int dir = 0;  // 0=N, 1=E, 2=S, 3=W
  if (std::abs(cam.dir_x) > std::abs(cam.dir_y)) {
    dir = (cam.dir_x > 0.0) ? 1 : 3;
  } else {
    dir = (cam.dir_y > 0.0) ? 2 : 0;
  }
  dir = (dir + quarter_turns) % 4;
  if (dir < 0) dir += 4;
  if (dir == 0) face(cam, 0.0, -1.0);
  if (dir == 1) face(cam, 1.0, 0.0);
  if (dir == 2) face(cam, 0.0, 1.0);
  if (dir == 3) face(cam, -1.0, 0.0);
}

void rotate(Camera& cam, double radians) {
  const double cos_r = std::cos(radians);
  const double sin_r = std::sin(radians);
  const double dx = cam.dir_x * cos_r - cam.dir_y * sin_r;
  const double dy = cam.dir_x * sin_r + cam.dir_y * cos_r;
  cam.dir_x = dx;
  cam.dir_y = dy;
  const double px = cam.plane_x * cos_r - cam.plane_y * sin_r;
  const double py = cam.plane_x * sin_r + cam.plane_y * cos_r;
  cam.plane_x = px;
  cam.plane_y = py;
}

// Distance → 256-color index. Use high-contrast greys for stone walls.
std::uint8_t wall_color(double dist, bool ns_side) {
  static constexpr std::uint8_t kNs[] = {250, 248, 246, 244, 240};
  static constexpr std::uint8_t kEw[] = {245, 243, 241, 239, 237};
  int band = static_cast<int>(dist / 1.15);
  band = std::clamp(band, 0, 4);
  return ns_side ? kNs[band] : kEw[band];
}

void paint_column(std::vector<std::vector<Pix>>& fb, int col, int draw_start,
                  int draw_end, std::uint8_t wall) {
  const int rows = static_cast<int>(fb.size());
  const int cols = fb.empty() ? 0 : static_cast<int>(fb[0].size());
  if (col < 0 || col >= cols) {
    return;
  }
  draw_start = std::clamp(draw_start, 0, rows - 1);
  draw_end = std::clamp(draw_end, 0, rows - 1);
  for (int y = 0; y < rows; ++y) {
    Pix& p = fb[static_cast<std::size_t>(y)][static_cast<std::size_t>(col)];
    p.ch = ' ';
    p.fg = 255;
    if (y < draw_start) {
      p.bg = 117;  // sky blue ceiling
      p.fg = 117;
      p.ch = ' ';
    } else if (y > draw_end) {
      // Plain black ground; no texture/glyphs so it doesn't read as gameplay UI.
      p.bg = 16;
      p.fg = 16;
      p.ch = ' ';
    } else {
      // Plain color wall; no glyph texture.
      p.bg = wall;
      p.fg = wall;
      p.ch = ' ';
    }
  }
}

// DDA raycast — same idea as Wolfenstein 3D / Lode Vandevenne's tutorial.
void render_view(std::vector<std::vector<Pix>>& fb, const Map& map,
                 const Camera& cam) {
  const int cols = fb.empty() ? 0 : static_cast<int>(fb[0].size());
  const int rows = static_cast<int>(fb.size());
  if (cols <= 0 || rows <= 0) {
    return;
  }

  for (int col = 0; col < cols; ++col) {
    const double camera_x = 2.0 * col / cols - 1.0;
    double ray_x = cam.dir_x + cam.plane_x * camera_x;
    double ray_y = cam.dir_y + cam.plane_y * camera_x;

    int map_x = static_cast<int>(cam.x);
    int map_y = static_cast<int>(cam.y);

    const double delta_x =
        (ray_x == 0.0) ? 1e30 : std::abs(1.0 / ray_x);
    const double delta_y =
        (ray_y == 0.0) ? 1e30 : std::abs(1.0 / ray_y);

    int step_x = 0;
    int step_y = 0;
    double side_x = 0.0;
    double side_y = 0.0;
    if (ray_x < 0.0) {
      step_x = -1;
      side_x = (cam.x - map_x) * delta_x;
    } else {
      step_x = 1;
      side_x = (map_x + 1.0 - cam.x) * delta_x;
    }
    if (ray_y < 0.0) {
      step_y = -1;
      side_y = (cam.y - map_y) * delta_y;
    } else {
      step_y = 1;
      side_y = (map_y + 1.0 - cam.y) * delta_y;
    }

    bool ns_side = false;
    for (int hops = 0; hops < 64; ++hops) {
      if (side_x < side_y) {
        side_x += delta_x;
        map_x += step_x;
        ns_side = false;
      } else {
        side_y += delta_y;
        map_y += step_y;
        ns_side = true;
      }
      if (map.blocked(map_x, map_y)) {
        break;
      }
    }

    double dist = ns_side ? (side_y - delta_y) : (side_x - delta_x);
    if (dist < 0.05) {
      dist = 0.05;
    }
    const int line_h = static_cast<int>(rows / dist);
    const int draw_start = -line_h / 2 + rows / 2;
    const int draw_end = line_h / 2 + rows / 2;
    paint_column(fb, col, draw_start, draw_end, wall_color(dist, ns_side));
  }
}

const char* character_name(int selected) {
  switch (selected % 3) {
    case 0: return "Harry";
    case 1: return "Hermione";
    default: return "Ron";
  }
}

void overlay_third_person_wizard(std::vector<std::vector<Pix>>& fb,
                                 const Camera& cam, int selected) {
  const int rows = static_cast<int>(fb.size());
  const int cols = fb.empty() ? 0 : static_cast<int>(fb[0].size());
  if (rows < 22 || cols < 60) return;

  static constexpr const char* kHarryFront[] = {
      "....HHHHHH......", "...HHHHHHHH.....", "...HSSSSSSH.....", "..HSGGSSGGSH....",
      "..HSSSRSSSSH....", "...HSSSSSSH.....", "....SSSSSS......", "....BBBBBB......",
      "...BBYYYYBB.....", "..BBYYYYYYBB....", "..BYYYYYYYYB....", "...BYYYYYYB.....",
      "....BYYYYB......", "....K.BB.K......", "...KK....KK.....", "................"};
  static constexpr const char* kHarryRight[] = {
      "....HHHHHH......", "...HHHHHHHH.....", "...HSSSSSSH.....", "...HSGGSRSH.....",
      "....HSSSSS......", "....SSSSS.......", "....BBBBBB......", "...BBYYYYBB.....",
      "..BYYYYYYYBWW...", "..BYYYYYYYBWW...", "...BYYYYYB.WW...", "....BYYYB.......",
      "....K.BB........", "...KK..KK.......", "................", "................"};
  static constexpr const char* kHarryLeft[] = {
      "......HHHHHH....", ".....HHHHHHHH...", ".....HSSSSSSH...", "....HSRSGGSH....",
      "......SSSSSH....", ".......SSSSS....", "......BBBBBB....", ".....BBYYYYBB...",
      "...WWBYYYYYYYB..", "...WWBYYYYYYYB..", "...WW.BYYYYYB...", ".......BYYYB....",
      "........BB.K....", ".......KK..KK...", "................", "................"};
  static constexpr const char* kHermioneFront[] = {
      "...hhhhhhhh.....", "..hhhhhhhhhh....", ".hhSSSSSSSSh....", ".hSSGSSGSSSh....",
      ".hSSSSSSSSSh....", "..hhSSSSShh.....", "...hhSSShh......", "....PPPPPP......",
      "...PPYYYYPP.....", "..PPYYYYYYPP....", "..PYYYYYYYYP....", "...PYYYYYYP.....",
      "....PYYYYP......", "....K.PP.K......", "...KK....KK.....", "................"};
  static constexpr const char* kHermioneRight[] = {
      "...hhhhhhhh.....", "..hhhhhhhhhh....", "..hSSSSSSSh.....", "..hSSGSSSSh.....",
      "..hhSSSSShh.....", "...hhSSSh.......", "....PPPPPP......", "...PPYYYYPP.....",
      "..PYYYYYYYPWW...", "..PYYYYYYYPWW...", "...PYYYYYP.WW...", "....PYYYP.......",
      "....K.PP........", "...KK..KK.......", "................", "................"};
  static constexpr const char* kHermioneLeft[] = {
      ".....hhhhhhhh...", "....hhhhhhhhhh..", ".....hSSSSSSSh..", ".....hSSSSGSh...",
      ".....hhSSSSShh..", ".......hSSShh...", "......PPPPPP....", ".....PPYYYYPP...",
      "...WWPYYYYYYYP..", "...WWPYYYYYYYP..", "...WW.PYYYYYP...", ".......PYYYP....",
      "........PP.K....", ".......KK..KK...", "................", "................"};
  static constexpr const char* kRonFront[] = {
      "....rrrrrr......", "...rrrrrrrr.....", "...rSSSSSSr.....", "..rSGSSGSSr.....",
      "..rSSSSSSSr.....", "...rSSSSSr......", "....SSSSS.......", "....BBBBBB......",
      "...BBYYYYBB.....", "..BBYYYYYYBB....", "..BYYYYYYYYB....", "...BYYYYYYB.....",
      "....BYYYYB......", "....K.BB.K......", "...KK....KK.....", "................"};
  static constexpr const char* kRonRight[] = {
      "....rrrrrr......", "...rrrrrrrr.....", "...rSSSSSSr.....", "...rSGSSSSr.....",
      "....rSSSSS......", "....SSSSS.......", "....BBBBBB......", "...BBYYYYBB.....",
      "..BYYYYYYYBWW...", "..BYYYYYYYBWW...", "...BYYYYYB.WW...", "....BYYYB.......",
      "....K.BB........", "...KK..KK.......", "................", "................"};
  static constexpr const char* kRonLeft[] = {
      "......rrrrrr....", ".....rrrrrrrr...", ".....rSSSSSSr...", ".....rSSSSGSr...",
      "......SSSSSr....", ".......SSSSS....", "......BBBBBB....", ".....BBYYYYBB...",
      "...WWBYYYYYYYB..", "...WWBYYYYYYYB..", "...WW.BYYYYYB...", ".......BYYYB....",
      "........BB.K....", ".......KK..KK...", "................", "................"};

  const char* const* front[3] = {kHarryFront, kHermioneFront, kRonFront};
  const char* const* right[3] = {kHarryRight, kHermioneRight, kRonRight};
  const char* const* lefts[3] = {kHarryLeft, kHermioneLeft, kRonLeft};
  const int who = ((selected % 3) + 3) % 3;
  const char* const* sprite = front[who];
  if (std::abs(cam.dir_x) > std::abs(cam.dir_y)) sprite = (cam.dir_x > 0.0) ? right[who] : lefts[who];

  auto color_for = [](char px) -> std::uint8_t {
    switch (px) {
      case 'H': return 94; case 'h': return 130; case 'r': return 166; case 'S': return 223;
      case 'G': return 16; case 'R': return 196; case 'B': return 16; case 'P': return 57;
      case 'Y': return 220; case 'K': return 232; case 'W': return 136; default: return 16;
    }
  };

  auto draw_sprite = [&](const char* const* spr, int x0, int y0, int scale, bool border) {
    constexpr int kH = 16, kW = 16;
    if (border) {
      for (int y = -1; y <= kH * scale; ++y) for (int x = -1; x <= kW * scale; ++x) {
        if (y != -1 && y != kH * scale && x != -1 && x != kW * scale) continue;
        const int r = y0 + y, c = x0 + x;
        if (r < 0 || c < 0 || r >= rows || c >= cols) continue;
        Pix& p = fb[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)];
        p.bg = 220; p.fg = 220; p.ch = ' ';
      }
    }
    for (int y = 0; y < kH; ++y) {
      std::string_view line(spr[y]);
      for (int x = 0; x < kW; ++x) {
        const char px = line[static_cast<std::size_t>(x)];
        if (px == '.') continue;
        const std::uint8_t color = color_for(px);
        for (int sy = 0; sy < scale; ++sy) for (int sx = 0; sx < scale; ++sx) {
          const int r = y0 + y * scale + sy, c = x0 + x * scale + sx;
          if (r < 0 || c < 0 || r >= rows || c >= cols) continue;
          Pix& p = fb[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)];
          p.bg = color; p.fg = color; p.ch = ' ';
        }
      }
    }
  };

  const int scale = (cols >= 120 && rows >= 36) ? 2 : 1;
  const int sprite_w = 16 * scale, sprite_h = 16 * scale;
  const int y = std::max(0, rows - sprite_h - 3);
  const int x = std::max(0, cols / 2 - sprite_w / 2);
  draw_sprite(sprite, x, y, scale, false);

  if (cols >= 80) {
    const int gap = 2, total_w = 3 * 16 + 2 * gap;
    const int ix0 = std::max(0, cols - total_w - 2);
    const int iy = std::max(0, rows - 19);
    for (int i = 0; i < 3; ++i) draw_sprite(front[i], ix0 + i * (16 + gap), iy, 1, i == who);
  }
}

void overlay_minimap(std::vector<std::vector<Pix>>& fb, const Map& map,
                     const Camera& cam) {
  const int rows = static_cast<int>(fb.size());
  const int cols = fb.empty() ? 0 : static_cast<int>(fb[0].size());
  if (rows <= 0 || cols <= 0) {
    return;
  }

  // Local 1:1 minimap around the player. The old downsampled full map could
  // hide thin walls and made blocked movement look wrong.
  const int cell_w = 2;
  const int map_w = std::min(25, std::max(9, (cols / 4) / cell_w));
  const int map_h = std::min(17, std::max(7, rows / 3));
  const int origin_c = 1;
  const int origin_r = std::max(0, rows - map_h - 1);
  const int center_x = map_w / 2;
  const int center_y = map_h / 2;
  const int base_x = static_cast<int>(cam.x) - center_x;
  const int base_y = static_cast<int>(cam.y) - center_y;

  for (int y = 0; y < map_h; ++y) {
    for (int x = 0; x < map_w; ++x) {
      const int mx = base_x + x;
      const int my = base_y + y;
      const char tile = map.at(mx, my);

      std::uint8_t bg = 250;
      std::uint8_t fg = 16;
      char ch = ' ';
      if (map.blocked(mx, my)) {
        bg = 236;
        fg = 236;
        ch = ' ';
      } else if (tile == 'D') {
        bg = 24;
        fg = 24;
        ch = ' ';
      } else if (tile >= '1' && tile <= '9') {
        bg = 250;
        fg = 250;
        ch = ' ';
      }

      for (int sx = 0; sx < cell_w; ++sx) {
        const int r = origin_r + y;
        const int c = origin_c + x * cell_w + sx;
        if (r < 0 || c < 0 || r >= rows || c >= cols) {
          continue;
        }
        Pix& p = fb[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)];
        p.bg = bg;
        p.fg = fg;
        p.ch = (sx == 0) ? ch : ' ';
      }
    }
  }

  const int pc = origin_c + cell_w * center_x;
  const int pr = origin_r + center_y;
  if (pr >= 0 && pr < rows && pc >= 0 && pc < cols) {
    char facing = '^';
    if (std::abs(cam.dir_x) > std::abs(cam.dir_y)) {
      facing = (cam.dir_x > 0.0) ? '>' : '<';
    } else {
      facing = (cam.dir_y > 0.0) ? 'v' : '^';
    }
    for (int sx = 0; sx < cell_w && pc + sx < cols; ++sx) {
      Pix& player = fb[static_cast<std::size_t>(pr)]
                      [static_cast<std::size_t>(pc + sx)];
      player.bg = 226;
      player.fg = 16;
      player.ch = (sx == 0) ? facing : ' ';
    }
  }
}

void present(Terminal& term, const std::vector<std::vector<Pix>>& fb,
             std::string_view hud) {
  const int rows = static_cast<int>(fb.size());
  const int cols = fb.empty() ? 0 : static_cast<int>(fb[0].size());
  term.write("\033[H");
  for (int y = 0; y < rows; ++y) {
    int last_bg = -1;
    int last_fg = -1;
    for (int x = 0; x < cols; ++x) {
      const Pix& p = fb[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
      if (static_cast<int>(p.bg) != last_bg || static_cast<int>(p.fg) != last_fg) {
        term.write("\033[38;5;");
        term.write(std::to_string(p.fg));
        term.write(";48;5;");
        term.write(std::to_string(p.bg));
        term.write("m");
        last_bg = p.bg;
        last_fg = p.fg;
      }
      char ch = p.ch;
      if (ch == '\0') {
        ch = ' ';
      }
      term.write(std::string_view(&ch, 1));
    }
    term.write("\033[0m");
    if (y + 1 < rows) {
      term.write("\r\n");
    }
  }

  // Status line: reset colors, pad to width.
  std::string line(hud);
  if (static_cast<int>(line.size()) > cols) {
    line.resize(static_cast<std::size_t>(cols));
  } else {
    line.append(static_cast<std::size_t>(cols) - line.size(), ' ');
  }
  term.write("\033[0m\r\n");
  term.write(line);
  term.flush();
}

}  // namespace

void play_raycast(Terminal& term, const Map& map) {
  const auto spawn = map.spawn();
  std::array<Camera, 3> cams;
  for (int i = 0; i < 3; ++i) {
    cams[static_cast<std::size_t>(i)].x = spawn.x + 0.5;
    cams[static_cast<std::size_t>(i)].y = spawn.y + 0.5;
    face(cams[static_cast<std::size_t>(i)], 0.0, -1.0);
  }
  // Start the party near each other, but keep independent positions/directions.
  if (walkable(map, spawn.x + 1.5, spawn.y + 0.5)) cams[1].x += 1.0;
  if (walkable(map, spawn.x - 0.5, spawn.y + 0.5)) cams[2].x -= 1.0;

  constexpr double kMove = 0.10;
  constexpr double kTurn = 0.10;
  int selected_character = 0;

  for (;;) {
    Camera& cam = cams[static_cast<std::size_t>(selected_character)];
    const auto sz = term.size_cells();
    const int view_h = std::max(8, sz.y - 1);
    const int view_w = std::max(20, sz.x);
    std::vector<std::vector<Pix>> fb(
        static_cast<std::size_t>(view_h),
        std::vector<Pix>(static_cast<std::size_t>(view_w)));

    render_view(fb, map, cam);
    overlay_third_person_wizard(fb, cam, selected_character);
    overlay_minimap(fb, map, cam);
    present(term, fb,
            std::string("The Thirteenth Chamber   ") + character_name(selected_character) +
                " selected   C/Tab switch   W/S move   A/D strafe   Left/Right turn   q quit");

    const int key = term.read_key();
    if (key == 0 || key == 'q' || key == 'Q' || key == 3 || key == 27) {
      return;
    }

    if (key == 'c' || key == 'C' || key == '\t') {
      selected_character = (selected_character + 1) % 3;
    } else if (key == 'w' || key == 'W' || key == 'k' || key == Terminal::kArrowUp) {
      try_move(map, cam, cam.dir_x * kMove, cam.dir_y * kMove);
    } else if (key == 's' || key == 'S' || key == 'j' ||
               key == Terminal::kArrowDown) {
      try_move(map, cam, -cam.dir_x * kMove, -cam.dir_y * kMove);
    } else if (key == 'a' || key == 'A' || key == 'h') {
      // Strafe left.
      try_move(map, cam, cam.dir_y * kMove, -cam.dir_x * kMove);
    } else if (key == 'd' || key == 'D' || key == 'l') {
      // Strafe right.
      try_move(map, cam, -cam.dir_y * kMove, cam.dir_x * kMove);
    } else if (key == Terminal::kArrowLeft) {
      // Arrow keys rotate one cardinal direction per press.
      turn_cardinal(cam, -1);
    } else if (key == Terminal::kArrowRight) {
      turn_cardinal(cam, 1);
    } else if (key == 'z' || key == 'Z' || key == ',') {
      rotate(cam, -kTurn);
    } else if (key == 'x' || key == 'X' || key == '.') {
      rotate(cam, kTurn);
    }
  }
}

}  // namespace hogwarts::session
