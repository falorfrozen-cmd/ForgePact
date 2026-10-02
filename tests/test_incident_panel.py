#!/usr/bin/env python3
"""The panel's half of incident reports (issue #76).

The plugin writes every report bundle under `bp_ipc\\reports\\`; the panel only
reads that directory, and contributes two small files of its own: `exit.json`
(what it saw when the game exited with a non-zero code, folded into the next
crash bundle by the plugin) and `panel.json` (its version and pid; the plugin
records the version in a report). Every report is recorded and listed, and
nobody is told about any of them: the panel starts no process to show a
notice (the owner, 2026-10-02, "No notice at all"). What is pinned here:

- the exit code is read from a handle held on the process, through a real
  child that exits with a signed `0xC0000005`;
- the Application log's crash record is parsed from an inline fixture shaped
  like a real one, and the real `wevtutil` read is checked to answer at all
  (`queried`, `records_seen`), so "no record" can be told from "could not
  read" (a machine with no record skips, it never passes);
- `exit.json` is written for a non-zero code and not for 0, and a crash exit
  starts no process but the event-log read;
- a new FPS-drop, freeze and crash report are all listed by `/api/state`,
  and the watcher's incident pass starts no process for any of them;
- no notice path remains on the module (no toast, no setting for one), and
  `/api/state`'s `incidents` carries the reports, the last exit and the exit
  watch only;
- the reports listing, `panel.json`, `/api/set` and `/api/state`;
- the exit watch leaves a trace (D15): it counts the pid it holds, the exits
  it read and the last code, so "no exit was recorded" can be told from "the
  panel never saw the game";
- an exit after ForgePact's clean-shutdown marker (a mod file aborting
  during exit, Known Limitations item 25) is written with
  `after_clean_shutdown` true for the plugin to fold in, and the same exit
  without the marker with it false;
- the keys the panel writes match the shared fixture the plugin's harness
  reads (`tests/fixtures/incident/`).

Everything is written under `tempfile`; the only port bound is the sandbox's
own (port 0).
"""

import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

TESTS = Path(__file__).resolve().parent
sys.path.insert(0, str(TESTS))
sys.path.insert(0, str(TESTS.parent / "src"))

import forgepact  # noqa: E402
from test_satanic_panel import PanelSandbox  # noqa: E402

FIXTURES = TESTS / "fixtures" / "incident"
BANNER = "==== BloodPact plugin loaded ==== v2.2.0"


def reset_incidents():
    """The module's incident state as a fresh panel has it: the counters are
    module globals, so a test that counts starts from zero."""
    forgepact.INCIDENTS["lastExit"] = None
    forgepact.INCIDENTS["exitWatch"] = {"pidHeld": None, "exitsSeen": 0, "lastCode": None}


# Every name the panel's notice path had (the owner, 2026-10-02: no notice
# of any kind). None of them may exist on the module.
NOTICE_NAMES = ("ToastNotificationManager", "show_toast", "toast_command", "INCIDENT_TOASTS",
                "TOAST_APP_ID", "notify_new_reports", "_TOAST_SCRIPT")


class ProcessGuard:
    """Patches every way the panel could start a process to tell someone
    (subprocess.run, subprocess.Popen, os.startfile) with mocks, so a test can
    see what was started. `run` answers like a `wevtutil` read that found
    EVENT_XML, so the event-log read still goes through the real path."""

    def __init__(self, test):
        run = patch.object(forgepact.subprocess, "run")
        popen = patch.object(forgepact.subprocess, "Popen")
        startfile = patch.object(forgepact.os, "startfile", create=True)
        self.run, self.popen, self.startfile = run.start(), popen.start(), startfile.start()
        for p in (startfile, popen, run):
            test.addCleanup(p.stop)
        self.run.return_value.returncode = 0
        self.run.return_value.stdout = EVENT_XML.encode("utf-8")
        self.run.return_value.stderr = b""

    def argvs(self):
        return [list(c.args[0]) if c.args else list(c.kwargs.get("args", [])) for c in self.run.call_args_list]

    def started_nothing(self, test):
        test.assertEqual(self.argvs(), [])
        self.popen.assert_not_called()
        self.startfile.assert_not_called()


def key_shape(value):
    """A JSON value's keys, nested objects included, with the values left out."""
    if isinstance(value, dict):
        return {k: key_shape(v) for k, v in value.items()}
    return None

# A record of the shape this machine's Application log answered with
# (record 71576, 2026-10-02), trimmed to the fields the panel reads; the
# names and numbers are not Hero Siege's.
EVENT_XML = (
    "<Event xmlns='http://schemas.microsoft.com/win/2004/08/events/event'><System>"
    "<Provider Name='Application Error' Guid='{a0e9b465-b939-57d7-b27d-95d8e925ff57}'/>"
    "<EventID>1000</EventID><TimeCreated SystemTime='2026-10-02T07:14:02.7028347Z'/>"
    "<EventRecordID>71576</EventRecordID><Channel>Application</Channel></System><EventData>"
    "<Data Name='AppName'>GbtCloudMatrix.exe</Data><Data Name='AppVersion'>22.9.21.1</Data>"
    "<Data Name='ModuleName'>KERNELBASE.dll</Data><Data Name='ExceptionCode'>e0434352</Data>"
    "<Data Name='FaultingOffset'>00000000000c483a</Data><Data Name='ProcessId'>0x2dc4</Data>"
    "</EventData></Event>"
    "<Event xmlns='http://schemas.microsoft.com/win/2004/08/events/event'><System>"
    "<Provider Name='Application Error'/><EventID>1000</EventID>"
    "<TimeCreated SystemTime='2026-10-02T07:20:00.0000000Z'/><EventRecordID>71590</EventRecordID>"
    "</System><EventData><Data Name='AppName'>Hero_Siege.exe</Data>"
    "<Data Name='ModuleName'>BloodPactPlugin.dll</Data><Data Name='ExceptionCode'>c0000005</Data>"
    "<Data Name='FaultingOffset'>0000000000012345</Data><Data Name='ProcessId'>0x1234</Data>"
    "</EventData></Event>"
)


class GameDir:
    """A throwaway game folder: `<tmp>/Hero_Siege.exe` and its `bp_ipc`."""

    def __init__(self, test):
        self.temp = tempfile.TemporaryDirectory(prefix="forgepact-incident-")
        test.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.exe = self.root / "Hero_Siege.exe"
        self.exe.write_bytes(b"")
        self.ipc = self.root / "bp_ipc"
        self.ipc.mkdir()
        self.reports = self.ipc / "reports"
        self.cfg = {**forgepact.DEFAULTS, "game_exe": str(self.exe)}

    def report(self, name, utc=None):
        d = self.reports / name
        d.mkdir(parents=True)
        if utc is not None:
            (d / "report.json").write_text(json.dumps({"kind": name.split("_")[-1], "utc": utc}), encoding="utf-8")
        return d


@unittest.skipUnless(os.name == "nt", "Win32 process handles")
class ExitCodeTests(unittest.TestCase):
    def _child(self, code):
        # os._exit takes a C int, so 0xC0000005 is passed signed; Windows
        # reports it back as the DWORD 0xC0000005.
        return subprocess.Popen([sys.executable, "-c", f"import os,time; time.sleep(0.3); os._exit({code})"])

    def test_the_held_handle_reads_an_access_violation(self):
        proc = self._child(-1073741819)
        handle = forgepact.open_exit_handle(proc.pid)
        self.assertTrue(handle, "OpenProcess on our own child failed")
        try:
            self.assertIsNone(forgepact.exit_code_of(handle), "a running process has no exit code yet")
            proc.wait(timeout=30)
            code = forgepact.exit_code_of(handle)
        finally:
            forgepact.close_exit_handle(handle)
        self.assertEqual(code, 0xC0000005)
        self.assertEqual(forgepact.exit_code_text(code), "0xC0000005")

    def test_a_clean_exit_reads_zero(self):
        # The control: the same instrument reads 0 when nothing went wrong.
        proc = self._child(0)
        handle = forgepact.open_exit_handle(proc.pid)
        self.assertTrue(handle)
        try:
            proc.wait(timeout=30)
            self.assertEqual(forgepact.exit_code_of(handle), 0)
        finally:
            forgepact.close_exit_handle(handle)

    def test_no_handle_for_a_pid_that_cannot_be_opened(self):
        # A pid that has gone may be recycled by the time it is opened, so the
        # refusal is shown on pid 0, which no user process can open.
        self.assertIsNone(forgepact.open_exit_handle(0))


class EventLogTests(unittest.TestCase):
    def test_the_parser_reads_every_field_the_report_needs(self):
        events = forgepact.parse_app_error_events(EVENT_XML)
        self.assertEqual(len(events), 2)
        first = events[0]
        self.assertEqual(first["app_name"], "GbtCloudMatrix.exe")
        self.assertEqual(first["module_name"], "KERNELBASE.dll")
        self.assertEqual(first["exception_code"], "e0434352")
        self.assertEqual(first["faulting_offset"], "00000000000c483a")
        self.assertEqual(first["event_record_id"], 71576)
        self.assertEqual(first["process_id"], 0x2DC4)
        self.assertEqual(first["time_created"], "2026-10-02T07:14:02.7028347Z")

    def test_the_parser_answers_nothing_for_nothing(self):
        self.assertEqual(forgepact.parse_app_error_events(""), [])
        self.assertEqual(forgepact.parse_app_error_events("not xml <"), [])

    def test_the_game_record_is_chosen_by_pid_then_by_name(self):
        events = forgepact.parse_app_error_events(EVENT_XML)
        self.assertEqual(forgepact.match_game_event(events, "Hero_Siege.exe", 0x1234)["event_record_id"], 71590)
        # Pid unknown: the newest record naming the exe.
        self.assertEqual(forgepact.match_game_event(events, "hero_siege.exe", None)["event_record_id"], 71590)
        # Another process's crash never stands in for the game's.
        self.assertIsNone(forgepact.match_game_event(events[:1], "Hero_Siege.exe", 0x1234))

    @unittest.skipUnless(os.name == "nt", "the Windows Application log")
    def test_the_real_log_is_read_without_admin_rights(self):
        events, probe = forgepact.query_app_errors()
        self.assertTrue(probe["queried"], f"wevtutil did not answer: {probe}")
        if probe["records_seen"] == 0:
            self.skipTest("this machine's Application log holds no Application Error 1000 record; "
                          "the read worked but there is nothing to parse")
        self.assertGreaterEqual(probe["records_seen"], 1)
        self.assertEqual(len(events), probe["records_seen"])
        self.assertTrue(all(isinstance(e["event_record_id"], int) for e in events), events[:2])


class ExitRecordTests(unittest.TestCase):
    def setUp(self):
        self.game = GameDir(self)
        reset_incidents()
        self.addCleanup(reset_incidents)

    def _out(self, *lines):
        (self.game.ipc / "out.txt").write_text("".join(line + "\n" for line in lines), encoding="utf-8")

    def _record(self, code, events=(), probe=None):
        """record_game_exit() with the log read answered by `events`; every
        process start is caught by a guard that this returns."""
        probe = probe or {"queried": True, "records_seen": len(events)}
        guard = ProcessGuard(self)
        with patch.object(forgepact, "query_app_errors", return_value=(list(events), probe)) as query:
            facts = forgepact.record_game_exit(self.game.cfg, code, pid=0x1234, attempts=1)
        return facts, query, guard

    def test_a_crash_exit_is_recorded_without_any_notice(self):
        # The owner, 2026-10-02: a crash exit is written for the plugin's next
        # crash bundle and shown in /api/state, and nobody is told. The real
        # log read runs through the guard: the only process started is
        # wevtutil's.
        guard = ProcessGuard(self)
        facts = forgepact.record_game_exit(self.game.cfg, 0xC0000005, pid=0x1234, attempts=1)
        written = json.loads((self.game.ipc / "exit.json").read_text(encoding="utf-8"))
        self.assertEqual(written, facts)
        self.assertEqual(forgepact.INCIDENTS["lastExit"], written)
        self.assertEqual(written["exit_code"], "0xC0000005")
        self.assertRegex(written["exit_utc"], r"^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\dZ$")
        self.assertIs(written["after_clean_shutdown"], False)
        argvs = guard.argvs()
        self.assertTrue(all(argv and argv[0] == "wevtutil" for argv in argvs), argvs)
        guard.popen.assert_not_called()
        guard.startfile.assert_not_called()
        if os.name == "nt":
            # The positive control: the guard saw the event-log read, and its
            # answer reached exit.json.
            self.assertEqual(len(argvs), 1, argvs)
            self.assertEqual(written["faulting_module"], "BloodPactPlugin.dll")
            self.assertEqual(written["faulting_offset"], "0000000000012345")
            self.assertEqual(written["exception_code"], "c0000005")
            self.assertEqual(written["event_record_id"], 71590)
            self.assertEqual(written["event_probe"], {"queried": True, "records_seen": 2})

    def test_no_record_and_no_read_are_told_apart(self):
        facts, _, _ = self._record(1, (), {"queried": True, "records_seen": 0})
        self.assertEqual(facts["exit_code"], "0x00000001")
        self.assertIsNone(facts["faulting_module"])
        self.assertIsNone(facts["event_record_id"])
        self.assertEqual(facts["event_probe"], {"queried": True, "records_seen": 0})
        facts, _, _ = self._record(1, (), {"queried": False, "records_seen": 0})
        self.assertEqual(facts["event_probe"], {"queried": False, "records_seen": 0})

    def test_a_clean_exit_writes_nothing_and_clears_the_last_exit(self):
        forgepact.INCIDENTS["lastExit"] = {"exit_code": "0xC0000005"}
        facts, query, guard = self._record(0)
        self.assertIsNone(facts)
        self.assertFalse((self.game.ipc / "exit.json").exists())
        self.assertIsNone(forgepact.INCIDENTS["lastExit"])
        query.assert_not_called()
        guard.started_nothing(self)

    def test_without_bp_ipc_nothing_is_created(self):
        (self.game.ipc).rmdir()
        facts, _, _ = self._record(0xC0000005)
        self.assertEqual(facts["exit_code"], "0xC0000005")
        self.assertFalse(self.game.ipc.exists(), "the panel must not create bp_ipc")

    def test_an_exit_after_a_clean_shutdown_is_recorded(self):
        # A mod file aborting during exit (Known Limitations item 25) ends the
        # game with 0xC0000409 after ForgePact already wrote its marker: the
        # exit is written for the plugin to fold into its next-load note,
        # marked as coming after a clean shutdown.
        self._out(BANNER, "incident: monitor running", "==== clean shutdown ====")
        facts, _, guard = self._record(0xC0000409)
        written = json.loads((self.game.ipc / "exit.json").read_text(encoding="utf-8"))
        self.assertIs(written["after_clean_shutdown"], True)
        self.assertEqual(written["exit_code"], "0xC0000409")
        self.assertEqual(forgepact.INCIDENTS["lastExit"], written)
        self.assertIs(forgepact.INCIDENTS["lastExit"]["after_clean_shutdown"], True)
        guard.started_nothing(self)
        # The plugin's unload fallback names its route; the prefix is what counts.
        self._out(BANNER, "==== clean shutdown (detach) ====")
        facts, _, guard = self._record(0xC0000409)
        self.assertIs(facts["after_clean_shutdown"], True)
        guard.started_nothing(self)

    def test_without_the_marker_the_same_exit_is_recorded_as_after_no_clean_shutdown(self):
        # The control for the test above: the same code with no marker in this
        # session is written with after_clean_shutdown false, and still
        # starts no process.
        def recorded_without_a_clean_shutdown():
            facts, _, guard = self._record(0xC0000409)
            written = json.loads((self.game.ipc / "exit.json").read_text(encoding="utf-8"))
            self.assertEqual(written, facts)
            self.assertIs(written["after_clean_shutdown"], False)
            self.assertEqual(forgepact.INCIDENTS["lastExit"], written)
            guard.started_nothing(self)
        self._out(BANNER, "incident: monitor running")
        recorded_without_a_clean_shutdown()
        # A marker before the last banner is the previous session's.
        self._out(BANNER, "==== clean shutdown ====", BANNER, "incident: monitor running")
        recorded_without_a_clean_shutdown()
        # The prefix counts only at the start of a line.
        self._out(BANNER, "chat: said ==== clean shutdown ====")
        recorded_without_a_clean_shutdown()
        # No out.txt at all is not a clean shutdown.
        (self.game.ipc / "out.txt").unlink()
        recorded_without_a_clean_shutdown()

    def test_a_marker_beyond_the_read_tail_still_counts_after_its_banner(self):
        # Only out.txt's tail is read (it grows to megabytes): a banner far
        # above it still leaves the tail inside this session.
        self._out(BANNER, *["x" * 200] * 2000, "==== clean shutdown ====")
        facts, _, guard = self._record(0xC0000409)
        self.assertIs(facts["after_clean_shutdown"], True)
        guard.started_nothing(self)


class SharedFixtureTests(unittest.TestCase):
    """The plugin reads exit.json and panel.json through IncidentMonitor.hpp's
    parsers; the harness scenario exit-json-fixture reads the same two files.
    These hold the panel's writers to the same keys."""

    def setUp(self):
        self.game = GameDir(self)
        reset_incidents()
        self.addCleanup(reset_incidents)

    def test_the_fixture_matches_the_writers(self):
        fixture_exit = json.loads((FIXTURES / "exit.json").read_text(encoding="utf-8"))
        fixture_panel = json.loads((FIXTURES / "panel.json").read_text(encoding="utf-8"))
        events = forgepact.parse_app_error_events(EVENT_XML)
        with patch.object(forgepact, "query_app_errors", return_value=(events, {"queried": True, "records_seen": 2})):
            facts = forgepact.record_game_exit(self.game.cfg, 0xC0000005, pid=0x1234, attempts=1)
        written_exit = json.loads((self.game.ipc / "exit.json").read_text(encoding="utf-8"))
        self.assertEqual(key_shape(facts), key_shape(fixture_exit))
        self.assertEqual(key_shape(written_exit), key_shape(fixture_exit))
        self.assertEqual(set(written_exit["event_probe"]), set(fixture_exit["event_probe"]))
        self.assertTrue(forgepact.write_panel_json(self.game.cfg))
        written_panel = json.loads((self.game.ipc / "panel.json").read_text(encoding="utf-8"))
        self.assertEqual(key_shape(written_panel), key_shape(fixture_panel))
        # The value kinds the plugin's parsers expect: strings and a number.
        self.assertIsInstance(written_exit["exit_code"], str)
        self.assertIsInstance(written_panel["version"], str)
        self.assertIsInstance(written_panel["pid"], int)


class CounterTests(unittest.TestCase):
    """The exit watch leaves a count in /api/state."""

    def setUp(self):
        self.game = GameDir(self)
        reset_incidents()
        self.addCleanup(reset_incidents)

    @unittest.skipUnless(os.name == "nt", "Win32 process handles")
    def test_the_exit_watch_is_counted(self):
        watch = forgepact.INCIDENTS["exitWatch"]
        hold = {"pid": None, "handle": None}
        proc = subprocess.Popen([sys.executable, "-c", "import os,time; time.sleep(0.5); os._exit(-1073741819)"])
        self.addCleanup(proc.kill)
        with patch.object(forgepact, "game_pid", return_value=proc.pid), \
                patch.object(forgepact, "record_game_exit", return_value=None) as record:
            forgepact.watch_game_exit(self.game.cfg, True, hold)
            self.assertEqual(forgepact.INCIDENTS["exitWatch"]["pidHeld"], proc.pid)
            self.assertEqual(forgepact.INCIDENTS["exitWatch"]["exitsSeen"], 0)
            proc.wait(timeout=30)
            forgepact.watch_game_exit(self.game.cfg, False, hold)
            # Nothing held and nothing running: a pass that reads nothing counts nothing.
            forgepact.watch_game_exit(self.game.cfg, False, hold)
        record.assert_called_once_with(self.game.cfg, 0xC0000005, proc.pid)
        watch = forgepact.INCIDENTS["exitWatch"]
        self.assertEqual(watch, {"pidHeld": None, "exitsSeen": 1, "lastCode": "0xC0000005"})
        # The counts reach /api/state's incidents, as a copy.
        state = forgepact.incidents_state(self.game.cfg)
        self.assertEqual(state["exitWatch"], {"pidHeld": None, "exitsSeen": 1, "lastCode": "0xC0000005"})
        state["exitWatch"]["exitsSeen"] = 99
        self.assertEqual(forgepact.INCIDENTS["exitWatch"]["exitsSeen"], 1, "/api/state must get a copy")


class ExitWatchTests(unittest.TestCase):
    """watch_game_exit(): open on first sight, read and close once it ended."""

    def test_the_handle_is_held_while_running_and_read_after(self):
        game = GameDir(self)
        hold = {"pid": None, "handle": None}
        with patch.object(forgepact, "game_pid", return_value=4321), \
                patch.object(forgepact, "open_exit_handle", return_value=99) as opened, \
                patch.object(forgepact, "exit_code_of", side_effect=[None, 0xC0000005]), \
                patch.object(forgepact, "close_exit_handle") as closed, \
                patch.object(forgepact, "record_game_exit", return_value={"exit_code": "0xC0000005"}) as record:
            forgepact.watch_game_exit(game.cfg, True, hold)      # first sight: open
            self.assertEqual(hold, {"pid": 4321, "handle": 99})
            forgepact.watch_game_exit(game.cfg, True, hold)      # still running
            record.assert_not_called()
            forgepact.watch_game_exit(game.cfg, False, hold)     # it ended
        opened.assert_called_once_with(4321)
        closed.assert_called_once_with(99)
        record.assert_called_once_with(game.cfg, 0xC0000005, 4321)
        self.assertEqual(hold, {"pid": None, "handle": None})


class ReportListingTests(unittest.TestCase):
    def test_newest_first_at_most_ten_reports_only(self):
        game = GameDir(self)
        for i in range(12):
            game.report(f"20261002-1000{i:02d}_perf")
        game.report("20261002-110000_crash", utc="2026-10-02T09:00:00Z")
        game.report("not-a-report")
        (game.reports / "20261002-120000_perf.txt").write_text("a file, not a bundle", encoding="utf-8")
        listing = forgepact.incident_reports(game.cfg)
        self.assertEqual(len(listing), 10)
        self.assertEqual(listing[0], {"dir": "20261002-110000_crash", "kind": "crash", "utc": "2026-10-02T09:00:00Z"})
        self.assertEqual(listing[1], {"dir": "20261002-100011_perf", "kind": "perf", "utc": "2026-10-02T10:00:11"})
        self.assertEqual([r["dir"] for r in listing], sorted((r["dir"] for r in listing), reverse=True))

    def test_no_reports_folder_is_an_empty_list(self):
        game = GameDir(self)
        self.assertEqual(forgepact.incident_reports(game.cfg), [])


class PanelJsonTests(unittest.TestCase):
    def test_version_and_pid_rewritten_only_on_change(self):
        game = GameDir(self)
        self.assertTrue(forgepact.write_panel_json(game.cfg))
        path = game.ipc / "panel.json"
        self.assertEqual(json.loads(path.read_text(encoding="utf-8")),
                         {"version": forgepact.__version__, "pid": os.getpid()})
        self.assertFalse(forgepact.write_panel_json(game.cfg), "same content must not be rewritten")
        path.write_text('{"version": "0.0.0", "pid": 1}', encoding="utf-8")
        self.assertTrue(forgepact.write_panel_json(game.cfg))

    def test_never_creates_bp_ipc(self):
        game = GameDir(self)
        game.ipc.rmdir()
        self.assertFalse(forgepact.write_panel_json(game.cfg))
        self.assertFalse(game.ipc.exists())


class ApiTests(unittest.TestCase):
    def setUp(self):
        self.sandbox = PanelSandbox()
        self.sandbox.__enter__()
        self.addCleanup(self.sandbox.__exit__)
        reset_incidents()
        self.addCleanup(reset_incidents)

    def _post(self, path, body):
        from http.client import HTTPConnection
        connection = HTTPConnection("127.0.0.1", self.sandbox.port, timeout=5)
        try:
            connection.request("POST", path, json.dumps(body), {"Content-Type": "application/json"})
            response = connection.getresponse()
            return response.status, json.loads(response.read())
        finally:
            connection.close()

    def test_state_carries_the_incidents_block(self):
        code, state = self.sandbox.request()
        self.assertEqual(code, 200)
        self.assertEqual(state["incidents"], {
            "reports": [], "lastExit": None,
            "exitWatch": {"pidHeld": None, "exitsSeen": 0, "lastCode": None}})

    def test_no_fps_drop_setting_remains(self):
        # The Setup switch went with the FPS-drop notice: no default, no panel
        # setting, nothing in /api/state.
        self.assertNotIn("notify_lag", forgepact.DEFAULTS)
        self.assertEqual(forgepact.PANEL_SETTINGS, ("theme",))
        _, state = self.sandbox.request()
        self.assertNotIn("notifyLag", state["incidents"])
        self.assertNotIn("notify_lag", state["cfg"])

    def test_no_notice_path_remains(self):
        # The owner, 2026-10-02 ("No notice at all"): nothing on the module
        # can show a notice, and what is recorded is all /api/state carries.
        # The control: the same lookup finds the names that do the recording.
        for name in ("record_game_exit", "incident_reports", "incidents_state", "INCIDENTS"):
            self.assertTrue(hasattr(forgepact, name), name)
        for name in NOTICE_NAMES:
            self.assertFalse(hasattr(forgepact, name), name)
        self.assertEqual(set(forgepact.INCIDENTS), {"lastExit", "exitWatch"})
        _, state = self.sandbox.request()
        self.assertEqual(set(state["incidents"]), {"reports", "lastExit", "exitWatch"})

    def test_new_reports_of_every_kind_are_listed_without_any_notice(self):
        # Two passes of the real watcher(): the first sees no report, then an
        # FPS-drop, a freeze and a crash report appear, and the second pass
        # sees them. Nothing is started for any of them (the old watcher
        # toasted the freeze and the crash through PowerShell here), and all
        # three are listed by /api/state.
        root = Path(self.sandbox.temp.name)
        (root / "bp_ipc").mkdir()
        reports = root / "bp_ipc" / "reports"
        names = ("20261002-100000_perf", "20261002-100100_freeze", "20261002-100200_crash")

        class StopWatcher(Exception):
            pass

        passes = []

        def sleep(_seconds):
            passes.append(_seconds)
            if len(passes) == 2:
                for name in names:
                    (reports / name).mkdir(parents=True)
            elif len(passes) == 3:
                raise StopWatcher

        guard = ProcessGuard(self)
        with patch.object(forgepact.time, "sleep", side_effect=sleep):
            with self.assertRaises(StopWatcher):
                forgepact.watcher()
        self.assertEqual(len(passes), 3)
        # The positive control: the incident pass ran (it wrote panel.json).
        self.assertTrue((root / "bp_ipc" / "panel.json").is_file(), "the watcher's incident pass never ran")
        guard.started_nothing(self)
        _, state = self.sandbox.request()
        listed = {r["dir"]: r["kind"] for r in state["incidents"]["reports"]}
        self.assertEqual(listed, {names[0]: "perf", names[1]: "freeze", names[2]: "crash"})

    def test_openreports_opens_the_reports_folder(self):
        reports = Path(self.sandbox.temp.name) / "bp_ipc" / "reports"
        with patch.object(forgepact.os, "startfile", create=True) as start:
            code, body = self._post("/api/openreports", {})
            self.assertEqual(code, 200)
            self.assertIn("err", body)          # no folder yet: nothing is opened or created
            start.assert_not_called()
            self.assertFalse(reports.exists())
            reports.mkdir(parents=True)
            code, body = self._post("/api/openreports", {})
        self.assertIn("ok", body)
        start.assert_called_once_with(str(reports))


if __name__ == "__main__":
    unittest.main()
