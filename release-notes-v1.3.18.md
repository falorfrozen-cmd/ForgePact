# ForgePact 1.3.18

Works together with Hero Siege Item Editor 2.15.3.

## New

- **Reveal full map now fills the map with monsters too.** Until now, revealing a
  zone showed you the terrain, the waypoint, the dungeon entrance, chests, shrines
  and mining nodes — but almost no monsters, which made a "fully revealed" map look
  strangely empty.

  The reason turned out not to be the map at all: **most mob packs do not exist
  until you walk near them.** Each zone is laid out with a few hundred spawn points
  that only create their pack once you come within about a screen and a half of
  them, so an unexplored zone genuinely has very few monsters in it to draw. In one
  measured zone: 310 spawn points waiting, and only 208 monsters actually in the
  world.

  With the new sub-toggle on, each zone you arrive in creates its packs right away,
  so the monsters are really there and show on the minimap. Same zone as above: 1273
  monsters. The game's own spawning code does the work, so pack contents, density
  and rarity are exactly what that zone would have produced anyway — including
  whatever your other ForgePact settings already do to them.

  It is a separate checkbox nested under **Reveal full map** because it is the only
  part that gives the game more to do. Measured at roughly 7.8 ms per frame (~128
  fps) in a zone with 816 monsters, but if a busy zone ever feels heavy, turn this
  one off and keep the map reveal.

- **Your pet can collect quest items for you.** New toggle in **Mods →
  Gameplay Mods**: while your pet is out, it walks to quest items on screen
  and picks them up, one at a time, instead of you hovering each one and
  pressing the interact key. The quest objective is credited exactly as if you
  had collected it by hand — the mod makes the game's own collect happen, it
  does not fake the counter.

  Only pick-up quest items are touched (bricks, bones, and the like). Things
  you activate, break or talk to are deliberately left alone, because the game
  collects those a different way and that way is not confirmed.

  Two caveats worth knowing before you turn it on:

  - The pickup is **silent** — no sound, no pickup effect. The item is
    collected and the objective moves; only the presentation is missing.
  - The pet collects any pick-up quest item on screen, **including ones for
    quests you have not accepted**. The game's own checks are what stop a
    collect that should not happen, and those are respected, but there is no
    extra "is this my quest" filter yet.

  Off by default.

## Changed

- **Reveal full map's description** now says what it actually does. It always
  revealed the mechanics, waypoints and chests along with the fog — that was never
  only a fog-of-war toggle.

## Fixed

- **Map reveal could leave a zone with fewer monsters than normal.** A first version
  of the monster pass ran while the zone was still loading, which caught the spawn
  points before they had finished setting themselves up and left them permanently
  inert — that zone then stayed empty even if you walked right over the spawn
  points. The pass now waits until the zone reports that its spawn points are ready
  before doing anything, and a zone that never reports ready is simply left alone.

  If you played a zone with the affected test build and it seemed unusually empty,
  that zone's state may still carry it; new zones are unaffected.

## How to update

Replace the panel and `modfiles/BloodPactPlugin.dll` with the ones from this
release, then restart the game.
