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
Prime Evil Parts and Gems of Incarnation. **Edit pool** opens Satanic Zone Mods.

The 2.0 features remain available: Enabled mods with Turn off and Undo, slider
switches, gem search/filter/save, the plugin warning tooltips, and all four
themes. The plugin status reports installation files, not a live connection.

## Development

- `panel/src/ember/Overview.svelte`: Overview, Help and native dialogs.
- `panel/src/ember/ember.js`: shortcut/navigation presentation. Writes delegate
  to existing controls and their serialized API handlers. It adds no polling
  loop, MutationObserver, game hook or watcher.
- `panel/src/ember/{ember,relief,compat,palette}.css`: theme-scoped styles.
  Other palettes keep their existing layout. Native controls are moved with
  their handlers intact and returned to their original positions on theme change.
- `panel/src/ember/assets/`: the exact 22 approved WebPs (301,192 bytes), bundled
  font and its license. Vite packages local assets; no remote fonts/images.
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
