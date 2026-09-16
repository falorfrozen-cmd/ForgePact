"""Tests for tools/fetch_toolchain.py -- no network, a fake fetcher stands in
for urllib the way tools/build_catalog.py's GitHubFetcher is substituted in
the hub's own tests.
"""

import hashlib
import io
import json
import sys
import unittest
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

import fetch_toolchain as ft  # noqa: E402


REPO_ROOT = Path(__file__).resolve().parents[1]
PINS_PATH = REPO_ROOT / "tools" / "toolchain-pins.json"


def sha256_hex(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def make_zip(entries: dict) -> bytes:
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, "w") as zf:
        for name, data in entries.items():
            zf.writestr(name, data)
    return buffer.getvalue()


class FakeFetcher:
    """`url -> bytes`, recording every call. Stands in for urllib."""

    def __init__(self, blobs: dict):
        self.blobs = blobs
        self.calls = []

    def __call__(self, url: str) -> bytes:
        self.calls.append(url)
        if url not in self.blobs:
            raise AssertionError(f"no fixture for {url}")
        return self.blobs[url]


def simple_manifest(tmp_files):
    """A manifest with one plain (non-archive) entry per (dest, content)."""
    files = []
    blobs = {}
    for i, (dest, content) in enumerate(tmp_files):
        url = f"https://example.invalid/{dest.replace('/', '_')}"
        files.append({"dest": dest, "url": url, "sha256": sha256_hex(content)})
        blobs[url] = content
    return {"files": files}, blobs


class AllGoodWritesEveryDest(unittest.TestCase):
    def test_writes_every_file(self):
        with ft.temp_root() as root:
            manifest, blobs = simple_manifest(
                [("a/one.txt", b"one"), ("b/two.txt", b"two two")]
            )
            fetcher = FakeFetcher(blobs)
            code = ft.run(manifest, root=root, fetcher=fetcher)
            self.assertEqual(code, 0)
            self.assertEqual((root / "a" / "one.txt").read_bytes(), b"one")
            self.assertEqual((root / "b" / "two.txt").read_bytes(), b"two two")


class OneBadHashWritesNothing(unittest.TestCase):
    def test_all_or_nothing(self):
        with ft.temp_root() as root:
            manifest, blobs = simple_manifest(
                [("a/one.txt", b"one"), ("b/two.txt", b"two two")]
            )
            # Corrupt the second entry's pinned hash.
            manifest["files"][1]["sha256"] = "0" * 64
            fetcher = FakeFetcher(blobs)
            code = ft.run(manifest, root=root, fetcher=fetcher)
            self.assertNotEqual(code, 0)
            self.assertFalse((root / "a" / "one.txt").exists())
            self.assertFalse((root / "b" / "two.txt").exists())


class BadArchiveHashRefusesBeforeExtraction(unittest.TestCase):
    def test_refuses_before_reading_a_member(self):
        with ft.temp_root() as root:
            archive = make_zip({"pkg/file.bin": b"payload"})
            url = "https://example.invalid/archive.zip"
            manifest = {
                "files": [
                    {
                        "dest": "out/file.bin",
                        "url": url,
                        "archive_sha256": "0" * 64,  # wrong on purpose
                        "member": "pkg/file.bin",
                        "sha256": sha256_hex(b"payload"),
                    }
                ]
            }
            fetcher = FakeFetcher({url: archive})
            code = ft.run(manifest, root=root, fetcher=fetcher)
            self.assertNotEqual(code, 0)
            self.assertFalse((root / "out" / "file.bin").exists())


class ExistingDifferingDestIsRefusedWithoutForce(unittest.TestCase):
    def test_refused_then_replaced_with_force(self):
        with ft.temp_root() as root:
            manifest, blobs = simple_manifest([("a/one.txt", b"pinned")])
            dest = root / "a" / "one.txt"
            dest.parent.mkdir(parents=True)
            dest.write_bytes(b"developer's own local copy")

            fetcher = FakeFetcher(blobs)
            code = ft.run(manifest, root=root, fetcher=fetcher)
            self.assertNotEqual(code, 0)
            self.assertEqual(dest.read_bytes(), b"developer's own local copy")

            code = ft.run(manifest, root=root, fetcher=fetcher, force=True)
            self.assertEqual(code, 0)
            self.assertEqual(dest.read_bytes(), b"pinned")


class ExistingMatchingDestIsSkipped(unittest.TestCase):
    def test_skip_reuses_the_present_file_and_fetches_nothing(self):
        with ft.temp_root() as root:
            manifest, blobs = simple_manifest([("a/one.txt", b"pinned")])
            dest = root / "a" / "one.txt"
            dest.parent.mkdir(parents=True)
            dest.write_bytes(b"pinned")

            fetcher = FakeFetcher(blobs)
            code = ft.run(manifest, root=root, fetcher=fetcher)
            self.assertEqual(code, 0)
            self.assertEqual(fetcher.calls, [], "an already-correct file must not be re-downloaded")


class VerifyOnly(unittest.TestCase):
    def test_empty_root_fails(self):
        with ft.temp_root() as root:
            manifest, _ = simple_manifest([("a/one.txt", b"pinned")])
            code = ft.run(manifest, root=root, fetcher=FakeFetcher({}), verify_only=True)
            self.assertNotEqual(code, 0, "an empty root must not pass verification (negative control)")

    def test_populated_root_passes(self):
        with ft.temp_root() as root:
            manifest, blobs = simple_manifest([("a/one.txt", b"pinned")])
            dest = root / "a" / "one.txt"
            dest.parent.mkdir(parents=True)
            dest.write_bytes(b"pinned")
            code = ft.run(manifest, root=root, fetcher=FakeFetcher(blobs), verify_only=True)
            self.assertEqual(code, 0)


class DotDotDestIsRejected(unittest.TestCase):
    def test_traversal_dest_refused(self):
        with ft.temp_root() as root:
            manifest = {
                "files": [
                    {"dest": "../escape.txt", "url": "https://example.invalid/x", "sha256": "0" * 64}
                ]
            }
            code = ft.run(manifest, root=root, fetcher=FakeFetcher({}))
            self.assertNotEqual(code, 0)

    def test_absolute_dest_refused(self):
        with ft.temp_root() as root:
            manifest = {
                "files": [
                    {"dest": "/etc/passwd", "url": "https://example.invalid/x", "sha256": "0" * 64}
                ]
            }
            code = ft.run(manifest, root=root, fetcher=FakeFetcher({}))
            self.assertNotEqual(code, 0)


class TheRealManifestParsesAndCoversWhatBuildReleaseNeeds(unittest.TestCase):
    def test_real_pins_file_parses(self):
        manifest = json.loads(PINS_PATH.read_text(encoding="utf-8"))
        self.assertTrue(manifest["files"])

    def test_covers_needed_minus_plugin_plus_tracker(self):
        sys.path.insert(0, str(REPO_ROOT))
        import build_release  # noqa: E402  (safe: main() is __main__-guarded)

        needed = set(build_release.NEEDED) - {"BloodPactPlugin.dll"}
        expected_names = needed | {"HSOfflineTrackerProducer.dll"}

        manifest = json.loads(PINS_PATH.read_text(encoding="utf-8"))
        modfile_dests = {
            Path(f["dest"]).name
            for f in manifest["files"]
            if f["dest"].startswith("modfiles_shipped/")
        }
        self.assertEqual(modfile_dests, expected_names)

    def test_eleven_entries(self):
        manifest = json.loads(PINS_PATH.read_text(encoding="utf-8"))
        self.assertEqual(len(manifest["files"]), 11)


if __name__ == "__main__":
    unittest.main()
