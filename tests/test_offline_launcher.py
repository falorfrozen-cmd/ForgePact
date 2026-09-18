"""Embedded launcher tests. Every process spawn and native scan is mocked."""
import ast
import inspect
import importlib.util
import json
import os
import sys
import tempfile
import threading
import unittest
from http.client import HTTPConnection
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch, Mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))
import forgepact
import offline_launcher as launcher
from test_mod_backup import write_test_pe


class OfflineLaunchTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="forgepact-launch-")
        self.addCleanup(self.temp.cleanup)
        # resolve(): the launcher resolves the exe, and a CI TEMP is an 8.3 short path (RUNNER~1).
        self.root = Path(self.temp.name).resolve()
        self.exe = self.root / "Game with spaces" / "bin" / "Hero_Siege.exe"
        self.exe.parent.mkdir(parents=True)
        write_test_pe(self.exe, b"new", patched=True)
        self.runtime = self.exe.parent / "steam_api64.dll"
        self.runtime.write_bytes(b"runtime fixture")
        self.cfg = {"game_exe": str(self.exe)}
        aurie = self.exe.parent / "mods" / "aurie"
        aurie.mkdir(parents=True)
        for path in (self.exe.parent / "AurieCore.dll", aurie / "YYToolkit.dll", aurie / "BloodPactPlugin.dll"):
            path.write_bytes(b"mod fixture")
        self.processes = self.enterContext(patch.object(launcher, "processes", return_value=[(1, "steam.exe")]))
        self.eac = self.enterContext(patch.object(launcher, "eac_service_status", return_value="stopped"))
        self.steam = self.enterContext(patch.object(launcher, "start_steam_if_needed", return_value=(True, "Steam ready")))
        self.spawn = self.enterContext(patch.object(launcher.subprocess, "Popen", return_value=SimpleNamespace(pid=1234)))
        # Never run verify_launch's real delay or a native thread in launch tests.
        self.thread = Mock()
        self.enterContext(patch.object(launcher, "threading", SimpleNamespace(Thread=self.thread)))
        self.prepare = self.enterContext(patch.object(forgepact, "ensure_ri_cache", return_value=False))
        self.enterContext(patch.object(launcher, "os", SimpleNamespace(name="nt", environ=dict(os.environ), pathsep=os.pathsep)))
        self.enterContext(patch.object(launcher, "_STATE", {"phase":"idle", "message":"Ready", "pid":0, "attempt":0}))
        self.enterContext(patch.object(launcher, "EXE_FACTS_CACHE", {}))

    def launch(self):
        return forgepact.launch_modded_game(self.cfg)

    def test_launch_uses_panel_path_steam_environment_and_argument_list(self):
        original = self.exe.read_bytes()
        result = self.launch()
        self.assertEqual(result["pid"], 1234)
        args, kwargs = self.spawn.call_args
        self.assertEqual(args, ([str(self.exe.resolve())],))
        self.assertEqual(kwargs["cwd"], str(self.exe.parent))
        self.assertEqual(kwargs["env"]["SteamAppId"], "269210")
        self.assertEqual(kwargs["env"]["SteamGameId"], "269210")
        self.assertNotIn("shell", kwargs)
        self.prepare.assert_called_once_with(self.cfg)
        self.assertEqual(self.exe.read_bytes(), original)
        self.assertEqual(result["launch"]["phase"], "started")

    def test_parent_runtime_is_added_only_to_child_environment(self):
        self.runtime.rename(self.exe.parent.parent / self.runtime.name)
        original = dict(launcher.os.environ)
        self.launch()
        self.assertTrue(self.spawn.call_args.kwargs["env"]["PATH"].startswith(str(self.exe.parent.parent)+os.pathsep))
        self.assertEqual(dict(launcher.os.environ), original)

    def test_missing_plugin_never_starts_steam_or_game(self):
        (self.exe.parent / "mods" / "aurie" / "BloodPactPlugin.dll").unlink()
        self.assertIn("Install Mod Plugin", self.launch()["err"])
        self.spawn.assert_not_called()
        self.steam.assert_not_called()
        self.prepare.assert_not_called()

    def test_missing_runtime_and_invalid_executable_fail_without_spawning(self):
        self.runtime.unlink()
        self.assertIn("steam_api64.dll", self.launch()["err"])
        self.runtime.write_bytes(b"fixture")
        self.exe.write_bytes(b"not a PE")
        self.assertIn("not a valid", self.launch()["err"])
        self.spawn.assert_not_called()
        self.steam.assert_not_called()

    def test_all_unconfirmed_or_active_protection_states_block(self):
        for state in ("running", "unknown", "transitioning"):
            with self.subTest(state=state):
                self.eac.return_value = state
                self.assertIn("err", self.launch())
        self.spawn.assert_not_called()
        self.steam.assert_not_called()

    def test_active_protected_launcher_blocks_even_when_service_is_stopped(self):
        self.processes.return_value = [(42, "start_protected_game.exe")]
        self.assertIn("EAC is currently active", self.launch()["err"])
        self.spawn.assert_not_called()

    def test_process_scan_failure_and_existing_game_block(self):
        self.processes.side_effect = OSError("scan failed")
        self.assertIn("could not be verified", self.launch()["err"])
        self.processes.side_effect = None
        self.processes.return_value = [(55, "Hero_Siege.exe")]
        self.assertIn("already running", self.launch()["err"])
        self.spawn.assert_not_called()

    def test_steam_error_is_returned_without_a_direct_launch_fallback(self):
        self.steam.return_value = (False, "Steam was not found")
        self.assertEqual(self.launch()["err"], "Steam was not found")
        self.spawn.assert_not_called()
        self.assertEqual(launcher.launch_status()["phase"], "error")

    def test_protection_is_rechecked_after_steam_starts(self):
        self.processes.side_effect = [[], [(42, "EasyAntiCheat_EOS.exe")]]
        self.assertIn("EAC is currently active", self.launch()["err"])
        self.spawn.assert_not_called()
        self.prepare.assert_not_called()

    def test_plugin_removed_during_steam_start_is_caught(self):
        def remove_plugin():
            (self.exe.parent / "AurieCore.dll").unlink()
            return True, "Steam started"
        self.steam.side_effect = remove_plugin
        self.assertIn("Install Mod Plugin", self.launch()["err"])
        self.spawn.assert_not_called()

    def test_final_protection_check_runs_after_cache_preparation(self):
        self.processes.side_effect = [[], [], [(42, "EasyAntiCheat_EOS.exe")]]
        self.assertIn("EAC is currently active", self.launch()["err"])
        self.prepare.assert_called_once()
        self.spawn.assert_not_called()

    def test_stale_validation_cache_is_not_used_for_launch(self):
        self.assertTrue(launcher.validate_game(self.exe)[0])
        payload = self.exe.read_bytes()
        timestamp = self.exe.stat().st_mtime_ns
        self.exe.write_bytes(b"ZZ" + payload[2:])
        os.utime(self.exe, ns=(timestamp, timestamp))
        self.assertIn("not a valid", self.launch()["err"])
        self.spawn.assert_not_called()

    def test_second_request_is_rejected_and_lock_released_after_failure(self):
        launcher.LAUNCH_LOCK.acquire()
        try:
            self.assertIn("already in progress", self.launch()["err"])
            self.spawn.assert_not_called()
        finally:
            launcher.LAUNCH_LOCK.release()
        self.spawn.side_effect = OSError("launch failed")
        self.assertIn("Offline launch failed", self.launch()["err"])
        self.assertFalse(launcher.LAUNCH_LOCK.locked())
        self.spawn.side_effect = None
        self.assertIn("ok", self.launch())

    def test_one_shot_verification_reports_exit_and_ignores_old_attempt(self):
        self.launch()
        attempt = launcher.launch_status()["attempt"]
        with patch.object(launcher.time, "sleep"):
            self.processes.return_value = [(1234, "Hero_Siege.exe")]
            launcher.verify_launch(1234, attempt)
            self.assertEqual(launcher.launch_status()["phase"], "verified")
            self.processes.return_value = []
            launcher.verify_launch(1234, attempt-1)
            self.assertEqual(launcher.launch_status()["phase"], "verified")
            launcher.verify_launch(1234, attempt)
            self.assertEqual(launcher.launch_status()["phase"], "error")

    def test_poll_reads_cached_launch_result_without_native_work(self):
        self.assertEqual(launcher.launch_status()["phase"], "idle")
        self.processes.assert_not_called()
        self.eac.assert_not_called()
        self.spawn.assert_not_called()

    def test_panel_http_launch_route_and_cached_status_use_embedded_engine(self):
        config = self.root / "forgepact.json"
        config.write_text(json.dumps(self.cfg), encoding="utf-8")
        with patch.object(forgepact, "CONFIG", config), patch.object(forgepact, "game_running", return_value=False):
            server = forgepact.ThreadingHTTPServer(("127.0.0.1", 0), forgepact.H)
            worker = threading.Thread(target=server.serve_forever, daemon=True)
            worker.start()
            try:
                connection = HTTPConnection("127.0.0.1", server.server_port, timeout=5)
                connection.request("POST", "/api/launch", "{}", {"Content-Type":"application/json"})
                result = json.loads(connection.getresponse().read())
                self.assertEqual(result["pid"], 1234)
                self.assertEqual(self.spawn.call_args.kwargs["env"]["SteamAppId"], "269210")
                self.processes.reset_mock()
                connection.request("GET", "/api/state")
                state = json.loads(connection.getresponse().read())
                self.assertEqual(state["launch"]["phase"], "started")
                self.processes.assert_not_called()
                connection.close()
            finally:
                server.shutdown();server.server_close();worker.join(timeout=5)


class LauncherReuseTests(unittest.TestCase):
    def test_package_refuses_missing_embedded_launcher_before_any_build(self):
        build_path = Path(__file__).resolve().parents[1] / "build_release.py"
        spec = importlib.util.spec_from_file_location("package_launcher_test", build_path)
        builder = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(builder)
        with tempfile.TemporaryDirectory() as tmp:
            with patch.object(builder, "OFFLINE_LAUNCHER", Path(tmp) / "missing.py"), patch.object(builder.subprocess, "run") as run:
                self.assertEqual(builder.main(), 1)
                run.assert_not_called()

    def test_audited_helpers_match_the_source_launcher(self):
        source = Path(__file__).resolve().parents[2] / "HS-Offline-Launcher" / "src" / "hs_offline_launcher.py"
        if not source.exists():
            self.skipTest("Upstream comparison requires the toolkit checkout")
        upstream = {node.name: node for node in ast.parse(source.read_text(encoding="utf-8-sig")).body
                    if isinstance(node, (ast.FunctionDef, ast.ClassDef))}
        for name in launcher.UPSTREAM_DEFINITIONS:
            actual = ast.parse(inspect.getsource(getattr(launcher, name))).body[0]
            self.assertEqual(ast.dump(actual), ast.dump(upstream[name]), name)

    def test_source_import_needs_no_separate_launcher_and_starts_nothing(self):
        source = inspect.getsource(launcher)
        self.assertNotIn("import hs_offline_launcher", source)
        self.assertFalse(hasattr(launcher, "ThreadingHTTPServer"))
        self.assertFalse(hasattr(launcher, "load_config"))
        self.assertIn("MIT License", source)
        self.assertIn('ctypes.WinDLL("kernel32", use_last_error=True)', source)
        spec = importlib.util.spec_from_file_location("isolated_launcher", launcher.__file__)
        module = importlib.util.module_from_spec(spec)
        with patch("subprocess.Popen") as spawn, patch("threading.Thread") as worker, patch.object(Path, "mkdir") as mkdir:
            spec.loader.exec_module(module)
            spawn.assert_not_called();worker.assert_not_called();mkdir.assert_not_called()


if __name__ == "__main__":
    unittest.main()
