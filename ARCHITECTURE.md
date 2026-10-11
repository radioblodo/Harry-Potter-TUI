# Harry Potter TUI — Architecture Plan

A low-resource, terminal-first multiplayer game. The goal of this document is
not only to describe *what* to build, but *in what order*, so you learn C++ and
TUI fundamentals instead of drowning in libraries.

---

## 1. Product constraints (these drive every technical choice)

| Constraint | Implication |
|---|---|
| Runs on *most* terminals | Never require Kitty/WezTerm. Fancy graphics are an upgrade path. |
| Friends play with **only a terminal** | No game install. They `ssh` in; the TUI runs on *your* server. |
| Low resource | No GPU, no huge framebuffers, no Electron, no GC-heavy scripting VM. |
| Background music (your MP3) | Plays **on the machine that runs the TUI**. Remote SSH players hear nothing unless they play locally. |
| Internet multiplayer via rooms | One public host. Sessions join named rooms. Server owns the world. |
| Learn C++ and TUI | Prefer small, readable modules over a kitchen-sink engine. |

**Genre recommendation (keep it shippable):** a top-down Hogwarts *exploration*
game with rooms, NPCs, items, and later **turn-based or slow real-time duels**.
This is far easier to network than Quidditch (physics + high tick rate).

Think: a shared castle, 8–16 players per *room instance*, chat, walking the
corridors, entering classrooms, picking up items, dueling.

---

## 2. The one rule that makes the rest work

**Simulation is a grid. Presentation is a skin.**

```
  ┌─────────────────────────────────────────┐
  │  World (integers, not pixels)           │
  │  tiles[width][height], entities, rooms  │
  └──────────────────┬──────────────────────┘
                     │ snapshot / delta
          ┌──────────▼──────────┐
          │  View / Camera      │
          └──────────┬──────────┘
                     │
     ┌───────────────┼────────────────┐
     ▼               ▼                ▼
  Glyph TUI     Braille bitmap    Kitty/Sixel pixels
  (any term)    (Unicode terms)   (Kitty, WezTerm, Ghostty…)
```

- Game logic, pathfinding, combat, and the network protocol speak **tiles and
  entity IDs**.
- The renderer decides whether a wall is `#`, `⣿`, a 2×4 Braille bitmap, or a
  PNG blit.
- Multiplayer stays cheap: you send “player 7 moved to (12, 34)”, never bitmaps.

This is also how real games work (logical world vs render backend).

---

## 3. High-level architecture

Friends never install the game. Their terminal is only a **screen + keyboard**.
The program that draws Braille/ASCII runs on the host, attached to the SSH PTY.

```
  Friend's WezTerm/Kitty/iTerm     Your laptop or a VPS
  ┌─────────────────────┐          ┌──────────────────────────────────┐
  │  ssh play@host      │  SSH+PTY │  sshd                            │
  │  (keys, UTF-8)      │─────────►│    ForceCommand: hp-session      │
  │  renders whatever   │  bytes   │         │ stdin/stdout = PTY     │
  │  ANSI/Braille you   │          │         ▼                        │
  │  send               │          │    hp-session  (TUI per player)  │
  └─────────────────────┘          │         │ Unix socket / localhost│
                                   │         ▼                        │
                                   │    hp-server   (rooms + world)   │
                                   └──────────────────────────────────┘
```

Three pieces, one shared library:

| Binary | Role |
|---|---|
| **hp-common** | World, protocol, `tick()`. No TTY, no SSH. |
| **hp-server** | Headless. Rooms, authority, 10 Hz sim. Listens on a **local** socket (not the public internet). |
| **hp-session** | One process per player. Raw TTY, renderer, HUD, music (if local). Talks to `hp-server`. |

`sshd` is the internet transport. You do **not** invent a custom public protocol
so that friends can join — every OS already has SSH, and it already does UTF-8,
window-size, and encryption.

Local play (you, with music):

```
hp-session --offline
  → InProcessServer (same tick code)
  → your real terminal, speakers work
```

Or the same path as friends: `ssh play@localhost`, then join a room.

That way “a second player” is just **another PTY**, not another codebase.

---

## 4. Rendering strategy (Braille vs pixels)

### 4.1 Capability ladder (detect at startup, never crash)

| Level | What | Where it works | Cost |
|---|---|---|---|
| **L0 ASCII** | `# . @` | Literally every terminal | Tiny |
| **L1 Unicode tiles** | Box drawing, blocks, Braille *glyphs as tiles* | Most modern terminals | Tiny |
| **L2 Braille framebuffer** | Each cell = 2×4 dots; plot a low-res bitmap | UTF-8 terminals with Braille font | Small CPU |
| **L3 Pixel protocol** | Kitty graphics / Sixel / iTerm2 | Kitty, WezTerm, Ghostty, foot (sixel), iTerm2 | More RAM, still no GPU |

Default: **L1 or L2**. L3 is optional portraits / cutscenes / minimap, gated
behind `TERM` + query (`CSI > 0 q` / Kitty `G` query). If detection fails,
fall back silently.

You asked for “run on most terminals” — so **L3 must never be required**.

### 4.2 Recommended look for Hogwarts

Do **not** try to draw the whole castle as one giant bitmap. Do this:

1. **Logical tile** = 1 world cell (floor, wall, door, stair, water…).
2. **On screen**, each tile occupies a small Braille patch, e.g. 2×4 or 4×8
   dots, or simply 1 character cell with a glyph.
3. **Actors** (you, other players, ghosts, books) are a second layer drawn on
   top.

Example L1 tiles (easy to start):

```
  █ wall     ░ floor     ┃ corridor     ▬ door
  @ you      A Alice     † ghost        ▪ item
```

Example L2: walls become filled Braille (`⣿`), floors sparse dots, player a
tiny 2×4 silhouette.

### 4.3 Renderer interface (you write this — it is the TUI lesson)

```cpp
class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual void begin_frame() = 0;
    virtual void draw_tile(int sx, int sy, TileVisual v) = 0;
    virtual void draw_actor(int sx, int sy, ActorVisual v) = 0;
    virtual void draw_text(int sx, int sy, std::string_view, Style) = 0;
    virtual void end_frame() = 0;          // one write() / buffered flush
    virtual Vec2 size_cells() const = 0;
};
```

Implementations:

- `AnsiRenderer` — raw termios + ANSI (you will *learn* TUI here).
- `BrailleRenderer` — packs a 2-bit or 1-bit bitmap into U+2800..U+28FF.
- `KittyRenderer` — optional, later. Same interface, different `end_frame`.

**Do not start with ncurses.** For learning, a 200-line ANSI layer teaches you
more (alternate screen, raw mode, UTF-8, SIGWINCH, mouse SGR). You can still
use **notcurses** later as a drop-in `IRenderer` if you want images/video; keep
it behind the interface so the game does not care.

### 4.4 Terminal I/O details that matter

- Enter **alternate screen** + **raw mode** (no echo, no canonical line read).
- Enable **UTF-8**, optional **SGR mouse**, **focus events**.
- Handle **SIGWINCH** (resize) by rebuilding the camera, not crashing.
- **Buffer the whole frame** into one `std::string` / `write()` — many small
  writes kill SSH and slow terminals.
- Never assume 80×24. Query `TIOCGWINSZ`.
- Color: 16-color always, 256 if `TERM` allows, truecolor only if
  `COLORTERM=truecolor`. Hogwarts gold/crimson works in 16 colors.

---

## 5. Client internals

```
                    ┌──────────── input thread / poll ────────────┐
                    │  keyboard, resize, optional mouse           │
                    └──────────────────┬──────────────────────────┘
                                       ▼
┌──────────┐   events    ┌─────────────────────┐  commands   ┌──────────┐
│ Terminal │────────────►│  ClientApp          │────────────►│ NetClient│
│          │◄────────────│  - menu / HUD       │◄────────────│          │
│ Renderer │   frames    │  - camera           │  snapshots  │          │
└──────────┘             │  - interpolation    │             └────▲─────┘
                         │  - audio triggers   │                  │
                         └──────────┬──────────┘                  │
                                    ▼                             │
                         ┌─────────────────────┐                  │
                         │  AudioEngine        │             network
                         │  (miniaudio thread) │                  │
                         └─────────────────────┘                  │
                                                                  ▼
                                                            hp-server
```

### Game loop (client)

Fixed sim if offline; otherwise **render as fast as the terminal allows**,
**network at 10–20 Hz**.

```
while (running) {
    dt = clock.tick();          // steady_clock
    input.poll();
    net.receive();              // apply snapshots to WorldView
    camera.update(local_player);
    renderer.draw(world_view, hud);
    audio.tick(dt);             // fade, location stingers
    sleep_until(next_frame);    // cap ~30 FPS; terminals don't need 144
}
```

30 FPS is plenty for Braille/ASCII and keeps CPU near idle.

### Screens (a tiny state machine — good C++ practice)

```
Boot → MainMenu → HostOrJoin → Connecting → InGame
                      ↓                        ↓
                   Settings                  Pause / Chat
```

Each screen is a class with `on_enter / handle_input / update / draw / on_exit`.
No giant `if (state == …)` switch in `main`.

---

## 6. World & game data

Keep data **plain structs**. This is the C++ you want to practice: ownership,
`std::vector`, `std::unordered_map`, no inheritance forest.

```
World
  Map
    width, height
    tiles[]          // TileId
    rooms[]          // named chambers: Great Hall, Dormitory, …
    portals[]        // door at (x,y) → other map + spawn
  Entities
    id, kind, pos, hp, house, name, facing
  Inventory (per player)
  Spells known
```

**Maps as text files** (you can edit them in vim):

```
################
#..............#
#..G........D..#    G = great hall marker, D = door
#..............#
################
```

Load into tiles at runtime. Later you can add a TUI map editor.

**Content vs code:** spells, items, houses live in `assets/*.toml` or JSON.
Code should not hardcode “Expelliarmus deals 12 damage” forever.

### Hogwarts as instances, not one giant map

One huge castle is painful to stream. Prefer:

- **Hub maps** (Great Hall, courtyard) — many players, social.
- **Instance maps** (classroom, dungeon, duel arena) — few players.

A “room” in multiplayer = a **server lobby instance**, which loads one hub +
optional instances. This matches “join a room” in the UI.

---

## 7. Multiplayer — “just a terminal”

Prior art: `ssh nethack@nethack.alt.org`, `ssh sshtron.zachlatta.com`,
`telnet towel.blinkenlights.nl`. The friend installs **nothing**; their
terminal already speaks the only protocol that matters (bytes + a PTY).

### 7.1 What you send friends

```text
ssh play@hogwarts.example.com
```

Optional room in the command (parsed by `hp-session` from SSH `original command`
or a wrapper):

```text
ssh play@hogwarts.example.com gryffindor-night
```

They type a password you set (or you put their SSH public key in
`~play/.ssh/authorized_keys`). The moment the session opens they are *in the
TUI* — name/house prompt, then the castle. No download, no CMake, no client
binary.

Windows / macOS / Linux all ship an SSH client. “Just a terminal” in practice
means: open Terminal / Windows Terminal / Kitty and paste that line.

**Do not use raw `telnet`/`nc` as the default.** UTF-8 Braille, window resize,
and special keys are painful. Telnet can be a later extra for 80×24 ASCII.

### 7.2 What actually runs

1. **You** start once: `hp-server` (systemd / tmux) on a VPS or port-forwarded PC.
2. **sshd** accepts `play@host`. `Match User play` → `ForceCommand /usr/local/bin/hp-session` — **no shell**, no `scp`, no port forward.
3. Each SSH login = one `hp-session` process. It puts the PTY in raw mode and draws the game.
4. That process connects to `hp-server` over a **Unix domain socket** (`/run/hogwarts/game.sock`).
5. In the TUI they pick or type a **room code**. Same code → same castle instance.

```
sshd ─┬─ hp-session (Harry)  ──┐
      ├─ hp-session (Hermione)─┼── Unix socket ── hp-server
      └─ hp-session (Ron)    ──┘                    ├── room gryffindor-night
                                                    └── room duel-club
```

Public internet port: **22** (SSH) only. Game port stays localhost.

### 7.3 Session ↔ server protocol (still exists, but private)

`hp-session` and `hp-server` still speak a small length-prefixed protocol.
Friends never see it; it does not leave the machine.

```
[u16 length][u8 type][payload…]
```

| Type | Direction | Payload |
|---|---|---|
| `Hello` | session→server | name, house |
| `Welcome` | server→session | player id, room id |
| `JoinRoom` | session→server | room code, password |
| `Move` | session→server | dir or target cell |
| `Snapshot` | server→session | tick, entities in AOI |
| `Cast` | session→server | spell id, target |
| `Chat` | both | utf8 text |

Start with POD + explicit little-endian writes. This is localhost IPC, so you
can even use a simple text line protocol at first (`MOVE N\n`) and tighten later.

### 7.4 Tick

10 Hz on `hp-server`: read inputs → validate → step world → snapshot each
session’s view. Sessions render at ~30 FPS to their own PTY. A slow SSH link
only affects that player’s screen.

### 7.5 Hosting options

| Setup | Friends type | When to use |
|---|---|---|
| **VPS** (recommended) | `ssh play@your.vps` | Real internet play, stable IP |
| Your PC + port 22 forwarded | `ssh play@your-public-ip` | LAN party over the internet; router pain |
| Same Wi‑Fi | `ssh play@192.168.1.42` | Living-room test |
| You only | `./hp-session --offline` | Music + no sshd |

### 7.6 Security (this account is on the internet)

- User `play` has **no shell** (`ForceCommand`, `nologin` + wrapper).
- Home dir owned root, not writable by `play` except a tiny `~/.ssh`.
- `AllowTcpForwarding no`, `X11Forwarding no`, `PermitTunnel no`.
- `ulimit` / systemd `MemoryMax` so one player cannot fork-bomb.
- Room passwords are game-level, not SSH-level (SSH gets them on the box; rooms keep randoms out of your private instance).
- Do not run `hp-server` as root.

Because the TUI binary runs on **your** host, friends cannot ship a hacked
client. Still validate moves on the server (a compromised session process is
still just one player).

### 7.7 Resize, Braille, pixels over SSH

The friend’s **local** emulator renders. You only send bytes.

- Braille works if *their* font has U+2800.
- `SIGWINCH` fires on the PTY when they resize — handle it.
- Kitty graphics *can* pass through SSH if *their* emulator supports them; still optional.
- `TERM` is theirs (`xterm-256color`, `kitty`, …). Detect capabilities per session.

### 7.8 Library choice (IPC, not public net)

| Option | Use when |
|---|---|
| **Unix socket + your framing** | Default. Simple, local, fast. |
| **Asio** | If many sessions and the accept loop gets messy |
| **sshd** | Public access. Do not reimplement SSH. |

---

## 8. Audio

Completely decoupled from TUI. SSH cannot carry your MP3 (it is a text
session). That is a hard limit of “friends only need a terminal.”

| Who | Music |
|---|---|
| You running `hp-session` on a machine with speakers | Yes — miniaudio plays `assets/music/theme.mp3` |
| Friend over SSH | No (unless they play their own soundtrack beside the terminal) |
| Headless VPS | No device; skip audio |

- Library: **miniaudio** (single header) + MP3 decoder.
- One mixer thread. Game thread posts `Play` / `SetVolume` / `Crossfade`.
- If open-device fails: game still runs.
- Do not decode MP3 on the UI thread.
- Do not try PulseAudio-over-SSH or streaming audio in v1.

Later (optional, not “just a terminal”): a tiny local helper or a web radio
page. Out of scope until the SSH game is fun.

---

## 9. Suggested directory layout

```
harry_potter_game/
  CMakeLists.txt
  ARCHITECTURE.md          ← this file
  README.md
  assets/
    maps/great_hall.txt
    maps/dungeon.txt
    data/spells.toml
    data/items.toml
    music/theme.mp3        ← you provide (gitignored)
  third_party/             ← vendored single headers only
    miniaudio.h
  src/
    common/
      types.hpp            // EntityId, Vec2i, House
      tile.hpp
      world.hpp / world.cpp
      protocol.hpp / protocol.cpp
      sim.hpp / sim.cpp    // one function: tick(World&, Inputs)
    session/                 // hp-session (TUI; name was “client”)
      main.cpp
      app.hpp
      terminal/            // raw mode, keys, resize
        tty.hpp
        ansi_renderer.cpp
        braille_renderer.cpp
      ui/                  // screens, hud, chat
      net/client.cpp          // Unix socket to hp-server
      audio/engine.cpp
    server/
      main.cpp
      room.hpp
      net/listener.cpp        // Unix socket accept
    deploy/
      sshd_play.conf          // Match User play / ForceCommand
      hogwarts.service        // systemd for hp-server
  tests/
    test_protocol.cpp
    test_sim.cpp
```

**CMake:** C++20, `-Wall -Wextra -Werror` in debug, ASan/UBSan option.
No Boost required.

---

## 10. Build it in layers (this *is* the C++ curriculum)

Each layer is a playable program. Do not skip ahead to multiplayer.

### Phase 0 — Tooling (1 evening)
CMake, `main` that prints `Hello Hogwarts`, sanitizers, `.gitignore`.

### Phase 1 — Terminal kindergarten
- Raw mode, alternate screen, restore on exit (`atexit` + signals).
- Read keys (`hjkl` / arrows).
- Draw a moving `@` on a blank field.
- Handle resize.

**You learn:** RAII (a `Terminal` class whose destructor restores the TTY),
file descriptors, UTF-8, the game loop.

### Phase 2 — Map & camera
- Load a `#`/`.` map.
- Collision.
- Camera follows player; HUD with name / house.
- Screen: title menu.

**You learn:** `std::vector`, file I/O, separating model from view.

### Phase 3 — Braille (or Unicode tiles)
- `BrailleRenderer`: plot dots, convert 2×4 to a codepoint.
- Walls/floors as patterns; player as a small sprite.
- Runtime flag `--render=ascii|unicode|braille`.

**You learn:** bit packing, encoding, interfaces (`IRenderer`).

### Phase 4 — Entities & content
- NPCs, items, doors between maps.
- Simple inventory and a spell that affects a tile (e.g. `Lumos` lights fog).
- Data files for spells.

**You learn:** IDs vs pointers, ownership (`std::unique_ptr` where needed),
serialization.

### Phase 5 — Audio
- Loop `theme.mp3`.
- Crossfade on map change.

**You learn:** threads, atomics / lock-free command queue (keep it simple:
`std::mutex` + queue is fine).

### Phase 6 — Embedded server
- Extract `tick(World, Inputs)`.
- Client talks to an in-process server via the *same* protocol structs.

**You learn:** the most important architecture habit in games.

### Phase 7 — Many PTYs, one dungeon
- `hp-server` on a Unix socket.
- Two `hp-session` processes (two terminals on your machine) join the same room.
- Then: `sshd` + user `play` + `ForceCommand`.
- Friends: `ssh play@host`.

**You learn:** PTYs, `poll()` on stdin + socket, IPC, process isolation.
sshd does the internet; you do not write a public game protocol.

### Phase 8 — Optional pixels
- Detect Kitty protocol.
- Draw a crest or character portrait in the HUD.
- Must degrade to Braille/text.

---

## 11. Tech choices (and what to avoid)

| Use | Avoid (for this project) |
|---|---|
| C++20, CMake, STL | Unreal, Godot, Unity (defeats TUI + learning C++) |
| Your own ANSI layer, then maybe notcurses | ncurses as the *first* thing you learn |
| miniaudio | Linking FFmpeg |
| SSH + Unix-socket rooms | Shipping a client, P2P, WebRTC, raw telnet as v1 |
| Text maps + TOML | A binary world editor in month one |
| 10–20 Hz net, 30 FPS draw | 60+ FPS terminal spam |
| Linux first (you are on Linux) | Immediate Windows port |

**notcurses** is the right *library* if you later want one backend that already
does Braille + Kitty + Sixel. Use it as an `IRenderer`, not as your game
engine.

**FTXUI** is excellent for forms/menus, weaker as a game framebuffer. Fine for
the main menu only.

---

## 12. Resource budget (keep yourself honest)

| Piece | Target |
|---|---|
| Session RSS | < 50 MB typical per SSH player |
| CPU | < 5% of one core per session at 30 FPS idle |
| Server | Few MB per room; 16 players trivial |
| Network | SSH carrying one screen of UTF-8 (~few KB/frame), not bitmaps |
| Frame draw | One buffered write per PTY per frame |

If you allocate a full-color framebuffer the size of the terminal in pixels,
you have already left “low resource”. Braille is ~1 bit per subpixel; ASCII is
2 bytes per cell. Stay there.

---

## 13. Risks (so you are not surprised)

1. **SSH / tmux** — Braille works. Kitty pixels only if the *friend’s* emulator
   supports the protocol through SSH. Fall back.
2. **Font** — some terminals lack Braille glyphs (boxes appear). L0 ASCII saves
   you. Document “use a Nerd Font / Noto” but do not require it.
3. **No remote music** — SSH is text. Friends will not hear your MP3.
4. **MP3 / HP IP** — do not redistribute copyrighted soundtrack; fan project only.
5. **User `play` on the internet** — `ForceCommand`, no shell, resource limits.
   This is the main ops risk, not the game protocol.
6. **SIGWINCH + blocking `read`** — `poll()` on the PTY and the Unix socket.

---

## 14. First concrete milestone (definition of done)

A single binary, offline:

- Full-screen TUI restores the terminal on quit.
- Walk around a Great Hall map with collision.
- `--render=ascii` and `--render=braille` both work.
- A status line: house, position, “press q to quit”.
- Optional: your MP3 loops if the file exists.

When that feels solid: two local terminals into one `hp-server`, then
`ssh play@host` from a friend’s machine with **no game install**.

---

## 15. Decision summary

| Decision | Choice |
|---|---|
| Language | C++20 |
| Sim vs view | Tile world, pluggable renderer |
| Default graphics | Unicode / Braille cells |
| Pixels | Optional L3, capability-detected |
| UI library | Homegrown ANSI first |
| Audio | miniaudio, **local session only** |
| How friends join | `ssh play@your-host` — TUI on the server |
| Rooms | Named instances inside `hp-server` (Unix socket) |
| Single-player | `hp-session --offline` |
| Tick | Server 10 Hz, each session draws ~30 FPS |
