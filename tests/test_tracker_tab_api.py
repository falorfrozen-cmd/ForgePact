#!/usr/bin/env python3
"""What the Tracker tab, its alerts and its overlay read from the panel (#185).

Part 2a's `/api/tracker*` endpoints are pinned by
`test_tracker_panel_contract.py`, which stays as it is; this file pins only
what the Tracker tab adds, plus a baseline that each existing endpoint still
answers as before:

- `GET /api/tracker` gains `zone_alert` (the latest Satanic-zone rotation
  that passed the buff pick, `seq` rising by 1 per rotation, never consumed
  by reading) and `overlay` (`{"available", "open"}`);
- `POST /api/tracker/announce`: only the first claim of an id plays, and a
  claimed entry or zone alert reads `announced: true`;
- custom sounds (`GET /api/tracker/sounds`, `GET`/`POST
  /api/tracker/sound/<key>`): keys, kinds, size and content checked, written
  whole, one file per key;
- the shopping list, the item codex and the run history's clear;
- `PANEL_MIME` serving the bundled sounds, and the autoplay flag `main()`
  sets before `webview.start()`;
- `tests/panel_sandbox_server.py`, run as the browser suites run it with the
  user's home pointed at a temporary one, writes nothing under that home and
  reads nothing from its HS Offline Tracker folder.

Every HTTP test runs on `TrackerSandbox` (test_tracker_panel_contract.py):
the panel's ROOT, the Tracker's folder and the process check in a temporary
directory, and the Tracker's folder checked unchanged after each test.
"""

import base64
import inspect
import json
import os
import shutil
import subprocess
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
from forgepact_tracker import codex  # noqa: E402
from forgepact_tracker import store as tracker_store  # noqa: E402
from test_tracker_panel_contract import FIXTURE, SESSION, TrackerSandbox, fake_tracker_folder, folder_state  # noqa: E402

SANDBOX_SERVER = TESTS / "panel_sandbox_server.py"
JOURNAL_ID = SESSION + ":9"   # the fixture's Heroic ground drop

# Each kind's smallest file that starts the way the kind does.
WAV = b"RIFF\x24\x00\x00\x00WAVEfmt " + b"\x00" * 24
MP3 = b"ID3\x04\x00" + b"\x00" * 16
MP3_BARE = b"\xff\xfb\x90\x00" + b"\x00" * 16
OGG = b"OggS" + b"\x00" * 24
FLAC = b"fLaC" + b"\x00" * 24


def b64(data: bytes) -> str:
    return base64.b64encode(data).decode("ascii")


class Box(TrackerSandbox):
    """TrackerSandbox plus raw replies and records appended to the session."""

    def __init__(self, test, **kwargs):
        super().__init__(test, **kwargs)
        self.sequence = 12

    def raw(self, method, path):
        connection = HTTPConnection("127.0.0.1", self.live.sandbox.port, timeout=10)
        try:
            connection.request(method, path)
            response = connection.getresponse()
            return response.status, dict(response.getheaders()), response.read()
        finally:
            connection.close()

    def append(self, kind, at_ms, **fields):
        self.sequence += 1
        record = {"protocol": "hs-offline-tracker/1", "v": 1, "kind": kind, "sequence": self.sequence,
                  "emitted_at_unix_ms": at_ms, "process_id": 777, "build_id": "forgepact-fixture", **fields}
        with open(self.sessions / SESSION, "a", encoding="utf-8", newline="\n") as fh:
            fh.write(json.dumps(record) + "\n")
        return self.sequence

    def zone(self, at_ms, zone, buffs, debuffs=(3,)):
        return self.append("satanic_zone", at_ms, zone=zone, buffs=list(buffs), debuffs=list(debuffs))

    def state(self):
        code, body = self.call("GET", "/api/tracker")
        assert code == 200, body
        return body


class BaselineEndpointTests(unittest.TestCase):
    """Every endpoint part 2a added answers as it did."""

    def test_the_existing_endpoints_answer_as_before(self):
        box = Box(self)
        state = box.state()
        for key in ("on", "snapshot", "revision", "extra_revision", "catching_up", "session"):
            self.assertIn(key, state)
        self.assertEqual(state["session"], {"file": SESSION, "pid": 777, "live": True, "bad_lines": 0})
        self.assertEqual(state["snapshot"]["gold"]["earned"], 200)
        code, extra = box.call("GET", "/api/tracker/extra")
        self.assertEqual(code, 200)
        self.assertEqual([d["id"] for d in extra["drops"]], [JOURNAL_ID])
        self.assertEqual(set(extra), {"character", "series", "drops"})
        self.assertEqual(box.call("GET", "/api/tracker/runs")[0], 200)
        code, settings = box.call("GET", "/api/tracker/settings")
        self.assertEqual(code, 200)
        self.assertTrue(settings["settings"]["auto_show"])
        self.assertEqual(box.call("POST", "/api/tracker/pause", {"on": True}), (200, {"ok": True}))
        self.assertEqual(box.call("POST", "/api/tracker/pause", {"on": 1})[0], 400)
        self.assertEqual(box.call("POST", "/api/tracker/reset")[0], 200)
        self.assertEqual(box.call("POST", "/api/tracker/import")[0], 200)
        self.assertEqual(box.call("GET", "/api/tracker/nothing")[0], 404)

    def test_the_bundled_page_types_are_unchanged(self):
        for suffix, kind in ((".html", "text/html; charset=utf-8"), (".js", "text/javascript"),
                             (".css", "text/css"), (".json", "application/json")):
            self.assertEqual(forgepact.PANEL_MIME[suffix], kind)


class ZoneAlertTests(unittest.TestCase):
    def test_no_rotation_no_alert(self):
        state = Box(self).state()
        self.assertIsNone(state["zone_alert"], "learning where the zone is is not a rotation")

    def test_a_rotation_is_the_alert_and_reading_never_consumes_it(self):
        box = Box(self)
        box.state()
        box.zone(1_700_000_100_000, "Satanic_2_4", [7, 21], [5])
        alert = box.state()["zone_alert"]
        self.assertEqual(alert, {"seq": 1, "zone": "Satanic_2_4", "buffs": [7, 21], "debuffs": [5],
                                 "at_ms": 1_700_000_100_000, "announced": False})
        self.assertEqual(box.state()["zone_alert"], alert, "a second read sees the same alert")
        box.zone(1_700_000_200_000, "Satanic_3_1", [1])
        self.assertEqual(box.state()["zone_alert"]["seq"], 2, "seq rises by 1 per rotation")
        box.zone(1_700_000_300_000, "Satanic_3_1", [1])
        self.assertEqual(box.state()["zone_alert"]["seq"], 2, "the same zone said again is no rotation")

    def test_the_buff_pick_holds_back_a_rotation_without_a_wanted_buff(self):
        box = Box(self)
        code, settings = box.call("GET", "/api/tracker/settings")
        doc = dict(settings["settings"], zone_buffs=[14])
        self.assertEqual(box.call("POST", "/api/tracker/settings", doc)[0], 200)
        box.zone(1_700_000_100_000, "Satanic_2_4", [7])
        self.assertIsNone(box.state()["zone_alert"])
        box.zone(1_700_000_200_000, "Satanic_2_5", [7, 14])
        self.assertEqual(box.state()["zone_alert"]["zone"], "Satanic_2_5")

    def test_a_rotation_in_a_session_whose_game_is_gone_is_not_announced(self):
        box = Box(self)
        box.state()
        box.game_live[777] = False
        box.zone(1_700_000_100_000, "Satanic_2_4", [7])
        self.assertIsNone(box.state()["zone_alert"])


class AnnounceTests(unittest.TestCase):
    def test_only_the_first_claim_of_a_journal_entry_plays(self):
        box = Box(self)
        box.state()
        code, extra = box.call("GET", "/api/tracker/extra")
        self.assertIs(extra["drops"][0]["announced"], False)
        self.assertEqual(box.call("POST", "/api/tracker/announce", {"id": JOURNAL_ID}), (200, {"play": True}))
        self.assertEqual(box.call("POST", "/api/tracker/announce", {"id": JOURNAL_ID}), (200, {"play": False}))
        code, extra = box.call("GET", "/api/tracker/extra")
        self.assertIs(extra["drops"][0]["announced"], True)

    def test_a_zone_alert_is_claimed_by_its_seq(self):
        box = Box(self)
        box.state()
        box.zone(1_700_000_100_000, "Satanic_2_4", [7])
        seq = box.state()["zone_alert"]["seq"]
        self.assertEqual(box.call("POST", "/api/tracker/announce", {"id": "zone:%d" % seq})[1], {"play": True})
        self.assertEqual(box.call("POST", "/api/tracker/announce", {"id": "zone:%d" % seq})[1], {"play": False})
        self.assertIs(box.state()["zone_alert"]["announced"], True)
        box.zone(1_700_000_200_000, "Satanic_3_1", [7])
        nxt = box.state()["zone_alert"]
        self.assertEqual((nxt["seq"], nxt["announced"]), (seq + 1, False))
        self.assertEqual(box.call("POST", "/api/tracker/announce", {"id": "zone:%d" % seq})[1], {"play": False},
                         "an earlier rotation is no longer held")

    def test_an_id_nobody_holds_never_plays_and_a_bad_body_is_refused(self):
        box = Box(self)
        box.state()
        for unknown in ("zone:1", "zone:0", SESSION + ":999", "nothing"):
            self.assertEqual(box.call("POST", "/api/tracker/announce", {"id": unknown}), (200, {"play": False}))
        for bad in ({}, {"id": 9}, {"id": ""}):
            code, body = box.call("POST", "/api/tracker/announce", bad)
            self.assertEqual(code, 400, bad)
            self.assertIn("err", body)


class SoundTests(unittest.TestCase):
    def setUp(self):
        self.box = Box(self)
        self.sounds = self.box.store / "sounds"
        # the first read builds the store, whose import copies the fake
        # Tracker folder's sounds/heroic.mp3
        self.box.state()

    def post(self, key, body):
        return self.box.call("POST", "/api/tracker/sound/%s" % key, body)

    def files(self):
        return sorted(p.name for p in self.sounds.iterdir())

    def test_only_the_imported_sound_at_first(self):
        code, body = self.box.call("GET", "/api/tracker/sounds")
        self.assertEqual((code, set(body["sounds"])), (200, {"heroic"}))
        self.assertEqual(body["sounds"]["heroic"]["ext"], "mp3")
        code, headers, data = self.box.raw("GET", "/api/tracker/sound/heroic")
        self.assertEqual((code, headers["Content-Type"], data), (200, "audio/mpeg", b"ID3fixture"))
        code, headers, _ = self.box.raw("GET", "/api/tracker/sound/satanic")
        self.assertEqual(code, 404)
        self.assertEqual(self.box.raw("GET", "/api/tracker/sound/..%5Ccursor.json")[0], 404)

    def test_an_upload_is_listed_served_and_replaces_the_keys_other_kind(self):
        code, body = self.post("heroic", {"name": "my alert.WAV", "data": b64(WAV)})
        self.assertEqual(self.files(), ["heroic.wav"], "the imported heroic.mp3 gave way")
        self.assertEqual(code, 200, body)
        self.assertEqual(set(body["sounds"]), {"heroic"})
        self.assertEqual(body["sounds"]["heroic"]["ext"], "wav")
        first = body["sounds"]["heroic"]["stamp"]
        code, headers, data = self.box.raw("GET", "/api/tracker/sound/heroic")
        self.assertEqual((code, data), (200, WAV))
        self.assertEqual(headers["Content-Type"], "audio/wav")
        self.assertEqual(headers["Cache-Control"], "no-store")
        code, body = self.post("heroic", {"name": "other.mp3", "data": b64(MP3 + b"longer")})
        self.assertEqual(body["sounds"]["heroic"]["ext"], "mp3")
        self.assertNotEqual(body["sounds"]["heroic"]["stamp"], first, "the stamp follows the file")
        self.assertEqual(self.files(), ["heroic.mp3"], "one key, one file")
        self.assertEqual(self.box.call("GET", "/api/tracker/sounds")[1], {"sounds": body["sounds"]})

    def test_every_kind_and_a_list_key_is_filed(self):
        for key, name, data in (("satanic", "a.mp3", MP3_BARE), ("zone", "a.ogg", OGG),
                                ("set", "a.flac", FLAC), ("list-Ab_9-x", "a.wav", WAV)):
            code, body = self.post(key, {"name": name, "data": b64(data)})
            self.assertEqual(code, 200, (key, body))
        self.assertEqual(set(self.box.call("GET", "/api/tracker/sounds")[1]["sounds"]),
                         {"heroic", "satanic", "zone", "set", "list-Ab_9-x"})

    def test_what_is_refused_never_reaches_the_folder(self):
        refused = [
            ("mail", {"name": "a.wav", "data": b64(WAV)}),             # online-only, not ported
            ("list-", {"name": "a.wav", "data": b64(WAV)}),
            ("list-" + "x" * 65, {"name": "a.wav", "data": b64(WAV)}),
            ("..%5Cheroic", {"name": "a.wav", "data": b64(WAV)}),
            ("heroic", {"name": "a.aac", "data": b64(WAV)}),           # a kind it does not file
            ("heroic", {"name": "noext", "data": b64(WAV)}),
            ("heroic", {"name": "a.wav", "data": b64(MP3)}),           # content of another kind
            ("heroic", {"name": "a.mp3", "data": b64(b"plain text")}),
            ("heroic", {"name": "a.wav", "data": "not base64!"}),
            ("heroic", {"name": "a.wav", "data": ""}),
            ("heroic", {"name": "a.wav"}),
            ("heroic", {"copy_from": "mail"}),
        ]
        for key, body in refused:
            code, answer = self.post(key, body)
            self.assertEqual(code, 400, (key, answer))
            self.assertIn("err", answer)
        self.assertEqual(self.files(), ["heroic.mp3"], "only the imported sound")
        self.assertEqual((self.sounds / "heroic.mp3").read_bytes(), b"ID3fixture")

    def test_more_than_8_mib_is_refused(self):
        big = WAV + b"\x00" * (8 * 1024 * 1024)
        code, body = self.post("heroic", {"name": "big.wav", "data": b64(big)})
        self.assertEqual(code, 400, body)
        exactly = WAV + b"\x00" * (8 * 1024 * 1024 - len(WAV))
        self.assertEqual(self.post("heroic", {"name": "max.wav", "data": b64(exactly)})[0], 200)

    def test_clear_and_copy(self):
        self.post("list-a", {"name": "a.ogg", "data": b64(OGG)})
        code, body = self.post("list-b", {"copy_from": "list-a"})
        self.assertEqual((code, body["sounds"]["list-b"]["ext"]), (200, "ogg"))
        self.assertEqual((self.sounds / "list-b.ogg").read_bytes(), OGG)
        code, body = self.post("list-c", {"copy_from": "list-none"})
        self.assertEqual(code, 200, "nothing to copy is not an error")
        self.assertNotIn("list-c", body["sounds"])
        code, body = self.post("list-a", {"clear": True})
        self.assertEqual(code, 200)
        self.assertEqual(set(body["sounds"]), {"heroic", "list-b"})
        self.assertEqual(self.box.raw("GET", "/api/tracker/sound/list-a")[0], 404)

    def test_a_write_that_fails_leaves_the_old_sound_whole(self):
        self.post("heroic", {"name": "a.wav", "data": b64(WAV)})
        with patch.object(tracker_store.os, "replace", side_effect=OSError("disk full")):
            code, body = self.post("heroic", {"name": "b.wav", "data": b64(WAV + b"new")})
        self.assertEqual(code, 400, body)
        self.assertEqual((self.sounds / "heroic.wav").read_bytes(), WAV)
        self.assertEqual(sorted(p.name for p in self.sounds.iterdir()), ["heroic.wav"], "no temporary left")

    def test_the_import_s_sound_is_listed(self):
        # the fake Tracker folder carries sounds/heroic.mp3, copied by the import
        self.box.state()
        listing = self.box.call("GET", "/api/tracker/sounds")[1]["sounds"]
        self.assertEqual(listing["heroic"]["ext"], "mp3")


class ShoppingTests(unittest.TestCase):
    def test_the_list_round_trips_in_the_trackers_own_format(self):
        box = Box(self)
        self.assertEqual(box.call("GET", "/api/tracker/shopping"), (200, {"items": []}),
                         "the import's file is not a list of strings, so it reads as empty, as the Tracker read it")
        code, body = box.call("POST", "/api/tracker/shopping", {"items": ["  Jah Rune ", "Angel"]})
        self.assertEqual((code, body), (200, {"ok": True, "items": ["Jah Rune", "Angel"]}))
        self.assertEqual(json.loads((box.store / "shopping.json").read_text(encoding="utf-8")), ["Jah Rune", "Angel"])
        self.assertEqual(box.call("GET", "/api/tracker/shopping"), (200, {"items": ["Jah Rune", "Angel"]}))
        self.assertEqual(box.call("POST", "/api/tracker/shopping", {"items": []})[1]["items"], [])

    def test_out_of_bounds_is_refused_and_the_list_kept(self):
        box = Box(self)
        box.call("POST", "/api/tracker/shopping", {"items": ["Kept"]})
        for bad in ({"items": ["   "]}, {"items": ["x" * 121]}, {"items": ["ok"] * 201}, {"items": [3]},
                    {"items": "Jah"}, {}):
            code, body = box.call("POST", "/api/tracker/shopping", bad)
            self.assertEqual(code, 400, bad)
            self.assertIn("err", body)
        self.assertEqual(box.call("GET", "/api/tracker/shopping")[1], {"items": ["Kept"]})
        self.assertEqual(box.call("POST", "/api/tracker/shopping", {"items": ["x" * 120] * 200})[0], 200)


class CodexEndpointTests(unittest.TestCase):
    def test_the_codex_answers_every_row(self):
        box = Box(self)
        code, body = box.call("GET", "/api/tracker/codex")
        self.assertEqual(code, 200)
        self.assertEqual(body["items"], codex.rows())
        self.assertGreater(len(body["items"]), 1000)


class RunsClearTests(unittest.TestCase):
    def test_clear_empties_the_history_whole(self):
        box = Box(self)
        box.state()
        self.assertTrue(box.call("GET", "/api/tracker/runs")[1]["runs"], "the import brought a run")
        self.assertEqual(box.call("POST", "/api/tracker/runs/clear"), (200, {"ok": True, "runs": []}))
        self.assertEqual(json.loads((box.store / "runs.json").read_text(encoding="utf-8")), [])
        self.assertEqual(box.call("GET", "/api/tracker/runs")[1], {"runs": []})


class PanelFilesAndStartTests(unittest.TestCase):
    def test_the_bundled_sounds_are_served_with_an_audio_type(self):
        dist = Path(tempfile.mkdtemp(prefix="forgepact-dist-"))
        self.addCleanup(shutil.rmtree, dist, ignore_errors=True)
        for name, data in (("zone.wav", WAV), ("set.mp3", MP3), ("a.ogg", OGG), ("a.flac", FLAC)):
            (dist / name).write_bytes(data)
        box = Box(self)
        with patch.object(forgepact, "PANEL_DIST", dist):
            for name, kind in (("zone.wav", "audio/wav"), ("set.mp3", "audio/mpeg"),
                               ("a.ogg", "audio/ogg"), ("a.flac", "audio/flac")):
                code, headers, _ = box.raw("GET", "/" + name)
                self.assertEqual((code, headers["Content-Type"]), (200, kind), name)

    def test_the_autoplay_flag_is_appended_and_what_was_there_kept(self):
        env = {}
        self.assertEqual(forgepact.allow_alert_autoplay(env), "--autoplay-policy=no-user-gesture-required")
        env = {"WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS": "--disable-gpu"}
        forgepact.allow_alert_autoplay(env)
        forgepact.allow_alert_autoplay(env)
        self.assertEqual(env["WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS"],
                         "--disable-gpu --autoplay-policy=no-user-gesture-required")

    def test_main_sets_it_before_webview_start(self):
        source = inspect.getsource(forgepact.main)
        self.assertIn("allow_alert_autoplay()", source)
        self.assertLess(source.index("allow_alert_autoplay()"), source.index("webview.start()"))


class StandardLibraryTests(unittest.TestCase):
    def test_the_tracker_package_imports_only_the_standard_library(self):
        import ast
        for path in sorted((ROOT / "src" / "forgepact_tracker").glob("*.py")):
            tree = ast.parse(path.read_text(encoding="utf-8"))
            for node in ast.walk(tree):
                if isinstance(node, ast.Import):
                    names = [a.name for a in node.names]
                elif isinstance(node, ast.ImportFrom) and node.level == 0:
                    names = [node.module]
                else:
                    continue
                for name in names:
                    self.assertTrue(name.split(".")[0] in sys.stdlib_module_names or name == "__future__",
                                    "%s imports %s" % (path.name, name))


def start_sandbox(test, home, *args):
    """tests/panel_sandbox_server.py as the browser suites start it, with the
    user's home (USERPROFILE, HOME, LOCALAPPDATA) pointed at `home`. Returns
    (port, cmds, tracker)."""
    env = dict(os.environ, USERPROFILE=str(home), HOME=str(home),
               LOCALAPPDATA=str(home / "AppData" / "Local"))
    err = tempfile.TemporaryFile()
    test.addCleanup(err.close)
    child = subprocess.Popen([sys.executable, str(SANDBOX_SERVER), "--offline", *map(str, args)],
                             stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=err, env=env)

    def stop():
        try:
            child.stdin.close()
            child.wait(timeout=15)
        except subprocess.TimeoutExpired:
            child.kill()
            child.wait()
        child.stdout.close()
    test.addCleanup(stop)
    found = {}
    while "tracker" not in found:
        line = child.stdout.readline().decode("utf-8").rstrip("\r\n")
        if not line:
            err.seek(0)
            test.fail("the sandbox stopped before it was ready: %s" % err.read().decode("utf-8", "replace"))
        key, _, value = line.partition("=")
        found[key] = value
    return int(found["port"]), Path(found["cmds"]), found["tracker"]


def call(port, method, path, body=None):
    connection = HTTPConnection("127.0.0.1", port, timeout=15)
    try:
        if body is None:
            connection.request(method, path)
        else:
            connection.request(method, path, json.dumps(body), {"Content-Type": "application/json"})
        response = connection.getresponse()
        return response.status, json.loads(response.read())
    finally:
        connection.close()


class SandboxIsolationTests(unittest.TestCase):
    """The hazard this item closes first: the browser suites' sandbox once
    built the tracker's store under the real %LOCALAPPDATA%\\Hero_Siege and
    imported the developer's real HS Offline Tracker data into it."""

    def setUp(self):
        self.home = Path(tempfile.mkdtemp(prefix="forgepact-home-"))
        self.addCleanup(shutil.rmtree, self.home, ignore_errors=True)
        self.local = self.home / "AppData" / "Local"
        self.local.mkdir(parents=True)
        self.users_tracker = self.local / "HS Offline Tracker"
        fake_tracker_folder(self.users_tracker)
        self.before = folder_state(self.users_tracker)

    def assert_home_untouched(self):
        self.assertFalse((self.local / "Hero_Siege").exists(),
                         "the sandbox wrote under the home's AppData\\Local\\Hero_Siege")
        self.assertEqual(folder_state(self.users_tracker), self.before, "the home's Tracker folder changed")
        self.assertEqual(sorted(p.name for p in self.local.iterdir()), ["HS Offline Tracker"])

    def test_a_session_is_served_live_and_nothing_reaches_the_home(self):
        port, cmds, tracker = start_sandbox(self, self.home, "--tracker-session", FIXTURE)
        sandbox_root = cmds.parent
        self.assertEqual(Path(tracker), sandbox_root / "bp_ipc" / "tracker" / SESSION)
        self.assertEqual(Path(tracker).read_bytes(), FIXTURE.read_bytes())
        code, state = call(port, "GET", "/api/tracker")
        self.assertEqual(code, 200, state)
        self.assertEqual(state["session"], {"file": SESSION, "pid": 777, "live": True, "bad_lines": 0})
        self.assertEqual(state["snapshot"]["gold"]["earned"], 200)
        # writes of every kind the tab makes
        code, settings = call(port, "GET", "/api/tracker/settings")
        self.assertEqual(call(port, "POST", "/api/tracker/settings", settings["settings"])[0], 200)
        self.assertEqual(call(port, "POST", "/api/tracker/sound/heroic", {"name": "a.wav", "data": b64(WAV)})[0], 200)
        self.assertEqual(call(port, "POST", "/api/tracker/shopping", {"items": ["Jah"]})[0], 200)
        self.assertEqual(call(port, "POST", "/api/tracker/overlay", {"on": True})[0], 409)
        imported = json.loads((sandbox_root / "Hero_Siege" / "tracker" / "import.json").read_text(encoding="utf-8"))
        self.assertIs(imported["source_found"], False, "the import read a Tracker folder: %s" % imported["source"])
        self.assertEqual(imported["copied"], [])
        self.assert_home_untouched()

    def test_without_a_session_nothing_is_live_and_the_tracker_line_is_empty(self):
        port, cmds, tracker = start_sandbox(self, self.home)
        self.assertEqual(tracker, "")
        code, state = call(port, "GET", "/api/tracker")
        self.assertEqual(state["session"]["file"], None)
        self.assertIsNone(state["zone_alert"])
        self.assert_home_untouched()

    def test_a_tracker_source_is_imported_from_and_left_as_it_was(self):
        source = self.home / "fixture-tracker"
        fake_tracker_folder(source)
        source_before = folder_state(source)
        port, cmds, _ = start_sandbox(self, self.home, "--tracker-source", source, "--tracker-session", FIXTURE)
        self.assertEqual(call(port, "GET", "/api/tracker")[0], 200)
        imported = json.loads((cmds.parent / "Hero_Siege" / "tracker" / "import.json").read_text(encoding="utf-8"))
        self.assertIn("settings.json", imported["copied"])
        self.assertEqual(Path(imported["source"]), source.resolve())
        self.assertEqual(folder_state(source), source_before)
        self.assert_home_untouched()


if __name__ == "__main__":
    unittest.main()
