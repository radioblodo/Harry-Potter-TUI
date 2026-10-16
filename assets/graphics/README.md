# Real graphics assets

The optional notcurses renderer reads bitmap images from this folder. Use PNG
files with transparency for characters and objects. JPEG also works when the
installed notcurses build has image decoding enabled.

Place these files here:

| File | Purpose |
|---|---|
| `title.png` | Full-screen title/background image |
| `sky.png` | Fallback background when `title.png` is absent |
| `floor.png` | Future raycast floor texture |
| `wall.png` | Future raycast wall texture |
| `wizard_harry.png` | Harry transparent character sprite |
| `wizard_hermione.png` | Hermione transparent character sprite |
| `wizard_ron.png` | Ron transparent character sprite |

The current notcurses scene displays `title.png` (or `sky.png`) as a true
terminal image using the best blitter supported by the terminal. It remains
optional: without notcurses, the existing ANSI renderer is used.

Recommended source art:

- `title.png`, `sky.png`: 16:9 or 16:10, at least 1280x720
- `floor.png`, `wall.png`: seamless square textures, 256x256 or 512x512
- character PNGs: transparent background, 256x256 or 512x512

Run with `HOGWARTS_NOTCURSES=1`. For a real bitmap protocol, use a terminal
with Kitty graphics, iTerm2, or Sixel support; otherwise notcurses degrades to
Unicode cell blitters.
