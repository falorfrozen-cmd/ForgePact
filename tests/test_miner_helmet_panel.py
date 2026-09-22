"""The item button uses real HTTP against isolated settings; no game or saves."""
import json
import unittest
from http.client import HTTPConnection
from unittest.mock import patch
from test_satanic_panel import PanelSandbox
import forgepact


class MinerHelmetPanelTests(unittest.TestCase):
    def setUp(self):
        self.sandbox = PanelSandbox().__enter__()
        self.addCleanup(self.sandbox.__exit__)

    def request(self, token='a' * 32):
        connection = HTTPConnection('127.0.0.1', self.sandbox.port, timeout=5)
        try:
            connection.request('POST', '/api/miner-helmet', json.dumps({'request': token}), {'Content-Type': 'application/json'})
            response = connection.getresponse()
            return response.status, json.loads(response.read())
        finally:
            connection.close()

    def test_closed_game_does_not_queue_a_future_item(self):
        before = self.sandbox.config.read_bytes()
        self.assertEqual(self.request()[0], 409)
        self.sandbox.mocks[3].assert_not_called()
        self.assertEqual(before, self.sandbox.config.read_bytes())

    def test_wrong_plugin_refuses(self):
        self.sandbox.mocks[1].return_value = True
        with patch.object(forgepact, 'plugin_mod_state', return_value={}):
            self.assertEqual(self.request()[0], 409)
        self.sandbox.mocks[3].assert_not_called()

    def test_valid_request_queues_exactly_one_command_without_touching_settings(self):
        before = self.sandbox.config.read_bytes()
        self.sandbox.mocks[1].return_value = True
        with patch.object(forgepact, 'plugin_mod_state', return_value={'minerHelmet': {'available': True}}):
            status, payload = self.request()
        self.assertEqual(status, 200)
        self.assertTrue(payload['queued'])
        self.assertNotIn('ok', payload)  # A queue acknowledgement is not a successful item drop.
        self.sandbox.mocks[3].assert_called_once()
        self.assertEqual(self.sandbox.mocks[3].call_args.args[0], ['minerhelm grant ' + 'a' * 32])
        self.assertEqual(before, self.sandbox.config.read_bytes())

    def test_invalid_token_never_reaches_ipc(self):
        for token in (None, 5, 'abc', 'a' * 31 + '\n', 'a' * 33):
            with self.subTest(token=token):
                self.assertEqual(self.request(token)[0], 400)
        self.sandbox.mocks[3].assert_not_called()

    def test_ipc_failure_is_not_reported_as_queued(self):
        self.sandbox.mocks[1].return_value = True
        self.sandbox.mocks[3].return_value = 'ERROR: no IPC folder'
        with patch.object(forgepact, 'plugin_mod_state', return_value={'minerHelmet': {'available': True}}):
            status, payload = self.request()
        self.assertEqual(status, 503)
        self.assertIn('err', payload)
