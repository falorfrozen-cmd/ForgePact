"""The Miner's Helmet card after the test button was retired (2026-09-23).

The helmet is forged in the Item Editor (Item Forge -> Forge a signature item
-> Miner's Helmet); the panel only reports whether it is worn. Real HTTP
against isolated settings keeps the old test path from coming back: no Create
button, no endpoint that can queue an item, no Prototype label. No game or
saves are involved.
"""
import json
import unittest
from http.client import HTTPConnection
from pathlib import Path
from test_satanic_panel import PanelSandbox
import forgepact

PANEL = Path(forgepact.__file__)


class MinerHelmetPanelTests(unittest.TestCase):
    def setUp(self):
        self.sandbox = PanelSandbox().__enter__()
        self.addCleanup(self.sandbox.__exit__)

    def test_create_endpoint_is_gone_and_queues_nothing(self):
        before = self.sandbox.config.read_bytes()
        self.sandbox.mocks[1].return_value = True   # the game is running
        connection = HTTPConnection('127.0.0.1', self.sandbox.port, timeout=5)
        try:
            connection.request('POST', '/api/miner-helmet', json.dumps({'request': 'a' * 32}),
                               {'Content-Type': 'application/json'})
            response = connection.getresponse()
            response.read()
        finally:
            connection.close()
        self.assertEqual(response.status, 404)
        self.sandbox.mocks[3].assert_not_called()   # send_cmds: nothing reaches the game
        self.assertEqual(before, self.sandbox.config.read_bytes())

    def test_card_describes_the_helmet_without_a_create_button(self):
        source = PANEL.read_text(encoding='utf-8')
        card = source[source.index('id="minerHelmetCard"'):source.index('id="minerHelmetStatus"')]
        self.assertNotIn('<button', card)
        self.assertNotIn('Prototype', card)
        self.assertIn('Item Editor', card)
        self.assertIn('Vein Resonance', card)
        self.assertIn('Mining Ore Amount slider', card)
        for gone in ('grantMinerHelmet', 'minerHelmetResult', 'minerHelmetBusy',
                     '/api/miner-helmet', 'minerhelm grant', 'Create test helmet'):
            self.assertNotIn(gone, source)

    def test_mining_slider_is_no_longer_marked_experimental(self):
        self.assertIn(('mining_ore', 'Mining Ore Amount', ''), forgepact.DROPS)
        self.assertNotIn('In-game verification pending', PANEL.read_text(encoding='utf-8'))


if __name__ == '__main__':
    unittest.main()
