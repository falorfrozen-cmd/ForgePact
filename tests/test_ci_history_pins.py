#!/usr/bin/env python3
"""The history pins in tests/test_toggle_skill_contract.py skip locally, fail in CI.

Eight tests in that module compare today's source with an earlier commit
(`git show <commit>:<path>`, or `origin/main`). Until ForgePact#176 they all
skipped when git could not read that commit, and the release build's checkout
was shallow, so the release never ran them and reported a green suite.

The rule is now:

- history unreadable, `CI` unset (a contributor's shallow clone): skip, as
  before -- the baseline below;
- history unreadable, `CI=true` (GitHub Actions sets it on every runner):
  fail, with a message naming the fix, `fetch-depth: 0` -- the target below.

Each case runs the module in a child process with `GIT_DIR` pointed at a path
that does not exist, which makes every history read fail the way a shallow
clone's does. The child's environment is built explicitly: on the release
runner this process already has `CI=true`, so a baseline that merely did not
set it would inherit it.
"""

import os
import re
import subprocess
import sys
import unittest
from pathlib import Path

TESTS_DIR = Path(__file__).resolve().parent
FORGEPACT_DIR = TESTS_DIR.parent
MODULE = "tests.test_toggle_skill_contract"

# (class, test) for every test in MODULE that reads git history.
HISTORY_TESTS = {
    ("ToggleProbeContractTests", "test_installers_the_probe_attaches_around_are_unchanged"),
    ("ToggleGuardContractTests", "test_talent_use_hook_is_unchanged_from_ab6fed5"),
    ("ToggleGuardContractTests", "test_indicator_decide_is_unchanged_from_ab6fed5"),
    ("SkillTimerBuffContractTests", "test_object_row_model_is_unchanged_from_30a851c"),
    ("ToggleTableProbeContractTests", "test_production_bodies_unchanged_from_62a67d2"),
    ("ToggleTableProbeContractTests", "test_kplayercommands_unchanged_from_62a67d2"),
    ("ToggleTableProbeContractTests", "test_shipped_draw_functions_unchanged_from_round_base"),
    ("ToggleTableProbeContractTests", "test_probe_bodies_unchanged_from_the_s_round_base"),
}

# The refs those tests read; the positive control runs only where all are readable.
HISTORY_REFS = ("origin/main", "ab6fed5", "30a851c", "62a67d2", "7169440", "2f40223")

# `test_x (tests.mod.Class.test_x) ... ok` (3.11+) or `test_x (tests.mod.Class) ... ok`,
# with a docstring's first line between the two when the test has one.
RESULT_LINE = re.compile(r"^(test_\w+) \(([\w.]+)\)(?:\n.*?)? \.\.\. (ok|skipped|FAIL|ERROR)", re.M)
# The header of each failure or error report after the run.
REPORT_HEAD = re.compile(r"^(FAIL|ERROR): (test_\w+) \(([\w.]+)\)", re.M)
# Colour codes, in case a runtime colours the output despite PYTHON_COLORS=0.
ANSI = re.compile("\x1b\\[[0-9;]*m")


def class_of(test: str, dotted: str) -> str:
    parts = dotted.split(".")
    return parts[-2] if parts[-1] == test else parts[-1]


def run_module(ci: bool, history: bool = False):
    """Run MODULE in a child process.

    Returns (exit code, {(class, test): status}, {(class, test): failure report}, output).
    """
    env = {k: v for k, v in os.environ.items() if k.upper() not in ("CI", "GIT_DIR", "GIT_WORK_TREE")}
    # 3.13+ colours unittest's output; the reader wants plain text.
    env["PYTHON_COLORS"] = "0"
    env["NO_COLOR"] = "1"
    if ci:
        env["CI"] = "true"
    if not history:
        env["GIT_DIR"] = str(FORGEPACT_DIR / "no-such-git-dir-176")
    proc = subprocess.run(
        [sys.executable, "-m", "unittest", "-v", MODULE],
        cwd=FORGEPACT_DIR, env=env, capture_output=True, timeout=600,
    )
    out = ANSI.sub("", proc.stderr.decode("utf-8", "replace").replace("\r\n", "\n"))
    statuses = {(class_of(m.group(1), m.group(2)), m.group(1)): m.group(3)
                for m in RESULT_LINE.finditer(out)}
    heads = list(REPORT_HEAD.finditer(out))
    reports = {}
    for i, m in enumerate(heads):
        end = heads[i + 1].start() if i + 1 < len(heads) else len(out)
        reports[(class_of(m.group(2), m.group(3)), m.group(2))] = out[m.start():end]
    return proc.returncode, statuses, reports, out


def history_readable() -> bool:
    for ref in HISTORY_REFS:
        try:
            subprocess.run(["git", "-C", str(FORGEPACT_DIR), "cat-file", "-e", f"{ref}^{{commit}}"],
                           capture_output=True, check=True)
        except (OSError, subprocess.CalledProcessError):
            return False
    return True


class TheReaderSeesTheModule(unittest.TestCase):
    """Positive control: the eight names this file expects exist in the module."""

    def test_every_history_test_is_named_in_the_module(self):
        source = (TESTS_DIR / "test_toggle_skill_contract.py").read_text(encoding="utf-8")
        for cls, test in HISTORY_TESTS:
            self.assertIn(f"class {cls}(", source)
            self.assertIn(f"def {test}(", source)


class WithoutCIUnreadableHistorySkips(unittest.TestCase):
    """Baseline: a shallow clone with no `CI` still passes, the eight skipped."""

    @classmethod
    def setUpClass(cls):
        cls.code, cls.statuses, cls.reports, cls.out = run_module(ci=False)

    def test_the_module_passes(self):
        self.assertEqual(self.code, 0, self.out[-3000:])

    def test_the_reader_saw_the_whole_module(self):
        # A reader that parses no result lines has measured nothing.
        self.assertGreater(len(self.statuses), 200, self.out[-3000:])

    def test_exactly_the_history_tests_skip(self):
        skipped = {key for key, status in self.statuses.items() if status == "skipped"}
        self.assertEqual(skipped, HISTORY_TESTS)


class InCIUnreadableHistoryFails(unittest.TestCase):
    """Target: with `CI=true` the same eight fail, each naming `fetch-depth`."""

    @classmethod
    def setUpClass(cls):
        cls.code, cls.statuses, cls.reports, cls.out = run_module(ci=True)

    def test_the_module_fails(self):
        self.assertNotEqual(self.code, 0)

    def test_the_reader_saw_the_whole_module(self):
        self.assertGreater(len(self.statuses), 200, self.out[-3000:])

    def test_exactly_the_history_tests_fail_and_none_skip(self):
        failed = {key for key, status in self.statuses.items() if status == "FAIL"}
        self.assertEqual(failed, HISTORY_TESTS)
        self.assertNotIn("skipped", self.statuses.values())
        self.assertNotIn("ERROR", self.statuses.values())

    def test_each_failure_names_the_fix(self):
        for key in HISTORY_TESTS:
            with self.subTest(test=key):
                self.assertIn(key, self.reports)
                self.assertIn("fetch-depth", self.reports[key])
                self.assertIn("ForgePact#176", self.reports[key])


class InCIReadableHistoryRuns(unittest.TestCase):
    """Positive control: where history is readable, `CI=true` runs the eight and they pass."""

    def test_the_history_tests_pass(self):
        if not history_readable():
            self.skipTest("this checkout cannot read every pinned commit")
        code, statuses, _reports, out = run_module(ci=True, history=True)
        self.assertEqual(code, 0, out[-3000:])
        for key in HISTORY_TESTS:
            self.assertEqual(statuses.get(key), "ok", key)


if __name__ == "__main__":
    unittest.main()
