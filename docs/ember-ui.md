# Ember Forge on the 2.0 panel

The approved forge, stone sidebar, copper frames and illustrated shortcuts now
run on the current Svelte panel. This integration starts at ForgePact main
`8cfdd6bead71bac01ec369a879f9d0274c65c634` (2.0.0). It does not replace the
Python API or native plugin with the older 1.4.7 implementation.

## For players

New/default configurations open with **Ember Forge**. An explicitly saved
Ledger, Graphite or Sigil choice stays selected. Change it in **Setup →
Appearance → Theme**.

Ember adds **Overview** and **Help**. The star on Overview selects up to three
quick controls. Each edits the original setting, including its on/off switch;
switching off retains the number for later. Shortcuts are stored on this
device. **Find a setting** (Ctrl+K / Cmd+K) searches all sections, including
Prime Evil Parts, Gems of Incarnation, Mods switches and Theme. Results use
the current controls' labels and descriptions. Choosing one opens its section
and focuses the original control (a disabled child focuses its row).
**Edit pool** opens Satanic Zone Mods.

The 2.0 features remain available: Enabled mods with Turn off and Undo, slider
switches, gem search/filter/save, the plugin warning tooltips, and all four
themes. The plugin status reports installation files, not a live connection.

Long pages scroll inside the content pane using a visible native scrollbar,
the mouse wheel or Page Down/End while the pane is focused. The action bar is
a separate grid row, not an overlay: its actual height is reserved even when
its controls wrap. Sidebar and filter lists keep independent scrolling. Page
changes reset the content pane to the top.
Theme choices and the expanded Enabled mods list stay inside that pane, with
their own scrolling when space is short. Opening either reveals its options
above the action bar. The Enabled mods heading remains visible when collapsed.
These in-flow menus open/close instantly; floating menus in the flat palettes
keep their existing fades. The action is consistently named **Apply all now**.
Undo lives in a separate row of the footer and fades without sliding across
controls. Long names wrap there; the content pane reserves the actual height.
Brief status messages occupy their own row instantly, without overlapping Undo.
Their live region stays in the accessibility tree while visually hidden.
Search leaves enabled controls in their original keyboard Tab order.
An active Undo follows theme changes without losing its action or timer.

## Development

- `panel/src/ember/Overview.svelte`: Overview, Help and native dialogs.
- `panel/src/ember/ember.js`: shortcut/navigation presentation. Writes delegate
  to existing controls and their serialized API handlers. It adds no polling
  loop, MutationObserver, game hook or watcher.
- `panel/src/ember/{ember,relief,compat,palette}.css`: theme-scoped styles.
  Other palettes keep their existing layout. Native controls are moved with
  their handlers intact and returned to their original positions on theme change.
- `panel/src/ember/assets/`: the exact 22 approved WebPs (301,192 bytes), bundled
  WOFF2 font and its license. Vite packages local assets; no remote fonts/images.
  IM Fell English was converted from 194,992-byte TTF to 93,544-byte WOFF2;
  all 372 glyph outlines, metrics and character mappings were verified equal.
- `design/ember/`: artwork provenance and full-size reference, excluded from
  the runtime bundle. `tools/pack_ember_relief.cjs` retains the crop recipe.

Run from the ForgePact directory:

```powershell
npm ci --prefix panel
npm --prefix panel run build
py -3 src/forgepact.py
```

The frontend build is served by the existing Python entry point and is included
by the existing EXE build. Updating these sources does not rebuild a game DLL.
The backend, plugin, SDK calls, game configuration and save format are unchanged.

## Verification

```powershell
npm --prefix panel test
npm --prefix panel run oracle:replay
npm --prefix panel run e2e
npm --prefix panel run e2e:gems
npm --prefix panel run e2e:ember
npm --prefix panel run e2e:ember-scroll
npm --prefix panel run e2e:review
npm --prefix panel run e2e:finish
npm --prefix panel run e2e:motion
npm --prefix panel run e2e:polish
npm --prefix panel run e2e:perf
```

Tests run against the real HTTP handler with temporary configuration and IPC
files. They never launch or install the game. Ember's additional suite covers
shortcut values and switches, persisted favorites, global search, all four
themes and all seven pages at 1600, 1280, 900 and 390 pixels. Screenshots go to
`panel/artifacts/ember/`. The original behavior recordings are unchanged; only
the derived oracle was regenerated to add the fourth theme selection.

Figma export files are unchanged. A shared Figma library is a separate follow-up.

`e2e:ember-scroll` disables headless Edge's scrollbar-hiding flag and verifies
wheel scrolling, native thumb dragging, keyboard navigation, last-card reachability,
independent lists/sidebar and all four themes. It exercises six viewport sizes,
including a 1093x614 CSS viewport (1366x768 at 125% scaling), 640x400 and mobile.
The thumb drag presses the thumb where a screenshot shows Edge drew it. Edge's
compositor hit-tests a scrollbar press against the last frame it finished, so
a point computed from the DOM right after a page switch could land on the
previous page's track on a slow runner (ForgePact#133).
`tests/test_panel_e2e_ember_scroll.py` includes it in the release browser-test group.
The form suite explicitly seeds Ledger for its original 1280px inline-width
contract; Ember's narrower content area correctly switches three long names to
the tray at 1280px. The scroll suite covers that transition and both menus down
to 640x400 and 390x640, using a pointer click on the last theme choice.
The motion suite uses 1600px for the three long inline entries; the performance
suite retains 1280px and three real entries (map reveal, Headhunter, Beacon),
without the long orb-pickup label that now correctly overflows. Sample counts
and performance budgets are unchanged.
