"""Issue #123: the plugin in the game against the one this ForgePact ships.

Updating ForgePact replaces the panel and its modfiles, never the copy of
BloodPactPlugin.dll in the game's mods\\aurie; only Install Mod Plugin writes
that. So after an update the game went on loading the old plugin with nothing
saying so. src/forgepact.py now reads which build each file is from its own
bytes (plugin_dll_facts, plugin_build_state), shows it in the panel through
/api/state's `pluginBuild`, and, launched through a release build of the
panel, replaces an older plugin before the game starts (refresh_stale_plugin).

Baseline tests pin what must not change: an up-to-date plugin launches with
the same message and writes nothing, a source run never writes, and nothing
is written while a game is running. Target tests pin the new states, the
update itself, and Install's plain-language refusals.

The same launch step replaces HS Offline Tracker's sensor when the game's is
a build known to abort every close (refresh_stale_tracker_sensor, #185): the
plugin refresh had cleared the only Install prompt, so a player who only
pressed Launch kept the sensor ForgePact shipped through 2.2.x. A sensor
outside the known-aborting set is the negative control: never replaced.
"""
import json
import os
import re
import sys
import tempfile
import unittest
from http.client import HTTPConnection
from pathlib import Path
from types import SimpleNamespace
from unittest import mock
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))
import forgepact
import offline_launcher as launcher
from test_mod_backup import write_test_pe

ROOT = Path(__file__).resolve().parents[1]
BOOT_LINE = b"==== BloodPact plugin loaded ===="


def write_plugin(path: Path, version, padding: bytes = b"") -> bytes:
    """A stand-in DLL carrying the boot line the way a compiled plugin does:
    one string among others, `version` None for a plugin from before 1.3.20."""
    line = BOOT_LINE + (b" v" + version.encode("ascii") if version else b"")
    data = b"MZ\0\0.rdata\0[BloodPact] ready\0" + line + b"\0other strings\0" + padding
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    return data


class PluginFixture(unittest.TestCase):
    """A game folder with the mod chain installed and a modfiles folder."""

    def setUp(self):
        temp = tempfile.TemporaryDirectory(prefix="forgepact-plugin-build-")
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name).resolve()
        self.bin = self.root / "game" / "bin"
        self.aurie = self.bin / "mods" / "aurie"
        self.aurie.mkdir(parents=True)
        self.exe = self.bin / "Hero_Siege.exe"
        write_test_pe(self.exe, b"new", patched=True)
        self.modfiles = self.root / "ForgePact" / "modfiles"
        self.modfiles.mkdir(parents=True)
        for name in ("AurieCore.dll", "YYToolkit.dll"):
            (self.modfiles / name).write_bytes(b"chain " + name.encode("ascii"))
        (self.bin / "AurieCore.dll").write_bytes(b"chain AurieCore.dll")
        (self.aurie / "YYToolkit.dll").write_bytes(b"chain YYToolkit.dll")
        self.installed = self.aurie / forgepact.PLUGIN_DLL
        self.bundled = self.modfiles / forgepact.PLUGIN_DLL
        self.cfg = {"game_exe": str(self.exe)}
        self.enterContext(patch.object(forgepact, "MODFILE_SOURCES", [self.modfiles]))
        self.enterContext(patch.object(forgepact, "PLUGIN_SOURCES", [self.modfiles]))
        self.enterContext(patch.object(forgepact, "_PLUGIN_FACTS", {}))

    def release_build(self):
        """What a player runs: ForgePact.exe, frozen by PyInstaller."""
        self.enterContext(patch.object(forgepact.sys, "frozen", True, create=True))

    def source_run(self):
        self.enterContext(patch.object(forgepact.sys, "frozen", False, create=True))


class PluginDllFactsTests(PluginFixture):
    def test_the_version_is_read_from_the_boot_line(self):
        data = write_plugin(self.installed, "2.0.1")
        facts = forgepact.plugin_dll_facts(self.installed)
        self.assertEqual(facts["version"], "2.0.1")
        self.assertTrue(facts["marker"])
        self.assertEqual(len(facts["sha256"]), 64)
        import hashlib
        self.assertEqual(facts["sha256"], hashlib.sha256(data).hexdigest())

    def test_a_plugin_from_before_1_3_20_has_the_marker_and_no_version(self):
        write_plugin(self.installed, None)
        facts = forgepact.plugin_dll_facts(self.installed)
        self.assertTrue(facts["marker"])
        self.assertIsNone(facts["version"])

    def test_a_foreign_file_has_neither_and_a_missing_one_is_none(self):
        self.installed.write_bytes(b"MZ not a ForgePact plugin")
        facts = forgepact.plugin_dll_facts(self.installed)
        self.assertFalse(facts["marker"])
        self.assertIsNone(facts["version"])
        self.assertIsNone(forgepact.plugin_dll_facts(self.aurie / "absent.dll"))

    def test_a_utf16_copy_of_the_line_is_not_a_version(self):
        # Only the narrow literal the plugin passes to Out() counts.
        self.installed.write_bytes(b"MZ\0" + "==== BloodPact plugin loaded ==== v9.9.9".encode("utf-16-le"))
        self.assertIsNone(forgepact.plugin_dll_facts(self.installed)["version"])

    def test_the_answer_is_cached_by_size_and_mtime_and_a_change_is_seen(self):
        # The poll asks every few seconds; hashing a DLL each time is waste.
        # Rewrite the file to the same size and put the old mtime back: a
        # cached answer is the proof the file was not read again.
        write_plugin(self.installed, "2.0.1")
        stamp = self.installed.stat().st_mtime_ns
        self.assertEqual(forgepact.plugin_dll_facts(self.installed)["version"], "2.0.1")
        write_plugin(self.installed, "2.0.9")
        os.utime(self.installed, ns=(stamp, stamp))
        self.assertEqual(forgepact.plugin_dll_facts(self.installed)["version"], "2.0.1")
        # use_cache=False (a launch decision) reads the file itself.
        self.assertEqual(forgepact.plugin_dll_facts(self.installed, use_cache=False)["version"], "2.0.9")
        # And an ordinary change of mtime is a different key.
        os.utime(self.installed, ns=(stamp + 10**9, stamp + 10**9))
        self.assertEqual(forgepact.plugin_dll_facts(self.installed)["version"], "2.0.9")


class PluginBuildStateTests(PluginFixture):
    def state(self):
        return forgepact.plugin_build_state(self.cfg, use_cache=False)

    def test_missing_when_the_game_has_no_plugin(self):
        write_plugin(self.bundled, "2.1.0")
        self.assertEqual(self.state(), {"state": "missing", "installed": None, "bundled": None})

    def test_current_when_the_files_are_identical(self):
        write_plugin(self.bundled, "2.1.0")
        write_plugin(self.installed, "2.1.0")
        self.assertEqual(self.state(), {"state": "current", "installed": "2.1.0", "bundled": "2.1.0"})

    def test_identical_bytes_are_current_even_without_a_boot_line(self):
        # Sameness is the hash's to decide (the fixtures elsewhere in this
        # suite install marker-less stand-ins byte for byte).
        self.bundled.write_bytes(b"stand-in plugin")
        self.installed.write_bytes(b"stand-in plugin")
        self.assertEqual(self.state()["state"], "current")

    def test_older_newer_and_different_compare_versions_numerically(self):
        write_plugin(self.bundled, "2.1.0")
        for installed, expected in (("2.0.1", "older"), ("1.4.7", "older"), ("2.10.0", "newer"),
                                    ("3.0.0", "newer"), ("2.1.0", "different")):
            with self.subTest(installed=installed):
                write_plugin(self.installed, installed, padding=b"another build")
                self.assertEqual(self.state()["state"], expected)

    def test_a_plugin_from_before_1_3_20_is_older(self):
        write_plugin(self.bundled, "2.1.0")
        write_plugin(self.installed, None)
        self.assertEqual(self.state(), {"state": "older", "installed": None, "bundled": "2.1.0"})

    def test_unknown_for_a_file_without_the_boot_line(self):
        write_plugin(self.bundled, "2.1.0")
        self.installed.write_bytes(b"MZ something else")
        self.assertEqual(self.state()["state"], "unknown")

    def test_no_bundle_when_there_is_nothing_to_compare_with(self):
        # A source checkout that has not run Prepare-Plugin.bat.
        write_plugin(self.installed, "2.0.1")
        self.assertEqual(self.state(), {"state": "no-bundle", "installed": "2.0.1", "bundled": None})

    def test_the_stale_states_are_the_ones_the_panel_warns_about(self):
        self.assertEqual(forgepact.PLUGIN_STALE_STATES, ("older", "different", "unknown"))
        panel = (ROOT / "panel" / "src" / "lib" / "plugin-build.js").read_text(encoding="utf-8")
        self.assertIn("export const PLUGIN_STALE_STATES = ['older', 'different', 'unknown'];", panel)


class StaleAdviceTests(unittest.TestCase):
    def test_each_stale_state_names_the_fix_and_the_others_say_nothing(self):
        older = forgepact.plugin_stale_advice({"state": "older", "installed": "2.0.1", "bundled": "2.1.0"})
        self.assertEqual(older, "The mod plugin in the game is v2.0.1, but this ForgePact ships v2.1.0. "
                                "Close the game, then click Install Mod Plugin in Setup.")
        self.assertIn("an old version", forgepact.plugin_stale_advice(
            {"state": "older", "installed": None, "bundled": "2.1.0"}))
        self.assertIn("not the v2.1.0 build", forgepact.plugin_stale_advice(
            {"state": "different", "installed": "2.1.0", "bundled": "2.1.0"}))
        self.assertIn("not one this ForgePact can read", forgepact.plugin_stale_advice(
            {"state": "unknown", "installed": None, "bundled": "2.1.0"}))
        for state in ("current", "missing", "newer", "no-bundle"):
            self.assertEqual(forgepact.plugin_stale_advice({"state": state}), "", state)


class RefreshStalePluginTests(PluginFixture):
    def test_baseline_a_current_plugin_is_left_alone_and_nothing_is_said(self):
        self.release_build()
        write_plugin(self.bundled, "2.1.0")
        data = write_plugin(self.installed, "2.1.0")
        stamp = self.installed.stat().st_mtime_ns
        self.assertEqual(forgepact.refresh_stale_plugin(self.cfg), "")
        self.assertEqual(self.installed.read_bytes(), data)
        self.assertEqual(self.installed.stat().st_mtime_ns, stamp)

    def test_baseline_a_source_run_only_advises(self):
        # From source, a plugin in the game is a developer's own build.
        self.source_run()
        write_plugin(self.bundled, "2.1.0")
        data = write_plugin(self.installed, "2.0.1")
        note = forgepact.refresh_stale_plugin(self.cfg)
        self.assertIn("Install Mod Plugin", note)
        self.assertEqual(self.installed.read_bytes(), data)

    def test_target_a_release_build_replaces_an_older_plugin(self):
        self.release_build()
        shipped = write_plugin(self.bundled, "2.1.0")
        write_plugin(self.installed, "2.0.1")
        note = forgepact.refresh_stale_plugin(self.cfg)
        self.assertEqual(note, "Mod plugin updated from v2.0.1 to v2.1.0 before launch.")
        self.assertEqual(self.installed.read_bytes(), shipped)
        # Nothing but the plugin was written, and nothing was left beside it.
        self.assertEqual(sorted(p.name for p in self.aurie.iterdir()), ["BloodPactPlugin.dll", "YYToolkit.dll"])
        self.assertEqual((self.aurie / "YYToolkit.dll").read_bytes(), b"chain YYToolkit.dll")

    def test_target_a_plugin_from_before_1_3_20_is_replaced_too(self):
        self.release_build()
        shipped = write_plugin(self.bundled, "2.1.0")
        write_plugin(self.installed, None)
        self.assertEqual(forgepact.refresh_stale_plugin(self.cfg),
                         "Mod plugin updated from an old version to v2.1.0 before launch.")
        self.assertEqual(self.installed.read_bytes(), shipped)

    def test_a_different_yytoolkit_or_auriecore_leaves_the_plugin_to_install(self):
        # The plugin is compiled against YYToolkit's headers; replacing it
        # alone beside another YYToolkit risks the vtable crash, and those DLLs
        # are shared with other tools, so the player decides with Install.
        self.release_build()
        write_plugin(self.bundled, "2.1.0")
        for chain_file in (self.aurie / "YYToolkit.dll", self.bin / "AurieCore.dll"):
            with self.subTest(chain_file=chain_file.name):
                data = write_plugin(self.installed, "2.0.1")
                original = chain_file.read_bytes()
                chain_file.write_bytes(b"another tool's copy")
                try:
                    note = forgepact.refresh_stale_plugin(self.cfg)
                finally:
                    chain_file.write_bytes(original)
                self.assertIn("Install Mod Plugin", note)
                self.assertEqual(self.installed.read_bytes(), data)

    def test_newer_different_and_unknown_are_never_replaced(self):
        self.release_build()
        write_plugin(self.bundled, "2.1.0")
        for version, padding, advises in (("2.2.0", b"", False), ("2.1.0", b"another build", True)):
            with self.subTest(version=version):
                data = write_plugin(self.installed, version, padding)
                note = forgepact.refresh_stale_plugin(self.cfg)
                self.assertEqual(bool(note), advises, note)
                self.assertEqual(self.installed.read_bytes(), data)
        self.installed.write_bytes(b"MZ something else")
        self.assertIn("Install Mod Plugin", forgepact.refresh_stale_plugin(self.cfg))
        self.assertEqual(self.installed.read_bytes(), b"MZ something else")

    def test_a_failed_copy_keeps_the_old_plugin_and_says_why(self):
        self.release_build()
        write_plugin(self.bundled, "2.1.0")
        data = write_plugin(self.installed, "2.0.1")
        with patch.object(forgepact, "_atomic_verified_copy", side_effect=PermissionError(13, "Access is denied")):
            note = forgepact.refresh_stale_plugin(self.cfg)
        self.assertIn("could not be updated", note)
        self.assertIn("Access is denied", note)
        self.assertEqual(self.installed.read_bytes(), data)


def sha256_of(data: bytes) -> str:
    import hashlib
    return hashlib.sha256(data).hexdigest()


# Stand-ins for HS Offline Tracker sensor builds (#185). The product's real
# sets hold the real hashes, which tests/test_tracker_producer_pin.py pins
# against tools/toolchain-pins.json; here each set is swapped for these
# stand-ins' hashes, so the decision runs on files a temp directory can hold.
SENSOR_ABORTING = b"MZ sensor stand-in: a build that aborts every graceful close"
SENSOR_FIXED = b"MZ sensor stand-in: the build measured to close cleanly"
SENSOR_UNKNOWN = b"MZ sensor stand-in: a newer HS Offline Tracker build"
SENSOR_REPLACED = "HS Offline Tracker sensor replaced with the fixed build before launch."


class SensorFixture(PluginFixture):
    """PluginFixture plus HS Offline Tracker's sensor: the fixed build bundled,
    nothing in the game until a test puts one there."""

    def setUp(self):
        super().setUp()
        self.sensor = self.aurie / forgepact.TRACKER_SENSOR_DLL
        self.bundled_sensor = self.modfiles / forgepact.TRACKER_SENSOR_DLL
        self.bundled_sensor.write_bytes(SENSOR_FIXED)
        self.enterContext(patch.object(forgepact, "TRACKER_SENSOR_ABORTING",
                                       {sha256_of(SENSOR_ABORTING): "stand-in for an aborting build"}))
        self.enterContext(patch.object(forgepact, "TRACKER_SENSOR_CLEAN",
                                       {sha256_of(SENSOR_FIXED): "stand-in for the measured-clean build"}))


class RefreshTrackerSensorTests(SensorFixture):
    """refresh_stale_tracker_sensor(): an auto plugin refresh used to clear the
    only Install prompt, so a player who only pressed Launch kept the sensor
    that aborts every close (#185, #201)."""

    def test_baseline_no_sensor_in_the_game_writes_nothing_and_says_nothing(self):
        self.release_build()
        self.assertEqual(forgepact.refresh_stale_tracker_sensor(self.cfg), "")
        self.assertFalse(self.sensor.exists())

    def test_baseline_the_fixed_sensor_is_left_alone(self):
        self.release_build()
        self.sensor.write_bytes(SENSOR_FIXED)
        stamp = self.sensor.stat().st_mtime_ns
        self.assertEqual(forgepact.refresh_stale_tracker_sensor(self.cfg), "")
        self.assertEqual(self.sensor.stat().st_mtime_ns, stamp)

    def test_control_a_sensor_outside_the_aborting_set_is_never_replaced(self):
        # A newer HS Offline Tracker build, or a developer's own: not ours to
        # judge, so nothing is written and nothing is said.
        self.release_build()
        for data in (SENSOR_UNKNOWN, b""):
            with self.subTest(data=data):
                self.sensor.write_bytes(data)
                self.assertEqual(forgepact.refresh_stale_tracker_sensor(self.cfg), "")
                self.assertEqual(self.sensor.read_bytes(), data)

    def test_target_a_release_build_replaces_an_aborting_sensor(self):
        self.release_build()
        self.sensor.write_bytes(SENSOR_ABORTING)
        self.assertEqual(forgepact.refresh_stale_tracker_sensor(self.cfg), SENSOR_REPLACED)
        self.assertEqual(self.sensor.read_bytes(), SENSOR_FIXED)
        # Nothing but the sensor was written, and nothing was left beside it.
        self.assertEqual(sorted(p.name for p in self.aurie.iterdir()),
                         [forgepact.TRACKER_SENSOR_DLL, "YYToolkit.dll"])
        self.assertEqual((self.aurie / "YYToolkit.dll").read_bytes(), b"chain YYToolkit.dll")

    def test_a_source_run_only_advises(self):
        self.source_run()
        self.sensor.write_bytes(SENSOR_ABORTING)
        note = forgepact.refresh_stale_tracker_sensor(self.cfg)
        self.assertIn("HS Offline Tracker sensor", note)
        self.assertIn("Install Mod Plugin", note)
        self.assertEqual(self.sensor.read_bytes(), SENSOR_ABORTING)

    def test_a_different_yytoolkit_or_auriecore_leaves_the_sensor_to_install(self):
        # The same gate as the plugin's: the sensor is built against the
        # loader's headers, and the loader files are the player's to replace.
        self.release_build()
        for chain_file in (self.aurie / "YYToolkit.dll", self.bin / "AurieCore.dll"):
            with self.subTest(chain_file=chain_file.name):
                self.sensor.write_bytes(SENSOR_ABORTING)
                original = chain_file.read_bytes()
                chain_file.write_bytes(b"another tool's copy")
                try:
                    note = forgepact.refresh_stale_tracker_sensor(self.cfg)
                finally:
                    chain_file.write_bytes(original)
                self.assertIn("Install Mod Plugin", note)
                self.assertEqual(self.sensor.read_bytes(), SENSOR_ABORTING)

    def test_only_the_measured_clean_build_is_put_in_its_place(self):
        self.release_build()
        self.sensor.write_bytes(SENSOR_ABORTING)
        for bundled in (SENSOR_UNKNOWN, SENSOR_ABORTING, None):
            with self.subTest(bundled=bundled):
                if bundled is None:
                    self.bundled_sensor.unlink()
                else:
                    self.bundled_sensor.write_bytes(bundled)
                note = forgepact.refresh_stale_tracker_sensor(self.cfg)
                # Install copies only the bundle, so it could not help here:
                # the advice says what can, instead of sending the player to
                # an Install that changes nothing on every launch.
                self.assertNotIn("Install Mod Plugin", note)
                self.assertIn("does not carry the fixed one", note)
                self.assertIn("HS Offline Tracker to 0.1.4", note)
                self.assertEqual(self.sensor.read_bytes(), SENSOR_ABORTING)

    def test_a_failed_copy_keeps_the_old_sensor_and_says_why(self):
        self.release_build()
        self.sensor.write_bytes(SENSOR_ABORTING)
        with patch.object(forgepact, "_atomic_verified_copy", side_effect=PermissionError(13, "Access is denied")):
            note = forgepact.refresh_stale_tracker_sensor(self.cfg)
        self.assertIn("could not be updated", note)
        self.assertIn("Access is denied", note)
        self.assertEqual(self.sensor.read_bytes(), SENSOR_ABORTING)


class LaunchFixture(SensorFixture):
    """launch_modded_game() with every process spawn and native scan mocked,
    as tests/test_offline_launcher.py does. The fixed sensor is bundled and
    the game has none, unless a test puts one there."""

    def setUp(self):
        super().setUp()
        (self.bin / "steam_api64.dll").write_bytes(b"runtime fixture")
        self.processes = self.enterContext(patch.object(launcher, "processes", return_value=[(1, "steam.exe")]))
        self.enterContext(patch.object(launcher, "eac_service_status", return_value="stopped"))
        self.enterContext(patch.object(launcher, "start_steam_if_needed", return_value=(True, "Steam ready")))
        self.spawn = self.enterContext(patch.object(launcher.subprocess, "Popen", return_value=SimpleNamespace(pid=1234)))
        self.enterContext(patch.object(launcher, "threading", SimpleNamespace(Thread=Mock())))
        self.enterContext(patch.object(launcher, "os", SimpleNamespace(name="nt", environ=dict(os.environ), pathsep=os.pathsep)))
        self.enterContext(patch.object(launcher, "_STATE", {"phase": "idle", "message": "Ready", "pid": 0, "attempt": 0}))
        self.enterContext(patch.object(launcher, "EXE_FACTS_CACHE", {}))


class LaunchRefreshTests(LaunchFixture):
    def test_baseline_an_up_to_date_plugin_launches_with_the_same_message(self):
        self.release_build()
        write_plugin(self.bundled, "2.1.0")
        write_plugin(self.installed, "2.1.0")
        result = forgepact.launch_modded_game(self.cfg)
        self.assertEqual(result["ok"], "Modded Hero Siege launch requested through the built-in HS Offline Launcher.")
        self.spawn.assert_called_once()

    def test_target_an_older_plugin_is_updated_before_the_game_starts(self):
        self.release_build()
        shipped = write_plugin(self.bundled, "2.1.0")
        write_plugin(self.installed, "2.0.1")
        seen_at_spawn = []
        self.spawn.side_effect = lambda *a, **k: (seen_at_spawn.append(self.installed.read_bytes()),
                                                  SimpleNamespace(pid=1234))[1]
        result = forgepact.launch_modded_game(self.cfg)
        self.assertTrue(result["ok"].endswith("Mod plugin updated from v2.0.1 to v2.1.0 before launch."), result)
        self.assertEqual(seen_at_spawn, [shipped], "the game must start on the new plugin")

    def test_nothing_is_written_while_a_game_is_running(self):
        # The update runs as launch_game's `prepare`, after the fail-closed
        # process check: a running game, or processes that cannot be listed,
        # stop the launch before the file is touched.
        self.release_build()
        write_plugin(self.bundled, "2.1.0")
        data = write_plugin(self.installed, "2.0.1")
        for rows in ([(55, "Hero_Siege.exe")], OSError("scan failed")):
            with self.subTest(rows=rows):
                if isinstance(rows, Exception):
                    self.processes.side_effect = rows
                else:
                    self.processes.side_effect = None
                    self.processes.return_value = rows
                self.assertIn("err", forgepact.launch_modded_game(self.cfg))
                self.assertEqual(self.installed.read_bytes(), data)
        self.spawn.assert_not_called()

    def test_a_source_run_launches_with_the_advice_and_the_old_plugin(self):
        self.source_run()
        write_plugin(self.bundled, "2.1.0")
        data = write_plugin(self.installed, "2.0.1")
        result = forgepact.launch_modded_game(self.cfg)
        self.assertIn("this ForgePact ships v2.1.0", result["ok"])
        self.assertEqual(self.installed.read_bytes(), data)
        self.spawn.assert_called_once()


class LaunchSensorRefreshTests(LaunchFixture):
    def test_baseline_the_fixed_sensor_launches_with_the_same_message(self):
        self.release_build()
        write_plugin(self.bundled, "2.1.0")
        write_plugin(self.installed, "2.1.0")
        self.sensor.write_bytes(SENSOR_FIXED)
        result = forgepact.launch_modded_game(self.cfg)
        self.assertEqual(result["ok"], "Modded Hero Siege launch requested through the built-in HS Offline Launcher.")

    def test_target_an_aborting_sensor_is_replaced_before_the_game_starts(self):
        # The plugin is current, so nothing else would have asked for Install.
        self.release_build()
        write_plugin(self.bundled, "2.1.0")
        write_plugin(self.installed, "2.1.0")
        self.sensor.write_bytes(SENSOR_ABORTING)
        seen_at_spawn = []
        self.spawn.side_effect = lambda *a, **k: (seen_at_spawn.append(self.sensor.read_bytes()),
                                                  SimpleNamespace(pid=1234))[1]
        result = forgepact.launch_modded_game(self.cfg)
        self.assertTrue(result["ok"].endswith(SENSOR_REPLACED), result)
        self.assertEqual(seen_at_spawn, [SENSOR_FIXED], "the game must start on the fixed sensor")

    def test_an_older_plugin_and_an_aborting_sensor_are_both_updated(self):
        # The case a 2.2.x player who only presses Launch is in.
        self.release_build()
        shipped = write_plugin(self.bundled, "2.1.0")
        write_plugin(self.installed, "2.0.1")
        self.sensor.write_bytes(SENSOR_ABORTING)
        result = forgepact.launch_modded_game(self.cfg)
        self.assertTrue(result["ok"].endswith(
            "Mod plugin updated from v2.0.1 to v2.1.0 before launch. " + SENSOR_REPLACED), result)
        self.assertEqual(self.installed.read_bytes(), shipped)
        self.assertEqual(self.sensor.read_bytes(), SENSOR_FIXED)

    def test_nothing_is_written_while_a_game_is_running(self):
        self.release_build()
        write_plugin(self.bundled, "2.1.0")
        write_plugin(self.installed, "2.1.0")
        self.sensor.write_bytes(SENSOR_ABORTING)
        self.processes.return_value = [(55, "Hero_Siege.exe")]
        self.assertIn("err", forgepact.launch_modded_game(self.cfg))
        self.assertEqual(self.sensor.read_bytes(), SENSOR_ABORTING)
        self.spawn.assert_not_called()

    def test_the_install_instruction_is_said_once(self):
        # A source run advises on both files; the player reads the
        # instruction once, after both leads.
        self.source_run()
        write_plugin(self.bundled, "2.1.0")
        write_plugin(self.installed, "2.0.1")
        self.sensor.write_bytes(SENSOR_ABORTING)
        message = forgepact.launch_modded_game(self.cfg)["ok"]
        self.assertEqual(message.count("click Install Mod Plugin"), 1, message)
        self.assertIn("this ForgePact ships v2.1.0", message)
        self.assertIn("HS Offline Tracker sensor in the game is an old build", message)
        self.assertTrue(message.endswith(forgepact.INSTALL_ADVICE), message)

    def test_the_loader_is_hashed_once_per_launch(self):
        self.release_build()
        write_plugin(self.bundled, "2.1.0")
        write_plugin(self.installed, "2.0.1")
        self.sensor.write_bytes(SENSOR_ABORTING)
        with patch.object(forgepact, "_loader_matches_bundle",
                          wraps=forgepact._loader_matches_bundle) as gate:
            result = forgepact.launch_modded_game(self.cfg)
        self.assertEqual(gate.call_count, 1)
        self.assertTrue(result["ok"].endswith(SENSOR_REPLACED), result)


class InstallFixture(unittest.TestCase):
    """op_install_mod(): the answer carries pluginBuild, a game started during
    the backup work is caught before any copy, and a locked file is refused in
    words rather than as the handler's HTTP 500."""

    def setUp(self):
        temp = tempfile.TemporaryDirectory(prefix="forgepact-install-")
        self.addCleanup(temp.cleanup)
        root = Path(temp.name)
        self.bin = root / "game" / "bin"
        self.bin.mkdir(parents=True)
        self.exe = self.bin / "Hero_Siege.exe"
        write_test_pe(self.exe, b"new")
        self.sources = root / "sources"
        self.sources.mkdir()
        for name in ("AurieCore.dll", "YYToolkit.dll", "AuriePatcher.exe"):
            (self.sources / name).write_bytes(("source-" + name).encode("ascii"))
        write_plugin(self.sources / "BloodPactPlugin.dll", "2.1.0")
        self.cfg = {"game_exe": str(self.exe)}
        self.running = self.enterContext(patch.object(forgepact, "game_running", return_value=False))
        self.enterContext(patch.object(forgepact, "MODFILE_SOURCES", [self.sources]))
        self.enterContext(patch.object(forgepact, "PLUGIN_SOURCES", [self.sources]))
        self.enterContext(patch.object(forgepact, "_PLUGIN_FACTS", {}))

    def patcher(self, command, **_kwargs):
        write_test_pe(Path(command[1]), b"new", patched=True)
        return SimpleNamespace(returncode=0, stdout="patched", stderr="")

    def install(self):
        with patch.object(forgepact.subprocess, "run", side_effect=self.patcher):
            return forgepact.op_install_mod(self.cfg)


class InstallSensorTests(InstallFixture):
    """Install Mod Plugin's copy of HS Offline Tracker's sensor is decided by
    hash (#185): it used to copy whatever was bundled over whatever was in the
    game, so an unprepared source checkout put the aborting build back and a
    newer Tracker's sensor was downgraded."""

    OTHER_CLEAN = b"MZ sensor stand-in: an earlier clean build ForgePact pinned"

    def setUp(self):
        super().setUp()
        self.target = self.bin / "mods" / "aurie" / forgepact.TRACKER_SENSOR_DLL
        self.bundled_sensor = self.sources / forgepact.TRACKER_SENSOR_DLL
        self.bundled_sensor.write_bytes(SENSOR_FIXED)
        self.enterContext(patch.object(forgepact, "TRACKER_SENSOR_ABORTING",
                                       {sha256_of(SENSOR_ABORTING): "stand-in"}))
        self.enterContext(patch.object(forgepact, "TRACKER_SENSOR_CLEAN",
                                       {sha256_of(SENSOR_FIXED): "stand-in",
                                        sha256_of(self.OTHER_CLEAN): "stand-in"}))

    def put(self, data):
        self.target.parent.mkdir(parents=True, exist_ok=True)
        self.target.write_bytes(data)

    def test_baseline_a_missing_sensor_is_installed(self):
        result = self.install()
        self.assertIn("HS Offline Tracker sensor installed", result["ok"])
        self.assertEqual(self.target.read_bytes(), SENSOR_FIXED)

    def test_an_aborting_or_known_clean_sensor_is_replaced(self):
        for data in (SENSOR_ABORTING, self.OTHER_CLEAN):
            with self.subTest(data=data):
                self.put(data)
                result = self.install()
                self.assertIn("HS Offline Tracker sensor installed", result["ok"])
                self.assertEqual(self.target.read_bytes(), SENSOR_FIXED)

    def test_control_an_unknown_sensor_is_left_as_it_was(self):
        self.put(SENSOR_UNKNOWN)
        result = self.install()
        self.assertIn("HS Offline Tracker sensor left as it was", result["ok"])
        self.assertEqual(self.target.read_bytes(), SENSOR_UNKNOWN)

    def test_an_unreadable_sensor_stops_install_before_anything_is_written(self):
        # The sensor is decided before the first copy: a read failure must not
        # surface as a write failure after the other DLLs were replaced.
        self.put(SENSOR_ABORTING)
        with patch.object(forgepact, "_tracker_sensor_install",
                          side_effect=PermissionError(13, "Access is denied")):
            result = self.install()
        self.assertIn("Could not read HSOfflineTrackerProducer.dll", result.get("err", ""), result)
        self.assertFalse((self.bin / "AurieCore.dll").exists())
        self.assertFalse((self.bin / "mods" / "aurie" / "BloodPactPlugin.dll").exists())

    def test_an_aborting_bundle_is_never_installed(self):
        # A source checkout that has not re-run Prepare-Plugin.bat.
        self.bundled_sensor.write_bytes(SENSOR_ABORTING)
        for present in (None, SENSOR_FIXED):
            with self.subTest(present=present):
                if present is None:
                    self.target.unlink(missing_ok=True)
                else:
                    self.put(present)
                result = self.install()
                self.assertIn("HS Offline Tracker sensor not installed", result["ok"])
                self.assertEqual(self.target.read_bytes() if self.target.exists() else None, present)


class InstallTests(InstallFixture):
    def test_the_answer_carries_the_plugin_build_so_the_panel_updates_at_once(self):
        result = self.install()
        self.assertIn("ok", result, result)
        self.assertEqual(result["pluginBuild"], {"state": "current", "installed": "2.1.0", "bundled": "2.1.0"})

    def test_a_game_started_during_the_backup_work_stops_the_copy(self):
        self.running.side_effect = [False, True]
        result = self.install()
        self.assertEqual(result, {"err": "Close the game first, then click Install again."})
        self.assertFalse((self.bin / "AurieCore.dll").exists())
        self.assertFalse((self.bin / "mods" / "aurie" / "BloodPactPlugin.dll").exists())

    def test_a_locked_dll_is_refused_in_words(self):
        # op_install_mod imports shutil inside itself, so the module attribute
        # is what its copies (and the backup's staged copy) resolve.
        import shutil

        def copy2(source, destination, *args, **kwargs):
            if Path(destination).name == "BloodPactPlugin.dll":
                raise PermissionError(13, "The process cannot access the file because it is being used by another process")
            return shutil.copyfile(source, destination)

        with patch("shutil.copy2", side_effect=copy2):
            result = self.install()
        self.assertIn("err", result)
        self.assertIn("Could not write BloodPactPlugin.dll", result["err"])
        self.assertIn("being used by another process", result["err"])
        self.assertIn("click Install again", result["err"])


class StatePayloadTests(unittest.TestCase):
    def test_api_state_carries_plugin_build(self):
        from test_satanic_panel import PanelSandbox
        with PanelSandbox() as sandbox:
            connection = HTTPConnection("127.0.0.1", sandbox.port, timeout=5)
            try:
                connection.request("GET", "/api/state")
                state = json.loads(connection.getresponse().read())
            finally:
                connection.close()
        # The sandbox's game folder has no plugin in it.
        self.assertEqual(state["pluginBuild"], {"state": "missing", "installed": None, "bundled": None})


class BootLineContractTests(unittest.TestCase):
    """The panel reads the plugin's version from the boot line ModManager.hpp
    writes; the two must stay in step."""

    def test_the_plugin_source_writes_the_line_the_panel_reads(self):
        header = (ROOT / "plugin" / "include" / "ForgePact" / "ModManager.hpp").read_text(encoding="utf-8")
        literal = re.search(r'Out\("(==== BloodPact plugin loaded ==== v)" FORGEPACT_VERSION\)', header)
        self.assertIsNotNone(literal, "ModManager.hpp's boot line changed shape; update _PLUGIN_VERSION_RE with it")
        version = re.search(r'#define FORGEPACT_VERSION "([^"]+)"',
                            (ROOT / "plugin" / "include" / "ForgePact" / "Version.hpp").read_text(encoding="utf-8"))
        line = (literal.group(1) + version.group(1)).encode("ascii")
        found = forgepact._PLUGIN_VERSION_RE.search(b"\0" + line + b"\0")
        self.assertIsNotNone(found)
        self.assertEqual(found.group(1).decode("ascii"), version.group(1))

    def test_positive_control_on_a_built_plugin(self):
        # The one check a stand-in cannot make: that a DLL the compiler really
        # produced carries the line whole. Runs wherever the plugin has been
        # built (plugin_build\build.bat release, or Prepare-Plugin.bat).
        candidates = [ROOT / "plugin_build" / "BloodPactPlugin_ship.dll",
                      ROOT / "modfiles_shipped" / "BloodPactPlugin.dll"]
        built = [path for path in candidates if path.is_file()]
        if not built:
            self.skipTest("no built BloodPactPlugin.dll in this checkout")
        version = re.search(r'#define FORGEPACT_VERSION "([^"]+)"',
                            (ROOT / "plugin" / "include" / "ForgePact" / "Version.hpp").read_text(encoding="utf-8"))
        for path in built:
            with self.subTest(dll=path.name):
                facts = forgepact.plugin_dll_facts(path, use_cache=False)
                self.assertTrue(facts["marker"], path)
                self.assertEqual(facts["version"], version.group(1), path)


if __name__ == "__main__":
    unittest.main()
