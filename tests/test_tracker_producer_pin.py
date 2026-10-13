"""ForgePact pins HS-Offline-Tracker v0.1.4's live sensor producer (#185, #201).

`Install Mod Plugin` places `modfiles_shipped/HSOfflineTrackerProducer.dll`
beside the plugin. Through v2.2.x that file was the 2026-09-05 build
extracted from ForgePact's own v1.3.16 release zip, and that build aborts
every graceful close of the game with `0xC0000409`, the Windows crash dump
naming the producer (#201, measured; see `docs/graceful-stop-exit-research.md`).
HS-Offline-Tracker v0.1.4 carries the exit-teardown fix (Tracker PR #8).

The expected hash is written in neither this repository nor the hub: only the
Tracker's v0.1.4 release states it (`SHA256SUMS-0.1.4.txt` for the archive,
the portable zip's member for the DLL). So this module states it once, below,
and reads `tools/toolchain-pins.json` against it:

  - a pure classifier sorts a pins entry into "fixed", "aborting" or
    "drifted";
  - controls feed it shapes, including the old 1.3.16-zip entry copied
    verbatim as the negative control, which must classify as "aborting";
  - the real pins file must carry exactly the v0.1.4 entry, classified
    "fixed".

When the producer is re-pinned again, update `FIXED_ENTRY` here and the pin
table in `docs/submodules/ForgePact/instructions.md` (hub) together, and add
the build being replaced to `ABORTING_SHAS` if it aborts.
"""
import json
import unittest
from pathlib import Path
from typing import Dict, Tuple

ROOT = Path(__file__).resolve().parents[1]
PINS_PATH = ROOT / "tools" / "toolchain-pins.json"
PIN_DEST = "modfiles_shipped/HSOfflineTrackerProducer.dll"

#: A release asset of the HS-Offline-Tracker repository.
TRACKER_RELEASE_URL_MARKER = "github.com/falorfrozen-cmd/HS-Offline-Tracker/releases/download/"
#: Where the producer sits inside the Tracker's portable zip (no wrapping directory).
TRACKER_MEMBER = "producer/HSOfflineTrackerProducer.dll"

#: Producer builds measured to abort a graceful close (#201: exit 0xC0000409,
#: dump raised in HSOfflineTrackerProducer).
ABORTING_SHAS = frozenset({
    "36608aa041b31e23f35fbf010554715657d14269c61c541e717fde76a481b14e",
})

#: The entry ForgePact pinned before #185, copied verbatim from
#: tools/toolchain-pins.json. The negative control, in its real shape.
OLD_ENTRY = {
    "dest": "modfiles_shipped/HSOfflineTrackerProducer.dll",
    "url": "https://github.com/falorfrozen-cmd/ForgePact/releases/download/v1.3.16/ForgePact-1.3.16.zip",
    "archive_sha256": "e0bbe3a087af20c4a89137db386e06e3421316286bd2a076a4982f02869d1516",
    "member": "ForgePact-1.3.16/modfiles/HSOfflineTrackerProducer.dll",
    "sha256": "36608aa041b31e23f35fbf010554715657d14269c61c541e717fde76a481b14e",
    "provenance": "HS-Offline-Tracker's live sensor producer (optional), a 2026-09-07-or-earlier build shipped in every ForgePact release since v1.3.10, extracted here from the published v1.3.16 zip",
}

#: HS-Offline-Tracker v0.1.4 (published 2026-10-06): the portable zip's hash
#: as SHA256SUMS-0.1.4.txt states it, and its producer member's hash
#: (155,648 bytes).
FIXED_ENTRY = {
    "url": "https://github.com/falorfrozen-cmd/HS-Offline-Tracker/releases/download/v0.1.4/hs-offline-tracker-0.1.4-portable.zip",
    "archive_sha256": "37b03813359d6235a13fe3bb74dd1c2ee5733452a904c01cf41299e02e139709",
    "member": TRACKER_MEMBER,
    "sha256": "5f83e542f41490ec57106198cf8546ec178421961e388699ff1da5de45956f94",
}


def classify_producer_pin(entry: Dict[str, object]) -> Tuple[str, str]:
    """"fixed" when `entry` is the producer member of an HS-Offline-Tracker
    release zip, with an archive hash, at a sha256 outside the known-aborting
    set; "aborting" when its sha256 is a build measured to abort a graceful
    close; otherwise "drifted". Each with the reason. Pure: no filesystem."""
    sha = entry.get("sha256")
    url = str(entry.get("url", ""))
    if not sha:
        return "drifted", "the entry has no \"sha256\" key"
    if sha in ABORTING_SHAS:
        return "aborting", f"sha256 {sha} is a producer build measured to abort a graceful close (#201)"
    if TRACKER_RELEASE_URL_MARKER not in url:
        return "drifted", f"url {url!r} is not a release asset of HS-Offline-Tracker"
    if not entry.get("archive_sha256"):
        return "drifted", "the entry has no \"archive_sha256\"; the release zip is not hash-checked"
    if entry.get("member") != TRACKER_MEMBER:
        return "drifted", f"member {entry.get('member')!r} is not {TRACKER_MEMBER!r}"
    return "fixed", "the producer member of an HS-Offline-Tracker release, outside the aborting set"


class ClassifierControls(unittest.TestCase):
    FIXED = dict(FIXED_ENTRY, dest=PIN_DEST)

    def test_the_old_entry_is_aborting(self):
        # The negative control, verbatim from the pins file before the re-pin.
        state, reason = classify_producer_pin(OLD_ENTRY)
        self.assertEqual(state, "aborting", reason)
        self.assertIn("#201", reason)

    def test_the_fixed_shape_is_accepted(self):
        state, reason = classify_producer_pin(self.FIXED)
        self.assertEqual(state, "fixed", reason)

    def test_another_source_is_drifted(self):
        entry = dict(self.FIXED, url="https://github.com/falorfrozen-cmd/ForgePact/releases/"
                                     "download/v2.2.0/ForgePact-2.2.0.zip")
        state, reason = classify_producer_pin(entry)
        self.assertEqual((state, "not a release asset of HS-Offline-Tracker" in reason),
                         ("drifted", True))

    def test_a_missing_hash_is_drifted(self):
        entry = {k: v for k, v in self.FIXED.items() if k != "sha256"}
        state, reason = classify_producer_pin(entry)
        self.assertEqual((state, "no \"sha256\"" in reason), ("drifted", True))

    def test_a_plain_file_pin_is_drifted(self):
        entry = {k: v for k, v in self.FIXED.items() if k not in ("member", "archive_sha256")}
        self.assertEqual(classify_producer_pin(entry)[0], "drifted")

    def test_another_member_is_drifted(self):
        entry = dict(self.FIXED, member="aurie-loader/YYToolkit.dll")
        state, reason = classify_producer_pin(entry)
        self.assertEqual((state, "member" in reason), ("drifted", True))

    def test_the_aborting_hash_off_the_tracker_is_still_aborting(self):
        # A known-bad build is named for what it is, wherever it came from.
        entry = dict(self.FIXED, sha256=OLD_ENTRY["sha256"])
        self.assertEqual(classify_producer_pin(entry)[0], "aborting")


class ForgePactPinsTheFixedProducer(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.files = json.loads(PINS_PATH.read_text(encoding="utf-8"))["files"]
        cls.entries = [f for f in cls.files if f.get("dest") == PIN_DEST]

    def test_exactly_one_producer_pin(self):
        self.assertEqual(len(self.entries), 1,
                         f"expected one {PIN_DEST} entry in tools/toolchain-pins.json")

    def test_the_pin_is_tracker_v0_1_4(self):
        entry = self.entries[0]
        for key, value in FIXED_ENTRY.items():
            self.assertEqual(entry.get(key), value, key)

    def test_the_pin_classifies_as_fixed(self):
        state, reason = classify_producer_pin(self.entries[0])
        self.assertEqual(state, "fixed", f"ForgePact's {PIN_DEST} pin: {reason}")

    def test_the_provenance_names_the_release(self):
        provenance = str(self.entries[0].get("provenance", ""))
        self.assertIn("HS-Offline-Tracker", provenance)
        self.assertIn("v0.1.4", provenance)

    def test_no_other_entry_changed_count(self):
        self.assertEqual(len(self.files), 11)


if __name__ == "__main__":
    unittest.main()
