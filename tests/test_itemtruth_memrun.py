"""tools/itemtruth_memrun.py: the parts that run without a game.

The harness measures Item Truth evaluation requests from outside the game, so
what it must get right on its own is pinned here: the request lines are ones
ForgePact's parser accepts (ItemTruth.hpp `ParseRequest`: `IsItemKey`,
`IsObjectText`, kMaxLineBytes), every item has its own timestamp in the run's
range, a runeword keeps its seed, it reads progress only from its own game
session, and it moves a journal file out of the Item Editor's way only when
every line in it is one of its own requests'.
"""

import json
import re
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import itemtruth_memrun as memrun  # noqa: E402

HEADER = (ROOT / "plugin" / "include" / "ForgePact" / "ItemTruth.hpp").read_text(encoding="utf-8")
MAX_LINE_BYTES = int(re.search(r"kMaxLineBytes = (\d+) \* 1024;", HEADER).group(1)) * 1024


def is_item_key(key: str) -> bool:
    """ItemTruth.hpp IsItemKey: four groups of 1-20 digits, the timestamp not zero."""
    parts = key.split("-")
    return (len(parts) == 4 and all(p.isdigit() and 1 <= len(p) <= 20 for p in parts)
            and parts[2].strip("0") != "")


def is_object_text(text: str) -> bool:
    """ItemTruth.hpp IsObjectText: one line, an object."""
    return len(text) >= 2 and text[0] == "{" and text[-1] == "}" and "\n" not in text and "\r" not in text


WHITE = {"w": 1.0, "o": 1.0, "a": 172693.0, "b": 0.0, "j": 0.0, "c": 0.0}
UNIQUE = {"w": 1.0, "m": 1.0, "a": 88780570.0, "b": 1.0, "j": 0.0, "c": 1.0}
SOCKET = {"w": 1.0, "o": 1.0, "zz": {"sockets": 1.0}, "a": 711745437.0, "b": 15.0, "j": 0.0, "c": 0.0}
RUNEWORD = {"s1": {"a": 49798150.0, "b": 26.0, "n": 0.0}, "s2": {"a": 16980294.0, "b": 15.0, "n": 0.0},
            "zz": {"sockets": 2.0}, "a": 4.0, "n": 0.0, "j": 1.0, "w": 1.0, "e": 0.0, "i": 424123.0,
            "b": 0.0, "d": 0.0, "c": 0.0}


def compact(record):
    """ForgePact writes its journal lines without spaces (ItemTruth.hpp FormatRecord)."""
    return json.dumps(record, separators=(",", ":"))


def eval_record(definition, item_type=3, request="1790000000000-abc123"):
    return {"v": 1, "src": "eval", "req": request, "build": "pe-6aaa6779-0cad4fc8", "t": 1,
            "ts": "17903613141427", "type": item_type, "hash": "h", "def": definition, "stats": {}}


class ClassifyAndTemplateTests(unittest.TestCase):
    def test_the_four_classes(self):
        self.assertEqual("white", memrun.classify(WHITE))
        self.assertEqual("unique", memrun.classify(UNIQUE))
        self.assertEqual("socket", memrun.classify(SOCKET))
        self.assertEqual("runeword", memrun.classify(RUNEWORD))

    def test_templates_come_only_from_evaluated_items(self):
        with tempfile.TemporaryDirectory() as tmp:
            journal = Path(tmp)
            lines = [eval_record(WHITE, 3), eval_record(UNIQUE, 0), eval_record(SOCKET, 7), eval_record(RUNEWORD, 3),
                     {**eval_record(WHITE, 5), "src": "live"},                     # an item of play
                     {"v": 1, "kind": "eval", "req": "1790000000000-abc123", "done": 4},   # progress
                     {**eval_record(WHITE), "type": True}]                        # not a type
            text = "\n".join(compact(line) for line in lines) + "\nnot json\n"
            (journal / "live-pe-x-20260926-000000-1-1.ndjson").write_text(text, encoding="utf-8")
            found = memrun.load_templates(journal)
        self.assertEqual({"white": [(3, WHITE)], "unique": [(0, UNIQUE)], "socket": [(7, SOCKET)],
                          "runeword": [(3, RUNEWORD)]}, found)

    def test_templates_stop_at_the_cap_per_class(self):
        with tempfile.TemporaryDirectory() as tmp:
            journal = Path(tmp)
            text = "\n".join(compact(eval_record(dict(WHITE, b=float(i)))) for i in range(30))
            (journal / "live-pe-x-20260926-000000-1-1.ndjson").write_text(text, encoding="utf-8")
            found = memrun.load_templates(journal, per_class=10)
        self.assertEqual(10, len(found["white"]))

    def test_a_spaced_line_reads_the_same(self):
        with tempfile.TemporaryDirectory() as tmp:
            journal = Path(tmp)
            path = journal / "live-pe-x-20260926-000000-1-1.ndjson"
            path.write_text(json.dumps(eval_record(UNIQUE, 4)) + "\n", encoding="utf-8")
            self.assertEqual([(4, UNIQUE)], memrun.load_templates(journal)["unique"])


class RequestLineTests(unittest.TestCase):
    TEMPLATES = {"white": [(3, WHITE)], "unique": [(0, UNIQUE)], "socket": [(7, SOCKET)], "runeword": [(3, RUNEWORD)]}

    def parse(self, line):
        self.assertTrue(line.endswith("\n"))
        key, _, text = line[:-1].partition("\t")
        return key, text

    def test_every_line_is_one_forgepacts_parser_accepts(self):
        lines = memrun.request_lines(self.TEMPLATES, 400, seed=5, ts_base=memrun.TS_BASE, mix=True)
        self.assertEqual(400, len(lines))
        for line in lines:
            key, text = self.parse(line)
            self.assertTrue(is_item_key(key), key)
            self.assertTrue(is_object_text(text), text)
            self.assertLessEqual(len(text.encode()), MAX_LINE_BYTES)
            self.assertIsInstance(json.loads(text), dict)

    def test_each_item_has_its_own_timestamp_in_the_runs_range(self):
        lines = memrun.request_lines(self.TEMPLATES, 300, seed=1, ts_base=memrun.TS_BASE + 500, mix=True)
        stamps = [int(self.parse(line)[0].split("-")[2]) for line in lines]
        self.assertEqual(list(range(memrun.TS_BASE + 500, memrun.TS_BASE + 800)), stamps)
        self.assertGreater(memrun.TS_BASE, 1_795_600_000_000 + 10_000_000, "clear of the seed-table ranges")

    def test_the_key_carries_the_items_type(self):
        lines = memrun.request_lines({"white": [(7, WHITE)]}, 3, seed=1, ts_base=memrun.TS_BASE, mix=False)
        self.assertTrue(all(self.parse(line)[0].endswith("-7") for line in lines))

    def test_seeds_are_redrawn_except_a_runewords(self):
        lines = memrun.request_lines(self.TEMPLATES, 2000, seed=9, ts_base=memrun.TS_BASE, mix=True)
        seen = {"white": set(), "unique": set(), "socket": set(), "runeword": set()}
        for line in lines:
            data = json.loads(self.parse(line)[1])
            seen[memrun.classify(data)].add(data["a"])
        self.assertEqual({RUNEWORD["a"]}, seen["runeword"], "a runeword's base must stay Common")
        for name in ("white", "unique", "socket"):
            self.assertGreater(len(seen[name]), 50, name)
            self.assertTrue(all(1 <= a < 2_147_483_647 for a in seen[name]))

    def test_the_mix_follows_its_shares_and_plain_runs_use_white_bases(self):
        lines = memrun.request_lines(self.TEMPLATES, 8000, seed=3, ts_base=memrun.TS_BASE, mix=True)
        counts = {name: 0 for name in memrun.CLASSES}
        for line in lines:
            counts[memrun.classify(json.loads(self.parse(line)[1]))] += 1
        for name, share in memrun.MIX:
            self.assertAlmostEqual(share, counts[name] / 8000, delta=0.03, msg=name)
        plain = memrun.request_lines(self.TEMPLATES, 200, seed=3, ts_base=memrun.TS_BASE, mix=False)
        self.assertEqual({"white"}, {memrun.classify(json.loads(self.parse(line)[1])) for line in plain})

    def test_the_same_seed_writes_the_same_request(self):
        one = memrun.request_lines(self.TEMPLATES, 50, seed=4, ts_base=memrun.TS_BASE, mix=True)
        two = memrun.request_lines(self.TEMPLATES, 50, seed=4, ts_base=memrun.TS_BASE, mix=True)
        self.assertEqual(one, two)

    def test_no_usable_template_is_an_error_not_an_empty_request(self):
        with self.assertRaises(ValueError):
            memrun.request_lines({"white": [], "unique": [(0, UNIQUE)]}, 5, seed=1, ts_base=memrun.TS_BASE, mix=False)

    def test_the_request_file_appears_whole_under_a_request_id(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp) / "requests"
            lines = memrun.request_lines(self.TEMPLATES, 5, seed=1, ts_base=memrun.TS_BASE, mix=False)
            request_id = memrun.write_request(folder, lines, "run")
            self.assertRegex(request_id, r"^\d+-memrunrun$")
            self.assertLessEqual(len(request_id), 64, "ItemTruth.hpp IsRequestId")
            self.assertEqual([request_id + ".req"], [p.name for p in folder.iterdir()])
            self.assertEqual("".join(lines), (folder / f"{request_id}.req").read_text(encoding="utf-8"))


class JournalTests(unittest.TestCase):
    OURS = "1790364021445-memrunrun"

    def write(self, journal, pid, part, records):
        path = journal / f"live-pe-6aaa6779-0cad4fc8-20260926-021900-{pid}-{part}.ndjson"
        path.write_text("".join(compact(r) + "\n" for r in records), encoding="utf-8")
        return path

    def progress(self, done, finished, request=None):
        return {"v": 1, "kind": "eval", "req": request or self.OURS, "total": 20000, "done": done,
                "ok": done, "failed": 0, "rejected": 0, "finished": finished}

    def test_progress_is_read_from_the_runs_own_game_session(self):
        with tempfile.TemporaryDirectory() as tmp:
            journal = Path(tmp)
            self.write(journal, 27552, 1, [self.progress(0, False), eval_record(WHITE, request=self.OURS)])
            self.write(journal, 27552, 2, [self.progress(19750, False), self.progress(20000, True),
                                           self.progress(5, True, request="1790000000000-other1")])
            self.write(journal, 8836, 1, [self.progress(20000, True)])   # another game session
            self.assertEqual((20000, True), memrun.eval_progress(journal, 27552, self.OURS))
            self.assertEqual((0, False), memrun.eval_progress(journal, 27552, "1790000000000-nothere"))
            self.assertEqual((20000, True), memrun.eval_progress(journal, 8836, self.OURS))

    def test_a_file_is_ours_only_when_every_line_is(self):
        with tempfile.TemporaryDirectory() as tmp:
            journal = Path(tmp)
            ours = {self.OURS}
            mine = self.write(journal, 1, 1, [self.progress(0, False), eval_record(WHITE, request=self.OURS),
                                              self.progress(1, True)])
            self.assertTrue(memrun.only_our_lines(mine, ours))
            played = self.write(journal, 2, 1, [eval_record(WHITE, request=self.OURS),
                                                {**eval_record(WHITE), "src": "live", "req": None}])
            self.assertFalse(memrun.only_our_lines(played, ours), "an item of play is the player's")
            editor = self.write(journal, 3, 1, [eval_record(WHITE, request="1790000000000-editor")])
            self.assertFalse(memrun.only_our_lines(editor, ours), "the Item Editor's own request")
            tooltip = self.write(journal, 4, 1, [{"v": 1, "kind": "tooltip", "req": self.OURS, "ts": "1"}])
            self.assertFalse(memrun.only_our_lines(tooltip, ours))
            broken = journal / "live-pe-x-5-1.ndjson"
            broken.write_text(compact(self.progress(1, True)) + "\n{broken\n", encoding="utf-8")
            self.assertFalse(memrun.only_our_lines(broken, ours))
            self.assertFalse(memrun.only_our_lines(journal / "missing.ndjson", ours))


class SettleTests(unittest.TestCase):
    """The three menu starts of 2026-09-26, one sample a second from the journal's start."""

    @staticmethod
    def menu(levels):
        out = []
        for seconds, value in levels:
            out += [value] * seconds
        return out

    def first_ready(self, samples, min_seconds=60):
        for count in range(1, len(samples) + 1):
            if memrun.settled(samples[:count], count, min_seconds):
                return count
        return None

    def test_a_request_waits_for_the_menus_one_time_release(self):
        early = self.menu([(10, 3150.0), (35, 3154.0), (200, 2784.0)])     # released at 45 s
        self.assertEqual(75, self.first_ready(early))
        late = self.menu([(12, 3032.0), (4, 2900.0), (90, 3032.0), (200, 2805.0)])   # a dip at 12 s, released at 106 s
        self.assertEqual(136, self.first_ready(late), "a dip that comes back is not the release")

    def test_a_menu_that_never_releases_is_never_settled(self):
        self.assertIsNone(self.first_ready(self.menu([(300, 3031.0)])))

    def test_an_unsteady_level_is_not_settled(self):
        wobbly = self.menu([(10, 3150.0)]) + [2800.0 + (i % 2) * 20 for i in range(200)]
        self.assertIsNone(self.first_ready(wobbly))


class SummaryTests(unittest.TestCase):
    def test_the_baseline_is_the_last_twenty_seconds_before_the_request(self):
        rows = [(float(t), "boot", 100.0, 0.0) for t in range(5)]
        rows += [(float(t), "settle", 3000.0 if t < 30 else 2800.0, 0.0) for t in range(5, 60)]
        rows += [(float(t), "eval", 2805.0, 0.0) for t in range(60, 100)]
        rows += [(float(t), "post", 2810.0, 0.0) for t in range(100, 280)]
        summary = memrun.summarize(rows, 20000)
        self.assertEqual(2800.0, summary["baseline_mb"])
        self.assertEqual(2805.0, summary["peak_while_building_mb"])
        self.assertEqual(10.0, summary["delta_mb"])
        self.assertEqual(0.51, summary["kb_per_item"])

    def test_a_run_cut_short_reports_what_it_has(self):
        summary = memrun.summarize([(0.0, "settle", 2800.0, 0.0)], 20000)
        self.assertEqual({"items": 20000, "baseline_mb": 2800.0}, summary)


class ExitCodeTests(unittest.TestCase):
    """A missing dump is not a clean exit: the report reads the exit code."""

    def test_an_abort_is_named_whatever_its_sign(self):
        for code in (0xC0000409, 3221226505, -1073740791):
            self.assertIn("0xC0000409: the game aborted", memrun.describe_exit(code))

    def test_other_exits_read_plainly(self):
        self.assertEqual("exit 0", memrun.describe_exit(0))
        self.assertEqual("exit 0x00000001", memrun.describe_exit(1))
        self.assertIn("unknown", memrun.describe_exit(None))

    def test_the_game_is_launched_with_the_default_error_mode(self):
        source = (ROOT / "tools" / "itemtruth_memrun.py").read_text(encoding="utf-8")
        launch = source[source.index("class Game:"):source.index("def alive(self)")]
        self.assertIn("creationflags=subprocess.CREATE_DEFAULT_ERROR_MODE", launch)
        close = source[source.index("def close(self"):source.index("def game_running()")]
        self.assertIn("self.exit_code = self.process.returncode", close)

    @unittest.skipUnless(sys.platform == "win32", "Windows error modes")
    def test_the_flag_is_what_stops_the_inherited_mode(self):
        """Positive control first: a child inherits SEM_NOGPFAULTERRORBOX (0x2), as a
        game started from Git Bash does; with CREATE_DEFAULT_ERROR_MODE it does not."""
        import ctypes
        import subprocess
        kernel32 = ctypes.windll.kernel32
        probe = [sys.executable, "-c", "import ctypes; print(ctypes.windll.kernel32.GetErrorMode())"]
        before = kernel32.SetErrorMode(0x3)
        try:
            inherited = int(subprocess.run(probe, capture_output=True, text=True, check=True).stdout)
            default = int(subprocess.run(probe, capture_output=True, text=True, check=True,
                                         creationflags=subprocess.CREATE_DEFAULT_ERROR_MODE).stdout)
        finally:
            kernel32.SetErrorMode(before)
        self.assertTrue(inherited & 0x2, f"control: the child should inherit 0x3, got 0x{inherited:x}")
        self.assertFalse(default & 0x2, f"with the flag the child must not have 0x2, got 0x{default:x}")


class CommandLineTests(unittest.TestCase):
    def test_the_control_is_always_the_mix(self):
        source = (ROOT / "tools" / "itemtruth_memrun.py").read_text(encoding="utf-8")
        self.assertIn('if args.action == "control":\n        args.mix = True', source)
        for step in ('"truthmem hold on"', '"truthmem hold off"', '"truthmem release"', '"truthmem gc"'):
            self.assertIn(step, source)

    def test_the_game_is_closed_like_the_player_closes_it(self):
        source = (ROOT / "tools" / "itemtruth_memrun.py").read_text(encoding="utf-8")
        self.assertIn("CloseMainWindow()", source)
        self.assertNotIn("Stop-Process", source)
        self.assertNotIn("TerminateProcess", source)
        self.assertNotIn(".kill()", source)


if __name__ == "__main__":
    unittest.main()
