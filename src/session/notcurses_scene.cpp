#include "session/notcurses_scene.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>

#ifdef HOGWARTS_HAS_NOTCURSES
#include <notcurses/notcurses.h>
#endif

namespace hogwarts::session {

#ifdef HOGWARTS_HAS_NOTCURSES
namespace {

struct Visual {
  ncvisual* value = nullptr;

  Visual() = default;

  ~Visual() {
    if (value) {
      ncvisual_destroy(value);
    }
  }
  Visual(const Visual&) = delete;
  Visual& operator=(const Visual&) = delete;
};

bool load_visual(Visual& visual, const std::filesystem::path& path) {
  if (!std::filesystem::is_regular_file(path)) {
    return false;
  }
  visual.value = ncvisual_from_file(path.string().c_str());
  return visual.value != nullptr;
}

void put_hud(ncplane* plane, int rows, int cols, const char* text) {
  ncplane_options options{};
  options.y = rows - 2;
  options.x = 0;
  options.rows = 2;
  options.cols = cols;
  ncplane* hud = ncplane_create(plane, &options);
  if (!hud) {
    return;
  }
  ncplane_set_bg_rgb8(hud, 10, 10, 18);
  ncplane_set_fg_rgb8(hud, 245, 220, 120);
  ncplane_erase(hud);
  ncplane_putstr_yx(hud, 0, 2, text);
}

}  // namespace
#endif

int run_notcurses_scene(const std::filesystem::path& assets) {
#ifndef HOGWARTS_HAS_NOTCURSES
  (void)assets;
  return -1;
#else
  notcurses_options options{};
  options.flags = NCOPTION_SUPPRESS_BANNERS;
  notcurses* nc = notcurses_init(&options, nullptr);
  if (!nc) {
    return 1;
  }

  ncplane* standard = notcurses_stdplane(nc);
  const auto graphics = assets / "graphics";
  const auto title_path = graphics / "title.png";
  const auto fallback_path = graphics / "sky.png";
  Visual title;
  const bool loaded = load_visual(title, title_path) ||
                      load_visual(title, fallback_path);

  if (loaded) {
    ncvisual_options visual_options{};
    visual_options.n = standard;
    visual_options.scaling = NCSCALE_STRETCH;
    visual_options.blitter = ncvisual_media_defblitter(nc, NCSCALE_STRETCH);
    if (!ncvisual_blit(nc, title.value, &visual_options)) {
      ncplane_erase(standard);
      ncplane_set_bg_rgb8(standard, 12, 12, 25);
    }
  } else {
    ncplane_erase(standard);
    ncplane_set_bg_rgb8(standard, 12, 12, 25);
  }

  unsigned rows = 24;
  unsigned cols = 80;
  notcurses_term_dim_yx(nc, &rows, &cols);
  put_hud(standard, static_cast<int>(rows), static_cast<int>(cols),
          "HOGWARTS  |  real graphics mode  |  Enter to return  |  q to quit");
  notcurses_render(nc);

  ncinput input{};
  for (;;) {
    notcurses_get_blocking(nc, &input);
    if (input.id == 'q' || input.id == 'Q' || input.id == NCKEY_ESC ||
        input.id == NCKEY_ENTER || input.id == ' ') {
      break;
    }
  }
  notcurses_stop(nc);
  return 0;
#endif
}

}  // namespace hogwarts::session
