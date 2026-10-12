# Flight charm icon pack

Six original pixel-art PNGs prepared for ForgePact on 2026-10-12.

| Charm | Inventory icon | Dropped icon |
| --- | --- | --- |
| Pocket Elf (1 x 1 cell) | [pocket_elf.png](pocket_elf.png), 32 x 32 | [pocket_elf_ground.png](pocket_elf_ground.png), 16 x 16 |
| Gabriel Wings (2 x 2 cells) | [gabriel_wings.png](gabriel_wings.png), 64 x 64 | [gabriel_wings_ground.png](gabriel_wings_ground.png), 24 x 24 |
| Witch's Broomstick (1 x 5 cells) | [witch_broomstick.png](witch_broomstick.png), 32 x 160, upright | [witch_broomstick_ground.png](witch_broomstick_ground.png), 34 x 17, lying down |

All six exports are **8-bit RGBA PNGs** with genuine transparency. Alpha is
either 0 or 255 for hard pixel edges; no background, checkerboard, text or frame
is embedded in the icons. Dimensions above include transparent padding.
Draw at native size or integer multiples, with smoothing disabled.

Each dropped icon was generated separately, using its inventory master as a
design reference and simplifying the shapes for the smaller pixel canvas.
The dropped broom has its own horizontal composition.

## Generation and preparation

The creative artwork was made with the built-in **imagegen** tool, one call per
icon. No game artwork was extracted or copied. The exact six production prompts
and their intended dimensions are in [prompts.json](prompts.json).

Production export removes unused transparent source margins, fits the artwork
proportionally onto the requested canvas with nearest-neighbor sampling, and
sets solid pixel alpha. RGB values in fully transparent pixels are zero.
No smoothing filter is used.

[manifest.json](manifest.json) records dimensions, alpha counts, visible bounds
and SHA-256 hashes. All six files were decoded and checked for the exact canvas
size, 8-bit RGBA color type, both visible and transparent pixels, and no partial
alpha values. Final native-resolution sprites were also inspected in an
integer-enlarged contact sheet.

These are asset files for integration. This pack does not register items,
install sprites into the game or change charm mechanics.


On import into ForgePact (2026-10-12), four PNGs were newer than this
manifest, so `manifest.json` here was recomputed from the files as shipped
(hashes, pixel counts, bounds). Sizes and the 0/255-only alpha were
re-checked then.
