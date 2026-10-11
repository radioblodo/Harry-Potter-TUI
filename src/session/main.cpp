#include "common/map.hpp"
#include "common/sprite.hpp"
#include "session/raycast.hpp"
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

    const auto cellar = hogwarts::load_map(assets / "maps" / "cellar.txt");
    hogwarts::session::play_raycast(term, cellar);
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "hp-session: " << e.what() << '\n';
    return 1;
  }
}
