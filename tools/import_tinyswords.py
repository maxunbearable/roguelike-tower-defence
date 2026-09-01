#!/usr/bin/env python3
"""Imports Tiny Swords (Pixel Frog) art into assets/sprites/.

    python3 tools/import_tinyswords.py ~/Downloads

Expects the two zips already unpacked, or pass the folder holding
"Tiny Swords (Free Pack)" and "Tiny Swords (Update 010)".

The pack gives WHOLE BUILDINGS rather than a base plus a crown, so each tower
specialisation becomes one sprite with its own silhouette and faction colour.
Frames are sliced from the animation sheets by their known frame size.
"""
import os, sys, glob
from PIL import Image

OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                   "assets", "sprites")


def find_root(base, needle):
    for p in glob.glob(os.path.join(base, "**", needle), recursive=True):
        if "__MACOSX" not in p:
            return p
    return None


def trim(im):
    """Crops transparent margins. Sprites are anchored by their real art, not by
    the padding the sheet happened to have."""
    bb = im.getbbox()
    return im.crop(bb) if bb else im


def half(im):
    """Exact 2:1 downscale. The pack is RTS-scale, where a tower spans two or
    three tiles; a defence game wants one tile per tower. Halving is the only
    clean reduction for pixel art -- any other ratio leaves uneven pixels."""
    return im.resize((max(1, im.width // 2), max(1, im.height // 2)), Image.NEAREST)


def save(im, name):
    im.save(os.path.join(OUT, f"{name}.png"))
    return f"{name} {im.width}x{im.height}"


def slice_frames(path, fw, fh):
    im = Image.open(path).convert("RGBA")
    return [im.crop((x, 0, x + fw, fh)) for x in range(0, im.width, fw)]


def _bands(flags):
    """Contiguous runs of True, as (start, end) pairs."""
    out, run = [], None
    for i, v in enumerate(list(flags) + [False]):
        if v and run is None:
            run = i
        elif not v and run is not None:
            out.append((run, i))
            run = None
    return out


def detect_pitch(im):
    """Measures a sheet's frame size instead of assuming it.

    Barrel_Red is 6x6 of 128px while every other troop sheet is 192px; assuming
    192 everywhere silently cropped 2x2 blocks of four goblins into one frame.
    The gap between columns of art gives the pitch away, so measure it and snap
    to a divisor of both dimensions.
    """
    px = im.split()[3].load()
    cols = _bands([any(px[x, y] > 8 for y in range(im.height)) for x in range(im.width)])
    cands = [p for p in range(32, min(im.width, im.height) + 1, 32)
             if im.width % p == 0 and im.height % p == 0]
    if not cands:
        return min(im.width, im.height)
    if len(cols) < 2:
        return max(cands)
    deltas = sorted(b[0] - a[0] for a, b in zip(cols, cols[1:]))
    median = deltas[len(deltas) // 2]
    return min(cands, key=lambda p: abs(p - median))


def cycle_frames(im, pitch, row):
    """One animation row, every frame cropped to the SAME box.

    Trimming each frame to its own bbox re-registers the figure every frame, so
    the sprite jitters and changes size as it walks. The union box keeps the
    whole cycle on one coordinate system.
    """
    frames = [im.crop((c * pitch, row * pitch, (c + 1) * pitch, (row + 1) * pitch))
              for c in range(im.width // pitch)]
    frames = [f for f in frames if f.getbbox()]
    if not frames:
        return []
    boxes = [f.getbbox() for f in frames]
    union = (min(b[0] for b in boxes), min(b[1] for b in boxes),
             max(b[2] for b in boxes), max(b[3] for b in boxes))
    return [f.crop(union) for f in frames]


def main(base):
    os.makedirs(OUT, exist_ok=True)
    free = find_root(base, "Tiny Swords (Free Pack)")
    cc0 = find_root(base, "Tiny Swords (Update 010)")
    if not free:
        sys.exit("could not find 'Tiny Swords (Free Pack)' under " + base)
    log = []

    # --- towers: one whole building per specialisation ---------------------
    # Distinct silhouettes AND distinct faction colours, so the board reads at a
    # glance: a round keep for the plain and sniper towers, a wide hall with a
    # roof for the two multi-archer specialisations.
    # Shape carries the meaning, not colour. The previous mapping gave sniper
    # the same silhouette as plain and hunter the same as elf, so the board said
    # nothing about what a tower actually did.
    #   plain  -> a modest house: the unspecialised starting structure
    #   sniper -> the tall narrow keep: height reads as reach
    #   elf    -> a timber perch: rustic and organic, not masonry
    #   hunter -> the archery hall, targets and arrows everywhere: many shots
    # Each tower type gets its own faction colour, and each spec its own
    # SILHOUETTE rather than just a recolour -- a spec that looks like the base
    # tower with a different palette does not read on a busy board.
    # Each tower type is a faction colour; each spec within it is a different
    # SILHOUETTE, not a recolour -- a spec that differs only in palette does not
    # read on a busy board, which was the complaint about the first pass.
    #
    # Castle (156px wide) and most Monasteries (132px tall) are excluded: at a
    # 64px tile they swallow their neighbours. The arcane base keeps its
    # Monastery deliberately, because a two-tile spire is what an arcane tower
    # should look like.
    towers = {
        # arrow -- blue/purple/red archer line
        "tower_plain":   ("Blue Buildings",   "House3.png"),    # 61x67 hut
        "tower_sniper":  ("Purple Buildings", "Tower.png"),     # 60x92 tall keep
        "tower_hunter":  ("Red Buildings",    "Archery.png"),   # 91x89 wide hall
        # cannon -- black siege line
        "tower_cannon":  ("Black Buildings",  "Barracks.png"),  # 92x93 squat fort
        "tower_mortar":  ("Black Buildings",  "Tower.png"),     # 60x92 long reach
        "tower_bombard": ("Black Buildings",  "House2.png"),    # 64x77 close work
        "tower_siege":   ("Black Buildings",  "Archery.png"),   # 91x89 heavy
        # arcane -- purple/yellow magic line
        "tower_arcane":  ("Purple Buildings", "Monastery.png"), # 80x132 spire
        "tower_tempest": ("Yellow Buildings", "Tower.png"),     # 60x92
        "tower_hex":     ("Yellow Buildings", "House1.png"),    # 56x78
        "tower_drain":   ("Yellow Buildings", "Barracks.png"),  # 92x93
        # ballista -- red war-machine line
        "tower_ballista": ("Red Buildings",    "Barracks.png"),  # 92x93 engine yard
        "tower_harpoon":  ("Red Buildings",    "Tower.png"),     # 60x92 long shot
        "tower_scorpion": ("Red Buildings",    "House2.png"),    # 64x77 rapid
        "tower_javelin":  ("Red Buildings",    "House1.png"),    # 56x78 impact
        # brazier -- the fire line
        "tower_brazier":  ("Yellow Buildings", "House3.png"),    # 61x67 small fire
        "tower_pyre":     ("Yellow Buildings", "Archery.png"),   # 91x89 wide blaze
        "tower_forge":    ("Yellow Buildings", "Monastery.png"), # 80x132 chimney
        "tower_cinder":   ("Yellow Buildings", "House2.png"),    # 64x77 relentless
    }
    for name, (faction, f) in towers.items():
        p = os.path.join(free, "Buildings", faction, f)
        if os.path.exists(p):
            log.append(save(half(trim(Image.open(p).convert("RGBA"))), name))

    # The elf perch comes from the goblin wood tower, which is the only timber
    # tower in either pack. Its first frame only; the sheet is an animation.
    if cc0:
        for colour in ("Blue", "Yellow", "Purple", "Red"):
            wt = os.path.join(cc0, "Factions", "Goblins", "Buildings", "Wood_Tower",
                              f"Wood_Tower_{colour}.png")
            if os.path.exists(wt):
                sheet = Image.open(wt).convert("RGBA")
                log.append(save(half(trim(sheet.crop((0, 0, 256, sheet.height)))), "tower_elf"))
                break

    # The castle makes a far better goal marker than a blue circle.
    castle = os.path.join(free, "Buildings", "Blue Buildings", "Castle.png")
    if os.path.exists(castle):
        log.append(save(half(trim(Image.open(castle).convert("RGBA"))), "goal_castle"))

    # --- the arrow in flight ----------------------------------------------
    arrow = os.path.join(free, "Units", "Blue Units", "Archer", "Arrow.png")
    if os.path.exists(arrow):
        # Halved like every other sprite: at source scale a single arrow spans
        # two thirds of a tile and reads as a thrown axe.
        log.append(save(half(trim(Image.open(arrow).convert("RGBA"))), "arrow"))

    # --- enemies: goblin troops from the CC0 pack --------------------------
    # Sheets are laid out as a row of 192x192 frames.
    if cc0:
        # Mapped onto the ids the game already uses, so no content churn:
        # the barrel goblin is round and rolling (the "slime" slot), the TNT
        # goblin is the fast one, the torch goblin is the goblin, and a knight
        # warrior is the armoured heavy.
        # (faction, folder, file, animation row). Row 0 is Idle and row 1 is
        # Run in this pack; walkers along a path want the run cycle, except the
        # torch goblin whose run drags a wide flame streak that reads as noise
        # at tile size.
        troops = {
            "slime":  ("Goblins", "Barrel",  "Barrel_Red.png",      1),
            "wolf":   ("Goblins", "TNT",     "TNT_Red.png",         1),
            "goblin": ("Goblins", "Torch",   "Torch_Red.png",       0),
            "wraith": ("Knights", "Warrior", "Warrior_Purple.png",  1),
        }
        for name, (faction, folder, fname, row) in troops.items():
            root = os.path.join(cc0, "Factions", faction, "Troops", folder)
            hits = [h for h in glob.glob(os.path.join(root, "**", fname), recursive=True)
                    if "__MACOSX" not in h]
            if not hits:
                hits = [h for h in glob.glob(os.path.join(root, "**", "*.png"), recursive=True)
                        if "__MACOSX" not in h and "Dead" not in h]
            if not hits:
                continue
            im = Image.open(hits[0]).convert("RGBA")
            pitch = detect_pitch(im)
            keep = [half(f) for f in cycle_frames(im, pitch, row)[:6]]
            if not keep:
                continue
            for i, f in enumerate(keep):
                save(f, f"{name}_{i}")
            log.append(f"{name}_0..{len(keep)-1} {keep[0].width}x{keep[0].height} "
                       f"(pitch {pitch}, row {row})")

    # --- UI kit: painted panels, buttons and ribbons ----------------------
    # NOT trimmed. These are nine-slice sources whose border must stay
    # symmetric; cropping transparent margins would shift the slice lines and
    # the corners would no longer match. The pack's "9Slides"/"3Slides" naming
    # is literal: each is a 3x3 (or 3x1) grid of 64px cells, so the slice inset
    # is 64 at source and 32 once halved -- see kUiInset in SpriteAtlas.
    if cc0:
        ui = {
            "ui_panel":       ("Banners", "Carved_9Slides.png"),
            "ui_banner":      ("Banners", "Banner_Horizontal.png"),
            "ui_btn":         ("Buttons", "Button_Blue_9Slides.png"),
            "ui_btn_hover":   ("Buttons", "Button_Hover_9Slides.png"),
            "ui_btn_off":     ("Buttons", "Button_Disable_9Slides.png"),
            "ui_btn_red":     ("Buttons", "Button_Red_9Slides.png"),
            "ui_ribbon":      ("Ribbons", "Ribbon_Yellow_3Slides.png"),
            "ui_ribbon_blue": ("Ribbons", "Ribbon_Blue_3Slides.png"),
            "ui_ribbon_red":  ("Ribbons", "Ribbon_Red_3Slides.png"),
        }
        for name, (folder, fname) in ui.items():
            src = os.path.join(cc0, "UI", folder, fname)
            if not os.path.exists(src):
                continue
            log.append(save(half(Image.open(src).convert("RGBA")), name))

        for i in range(1, 11):
            src = os.path.join(cc0, "UI", "Icons", f"Regular_{i:02d}.png")
            if os.path.exists(src):
                save(half(trim(Image.open(src).convert("RGBA"))), f"ui_icon_{i:02d}")

    # --- scenery: the pack's own props ------------------------------------
    # Ground-level only. Anything tall (the signpost, the scarecrow) competes
    # with the towers for the eye, which is the one thing scenery must not do.
    if cc0:
        deco = {
            "prop_bush":     "08.png",   # leafy bush
            "prop_rock":     "06.png",   # rock cluster
            "prop_flowers":  "11.png",   # grass tuft
            "prop_stump":    "13.png",   # pumpkins
            "prop_mushroom": "03.png",   # toadstool
            "prop_bones":    "14.png",   # bone
        }
        for name, fname in deco.items():
            src = os.path.join(cc0, "Deco", fname)
            if not os.path.exists(src):
                continue
            log.append(save(half(trim(Image.open(src).convert("RGBA"))), name))

    # --- terrain: their grass, so the ground matches the buildings ---------
    tm = os.path.join(free, "Terrain", "Tileset", "Tilemap_color1.png")
    if os.path.exists(tm):
        sheet = Image.open(tm).convert("RGBA")
        # The 9x6 sheet is a grass plateau autotile set; (1,1) is solid interior.
        grass = sheet.crop((64, 64, 128, 128))
        for v in range(4):
            log.append(save(grass, f"ground_grass_{v}"))
        # No dirt road exists in an RTS pack, so derive one from their own grass:
        # same texture, shifted toward warm earth, which keeps it in palette.
        px = grass.load()
        road = Image.new("RGBA", grass.size)
        rp = road.load()
        for y in range(grass.height):
            for x in range(grass.width):
                r, g, b, a = px[x, y]
                lum = (r * 0.35 + g * 0.5 + b * 0.15) / 255.0
                rp[x, y] = (int(150 + 95 * lum), int(108 + 78 * lum), int(72 + 58 * lum), a)
        for v in range(3):
            log.append(save(road, f"ground_dirt_{v}"))

    print("imported:")
    for l in log:
        print("  ", l)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.expanduser("~/Downloads"))
