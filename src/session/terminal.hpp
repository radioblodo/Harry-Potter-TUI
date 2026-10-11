#pragma once

#include "common/types.hpp"

#include <string>
#include <string_view>
#include <termios.h>

namespace hogwarts::session {

// RAII: raw mode + alternate screen. Destructor always restores the TTY.
class Terminal {
 public:
  Terminal();
  ~Terminal();

  Terminal(const Terminal&) = delete;
  Terminal& operator=(const Terminal&) = delete;

  void restore();

  hogwarts::Vec2i size_cells() const;

  void write(std::string_view s);
  void flush();

  void clear();
  void hide_cursor();
  void show_cursor();
  void move_to(int row, int col);  // 1-based, like ANSI

  // Blocking. ASCII byte, 0 on EOF, or kArrow* for CSI arrows.
  int read_key();

  static constexpr int kArrowUp = 0x440;
  static constexpr int kArrowDown = 0x441;
  static constexpr int kArrowRight = 0x442;
  static constexpr int kArrowLeft = 0x443;

 private:
  termios original_{};
  bool active_ = false;
  std::string buf_;
};

}  // namespace hogwarts::session
