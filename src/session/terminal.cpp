#include "session/terminal.hpp"

#include <csignal>
#include <poll.h>
#include <stdexcept>
#include <unistd.h>
#include <sys/ioctl.h>

namespace hogwarts::session {
namespace {

Terminal* g_term = nullptr;

void handle_signal(int sig) {
  if (g_term) {
    g_term->restore();
  }
  _exit(128 + sig);
}

}  // namespace

Terminal::Terminal() {
  if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
    throw std::runtime_error("hp-session needs a real terminal (tty)");
  }
  if (tcgetattr(STDIN_FILENO, &original_) != 0) {
    throw std::runtime_error("tcgetattr failed");
  }

  termios raw = original_;
  cfmakeraw(&raw);
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;
  if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
    throw std::runtime_error("tcsetattr failed");
  }
  active_ = true;
  g_term = this;

  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);

  write("\033[?1049h");  // alternate screen
  hide_cursor();
  clear();
  flush();
}

Terminal::~Terminal() {
  restore();
}

void Terminal::restore() {
  if (!active_) {
    return;
  }
  show_cursor();
  write("\033[?1049l");  // leave alternate screen
  flush();
  tcsetattr(STDIN_FILENO, TCSANOW, &original_);
  active_ = false;
  if (g_term == this) {
    g_term = nullptr;
  }
}

hogwarts::Vec2i Terminal::size_cells() const {
  winsize ws{};
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != 0 || ws.ws_col == 0) {
    return {80, 24};
  }
  return {static_cast<int>(ws.ws_col), static_cast<int>(ws.ws_row)};
}

void Terminal::write(std::string_view s) {
  buf_.append(s);
}

void Terminal::flush() {
  std::size_t off = 0;
  while (off < buf_.size()) {
    const ssize_t n =
        ::write(STDOUT_FILENO, buf_.data() + off, buf_.size() - off);
    if (n < 0) {
      buf_.clear();
      return;
    }
    off += static_cast<std::size_t>(n);
  }
  buf_.clear();
}

void Terminal::clear() {
  write("\033[2J\033[H");
}

void Terminal::hide_cursor() {
  write("\033[?25l");
}

void Terminal::show_cursor() {
  write("\033[?25h");
}

void Terminal::move_to(int row, int col) {
  if (row < 1) row = 1;
  if (col < 1) col = 1;
  buf_ += "\033[";
  buf_ += std::to_string(row);
  buf_ += ';';
  buf_ += std::to_string(col);
  buf_ += 'H';
}

int Terminal::read_key() {
  unsigned char c = 0;
  const ssize_t n = ::read(STDIN_FILENO, &c, 1);
  if (n <= 0) {
    return 0;
  }
  if (c != 27) {
    return static_cast<int>(c);
  }

  pollfd pfd{STDIN_FILENO, POLLIN, 0};
  if (poll(&pfd, 1, 25) <= 0) {
    return 27;
  }
  unsigned char c2 = 0;
  if (::read(STDIN_FILENO, &c2, 1) <= 0) {
    return 27;
  }
  if (c2 != '[' && c2 != 'O') {
    return static_cast<int>(c2);
  }
  if (poll(&pfd, 1, 25) <= 0) {
    return 27;
  }
  unsigned char c3 = 0;
  if (::read(STDIN_FILENO, &c3, 1) <= 0) {
    return 27;
  }
  switch (c3) {
    case 'A':
      return kArrowUp;
    case 'B':
      return kArrowDown;
    case 'C':
      return kArrowRight;
    case 'D':
      return kArrowLeft;
    default:
      return 27;
  }
}

}  // namespace hogwarts::session
