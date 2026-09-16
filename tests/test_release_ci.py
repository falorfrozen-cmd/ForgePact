"""Tests for tools/release_ci.py -- the three pieces that only exist for CI:
normalising a typed tag, proving a copied build.bat still compiles the same
thing, and packaging dist/ForgePact/ into the release zip. No network, no
compiler; `package` runs against a fake `dist/ForgePact/`.
"""

import hashlib
import io
import json
import subprocess
import sys
import unittest
import zipfile
from pathlib import Path
from tempfile import TemporaryDirectory

REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "tools"))

import release_ci  # noqa: E402


def sha256_hex(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


class TagNormalisation(unittest.TestCase):
    def test_bare_version(self):
        self.assertEqual(release_ci.cmd_tag("1.3.20"), 0)

    def test_v_prefixed(self):
        self.assertEqual(release_ci.cmd_tag("v1.3.20"), 0)

    def test_output_lines(self):
        out = io.StringIO()
        old = sys.stdout
        sys.stdout = out
        try:
            code = release_ci.cmd_tag("v1.3.20")
        finally:
            sys.stdout = old
        self.assertEqual(code, 0)
        self.assertIn("tag=v1.3.20", out.getvalue())
        self.assertIn("version=1.3.20", out.getvalue())

    def test_double_v_is_refused(self):
        self.assertNotEqual(release_ci.cmd_tag("vv1.3.20"), 0)

    def test_two_part_version_is_refused(self):
        self.assertNotEqual(release_ci.cmd_tag("1.3"), 0)

    def test_suffix_is_refused(self):
        self.assertNotEqual(release_ci.cmd_tag("1.3.20-rc1"), 0)


class CompileLineContract(unittest.TestCase):
    BASE = (
        '@echo off\r\n'
        'setlocal\r\n'
        'if exist "C:\\legacy\\vcvars64.bat" (\r\n'
        '    set "VS=C:\\legacy\\vcvars64.bat"\r\n'
        ') else (\r\n'
        '    echo ERROR: Visual Studio not found\r\n'
        '    exit /b 1\r\n'
        ')\r\n'
        'call "%VS%" >nul\r\n'
        'set "SOURCE=%~dp0..\\plugin\\ModuleMain.cpp"\r\n'
        'if /I "%~1"=="dev" (\r\n'
        '    set "FLAGS="\r\n'
        '    set "OUTPUT=BloodPactPlugin_rel.dll"\r\n'
        '    set "OBJDIR=obj_dev"\r\n'
        ') else (\r\n'
        '    set "FLAGS=/DFORGEPACT_RELEASE"\r\n'
        '    set "OUTPUT=BloodPactPlugin_ship.dll"\r\n'
        '    set "OBJDIR=obj_ship"\r\n'
        ')\r\n'
        'cl /nologo /std:c++20 %FLAGS% "%SOURCE%" /Fe:%OUTPUT% /Fo:%OBJDIR%\\ /link\r\n'
    )

    def _write(self, tmp: Path, name: str, text: str) -> Path:
        p = tmp / name
        p.write_bytes(text.encode("utf-8"))
        return p

    def test_identical_files_agree(self):
        with TemporaryDirectory() as d:
            tmp = Path(d)
            a = self._write(tmp, "a.bat", self.BASE)
            b = self._write(tmp, "b.bat", self.BASE)
            self.assertEqual(release_ci.cmd_compile_line(a, b), 0)

    def test_a_changed_cl_line_is_refused(self):
        with TemporaryDirectory() as d:
            tmp = Path(d)
            a = self._write(tmp, "a.bat", self.BASE)
            changed = self.BASE.replace("/std:c++20", "/std:c++17")
            b = self._write(tmp, "b.bat", changed)
            self.assertNotEqual(release_ci.cmd_compile_line(a, b), 0)

    def test_a_changed_flags_line_is_refused(self):
        with TemporaryDirectory() as d:
            tmp = Path(d)
            a = self._write(tmp, "a.bat", self.BASE)
            changed = self.BASE.replace(
                'set "FLAGS=/DFORGEPACT_RELEASE"', 'set "FLAGS=/DFORGEPACT_RELEASE /DEXTRA"'
            )
            b = self._write(tmp, "b.bat", changed)
            self.assertNotEqual(release_ci.cmd_compile_line(a, b), 0)

    def test_different_discovery_same_compile_line_agrees(self):
        with TemporaryDirectory() as d:
            tmp = Path(d)
            a = self._write(tmp, "a.bat", self.BASE)
            rewritten_discovery = self.BASE.replace(
                'if exist "C:\\legacy\\vcvars64.bat" (\r\n'
                '    set "VS=C:\\legacy\\vcvars64.bat"\r\n'
                ') else (\r\n'
                '    echo ERROR: Visual Studio not found\r\n'
                '    exit /b 1\r\n'
                ')\r\n',
                'if defined VSCMD_VER (\r\n'
                '    goto :have_vs\r\n'
                ')\r\n'
                'set "VS=C:\\different\\path\\vcvars64.bat"\r\n'
                ':have_vs\r\n',
            )
            b = self._write(tmp, "b.bat", rewritten_discovery)
            self.assertEqual(release_ci.cmd_compile_line(a, b), 0)


class Package(unittest.TestCase):
    def _fake_dist(self, root: Path, version: str = "1.3.20") -> None:
        dist = root / "dist" / "ForgePact"
        modfiles = dist / "modfiles"
        modfiles.mkdir(parents=True)
        (dist / "ForgePact.exe").write_bytes(b"fake exe bytes")
        (dist / "README.md").write_bytes(b"readme")
        for name, content in [
            ("AurieCore.dll", b"aurie core"),
            ("AuriePatcher.exe", b"aurie patcher"),
            ("YYToolkit.dll", b"yytoolkit"),
            ("BloodPactPlugin.dll", b"plugin bytes"),
        ]:
            (modfiles / name).write_bytes(content)

        src = root / "src"
        src.mkdir(parents=True)
        (src / "forgepact.py").write_text(
            f'__version__ = "{version}"\n', encoding="utf-8"
        )

    def _fake_pins(self, root: Path) -> None:
        tools = root / "tools"
        tools.mkdir(parents=True, exist_ok=True)
        (tools / "toolchain-pins.json").write_text(
            json.dumps({"files": [{"dest": "modfiles_shipped/YYToolkit.dll",
                                    "url": "https://example.invalid/x",
                                    "sha256": "0" * 64,
                                    "provenance": "test fixture"}]}),
            encoding="utf-8",
        )

    def test_package_layout_and_sums(self):
        with TemporaryDirectory() as d:
            root = Path(d) / "ForgePact"
            self._fake_dist(root)
            self._fake_pins(root)
            out = Path(d) / "out"

            code = release_ci.cmd_package(
                root, "1.3.20", out, ["source_commit=deadbeef", "tag=v1.3.20"]
            )
            self.assertEqual(code, 0)

            zip_path = out / "ForgePact-1.3.20.zip"
            sha_path = out / "ForgePact-1.3.20.zip.sha256"
            self.assertTrue(zip_path.is_file())
            self.assertTrue(sha_path.is_file())

            with zipfile.ZipFile(zip_path) as zf:
                names = zf.namelist()
                roots = {n.split("/", 1)[0] for n in names}
                self.assertEqual(roots, {"ForgePact-1.3.20"})

                sums = zf.read("ForgePact-1.3.20/SHA256SUMS.txt").decode("utf-8")
                exe_hash = sha256_hex(b"fake exe bytes")
                self.assertIn(f"{exe_hash}  ForgePact.exe", sums)
                plugin_hash = sha256_hex(b"plugin bytes")
                self.assertIn(f"{plugin_hash}  modfiles/BloodPactPlugin.dll", sums)
                # SHA256SUMS.txt lists neither itself nor BUILD-INFO.json.
                self.assertNotIn("SHA256SUMS.txt", sums)
                self.assertNotIn("BUILD-INFO.json", sums)

                info = json.loads(zf.read("ForgePact-1.3.20/BUILD-INFO.json"))
                self.assertIs(info["live_gameplay_verified"], False)
                self.assertEqual(info["source_commit"], "deadbeef")
                self.assertEqual(info["tag"], "v1.3.20")
                self.assertEqual(info["version"], "1.3.20")
                self.assertEqual(info["plugin_sha256"], plugin_hash)
                self.assertEqual(info["exe_sha256"], exe_hash)

            zip_hash = sha256_hex(zip_path.read_bytes())
            self.assertEqual(
                sha_path.read_text(encoding="utf-8"),
                f"{zip_hash}  ForgePact-1.3.20.zip\n",
            )

    def test_missing_modfile_exits_1_and_writes_no_zip(self):
        with TemporaryDirectory() as d:
            root = Path(d) / "ForgePact"
            self._fake_dist(root)
            self._fake_pins(root)
            (root / "dist" / "ForgePact" / "modfiles" / "YYToolkit.dll").unlink()
            out = Path(d) / "out"

            code = release_ci.cmd_package(root, "1.3.20", out, [])
            self.assertNotEqual(code, 0)
            self.assertFalse((out / "ForgePact-1.3.20.zip").exists())

    def test_version_mismatch_exits_1(self):
        with TemporaryDirectory() as d:
            root = Path(d) / "ForgePact"
            self._fake_dist(root, version="1.3.19")
            self._fake_pins(root)
            out = Path(d) / "out"

            code = release_ci.cmd_package(root, "1.3.20", out, [])
            self.assertNotEqual(code, 0)
            self.assertFalse((out / "ForgePact-1.3.20.zip").exists())


class CliSmoke(unittest.TestCase):
    """The argparse wiring itself, run as a subprocess like CI does."""

    def test_tag_via_cli(self):
        r = subprocess.run(
            [sys.executable, str(REPO_ROOT / "tools" / "release_ci.py"), "tag", "--tag", "1.3.20"],
            capture_output=True, text=True,
        )
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn("tag=v1.3.20", r.stdout)
        self.assertIn("version=1.3.20", r.stdout)


if __name__ == "__main__":
    unittest.main()
