---
name: ForgePact control panel
description: ForgePact 2.0's Ember Forge presentation, alongside the preserved Ledger, Graphite and Sigil palettes.
colors:
  accent: "#e99a4c"
  accent-hover: "#f2ae68"
  accent-ink: "#1a1109"
  accent-tint: "#2b1e12"
  bg-base: "#15110d"
  bg-raised: "#251e17"
  bg-hover: "#2d241c"
  bg-sunken: "#100d0a"
  border-strong: "#826848"
  border-subtle: "#3a2f25"
  text-primary: "#efe4d3"
  text-muted: "#b5a794"
  text-faint: "#948672"
  text-inverse: "#1a1109"
  track: "#816852"
  switch-idle: "#79818d"
  focus: "#ffc47e"
  ok: "#8bd3a6"
  warn: "#e8c06a"
  warn-tint: "#261f14"
  danger: "#f29ba6"
typography:
  ember-display:
    fontFamily: "EmberFell, IM Fell English, Georgia, serif"
    fontSize: "26px"
    fontWeight: 400
  ember-body:
    fontFamily: "Segoe UI, Arial, sans-serif"
    fontSize: "14px"
    fontWeight: 400
  ember-symbol:
    fontFamily: "Arial, sans-serif"
    fontSize: "23px"
    fontWeight: 400
  display:
    fontFamily: "IBM Plex Sans, Segoe UI, system-ui, sans-serif"
    fontSize: "26px"
    fontWeight: 600
    lineHeight: 1.2
    letterSpacing: "-0.01em"
  headline:
    fontFamily: "IBM Plex Sans, Segoe UI, system-ui, sans-serif"
    fontSize: "16px"
    fontWeight: 600
    lineHeight: 1.2
  title:
    fontFamily: "IBM Plex Sans, Segoe UI, system-ui, sans-serif"
    fontSize: "14px"
    fontWeight: 500
    lineHeight: 1.45
  body:
    fontFamily: "IBM Plex Sans, Segoe UI, system-ui, sans-serif"
    fontSize: "14px"
    fontWeight: 400
    lineHeight: 1.45
    fontFeature: "tnum"
  body-sm:
    fontFamily: "IBM Plex Sans, Segoe UI, system-ui, sans-serif"
    fontSize: "13px"
    fontWeight: 400
    lineHeight: 1.45
  label:
    fontFamily: "IBM Plex Sans, Segoe UI, system-ui, sans-serif"
    fontSize: "12px"
    fontWeight: 400
    lineHeight: 1.45
  mono-value:
    fontFamily: "IBM Plex Mono, Consolas, monospace"
    fontSize: "13px"
    fontWeight: 400
    lineHeight: 1.45
  mono-label:
    fontFamily: "IBM Plex Mono, Consolas, monospace"
    fontSize: "12px"
    fontWeight: 400
    lineHeight: 1.2
rounded:
  sm: "4px"
  md: "6px"
  lg: "8px"
  pill: "999px"
spacing:
  space-1: "2px"
  space-2: "4px"
  space-3: "6px"
  space-4: "8px"
  space-5: "10px"
  space-6: "12px"
  space-7: "16px"
  space-8: "20px"
  space-9: "24px"
  space-10: "32px"
  space-11: "40px"
components:
  button-primary:
    backgroundColor: "{colors.accent}"
    textColor: "{colors.accent-ink}"
    typography: "{typography.body-sm}"
    rounded: "{rounded.md}"
    padding: "0 16px"
    height: "32px"
  button-primary-hover:
    backgroundColor: "{colors.accent-hover}"
  button-secondary:
    backgroundColor: "transparent"
    textColor: "{colors.text-primary}"
    typography: "{typography.body-sm}"
    rounded: "{rounded.md}"
    padding: "0 16px"
    height: "32px"
  button-secondary-hover:
    backgroundColor: "{colors.bg-hover}"
  button-quiet:
    backgroundColor: "transparent"
    textColor: "{colors.text-muted}"
    typography: "{typography.label}"
    rounded: "{rounded.sm}"
    padding: "4px 6px"
  button-quiet-hover:
    backgroundColor: "{colors.bg-hover}"
    textColor: "{colors.text-primary}"
  rail-tab:
    backgroundColor: "transparent"
    textColor: "{colors.text-muted}"
    typography: "{typography.body}"
    padding: "0 12px"
    height: "56px"
  rail-tab-active:
    textColor: "{colors.text-primary}"
  rail-tab-hover:
    backgroundColor: "{colors.bg-hover}"
    textColor: "{colors.text-primary}"
  segmented-control:
    backgroundColor: "{colors.bg-sunken}"
    rounded: "{rounded.md}"
    padding: "2px"
  segment:
    backgroundColor: "transparent"
    textColor: "{colors.text-muted}"
    typography: "{typography.body-sm}"
    rounded: "{rounded.sm}"
    padding: "0 10px"
    height: "24px"
  segment-selected:
    backgroundColor: "{colors.bg-hover}"
    textColor: "{colors.text-primary}"
  card:
    backgroundColor: "{colors.bg-raised}"
    rounded: "{rounded.lg}"
    padding: "16px"
  input-search:
    backgroundColor: "{colors.bg-sunken}"
    textColor: "{colors.text-primary}"
    typography: "{typography.body-sm}"
    rounded: "{rounded.md}"
    padding: "0 12px 0 36px"
    height: "32px"
  input-path:
    backgroundColor: "{colors.bg-sunken}"
    textColor: "{colors.text-primary}"
    typography: "{typography.mono-value}"
    rounded: "{rounded.md}"
    padding: "0 12px"
    height: "38px"
  value-stepper:
    backgroundColor: "{colors.bg-sunken}"
    textColor: "{colors.text-primary}"
    typography: "{typography.mono-value}"
    rounded: "{rounded.md}"
    height: "32px"
  select:
    backgroundColor: "{colors.bg-hover}"
    textColor: "{colors.text-primary}"
    typography: "{typography.body-sm}"
    rounded: "{rounded.sm}"
    padding: "0 6px 0 8px"
    height: "28px"
  select-hover:
    backgroundColor: "{colors.border-subtle}"
  theme-picker-trigger:
    backgroundColor: "{colors.bg-hover}"
    textColor: "{colors.text-primary}"
    typography: "{typography.body-sm}"
    rounded: "{rounded.sm}"
    padding: "0 6px 0 8px"
    height: "28px"
  theme-picker-list:
    backgroundColor: "{colors.bg-raised}"
    rounded: "{rounded.lg}"
    padding: "4px"
  theme-picker-option:
    textColor: "{colors.text-primary}"
    typography: "{typography.body-sm}"
    rounded: "{rounded.md}"
    height: "32px"
  theme-picker-option-active:
    backgroundColor: "{colors.bg-hover}"
  switch-off:
    backgroundColor: "{colors.border-subtle}"
    rounded: "{rounded.pill}"
    width: "40px"
    height: "22px"
  switch-on:
    backgroundColor: "{colors.accent}"
  switch-idle:
    backgroundColor: "{colors.switch-idle}"
  status-chip:
    backgroundColor: "{colors.bg-raised}"
    textColor: "{colors.text-muted}"
    typography: "{typography.label}"
    rounded: "{rounded.pill}"
    padding: "0 12px"
    height: "28px"
  enabled-mod-entry:
    backgroundColor: "{colors.bg-raised}"
    textColor: "{colors.text-primary}"
    typography: "{typography.body-sm}"
    rounded: "{rounded.md}"
    padding: "0 4px 0 10px"
    height: "28px"
  enabled-mods-toggle:
    backgroundColor: "{colors.accent-tint}"
    textColor: "{colors.accent}"
    typography: "{typography.label}"
    rounded: "{rounded.pill}"
    padding: "0 10px"
    height: "28px"
  tooltip:
    backgroundColor: "{colors.bg-hover}"
    textColor: "{colors.text-primary}"
    typography: "{typography.body-sm}"
    rounded: "{rounded.md}"
    padding: "8px 12px"
  toast:
    backgroundColor: "{colors.bg-hover}"
    textColor: "{colors.text-primary}"
    typography: "{typography.body-sm}"
    rounded: "{rounded.md}"
    padding: "0 16px"
    height: "40px"
---

# Design System: ForgePact control panel

This file records the panel as shipped in ForgePact 2.0.0. The design contract it was built against is [`design/figma-export.json`](design/figma-export.json), explained in [`design/README.md`](design/README.md). The tokens above are the Ledger palette's values from [`src/tokens.css`](src/tokens.css), which is generated from that export and never edited by hand. Where this file and the export disagree, the export leads, and this file is regenerated.

## Overview

**Creative North Star: "The Graphite Console"**

The panel is an instrument, not a showcase. It is a dark, flat console of rows: a label, a switch, a slider, a mono value. Each row says what a mod does and whether it is on. Depth comes from four tonal steps of one warm-to-cool dark (sunken, base, raised, hover). There is no imagery, no illustration beside a label, and no ornament. The one warm accent marks what is on, selected or actionable, and it stays scarce enough to find at a glance.

Density is deliberate. Rows are 44px tall, type runs from 12px to 26px, and a two-column workspace fits a whole tab's settings in one 1280px screen. Colour, radius, space and type all come from custom properties, so a palette switch repaints everything and changes nothing else. There are three palettes, all modes of one variable collection: **Ledger** (warm umber and amber, the default), **Graphite** (neutral grey with a copper accent) and **Sigil** (blue-black with a rose accent).

Motion is quiet and functional. A hover eases its colours in only where there is a real pointer. A button gives under the press. Popovers scale from their trigger, and toasts rise into place. Nothing that a poll repaints moves, and a keyboard action never animates.

**Key Characteristics:**
- Flat surfaces, with depth carried by tonal steps. Shadows appear only under floating layers.
- One accent per palette, reserved for on, selected and primary states.
- IBM Plex Sans for words and IBM Plex Mono for numbers, paths and counts. Both are bundled offline.
- Borders only where the export's `borders` allowlist names one, each with a reason.
- A generated token layer: switching a theme swaps colour variables only.

## Colors

The palette is a narrow tonal ramp of one dark hue with a single saturated accent and three signal colours. Every colour below exists in all three palettes under the same token name.

### Primary
- **Forge Amber** (`accent`): the active rail tab's 2px underline, a switched-on switch, the primary button, the enabled mods' values, the tray toggle's text and the helmet stat lines. It is the "this is on" colour.
- **Heated Amber** (`accent-hover`): the primary button and the accent text controls under a hovering pointer. Nothing else uses it.
- **Accent Ink** (`accent-ink`): text on the accent, and the knob of a switched-on switch.
- **Amber Wash** (`accent-tint`): the tray toggle's pill and the text selection. It is a low-contrast field behind accent text.

### Neutral
- **Soot Umber** (`bg-base`): the page, the top rail and the status bar.
- **Raised Umber** (`bg-raised`): cards, a Mods tab card, the status chip, the enabled-mods entries and the open tray.
- **Hover Umber** (`bg-hover`): hovered rows and buttons, the selected segment and sub-tab, the select controls, toasts and tooltips.
- **Well Umber** (`bg-sunken`): edit wells: the search fields, the path field, the value stepper and the segmented control's trough. It also tints the floating shadows.
- **Strong Edge** (`border-strong`): the 1px edge of editable fields and the secondary button, and the scrollbar thumb.
- **Subtle Edge** (`border-subtle`): the rail and status bar rules, the tray and tooltip edges, the modifier group dividers, and the body of a switch that is off.
- **Parchment** (`text-primary`): headings, labels and values.
- **Muted Parchment** (`text-muted`): descriptions, hints, inactive tabs, status text and a knob that is off.
- **Faint Parchment** (`text-faint`): notes, the "Nothing is on" empty state, remembered values and the offline status dot.
- **Inverse Ink** (`text-inverse`): text on light fills. It carries the same value as `accent-ink`.
- **Slider Track** (`track`): the range track.
- **Idle Slate** (`switch-idle`): a slider switch that is on but whose mod is not yet listed as enabled. It is neutral until the mod goes live, then takes the accent.

### Signal
- **Signal Green** (`ok`): "game open" on the status dot, the live badge text, a successful launch and a valid Satanic summary. It is also the buff polarity sign.
- **Signal Gold** (`warn`): the plugin warning icons, the warning state of the apply chip, and the "starting" launch state. `warn-tint` is its low-contrast field.
- **Signal Rose** (`danger`): save errors, a failed launch, an invalid Satanic selection and the debuff polarity sign.

### The other palettes

| Token | Graphite | Sigil |
|---|---|---|
| accent | #d89c6b | #e8707a |
| accent-hover | #ecc299 | #f08c94 |
| bg-base | #131517 | #0e1013 |
| bg-raised | #1e2225 | #1b1f25 |
| bg-hover | #25282b | #22262e |
| bg-sunken | #101214 | #0a0b0e |
| border-strong | #686f79 | #616d80 |
| border-subtle | #303439 | #2a2f38 |
| text-primary | #eeedeb | #eceef2 |
| text-muted | #abb0b8 | #a9b0bc |
| text-faint | #9a9fa6 | #8c94a1 |
| focus | #ecc299 | #f08c94 |
| ok / warn / danger | #8fcfa8 / #e3bd6f / #f08a94 | #7fd1ae / #e9d86e / #fa9529 |

The full set, including the ink, tint, track and idle values, is in [`src/tokens.css`](src/tokens.css).

### Named Rules
**The Token-Only Rule.** No raw colour literal appears in `src/` outside `tokens.css` and `icons.js`. Every style reads a `--color-*` token, and `tokens.css` changes only by regenerating it from the export.

**The Swap Colours Only Rule.** A theme is a `:root[data-theme]` block of colour variables. Space, radius, type and motion never vary by palette.

**The Accent Means On Rule.** The accent marks state (on, selected, primary, live), never decoration. A switch turned on for a mod that is not yet listed stays Idle Slate until the mod goes live.

**The Contrast Pair Rule.** Every text and background pairing is listed in the export's `contrastPairs` and passes WCAG AA in all three palettes (`scripts/contrast-check.mjs`). A failure is fixed in Figma, not patched in CSS.

## Typography

**Body Font:** IBM Plex Sans (with Segoe UI, system-ui, sans-serif)
**Label/Mono Font:** IBM Plex Mono (with Consolas, monospace)

Both are single upright variable WOFF2 files covering weights 100 to 700 (OFL-1.1), bundled under `src/fonts/`. They are preloaded in `index.html` and declared with `font-display: block`, so the panel needs no network connection.

**Character:** Plex's engineered grotesque reads as tooling rather than marketing. Its mono twin gives numbers a steady column. The body sets `font-variant-numeric: tabular-nums`, so even sans digits line up.

### Hierarchy
- **Display** (600, 26px, 1.2, -0.01em): the page heading (`h1`), one per tab.
- **Headline** (600, 16px, 1.2): card headings.
- **Title** (500, 14px): a Mods tab feature label and a group title's weight. Row labels in slider rows use 500 at 13px with a tight 1.2 line height, in a fixed 168px column.
- **Body** (400, 14px, 1.45): the default text size.
- **Body small** (400, 13px): hints, descriptions (the Setup launcher's description included, in muted text), buttons, fields, segments and tooltips.
- **Label** (400, 12px): the status bar, notes, tags, the enabled-mods row and the chip.
- **Mono value** (400, 13px): slider values, the path field and the number editor.
- **Mono label** (400, 12px): counts, enabled-mod values, remembered values and the brand subline.
- **Brand mark** (600, 14px, 0.08em tracking): "FORGEPACT" in the rail. It sits over a 12px mono subline tracked at 0.1em, which is hidden below 1100px.

### Named Rules
**The Mono For Numbers Rule.** A value the user reads as a number, count or path is set in Plex Mono. Words are set in Plex Sans. The one exception is a control's name inside a sentence (the chain warning's Install Mod Plugin), which is a mono run with no quotes.

**The Three Weights Rule.** Only 400, 500 and 600 are used (`--font-weight-regular`, `-medium`, `-semibold`). Emphasis inside prose (`b`, `strong`) uses 600.

## Layout

The shell is a grid with three bands. At the top is a 56px rail with the brand, the tab bar, and a fixed status chip at the right. Below it the page scrolls with 32px gutters on both sides: the thin 10px scrollbar sits inside the right one (`--scrollbar-w` comes out of the right padding), so content is inset the same from each window edge. A 33px status bar is fixed to the bottom. The page's bottom padding grows when the undo toast is open, so the toast never covers content.

Inside the page, the order is: the enabled-mods row, the page heading with its actions (auto-apply, Apply all, the plugin warning), an optional search toolbar with a segmented filter, then the workspace. The workspace is a two-column grid with 16px row gaps and 24px column gaps. Cards span both columns. Content inside a card lays out as rows:

- **Rows** are 44px minimum, with a 16px gap: label column, switch, slider, then a 32px value stepper.
- **Modifiers** splits into two columns of groups (20px row gap, 32px column gap), with a 1px subtle rule above each group that has another group above it.
- **World** splits 500fr to 692fr: settings on the left, and the Satanic pool spanning four rows on the right.
- **Mods** is two flex columns, each mod its own raised block, with child features indented 20px. The indent alone marks a child; no glyph does. In the Miner's Helmet block, "Vein Resonance:" starts its own line.

The spacing scale is an 11-step ramp (2, 4, 6, 8, 10, 12, 16, 20, 24, 32, 40). The 16px step is the workhorse: card padding, row gaps and the gap between cards.

**Responsive behaviour.** At 1100px and below, everything drops to one column, the gutters become 20px on both sides, the label column narrows to 152px, and the brand subline and "Offline tools" hide. At 1100px wide and 640px tall or less, the page heading's title and description share a line. At 720px and below, the rail tightens, the brand text hides, rows wrap with full-width labels, cards pad 12px, the segmented controls stretch full width, and the status bar's credits hide.

**The enabled-mods row has two forms.** Inline, it is a wrapping row under the rail, as tall when empty as with entries, so the first entry moves nothing. When the row's natural one-line width exceeds the space available, it becomes a tray: a pill toggle in the rail beside the status chip, opening a 320px list. The switch back needs 160px of spare width (hysteresis), so the form does not flicker at the threshold.

## Elevation & Depth

The system is flat and tonal. Four steps carry depth: **sunken** for wells you type into, **base** for the page, **raised** for cards, and **hover** for what the pointer is on and for floating surfaces. Shadows exist only under layers that float above the page. Each shadow is tinted from `bg-sunken`, so it deepens the palette rather than greying it.

### Shadow Vocabulary
- **Tray** (`box-shadow: 0 12px 32px color-mix(in srgb, var(--color-bg-sunken) 80%, transparent)`): the enabled-mods tray list and the theme picker's list.
- **Float** (`box-shadow: 0 8px 24px color-mix(in srgb, var(--color-bg-sunken) 80%, transparent)`): both toasts and both tooltips (the plugin warning and an idle slider's note).

### Named Rules
**The No Nested Card Rule.** No card sits inside a card. The Mods tab's two sub-tab panels keep their card role but draw no surface, and each mod is its own raised block.

**The Floating-Only Shadow Rule.** A shadow means "this layer floats over the page". Cards, rows and buttons carry none.

## Shapes

Corners are gently rounded on a four-step scale. `sm` (4px) is for controls inside a container: segments, step buttons, selects, quiet buttons and the live badge. `md` (6px) is for standalone controls and small surfaces: buttons, fields, the stepper, entries, toasts and tooltips. `lg` (8px) is for cards and the tray. `pill` is for switches, the status chip, the tray toggle, the slider track and the thin 6x18px slider thumb.

Borders are rules, not outlines. Each one is named in the export's `borders` list with a reason:
- the rail's bottom rule and the status bar's top rule;
- the 2px accent underline of the active tab and the selected sub-tab;
- the 1px strong edge of editable fields and the secondary button;
- the 1px subtle edge of the tray and the two tooltips;
- the modifier group divider;
- the focus outline.

Icons are 16px line drawings with a 1.5 to 1.6 stroke and round caps, drawn in `currentColor` or through CSS masks. The brand mark is two nested diamonds in the accent.

### Named Rules
**The Allowlisted Edge Rule.** A new border needs an entry in the export's `borders` list with its `why`. `tests/design-match.mjs` fails on any border outside that list.

## Components

The components are tactile but restrained. Every control is a flat fill or a bare label until a pointer or a state gives it colour.

### Buttons
- **Shape:** gently rounded (6px), 32px minimum height, 16px side padding, body-small at 500 weight.
- **Primary:** an accent fill with accent-ink text. It turns Heated Amber on hover.
- **Secondary:** transparent with a 1px strong edge and primary text, turning Hover Umber on hover. "Apply all now" carries a masked refresh icon.
- **Quiet:** transparent muted text with a 4px radius. Used for Turn off, the step buttons, the Satanic tools and the undo action (which is accent text). It gains the Hover Umber fill and primary text on hover.
- **Press:** under a pointer, a button scales to 0.97 over 120ms on the standard easing. A press from the keyboard never scales.
- **Disabled:** 45% opacity, default cursor.

### Switches
- **Style:** a 40x22px pill with a 16px round knob inset by 3px. Off is a Subtle Edge body with a Muted Parchment knob on the left.
- **On:** the accent body with an accent-ink knob, 18px to the right.
- **Idle (slider switch):** an Idle Slate body with a Parchment knob until the mod is listed as enabled, then the accent. A slider whose switch is off dims its range to 35%.
- **Disabled:** 45% opacity. The knob sits left in muted colour, reading as neither on nor off.
- The knob moves without a transition. The Satanic and gem pools draw the same switch on a native checkbox.

### Sliders and value stepper
- **Range:** a 4px pill track in the track colour, with a 6x18px Parchment pill thumb.
- **Stepper:** a sunken 6px well, 32px tall. It holds a minus step button, a 60px mono value, and a plus step button. "off" shows in muted colour. Editing swaps in a 26px Hover Umber number field. A switched-off slider shows the value it returns to in faint mono, to the left of the stepper.

### Segmented control and sub-tabs
- **Style:** a sunken 6px trough with 2px padding. Its 24px segments are transparent muted text.
- **Selected:** a Hover Umber fill with primary text. A Mods sub-tab also takes the 2px accent underline, like the rail's active tab.

### Cards / Containers
- **Corner Style:** 8px.
- **Background:** Raised Umber on Soot Umber.
- **Shadow Strategy:** none (see Elevation & Depth).
- **Border:** none.
- **Internal Padding:** 16px, with 16px gaps between children. At 720px and below the padding is 12px.
- A heading's hint pulls up 12px under it, and consecutive rows sit flush.

### Inputs / Fields
- **Style:** a Well Umber fill, a 1px strong edge and a 6px radius, 32px tall. The search fields carry a 16px masked search glyph inset 12px. The executable path field is 38px tall and set in mono.
- **Focus:** the global 2px focus-colour outline at a 3px offset. The caret is the accent.
- **Selects:** 28px, a Hover Umber fill, no edge, a 4px radius (the Mods tab's skill timer style).

### Theme picker
- **Trigger:** a compact, quiet button on Setup's Appearance card: the palette's name and a small muted chevron, 28px, a Hover Umber fill, no edge, a 4px radius, turning Subtle Edge on hover. It is named by the row's "Theme" label and its own value ("Theme Ledger") and carries `aria-haspopup="listbox"` and `aria-expanded`.
- **List:** a listbox that opens below the trigger: Raised Umber, a 1px Subtle Edge, 8px corners, 4px padding and the tray shadow. Each option is a 32px row with a 40x16px swatch strip of its palette's own Strong Edge, Muted Parchment and accent (read from `tokens.css` at run time), then its name. The active option takes the Hover Umber fill, and the chosen one carries the drawn check in the accent at the right.
- **Keyboard:** Enter, Space and the arrows open it on the chosen palette; the arrows, Home, End and a first letter move; Enter and Space choose; Escape closes with no change; Tab closes and moves on. Focus returns to the trigger.
- **The control of record** stays the theme's own `select` element underneath, clipped to 1px, `aria-hidden` and out of the tab order. Choosing sets its value and dispatches `change` only on a real change, so what is saved is exactly what the select saved.

### Check marks
One drawn check (a masked 16px stroke, `--icon-check`) serves every "done" mark: the settled save indicator ("Settings loaded", "Saved") and a valid Satanic selection in the ok colour, the launch feedback's ready line, and the chosen theme in the accent. No text glyph stands in for an icon anywhere in the panel.

### Navigation
- **Rail tabs:** full rail height, 12px side padding, a 16px line icon and 14px muted text. Hover takes the Hover Umber fill and primary text. Active has primary text at 500 weight, an accent icon and the 2px accent underline. Keyboard focus draws the outline inset.
- **Status chip:** a raised pill, 28px, with a 6px dot. The dot carries the game's state (green when on, gold when warning or in error, faint when offline); the words stay muted in every state.
- **Status bar:** 12px muted text holding the save indicator (with the drawn check in the ok colour once settled, a spinner ring while saving), the apply chip, and the credits and version at the right.
- **Rail icons:** Setup is a ring with a centre, Mods a 2x2 grid of squares, drawn in the same stroke as Modifiers, World and Loot.

### Enabled mods (signature)
Each entry is a 28px raised block holding the mod name (13px, 500), its value in accent mono, and a quiet Turn off button. In the tray, the entries become plain 32px list rows that highlight on hover. When the list overflows, a gradient fades it into the surface. A turned-off entry leaves a placeholder that fades and shrinks to 0.95 over 120ms, and an undo toast rises above the status toast for eight seconds, paused while it is hovered or focused, or while the window is hidden.

### Toasts and tooltips
- **Toast:** Hover Umber, 6px, 40px tall (undo 44px), with the float shadow, fixed at the bottom right above the status bar. It rises 8px into place over 200ms and sinks out over 120ms on the standard easing.
- **Tooltip:** Hover Umber with a 1px subtle edge, 6px corners, 8px by 12px padding and the float shadow. It scales from 0.97 at its trigger over 120ms on the emphasized easing. The plugin warning's tooltip and an idle slider's note share this one look. A note tooltip passes the pointer through and never moves a row.

### Motion
Tokens: `--motion-duration-fast` (120ms) and `--motion-duration-base` (200ms); `--motion-easing-standard`, `-emphasized` and `-hover`. Only CSS transitions and `@starting-style` are used; there is no animation library.
- A hover eases colour, background and border over `fast` on the `hover` easing, only under `(hover: hover) and (pointer: fine)`, and only on the element under the pointer. A list row's highlight is instant.
- The tray opens over `base` on `emphasized` (0.97 to 1, with opacity) and closes over `fast` on `standard`. Opened or closed from the keyboard, or rebuilt while open, it appears at once (`data-instant`).
- The theme picker's list does the same from its trigger's corner: open over `base` on `emphasized`, close over `fast` on `standard`, at once from the keyboard. A closing list takes no pointer.
- **Reduced motion** (`prefers-reduced-motion: reduce`): the press, every scale and every rise are removed. The opacity fades of the tray, the theme picker's list, tooltips, toasts and removed entry stay, and so do the hover colour fades (the owner's "Keep colour fades too", `amendments.ship`).

## Ember Forge (owner-approved integration)

The owner approved the illustrated forge reference in `../design/ember/` and
asked to carry it onto the current Svelte panel before any Figma work. Ember
is the default for an unset or unknown theme; explicitly saved Ledger,
Graphite and Sigil choices remain unchanged. The flat-panel specifications
above and below continue to describe those three palettes.

Ember's intentional differences are scoped under `html[data-theme="ember"]`
in `src/ember/`: local copper/stone material textures, the IM Fell English
display face, illustrated setting icons, a vertical desktop sidebar and a
bottom action footer. The shell uses a constrained content viewport and a
separate footer grid row, reserving the footer's actual height. A visible
native scrollbar, wheel and keyboard expose the entire page; sidebar and
filter-list scrolling remain independent. Its seven sections include Overview and Help. Overview
offers up to three selectable shortcuts and searches all current controls.
Every write delegates to an existing setting handler; no game hooks or
polling loop are added. At narrow widths the sidebar becomes a horizontal
navigation rail and controls stack. Dialogs use native focus trapping.

Shared tooltips and Undo keep the existing motion tokens, keyboard behavior
and reduced-motion rules. In reduced motion, opacity and colour fades remain
while movement is removed. Ember's theme menu
and expanded Enabled mods list participate in the pane's scroll flow, reveal
themselves on open and cap their list height to the pane. Every palette and
Turn off action must remain reachable above the separate footer. These two
in-flow menus open and close instantly, including pointer input, so their
layout space never lingers after a fade. Flat palettes keep floating-menu
animations. Apply status text uses at least `--font-size-xs` at every width.

The artwork, typography, literal material colours and ornamentation are
approved exceptions to the flat-panel rules below, confined to Ember. The
material palette is fixed artwork-matching chrome; `palette.css` supplies the
semantic colours of shared controls rather than recolouring that artwork.
Segoe UI is Ember's body face, with Arial as the system fallback; the existing
arrow/check glyphs explicitly use Arial. No Arial font file is bundled. The
existing Figma export and generated `tokens.css` are unchanged; shared Figma
design-library work is deferred. See `../docs/ember-ui.md` for provenance and
tests. `e2e:finish` retains Ledger's gutter/type baseline and exercises all
four picker options; `e2e:ember` verifies the new layout at four widths.

## Do's and Don'ts

### Do:
- **Do** read every colour, space, radius, font and duration from a token (`var(--color-*)`, `--space-*`, `--radius-*`, `--font-*`, `--motion-*`). To change one, change Figma, re-export, and regenerate `tokens.css`.
- **Do** build settings as rows: a 44px row, a label column (168px), a switch, a slider, a sunken mono stepper.
- **Do** mark on, selected and primary with the accent and nothing else. Keep status words muted and let a dot or icon carry the signal colour.
- **Do** step depth tonally: sunken for wells, raised for cards, hover for pointer and floating layers.
- **Do** gate hover styles behind `(hover: hover) and (pointer: fine)`, and mark any keyboard-triggered appearance `data-instant`.
- **Do** check a new text and background pairing in all three palettes by adding it to `contrastPairs`.

### Don't:
- **Don't** hand-edit `src/tokens.css`, or put a colour literal anywhere in `src/` except `tokens.css` and `icons.js`.
- **Don't** add a border that is not in the export's `borders` allowlist, and don't put a card inside a card.
- **Don't** put a shadow on anything that does not float over the page.
- **Don't** draw an illustration beside a label, heading or button. The legacy setting sprite is carried but hidden (`.setting-icon { display: none }`).
- **Don't** set uppercase tracked labels above headings. The single "LIVE MODIFIERS" badge is not a pattern to extend.
- **Don't** animate anything that a poll repaints, animate on keyboard focus, or move layout during a transition. Animate transform and opacity only, and under reduced motion animate opacity and colour only.
- **Don't** use a text glyph (an arrow, a tick) as an icon. Draw it (the check mask) or let spacing carry it (the child indent).
- **Don't** remove the hidden theme select under the theme picker, or give the picker's trigger or options a button id: the behaviour oracle, the browser suites and the panel's change handler drive that select, and the oracle's coverage walk counts every `button[id]`.
- **Don't** fill a quiet note with a resting default. A note that only says "off" at rest (Monster Rarity's) is hidden until a row is on; it keeps its words, since the toast after a change reads them.
- **Don't** load a font or any other asset from the network. The panel runs offline.
- **Don't** use game art or game text in the panel's chrome.
