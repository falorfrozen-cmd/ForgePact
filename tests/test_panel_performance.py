#!/usr/bin/env python3
"""Performance contract for the panel's two per-poll costs and its poll rate.

Three separate problems, all of which only show up over a long session:

1. `plugin_boot_count` read the whole of an append-only `out.txt` every five
   seconds, so the cost of one poll grew with the length of the session. The
   incremental scan must produce EXACTLY the number a full-file count would -
   the value is how `watcher()` notices a new game process - so most of the
   cases here are correctness cases pinning the two against each other,
   including the awkward ones (rotation, replacement, a marker straddling a
   chunk boundary).
2. `running_paths` spawned `tasklist.exe` on every poll, and
   `wait_for_plugin_ready` calls it every 0.25 s for up to 60 s during game
   startup - up to ~240 short-lived processes at the most latency-sensitive
   moment there is.
3. Both UIs polled on a fixed timer with no hidden-window gate. The shared
   adaptive policy is a pure function so it can be asserted structurally
   always, and executed through `node` when one is installed.
"""

import inspect
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import unittest
import unittest.mock
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
FORGEPACT_DIR = REPO_ROOT / "ForgePact"
SRC_DIR = FORGEPACT_DIR / "src"
LAUNCHER_SRC = REPO_ROOT / "HS-Offline-Launcher" / "src" / "hs_offline_launcher.py"

if str(SRC_DIR) not in sys.path:
    sys.path.insert(0, str(SRC_DIR))

import forgepact

MARKER = "BloodPact plugin loaded"

# The pairs both apps must agree on, asserted by parsing each source rather
# than by restating the numbers here in prose.
POLL_CONSTANTS = ("POLL_FAST_MS", "POLL_IDLE_MS", "POLL_FAST_WINDOW_MS")


def naive_boot_count(path: Path) -> int:
    """The pre-change implementation, kept as the oracle."""
    return path.read_text(encoding="utf-8", errors="ignore").count(MARKER)


def js_constants(source: str) -> dict:
    """`const NAME = <int>;` pairs declared anywhere in a Python source file."""
    return {m.group(1): int(m.group(2))
            for m in re.finditer(r"const\s+(POLL_[A-Z_]+)\s*=\s*(\d+)\s*;", source)}


def run_node(policy_js: str, driver_js: str) -> dict:
    """Execute the policy through a real JS runtime, or skip cleanly."""
    node = shutil.which("node")
    if not node:
        raise unittest.SkipTest(
            "node is not on PATH; the poll policy's structure is still asserted, "
            "but its truth table needs a JavaScript runtime to execute")
    with tempfile.TemporaryDirectory() as tmp:
        script = Path(tmp) / "policy.js"
        script.write_text(policy_js + "\n" + driver_js, encoding="utf-8")
        result = subprocess.run([node, str(script)], capture_output=True, text=True)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        return json.loads(result.stdout.strip().splitlines()[-1])


DELAY_DRIVER = """
console.log(JSON.stringify({
  fast: POLL_FAST_MS, idle: POLL_IDLE_MS, window: POLL_FAST_WINDOW_MS,
  fields: POLL_WATCHED_FIELDS,
  hiddenNow: pollDelayMs(true, 0),
  hiddenLater: pollDelayMs(true, 999999),
  freshChange: pollDelayMs(false, 0),
  justInside: pollDelayMs(false, 14999),
  atBoundary: pollDelayMs(false, 15000),
  longIdle: pollDelayMs(false, 999999)
}));
"""


class BootCountTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.ipc = self.root / "bin" / "bp_ipc"
        self.ipc.mkdir(parents=True)
        self.cfg = {"game_exe": str(self.root / "bin" / "Hero_Siege.exe")}
        self.log = self.ipc / "out.txt"
        forgepact.reset_boot_count_cache()
        self.addCleanup(forgepact.reset_boot_count_cache)
        self.addCleanup(self.tmp.cleanup)

    def write(self, text: str):
        self.log.write_bytes(text.encode("utf-8"))

    def append(self, text: str):
        with self.log.open("ab") as fh:
            fh.write(text.encode("utf-8"))

    def assertMatchesNaive(self, label: str):
        self.assertEqual(forgepact.plugin_boot_count(self.cfg),
                         naive_boot_count(self.log), label)

    # --- replacement cases (PR #22 review, 2026-09-16) ---------------------
    # The cache used to invalidate only on "different path" or "smaller file",
    # so a REPLACEMENT of the same or greater size kept the stale offset and
    # count. That loses the count CHANGE watcher() uses to spot a restart
    # between polls, and auto-apply then silently stops working.

    def test_same_size_replacement_is_recounted(self):
        # The reviewer's exact reproduction: count 2, atomically replace with a
        # same-size log holding ONE marker, and the answer must follow the file.
        self.write(MARKER + "\npad pad pad\n" + MARKER + "\n")
        self.assertEqual(forgepact.plugin_boot_count(self.cfg), 2)
        original = self.log.stat().st_size

        replacement = MARKER + "\n"
        replacement += "x" * (original - len(replacement.encode("utf-8")))
        other = self.ipc / "replacement.txt"
        other.write_bytes(replacement.encode("utf-8"))
        self.assertEqual(other.stat().st_size, original, "fixture must be same-size")
        os.replace(other, self.log)

        self.assertEqual(naive_boot_count(self.log), 1, "fixture sanity")
        self.assertMatchesNaive("same-size replacement")

    def test_larger_replacement_is_recounted(self):
        self.write(MARKER + "\n" + MARKER + "\n" + MARKER + "\n")
        self.assertEqual(forgepact.plugin_boot_count(self.cfg), 3)
        other = self.ipc / "bigger.txt"
        other.write_bytes((MARKER + "\n" + "y" * 4096).encode("utf-8"))
        self.assertGreater(other.stat().st_size, self.log.stat().st_size)
        os.replace(other, self.log)
        self.assertMatchesNaive("larger replacement")

    def test_truncate_then_regrow_in_place_is_recounted(self):
        # Same path, same file identity, and back to the same size - so neither
        # the path check nor st_ino nor the size can see it. Only the anchor
        # bytes ending at the stored offset differ, which is why they are kept.
        self.write(MARKER + "\nAAAA\n" + MARKER + "\nBBBB\n")
        before = self.log.stat().st_size
        self.assertEqual(forgepact.plugin_boot_count(self.cfg), 2)
        rewritten = MARKER + "\nCCCC\n"
        rewritten += "z" * (before - len(rewritten.encode("utf-8")))
        with self.log.open("r+b") as fh:        # in place: no rename, same inode
            fh.truncate(0)
            fh.seek(0)
            fh.write(rewritten.encode("utf-8"))
        self.assertEqual(self.log.stat().st_size, before, "fixture must be same-size")
        self.assertEqual(naive_boot_count(self.log), 1, "fixture sanity")
        self.assertMatchesNaive("truncate then regrow in place")

    def test_rewrite_that_preserves_the_trailing_bytes_is_recounted(self):
        # REPORTED 2026-09-16 (second review pass), and the sharper version of
        # the case above. A rewrite in place can keep the anchor bytes intact
        # while removing a marker EARLIER in the file, so identity, size and the
        # trailing bytes all agree and only the content between them differs.
        #
        # My own truncate-then-regrow test missed this because its fixture was
        # short enough to sit entirely inside the 64-byte anchor - the assertion
        # could not fail in the direction it was testing. The tail here is
        # deliberately much longer than the anchor.
        marker = MARKER + "\n"
        tail = "Settings applied successfully.\n" * 10        # ~310 bytes > anchor
        old = marker * 2 + tail
        new = marker + " " * len(marker) + tail                # same size, one marker
        self.assertEqual(len(old.encode("utf-8")), len(new.encode("utf-8")))
        self.assertGreater(len(tail.encode("utf-8")), 64, "tail must exceed the anchor")

        self.write(old)
        self.assertEqual(forgepact.plugin_boot_count(self.cfg), 2)
        with self.log.open("r+b") as fh:
            fh.truncate(0)
            fh.seek(0)
            fh.write(new.encode("utf-8"))
        self.assertEqual(naive_boot_count(self.log), 1, "fixture sanity")
        self.assertMatchesNaive("rewrite preserving the trailing bytes")

    def test_a_plain_append_still_uses_the_cache(self):
        # The negative control for the three above: hardening invalidation must
        # not turn every poll into a full rescan, or the fix quietly undoes the
        # optimisation it exists to protect.
        self.write(MARKER + "\n" + "q" * 200000 + "\n")
        forgepact.plugin_boot_count(self.cfg)
        forgepact.BOOT_SCAN_BYTES = 0
        self.append(MARKER + "\n")
        self.assertMatchesNaive("append after warm call")
        self.assertLess(forgepact.BOOT_SCAN_BYTES, 4096,
                        "an append must not trigger a full rescan")

    def test_empty_log(self):
        self.write("")
        self.assertMatchesNaive("empty log")

    def test_log_without_the_marker(self):
        self.write("some other line\nand another\n")
        self.assertMatchesNaive("no marker")

    def test_three_markers(self):
        self.write(f"a\n==== {MARKER} ====\nb\n==== {MARKER} ====\nc\n==== {MARKER} ====\n")
        self.assertMatchesNaive("three markers")
        self.assertEqual(forgepact.plugin_boot_count(self.cfg), 3)

    def test_marker_straddling_a_chunk_boundary(self):
        # The incremental reader carries len(marker)-1 bytes between chunks;
        # without that a boot vanishes whenever the log happens to be the
        # wrong length, which nothing else in the panel would ever notice.
        chunk = forgepact.BOOT_SCAN_CHUNK
        pad = "." * (chunk - len(MARKER) // 2)
        self.write(f"{pad}==== {MARKER} ====\n")
        self.assertMatchesNaive("straddling the chunk boundary")
        self.assertEqual(forgepact.plugin_boot_count(self.cfg), 1)

    def test_append_is_counted(self):
        self.write(f"==== {MARKER} ====\n")
        self.assertEqual(forgepact.plugin_boot_count(self.cfg), 1)
        self.append("noise\n")
        self.assertMatchesNaive("after noise")
        self.append(f"==== {MARKER} ====\n")
        self.assertMatchesNaive("after a second boot")
        self.assertEqual(forgepact.plugin_boot_count(self.cfg), 2)

    def test_a_marker_appended_across_two_polls_is_counted_once(self):
        # The plugin writes its banner with one Out() call, but a poll can
        # still land mid-write. A marker split across two reads must be seen
        # exactly once - not zero times, and not twice.
        self.write("noise\n==== BloodPact plugin")
        self.assertEqual(forgepact.plugin_boot_count(self.cfg), 0)
        self.append(" loaded ====\n")
        self.assertMatchesNaive("marker completed by the next poll")
        self.assertEqual(forgepact.plugin_boot_count(self.cfg), 1)

    def test_truncation_restarts_the_count(self):
        self.write(f"==== {MARKER} ====\n" * 3)
        self.assertEqual(forgepact.plugin_boot_count(self.cfg), 3)
        self.write(f"==== {MARKER} ====\n")          # truncated to zero, one boot
        self.assertEqual(forgepact.plugin_boot_count(self.cfg), 1)
        self.assertMatchesNaive("after truncation")

    def test_a_different_log_is_counted_from_scratch(self):
        self.write(f"==== {MARKER} ====\n" * 4)
        self.assertEqual(forgepact.plugin_boot_count(self.cfg), 4)
        other_root = self.root / "other"
        (other_root / "bin" / "bp_ipc").mkdir(parents=True)
        other_cfg = {"game_exe": str(other_root / "bin" / "Hero_Siege.exe")}
        other_log = other_root / "bin" / "bp_ipc" / "out.txt"
        other_log.write_text(f"==== {MARKER} ====\n", encoding="utf-8")
        self.assertEqual(forgepact.plugin_boot_count(other_cfg), 1)
        self.assertEqual(naive_boot_count(other_log), 1)

    def test_a_warm_call_reads_only_what_was_appended(self):
        big = ("filler line that stands in for real plugin chatter\n" * 4000)
        self.write(f"==== {MARKER} ====\n{big}")
        size = self.log.stat().st_size
        self.assertGreater(size, 100_000, "the fixture must be big enough to matter")

        forgepact.BOOT_SCAN_BYTES = 0
        self.assertEqual(forgepact.plugin_boot_count(self.cfg), 1)
        cold = forgepact.BOOT_SCAN_BYTES
        self.assertGreaterEqual(cold, size, "a cold call reads the whole file")

        self.append("one more line\n")
        forgepact.BOOT_SCAN_BYTES = 0
        self.assertEqual(forgepact.plugin_boot_count(self.cfg), 1)
        self.assertLess(forgepact.BOOT_SCAN_BYTES, 4096,
                        "a warm call must read only the tail, not the whole log")

        # ...and dropping the cache goes back to reading everything, so the
        # saving really is the cache and not a smaller file.
        forgepact.reset_boot_count_cache()
        forgepact.BOOT_SCAN_BYTES = 0
        self.assertEqual(forgepact.plugin_boot_count(self.cfg), 1)
        self.assertGreaterEqual(forgepact.BOOT_SCAN_BYTES, size)

    def test_missing_log_returns_minus_one(self):
        self.assertEqual(forgepact.plugin_boot_count(self.cfg), -1)

    def test_missing_log_after_a_good_read_returns_minus_one(self):
        self.write(f"==== {MARKER} ====\n")
        self.assertEqual(forgepact.plugin_boot_count(self.cfg), 1)
        self.log.unlink()
        self.assertEqual(forgepact.plugin_boot_count(self.cfg), -1)

    def test_the_cache_is_guarded_by_a_lock(self):
        # /api/state runs on ThreadingHTTPServer handler threads while the
        # watcher daemon polls. Advancing the offset past bytes that were
        # never counted loses a boot forever, and auto-apply then silently
        # stops working after a game restart.
        self.assertIsInstance(forgepact._BOOT_LOCK, type(threading.Lock()))
        self.write(f"==== {MARKER} ====\n")
        errors = []

        def hammer():
            try:
                for _ in range(50):
                    self.assertEqual(forgepact.plugin_boot_count(self.cfg), 1)
            except Exception as exc:  # pragma: no cover - only on a real race
                errors.append(exc)

        threads = [threading.Thread(target=hammer) for _ in range(4)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()
        self.assertEqual(errors, [])


class ProcessEnumerationTests(unittest.TestCase):
    @unittest.skipUnless(os.name == "nt", "Win32 process enumeration")
    def test_overlapping_scans_do_not_corrupt_each_other(self):
        # REPORTED 2026-09-16 (PR #22 review): the ctypes prototypes used to be
        # assigned onto the SHARED ctypes.windll.kernel32 function objects
        # inside every call, so two threads mid-call corrupted each other.
        # Measured against the real functions: 159 of 160 concurrent scans
        # raised TypeError, and with only TWO workers 39 of 40 running_paths()
        # calls returned a false negative against 5/5 correct sequentially.
        #
        # That is reachable in normal use - watcher() polls on its own thread
        # while /api/state is served on ThreadingHTTPServer handler threads -
        # and a false negative is a wrong answer, not a slow one:
        # running_paths() turns OSError into [], so game_running() reads False
        # while the game is up.
        #
        # This drives the REAL functions against the real OS on purpose. The
        # defect lives entirely in the ctypes plumbing that a mocked row list
        # replaces, so a test with fake rows cannot see it - which is why the
        # existing mocked cases all passed over it.
        import concurrent.futures
        me = Path(sys.executable).name          # certainly running: this process

        errors = []

        def scan():
            try:
                return len(forgepact.snapshot_processes())
            except Exception as exc:                      # noqa: BLE001
                errors.append(repr(exc))
                return -1

        def find():
            try:
                return len(forgepact.running_paths(me))
            except Exception as exc:                      # noqa: BLE001
                errors.append(repr(exc))
                return -1

        # Sequential control first: if these fail, the environment is the
        # problem and a concurrent failure below would mean nothing.
        self.assertGreater(scan(), 0, "sequential snapshot found no processes")
        self.assertGreater(find(), 0, f"sequential scan did not find {me}")

        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
            results = list(pool.map(lambda fn: fn(), [scan, find] * 40))

        self.assertEqual(errors, [], "concurrent scans must not raise")
        self.assertTrue(all(r > 0 for r in results),
                        f"every concurrent scan must succeed; got {sorted(set(results))}")

    def test_the_prototypes_are_not_rebuilt_per_call(self):
        # The structural half of the case above, so the reason survives even on
        # a host where the threaded test skips: nothing inside either function
        # may define the structure or assign argtypes, and neither may touch
        # ctypes.windll - a process-wide cache shared with every other library
        # in this process.
        for fn in (forgepact.snapshot_processes, forgepact.process_image_path):
            body = inspect.getsource(fn)
            self.assertNotIn("windll", body, f"{fn.__name__} must not use ctypes.windll")
            self.assertNotIn(".argtypes", body, f"{fn.__name__} must not set argtypes per call")
            self.assertNotIn("class PROCESSENTRY32W", body)

    def test_no_process_is_spawned_to_list_processes(self):
        self.assertNotIn("tasklist", inspect.getsource(forgepact.running_paths))
        self.assertNotIn("tasklist", inspect.getsource(forgepact.snapshot_processes))

    def test_the_snapshot_is_taken_in_process(self):
        source = (SRC_DIR / "forgepact.py").read_text(encoding="utf-8")
        self.assertIn("CreateToolhelp32Snapshot", source)

    def test_names_are_matched_case_insensitively(self):
        rows = [(11, "Hero_Siege.exe"), (12, "HERO_SIEGE.EXE"),
                (13, "hero_siege.exe.bak"), (14, "steam.exe")]
        with unittest.mock.patch.object(forgepact, "snapshot_processes", return_value=rows), \
             unittest.mock.patch.object(forgepact, "process_image_path",
                                        side_effect=lambda pid: f"C:\\g{pid}\\Hero_Siege.exe"):
            self.assertEqual(forgepact.running_paths("hero_siege.exe"),
                             ["C:\\g11\\Hero_Siege.exe", "C:\\g12\\Hero_Siege.exe"])

    def test_a_process_that_cannot_be_opened_is_skipped(self):
        rows = [(11, "Hero_Siege.exe"), (12, "Hero_Siege.exe")]
        with unittest.mock.patch.object(forgepact, "snapshot_processes", return_value=rows), \
             unittest.mock.patch.object(forgepact, "process_image_path",
                                        side_effect=lambda pid: "" if pid == 11 else "C:\\g\\Hero_Siege.exe"):
            self.assertEqual(forgepact.running_paths("Hero_Siege.exe"), ["C:\\g\\Hero_Siege.exe"])

    def test_a_failed_snapshot_reads_as_not_running(self):
        # Deliberately NOT the launcher's fail-closed posture: an empty list
        # here makes the panel queue commands into cmd.txt instead of sending
        # them live, which is harmless. Changing that is a behaviour change.
        boom = OSError("Windows process snapshot could not be created")
        with unittest.mock.patch.object(forgepact, "snapshot_processes", side_effect=boom):
            self.assertEqual(forgepact.running_paths("Hero_Siege.exe"), [])
            self.assertFalse(forgepact.game_running({"game_exe": r"C:\g\Hero_Siege.exe"}))

    def test_no_cache_memoises_the_scan(self):
        # game_running() decides whether a command goes out live or is queued.
        # A cached "running" sends a command to a dead game; a cached "not
        # running" silently queues one the player expected to apply now.
        calls = []

        def counted():
            calls.append(1)
            return [(11, "Hero_Siege.exe")]

        with unittest.mock.patch.object(forgepact, "snapshot_processes", side_effect=counted), \
             unittest.mock.patch.object(forgepact, "process_image_path",
                                        side_effect=lambda pid: r"C:\g\Hero_Siege.exe"):
            for _ in range(3):
                forgepact.running_paths("Hero_Siege.exe")
        self.assertEqual(len(calls), 3, "the process scan must not be memoised")


class PollPolicyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (SRC_DIR / "forgepact.py").read_text(encoding="utf-8")

    def test_the_policy_is_a_named_constant_concatenated_into_the_page(self):
        self.assertTrue(hasattr(forgepact, "POLL_POLICY_JS"))
        self.assertIn("pollDelayMs(", forgepact.POLL_POLICY_JS)
        for name in POLL_CONSTANTS:
            self.assertIn(name, forgepact.POLL_POLICY_JS)
        self.assertIn(forgepact.POLL_POLICY_JS, forgepact.HTML)

    def test_the_fixed_five_second_timer_is_gone(self):
        self.assertNotIn("setInterval(async()=>{const s=await j('/api/state')", forgepact.HTML)
        self.assertNotIn(",5000)", forgepact.HTML)

    def test_a_hidden_window_is_gated(self):
        self.assertIn("document.hidden", forgepact.HTML)
        self.assertIn("visibilitychange", forgepact.HTML)

    def test_a_failed_boot_still_schedules_the_poll(self):
        # boot() does ~40 unguarded DOM lookups after its first await. With the
        # poll scheduled from .then(), any throw in there left the panel with no
        # timer and no message - frozen chips, stale gameRunning, forever. The
        # fixed setInterval this replaced was registered unconditionally, so
        # .then() alone was a regression rather than a like-for-like swap.
        #
        # Asserted on the source because the failure is "the scheduler was never
        # reached", which a DOM-less node run of pollDelayMs cannot observe.
        self.assertIn("boot().catch(", forgepact.HTML)
        self.assertIn(".finally(()=>{pollPrev=ST;schedulePoll()})", forgepact.HTML)
        self.assertNotIn("boot().then(()=>{pollPrev=ST;schedulePoll()})", forgepact.HTML)

    def test_the_watched_fields_are_declared(self):
        self.assertEqual(forgepact.POLL_WATCHED_FIELDS,
                         ["gameRunning", "ipcOk", "lastApplied", "queued"])
        for field in forgepact.POLL_WATCHED_FIELDS:
            self.assertIn(f'"{field}"', forgepact.POLL_POLICY_JS)

    def test_both_apps_share_the_same_constants(self):
        if not LAUNCHER_SRC.is_file():
            raise unittest.SkipTest(
                f"{LAUNCHER_SRC} is not checked out; the shared poll policy's "
                "constants can only be compared inside a full toolkit checkout")
        mine = js_constants(self.source)
        theirs = js_constants(LAUNCHER_SRC.read_text(encoding="utf-8"))
        for name in POLL_CONSTANTS:
            self.assertIn(name, mine)
            self.assertIn(name, theirs)
            self.assertEqual(mine[name], theirs[name],
                             f"{name} differs between the panel and the launcher")

    def test_the_delay_truth_table_executes(self):
        got = run_node(forgepact.POLL_POLICY_JS, DELAY_DRIVER)
        self.assertIsNone(got["hiddenNow"])
        self.assertIsNone(got["hiddenLater"])
        self.assertEqual(got["freshChange"], got["fast"])
        self.assertEqual(got["justInside"], got["fast"])
        self.assertEqual(got["atBoundary"], got["idle"])
        self.assertEqual(got["longIdle"], got["idle"])
        self.assertEqual(got["fields"], forgepact.POLL_WATCHED_FIELDS)

    def test_the_change_detector_executes(self):
        driver = """
const A={gameRunning:false,ipcOk:true,lastApplied:"12:00:00",queued:false,chain:{plugin:true}};
const same=JSON.parse(JSON.stringify(A));
const unwatched=JSON.parse(JSON.stringify(A)); unwatched.chain.plugin=false;
const watched=JSON.parse(JSON.stringify(A)); watched.queued=true;
console.log(JSON.stringify({
  identical: pollNextChangeAt(A, same, false, 9000, 100),
  unwatched: pollNextChangeAt(A, unwatched, false, 9000, 100),
  watched: pollNextChangeAt(A, watched, false, 9000, 100),
  localAction: pollNextChangeAt(A, same, true, 9000, 100),
  firstPayload: pollNextChangeAt(null, A, false, 9000, 100)
}));
"""
        got = run_node(forgepact.POLL_POLICY_JS, driver)
        self.assertEqual(got["identical"], 100, "an identical payload must not reset the clock")
        self.assertEqual(got["unwatched"], 100, "an unwatched field must not reset the clock")
        self.assertEqual(got["watched"], 9000, "a watched field difference resets the clock")
        self.assertEqual(got["localAction"], 9000, "a local action resets the clock")
        self.assertEqual(got["firstPayload"], 9000, "the first payload counts as a change")


class VersionStampTests(unittest.TestCase):
    """Finding 9's half that touches the panel, and the trap it must not spring."""

    @classmethod
    def setUpClass(cls):
        cls.source = (SRC_DIR / "forgepact.py").read_text(encoding="utf-8")
        cls.plugin_boot_line = (
            FORGEPACT_DIR / "plugin" / "include" / "ForgePact" / "ModManager.hpp"
        ).read_text(encoding="utf-8")

    def test_the_panel_declares_a_version(self):
        self.assertRegex(self.source, r'(?m)^__version__ = "\d+\.\d+\.\d+"$')
        self.assertRegex(forgepact.__version__, r"^\d+\.\d+\.\d+$")

    def test_the_version_appears_exactly_once_in_the_panel(self):
        # Rendered from /api/state, never embedded in HTML, so there is no
        # second literal that can go stale.
        self.assertEqual(self.source.count(forgepact.__version__), 1)
        self.assertNotIn(forgepact.__version__, forgepact.HTML)

    def test_the_state_payload_carries_the_version(self):
        self.assertIn('"version": __version__', self.source)
        self.assertIn("ST.version", forgepact.HTML)

    def test_the_boot_marker_stays_contiguous(self):
        # THE TRAP. plugin_boot_count() counts occurrences of this literal and
        # watcher() detects a new game process from a CHANGE in that count.
        # "BloodPact 1.3.20 plugin loaded" would break auto-apply after a game
        # restart, silently, with no error anywhere.
        self.assertIn(MARKER, self.plugin_boot_line)
        self.assertIn("FORGEPACT_VERSION", self.plugin_boot_line)
        stamped = self.plugin_boot_line[self.plugin_boot_line.index(MARKER):]
        self.assertLess(stamped.index(MARKER), stamped.index("FORGEPACT_VERSION"),
                        "the version must come after the marker, never inside it")

    def test_a_stamped_boot_line_is_still_counted_once(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "bin" / "bp_ipc").mkdir(parents=True)
            log = root / "bin" / "bp_ipc" / "out.txt"
            cfg = {"game_exe": str(root / "bin" / "Hero_Siege.exe")}
            # The exact shape the plugin now writes.
            log.write_text(f"==== {MARKER} ==== v{forgepact.__version__}\n",
                           encoding="utf-8")
            forgepact.reset_boot_count_cache()
            self.addCleanup(forgepact.reset_boot_count_cache)
            self.assertEqual(forgepact.plugin_boot_count(cfg), 1)
            self.assertEqual(naive_boot_count(log), 1)


if __name__ == "__main__":
    unittest.main()
