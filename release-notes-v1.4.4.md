# ForgePact 1.4.4

Two new, off-by-default Gameplay Mods for the White Mage's Soul Spurn: outline
its skill-bar slot while the Purgatory-toggled drain is running, so you can
see at a glance that it is still active, and stop a double cast proc from
flipping the toggle back.

## New

- **Outline Soul Spurn while draining.** Soul Spurn with the Purgatory
  sub-talent toggled on has no on-screen sign of whether it is still active -
  you had to remember whether you pressed it, or watch your health bar for
  the drain. With this mod turned on (Mods tab, off by default), a gold
  outline appears around Soul Spurn's skill-bar slot the whole time the
  Purgatory drain is running, and disappears the moment it stops - on a
  re-press, on a zone change, or if your health forces it to cancel. A plain
  cast of Soul Spurn without Purgatory does not light the outline.
- **Stop double cast re-casting Soul Spurn.** With a double cast effect
  equipped, a double cast proc could cast Soul Spurn a second time on its
  own, a moment after your press - which flipped the Purgatory toggle
  straight back, so it ended off when you had just turned it on, or on when
  you had just turned it off. With this mod turned on (Mods tab, off by
  default), that extra cast of Soul Spurn is skipped and the toggle stays the
  way your press left it. Your own presses, and double casts of every other
  skill, are not affected. One trade-off: the double cast's extra Soul Spurn
  is skipped even when you do not have Purgatory.

## How to update

Download and extract the complete release, then reopen ForgePact. Your existing
settings are retained. Source users can run `Prepare-Plugin.bat` if plugin files
are missing before using **Install Mod Plugin**. This change is in the plugin,
so pressing **Install Mod Plugin** matters - updating only the panel leaves the
old plugin in place.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
