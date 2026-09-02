#!/usr/bin/env bash
# Regenerates the screenshots the README shows, reproducibly.
#
# Previous rounds captured these by hand, which went wrong twice: the dev
# capture path built its world from an empty Loadout, and that silently meant
# "owns the entire skill tree" (see plan 30), so every gameplay screenshot was of
# a fully upgraded board while claiming to show the game. And a run started
# before a profile was opened read a default profile, so a capture of the
# tutorial showed step 0 whatever the save said (plan 31).
#
# This script makes the state explicit instead:
#   * a temp save directory, so it never depends on or clobbers a real profile
#   * TD_RUN_SEED pinned, so the same command gives the same frame
#   * the showcase profile owns the whole tree ON PURPOSE, which is a state a
#     player reaches, and is written out here where it can be read
#
# Needs a built td_shot (raylib's software backend, so no display required):
#   cmake --build build --target td_shot && tools/shots.sh
set -euo pipefail
cd "$(dirname "$0")/.."

SHOT=build/td_shot
OUT=docs/screenshots
[ -x "$SHOT" ] || { echo "build $SHOT first: cmake --build build --target td_shot"; exit 1; }
mkdir -p "$OUT"

SAVE=$(mktemp -d)
trap 'rm -rf "$SAVE"' EXIT
export TD_SAVE_DIR="$SAVE"
export TD_RUN_SEED=20260902

# A profile that has finished the game: every node bought, every map cleared.
python3 - "$SAVE" <<'PY'
import json, re, sys, pathlib
nodes = []
for f in sorted(pathlib.Path('content/trees').glob('*.toml')):
    nodes += re.findall(r'^id\s*=\s*"([^"]+)"', f.read_text(), re.M)
maps = [re.search(r'^id\s*=\s*"([^"]+)"', f.read_text(), re.M).group(1)
        for f in sorted(pathlib.Path('content/maps').glob('*.toml'))]
json.dump({
    "version": 1, "used": True, "profileName": "Showcase", "run": None,
    "meta": {
        "shards": 900, "runsPlayed": 40, "bestWave": 50,
        "ownedNodes": sorted(set(nodes)),
        "mapProgress": {m: {"bestWave": 50, "cleared": True} for m in maps},
        "musicVolume": 0.5, "sfxVolume": 0.85, "colorAlternatives": False,
        "shake": 1.0, "integerScaling": True, "difficulty": 1,
        "tutorialStep": 5, "seenHints": [],
    },
}, open(pathlib.Path(sys.argv[1]) / "slot0.json", "w"), indent=1)
print(f"showcase profile: {len(set(nodes))} nodes, {len(maps)} maps cleared")
PY

shot() { # shot <file> <args...>
  local f="$1"; shift
  "$SHOT" --shot "$OUT/$f" --openslot 0 "$@" >/dev/null 2>&1
  printf '  %-22s %s\n' "$f" "$(du -h "$OUT/$f" | cut -f1)"
}

echo "rendering into $OUT"
shot 01-gameplay.png    --autostart --cluster 18 --wave 16 --after 9
shot 02-tower-stats.png --autostart --cluster 12 --menu 8 2 --after 1.5
shot 03-skill-tree.png  --hub --tab 0 --after 0.5
shot 04-map-select.png  --maps --after 0.5
# --wave is a zero-based index, so 24 is wave 25 -- greenfields' first boss.
# after=12 lands while the Ogre Warlord is still on the field: a maxed board
# kills it within a couple more seconds, and at 16 the wave is already over.
shot 05-boss.png        --autostart --cluster 22 --wave 24 --after 12
shot 06-pause.png       --autostart --cluster 14 --settings --after 2.5
echo "done"
