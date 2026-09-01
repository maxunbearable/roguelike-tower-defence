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
}
# Per-map enemy variants inherit their base creature's bounty.
for _theme in ("ash", "frost", "blight", "obsidian"):
    for _c in ("slime", "wolf", "goblin", "wraith"):
        BASE_BOUNTY[f"{_theme}_{_c}"] = BASE_BOUNTY[_c]

PROFILES = {
    # What the game shipped with. Fresh reaches wave 18, fully upgraded 34-37.
    "original": dict(damage=1.0, fireRate=1.0, range=1.0, armorPen=1.0,
                     startGold=1.0, bounty=1.0),

    # Gold is scarce, towers are meaningfully weaker, placement matters more --
    # but a boss is still reachable and the meta loop still turns over.
    "lean": dict(damage=0.72, fireRate=0.72, range=0.82, armorPen=0.8,
                 startGold=0.68, bounty=0.75),

    # Gold deficit only: stats near intent, range slightly in, income tight. The
    # gold cut alone is a large nerf because bounty income COMPOUNDS -- fewer
    # towers means fewer kills means less gold. Measured: dropping start gold to
    # 68% and bounty to 75% took the autoplayer from 23 towers to 6.
    "tight": dict(damage=1.0, fireRate=1.0, range=0.9, armorPen=1.0,
                  startGold=0.85, bounty=0.9),

    # Harsher again. For players who find "lean" too soft.
    "brutal": dict(damage=0.6, fireRate=0.6, range=0.72, armorPen=0.7,
                   startGold=0.6, bounty=0.68),

    # Literally "cut all stats and range in half". MEASURED UNPLAYABLE: a profile
    # owning all 126 skill nodes dies on wave 5 of 50, so no boss (wave 25) and no
    # second map is ever reachable. Kept so the claim is checkable, not to ship.
    "half": dict(damage=0.5, fireRate=0.5, range=0.5, armorPen=0.5,
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
            out += "[[enemy]]" + b
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
