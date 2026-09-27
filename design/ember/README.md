# Ember Forge artwork sources

`reference.png` is Falor's approved AI-generated UI concept, supplied on
2026-09-27 (1586 x 992). `materials.png` is its generated clean material atlas
(1585 x 992): no UI controls or labels, retaining the scene, sidebar and frames.

These full-size development images are not included in the runtime bundle.
The panel serves only the crops under `panel/src/ember/assets/`, bundled by Vite with the frontend.
All control labels, setting values, status indicators and actions are real DOM.
Brand artwork includes its original ForgePact lettering.

Rebuild from the repository root with Node and `@napi-rs/canvas` available:

```powershell
node tools/pack_ember_relief.cjs design/ember/reference.png design/ember/materials.png
```

Optionally set `FORGEPACT_CANVAS_MODULE` to an existing absolute module path.
No Node dependency is needed to run ForgePact itself. The crop recipe scales
source coordinates to the approved 1586 x 992 layout before WebP encoding.
`action.webp` contains an original button crop, but CSS samples its border
only (no center fill). Do not display it as a background image: that would
introduce baked lettering beneath the actual button label.

The material atlas was produced with the built-in image-generation tool using
the approved reference as its sole image input and this prompt:

> Edit this exact ForgePact UI reference into a CLEAN EMPTY UI MATERIAL ATLAS for actual software implementation. Preserve the exact layout, camera, dark gritty pixel-detailed fantasy materials, bevels, copper edges, stone relief and especially the hanging chains, torn red banner on the left sidebar and forge illustration. REMOVE ALL text and ALL controls from the entire image: logos, words, headings, numbers, navigation icons, sliders, switches, buttons, item icons, status dots, stars, pentagram and checkmarks. Fill removed areas with the same existing dark iron/stone surface without visible patches. Preserve the empty copper-outlined panel frames (the two large panels side by side, full-width slim panel below, full-width footer panel), sidebar stone structure and its banner/chains, top bar and forge hero background. No new panels, no redesign, keep the exact panel positions from the reference. This is texture artwork only; there must be ZERO lettering, symbols, UI pictograms, switches or sliders anywhere. The decorative diamond rune on the physical forge anvil and cloth banners may remain because they are part of the scenery. Same wide aspect ratio as reference. High fidelity, dark and restrained, never cartoon, no plastic smoothness.

The serif font is IM Fell English by Igino Marini from Google Fonts, converted
to WOFF2 with fontTools (194,992 to 93,544 bytes). All 372 glyph outlines,
horizontal metrics and character mappings are unchanged. Its OFL notice ships
at `panel/src/ember/assets/OFL-IMFellEnglish.txt`; see `CREDITS.md`.
