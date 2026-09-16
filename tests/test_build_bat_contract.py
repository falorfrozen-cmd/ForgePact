"""Contract for `plugin_build/build.bat`'s compiler discovery.

Baseline first, then target: the `cl ` line and the FLAGS/OUTPUT/OBJDIR block
are the compile contract and must never move (`tests/test_release_ci.py`'s
`compile-line` check depends on that line being byte-identical between a
tagged tree and `main`); the discovery block above them is what changes, to
find MSVC on a GitHub-hosted Windows runner (VS 18.9, `windows-2025-vs2026`)
that none of the four hardcoded paths this file currently probes matches.

The file on disk is CRLF (`.gitattributes`: `*.bat text eol=crlf`), so it is
read in binary and decoded explicitly rather than through `Path.read_text`,
which would normalise line endings and hide a real regression.
"""

import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
BUILD_BAT = PROJECT_ROOT / "plugin_build" / "build.bat"


def build_bat_text() -> str:
    return BUILD_BAT.read_bytes().decode("utf-8")


class TheFileIsCrlf(unittest.TestCase):
    def test_on_disk_line_endings_are_crlf(self):
        raw = BUILD_BAT.read_bytes()
        self.assertIn(b"\r\n", raw)
        # No bare LF anywhere -- a bare LF label inside a batch file is a
        # `goto` trap (cmd mis-parses labels in LF-only batch files).
        stripped = raw.replace(b"\r\n", b"")
        self.assertNotIn(b"\n", stripped, "a bare LF survived CRLF normalisation")


class TheCompileLineIsUnchanged(unittest.TestCase):
    """Baseline: this line and the block around it are the compile contract."""

    EXPECTED_CL_LINE = (
        'cl /nologo /std:c++20 /EHsc /MD /LD /O2 /DNDEBUG /DYYTK_DEFINE_INTERNAL=1 '
        '%FLAGS% /I "include" /I "%~dp0..\\plugin\\include" '
        '/I "%~dp0..\\..\\hs-game-sdk\\cpp\\include" "%SOURCE%" '
        '"include\\YYToolkit\\YYTK_Shared_Types.cpp" /Fe:%OUTPUT% /Fo:%OBJDIR%\\ '
        '/link /DLL user32.lib'
    )

    def test_the_cl_line_is_verbatim(self):
        lines = [line.strip() for line in build_bat_text().splitlines()]
        cl_lines = [line for line in lines if line.startswith("cl ")]
        self.assertEqual(
            len(cl_lines), 1, f"expected exactly one 'cl ' line, found {cl_lines}"
        )
        self.assertEqual(cl_lines[0], self.EXPECTED_CL_LINE)

    def test_release_branch_sets_forgepact_release(self):
        text = build_bat_text()
        self.assertIn('set "FLAGS=/DFORGEPACT_RELEASE"', text)

    def test_dev_branch_sets_empty_flags(self):
        text = build_bat_text()
        self.assertIn('set "FLAGS="', text)

    def test_the_four_legacy_vcvars_paths_are_still_present(self):
        text = build_bat_text()
        for path in (
            r"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools"
            r"\VC\Auxiliary\Build\vcvars64.bat",
            r"C:\Program Files\Microsoft Visual Studio\18\Community"
            r"\VC\Auxiliary\Build\vcvars64.bat",
            r"C:\Program Files\Microsoft Visual Studio\2022\Community"
            r"\VC\Auxiliary\Build\vcvars64.bat",
            r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
            r"\VC\Auxiliary\Build\vcvars64.bat",
        ):
            self.assertIn(path, text, f"legacy fallback path missing: {path}")


class TheCompilerDiscoveryFindsAGitHubRunner(unittest.TestCase):
    """Target: not satisfied until build.bat's discovery block is rewritten."""

    def test_vswhere_is_tried_before_the_first_legacy_path(self):
        text = build_bat_text()
        vswhere_at = text.find("vswhere.exe")
        self.assertNotEqual(vswhere_at, -1, "vswhere.exe is not used at all")
        first_legacy_at = text.find(
            r"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools"
        )
        self.assertNotEqual(first_legacy_at, -1)
        self.assertLess(
            vswhere_at, first_legacy_at,
            "vswhere must be tried before falling back to a hardcoded path",
        )

    def test_vswhere_requires_the_vc_tools_component(self):
        self.assertIn(
            "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", build_bat_text()
        )

    def test_a_vscmd_ver_check_precedes_vswhere(self):
        text = build_bat_text()
        vscmd_at = text.find("VSCMD_VER")
        vswhere_at = text.find("vswhere.exe")
        self.assertNotEqual(vscmd_at, -1, "no already-initialised-environment check")
        self.assertNotEqual(vswhere_at, -1)
        self.assertLess(
            vscmd_at, vswhere_at,
            "an already-initialised MSVC environment must be tried before vswhere",
        )

    def test_error_message_on_total_failure_is_kept(self):
        self.assertIn("ERROR: Visual Studio not found", build_bat_text())


if __name__ == "__main__":
    unittest.main()
