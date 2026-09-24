"""Ore amount UI/IPC contract: mining is distinct from legacy ore drop rolls."""
import copy
import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'src'))
import forgepact
from test_satanic_panel import PanelSandbox


class MiningOrePanelTests(unittest.TestCase):
    def test_baseline_never_installs_mining_hooks(self):
        cfg = copy.deepcopy(forgepact.DEFAULTS)
        self.assertFalse(any(c.startswith('miningore ') for c in forgepact.build_cmds(cfg)))

    def test_target_setting_has_a_separate_default(self):
        self.assertEqual(1, forgepact.DEFAULTS['drops']['mining_ore'])

    def test_target_emits_mining_command_not_legacy_dropmult(self):
        cfg = copy.deepcopy(forgepact.DEFAULTS)
        cfg['drops']['mining_ore'] = 5
        commands = forgepact.build_cmds(cfg)
        self.assertIn('miningore 5', commands)
        self.assertNotIn('dropmult mining_ore 5', commands)
        self.assertNotIn('dropmult ore 5', commands)

    def test_legacy_ore_setting_does_not_activate_mining(self):
        cfg = copy.deepcopy(forgepact.DEFAULTS)
        cfg['drops']['ore'] = 100
        self.assertFalse(any(c.startswith('miningore ') for c in forgepact.build_cmds(cfg)))

    def test_amount_bounds_and_explicit_reset(self):
        for value, expected in ((-8, 1), (1, 1), (5, 5), (10, 10), (999, 10),
                                ('bad', 1), (None, 1), (float('nan'), 1), (float('inf'), 1)):
            with self.subTest(value=value):
                self.assertEqual(expected, forgepact.drop_multiplier('mining_ore', value))
        self.assertEqual('miningore 1', forgepact.drop_command('mining_ore', 1))

    def test_gold_behavior_is_unchanged(self):
        self.assertEqual('dropmult gold 100', forgepact.drop_command('gold', 100))

    def test_unknown_control_is_rejected(self):
        with self.assertRaises(ValueError):
            forgepact.drop_command('not_a_setting', 5)


class MiningOreHttpTests(unittest.TestCase):
    def setUp(self):
        self.sandbox = PanelSandbox()
        self.sandbox.__enter__()
        self.addCleanup(self.sandbox.__exit__)

    def test_save_and_reload_preserves_other_settings_without_game_ipc(self):
        _, before = self.sandbox.request()
        code, _ = self.sandbox.request(dict(section='drops', key='mining_ore', value=5))
        self.assertEqual(200, code)
        _, after = self.sandbox.request()
        expected = copy.deepcopy(before['cfg'])
        expected['drops']['mining_ore'] = 5
        self.assertEqual(expected, after['cfg'])
        self.sandbox.mocks[3].assert_not_called()

    def test_live_commands_use_saved_bounds_and_can_reset(self):
        self.sandbox.mocks[1].return_value = True
        for requested, expected in ((5, 5), (999, 10), (1, 1)):
            with self.subTest(requested=requested):
                self.sandbox.mocks[3].reset_mock()
                code, _ = self.sandbox.request(dict(section='drops', key='mining_ore', value=requested))
                self.assertEqual(200, code)
                self.assertEqual(expected, json.loads(self.sandbox.config.read_text())['drops']['mining_ore'])
                self.sandbox.mocks[3].assert_called_once()
                self.assertEqual([f'miningore {expected}'], self.sandbox.mocks[3].call_args.args[0])

    def test_unknown_drop_rejected_without_mutation(self):
        before = self.sandbox.config.read_bytes()
        code, result = self.sandbox.request(dict(section='drops', key='ore', value=5))
        self.assertEqual(400, code)
        self.assertIn('unknown', result['err'])
        self.assertEqual(before, self.sandbox.config.read_bytes())
        self.sandbox.mocks[3].assert_not_called()


if __name__ == '__main__':
    unittest.main()
