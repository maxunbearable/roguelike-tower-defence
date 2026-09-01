#!/usr/bin/env python3
"""Imports element icons and per-tower projectiles from the fire/water magic pack.

    python3 tools/import_magic.py ~/Downloads

The pack is 640px vector-derived art, not pixel art. Downscaled hard it works
anyway: at 18-24px the smooth gradients collapse into flat colour bands, which is
what pixel art is, and a palette quantise finishes the job. Tested before relying
on it -- see docs/previews/magic-test.png.

The pack only covers fire and water. The other four elements are hue-rotated from
those two, which the CraftPix licence permits (modification is allowed,
redistribution is not) and which keeps all six stylistically identical -- the
thing that actually matters when they sit next to each other in a menu.
"""
import colorsys
import glob
import os
import sys

from PIL import Image

OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                   "assets", "sprites")


def find_pack(base):
    for p in glob.glob(os.path.join(base, "**", "Icons", "PNG"), recursive=True):
        if "__MACOSX" in p:
            continue
        if "magic" in p.lower():
            return os.path.dirname(os.path.dirname(p))
    return None


def trim(im):
    bb = im.getbbox()
    return im.crop(bb) if bb else im


def shift(im, hue_rot, sat_mult=1.0, val_mult=1.0):
    """Rotates hue in HSV, preserving alpha. hue_rot is in turns (0..1)."""
    im = im.convert("RGBA")
    px = im.load()
    for y in range(im.height):
        for x in range(im.width):
            r, g, b, a = px[x, y]
            if a == 0:
                continue
            h, s, v = colorsys.rgb_to_hsv(r / 255.0, g / 255.0, b / 255.0)
            h = (h + hue_rot) % 1.0
            s = min(1.0, s * sat_mult)
            v = min(1.0, v * val_mult)
            nr, ng, nb = colorsys.hsv_to_rgb(h, s, v)
            px[x, y] = (int(nr * 255), int(ng * 255), int(nb * 255), a)
    return im


def to_size(im, side, colors=16):
    im = trim(im)
    k = side / max(im.width, im.height)
    small = im.resize((max(1, round(im.width * k)), max(1, round(im.height * k))),
                      Image.LANCZOS)
    q = small.convert("RGB").quantize(colors=colors, method=Image.MEDIANCUT).convert("RGBA")
    # Hard alpha cut: a soft edge on a 20px sprite reads as blur, not as shape.
    q.putalpha(small.split()[3].point(lambda a: 255 if a > 100 else 0))
    return q


def save(im, name, log):
    im.save(os.path.join(OUT, f"{name}.png"))
    log.append(f"{name} {im.width}x{im.height}")


def main(base):
    root = find_pack(base)
    if not root:
        print("magic pack not found; pass the folder holding it", file=sys.stderr)
        return 1
    icons = os.path.join(root, "Icons", "PNG")
    fire = Image.open(os.path.join(icons, "Icons_Fire Ball.png")).convert("RGBA")
    water = Image.open(os.path.join(icons, "Icons_Water Ball.png")).convert("RGBA")
    spell_f = Image.open(os.path.join(icons, "Icons_Fire Spell.png")).convert("RGBA")

    log = []

    # --- element icons, all six the same size so the menu ring is even -------
    # hue rotations chosen against the element's obvious colour, not arbitrarily.
    save(to_size(fire, 22), "icon_fire", log)
    save(to_size(water, 22), "icon_water", log)
    save(to_size(shift(fire, 0.28, 0.85, 0.92), 22), "icon_earth", log)     # orange -> green
    save(to_size(shift(water, 0.10, 0.45, 1.10), 22), "icon_wind", log)     # cyan -> pale
    save(to_size(shift(fire, 0.72, 0.80, 0.70), 22), "icon_shadow", log)    # orange -> violet
    save(to_size(shift(fire, 0.08, 0.45, 1.15), 22), "icon_light", log)     # orange -> gold

    # --- per-tower projectiles ---------------------------------------------
    # A cannon shell drawn as an arrow was the single most obviously wrong thing
    # on the board. `arrow` stays for the arrow tower and as the fallback.
    arrows = sorted(g for g in glob.glob(os.path.join(root, "Fire Arrow", "PNG", "*.png"))
                    if "__MACOSX" not in g)
    warrows = sorted(g for g in glob.glob(os.path.join(root, "Water Arrow", "PNG", "*.png"))
                     if "__MACOSX" not in g)
    if arrows:
        # ballista: a heavy steel bolt -- desaturated, slightly larger.
        save(to_size(shift(Image.open(arrows[0]).convert("RGBA"), 0.55, 0.25, 1.0), 26),
             "proj_ballista", log)
    if warrows:
        # arcane: a violet mote. The source is CYAN (hue ~0.5), so this is +0.25,
        # not the +0.72 used on the orange sources -- rotating cyan by 0.72 lands
        # on green, which is what the first attempt produced.
        save(to_size(shift(Image.open(warrows[0]).convert("RGBA"), 0.25, 0.95, 1.0), 20),
             "proj_arcane", log)
    # cannon: a round stone shot.
    save(to_size(shift(fire, 0.10, 0.18, 0.75), 18), "proj_cannon", log)
    # brazier: a small hot ember.
    save(to_size(fire, 13), "proj_brazier", log)

    # --- one icon per element SPEC -----------------------------------------
    # 18 specs from 3 shapes x 6 element hues. Shape says WHICH spec within the
    # element, hue says which element, so a spec is identifiable in the ring and
    # in its skill-tree node without any per-spec artwork.
    ball = fire
    spell = spell_f
    arrow_src = None
    _a = sorted(g for g in glob.glob(os.path.join(root, "Fire Arrow", "PNG", "*.png"))
                if "__MACOSX" not in g)
    if _a:
        arrow_src = Image.open(_a[0]).convert("RGBA")

    # Hue rotation per element, measured from the orange source.
    ELEMENT_HUE = {
        "earth":  (0.28, 0.85, 0.92),
        "fire":   (0.00, 1.00, 1.00),
        "water":  (0.52, 0.90, 1.00),
        "wind":   (0.45, 0.40, 1.15),
        "shadow": (0.72, 0.80, 0.70),
        "light":  (0.08, 0.45, 1.15),
    }
    # Spec order must match each element tree's `specs` list.
    ELEMENT_SPECS = {
        "earth":  ["poison", "rock", "quake"],
        "fire":   ["burn", "blast", "melt"],
        "water":  ["chill", "shatter", "freeze"],
        "wind":   ["shock", "gust", "cyclone"],
        "shadow": ["wither", "siphon", "rift"],
        "light":  ["sear", "judgement", "beacon"],
    }
    shapes = [ball, spell, arrow_src]
    for elem, specs in ELEMENT_SPECS.items():
        h, sa, v = ELEMENT_HUE[elem]
        for i, spec in enumerate(specs):
            src = shapes[i % len(shapes)]
            if src is None:
                continue
            # earth already has three bespoke overlays and icons; do not clobber.
            if elem == "earth":
                continue
            save(to_size(shift(src, h, sa, v), 20), f"icon_{spec}", log)
            save(to_size(shift(src, h, sa, v), 24), f"enchant_{spec}", log)

    # --- element overlays for the specs that had none -----------------------
    # One per element rather than per spec: the spec is already named on the
    # stats panel, and 18 hand-authored overlays is not a thing this pack can
    # give. The overlay says WHICH ELEMENT is imbued, which is the readable bit.
    overlays = {
        "enchant_earth": (spell_f, 0.28, 0.85, 0.92),
        "enchant_fire": (spell_f, 0.0, 1.0, 1.0),
        "enchant_water": (spell_f, 0.52, 0.9, 1.0),
        "enchant_wind": (spell_f, 0.45, 0.4, 1.15),
        "enchant_shadow": (spell_f, 0.72, 0.8, 0.7),
        "enchant_light": (spell_f, 0.08, 0.45, 1.15),
    }
    for name, (src, h, s, v) in overlays.items():
        save(to_size(shift(src, h, s, v), 26), name, log)

    for line in log:
        print("  " + line)
    print(f"  {len(log)} sprites")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else os.path.expanduser("~/Downloads")))
