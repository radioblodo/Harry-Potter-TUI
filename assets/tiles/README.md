# Tiles — copy this pattern

Each file is one reusable stamp: **floor, character, or building**.

The opening screen loads `cobblestone.txt` and repeats it. Make `wizard.txt`,
`tower.txt`, `door.txt` the same way.

## File format

```
name cobblestone
kind floor
dots 8 8
.###.##.
#...#..#
...
```

| Header | Meaning |
|---|---|
| `name` | Optional. Defaults to the filename. |
| `kind` | `floor`, `actor`, `building`, or `ui` |
| `dots W H` | Bitmap. Next **H** lines, **W** characters each. |
| `cells W H` | Raw terminal art (the title screen uses this). |

In `dots` mode:

- `#` `X` `1` → pixel **on**
- `.` space `0` → pixel **off**

The loader packs every **2×4 dots** into one Braille character (`⣿` etc.).
So pick sizes that are multiples of **2 wide** and **4 tall**:

| Dots | On screen | Good for |
|---|---|---|
| 8×8 | 4×2 cells | floor stamps (cobblestone) |
| 8×16 | 4×4 cells | a person |
| 16×16 | 8×4 cells | a small building |

`assets/maps/` is still the **floor plan** (where walls are). Tiles are only
**how a cell looks**.
