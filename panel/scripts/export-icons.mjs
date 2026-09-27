// Export the ForgePact SVG icon pack: one standalone SVG per icon, a gallery
// page and a README, from src/icons.js (what `py src/panel_icons.py <dir>`
// did before the panel moved here).
//
//   node scripts/export-icons.mjs <output-directory>

import { mkdirSync, writeFileSync } from 'node:fs';
import { join, resolve } from 'node:path';
import { ICONS, svg } from '../src/icons.js';

const escape = (s) => s.replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#x27;' }[c]));

export function exportPack(directory) {
  mkdirSync(directory, { recursive: true });
  const names = Object.keys(ICONS);
  for (const name of names) writeFileSync(join(directory, `${name}.svg`), svg(name));
  const cards = names.map((name) => `<figure><img src="${name}.svg" alt=""><figcaption>${escape(name)}</figcaption></figure>`).join('');
  const gallery = '<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>ForgePact Icon Pack</title><style>body{margin:40px;background:#100d0b;color:#eee2d2;font:14px Segoe UI,sans-serif}h1{font-size:28px}p{color:#b7a996}main{display:grid;grid-template-columns:repeat(auto-fit,minmax(135px,1fr));gap:12px}figure{margin:0;padding:20px 12px;background:#211b16;border:1px solid #43362a;border-radius:10px;text-align:center}img{width:48px;height:48px}figcaption{margin-top:12px;color:#e9c7a4;font-size:12px}</style><h1>ForgePact · Icon Pack</h1><p>Original SVG artwork · Falor · 32 × 32 viewBox · AGPL-3.0</p><main>' + cards + '</main></html>';
  writeFileSync(join(directory, 'index.html'), gallery);
  writeFileSync(join(directory, 'README.txt'), 'ForgePact icon pack\nOriginal UI artwork created for Falor\nLicense: AGPL-3.0, same as ForgePact (see the project LICENSE).\nNo game-extracted artwork, runtime libraries or remote requests.\nEvery SVG is self-contained. Recommended UI size: 24-28 px.\nKeep the visible setting name beside the icon.\nSource: ForgePact/panel/src/icons.js\n');
  return names.length;
}

const target = process.argv[2];
if (!target) {
  console.error('usage: node scripts/export-icons.mjs <output-directory>');
  process.exitCode = 2;
} else {
  const out = resolve(process.env.INIT_CWD || process.cwd(), target);
  console.log(`Exported ${exportPack(out)} icons to ${out}`);
}
