"""Exercise the panel's existing pool API with isolated settings, never game IPC."""
import copy
import json
import sys
import tempfile
import threading
import unittest
from http.client import HTTPConnection
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))
import forgepact


class PanelSandbox:
    def __enter__(self):
        self.temp = tempfile.TemporaryDirectory(prefix="forgepact-panel-")
        self.config = Path(self.temp.name) / "forgepact.json"
        cfg = copy.deepcopy(forgepact.DEFAULTS)
        cfg["game_exe"] = str(Path(self.temp.name) / "Hero_Siege.exe")
        cfg["density"] = 3
        cfg["auto_apply"] = False
        self.config.write_text(json.dumps(cfg), encoding="utf-8")
        self.patches = [patch.object(forgepact, "CONFIG", self.config),
                        patch.object(forgepact, "game_running", return_value=False),
                        patch.object(forgepact, "mod_chain", return_value={}),
                        patch.object(forgepact, "send_cmds")]
        self.mocks = [p.start() for p in self.patches]
        self.server = forgepact.ThreadingHTTPServer(("127.0.0.1", 0), forgepact.H)
        self.port = self.server.server_port
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        return self

    def __exit__(self, *args):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=5)
        for p in reversed(self.patches):
            p.stop()
        self.temp.cleanup()

    def request(self, change=None):
        connection = HTTPConnection("127.0.0.1", self.port, timeout=5)
        try:
            if change is None:
                connection.request("GET", "/api/state")
            else:
                body = json.dumps({"section": "satanic_mods", **change})
                connection.request("POST", "/api/set", body,
                                   {"Content-Type": "application/json"})
            response = connection.getresponse()
            return response.status, json.loads(response.read())
        finally:
            connection.close()

    def at_minimum(self):
        cfg = json.loads(self.config.read_text(encoding="utf-8"))
        for polarity, floor in (("buff", 3), ("debuff", 2)):
            for index, key in enumerate(cfg["satanic_mods"][polarity]):
                cfg["satanic_mods"][polarity][key] = index < floor
        self.config.write_text(json.dumps(cfg), encoding="utf-8")
        return cfg


class SatanicPoolTests(unittest.TestCase):
    def setUp(self):
        self.sandbox = PanelSandbox()
        self.sandbox.__enter__()
        self.addCleanup(self.sandbox.__exit__)

    def test_initial_state_has_complete_sdk_rows_and_preserves_preferences(self):
        code, state = self.sandbox.request()
        self.assertEqual(code, 200)
        self.assertEqual(len(state["satanicBuffs"]), 25)
        self.assertEqual(len(state["satanicDebuffs"]), 26)
        self.assertEqual(state["minEnabledSatanicBuffs"], 3)
        self.assertEqual(state["minEnabledSatanicDebuffs"], 2)
        self.assertEqual(state["cfg"]["density"], 3)
        self.assertFalse(state["cfg"]["auto_apply"])

    def test_saved_selections_are_loaded_without_resetting_to_defaults(self):
        cfg = self.sandbox.at_minimum()
        _, state = self.sandbox.request()
        self.assertEqual(state["cfg"]["satanic_mods"], cfg["satanic_mods"])

    def test_minimum_rejection_does_not_write_or_send(self):
        self.sandbox.at_minimum()
        before = self.sandbox.config.read_bytes()
        for polarity in ("buff", "debuff"):
            with self.subTest(polarity=polarity):
                code, result = self.sandbox.request(dict(polarity=polarity, key="1", value=False))
                self.assertEqual(code, 400)
                self.assertIn("must stay enabled", result["err"])
                self.assertEqual(self.sandbox.config.read_bytes(), before)
        self.sandbox.mocks[3].assert_not_called()

    def test_enable_replacement_then_disable_original_persists(self):
        cfg = self.sandbox.at_minimum()
        for key, value in (("4", True), ("1", False)):
            code, result = self.sandbox.request(dict(polarity="buff", key=key, value=value))
            self.assertEqual(code, 200)
            self.assertNotIn("err", result)
        _, state = self.sandbox.request()
        pool = state["cfg"]["satanic_mods"]["buff"]
        self.assertTrue(pool["4"])
        self.assertFalse(pool["1"])
        self.assertEqual(sum(pool.values()), 3)
        self.assertEqual(state["cfg"]["satanic_mods"]["debuff"], cfg["satanic_mods"]["debuff"])

    def test_enable_all_restores_only_the_requested_pool(self):
        cfg = self.sandbox.at_minimum()
        code, result = self.sandbox.request(dict(polarity="buff", keys=list(cfg["satanic_mods"]["buff"]), value=True))
        self.assertEqual(code, 200)
        self.assertTrue(all(result["cfg"]["satanic_mods"]["buff"].values()))
        self.assertEqual(result["cfg"]["satanic_mods"]["debuff"], cfg["satanic_mods"]["debuff"])
        self.assertEqual(result["cfg"]["density"], 3)

    def test_restore_defaults_preserves_other_settings(self):
        cfg = self.sandbox.at_minimum()
        for polarity in ("buff", "debuff"):
            code, _ = self.sandbox.request(dict(polarity=polarity, keys=list(cfg["satanic_mods"][polarity]), value=True))
            self.assertEqual(code, 200)
        _, state = self.sandbox.request()
        expected = copy.deepcopy(cfg)
        expected["satanic_mods"] = copy.deepcopy(forgepact.DEFAULTS["satanic_mods"])
        self.assertEqual(state["cfg"], expected)

    def test_live_pool_change_sends_one_existing_command(self):
        self.sandbox.mocks[1].return_value = True
        code, _ = self.sandbox.request(dict(polarity="buff", key="1", value=False))
        self.assertEqual(code, 200)
        self.sandbox.mocks[3].assert_called_once()
        self.assertEqual(self.sandbox.mocks[3].call_args.args[0], ["satmods buff 1"])

    def test_invalid_id_is_rejected_without_mutation(self):
        before = self.sandbox.config.read_bytes()
        code, result = self.sandbox.request(dict(polarity="buff", key="invalid", value=False))
        self.assertEqual(code, 400)
        self.assertIn("unknown", result["err"])
        self.assertEqual(self.sandbox.config.read_bytes(), before)


if __name__ == "__main__":
    unittest.main()
