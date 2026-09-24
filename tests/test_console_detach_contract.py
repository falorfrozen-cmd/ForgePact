"""The player build detaches from YYToolkit's console (#58).

YYToolkit opens its "YYToolkit Log" console inside the game's own process, and
that console becomes the game's standard output, so GameMaker writes every
runtime warning to it synchronously. Underground Garden's zone generation
(entered from Misty Swamp) emits thousands of `tilemap_get()` warnings, and
writing them to the console froze the load for 30-70 seconds. MEASURED
2026-09-23: the game's main thread sat in WriteFile for the whole freeze,
called from the game's own code, and the console buffer held nothing but that
warning. The unmodded game has no console, so there those writes fail at
once. Hiding the console window did not stop the writes; detaching does.
"""
import unittest
from pathlib import Path

from test_release_hook_contract import function_body, strip_comments, strip_research_blocks

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"


class ConsoleDetachContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = PLUGIN.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.player = strip_research_blocks(source)

    def test_the_player_build_detaches_instead_of_hiding(self):
        body = strip_comments(function_body(self.player, "static void DetachConsole()"))
        self.assertIn("FreeConsole()", body)
        self.assertNotIn("ShowWindow", body)

    def test_module_initialize_detaches_in_the_player_build(self):
        init = strip_comments(function_body(self.player, "EXPORTED AurieStatus ModuleInitialize("))
        self.assertIn("DetachConsole();", init)
        self.assertNotIn("KonsoluGizle", self.player)


if __name__ == "__main__":
    unittest.main()
