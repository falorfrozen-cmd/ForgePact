# Minimap pack icons

Seven original dark-fantasy sprites made with the built-in image generator.
The final PNGs are 16x16 RGBA with transparent backgrounds. Each kind has a
separate silhouette as well as its own colour. These mark unspawned **groups**,
not individual monsters, and are separate from the game's normal enemy dots.

| File | Shape | Palette |
| --- | --- | --- |
| normal.png | Skull | Ivory / bone |
| ambush.png | Hooded face | Charcoal / ivory / yellow eyes |
| ancient.png | Horned mask | Magenta / purple |
| champion.png | Crested helmet | Cyan / steel |
| colossal_chest.png | Treasure chest | Gold / warm wood |
| legion.png | Three skulls | Amber / orange |
| miniboss.png | Crowned demon skull | Crimson / gold |

Open `preview.html` to compare actual 16px, 24px and 32px display sizes on
light and dark map colours. This is an asset preview, not an in-game capture.

## Editing and rebuilding

The larger generated masters are in `masters/`; the complete generation prompts
are in `generation.json`. The 16x16 files are the reviewed production artwork.
Run `py -3 tools/make_packmark_icons.py` from the ForgePact root to validate all
seven final PNGs and embed their exact bytes in `PackMarkerIcons.hpp`. It also
copies them to `build/packmark-icons/`. This does not regenerate the artwork,
install a plugin or modify the game. The existing DLL accepts these external
PNGs without rebuilding; the updated header is for future plugin builds.

To install, replace the same seven filenames in the configured game's
`bin/bp_ipc/packmarks/` directory, keeping a backup. If the game is running,
`packmarks reload` reloads them on the next minimap draw. Otherwise they load
when the map is next drawn after launch. `packmarks iconscale 1.5` displays
16px assets at 24px before the game's HUD scale; `1` is the default.

The runtime deliberately preserves existing custom PNGs. Updating the compiled
defaults alone never replaces files already installed in that directory.
