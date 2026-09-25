# Panel design data

The panel's look is designed in Figma and reaches the code through the two
files in this directory. Nothing in the build reads Figma; everything
visual comes from here.

## Where the design lives

The Figma file is
[`75EleO8U3zngY8JU9adWpk`](https://www.figma.com/design/75EleO8U3zngY8JU9adWpk).
The chosen direction is **Graphite Console**, with three palettes kept as
modes of one variable collection: **Ledger** (the default), **Graphite** and
**Sigil**.

- `figma-manifest.json` records where things are in that file: the file key,
  the chosen direction, and the page, frame, component, variable-collection
  and variable ids of the screens, states, components and token sheets. It
  holds ids only, so a later export can find the same nodes.
- `figma-export.json` is what the design says: the palettes and their
  variables, the borders, the contrast pairs, the screens and states, the
  components, the fonts, the motion values and the amendments (the
  enabled-mods list's forms, the undo toast, the idle switch, the plugin
  warning and the owner's polish pass).

## The export is the contract

`figma-export.json` is the source of truth for the panel's design, and the
code is checked against it rather than the other way round:

- `src/tokens.css` is generated from it, one custom property per variable
  (see below). It and `src/icons.js` are the only files in `src/` allowed to
  hold raw colour literals; every other style reads a token.
- `tests/tokens.test.js` fails if `tokens.css` and the export disagree on any
  variable of the default palette, or any colour variable of the other
  palettes, in either direction.
- `scripts/contrast-check.mjs` checks every exported contrast pair in every
  palette against WCAG AA:

  ```bash
  node scripts/contrast-check.mjs design/figma-export.json
  ```

- `tests/design-match.mjs` checks the built panel against the export: each
  screen's texts, each `selectorTokens` entry's computed value in every
  palette, and no border outside the export's `borders` allowlist. It also
  writes each Figma screen beside a fresh render for a visual comparison.
- `src/theme.js`'s theme picker offers the export's palettes, in order.

To take a new design, copy the new export over `figma-export.json` (and the
ids over `figma-manifest.json` if nodes moved), regenerate `tokens.css`, and
run the checks above. A contrast failure is a colour problem to fix in
Figma, not something to patch around in CSS.

## Amending the export

When the owner decides a change in words, before anyone draws it, the export
can be amended by hand and Figma brought in line afterwards, so the contract
still leads the code. The owner's polish pass (2026-09-25) went this way:

- The decision is recorded under `amendments` with the owner's words and the
  date. `amendments.polish` holds one entry per item, each naming what the
  panel now does and the selectors it does it with, including item 9, an idle
  slider's note shown as a tooltip above its row. A slider at its default has
  no note, so an idle row at its default opens no tooltip.
- The owner's later answers join the same entry with their own date. On
  2026-09-26 (`decidedRound2`, `ownerWords20260926`): F1, no note for a slider
  at its default (item 9 and the `idle-switch` state say so); F6, "CT only"
  stays `color/text/muted` in both, pinned as a `#enemyspeedctval` row in
  `selectorTokens` and named in item 5, so Figma redraws it muted.
- Everything the amendment changes is edited where the checks read it: each
  screen's `texts`, the `selectorTokens` rows (a row whose element is gone is
  dropped or re-pointed; a new look gets a row design-match can reach), the
  `borders` allowlist with a `why` for each new border, the `contrastPairs`,
  and the `states` notes. `variables` and `palettes` never change in an
  amendment, so `tokens.css` does not either.
- A version number is a runtime value, like any field's value, and is never
  listed in a screen's `texts`.
- Then the Figma file is edited to match the amended export and read back,
  and the new state frames' node ids are written into `figma-manifest.json`
  and the export's `states`.

## Regenerating `tokens.css`

From the `ForgePact/` directory:

```bash
node panel/scripts/tokens-from-export.mjs panel/design/figma-export.json --out panel/src/tokens.css
```

The default palette becomes `:root`, with every one of its variables sorted
by name; each other palette becomes a `:root[data-theme="<name>"]` block
holding its colour variables only, so switching the theme swaps colours and
nothing else. A variable's name becomes the property's name with `/`
replaced by `-` (`color/bg/base` is `--color-bg-base`).

Commit the generator's output as it is. `tokens.css` is never edited by
hand: a change to a token is a change in Figma, exported and regenerated.
