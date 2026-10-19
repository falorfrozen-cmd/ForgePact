// The Tracker tab's numbers, said the same way everywhere (#185). Ported from
// HS Offline Tracker's src/format.js (MIT, Copyright (c) 2026 falorfrozen-cmd
// and HS Offline Tracker contributors; the notice is in CREDITS.md), with the
// rarity classes renamed to the panel's (`.r-satanic` ..., panel/DESIGN.md §
// Tracker). Pure: no DOM, no state, so node runs it as it is.
//
// Hero Siege says kk and kkk rather than M and B, which is why these are not
// the SI suffixes a general-purpose helper would reach for.

/** 1234 -> "1,234"; 12345 -> "12.3k"; 1234567 -> "1.23kk". */
export function fmt(n) {
  const v = n ?? 0;
  const abs = Math.abs(v);
  if (abs >= 1e9) return `${(v / 1e9).toFixed(2)}kkk`;
  if (abs >= 1e6) return `${(v / 1e6).toFixed(2)}kk`;
  // Below ten thousand the digits still fit, and reading them exactly is
  // worth more than the two characters saved.
  if (abs >= 10_000) return `${(v / 1e3).toFixed(1)}k`;
  return v.toLocaleString('en-US');
}

/** A per-hour rate: 14570000 -> "14.57kk/h". */
export function rate(n) {
  return `${fmt(Math.round(n ?? 0))}/h`;
}

/** Seconds as a running clock: 3661 -> "1:01:01". Hours never wrap. */
export function clock(secs) {
  const s = Math.max(0, Math.floor(secs ?? 0));
  const h = Math.floor(s / 3600);
  const m = Math.floor((s % 3600) / 60);
  return `${h}:${String(m).padStart(2, '0')}:${String(s % 60).padStart(2, '0')}`;
}

/** Seconds as a length of time, for reading rather than watching: "1h 24m". */
export function span(secs) {
  const s = Math.max(0, Math.floor(secs ?? 0));
  // Rounding the remainder gives "60m" for anything from 59m30s: round the
  // whole thing into minutes first, then split, and an hour stays an hour.
  const mins = Math.round(s / 60);
  const h = Math.floor(mins / 60);
  const m = mins % 60;
  if (h && m) return `${h}h ${m}m`;
  if (h) return `${h}h`;
  return `${m}m`;
}

/** The five rarities the tracker counts, in the order every section lists them. */
export const RARITIES = ['Satanic', 'Set', 'Heroic', 'Angelic', 'Unholy'];

/** The class each rarity's name is drawn with (text only, never a fill). */
export const RARITY_CLASS = {
  Satanic: 'r-satanic',
  Set: 'r-set',
  Heroic: 'r-heroic',
  Angelic: 'r-angelic',
  Unholy: 'r-unholy',
};

/** The grades a drop is given, lowest first. */
export const GRADES = ['D', 'C', 'B', 'A', 'S', 'SS'];

/**
 * What the game calls the difficulty a character is on. The Season 10 client
 * ships these four labels in this order, and its save gate accepts 0..3;
 * anything else stays explicit (`D4`) rather than given a plausible name.
 */
export function difficulty(n, hellSub = 0) {
  if (n == null) return null;
  const name = ['Normal', 'Nightmare', 'Hell', 'Inferno'][n] ?? `D${n}`;
  // Hell is five difficulties wearing one name, and the game says which in a
  // field of its own; only shown on Hell, where it means something.
  return name === 'Hell' && hellSub >= 1 && hellSub <= 5 ? `Hell ${hellSub}` : name;
}
