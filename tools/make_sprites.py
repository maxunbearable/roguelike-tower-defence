#!/usr/bin/env python3
"""Generates content/art/sprites.toml in full.

Single source of truth for the art. Regenerate with:

    python3 tools/make_sprites.py

Sprites are DRAWN with primitives rather than typed as ASCII rows, because you
cannot feel a curve one character at a time. Shading follows the standard pixel
art method -- base, then highlight toward the light, then shadow away from it,
outline last -- and every ramp is hue-shifted, shadows toward blue/purple and
highlights toward yellow.
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pixelart import *

PALETTE = {
    ".": "00000000",
    # Outline is dark GREY, not near-black. Measured from the reference pack:
    # pure black outlines make a bright scene look grimy.
    "#": "3a3340ff",
    "%": "00000055",

    # Every ramp below was rebuilt after measuring a reference sheet. The old
    # palette sat far too dark and desaturated -- grass at #2f4a28 against a
    # reference grass of #7bad2c -- which is what made the whole board look
    # muddy no matter how well the sprites were drawn.
    "1": "3d8a2a", "2": "58a832", "3": "7bc93c", "4": "9fe055", "5": "c8f57e",  # green
    "6": "6a7385", "7": "949db2", "8": "bcc4d4", "9": "dde2ec", "0": "f4f6fa",  # stone (light!)
    "a": "6b4a24", "b": "8f673f", "c": "b98b5e", "d": "c8a480", "e": "e6dabf",  # wood/dirt
    "f": "5f6a7d", "g": "8b96aa", "h": "b3bccd", "i": "d5dce8", "j": "f0f4fa",  # fur
    "k": "a8700f", "l": "d99a1c", "m": "f2bd33", "n": "ffd95c", "o": "fff3ad",  # gold
    "p": "7a2020", "q": "b83232", "r": "e0533f", "s": "ff8a63",                 # red
    "t": "2b7fae", "u": "4fb0dd", "v": "7fd0f0", "w": "a8e4fa", "x": "d8f4ff",  # blue
    "y": "4a7a24", "z": "5f9330", "A": "76ad3c",                                # scenery
    # dark-fantasy additions: corrupted flesh, spectral cloth, sick glow
    "B": "2b1f3d", "C": "44305c", "D": "5f4780", "E": "8264a8", "F": "a98fd0",  # violet
    "G": "1a2416", "H": "2e3f26", "I": "46603a", "J": "63834f",                 # rot green
    "K": "0f1218", "L": "1e2430", "M": "36404f",                                # shadow
}
PALETTE = {k: (v if len(v) == 8 else v + "ff") for k, v in PALETTE.items()}

GREEN=["1","2","3","4","5"]; STONE=["6","7","8","9","0"]; WOOD=["a","b","c","d","e"]
FUR=["f","g","h","i","j"];   GOLD=["k","l","m","n","o"];  RED=["p","q","r","s","s"]
BLUE=["t","u","v","w","x"];  SCEN=["y","y","z","A","A"]


def creatures():
    """Six-frame cycles, front-facing, three tones plus outline.

    Frame count follows the usual guidance for this sprite size: four is the
    minimum that reads as motion, six is smooth. Squash and stretch preserves
    VOLUME -- two pixels lost in height are added across the width -- otherwise a
    creature appears to shrink rather than compress. Ears and antennae lag the
    body by a frame, which is the secondary motion that stops a cycle looking
    mechanical.
    """
    out = []
    # Dark-fantasy tone: the ooze is rot, not lime; the wolf is black-furred,
    # not a husky. Bright, friendly creatures undercut the whole setting.
    GRN  = tri("H", "I", "J")     # rot green
    FURC = tri("L", "f", "h")     # near-black fur with cold highlights
    GLD  = tri("k", "m", "o")
    SKIN = tri("H", "I", "J")

    FRAMES = 6

    # ---- SLIME: a hop. Compress, launch, float, land, absorb, recover. -----
    # (dy, extra width, extra height) per phase; width grows as height shrinks.
    SLIME_PHASE = [(0, 0.0, 0.0), (-1, -0.6, 0.9), (-3, -1.0, 1.4),
                   (-1, 0.4, -0.6), (1, 1.4, -1.6), (0, 0.6, -0.7)]

    def slime(f):
        c = Canvas(24, 24)
        dy, dw, dh = SLIME_PHASE[f]
        cy = 15.5 + dy
        rx, ry = 8.5 + dw, 6.5 + dh
        body = ellipse(12, cy, rx, ry)
        body |= rect(int(12 - rx) + 1, int(cy), int(12 + rx) - 1, int(cy + ry) - 1)
        shade(c, body, GRN, depth=2)
        eye = cy - 2.0
        for ex in (9, 15):
            paint(c, ellipse(ex, eye, 1.4, 1.8), "#")
            paint(c, {(ex - 1, int(eye) - 1)}, "0")
        paint(c, ellipse(12, cy + 2.5, 2.2, 1.0), "2")
        outline(c)
        return c

    # ---- WOLF: front-facing walk. Body bobs, legs alternate, ears lag. -----
    def wolf(f):
        c = Canvas(24, 24)
        bob = [0, -1, -1, 0, -1, -1][f]
        earLag = [0, 0, -1, -1, 0, 0][f]          # secondary motion
        legA = [0, 1, 2, 2, 1, 0][f]
        head = ellipse(12, 9.5 + bob, 6.5, 5.5)
        ears = (triangle((5, 8 + bob + earLag), (6, 0 + bob + earLag), (11, 5 + bob + earLag)) |
                triangle((18, 8 + bob + earLag), (17, 0 + bob + earLag), (12, 5 + bob + earLag)))
        body = ellipse(12, 18 + bob, 5.5, 4.0)
        ruff = triangle((5, 14 + bob), (12, 20 + bob), (19, 14 + bob))
        lx, rx_ = 8 - (legA - 1), 15 + (legA - 1)
        legs = rect(lx, 20, lx + 1, 22) | rect(rx_, 20, rx_ + 1, 22)
        shade(c, head | ears | body | ruff | legs, FURC, depth=2)
        paint(c, ellipse(12, 12.5 + bob, 3.0, 2.4), "j")
        paint(c, triangle((9, 16 + bob), (12, 20 + bob), (15, 16 + bob)), "j")
        paint(c, ellipse(12, 13.5 + bob, 1.1, 0.8), "#")
        for ex in (9, 15):
            paint(c, ellipse(ex, 9 + bob, 1.5, 1.3), "#")
            paint(c, {(ex, 9 + bob)}, "r")
        paint(c, ellipse(6.5, 5.5 + bob + earLag, 1.0, 1.6), "p")
        paint(c, ellipse(17.5, 5.5 + bob + earLag, 1.0, 1.6), "p")
        outline(c)
        return c

    # ---- GOBLIN: a lurching walk with a shield that swings a frame behind. --
    def goblin(f):
        c = Canvas(24, 24)
        bob = [0, -1, 0, 1, 0, -1][f]
        lean = [0, 0, 1, 1, 0, 0][f]
        shieldY = [0, 0, -1, -1, 0, 1][f]
        legA = [0, 1, 2, 1, 0, 1][f]
        head = ellipse(12 + lean, 9 + bob, 6.0, 5.2)
        ears = (triangle((6 + lean, 10 + bob), (0 + lean, 4 + bob), (7 + lean, 5 + bob)) |
                triangle((18 + lean, 10 + bob), (24 + lean, 4 + bob), (17 + lean, 5 + bob)))
        body = ellipse(12, 17.5 + bob, 4.6, 4.0)
        lx, rx_ = 9 - (legA - 1), 14 + (legA - 1)
        legs = rect(lx, 20, lx + 1, 22) | rect(rx_, 20, rx_ + 1, 22)
        shade(c, head | ears | body | legs, GRN, depth=2)
        for ex in (9 + lean, 15 + lean):
            paint(c, ellipse(ex, 8 + bob, 1.6, 1.8), "#")
            paint(c, {(ex - 1, 7 + bob)}, "o")
        paint(c, rect(9 + lean, 12 + bob, 15 + lean, 13 + bob), "#")
        for tx in (10, 12, 14):
            paint(c, {(tx + lean, 12 + bob)}, "0")
        shade(c, ellipse(4, 17 + shieldY, 3.0, 4.0), tri("a", "c", "e"), depth=1)
        paint(c, ellipse(4, 17 + shieldY, 1.1, 1.3), "9")
        outline(c)
        return c

    # ---- WRAITH: a hooded spectre. No legs -- it hangs above the ground, and
    # its cloak drifts. Flying enemies should never look cheerful.
    WRA = tri("C", "E", "F")
    def wraith(f):
        c = Canvas(24, 24)
        hover = [0, -1, -2, -2, -1, 0][f]
        drift = [0, 1, 1, 0, -1, -1][f]          # the hem swings behind the body

        hood = ellipse(12, 8 + hover, 5.4, 5.0)
        # A tattered cloak: wide at the shoulders, fraying into points below.
        cloak = triangle((5, 11 + hover), (12, 10 + hover), (19, 11 + hover))
        cloak |= triangle((6, 12 + hover), (12, 22 + hover), (18, 12 + hover))
        shade(c, hood | cloak, WRA, depth=2)

        # Ragged hem: alternating tags, shifted by the drift so it flutters.
        for hx in range(6, 19, 3):
            tail = 2 + ((hx + drift) % 3)
            paint(c, rect(hx + drift, 19 + hover, hx + drift + 1, 19 + hover + tail), "C")
        # The hood interior is a void, with two burning eyes in it.
        paint(c, ellipse(12, 9 + hover, 3.4, 3.0), "K")
        for ex in (10, 14):
            paint(c, ellipse(ex, 9 + hover, 1.0, 1.2), "n")
            paint(c, {(ex, 9 + hover)}, "o")
        # Skeletal hands emerging from the cloak.
        paint(c, {(5, 13 + hover), (4, 14 + hover), (19, 13 + hover), (20, 14 + hover)}, "e")
        outline(c)
        return c

    for name, fn in (("slime", slime), ("wolf", wolf), ("goblin", goblin),
                     ("wraith", wraith)):
        for f in range(FRAMES):
            out.append((f"{name}_{f}", fn(f)))
    return out


def structures():
    """The tower line.

    Kingdom Rush's archer towers are "manned with two archers" -- the shooters
    are visible, which is what stops a tower reading as an empty keep. So the
    base here is only the STRUCTURE, and each specialisation is a manned
    platform with its own weapon. An unspecialised tower gets a plain pair of
    archers, so it never looks unfinished.

    Silhouette does the work: one tall longbow, a leafy canopy, or a rack of
    three. At 32 px wide, detail turns to noise but an outline still reads.
    """
    out = []
    # A wider value spread than 7/9/0: those three sit so close together that the
    # shaft reads as one flat column. 6 -> 8 -> 0 gives the masonry real form
    # while staying light enough to pop against bright grass.
    STN = tri("6", "8", "0")
    WD  = tri("a", "c", "e")     # warm timber
    GRN = tri("1", "3", "5")
    SKN = tri("b", "d", "e")     # skin/cloth

    # ---- archer figure, ~7px tall, used by every crown ------------------
    def archer(c, x, y, ramp, bowSide=1, bowSize=4):
        paint(c, ellipse(x, y - 4, 1.8, 1.8), ramp[3])        # head
        paint(c, {(x, y - 5)}, ramp[4])                       # lit crown of head
        paint(c, rect(x - 1, y - 2, x + 1, y + 1), ramp[2])   # torso
        paint(c, {(x - 1, y + 2), (x + 1, y + 2)}, ramp[1])   # legs
        if bowSize <= 0:
            return                       # the sniper's bow is drawn separately
        bx = x + bowSide * 3
        for t in range(-bowSize, bowSize + 1):                # bow arc
            dx = int(1.6 * (1 - (t / float(bowSize)) ** 2))
            paint(c, {(bx + bowSide * dx, y - 2 + t)}, "9")
        paint(c, {(bx, y - 2 - bowSize), (bx, y - 2 + bowSize)}, "b")

    # ---- tower structure -------------------------------------------------
    c = Canvas(32, 56)
    plinth = rect(1, 45, 30, 53)          # wider foot: the tower should feel planted
    body   = rect(6, 26, 25, 46)
    corbel = rect(3, 20, 28, 27)          # timber hoarding, overhangs the shaft
    parapet= rect(5, 13, 26, 21)
    shade(c, plinth | body | parapet, STN, depth=2)
    shade(c, corbel, WD, depth=2)

    for x in range(6, 27, 7):             # crenellations cut out of the cap
        paint(c, rect(x, 13, x + 3, 17), ".")

    # Masonry courses. Without them a stone shaft is just a light rectangle.
    for cy in range(30, 46, 5):
        paint(c, rect(7, cy, 24, cy), "6")
        paint(c, rect(7, cy + 1, 24, cy + 1), "9")
        offset = 4 if (cy // 5) % 2 else 9
        for jx in range(7 + offset, 24, 9):
            paint(c, rect(jx, cy - 3, jx, cy - 1), "6")   # vertical joints

    # Loopholes, dark and wide enough to survive at 1x -- these are the single
    # clearest cue that the building is full of archers.
    for lx in (10, 19):
        paint(c, rect(lx, 30, lx + 2, 42), "#")
        paint(c, rect(lx - 2, 34, lx + 4, 36), "#")
        paint(c, rect(lx, 30, lx + 2, 30), "6")           # lintel shadow

    for x in range(5, 29, 4):             # timber beam ends under the hoarding
        paint(c, rect(x, 26, x, 28), "a")
    paint(c, rect(2, 45, 29, 45), "e")    # cream trim where tiers will show
    paint(c, rect(1, 52, 30, 53), "6")    # dark footing grounds the whole thing
    outline(c)
    out.append(("tower_base", c))

    # ---- crowns: the manned platform, one per specialisation -------------
    # Plain: two ordinary archers. An unspecialised tower is still garrisoned.
    c = Canvas(32, 24)
    shade(c, rect(6, 18, 25, 22), STN, depth=1)
    archer(c, 11, 16, SKN, bowSide=-1, bowSize=3)
    archer(c, 20, 16, SKN, bowSide=1, bowSize=3)
    outline(c)
    out.append(("crown_plain", c))

    # Sniper: ONE archer, an enormous longbow, raised on a stone step.
    c = Canvas(32, 24)
    shade(c, rect(8, 18, 23, 22), STN, depth=1)
    shade(c, rect(12, 14, 19, 19), STN, depth=1)      # firing step
    bow = set()
    for t in range(0, 22):
        dx = int(4.6 * (1 - ((t - 10.5) / 10.5) ** 2))
        bow |= {(23 - dx, 1 + t), (24 - dx, 1 + t)}
    shade(c, bow, WD, depth=1)
    paint(c, line(25, 2, 25, 22, 1), "0")             # string
    archer(c, 15, 15, SKN, bowSide=0, bowSize=0)
    paint(c, line(17, 12, 28, 12, 1), "9")            # nocked arrow, drawn long
    paint(c, triangle((28, 10), (31, 12), (28, 14)), "r")
    outline(c)
    out.append(("crown_sniper", c))

    # Elf: a leafy canopy over a light platform. Organic, not masonry.
    c = Canvas(32, 24)
    shade(c, rect(5, 18, 26, 22), WD, depth=1)
    for lx, ly, r in ((9, 8, 4.6), (16, 5, 5.4), (23, 8, 4.6), (12, 11, 3.6), (20, 11, 3.6)):
        shade(c, ellipse(lx, ly, r, r * 0.78), GRN, depth=1)
    archer(c, 12, 17, GRN, bowSide=-1, bowSize=4)
    archer(c, 21, 17, GRN, bowSide=1, bowSize=4)
    paint(c, {(16, 14), (16, 15)}, "a")               # trunk peeking through
    outline(c)
    out.append(("crown_elf", c))

    # Hunter: three archers shoulder to shoulder on a wide timber deck.
    c = Canvas(32, 24)
    shade(c, rect(2, 17, 29, 22), WD, depth=1)
    paint(c, rect(2, 17, 29, 17), "e")
    archer(c, 7, 15, SKN, bowSide=-1, bowSize=3)
    archer(c, 16, 15, SKN, bowSide=1, bowSize=3)
    archer(c, 25, 15, SKN, bowSide=1, bowSize=3)
    for qx in (4, 28):                                # quivers on the rail
        paint(c, rect(qx, 13, qx + 1, 17), "b")
        paint(c, {(qx, 12), (qx + 1, 12)}, "9")
    outline(c)
    out.append(("crown_hunter", c))

    # ---- element enchantment: grows ON the tower, not a gem beside it ----
    c = Canvas(32, 20)                                # poison: vines and ooze
    for vx, vy in ((5, 4), (12, 2), (20, 3), (27, 5)):
        paint(c, line(vx, vy, vx, vy + 9, 1), "1")
        paint(c, ellipse(vx, vy, 2.0, 1.6), "3")
        paint(c, {(vx - 1, vy - 1)}, "5")
    for dx in (8, 16, 24):                            # drips
        paint(c, {(dx, 13), (dx, 14), (dx, 16)}, "3")
    outline(c)
    out.append(("enchant_poison", c))

    c = Canvas(32, 20)                                # rock: crystals jutting out
    for sx, sy, h in ((6, 12, 7), (13, 9, 10), (21, 10, 9), (27, 13, 6)):
        paint(c, triangle((sx - 2, sy + h), (sx, sy), (sx + 2, sy + h)), "7")
        paint(c, triangle((sx - 1, sy + h), (sx, sy), (sx + 1, sy + h)), "9")
        paint(c, {(sx, sy + 1)}, "0")
    outline(c)
    out.append(("enchant_rock", c))

    c = Canvas(32, 20)                                # quake: cracks and runes
    paint(c, line(3, 16, 10, 12, 1) | line(10, 12, 15, 17, 1), "a")
    paint(c, line(18, 17, 23, 12, 1) | line(23, 12, 29, 15, 1), "a")
    for rx in (8, 16, 24):
        paint(c, ellipse(rx, 8, 2.2, 2.2), "l")
        paint(c, ellipse(rx, 8, 1.0, 1.0), "n")
    outline(c)
    out.append(("enchant_quake", c))

    c = Canvas(12, 12)
    paint(c, line(1, 6, 8, 6, 2), "9")
    paint(c, triangle((8, 3), (11, 6), (8, 9)), "0")
    paint(c, {(0, 4), (1, 5), (0, 8), (1, 7), (2, 5), (2, 7)}, "e")
    outline(c)
    out.append(("arrow", c))

    c = Canvas(12, 12)
    g = {(x, y) for y in range(1, 11) for x in range(1, 11)
         if abs(x - 5.5) + abs((y - 5.5) * 0.8) <= 4.6}
    shade(c, g, tri("t", "v", "x"), depth=1)
    paint(c, {(4, 3), (5, 3), (4, 4)}, "x")
    outline(c)
    out.append(("gem", c))
    return out


def scenery():
    out = []
    c = Canvas(16,16); shade(c, ellipse(8,11,5.0,3.0), SCEN, depth=1); outline(c)
    out.append(("prop_bush", c))
    c = Canvas(16,16); shade(c, ellipse(8,11,4.4,3.0), STONE, depth=1); outline(c)
    out.append(("prop_rock", c))
    c = Canvas(16,16)
    for fx, col in ((5,"n"), (8,"o"), (11,"n")):
        paint(c, line(fx,13,fx,9,1), "z")
        paint(c, ellipse(fx,8,1.8,1.8), col)
        paint(c, {(fx,8)}, "l")
    paint(c, rect(4,13,12,13), "y"); outline(c)
    out.append(("prop_flowers", c))
    c = Canvas(16,16)
    shade(c, ellipse(8,11,3.6,2.6), WOOD, depth=1)
    paint(c, ellipse(8,10,1.6,1.0), "b"); outline(c)
    out.append(("prop_stump", c))
    return out


def icons():
    def ic(fn):
        c = Canvas(12, 12); fn(c); outline(c); return c
    def build(c):                                     # a little battlemented tower
        shade(c, rect(2,4,9,10), STONE, depth=1)
        paint(c, rect(2,2,3,4), "9"); paint(c, rect(5,2,6,4), "9")
        paint(c, rect(8,2,9,4), "9"); paint(c, rect(5,7,6,10), "6")
    def level(c):                                     # a fat upward chevron
        arrow = {(x,y) for y in range(1,6) for x in range(1,11) if abs(x-5.5) <= y*1.1}
        shade(c, arrow | rect(4,5,7,10), GOLD, depth=1)
    def spec(c):
        paint(c, line(6,1,6,10,1) | line(2,4,9,4,1) | line(3,9,9,2,1) | line(3,2,9,9,1), "n")
    def gem(c):                                       # faceted, not a rounded bar
        g = {(x,y) for y in range(1,11) for x in range(1,11)
             if abs(x-5.5) + abs((y-5.5)*0.8) <= 4.6}
        shade(c, g, BLUE, depth=1)
        paint(c, {(4,3),(5,3),(4,4)}, "x")
    def sell(c):
        shade(c, ellipse(5.5,5.5,4.2,4.2), GOLD, depth=1); paint(c, ellipse(5.5,5.5,1.6,2.2), "k")
    def back(c):
        paint(c, line(2,6,10,6,2) | line(2,6,6,2,2) | line(2,6,6,10,2), "0")
    def sniper(c):                                    # a drawn bow, seen side on
        bowl = set()
        for t in range(1,11):
            dx = int(2.6 * (1 - ((t-5.5)/4.5)**2))
            bowl |= line(3-dx, t, 3-dx+1, t, 1)
        shade(c, bowl, WOOD, depth=1)
        paint(c, line(4,1,4,10,1), "0")
        paint(c, line(2,5,10,5,2), "9"); paint(c, {(10,4),(11,5),(10,6)}, "r")
    def elf(c):                                       # twin banners with weight
        paint(c, line(2,1,2,10,2) | line(9,1,9,10,2), "b")
        shade(c, ellipse(2,4,2.4,3.4) | ellipse(9,4,2.4,3.4), GREEN, depth=1)
    def hunter(c):                                    # three arrows with real heads
        for ax in (2,5,8):
            paint(c, line(ax,3,ax,10,2), "9")
            paint(c, ellipse(ax+0.5,2,1.8,1.8), "r")
    def poison(c): shade(c, ellipse(5.5,7,3.4,3.4) | ellipse(5.5,3,1.2,2.0), GREEN, depth=1)
    def rock(c):   shade(c, ellipse(5.5,7,4.2,3.2), STONE, depth=1)
    def quake(c):
        paint(c, ellipse(5.5,5.5,5.0,3.0) - ellipse(5.5,5.5,3.6,2.0), "d")
        paint(c, ellipse(5.5,5.5,2.2,1.4), "s")
    def heart(c):
        h = ellipse(3.5,4,2.4,2.4) | ellipse(7.5,4,2.4,2.4)
        h |= {(x,y) for (x,y) in rect(1,4,10,10) if abs(x-5.5) <= (10-y)*0.9}
        shade(c, h, RED, depth=1)
    def coin(c):
        shade(c, ellipse(5.5,5.5,4.4,4.4), GOLD, depth=1); paint(c, line(5,3,5,8,1), "k")
    def play(c):
        paint(c, {(x,y) for y in range(2,10) for x in range(3,9)
                  if (x-3) <= (4-abs(y-5.5))*1.6}, "n")
    def ffwd(c):
        for ox in (1,6):
            paint(c, {(x,y) for y in range(3,9) for x in range(ox,ox+4)
                      if (x-ox) <= (3-abs(y-5.5))*1.6}, "n")
    def pause(c): shade(c, rect(2,2,4,9) | rect(7,2,9,9), GOLD, depth=1)
    names = [("icon_build",build),("icon_level",level),("icon_spec",spec),("icon_gem",gem),
             ("icon_sell",sell),("icon_back",back),("icon_sniper",sniper),("icon_elf",elf),
             ("icon_hunter",hunter),("icon_poison",poison),("icon_rock",rock),
             ("icon_quake",quake),("icon_heart",heart),("icon_coin",coin),("icon_play",play),
             ("icon_ffwd",ffwd),("icon_pause",pause)]
    return [(n, ic(f)) for n, f in names]


def main():
    sprites = creatures() + structures() + scenery() + icons()
    head = ("# GENERATED by tools/make_sprites.py -- do not hand-edit.\n"
            "# Ramps are hue-shifted (shadows toward blue/purple, highlights toward\n"
            "# yellow) and every sprite is lit from the upper left.\n\n[palette]\n")
    head += "".join(f'"{k}" = "{v}"\n' for k, v in PALETTE.items()) + "\n"
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    with open(os.path.join(here, "content/art/sprites.toml"), "w") as f:
        f.write(head + emit(sprites))
    print(f"wrote {len(sprites)} sprites")


if __name__ == "__main__":
    main()
