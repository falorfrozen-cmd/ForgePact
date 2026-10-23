"""The Mark 1 pieces' icon generator, run against a temporary folder.

ForgePact #166: assets/mark1/make_mark1_icons.py embeds the owner's PNGs for
the four Mark 1 pieces in plugin/include/ForgePact/Mark1Icons.hpp, by the
Flight charms' route. Unlike that generator, a missing PNG is not an error:
the header embeds what is present and listed in manifest.json, and with no
PNG it has no icon and a zero count, and still compiles (the baseline, which
is what ships until the owner's art is in). These tests never read or write
the shipped folder: every case builds its own folder and synthetic PNGs.

Test names carry `no_png` for the baseline (no art: a zero count and a header
that compiles, with MSVC as the other harness tests are, skipped without a
toolchain), `refused` for each input `--check` must refuse, `embedded` for one
valid synthetic pair, and `stale` for a header that no longer matches.
"""
import hashlib
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "assets/mark1/make_mark1_icons.py"

COMPILE_PROBE = r"""
#include <ForgePact/Mark1Icons.hpp>
#include <cstdio>
#include <cstring>

// The plugin's shape: per-icon state sized by kMaxIcons, a loop to kIconCount.
static bool g_loaded[ForgePact::Mark1Icons::kMaxIcons] = {};

int main() {
    size_t bag = 0, ground = 0, bytes = 0;
    for (size_t k = 0; k < ForgePact::Mark1Icons::kIconCount; ++k) {
        const auto& icon = ForgePact::Mark1Icons::kIcons[k];
        if (icon.ground) ++ground; else ++bag;
        bytes += icon.size;
        g_loaded[k] = std::strlen(icon.file) > 0 && icon.width > 0 && icon.height > 0;
    }
    std::printf("count=%zu max=%zu bag=%zu ground=%zu bytes=%zu first=%d\n",
                ForgePact::Mark1Icons::kIconCount, ForgePact::Mark1Icons::kMaxIcons, bag, ground, bytes,
                g_loaded[0] ? 1 : 0);
    return 0;
}
"""


def filtered(kind, line, prior, bpp):
    """Encode one scanline with PNG row filter `kind` (0 none .. 4 Paeth)."""
    out = bytearray([kind])
    for i, value in enumerate(line):
        left = line[i - bpp] if i >= bpp else 0
        up = prior[i]
        corner = prior[i - bpp] if i >= bpp else 0
        if kind == 4:
            p = left + up - corner
            pa, pb, pc = abs(p - left), abs(p - up), abs(p - corner)
            guess = left if pa <= pb and pa <= pc else (up if pb <= pc else corner)
        else:
            guess = (0, left, up, (left + up) // 2)[kind]
        out.append((value - guess) & 0xFF)
    return bytes(out)


def png(width, height, *, depth=8, colour=6, alpha=255, filters=(0,), partial_at=None):
    """A synthetic PNG: a filled square with a one-pixel transparent border.

    Rows cycle through `filters`, as a real encoder's do; `partial_at` (x, y)
    gives that one pixel alpha 128."""
    channels = {6: 4, 2: 3}[colour]
    sample = depth // 8
    rows = []
    prior = bytes(width * channels * sample)
    for y in range(height):
        line = bytearray()
        for x in range(width):
            edge = x in (0, width - 1) or y in (0, height - 1)
            a = 128 if (x, y) == partial_at else (0 if edge else alpha)
            pixel = [(x * 7) & 0xFF, (y * 13) & 0xFF, 40, a][:channels]
            if channels == 3:
                pixel = [0, 0, 0] if edge else pixel
            for value in pixel:
                line += value.to_bytes(sample, "big")
        rows.append(filtered(filters[y % len(filters)], line, prior, channels * sample))
        prior = bytes(line)

    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", width, height, depth, colour, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(b"".join(rows)))
            + chunk(b"IEND", b""))


class Mark1IconGeneratorTests(unittest.TestCase):
    def setUp(self):
        self.dir = Path(tempfile.mkdtemp(prefix="mark1-icons-"))
        self.addCleanup(shutil.rmtree, self.dir, ignore_errors=True)
        self.assets = self.dir / "assets"
        self.assets.mkdir()
        self.header = self.dir / "Mark1Icons.hpp"
        self.write_manifest([])

    def write_manifest(self, names, *, sha=None):
        assets = []
        for name in names:
            data = (self.assets / name).read_bytes()
            width, height = struct.unpack(">II", data[16:24])
            assets.append({"file": name, "width": width, "height": height,
                           "sha256": sha or hashlib.sha256(data).hexdigest()})
        (self.assets / "manifest.json").write_text(json.dumps({"schema": 1, "assets": assets}), encoding="utf-8")

    def add(self, name, data, *, listed=True):
        (self.assets / name).write_bytes(data)
        if listed:
            current = json.loads((self.assets / "manifest.json").read_text(encoding="utf-8"))
            self.write_manifest([entry["file"] for entry in current["assets"]] + [name])

    def run_script(self, *flags):
        return subprocess.run([sys.executable, str(SCRIPT), "--assets", str(self.assets), "--header", str(self.header),
                               *flags], capture_output=True, text=True, encoding="utf-8", errors="replace")

    def generate(self):
        result = self.run_script()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return self.header.read_text(encoding="utf-8")

    def assertRefused(self, *needles):
        result = self.run_script("--check")
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        for needle in needles:
            self.assertIn(needle, result.stderr)

    def compile_and_run(self):
        """Compile COMPILE_PROBE against self.header and answer its output line."""
        include = self.dir / "include"
        (include / "ForgePact").mkdir(parents=True, exist_ok=True)
        shutil.copyfile(self.header, include / "ForgePact" / "Mark1Icons.hpp")
        out = self.dir / "build"
        out.mkdir(exist_ok=True)
        cpp = out / "mark1icons.cpp"
        cpp.write_text(COMPILE_PROBE, encoding="utf-8")
        binary = out / ("mark1icons.exe" if os.name == "nt" else "mark1icons")
        if os.name == "nt":
            vswhere = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
            if not vswhere.is_file():
                self.skipTest("Visual Studio C++ compiler is required for the header compile test")
            install = subprocess.check_output(
                [str(vswhere), "-latest", "-products", "*", "-requires",
                 "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"],
                text=True).strip()
            if not install:
                self.skipTest("Visual Studio C++ toolchain not installed")
            vcvars = Path(install) / "VC/Auxiliary/Build/vcvars64.bat"
            batch = out / "compile.cmd"
            batch.write_text(
                f'@echo off\ncall "{vcvars}" >nul\nif errorlevel 1 exit /b 1\n'
                f'cl /nologo /std:c++20 /EHsc /W4 /WX /I "{include}" "{cpp}" /Fe:"{binary}" /Fo:"{out / "mark1icons.obj"}"\n'
                f'exit /b %errorlevel%\n', encoding="utf-8")
            command = ["cmd", "/d", "/c", str(batch)]
        else:
            compiler = shutil.which("c++")
            if not compiler:
                self.skipTest("A C++20 compiler is required for the header compile test")
            command = [compiler, "-std=c++20", "-Wall", "-Werror", "-I", str(include), str(cpp), "-o", str(binary)]
        result = subprocess.run(command, cwd=out, capture_output=True, text=True, encoding="utf-8", errors="replace")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        run = subprocess.run([str(binary)], capture_output=True, text=True, encoding="utf-8", errors="replace")
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        return run.stdout.strip()

    # ---- the baseline: no PNG at all ---------------------------------------

    def test_no_png_writes_a_header_with_no_icon_and_a_zero_count(self):
        text = self.generate()
        self.assertIn("do not edit by hand", text)
        self.assertIn("inline constexpr size_t kIconCount = 0;", text)
        self.assertIn("inline constexpr size_t kMaxIcons = 8;", text)
        self.assertIn("inline constexpr const Icon* kIcons = nullptr;", text)
        self.assertNotIn("unsigned char k_", text)
        result = self.run_script("--check")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("OK 0 of 8 icons", result.stdout)

    def test_no_png_header_compiles(self):
        self.generate()
        self.assertEqual(self.compile_and_run(), "count=0 max=8 bag=0 ground=0 bytes=0 first=0")

    def test_no_png_write_manifest_lists_nothing(self):
        result = self.run_script("--write-manifest")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(json.loads((self.assets / "manifest.json").read_text(encoding="utf-8"))["assets"], [])

    # ---- what --check refuses ----------------------------------------------

    def test_refused_wrong_bag_size(self):
        self.add("mark1_helmet.png", png(64, 96))
        self.assertRefused("mark1_helmet.png: is 64x96, expected 64x64")

    def test_refused_wrong_body_armor_bag_size(self):
        self.add("mark1_body_armor.png", png(64, 64))
        self.assertRefused("mark1_body_armor.png: is 64x64, expected 64x96")

    def test_refused_non_rgba(self):
        self.add("mark1_boots.png", png(64, 64, colour=2))
        self.assertRefused("mark1_boots.png: is depth 8 colour type 2, expected 8-bit RGBA")

    def test_refused_non_8_bit(self):
        self.add("mark1_gloves_ground.png", png(24, 24, depth=16))
        self.assertRefused("mark1_gloves_ground.png: is depth 16 colour type 6, expected 8-bit RGBA")

    def test_refused_partial_alpha_pixel(self):
        self.add("mark1_helmet_ground.png", png(24, 24, alpha=128))
        self.assertRefused("mark1_helmet_ground.png: has partial-alpha pixels (alpha 128)")

    def test_refused_partial_alpha_pixel_under_every_row_filter(self):
        # A real encoder mixes the five row filters; the check must undo each.
        # The control: the same art with no partial pixel passes.
        self.add("mark1_gloves.png", png(64, 64, filters=(0, 1, 2, 3, 4)))
        result = self.run_script("--check")
        self.assertNotIn("mark1_gloves.png", result.stderr)
        self.add("mark1_boots.png", png(64, 64, filters=(0, 1, 2, 3, 4), partial_at=(40, 33)))
        self.assertRefused("mark1_boots.png: has partial-alpha pixels (alpha 128)")
        self.assertNotIn("mark1_gloves.png", self.run_script("--check").stderr)

    def test_refused_manifest_hash_mismatch(self):
        self.add("mark1_helmet.png", png(64, 64), listed=False)
        self.write_manifest(["mark1_helmet.png"], sha="0" * 64)
        self.assertRefused("mark1_helmet.png: sha256", "does not match manifest.json's " + "0" * 64)

    def test_refused_png_not_in_manifest(self):
        self.add("mark1_helmet.png", png(64, 64), listed=False)
        self.assertRefused("mark1_helmet.png: present but not in manifest.json")

    def test_refused_manifest_lists_a_missing_png(self):
        (self.assets / "manifest.json").write_text(json.dumps({"schema": 1, "assets": [
            {"file": "mark1_boots.png", "width": 64, "height": 64, "sha256": "0" * 64}]}), encoding="utf-8")
        self.assertRefused("mark1_boots.png: listed in manifest.json but not present")

    def test_refused_unexpected_file_name(self):
        self.add("helmet.png", png(64, 64), listed=False)
        self.assertRefused("helmet.png: not one of the eight expected names")

    def test_refused_inputs_write_no_header(self):
        self.add("mark1_helmet.png", png(32, 32))
        result = self.run_script()
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertFalse(self.header.exists())

    # ---- one valid synthetic pair ------------------------------------------

    def test_embedded_one_valid_pair(self):
        bag, ground = png(64, 64), png(24, 24)
        self.add("mark1_helmet.png", bag)
        self.add("mark1_helmet_ground.png", ground)
        text = self.generate()
        self.assertIn('{ "mark1_helmet.png", "helmet", false, 64, 64, k_mark1_helmet, sizeof(k_mark1_helmet) },', text)
        self.assertIn('{ "mark1_helmet_ground.png", "helmet", true, 24, 24, k_mark1_helmet_ground, '
                      'sizeof(k_mark1_helmet_ground) },', text)
        self.assertIn("inline constexpr const Icon* kIcons = kIconTable;", text)
        self.assertNotIn("nullptr", text)
        self.assertIn(", ".join(f"0x{b:02x}" for b in bag[:16]), text)
        result = self.run_script("--check")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("OK 2 of 8 icons", result.stdout)
        self.assertEqual(self.compile_and_run(),
                         f"count=2 max=8 bag=1 ground=1 bytes={len(bag) + len(ground)} first=1")

    def test_embedded_ground_size_is_read_from_the_file(self):
        self.add("mark1_body_armor_ground.png", png(24, 32))
        self.assertIn('{ "mark1_body_armor_ground.png", "body", true, 24, 32,', self.generate())

    def test_embedded_write_manifest_records_the_pngs_present(self):
        bag = png(64, 96)
        (self.assets / "mark1_body_armor.png").write_bytes(bag)
        result = self.run_script("--write-manifest")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        entries = json.loads((self.assets / "manifest.json").read_text(encoding="utf-8"))["assets"]
        self.assertEqual([(e["file"], e["width"], e["height"], e["sha256"]) for e in entries],
                         [("mark1_body_armor.png", 64, 96, hashlib.sha256(bag).hexdigest())])
        self.assertIn('"mark1_body_armor.png", "body", false, 64, 96,', self.generate())

    # ---- a header that no longer matches -----------------------------------

    def test_stale_header_after_a_png_is_added(self):
        self.generate()
        self.add("mark1_gloves.png", png(64, 64))
        result = self.run_script("--check")
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("Mark1Icons.hpp is stale", result.stderr)

    def test_stale_header_edited_by_hand(self):
        text = self.generate()
        self.header.write_text(text.replace("kMaxIcons = 8", "kMaxIcons = 9"), encoding="utf-8", newline="\n")
        self.assertRefused("Mark1Icons.hpp is stale")

    def test_stale_check_ignores_crlf_line_endings(self):
        text = self.generate()
        self.header.write_bytes(text.replace("\n", "\r\n").encode("utf-8"))
        result = self.run_script("--check")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_stale_missing_header(self):
        self.assertRefused("Mark1Icons.hpp: unreadable")


if __name__ == "__main__":
    unittest.main()
