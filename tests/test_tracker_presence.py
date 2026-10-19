#!/usr/bin/env python3
"""Discord Rich Presence for the Tracker tab (`src/forgepact_tracker/presence.py`, #185).

No test talks to the real Discord. The worker is driven step by step with a
fake clock and a fake transport, and one test runs the real named-pipe
transport against a pipe this test serves itself. Pinned here:

- baseline: with no application ID the panel starts no thread and
  `/api/tracker` answers `presence: {"available": false}`; with an ID and the
  settings' `discord` false, nothing connects and nothing is sent;
- target: the handshake (op 0, `{"v": 1, "client_id"}`) and one
  `SET_ACTIVITY` (op 1) carrying the panel's pid, the zone and the haul;
- at most one activity per 4 s, and a clear when the game stops or the
  switch goes off;
- a reconnect with capped backoff while the pipe is absent;
- the tracker lock free during every transport call (the fake asserts it
  from a second thread, which works for a `Lock` and an `RLock` alike);
- `stop()` ending the daemon thread `forgepact-presence` within a second.
"""

import inspect
import json
import os
import sys
import threading
import time
import unittest
import uuid
from pathlib import Path
from unittest.mock import patch

TESTS = Path(__file__).resolve().parent
ROOT = TESTS.parent
sys.path.insert(0, str(ROOT / "src"))
sys.path.insert(0, str(TESTS))

import forgepact  # noqa: E402
from forgepact_tracker import presence  # noqa: E402

APP_ID = "123456789012345678"


def lock_is_free(lock) -> bool:
    """Whether another thread could take `lock` now. Asked from a second
    thread, so an RLock the caller holds reads as held."""
    answer = []

    def probe():
        if lock.acquire(blocking=False):
            lock.release()
            answer.append(True)
        else:
            answer.append(False)

    t = threading.Thread(target=probe)
    t.start()
    t.join(2)
    return bool(answer and answer[0])


class FakeDiscord:
    """Stands in for the Discord client: `connect()` is the transport factory
    the worker is given. Records every frame and answers as Discord does."""

    def __init__(self, lock):
        self.lock = lock
        self.absent = False          # no pipe: connect raises
        self.silent = False          # accepts writes, never answers
        self.connects = []           # clock time of each attempt
        self.frames = []             # (op, payload) written
        self.closed = 0
        self.lock_held_during_io = 0
        self.clock = None
        self._pending = []

    def _io(self):
        if not lock_is_free(self.lock):
            self.lock_held_during_io += 1

    def connect(self):
        self._io()
        self.connects.append(self.clock() if self.clock else None)
        if self.absent:
            raise FileNotFoundError("no discord-ipc pipe")
        return FakeConnection(self)

    def activities(self):
        """Every SET_ACTIVITY's activity, None for a clear."""
        return [p["args"].get("activity") for op, p in self.frames
                if op == presence.OP_FRAME and p.get("cmd") == "SET_ACTIVITY"]


class FakeConnection:
    def __init__(self, discord):
        self.d = discord

    def write(self, op, payload):
        self.d._io()
        self.d.frames.append((op, payload))
        if self.d.silent:
            return
        if op == presence.OP_HANDSHAKE:
            self.d._pending.append((presence.OP_FRAME, {"cmd": "DISPATCH", "evt": "READY", "data": {"v": 1}}))
        elif op == presence.OP_FRAME:
            self.d._pending.append((presence.OP_FRAME, {"cmd": payload["cmd"], "evt": None,
                                                        "nonce": payload.get("nonce"), "data": {}}))

    def read(self, timeout):
        self.d._io()
        if not self.d._pending:
            raise TimeoutError("no answer in %.1f s" % timeout)
        return self.d._pending.pop(0)

    def close(self):
        self.d._io()
        self.d.closed += 1


def snapshot(room="Act_01_01", act=1, gold=1267, ss=3, angelic=1, unholy=0, secs=600, file="session-777.ndjson",
             zone=None, here=False):
    items = {}
    if angelic:
        items["Angelic"] = {"total": angelic, "mf": 0, "per_hour": 0}
    if unholy:
        items["Unholy"] = {"total": unholy, "mf": 0, "per_hour": 0}
    return {"room": room, "act": act, "gold": {"total": None, "earned": gold, "per_hour": 0},
            "items": items, "ss": ss, "session_secs": secs,
            "satanic_zone": {"zone": zone, "buffs": [], "debuffs": []} if zone else None,
            "satanic_here": here,
            "character": {"name": "Sorak", "level": 40, "herolevel": 12, "difficulty": 2, "hardcore": False},
            "session": {"file": file, "pid": 777, "live": True}}


class Rig:
    """One worker on a fake clock, its view taken under a tracker lock."""

    def __init__(self, test, discord_on=True, running=True):
        self.lock = threading.RLock()
        self.now = 1000.0
        self.discord = FakeDiscord(self.lock)
        self.discord.clock = lambda: self.now
        self.on = discord_on
        self.running = running
        self.snap = snapshot()
        self.views = 0
        self.worker = presence.Presence(APP_ID, self.view, connect=self.discord.connect,
                                        clock=lambda: self.now, wall_ms=lambda: 1_760_000_000_000,
                                        pid=4242)

    def view(self):
        with self.lock:
            self.views += 1
            if not self.on:
                return None
            return {"running": self.running, "snapshot": json.loads(json.dumps(self.snap))}

    def run(self, seconds, step=1.0):
        end = self.now + seconds
        while self.now < end:
            self.worker.step()
            self.now += step


class AppIdTest(unittest.TestCase):
    def test_no_id_means_not_available(self):
        with patch.object(presence, "APP_ID", ""):
            self.assertIsNone(presence.app_id({}))
            self.assertFalse(presence.available({}))

    def test_the_variable_must_hold_a_16_to_24_digit_id(self):
        with patch.object(presence, "APP_ID", ""):
            self.assertEqual(presence.app_id({presence.APP_ID_ENV: APP_ID}), APP_ID)
            self.assertEqual(presence.app_id({presence.APP_ID_ENV: "  %s\n" % APP_ID}), APP_ID)
            for bad in ("", "1234", "hs-tracker-app-id", "1" * 25, "12345678901234567x"):
                self.assertIsNone(presence.app_id({presence.APP_ID_ENV: bad}), bad)

    def test_a_shipped_constant_is_used_when_the_variable_is_unset(self):
        with patch.object(presence, "APP_ID", "987654321098765432"):
            self.assertEqual(presence.app_id({}), "987654321098765432")
            self.assertEqual(presence.app_id({presence.APP_ID_ENV: APP_ID}), APP_ID, "the variable wins")


class FrameTest(unittest.TestCase):
    def test_a_frame_is_two_little_endian_int32_then_json(self):
        raw = presence.encode_frame(presence.OP_HANDSHAKE, {"v": 1, "client_id": APP_ID})
        body = json.dumps({"v": 1, "client_id": APP_ID}, separators=(",", ":")).encode("utf-8")
        self.assertEqual(raw[:4], b"\x00\x00\x00\x00")
        self.assertEqual(int.from_bytes(raw[4:8], "little"), len(body))
        self.assertEqual(raw[8:], body)
        self.assertEqual(presence.decode_header(raw[:8]), (0, len(body)))


class BaselineTest(unittest.TestCase):
    def test_switch_off_connects_to_nothing_and_sends_nothing(self):
        rig = Rig(self, discord_on=False)
        rig.run(60)
        self.assertGreater(rig.views, 0, "the worker did look")
        self.assertEqual(rig.discord.connects, [])
        self.assertEqual(rig.discord.frames, [])

    def test_game_not_running_connects_to_nothing(self):
        rig = Rig(self, running=False)
        rig.run(60)
        self.assertEqual(rig.discord.connects, [])
        self.assertEqual(rig.discord.frames, [])

    def test_no_id_starts_no_thread_and_the_api_says_unavailable(self):
        from test_tracker_panel_contract import TrackerSandbox
        with patch.dict(os.environ), patch.object(presence, "APP_ID", ""):
            os.environ.pop(presence.APP_ID_ENV, None)
            before = {t.name for t in threading.enumerate()}
            self.assertIsNone(forgepact.start_tracker_presence())
            self.assertNotIn(presence.THREAD_NAME, {t.name for t in threading.enumerate()} - before)
            box = TrackerSandbox(self)
            status, body = box.call("GET", "/api/tracker")
        self.assertEqual(status, 200)
        self.assertEqual(body["presence"], {"available": False})

    def test_the_view_builds_no_tracker_store_while_the_tracker_is_off(self):
        with patch.object(forgepact, "load_cfg", return_value={"mod_tracker": False}), \
                patch.object(forgepact, "tracker_service", side_effect=AssertionError("built the store")):
            self.assertIsNone(forgepact.tracker_presence_view())


class TargetTest(unittest.TestCase):
    def test_handshake_then_one_activity_with_the_zone_and_the_haul(self):
        rig = Rig(self)
        rig.run(1)
        frames = rig.discord.frames
        self.assertEqual(frames[0], (presence.OP_HANDSHAKE, {"v": 1, "client_id": APP_ID}))
        self.assertEqual(len(frames), 2, frames)
        op, payload = frames[1]
        self.assertEqual(op, presence.OP_FRAME)
        self.assertEqual(payload["cmd"], "SET_ACTIVITY")
        self.assertTrue(payload["nonce"])
        self.assertEqual(payload["args"]["pid"], 4242)
        activity = payload["args"]["activity"]
        self.assertEqual(activity["details"], "Act 1 · Outskirts of Inoya · Hell")
        # 3 graded SS less the 1 Angelic counted by name, the Angelic, the gold
        self.assertEqual(activity["state"], "2 SS · 1 Angelic · 1.3k gold")
        self.assertEqual(activity["timestamps"], {"start": 1_760_000_000_000 - 600 * 1000})

    def test_nothing_earned_yet_reads_just_started(self):
        card = presence.build_card(snapshot(gold=0, ss=0, angelic=0), 0)
        self.assertEqual(card["state"], "just started")

    def test_no_act_and_no_room_is_still_a_line(self):
        card = presence.build_card(dict(snapshot(), room=None, act=0, character=None), 0)
        self.assertEqual(card["details"], "Somewhere in Hero Siege")

    def test_the_satanic_zone_badge_only_while_standing_in_it(self):
        here = presence.build_card(snapshot(zone="Act_01_01", here=True), 0)
        self.assertEqual(here["assets"]["small_text"], "Satanic Zone · Outskirts of Inoya")
        away = presence.build_card(snapshot(zone="Act_01_01", here=False), 0)
        self.assertNotIn("small_image", away["assets"])

    def test_long_numbers_shorten(self):
        self.assertEqual(presence.compact(940), "940")
        self.assertEqual(presence.compact(7317), "7.3k")
        self.assertEqual(presence.compact(42000), "42k")
        self.assertEqual(presence.compact(2_400_000), "2.4M")
        self.assertEqual(presence.compact(3_140_000_000), "3.1B")


class PacingTest(unittest.TestCase):
    def test_no_more_than_one_activity_per_four_seconds(self):
        rig = Rig(self)
        sent_at = []
        gold = 0
        while rig.now < 1030:
            gold += 100           # the haul moves every second
            rig.snap["gold"]["earned"] = gold
            before = len(rig.discord.activities())
            rig.worker.step()
            if len(rig.discord.activities()) > before:
                sent_at.append(rig.now)
            rig.now += 1.0
        self.assertGreaterEqual(len(sent_at), 6)
        gaps = [b - a for a, b in zip(sent_at, sent_at[1:])]
        self.assertTrue(all(g >= presence.SEND_GAP for g in gaps), gaps)

    def test_an_unchanged_card_is_not_sent_again(self):
        rig = Rig(self)
        rig.run(30)
        self.assertEqual(len(rig.discord.activities()), 1)

    def test_cleared_when_the_game_stops(self):
        rig = Rig(self)
        rig.run(5)
        rig.running = False
        rig.run(2)
        self.assertEqual(rig.discord.activities()[-1], None, "the last frame clears the activity")
        self.assertEqual(rig.discord.closed, 1)
        frames = len(rig.discord.frames)
        rig.run(30)
        self.assertEqual(len(rig.discord.frames), frames, "nothing more while the game is stopped")

    def test_cleared_when_the_switch_goes_off(self):
        rig = Rig(self)
        rig.run(5)
        rig.on = False
        rig.run(2)
        self.assertEqual(rig.discord.activities()[-1], None)
        self.assertEqual(rig.discord.closed, 1)

    def test_shown_again_when_the_game_comes_back(self):
        rig = Rig(self)
        rig.run(5)
        rig.running = False
        rig.run(5)
        rig.running = True
        rig.run(5)
        acts = rig.discord.activities()
        self.assertEqual([a is None for a in acts], [False, True, False])
        self.assertEqual(len(rig.discord.connects), 2)


class ReconnectTest(unittest.TestCase):
    def test_backoff_while_the_pipe_is_absent_capped_at_30_s(self):
        rig = Rig(self)
        rig.discord.absent = True
        rig.run(400)
        tries = rig.discord.connects
        gaps = [b - a for a, b in zip(tries, tries[1:])]
        self.assertGreater(len(tries), 5)
        self.assertEqual(gaps, sorted(gaps), "the wait never shrinks while it keeps failing")
        self.assertLessEqual(max(gaps), presence.RETRY_CAP + 1)
        self.assertGreaterEqual(gaps[-1], presence.RETRY_CAP, "and it reaches the cap")
        self.assertLess(gaps[0], presence.RETRY_CAP, "starting below it")
        self.assertEqual(rig.discord.frames, [])
        # Discord starts: the next try connects and shows the card
        rig.discord.absent = False
        rig.run(presence.RETRY_CAP + 2)
        self.assertEqual(len(rig.discord.activities()), 1)

    def test_a_silent_discord_is_written_off_and_retried(self):
        rig = Rig(self)
        rig.discord.silent = True
        rig.run(3)
        self.assertEqual(len(rig.discord.connects), 1)
        self.assertEqual(rig.discord.closed, 1, "the connection that never answered is closed")
        rig.discord.silent = False
        rig.run(presence.RETRY_CAP + 2)
        self.assertEqual(len(rig.discord.connects), 2)
        self.assertEqual(len(rig.discord.activities()), 1, "the handshake never answered, so no card went out")

    def test_an_error_answer_drops_the_connection(self):
        rig = Rig(self)
        rig.run(1)
        conn_frames = len(rig.discord.frames)
        rig.discord._pending.clear()
        orig = FakeConnection.write

        def failing(self_, op, payload):
            orig(self_, op, payload)
            if op == presence.OP_FRAME:
                self_.d._pending[-1] = (presence.OP_FRAME, {"cmd": "SET_ACTIVITY", "evt": "ERROR",
                                                          "nonce": payload.get("nonce"),
                                                          "data": {"code": 4000, "message": "bad"}})
        with patch.object(FakeConnection, "write", failing):
            rig.snap["gold"]["earned"] = 99999
            rig.run(5)
        self.assertGreater(len(rig.discord.frames), conn_frames)
        self.assertEqual(rig.discord.closed, 1)


class LockTest(unittest.TestCase):
    def test_the_tracker_lock_is_free_during_every_transport_call(self):
        rig = Rig(self)
        rig.discord.absent = True
        rig.run(40)
        rig.discord.absent = False
        rig.run(40)
        rig.snap["gold"]["earned"] = 5000
        rig.run(10)
        rig.running = False
        rig.run(2)
        self.assertGreater(len(rig.discord.frames), 3)
        self.assertEqual(rig.discord.lock_held_during_io, 0)

    def test_the_lock_is_really_probed(self):
        # negative control: the probe does see a lock this thread holds
        lock = threading.RLock()
        with lock:
            self.assertFalse(lock_is_free(lock))
        self.assertTrue(lock_is_free(lock))


class ThreadTest(unittest.TestCase):
    def test_one_daemon_thread_that_stop_ends_within_a_second(self):
        rig = Rig(self)
        worker = presence.Presence(APP_ID, rig.view, connect=rig.discord.connect, pid=4242)
        worker.start()
        try:
            self.assertTrue(worker.thread.daemon)
            self.assertEqual(worker.thread.name, presence.THREAD_NAME)
            deadline = time.monotonic() + 5
            while not rig.discord.activities() and time.monotonic() < deadline:
                time.sleep(0.05)
            self.assertTrue(rig.discord.activities(), "the thread sent the card")
        finally:
            began = time.monotonic()
            worker.stop()
            took = time.monotonic() - began
        self.assertFalse(worker.thread.is_alive())
        self.assertLess(took, 1.0)
        self.assertEqual(rig.discord.activities()[-1], None, "stopping clears the activity")

    def test_main_starts_it_through_start_tracker_presence(self):
        source = inspect.getsource(forgepact.main)
        self.assertIn("start_tracker_presence()", source)
        self.assertLess(source.index("start_tracker_presence()"), source.index("webview.start()"))

    def test_with_an_id_the_panel_starts_one_named_daemon(self):
        discord = FakeDiscord(threading.RLock())
        with patch.dict(os.environ, {presence.APP_ID_ENV: APP_ID}), \
                patch.object(forgepact, "tracker_presence_view", return_value=None), \
                patch.object(forgepact, "_PRESENCE", None), \
                patch.object(presence, "PipeTransport", lambda: discord):
            worker = forgepact.start_tracker_presence()
            try:
                self.assertIsNotNone(worker)
                self.assertTrue(worker.thread.daemon and worker.thread.is_alive())
                self.assertIs(forgepact.start_tracker_presence(), worker, "one thread, not one per call")
                self.assertEqual(forgepact.tracker_presence_state(),
                                 {"available": True, "connected": False, "showing": False})
            finally:
                worker.stop()
        self.assertEqual(discord.connects, [], "the switch was off")


@unittest.skipUnless(os.name == "nt", "Discord's IPC is a Windows named pipe")
class NamedPipeTest(unittest.TestCase):
    """The real transport against a pipe this test serves, as Discord does."""

    def serve(self, answer):
        from multiprocessing.connection import Listener
        name = r"\\.\pipe\forgepact-test-%s" % uuid.uuid4().hex
        listener = Listener(name, family="AF_PIPE")
        got = []

        def run():
            conn = listener.accept()
            try:
                data = conn.recv_bytes()
                op, length = presence.decode_header(data[:8])
                got.append((op, json.loads(data[8:8 + length])))
                if answer:
                    conn.send_bytes(presence.encode_frame(presence.OP_FRAME, {"cmd": "DISPATCH", "evt": "READY"}))
                time.sleep(1.5)
            finally:
                conn.close()

        t = threading.Thread(target=run, daemon=True)
        t.start()
        self.addCleanup(listener.close)
        return name, got, t

    def test_the_handshake_is_written_and_the_answer_read(self):
        name, got, t = self.serve(answer=True)
        conn = presence.PipeTransport([name]).connect()
        try:
            conn.write(presence.OP_HANDSHAKE, {"v": 1, "client_id": APP_ID})
            op, payload = conn.read(5)
        finally:
            conn.close()
        t.join(3)
        self.assertEqual(got, [(0, {"v": 1, "client_id": APP_ID})])
        self.assertEqual((op, payload["evt"]), (presence.OP_FRAME, "READY"))

    def test_a_read_with_no_answer_times_out(self):
        name, got, t = self.serve(answer=False)
        conn = presence.PipeTransport([name]).connect()
        try:
            conn.write(presence.OP_HANDSHAKE, {"v": 1, "client_id": APP_ID})
            began = time.monotonic()
            with self.assertRaises(TimeoutError):
                conn.read(0.5)
            self.assertLess(time.monotonic() - began, 1.4)
        finally:
            conn.close()

    def test_no_pipe_is_an_oserror(self):
        with self.assertRaises(OSError):
            presence.PipeTransport([r"\\.\pipe\forgepact-test-absent-%s" % uuid.uuid4().hex]).connect()


if __name__ == "__main__":
    unittest.main()
