"""ForgePact stops shipping HS Offline Tracker's sensor (#185 Q11, #217).

Through v2.2.x Install Mod Plugin copied `HSOfflineTrackerProducer.dll` into
the game's `mods\\aurie`, and every build it shipped before v2.3.0 aborts
every graceful close of the game (#201). From v2.3.0 the sensor is not
ForgePact's: its own Tracker tab replaces HS Offline Tracker, and the
Tracker's own installer places its own sensor. So:

  - Install Mod Plugin copies no sensor. A sensor in the game whose SHA-256
    is in `TRACKER_SENSOR_ABORTING` is moved to ForgePact's backup folder
    (`TRACKER_SENSOR_BACKUP_DIR`) under a name that never overwrites, checked
    by hash after the move, with one step saying so. One it cannot move is
    named in a step, and Install carries on. Any other sensor (the clean
    0.1.4 build, a newer Tracker's, a developer's own) is left as it is,
    without a word;
  - Remove Plugin deletes no sensor;
  - Launch writes nothing: for an aborting sensor it says so and that
    Install Mod Plugin moves it aside, otherwise it says nothing.

Baseline cases pin what must stay quiet (no sensor; a clean or unknown one),
target cases pin the move, its refusals, and Remove and Launch leaving the
file alone. A sensor still bundled in the modfiles folder (a source checkout
that fetched it before the retirement) is in every fixture, as the control
that nothing is copied from it any more.
"""
import os
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))
import forgepact  # noqa: E402
from test_plugin_build_check import (  # noqa: E402
    SENSOR_ABORTING, SENSOR_FIXED, SENSOR_UNKNOWN, InstallFixture, SensorFixture, sha256_of,
)

STAMP = "20261014T120000"
# Read before any fixture patches them (None until the product has them, so
# the module still imports against the pre-retirement product).
REAL_BACKUP_DIR = getattr(forgepact, "TRACKER_SENSOR_BACKUP_DIR", None)
REAL_UTC_STAMP = getattr(forgepact, "_utc_stamp", lambda: "")


class RetireInstallFixture(InstallFixture):
    """InstallFixture with the stand-in sensor sets, a leftover bundled
    sensor, a backup folder in the temp directory and a fixed UTC stamp."""

    def setUp(self):
        super().setUp()
        self.target = self.bin / "mods" / "aurie" / forgepact.TRACKER_SENSOR_DLL
        # The leftover: a fetched sensor a source checkout still holds.
        (self.sources / forgepact.TRACKER_SENSOR_DLL).write_bytes(SENSOR_FIXED)
        self.backup = self.bin.parent.parent / "LocalAppData" / "Hero_Siege" / "backup"
        self.enterContext(patch.object(forgepact, "TRACKER_SENSOR_BACKUP_DIR", self.backup))
        self.enterContext(patch.object(forgepact, "_utc_stamp", return_value=STAMP))
        self.enterContext(patch.object(forgepact, "TRACKER_SENSOR_ABORTING",
                                       {sha256_of(SENSOR_ABORTING): "stand-in for an aborting build"}))
        self.enterContext(patch.object(forgepact, "TRACKER_SENSOR_CLEAN",
                                       {sha256_of(SENSOR_FIXED): "stand-in for the measured-clean build"}))

    def put(self, data: bytes) -> None:
        self.target.parent.mkdir(parents=True, exist_ok=True)
        self.target.write_bytes(data)

    def moved_name(self, suffix: str = "") -> str:
        return "HSOfflineTrackerProducer-%s-%s%s.dll" % (sha256_of(SENSOR_ABORTING)[:8], STAMP, suffix)

    def backed_up(self) -> list:
        return sorted(p.name for p in self.backup.iterdir()) if self.backup.exists() else []


class InstallMovesAnAbortingSensorAside(RetireInstallFixture):
    def test_the_backup_folder_is_forgepacts_own(self):
        # %LOCALAPPDATA%\Hero_Siege\backup, read before the fixture patched it.
        self.assertEqual(REAL_BACKUP_DIR, forgepact.ROOT / "backup")
        self.assertFalse(hasattr(forgepact, "_tracker_sensor_install"), "Install still copies a sensor")

    def test_baseline_no_sensor_nothing_moved_nothing_said(self):
        result = self.install()
        self.assertIn("ok", result, result)
        self.assertIn("mod DLLs installed/updated", result["ok"])
        self.assertNotIn("HS Offline Tracker", result["ok"])
        # The leftover bundle is not copied in.
        self.assertFalse(self.target.exists())
        self.assertEqual(self.backed_up(), [])

    def test_baseline_clean_and_unknown_sensors_stay_and_nothing_is_said(self):
        for data in (SENSOR_FIXED, SENSOR_UNKNOWN, b""):
            with self.subTest(data=data):
                self.put(data)
                result = self.install()
                self.assertIn("ok", result, result)
                self.assertNotIn("HS Offline Tracker", result["ok"])
                self.assertEqual(self.target.read_bytes(), data)
                self.assertEqual(self.backed_up(), [])

    def test_target_an_aborting_sensor_is_moved_with_its_step(self):
        self.put(SENSOR_ABORTING)
        result = self.install()
        self.assertIn("ok", result, result)
        self.assertFalse(self.target.exists(), "the aborting sensor is still in the game")
        self.assertEqual(self.backed_up(), [self.moved_name()])
        self.assertEqual((self.backup / self.moved_name()).read_bytes(), SENSOR_ABORTING)
        message = result["ok"]
        # What, where, why, what replaces it, and where the fixed one is.
        self.assertIn("HS Offline Tracker's old sensor", message)
        self.assertIn(str(self.backup / self.moved_name()), message)
        self.assertIn("crashes the game every time it closes", message)
        self.assertIn("Tracker tab", message)
        self.assertIn("HS Offline Tracker 0.1.4 or later", message)
        # Install did everything else as before.
        self.assertIn("mod DLLs installed/updated", message)
        self.assertTrue((self.bin / "mods" / "aurie" / "BloodPactPlugin.dll").exists())

    def test_a_second_move_never_overwrites_the_first(self):
        self.put(SENSOR_ABORTING)
        self.install()
        self.put(SENSOR_ABORTING)
        result = self.install()
        self.assertIn(self.moved_name("-2"), result["ok"])
        self.assertEqual(set(self.backed_up()), {self.moved_name(), self.moved_name("-2")})
        for name in self.backed_up():
            self.assertEqual((self.backup / name).read_bytes(), SENSOR_ABORTING)
        self.assertFalse(self.target.exists())

    def test_the_real_stamp_is_utc_yyyymmddThhmmss(self):
        # The fixture's stamp is fixed; the product's own is read here.
        self.assertRegex(REAL_UTC_STAMP(), r"\A\d{8}T\d{6}\Z")
        with patch.object(forgepact.time, "gmtime", return_value=(2026, 1, 2, 3, 4, 5, 4, 2, 0)):
            self.assertEqual(REAL_UTC_STAMP(), "20260102T030405")

    def test_a_locked_sensor_is_reported_and_install_carries_on(self):
        self.put(SENSOR_ABORTING)
        with patch.object(forgepact.os, "rename", side_effect=PermissionError(
                13, "The process cannot access the file because it is being used by another process")):
            result = self.install()
        self.assertIn("ok", result, result)
        message = result["ok"]
        self.assertIn("could not be moved", message)
        self.assertIn(forgepact.TRACKER_SENSOR_DLL, message)
        self.assertIn("being used by another process", message)
        # Everything else was installed, and the file is where it was.
        self.assertIn("mod DLLs installed/updated", message)
        self.assertIn("exe patched", message)
        self.assertEqual(self.target.read_bytes(), SENSOR_ABORTING)
        self.assertEqual(self.backed_up(), [])

    @unittest.skipUnless(os.name == "nt", "a handle held open blocks a rename only on Windows")
    def test_a_sensor_held_open_is_reported_and_install_carries_on(self):
        # The real lock: Python opens files without FILE_SHARE_DELETE.
        self.put(SENSOR_ABORTING)
        with open(self.target, "rb"):
            result = self.install()
        self.assertIn("ok", result, result)
        self.assertIn("could not be moved", result["ok"])
        self.assertEqual(self.target.read_bytes(), SENSOR_ABORTING)
        self.assertEqual(self.backed_up(), [])

    def test_a_move_across_volumes_copies_then_deletes(self):
        # The game on another drive than %LOCALAPPDATA%: rename refuses with
        # "not same device", and the file is copied, checked and deleted.
        self.put(SENSOR_ABORTING)
        cross = OSError(18, "The system cannot move the file to a different disk drive")
        cross.winerror = 17
        with patch.object(forgepact.os, "rename", side_effect=cross):
            result = self.install()
        self.assertIn(str(self.backup / self.moved_name()), result["ok"])
        self.assertEqual((self.backup / self.moved_name()).read_bytes(), SENSOR_ABORTING)
        self.assertFalse(self.target.exists())

    def test_a_copy_that_does_not_match_is_reported(self):
        self.put(SENSOR_ABORTING)
        real = forgepact._sha256_file

        def hashed(path):
            return "0" * 64 if Path(path).parent == self.backup else real(path)

        with patch.object(forgepact, "_sha256_file", side_effect=hashed):
            result = self.install()
        self.assertIn("ok", result, result)
        self.assertIn("does not match", result["ok"])

    def test_an_err_answer_moves_nothing(self):
        # Install that stops before its copies leaves the sensor alone too.
        self.put(SENSOR_ABORTING)
        self.running.side_effect = [False, True]
        self.assertIn("err", self.install())
        self.assertEqual(self.target.read_bytes(), SENSOR_ABORTING)
        self.assertEqual(self.backed_up(), [])


class RemoveLeavesEverySensor(RetireInstallFixture):
    def test_remove_leaves_every_sensor_in_place(self):
        for data in (SENSOR_ABORTING, SENSOR_FIXED, SENSOR_UNKNOWN):
            with self.subTest(data=data):
                self.assertIn("ok", self.install())
                self.put(data)
                result = forgepact.op_remove_mod(self.cfg)
                self.assertIn("ok", result, result)
                # The positive control: Remove did remove the mod files.
                self.assertIn("mod files removed", result["ok"])
                self.assertFalse((self.bin / "mods" / "aurie" / "BloodPactPlugin.dll").exists())
                self.assertEqual(self.target.read_bytes(), data)


class LaunchWritesNoSensor(SensorFixture):
    """refresh_stale_tracker_sensor(): every gate the old replacement had is
    open here (a release build, the clean bundle, the bundled loader files),
    so a write would have happened before the retirement."""

    def setUp(self):
        super().setUp()
        self.release_build()
        self.backup = self.root / "backup"
        self.enterContext(patch.object(forgepact, "TRACKER_SENSOR_BACKUP_DIR", self.backup))

    def listing(self):
        return {p.name: p.read_bytes() for p in self.aurie.iterdir()}

    def test_baseline_no_sensor_says_nothing(self):
        before = self.listing()
        self.assertEqual(forgepact.refresh_stale_tracker_sensor(self.cfg), "")
        self.assertEqual(self.listing(), before)

    def test_baseline_clean_and_unknown_sensors_say_nothing(self):
        for data in (SENSOR_FIXED, SENSOR_UNKNOWN, b""):
            with self.subTest(data=data):
                self.sensor.write_bytes(data)
                before = self.listing()
                self.assertEqual(forgepact.refresh_stale_tracker_sensor(self.cfg), "")
                self.assertEqual(self.listing(), before)

    def test_target_an_aborting_sensor_is_named_and_left_for_install(self):
        for run in (self.release_build, self.source_run):
            with self.subTest(run=run.__name__):
                run()
                self.sensor.write_bytes(SENSOR_ABORTING)
                before = self.listing()
                note = forgepact.refresh_stale_tracker_sensor(self.cfg)
                self.assertIn("HS Offline Tracker sensor in the game is an old build", note)
                self.assertIn("Install Mod Plugin moves it", note)
                self.assertTrue(note.endswith(forgepact.INSTALL_ADVICE), note)
                self.assertEqual(self.listing(), before)
                self.assertFalse(self.backup.exists())

    def test_an_unreadable_sensor_says_nothing_and_never_raises(self):
        self.sensor.write_bytes(SENSOR_ABORTING)
        with patch.object(forgepact, "_sha256_file", side_effect=PermissionError(13, "Access is denied")):
            self.assertEqual(forgepact.refresh_stale_tracker_sensor(self.cfg), "")
        self.assertEqual(self.sensor.read_bytes(), SENSOR_ABORTING)


# launch_modded_game() starting the game on the sensor it had, with this note,
# is pinned in tests/test_plugin_build_check.py's LaunchSensorRefreshTests.


if __name__ == "__main__":
    unittest.main()
