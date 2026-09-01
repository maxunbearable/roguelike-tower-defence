#!/usr/bin/env python3
"""Imports the CraftPix free monster pack into the sprite atlas.

    python3 tools/import_monsters.py ~/Downloads

Licence: CraftPix free licence -- commercial use permitted, no attribution
required, redistribution of the source art forbidden. That is why the output
lands in assets/sprites/ (gitignored) and only this importer is committed. See
docs/ASSET-POLICY.md.

The pack ships ten monsters as PNG *sequences* of large painted frames, not as
spritesheets:

    Monster_<n>/PNG/PNG Sequences/<Fly|Walking|Dying|...>/*.png

Two things matter when bringing them down to a 64px tile:

1. Every frame is cropped to the UNION bounding box of its whole sequence, not
   to its own. Cropping per frame re-centres the sprite on every frame and the
   creature jitters in place -- the same mistake already documented for the Tiny
   Swords troop cycles in docs/ART.md.
2. Monsters 1-5 have Fly/Fall and no Walking; 6-10 have Walking/Jump. So the
   pack is five fliers and five walkers, which is where the flying enemies come
   from.
"""
import os
import sys
import glob

from PIL import Image

OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                   "assets", "sprites")

# (id, monster number, movement sequence, target height in px)
# Heights differ because these are creatures of different mass, and a uniform
# height makes a gnat and a grub the same size, which reads as wrong.
MONSTERS = [
    ("bloodgnat",   1,  "Fly",     34),
    ("hivewasp",    2,  "Fly",     40),
    ("cryptbat",    3,  "Fly",     38),
    ("emberimp",    4,  "Fly",     42),
    ("voidgazer",   5,  "Fly",     44),
    ("gnashmaw",    6,  "Walking", 46),
    ("hornfiend",   7,  "Walking", 44),
    ("palecrawler", 8,  "Walking", 42),
    ("tombgrub",    9,  "Walking", 40),
    ("gorgemaw",   10,  "Walking", 46),
]

FRAMES = 6  # the atlas convention: <id>_0 .. <id>_5


def union_bbox(paths):
    box = None
    for p in paths:
        b = Image.open(p).convert("RGBA").getbbox()
        if not b:
            continue
        box = b if box is None else (min(box[0], b[0]), min(box[1], b[1]),
                                     max(box[2], b[2]), max(box[3], b[3]))
    return box


def sample(paths, n):
    """Evenly spaced frames. The sequences are 18 frames; the atlas wants 6, and
    every third frame still reads as the same motion at tile size."""
    if len(paths) <= n:
        return paths
    step = len(paths) / float(n)
    return [paths[min(len(paths) - 1, int(i * step))] for i in range(n)]


def emit(paths, name, height):
    box = union_bbox(paths)
    if not box:
        return 0
    written = 0
    for i, p in enumerate(sample(paths, FRAMES)):
        im = Image.open(p).convert("RGBA").crop(box)
        w = max(1, round(im.width * height / im.height))
        # NEAREST: these are painted, not pixel art, but the game is pixel art
        # and a smooth resample leaves soft half-transparent edges that read as
        # blur against the hard-edged Tiny Swords sprites.
        im = im.resize((w, height), Image.NEAREST)
        im.save(os.path.join(OUT, f"{name}_{i}.png"))
        written += 1
    return written


def main(base):
    os.makedirs(OUT, exist_ok=True)
    root = None
    for cand in glob.glob(os.path.join(base, "**", "Monster_1"), recursive=True):
        root = os.path.dirname(cand)
        break
    if not root:
        sys.exit("could not find 'Monster_1' under " + base)

    log = []
    for name, num, seq, height in MONSTERS:
        seqs = os.path.join(root, f"Monster_{num}", "PNG", "PNG Sequences")
        move = sorted(glob.glob(os.path.join(seqs, seq, "*.png")))
        if not move:  # some monsters label the loop differently
            move = sorted(glob.glob(os.path.join(seqs, "Idle", "*.png")))
        dying = sorted(glob.glob(os.path.join(seqs, "Dying", "*.png")))

        n = emit(move, name, height)
        # The death sequence is the reason a kill can look like a death rather
        # than a puff of particles. Same union-crop rule, so the corpse does not
        # jump on the frame it starts.
        d = emit(dying, name + "_die", height)
        log.append(f"   {name:12s} {n} move + {d} death frames  ({seq}, h={height})")

    print("\n".join(log))
    print(f"   -> {OUT}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.expanduser("~/Downloads"))
