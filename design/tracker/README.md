# Tracker tab and overlay: direction comps (#185)

These comps set the look of ForgePact's Tracker tab and its overlay window
before anything is built on them. They are static HTML over the panel's own
stylesheets, linked by relative path (`../../panel/src/`), plus
`tracker.css`, the proposed styles the building items port into
`panel/src/tracker/` and `panel/src/overlay/`. Nothing is downloaded; open the
files straight from disk. Every sample value in them is invented.

- `tab.html`: the Tracker tab. `?theme=ember` (the default) or `?theme=ledger`
  picks the palette; resize the window to 1100 to see the one-column form;
  `?board` shows Ember and Ledger at 1280 and at 1100 wide, one frame each.
- `overlay.html`: the overlay window, every frame at its default 444 px width.
  `?theme=` and `?board` work the same way.
- `tracker.css`: tokens only (`var(--color-*)`, `--space-*`, `--radius-*`,
  `--font-*`, `--motion-*`), so a palette switch repaints it and changes
  nothing else.

The rules these comps settled are in `panel/DESIGN.md` § Tracker. The five
rarity colours they use are new design data in `panel/design/figma-export.json`
(`color/rarity/<name>` in every palette), and `panel/src/tokens.css` is
regenerated from it.

## Screens

The tab (`tab.html`), top to bottom, in Ember and in Ledger, at 1280 and at
1100 wide:

1. **The switch card**, heading the tab: "Session tracker" with the
   `mod_tracker` switch (`#mod_tracker`, the one control the oracle's coverage
   walk counts), one sentence of what it does, and one state line.
2. **Sub-tabs**, as the Mods tab's: Session, Runs, Alerts, Overlay, Shopping
   list, Item codex. Each sub-tab is one component of the component table.
3. **Session** (`Session.svelte`): the session header (character name, level,
   hero level, difficulty, the session clock, Pause clock and End run), the
   four totals with their per-hour rates (gold, experience, kills, drops), the
   loot table by rarity (drops, Magic Find, per hour), the notable counts
   (SS-grade gear from `ss`, then the snapshot's `notable`), the tallies
   (bosses and chests), the Satanic zone chip (zone, "You are here", buffs and
   debuffs by name), the series as two small charts, and the journal. Then
   the other states of the same cards: the tracker off, catching up with the
   clock paused, no session yet, and End run's confirmation.
4. **Runs** (`Runs.svelte`): the run list, the picked run (totals, loot,
   where it happened, finds), the run card that Copy card draws, Clear
   history's confirmation and the copy failure's wording.
5. **Alerts** (`AlertSettings.svelte`): the gesture note, one row per sound
   (the five rarities and the zone rotation: switch, volume, Test, the sound's
   source with Choose a file and Use the bundled sound), a refused file's
   message, the lowest grade, the zone-rotation buff pick, and the sound
   lists (filter, lists, the picked list's sound and items).
6. **Overlay** (`OverlaySettings.svelte`): Open overlay, open with the game,
   click-through, newest drops, counters only, opacity, size, the
   transparency note; the drop card (flourish) settings; Discord status.
7. **Shopping list** (`Shop.svelte`): entry field, the list with one entry
   just copied, and the empty list.
8. **Item codex** (`Codex.svelte`): search, rarity, kind and order filters,
   the table, and the "showing 60 of N" foot.

The overlay (`overlay.html`), each frame one window at scale 1:

1. Solid, 444 × 147: state, clock, zone; gold, experience and kills with
   rates; the five rarity counts and SS.
2. Transparent over a white frame (the worst case for contrast), plate at
   90%, its floor.
3. Newest drops on: 444 × 211, the three newest journal entries under the
   counters.
4. The drop card for an Angelic drop, twice in a row (×2).
5. The drop card for a Satanic zone rotation, transparent.
6. Counters only (444 × 104), with the clock paused; and the window before any
   session.

## Decisions

- **The tab is sub-tabbed, like Mods.** One page holding every section ran
  past 5,000 px at 1280. The Session sub-tab is what a player watches; the
  rest are visited. The switch card sits above the sub-tabs, so the switch
  heads the tab whichever sub-tab is open.
- **The tab's container draws no surface; each section is its own card** (the
  No Nested Card Rule, the Mods tab's own answer). The tab is a two-column grid
  at 1280 and one column at 1100 and below, the panel's own breakpoint.
- **Numbers are Plex Mono in every palette, Ember included.** Totals are
  26px, the clock 20px, rates and counts 12px. Ember keeps its serif for
  headings and its own stepper values; the Tracker's figures stay mono so a
  column of them lines up.
- **Rarity colour goes on the name, never as a fill, and the rarity is
  written wherever it is the point**: the loot table, the journal, the codex,
  the alert rows, the drop card. Where the name alone is the point (a run's
  finds, a sound list, the overlay's ticker) the colour is a redundant hint.
  The five hues follow the game (Satanic red, Set green, Heroic cyan, Angelic
  gold, Unholy violet), tuned per palette for contrast; Sigil's Satanic is a
  purer red so it does not read as Sigil's rose accent.
- **The accent still means on.** No Tracker total, bar or chart line is drawn
  in the accent; the series lines are primary text on a subtle grid, and the
  time-in-zone bars are muted text.
- **The series is two small charts, one counter each**, never one chart with
  two axes: gold and experience differ by a factor of three or more. Each has
  its own caption with the latest value; no legend, no fill, no points.
- **The journal leads with the grade**, beside the time, then the rarity and
  the name; Magic Find and "alerted" sit at the row's end.
- **No setting icons on Tracker headings.** Ember's illustrated icons belong
  to settings. The Tracker rail button has no illustrated icon either: Ember
  shows its line drawing at the artwork's size, in the accent.
- **The overlay is flat in every palette.** It sits over the fight at 444 px,
  so it takes the palette's colours (Ember's from `palette.css`) and none of
  Ember's artwork, serif or glow.
- **Transparency is a plate, not the text.** With a transparent window only
  the plate's background is translucent, with a 90% floor: over a white frame
  every overlay text token still passes 4.5:1 there, which 85% does not
  (Graphite's Satanic falls to 4.34). The overlay uses no faint text. The
  window stays solid until the `overlay` item's probe shows transparency
  works on this machine (Q8's fallback).
- **The drop card takes the counters' place for its seconds.** It covers
  everything under the top line whole, so the state, clock and zone stay and
  no line of counters is cut through. It is the one thing in the overlay that
  moves: it enters with an 8px rise and a fade, and leaves with a fade.
- **Nothing a poll repaints moves.** Totals, rates, counts, the clock, the
  journal and the charts change in place, with no count-up, no flash and no
  transition.
- **Destructive actions ask first, in the panel's own buttons**: End run and
  Clear history each open a confirmation with a primary button that repeats
  the action and a secondary that keeps things as they are.
- **New borders are allowlisted**: `.trk-field` (the text fields' 1px strong
  edge) and `.ovl-ticker` (the 1px subtle rule above the overlay's newest
  drops), both in the export's `borders`.

## Detector findings

`impeccable detect --json ForgePact/design/tracker/tab.html
ForgePact/design/tracker/overlay.html` (impeccable 4.3.1), last run after the
fixes below: 41 findings, all in `tab.html`; `overlay.html` is clean. A run
with the comp's `#workspace` emptied (the shell alone) reports the same
cramped-padding, undersized-ui-text and wide-tracking findings and three of
the dark-glow ones, so those come from the panel's existing shell, not from
the Tracker design.

- **dark-glow** (21): kept. Every one is Ember's approved material
  (`panel/DESIGN.md` § Ember Forge): the switch's 8px glow when on
  (`#e9832b`, one per switched-on switch, 18), the active rail button and the
  footer's Apply button (`#bb6120`, `#ce762b`, `#ec6114`). Ledger, Graphite
  and Sigil draw no glow, and `tracker.css` adds none.
- **low-contrast** (6 in `tab.html`): kept, a false positive. The detector
  reads the primary button's background as the accent (`#e99748`), but Ember
  paints `.btn.primary` with a gradient from `#754225` to `#331b12` and a
  transparent background colour (measured in Edge on this comp). Its text
  `#ffe3b6` is 6.60:1 on the gradient's lighter end and 12.95:1 on the darker.
  The same holds for the hover state.
- **low-contrast** (earlier run, 27 in `overlay.html`): fixed. The plate was a
  separate layer the detector could not see, and the frames sat on a striped
  stand-in for the game, so real text over a bright frame was never checked.
  The plate is now the overlay's own translucent background with a 90% floor,
  the stand-in is a white frame, and the overlay uses no faint text.
- **repeating-stripes-gradient** (earlier run, `overlay.html`): fixed. It was
  the comp's striped stand-in for the game, now a flat white frame.
- **wide-tracking** (1): kept. It is the rail's brand mark (`.brand-name`,
  0.08em, `panel/DESIGN.md` § Typography, "Brand mark"). The run card's
  tracked mono caps label that also triggered it was fixed: it is now
  sentence-case "ForgePact Tracker" with no tracking.
- **undersized-ui-text** (12): kept. The detector sizes the shell's rail
  labels, status chip and footer from the panel's narrow-width rules; measured
  in Edge at 1280 the rail labels are 16px (Ember) and 14px (Ledger), Ember's
  plugin chip 14px and Auto-apply 14px. The two that really are small are the
  shell's brand subline and version (10px in Ember, 12px in Ledger), which
  this design does not touch. The Tracker's own smallest text is 12px
  (`--font-size-xs`).
- **cramped-padding** (1): kept. Measured in Edge, the bordered elements with
  no top padding are Ember's own value stepper (its step buttons sit flush in
  its frame) and, in Ledger, `#statusbar`'s top rule. Neither is Tracker
  markup.

## Critique

`impeccable critique` on both comps. ⚠️ DEGRADED: single-context (this
workflow agent has no sub-agent tool, so Assessment A, the design review, ran
first and Assessment B, the detector, after it, in one context). Questions
skipped: a workflow item with no channel to the owner; the owner sees the
comps with the first tab build (ctx "Owner look at the design comps").

| # | Heuristic | Score | Key issue |
|---|---|---|---|
| 1 | Visibility of system status | 3 | State line, clock, paused and catching-up states are shown; the overlay's "you are here" was a bare dot |
| 2 | Match system / real world | 3 | The game's own words (kk, grades D to SS, rarity names); "Drop card" is ours |
| 3 | User control and freedom | 3 | End run and Clear history confirm; a refused sound file changes nothing |
| 4 | Consistency and standards | 3 | Panel rows, switches, steppers and sub-tabs reused; a switched-off alert drew as live |
| 5 | Error prevention | 3 | Destructive actions confirm; the opacity floor prevents an unreadable overlay |
| 6 | Recognition rather than recall | 3 | Rarity written beside its colour in the main lists; the codex had colour only |
| 7 | Flexibility and efficiency | 2 | No shortcuts for pause or end run |
| 8 | Aesthetic and minimalist design | 3 | Calm and dense; the sound-list toolbar showed six actions at once |
| 9 | Error recovery | 2 | No refused-file message existed in the comp |
| 10 | Help and documentation | 2 | Hints under each card; no link to Help |
| **Total** | | **27/40** | **Good** |

**Design specificity.** The tab reads as ForgePact's: the same rows,
switches, steppers and sub-tabs as the rest of the panel, the game's own
number dialect, and the game's rarity hues as the one new colour family. The
risk was a generic stats dashboard; the per-rarity table, the grade column and
the zone chip keep it about Hero Siege.

**What works.** One switch heading the tab, with every other control below
it; the session card's four totals scan at a glance in both palettes; the
overlay's three totals stay readable in every state, the drop card included.

Findings, each fixed or kept:

- [P1] Alert rows broke in Ember: the plain switch took Ember's
  label-then-switch layout and wrapped the rarity row onto two lines. Fixed:
  the rows are the panel's `.setting-entry` with a `.switch.slider-switch`,
  like Monster Rarity's, and the sound's source moved to a note under the row.
- [P1] The drop card cut through a line of counters, half-covering the
  rates. Fixed: it now covers everything under the top line whole.
- [P1] Over a bright game frame a transparent overlay's text failed contrast
  below about 90% plate opacity. Fixed: the plate is translucent, not the
  text, with a 90% floor and no faint text in the overlay.
- [P2] The codex coloured each item by rarity without saying which (colour
  alone). Fixed: a Rarity column.
- [P2] The overlay's "you are here" was a green dot only. Fixed: the words
  "· here" follow the zone.
- [P2] A switched-off alert (Unholy) drew with the live accent body. Fixed:
  only a switched-on alert row is live.
- [P2] The Loot table's rarity names lost their colour to the table's text
  colour, and the tally lists likewise. Fixed: the colour is set on the table
  and the list, so a rarity class on a cell wins.
- [P2] The sound lists' toolbar showed six actions beside the filter.
  Fixed: New filter stays; duplicate, import, export and delete go in one
  "More filter actions" menu.
- [P2] No message for a refused sound file. Fixed: the Alerts sub-tab shows
  one in place, with the danger icon and the reason.
- [P2] The journal's grade sat at the far end of a wide row, away from the
  name it grades. Fixed: the grade follows the time.
- [P3] The run card's label was tracked mono capitals. Fixed: sentence case,
  no tracking.
- [P3] A long zone name could push the overlay's clock off its line. Fixed:
  the zone name truncates with an ellipsis.
- [P3] Paired switches in the buff pick and the drop-card rarities sat close
  enough to the next column's label to be read as its switch. Fixed: the
  column gap is 40px.
- [P3] The journal's time and "alerted" were faint text, which fails 4.5:1 on
  a hovered row in Ledger (4.28). Fixed: both are muted text.
- [P3] Gold's note said "not read offline", which read as if gold were not
  counted. Fixed: "bank not read offline".
- [P2] Alex, the power user: no keyboard shortcut for Pause clock or End run.
  Kept: the Tracker's were global hotkeys, which a panel window cannot
  register while the game has focus; the sub-tabs take the Mods tab's arrow
  keys.
- [P3] Jordan, the first-timer: "Drop card" is a name of ours. Kept: its
  card says what it does in its first sentence, and "flourish" (the Tracker's
  word) meant less.
- [P3] Sam, keyboard and screen reader: a run's finds, a sound list and the
  overlay's ticker colour names without writing the rarity. Kept: there the
  name is the point and the colour a redundant hint; every list where the
  rarity matters writes it.
- [P3] No Help link from the tab. Kept: Ember's Help section covers the
  panel, and each card carries its own hint.
- [P3] Ember's serif headings take a lot of height on six sub-tabs. Kept:
  they are Ember's approved look; Ledger, Graphite and Sigil use the panel's
  16px headings.
