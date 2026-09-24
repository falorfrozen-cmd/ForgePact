"""The Angelic gate finder after DropItem is hooked (#69).

`raredrop angelic` opens the game's own Angelic roll by finding one branch in
DropItem's compiled code. The finder used to read DropItem through its
script-table entry, which HookOneScript swaps for ForgePact's own
Hook_DropItem as soon as `dropmult` installs DropManager. After any drop
multiplier above x1 it therefore scanned ForgePact's code and answered "call
site not found". The harness runs the production finder against a modelled
game image and script table; the contract tests pin where the startup record
is taken and that the finder scans only that record.
"""
import unittest
from pathlib import Path

import test_adaptive_population as compiler
from test_map_reveal_behavior import implementation
from test_release_hook_contract import function_body, strip_comments, strip_research_blocks

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"
HARNESS = ROOT / "tests" / "angelic_gate_harness.cpp"

PRODUCTION = (
    "static unsigned char* ScriptCode(const char* fullName)",
    "static unsigned char* GameScriptCode(const char* fullName, unsigned char*& recorded)",
    "static bool AngelicScriptCodeReady()",
    "static void CaptureAngelicScriptCode()",
    "static unsigned char* FindAngelicGate()",
)


class AngelicGateBehaviorTests(unittest.TestCase):
    compile_and_run = compiler.AdaptivePopulationTests.compile_and_run

    @classmethod
    def setUpClass(cls):
        cls.source = PLUGIN.read_text(encoding="utf-8").replace("\r\n", "\n")

    def test_the_finder_survives_the_hooks_that_swap_its_table_entries(self):
        code = "\n\n".join(implementation(self.source, signature) for signature in PRODUCTION)
        harness = HARNESS.read_text(encoding="utf-8").replace("// PRODUCTION_GATE", code)
        self.compile_and_run("angelic-gate", harness, "RESULT OK")

    def test_both_builds_record_the_code_before_any_hook(self):
        for label, text in (("research", self.source), ("player", strip_research_blocks(self.source))):
            with self.subTest(build=label):
                body = strip_comments(function_body(text, "static void InstallHook()"))
                self.assertIn("CaptureAngelicScriptCode();", body)
                capture = body.index("CaptureAngelicScriptCode();")
                for later in ("LoadCustomForgeEntries();", "InstallCustomForgeItemHooks();",
                              "InstallDropMultHooks();"):
                    if later in body:
                        self.assertLess(capture, body.index(later), later)

    def test_the_finder_scans_only_the_recorded_game_code(self):
        body = " ".join(strip_comments(function_body(self.source, "static unsigned char* FindAngelicGate()")).split())
        self.assertNotIn("ScriptCode(", body.replace("GameScriptCode(", ""))
        self.assertIn("AngelicScriptCodeReady()", body)
        self.assertIn("unsigned char* drop = g_DropItemCode;", body)
        self.assertIn("unsigned char* chance = g_AngelicChanceCode;", body)

    def test_only_game_code_is_recorded(self):
        body = strip_comments(function_body(
            self.source, "static unsigned char* GameScriptCode(const char* fullName, unsigned char*& recorded)"))
        self.assertIn("AddrIsExecutableInModule(GetModuleHandleA(nullptr)", body)


if __name__ == "__main__":
    unittest.main()
