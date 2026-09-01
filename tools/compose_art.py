#!/usr/bin/env python3
"""Composes game sprites from Kenney's CC0 "Tiny" family.

Everything here is built from tiles in Tiny Town, Tiny Dungeon and Tiny
Creatures -- all CC0, all 16x16, all the same artist and style, so the pieces
sit together. Towers are ASSEMBLED from real masonry tiles rather than drawn,
which is why they beat anything generated from primitives.

    python3 tools/compose_art.py /path/to/tiny-packs

expects that directory to contain town/ dungeon/ ref/ subfolders with Tiles/.
Writes PNG overrides into assets/sprites/, which the game prefers over its
generated art.
"""
import os, sys
from PIL import Image

T = 16  # source tile size


def load(pack, tid):
    p = os.path.join(SRC, pack, "Tiles", f"tile_{tid}.png")
    return Image.open(p).convert("RGBA")


def blank(w, h):
    return Image.new("RGBA", (w, h), (0, 0, 0, 0))


def put(dst, src, x, y):
    dst.alpha_composite(src, (x, y))


def strip_backing(im):
    """Tiny Creatures tiles sit on a flat dark plum square; drop it."""
    px = im.load()
    bg = px[0, 0]
    if bg[3] == 0:
        return im
    out = blank(*im.size)
    op = out.load()
    for y in range(im.height):
        for x in range(im.width):
            c = px[x, y]
            if c[3] > 0 and not (abs(c[0]-bg[0]) < 14 and abs(c[1]-bg[1]) < 14
                                 and abs(c[2]-bg[2]) < 14):
                op[x, y] = c
    return out


def crop_nonempty(im):
    bbox = im.getbbox()
    return im.crop(bbox) if bbox else im


# ---------------------------------------------------------------------------
# TOWER: three courses of real masonry, 32 wide x 48 tall.
#   crenellated cap  ->  wall with an arrow slit  ->  wall with a gate
# ---------------------------------------------------------------------------
def tower_base():
    t = blank(32, 48)
    put(t, load("town", "0096"), 0, 0)     # crenellated top-left
    put(t, load("town", "0098"), 16, 0)    # crenellated top-right
    put(t, load("town", "0126"), 0, 16)    # brick course
    put(t, load("town", "0126"), 16, 16)
    put(t, load("town", "0126"), 0, 32)
    put(t, load("town", "0126"), 16, 32)

    # An arrow slit, taken from the window tile and centred on the shaft.
    slit = crop_nonempty(load("town", "0125"))
    sx = (32 - slit.width) // 2
    put(t, slit, sx, 17)
    # A gate at the foot, so the tower reads as a building people are inside.
    gate = crop_nonempty(load("town", "0103"))
    put(t, gate, (32 - gate.width) // 2, 48 - gate.height - 1)
    return t


def figure(tid, flip=False):
    f = crop_nonempty(load("dungeon", tid))
    return f.transpose(Image.FLIP_LEFT_RIGHT) if flip else f


# ---------------------------------------------------------------------------
# CROWNS: the garrison. Each specialisation is a different silhouette on the
# battlement, which is how the board stays readable without colour.
# ---------------------------------------------------------------------------
def crown_plain():
    c = blank(32, 24)
    a, b = figure("0097"), figure("0097", flip=True)
    put(c, a, 3, 24 - a.height)
    put(c, b, 32 - b.width - 3, 24 - b.height)
    return c


def crown_sniper():
    c = blank(32, 26)
    bow = crop_nonempty(load("town", "0118"))
    bow = bow.resize((bow.width * 2, bow.height * 2), Image.NEAREST)  # a big longbow
    put(c, bow, 32 - bow.width - 1, 26 - bow.height - 4)
    f = figure("0100")                      # the grey-haired veteran marksman
    put(c, f, 4, 26 - f.height)
    return c


def crown_elf():
    c = blank(32, 26)
    for tid, x in (("0005", -2), ("0006", 9), ("0005", 18)):   # leafy canopy
        leaf = crop_nonempty(load("town", tid))
        put(c, leaf, max(0, x), 0)
    a, b = figure("0112"), figure("0112", flip=True)           # green rangers
    put(c, a, 2, 26 - a.height)
    put(c, b, 32 - b.width - 2, 26 - b.height)
    return c


def crown_hunter():
    c = blank(32, 24)
    for i, (tid, fl) in enumerate((("0098", False), ("0097", False), ("0098", True))):
        f = figure(tid, flip=fl)
        put(c, f, 1 + i * 11, 24 - f.height)
    return c


# ---------------------------------------------------------------------------
# CREATURES: from Tiny Creatures, with a cycle synthesised per frame.
# ---------------------------------------------------------------------------
PICKS = {"slime": ("ref", "0099"), "wolf": ("ref", "0025"),
         "goblin": ("ref", "0026"), "wraith": ("ref", "0088")}
PHASES = {
    "slime":  [(0, 0, 0.0), (0, -1, -0.10), (0, -3, -0.14), (0, -1, 0.06), (0, 1, 0.18), (0, 0, 0.08)],
    "wolf":   [(0, 0, 0.0), (0, -1, 0.0), (1, -1, 0.0), (0, 0, 0.0), (0, -1, 0.0), (-1, -1, 0.0)],
    "goblin": [(0, 0, 0.0), (0, -1, 0.0), (1, 0, 0.0), (0, 1, -0.05), (0, 0, 0.0), (-1, -1, 0.0)],
    "wraith": [(0, -1, 0.0), (0, -2, 0.0), (1, -3, 0.0), (0, -3, 0.0), (-1, -2, 0.0), (0, -1, 0.0)],
}


def creature_frames(name, pack, tid, canvas=22):
    src = crop_nonempty(strip_backing(load(pack, tid)))
    out = []
    for dx, dy, sq in PHASES[name]:
        s = src
        if abs(sq) > 0.001:   # squash preserving volume
            nh = max(1, round(s.height * (1 - sq)))
            nw = max(1, round(s.width * (1 + sq * 0.8)))
            s = s.resize((nw, nh), Image.NEAREST)
        f = blank(canvas, canvas)
        put(f, s, max(0, (canvas - s.width) // 2 + dx), max(0, canvas - s.height - 1 + dy))
        out.append(f)
    return out


def main():
    os.makedirs(OUT, exist_ok=True)
    written = []
    for name, im in (("tower_base", tower_base()), ("crown_plain", crown_plain()),
                     ("crown_sniper", crown_sniper()), ("crown_elf", crown_elf()),
                     ("crown_hunter", crown_hunter())):
        im.save(os.path.join(OUT, f"{name}.png"))
        written.append(f"{name} {im.width}x{im.height}")
    for name, (pack, tid) in PICKS.items():
        for i, f in enumerate(creature_frames(name, pack, tid)):
            f.save(os.path.join(OUT, f"{name}_{i}.png"))
        written.append(f"{name}_0..5 {f.width}x{f.height}")
    print("composed:")
    for w in written:
        print("  ", w)


if __name__ == "__main__":
    SRC = sys.argv[1] if len(sys.argv) > 1 else "."
    OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                       "assets", "sprites")
    main()
