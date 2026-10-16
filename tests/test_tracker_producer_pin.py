"""ForgePact no longer pins or ships HS Offline Tracker's sensor (#185 Q11).

Through v2.2.x `tools/toolchain-pins.json` pinned
`modfiles_shipped/HSOfflineTrackerProducer.dll` and Install Mod Plugin put it
beside the plugin; the build ForgePact shipped from 1.3.10 aborts every
graceful close of the game with `0xC0000409` (#201, measured; see
`docs/graceful-stop-exit-research.md`). For #185 the pin moved to
HS-Offline-Tracker v0.1.4's build, which carries the exit-teardown fix
(Tracker PR #8), and in v2.3.0 ForgePact stopped shipping a sensor at all:
its own Tracker tab replaces HS Offline Tracker, whose own installer places
its own sensor.

What stays is the product's knowledge of the Tracker's builds:
`src/forgepact.py`'s `TRACKER_SENSOR_ABORTING` and `TRACKER_SENSOR_CLEAN`.
Install Mod Plugin moves a sensor in the first set out of the game
(tests/test_tracker_sensor_retire.py), the launch note names it, and the hub's
`docs/tools/hs-drive-mcp.md` classifies a game's sensor with both. This module
states every Tracker release's sensor hash once more, independently of the
product, and checks:

  - the two sets: 0.1.1, 0.1.2 (ForgePact's old pin) and 0.1.3 aborting,
    0.1.4 clean, no build in both, every hash a full SHA-256;
  - the retirement: no pins-file entry is a producer pin, through a pure
    finder fed both producer pins ForgePact ever had (the 1.3.16-zip entry
    copied verbatim, and the v0.1.4 entry) as positive controls;
  - the loader files ForgePact pins are still the ones HS-Offline-Tracker
    v0.1.4 ships (`FIXED_LOADER`), so a player who keeps that Tracker and
    its sensor runs it on the same YYToolkit and AurieCore.

When HS Offline Tracker publishes a new sensor build, add its hash to the
set it belongs in, with what was measured: an unmeasured build goes in
neither. The constants below never change.
"""
import json
import sys
import unittest
from pathlib import Path
from typing import Dict, List

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))
import forgepact  # noqa: E402

PINS_PATH = ROOT / "tools" / "toolchain-pins.json"
SENSOR_NAME = "HSOfflineTrackerProducer"

#: The entry ForgePact pinned before #185, copied verbatim from
#: tools/toolchain-pins.json. A positive control for the finder.
OLD_ENTRY = {
    "dest": "modfiles_shipped/HSOfflineTrackerProducer.dll",
    "url": "https://github.com/falorfrozen-cmd/ForgePact/releases/download/v1.3.16/ForgePact-1.3.16.zip",
    "archive_sha256": "e0bbe3a087af20c4a89137db386e06e3421316286bd2a076a4982f02869d1516",
    "member": "ForgePact-1.3.16/modfiles/HSOfflineTrackerProducer.dll",
    "sha256": "36608aa041b31e23f35fbf010554715657d14269c61c541e717fde76a481b14e",
    "provenance": "HS-Offline-Tracker's live sensor producer (optional), a 2026-09-07-or-earlier build shipped in every ForgePact release since v1.3.10, extracted here from the published v1.3.16 zip",
}

#: The entry #185 pinned until the retirement: HS-Offline-Tracker v0.1.4
#: (published 2026-10-06), the portable zip's hash as SHA256SUMS-0.1.4.txt
#: states it, and its producer member's hash (155,648 bytes). The second
#: positive control for the finder.
FIXED_ENTRY = {
    "dest": "modfiles_shipped/HSOfflineTrackerProducer.dll",
    "url": "https://github.com/falorfrozen-cmd/HS-Offline-Tracker/releases/download/v0.1.4/hs-offline-tracker-0.1.4-portable.zip",
    "archive_sha256": "37b03813359d6235a13fe3bb74dd1c2ee5733452a904c01cf41299e02e139709",
    "member": "producer/HSOfflineTrackerProducer.dll",
    "sha256": "5f83e542f41490ec57106198cf8546ec178421961e388699ff1da5de45956f94",
}

#: HS-Offline-Tracker v0.1.3 (published 2026-09-19): its portable zip's
#: producer member (153,600 bytes), hashed from the release asset on
#: 2026-10-13. Built from source that still keeps the publisher in a global
#: std::thread.
V0_1_3_SHA = "9e0c63c8b84f03e517a35ba9b5e7a7017516e713dcde964fe105f0c0992ea58d"

#: HS-Offline-Tracker v0.1.1 (published 2026-09-04) shipped no portable zip:
#: its producer (147,968 bytes, PE link time 2026-09-04) was extracted from the
#: release's NSIS installer `HS.Offline.Tracker_0.1.1_x64-setup.exe` (asset
#: hash as SHA256SUMS.txt states it, 04109842...) on 2026-10-13. The same
#: extraction of v0.1.2's installer gave 36608aa0..., the hash of v0.1.2's zip
#: member, so the method reads the file the installer carries.
V0_1_1_SHA = "8594d096b157eb955c72a4645f118a65916782b36e7ea5343634644a8260676d"

#: The loader files in HS-Offline-Tracker v0.1.4's portable zip
#: (`aurie-loader/...`), hashed from the release asset on 2026-10-13, keyed
#: by ForgePact's pin dest.
FIXED_LOADER = {
    "modfiles_shipped/AurieCore.dll": "3cf98af99a0ef38dea1a2d4627e6466631f6e7dd2b254b1a80e5438ac06800cb",
    "modfiles_shipped/YYToolkit.dll": "51a393d7e5291ad76bdb85b9f44faf5178b6b20e0ce8432fa26bdaf9e21eadf8",
    "modfiles_shipped/AuriePatcher.exe": "4d3aec439dbba5209ad48fb0a40d6324406247fe678541df9031f5b89363d536",
}

KNOWN_ABORTING = (V0_1_1_SHA, OLD_ENTRY["sha256"], V0_1_3_SHA)


def producer_pins(files: List[Dict[str, object]]) -> List[str]:
    """The dest of every pins-file entry that would place HS Offline Tracker's
    sensor: one whose dest or member names it, or whose sha256 is a sensor
    build the product knows. Pure: no filesystem."""
    known = set(forgepact.TRACKER_SENSOR_ABORTING) | set(forgepact.TRACKER_SENSOR_CLEAN)
    return [str(f.get("dest")) for f in files
            if SENSOR_NAME in str(f.get("dest", "")) or SENSOR_NAME in str(f.get("member", ""))
            or f.get("sha256") in known]


class TheProductKnowsTheBuilds(unittest.TestCase):
    """src/forgepact.py's sets, which decide what Install moves aside."""

    def test_every_tracker_release_before_the_fix_is_aborting(self):
        # 0.1.1, 0.1.2 (= ForgePact's old pin) and 0.1.3.
        for sha in KNOWN_ABORTING:
            with self.subTest(sha=sha):
                self.assertIn(sha, forgepact.TRACKER_SENSOR_ABORTING)
        self.assertEqual(len(forgepact.TRACKER_SENSOR_ABORTING), 3)

    def test_tracker_0_1_4_is_clean(self):
        self.assertIn(FIXED_ENTRY["sha256"], forgepact.TRACKER_SENSOR_CLEAN)
        self.assertNotIn(FIXED_ENTRY["sha256"], forgepact.TRACKER_SENSOR_ABORTING)
        self.assertEqual(len(forgepact.TRACKER_SENSOR_CLEAN), 1)

    def test_no_build_is_in_both_sets(self):
        self.assertEqual(set(forgepact.TRACKER_SENSOR_ABORTING) & set(forgepact.TRACKER_SENSOR_CLEAN), set())

    def test_every_hash_is_a_full_sha256(self):
        for sha in list(forgepact.TRACKER_SENSOR_ABORTING) + list(forgepact.TRACKER_SENSOR_CLEAN):
            with self.subTest(sha=sha):
                self.assertRegex(sha, r"\A[0-9a-f]{64}\Z")


class FinderControls(unittest.TestCase):
    def test_both_producer_pins_forgepact_had_are_found(self):
        for entry in (OLD_ENTRY, FIXED_ENTRY):
            with self.subTest(url=entry["url"]):
                self.assertEqual(producer_pins([entry]), [entry["dest"]])

    def test_a_renamed_dest_is_still_found_by_member_or_hash(self):
        self.assertEqual(len(producer_pins([dict(FIXED_ENTRY, dest="modfiles_shipped/sensor.dll")])), 1)
        renamed = dict(OLD_ENTRY, dest="modfiles_shipped/x.dll", member="x.dll")
        self.assertEqual(producer_pins([renamed]), ["modfiles_shipped/x.dll"])

    def test_a_loader_pin_is_not_a_producer_pin(self):
        entry = {"dest": "modfiles_shipped/YYToolkit.dll", "sha256": FIXED_LOADER["modfiles_shipped/YYToolkit.dll"]}
        self.assertEqual(producer_pins([entry]), [])


class ForgePactPinsNoProducer(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.files = json.loads(PINS_PATH.read_text(encoding="utf-8"))["files"]

    def test_baseline_the_pins_file_still_pins_the_loader(self):
        # The positive control: the file is read and holds the shipped files.
        dests = {f.get("dest") for f in self.files}
        self.assertLessEqual(set(FIXED_LOADER), dests)

    def test_target_no_producer_pin(self):
        self.assertEqual(producer_pins(self.files), [])

    def test_the_loader_files_are_tracker_0_1_4s(self):
        pinned = {f.get("dest"): f.get("sha256") for f in self.files}
        for dest, sha in FIXED_LOADER.items():
            with self.subTest(dest=dest):
                self.assertEqual(pinned.get(dest), sha)

    def test_build_release_packages_no_sensor(self):
        source = (ROOT / "build_release.py").read_text(encoding="utf-8")
        self.assertNotIn(SENSOR_NAME, source)
        self.assertNotIn(forgepact.TRACKER_SENSOR_DLL, source)


if __name__ == "__main__":
    unittest.main()
