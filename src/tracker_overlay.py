"""The Tracker tab's overlay: a small always-on-top window over the game.

The overlay is a second pywebview window on the panel's own `/overlay.html`
(HS Offline Tracker's compact window, Q8 (b) of ForgePact #185). It shows the
counters, the ticker and the flourish, reads `/api/tracker` like the main
page, and never plays a sound: alerts are the main window's.

This module owns the window's life and nothing else:

- `available` is false without a pywebview main window (the browser
  fallback, the test sandbox), and then nothing is ever opened.
- It opens frameless, on top, and with `focus=False`, so it never takes the
  keyboard from the game. It is solid unless `OVERLAY_TRANSPARENT` says
  otherwise. Its size and place are the settings' `overlay_w`, `overlay_h`,
  `overlay_x` and `overlay_y` (444 x 147 and pywebview's own placement while
  unset), and where the player last moved or sized it is saved when it closes.
- `auto_show` follows the watcher's tracker step: when the game starts and the
  player wants it (`auto_show` in the settings, the tracker on) the overlay
  opens, and when the game stops it closes, but only a window this opened
  itself: one the player opened by hand stays.

An exclusive-fullscreen game draws over every window, this one included; the
overlay needs the game windowed or borderless.

pywebview is imported only when a window is first opened, and any module with
pywebview's `create_window` can stand in for it (the tests pass a fake). No
thread is started here: pywebview runs window events on its own.
"""

from __future__ import annotations

import threading

# Whether the window is created transparent. Off until the `overlay` item's
# probe measures that a transparent pywebview window really draws through on
# WebView2 here; until then the overlay is the solid compact window (Q8).
OVERLAY_TRANSPARENT = False

OVERLAY_PATH = "/overlay.html"
OVERLAY_TITLE = "ForgePact Tracker"
# HS Offline Tracker's own compact window (`about()` in its lib.rs).
DEFAULT_SIZE = (444, 147)
MIN_SIZE = (200, 60)
MAX_SIZE = (3840, 2160)
# the panel's base colour (tokens.css --color-bg-base), so a solid window
# never flashes white before the page paints
BACKGROUND = "#15110D"

PLACE_FIELDS = ("overlay_x", "overlay_y", "overlay_w", "overlay_h")


def _whole(value) -> int | None:
    return value if isinstance(value, int) and not isinstance(value, bool) else None


def window_geometry(settings: dict) -> dict:
    """`width`, `height`, `x` and `y` for `create_window`, from the settings.
    A size out of bounds or not a whole number reads as the default; a place
    that is not a whole number is left to pywebview."""
    settings = settings if isinstance(settings, dict) else {}
    width, height = _whole(settings.get("overlay_w")), _whole(settings.get("overlay_h"))
    if width is None or not MIN_SIZE[0] <= width <= MAX_SIZE[0]:
        width = DEFAULT_SIZE[0]
    if height is None or not MIN_SIZE[1] <= height <= MAX_SIZE[1]:
        height = DEFAULT_SIZE[1]
    return {"width": width, "height": height,
            "x": _whole(settings.get("overlay_x")), "y": _whole(settings.get("overlay_y"))}


class TrackerOverlay:
    """The overlay window's controller. One per panel process.

    `main_window()` answers the panel's pywebview window or None;
    `base_url()` the panel's `http://127.0.0.1:<port>`; `settings()` the
    tracker's settings document; `save_place(fields)` stores `overlay_*`
    fields. `webview` is the pywebview module (imported on first use when not
    given).
    """

    def __init__(self, main_window, base_url, settings, save_place, webview=None):
        self._main_window = main_window
        self._base_url = base_url
        self._settings = settings
        self._save_place = save_place
        self._webview = webview
        self._lock = threading.RLock()
        self.window = None
        self.opened_by_auto = False
        self._game_running = False
        self._place: dict = {}
        self._saved_place: dict = {}

    # -- what the panel reads ------------------------------------------------

    def available(self) -> bool:
        """Whether an overlay can be opened: the panel runs in its own
        pywebview window."""
        try:
            return self._main_window() is not None
        except Exception:
            return False

    def is_open(self) -> bool:
        with self._lock:
            return self.window is not None

    def state(self) -> dict:
        """`/api/tracker`'s `overlay`: `{"available", "open"}`."""
        return {"available": self.available(), "open": self.is_open()}

    # -- opening and closing -------------------------------------------------

    def set_open(self, on: bool) -> dict:
        """`POST /api/tracker/overlay`: open or close it by hand. A window
        opened here is the player's, so the game stopping never closes it."""
        if not self.available():
            return {"ok": False, "error": "the overlay needs ForgePact's own window, not a browser"}
        try:
            if on:
                self.open(auto=False)
            else:
                self.close()
        except Exception as exc:
            return {"ok": False, "error": "the overlay window failed: %s" % exc}
        return {"ok": True, "open": self.is_open()}

    def open(self, auto: bool = False) -> bool:
        """Open the window, unless one is open or none can be. Returns whether
        one is open afterwards. A hand open of an auto-opened window makes it
        the player's."""
        with self._lock:
            if self.window is not None:
                if not auto:
                    self.opened_by_auto = False
                return True
            if not self.available():
                return False
            webview = self._webview
            if webview is None:
                import webview as _webview  # lazy: only a real window needs it
                webview = self._webview = _webview
            geometry = window_geometry(self._settings())
            self._place = {"overlay_x": geometry["x"], "overlay_y": geometry["y"],
                           "overlay_w": geometry["width"], "overlay_h": geometry["height"]}
            self._saved_place = dict(self._place)
            window = webview.create_window(
                OVERLAY_TITLE, self._base_url() + OVERLAY_PATH,
                width=geometry["width"], height=geometry["height"], x=geometry["x"], y=geometry["y"],
                min_size=MIN_SIZE, frameless=True, easy_drag=True, on_top=True, focus=False,
                transparent=OVERLAY_TRANSPARENT, background_color=BACKGROUND, shadow=False)
            if window is None:
                return False
            self.window = window
            self.opened_by_auto = auto
            events = getattr(window, "events", None)
            if events is not None:
                events.moved += self._moved
                events.resized += self._resized
                events.closed += self._closed
            return True

    def close(self, only_auto: bool = False) -> bool:
        """Close the window. With `only_auto`, only one this opened for the
        game. Returns whether a window was closed."""
        with self._lock:
            window = self.window
            if window is None or (only_auto and not self.opened_by_auto):
                return False
            self.window = None
            self.opened_by_auto = False
        self._store_place()
        window.destroy()
        return True

    def auto_show(self, running: bool, wanted) -> None:
        """The watcher's step. On the game starting, open the window when
        `wanted()` says the player wants it; on the game stopping, close one
        this opened. `wanted` is asked only on a start."""
        with self._lock:
            started = running and not self._game_running
            stopped = self._game_running and not running
            self._game_running = running
        if started:
            try:
                if wanted():
                    self.open(auto=True)
            except Exception:
                pass
        elif stopped:
            self.close(only_auto=True)

    # -- pywebview's window events -------------------------------------------

    def _moved(self, x, y) -> None:
        with self._lock:
            self._place.update(overlay_x=int(x), overlay_y=int(y))

    def _resized(self, width, height) -> None:
        with self._lock:
            self._place.update(overlay_w=int(width), overlay_h=int(height))

    def _closed(self) -> None:
        """The window went (closed by the player or the system): forget it."""
        with self._lock:
            self.window = None
            self.opened_by_auto = False
        self._store_place()

    def _store_place(self) -> None:
        with self._lock:
            place = {k: v for k, v in self._place.items() if v is not None}
            if not place or place == {k: v for k, v in self._saved_place.items() if v is not None}:
                return
            self._saved_place = dict(self._place)
        try:
            self._save_place(place)
        except Exception:
            pass
