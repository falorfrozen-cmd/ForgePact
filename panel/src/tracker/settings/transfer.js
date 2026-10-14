// Sound files and the sound-filter export (#185), for the Alerts section.
//
// A filter travels as HS Offline Tracker's own export (MIT, Q7(b); its
// `export_filter`/`import_filter`): `{app, version: 1, name, lists: [{name,
// enabled, volume, items, sound}]}`, where `sound` is `{ext, data}` with the
// list's custom file in base64, or null. ForgePact writes the Tracker's `app`
// name, so a file goes either way between the two, and reads the original
// project's older name and its own as well. Ids never travel: an imported
// filter and its lists are given fresh ones, so it cannot fight with one
// already here, and each inlined sound is filed under its list's new id.
//
// Pure: no DOM, no runes. tests/tracker-settings.test.js runs it under node.

import { LIST_VOLUME, makeId } from './edit.js';

export const EXPORT_APP = 'hs-offline-tracker';
const READABLE_APPS = new Set([EXPORT_APP, 'hs-tracker', 'forgepact']);
export const EXPORT_SUFFIX = 'hsofflinetracker.json';
// The kinds the sound store files (store.py's SOUND_TYPES), in the order it prefers them.
export const SOUND_EXTS = ['mp3', 'wav', 'ogg', 'flac'];
export const SOUND_ACCEPT = '.wav,.mp3,.ogg,.flac';

/** A file name's sound kind (`wav`, `mp3`, `ogg`, `flac`), or null. */
export function soundExt(fileName) {
  const m = /\.([A-Za-z0-9]+)$/.exec(String(fileName ?? ''));
  const ext = m ? m[1].toLowerCase() : null;
  return ext && SOUND_EXTS.includes(ext) ? ext : null;
}

/** What a row names as its sound: `<key>.<ext>` when the player filed one, else null (the bundled one). */
export function soundSource(key, sounds) {
  const entry = sounds?.[key];
  return entry?.ext ? `${key}.${entry.ext}` : null;
}

export function bytesToBase64(bytes) {
  let text = '';
  const CHUNK = 0x8000;
  for (let i = 0; i < bytes.length; i += CHUNK) text += String.fromCharCode(...bytes.subarray(i, i + CHUNK));
  return btoa(text);
}

export function base64ToBytes(text) {
  const raw = atob(text);
  const out = new Uint8Array(raw.length);
  for (let i = 0; i < raw.length; i++) out[i] = raw.charCodeAt(i);
  return out;
}

/** A chosen File as `POST /api/tracker/sound/<key>`'s body: `{name, data}`, the bytes in base64. */
export async function fileUpload(file) {
  return { name: file.name, data: bytesToBase64(new Uint8Array(await file.arrayBuffer())) };
}

/** The file name the Tracker suggested for an export: the filter's name, made safe. */
export function exportFileName(name) {
  const safe = String(name ?? 'filter').replace(/[^\p{L}\p{N} -]/gu, '-').replace(/\s+/g, ' ').trim() || 'filter';
  return `${safe}.${EXPORT_SUFFIX}`;
}

/** A filter as the export carries it. `soundsByListId` maps a list's id to its `{ext, data}`. */
export function exportFilter(filter, soundsByListId = {}) {
  return {
    app: EXPORT_APP,
    version: 1,
    name: String(filter?.name ?? ''),
    lists: (filter?.lists ?? []).map((l) => ({
      name: String(l?.name ?? ''),
      enabled: l?.enabled !== false,
      volume: Number.isFinite(l?.volume) ? l.volume : LIST_VOLUME,
      items: Array.isArray(l?.items) ? [...l.items] : [],
      sound: soundsByListId[l?.id] ?? null,
    })),
  };
}

const NOT_A_FILTER = 'That is not a sound filter exported from ForgePact or HS Offline Tracker.';

/** Read an export's text: `{name, lists}` with every field checked, or throws a sentence the page can show. */
export function parseFilterExport(text) {
  let doc;
  try { doc = JSON.parse(text); } catch { throw new Error(NOT_A_FILTER); }
  if (!doc || typeof doc !== 'object' || !READABLE_APPS.has(doc.app) || !Array.isArray(doc.lists)) throw new Error(NOT_A_FILTER);
  const lists = doc.lists.map((l) => {
    if (!l || typeof l !== 'object' || typeof (l.name ?? '') !== 'string') throw new Error(NOT_A_FILTER);
    const items = l.items ?? [];
    if (!Array.isArray(items) || !items.every((n) => typeof n === 'string')) throw new Error(NOT_A_FILTER);
    const s = l.sound;
    const sound = s && typeof s === 'object' && SOUND_EXTS.includes(s.ext) && typeof s.data === 'string' && s.data
      ? { ext: s.ext, data: s.data } : null;
    const volume = Number.isFinite(l.volume) ? Math.min(1, Math.max(0, l.volume)) : LIST_VOLUME;
    return { name: l.name ?? '', enabled: l.enabled !== false, volume, items: [...items], sound };
  });
  return { name: typeof doc.name === 'string' && doc.name.trim() ? doc.name.trim() : 'Imported filter', lists };
}

/**
 * Add a parsed export to the document under fresh ids, made the active
 * filter. Answers its id and the sounds to file: `[{key, ext, data}]`.
 */
export function importFilter(doc, parsed) {
  if (!Array.isArray(doc.filters)) doc.filters = [];
  const taken = new Set();
  for (const f of doc.filters) {
    taken.add(f?.id);
    for (const l of f?.lists ?? []) taken.add(l?.id);
  }
  const id = makeId(taken);
  taken.add(id);
  const sounds = [];
  const lists = parsed.lists.map((l, i) => {
    const listId = makeId(taken);
    taken.add(listId);
    if (l.sound) sounds.push({ key: `list-${listId}`, ext: l.sound.ext, data: l.sound.data });
    return { id: listId, name: l.name || `List ${i + 1}`, enabled: l.enabled, volume: l.volume, items: [...l.items] };
  });
  doc.filters.push({ id, name: parsed.name, lists });
  doc.filter = id;
  return { id, sounds };
}
