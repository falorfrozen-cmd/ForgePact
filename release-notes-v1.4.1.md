# ForgePact 1.4.1

A clearer control panel with the offline launcher built in.

## New

- **HS Offline Launcher is built into ForgePact.** Press **Launch Modded Game**
  to start the selected game with Steam set up automatically. No separate
  launcher installation is needed. Launch progress and errors stay visible in
  Setup, and repeated clicks cannot launch another copy while a request runs.

## Changed

- **A simpler sidebar.** Removed the fixed Map Reveal, Headhunter and Pet Quest
  Collector shortcuts, which could be mistaken for a list of enabled mods.
  All three settings remain available in the Mods tab.

- **Gameplay Mods and Drop Rates are better aligned.** Enabled Map Reveal now
  uses the same border as other enabled mods, and the Pet Quest Collector card
  stays compact. Drop Rates fills both columns from the first row, including
  when searching or showing only modified settings.

- **Missing plugin files are easier to fix.** Source users can run
  `Prepare-Plugin.bat` before installing. An incomplete installation now shows
  a warning on every tab, even when the game is open, and sent commands are no
  longer labelled as already applied.

- **The ForgePact anvil is complete again.** Its upper-left surface no longer
  looks cut out, including at compact sidebar sizes.

- **Find settings by their icons.** Gold, key families, portals, Chaos Pillars,
  Chaos Tower, character stats and gameplay mods now have matching icons next
  to their names. Satanic Zone choices have effect icons too. Labels and
  checkmarks stay visible, and the artwork works offline at any display scale.

- **The whole panel has a new layout.** A sidebar brings Setup, Modifiers,
  World, Loot and Mods together, with the Toolkit's bronze anvil, consistent
  cards and readable switches. Narrow windows use top tabs. Search Modifiers
  and Loot or show only modified settings. Sliders now have minus/plus buttons;
  exact values remain editable with the keyboard. Full setting explanations
  are available under **How it works**.

- **Saving is easier to follow.** The header shows saving, saved and connection
  errors. Rapid changes are saved in order, and interrupted saves read back
  the stored settings where possible. Saved decimal values no longer snap to
  the slider step when the panel refreshes. Existing preferences are retained.

- **Satanic Zone Mods is easier to choose from.** Positive and negative modifiers
  now use readable, clickable cards with checkmarks. Search by name or effect,
  show only enabled or disabled modifiers, and see how many are selected. The
  panel explains the minimum of 3 positive / 2 negative choices before you try
  to remove one. Enable a whole column or restore both pools to their defaults.
  Your existing selections stay intact, changes save automatically, and the
  layout adapts to narrow windows. The game rules are unchanged.

## How to update

Download and extract the complete release, then reopen ForgePact. Your existing
settings are retained. Source users can run `Prepare-Plugin.bat` if plugin files
are missing before using **Install Mod Plugin**.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
