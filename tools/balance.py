#!/usr/bin/env python3
"""Applies a named balance profile to the content.

    python3 tools/balance.py list
    python3 tools/balance.py apply <profile>

Why this exists: balance was being tuned by hand-editing TOML, which is not
reversible and not auditable -- once damage has been halved twice you cannot tell
what the design intent was. The BASE table below is the authored intent; a
profile is a set of multipliers against it, so applying a profile is idempotent
and switching between them is one command.

Range is the stat to be careful with. A tower's coverage scales with the SQUARE
of its range, and every enemy spends proportionally less time inside it, so
halving range is closer to quartering a tower's contribution than halving it.
Measured: stats x0.5 plus range x0.5 took a fully-upgraded profile from wave 35
to wave 5.
"""
import re
import sys
import glob
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# The authored intent, before any profile is applied.
BASE_TOWERS = {
    "arrow":    {"damage": 12.0, "fireRate": 1.10, "range": 3.5, "armorPen": 0.0},
    "cannon":   {"damage": 34.0, "fireRate": 0.55, "range": 3.2, "armorPen": 2.0},
    "arcane":   {"damage": 18.0, "fireRate": 0.90, "range": 3.8, "armorPen": 6.0},
    "ballista": {"damage": 26.0, "fireRate": 0.65, "range": 4.2, "armorPen": 5.0},
    "brazier":  {"damage": 7.0,  "fireRate": 3.20, "range": 2.1, "armorPen": 0.0},
}
# Authored build and per-level upgrade costs, kept separate from BASE_TOWERS
# because they are integers and because they need their own multiplier: cost is
# the only lever that creates a gold deficit WITHOUT risking a death spiral.
# Cutting bounty reduces income, which reduces towers, which reduces kills,
# which reduces income again -- measured as a collapse at bounty x0.45. Raising
# cost binds what gold BUYS while leaving the kill economy intact.
BASE_COSTS = {
    "arrow":    {"build": 60,  "upgrades": [75, 140]},
    "cannon":   {"build": 95,  "upgrades": [110, 195]},
    "arcane":   {"build": 110, "upgrades": [125, 215]},
    "ballista": {"build": 105, "upgrades": [120, 205]},
    "brazier":  {"build": 85,  "upgrades": [100, 180]},
}
BASE_START_GOLD = {
    "greenfields": 500, "ashen-wastes": 520, "frostmere": 540,
    "blightmarsh": 560, "obsidian-gate": 580,
}
BASE_BOUNTY = {
    "slime": 9, "wolf": 12, "goblin": 20, "wraith": 16,
    "boss_ogre_warlord": 120, "boss_warlord_grulk": 300,
    "boss_cinder_brute": 140, "boss_cinder_colossus": 330,
    "boss_rime_stalker": 140, "boss_rime_tyrant": 330,
    "boss_plague_carrier": 145, "boss_plague_mother": 340,
    "boss_gate_sentinel": 170, "boss_gate_warden": 420,
    "bloodgnat": 7,
    "hivewasp": 11,
    "cryptbat": 13,
    "emberimp": 17,
    "voidgazer": 24,
    "gnashmaw": 19,
    "hornfiend": 18,
    "palecrawler": 15,
    "tombgrub": 26,
    "gorgemaw": 25,
    "boss_grub_matron": 190,
    "boss_cinder_fiend": 198,
    "boss_rime_crawler": 194,
    "boss_bloat_gorger": 205,
    "boss_void_tyrant": 202,
}
# Per-map enemy variants inherit their base creature's bounty.
for _theme in ("ash", "frost", "blight", "obsidian"):
    for _c in ("slime", "wolf", "goblin", "wraith"):
        BASE_BOUNTY[f"{_theme}_{_c}"] = BASE_BOUNTY[_c]

# Authored shard payout per kill. Shards are the META currency, so this is the
# rate the whole progression turns at -- it is a separate lever from bounty
# (in-run gold) and must not be tuned with it.
BASE_SHARDS = {
    "ash_goblin": 1,
    "ash_slime": 0,
    "ash_wolf": 0,
    "ash_wraith": 1,
    "blight_goblin": 1,
    "blight_slime": 0,
    "blight_wolf": 0,
    "blight_wraith": 1,
    "boss_cinder_brute": 9,
    "boss_cinder_colossus": 22,
    "boss_gate_sentinel": 12,
    "boss_gate_warden": 30,
    "boss_ogre_warlord": 8,
    "boss_plague_carrier": 10,
    "boss_plague_mother": 23,
    "boss_rime_stalker": 9,
    "boss_rime_tyrant": 22,
    "boss_warlord_grulk": 20,
    "frost_goblin": 1,
    "frost_slime": 0,
    "frost_wolf": 0,
    "frost_wraith": 1,
    "goblin": 1,
    "obsidian_goblin": 1,
    "obsidian_slime": 0,
    "obsidian_wolf": 0,
    "obsidian_wraith": 1,
    "slime": 0,
    "wolf": 0,
    "wraith": 1,
    "bloodgnat": 0,
    "hivewasp": 0,
    "cryptbat": 1,
    "emberimp": 1,
    "voidgazer": 1,
    "gnashmaw": 1,
    "hornfiend": 1,
    "palecrawler": 1,
    "tombgrub": 1,
    "gorgemaw": 1,
    "boss_grub_matron": 8,
    "boss_cinder_fiend": 8,
    "boss_rime_crawler": 8,
    "boss_bloat_gorger": 8,
    "boss_void_tyrant": 8,
}

# Authored skill-node costs. A lever of their own because shard prices and
# in-run gold are different economies: gold is spent and lost every run,
# shards are permanent. Tightening one while loosening the other is the
# whole point of a roguelike meta loop.
BASE_NODE_COSTS = {
    "arcane.drain.core": 58,
    "arcane.drain.dmg": 46,
    "arcane.drain.more": 46,
    "arcane.hex.core": 58,
    "arcane.hex.deep": 46,
    "arcane.hex.long": 46,
    "arcane.tempest.core": 58,
    "arcane.tempest.hard": 46,
    "arcane.tempest.more": 46,
    "arcane.trunk.dmg1": 18,
    "arcane.trunk.range1": 22,
    "arcane.trunk.rate1": 22,
    "arcane.unlock": 70,
    "arrow.elf.core": 55,
    "arrow.elf.ramp": 42,
    "arrow.elf.rate": 42,
    "arrow.hunter.core": 55,
    "arrow.hunter.pierce": 42,
    "arrow.hunter.spread": 42,
    "arrow.sniper.core": 55,
    "arrow.sniper.crit": 42,
    "arrow.sniper.pen": 42,
    "arrow.trunk.dmg1": 17,
    "arrow.trunk.range1": 21,
    "arrow.trunk.rate1": 21,
    "ballista.harpoon.core": 58,
    "ballista.harpoon.dmg": 46,
    "ballista.harpoon.pierce": 46,
    "ballista.javelin.core": 58,
    "ballista.javelin.hard": 46,
    "ballista.javelin.long": 46,
    "ballista.scorpion.core": 58,
    "ballista.scorpion.crit": 46,
    "ballista.scorpion.fast": 46,
    "ballista.trunk.dmg1": 18,
    "ballista.trunk.pierce1": 24,
    "ballista.trunk.range1": 22,
    "ballista.unlock": 55,
    "brazier.cinder.core": 58,
    "brazier.cinder.fast": 46,
    "brazier.cinder.pen": 46,
    "brazier.forge.core": 58,
    "brazier.forge.strong": 46,
    "brazier.forge.wide": 46,
    "brazier.pyre.core": 58,
    "brazier.pyre.far": 46,
    "brazier.pyre.wide": 46,
    "brazier.trunk.dmg1": 18,
    "brazier.trunk.range1": 24,
    "brazier.trunk.rate1": 22,
    "brazier.unlock": 45,
    "cannon.bombard.core": 58,
    "cannon.bombard.fast": 46,
    "cannon.bombard.hard": 46,
    "cannon.mortar.core": 58,
    "cannon.mortar.far": 46,
    "cannon.mortar.wide": 46,
    "cannon.siege.core": 58,
    "cannon.siege.dmg": 46,
    "cannon.siege.pen": 46,
    "cannon.trunk.dmg1": 18,
    "cannon.trunk.range1": 22,
    "cannon.trunk.rate1": 22,
    "cannon.unlock": 35,
    "earth.poison.core": 55,
    "earth.poison.spread": 42,
    "earth.quake.core": 55,
    "earth.quake.tremor": 42,
    "earth.rock.core": 55,
    "earth.rock.petrify": 42,
    "earth.trunk.duration1": 21,
    "earth.trunk.potency1": 17,
    "fire.blast.core": 56,
    "fire.blast.hard": 44,
    "fire.blast.wide": 44,
    "fire.burn.core": 56,
    "fire.burn.deep": 44,
    "fire.burn.long": 44,
    "fire.melt.core": 56,
    "fire.melt.deep": 44,
    "fire.melt.fast": 44,
    "fire.trunk.duration": 22,
    "fire.trunk.potency": 18,
    "global.crit1": 120,
    "global.crit2": 176,
    "global.dmg1": 58,
    "global.dmg2": 98,
    "global.dmg3": 164,
    "global.dmg4": 268,
    "global.gold1": 20,
    "global.gold2": 40,
    "global.gold3": 92,
    "global.gold4": 158,
    "global.level2": 22,
    "global.level3": 46,
    "global.lives1": 34,
    "global.lives2": 72,
    "global.lives3": 128,
    "global.lives4": 210,
    "global.pen1": 112,
    "global.range1": 76,
    "global.range2": 126,
    "global.range3": 206,
    "global.rate1": 80,
    "global.rate2": 134,
    "global.rate3": 222,
    "light.beacon.core": 58,
    "light.beacon.hard": 46,
    "light.beacon.wide": 46,
    "light.judgement.core": 58,
    "light.judgement.hard": 46,
    "light.judgement.often": 46,
    "light.sear.core": 58,
    "light.sear.hard": 46,
    "light.sear.more": 46,
    "light.trunk.duration": 24,
    "light.trunk.potency": 20,
    "shadow.rift.core": 58,
    "shadow.rift.hard": 46,
    "shadow.rift.wide": 46,
    "shadow.siphon.core": 58,
    "shadow.siphon.gold": 46,
    "shadow.siphon.often": 46,
    "shadow.trunk.duration": 24,
    "shadow.trunk.potency": 20,
    "shadow.wither.core": 58,
    "shadow.wither.deep": 46,
    "shadow.wither.fast": 46,
    "water.chill.cap": 44,
    "water.chill.core": 56,
    "water.chill.deep": 44,
    "water.freeze.core": 56,
    "water.freeze.long": 44,
    "water.freeze.often": 44,
    "water.shatter.core": 56,
    "water.shatter.easy": 44,
    "water.shatter.hard": 44,
    "water.trunk.duration": 22,
    "water.trunk.potency": 18,
    "wind.cyclone.core": 56,
    "wind.cyclone.often": 44,
    "wind.cyclone.wide": 44,
    "wind.gust.core": 56,
    "wind.gust.far": 44,
    "wind.gust.often": 44,
    "wind.shock.core": 56,
    "wind.shock.hard": 44,
    "wind.shock.more": 44,
    "wind.trunk.duration": 22,
    "wind.trunk.potency": 18,
}

PROFILES = {
    # What the game shipped with. Fresh reaches wave 18, fully upgraded 34-37.
    "original": dict(nodeCost=1.0, shard=1.0, cost=1.0, damage=1.0, fireRate=1.0, range=1.0, armorPen=1.0,
                     startGold=1.0, bounty=1.0),

    # Gold is scarce, towers are meaningfully weaker, placement matters more --
    # but a boss is still reachable and the meta loop still turns over.
    "lean": dict(nodeCost=1.0, shard=1.0, cost=1.0, damage=0.72, fireRate=0.72, range=0.82, armorPen=0.8,
                 startGold=0.68, bounty=0.75),

    # A severe gold deficit. Measured evidence it was needed: at "scarce" the
    # autoplayer hit its 34-tower ceiling on EVERY profile including a fresh one,
    # which means gold was never the binding constraint -- the cap was.
    "famine": dict(nodeCost=1.0, shard=1.0, cost=1.0, damage=1.0, fireRate=1.0, range=0.9, armorPen=1.0,
                   startGold=0.40, bounty=0.45),

    # A hard gold deficit. Paths are now ~2x longer, which roughly doubles how
    # much damage a board deals, so the economy can carry a much tighter squeeze
    # than "tight" without the death spiral that 0.68/0.75 produced on the old
    # short routes.
    "scarce": dict(nodeCost=1.0, shard=1.0, cost=1.0, damage=1.0, fireRate=1.0, range=0.9, armorPen=1.0,
                   startGold=0.62, bounty=0.7),

    # Gold deficit only: stats near intent, range slightly in, income tight. The
    # gold cut alone is a large nerf because bounty income COMPOUNDS -- fewer
    # towers means fewer kills means less gold. Measured: dropping start gold to
    # 68% and bounty to 75% took the autoplayer from 23 towers to 6.
    "tight": dict(nodeCost=1.0, shard=1.0, cost=1.0, damage=1.0, fireRate=1.0, range=0.9, armorPen=1.0,
                  startGold=0.85, bounty=0.9),

    # The shipping profile. Three separate economies, tuned in opposite
    # directions on purpose:
    #
    #   in-run GOLD      tight, and tightened at the build-cost end. Measured:
    #                    at cost x1.85 a fresh profile built 15-17 towers, which
    #                    was still comfortable, so cost goes to x2.4 and bounty
    #                    down again. Cutting income alone risks a death spiral --
    #                    fewer towers, fewer kills, less gold -- while cost binds
    #                    what gold BUYS and leaves the kill economy intact.
    #   meta SHARDS      cheaper (nodeCost x0.62). Gold is spent and lost every
    #                    run; shards are permanent. A hard run should still visibly
    #                    move the tree, or the deficit reads as punishment rather
    #                    than pacing.
    #   base TOWER STATS weaker (damage x0.78). A level-1 tower should be a
    #                    foothold, not a solution, so that levels and the tree are
    #                    what make a board work.
    # MEASURED FLOOR, do not push past it without re-measuring. At cost x2.4 /
    # startGold x0.5 / bounty x0.55 a fresh profile died on wave 5 having built
    # THREE towers, and a fully upgraded one stopped clearing (49 of 50). Two
    # things compound there: 250 start gold against a 144 tower is one tower on
    # wave 1, and the damage cut lowers kills, which lowers bounty, which lowers
    # towers. Starting gold must buy at least two towers or the spiral is
    # immediate.
    "deficit": dict(nodeCost=0.62, shard=0.4, cost=1.75, damage=0.78, fireRate=1.0,
                    range=0.9, armorPen=1.0, startGold=0.6, bounty=0.68),

    # Harsher again. For players who find "lean" too soft.
    "brutal": dict(nodeCost=1.0, shard=1.0, cost=1.0, damage=0.6, fireRate=0.6, range=0.72, armorPen=0.7,
                   startGold=0.6, bounty=0.68),

    # Literally "cut all stats and range in half". MEASURED UNPLAYABLE: a profile
    # owning all 126 skill nodes dies on wave 5 of 50, so no boss (wave 25) and no
    # second map is ever reachable. Kept so the claim is checkable, not to ship.
    "half": dict(nodeCost=1.0, shard=1.0, cost=1.0, damage=0.5, fireRate=0.5, range=0.5, armorPen=0.5,
                 startGold=0.68, bounty=0.75),
}


def sub_stat(text, key, value):
    fmt = f"{value:g}"
    return re.sub(rf"^({key} = )([0-9.]+)", lambda m: m.group(1) + fmt, text, flags=re.M)


def apply(name):
    p = PROFILES[name]
    for f in sorted(glob.glob(os.path.join(ROOT, "content/towers/*.toml"))):
        tid = os.path.basename(f)[:-5]
        if tid not in BASE_TOWERS:
            continue
        s = open(f).read()
        for key, base in BASE_TOWERS[tid].items():
            s = sub_stat(s, key, base * p[key])
        # Costs: buildCost is a single key; the per-level "cost" keys are
        # positional inside their [[level]] blocks, so rewrite the nth match.
        c = BASE_COSTS[tid]
        s = sub_stat(s, "buildCost", max(1, round(c["build"] * p["cost"])))
        ups = iter([max(1, round(u * p["cost"])) for u in c["upgrades"]])
        s = re.sub(r"^(cost = )(\d+)",
                   lambda m: m.group(1) + str(next(ups, int(m.group(2)))),
                   s, flags=re.M)
        open(f, "w").write(s)

    for f in sorted(glob.glob(os.path.join(ROOT, "content/maps/*.toml"))):
        mid = os.path.basename(f)[:-5]
        if mid not in BASE_START_GOLD:
            continue
        s = open(f).read()
        s = re.sub(r"^(startGold = )(\d+)",
                   lambda m: m.group(1) + str(int(BASE_START_GOLD[mid] * p["startGold"])),
                   s, flags=re.M)
        open(f, "w").write(s)

    # Bounty is per-enemy, so walk each [[enemy]] block and rewrite by id.
    for f in sorted(glob.glob(os.path.join(ROOT, "content/enemies/*.toml"))):
        s = open(f).read()
        out, blocks = s.split("[[enemy]]")[0], s.split("[[enemy]]")[1:]
        for b in blocks:
            m = re.search(r'^id = "([^"]+)"', b, re.M)
            if m and m.group(1) in BASE_BOUNTY:
                want = max(1, int(BASE_BOUNTY[m.group(1)] * p["bounty"]))
                b = re.sub(r"^(bounty = )(\d+)", lambda x: x.group(1) + str(want), b, flags=re.M)
            if m and m.group(1) in BASE_SHARDS:
                # Zero stays zero: a trash enemy paying no shards is authored
                # intent, and max(1, ...) would silently give every one of them a
                # shard and swamp the boss payouts.
                sv = BASE_SHARDS[m.group(1)]
                sw = 0 if sv == 0 else max(1, round(sv * p["shard"]))
                b = re.sub(r"^(shardValue = )(\d+)", lambda x: x.group(1) + str(sw), b, flags=re.M)
            out += "[[enemy]]" + b
        open(f, "w").write(out)

    # Skill-node costs.
    for f in sorted(glob.glob(os.path.join(ROOT, "content/trees/*.toml"))):
        s = open(f).read()
        out, blocks = s.split("[[node]]")[0], s.split("[[node]]")[1:]
        for b in blocks:
            m = re.search(r'^id = "([^"]+)"', b, re.M)
            if m and m.group(1) in BASE_NODE_COSTS:
                want = max(1, round(BASE_NODE_COSTS[m.group(1)] * p["nodeCost"]))
                b = re.sub(r"^(cost = )(\d+)", lambda x: x.group(1) + str(want), b, flags=re.M)
            out += "[[node]]" + b
        open(f, "w").write(out)

    print(f"applied '{name}': " + "  ".join(f"{k} x{v:g}" for k, v in p.items()))
    for tid in sorted(BASE_TOWERS):
        vals = {k: BASE_TOWERS[tid][k] * p[k] for k in BASE_TOWERS[tid]}
        print(f"  {tid:9s} " + "  ".join(f"{k}={v:g}" for k, v in vals.items()))


def main():
    if len(sys.argv) < 2 or sys.argv[1] not in ("list", "apply"):
        print(__doc__)
        return 1
    if sys.argv[1] == "list":
        for n, p in PROFILES.items():
            print(f"  {n:9s} " + "  ".join(f"{k} x{v:g}" for k, v in p.items()))
        return 0
    if len(sys.argv) < 3 or sys.argv[2] not in PROFILES:
        print(f"unknown profile; choose from {', '.join(PROFILES)}")
        return 1
    apply(sys.argv[2])
    return 0


if __name__ == "__main__":
    sys.exit(main())
