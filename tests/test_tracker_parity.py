"""The sensor and the ledger agree on one session (ForgePact #185).

The plugin's `tracker` mod (plugin/include/ForgePact/TrackerMod.hpp) writes the
session file; the panel's tracker service (src/forgepact_tracker) reads it. Two
bindings answer the same question, so they are held to one shared fixture,
tests/fixtures/tracker/parity-script.txt (AGENTS.md, "HS Game SDK Usage"):

- tests/tracker_mod_parity_harness.cpp plays the script through the real core
  and its Win32 writer - kills, gold and XP calls the core accepts and ones it
  refuses, drops of every tracked rarity, Mythic and Common drops, a duplicate,
  a bag drop the creation guard holds, room changes, a Satanic zone and a
  heartbeat - into a real session file, and prints its own counters;
- the service reads that file as the panel would, and its ledger must report
  the same gold, XP and kill sums, the same recorded drops, by rarity and in
  the journal, and the same rooms.

The record-kind list is pinned on both sides: the kinds the header writes, the
kinds the ledger calls the sensor's (`protocol.SENSOR_KINDS`) and the kinds in
the file the script produced are one set, so neither side can add one alone.

Writes only under build/tracker-parity and a temporary store.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))
sys.path.insert(0, str(ROOT / "tools"))

from forgepact_tracker.protocol import SENSOR_KINDS, parse_line  # noqa: E402
from forgepact_tracker.service import TrackerService  # noqa: E402
import tracker_replay  # noqa: E402

SCRIPT = ROOT / "tests" / "fixtures" / "tracker" / "parity-script.txt"
HEADER = ROOT / "plugin" / "include" / "ForgePact" / "TrackerMod.hpp"
HARNESS = ROOT / "tests" / "tracker_mod_parity_harness.cpp"
TRACKED = ("Set", "Satanic", "Angelic", "Heroic", "Unholy")


def compile_harness(out: Path) -> Path:
    """The parity harness with the real header injected, built by MSVC."""
    header = HEADER.read_text(encoding="utf-8")
    core = "\n".join(line for line in header.split("\n") if not line.strip().startswith("#pragma once"))
    cpp = out / "parity.cpp"
    cpp.write_text(HARNESS.read_text(encoding="utf-8").replace("// PRODUCTION_TRACKER_MOD", core), encoding="utf-8")
    binary = out / "parity.exe"
    if os.name != "nt":
        raise unittest.SkipTest("the session writer is Win32 only")
    vswhere = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
    if not vswhere.is_file():
        raise unittest.SkipTest("Visual Studio C++ compiler is required for the parity harness")
    install = subprocess.check_output(
        [str(vswhere), "-latest", "-products", "*", "-requires",
         "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"], text=True).strip()
    if not install:
        raise unittest.SkipTest("Visual Studio C++ toolchain not installed")
    vcvars = Path(install) / "VC/Auxiliary/Build/vcvars64.bat"
    batch = out / "compile-parity.cmd"
    batch.write_text(
        f'@echo off\ncall "{vcvars}" >nul\nif errorlevel 1 exit /b 1\n'
        f'cl /nologo /std:c++20 /EHsc /O2 /W4 /utf-8 /I "{ROOT / "plugin/include"}" "{cpp}" '
        f'/Fe:"{binary}" /Fo:"{out / "parity.obj"}"\nexit /b %errorlevel%\n', encoding="utf-8")
    result = subprocess.run(["cmd", "/d", "/c", str(batch)], cwd=out, capture_output=True, text=True,
                            encoding="utf-8", errors="replace")
    (out / "compile-parity.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode:
        raise AssertionError(result.stdout + result.stderr)
    return binary


def stat_value(stat: str, key: str) -> int:
    match = re.search(r"(?:^| )%s=(-?\d+)(?: |$)" % re.escape(key), stat)
    if match is None:
        raise AssertionError("%s not in the stat line: %s" % (key, stat))
    return int(match.group(1))


class TrackerParityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        out = ROOT / "build" / "tracker-parity" / f"pid-{os.getpid()}"
        shutil.rmtree(out, ignore_errors=True)
        out.mkdir(parents=True)
        cls.addClassCleanup(shutil.rmtree, out, ignore_errors=True)
        binary = compile_harness(out)
        ipc = out / "bp_ipc"
        ipc.mkdir()
        run = subprocess.run([str(binary), str(SCRIPT), str(ipc)], capture_output=True, text=True,
                             encoding="utf-8", errors="replace")
        (out / "run.log").write_text(run.stdout + run.stderr, encoding="utf-8")
        cls.output = run.stdout
        cls.returncode = run.returncode
        lines = cls.output.splitlines()
        cls.recorded = [tuple(l[len("RECORDED "):].split("\t", 1)) for l in lines if l.startswith("RECORDED ")]
        cls.rooms = [l[len("ROOM "):] for l in lines if l.startswith("ROOM ")]
        cls.stat = next((l[len("STAT "):] for l in lines if l.startswith("STAT ")), "")
        cls.file = next((Path(l[len("FILE "):].strip()) for l in lines if l.startswith("FILE ")), None)
        cls.store = Path(tempfile.mkdtemp(prefix="forgepact-tracker-parity-"))
        cls.addClassCleanup(shutil.rmtree, cls.store, ignore_errors=True)

    def service(self, live=True):
        # One service per test, over its own store; the session is live, so the
        # snapshot shows its ledger rather than a finished run.
        store = Path(tempfile.mkdtemp(dir=self.store))
        service = TrackerService(store, self.file.parent, is_live=lambda pid: live, clock_ms=lambda: 1700000099000)
        service.tick()
        return service

    def test_the_harness_played_the_whole_script(self):
        self.assertEqual(self.returncode, 0, self.output)
        self.assertIn("RESULT OK", self.output)
        self.assertIsNotNone(self.file)
        self.assertTrue(self.file.is_file(), self.file)
        self.assertRegex(self.file.name, r"^session-4242-1700000000000\.ndjson$")

    def test_every_line_is_a_protocol_v1_record_with_a_sequence_rising_by_one(self):
        lines = self.file.read_bytes().split(b"\n")
        self.assertEqual(lines[-1], b"", "the file ends on a whole line")
        sequences = [parse_line(line)["sequence"] for line in lines[:-1]]
        self.assertEqual(sequences, list(range(1, len(sequences) + 1)))
        self.assertEqual(len(sequences), stat_value(self.stat, "lines"))

    def test_the_counters_agree(self):
        snap = self.service().snapshot()
        sums = {key: stat_value(self.stat, key) for key in ("gold-sum", "xp-sum", "kills-sum")}
        # what the script feeds, so a harness that counted nothing cannot pass
        self.assertEqual(sums, {"gold-sum": 268, "xp-sum": 405, "kills-sum": 5})
        self.assertEqual((snap["gold"]["earned"], snap["xp"]["earned"], snap["kills"]["earned"]),
                         (sums["gold-sum"], sums["xp-sum"], sums["kills-sum"]))

    def test_the_refused_calls_are_counted_as_calls_and_not_as_gains(self):
        self.assertEqual(stat_value(self.stat, "kill-calls"), 7)
        self.assertEqual(stat_value(self.stat, "gold-calls"), 5)
        self.assertEqual(stat_value(self.stat, "xp-calls"), 5)
        self.assertEqual(stat_value(self.stat, "xp-rejected"), 1)

    def test_the_recorded_drops_agree(self):
        service = self.service()
        self.assertEqual(sorted(r for r, _ in self.recorded), sorted(TRACKED), "one recorded drop per rarity")
        self.assertEqual(len(self.recorded), stat_value(self.stat, "drops-written"))
        items = service.snapshot()["items"]
        by_rarity = {name: count["total"] for name, count in items.items() if count["total"]}
        self.assertEqual(by_rarity, {rarity: 1 for rarity in TRACKED})
        # the journal, oldest first, is the harness's recorded list: the five
        # tracked rarities are the default alerts
        journal = [(d["rarity"], d["name"]) for d in reversed(service.extra()["drops"])]
        self.assertEqual(journal, self.recorded)

    def test_what_the_sensor_held_back_never_reaches_the_ledger(self):
        self.assertEqual(stat_value(self.stat, "held-bag-drop"), 1)
        self.assertEqual(stat_value(self.stat, "drops-untracked"), 2)
        self.assertEqual(stat_value(self.stat, "drops-duplicate"), 1)
        names = [d["name"] for d in self.service().extra()["drops"]]
        for held in ("Parity Bag Drop", "Parity Mythic Sword", "Parity Common Dagger"):
            self.assertNotIn(held, names)
        self.assertEqual(names.count("Parity Heroic Axe"), 1)

    def test_the_rooms_agree(self):
        self.assertEqual(self.rooms, ["Town_01", "Act_01_02", "Act_01_03", "Town_01"])
        replay = tracker_replay.replay_session(self.file)
        self.assertEqual(replay["rooms"], self.rooms)
        snap = self.service().snapshot()
        self.assertEqual(snap["room"], self.rooms[-1])
        self.assertEqual(snap["satanic_zone"]["zone"], "Satanic_1_3")
        self.assertEqual(snap["character"]["name"], "Sorak")

    def test_the_replay_tool_reads_the_same_totals(self):
        replay = tracker_replay.replay_session(self.file)
        snap = self.service().snapshot()
        self.assertEqual((replay["gold"], replay["xp"], replay["kills"]),
                         (snap["gold"]["earned"], snap["xp"]["earned"], snap["kills"]["earned"]))
        self.assertEqual(replay["bad_lines"], 0)
        self.assertEqual(replay["drop_records"], len(self.recorded))

    def test_the_finished_run_carries_the_same_sums(self):
        service = self.service(live=False)
        runs = service.runs()
        self.assertEqual(len(runs), 1, runs)
        run = runs[0]
        self.assertEqual((run["gold"], run["xp"], run["kills"]),
                         tuple(stat_value(self.stat, k) for k in ("gold-sum", "xp-sum", "kills-sum")))
        self.assertEqual({k: v for k, v in run["items"].items() if v}, {rarity: 1 for rarity in TRACKED})

    def test_the_record_kinds_are_one_list_on_both_sides(self):
        written = {parse_line(line)["kind"] for line in self.file.read_bytes().split(b"\n") if line}
        header_kinds = set(re.findall(r'Envelope\("([a-z_]+)"\)', HEADER.read_text(encoding="utf-8")))
        self.assertEqual(header_kinds, set(SENSOR_KINDS))
        self.assertEqual(written, set(SENSOR_KINDS), "the script exercises every kind the sensor writes")


if __name__ == "__main__":
    unittest.main()
