#!/usr/bin/env python3
"""Session-file ingest and finalization for the tracker ledger (#185, part 2a).

The plugin's `tracker` mod appends protocol-v1 records to one file per game
process, `bp_ipc\\tracker\\session-<pid>-<unix ms>.ndjson`. The panel reads
those files read-only, and what it derives is a pure function of the file plus
the controls it persisted itself (`cursor.json`). Pinned here:

- a partial last line waits for its newline, and a malformed line, one with an
  unknown kind or another protocol version is counted and skipped;
- a session played while the panel was closed is caught up from event times,
  and its run is filed;
- re-reading a session after a restart gives identical totals and journal;
- a finalized session is filed once, however often it is read again;
- retention keeps the newest 20 finalized session files and never removes an
  unfinalized one or a live pid's;
- one ingest reads a bounded number of bytes, and the snapshot says
  `catching_up` while bytes are left;
- whether a pid is live is asked of an injected callable, never of the OS.

Every root is a temporary directory; nothing here reads the real game folder,
`%LOCALAPPDATA%` or the Tracker's data.
"""

import json
import sys
import tempfile
import unittest
from pathlib import Path

TESTS = Path(__file__).resolve().parent
sys.path.insert(0, str(TESTS))
sys.path.insert(0, str(TESTS.parent / "src"))

from forgepact_tracker.protocol import PROTOCOL_ID  # noqa: E402
from forgepact_tracker.service import TrackerService  # noqa: E402

T0 = 1_760_000_000_000


def line(kind, seq, t, pid=4242, **fields):
    rec = {"protocol": PROTOCOL_ID, "v": 1, "kind": kind, "sequence": seq, "emitted_at_unix_ms": t,
           "process_id": pid, "build_id": "forgepact-test"}
    rec.update(fields)
    return (json.dumps(rec, separators=(",", ":")) + "\n").encode("utf-8")


def farm(pid, start, minutes, kills_per_record=1):
    """A session's worth of records: a room, then a kill and some gold every
    30 seconds, and a Satanic drop every five minutes."""
    out = [line("bridge_status", 1, start, pid, state="transport_ready", detail="test"),
           line("progress", 2, start, pid, name="Sorak"),
           line("room", 3, start, pid, room="Act_04_02")]
    seq = 3
    for half_minute in range(1, minutes * 2 + 1):
        seq += 1
        t = start + half_minute * 30_000
        out.append(line("session_delta", seq, t, pid, gold=25, xp=1000, kills=kills_per_record,
                        source="forgepact_tracker"))
        if half_minute % 10 == 0:
            seq += 1
            out.append(line("ground_drop", seq, t, pid, event_id="fp-%d-%d" % (pid, seq),
                            source="game_ground_item_create", room="Act_04_02",
                            item={"name": "", "rarity": "Satanic", "amount": 1}))
    return out


class Sandbox:
    def __init__(self, test):
        tmp = tempfile.TemporaryDirectory()
        test.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        self.sessions = self.root / "game" / "bin" / "bp_ipc" / "tracker"
        self.sessions.mkdir(parents=True)
        self.store = self.root / "localappdata" / "Hero_Siege" / "tracker"
        self.live = set()
        self.asked = []
        self.now = [T0 + 86_400_000]

    def is_live(self, pid):
        self.asked.append(pid)
        return pid in self.live

    def service(self, **kw):
        kw.setdefault("is_live", self.is_live)
        return TrackerService(self.store, self.sessions, clock_ms=lambda: self.now[0], **kw)

    def write(self, pid, start, lines, mode="wb"):
        path = self.sessions / ("session-%d-%d.ndjson" % (pid, start))
        with open(path, mode) as fh:
            fh.write(b"".join(lines))
        return path


def drain(svc, limit=500):
    for _ in range(limit):
        if not svc.tick()["catching_up"]:
            return
    raise AssertionError("still catching up after %d ticks" % limit)


class IngestTests(unittest.TestCase):
    def test_a_partial_last_line_waits_and_a_malformed_line_is_counted(self):
        box = Sandbox(self)
        box.live.add(4242)
        whole = line("session_delta", 5, T0 + 2000, gold=0, xp=0, kills=1, source="forgepact_tracker")
        path = box.write(4242, T0, [
            line("room", 1, T0, room="Act_01_02"),
            b"{this is not json\n",
            line("teleport", 2, T0 + 500),
            b'{"protocol":"hs-offline-tracker/2","v":2,"kind":"heartbeat"}\n',
            line("heartbeat", 3, T0 + 1000),
            whole[:40],
        ])
        svc = box.service()
        svc.tick()
        snap = svc.snapshot()
        self.assertEqual(snap["room"], "Act_01_02")
        self.assertEqual(snap["kills"]["earned"], 0, "the partial line is not read yet")
        self.assertEqual(snap["session"]["bad_lines"], 3, "malformed, unknown kind, wrong version")
        self.assertFalse(snap["catching_up"], "a partial line is waiting, not unread")

        with open(path, "ab") as fh:
            fh.write(whole[40:])
        svc.tick()
        snap = svc.snapshot()
        self.assertEqual(snap["kills"]["earned"], 1, "its newline arrived")
        self.assertEqual(snap["session"]["bad_lines"], 3)

    def test_a_dead_session_s_partial_tail_is_counted_when_it_is_finalized(self):
        box = Sandbox(self)
        lines = farm(77, T0, 3)
        box.write(77, T0, lines + [line("heartbeat", 99, T0 + 200_000, 77)[:20]])
        svc = box.service()
        drain(svc)
        snap = svc.snapshot()
        self.assertEqual(snap["session"]["bad_lines"], 1)
        self.assertEqual(len(svc.runs()), 1)

    def test_catch_up_after_the_panel_was_closed(self):
        box = Sandbox(self)
        box.write(501, T0, farm(501, T0, 20))
        svc = box.service()
        drain(svc)
        runs = svc.runs()
        self.assertEqual(len(runs), 1, runs)
        run = runs[0]
        self.assertEqual(run["kills"], 40)
        self.assertEqual(run["gold"], 1000)
        self.assertEqual(run["secs"], 1200, "the run's length comes from its records, not the panel's clock")
        self.assertEqual((run["started_ms"], run["ended_ms"]), (T0, T0 + 1_200_000))
        self.assertEqual(run["items"]["Satanic"], 4)
        self.assertEqual(run["character"], "Sorak")
        self.assertEqual(run["zones"], [["Act_04_02", 1200]])

    def test_re_derivation_after_a_restart_is_identical(self):
        box = Sandbox(self)
        box.live.add(4242)
        lines = farm(4242, T0, 30)
        path = box.write(4242, T0, lines[:25])
        first = box.service()
        first.tick()
        box.now[0] = T0 + 400_000
        self.assertTrue(first.pause(True)["ok"])
        with open(path, "ab") as fh:
            fh.write(b"".join(lines[25:50]))
        first.tick()
        box.now[0] = T0 + 700_000
        self.assertTrue(first.pause(False)["ok"])
        self.assertTrue(first.accept_progress({"kind": "progress", "name": "Sorak", "experience": 5_000,
                                               "level": 90, "hero_level": 10, "kills": 300,
                                               "tallies": {"statisticsatankills": 4}})["ok"])
        with open(path, "ab") as fh:
            fh.write(b"".join(lines[50:]))
        first.tick()
        box.now[0] = T0 + 1_500_000
        self.assertTrue(first.reset()["ok"])
        first.tick()
        before = (first.snapshot(), first.extra(), first.runs())
        self.assertEqual(len(before[2]), 1, "the reset filed its run")

        again = box.service()
        drain(again)
        after = (again.snapshot(), again.extra(), again.runs())
        self.assertEqual(after[0], before[0])
        self.assertEqual(after[1], before[1])
        self.assertEqual(after[2], before[2], "and filed nothing twice")

    def test_a_finalized_session_is_filed_once(self):
        box = Sandbox(self)
        box.write(600, T0, farm(600, T0, 5))
        svc = box.service()
        drain(svc)
        svc.tick()
        self.assertEqual(len(svc.runs()), 1)
        cursor = json.loads((box.store / "cursor.json").read_text(encoding="utf-8"))
        entry = cursor["sessions"]["session-600-%d.ndjson" % T0]
        self.assertTrue(entry["finalized"])

        again = box.service()
        drain(again)
        again.tick()
        self.assertEqual(len(again.runs()), 1, "read again, filed once")

    def test_retention_keeps_the_newest_twenty_finalized_sessions(self):
        box = Sandbox(self)
        box.live.add(1)
        box.write(1, T0, farm(1, T0, 1))
        dead = []
        for n in range(23):
            pid = 1000 + n
            start = T0 + (n + 1) * 3_600_000
            dead.append(box.write(pid, start, farm(pid, start, 1)))
        svc = box.service()
        drain(svc)
        svc.tick()
        left = sorted(p.name for p in box.sessions.iterdir())
        self.assertIn("session-1-%d.ndjson" % T0, left, "a live pid's session is never removed")
        self.assertEqual(sorted(p.name for p in dead[3:]), sorted(n for n in left if not n.startswith("session-1-")))

    def test_a_bounded_read_says_it_is_catching_up(self):
        box = Sandbox(self)
        box.live.add(4242)
        box.write(4242, T0, farm(4242, T0, 25))
        svc = box.service(read_budget=600)
        svc.tick()
        self.assertTrue(svc.snapshot()["catching_up"])
        ticks = 1
        while svc.tick()["catching_up"]:
            ticks += 1
            self.assertLess(ticks, 400)
        self.assertGreater(ticks, 5, "one ingest must not read the whole file")
        snap = svc.snapshot()
        self.assertFalse(snap["catching_up"])

        whole = Sandbox(self)
        whole.live.add(4242)
        whole.write(4242, T0, farm(4242, T0, 25))
        reference = whole.service()
        drain(reference)
        self.assertEqual(snap["kills"], reference.snapshot()["kills"])
        self.assertEqual(svc.extra(), reference.extra())

    def test_the_live_pid_check_is_injected(self):
        box = Sandbox(self)
        box.live.add(4242)
        box.write(4242, T0, farm(4242, T0, 5))
        svc = box.service()
        drain(svc)
        svc.tick()
        self.assertIn(4242, box.asked, "the pid is the one the file name gives")
        snap = svc.snapshot()
        self.assertTrue(snap["session"]["live"])
        self.assertEqual(snap["session"]["pid"], 4242)
        self.assertEqual(snap["session"]["file"], "session-4242-%d.ndjson" % T0)
        self.assertEqual(svc.runs(), [], "a live session is not finished, however far it has been read")

        box.live.discard(4242)
        svc.tick()
        self.assertEqual(len(svc.runs()), 1)
        self.assertFalse(svc.snapshot()["session"]["live"])

    def test_no_session_folder_is_an_empty_snapshot(self):
        box = Sandbox(self)
        svc = TrackerService(box.store, lambda: None, is_live=box.is_live, clock_ms=lambda: T0)
        self.assertFalse(svc.tick()["catching_up"])
        snap = svc.snapshot()
        self.assertIsNone(snap["session"]["file"])
        self.assertEqual(snap["kills"]["earned"], 0)


if __name__ == "__main__":
    unittest.main()
