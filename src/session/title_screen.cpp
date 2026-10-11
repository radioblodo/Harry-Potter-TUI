#include "session/title_screen.hpp"

#include "common/sprite.hpp"

#include <algorithm>
#include <string>

namespace hogwarts::session {
namespace {

void draw_sprite(Terminal& term, int row, int col, const Sprite& spr,
                 std::string_view color) {
  if (row < 1 || col < 1) {
    return;
  }
  for (int y = 0; y < spr.cell_h; ++y) {
    term.move_to(row + y, col);
    term.write(color);
    term.write(spr.rows[static_cast<std::size_t>(y)]);
    term.write("\033[0m");
  }
}

void fill_cobble(Terminal& term, const Sprite& cobble, int row, int col,
                 int rows, int cols, std::string_view color) {
  if (cobble.cell_w <= 0 || cobble.cell_h <= 0) {
    return;
  }
  const int tiles_y = (rows + cobble.cell_h - 1) / cobble.cell_h;
  const int tiles_x = (cols + cobble.cell_w - 1) / cobble.cell_w;
  for (int ty = 0; ty < tiles_y; ++ty) {
    for (int tx = 0; tx < tiles_x; ++tx) {
      draw_sprite(term, row + ty * cobble.cell_h, col + tx * cobble.cell_w,
                  cobble, color);
    }
  }
}

int center_col(int screen_w, int piece_w) {
  if (piece_w >= screen_w) {
    return 1;
  }
  return 1 + (screen_w - piece_w) / 2;
}

}  // namespace

TitleResult show_title(Terminal& term, const Sprite& title, const Sprite& cobble,
                       const Sprite& wizard) {
  const auto sz = term.size_cells();
  term.clear();

  // Courtyard: cobblestone stamped across the lower half.
  const int yard_row = std::max(1, sz.y / 2);
  const int yard_rows = sz.y - yard_row + 1;
  fill_cobble(term, cobble, yard_row, 1, yard_rows, sz.x, "\033[38;5;242m");

  // Wordmark above the yard.
  const int title_col = center_col(sz.x, title.cell_w);
  const int title_row = std::max(2, (yard_row - title.cell_h) / 2);
  draw_sprite(term, title_row, title_col, title, "\033[1;33m");

  // Wizard standing on the cobbles.
  const int wiz_col = center_col(sz.x, wizard.cell_w);
  const int wiz_row = std::min(sz.y - wizard.cell_h, yard_row + 1);
  draw_sprite(term, std::max(1, wiz_row), wiz_col, wizard, "\033[1;37m");

  const std::string hint = "Enter to begin    q to quit";
  const int hint_col = center_col(sz.x, display_cols(hint));
  term.move_to(sz.y, hint_col);
  term.write("\033[33m");
  term.write(hint);
  term.write("\033[0m");
  term.flush();

  for (;;) {
    const int k = term.read_key();
    if (k == 'q' || k == 'Q' || k == 3 /* Ctrl-C */) {
      return TitleResult::Quit;
    }
    if (k == '\r' || k == '\n' || k == ' ') {
      return TitleResult::Play;
    }
  }
}

}  // namespace hogwarts::session
