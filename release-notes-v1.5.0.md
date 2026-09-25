# ForgePact 1.5.0

The panel gets three new controls: a list of the mods you have turned on, with
a button to turn each one off, an on/off switch on every slider, and a choice
of colour theme.

## New

- **Enabled mods.** A new list near the top of the panel, just under the Apply
  controls, shows every mod you have turned on, with its current value, and
  says how many are on (**Nothing is on** when none are). Each entry has a
  **Turn off** button that switches that mod off exactly as its own control
  would, and the mod leaves the list. Settings that are only options of
  another mod (map population, the auto-prospect material move) and the
  Satanic Zone modifiers are not listed.
- **An on/off switch on every slider.** Every slider now has its own switch,
  like Monster Density's. Turning a slider off keeps the value you set, while
  the game plays as if the slider were at its default; turning it back on
  sends your value again. **Apply all** leaves a switched-off slider at the
  default.
- **Theme.** A new **Theme** choice, next to the game status in the header,
  picks the panel's colour theme. The panel remembers it with its other
  settings, so your choice is still there the next time you open ForgePact.

## How to update

Download and extract the complete release, then reopen ForgePact. Your existing
settings are retained. Source users can run `Prepare-Plugin.bat` if plugin files
are missing before using **Install Mod Plugin**. This release changes only the
panel: the mod plugin is the same as in 1.4.5, so you do not need to press
**Install Mod Plugin** again.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
