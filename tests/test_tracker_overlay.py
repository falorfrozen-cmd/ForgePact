#!/usr/bin/env python3
"""The Tracker tab's overlay window (`src/tracker_overlay.py`, #185).

A fake `webview` module stands in for pywebview throughout: no test opens a
real window. The fake records what `create_window` was asked and fires the
window events pywebview fires (`moved`, `resized`, `closed`), so the tests
pin:

- no window without the panel's own pywebview window (`available` false: the
  browser fallback and the sandbox), and `POST /api/tracker/overlay` 409 then;
- the window's form: `/overlay.html`, frameless, on top, `focus=False`, solid
  (`OVERLAY_TRANSPARENT`, off as the overlay item's probe measured), 444 x 147
  unless the settings say otherwise, dragged only from the page's drag region
  (so a locked overlay stays put);
- one window at most, closed on request, its place saved when the player lets
  go of a drag and when it closes;
- the page's bridge (`js_api`): `place_done` and `fit`, and nothing else;
- `auto_show`: opened on a game start when wanted, closed on the stop only
  when it opened the window itself;
- the watcher's tracker step reads no settings (and so builds no store) for
  a player who never turned the tracker on.
"""

import ast
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

TESTS = Path(__file__).resolve().parent
ROOT = TESTS.parent
sys.path.insert(0, str(ROOT / "src"))
sys.path.insert(0, str(TESTS))

import forgepact  # noqa: E402
import tracker_overlay  # noqa: E402
from tracker_overlay import TrackerOverlay  # noqa: E402


class FakeEvent:
    def __init__(self):
        self.handlers = []

    def __iadd__(self, handler):
        self.handlers.append(handler)
        return self

    def set(self, *args):
        for handler in list(self.handlers):
            handler(*args)


class FakeEvents:
    def __init__(self):
        self.moved = FakeEvent()
        self.resized = FakeEvent()
        self.closed = FakeEvent()


class FakeWindow:
    def __init__(self, kwargs):
        self.kwargs = kwargs
        self.events = FakeEvents()
        self.destroyed = False
        self.sizes = []

    def resize(self, width, height):
        # pywebview's own resize fires `resized` with the new size
        self.sizes.append((width, height))
        self.events.resized.set(width, height)

    def destroy(self):
        self.destroyed = True
        self.events.closed.set()


class FakeWebview:
    """pywebview's `create_window`, recording each window it was asked for."""

    def __init__(self):
        self.windows = []

    def create_window(self, title, url, **kwargs):
        window = FakeWindow(dict(kwargs, title=title, url=url))
        self.windows.append(window)
        return window


class Harness:
    def __init__(self, main_window=object(), settings=None):
        self.webview = FakeWebview()
        self.main = main_window
        self.settings_doc = dict(settings or {})
        self.saved = []
        self.overlay = TrackerOverlay(main_window=lambda: self.main, base_url=lambda: "http://127.0.0.1:8780",
                                      settings=lambda: self.settings_doc, save_place=self.saved.append,
                                      webview=self.webview)


class OverlayBaselineTests(unittest.TestCase):
    """No pywebview main window: nothing opens, and it says so."""

    def test_without_a_main_window_it_is_unavailable_and_opens_nothing(self):
        h = Harness(main_window=None)
        self.assertEqual(h.overlay.state(), {"available": False, "open": False, "transparent": False})
        self.assertFalse(h.overlay.open())
        refused = h.overlay.set_open(True)
        self.assertIs(refused["ok"], False)
        self.assertIn("window", refused["error"])
        h.overlay.auto_show(True, lambda: True)
        self.assertEqual(h.webview.windows, [], "no window without the panel's own")

    def test_it_is_solid_as_the_probe_measured(self):
        # The overlay item's probe (2026-10-14): a transparent pywebview 6.2.1
        # window on WebView2 here painted the form's grey, not what was behind
        # it. The measurement is recorded beside the constant.
        self.assertIs(tracker_overlay.OVERLAY_TRANSPARENT, False)
        self.assertEqual(tracker_overlay.DEFAULT_SIZE, (444, 147))
        lines = (ROOT / "src" / "tracker_overlay.py").read_text(encoding="utf-8").split("\n")
        (at,) = [i for i, line in enumerate(lines) if line.startswith("OVERLAY_TRANSPARENT")]
        near = " ".join(lines[max(0, at - 8):at + 1]).lower()
        self.assertIn("measured", near)
        self.assertIn("2026-10-14", near)

    def test_pywebview_is_imported_only_when_a_window_opens(self):
        tree = ast.parse((ROOT / "src" / "tracker_overlay.py").read_text(encoding="utf-8"))
        top = [n for n in tree.body if isinstance(n, (ast.Import, ast.ImportFrom))]
        names = [a.name for n in top if isinstance(n, ast.Import) for a in n.names] \
            + [n.module for n in top if isinstance(n, ast.ImportFrom)]
        self.assertNotIn("webview", names)
        self.assertTrue(all(name.split(".")[0] in sys.stdlib_module_names or name == "__future__"
                            for name in names if name), names)


class OverlayWindowTests(unittest.TestCase):
    def test_it_opens_the_overlay_page_frameless_on_top_without_focus(self):
        h = Harness()
        self.assertEqual(h.overlay.state(), {"available": True, "open": False, "transparent": False})
        self.assertEqual(h.overlay.set_open(True), {"ok": True, "open": True})
        (window,) = h.webview.windows
        k = window.kwargs
        self.assertEqual(k["url"], "http://127.0.0.1:8780/overlay.html")
        self.assertIs(k["frameless"], True)
        self.assertIs(k["on_top"], True)
        self.assertIs(k["focus"], False, "it must never take the keyboard from the game")
        self.assertIs(k["transparent"], tracker_overlay.OVERLAY_TRANSPARENT)
        # dragged only from the page's drag region, which a locked overlay drops
        self.assertIs(k["easy_drag"], False)
        self.assertIsInstance(k["js_api"], tracker_overlay.OverlayBridge)
        self.assertEqual((k["width"], k["height"], k["x"], k["y"]), (444, 147, None, None))
        self.assertEqual(h.overlay.state(), {"available": True, "open": True, "transparent": False})

    def test_the_saved_size_and_place_are_used_and_bad_ones_are_not(self):
        h = Harness(settings={"overlay_w": 520, "overlay_h": 180, "overlay_x": -1900, "overlay_y": 40})
        h.overlay.open()
        k = h.webview.windows[0].kwargs
        self.assertEqual((k["width"], k["height"], k["x"], k["y"]), (520, 180, -1900, 40))
        for bad in ({"overlay_w": True, "overlay_h": "180"}, {"overlay_w": 5, "overlay_h": 99999},
                    {"overlay_x": 1.5, "overlay_y": None}):
            geometry = tracker_overlay.window_geometry(bad)
            self.assertEqual((geometry["width"], geometry["height"]) if "overlay_w" in bad else (444, 147),
                             (444, 147), bad)
            if "overlay_x" in bad:
                self.assertEqual((geometry["x"], geometry["y"]), (None, None), bad)

    def test_one_window_at_most_and_close_destroys_it(self):
        h = Harness()
        h.overlay.open()
        h.overlay.open()
        self.assertEqual(len(h.webview.windows), 1)
        self.assertEqual(h.overlay.set_open(False), {"ok": True, "open": False})
        self.assertTrue(h.webview.windows[0].destroyed)
        self.assertFalse(h.overlay.close(), "nothing left to close")
        h.overlay.open()
        self.assertEqual(len(h.webview.windows), 2, "a closed overlay opens again")

    def test_where_the_player_moved_it_is_saved_when_it_closes_and_only_then(self):
        h = Harness(settings={"overlay_x": 10, "overlay_y": 20})
        h.overlay.open()
        window = h.webview.windows[0]
        window.events.moved.set(300, 400)
        window.events.resized.set(500, 160)
        self.assertEqual(h.saved, [], "a drag is not written move by move")
        h.overlay.close()
        self.assertEqual(h.saved, [{"overlay_x": 300, "overlay_y": 400, "overlay_w": 500, "overlay_h": 160}])

    def test_an_unmoved_overlay_saves_nothing(self):
        h = Harness(settings={"overlay_x": 10, "overlay_y": 20})
        h.overlay.open()
        h.overlay.close()
        self.assertEqual(h.saved, [])

    def test_a_window_closed_by_the_player_is_forgotten(self):
        h = Harness()
        h.overlay.open()
        h.webview.windows[0].events.moved.set(5, 6)
        h.webview.windows[0].events.closed.set()
        self.assertEqual(h.overlay.state(), {"available": True, "open": False, "transparent": False})
        self.assertEqual(h.saved, [{"overlay_x": 5, "overlay_y": 6, "overlay_w": 444, "overlay_h": 147}])


class OverlayBridgeTests(unittest.TestCase):
    """The page's two calls into the window (`window.pywebview.api`): the end
    of a drag, and the size its plate needs."""

    def bridge(self, h):
        return h.webview.windows[0].kwargs["js_api"]

    def test_the_bridge_exposes_only_its_two_calls(self):
        h = Harness()
        h.overlay.open()
        # pywebview exposes every public attribute it can reach, recursively
        self.assertEqual([n for n in dir(self.bridge(h)) if not n.startswith("_")], ["fit", "place_done"])

    def test_letting_go_of_a_drag_saves_the_place_once(self):
        h = Harness(settings={"overlay_x": 10, "overlay_y": 20})
        h.overlay.open()
        window = h.webview.windows[0]
        for x in range(30, 300, 30):
            window.events.moved.set(x, 400)
        self.assertEqual(h.saved, [], "a drag is not written move by move")
        self.bridge(h).place_done()
        self.assertEqual(h.saved, [{"overlay_x": 270, "overlay_y": 400, "overlay_w": 444, "overlay_h": 147}])
        self.bridge(h).place_done()
        h.overlay.close()
        self.assertEqual(len(h.saved), 1, "an unchanged place is not written again")

    def test_a_click_without_a_move_saves_nothing(self):
        h = Harness(settings={"overlay_x": 10, "overlay_y": 20})
        h.overlay.open()
        self.bridge(h).place_done()
        self.assertEqual(h.saved, [])

    def test_fit_sizes_the_window_to_the_plate_within_bounds(self):
        h = Harness()
        h.overlay.open()
        window = h.webview.windows[0]
        bridge = self.bridge(h)
        self.assertIs(bridge.fit(444, 210.4), True)
        self.assertEqual(window.sizes, [(444, 211)], "a fraction of a pixel rounds up, so nothing is cut")
        bridge.fit(444, 211)
        self.assertEqual(window.sizes, [(444, 211)], "the size it has is not asked again")
        bridge.fit(10, 99999)
        self.assertEqual(window.sizes[-1], (tracker_overlay.MIN_SIZE[0], tracker_overlay.MAX_SIZE[1]))
        for bad in ((True, 100), ("444", 147), (None, 147), (float("nan"), 147)):
            self.assertIs(bridge.fit(*bad), False, bad)
        self.assertEqual(len(window.sizes), 2)

    def test_the_bridge_does_nothing_once_the_window_is_gone(self):
        h = Harness()
        h.overlay.open()
        bridge = self.bridge(h)
        window = h.webview.windows[0]
        h.overlay.close()
        self.assertIs(bridge.fit(500, 200), False)
        bridge.place_done()
        self.assertEqual(window.sizes, [])
        self.assertEqual(h.saved, [])


class AutoShowTests(unittest.TestCase):
    def test_a_game_start_opens_it_and_the_stop_closes_it(self):
        h = Harness()
        h.overlay.auto_show(True, lambda: True)
        self.assertTrue(h.overlay.is_open())
        self.assertTrue(h.overlay.opened_by_auto)
        h.overlay.auto_show(True, lambda: self.fail("asked again while the game still runs"))
        h.overlay.auto_show(False, lambda: self.fail("asked on a stop"))
        self.assertFalse(h.overlay.is_open())
        self.assertTrue(h.webview.windows[0].destroyed)

    def test_not_wanted_opens_nothing(self):
        h = Harness()
        h.overlay.auto_show(True, lambda: False)
        self.assertEqual(h.webview.windows, [])

    def test_the_stop_never_closes_a_window_the_player_opened(self):
        h = Harness()
        h.overlay.set_open(True)
        h.overlay.auto_show(True, lambda: True)
        self.assertEqual(len(h.webview.windows), 1)
        h.overlay.auto_show(False, lambda: True)
        self.assertTrue(h.overlay.is_open())

    def test_a_hand_open_makes_an_auto_opened_window_the_players(self):
        h = Harness()
        h.overlay.auto_show(True, lambda: True)
        h.overlay.set_open(True)
        h.overlay.auto_show(False, lambda: True)
        self.assertTrue(h.overlay.is_open())

    def test_a_wanted_check_that_raises_opens_nothing_and_raises_nothing(self):
        h = Harness()

        def boom():
            raise OSError("settings unreadable")
        h.overlay.auto_show(True, boom)
        self.assertEqual(h.webview.windows, [])


class PanelOverlayTests(unittest.TestCase):
    """src/forgepact.py's side: the controller it builds, the endpoint and the
    watcher's step."""

    def setUp(self):
        from test_tracker_panel_contract import TrackerSandbox
        self.box = TrackerSandbox(self)
        saved = forgepact._TRACKER["overlay"]
        forgepact._TRACKER["overlay"] = None
        self.addCleanup(forgepact._TRACKER.__setitem__, "overlay", saved)

    def test_the_sandbox_has_no_main_window_so_the_endpoint_answers_409(self):
        code, state = self.box.call("GET", "/api/tracker")
        self.assertEqual(code, 200)
        self.assertEqual(state["overlay"], {"available": False, "open": False, "transparent": False})
        code, body = self.box.call("POST", "/api/tracker/overlay", {"on": True})
        self.assertEqual(code, 409, body)
        self.assertIn("err", body)
        code, body = self.box.call("POST", "/api/tracker/overlay", {"on": "yes"})
        self.assertEqual(code, 400, body)

    def test_beside_a_main_window_the_endpoint_opens_and_closes_it(self):
        fake = FakeWebview()
        with patch.object(forgepact, "WEBVIEW_WINDOW", object()), patch.dict(sys.modules, {"webview": fake}):
            code, body = self.box.call("POST", "/api/tracker/overlay", {"on": True})
            self.assertEqual((code, body), (200, {"ok": True, "open": True}))
            self.assertTrue(fake.windows[0].kwargs["url"].endswith("/overlay.html"))
            code, state = self.box.call("GET", "/api/tracker")
            self.assertEqual(state["overlay"], {"available": True, "open": True, "transparent": False})
            fake.windows[0].events.moved.set(12, 34)
            code, body = self.box.call("POST", "/api/tracker/overlay", {"on": False})
            self.assertEqual((code, body), (200, {"ok": True, "open": False}))
        code, settings = self.box.call("GET", "/api/tracker/settings")
        self.assertEqual((settings["settings"]["overlay_x"], settings["settings"]["overlay_y"]), (12, 34),
                         "the place is saved in the tracker's settings")

    def test_the_watcher_step_reads_no_settings_while_the_tracker_is_off(self):
        asked = []
        fake = FakeWebview()
        cfg = json.loads(self.box.live.sandbox.config.read_text(encoding="utf-8"))
        shutil.rmtree(self.box.sessions, ignore_errors=True)
        with patch.object(forgepact, "WEBVIEW_WINDOW", object()), patch.dict(sys.modules, {"webview": fake}), \
                patch.object(forgepact, "tracker_service", side_effect=lambda: asked.append(1)):
            forgepact.tracker_watch(dict(cfg, mod_tracker=False), True)
        self.assertEqual(asked, [], "a player who never turned the tracker on gets no store")
        self.assertEqual(fake.windows, [])

    def test_the_watcher_step_opens_it_on_a_game_start_with_the_tracker_on(self):
        fake = FakeWebview()
        cfg = json.loads(self.box.live.sandbox.config.read_text(encoding="utf-8"))
        with patch.object(forgepact, "WEBVIEW_WINDOW", object()), patch.dict(sys.modules, {"webview": fake}):
            forgepact.tracker_watch(dict(cfg, mod_tracker=True), True)
            self.assertEqual(len(fake.windows), 1, "auto_show is on by default, as in the Tracker")
            forgepact.tracker_watch(dict(cfg, mod_tracker=True), False)
            self.assertTrue(fake.windows[0].destroyed)


if __name__ == "__main__":
    unittest.main()
