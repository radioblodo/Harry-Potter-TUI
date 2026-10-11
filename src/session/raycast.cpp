#include "session/raycast.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
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
  std::uint8_t bg = 0;
};

bool walkable(const Map& map, double x, double y) {
  const double r = 0.18;
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

// Distance → 256-color index. NS walls are warmer (stone), EW darker.
std::uint8_t wall_color(double dist, bool ns_side) {
  static constexpr std::uint8_t kNs[] = {185, 178, 136, 94, 58};
  static constexpr std::uint8_t kEw[] = {138, 101, 95, 59, 52};
  int band = static_cast<int>(dist / 1.15);
  band = std::clamp(band, 0, 4);
  return ns_side ? kNs[band] : kEw[band];
}

std::uint8_t floor_color(int row, int rows) {
  const double t = static_cast<double>(row) / std::max(1, rows - 1);
  if (t > 0.88) return 94;
  if (t > 0.72) return 58;
  return 52;
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
    if (y < draw_start) {
      p.bg = 17;  // ceiling
    } else if (y > draw_end) {
      p.bg = floor_color(y, rows);
    } else {
      p.bg = wall;
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

void overlay_minimap(std::vector<std::vector<Pix>>& fb, const Map& map,
                     const Camera& cam) {
  const int rows = static_cast<int>(fb.size());
  const int cols = fb.empty() ? 0 : static_cast<int>(fb[0].size());
  constexpr int kRadX = 6;
  constexpr int kRadY = 4;
  const int origin_c = 1;
  const int origin_r = 1;
  const int px = static_cast<int>(cam.x);
  const int py = static_cast<int>(cam.y);

  char facing = '^';
  if (std::abs(cam.dir_x) > std::abs(cam.dir_y)) {
    facing = cam.dir_x > 0 ? '>' : '<';
  } else {
    facing = cam.dir_y > 0 ? 'v' : '^';
  }

  for (int dy = -kRadY; dy <= kRadY; ++dy) {
    for (int dx = -kRadX; dx <= kRadX; ++dx) {
      const int r = origin_r + dy + kRadY;
      const int c = origin_c + dx + kRadX;
      if (r < 0 || c < 0 || r >= rows || c >= cols) {
        continue;
      }
      Pix& p = fb[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)];
      const int mx = px + dx;
      const int my = py + dy;
      p.bg = 16;
      if (dx == 0 && dy == 0) {
        p.ch = facing;
      } else if (!map.in_bounds(mx, my)) {
        p.ch = ' ';
      } else if (map.blocked(mx, my)) {
        p.ch = '#';
      } else if (map.at(mx, my) == 'D') {
        p.ch = '+';
      } else {
        p.ch = '.';
      }
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
    for (int x = 0; x < cols; ++x) {
      const Pix& p = fb[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
      if (static_cast<int>(p.bg) != last_bg) {
        term.write("\033[48;5;");
        term.write(std::to_string(p.bg));
        term.write("m");
        last_bg = p.bg;
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
  Camera cam;
  cam.x = spawn.x + 0.5;
  cam.y = spawn.y + 0.5;
  cam.dir_x = 0.0;
  cam.dir_y = -1.0;
  cam.plane_x = 0.66;
  cam.plane_y = 0.0;

  constexpr double kMove = 0.22;
  constexpr double kTurn = 0.12;

  for (;;) {
    const auto sz = term.size_cells();
    const int view_h = std::max(8, sz.y - 1);
    const int view_w = std::max(20, sz.x);
    std::vector<std::vector<Pix>> fb(
        static_cast<std::size_t>(view_h),
        std::vector<Pix>(static_cast<std::size_t>(view_w)));

    render_view(fb, map, cam);
    overlay_minimap(fb, map, cam);
    present(term, fb,
            "Hogwarts cellar   wasd/arrows move   q quit");

    const int key = term.read_key();
    if (key == 0 || key == 'q' || key == 'Q' || key == 3 || key == 27) {
      return;
    }

    if (key == 'w' || key == 'W' || key == 'k' || key == Terminal::kArrowUp) {
      try_move(map, cam, cam.dir_x * kMove, cam.dir_y * kMove);
    } else if (key == 's' || key == 'S' || key == 'j' ||
               key == Terminal::kArrowDown) {
      try_move(map, cam, -cam.dir_x * kMove, -cam.dir_y * kMove);
    } else if (key == 'a' || key == 'A' || key == 'h' ||
               key == Terminal::kArrowLeft) {
      rotate(cam, kTurn);
    } else if (key == 'd' || key == 'D' || key == 'l' ||
               key == Terminal::kArrowRight) {
      rotate(cam, -kTurn);
    }
  }
}

}  // namespace hogwarts::session
