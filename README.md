# Hogwarts TUI

A terminal Harry Potter game in C++. You play locally; friends later join with
**only SSH** — they do not install this repo.

See [ARCHITECTURE.md](ARCHITECTURE.md) for the full plan.

## Do I need `CMakeLists.txt`?

Yes. C++ is compiled, not interpreted. `CMakeLists.txt` tells CMake how to
build two programs from `src/`:

| Binary | What it is |
|---|---|
| `hp-session` | The game TUI (what you run; what SSH will launch per player) |
| `hp-server` | Headless world + rooms (stub for now) |

You do not run CMake in front of friends. They will only type
`ssh play@your-host` once Phase 7 is done.

## Build

```bash
cmake -S . -B build
cmake --build build
./build/hp-session
./build/hp-server
```

Sanitizers (recommended while learning):

```bash
cmake -S . -B build -DHOGWARTS_SANITIZE=ON
cmake --build build
```

Needs a C++20 compiler (`g++` or `clang++`) and CMake ≥ 3.20.

On Omarchy/Arch, if `cmake` is missing:

```bash
sudo pacman -S cmake
```

## Layout

```
assets/maps/     floor plans (walls, doors) — not pictures
assets/tiles/    reusable stamps: cobblestone, wizard, your own
assets/graphics/ bitmap art for the optional notcurses renderer
assets/ui/       title screen art
assets/data/     spells and items (loaded later)
assets/music/    drop theme.mp3 here (gitignored)
src/common/      shared world/map/sprite code
src/session/     TUI process
src/server/      room authority
deploy/          example sshd + systemd (Phase 7)
third_party/     single-header libs later (miniaudio)
```

Copy `assets/tiles/cobblestone.txt` to make more floors, people, and buildings.
See `assets/tiles/README.md`.

## Now vs later

- **Now:** title screen, then a first-person cellar (`wasd` / arrows, `q` to quit).
- **Edit the scene:** `assets/maps/cellar.txt` (`#` wall, `.` floor, `D` door, `@` spawn).
- **Phase 7:** `ssh play@host` → same TUI, no client install.

## Optional real graphics

The game includes an optional notcurses image scene. Install notcurses and its
development headers before configuring CMake; CMake will detect it
automatically. Put bitmap art in `assets/graphics/` as described in
[`assets/graphics/README.md`](assets/graphics/README.md), then run:

```bash
HOGWARTS_NOTCURSES=1 ./build/hp-session
```

`title.png` is shown full-screen, with `sky.png` as a fallback. The renderer
uses the terminal's best available image protocol and degrades to Unicode
blitters when pixel graphics are unavailable. Without notcurses, the existing
ANSI renderer remains the default.
