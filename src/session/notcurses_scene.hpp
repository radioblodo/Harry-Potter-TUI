#pragma once

#include <filesystem>

namespace hogwarts::session {

// Returns 0/1 when notcurses mode was requested and ran, or -1 when the
// optional notcurses backend is not compiled in.
int run_notcurses_scene(const std::filesystem::path& assets);

}  // namespace hogwarts::session
