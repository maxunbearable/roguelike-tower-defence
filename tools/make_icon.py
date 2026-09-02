#!/usr/bin/env python3
"""Draws the application icon and writes .icns and .ico.

Hand-drawn from the game's own palette rather than derived from the sprite
packs, whose licences forbid redistribution -- the icon ships inside the
installer and is committed, so it has to be ours.
"""
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "packaging"
OUT.mkdir(exist_ok=True)

# From content/art/sprites.toml.
BG_DARK = (58, 51, 64)
STONE = (107, 115, 133)
STONE_LIT = (188, 196, 212)
STONE_DEEP = (95, 106, 125)
RUNE = (123, 201, 60)
RUNE_HOT = (200, 245, 126)
PARCH = (230, 218, 191)

# Drawn at 32x32 and scaled with nearest neighbour, so it stays pixel art.
# Built from shapes rather than a hand-typed grid: the emblem has to be
# symmetrical, and a grid typed by hand was not.
W = 32


def draw_base() -> Image.Image:
    img = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    # A standing stone: plinth, tapered body, lit left edge, shadowed right.
    d.rectangle([7, 26, 24, 28], fill=(*STONE_DEEP, 255))
    d.rectangle([8, 25, 23, 26], fill=(*STONE, 255))
    d.polygon([(11, 25), (11, 8), (13, 5), (18, 5), (20, 8), (20, 25)],
              fill=(*STONE, 255))
    d.polygon([(11, 25), (11, 8), (13, 5), (14, 5), (13, 8), (13, 25)],
              fill=(*STONE_LIT, 255))
    d.polygon([(18, 25), (18, 8), (17, 5), (18, 5), (20, 8), (20, 25)],
              fill=(*STONE_DEEP, 255))

    # The rune, centred: a diamond with a hot core.
    cx, cy = 15.5, 15
    for r, col in ((5, RUNE), (3, RUNE_HOT)):
        d.polygon([(cx, cy - r), (cx + r, cy), (cx, cy + r), (cx - r, cy)],
                  fill=(*col, 255))
    return img


def compose(size: int) -> Image.Image:
    """A rounded dark plate with the stone centred on it."""
    scale = max(1, size // W)
    plate = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(plate)
    r = int(size * 0.22)
    d.rounded_rectangle([0, 0, size - 1, size - 1], radius=r, fill=(*BG_DARK, 255))
    d.rounded_rectangle([0, 0, size - 1, size - 1], radius=r,
                        outline=(*STONE_DEEP, 255), width=max(1, size // 64))

    art = draw_base().crop((4, 3, 28, 30))  # symmetric margins, kept centred
    k = max(1, int(size * 0.70) // max(art.width, art.height))
    art = art.resize((art.width * k, art.height * k), Image.NEAREST)
    plate.alpha_composite(art, ((size - art.width) // 2, (size - art.height) // 2))
    return plate


def main() -> int:
    master = compose(1024)
    master.save(OUT / "icon.png")

    sizes = [16, 32, 64, 128, 256, 512, 1024]
    master.save(OUT / "Wardstone.ico",
                sizes=[(s, s) for s in sizes if s <= 256])
    print(f"wrote {OUT / 'Wardstone.ico'}")

    if sys.platform == "darwin":
        iconset = OUT / "Wardstone.iconset"
        subprocess.run(["rm", "-rf", str(iconset)], check=False)
        iconset.mkdir()
        for s in [16, 32, 128, 256, 512]:
            compose(s).save(iconset / f"icon_{s}x{s}.png")
            compose(s * 2).save(iconset / f"icon_{s}x{s}@2x.png")
        subprocess.run(["iconutil", "-c", "icns", str(iconset),
                        "-o", str(OUT / "Wardstone.icns")], check=True)
        subprocess.run(["rm", "-rf", str(iconset)], check=False)
        print(f"wrote {OUT / 'Wardstone.icns'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
