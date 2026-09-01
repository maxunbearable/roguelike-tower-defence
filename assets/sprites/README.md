# Sprite overrides

Any PNG dropped here **replaces the generated sprite of the same name**, with no
code change. `tower_base.png` overrides `tower_base`, `crown_sniper.png`
overrides `crown_sniper`. Anything not overridden keeps its generated art, so a
pack can be adopted one sprite at a time.

Run `./build/td_app --sprites` to list every sprite id and its current size.

## What the game expects

| id | size | anchor |
|---|---|---|
| `tower_base` | 32 x 56 | bottom-centre, stands on the tile centre + 12px |
| `crown_plain` `crown_sniper` `crown_elf` `crown_hunter` | 32 x 24 | bottom-centre, sits on the parapet |
| `enchant_poison` `enchant_rock` `enchant_quake` | 32 x 20 | bottom-centre, climbs the shaft |
| `slime_0..5` `wolf_0..5` `goblin_0..5` `wraith_0..5` | 24 x 24 | centre |
| `arrow` | 12 x 12 | centre, points +x, rotated in flight |
| `prop_*` | 16 x 16 | centre |
| `icon_*` | 12 x 12 | centre |

Other sizes are fine — sprites draw from their anchor, so a taller tower simply
stands taller. Keep transparency. Keep animation frames contiguous from `_0`.

## Licensing — read before adding anything

This game is intended for sale, so **"free to download" is not a licence**.
Every file added here must be cleared for commercial use and recorded in
`docs/ASSET-POLICY.md` first.

**CraftPix freebies are cleared** (verified 2026-08-31): commercial use allowed,
no attribution required, explicitly permits selling on Steam and the App Store.

**But their licence forbids redistributing the source files** — assets may not
be "made available for others to extract and reuse independently". A public git
repository containing the PNGs would arguably breach that. So either keep this
repository private, or leave the pack files untracked. `.gitignore` excludes
`assets/sprites/*.png` for exactly this reason; remove that line only if the
repo is private.
