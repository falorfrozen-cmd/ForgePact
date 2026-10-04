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
    much faster. Checked in play: +50% made a Shadow Bolt fly 1.5 times as
    fast, and +100% twice as fast.
  - **Projectile Amount**, up to +5: each cast fires that many more
    projectiles. Checked in play: +2 turned one Shadow Bolt into three, and +5
    into six. Some casts also count projectiles a second time, for what looks
    like an item effect (in a research session a count of 6, which +2 raised to
    8); the slider cannot tell that count from the skill's own, so it raises it
    too. A Shadow Bolt sometimes makes 2 or 3 bolts on its own even with the
    slider off; that is the game.
  - **Area of Effect**, up to +100: adds to your skills' Area of Effect.
    Checked in play: +50 grew a Soul Spurn from 7.5 to 8.0 in the game's own
    size units, and +100 to 8.5. Some skills keep their own size: **Healing
    Zone** does not grow.
  - Only your own casts are changed, including the repeat a double-cast effect
    makes of them. Your mercenary and the enemies are left alone: in play, the
    mercenary's calls reached the plugin and were left unchanged (no enemy was
    seen reaching it). The sliders
    are not built to change your basic attack, but that has not been observed
    for any of the three (no basic attack was cast in the checks).
  - With all three off, the default, the game behaves exactly as without
    ForgePact. If the plugin cannot set a slider up on your game, it stays at 0
    and the log says why.
  - Checked in play on 2026-10-04 with a development build of this release's
    plugin, on a White Mage; turned back to 0, the speed and the size were the
    game's own again. Other classes have not been played with the sliders.

## How to update

Download and extract the complete release, then reopen ForgePact: the panel
gained three sliders, so updating only the plugin leaves them out. Your
existing settings are retained. Source users can run `Prepare-Plugin.bat` if
plugin files are missing before using **Install Mod Plugin**. The plugin
changed too, so **Launch Modded Game** brings it up to date for you. If you
start the game from Steam instead, or ForgePact's warning asks for it, press
**Install Mod Plugin** once after updating.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
