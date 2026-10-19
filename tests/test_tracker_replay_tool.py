#!/usr/bin/env python3
"""tools/tracker_replay.py, the tracker's fast loop (ForgePact #185).

A session file goes through the panel's own ledger and comes out as its
totals, drops, journal, rooms and bad lines; `--save <slot>` reads one
character save through the panel's save reader. Live procedure 1 compares
both with what the game did, so this pins:

- the session totals of tests/fixtures/tracker/session-fixture.ndjson, in text
  and `--json`;
- a malformed line and a last line without its newline are bad lines, and a
  line of an unknown kind is one too;
- `--save` reads `hs2saves\\herosiege<slot>.hss`, and the legacy root only when
  `hs2saves` holds no character slot;
- an empty slot and a missing file exit 1; a call with neither or both inputs
  exits 2;
- neither mode writes anything: the folders it reads are byte for byte the
  same afterwards.

Every save and session is in a temporary directory.
"""

import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TESTS = Path(__file__).resolve().parent
ROOT = TESTS.parent
TOOL = ROOT / "tools" / "tracker_replay.py"
FIXTURE = TESTS / "fixtures" / "tracker" / "session-fixture.ndjson"
sys.path.insert(0, str(TESTS))

from test_tracker_saves import encode_hss, save  # noqa: E402


def run(*args):
    result = subprocess.run([sys.executable, str(TOOL), *map(str, args)], capture_output=True, text=True,
                            encoding="utf-8", errors="replace")
    return result.returncode, result.stdout, result.stderr


def folder_state(folder: Path) -> dict:
    return {str(p.relative_to(folder)): (p.read_bytes(), p.stat().st_mtime_ns)
            for p in sorted(folder.rglob("*")) if p.is_file()}


class ReplaySessionTests(unittest.TestCase):
    def setUp(self):
        self.temp = Path(tempfile.mkdtemp(prefix="forgepact-tracker-replay-"))
        self.addCleanup(shutil.rmtree, self.temp, ignore_errors=True)
        self.session = self.temp / "session-777-1700000000000.ndjson"
        shutil.copyfile(FIXTURE, self.session)

    def test_json_reports_the_totals_the_drops_and_the_rooms(self):
        before = folder_state(self.temp)
        code, out, err = run(self.session, "--json")
        self.assertEqual(code, 0, err)
        result = json.loads(out)
        self.assertEqual((result["gold"], result["xp"], result["kills"]), (200, 400, 6))
        self.assertEqual(result["records"], 12)
        self.assertEqual(result["bad_lines"], 0)
        self.assertEqual(result["drops"], {"Heroic": 1})
        self.assertEqual(result["journal"], 1)
        self.assertEqual(result["journal_entries"], [{"rarity": "Heroic", "name": "Fixture Heroic Axe",
                                                      "room": "Act_01_02"}])
        self.assertEqual(result["rooms"], ["Town_01", "Act_01_02"])
        self.assertEqual(result["character"], "Sorak")
        self.assertEqual(result["satanic_zone"]["zone"], "Satanic_1_3")
        self.assertEqual(result["kinds"]["session_delta"], 2)
        self.assertEqual(folder_state(self.temp), before, "the replay wrote something")

    def test_text_names_the_same_numbers(self):
        code, out, err = run(self.session)
        self.assertEqual(code, 0, err)
        for line in ("gold        200", "xp          400", "kills       6", "journal     1",
                     "rooms       Town_01 > Act_01_02", "records     12  (bad lines 0)"):
            self.assertIn(line, out)

    def test_bad_lines_are_counted_not_fatal(self):
        data = FIXTURE.read_bytes()
        bad = (data + b"{not json}\n"
               + b'{"protocol":"hs-offline-tracker/1","v":1,"kind":"made_up","sequence":13}\n'
               + b'{"protocol":"hs-offline-tracker/1","v":1,"kind":"session_delta","gold":5')
        self.session.write_bytes(bad)
        code, out, err = run(self.session, "--json")
        self.assertEqual(code, 0, err)
        result = json.loads(out)
        self.assertEqual(result["bad_lines"], 3)
        self.assertEqual(result["gold"], 200, "the unfinished last line is not counted")

    def test_a_missing_file_exits_1(self):
        code, _out, err = run(self.temp / "session-1-1.ndjson")
        self.assertEqual(code, 1)
        self.assertIn("tracker_replay:", err)

    def test_neither_or_both_inputs_exit_2(self):
        self.assertEqual(run()[0], 2)
        self.assertEqual(run(self.session, "--save", "14")[0], 2)


class ReplaySaveTests(unittest.TestCase):
    def setUp(self):
        self.root = Path(tempfile.mkdtemp(prefix="forgepact-tracker-saves-"))
        self.addCleanup(shutil.rmtree, self.root, ignore_errors=True)

    def test_the_nested_slot_is_read(self):
        (self.root / "hs2saves").mkdir()
        (self.root / "hs2saves" / "herosiege14.hss").write_bytes(save("Sorak", 123456, 789))
        (self.root / "herosiege14.hss").write_bytes(save("Legacy", 1, 1))
        before = folder_state(self.root)
        code, out, err = run("--save", 14, "--json", "--saves-root", self.root)
        self.assertEqual(code, 0, err)
        record = json.loads(out)
        self.assertEqual((record["name"], record["experience"], record["kills"]), ("Sorak", 123456, 789))
        self.assertEqual(record["kind"], "progress")
        self.assertEqual(record["tallies"]["statisticuberphantomleviathankills"], 4)
        self.assertEqual(Path(record["file"]).name, "herosiege14.hss")
        self.assertEqual(Path(record["file"]).parent.name, "hs2saves")
        self.assertEqual(folder_state(self.root), before, "the reader wrote something")

    def test_the_legacy_root_is_read_only_without_nested_slots(self):
        (self.root / "hs2saves").mkdir()
        (self.root / "herosiege3.hss").write_bytes(save("Legacy", 50, 2))
        code, out, err = run("--save", 3, "--json", "--saves-root", self.root)
        self.assertEqual(code, 0, err)
        self.assertEqual(json.loads(out)["name"], "Legacy")

    def test_text_names_the_character(self):
        (self.root / "hs2saves").mkdir()
        (self.root / "hs2saves" / "herosiege14.hss").write_bytes(save("Sorak", 10, 20))
        code, out, err = run("--save", 14, "--saves-root", self.root)
        self.assertEqual(code, 0, err)
        self.assertIn("name        Sorak", out)
        self.assertIn("kills       20", out)

    def test_an_empty_or_missing_slot_exits_1(self):
        (self.root / "hs2saves").mkdir()
        (self.root / "hs2saves" / "herosiege14.hss").write_bytes(b"\x00" * 64)
        self.assertEqual(run("--save", 14, "--saves-root", self.root)[0], 1)
        self.assertEqual(run("--save", 15, "--saves-root", self.root)[0], 1)

    def test_a_placeholder_character_is_not_a_character(self):
        (self.root / "hs2saves").mkdir()
        (self.root / "hs2saves" / "herosiege2.hss").write_bytes(
            encode_hss('[0]\nname="New Char"\nclass="1.000000"\nlevel="1.000000"\nexperience="0.000000"\n'))
        self.assertEqual(run("--save", 2, "--saves-root", self.root)[0], 1)


if __name__ == "__main__":
    unittest.main()
