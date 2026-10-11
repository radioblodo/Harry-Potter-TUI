#pragma once

#include "common/sprite.hpp"
#include "session/terminal.hpp"

namespace hogwarts::session {

enum class TitleResult { Play, Quit };

TitleResult show_title(Terminal& term, const Sprite& title, const Sprite& cobble,
                       const Sprite& wizard);

}  // namespace hogwarts::session
