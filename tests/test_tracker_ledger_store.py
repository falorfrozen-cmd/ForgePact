#!/usr/bin/env python3
"""The tracker's own files under `%LOCALAPPDATA%\\Hero_Siege\\tracker\\` (#185).

The panel writes only whole files there: a temporary file, then `os.replace`.
Pinned here:

- a write is whole-file and atomic: a failed replace leaves the old file and
  no temporary behind;
- `runs.json` is the Tracker's run list, newest first, at most 200;
- the settings document is validated field by field for what the ledger
  reads, and fields it does not know are kept;
- a control (a pause, a resume, an end-run by hand, a save snapshot, a
  settings change) is persisted in `cursor.json` before it takes effect, and a
  control that cannot be persisted does not take effect;
- the one-time import copies only missing files from HS Offline Tracker's
  folder, records what it did in `import.json`, and leaves every file in that
  folder with the same bytes and modification time.

Every root is a temporary directory; the owner's real Tracker data is what
the import must never change, so no test comes near it.
"""

import json
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

TESTS = Path(__file__).resolve().parent
sys.path.insert(0, str(TESTS))
sys.path.insert(0, str(TESTS.parent / "src"))

from forgepact_tracker import store as store_module  # noqa: E402
from forgepact_tracker.ledger import Ledger  # noqa: E402
from forgepact_tracker.service import TrackerService  # noqa: E402
from forgepact_tracker.store import (RUNS_KEPT, SettingsError, TrackerStore,  # noqa: E402
                                     import_tracker_data, validate_settings)
from test_tracker_ledger_ingest import Sandbox, farm, line, T0  # noqa: E402


def temp_dir(test):
    tmp = tempfile.TemporaryDirectory()
    test.addCleanup(tmp.cleanup)
    return Path(tmp.name)


class WholeFileTests(unittest.TestCase):
    def test_whole_file_writes_are_atomic(self):
        root = temp_dir(self)
        store = TrackerStore(root / "tracker")
        store.write_json("runs.json", [{"id": "a"}])
        self.assertEqual(json.loads((root / "tracker" / "runs.json").read_text(encoding="utf-8")), [{"id": "a"}])

        with patch.object(store_module.os, "replace", side_effect=OSError("disk full")):
            with self.assertRaises(OSError):
                store.write_json("runs.json", [{"id": "b"}])
        self.assertEqual(json.loads((root / "tracker" / "runs.json").read_text(encoding="utf-8")), [{"id": "a"}],
                         "a failed write leaves the old file whole")
        self.assertEqual(sorted(p.name for p in (root / "tracker").iterdir()), ["runs.json"],
                         "and no temporary file behind")

    def test_runs_are_capped_at_two_hundred_newest_first(self):
        store = TrackerStore(temp_dir(self) / "tracker")
        self.assertEqual(RUNS_KEPT, 200)
        for n in range(205):
            self.assertTrue(store.file_run({"id": "run-%d" % n, "started_ms": n}))
        runs = store.runs()
        self.assertEqual(len(runs), 200)
        self.assertEqual(runs[0]["id"], "run-204", "newest first")
        self.assertEqual(runs[-1]["id"], "run-5")
        self.assertFalse(store.file_run({"id": "run-204", "started_ms": 204}), "a run is filed once")
        self.assertEqual(len(store.runs()), 200)


class SettingsTests(unittest.TestCase):
    def test_settings_validation_keeps_unknown_fields(self):
        store = TrackerStore(temp_dir(self) / "tracker")
        doc = {"alerts": ["Satanic", "Heroic"], "min_tier": 5, "zone_buffs": [3, 14],
               "theme": "obsidian", "a_future_field": {"nested": [1, 2]},
               "filters": [{"id": "f1", "name": "Farm", "lists": [
                   {"id": "l1", "name": "Chase", "enabled": True, "volume": 0.5, "items": ["AK-47"]}]}],
               "filter": "f1"}
        saved = store.save_settings(doc)
        self.assertEqual(saved["a_future_field"], {"nested": [1, 2]})
        again = store.settings()
        self.assertEqual(again["a_future_field"], {"nested": [1, 2]}, "unknown fields are kept")
        self.assertEqual(again["theme"], "obsidian")
        self.assertEqual(again["min_tier"], 5)
        self.assertIn("notable", again, "fields the document left out read as the defaults")

        for bad in ({"alerts": "Satanic"}, {"min_tier": "6"}, {"min_tier": True}, {"zone_buffs": [300]},
                    {"notable": [{"label": "x"}]}, {"use_filter": 1}, {"filters": [{"lists": "no"}]},
                    {"flourish": "yes"}, [1, 2]):
            with self.assertRaises(SettingsError, msg=repr(bad)):
                validate_settings(bad)
        self.assertEqual(store.settings()["min_tier"], 5, "a rejected document changes nothing")

    def test_the_service_rejects_a_bad_document_without_writing(self):
        box = Sandbox(self)
        svc = box.service()
        self.assertTrue(svc.set_settings({"alerts": ["Unholy"], "extra": 1})["ok"])
        result = svc.set_settings({"alerts": 5})
        self.assertFalse(result["ok"])
        self.assertIn("alerts", result["error"])
        self.assertEqual(svc.get_settings()["alerts"], ["Unholy"])
        self.assertEqual(svc.get_settings()["extra"], 1)


class ControlTests(unittest.TestCase):
    def live_service(self):
        box = Sandbox(self)
        box.live.add(4242)
        box.write(4242, T0, farm(4242, T0, 2))
        svc = box.service()
        svc.tick()
        return box, svc

    def cursor_controls(self, box):
        cursor = json.loads((box.store / "cursor.json").read_text(encoding="utf-8"))
        return cursor["sessions"]["session-4242-%d.ndjson" % T0]["controls"]

    def test_controls_are_persisted_before_they_take_effect(self):
        box, svc = self.live_service()
        seen = []
        real = Ledger.set_paused

        def spy(ledger, on, at_ms):
            seen.append([c["kind"] for c in self.cursor_controls(box)])
            return real(ledger, on, at_ms)

        with patch.object(Ledger, "set_paused", spy):
            self.assertTrue(svc.pause(True)["ok"])
        self.assertEqual(seen, [["pause"]], "the pause was on disk before the ledger saw it")
        self.assertTrue(svc.snapshot()["paused"])

        real_save = Ledger.apply_save

        def save_spy(ledger, progress, at_ms):
            seen.append([c["kind"] for c in self.cursor_controls(box)])
            return real_save(ledger, progress, at_ms)

        with patch.object(Ledger, "apply_save", save_spy):
            self.assertTrue(svc.accept_progress({"kind": "progress", "name": "Sorak", "experience": 10})["ok"])
        self.assertEqual(seen[-1], ["pause", "save"])

    def test_a_control_that_cannot_be_persisted_does_not_take_effect(self):
        box, svc = self.live_service()
        with patch.object(TrackerStore, "write_json", side_effect=OSError("read-only")):
            result = svc.pause(True)
            self.assertFalse(result["ok"])
            self.assertFalse(svc.snapshot()["paused"])
            self.assertFalse(svc.reset()["ok"])
            self.assertFalse(svc.accept_progress({"kind": "progress", "name": "Sorak", "experience": 1})["ok"])
        self.assertEqual(svc.snapshot()["kills"]["earned"], 4, "the reset did not happen either")

    def test_a_settings_change_reaches_the_ledger_as_a_control(self):
        box, svc = self.live_service()
        self.assertTrue(svc.set_settings({"alerts": ["Unholy"]})["ok"])
        self.assertEqual(self.cursor_controls(box)[-1]["kind"], "prefs")
        with open(box.sessions / ("session-4242-%d.ndjson" % T0), "ab") as fh:
            fh.write(line("ground_drop", 900, T0 + 200_000, event_id="fp-4242-900",
                          source="game_ground_item_create", room="Act_04_02",
                          item={"name": "", "rarity": "Satanic", "amount": 1}))
        svc.tick()
        self.assertEqual(svc.extra()["drops"], [], "Satanic is no longer an alert")
        self.assertEqual(svc.snapshot()["items"]["Satanic"]["total"], 1, "and it still counts")

    def test_a_repeated_save_snapshot_is_one_control(self):
        box, svc = self.live_service()
        progress = {"kind": "progress", "name": "Sorak", "experience": 10, "kills": 3}
        self.assertTrue(svc.accept_progress(progress)["ok"])
        self.assertTrue(svc.accept_progress(dict(progress))["ok"])
        self.assertEqual([c["kind"] for c in self.cursor_controls(box)], ["save"])


def fake_tracker(root):
    src = root / "HS Offline Tracker"
    (src / "sounds").mkdir(parents=True)
    files = {
        "settings.json": b'{"alerts":["Set"],"theme":"crimson"}',
        "runs.json": b'[{"started_ms":1,"ended_ms":2,"secs":60}]',
        "shopping.json": b'{"items":["Zed"]}',
        "positions.json": b'{"main":[1,2]}',
        "carried.json": b'{"gold":5}',
        "sounds/chime.mp3": b"ID3-chime",
        "sounds/horn.mp3": b"ID3-horn",
    }
    for rel, data in files.items():
        (src / rel).write_bytes(data)
        os.utime(src / rel, ns=(1_600_000_000_000_000_000, 1_600_000_000_000_000_000))
    return src


def fingerprint(folder):
    return {str(p.relative_to(folder)): (p.read_bytes(), p.stat().st_mtime_ns)
            for p in sorted(folder.rglob("*")) if p.is_file()}


class ImportTests(unittest.TestCase):
    def test_import_copies_only_missing_files_and_records_it(self):
        root = temp_dir(self)
        src = fake_tracker(root)
        before = fingerprint(src)
        store = TrackerStore(root / "Hero_Siege" / "tracker")
        (store.root / "sounds").mkdir(parents=True)
        (store.root / "sounds" / "horn.mp3").write_bytes(b"ours")

        report = import_tracker_data(store, src)
        self.assertEqual(fingerprint(src), before, "the Tracker's folder is untouched")
        self.assertEqual(sorted(report["copied"]),
                         ["runs.json", "settings.json", "shopping.json", "sounds/chime.mp3"])
        self.assertIn("sounds/horn.mp3", [s["file"] for s in report["skipped"]])
        self.assertEqual((store.root / "sounds" / "horn.mp3").read_bytes(), b"ours", "nothing is overwritten")
        self.assertEqual((store.root / "settings.json").read_bytes(), b'{"alerts":["Set"],"theme":"crimson"}')
        self.assertFalse((store.root / "positions.json").exists())
        self.assertFalse((store.root / "carried.json").exists())
        recorded = json.loads((store.root / "import.json").read_text(encoding="utf-8"))
        self.assertEqual(sorted(recorded["copied"]), sorted(report["copied"]))
        self.assertEqual(recorded["source"], str(src))

        again = import_tracker_data(store, src)
        self.assertEqual(again["copied"], [], "a second import copies only what is missing")
        self.assertEqual(fingerprint(src), before)

    def test_a_missing_tracker_folder_is_recorded_not_an_error(self):
        root = temp_dir(self)
        store = TrackerStore(root / "tracker")
        report = import_tracker_data(store, root / "nowhere")
        self.assertEqual(report["copied"], [])
        self.assertFalse(report["source_found"])
        self.assertTrue((store.root / "import.json").exists())

    def test_the_first_start_imports_once_and_the_endpoint_re_runs_it(self):
        box = Sandbox(self)
        src = fake_tracker(box.root)
        before = fingerprint(src)
        svc = box.service(tracker_source=src)
        self.assertEqual(svc.get_settings()["theme"], "crimson", "the first start imported the settings")
        self.assertEqual(len(svc.runs()), 1, "and the run history")
        (box.store / "shopping.json").unlink()

        later = box.service(tracker_source=src)
        later.snapshot()
        self.assertFalse((box.store / "shopping.json").exists(), "import.json says it already ran")
        result = later.run_import()
        self.assertEqual(result["copied"], ["shopping.json"])
        self.assertEqual(fingerprint(src), before)


if __name__ == "__main__":
    unittest.main()
