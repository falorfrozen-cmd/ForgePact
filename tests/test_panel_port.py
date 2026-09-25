"""The panel's ports stay clear of the other toolkit tools' ports (1.4.6).

The Item Editor keeps 8765-8774 for itself, and the Toolkit Hub checks
ForgePact on its first candidate port. While 8766 led the list, an open editor
held it: the panel landed on 8780 anyway, and the hub's check of 8766 reached
the editor. The ports below are each tool's own, as the hub's
catalog/sources.toml and the tools' sources recorded them on 2026-09-25.
"""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))
import forgepact

OTHER_TOOLS = {
    # hs_item_editor_gui.py binds PORT..PORT + 9 and keeps all ten.
    "Item Editor": set(range(8765, 8775)),
    # HS-AFK-Expedition tools/panel.py, the --port default.
    "AFK FARM panel": {8787},
    # catalog/sources.toml, [tool.launch] ports.
    "HS Offline Launcher": {8861, 8862, 8863, 8961},
    "HSCraftSim": set(range(17870, 17880)),
}


class PanelPortTests(unittest.TestCase):
    def test_the_panel_starts_on_its_first_candidate(self):
        self.assertEqual(forgepact.PORT, forgepact.PORT_CANDIDATES[0])

    def test_the_first_candidate_is_the_port_the_hub_checks(self):
        # The hub's catalog/sources.toml checks http://127.0.0.1:8780/
        # ([tool.launch.health]); move the two together.
        self.assertEqual(forgepact.PORT_CANDIDATES[0], 8780)

    def test_no_candidate_belongs_to_another_toolkit_tool(self):
        for tool, ports in OTHER_TOOLS.items():
            with self.subTest(tool=tool):
                self.assertFalse(set(forgepact.PORT_CANDIDATES) & ports)


if __name__ == "__main__":
    unittest.main()
