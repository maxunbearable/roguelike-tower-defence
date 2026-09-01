# Plan 17 — The instrument was biased, not the content

**Goal:** Finish the job started last round, and undo the part of it that was
wrong.

**Why this one.** Last round ended with a flagged risk and a claim. The risk:
`simulateCombo` levels only the specialised tower, so forge had been retuned to a
+100% aura against **level-1** neighbours, which would be far too strong beside
maxed ones. The claim, written into the previous plan and the commit message,
was that forge was *"genuinely a trap, not merely mismeasured"*.

The risk was real. The claim was wrong. Both are now measured.

## What the measurements said

Levelling the supporting towers, so the scenario is a board rather than one maxed
tower standing among beginners:

| brazier build | 2 towers, lvl-1 support | 4 towers, all levelled |
|---|---|---|
| unspecialised | 5.88% | 18.97% |
| **forge as I retuned it** | 6.80% | **70.06%** |
| pyre | 9.48% | 26.23% |
| cinder | 13.44% | 30.81% |

My "fix" made forge more than twice as strong as its best sibling. I had turned a
build the harness could not see into one that trivialises the tower.

Then the decisive test — the **original, untouched** values against the fixed
scenario:

| brazier build | 4 towers, all levelled |
|---|---|
| unspecialised | 18.97% |
| **forge, original values** | **24.37%** |
| pyre | 26.23% |
| cinder | 30.81% |

Comfortably above unspecialised, below both siblings. **Forge was never broken.**
It was well designed from the start. Every symptom came from the instrument, and
last round's content change was unnecessary before it was harmful.

The content change is reverted in full. Only the harness fix remains.

## The bias was general, not one build

Re-baselining the whole matrix showed the distortion was never really about
forge:

| | old scenario | fixed scenario |
|---|---|---|
| spread across specs | **7.9x** | **3.6x** |
| weakest spec | 0.16x median | 0.41x median |
| arcane/hex | 0.63x | **1.17x** |
| arcane/tempest | 0.65x | 1.00x |
| arcane/drain | 0.61x | 0.98x |

The three arcane specs move as much as forge does. That is not a coincidence:
hex is an **amplify** trait that raises incoming damage on enemies, and drain
pays per kill — like forge, their value lands on **other towers**. With level-1
supporters there was nothing worth amplifying.

So the harness was systematically biased against every support-shaped effect in
the game, and the game's own spread looked nearly twice as bad as it is.

## What this says about the previous round

The guardrail added last round — *no specialisation is worse than not
specialising* — is still right and still worth having. What was wrong was
reaching for the content when the measurement disagreed with the design, instead
of asking whether the measurement was sound. The forge numbers were evidence
about the harness and I read them as evidence about the tree.

The order matters: **check the instrument before you retune the thing it is
pointed at.**

---

## Execution log

**The guardrail caught me.** Deleting the old comment block from the forge node
also removed the two lines above the ones I meant to keep -- the
`brazier.trait.towerBuff` flag and the aura radius -- so forge briefly had no
aura at all and measured 15.1% against 18.97% unspecialised. The full suite went
red on `no specialisation is worse than not specialising`: the test written last
round, catching a bug introduced this round, in exactly the case it was written
for.

That is twice in two rounds that text surgery on this one file has silently
dropped content. The fix this time was not another patch but
`git checkout 5025f1d -- content/trees/brazier.toml` to restore it byte-exactly,
then adding only a comment, then diffing the whole file against the original with
comments stripped to prove nothing else moved. Modifier count 26 before and
after.

Otherwise straightforward once the diagnosis was right. `simulateCombo` now levels all four
towers, the forge modifiers are byte-identical to their pre-plan-16 values, and
the guardrail from last round still passes with forge at 24.4% against 19.0%
unspecialised.

Worth noting the shape of the mistake, because it is a general one: I tuned a
value until a number looked right, in an environment I had not verified was
representative. The bisection was careful, the arithmetic was checked, and the
answer was wrong by roughly 4x because the premise was.

## Not done

`brazier/forge` is still the weakest spec at 0.41x of median, and the whole
brazier family sits low (pyre 0.73x, cinder 0.91x). That now looks like a genuine
tower-family balance question rather than a measurement artefact — but base tower
stats belong to `tools/balance.py` profiles rather than hand-edited TOML, and
moving brazier's base damage moves every brazier measurement in the project. Its
own round.
