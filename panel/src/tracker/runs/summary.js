// What a finished run says, in the Runs section's list, its picked run and the
// run card (#185). Ported from HS Offline Tracker's src/Runs.svelte and
// src/runcard.js (MIT, Copyright (c) 2026 falorfrozen-cmd and HS Offline
// Tracker contributors; the notice is in CREDITS.md), in the panel's words.
// Pure: no DOM, no state, so tests/tracker-runs.test.js runs it under node.
//
// A run is what `/api/tracker/runs` answers: one ForgePact filed (with an
// `id`), or one imported from the Tracker's own runs.json, which has no id
// and, from before the Tracker's 0.9.8, no tallies either. Both read the same
// here; a field a run lacks is left out, never shown as a zero it did not
// record.

import { fmt, difficulty, RARITIES, GRADES } from '../format.js';
import { ROOMS } from './rooms.js';

const MONTHS = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];

const num = (v) => (typeof v === 'number' && Number.isFinite(v) ? v : 0);

/** A run's identity: its id, else its own start and end (the Tracker's runs have no id). */
export function runKey(run) {
  if (typeof run?.id === 'string' && run.id) return run.id;
  return `${run?.started_ms}-${run?.ended_ms}`;
}

/** What the game calls a room, else Town, else its act and zone, else its asset name. */
export function zoneLabel(room) {
  if (typeof room !== 'string' || !room) return 'Somewhere';
  const known = ROOMS.get(room);
  if (known) return known;
  if (/^town/i.test(room)) return 'Town';
  const act = /^Act_(\d+)_(\d+)$/i.exec(room);
  if (act) return `Act ${Number(act[1])} · Zone ${Number(act[2])}`;
  return room.replace(/_rm$/i, '').replace(/_/g, ' ');
}

const two = (n) => String(n).padStart(2, '0');

const sameDay = (a, b) => a.getFullYear() === b.getFullYear() && a.getMonth() === b.getMonth() && a.getDate() === b.getDate();

/** When a run started, in local time: "Today, 20:39", "Yesterday, 22:15", "12 Oct, 19:40", "2 Jan 2025, 07:00". */
export function when(ms, now = Date.now()) {
  if (typeof ms !== 'number' || !Number.isFinite(ms)) return 'Unknown time';
  const at = new Date(ms);
  const today = new Date(now);
  const yesterday = new Date(today.getFullYear(), today.getMonth(), today.getDate() - 1);
  const time = `${two(at.getHours())}:${two(at.getMinutes())}`;
  if (sameDay(at, today)) return `Today, ${time}`;
  if (sameDay(at, yesterday)) return `Yesterday, ${time}`;
  const day = `${at.getDate()} ${MONTHS[at.getMonth()]}`;
  return at.getFullYear() === today.getFullYear() ? `${day}, ${time}` : `${day} ${at.getFullYear()}, ${time}`;
}

/** How many of the five rarities the run dropped. */
export function drops(run) {
  return RARITIES.reduce((sum, name) => sum + num(run?.items?.[name]), 0);
}

/** A count over the run's own seconds, per hour. */
export function perHour(value, secs) {
  return secs > 0 ? Math.round((num(value) * 3600) / secs) : 0;
}

/** The list's one line: gold, then the first two rarities that dropped, else the kills. */
export function headline(run) {
  const parts = [`${fmt(num(run?.gold))} gold`];
  const got = RARITIES.filter((name) => num(run?.items?.[name]) > 0).slice(0, 2);
  if (got.length) for (const name of got) parts.push(`${fmt(run.items[name])} ${name}`);
  else parts.push(`${fmt(num(run?.kills))} kills`);
  return parts.join(' · ');
}

const characterName = (run) => (typeof run?.character === 'string' && run.character ? run.character : null);

/** The picked run's line under its heading: who, at what levels, on which difficulty. */
export function whoLine(run) {
  const parts = [characterName(run) ?? 'Unknown character'];
  if (num(run?.level) > 0) parts.push(`Level ${run.level}`);
  if (num(run?.herolevel) > 0) parts.push(`Hero ${run.herolevel}`);
  const diff = difficulty(run?.difficulty, num(run?.hell_sub));
  if (diff) parts.push(diff);
  if (run?.hardcore === true) parts.push('Hardcore');
  return parts.join(' · ');
}

/** The run card's title: the character and the difficulty. */
export function titleLine(run) {
  const diff = difficulty(run?.difficulty, num(run?.hell_sub));
  return [characterName(run) ?? 'Unknown character', diff].filter(Boolean).join(' · ');
}

/** A drop's grade, D to SS; nothing for an ungraded one. */
export function grade(tier) {
  return GRADES[tier - 1] ?? '';
}

/** The bosses (`boss`) or chests (`chest`) a run put down, the ones it did at least once. */
export function tallies(run, group) {
  return (Array.isArray(run?.tallies) ? run.tallies : [])
    .filter((t) => t && t.group === group && num(t.total) > 0 && typeof t.label === 'string');
}

/** Where the run's time went, longest first as filed: each room, its name, its seconds and its share of the run. */
export function zones(run) {
  const secs = num(run?.secs);
  return (Array.isArray(run?.zones) ? run.zones : [])
    .filter((z) => Array.isArray(z) && typeof z[0] === 'string')
    .map(([room, spent]) => ({
      room,
      label: zoneLabel(room),
      secs: num(spent),
      share: secs > 0 ? Math.min(1, num(spent) / secs) : 0,
    }));
}

/** The named drops the run kept, newest first as filed. */
export function finds(run) {
  return (Array.isArray(run?.notable) ? run.notable : []).filter((d) => d && typeof d.name === 'string' && d.name);
}
