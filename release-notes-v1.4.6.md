# ForgePact 1.4.6

## New

- **`menulayout` now also lists the town stash and the bag.** `menulayout` is
  the read-only command a tool such as the toolkit's `hs-drive` helper uses
  to find buttons on the game window instead of clicking fixed spots. It now
  also lists the stash window and the stash in town, the stash and bag tab
  buttons, the item grids with their size in grid cells, the split-stack
  dialog, the inventory's drag object (`UI_Inventory_Drag_obj`) and your
  character, and prints a few more of each one's own settings
  beside its position. That is the groundwork for `hs-drive` opening the stash
  and moving items in a test session without anyone at the keyboard; those
  tools are not in this release. Nothing in play changes, and there is no new
  switch in the panel.

## How to update

Download and extract the complete release, then reopen ForgePact. Your existing
settings are retained. Source users can run `Prepare-Plugin.bat` if plugin files
are missing before using **Install Mod Plugin**. This release changes the
plugin, so pressing **Install Mod Plugin** matters - updating only the panel
leaves the old plugin in place.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
