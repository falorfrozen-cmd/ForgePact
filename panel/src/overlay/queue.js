// The Tracker's overlay window (#185), its pure half: the top line, the
// newest-drops strip (the ticker) and the drop card's queue (the flourish).
// Overlay.svelte draws what these answer and nothing else;
// tests/tracker-overlay.test.js runs them under plain node, so nothing here
// may touch the DOM, a timer or a rune. Time is always passed in.
//
// Ported from HS Offline Tracker's Ticker and Flourish views (MIT, Copyright
// (c) 2026 falorfrozen-cmd and HS Offline Tracker contributors; the notice is
// in CREDITS.md) into ForgePact's design (panel/DESIGN.md § Tracker,
// design/tracker/overlay.html): the ticker is the journal's three newest
// entries, still, rather than a stack of plates that slide in and expire; and
// the drop card is one card at a time over the counters, rather than a stack
// in a window of its own.
//
// Which drops earn a card is the ledger's question, answered when the drop was
// read (`flourish` on the journal entry, from the drop card's rarities, lowest
// grade and sound-list settings). The overlay only shows the ones that arrive
// after it opened: what the journal already held is history, not news.

import { RARITY_CLASS } from '../tracker/format.js';
import { grade, timeOfDay, zoneName } from '../tracker/session/view.js';

/** How many journal entries the ticker shows. */
export const TICKER_ROWS = 3;

/** A drop's name, or what it was when the game gave none. */
function nameOf(d, rarity) {
  return typeof d?.name === 'string' && d.name.trim() ? d.name : `${rarity} drop`;
}

function rarityOf(d) {
  return typeof d?.rarity === 'string' && d.rarity ? d.rarity : 'Drop';
}

/** The ticker's rows: the journal's newest entries (it is newest first already). */
export function tickerRows(drops, n = TICKER_ROWS) {
  return (Array.isArray(drops) ? drops : []).slice(0, n).map((d, index) => {
    const rarity = rarityOf(d);
    return {
      key: typeof d.id === 'string' && d.id ? d.id : `${d.ts_ms}:${index}`,
      time: timeOfDay(d.ts_ms),
      name: nameOf(d, rarity),
      cls: RARITY_CLASS[rarity] ?? '',
      grade: grade(d.tier),
    };
  });
}

/** The top line's state, in the fewest words the 444 px plate has room for. */
export function overlayState({ on = false, session = null, snapshot = null, catching_up = false } = {}) {
  if (!on) return { text: 'Tracker off', tone: 'off' };
  if (!session?.live) return { text: 'Waiting for the game', tone: 'warn' };
  if (catching_up) return { text: 'Catching up', tone: 'warn' };
  if (snapshot?.paused) return { text: 'Clock paused', tone: 'off' };
  return { text: 'Recording', tone: 'on' };
}

// ---- The drop card -------------------------------------------------------

/** The same drop again within this many ms counts on the card (×2) instead of queueing. */
export const GROUP_MS = 1500;
/** The card's leaving fade (`--motion-duration-fast`; tests/tracker-overlay.test.js holds them equal). */
export const LEAVE_MS = 120;
/** How many cards may wait behind the one shown. */
export const MAX_WAITING = 3;
// Which card a full queue keeps: the rarer drop. A zone rotation outranks them all.
const PRIORITY = { Set: 1, Satanic: 2, Heroic: 3, Angelic: 4, Unholy: 5 };
const ZONE_PRIORITY = 6;

/** How long a card stays up: the drop card setting, 2 to 10 s, 4 s by default. */
export function cardMs(settings) {
  const secs = settings?.flourish_secs;
  if (typeof secs !== 'number' || !Number.isFinite(secs)) return 4000;
  return Math.round(Math.min(10, Math.max(2, secs)) * 1000);
}

/** A journal entry's card: its name in its rarity's colour, then rarity, grade and Magic Find. */
export function dropCard(d) {
  const rarity = rarityOf(d);
  const name = nameOf(d, rarity);
  const g = grade(d.tier);
  const tier = d.tier | 0;
  return {
    key: `drop:${d.id}`,
    kind: 'drop',
    group: `${rarity.toLowerCase()}|${name.toLowerCase()}|${tier}`,
    priority: PRIORITY[rarity] ?? 0,
    name,
    cls: RARITY_CLASS[rarity] ?? '',
    meta: [rarity, g ? `Grade ${g}` : null, d.mf ? 'Magic Find' : null].filter(Boolean).join(' · '),
  };
}

/**
 * A zone rotation's card: the zone by name, then its first two buffs by name
 * (`/api/state`'s `satanicBuffs`, `[id, name, description]` rows) and how many
 * more.
 */
export function zoneCard(alert, buffList) {
  const byId = new Map();
  for (const row of Array.isArray(buffList) ? buffList : []) if (Array.isArray(row)) byId.set(Number(row[0]), String(row[1]));
  const names = (Array.isArray(alert?.buffs) ? alert.buffs : []).map((id) => byId.get(Number(id)) ?? `Buff ${id}`);
  let meta = 'No buffs this rotation';
  if (names.length) meta = names.slice(0, 2).join(' · ') + (names.length > 2 ? ` · +${names.length - 2}` : '');
  return {
    key: `zone:${alert?.seq}`,
    kind: 'zone',
    group: `zone:${alert?.zone ?? ''}:${names.join(',')}`,
    priority: ZONE_PRIORITY,
    name: `Satanic zone: ${zoneName(alert?.zone)}`,
    cls: '',
    meta,
  };
}

/**
 * The drop card's queue: one card shown, at most MAX_WAITING behind it. The
 * first journal and the first zone alert it is given are only remembered, so
 * only what arrives after the overlay opened raises a card.
 */
export class FlourishQueue {
  card = null;
  waiting = [];
  #seen = null;
  #zoneSeeded = false;
  #zoneSeq = null;

  /** `/api/tracker/extra`'s journal (newest first): offer each new entry with `flourish`, oldest first. */
  takeJournal(drops, now, lifeMs) {
    if (!Array.isArray(drops)) return;
    const ids = drops.map((d) => d?.id).filter((id) => typeof id === 'string');
    if (this.#seen === null) {
      this.#seen = new Set(ids);
      return;
    }
    for (const d of [...drops].reverse()) {
      if (typeof d?.id !== 'string' || this.#seen.has(d.id)) continue;
      this.#seen.add(d.id);
      if (d.flourish) this.offer(dropCard(d), now, lifeMs);
    }
  }

  /** `/api/tracker`'s `zone_alert`: a rotation the page has not seen raises a card when `wanted`. */
  takeZone(alert, now, lifeMs, wanted, buffList) {
    const seq = alert && Number.isFinite(alert.seq) ? alert.seq : null;
    if (!this.#zoneSeeded) {
      this.#zoneSeeded = true;
      this.#zoneSeq = seq;
      return;
    }
    if (seq === null || seq === this.#zoneSeq) return;
    this.#zoneSeq = seq;
    if (wanted) this.offer(zoneCard(alert, buffList), now, lifeMs);
  }

  /** Show a card, count it on one already up or waiting, or queue it. */
  offer(card, now, lifeMs) {
    this.advance(now);
    const same = [this.card, ...this.waiting].find((c) => c && c.group === card.group && now - c.lastSeen <= GROUP_MS);
    if (same) {
      same.count += 1;
      same.lastSeen = now;
      if (same === this.card) same.until = now + lifeMs;
      return;
    }
    const next = { ...card, count: 1, lastSeen: now, lifeMs, until: now + lifeMs };
    if (!this.card) {
      this.card = next;
      return;
    }
    if (this.waiting.length >= MAX_WAITING) {
      const weakest = [...this.waiting].sort((a, b) => a.priority - b.priority || a.lastSeen - b.lastSeen)[0];
      if (next.priority <= weakest.priority) return;
      this.waiting = this.waiting.filter((c) => c !== weakest);
    }
    this.waiting.push(next);
  }

  /** Retire the card whose time is up and bring on the next, which starts its own time now. */
  advance(now) {
    while (this.card && now >= this.card.until) {
      const next = this.waiting.shift() ?? null;
      if (next) next.until = Math.max(now, this.card.until) + next.lifeMs;
      this.card = next;
    }
  }

  /** The card to draw at `now`, and whether it is entering or leaving. */
  view(now) {
    this.advance(now);
    if (!this.card) return { card: null, state: null };
    return { card: this.card, state: now >= this.card.until - LEAVE_MS ? 'leave' : 'enter' };
  }

  /** When the view next changes (the leaving fade, or the card going), or null when nothing is up. */
  nextAt(now) {
    this.advance(now);
    if (!this.card) return null;
    const leave = this.card.until - LEAVE_MS;
    return now < leave ? leave : this.card.until;
  }
}
