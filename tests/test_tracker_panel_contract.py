#!/usr/bin/env python3
"""The panel half of HS Offline Tracker in ForgePact (`tracker`, #185).

The plugin's sensor and the ledger are pinned by their own tests
(test_tracker_mod_*, test_tracker_ledger*, test_tracker_parity). This file
pins what src/forgepact.py owns, which the Tracker tab will be built on:

- `mod_tracker` is off by default, so a fresh install sends no `tracker` line;
- with it on, startup sends `tracker 1`, and nothing else for it;
- `/api/set` of it sends `tracker 1|0` while the game runs, and nothing while
  it is closed;
- `GET /api/tracker`, `/api/tracker/extra`, `/api/tracker/runs` and
  `/api/tracker/settings`, and `POST /api/tracker/reset`, `/pause`,
  `/settings` and `/import`, over a fixture session file
  (tests/fixtures/tracker/session-fixture.ndjson) and a fake HS Offline
  Tracker folder that must come out of every call byte for byte unchanged;
- the watcher's tracker step does nothing for a player who never turned the
  tracker on, and a tracker that raises never stops the watcher from applying
  the settings to a launch.

Runs against `PanelSandbox` (isolated settings, `send_cmds` captured, never
the game's IPC) with the panel's ROOT, the Tracker's folder and the process
check moved into a temporary directory: no test reads or writes real data.
"""

import json
import shutil
import sys
import tempfile
import unittest
from http.client import HTTPConnection
from pathlib import Path
from unittest.mock import patch

TESTS = Path(__file__).resolve().parent
ROOT = TESTS.parent
sys.path.insert(0, str(ROOT / "src"))
sys.path.insert(0, str(TESTS))

import forgepact  # noqa: E402
from forgepact_tracker.store import default_settings  # noqa: E402
from test_jump_scenery_panel_contract import LiveSandbox, defaults  # noqa: E402

FIXTURE = TESTS / "fixtures" / "tracker" / "session-fixture.ndjson"
SESSION = "session-777-1700000000000.ndjson"


def tracker_lines(cmds):
    return [c for c in cmds if c.split(" ", 1)[0] == "tracker"]


def fake_tracker_folder(folder: Path) -> None:
    """HS Offline Tracker's data folder, as the import finds it."""
    folder.mkdir(parents=True)
    settings = default_settings()
    settings["tracker_only_field"] = {"kept": True}
    (folder / "settings.json").write_text(json.dumps(settings), encoding="utf-8")
    (folder / "runs.json").write_text(json.dumps([{"id": "tracker-run-1", "gold": 7}]), encoding="utf-8")
    (folder / "shopping.json").write_text(json.dumps({"items": ["Fixture Heroic Axe"]}), encoding="utf-8")
    (folder / "positions.json").write_text("{}", encoding="utf-8")
    (folder / "sounds").mkdir()
    (folder / "sounds" / "heroic.mp3").write_bytes(b"ID3fixture")


def folder_state(folder: Path) -> dict:
    return {str(p.relative_to(folder)): (p.read_bytes(), p.stat().st_mtime_ns)
            for p in sorted(folder.rglob("*")) if p.is_file()}


class TrackerSandbox:
    """LiveSandbox with the tracker's folders in a temporary directory and the
    fixture session's pid reported as a running game."""

    def __init__(self, test, running=True, session=True):
        self.live = LiveSandbox(test, running=running)
        self.temp = Path(tempfile.mkdtemp(prefix="forgepact-tracker-panel-"))
        test.addCleanup(shutil.rmtree, self.temp, ignore_errors=True)
        self.root = self.temp / "Hero_Siege"
        self.root.mkdir()
        self.source = self.temp / "HS Offline Tracker"
        fake_tracker_folder(self.source)
        self.source_before = folder_state(self.source)
        self.game_live = {777: True}
        for p in (patch.object(forgepact, "ROOT", self.root),
                  patch.object(forgepact, "TRACKER_SOURCE", self.source),
                  patch.object(forgepact, "tracker_pid_is_live", side_effect=lambda pid: self.game_live.get(pid, False))):
            p.start()
            test.addCleanup(p.stop)
        self.sessions = Path(json.loads(self.live.sandbox.config.read_text(encoding="utf-8"))["game_exe"]).parent \
            / "bp_ipc" / "tracker"
        if session:
            self.sessions.mkdir(parents=True)
            shutil.copyfile(FIXTURE, self.sessions / SESSION)
        test.addCleanup(lambda: test.assertEqual(folder_state(self.source), self.source_before,
                                                 "the Tracker's folder was changed"))

    def call(self, method, path, body=None):
        connection = HTTPConnection("127.0.0.1", self.live.sandbox.port, timeout=10)
        try:
            if body is None:
                connection.request(method, path)
            else:
                connection.request(method, path, json.dumps(body), {"Content-Type": "application/json"})
            response = connection.getresponse()
            return response.status, json.loads(response.read())
        finally:
            connection.close()

    @property
    def store(self) -> Path:
        return self.root / "tracker"


class TrackerPanelBaselineTests(unittest.TestCase):
    """The defaults: off, and nothing sent."""

    def test_the_switch_is_off_by_default(self):
        self.assertIs(forgepact.DEFAULTS["mod_tracker"], False)

    def test_build_cmds_with_the_defaults_sends_no_tracker_line(self):
        self.assertEqual(tracker_lines(forgepact.build_cmds(defaults())), [])

    def test_the_store_lives_under_the_panels_root(self):
        self.assertEqual(forgepact.TRACKER_DIR, "tracker")
        self.assertEqual(forgepact.TRACKER_SOURCE.name, "HS Offline Tracker")


class TrackerPanelSwitchTests(unittest.TestCase):
    """Startup and `/api/set`."""

    def test_on_sends_tracker_1_once(self):
        cfg = {**defaults(), "mod_tracker": True}
        self.assertEqual(tracker_lines(forgepact.build_cmds(cfg)), ["tracker 1"])
        self.assertEqual(len(forgepact.build_cmds(cfg)), len(forgepact.build_cmds(defaults())) + 1)

    def test_the_switch_sends_tracker_1_and_0_while_the_game_runs(self):
        live = LiveSandbox(self)
        for value, line in ((True, "tracker 1"), (False, "tracker 0")):
            code, body, sent = live.post("mod_tracker", value)
            self.assertEqual(code, 200, body)
            self.assertEqual(sent, [[line]])
            self.assertIs(live.saved()["mod_tracker"], value)

    def test_nothing_is_sent_while_the_game_is_closed(self):
        closed = LiveSandbox(self, running=False)
        for value in (True, False):
            code, _, sent = closed.post("mod_tracker", value)
            self.assertEqual((code, sent), (200, []))
            self.assertIs(closed.saved()["mod_tracker"], value)


class TrackerPanelReadTests(unittest.TestCase):
    """The GET endpoints over the fixture session."""

    def setUp(self):
        self.box = TrackerSandbox(self)

    def test_api_tracker_reports_the_switch_the_snapshot_and_the_session(self):
        code, body = self.box.call("GET", "/api/tracker")
        self.assertEqual(code, 200, body)
        self.assertEqual(set(body), {"on", "snapshot", "revision", "extra_revision", "catching_up", "session"})
        self.assertIs(body["on"], False)
        snap = body["snapshot"]
        self.assertEqual((snap["gold"]["earned"], snap["xp"]["earned"], snap["kills"]["earned"]), (200, 400, 6))
        self.assertEqual(snap["items"]["Heroic"]["total"], 1)
        self.assertEqual(snap["room"], "Act_01_02")
        self.assertEqual(snap["satanic_zone"]["zone"], "Satanic_1_3")
        self.assertEqual(snap["status"], "offline-live|12")
        self.assertGreater(body["revision"], 0)
        self.assertEqual(body["revision"], snap["revision"])
        self.assertIs(body["catching_up"], False)
        self.assertEqual(body["session"], {"file": SESSION, "pid": 777, "live": True, "bad_lines": 0})
        self.assertNotIn("session", snap)

    def test_the_switch_reads_back_on(self):
        self.box.live.post("mod_tracker", True)
        self.assertIs(self.box.call("GET", "/api/tracker")[1]["on"], True)

    def test_extra_carries_the_character_the_series_and_the_journal(self):
        self.box.call("GET", "/api/tracker")
        code, body = self.box.call("GET", "/api/tracker/extra")
        self.assertEqual(code, 200, body)
        self.assertEqual(set(body), {"character", "series", "drops"})
        self.assertEqual(body["character"]["name"], "Sorak")
        self.assertEqual([(d["rarity"], d["name"]) for d in body["drops"]], [("Heroic", "Fixture Heroic Axe")])
        self.assertIsInstance(body["series"], list)

    def test_the_journal_is_served_newest_first_and_capped(self):
        self.assertEqual(forgepact.TRACKER_DROPS_SERVED, 400)
        service = forgepact.tracker_service()
        with patch.object(service, "extra", return_value={"character": None, "series": [],
                                                          "drops": [{"n": i} for i in range(450, 0, -1)]}):
            code, body = self.box.call("GET", "/api/tracker/extra")
        self.assertEqual(code, 200, body)
        self.assertEqual(len(body["drops"]), 400)
        self.assertEqual(body["drops"][0], {"n": 450})

    def test_runs_start_from_the_trackers_own_history(self):
        code, body = self.box.call("GET", "/api/tracker/runs")
        self.assertEqual(code, 200, body)
        self.assertEqual(body["runs"], [{"id": "tracker-run-1", "gold": 7}])

    def test_settings_are_the_trackers_document_with_unknown_fields_kept(self):
        code, body = self.box.call("GET", "/api/tracker/settings")
        self.assertEqual(code, 200, body)
        self.assertEqual(body["settings"]["tracker_only_field"], {"kept": True})
        self.assertEqual(body["settings"]["alerts"], default_settings()["alerts"])

    def test_the_first_start_imported_only_what_the_store_lacked(self):
        self.box.call("GET", "/api/tracker")
        record = json.loads((self.box.store / "import.json").read_text(encoding="utf-8"))
        self.assertEqual(sorted(record["copied"]), ["runs.json", "settings.json", "shopping.json", "sounds/heroic.mp3"])
        self.assertFalse((self.box.store / "positions.json").exists())


class TrackerPanelControlTests(unittest.TestCase):
    """The POST endpoints."""

    def setUp(self):
        self.box = TrackerSandbox(self)
        self.box.call("GET", "/api/tracker")

    def test_pause_takes_a_boolean(self):
        code, body = self.box.call("POST", "/api/tracker/pause", {"on": True})
        self.assertEqual((code, body.get("ok")), (200, True), body)
        self.assertIs(self.box.call("GET", "/api/tracker")[1]["snapshot"]["paused"], True)
        code, body = self.box.call("POST", "/api/tracker/pause", {"on": "yes"})
        self.assertEqual(code, 400, body)
        self.assertIn("err", body)
        code, body = self.box.call("POST", "/api/tracker/pause", {"on": False})
        self.assertEqual(code, 200, body)
        self.assertIs(self.box.call("GET", "/api/tracker")[1]["snapshot"]["paused"], False)

    def test_a_control_is_persisted_in_the_cursor(self):
        self.box.call("POST", "/api/tracker/pause", {"on": True})
        cursor = json.loads((self.box.store / "cursor.json").read_text(encoding="utf-8"))
        controls = cursor["sessions"][SESSION]["controls"]
        self.assertEqual([(c["kind"], c["on"]) for c in controls], [("pause", True)])

    def test_reset_files_the_run_and_clears_the_session(self):
        code, body = self.box.call("POST", "/api/tracker/reset")
        self.assertEqual((code, body.get("ok")), (200, True), body)
        runs = self.box.call("GET", "/api/tracker/runs")[1]["runs"]
        self.assertEqual(len(runs), 2, runs)
        self.assertEqual((runs[0]["gold"], runs[0]["xp"], runs[0]["kills"]), (200, 400, 6))
        snap = self.box.call("GET", "/api/tracker")[1]["snapshot"]
        self.assertEqual(snap["gold"]["earned"], 0)
        self.assertEqual(snap["character"]["name"], "Sorak")

    def test_a_control_with_no_session_is_refused(self):
        empty = TrackerSandbox(self, session=False)
        code, body = empty.call("POST", "/api/tracker/reset")
        self.assertEqual(code, 400, body)
        self.assertIn("err", body)

    def test_settings_are_type_checked_and_unknown_fields_kept(self):
        doc = self.box.call("GET", "/api/tracker/settings")[1]["settings"]
        code, body = self.box.call("POST", "/api/tracker/settings", dict(doc, alerts="Heroic"))
        self.assertEqual(code, 400, body)
        self.assertIn("err", body)
        code, body = self.box.call("POST", "/api/tracker/settings", dict(doc, alerts=["Heroic"], panel_only=3))
        self.assertEqual((code, body.get("ok")), (200, True), body)
        saved = self.box.call("GET", "/api/tracker/settings")[1]["settings"]
        self.assertEqual((saved["alerts"], saved["panel_only"], saved["tracker_only_field"]),
                         (["Heroic"], 3, {"kept": True}))

    def test_import_copies_only_what_is_missing(self):
        (self.box.store / "shopping.json").unlink()
        (self.box.store / "runs.json").write_text("[]", encoding="utf-8")
        code, body = self.box.call("POST", "/api/tracker/import")
        self.assertEqual((code, body.get("ok")), (200, True), body)
        self.assertEqual(body["copied"], ["shopping.json"])
        self.assertEqual(json.loads((self.box.store / "runs.json").read_text(encoding="utf-8")), [])


class TrackerWatcherTests(unittest.TestCase):
    """The watcher's tracker step."""

    def test_a_player_who_never_turned_it_on_gets_no_store_and_no_import(self):
        box = TrackerSandbox(self, running=False, session=False)
        forgepact.tracker_watch(forgepact.load_cfg(), True)
        self.assertFalse(box.store.exists())

    def test_with_the_switch_on_the_step_ingests_and_finalizes(self):
        box = TrackerSandbox(self)
        box.live.post("mod_tracker", True)
        forgepact.tracker_watch(forgepact.load_cfg(), True)
        self.assertEqual(forgepact.tracker_service().snapshot()["gold"]["earned"], 200)
        box.game_live[777] = False
        forgepact.tracker_watch(forgepact.load_cfg(), False)
        cursor = json.loads((box.store / "cursor.json").read_text(encoding="utf-8"))
        self.assertIs(cursor["sessions"][SESSION]["finalized"], True)
        self.assertEqual(len(forgepact.tracker_service().runs()), 2)

    def test_a_raising_tracker_does_not_stop_the_launch_watcher(self):
        class Stop(BaseException):
            pass

        sleeps = []

        def sleep(_seconds):
            sleeps.append(1)
            if len(sleeps) > 2:
                raise Stop()

        cfg = {**defaults(), "auto_apply": True, "mod_tracker": True}
        with patch.object(forgepact.time, "sleep", side_effect=sleep), \
                patch.object(forgepact, "load_cfg", return_value=cfg), \
                patch.object(forgepact, "game_running", return_value=True), \
                patch.object(forgepact, "plugin_boot_generation", return_value=1), \
                patch.object(forgepact, "wait_for_plugin_ready", return_value=True), \
                patch.object(forgepact, "apply_all") as apply_all, \
                patch.object(forgepact, "write_panel_json"), \
                patch.object(forgepact, "watch_game_exit"), \
                patch.object(forgepact, "tracker_watch", side_effect=RuntimeError("tracker broke")) as watch:
            with self.assertRaises(Stop):
                forgepact.watcher()
        apply_all.assert_called_once_with(cfg)
        self.assertEqual(watch.call_count, 2)


if __name__ == "__main__":
    unittest.main()
