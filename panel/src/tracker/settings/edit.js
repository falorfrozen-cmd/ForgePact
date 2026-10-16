// The Alerts section's edits to the tracker's settings document (#185).
//
// AlertSettings.svelte never changes the document it is showing. Each control
// hands one of these functions to `client.updateSettings`, which GETs the
// whole document, applies the edit to that fresh copy and POSTs the whole of
// it back (client.svelte.js's readModifyWrite). So an edit made here sees
// whatever another window wrote a moment ago, and every field it does not
// name, unknown ones included, goes back as it came. The edits change the
// copy they are given in place and answer it (or, for the ones the page needs
// a result from, answer that).
//
// The fields and their meaning are the store's (forgepact_tracker/store.py,
// default_settings and _CHECKS) and the ledger's (prefs_from_settings):
// - `<rarity>`, `zone`: `{enabled, volume}`, a sound's switch and loudness;
// - `alerts`: the rarities worth an alert at all; with `min_tier` (0 any, 1 D
//   to 6 SS) the ledger's rule for which drops announce;
// - `zone_buffs`: buff ids; empty means every rotation alerts;
// - `notable`: `{label, names}` groups the Session section counts;
// - `filters`, `filter`, `use_filter`: the sound lists. The active filter is
//   the one `filter` names; each of its lists names items whose drop plays the
//   list's own sound (`list-<id>` in the sound store) whatever their rarity.
//
// A rarity's row has one switch, as in HS Offline Tracker's Alerts page (MIT,
// Q7(b); the notice is in CREDITS.md): it writes both the rarity's place in
// `alerts` and its sound's `enabled`, so "does Heroic alert" is asked once.
//
// Pure: no DOM, no runes. tests/tracker-settings.test.js runs it under node.

export const RARITY_KEYS = ['satanic', 'set', 'heroic', 'angelic', 'unholy'];
export const RARITY_NAMES = { satanic: 'Satanic', set: 'Set', heroic: 'Heroic', angelic: 'Angelic', unholy: 'Unholy' };
export const ALERT_KEYS = [...RARITY_KEYS, 'zone'];
export const GRADE_OPTIONS = [[0, 'Any'], [1, 'D'], [2, 'C'], [3, 'B'], [4, 'A'], [5, 'S'], [6, 'SS']];
// The store's default_settings() volume, and the Tracker's for a new list.
export const DEFAULT_VOLUME = 0.5;
export const LIST_VOLUME = 0.7;

const isRarity = (key) => RARITY_KEYS.includes(key);
const clamp01 = (v) => Math.min(1, Math.max(0, v));
const hundredths = (v) => Math.round(clamp01(Number(v) || 0) * 100) / 100;

function sound(doc, key) {
  const current = doc[key];
  return current && typeof current === 'object' && !Array.isArray(current) ? current : { enabled: true, volume: DEFAULT_VOLUME };
}

/** Whether a row's switch reads on: a rarity alerts and its sound is enabled; the zone by its own switch. */
export function alertOn(doc, key) {
  if (!doc) return false;
  const enabled = sound(doc, key).enabled !== false;
  if (!isRarity(key)) return enabled;
  return enabled && Array.isArray(doc.alerts) && doc.alerts.includes(RARITY_NAMES[key]);
}

/** Switch a row: `<key>.enabled`, and for a rarity its place in `alerts` (kept in the rarities' order). */
export function setAlertOn(doc, key, on) {
  doc[key] = { ...sound(doc, key), enabled: !!on };
  if (isRarity(key)) {
    const name = RARITY_NAMES[key];
    const was = Array.isArray(doc.alerts) ? doc.alerts : [];
    const known = new Set(was.filter((r) => r !== name));
    if (on) known.add(name);
    const ordered = RARITY_KEYS.map((k) => RARITY_NAMES[k]).filter((r) => known.has(r));
    doc.alerts = [...ordered, ...was.filter((r) => !ordered.includes(r) && r !== name)];
  }
  return doc;
}

/** A sound's volume, 0 to 1 in hundredths. */
export function setVolume(doc, key, volume) {
  doc[key] = { ...sound(doc, key), volume: hundredths(volume) };
  return doc;
}

/** A `{volume}` as the whole percentage a row shows; the store's default when it has none. */
export function volumePercent(entry) {
  const v = Number.isFinite(entry?.volume) ? entry.volume : DEFAULT_VOLUME;
  return Math.round(clamp01(v) * 100);
}

/** The lowest grade that alerts: 0 (any) to 6 (SS). */
export function setMinTier(doc, tier) {
  const n = Math.round(Number(tier));
  doc.min_tier = Number.isFinite(n) ? Math.min(6, Math.max(0, n)) : 0;
  return doc;
}

/** Pick or unpick a buff id for the zone alert; `zone_buffs` stays sorted and unique. */
export function toggleBuff(doc, id, on) {
  const n = Number(id);
  if (!Number.isInteger(n) || n < 0 || n > 255) throw new Error(`not a buff id: ${id}`);
  const picked = new Set(Array.isArray(doc.zone_buffs) ? doc.zone_buffs : []);
  if (on) picked.add(n); else picked.delete(n);
  doc.zone_buffs = [...picked].sort((a, b) => a - b);
  return doc;
}

/** Every rotation alerts again. */
export function clearBuffs(doc) {
  doc.zone_buffs = [];
  return doc;
}

/** Names typed as one line: split on commas and new lines, trimmed, blanks and repeats (any case) dropped. */
export function parseNames(text) {
  const seen = new Set();
  const out = [];
  for (const part of String(text ?? '').split(/[,\n]/)) {
    const name = part.trim();
    if (!name || seen.has(name.toLowerCase())) continue;
    seen.add(name.toLowerCase());
    out.push(name);
  }
  return out;
}

/** Add a notable group; throws a sentence the page can show when it cannot. */
export function addNotable(doc, label, namesText) {
  const name = String(label ?? '').trim();
  if (!name) throw new Error('Name the group first.');
  const names = parseNames(namesText);
  if (!names.length) throw new Error('Give the group at least one item name.');
  const groups = Array.isArray(doc.notable) ? doc.notable : [];
  if (groups.some((g) => String(g?.label).toLowerCase() === name.toLowerCase())) {
    throw new Error(`There is already a group called ${name}.`);
  }
  doc.notable = [...groups, { label: name, names }];
  return doc;
}

/** Remove the notable group with this label. */
export function removeNotable(doc, label) {
  doc.notable = (Array.isArray(doc.notable) ? doc.notable : []).filter((g) => g?.label !== label);
  return doc;
}

// ---- The sound lists -------------------------------------------------------

const ID_CHARS = 'abcdefghijklmnopqrstuvwxyz0123456789';

/** A short id the sound store files (`[A-Za-z0-9_-]{1,64}`) that `taken` does not hold. */
export function makeId(taken = new Set()) {
  for (;;) {
    let id = '';
    for (let i = 0; i < 8; i++) id += ID_CHARS[Math.floor(Math.random() * ID_CHARS.length)];
    if (!taken.has(id)) return id;
  }
}

function filtersOf(doc) {
  if (!Array.isArray(doc.filters)) doc.filters = [];
  return doc.filters;
}

function takenIds(doc) {
  const ids = new Set();
  for (const f of filtersOf(doc)) {
    ids.add(f?.id);
    for (const l of f?.lists ?? []) ids.add(l?.id);
  }
  return ids;
}

function filterIn(doc, filterId) {
  const found = filtersOf(doc).find((f) => f?.id === filterId);
  if (!found) throw new Error('That filter is gone. Pick another.');
  if (!Array.isArray(found.lists)) found.lists = [];
  return found;
}

function listIn(doc, filterId, listId) {
  const found = filterIn(doc, filterId).lists.find((l) => l?.id === listId);
  if (!found) throw new Error('That list is gone. Pick another.');
  if (!Array.isArray(found.items)) found.items = [];
  return found;
}

/** The filter `filter` names, or null: the ledger uses no other. */
export function activeFilter(doc) {
  if (!doc || !Array.isArray(doc.filters)) return null;
  return doc.filters.find((f) => f?.id === doc.filter) ?? null;
}

export function selectFilter(doc, filterId) {
  doc.filter = filterId;
  return doc;
}

function newList(doc, n) {
  return { id: makeId(takenIds(doc)), name: `List ${n}`, enabled: true, volume: LIST_VOLUME, items: [] };
}

/** A new filter with one empty list, made the active one. Answers it. */
export function addFilter(doc, name) {
  const filters = filtersOf(doc);
  const id = makeId(takenIds(doc));
  const made = { id, name: String(name ?? '').trim() || `Filter ${filters.length + 1}`, lists: [] };
  filters.push(made);
  made.lists.push(newList(doc, 1));
  doc.filter = id;
  return made;
}

/** Delete a filter. Answers the sound keys of its lists, for the page to clear. */
export function removeFilter(doc, filterId) {
  const gone = filterIn(doc, filterId);
  doc.filters = filtersOf(doc).filter((f) => f !== gone);
  if (doc.filter === filterId || !doc.filters.some((f) => f?.id === doc.filter)) doc.filter = doc.filters[0]?.id ?? '';
  return gone.lists.map((l) => `list-${l.id}`);
}

/**
 * Copy a filter under fresh ids, made the active one. Answers its id and the
 * `[from, to]` sound keys to copy, since a list's sound is filed by its id.
 */
export function duplicateFilter(doc, filterId) {
  const source = filterIn(doc, filterId);
  const taken = takenIds(doc);
  const id = makeId(taken);
  taken.add(id);
  const pairs = [];
  const lists = source.lists.map((l) => {
    const copy = structuredClone(l);
    copy.id = makeId(taken);
    taken.add(copy.id);
    pairs.push([`list-${l.id}`, `list-${copy.id}`]);
    return copy;
  });
  filtersOf(doc).push({ ...structuredClone(source), id, name: `${source.name ?? 'Filter'} copy`, lists });
  doc.filter = id;
  return { id, pairs };
}

export function renameFilter(doc, filterId, name) {
  const text = String(name ?? '').trim();
  if (text) filterIn(doc, filterId).name = text;
  return doc;
}

/** Add an empty list to a filter. Answers its id. */
export function addList(doc, filterId) {
  const filter = filterIn(doc, filterId);
  const made = newList(doc, filter.lists.length + 1);
  filter.lists.push(made);
  return made.id;
}

/** Remove a list. Answers its sound key, for the page to clear. */
export function removeList(doc, filterId, listId) {
  const filter = filterIn(doc, filterId);
  listIn(doc, filterId, listId);
  filter.lists = filter.lists.filter((l) => l?.id !== listId);
  return `list-${listId}`;
}

/** Move a list one place earlier (-1) or later (1): the first list that names an item wins. */
export function moveList(doc, filterId, listId, step) {
  const lists = filterIn(doc, filterId).lists;
  const from = lists.findIndex((l) => l?.id === listId);
  const to = from + step;
  if (from < 0 || to < 0 || to >= lists.length) return false;
  [lists[from], lists[to]] = [lists[to], lists[from]];
  return true;
}

/** A list's `name`, `enabled` or `volume`. */
export function setListField(doc, filterId, listId, field, value) {
  const list = listIn(doc, filterId, listId);
  if (field === 'name') {
    const text = String(value ?? '').trim();
    if (text) list.name = text;
  } else if (field === 'enabled') {
    list.enabled = !!value;
  } else if (field === 'volume') {
    list.volume = hundredths(value);
  } else {
    throw new Error(`not a list field: ${field}`);
  }
  return doc;
}

/** Add an item name to a list; false when it is blank or already there (any case). */
export function addItem(doc, filterId, listId, name) {
  const text = String(name ?? '').trim();
  const list = listIn(doc, filterId, listId);
  if (!text || list.items.some((n) => String(n).toLowerCase() === text.toLowerCase())) return false;
  list.items.push(text);
  return true;
}

export function removeItem(doc, filterId, listId, name) {
  const list = listIn(doc, filterId, listId);
  list.items = list.items.filter((n) => n !== name);
  return doc;
}

/** The names on more than one of a filter's lists (lower case), with the lists' names. Only the first list's sound plays. */
export function clashes(filter) {
  const owners = new Map();
  for (const list of filter?.lists ?? []) {
    for (const name of list?.items ?? []) {
      const key = String(name).toLowerCase();
      const names = owners.get(key) ?? [];
      if (!names.includes(list.name)) names.push(list.name);
      owners.set(key, names);
    }
  }
  return new Map([...owners].filter(([, names]) => names.length > 1));
}
