"""Contract tests for tools/cut_release.py.

Modelled on the superproject's tests/test_cut_release.py, against a copied
fixture tree via --root so nothing here can move the real version.

The case that matters most is the CRLF one. This worktree is CRLF
(`core.autocrlf=true` in both repositories) and `src/forgepact.py` is UTF-8
with a BOM; a text-mode read/rewrite would flip the whole file to LF and drop
the BOM for a one-line change, which is an enormous spurious diff and, for a
signed artifact, a signature failure. That has bitten this repository before.
"""

import importlib.util
import shutil
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools" / "cut_release.py"

_spec = importlib.util.spec_from_file_location("forgepact_cut_release", SCRIPT)
cut_release = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(cut_release)


class CutReleaseTests(unittest.TestCase):
    def setUp(self):
        self._tmp = Path(__file__).resolve().parents[1] / "build" / "cut-release-fixture"
        if self._tmp.exists():
            shutil.rmtree(self._tmp)
        self._tmp.mkdir(parents=True)
        self.addCleanup(shutil.rmtree, self._tmp, True)

        self.version = cut_release.current(ROOT)
        for site in cut_release.sites(self.version.encode()):
            target = self._tmp / site.path
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / site.path, target)
        boot = self._tmp / cut_release.BOOT_LINE_FILE
        boot.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(ROOT / cut_release.BOOT_LINE_FILE, boot)
        self.notes = self._tmp / cut_release.RELEASE_NOTES.format(version=self.version)
        self.notes.write_bytes(b"# ForgePact " + self.version.encode() + b"\r\n")

    def run_cli(self, *args):
        return subprocess.run(
            [sys.executable, str(SCRIPT), "--root", str(self._tmp), *args],
            capture_output=True, text=True)

    # ---- reading ----------------------------------------------------------
    def test_check_passes_on_a_tree_that_agrees(self):
        result = self.run_cli("--check")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(f"ForgePact version: {self.version}", result.stdout)
        for site in cut_release.sites(self.version.encode()):
            self.assertIn(f"ok      {self.version}  {site.path}", result.stdout)

    def test_expect_matching_the_tree_passes(self):
        result = self.run_cli("--check", "--expect", self.version)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_expect_naming_a_different_version_fails_and_says_both(self):
        result = self.run_cli("--check", "--expect", "9.9.9")
        self.assertEqual(result.returncode, 1)
        self.assertIn(self.version, result.stdout)
        self.assertIn("9.9.9", result.stdout)
        self.assertIn("MISMATCH", result.stdout)

    def test_help_documents_the_flags(self):
        result = self.run_cli("--help")
        self.assertEqual(result.returncode, 0)
        for flag in ("version", "--check", "--expect", "--root"):
            self.assertIn(flag, result.stdout)

    # ---- refusals ---------------------------------------------------------
    def test_a_hand_edited_site_fails_the_check(self):
        target = self._tmp / "plugin/include/ForgePact/Version.hpp"
        target.write_bytes(target.read_bytes().replace(
            self.version.encode(), b"9.9.9"))
        result = self.run_cli("--check")
        self.assertEqual(result.returncode, 1)
        self.assertIn("MISSING", result.stdout)

    def test_a_disagreeing_tree_is_not_half_bumped(self):
        target = self._tmp / "plugin/include/ForgePact/Version.hpp"
        target.write_bytes(target.read_bytes().replace(
            self.version.encode(), b"9.9.9"))
        panel_before = (self._tmp / "src/forgepact.py").read_bytes()

        result = self.run_cli("1.3.99")

        self.assertEqual(result.returncode, 1)
        self.assertIn("cannot", result.stdout + result.stderr)
        self.assertEqual((self._tmp / "src/forgepact.py").read_bytes(), panel_before,
                         "a half-bumped tree is worse than an un-bumped one")

    def test_a_non_version_argument_is_rejected(self):
        for bad in ("1.2", "01.0.2", "1.0.٣", "v1.2.3", "1.2.3.4"):
            with self.subTest(bad=bad):
                result = self.run_cli(bad)
                self.assertEqual(result.returncode, 1, f"{bad!r} was accepted")
                self.assertIn("three-part version", result.stdout + result.stderr)

    def test_missing_release_notes_fail_the_check(self):
        self.notes.unlink()
        result = self.run_cli("--check")
        self.assertEqual(result.returncode, 1)
        self.assertIn("release notes", result.stdout)
        self.notes.write_bytes(b"# ForgePact " + self.version.encode() + b"\r\n")
        self.assertEqual(self.run_cli("--check").returncode, 0)

    def test_allow_missing_notes_does_not_fail_the_check(self):
        # Only the tag workflow passes this: the notes file is composed into
        # the draft release body instead of being a prerequisite to tagging.
        self.notes.unlink()
        result = self.run_cli("--check", "--allow-missing-notes")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("NOTE", result.stdout)
        self.assertIn(self.notes.name, result.stdout)

    def test_allow_missing_notes_still_fails_a_literal_boot_line(self):
        # Negative control: the flag only relaxes the notes-file site, not
        # every derived check.
        self.notes.unlink()
        boot = self._tmp / cut_release.BOOT_LINE_FILE
        boot.write_bytes(boot.read_bytes().replace(
            b'==== v" FORGEPACT_VERSION', b'==== v1.3.19"'))
        result = self.run_cli("--check", "--allow-missing-notes")
        self.assertEqual(result.returncode, 1)
        self.assertIn("FORGEPACT_VERSION", result.stdout)

    def test_allow_missing_notes_needs_check(self):
        result = self.run_cli("1.3.99", "--allow-missing-notes")
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertIn("--allow-missing-notes", result.stderr)
        self.assertIn("--check", result.stderr)

    def test_a_literal_version_in_the_boot_line_fails_the_check(self):
        boot = self._tmp / cut_release.BOOT_LINE_FILE
        boot.write_bytes(boot.read_bytes().replace(
            b'==== v" FORGEPACT_VERSION', b'==== v1.3.19"'))
        result = self.run_cli("--check")
        self.assertEqual(result.returncode, 1)
        self.assertIn("FORGEPACT_VERSION", result.stdout)

    # ---- writing ----------------------------------------------------------
    def test_a_bump_moves_every_site(self):
        result = self.run_cli("1.3.99")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(cut_release.current(self._tmp), "1.3.99")
        # ...and says the notes for the new version are not written yet.
        self.assertIn("release-notes-v1.3.99.md", result.stdout)
        for site in cut_release.sites(b"1.3.99"):
            blob = (self._tmp / site.path).read_bytes()
            self.assertEqual(len(__import__("re").findall(site.pattern, blob)), 1)

    def test_the_boot_marker_is_left_contiguous(self):
        # The trap this whole finding has to avoid: plugin_boot_count() counts
        # occurrences of the literal marker, and watcher()'s new-process
        # detection is built on that count. Interpolating the version into the
        # marker would silently stop auto-apply after a game restart.
        self.run_cli("1.3.99")
        boot = (self._tmp / cut_release.BOOT_LINE_FILE).read_text(encoding="utf-8")
        self.assertIn(cut_release.BOOT_MARKER, boot)
        self.assertNotIn("1.3.99", boot, "the version must come from the macro")

    def test_crlf_and_the_bom_survive_a_bump(self):
        before = {site.path: (self._tmp / site.path).read_bytes()
                  for site in cut_release.sites(self.version.encode())}
        self.assertEqual(self.run_cli("1.3.99").returncode, 0)
        for path, old in before.items():
            after = (self._tmp / path).read_bytes()
            self.assertEqual(after.count(b"\r\n"), old.count(b"\r\n"),
                             f"{path}: CRLF count changed")
            self.assertEqual(after.count(b"\n") - after.count(b"\r\n"), 0,
                             f"{path}: a bare LF was introduced")
        panel = self._tmp / "src/forgepact.py"
        self.assertEqual(panel.read_bytes()[:3], b"\xef\xbb\xbf",
                         "src/forgepact.py's UTF-8 BOM must survive")

    def test_bumping_to_the_current_version_is_refused(self):
        result = self.run_cli(self.version)
        self.assertEqual(result.returncode, 1)
        self.assertIn("already at", result.stdout + result.stderr)

    # ---- the derived version resource -------------------------------------
    def test_the_windows_version_resource_is_derived_from_the_version(self):
        spec = importlib.util.spec_from_file_location(
            "forgepact_build_release", ROOT / "build_release.py")
        build_release = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(build_release)

        self.assertEqual(build_release.version_tuple("1.3.20"), (1, 3, 20, 0))
        body = build_release.version_info("1.3.20")
        self.assertIn("filevers=(1, 3, 20, 0)", body)
        self.assertIn("prodvers=(1, 3, 20, 0)", body)
        self.assertIn("StringStruct('FileVersion', '1.3.20')", body)
        self.assertIn("StringStruct('ProductVersion', '1.3.20')", body)

    def test_no_tracked_version_info_file_was_added(self):
        # HS-Offline-Launcher keeps one; ForgePact deliberately generates its
        # resource at build time from __version__, so there is no third place
        # to keep in step.
        self.assertFalse((ROOT / "version_info.txt").exists())
        source = (ROOT / "build_release.py").read_text(encoding="utf-8")
        self.assertIn("--version-file", source)
        self.assertIn("panel_version()", source)

    # ---- the tool's own boundaries ----------------------------------------
    def test_the_tool_touches_neither_git_nor_the_build(self):
        # It moves version strings and verifies them. Nothing else - same
        # boundary as the superproject's copy, which says so in its own
        # docstring. Checked as invocations rather than as words, because the
        # docstring legitimately says "does not touch git".
        source = SCRIPT.read_text(encoding="utf-8")
        for forbidden in ("import subprocess", "subprocess.", "Popen", "os.system",
                          "check_output", "import build_release", "build_release.main",
                          "'git'", '"git"'):
            self.assertFalse(forbidden in source,
                             f"cut_release.py must not reach for {forbidden}")


if __name__ == "__main__":
    unittest.main()
