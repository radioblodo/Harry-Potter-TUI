cmake_minimum_required(VERSION 3.20)
project(hogwarts
  VERSION 0.1.0
  DESCRIPTION "Harry Potter TUI — terminal Hogwarts"
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

option(HOGWARTS_SANITIZE "Enable ASan + UBSan (Debug-friendly)" OFF)
if(HOGWARTS_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

set(HOGWARTS_ASSETS "${CMAKE_SOURCE_DIR}/assets")

add_library(hp-common
  src/common/map.cpp
  src/common/sprite.cpp
)
target_include_directories(hp-common PUBLIC "${CMAKE_SOURCE_DIR}/src")

add_executable(hp-session
  src/session/main.cpp
  src/session/terminal.cpp
  src/session/title_screen.cpp
  src/session/raycast.cpp
)
target_link_libraries(hp-session PRIVATE hp-common)
target_compile_definitions(hp-session PRIVATE
  HOGWARTS_ASSETS="${HOGWARTS_ASSETS}"
)

add_executable(hp-server src/server/main.cpp)
target_link_libraries(hp-server PRIVATE hp-common)
target_compile_definitions(hp-server PRIVATE
  HOGWARTS_VERSION="${PROJECT_VERSION}"
)
