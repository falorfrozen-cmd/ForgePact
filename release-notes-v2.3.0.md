# ForgePact 2.3.0

Release date: 2026-10-16

Three new sliders on the Modifiers tab, in a new **Skills** group:
**Projectile Speed**, **Projectile Amount** and **Area of Effect**. Each one
changes only your own skills, has its own switch and is off by default.

## New

- **Skill sliders: Projectile Speed, Projectile Amount and Area of Effect
  (#160).** Modifiers → Skills. Each slider adds to what the game has already
  worked out for your skill from your gear and buffs, so those still count
  underneath.
  - **Projectile Speed**, up to +100%: the projectiles of your skills fly that
    much faster. In a research session that added to the same value, +50% made
    a Shadow Bolt fly 1.5 times as fast.
  - **Projectile Amount**, up to +5: each cast fires that many more
    projectiles. In the research session, +2 turned one Shadow Bolt into three.
    Some casts also count projectiles a second time, for what looks like an item
    effect (in that session a count of 6, which +2 raised to 8); the slider
    cannot tell that count from the skill's own, so it raises it too.
  - **Area of Effect**, up to +100: adds to your skills' Area of Effect. In the
    research session, +50 grew a Soul Spurn from 7.5 to 8.0 in the game's own
    size units. Some skills keep their own size: **Healing Zone** does not grow.
  - Only your own casts are changed, including the repeat a double-cast effect
    makes of them. Your mercenary and the enemies are not affected. The
    sliders are not built to change your basic attack, but that has not been
    observed for any of the three (no basic attack was seen in the research
    sessions).
  - With all three off, the default, the game behaves exactly as without
    ForgePact. If the plugin cannot set a slider up on your game, it stays at 0
    and the log says why.
  - The sliders have been tested outside the game; they have not yet been
    checked in play with this release's plugin. The numbers above come from the
    research build, which changed the same values with a test command, on a
    White Mage. Values above +2 projectiles, +50 Area of Effect and +50% speed
    have not been played.

## How to update

Download and extract the complete release, then reopen ForgePact: the panel
gained three sliders, so updating only the plugin leaves them out. Your
existing settings are retained. Source users can run `Prepare-Plugin.bat` if
plugin files are missing before using **Install Mod Plugin**. The plugin
changed too, so **Launch Modded Game** brings it up to date for you. If you
start the game from Steam instead, or ForgePact's warning asks for it, press
**Install Mod Plugin** once after updating.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
