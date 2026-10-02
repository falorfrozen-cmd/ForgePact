"""The release PDB changes no instruction: evidence for issue #76's symbols step.

forgepact-release.yml's compile step sets `CL` and `_LINK_` so the tag's
build writes `plugin_build/BloodPactPlugin_ship.pdb` beside the DLL, which the
workflow keeps as an artifact (test_forgepact_release_workflow.py,
ThePluginSymbolsAreKeptBesideTheZip). That is only safe if the options change
nothing players run. This compiles tests/pdb_codegen_probe.cpp with
build.bat's own compile options, read from its `cl ` line, twice: plain, and
under exactly the `CL`/`_LINK_` values read from the workflow file. Then:

- the compiler's code is the same: every `.text` COFF section of the two
  probe.obj files is byte-identical, with relocations at the same offsets and
  of the same types (symbol indices may differ, since /Zi adds symbols);
- the linked DLLs' `.text` sits at the same address with the same size, so
  every function keeps its offset and the PDB maps a crash offset correctly;
- a PDB exists only after the second, and the DLL names it by /PDBALTPATH.

The linked `.text` is not byte-identical, and this does not claim it is:
/DEBUG grows the debug directory inside `.rdata`, which moves the data after
it, so the RIP-relative displacements that point there change (on this probe,
14 bytes in 7 fields). The shipped DLL's `.rdata` layout differs from a plain
build's; its instructions and function addresses do not.

Instrument check: a third compile turns optimisation off, and both readers
must see it, so a comparison that reads the same bytes twice, or reads no
code at all, cannot pass. What this proves is about this probe on this
toolchain; the whole plugin is confirmed by a dry-run release build.

Writes only under build/release-pdb-symbols. Skips, saying why, where there
is no MSVC.
"""
import os
import shlex
import shutil
import struct
import subprocess
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from test_forgepact_release_workflow import compile_step_env  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "tests" / "pdb_codegen_probe.cpp"
BUILD_BAT = ROOT / "plugin_build" / "build.bat"
OUT = ROOT / "build" / "release-pdb-symbols"


def build_bat_options() -> tuple[list[str], list[str]]:
    """(compile options, link options) from build.bat's one `cl ` line.

    The compile options are those before `%FLAGS%` plus the release build's
    FLAGS; the link options are those after `/link`. Sources, include paths
    and output names are the probe's own.
    """
    text = BUILD_BAT.read_bytes().decode("utf-8")
    line = next(line.strip() for line in text.splitlines() if line.strip().startswith("cl "))
    words = line.split()
    compile_options = words[1:words.index("%FLAGS%")] + ["/DFORGEPACT_RELEASE"]
    link_options = words[words.index("/link") + 1:]
    return compile_options, link_options


def pe_sections(data: bytes) -> dict:
    """{section name: (virtual address, bytes up to its virtual size)}."""
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise AssertionError("not a PE image")
    count = struct.unpack_from("<H", data, pe + 6)[0]
    optional = struct.unpack_from("<H", data, pe + 20)[0]
    table = pe + 24 + optional
    sections = {}
    for index in range(count):
        at = table + 40 * index
        name = data[at:at + 8].rstrip(b"\0").decode("ascii", "replace")
        virtual_size, address, raw_size, raw_at = struct.unpack_from("<IIII", data, at + 8)
        sections[name] = (address, data[raw_at:raw_at + min(virtual_size, raw_size)])
    return sections


BIGOBJ_CLASS_ID = bytes.fromhex("c7a1bad1eebaa94baf20faf66aa4dcb8")
NRELOC_OVFL = 0x01000000


def coff_code_sections(data: bytes) -> list:
    """[(name, raw bytes, [(relocation offset, type), ...])] for each `.text*`
    section of a /bigobj COFF object, in the object's order.

    build.bat compiles with /bigobj, so the header is ANON_OBJECT_HEADER_BIGOBJ
    (56 bytes, a 32-bit section count) and the 40-byte section headers follow
    it. A relocation's symbol index is left out: /Zi adds symbols to the
    table, which renumbers it without changing what the code refers to.
    """
    sig1, sig2, version = struct.unpack_from("<HHH", data, 0)
    if (sig1, sig2) != (0, 0xFFFF) or version < 2 or data[12:28] != BIGOBJ_CLASS_ID:
        raise AssertionError("not a /bigobj COFF object")
    count = struct.unpack_from("<I", data, 44)[0]
    sections = []
    for index in range(count):
        at = 56 + 40 * index
        name = data[at:at + 8].rstrip(b"\0").decode("ascii", "replace")
        if not name.startswith(".text"):
            continue
        raw_size, raw_at, relocs_at = struct.unpack_from("<III", data, at + 16)
        reloc_count = struct.unpack_from("<H", data, at + 32)[0]
        flags = struct.unpack_from("<I", data, at + 36)[0]
        if flags & NRELOC_OVFL and reloc_count == 0xFFFF:
            # The real count sits in the first entry, which is not a relocation.
            reloc_count = struct.unpack_from("<I", data, relocs_at)[0] - 1
            relocs_at += 10
        relocations = [struct.unpack_from("<I", data, relocs_at + 10 * r)[0:1]
                       + struct.unpack_from("<H", data, relocs_at + 10 * r + 8)
                       for r in range(reloc_count)]
        sections.append((name, data[raw_at:raw_at + raw_size], relocations))
    return sections


@unittest.skipUnless(os.name == "nt", "the release build is an MSVC build on Windows")
class TheReleasePdbChangesNoCode(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        vswhere = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
        if not vswhere.is_file():
            raise unittest.SkipTest("no vswhere.exe: the Visual Studio C++ compiler is required to compile the PDB probe")
        install = subprocess.check_output(
            [str(vswhere), "-latest", "-products", "*", "-requires",
             "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"],
            text=True).strip()
        if not install:
            raise unittest.SkipTest("the Visual Studio C++ toolchain is not installed: the PDB probe cannot be compiled")
        cls.vcvars = Path(install) / "VC/Auxiliary/Build/vcvars64.bat"
        cls.env = compile_step_env()
        cls.compile_options, cls.link_options = build_bat_options()
        if OUT.exists():
            shutil.rmtree(OUT)
        cls.plain = cls.compile("plain")
        cls.symbols = cls.compile("symbols", cl=cls.env.get("CL", ""), link=cls.env.get("_LINK_", ""))
        cls.control = cls.compile("unoptimised", extra=["/Od"])

    @classmethod
    def compile(cls, label, cl="", link="", extra=()):
        out = OUT / label
        out.mkdir(parents=True)
        batch = out / "compile.cmd"
        # CL and _LINK_ are set (or cleared) after vcvars, so a developer
        # shell that carries its own cannot leak into the plain build.
        # `extra` comes after build.bat's options, so /Od wins over its /O2.
        batch.write_text(
            f'@echo off\ncall "{cls.vcvars}" >nul\nif errorlevel 1 exit /b 1\n'
            f'set "CL={cl}"\nset "_LINK_={link}"\n'
            f'cl {" ".join(cls.compile_options + list(extra))} "{PROBE}" /Fe:probe.dll /Fo:probe.obj '
            f'/link {" ".join(cls.link_options)}\n'
            f'exit /b %errorlevel%\n', encoding="utf-8")
        result = subprocess.run(["cmd", "/d", "/c", str(batch)], cwd=out, capture_output=True, text=True,
                                encoding="utf-8", errors="replace")
        (out / "compile.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        if result.returncode:
            raise AssertionError(f"{label}: {result.stdout}{result.stderr}")
        return out

    def text_section(self, out):
        sections = pe_sections((out / "probe.dll").read_bytes())
        self.assertIn(".text", sections, f"{out.name}: no .text section")
        return sections[".text"]

    def test_the_workflow_values_are_what_is_compiled_under(self):
        # Positive control on the reader: an empty env would make the second
        # build the first one again, and the comparison below meaningless.
        self.assertIn("/Zi", shlex.split(self.env["CL"]))
        self.assertIn("/DEBUG:FULL", self.env["_LINK_"].split())

    def code_sections(self, out):
        return coff_code_sections((out / "probe.obj").read_bytes())

    def test_the_code_is_byte_identical(self):
        # (a) What the compiler emitted: the same code and the same fixups.
        plain = self.code_sections(self.plain)
        symbols = self.code_sections(self.symbols)
        self.assertIn(".text$mn", [name for name, _, _ in plain], "probe.obj has no .text$mn section")
        self.assertGreater(sum(len(raw) for _, raw, _ in plain), 4096,
                           "the probe compiled to almost no code; nothing was compared")
        self.assertEqual([name for name, _, _ in plain], [name for name, _, _ in symbols],
                         "the code sections differ in number or order")
        for index, ((name, raw, relocs), (_, other_raw, other_relocs)) in enumerate(zip(plain, symbols)):
            self.assertEqual(raw, other_raw, f"code section {index} ({name}) differs")
            self.assertEqual(relocs, other_relocs, f"code section {index} ({name}) has other relocations")

    def test_every_function_keeps_its_address(self):
        # (b) What link laid out: .text at the same place and of the same size,
        # so an offset in a crash report means the same function in either.
        plain_at, plain = self.text_section(self.plain)
        symbols_at, symbols = self.text_section(self.symbols)
        self.assertGreater(len(plain), 4096, "the linked probe has almost no code; nothing was compared")
        self.assertEqual(plain_at, symbols_at, ".text moved")
        self.assertEqual(len(plain), len(symbols), ".text changed size")

    def test_the_comparison_sees_a_code_change(self):
        # Negative control: /Od changes the code, and both readers say so.
        plain = self.code_sections(self.plain)
        control = self.code_sections(self.control)
        self.assertNotEqual([raw for _, raw, _ in plain], [raw for _, raw, _ in control])
        _, linked_plain = self.text_section(self.plain)
        _, linked_control = self.text_section(self.control)
        self.assertNotEqual(len(linked_plain), len(linked_control))

    def test_only_the_symbols_build_points_at_a_pdb(self):
        # The symbols build carries a CodeView record the plain one has not, so
        # a pass above compared the two links it claims to. (The files differ
        # anyway, by their link timestamp, which is why this reads the record.)
        self.assertNotIn(b"RSDS", (self.plain / "probe.dll").read_bytes())
        self.assertIn(b"RSDS", (self.symbols / "probe.dll").read_bytes())

    def test_only_the_symbols_build_writes_a_pdb(self):
        self.assertEqual(sorted(p.name for p in self.plain.glob("*.pdb")), [])
        self.assertTrue((self.symbols / "probe.pdb").is_file(), "link wrote no probe.pdb")

    def test_the_dll_names_its_pdb_without_the_build_path(self):
        alt = next(o for o in self.env["_LINK_"].split() if o.startswith("/PDBALTPATH:")).split(":", 1)[1]
        image = (self.symbols / "probe.dll").read_bytes()
        record = image.find(b"RSDS")
        self.assertNotEqual(record, -1, "no CodeView record: the DLL does not point at a PDB")
        name = image[record + 24:image.index(b"\0", record + 24)].decode("utf-8", "replace")
        self.assertEqual(name, alt)
        self.assertNotIn(str(OUT).encode("utf-8").lower(), image.lower())


if __name__ == "__main__":
    unittest.main()
