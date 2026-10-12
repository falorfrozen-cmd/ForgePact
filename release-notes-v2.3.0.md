# ForgePact 2.3.0

Release date: 2026-10-16

A new **Always show Chaos Pillar mods** switch, off by default, shows each
Chaos Pillar's mods while the pillar is on screen, without hovering over it.
A new **Boss soul pieces** switch, also off by default, lets monsters drop a
piece of their act boss's soul that takes you straight to that boss for a
fresh fight.

## New

- **Always show Chaos Pillar mods (#7).** A Chaos Pillar shows its box of
  mods (pack size, magic find, experience, extra loot and the rest) only while
  your mouse cursor is on it, so choosing which pillar to activate means
  pointing at each one in turn. With this switch on (Mods → Quality of Life,
  off by default), every Chaos Pillar on screen shows its box on its own, like
  a strongbox in Path of Exile. It is the game's own box, with the game's own
  text and colours; a pillar that goes off screen hides it again. A pillar you
  have activated stops showing its box; in our session the game then replaced
  it with its fight within a few seconds. Switching it off returns every
  pillar to hover-only at once. It works with mouse play; gamepad is not
  supported. Checked in a game session with mouse play: boxes showed without
  hovering, hid when the pillars went off screen and when one was activated,
  and switching off brought back the hover-only boxes.
- **Boss soul pieces (#197).** Fighting an act's boss again means making your
  way back through the act to its lair. With
  this switch on (Loot → Boss soul pieces, off by default), monsters in an
  act's zones sometimes drop a Piece of that act's boss's Soul (Gurag's in Act
  1, the Grim Reaper's in Act 2, Mevius' in Act 7, and so on for all nine
  acts), a little more often than a Gem of Incarnation: 1 in 7,200 whenever
  the game rolls a gem for a dying monster. Right-click a piece in your bag
  and confirm, and you are taken straight to that act's boss lair for a fresh
  fight, even if you beat the boss before; no portal opens. It works from a
  town or any zone of an act, and is refused (you keep the piece) inside a
  boss lair or an uber realm. Each piece takes its own bag cell; pieces do not
  stack. Anything left on the lair's floor is gone when you use a piece for
  it. A piece is an Edible PAS Logo underneath, so your save loads without
  ForgePact, where a piece shows and works as an Edible PAS Logo. Checked in
  a research session: pieces stay apart in the bag, a right-click uses one up,
  the game's own travel reaches the Act 1 and Act 2 lairs, and Gurag and the
  Grim Reaper come out there and die without a crash. The use from a piece to
  the lair, and the boss returning after you beat it, are not yet checked in
  play with this release's plugin.

## How to update

Download and extract the complete release, then reopen ForgePact and press
**Install Mod Plugin** once. Both parts changed: the plugin (the Chaos Pillar
and Boss soul pieces switches) and the panel (their switches on the Mods and
Loot tabs), so updating only one of them leaves the other behind. Your existing settings are retained. Start the
game after installing; a game that was already open keeps the old plugin
until it is restarted.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
