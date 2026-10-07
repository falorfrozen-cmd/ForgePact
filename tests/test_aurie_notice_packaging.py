"""The `AurieCore.dll` ForgePact ships is a modified AGPL-3.0 binary (the hub's
`third_party/aurie/` patch series, issue #151), so its modification notice and
build record have to travel with it, the way YYToolkit's do. Pins three things:

  - `aurie-modified/` holds `AurieCore-NOTICE.md` and `AurieCore-BUILD-INFO.json`,
    and both name the same DLL the `modfiles_shipped/AurieCore.dll` pin does;
  - `build_release.py` refuses to package without either file, and with both
    present the next guard decides (the negative control);
  - packaging copies both pairs into `modfiles/` under names that cannot
    collide: `modfiles/NOTICE.md` is already YYToolkit's notice.

Runs alone (`py -3 -m unittest discover -s tests -p test_aurie_notice_packaging.py`)
and never reaches PyInstaller, the network or the game."""
import contextlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

HERE = Path(__file__).resolve().parent
REPO = HERE.parent
sys.path.insert(0, str(REPO))

import build_release  # noqa: E402  (safe: main() is __main__-guarded)

PINS_PATH = REPO / "tools" / "toolchain-pins.json"
NOTICE = REPO / "aurie-modified" / "AurieCore-NOTICE.md"
BUILD_INFO = REPO / "aurie-modified" / "AurieCore-BUILD-INFO.json"
#: Upstream's unmodified v2.0.2 release DLL -- what the pin held before #151.
UPSTREAM_SHA = "18e3a1de980f487a6b3858b673d2030e96984dd96de3a047b43a263a5ba829ae"
INDEX = b'<!doctype html><html><body><div id="app"></div></body></html>'


def aurie_pin() -> dict:
    files = json.loads(PINS_PATH.read_text(encoding="utf-8"))["files"]
    entries = [f for f in files if f["dest"] == "modfiles_shipped/AurieCore.dll"]
    if len(entries) != 1:
        raise AssertionError(f"expected one modfiles_shipped/AurieCore.dll pin, found {len(entries)}")
    return entries[0]


class TheNoticePairIsCommittedAndAgreesWithThePin(unittest.TestCase):
    def test_build_release_names_the_aurie_pair(self):
        self.assertEqual(build_release.AURIE_NOTICE_DIR, REPO / "aurie-modified")
        self.assertEqual(build_release.AURIE_NOTICE_FILES,
                         ["AurieCore-NOTICE.md", "AurieCore-BUILD-INFO.json"])

    def test_both_files_are_present(self):
        for p in (NOTICE, BUILD_INFO):
            self.assertTrue(p.is_file(), f"{p.relative_to(REPO).as_posix()} is missing")

    def test_build_info_describes_the_pinned_dll(self):
        info = json.loads(BUILD_INFO.read_text(encoding="utf-8"))
        pin = aurie_pin()
        self.assertEqual(info["tool"], "tools/build_aurie.py")
        self.assertEqual(info["dll"]["name"], "AurieCore.dll")
        self.assertEqual(info["dll"]["sha256"], pin["sha256"])
        self.assertIs(info["live_gameplay_verified"], False)
        self.assertIs(info["hub"]["patch_directory_dirty"], False)

    def test_the_pin_is_not_upstreams_release(self):
        # Negative control for the agreement above: upstream's v2.0.2 DLL has
        # no notice to agree with, and a pin back at it ships no modification.
        pin = aurie_pin()
        self.assertNotEqual(pin["sha256"], UPSTREAM_SHA)
        self.assertNotIn("AurieFramework/Aurie/releases", pin["url"])

    def test_notice_says_what_a_modification_notice_must(self):
        text = NOTICE.read_text(encoding="utf-8")
        for needle in ("modified", "AGPL-3.0", "https://github.com/AurieFramework/Aurie",
                       "third_party/aurie/upstream.json", "third_party/aurie/patches",
                       "tools/build_aurie.py", "AurieCore-BUILD-INFO.json",
                       aurie_pin()["sha256"]):
            self.assertIn(needle, text, f"AurieCore-NOTICE.md does not mention {needle!r}")

    def test_the_names_do_not_collide_with_yytoolkits(self):
        self.assertFalse(set(build_release.AURIE_NOTICE_FILES)
                         & set(build_release.YYTOOLKIT_NOTICE_FILES))


class PackagingRefusesWithoutTheNotice(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="forgepact-aurie-notice-")
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        (self.base / "dist").mkdir()
        (self.base / "dist" / "index.html").write_bytes(INDEX)
        modfiles = self.base / "modfiles"
        modfiles.mkdir()
        for name in build_release.NEEDED:
            (modfiles / name).write_bytes(b"fake " + name.encode())
        (self.base / "aurie-modified").mkdir()
        # An empty root: no plugin_build/, so the guard after the notice
        # guards is the one that names BloodPactPlugin_ship.dll.
        (self.base / "root").mkdir()

    def run_main(self, **patches):
        out = io.StringIO()
        base = {"PANEL_DIST": self.base / "dist", "MODFILES": self.base / "modfiles",
                "AURIE_NOTICE_DIR": self.base / "aurie-modified", "ROOT": self.base / "root"}
        base.update(patches)
        with contextlib.ExitStack() as stack:
            for name, value in base.items():
                stack.enter_context(patch.object(build_release, name, value))
            # Nothing past the guards may run from a test.
            run = stack.enter_context(patch.object(build_release.subprocess, "run"))
            stack.enter_context(contextlib.redirect_stdout(out))
            code = build_release.main()
        run.assert_not_called()
        return code, out.getvalue()

    def test_an_empty_aurie_modified_is_refused(self):
        code, out = self.run_main()
        self.assertEqual(code, 1)
        self.assertIn("aurie-modified is incomplete", out)
        self.assertIn("AurieCore-NOTICE.md", out)
        self.assertIn("AurieCore-BUILD-INFO.json", out)

    def test_the_build_record_alone_is_refused(self):
        (self.base / "aurie-modified" / "AurieCore-BUILD-INFO.json").write_text("{}", encoding="utf-8")
        code, out = self.run_main()
        self.assertEqual(code, 1)
        self.assertIn("aurie-modified is incomplete -> AurieCore-NOTICE.md", out)

    def test_with_both_present_the_next_guard_decides(self):
        for name in build_release.AURIE_NOTICE_FILES:
            (self.base / "aurie-modified" / name).write_text("x", encoding="utf-8")
        code, out = self.run_main()
        self.assertEqual(code, 1)
        self.assertNotIn("aurie-modified is incomplete", out)
        self.assertIn("BloodPactPlugin_ship.dll is missing", out)


class PackagingShipsBothPairs(unittest.TestCase):
    def test_both_notices_and_build_records_land_in_modfiles(self):
        with tempfile.TemporaryDirectory(prefix="forgepact-aurie-ship-") as d:
            out_mod = Path(d) / "modfiles"
            out_mod.mkdir()
            build_release.ship_notices(out_mod)
            shipped = sorted(p.name for p in out_mod.iterdir())
            self.assertEqual(shipped, sorted(build_release.YYTOOLKIT_NOTICE_FILES
                                             + build_release.AURIE_NOTICE_FILES))
            self.assertEqual((out_mod / "AurieCore-NOTICE.md").read_bytes(), NOTICE.read_bytes())
            self.assertEqual((out_mod / "NOTICE.md").read_bytes(),
                             (build_release.YYTOOLKIT_NOTICE_DIR / "NOTICE.md").read_bytes())


if __name__ == "__main__":
    unittest.main()
