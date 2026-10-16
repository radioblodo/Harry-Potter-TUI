#include "common/map.hpp"
#include "common/sprite.hpp"
#include "session/raycast.hpp"
#include "session/notcurses_scene.hpp"
#include "session/terminal.hpp"
#include "session/title_screen.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace {

std::filesystem::path assets_dir() {
  if (const char* env = std::getenv("HOGWARTS_ASSETS")) {
    return env;
  }
  return HOGWARTS_ASSETS;
}

}  // namespace

int main() {
  try {
    const auto assets = assets_dir();

    if (const char* graphics = std::getenv("HOGWARTS_NOTCURSES");
        graphics && std::string(graphics) == "1") {
      const int result = hogwarts::session::run_notcurses_scene(assets);
      if (result >= 0) {
        return result;
      }
    }

    const auto title =
        hogwarts::load_sprite(assets / "ui" / "title.txt");
    const auto cobble =
        hogwarts::load_sprite(assets / "tiles" / "cobblestone.txt");
    const auto wizard =
        hogwarts::load_sprite(assets / "tiles" / "wizard.txt");

    hogwarts::session::Terminal term;
    const auto result =
        hogwarts::session::show_title(term, title, cobble, wizard);

    if (result == hogwarts::session::TitleResult::Quit) {
      return 0;
    }

    const auto chamber =
        hogwarts::load_map(assets / "maps" / "thirteenth_chamber.txt");
    hogwarts::session::play_raycast(term, chamber);
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "hp-session: " << e.what() << '\n';
    return 1;
  }
}
