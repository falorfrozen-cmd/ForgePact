"""The dungeon chest's unlock, chat and total callbacks, read from the plugin source.

Dungeon chest opens early (issue #31) lets the game's own chest open by
answering one call: the chest's Step asks instance_exists whether any
Enemy_Parent_obj is left (measured in Live procedure 1), and once this room's
tally has latched, ModuleMain's instance_exists detour answers that call
`false`. The decision itself (latched view, chest `self`, enemy argument) is
DungeonChestMod.hpp's AnswerPoll, run by tests/dungeon_chest_harness.cpp; this
file pins how the plugin wires it in, the way test_release_hook_contract.py
pins the other hooks:

- one instance_exists installer in the whole file, reachable from the player
  build, called from DungeonChestInstall, which only a share that would be
  stored reaches (all-off installs nothing). A second HookBuiltin on the same
  routine would patch the first detour instead of the game's code, so the
  research probe's instance_exists counters run inside this detour rather
  than in a detour of their own;
- the detour's first statement tests the room's latched view: instance_exists
  is the hottest builtin measured (28 M calls in Live procedure 1), so every
  call before a latch must cost one flag test before the original runs;
- the research commands (`dungeonprobe creators`, `dungeonprobe total`, the
  `cand`/`cdiff`/`cvar` rows, the births counter) never reach the player
  build, and the births counter rides the create hooks InstallCreateHooks
  already installs rather than a second HookBuiltin on instance_create_*;
- the player build supplies the unlock and chat callbacks (the chat line is
  ChatAddServerMessage by its SDK name, the shape Live procedure 1 proved),
  and has no total source until Join step J7 writes the one Live procedure 1b
  selects.
"""
import re
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_release_hook_contract import function_body, strip_comments, strip_research_blocks

ROOT = Path(__file__).resolve().parents[1]
PLUGIN_SRC = ROOT / "plugin" / "ModuleMain.cpp"


def definition_body(source, signature):
    """The body of the definition `signature {` (not a forward declaration)."""
    match = re.search(re.escape(signature) + r"\s*\{", source)
    if not match:
        raise AssertionError(f"{signature} has no definition")
    depth = 0
    for index in range(match.end() - 1, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[match.end():index]
    raise AssertionError(f"unterminated body for {signature}")


def code_statements(body):
    """The body's code, comments and preprocessor lines removed, one string."""
    lines = [line for line in strip_comments(body).split("\n") if not line.strip().startswith("#")]
    return " ".join(" ".join(lines).split())


class DungeonChestUnlockContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_SRC.read_text(encoding="utf-8", errors="replace")
        cls.player = strip_comments(strip_research_blocks(cls.plugin))

    def test_one_instance_exists_installer_shared_by_probe_and_unlock(self):
        code = strip_comments(self.plugin)
        sites = [m.start() for m in re.finditer(r'HookBuiltin\(\s*"instance_exists"', code)]
        self.assertEqual(len(sites), 1, "instance_exists must be detoured by exactly one installer")
        installer = function_body(code, "static void InstallDungeonChestExistsHook()")
        self.assertRegex(installer, r'HookBuiltin\(\s*"instance_exists"\s*,[^;]*Hook_DcInstanceExists')
        # The installer and its detour are in the player build, not a research block.
        self.assertIn("static void InstallDungeonChestExistsHook()", self.player)
        self.assertIn("static void Hook_DcInstanceExists(", self.player)
        install = function_body(self.player, "static void DungeonChestInstall()")
        self.assertIn("InstallHeadhunterHook();", install)
        self.assertIn("InstallDungeonChestExistsHook();", install)
        # In the player build the installer is reached from DungeonChestInstall only.
        self.assertEqual(len(re.findall(r"\bInstallDungeonChestExistsHook\(\)", self.player)), 2,
                         "definition plus the one call in DungeonChestInstall")
        # The research probe counts through the shared detour, never installs its own.
        install_builtins = function_body(self.plugin, "static void DpInstallBuiltins()")
        self.assertNotIn("Hook_DpBuiltin<1>", strip_comments(install_builtins))
        self.assertIn("DpCountInstanceExists", self.plugin)
        self.assertNotIn("DpCountInstanceExists", self.player)

    def test_the_detour_tests_the_latched_flag_first(self):
        body = function_body(self.plugin, "static void Hook_DcInstanceExists(")
        code = code_statements(body)
        self.assertTrue(code.startswith("if (ForgePact::DungeonChest::state.tally.pollLatched &&"),
                        f"the detour's first statement must test the latched view: {code[:120]}")
        # Every call it does not answer runs the original, through the trampoline.
        self.assertIn("else if (g_DcOrigInstanceExists) g_DcOrigInstanceExists(Result, S, O, argc, Args);", code)
        self.assertIn("ForgePact::DungeonChest::AnswerPoll(", code)
        self.assertIn("Result = RValue(false);", code)
        # The probe's counters are research-only inside it.
        self.assertNotIn("DpCountInstanceExists", strip_research_blocks(body))

    def test_the_detour_installs_only_from_a_non_off_share(self):
        command = function_body(self.player, "static void DungeonChestCommand(")
        self.assertIn("if (pct != 0 && DC::InRange(pct) && totalAvailable) DungeonChestInstall();", command)
        self.assertIn("DC::StoresMode(pct, hook, unlock, totalAvailable)", command)
        # DungeonChestInstall is reached from that one guarded call in the player build.
        self.assertEqual(len(re.findall(r"\bDungeonChestInstall\(\)", self.player)), 2,
                         "definition plus the guarded call in DungeonChestCommand")

    def test_research_commands_and_rows_stay_in_research_blocks(self):
        player = strip_research_blocks(self.plugin)   # comments kept: not even a mention ships
        for text in ("dungeonprobe creators", "dungeonprobe total", "dungeonprobe cand", "dungeonprobe cdiff",
                     "dungeonprobe cvar", 'sub == "creators"', 'sub == "total"', "DpTotalOverrideSource",
                     "DpCreatorBirth(", "DpCreatorCensus(", "chat-sender"):
            with self.subTest(text=text):
                self.assertIn(text, self.plugin, f"the research build has no {text}")
                self.assertNotIn(text, player, f"{text} survives outside a research block")

    def test_births_ride_the_existing_create_hooks(self):
        code = strip_comments(self.plugin)
        sites = [m.start() for m in re.finditer(r'HookBuiltin\(\s*"instance_create', code)]
        self.assertEqual(len(sites), 2, "instance_create_depth and _layer are hooked once each, by InstallCreateHooks")
        # InstallCreateHooks has forward declarations after its definition
        # (function_body takes the last match), so take the body that follows
        # the signature directly.
        create = definition_body(code, "static void InstallCreateHooks()")
        self.assertEqual(len(re.findall(r'HookBuiltin\(\s*"instance_create', create)), 2)
        for signature in ("static void HookICD(", "static void HookICL("):
            with self.subTest(hook=signature):
                hook = function_body(self.plugin, signature)
                self.assertRegex(hook, r"#ifndef FORGEPACT_RELEASE\s*DpCreatorBirth\(callerInfo, argc, Args\);\s*#endif")

    def test_the_player_build_supplies_unlock_and_chat_and_no_total_yet(self):
        wire = function_body(self.player, "static void DungeonChestWire()")
        self.assertIn("ForgePact::DungeonChest::state.unlock = &DungeonChestUnlock;", wire)
        self.assertIn("ForgePact::DungeonChest::state.chat = &DungeonChestChat;", wire)
        self.assertIn("ForgePact::DungeonChest::state.totalSource = g_DcBuildTotalSource;", wire)
        chat = function_body(self.player, "static bool DungeonChestChat(")
        self.assertIn("SdkShortScriptName(HeroSiege::Scripts::gml_Script_ChatAddServerMessage)", chat)
        self.assertIn("ApCallScript(script, self, { RValue(line) }, ret)", chat)
        self.assertIn("HhResolveLocalPlayer(", chat)
        # The unlock answers from the detour's own state and the captured chest.
        unlock = function_body(self.player, "static bool DungeonChestUnlock()")
        self.assertIn("DungeonChestUnlockOk()", unlock)
        self.assertIn("state.tally.chest != nullptr", unlock)
        # Both commands wire the callbacks before reading them.
        self.assertIn("DungeonChestWire();", function_body(self.player, "static void DungeonChestCommand("))

    def test_total_source_reads_the_measured_route(self):
        raise unittest.SkipTest("written in J7")


if __name__ == "__main__":
    unittest.main()
