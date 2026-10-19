#!/usr/bin/env python3
"""The panel-side tracker ledger (ForgePact #185, part 2a).

The ledger is the offline half of HS Offline Tracker's `stats.rs`, ported by
behaviour: it turns the protocol-v1 records the plugin's `tracker` mod writes
into the Tracker's session totals, per-hour rates, its drop journal with alert
decisions, Satanic zone rotations and finished runs.

Each test named after a `stats.rs` test is that test, ported. Two things
change in every one of them, because they changed in the design:

- **The clock is the records'.** A Rust test aged a session by back-dating an
  `Instant`; here every record carries `emitted_at_unix_ms` and every control
  an `at_ms`, so a test moves time by stamping a record later. That is what
  lets the panel catch up on a session played while it was closed and get the
  same answer.
- **The sensor is in the game process.** XP and kills come from
  `session_delta` records only; a character save sets the character, the
  lifetime totals and the boss and chest tallies, and never adds gold, XP or
  kills. The Rust tests that used two saves to make earnings use deltas here.

Every drop is a ground sighting (the sensor records drops as they are
created, and there is no pickup record), so the Tracker's "announce on the
ground or on pickup" preference has nothing to choose between and is not
ported: the Rust tests' `set_prefer_ground` calls are dropped.

Below the ported tests are ForgePact's own rules, each with its own test.
"""

import sys
import unittest
from pathlib import Path
from unittest.mock import patch

TESTS = Path(__file__).resolve().parent
sys.path.insert(0, str(TESTS))
sys.path.insert(0, str(TESTS.parent / "src"))

from forgepact_tracker import items  # noqa: E402
from forgepact_tracker.ledger import I64_MAX, IDLE_AFTER_MS, Ledger  # noqa: E402
from forgepact_tracker.protocol import PROTOCOL_ID  # noqa: E402

T0 = 1_760_000_000_000
FILE = "session-4242-1760000000000.ndjson"


class Feed:
    """Drives one ledger the way a session file does: every record is stamped
    with the feed's clock and the next sequence number."""

    def __init__(self, ledger=None):
        self.ledger = ledger or Ledger(started_ms=T0, file=FILE)
        self.t = T0
        self.seq = 0

    def at(self, secs):
        self.t = T0 + int(secs * 1000)
        return self

    def record(self, kind, **fields):
        self.seq += 1
        rec = {"protocol": PROTOCOL_ID, "v": 1, "kind": kind, "sequence": self.seq,
               "emitted_at_unix_ms": self.t, "process_id": 4242, "build_id": "forgepact-test"}
        rec.update(fields)
        return rec

    def send(self, kind, **fields):
        return self.ledger.apply(self.record(kind, **fields))

    def delta(self, gold=0, xp=0, kills=0):
        return self.send("session_delta", gold=gold, xp=xp, kills=kills, source="forgepact_tracker")

    def room(self, room):
        return self.send("room", room=room)

    def zone(self, zone, buffs, debuffs=(4,)):
        return self.send("satanic_zone", zone=zone, buffs=list(buffs), debuffs=list(debuffs))

    def drop_record(self, rarity, name="", tier=None, item_type=None, item_id=None,
                    weapon_type=None, amount=1, mf=False, event_id=None):
        """A `ground_drop` record. The identity (`item_type` with `item_id`)
        is only sent where a test depends on it, the way the Rust tests built
        an event with or without one."""
        item = {"name": name, "rarity": rarity, "amount": amount}
        if tier is not None:
            item["tier"] = tier
        if item_type is not None:
            item["item_type"] = item_type
        if item_id is not None:
            item["item_id"] = item_id
        if weapon_type is not None:
            item["weapon_type"] = weapon_type
        if mf:
            item["magic_find_drop"] = True
        rec = self.record("ground_drop", source="game_ground_item_create", room="Act_01_01", item=item)
        rec["event_id"] = event_id or "fp-4242-%d" % rec["sequence"]
        return rec

    def drop(self, rarity, **kw):
        return self.ledger.apply(self.drop_record(rarity, **kw))

    def save(self, **progress):
        progress.setdefault("kind", "progress")
        return self.ledger.apply_save(progress, self.t)


def tiered_satanic(feed, tier, event_id=None):
    return feed.drop(6, tier=tier, event_id=event_id)


def notable_item(feed, name, item_type, amount):
    return feed.drop(1, name=name, tier=0, item_type=item_type, amount=amount)


def account_xp(season, hardcore, blood_pact, experience, act=0):
    """`stats.rs`'s `account_xp`: a character save, with experience only when
    it is above zero (the save omitting it is a different fact)."""
    p = {"name": "Test", "season": season, "hardcore": hardcore, "blood_pact": blood_pact,
         "level": 10, "hero_level": 20, "difficulty": 2, "hell_sub": 0, "act": act, "kills": 0}
    if experience > 0:
        p["experience"] = experience
    return p


def account_packet(name, kills, experience, tallies=None):
    return {"name": name, "experience": experience, "season": 10, "hardcore": 0, "blood_pact": 0,
            "level": 100, "hero_level": 112, "difficulty": 2, "hell_sub": 0, "act": 0,
            "kills": kills, "tallies": dict(tallies or {})}


class PortedStatsTests(unittest.TestCase):
    """The 31 `stats.rs` tests that carry over, by name."""

    def test_a_room_from_another_act_is_retired(self):
        f = Feed()
        f.save(**account_xp(1, 0, 0, 1, act=8))
        f.room("Act_08_02")
        self.assertEqual(f.ledger.snapshot()["room"], "Act_08_02")

        # a room heard moments ago is fresher than any save
        f.save(**account_xp(1, 0, 0, 1, act=6))
        self.assertEqual(f.ledger.snapshot()["room"], "Act_08_02", "a fresh room outranks the save")

        # once the room has gone quiet the save moves on
        f.at(900).save(**account_xp(1, 0, 0, 1, act=6))
        snap = f.ledger.snapshot()
        self.assertIsNone(snap["room"], "the room was in act 8")
        self.assertEqual(snap["act"], 6, "and the act is all that is known now")

        # a town belongs to its act too
        f.room("Town_06_rm")
        f.at(1800).save(**account_xp(1, 0, 0, 1, act=2))
        self.assertIsNone(f.ledger.snapshot()["room"], "towns are numbered by act")

        # and a room that names no act is left alone, however old it is
        f.room("Shadow_Realm_rm")
        f.at(2700).save(**account_xp(1, 0, 0, 1, act=9))
        self.assertEqual(f.ledger.snapshot()["room"], "Shadow_Realm_rm", "the Shadow Realm belongs to no act")

    def test_items_count_by_rarity_id_and_name(self):
        f = Feed()
        f.drop(6, tier=3, mf=True)
        f.drop("Satanic", tier=3)
        f.drop("satanic", tier=3)
        f.drop(999, tier=3)
        snap = f.ledger.snapshot()
        self.assertEqual(snap["items"]["Satanic"]["total"], 3)
        self.assertEqual(snap["items"]["Satanic"]["mf"], 1)
        self.assertEqual(len(f.ledger.extra()["drops"]), 3)

    def test_a_drop_reaches_the_journal_as_the_item_it_is(self):
        # The record claims Angelic for both; the identity says what each is.
        # (3, 11, 14) is Angel the Set gun, (13, 30, 0) is Justice the card.
        f = Feed()
        gun = f.drop("Angelic", name="Angel", item_type=3, item_id=11, weapon_type=14)
        self.assertIsNotNone(gun, "a Set find is worth announcing")
        self.assertEqual((gun["name"], gun["rarity"], gun["tier"]), ("Angel", "Set", 5))

        card = f.drop("Angelic", name="Justice", item_type=13, item_id=30, weapon_type=0)
        self.assertIsNone(card, "a Common tarot card is not news")

        snap = f.ledger.snapshot()
        self.assertEqual(snap["items"]["Set"]["total"], 1)
        self.assertEqual(snap["items"]["Angelic"]["total"], 0, "which is what the record claimed for both")
        # a collectible is counted as one, and not in a rarity column
        self.assertEqual(f.ledger.resources["collectibles"], 1)
        self.assertEqual(snap["items"]["Common"]["total"], 0)

    def test_a_list_can_name_one_of_the_seven_vaults(self):
        self.assertEqual(items.item_name(19, 5, 0), "Essence Vault")

        def vault(f, which, event_id):
            return f.drop(8, name="Essence Vault", item_type=19, item_id=which, weapon_type=0,
                          event_id=event_id)

        f = Feed()
        f.ledger.set_sound_lists([("list-vault", ["Essence Vault (Angelic)"])])
        angelic = vault(f, 5, "a")
        self.assertIsNotNone(angelic, "the one the list names")
        self.assertEqual(angelic["rarity"], "Angelic")
        self.assertEqual(angelic["sound"], "list-vault")
        self.assertIsNone(vault(f, 0, "b"), "the Superior one is another item")

        # the bare name still means any of them
        f.ledger.set_sound_lists([("list-any", ["Essence Vault"])])
        superior = vault(f, 0, "c")
        self.assertIsNotNone(superior, "any vault")
        self.assertEqual(superior["rarity"], "Superior")
        self.assertEqual(superior["sound"], "list-any")

    def test_float_rarities_are_recognised(self):
        self.assertEqual(items.rarity_from_packet(6.0), "Satanic")
        self.assertEqual(items.rarity_from_packet("9.0"), "Heroic")

    def test_filter_silences_alerts_without_touching_counters(self):
        f = Feed()
        f.ledger.set_filter(["Satanic"], 4)
        self.assertIsNone(tiered_satanic(f, 2), "low tier must not alert")
        self.assertIsNotNone(tiered_satanic(f, 7))
        self.assertIsNone(f.drop(9, tier=3), "a filtered-out rarity")
        snap = f.ledger.snapshot()
        self.assertEqual(snap["items"]["Satanic"]["total"], 2, "counters ignore the filter")
        self.assertEqual(snap["items"]["Heroic"]["total"], 1)

    def test_notable_drops_are_counted_by_name(self):
        f = Feed()
        notable_item(f, "Angelic Key", 12, 2)
        notable_item(f, "Jol", 15, 1)
        notable_item(f, "Zed", 15, 1)
        notable_item(f, "Ol", 15, 1)
        by = {n["label"]: n["total"] for n in f.ledger.snapshot()["notable"]}
        self.assertEqual(by["Angelic Key"], 2)
        self.assertEqual(by["SS runes"], 1, "Jol is one of the level-100 runes")
        self.assertEqual(by["S runes"], 1, "Zed is graded S")

    def test_a_rune_counts_under_either_spelling(self):
        f = Feed()
        notable_item(f, "Ber", 15, 1)
        notable_item(f, "Jah Rune", 15, 1)
        by = {n["label"]: n["total"] for n in f.ledger.snapshot()["notable"]}
        self.assertEqual(by["S runes"], 2, "both spellings land in the same group")

    def test_a_vial_is_gear_and_counts_in_both_columns(self):
        f = Feed()
        f.drop(9, name="Elixir of Unworldly Cognition", tier=6, item_type=18)
        self.assertEqual(f.ledger.snapshot()["items"]["Heroic"]["total"], 1, "a Heroic find")
        self.assertEqual(f.ledger.graded(6), 1, "and an SS one, which is what the columns disagreed about")

    def test_a_list_adds_a_voice_rather_than_taking_the_others_away(self):
        f = Feed()
        # both names are real items the tables call Heroic; a known name
        # outranks whatever the record claims
        f.ledger.set_filter(["Heroic"], 0)
        f.ledger.set_sound_lists([("list-target", ["AK-47"])])
        listed = f.drop(9, name="AK-47", tier=0, item_type=3, weapon_type=14)
        self.assertIsNotNone(listed, "on a list, so it is announced")
        self.assertEqual(listed["sound"], "list-target", "the list's own sound")
        plain = f.drop(9, name="Eternity", tier=0, item_type=3, weapon_type=14)
        self.assertIsNotNone(plain, "on no list, but its rarity is armed")
        self.assertEqual(plain["sound"], "heroic", "still the rarity's sound")

    def test_a_list_outranks_the_rarity_alerts(self):
        f = Feed()
        f.ledger.set_filter([], 6)
        f.ledger.set_sound_lists([("list-chase", ["AK-47"])])
        record = f.drop_record(2, name="AK-47", tier=0, item_type=3, weapon_type=14, event_id="fp-4242-a")
        listed = f.ledger.apply(record)
        self.assertIsNotNone(listed, "a listed item is always announced")
        self.assertEqual(listed["sound"], "list-chase")
        self.assertIsNone(f.drop(2, name="Eternity", tier=0, item_type=3, weapon_type=14),
                          "unlisted items follow the filter")
        # the same record read again is the same item: it has been told
        self.assertIsNone(f.ledger.apply(record), "a list is not told twice about one item")

    def test_the_flourish_asks_its_own_question(self):
        f = Feed()
        f.ledger.set_filter(["Unholy"], 6)
        f.ledger.set_flourish_filter(["Satanic"], 5)
        drop = tiered_satanic(f, 5)
        self.assertIsNotNone(drop, "the flourish wants it")
        self.assertFalse(drop["announce"], "the alert rules did not ask for this one")
        self.assertTrue(drop["flourish"])
        self.assertEqual(f.ledger.extra()["drops"], [], "and it does not join the journal")
        self.assertIsNone(tiered_satanic(f, 4))
        f.ledger.set_flourish_filter([], 6)
        self.assertIsNone(tiered_satanic(f, 5))

    def test_the_flourish_is_seen_and_not_heard(self):
        f = Feed()
        f.ledger.set_filter(["Satanic"], 6)
        f.ledger.set_flourish_filter(["Satanic"], 1)
        drop = tiered_satanic(f, 1)
        self.assertIsNotNone(drop, "the flourish wants it")
        self.assertTrue(drop["flourish"])
        self.assertFalse(drop["announce"], "a D item is below the alerts")
        self.assertIsNone(drop["sound"], "so it must not make a sound either")
        unknown = tiered_satanic(f, 0)
        self.assertIsNotNone(unknown, "D means anything at all")
        self.assertTrue(unknown["flourish"])

    def test_a_finished_run_leaves_the_settings_alone(self):
        f = Feed()
        f.ledger.set_filter(["Satanic"], 6)
        f.ledger.set_flourish_filter(["Satanic"], 1)
        f.ledger.reset(f.t)
        drop = tiered_satanic(f, 1)
        self.assertIsNotNone(drop, "the flourish is still armed")
        self.assertTrue(drop["flourish"])

    def test_bosses_and_chests_count_from_the_first_save_on(self):
        def save(f, satan, odin, ruby):
            f.save(**account_packet("x", 1, 1, {"statisticsatankills": satan, "statisticodinkills": odin,
                                                "statisticrubychestsopened": ruby}))

        def counted(f, label):
            return next((t["total"] for t in f.ledger.tallies() if t["label"] == label), 0)

        f = Feed()
        save(f, 60, 0, 376)
        self.assertEqual(f.ledger.tallies(), [], "the first save only sets the mark")
        save(f, 63, 1, 380)
        self.assertEqual(counted(f, "Satan"), 3)
        self.assertEqual(counted(f, "Ruby"), 4)
        self.assertEqual(counted(f, "Odin"), 1, "a counter that stood at zero still counts its first kill")
        f.ledger.reset(f.t)
        self.assertEqual(f.ledger.tallies(), [], "a reset starts the tally again")
        save(f, 64, 1, 380)
        self.assertEqual(counted(f, "Satan"), 1, "and the game is not made to recount")
        self.assertEqual(counted(f, "Odin"), 0)

    def test_the_session_tallies_drops_by_grade(self):
        f = Feed()
        tiered_satanic(f, 6)
        notable_item(f, "Angelic Key", 12, 1)
        self.assertEqual(f.ledger.graded(6), 1, "a key is not an SS find")
        tiered_satanic(f, 3)
        notable_item(f, "Mystery Blade", 3, 1)
        self.assertEqual(f.ledger.graded(3), 1)
        self.assertEqual(f.ledger.graded(6), 1, "an item the table cannot grade is not an SS")
        self.assertEqual(f.ledger.snapshot()["ss"], 1)
        f.ledger.reset(f.t)
        self.assertEqual(f.ledger.graded(6), 0, "the tally belongs to the session")

    def test_a_named_drop_is_graded_by_the_item_table(self):
        f = Feed()
        f.ledger.set_filter(["Satanic", "Heroic"], 6)
        ss = f.drop(2, name="AK-47", tier=0, item_type=3, weapon_type=14)
        self.assertIsNotNone(ss, "an SS drop passes an SS filter")
        self.assertEqual(ss["tier"], 6)
        self.assertIsNone(f.drop(2, name="Sky Crusader Helm", tier=0, item_type=3, weapon_type=14),
                          "tier C is below SS")
        self.assertIsNone(f.drop(2, name="Mystery Blade", tier=0, item_type=3, weapon_type=14),
                          "an ungraded item stays quiet")

    def test_without_a_minimum_tier_the_drop_itself_is_announced(self):
        f = Feed()
        f.ledger.set_filter(["Heroic"], 0)
        entry = f.drop(2, name="AK-47", tier=0, item_type=3, weapon_type=14, mf=True)
        self.assertIsNotNone(entry, "an SS drop must be announced")
        self.assertEqual(entry["rarity"], "Heroic", "the record claims Superior; the name decides")
        self.assertTrue(entry["ground"])

    def test_extreme_gold_and_rate_values_saturate_instead_of_overflowing(self):
        f = Feed()
        f.delta(gold=I64_MAX)
        f.delta(gold=I64_MAX)
        self.assertEqual(f.ledger.snapshot()["gold"]["earned"], I64_MAX)
        f.at(1).send("heartbeat")
        self.assertEqual(f.ledger.per_hour(I64_MAX), I64_MAX)

    def test_the_room_a_reset_carries_starts_its_clock(self):
        f = Feed()
        f.room("Act_09_06")
        f.at(30)
        f.ledger.reset(f.t)
        self.assertEqual(f.ledger.room, "Act_09_06")
        self.assertEqual(f.ledger.room_since_ms, f.t, "the carried room is being timed, from the reset")

    def test_a_paused_run_does_not_credit_a_room_with_the_pause(self):
        # 1500 seconds on the clock, 900 of them paused: a 600-second run
        f = Feed()
        f.room("Act_07_03")
        f.delta(xp=20_000, kills=150)
        f.at(250).delta(xp=20_000, kills=150)
        f.at(500).delta(xp=10_000, kills=100)

        f.ledger.set_paused(True, f.at(600).t)
        f.at(700).room("Town_01")
        self.assertIsNone(f.ledger.room_since_ms, "a room change must not restart a stopped clock")
        self.assertEqual(f.ledger.room, "Town_01", "the panel still says where we are")

        # a room clock left running by any other route banks nothing while held
        f.ledger.room_since_ms = T0 + 600_000
        f.at(1500).room("Act_07_03")
        f.ledger.set_paused(False, f.t)

        run = f.ledger.finish(f.t)
        self.assertIsNotNone(run, "a session with earnings is worth keeping")
        self.assertEqual(run["secs"], 600)
        banked = sum(secs for _, secs in run["zones"])
        self.assertLessEqual(banked, run["secs"], run["zones"])
        self.assertFalse(any(room == "Town_01" for room, _ in run["zones"]),
                         "the paused town trip is not where the run happened")

    def test_a_glance_at_the_app_is_not_a_run(self):
        self.assertIsNone(Feed().ledger.finish(T0), "nothing happened and no time passed")
        f = Feed()
        f.at(120).send("heartbeat")
        f.at(240).send("heartbeat")
        self.assertIsNone(f.ledger.finish(f.t), "an idle session is not a run either")

    def test_a_drop_only_session_is_a_run(self):
        f = Feed()
        f.drop(6, tier=3)
        f.at(90).send("heartbeat")
        run = f.ledger.finish(f.t)
        self.assertIsNotNone(run, "a tracked drop is a run even without XP, gold or kills")
        self.assertEqual((run["gold"], run["xp"], run["kills"]), (0, 0, 0))
        self.assertEqual(run["items"]["Satanic"], 1)

    def test_a_finished_run_carries_the_session_and_where_it_happened(self):
        f = Feed()
        f.send("progress", name="Test")
        f.delta(xp=10_000, kills=100)
        f.at(100).room("Act_07_02")
        f.at(200).delta(xp=20_000, kills=150)
        f.at(400).delta(xp=20_000, kills=150)
        f.at(500).room("Act_07_03")
        f.at(600)
        run = f.ledger.finish(f.t)
        self.assertIsNotNone(run, "a session with earnings is worth keeping")
        self.assertEqual(run["kills"], 400)
        self.assertEqual(run["xp"], 50_000)
        self.assertGreaterEqual(run["secs"], 600)
        self.assertEqual(run["character"], "Test")
        self.assertEqual(run["zones"][0][0], "Act_07_02", "the room it spent longest in comes first")
        self.assertGreaterEqual(run["zones"][0][1], 300, run["zones"])
        self.assertEqual((run["started_ms"], run["ended_ms"]), (T0, T0 + 600_000))

    def test_five_quiet_minutes_stop_the_clock(self):
        f = Feed()
        f.ledger.watch_idle(T0)
        self.assertFalse(f.ledger.paused, "a run that has just started is not idle")
        f.ledger.watch_idle(T0 + IDLE_AFTER_MS + 1000)
        self.assertTrue(f.ledger.paused, "quiet for longer than the limit")
        self.assertEqual(f.ledger.paused_at_ms, T0, "stopped as of the last sign of life")

        # anything the run does lifts it, without being asked to
        f.at(302).delta(kills=1)
        self.assertFalse(f.ledger.paused, "a kill, a drop or a coin is a sign of life")

        # a pause the player asked for outranks it
        f.ledger.set_paused(True, f.at(303).t)
        f.at(304).delta(kills=1)
        self.assertTrue(f.ledger.paused, "a hand-made pause lasts until the same hand lifts it")
        f.ledger.set_paused(False, f.at(305).t)
        self.assertFalse(f.ledger.paused)

    def test_a_reset_clears_the_session_but_keeps_the_character(self):
        f = Feed()
        f.save(**account_xp(10, 1, 0, 0))
        f.delta(xp=15)
        f.ledger.reset(f.t)
        snap = f.ledger.snapshot()
        self.assertEqual(snap["xp"]["earned"], 0)
        self.assertEqual(snap["character"]["name"], "Test")

    def test_only_a_real_rotation_announces_the_satanic_zone(self):
        f = Feed()
        f.zone("Act_08_02", [1, 2, 3])
        self.assertIsNone(f.ledger.take_zone_change(), "learning where the zone is is not it moving")
        f.zone("Act_08_02", [1, 2, 3])
        self.assertIsNone(f.ledger.take_zone_change(), "the same zone said twice is one zone")
        f.zone("Act_03_01", [1, 2, 3])
        moved = f.ledger.take_zone_change()
        self.assertIsNotNone(moved, "the zone moved")
        self.assertEqual(moved["zone"], "Act_03_01", "and it says which zone it moved to")
        self.assertIsNone(f.ledger.take_zone_change(), "announced once, not until the next look")

    def test_a_reroll_onto_the_same_zone_is_still_a_rotation(self):
        f = Feed()
        f.zone("Act_08_02", [1, 2, 3])
        f.ledger.take_zone_change()
        f.zone("Act_08_02", [3, 2, 1])
        self.assertIsNone(f.ledger.take_zone_change(), "the same buffs in another order are the same roll")
        f.zone("Act_08_02", [1, 2, 14])
        self.assertIsNotNone(f.ledger.take_zone_change(), "the same room with a different set on it rotated")

    def test_the_buff_pick_narrows_the_alert_and_an_empty_pick_narrows_nothing(self):
        f = Feed()
        f.zone("Act_01_01", [7])
        f.ledger.take_zone_change()
        self.assertEqual(f.ledger.prefs.zone_buffs, [])
        f.zone("Act_02_02", [7])
        self.assertIsNotNone(f.ledger.take_zone_change(), "an empty pick lets every rotation through")
        f.zone("Act_02_03", [])
        self.assertIsNotNone(f.ledger.take_zone_change(), "including one the game gave no buffs at all")

        f.ledger.prefs.zone_buffs = [14, 15, 16]
        f.zone("Act_03_01", [7, 21])
        self.assertIsNone(f.ledger.take_zone_change(), "none of the three: it passes in silence")
        f.zone("Act_03_02", [])
        self.assertIsNone(f.ledger.take_zone_change(), "a zone with no buffs cannot match a pick")
        f.zone("Act_04_01", [7, 15])
        hit = f.ledger.take_zone_change()
        self.assertIsNotNone(hit, "one of the three is enough")
        self.assertEqual(hit["buffs"], [7, 15], "and the alert carries what the zone rolled")
        f.zone("Act_05_01", [21])
        self.assertIsNone(f.ledger.take_zone_change())
        self.assertIsNone(f.ledger.take_zone_change(), "a filtered rotation is not left pending")

    def test_native_session_deltas_are_immediate_and_not_recounted_by_the_save(self):
        f = Feed()
        f.save(**account_packet("Sgham", 44_000, 1_000))
        f.delta(gold=75, xp=200, kills=1)
        live = f.ledger.snapshot()
        self.assertEqual((live["gold"]["earned"], live["xp"]["earned"], live["kills"]["earned"]), (75, 200, 1))

        # the delayed save holds the same XP and kill: totals move, earnings do not
        f.save(**account_packet("Sgham", 44_001, 1_200))
        saved = f.ledger.snapshot()
        self.assertEqual(saved["xp"]["earned"], 200)
        self.assertEqual(saved["xp"]["total"], 1_200)
        self.assertEqual(saved["kills"]["earned"], 1)
        self.assertEqual(saved["kills"]["total"], 44_001)
        self.assertIsNone(saved["gold"]["total"], "no gold total is read in ForgePact")

        # and a save the sensor missed something in still adds nothing
        f.save(**account_packet("Sgham", 44_500, 90_000))
        self.assertEqual(f.ledger.snapshot()["kills"]["earned"], 1)

    def test_the_heavy_payload_moves_only_when_it_has_something_new(self):
        f = Feed()
        quiet = f.ledger.extra_revision
        f.room("Act_07_03")
        f.send("vitals", satanic_here=False)
        f.delta(gold=500)
        f.delta(xp=15)
        self.assertEqual(f.ledger.extra_revision, quiet, "a heartbeat adds nothing to the journal")
        self.assertGreater(f.ledger.revision, quiet, "the counters themselves did move")
        self.assertIsNotNone(f.drop(6, name="Azazel's Despair", tier=3))
        self.assertGreater(f.ledger.extra_revision, quiet, "a journalled drop is something new")


class ForgePactRuleTests(unittest.TestCase):
    """Rules of ForgePact's own, one test each."""

    def test_event_time_alone_drives_every_duration(self):
        def no_wall_clock(*_a, **_k):
            raise AssertionError("the ledger read the wall clock")

        with patch("time.time", no_wall_clock), patch("time.monotonic", no_wall_clock), \
                patch("time.perf_counter", no_wall_clock), patch("time.time_ns", no_wall_clock):
            f = Feed()
            f.room("Act_02_01")
            for minute in range(0, 31, 2):
                f.at(minute * 60).delta(gold=100, kills=1)
            snap = f.ledger.snapshot()
            run = f.ledger.finish(f.t)
        self.assertEqual(snap["session_secs"], 1800)
        self.assertEqual(snap["kills"]["earned"], 16)
        self.assertEqual(snap["kills"]["per_hour"], 32)
        self.assertEqual(snap["gold"]["per_hour"], 3200)
        self.assertEqual(run["zones"], [["Act_02_01", 1800]])

    def test_a_quiet_stretch_is_cut_from_the_clock_by_the_records_alone(self):
        f = Feed()
        f.delta(kills=1)
        f.at(10).delta(kills=1)
        # ten minutes of heartbeats and nothing else, then a kill: the idle
        # pause is back-dated to the last kill, as stats.rs's watch_idle does
        for secs in range(20, 601, 10):
            f.at(secs).send("heartbeat")
        f.at(610).delta(kills=1)
        self.assertEqual(f.ledger.snapshot()["session_secs"], 10,
                         "the quiet ten minutes are not part of the run")

    def test_a_progress_naming_another_character_ends_the_run(self):
        f = Feed()
        f.send("progress", name="Alpha")
        f.delta(gold=10, xp=100, kills=5)
        f.at(120).send("progress", name="Alpha")
        self.assertEqual(f.ledger.take_finished_runs(), [], "the same character changes nothing")
        f.at(180).send("progress", name="Beta")
        finished = f.ledger.take_finished_runs()
        self.assertEqual(len(finished), 1)
        key, run = finished[0]
        self.assertEqual(run["character"], "Alpha")
        self.assertEqual((run["gold"], run["xp"], run["kills"]), (10, 100, 5))
        self.assertEqual(run["ended_ms"], T0 + 180_000)
        self.assertTrue(key.startswith(FILE + "#"), key)
        snap = f.ledger.snapshot()
        self.assertEqual(snap["character"]["name"], "Beta")
        self.assertEqual(snap["kills"]["earned"], 0)
        self.assertEqual(f.ledger.started_ms, T0 + 180_000, "the new run starts at that record")

    def test_a_save_for_another_character_than_the_sensor_names_is_not_applied(self):
        f = Feed()
        f.send("progress", name="Alpha")
        f.save(**account_packet("Beta", 10, 10))
        snap = f.ledger.snapshot()
        self.assertEqual(snap["character"]["name"], "Alpha")
        self.assertIsNone(snap["xp"]["total"])
        f.save(**account_packet("Alpha", 7, 70))
        snap = f.ledger.snapshot()
        self.assertEqual((snap["character"]["level"], snap["xp"]["total"], snap["kills"]["total"]), (100, 70, 7))

    def test_a_journal_entry_names_its_session_and_sequence(self):
        f = Feed()
        entry = f.drop("Unholy", name="", tier=6)
        self.assertEqual(entry["file"], FILE)
        self.assertEqual(entry["sequence"], f.seq)
        self.assertEqual(entry["id"], "%s:%d" % (FILE, f.seq))
        self.assertEqual(entry["ts_ms"], f.t)
        self.assertEqual(f.ledger.extra()["drops"][0]["id"], entry["id"])

    def test_the_series_is_sampled_on_active_event_time(self):
        f = Feed()
        for secs in range(0, 181, 30):
            f.at(secs).delta(gold=10, xp=1)
        series = f.ledger.extra()["series"]
        self.assertEqual([p["t"] for p in series], [60, 120, 180])
        self.assertEqual(series[-1]["gold"], 70)

    def test_the_snapshot_leaves_out_what_the_offline_tracker_cannot_know(self):
        snap = Feed().ledger.snapshot()
        for gone in ("has_mail", "bank_age_secs", "carried_bank", "carried_totals", "resources", "mf"):
            self.assertNotIn(gone, snap)
        for kept in ("status", "session_secs", "paused", "save_age_secs", "gold", "xp", "kills", "notable",
                     "items", "satanic_zone", "satanic_at", "room", "act", "satanic_here", "character",
                     "tallies", "ss", "revision", "extra_revision"):
            self.assertIn(kept, snap)
        self.assertEqual(sorted(snap["items"]), sorted(["Common", "Superior", "Rare", "Set", "Mythic",
                                                        "Satanic", "Angelic", "Blessed", "Heroic", "Unholy"]))


if __name__ == "__main__":
    unittest.main()
