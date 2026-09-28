// Slice approved/generated artwork into reusable UI skins. No baked controls.
const fs = require("fs"),
  path = require("path");
const { createCanvas, loadImage } = require(
  process.env.FORGEPACT_CANVAS_MODULE || "@napi-rs/canvas",
);
async function main() {
  const [reference, materials] = process.argv.slice(2);
  if (!reference || !materials)
    throw Error(
      "Usage: node tools/pack_ember_relief.cjs <reference.png> <empty-material-atlas.png>",
    );
  const ref = await loadImage(reference),
    clean = await loadImage(materials);
  const out = path.join(__dirname, "../panel/src/ember/assets");
  function pack(image, name, x, y, width, height) {
    const canvas = createCanvas(width, height),
      ctx = canvas.getContext("2d");
    ctx.drawImage(
      image,
      (x * image.width) / 1586,
      (y * image.height) / 992,
      (width * image.width) / 1586,
      (height * image.height) / 992,
      0,
      0,
      width,
      height,
    );
    fs.writeFileSync(
      path.join(out, name + ".webp"),
      canvas.toBuffer("image/webp", 94),
    );
  }
  for (const r of [
    ["sidebar", 0, 0, 270, 992],
    ["forge", 272, 50, 1314, 364],
    ["panel", 284, 412, 772, 332],
    ["iron", 430, 440, 256, 256],
    ["footer", 285, 882, 1283, 94],
  ])
    pack(clean, ...r);
  for (const r of [
    ["brand", 30, 20, 228, 185],
    ["anvil", 305, 425, 52, 40],
    ["skull", 316, 489, 62, 69],
    ["gem", 316, 574, 64, 69],
    ["ore", 310, 659, 77, 73],
    ["map", 1093, 487, 62, 66],
    ["banner", 1093, 573, 59, 67],
    ["paw", 1095, 660, 65, 72],
    ["rune", 303, 768, 84, 82],
    ["action", 1276, 895, 280, 70],
    ["nav-overview", 33, 241, 38, 37],
    ["nav-modifiers", 33, 310, 38, 38],
    ["nav-world", 33, 370, 38, 39],
    ["nav-loot", 33, 432, 38, 39],
    ["nav-mods", 33, 495, 39, 41],
    ["nav-setup", 33, 669, 39, 38],
    ["nav-help", 33, 724, 39, 38],
  ])
    pack(ref, ...r);
  console.log(
    JSON.stringify({
      reference: [ref.width, ref.height],
      materials: [clean.width, clean.height],
      files: fs
        .readdirSync(out)
        .filter((x) => x.endsWith(".webp"))
        .map((name) => ({
          name,
          bytes: fs.statSync(path.join(out, name)).size,
        })),
    }),
  );
}
main().catch((e) => {
  console.error(e);
  process.exitCode = 1;
});
