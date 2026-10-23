# ForgePact 2.5.0

Release date: 2026-10-30

## Fixed

- **Loot announcements now work in the Chaos Tower (#227).** With Loot
  announcements on, a Heroic, Angelic or Unholy item that dropped on a Chaos
  Tower floor got no chat line, while the same items were announced
  everywhere else. The tower keeps a floor's drops off the ground until the
  floor is cleared and then puts them all down at once, and ForgePact took
  that late landing for an item you had dropped from your bag, which it never
  announces. Now an item that dropped on the floor is announced when the
  floor's loot comes out at its end, with the usual red `SERVER: <your
  character> found <item>` line, at the moment it lands. An item you drop
  from your bag on a tower floor is meant to stay unannounced, as it does
  everywhere else; that case is covered by ForgePact's own tests but was not
  tried in a game session yet. Tested in a game session on 2026-10-23 with a
  development build of the plugin, on the first floor of the tower: a Heroic
  item put on the floor with a development command was announced when the
  floor was cleared, and with the fix switched off another one got no line,
  as in the report.
  Angelic and Unholy items in the tower were not tried in play; they take the
  same path as Heroic.

## How to update

Download and extract the complete release, then reopen ForgePact and press
**Install Mod Plugin** once. Only the plugin changed. Your existing settings
are retained. Start the game after installing; a game that was already open
keeps the old plugin until it is restarted.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
