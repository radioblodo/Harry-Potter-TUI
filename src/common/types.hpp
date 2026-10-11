#pragma once

#include <cstdint>
#include <string_view>

namespace hogwarts {

enum class House : std::uint8_t {
  Unsorted = 0,
  Gryffindor,
  Hufflepuff,
  Ravenclaw,
  Slytherin,
};

struct Vec2i {
  int x = 0;
  int y = 0;

  friend constexpr bool operator==(Vec2i a, Vec2i b) {
    return a.x == b.x && a.y == b.y;
  }
};

using EntityId = std::uint32_t;

inline constexpr std::string_view kProtocolName = "hogwarts";
inline constexpr std::uint16_t kProtocolVersion = 1;

}  // namespace hogwarts
