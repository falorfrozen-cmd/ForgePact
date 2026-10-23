# Mark 1 - the pieces' icons

The four Mark 1 pieces (ForgePact #166) draw the owner's own art in the bag and
on the ground, by the same route as the Flight charms
([../flight-charms/ICONS.md](../flight-charms/ICONS.md)). The owner supplies the
eight PNGs, and all eight are in this folder and embedded in the plugin. A
piece whose PNGs are missing, or whose load failed, shows its base item's own
bag and ground art. No placeholder art is generated or shipped.

| Piece | Inventory icon | Dropped icon |
| --- | --- | --- |
| Mark 1 Helmet | `mark1_helmet.png`, 64 x 64 (2 x 2 cells) | `mark1_helmet_ground.png`, 32 x 32 |
| Mark 1 Body Armor | `mark1_body_armor.png`, 64 x 96 (2 x 3 cells) | `mark1_body_armor_ground.png`, 32 x 48 |
| Mark 1 Boots | `mark1_boots.png`, 64 x 64 (2 x 2 cells) | `mark1_boots_ground.png`, 32 x 32 |
| Mark 1 Gloves | `mark1_gloves.png`, 64 x 64 (2 x 2 cells) | `mark1_gloves_ground.png`, 32 x 32 |

## Art rules

- 8-bit RGBA PNG, not interlaced, with genuine transparency.
- Every pixel's alpha is 0 or 255: no partial-alpha edges.
- Exact canvas size for the inventory icon (32 px a cell); the dropped icon's
  size is read from the file, since the plugin re-centres it.
- One pixel of transparent padding on every side; no background, no text.
- Draw at native size or integer multiples with smoothing disabled.

`make_mark1_icons.py` checks every rule above but the padding, which is left to
the eye.

## Adding the art

1. Put the PNGs in this folder under the names above. Any subset works: a
   piece without its PNGs keeps its base item's art.
2. `py -3 assets/mark1/make_mark1_icons.py --write-manifest` records each PNG
   present in [manifest.json](manifest.json) (size, format, SHA-256).
   `manifest.json` lists only the PNGs that are present.
3. `py -3 assets/mark1/make_mark1_icons.py` writes
   `plugin/include/ForgePact/Mark1Icons.hpp`, the PNGs' bytes embedded in the
   plugin. Never edit the header by hand.
4. `py -3 assets/mark1/make_mark1_icons.py --check` must exit 0, and
   `py -3 -m unittest discover -s tests -p test_mark1_icons.py` stays green.

The generator refuses a PNG with an unexpected name, the wrong inventory size,
a format other than 8-bit RGBA, a partial-alpha pixel, a missing manifest entry
or a hash that does not match it, and a manifest entry with no PNG.

## In the game

The art follows the set's switch. Nothing is loaded, written or installed for
it until the session's first `mark1set bonus 1` (character select included);
from then on it stays armed for the rest of the session, and turning the
switch off does not disarm it. `mark1probe spawn` arms nothing. While the art
is not armed, every piece shows its base item's art, in the bag and on the
ground.

Once armed, a piece's embedded PNG is written to `bp_ipc\mark1\` under its file
name only when no file of that name is there yet, and loaded once a session.
A dressing writes our bag icon only for a piece whose bag icon and ground copy
both loaded, and only while the shared `LootGroundInit` detour is not known to
run table-only. The ground copy is drawn by one more consumer of that detour
(hidden loot's, after the Flight charms'), installed once, when an icon is
embedded and a character exists. With no PNG embedded, or a load that failed,
nothing is written or replaced and the piece shows its base item's art.
`mark1set stat` reports how many loaded as `icons=<loaded>/<embedded>` (`0/8`
before the art is armed, `0/0` in a build with no PNG embedded), `art=`
`off` or `armed`, and the ground route; a failed load prints one line naming
the files.

Measured in Mod live 2 (2026-10-19, `docs/mark1-set-research.md`, "Mod live 2
results"): before arming, the body armour showed its base item's art; after
`mark1set bonus 1`, `icons=8/8`, and the bag, the ground and the worn body slot
drew the owner's art. The tooltip draws no item icon at all.
