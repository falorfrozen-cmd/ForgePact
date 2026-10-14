// The Tracker tab's Session section (#185), its pure half: what each card
// shows, worked out from `/api/tracker`'s snapshot and `/api/tracker/extra`.
// Session.svelte draws these and nothing else; tests/tracker-session.test.js
// runs them under plain node, and tests/tracker/session.check.mjs uses them to
// say what the page must show for the values the API answered. No DOM, no
// state, no runes.
//
// The meanings are the ledger's (src/forgepact_tracker/ledger.py, `snapshot()`
// and `extra()`): `earned` is what this run earned, `total` the character's
// lifetime count from its save (none for gold, which is not read offline),
// `per_hour` the rate over the run's active time; `ss` the SS-grade gear this
// run dropped; `tallies` the bosses and chests the save counted since the run
// began; `series` one point a minute of play; `drops` the journal, newest
// first.

import { RARITIES, RARITY_CLASS, GRADES, fmt, rate, clock, difficulty } from '../format.js';

/** Which of the section's states to draw. */
export function sessionState({ snapshot = null, session = null } = {}) {
  if (!snapshot) return 'loading';
  // A session file read, live or finished, or a character the save named: there is something to show.
  if (session?.file || snapshot.character) return 'session';
  return 'none';
}

/** The character's line under its name: "Level 100 · Hero 300 · Hell 5 · Season 10". */
export function characterLine(c) {
  if (!c) return '';
  const parts = [];
  // A level of 0 is the sensor naming the character before its save is read:
  // nothing in it is known yet, the difficulty included.
  if (c.level > 0) {
    parts.push(`Level ${c.level}`);
    if (c.herolevel > 0) parts.push(`Hero ${c.herolevel}`);
    const diff = difficulty(c.difficulty, c.hell_sub);
    if (diff) parts.push(diff);
  }
  if (c.season > 0) parts.push(`Season ${c.season}`);
  if (c.hardcore) parts.push('Hardcore');
  return parts.join(' · ');
}

/** The run's drops of the five rarities, and their rate. */
export function dropTotals(snap) {
  let total = 0;
  let perHour = 0;
  for (const name of RARITIES) {
    total += snap?.items?.[name]?.total ?? 0;
    perHour += snap?.items?.[name]?.per_hour ?? 0;
  }
  return { total, per_hour: perHour };
}

/**
 * The four totals across the session card: what the run earned, its rate,
 * and a note (the lifetime count, where the save gives one).
 */
export function totals(snap) {
  const line = (key) => snap?.[key] ?? { total: null, earned: 0, per_hour: 0 };
  const gold = line('gold');
  const xp = line('xp');
  const kills = line('kills');
  const drops = dropTotals(snap);
  return [
    { key: 'gold', label: 'Gold', earned: gold.earned ?? 0, per_hour: gold.per_hour ?? 0, total: gold.total ?? null,
      note: gold.total == null ? 'bank not read offline' : `${fmt(gold.total)} banked` },
    { key: 'xp', label: 'Experience', earned: xp.earned ?? 0, per_hour: xp.per_hour ?? 0, total: xp.total ?? null,
      note: xp.total == null ? 'total not read yet' : `${fmt(xp.total)} total` },
    { key: 'kills', label: 'Kills', earned: kills.earned ?? 0, per_hour: kills.per_hour ?? 0, total: kills.total ?? null,
      note: kills.total == null ? 'total not read yet' : `${fmt(kills.total)} total` },
    { key: 'drops', label: 'Drops', earned: drops.total, per_hour: drops.per_hour, total: null, note: 'the five rarities' },
  ].map((t) => ({ ...t, value: fmt(t.earned), rate: rate(t.per_hour) }));
}

/** The loot table: the five rarities in the panel's order. */
export function lootRows(snap) {
  return RARITIES.map((name) => {
    const it = snap?.items?.[name] ?? {};
    return { name, cls: RARITY_CLASS[name], total: it.total ?? 0, mf: it.mf ?? 0, per_hour: it.per_hour ?? 0 };
  });
}

/** Notable counts: SS-grade gear first (the grade the ledger counts), then the named groups. */
export function notableRows(snap) {
  const rows = [{ label: 'SS-grade gear', total: snap?.ss ?? 0 }];
  for (const n of snap?.notable ?? []) {
    if (n && typeof n.label === 'string') rows.push({ label: n.label, total: n.total ?? 0 });
  }
  return rows;
}

/** The save's tallies, split into bosses and chests in the order they came. */
export function tallyGroups(tallies) {
  const list = Array.isArray(tallies) ? tallies : [];
  return {
    bosses: list.filter((t) => t.group === 'boss'),
    chests: list.filter((t) => t.group === 'chest'),
  };
}

// The act zones' names, by act and then by the zone's number in it. Ported
// from HS Offline Tracker's src/buffs.js (MIT, Copyright (c) 2026
// falorfrozen-cmd and HS Offline Tracker contributors; the notice is in
// CREDITS.md).
export const ZONES = {
  1: ['Outskirts of Inoya', 'Fields of Battle', 'The Pumpkin Patch', 'Woodhill Plains', "King's Garden", 'Witching River'],
  2: ['Crystal Village', 'Chilling Lake', 'Arctic Tundra', 'Snowy Mountains', 'The Glacial Trail'],
  3: ['Corrupted Oasis', 'Dry Hills', "Mos'Arathim Desert", 'Pyramid Level 1', 'Pyramid Level 2', 'Curacan Hollow'],
  4: ['Old Mining Village', 'The Highland Mines', 'Corrupted Cave', 'The Nightmare', "The Devil's Breach"],
  5: ['Mt. Fuji', 'Misty Swamp', 'Fuji Coast', 'Sea of Karponia', 'Temple of Zamjo'],
  6: ['Highland Graveyard', 'The Cathedral', 'Prison Dungeon', 'Steam Train', 'The Depths of Hell'],
  7: ['Deep Space', 'Event Horizon', 'The Black Hole', 'Parallel Dimension', 'Subconscious Mind', 'Shattered Realm'],
  8: ['Forest of the Slain', 'Flooded Plains', 'Forgotten Caves', 'Camp of Souls', 'Helheim'],
  9: ['Abyss Jungle', 'Shipwreck Cove', 'Tormented Reef', 'Boreal Island', 'Volcanic Island', 'Abyss Realm'],
};

/** "Satanic_1_3" -> "Act 1 · The Pumpkin Patch". A code it cannot read is shown as it came. */
export function zoneName(raw) {
  const text = String(raw ?? '');
  const m = /^(?:Satanic|SZ|Act)_(\d+)_(\d+)/i.exec(text);
  if (!m) return text;
  const act = Number(m[1]);
  const zone = Number(m[2]);
  const name = ZONES[act]?.[zone - 1];
  return name ? `Act ${act} · ${name}` : `Act ${act} · Zone ${zone}`;
}

/**
 * The zone's buffs or debuffs by name, from `/api/state`'s `satanicBuffs` or
 * `satanicDebuffs` (`[id, name, description]` rows). An id the list does not
 * have is still shown, by its number.
 */
export function modRows(ids, list, word = 'Buff') {
  const byId = new Map();
  for (const row of Array.isArray(list) ? list : []) {
    if (Array.isArray(row)) byId.set(Number(row[0]), row);
  }
  return (Array.isArray(ids) ? ids : []).map((id) => {
    const row = byId.get(Number(id));
    return row ? { id, name: String(row[1]), desc: String(row[2] ?? '') } : { id, name: `${word} ${id}`, desc: '' };
  });
}

/** When the zone was rolled, relative to now: "rolled 11 min ago". */
export function rolledAgo(atMs, nowMs) {
  if (!Number.isFinite(atMs) || !Number.isFinite(nowMs)) return '';
  const mins = Math.floor(Math.max(0, nowMs - atMs) / 60000);
  if (mins < 1) return 'rolled just now';
  if (mins < 60) return `rolled ${mins} min ago`;
  const hours = Math.floor(mins / 60);
  if (hours < 24) return `rolled ${hours} h ago`;
  return 'rolled in an earlier session';
}

// The most points a chart draws; a longer series is thinned evenly, its last
// point always kept.
export const CHART_MAX_POINTS = 240;
// The chart's own coordinates (the SVG's viewBox).
export const CHART_W = 100;
export const CHART_H = 40;

/**
 * One counter's chart from the series: the polyline's points in the viewBox,
 * starting from nothing earned at 0:00, the latest value and the time it
 * spans. `samples` is how many points the series held.
 */
export function chartPoints(series, key, max = CHART_MAX_POINTS) {
  const list = (Array.isArray(series) ? series : []).filter((p) => Number.isFinite(p?.t) && Number.isFinite(p?.[key]));
  let picked = list;
  if (list.length > max) {
    picked = [];
    const step = list.length / max;
    for (let i = 0; i < max - 1; i++) picked.push(list[Math.floor(i * step)]);
    picked.push(list[list.length - 1]);
  }
  const last = list.at(-1);
  const span = last ? last.t : 0;
  const top = Math.max(0, ...list.map((p) => p[key]));
  const x = (t) => (span > 0 ? (t / span) * CHART_W : 0);
  const y = (v) => (top > 0 ? CHART_H - (Math.max(0, v) / top) * CHART_H : CHART_H);
  const points = [[0, CHART_H], ...picked.map((p) => [x(p.t), y(p[key])])];
  return {
    samples: list.length,
    points,
    attr: points.map(([px, py]) => `${round2(px)},${round2(py)}`).join(' '),
    latest: last ? last[key] : 0,
    span,
  };
}

const round2 = (n) => Math.round(n * 100) / 100;

/** A drop's grade from its tier (1 = D .. 6 = SS); null when it has none. */
export function grade(tier) {
  return GRADES[(tier | 0) - 1] ?? null;
}

const TIME = new Intl.DateTimeFormat('en-GB', { hour: '2-digit', minute: '2-digit', second: '2-digit' });

/** A journal entry's time of day: "21:41:08". One formatter, built once. */
export function timeOfDay(ms) {
  return Number.isFinite(ms) ? TIME.format(ms) : '';
}

/** The journal's rows, in the order `/extra` gave them (newest first). */
export function journalRows(drops) {
  return (Array.isArray(drops) ? drops : []).map((d, index) => {
    const named = typeof d.name === 'string' && d.name.trim() !== '';
    const rarity = typeof d.rarity === 'string' && d.rarity ? d.rarity : 'Drop';
    return {
      key: typeof d.id === 'string' && d.id ? d.id : `${d.ts_ms}:${index}`,
      id: d.id ?? '',
      time: timeOfDay(d.ts_ms),
      grade: grade(d.tier),
      rarity,
      cls: RARITY_CLASS[rarity] ?? '',
      name: named ? d.name : 'A drop whose name the game did not give',
      named,
      mf: !!d.mf,
      alerted: !!d.announced,
    };
  });
}

/** The session clock as the card shows it. */
export function sessionClock(snap) {
  return clock(snap?.session_secs ?? 0);
}
