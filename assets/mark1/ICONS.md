# Mark 1 - the pieces' icons

The four Mark 1 pieces (ForgePact #166) draw the owner's own art in the bag and
on the ground, by the same route as the Flight charms
([../flight-charms/ICONS.md](../flight-charms/ICONS.md)). The owner supplies the
eight PNGs. Until a piece's PNGs are here, it shows its base item's own bag and
ground art. No placeholder art is generated or shipped.

| Piece | Inventory icon | Dropped icon |
| --- | --- | --- |
| Mark 1 Helmet | `mark1_helmet.png`, 64 x 64 (2 x 2 cells) | `mark1_helmet_ground.png`, about 24 x 24 |
| Mark 1 Body Armor | `mark1_body_armor.png`, 64 x 96 (2 x 3 cells) | `mark1_body_armor_ground.png`, about 24 x 32 |
| Mark 1 Boots | `mark1_boots.png`, 64 x 64 (2 x 2 cells) | `mark1_boots_ground.png`, about 24 x 24 |
| Mark 1 Gloves | `mark1_gloves.png`, 64 x 64 (2 x 2 cells) | `mark1_gloves_ground.png`, about 24 x 24 |

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

A piece's embedded PNG is written to `bp_ipc\mark1\` under its file name only
when no file of that name is there yet, and loaded once a session. The bag
icon is set on every dressing of a piece whose icon loaded, and the dropped
icon replaces a piece's ground sprite only when its own copy loaded. With no
PNG embedded, or a load that failed, nothing is written or replaced and the
piece shows its base item's art. `mark1set stat` reports how many loaded as
`icons=<loaded>/<embedded>` (`0/0` while no PNG is embedded); a failed load
prints one line naming the files.
