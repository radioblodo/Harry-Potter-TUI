#pragma once

#include "common/map.hpp"
#include "session/terminal.hpp"

namespace hogwarts::session {

// First-person walk around a tile map. q / ESC to leave.
void play_raycast(Terminal& term, const Map& map);

}  // namespace hogwarts::session
