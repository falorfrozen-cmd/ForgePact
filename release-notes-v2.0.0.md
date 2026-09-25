# ForgePact 2.0.0

The panel gets three new controls: a list of the mods you have turned on, with
a button to turn each one off, an on/off switch on every slider, and a choice
of colour theme.

## New

- **Enabled mods.** A new list near the top of the panel, just under the Apply
  controls, shows every mod you have turned on, with its current value, and
  says how many are on (**Nothing is on** when none are). Each entry has a
  **Turn off** button that switches that mod off exactly as its own control
  would, and the mod leaves the list. Settings that are only options of
  another mod (map population, the auto-prospect material move, the
  gem mod filter) and the Satanic Zone modifiers are not listed.
- **An on/off switch on every slider.** Every slider now has its own switch,
  like Monster Density's. Turning a slider off keeps the value you set, while
  the game plays as if the slider were at its default; turning it back on
  sends your value again. **Apply all** leaves a switched-off slider at the
  default.
- **Theme.** A new **Theme** choice, in the **Appearance** card at the end of
  the Setup tab, picks the panel's colour theme. The panel remembers it with its other
  settings, so your choice is still there the next time you open ForgePact.

## Changed

- **Gems of Incarnation moved to the Loot tab.** The two switches and the mod
  filter now have their own card at the end of the Loot tab, instead of sitting
  among the Mods tab's Quality of Life switches. They work as before. The mod
  filter's list can now be searched and narrowed to **Enabled** or
  **Disabled** mods, like the Satanic Zone mods on the World tab, and it says
  **Unsaved changes** until you press **Save filter**.
- **Mining Ore Amount is now called Mining Ore Multiplier.** The Loot slider
  works as before; only its name changed, and the help sentence under it is
  gone. While the game runs, the line under it still says when a worn Miner's
  Helmet replaces the slider, or when the plugin could not enable it.
- **The "Choose your Hero_Siege.exe" band is now a warning icon.** Instead of
  a band across the top of every page, a small warning icon sits beside
  **Apply all now** and another beside **Settings loaded** at the bottom.
  Point at either one, or tab to it, to read the same message; click it to go
  to Setup.

## How to update

Download and extract the complete release, then reopen ForgePact. Your existing
settings are retained. Source users can run `Prepare-Plugin.bat` if plugin files
are missing before using **Install Mod Plugin**. This release's own changes
are all in the panel and need no new mod plugin. If you are updating from
1.4.5 or earlier, press **Install Mod Plugin** once anyway: 1.4.6 changed the
plugin (Gems of Incarnation).

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
