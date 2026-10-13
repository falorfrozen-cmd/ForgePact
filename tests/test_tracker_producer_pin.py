"""ForgePact pins HS-Offline-Tracker v0.1.4's live sensor producer (#185, #201).

`Install Mod Plugin` places `modfiles_shipped/HSOfflineTrackerProducer.dll`
beside the plugin. Through v2.2.x that file was the 2026-09-05 build
extracted from ForgePact's own v1.3.16 release zip, and that build aborts
every graceful close of the game with `0xC0000409`, the Windows crash dump
naming the producer (#201, measured; see `docs/graceful-stop-exit-research.md`).
HS-Offline-Tracker v0.1.4 carries the exit-teardown fix (Tracker PR #8).

The hash appears in several places: `tools/toolchain-pins.json` (what
`fetch_toolchain.py` downloads), `src/forgepact.py`'s `TRACKER_SENSOR_CLEAN`
and `TRACKER_SENSOR_ABORTING` (what the panel decides a launch-time
replacement on), and the hub guide's pin table. This module states the
v0.1.4 entry once more, independently of the pins file, from the Tracker's
own v0.1.4 release (`SHA256SUMS-0.1.4.txt` for the archive, the portable
zip's member for the DLL), and checks the pins file and the product's sets
against it:

  - a pure classifier sorts a pins entry into "fixed" (a build in the
    product's measured-clean set), "aborting" (in its known-aborting set),
    "unmeasured" (a Tracker release build in neither) or "drifted" (not the
    shape of a Tracker release pin);
  - controls feed it shapes, including the old 1.3.16-zip entry copied
    verbatim and HS-Offline-Tracker v0.1.3's real entry as negative controls,
    and an unknown Tracker build, which must not pass as "fixed";
  - the real pins file must carry exactly `FIXED_ENTRY`, classified
    "fixed", listing every build it replaced as superseded, so that
    `fetch_toolchain.py` replaces that file on an existing checkout;
  - the pins file's loader files must be the ones in the same Tracker
    release (`FIXED_LOADER`), which is what lets the release notes say that
    ForgePact and that Tracker install the same sensor and loader files.

A re-pin is data edits only, in this order (the hub guide's Maintenance
Trigger "A new HS-Offline-Tracker producer" says the same):
  1. `tools/toolchain-pins.json`: the new url, `archive_sha256`, member,
     `sha256` and a `provenance` that names the new release (`v<x.y.z>`);
     append the replaced `sha256` to `superseded_sha256` (keep the older
     ones, since a checkout may still hold any of them). From here four
     tests fail: `test_the_pin_is_fixed_entry`,
     `test_the_pin_classifies_as_fixed`, `test_the_provenance_names_the_release`
     and `test_replaced_builds_are_superseded_so_existing_checkouts_update`
     (the replaced hash is still `FIXED_ENTRY`'s);
  2. here: `FIXED_ENTRY` to the new values, and `FIXED_LOADER` to the new
     release's loader-file hashes (or `{}` when they differ from ForgePact's
     pins, and then drop the release notes' "same files" claim).
     `test_the_pin_is_fixed_entry`, `test_the_provenance_names_the_release`
     and `test_replaced_builds_are_superseded_so_existing_checkouts_update`
     pass again; `test_the_pin_classifies_as_fixed` still fails, and
     `test_the_pinned_build_is_measured_clean` and
     `test_the_fixed_shape_is_accepted` fail from here;
  3. re-run `Prepare-Plugin.bat` (`tools/fetch_toolchain.py`), which replaces
     the old file through `superseded_sha256`, and check `--verify-only`
     exits 0. Until then the fetched-producer control still passes on the
     old build, which stays in `TRACKER_SENSOR_CLEAN`; from here it fails;
  4. the live gate, on that fetched file;
  5. `src/forgepact.py`: add the new build to `TRACKER_SENSOR_CLEAN` with
     what the gate measured; the replaced build stays in the set it is in.
     Every test passes again. The failures between steps 1 and 5 are on
     purpose: an unmeasured build must not ship. (Walked on a scratch copy
     with a made-up v0.1.5 on 2026-10-13, on a checkout whose fetched
     producer is the clean build being replaced: 4 failures after step 1, 3
     after step 2, 4 after step 3, none after step 5. Where nothing was
     fetched, as in CI, the fetched-producer control skips: 4, 3, 3, 0.
     Where the fetched file is still an aborting build, the control fails
     before step 1 already: 1, then 5, 4, 4, 0.)
     The next re-pin is the first to leave two builds in
     `TRACKER_SENSOR_CLEAN`; Install would then put either over the other, so
     that re-pin has to order them first (ForgePact #217).
`OLD_ENTRY` and the v0.1.1/v0.1.3 controls never change.

`test_positive_control_on_the_fetched_producer` runs only where
`Prepare-Plugin.bat` (`tools/fetch_toolchain.py`) has fetched the producer:
a developer's checkout. CI's contract tests run before any fetch, so there
it always skips.
"""
import hashlib
import json
import re
import sys
import unittest
from pathlib import Path
from typing import Dict, Tuple

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))
import forgepact  # noqa: E402

PINS_PATH = ROOT / "tools" / "toolchain-pins.json"
PIN_DEST = "modfiles_shipped/HSOfflineTrackerProducer.dll"

#: A release asset of the HS-Offline-Tracker repository.
TRACKER_RELEASE_URL_MARKER = "github.com/falorfrozen-cmd/HS-Offline-Tracker/releases/download/"
#: Where the producer sits inside the Tracker's portable zip (no wrapping directory).
TRACKER_MEMBER = "producer/HSOfflineTrackerProducer.dll"

#: The entry ForgePact pinned before #185, copied verbatim from
#: tools/toolchain-pins.json. A negative control, in its real shape.
OLD_ENTRY = {
    "dest": "modfiles_shipped/HSOfflineTrackerProducer.dll",
    "url": "https://github.com/falorfrozen-cmd/ForgePact/releases/download/v1.3.16/ForgePact-1.3.16.zip",
    "archive_sha256": "e0bbe3a087af20c4a89137db386e06e3421316286bd2a076a4982f02869d1516",
    "member": "ForgePact-1.3.16/modfiles/HSOfflineTrackerProducer.dll",
    "sha256": "36608aa041b31e23f35fbf010554715657d14269c61c541e717fde76a481b14e",
    "provenance": "HS-Offline-Tracker's live sensor producer (optional), a 2026-09-07-or-earlier build shipped in every ForgePact release since v1.3.10, extracted here from the published v1.3.16 zip",
}

#: HS-Offline-Tracker v0.1.3 (published 2026-09-19), as a pin of it would
#: read: the portable zip's hash as SHA256SUMS-0.1.3.txt states it, and its
#: producer member's hash (153,600 bytes), both hashed from the release
#: assets on 2026-10-13. A Tracker release built before the teardown fix: the
#: shape every check below accepts, at a build that must not pass as fixed.
V0_1_3_ENTRY = {
    "dest": PIN_DEST,
    "url": "https://github.com/falorfrozen-cmd/HS-Offline-Tracker/releases/download/v0.1.3/hs-offline-tracker-0.1.3-portable.zip",
    "archive_sha256": "14402ef8e4ef24e8eace9b840c9abc12497450d4b5c528d681e905dfa416a484",
    "member": TRACKER_MEMBER,
    "sha256": "9e0c63c8b84f03e517a35ba9b5e7a7017516e713dcde964fe105f0c0992ea58d",
}

#: HS-Offline-Tracker v0.1.1 (published 2026-09-04) shipped no portable zip:
#: its producer (147,968 bytes, PE link time 2026-09-04) was extracted from the
#: release's NSIS installer `HS.Offline.Tracker_0.1.1_x64-setup.exe` (asset
#: hash as SHA256SUMS.txt states it, 04109842...) on 2026-10-13. The same
#: extraction of v0.1.2's installer gave 36608aa0..., the hash of v0.1.2's zip
#: member, so the method reads the file the installer carries.
V0_1_1_SHA = "8594d096b157eb955c72a4645f118a65916782b36e7ea5343634644a8260676d"

#: HS-Offline-Tracker v0.1.4 (published 2026-10-06): the portable zip's hash
#: as SHA256SUMS-0.1.4.txt states it, and its producer member's hash
#: (155,648 bytes).
FIXED_ENTRY = {
    "url": "https://github.com/falorfrozen-cmd/HS-Offline-Tracker/releases/download/v0.1.4/hs-offline-tracker-0.1.4-portable.zip",
    "archive_sha256": "37b03813359d6235a13fe3bb74dd1c2ee5733452a904c01cf41299e02e139709",
    "member": TRACKER_MEMBER,
    "sha256": "5f83e542f41490ec57106198cf8546ec178421961e388699ff1da5de45956f94",
}

#: The loader files in the same release's portable zip (`aurie-loader/...`),
#: hashed from the release asset on 2026-10-13, keyed by ForgePact's pin dest.
FIXED_LOADER = {
    "modfiles_shipped/AurieCore.dll": "3cf98af99a0ef38dea1a2d4627e6466631f6e7dd2b254b1a80e5438ac06800cb",
    "modfiles_shipped/YYToolkit.dll": "51a393d7e5291ad76bdb85b9f44faf5178b6b20e0ce8432fa26bdaf9e21eadf8",
    "modfiles_shipped/AuriePatcher.exe": "4d3aec439dbba5209ad48fb0a40d6324406247fe678541df9031f5b89363d536",
}


def classify_producer_pin(entry: Dict[str, object]) -> Tuple[str, str]:
    """"aborting" when `entry`'s sha256 is in the product's known-aborting
    set; "drifted" when it is not the producer member of an HS-Offline-Tracker
    release zip with an archive hash; "fixed" when it is, at a sha256 in the
    product's measured-clean set; otherwise "unmeasured". Each with the
    reason. Pure: no filesystem."""
    sha = entry.get("sha256")
    url = str(entry.get("url", ""))
    if not sha:
        return "drifted", "the entry has no \"sha256\" key"
    if sha in forgepact.TRACKER_SENSOR_ABORTING:
        return "aborting", f"sha256 {sha} is a producer build known to abort a graceful close (#201)"
    if TRACKER_RELEASE_URL_MARKER not in url:
        return "drifted", f"url {url!r} is not a release asset of HS-Offline-Tracker"
    if not entry.get("archive_sha256"):
        return "drifted", "the entry has no \"archive_sha256\"; the release zip is not hash-checked"
    if entry.get("member") != TRACKER_MEMBER:
        return "drifted", f"member {entry.get('member')!r} is not {TRACKER_MEMBER!r}"
    if sha in forgepact.TRACKER_SENSOR_CLEAN:
        return "fixed", "the producer member of an HS-Offline-Tracker release, measured to close cleanly"
    return "unmeasured", (f"sha256 {sha} is an HS-Offline-Tracker release build no live close has "
                          "measured; measure it, then add it to TRACKER_SENSOR_CLEAN")


class ClassifierControls(unittest.TestCase):
    FIXED = dict(FIXED_ENTRY, dest=PIN_DEST)

    def test_the_old_entry_is_aborting(self):
        # A negative control, verbatim from the pins file before the re-pin.
        state, reason = classify_producer_pin(OLD_ENTRY)
        self.assertEqual(state, "aborting", reason)
        self.assertIn("#201", reason)

    def test_tracker_v0_1_3_is_aborting_not_fixed(self):
        # The right shape, from the right repository, before the fix.
        state, reason = classify_producer_pin(V0_1_3_ENTRY)
        self.assertEqual(state, "aborting", reason)

    def test_the_fixed_shape_is_accepted(self):
        state, reason = classify_producer_pin(self.FIXED)
        self.assertEqual(state, "fixed", reason)

    def test_an_unknown_tracker_build_is_unmeasured_not_fixed(self):
        entry = dict(self.FIXED, sha256="0" * 64)
        state, reason = classify_producer_pin(entry)
        self.assertEqual((state, "TRACKER_SENSOR_CLEAN" in reason), ("unmeasured", True))

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


class TheProductKnowsTheBuilds(unittest.TestCase):
    """src/forgepact.py's sets, which decide a launch-time replacement."""

    def test_every_tracker_release_before_the_fix_is_aborting(self):
        # 0.1.1, 0.1.2 (= ForgePact's old pin) and 0.1.3.
        for sha in (V0_1_1_SHA, OLD_ENTRY["sha256"], V0_1_3_ENTRY["sha256"]):
            with self.subTest(sha=sha):
                self.assertIn(sha, forgepact.TRACKER_SENSOR_ABORTING)

    def test_the_pinned_build_is_measured_clean(self):
        self.assertIn(FIXED_ENTRY["sha256"], forgepact.TRACKER_SENSOR_CLEAN)

    def test_no_build_is_in_both_sets(self):
        self.assertEqual(set(forgepact.TRACKER_SENSOR_ABORTING) & set(forgepact.TRACKER_SENSOR_CLEAN), set())

    def test_every_hash_is_a_full_sha256(self):
        for sha in list(forgepact.TRACKER_SENSOR_ABORTING) + list(forgepact.TRACKER_SENSOR_CLEAN):
            with self.subTest(sha=sha):
                self.assertRegex(sha, r"\A[0-9a-f]{64}\Z")

    def test_positive_control_on_the_fetched_producer(self):
        # The one check a stated hash cannot make: that the DLL
        # fetch_toolchain.py really wrote hashes into the clean set. Runs
        # wherever Prepare-Plugin.bat has been run.
        fetched = ROOT / PIN_DEST
        if not fetched.is_file():
            self.skipTest(f"no {PIN_DEST} in this checkout")
        sha = hashlib.sha256(fetched.read_bytes()).hexdigest()
        self.assertIn(sha, forgepact.TRACKER_SENSOR_CLEAN,
                      f"{PIN_DEST} is {sha}, not a build measured clean. If this checkout ran "
                      "Prepare-Plugin.bat before the producer was re-pinned, run it again (or "
                      "py tools/fetch_toolchain.py): it replaces a superseded pin's file.")


class ForgePactPinsTheFixedProducer(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.files = json.loads(PINS_PATH.read_text(encoding="utf-8"))["files"]
        cls.entries = [f for f in cls.files if f.get("dest") == PIN_DEST]

    def test_exactly_one_producer_pin(self):
        self.assertEqual(len(self.entries), 1,
                         f"expected one {PIN_DEST} entry in tools/toolchain-pins.json")

    def test_the_pin_is_fixed_entry(self):
        entry = self.entries[0]
        for key, value in FIXED_ENTRY.items():
            self.assertEqual(entry.get(key), value, key)

    def test_the_pin_classifies_as_fixed(self):
        state, reason = classify_producer_pin(self.entries[0])
        self.assertEqual(state, "fixed", f"ForgePact's {PIN_DEST} pin: {reason}")

    def test_replaced_builds_are_superseded_so_existing_checkouts_update(self):
        superseded = self.entries[0].get("superseded_sha256") or []
        self.assertIn(OLD_ENTRY["sha256"], superseded)
        self.assertNotIn(FIXED_ENTRY["sha256"], superseded)
        known = set(forgepact.TRACKER_SENSOR_ABORTING) | set(forgepact.TRACKER_SENSOR_CLEAN)
        for sha in superseded:
            with self.subTest(sha=sha):
                self.assertIn(sha, known, "a superseded pin is a build the product knows")

    def test_the_provenance_names_the_release(self):
        version = re.search(r"/releases/download/(v[0-9.]+)/", FIXED_ENTRY["url"]).group(1)
        provenance = str(self.entries[0].get("provenance", ""))
        self.assertIn("HS-Offline-Tracker", provenance)
        self.assertIn(version, provenance)

    def test_the_loader_files_are_the_same_releases(self):
        pinned = {f.get("dest"): f.get("sha256") for f in self.files}
        for dest, sha in FIXED_LOADER.items():
            with self.subTest(dest=dest):
                self.assertEqual(pinned.get(dest), sha)


if __name__ == "__main__":
    unittest.main()
