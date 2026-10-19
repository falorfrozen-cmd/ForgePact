// The item codex's rows (#185): the search, the filters, the two orders, and
// how a chance, a grade and a place are written. Ported from HS Offline
// Tracker's src/Codex.svelte (MIT, Copyright (c) 2026 falorfrozen-cmd and HS
// Offline Tracker contributors; the notice is in CREDITS.md), where the rows
// were built in the page from its own tables; here `/api/tracker/codex`
// builds them (src/forgepact_tracker/codex.py) and this module only narrows
// and orders what it answered. Pure: no DOM, no state, so
// tests/tracker-codex.test.js runs it under plain node.
//
// A row is `{name, rarity, tier, type, group, rate, chase, zones, places}`.
// `rate` is the general "one in N" anywhere, `chase` the better one inside the
// places the item is tied to (the number the game's tooltip prints in green);
// 0 where the tables cannot price it.

import { GRADES, RARITIES, RARITY_CLASS } from '../format.js';

/** How many rows the page draws at once; the rest wait behind a narrower search. */
export const SHOWN = 60;

const NAME_LOCALE = 'en';
const byName = (a, b) => String(a.name).localeCompare(String(b.name), NAME_LOCALE);

/** Whether a row passes the name search and the rarity and kind filters. */
export function matches(row, { query = '', rarity = '', group = '' } = {}) {
  const q = String(query ?? '').trim().toLowerCase();
  return (!q || String(row.name).toLowerCase().includes(q)) &&
    (!rarity || row.rarity === rarity) &&
    (!group || row.group === group);
}

/**
 * A sorted copy. `name`: alphabetical. Anything else, rarest first: the
 * longest odds lead, what the tables cannot price goes last, and equal odds
 * fall back to the name, so the order never depends on the answer's.
 */
export function sortRows(rows, sort = 'rarest') {
  const list = [...(rows ?? [])];
  if (sort === 'name') return list.sort(byName);
  return list.sort((a, b) => (b.rate > 0) - (a.rate > 0) || (b.rate || 0) - (a.rate || 0) || byName(a, b));
}

/** The rows that pass `matches`, in the order `sort` asks for. */
export function findRows(rows, { query = '', rarity = '', group = '', sort = 'rarest' } = {}) {
  return sortRows((rows ?? []).filter((r) => matches(r, { query, rarity, group })), sort);
}

/**
 * The rarities the rows carry: the five the tab counts in its own order, then
 * any other (Common) by name. Offering one the rows lack would only ever find
 * nothing.
 */
export function rarityOptions(rows) {
  const present = new Set((rows ?? []).map((r) => r.rarity).filter(Boolean));
  const known = RARITIES.filter((r) => present.has(r));
  const rest = [...present].filter((r) => !RARITIES.includes(r)).sort((a, b) => a.localeCompare(b, NAME_LOCALE));
  return [...known, ...rest];
}

/** The kinds (item types) the rows carry, by name. */
export function groupOptions(rows) {
  return [...new Set((rows ?? []).map((r) => r.group).filter(Boolean))].sort((a, b) => a.localeCompare(b, NAME_LOCALE));
}

/** A chance as "1 in N" with the digits grouped: 48200 -> "1/48,200"; none is a dash. */
export function odds(n) {
  return n > 0 ? `1/${Math.round(n).toLocaleString('en-US')}` : '—';
}

/** The game's grade letter for a tier, D to SS; one the letters lack says its number. */
export function gradeLabel(tier) {
  if (!(tier > 0)) return '—';
  return GRADES[tier - 1] ?? `T${tier}`;
}

/** Where it drops: its bosses and chests in words, else its zone codes, else anywhere. */
export function whereLabel(row) {
  const places = (row?.places ?? []).filter(Boolean);
  if (places.length) return places.join(' · ');
  const zones = (row?.zones ?? []).filter(Boolean);
  if (zones.length) return zones.join(', ');
  return 'anywhere';
}

/** The class a rarity's name and word are drawn with; none for a rarity the panel does not colour. */
export function rarityClass(rarity) {
  return RARITY_CLASS[rarity] ?? '';
}
