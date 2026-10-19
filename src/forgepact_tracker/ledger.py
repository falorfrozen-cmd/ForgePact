"""The tracker's session ledger: the offline half of HS Offline Tracker's `stats.rs`.

A `Ledger` turns the protocol-v1 records of one game process's session file
into the Tracker's session totals, per-hour rates, its drop journal with alert
decisions, Satanic zone rotations and finished runs. The behaviour is the
Tracker's, ported test by test (`tests/test_tracker_ledger.py`); what changed
is where time and earnings come from.

**Event time.** Every duration -- active time, the five-minute idle pause,
time per room, per-hour rates, the series, a run's start and end -- is
measured on the records' `emitted_at_unix_ms` and on the `at_ms` the panel
stamped on its own controls (a pause, a resume, an end-run by hand, a save
snapshot). Nothing here reads the wall clock. So a session read an hour after
it was played gives the same answer as one read live, and re-reading a file
after a panel restart gives identical totals. Time never runs backwards: an
event stamped earlier than one already applied is taken as happening at the
later moment.

**One source of earnings.** Gold, XP and kills come from the sensor's
`session_delta` records only. A character save (`apply_save`) sets the
character, the lifetime XP and kill totals and the boss and chest tallies; it
never adds gold, XP or kills, because with the sensor in the game process
there is no second source to reconcile. The Tracker's reconciliation of
online packets against saves is therefore not ported, nor are purses, mail,
pickups and ownership, server announcements, guild XP and carried totals.

Every drop the sensor records is a ground sighting, so the Tracker's "announce
on the ground or on pickup" preference has nothing to choose and is not kept.
"""

from __future__ import annotations

from dataclasses import dataclass, field

from . import items

I64_MAX = (1 << 63) - 1
I64_MIN = -(1 << 63)

# How long a run goes without a sign of life (gold, XP, a kill, a drop) before
# its clock stops, back-dated to that last sign of life.
IDLE_AFTER_MS = 300_000
# A room named this recently outranks the act a save reports.
ROOM_FRESH_MS = 600_000
# The series gets one point per this much active time.
SAMPLE_EVERY_SECS = 60

JOURNAL_RARITIES = ("Satanic", "Set", "Heroic", "Angelic", "Unholy")
SS_TIER = 6
JOURNAL_CAP = 400
SERIES_CAP = 4000
RUN_DROPS = 40
RUN_ZONES = 6

# Stackable resources by item type.
RESOURCES = ((12, "keys"), (13, "collectibles"), (14, "materials"), (15, "socketables"))
_RESOURCE_BY_TYPE = dict(RESOURCES)
# What a character wears and carries; only these count in the grade columns.
GEAR = frozenset((0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 18))
# Keys that drop by the handful and would bury the ones the counter is for.
DULL_KEYS = frozenset(("basic key", "crystal key"))

# The save's `statistic...` counters worth a tally: flattened key, label, group.
TALLIES = (
    ("statisticsatankills", "Satan", "boss"),
    ("statisticdamienkills", "Damien", "boss"),
    ("statisticreaperkills", "Reaper", "boss"),
    ("statisticanubiskills", "Anubis", "boss"),
    ("statisticguragkills", "Gurag", "boss"),
    ("statisticmeviuskills", "Mevius", "boss"),
    ("statisticodinkills", "Odin", "boss"),
    ("statisticcthulhukills", "Cthulhu", "boss"),
    ("statistickarpkingkills", "Karp King", "boss"),
    ("statisticuberdamienkills", "Uber Damien", "boss"),
    ("statisticuberreaperkills", "Uber Reaper", "boss"),
    ("statisticuberlunakills", "Uber Luna", "boss"),
    ("statisticuberendrixiakills", "Uber Endrixia", "boss"),
    ("statisticubergabrielkills", "Uber Gabriel", "boss"),
    ("statisticuberkingrakhulkills", "Uber King Rakhul", "boss"),
    ("statisticubersheepkingkills", "Uber Sheep King", "boss"),
    ("statisticubersungleekills", "Uber Sung Lee", "boss"),
    ("statisticuberamunrakills", "Uber Amun Ra", "boss"),
    ("statisticuberarchitectkills", "Uber Architect", "boss"),
    ("statisticuberpapalegbakills", "Uber Papa Legba", "boss"),
    ("statisticubercaptaingrimtidekills", "Uber Captain Grimtide", "boss"),
    ("statisticuberbloodmaidenkills", "Uber Blood Maiden", "boss"),
    ("statisticuberphantomleviathankills", "Uber Phantom Leviathan", "boss"),
    ("statisticuberchaostowerkills", "Uber Chaos Tower", "boss"),
    ("statisticchaostowerfloorclears", "Chaos Tower floors", "boss"),
    ("statisticwormholeclears", "Wormholes", "boss"),
    ("statisticcommonchestsopened", "Common", "chest"),
    ("statisticrarechestopened", "Rare", "chest"),
    ("statisticcrystalchestopened", "Crystal", "chest"),
    ("statisticrubychestsopened", "Ruby", "chest"),
    ("statisticdungeonchestsopened", "Dungeon", "chest"),
)

# Sources the protocol says are never a find.
IGNORED_SOURCES = frozenset(("vendor", "trade", "quest", "craft", "player_drop"))


def default_notable() -> list[tuple[str, list[str]]]:
    """Drops worth their own counter, matched by name. The rune groups follow
    the game's grades: S is Qi through Zed, SS the level-100 runes."""
    def group(label, names):
        return (label, [n.lower() for n in names])
    return [
        group("Angelic Key", ["Angelic Key"]),
        group("Satanic Dice", ["Satanic Dice"]),
        group("S runes", ["Qi", "Xo", "Sur", "Ber", "Jah", "Drax", "Zed"]),
        group("SS runes", ["Fawn", "Flo", "Nju", "Jol", "Sus", "Kek", "Jord"]),
    ]


def sat(value: int) -> int:
    """Clamp to the Tracker's i64, so an absurd value saturates rather than
    growing without bound."""
    return I64_MAX if value > I64_MAX else I64_MIN if value < I64_MIN else value


def _int(value, default: int = 0) -> int:
    number = items._int(value)
    return default if number is None else number


def _byte_list(values) -> list[int]:
    """Buff and debuff ids: integers 0..255, anything else discarded."""
    out = []
    for value in values if isinstance(values, list) else []:
        number = items._int(value)
        if number is not None and 0 <= number <= 255 and not isinstance(value, bool):
            out.append(number)
    return out


def act_of_room(room: str) -> int | None:
    """Which act a room's name says it belongs to: `Act_08_02` is act 8 and
    `Town_04_rm` act 4. Anything else (the Shadow Realm, wormholes, arenas)
    belongs to no act."""
    for prefix in ("Act_", "act_", "Town_", "town_"):
        if room.startswith(prefix):
            rest = room[len(prefix):]
            digits = ""
            for ch in rest:
                if not ch.isdigit():
                    break
                digits += ch
            if digits and int(digits) > 0:
                return int(digits)
            return None
    return None


@dataclass
class Prefs:
    """Everything the player chose, as against everything the session earned.
    One object, so a reset or a finished run cannot mislay a piece of it."""

    alerts: list = field(default_factory=lambda: list(JOURNAL_RARITIES))
    min_tier: int = 0
    # what the flourish answers to: a separate question from the alerts
    fx_rarities: list = field(default_factory=list)
    fx_tier: int = 6
    fx_listed: bool = False
    notable_defs: list = field(default_factory=default_notable)
    # (sound key, lower-cased item names): an item on a list is announced by it
    sound_lists: list = field(default_factory=list)
    # Satanic zone buffs worth an alert; empty means every rotation
    zone_buffs: list = field(default_factory=list)

    def to_dict(self) -> dict:
        return {
            "alerts": list(self.alerts), "min_tier": self.min_tier, "fx_rarities": list(self.fx_rarities),
            "fx_tier": self.fx_tier, "fx_listed": self.fx_listed,
            "notable_defs": [[label, list(names)] for label, names in self.notable_defs],
            "sound_lists": [[key, list(names)] for key, names in self.sound_lists],
            "zone_buffs": list(self.zone_buffs),
        }

    @classmethod
    def from_dict(cls, doc: dict) -> "Prefs":
        return cls(
            alerts=list(doc.get("alerts", JOURNAL_RARITIES)),
            min_tier=int(doc.get("min_tier", 0)),
            fx_rarities=list(doc.get("fx_rarities", [])),
            fx_tier=int(doc.get("fx_tier", 6)),
            fx_listed=bool(doc.get("fx_listed", False)),
            notable_defs=[(label, list(names)) for label, names in doc.get("notable_defs", default_notable())],
            sound_lists=[(key, list(names)) for key, names in doc.get("sound_lists", [])],
            zone_buffs=list(doc.get("zone_buffs", [])),
        )


def prefs_from_settings(settings: dict) -> Prefs:
    """The ledger's half of the Tracker's settings document, mapped the way its
    `apply_stats_settings` does. `settings` is a validated document with every
    field present (`store.TrackerStore.settings`)."""
    alerts = [r for r in settings.get("alerts", []) if r in JOURNAL_RARITIES]
    notable = [(g["label"], [n.lower() for n in g["names"]]) for g in settings.get("notable", [])]
    use_filter = bool(settings.get("use_filter", True))
    flourish = bool(settings.get("flourish", True))
    active = None
    if use_filter:
        active = next((f for f in settings.get("filters", []) if f.get("id") == settings.get("filter")), None)
    lists = []
    for lst in (active or {}).get("lists", []):
        if lst.get("enabled", True) and lst.get("id") and lst.get("items"):
            lists.append(("list-%s" % lst["id"], [n.strip().lower() for n in lst["items"]]))
    min_tier = int(settings.get("min_tier", 0))
    return Prefs(
        alerts=alerts,
        min_tier=min_tier,
        # sound and picture follow one rule, as in the Tracker
        fx_rarities=list(alerts) if flourish else [],
        fx_tier=max(0, min(6, min_tier)),
        fx_listed=flourish and bool(settings.get("flourish_listed", False)) and use_filter,
        notable_defs=notable or default_notable(),
        sound_lists=lists,
        zone_buffs=list(settings.get("zone_buffs", [])),
    )


class Ledger:
    """One session's ledger. `file` is the session file's name, which every
    journal entry and finished run is keyed by."""

    def __init__(self, started_ms: int | None = None, file: str | None = None, prefs: Prefs | None = None):
        self.file = file or ""
        self.prefs = prefs or Prefs()
        self.revision = 0
        self.extra_revision = 0
        self.status = ""
        self.valid_records = 0
        self.ignored_records = 0
        self.sensor_name: str | None = None
        self._finished: list = []
        self._begin(started_ms)
        # what survives a reset: the character, where they are, the zone
        self.character: dict | None = None
        self.baseline_for: str | None = None
        self.total_xp: int | None = None
        self.total_kills: int | None = None
        self.last_save_ms: int | None = None
        self.tally_base: dict = {}
        self.room: str | None = None
        self.room_at_ms: int | None = None
        self.act = 0
        self.satanic: dict | None = None
        self.satanic_at: int | None = None
        self.satanic_here = False
        self.sz_changed = False
        self.room_since_ms: int | None = None

    # -- the session's own state ------------------------------------------

    def _begin(self, started_ms: int | None) -> None:
        """Everything a run earned, cleared; the clock starting at `started_ms`."""
        self.started_ms = started_ms
        self.now_ms = started_ms
        self.zone_time: dict = {}
        self.paused_at_ms: int | None = None
        self.paused_total_ms = 0
        self.by_hand = False
        self.last_progress_ms = started_ms
        self.gold_earned = 0
        self.xp_earned = 0
        self.kills_earned = 0
        self.items = {name: {"total": 0, "mf": 0} for name in items.RARITY_NAMES}
        self.grades: dict = {}
        self.tally_earned: dict = {}
        self.resources = {name: 0 for _, name in RESOURCES}
        self.notable: dict = {}
        self.seen: set = set()
        self.counted: set = set()
        self.told: set = set()
        self.drops: list = []
        self.series: list = []
        self.next_sample_secs = SAMPLE_EVERY_SECS

    def _at(self, at_ms) -> int:
        """The moment an event happens, on the ledger's clock: never earlier
        than an event already applied. The first event starts the clock."""
        at = _int(at_ms, self.now_ms if self.now_ms is not None else 0)
        if self.started_ms is None:
            self.started_ms = at
            self.last_progress_ms = at
            self.now_ms = at
        if self.now_ms is None or at > self.now_ms:
            self.now_ms = at
        return self.now_ms

    @property
    def paused(self) -> bool:
        return self.paused_at_ms is not None

    def active_ms(self, now_ms: int | None = None) -> int:
        """How long the run has actually been running: the clock less whatever
        it has spent paused. Every rate divides by this."""
        if self.started_ms is None:
            return 0
        now = self.now_ms if now_ms is None else now_ms
        ran = now - self.started_ms - self.paused_total_ms
        if self.paused_at_ms is not None:
            ran -= now - self.paused_at_ms
        return max(0, ran)

    def _active_secs(self, now_ms: int | None = None) -> int:
        return self.active_ms(now_ms) // 1000

    def per_hour(self, value: int) -> int:
        secs = self._active_secs()
        if secs == 0:
            return 0
        return sat(value * 3600) // secs

    def graded(self, tier: int) -> int:
        """How many gear drops of one grade (1 = D .. 6 = SS) this run produced."""
        return self.grades.get(tier, 0)

    def tallies(self) -> list:
        """The bosses and chests this run has to its name, in the table's order,
        without the ones still at zero."""
        out = []
        for key, label, group in TALLIES:
            total = self.tally_earned.get(key, 0)
            if total > 0:
                out.append({"label": label, "group": group, "total": total})
        return out

    # -- the room clock and the pause ---------------------------------------

    def _bank_room_time(self, at: int, restart_at: int | None = None) -> None:
        """Credit the current room with the time since its clock started, up to
        `at`, and start counting again (from `restart_at`, or `at`)."""
        if self.room is not None and self.room_since_ms is not None:
            spent = at - self.room_since_ms
            if spent > 0:
                self.zone_time[self.room] = self.zone_time.get(self.room, 0) + spent
        self.room_since_ms = at if restart_at is None else restart_at

    def _hold(self, since: int, by_hand: bool) -> None:
        """Stop the clock as of `since`: now for a pause the player asked for,
        the last sign of life for one the idle watch decided on."""
        if self.paused_at_ms is None:
            self._bank_room_time(since)
            self.room_since_ms = None
            self.paused_at_ms = since
            # points sampled after a back-dated stop were never real run time
            cut = self._active_secs()
            if any(p["t"] > cut for p in self.series):
                self.series = [p for p in self.series if p["t"] <= cut]
                self.next_sample_secs = (cut // SAMPLE_EVERY_SECS + 1) * SAMPLE_EVERY_SECS
                self.extra_revision += 1
            self.revision += 1
        self.by_hand = self.by_hand or by_hand

    def _release(self, at: int) -> None:
        if self.paused_at_ms is not None:
            self.paused_total_ms += max(0, at - self.paused_at_ms)
            self.paused_at_ms = None
            self.room_since_ms = at
            self.revision += 1
        self.by_hand = False
        self.last_progress_ms = at

    def set_paused(self, on: bool, at_ms: int) -> None:
        """The pause button. A pause the player asked for outranks the idle
        watch: it lasts until the same hand lifts it."""
        at = self._at(at_ms)
        if on:
            self._hold(at, True)
        else:
            self._release(at)

    def watch_idle(self, now_ms: int) -> None:
        """Quiet for long enough: stop the clock as of the last sign of life,
        since the minutes it took to notice were not part of the run either."""
        if self.paused or self.last_progress_ms is None:
            return
        if now_ms - self.last_progress_ms < IDLE_AFTER_MS:
            return
        self._hold(self.last_progress_ms, False)

    def _progressed(self, at: int) -> None:
        self.last_progress_ms = at
        if self.paused and not self.by_hand:
            self._release(at)

    def _sample(self) -> None:
        """One series point per minute of active time, stamped on that time."""
        if self.paused:
            return
        secs = self._active_secs()
        if secs < self.next_sample_secs or len(self.series) >= SERIES_CAP:
            return
        self.series.append({"t": secs, "gold": self.gold_earned, "xp": self.xp_earned})
        self.next_sample_secs = (secs // SAMPLE_EVERY_SECS + 1) * SAMPLE_EVERY_SECS
        self.extra_revision += 1

    # -- preferences -------------------------------------------------------

    def set_prefs(self, prefs: Prefs) -> None:
        self.revision += 1
        self.prefs = prefs

    def set_filter(self, alerts, min_tier: int) -> None:
        self.revision += 1
        self.prefs.alerts = list(alerts)
        self.prefs.min_tier = min_tier

    def set_flourish_filter(self, rarities, tier: int) -> None:
        self.revision += 1
        self.prefs.fx_rarities = list(rarities)
        self.prefs.fx_tier = tier

    def set_sound_lists(self, lists) -> None:
        self.revision += 1
        self.prefs.sound_lists = [(key, [n.strip().lower() for n in names]) for key, names in lists]

    # -- records -----------------------------------------------------------

    def apply(self, record: dict) -> dict | None:
        """Apply one protocol-v1 record. Returns the drop entry when the record
        was a new drop worth an alert or a flourish, else None."""
        at = self._at(record.get("emitted_at_unix_ms"))
        self.revision += 1
        self.valid_records += 1
        self.watch_idle(at)
        kind = record.get("kind")
        entry = None
        if kind == "session_delta":
            self._delta(record, at)
        elif kind in ("ground_drop", "drop_spawned"):
            entry = self._drop(record, at)
        elif kind == "room":
            self._room(record.get("room"), at)
        elif kind == "satanic_zone":
            self._zone(record, at)
        elif kind == "vitals":
            self._vitals(record)
        elif kind in ("progress", "character"):
            self._sensor_progress(record, at)
        elif kind == "bridge_status":
            state = str(record.get("state", ""))
            reason = record.get("reason") or record.get("detail") or ""
            self.status = "blocked|%s" % reason if state in ("blocked", "transport_error") else state
        elif kind != "heartbeat":
            self.ignored_records += 1
        self._sample()
        return entry

    def _delta(self, record: dict, at: int) -> None:
        gold = max(0, _int(record.get("gold")))
        xp = max(0, _int(record.get("xp")))
        kills = max(0, _int(record.get("kills")))
        if gold:
            self.gold_earned = sat(self.gold_earned + gold)
        if xp:
            self.xp_earned = sat(self.xp_earned + xp)
        if kills:
            self.kills_earned = sat(self.kills_earned + kills)
        if gold or xp or kills:
            self._progressed(at)

    def _room(self, room, at: int) -> None:
        if not isinstance(room, str) or not room:
            return
        if self.room != room:
            # Close the books on the room being left -- never while the clock
            # is stopped, or a paused trip to town banks paused time.
            if not self.paused:
                self._bank_room_time(at)
            self.room = room
            act = act_of_room(room)
            if act is not None:
                self.act = act
            self.revision += 1
        self.room_at_ms = at

    def _zone(self, record: dict, at: int) -> None:
        zone = record.get("zone")
        if not isinstance(zone, str):
            return
        buffs = _byte_list(record.get("buffs"))
        debuffs = _byte_list(record.get("debuffs"))
        held = self.satanic
        # a zone where there was none is learning where it is, not it moving;
        # the same room with a different roll on it did rotate
        if held is not None and (held["zone"] != zone or sorted(held["buffs"]) != sorted(buffs)
                                 or sorted(held["debuffs"]) != sorted(debuffs)):
            self.sz_changed = True
        self.satanic = {"zone": zone, "buffs": buffs, "debuffs": debuffs}
        self.satanic_at = at

    def _vitals(self, record: dict) -> None:
        if "satanic_here" in record:
            here = bool(record.get("satanic_here"))
            if here != self.satanic_here:
                self.revision += 1
            self.satanic_here = here
        if self.character is not None:
            for field_name, key in (("level", "level"), ("herolevel", "hero_level")):
                value = _int(record.get(key))
                if value > 0 and self.character[field_name] != value:
                    self.character[field_name] = value
                    self.extra_revision += 1

    def _sensor_progress(self, record: dict, at: int) -> None:
        """The sensor naming the character. Another name than the one it named
        before ends the run at this record and starts the next one."""
        name = record.get("name")
        if not isinstance(name, str) or not name.strip():
            return
        name = name.strip()
        if self.sensor_name is not None and self.sensor_name.lower() != name.lower():
            run = self.finish(at)
            if run is not None:
                self._finished.append((run["id"], run))
            self._restart(at, keep_character=False)
        self.sensor_name = name
        if self.character is None or self.character["name"].lower() != name.lower():
            self._forget_character_marks()
            self.character = _character(name)
            self.extra_revision += 1

    def _forget_character_marks(self) -> None:
        self.total_xp = None
        self.total_kills = None
        self.last_save_ms = None
        self.tally_base = {}
        self.baseline_for = None

    def _drop(self, record: dict, at: int) -> dict | None:
        if str(record.get("source", "")).lower() in IGNORED_SOURCES:
            return None
        item = record.get("item")
        if not isinstance(item, dict):
            item = record
        name = item.get("name") if isinstance(item.get("name"), str) else ""
        item_type = _int(item.get("item_type"))
        item_id = _int(item.get("item_id"))
        weapon_type = _int(item.get("weapon_type"))
        if not name and "item_type" in item and "item_id" in item:
            name = items.item_name(item_type, item_id, weapon_type) or ""
        unscaled = bool(item.get("unscaled", False))
        mf = bool(item.get("magic_find_drop", False))
        seed = _int(item.get("seed"))
        amount = max(1, _int(item.get("amount"), 1))
        hash_ = item.get("hash") if isinstance(item.get("hash"), str) else ""
        fingerprint = item.get("fingerprint") if isinstance(item.get("fingerprint"), str) else ""
        event_id = record.get("event_id") if isinstance(record.get("event_id"), str) else ""
        identity = ("h:" + hash_) if hash_ else fingerprint or (("e:" + event_id) if event_id else "")

        if identity:
            # the same record read twice is one sighting
            if "d:" + identity in self.seen:
                return None
            self.seen.add("d:" + identity)
            if len(self.seen) > 20_000:
                # bounded the way the Tracker bounds it
                self.seen.clear()
                self.counted.clear()
                self.told.clear()
        first = not identity or identity not in self.counted
        if identity:
            self.counted.add(identity)

        triple = (item_type, item_id, weapon_type)
        known = items.known_item(name, triple, unscaled)
        tier = _int(item.get("tier"))
        if tier == 0 and name:
            tier = known.tier if known is not None else items.tier_by_name(name)
        rarity = items.resolve_rarity(item.get("rarity"), name, triple, unscaled)
        is_resource = item_type in _RESOURCE_BY_TYPE or "essence vault" in name.lower()

        if first:
            if tier > 0 and item_type in GEAR:
                self.grades[tier] = self.grades.get(tier, 0) + amount
            if not is_resource and rarity in self.items:
                self.items[rarity]["total"] += amount
                if mf:
                    self.items[rarity]["mf"] += amount
            resource = _RESOURCE_BY_TYPE.get(item_type)
            if resource is not None and name.lower() not in DULL_KEYS:
                self.resources[resource] += amount
            self._count_notable(name, amount)
            self._progressed(at)

        listed = self._listed_sound(name, known)
        announce = listed is not None or (not is_resource and self._passes_filter(rarity, tier))
        flourish = not is_resource and self._worth_a_flourish(rarity, tier, listed is not None)
        if not (announce or flourish):
            return None
        if identity:
            if identity in self.told:
                return None
            self.told.add(identity)
        # only what the alert rules asked for makes a sound
        sound = None
        if announce:
            sound = listed if listed is not None else (rarity.lower() if rarity in self.prefs.alerts else None)
        sequence = _int(record.get("sequence"))
        room = record.get("room") if isinstance(record.get("room"), str) else self.room
        entry = {
            "id": "%s:%d" % (self.file, sequence),
            "file": self.file,
            "sequence": sequence,
            "ts_ms": at,
            "rarity": rarity,
            "mf": mf,
            "tier": tier,
            "item_type": item_type,
            "item_id": item_id,
            "weapon_type": weapon_type,
            "seed": seed,
            "name": name,
            "announced": False,
            "ground": True,
            "zone": self.satanic["zone"] if self.satanic else None,
            "room": room,
            "sound": sound,
            "announce": announce,
            "flourish": flourish,
        }
        # the journal is the alert rules' list; a flourish alone does not join it
        if announce:
            if len(self.drops) >= JOURNAL_CAP:
                self.drops.pop(0)
            self.drops.append(dict(entry))
            self.extra_revision += 1
        return entry

    def _listed_sound(self, name: str, known) -> str | None:
        """Which list names this drop, if any. A list may qualify a name two
        items share by the rarity, as `Essence Vault (Angelic)`; the bare name
        still means any of them."""
        if not name:
            return None
        lower = name.lower()
        qualified = "%s (%s)" % (lower, known.rarity.lower()) if known is not None else None
        for key, names in self.prefs.sound_lists:
            if lower in names or (qualified is not None and qualified in names):
                return key
        return None

    def _passes_filter(self, rarity: str, tier: int) -> bool:
        return rarity in self.prefs.alerts and tier >= self.prefs.min_tier

    def _worth_a_flourish(self, rarity: str, tier: int, listed: bool) -> bool:
        """The flourish's own question. Its lowest grade, D, means any grade,
        an ungraded item included."""
        if self.prefs.fx_listed and listed:
            return True
        graded = tier >= self.prefs.fx_tier or self.prefs.fx_tier <= 1
        return rarity in self.prefs.fx_rarities and graded

    def _count_notable(self, name: str, amount: int) -> None:
        if not name:
            return
        lower = name.lower()
        bare = _strip_rune(lower)
        for label, names in self.prefs.notable_defs:
            if any(n == lower or _strip_rune(n) == bare for n in names):
                self.notable[label] = self.notable.get(label, 0) + amount
                return

    # -- saves -------------------------------------------------------------

    def apply_save(self, progress: dict, at_ms: int) -> bool:
        """A character save's protocol-v1 `progress` dict. It sets the character,
        the lifetime XP and kill totals and the boss and chest tallies, and
        never adds gold, XP or kills. A save for another character than the one
        the sensor names is not this session's, and is not applied."""
        name = progress.get("name") if isinstance(progress.get("name"), str) else ""
        name = name.strip()
        if self.sensor_name is not None and name and name.lower() != self.sensor_name.lower():
            return False
        at = self._at(at_ms)
        self.revision += 1
        has_experience = "experience" in progress
        if name and self.baseline_for is not None and self.baseline_for != name:
            # another character's counters are not a continuation of these
            self.tally_base = {}
            self.total_kills = None
        if has_experience and name:
            self.baseline_for = name
        if has_experience:
            self.last_save_ms = at
            self.total_xp = max(0, _int(progress.get("experience")))
        tallies = progress.get("tallies") if isinstance(progress.get("tallies"), dict) else {}
        kills = _int(progress.get("kills"))
        if kills > 0 or "statistictotalmonsterkills" in tallies:
            self.total_kills = max(0, kills)
        for key, _label, _group in TALLIES:
            if key not in tallies:
                continue
            now = _int(tallies[key])
            if key in self.tally_base:
                diff = now - self.tally_base[key]
                if diff > 0:
                    self.tally_earned[key] = self.tally_earned.get(key, 0) + diff
            self.tally_base[key] = now
        act = _int(progress.get("act"))
        if act > 0 and self.act != act:
            self.act = act
        # a room in another act is not where the character is -- unless the
        # sensor named it recently, which outranks the save
        fresh = self.room_at_ms is not None and at - self.room_at_ms < ROOM_FRESH_MS
        if self.room is not None and act > 0 and not fresh:
            was = act_of_room(self.room)
            if was is not None and was != act:
                if not self.paused:
                    self._bank_room_time(at)
                self.room = None
                self.room_since_ms = None
        if has_experience or self.character is None:
            season = _int(progress.get("season")) if "season" in progress else (
                self.character["season"] if self.character else 0)
            self.character = {
                "name": name,
                "level": _int(progress.get("level")),
                "herolevel": _int(progress.get("hero_level")),
                "difficulty": _int(progress.get("difficulty")),
                "hell_sub": _int(progress.get("hell_sub")),
                "hardcore": _int(progress.get("hardcore")) == 1,
                "season": season,
            }
            self.extra_revision += 1
        return True

    # -- zone alerts -------------------------------------------------------

    def take_zone_change(self) -> dict | None:
        """The Satanic zone moved and the rotation passes the buff pick. Taken,
        so one rotation is announced once. An empty pick is every rotation.
        `at_ms` is the event time of the record that rotated it."""
        if not self.sz_changed:
            return None
        self.sz_changed = False
        zone = self.satanic
        if zone is None:
            return None
        wanted = self.prefs.zone_buffs
        if wanted and not any(b in wanted for b in zone["buffs"]):
            return None
        return {"zone": zone["zone"], "buffs": list(zone["buffs"]), "debuffs": list(zone["debuffs"]),
                "at_ms": self.satanic_at}

    # -- runs --------------------------------------------------------------

    def run_key(self) -> str:
        return "%s#%d" % (self.file, self.started_ms or 0)

    def finish(self, at_ms: int) -> dict | None:
        """What this run amounted to, or None when there is nothing to say: less
        than a minute, or nothing earned, found or completed."""
        at = self._at(at_ms)
        if not self.paused:
            self._bank_room_time(at)
        secs = self._active_secs(at)
        found = (any(c["total"] > 0 for c in self.items.values()) or any(v > 0 for v in self.resources.values())
                 or any(v > 0 for v in self.notable.values()) or bool(self.drops))
        completed = any(v > 0 for v in self.tally_earned.values())
        nothing = (self.gold_earned == 0 and self.xp_earned == 0 and self.kills_earned == 0
                   and not found and not completed)
        if secs < 60 or nothing:
            return None
        zones = sorted(((room, ms // 1000) for room, ms in self.zone_time.items() if ms >= 1000),
                       key=lambda z: (-z[1], z[0]))[:RUN_ZONES]
        notable = [{"name": d["name"], "rarity": d["rarity"], "tier": d["tier"], "ts_ms": d["ts_ms"]}
                   for d in reversed(self.drops) if d["name"]][:RUN_DROPS]
        character = self.character or {}
        return {
            "id": self.run_key(),
            "file": self.file,
            "started_ms": self.started_ms,
            "ended_ms": at,
            "secs": secs,
            "character": character.get("name"),
            "level": character.get("level", 0),
            "difficulty": character.get("difficulty", 0),
            "hell_sub": character.get("hell_sub", 0),
            "herolevel": character.get("herolevel", 0),
            "hardcore": bool(character.get("hardcore", False)),
            "season": character.get("season", 0),
            "gold": self.gold_earned,
            "xp": self.xp_earned,
            "kills": self.kills_earned,
            "items": {name: count["total"] for name, count in self.items.items()},
            "notable": notable,
            "zones": [[room, secs_in] for room, secs_in in zones],
            "tallies": self.tallies(),
        }

    def take_finished_runs(self) -> list:
        """Runs ended inside the stream (by hand, or by a change of character),
        as (key, run) pairs, each handed out once."""
        done, self._finished = self._finished, []
        return done

    def reset(self, at_ms: int) -> dict | None:
        """End the run by hand: file it when it qualifies, then start the next
        one at `at_ms`. The character, the room, the zone, the save marks and
        every setting carry over; only what the run earned restarts."""
        at = self._at(at_ms)
        run = self.finish(at)
        if run is not None:
            self._finished.append((run["id"], run))
        self._restart(at, keep_character=True)
        return run

    def _restart(self, at: int, keep_character: bool) -> None:
        self._begin(at)
        if not keep_character:
            self.character = None
            self._forget_character_marks()
        # the room travels, and so does its clock, from the reset
        self.room_since_ms = at if self.room is not None else None
        self.revision += 1
        self.extra_revision += 1

    # -- what the panel reads ----------------------------------------------

    def snapshot(self, status: str | None = None) -> dict:
        def line(total, earned):
            return {"total": total, "earned": earned, "per_hour": self.per_hour(earned)}

        return {
            "status": self.status if status is None else status,
            "session_secs": self._active_secs(),
            "paused": self.paused,
            "save_age_secs": (None if self.last_save_ms is None or self.now_ms is None
                              else max(0, self.now_ms - self.last_save_ms) // 1000),
            "gold": line(None, self.gold_earned),
            "xp": line(self.total_xp, self.xp_earned),
            "kills": line(self.total_kills, self.kills_earned),
            "notable": [{"label": label, "total": self.notable.get(label, 0)}
                        for label, _names in self.prefs.notable_defs],
            "items": {name: {"total": c["total"], "mf": c["mf"], "per_hour": self.per_hour(c["total"])}
                      for name, c in self.items.items()},
            "satanic_zone": dict(self.satanic) if self.satanic else None,
            "satanic_at": self.satanic_at,
            "room": self.room,
            "act": self.act,
            "satanic_here": self.satanic_here,
            "character": dict(self.character) if self.character else None,
            "tallies": self.tallies(),
            "ss": self.graded(SS_TIER),
            "revision": self.revision,
            "extra_revision": self.extra_revision,
        }

    def extra(self) -> dict:
        return {
            "character": dict(self.character) if self.character else None,
            "series": [dict(p) for p in self.series],
            "drops": [dict(d) for d in reversed(self.drops)],
        }


def _character(name: str) -> dict:
    return {"name": name, "level": 0, "herolevel": 0, "difficulty": 0, "hell_sub": 0, "hardcore": False,
            "season": 0}


def _strip_rune(name: str) -> str:
    while name.endswith(" rune"):
        name = name[: -len(" rune")]
    return name
