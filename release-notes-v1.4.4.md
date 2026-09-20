# ForgePact 1.4.4

Two new, off-by-default Gameplay Mods for toggle skills: mark the skill-bar
slot of a toggle you have running, so you can see at a glance that it is still
active, and stop a double cast proc from flipping a toggle straight back.

Five skills are covered: the White Mage's **Soul Spurn**, the Exo's **Lunar
Orbit**, the Plague Doctor's **Crematus**, the Butcher's **Submerged Knives**
and the Prophet's **Maelstrom of Frost**.

## New

- **Mark a running toggle skill on the skill bar.** A toggle you have switched
  on has no on-screen sign that it is still running - you had to remember
  whether you pressed it, or watch your health bar or the ground for the
  effect. With this mod turned on (Mods tab, off by default), a soft red
  outline appears around that skill's slot on the skill bar the whole time it
  is running, and disappears the moment it stops - on a re-press, on a zone
  change, or if the game cancels it. It covers Soul Spurn, Lunar Orbit,
  Crematus, Submerged Knives and Maelstrom of Frost. A plain cast - one made
  without the sub-talent that turns the skill into a toggle - does not light
  the outline, and neither does a skill outside that list.
- **Stop double cast re-casting a toggle skill.** With a double cast effect
  equipped, a double cast proc could cast one of these skills a second time on
  its own, a moment after your press - which flipped the toggle straight back,
  so it ended off when you had just turned it on, or on when you had just
  turned it off. With this mod turned on (Mods tab, off by default), that
  extra cast is skipped and the toggle stays the way your press left it. It
  only steps in when you actually have the sub-talent that makes the skill a
  toggle; without it, the double cast's extra cast goes through exactly as it
  does in the unmodded game. Your own presses, and double casts of every other
  skill, are not affected.

## How to update

Download and extract the complete release, then reopen ForgePact. Your existing
settings are retained. Source users can run `Prepare-Plugin.bat` if plugin files
are missing before using **Install Mod Plugin**. This change is in the plugin,
so pressing **Install Mod Plugin** matters - updating only the panel leaves the
old plugin in place.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
