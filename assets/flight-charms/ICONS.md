# Flight charms - version 2

This version follows the requested visual references:

- **Pocket Elf:** Tinker Bell's blonde bun, leaf-green dress, green slippers and pale fairy wings.
- **Witch's Broomstick:** Nimbus 2000's mahogany shaft, brass binding and streamlined brown bristle bundle.
- **Gabriel Wings:** a pure white pair of classic feathered angel wings.

| Charm | Inventory icon | Dropped icon |
| --- | --- | --- |
| Pocket Elf | [pocket_elf.png](pocket_elf.png), 32 x 32 | [pocket_elf_ground.png](pocket_elf_ground.png), 16 x 16 |
| Gabriel Wings | [gabriel_wings.png](gabriel_wings.png), 64 x 64 | [gabriel_wings_ground.png](gabriel_wings_ground.png), 24 x 24 |
| Witch's Broomstick | [witch_broomstick.png](witch_broomstick.png), 32 x 160 | [witch_broomstick_ground.png](witch_broomstick_ground.png), 34 x 17 |

Use the six files **from this folder together** for the revised style. The
earlier icon pack remains in the parent folder. This is asset preparation only;
no game item registration or runtime code was changed.

## Production notes

The artwork was generated with the built-in **imagegen** tool. The ground wings
and ground broom are separate simplified drawings based on their new inventory
designs. The fairy ground icon is a 16 x 16 adaptation of the accepted fairy
inventory artwork; the image tool rejected its separate generation attempts.

All six exports are 8-bit RGBA PNGs with genuine transparency, solid pixel alpha
(0 or 255), exact canvas dimensions and no embedded background or text.
Transparent source margins are cropped, artwork is fitted proportionally with
nearest-neighbor sampling, and one logical pixel of transparent padding is
reserved. Draw at native size or integer multiples with smoothing disabled.

The final files were decoded and checked for exact dimensions, RGBA color type,
nonempty art, transparent pixels and absence of partial-alpha edges. They were
visually inspected in an integer-enlarged contact sheet.

[manifest.json](manifest.json) records dimensions, visible bounds, alpha counts
and SHA-256 hashes. [prompts.json](prompts.json) records the accepted artwork
prompts, intended canvas sizes and design references.


On import into ForgePact (2026-10-12), two PNGs (`pocket_elf.png`,
`witch_broomstick_ground.png`) were newer than this manifest, so
`manifest.json` here was recomputed from the files as shipped (hashes, pixel
counts, bounds). Sizes and the 0/255-only alpha were re-checked then.
