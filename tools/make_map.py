#!/usr/bin/env python3
"""Generates map TOML from waypoints.

    python3 tools/make_map.py

The first hand-authored map shipped with its tiles block disagreeing with its
path: a vertical run sat at x=12 while the corners were written at x=13. Nothing
caught it until the route was walked by eye. Every map since is GENERATED from
its waypoints, so the two cannot disagree -- the tiles are derived, not authored.

This also verifies, before writing anything:
  - every segment is axis-aligned (the sim lerps straight lines only)
  - every tile is inside the grid
  - the route does not cross itself (ambiguous for the player to read)
  - the first and last waypoints sit on a grid edge
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GRID_W, GRID_H = 22, 11  # fixed by the renderer's virtual resolution


def route_tiles(waypoints):
    """Every tile the route covers, in order, with no duplicates."""
    tiles = []
    for (x0, y0), (x1, y1) in zip(waypoints, waypoints[1:]):
        if x0 != x1 and y0 != y1:
            raise ValueError(f"segment ({x0},{y0})->({x1},{y1}) is not axis-aligned")
        dx = (x1 > x0) - (x1 < x0)
        dy = (y1 > y0) - (y1 < y0)
        steps = max(abs(x1 - x0), abs(y1 - y0))
        for i in range(steps + 1):
            t = (x0 + dx * i, y0 + dy * i)
            if not tiles or tiles[-1] != t:
                tiles.append(t)
    # Drop consecutive duplicates introduced at corners.
    out = []
    for t in tiles:
        if not out or out[-1] != t:
            out.append(t)
    return out


def verify(name, waypoints):
    tiles = route_tiles(waypoints)
    for (x, y) in tiles:
        if not (0 <= x < GRID_W and 0 <= y < GRID_H):
            raise ValueError(f"{name}: tile ({x},{y}) is outside the {GRID_W}x{GRID_H} grid")

    seen = set()
    for t in tiles:
        if t in seen:
            raise ValueError(f"{name}: route crosses itself at {t}")
        seen.add(t)

    for label, (x, y) in (("spawn", waypoints[0]), ("exit", waypoints[-1])):
        on_edge = x == 0 or y == 0 or x == GRID_W - 1 or y == GRID_H - 1
        if not on_edge:
            raise ValueError(f"{name}: {label} ({x},{y}) is not on a grid edge")
    return tiles


# Build plots, not open grass. Measured before this change: greenfields offered
# 144 buildable tiles and the strongest possible run used 34 of them, so a plot
# was never a scarce thing and the only question a tower asked was what it cost.
# That made the cheapest tower the right answer for every marginal build, and
# every tower unlocked with shards a worse buy than the free starting one.
#
# Plots hug the path, because a tower that cannot reach the road is not a
# choice, and they are kept apart so each one is a distinct position rather than
# a blob. `plotEvery` sets how many path tiles pass between them.
def place_plots(grid, tiles, every):
    """Plots in small clumps beside the road, sampled along the path.

    Clumped, not scattered: the brazier's forge spec trades 61% of its own
    output for an aura over towers within 2.6 tiles, so it breaks even only at
    1.57 buffed neighbours. Plots spaced further apart than that would leave the
    game's one support archetype structurally dead -- a whole tower spec deleted
    by a map-generation rule. Each clump holds up to three plots, all within the
    aura, so a support build has something to support.
    """
    path = set(tiles)
    plots = []
    for i, (x, y) in enumerate(tiles):
        if i % every:
            continue
        # Direction along the path here, and the two sides of the road.
        nx, ny = tiles[min(i + 1, len(tiles) - 1)]
        dx, dy = (nx - x, ny - y) if (nx, ny) != (x, y) else (1, 0)
        for perp in ((-dy, dx), (dy, -dx)):
            first = (x + perp[0], y + perp[1])
            # Beside a horizontal road the clump runs ALONG it, three abreast:
            # stacking plots outward would put the far ones at the edge of a
            # tower's reach, and a forge's aura would then land on towers that
            # can barely fire -- measured, that scored the support spec below
            # plain arrow.
            #
            # Beside a VERTICAL road it runs across instead. Along would mean two
            # plots in the same column, and a tower sprite is taller than its
            # tile, so the upper one is drawn through the lower one. Two wide
            # rather than three keeps both within reach of the road.
            if dy == 0:
                clump = [first,
                         (first[0] + dx, first[1] + dy),
                         (first[0] + 2 * dx, first[1] + 2 * dy)]
            else:
                # A zigzag, so no two plots share a column on consecutive rows.
                # Two in one column would draw through each other; spacing them
                # two rows apart keeps them clear and still inside the aura,
                # which a plain two-wide pair was not -- obsidian-gate fell to
                # 1.18 mean neighbours that way, under forge's 1.57 break-even.
                clump = [first,
                         (first[0] + perp[0], first[1] + dy),
                         (first[0], first[1] + 2 * dy)]
            placed = []
            for (px, py) in clump:
                if not (0 <= px < GRID_W and 0 <= py < GRID_H):
                    continue
                if (px, py) in path or grid[py][px] != ".":
                    continue
                # Keep clumps apart from each other so they read as distinct
                # positions on the board rather than one long wall of pads.
                if any(abs(px - qx) <= 1 and abs(py - qy) <= 1 for (qx, qy) in plots
                       if (qx, qy) not in placed):
                    continue
                grid[py][px] = "o"
                placed.append((px, py))
            plots.extend(placed)
            if placed:
                break
    return plots


def tile_rows(tiles, plot_every):
    grid = [["." for _ in range(GRID_W)] for _ in range(GRID_H)]
    for (x, y) in tiles:
        grid[y][x] = "="
    plots = place_plots(grid, tiles, plot_every)
    sx, sy = tiles[0]
    ex, ey = tiles[-1]
    grid[sy][sx] = "S"
    grid[ey][ex] = "E"
    return ["".join(row) for row in grid], plots


# Difficulty is normalised against PATH LENGTH, because total damage a board can
# deal is proportional to how long enemies spend under fire. The reference is the
# original greenfields: 48 path tiles, a wave-50 HP multiplier of 27.1, and 27.6
# enemies on wave 50, which measured as a fair map. A route twice as long earns
# twice the health for the same felt difficulty.
REF_TILES, REF_W50, REF_COUNT = 48, 27.1, 27.6

# ...and against an EARLY anchor as well, which is the half that was missing.
#
# The curve is hpPerWave ^ (wave ^ exp), and only its wave-50 end was pinned. A
# single parameter then had to serve both ends, so a map's `target` -- the dial a
# designer turns to say "this one is harder" -- moved wave 50 by 18% and wave 10
# by 1.6%. Measured across all five maps the early multiplier sat between 1.85
# and 1.89 whatever the target said, which is to say the dial did nothing in the
# range where a player actually loses: a fresh profile dies around wave 9. The
# ordering of the campaign was left to incidental geometry, and came out 10.0,
# 8.5, 9.0, 9.5, 4.0 waves survived against an authored order of 1..5.
#
# Pinning both ends and solving for BOTH unknowns fixes that:
#
#     ln w50 = 49^exp . ln h        ln wE = (E-1)^exp . ln h
#     =>  exp = ln(ln w50 / ln wE) / ln(49 / (E-1))
#     =>  h   = exp(ln w50 / 49^exp)
#
# `exp` stops being an authored constant and becomes what the two anchors imply.
REF_EARLY_WAVE, REF_W_EARLY = 10, 1.86

# Exposure -- how many towers reach each tile of the route, summed along it --
# was tried as a normalising term here and REMOVED. It is a much better
# description of what a board delivers than route length (across the five maps
# route length spans 15%, exposure 54%), and it explains Obsidian Gate: a dozen
# towers reach each of its path tiles 2.07 times against greenfields' 2.66, so
# the same board deals 27% less damage there.
#
# But it did not predict the others. Compensating for it moved obsidian the
# right way and ashen the wrong way, and no exponent flattened the set. Modelling
# early difficulty from geometry is evidently harder than it looks, so the dial
# is calibrated against measured runs instead and the geometry stays an
# observation rather than a formula. REACH and the exposure helper are kept
# because the report uses them.
REACH = 4.0
OPENING_TOWERS = 12


def exposure(path_tiles, plots, n=None):
    """Sum over route tiles of how many towers reach each -- the board's reach."""
    import math
    d = lambda a, b: math.dist(a, b)
    sel = plots
    if n is not None:
        sel = sorted(plots, key=lambda q: -sum(1 for p in path_tiles if d(p, q) <= REACH))[:n]
    return sum(sum(1 for q in sel if d(p, q) <= REACH) for p in path_tiles)


def hp_curve(target, count50, tiles, early_target):
    """Solves hpPerWave and the curve exponent from the early and late anchors.

    Returns (hpPerWave, exp, w50, wEarly).
    """
    import math
    w50 = target * REF_W50 * (REF_COUNT / count50) * (tiles / REF_TILES)
    # The two anchors take path length in OPPOSITE directions, which is the
    # part the single-anchor model could not express.
    #
    # A finished board covers the whole route, so a longer route holds more
    # towers firing for longer and earns more health -- that is the w50 term
    # above. An OPENING board does not: what a fresh purse buys is about a dozen
    # towers whatever the route's length, so they cover a fixed absolute stretch
    # of it and a longer route simply means a larger uncovered remainder.
    # Scaling early health up with length therefore punished long maps twice.
    #
    # Measured: ranking the maps by path length reproduced the measured
    # difficulty order exactly -- ashen 106, frostmere 100, blightmarsh 99,
    # greenfields 98 against 8.0, 9.0, 9.0, 10.0 waves survived -- while the
    # authored targets said the order should be the reverse. Eight tiles of route
    # outweighed a 6% step of the dial.
    w_early = REF_W_EARLY * early_target
    if w_early <= 1.0:
        raise ValueError("early anchor must exceed 1.0 to be a growth curve")
    exp = math.log(math.log(w50) / math.log(w_early)) / math.log(49.0 / (REF_EARLY_WAVE - 1))
    h = math.exp(math.log(w50) / (49 ** exp))
    return h, exp, w50, w_early


def emit(m):
    tiles = verify(m["id"], m["path"])
    # Recomputed at generation time, so changing a route cannot silently change
    # how hard the map is.
    count50 = m["countBase"] + m["countPerWave"] * 49
    rows, plots = tile_rows(tiles, m.get("plotEvery", 8))
    early_exp = exposure(tiles, plots, OPENING_TOWERS)
    hp, exp, w50, w_early = hp_curve(m["target"], count50, len(tiles), m["earlyTarget"])
    m = dict(m, hpPerWave=round(hp, 4), hpCurveExp=round(exp, 4))
    m["_w50"] = round(w50, 1)
    m["_wEarly"] = round(w_early, 2)
    m["_plots"] = len(plots)
    m["_exposure"] = early_exp
    wp = ",".join(f"[{x},{y}]" for x, y in m["path"])
    pool = "\n\n".join(
        f'[[waves.pool]]\nenemy = "{e}"\nfromWave = {w}' for e, w in m["pool"])
    bosses = "\n\n".join(
        f'[[waves.boss]]\nwave = {w}\nenemy = "{e}"' for w, e in m["bosses"])

    return f'''# GENERATED by tools/make_map.py -- edit the spec there, not this file.
# The tiles block is derived from `path`, so the two cannot disagree.

[map]
id = "{m['id']}"
name = "{m['name']}"
order = {m['order']}
blurb = "{m['blurb']}"
gridW = {GRID_W}
gridH = {GRID_H}
startGold = {m['startGold']}
buildTime = {m['buildTime']}

# {m['blurb']}

# 'o' build plot   '.' open ground   '#' blocked scenery   '=' path   'S' spawn   'E' exit
tiles = """
{chr(10).join(rows)}
"""

path = [{wp}]

[waves]
count = 50
countBase = {m['countBase']}
countPerWave = {m['countPerWave']}
intervalBase = {m['intervalBase']}
intervalDecay = 0.985
intervalMin = 0.30
hpPerWave = {m['hpPerWave']}
hpCurveExp = {m['hpCurveExp']}
armorPerWave = {m['armorPerWave']}
bountyPerWave = 1.02
secondaryFromWave = 4
secondaryFraction = 0.6
secondaryDelay = 2.5
delay = 5.0
bossDelay = 9.0

{bosses}

{pool}
'''


# Difficulty rises across the five maps: later maps start with less relative
# gold, ramp harder and armour up faster. Each is 22x11 because the renderer's
# virtual resolution fixes the board size.
MAPS = [
    {
        "id": "greenfields", "order": 1, "name": "Greenfields",
        "blurb": "Open meadow. Where every build starts.",
        "path": [(0, 1), (19, 1), (19, 3), (2, 3), (2, 5), (19, 5), (19, 7), (2, 7), (2, 9), (21, 9)],
        "startGold": 275, "buildTime": 12.0,
        "countBase": 8, "countPerWave": 0.4, "intervalBase": 0.9,
        "target": 1.0, "earlyTarget": 1.0, "armorPerWave": 0.11,
        "bosses": [(25, "boss_ogre_warlord"), (40, "boss_grub_matron"),
                   (50, "boss_warlord_grulk")],
        "pool": [("slime", 1), ("wolf", 5), ("goblin", 8), ("wraith", 12),
                 ("cryptbat", 16), ("hivewasp", 20)],
    },
    {
        "id": "ashen-wastes", "order": 2, "name": "Ashen Wastes",
        "blurb": "Three long straights: good for reach, punishing for short range.",
        "path": [(0, 1), (20, 1), (20, 3), (1, 3), (1, 5), (20, 5), (20, 7), (1, 7), (1, 9), (21, 9)],
        "startGold": 286, "buildTime": 12.0,
        "countBase": 9, "countPerWave": 0.42, "intervalBase": 0.88,
        "target": 1.06, "earlyTarget": 0.97, "armorPerWave": 0.12,
        "bosses": [(25, "boss_cinder_brute"), (40, "boss_cinder_fiend"),
                   (50, "boss_cinder_colossus")],
        "pool": [("ash_slime", 1), ("ash_wolf", 4), ("ash_goblin", 7), ("ash_wraith", 11),
                 ("emberimp", 14), ("hornfiend", 18)],
    },
    {
        "id": "frostmere", "order": 3, "name": "Frostmere",
        "blurb": "A long winding route. Every tower gets several passes if placed well.",
        "path": [(0, 0), (21, 0), (21, 2), (2, 2), (2, 4), (19, 4), (19, 6), (4, 6), (4, 8), (17, 8), (17, 10), (21, 10)],
        "startGold": 297, "buildTime": 12.0,
        "countBase": 9, "countPerWave": 0.42, "intervalBase": 0.86,
        "target": 1.12, "earlyTarget": 1.23, "armorPerWave": 0.12,
        "bosses": [(25, "boss_rime_stalker"), (40, "boss_rime_crawler"),
                   (50, "boss_rime_tyrant")],
        "pool": [("frost_slime", 1), ("frost_wolf", 4), ("frost_goblin", 7),
                 ("frost_wraith", 10), ("palecrawler", 13), ("bloodgnat", 17)],
    },
    {
        "id": "blightmarsh", "order": 4, "name": "Blightmarsh",
        "blurb": "Wide open ground with few corners: coverage matters more than depth.",
        "path": [(0, 9), (18, 9), (18, 6), (3, 6), (3, 3), (20, 3), (20, 1), (1, 1), (1, 0), (21, 0)],
        "startGold": 308, "buildTime": 11.0,
        "countBase": 9, "countPerWave": 0.42, "intervalBase": 0.84,
        "target": 1.18, "earlyTarget": 1.56, "armorPerWave": 0.12,
        "bosses": [(25, "boss_plague_carrier"), (40, "boss_bloat_gorger"),
                   (50, "boss_plague_mother")],
        "pool": [("blight_slime", 1), ("blight_wolf", 4), ("blight_goblin", 6),
                 ("blight_wraith", 9), ("gorgemaw", 12), ("gnashmaw", 16)],
    },
    {
        "id": "obsidian-gate", "order": 5, "name": "Obsidian Gate",
        "blurb": "The last map. Heavily armoured, resistant to everything, and long.",
        "path": [(0, 10), (2, 10), (2, 0), (5, 0), (5, 10), (8, 10), (8, 0), (11, 0), (11, 10), (14, 10), (14, 0), (17, 0), (17, 10), (20, 10), (20, 0), (21, 0)],
        "startGold": 319, "buildTime": 11.0,
        "countBase": 10, "countPerWave": 0.44, "intervalBase": 0.82,
        "target": 1.24, "earlyTarget": 0.65, "armorPerWave": 0.1,
        "bosses": [(25, "boss_gate_sentinel"), (40, "boss_void_tyrant"),
                   (50, "boss_gate_warden")],
        "pool": [("obsidian_slime", 1), ("obsidian_wolf", 3), ("obsidian_goblin", 6),
                 ("obsidian_wraith", 9), ("voidgazer", 12), ("tombgrub", 15),
                 ("bloodgnat", 19)],
    },
]


def main():
    out_dir = os.path.join(ROOT, "content", "maps")
    for m in MAPS:
        text = emit(m)
        path = os.path.join(out_dir, m["id"] + ".toml")
        with open(path, "w") as f:
            f.write(text)
        tiles = route_tiles(m["path"])
        count50 = m["countBase"] + m["countPerWave"] * 49
        _, plots = tile_rows(tiles, m.get("plotEvery", 8))
        early_exp = exposure(tiles, plots, OPENING_TOWERS)
        hp, exp, w50, w_early = hp_curve(m["target"], count50, len(tiles), m["earlyTarget"])
        print(f"  {m['id']:16s} {len(tiles):3d} tiles  {len(plots):3d} plots  "
              f"exposure {early_exp:5.0f}  w10 hp {w_early:4.2f}  w50 hp {w50:5.1f}  "
              f"exp {exp:.3f}")


if __name__ == "__main__":
    try:
        main()
    except ValueError as e:
        print(f"map spec rejected: {e}", file=sys.stderr)
        sys.exit(1)
