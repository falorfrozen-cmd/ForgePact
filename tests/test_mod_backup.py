import importlib.util
import struct
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock


MODULE_PATH = Path(__file__).resolve().parents[1] / "src" / "forgepact.py"
SPEC = importlib.util.spec_from_file_location("forgepact_under_test", MODULE_PATH)
forgepact = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(forgepact)


FILE_ALIGNMENT = 0x200
SECTION_ALIGNMENT = 0x1000


def _aligned(value, alignment):
    return (value + alignment - 1) // alignment * alignment


def mapped_aurie_payload(old_entry_point: int) -> bytes:
    """Build a tiny mapped PE image exporting Aurie's stored g_OldOEP."""
    image = bytearray(0x200)
    pe_offset = 0x40
    optional_size = 0xF0
    optional_offset = pe_offset + 24
    export_rva = 0x150
    functions_rva = 0x178
    names_rva = 0x17C
    ordinals_rva = 0x180
    name_rva = 0x188
    value_rva = 0x1C0

    image[:2] = b"MZ"
    struct.pack_into("<I", image, 0x3C, pe_offset)
    image[pe_offset:pe_offset + 4] = b"PE\0\0"
    struct.pack_into(
        "<HHIIIHH", image, pe_offset + 4, 0x8664, 0, 0, 0, 0, optional_size, 0x22
    )
    struct.pack_into("<H", image, optional_offset, 0x20B)
    struct.pack_into("<I", image, optional_offset + 108, 16)
    struct.pack_into("<II", image, optional_offset + 112, export_rva, 0xB0)
    struct.pack_into("<II", image, export_rva + 20, 1, 1)
    struct.pack_into(
        "<III", image, export_rva + 28, functions_rva, names_rva, ordinals_rva
    )
    struct.pack_into("<I", image, functions_rva, value_rva)
    struct.pack_into("<I", image, names_rva, name_rva)
    struct.pack_into("<H", image, ordinals_rva, 0)
    image[name_rva:name_rva + len(b"g_OldOEP\0")] = b"g_OldOEP\0"
    struct.pack_into("<I", image, value_rva, old_entry_point)
    return bytes(image)


def write_test_pe(path: Path, build: bytes, patched: bool = False) -> bytes:
    """Write a small valid PE whose original sections identify ``build``."""
    sections = [
        (b".text", (b"CODE-" + build) * 20, 0x60000020),
        (b".rdata", (b"DATA-" + build) * 20, 0x40000040),
    ]
    if patched:
        sections.append((b".aurie", mapped_aurie_payload(0x1000), 0xE0000000))

    pe_offset = 0x80
    optional_size = 0xF0
    header_size = FILE_ALIGNMENT
    image = bytearray(header_size)
    image[:2] = b"MZ"
    struct.pack_into("<I", image, 0x3C, pe_offset)
    image[pe_offset:pe_offset + 4] = b"PE\0\0"
    coff_offset = pe_offset + 4
    struct.pack_into("<HHIIIHH", image, coff_offset, 0x8664, len(sections), 0, 0, 0,
                     optional_size, 0x22)
    optional_offset = coff_offset + 20
    struct.pack_into("<H", image, optional_offset, 0x20B)
    struct.pack_into("<I", image, optional_offset + 16,
                     0x3000 if patched else 0x1000)
    struct.pack_into("<I", image, optional_offset + 32, SECTION_ALIGNMENT)
    struct.pack_into("<I", image, optional_offset + 36, FILE_ALIGNMENT)
    struct.pack_into("<I", image, optional_offset + 56,
                     _aligned((len(sections) + 1) * SECTION_ALIGNMENT, SECTION_ALIGNMENT))
    struct.pack_into("<I", image, optional_offset + 60, header_size)

    section_table = optional_offset + optional_size
    raw_offset = header_size
    virtual_address = SECTION_ALIGNMENT
    for index, (name, content, characteristics) in enumerate(sections):
        raw_size = _aligned(len(content), FILE_ALIGNMENT)
        entry_offset = section_table + index * 40
        struct.pack_into("<8sIIIIIIHHI", image, entry_offset, name.ljust(8, b"\0"),
                         len(content), virtual_address, raw_size, raw_offset,
                         0, 0, 0, 0, characteristics)
        image.extend(content)
        image.extend(b"\0" * (raw_size - len(content)))
        raw_offset += raw_size
        virtual_address += SECTION_ALIGNMENT

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(image)
    return bytes(image)


class ModBackupTests(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        self.root = Path(self.temp_dir.name)
        self.bin_dir = self.root / "game" / "bin"
        self.bin_dir.mkdir(parents=True)
        self.exe = self.bin_dir / "Hero_Siege.exe"
        self.backup = self.bin_dir / "Hero_Siege.exe.aurie_backup"
        self.sources = self.root / "sources"
        self.sources.mkdir()
        for name in ("AurieCore.dll", "YYToolkit.dll", "BloodPactPlugin.dll", "AuriePatcher.exe"):
            (self.sources / name).write_bytes(("source-" + name).encode("ascii"))
        self.cfg = {"game_exe": str(self.exe)}
        self.patchers = [
            mock.patch.object(forgepact, "MODFILE_SOURCES", [self.sources]),
            mock.patch.object(forgepact, "PLUGIN_SOURCES", [self.sources]),
            mock.patch.object(forgepact, "game_running", return_value=False),
        ]
        for patcher in self.patchers:
            patcher.start()

    def tearDown(self):
        for patcher in reversed(self.patchers):
            patcher.stop()
        self.temp_dir.cleanup()

    def _successful_patcher(self, command, **_kwargs):
        exe = Path(command[1])
        # Recreate the same base from the marker embedded in its first section.
        data = exe.read_bytes()
        build = b"new" if b"CODE-new" in data else b"old"
        write_test_pe(exe, build, patched=True)
        self.assertTrue(forgepact._same_aurie_base(exe, self.backup))
        return SimpleNamespace(returncode=0, stdout="patched", stderr="")

    def _install_successfully(self):
        with mock.patch.object(forgepact.subprocess, "run", side_effect=self._successful_patcher):
            return forgepact.op_install_mod(self.cfg)

    def _install_mod_files(self):
        (self.bin_dir / "AurieCore.dll").write_bytes(b"installed")
        aurie = self.bin_dir / "mods" / "aurie"
        aurie.mkdir(parents=True, exist_ok=True)
        (aurie / "YYToolkit.dll").write_bytes(b"installed")
        (aurie / "BloodPactPlugin.dll").write_bytes(b"installed")

    def test_install_archives_stale_backup_and_refreshes_it_before_patching(self):
        write_test_pe(self.exe, b"new")
        old_backup = write_test_pe(self.backup, b"old")

        result = self._install_successfully()

        self.assertIn("ok", result, result)
        self.assertTrue(forgepact.exe_is_patched(self.exe))
        self.assertFalse(forgepact.exe_is_patched(self.backup))
        self.assertTrue(forgepact._same_aurie_base(self.exe, self.backup))
        archived = list(self.bin_dir.glob("Hero_Siege.exe.aurie_backup.stale-*"))
        self.assertEqual(len(archived), 1)
        self.assertEqual(archived[0].read_bytes(), old_backup)
        self.assertIn("backup refreshed for the current build", result["ok"])

    def test_install_refuses_patched_exe_without_backup_before_copying_dlls(self):
        write_test_pe(self.exe, b"new", patched=True)

        result = self._install_successfully()

        self.assertIn("err", result)
        self.assertIn("missing", result["err"])
        self.assertFalse((self.bin_dir / "AurieCore.dll").exists())

    def test_install_refuses_patched_exe_with_backup_from_another_build(self):
        original = write_test_pe(self.exe, b"new", patched=True)
        write_test_pe(self.backup, b"old")

        result = self._install_successfully()

        self.assertIn("err", result)
        self.assertIn("different Hero Siege build", result["err"])
        self.assertEqual(self.exe.read_bytes(), original)
        self.assertFalse((self.bin_dir / "AurieCore.dll").exists())

    def test_failed_patch_restores_the_verified_clean_exe(self):
        clean = write_test_pe(self.exe, b"new")

        def fail_after_mutating(command, **_kwargs):
            Path(command[1]).write_bytes(b"partially-corrupted")
            return SimpleNamespace(returncode=7, stdout="failed", stderr="")

        with mock.patch.object(forgepact.subprocess, "run", side_effect=fail_after_mutating):
            result = forgepact.op_install_mod(self.cfg)

        self.assertIn("err", result)
        self.assertIn("clean exe was restored", result["err"])
        self.assertEqual(self.exe.read_bytes(), clean)
        self.assertEqual(self.backup.read_bytes(), clean)

    def test_remove_does_not_downgrade_an_already_clean_updated_exe(self):
        current = write_test_pe(self.exe, b"new")
        stale = write_test_pe(self.backup, b"old")
        self._install_mod_files()

        result = forgepact.op_remove_mod(self.cfg)

        self.assertIn("ok", result, result)
        self.assertEqual(self.exe.read_bytes(), current)
        self.assertEqual(self.backup.read_bytes(), current)
        archived = list(self.bin_dir.glob("Hero_Siege.exe.aurie_backup.stale-*"))
        self.assertEqual(len(archived), 1)
        self.assertEqual(archived[0].read_bytes(), stale)
        self.assertFalse((self.bin_dir / "AurieCore.dll").exists())

    def test_remove_refuses_backup_from_another_build_without_mutation(self):
        current = write_test_pe(self.exe, b"new", patched=True)
        backup = write_test_pe(self.backup, b"old")
        self._install_mod_files()

        result = forgepact.op_remove_mod(self.cfg)

        self.assertIn("err", result)
        self.assertIn("different Hero Siege build", result["err"])
        self.assertEqual(self.exe.read_bytes(), current)
        self.assertEqual(self.backup.read_bytes(), backup)
        self.assertTrue((self.bin_dir / "AurieCore.dll").exists())

    def test_remove_refuses_same_sections_with_modified_backup_header(self):
        current = write_test_pe(self.exe, b"new", patched=True)
        backup = bytearray(write_test_pe(self.backup, b"new"))
        pe_offset = struct.unpack_from("<I", backup, 0x3C)[0]
        optional_header_offset = pe_offset + 24
        original_entry_point = struct.unpack_from(
            "<I", backup, optional_header_offset + 16
        )[0]
        struct.pack_into(
            "<I", backup, optional_header_offset + 16, original_entry_point + 0x10
        )
        self.backup.write_bytes(backup)
        self._install_mod_files()

        result = forgepact.op_remove_mod(self.cfg)

        self.assertIn("err", result)
        self.assertIn("different Hero Siege build", result["err"])
        self.assertEqual(self.exe.read_bytes(), current)
        self.assertEqual(self.backup.read_bytes(), backup)
        self.assertTrue((self.bin_dir / "AurieCore.dll").exists())

    def test_remove_restores_only_a_matching_clean_backup(self):
        patched = write_test_pe(self.exe, b"new", patched=True)
        clean = write_test_pe(self.backup, b"new")
        self.assertNotEqual(patched, clean)
        self._install_mod_files()

        result = forgepact.op_remove_mod(self.cfg)

        self.assertIn("ok", result, result)
        self.assertEqual(self.exe.read_bytes(), clean)
        self.assertFalse(forgepact.exe_is_patched(self.exe))
        self.assertFalse((self.bin_dir / "AurieCore.dll").exists())
        self.assertFalse((self.bin_dir / "mods" / "aurie" / "YYToolkit.dll").exists())
        self.assertFalse((self.bin_dir / "mods" / "aurie" / "BloodPactPlugin.dll").exists())


if __name__ == "__main__":
    unittest.main()
