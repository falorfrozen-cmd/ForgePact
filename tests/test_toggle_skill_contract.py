#!/usr/bin/env python3
"""Contract tests for the toggle-skills work (ForgePact issue #11).

Phase 1 is a research instrument only: `tgprobe`, research build, which hooks
every candidate the static search found in one build so a single live session
can say which routine runs per toggle press, where the on/off state lives, and
what a zone change does to it. See docs/toggle-skills-research.md.

What these tests pin is the part that is easy to get quietly wrong:

- the instrument never reaches the player build, including the three one-line
  entry notes it puts inside hook bodies that do ship;
- every row names an hs-game-sdk constant or enumerator, never a literal;
- a row ForgePact's own installer already holds is attached through the one
  resolver that checks every pointer is game code before handing it to the
  hooking library, and a row it cannot reach says `blocked` / `n/a` rather
  than reporting a 0 that would be the instrument talking;
- the installers the probe attaches around are byte-for-byte unchanged.
"""

import re
import subprocess
import sys
import unittest
from pathlib import Path

TESTS_DIR = Path(__file__).resolve().parent
FORGEPACT_DIR = TESTS_DIR.parent
REPO_ROOT = FORGEPACT_DIR.parent
PLUGIN_SRC = FORGEPACT_DIR / "plugin" / "ModuleMain.cpp"
SDK_INCLUDE = REPO_ROOT / "hs-game-sdk" / "cpp" / "include" / "hs_game_sdk"

if str(TESTS_DIR) not in sys.path:
    sys.path.insert(0, str(TESTS_DIR))

from test_release_hook_contract import function_body, strip_research_blocks  # noqa: E402

BLOCK_START = "// ---- tgprobe: toggle-skill research instrument"
BLOCK_END = "#endif // FORGEPACT_RELEASE (tgprobe)"

EXPECTED_SCRIPTS = {
    "gml_Script_TalentUse", "gml_Script_TalentUseClass", "gml_Script_CheckTalentUse",
    "gml_Script_TalentUseSetSpeed", "gml_Script_NetworkSendClientTalentUse",
    "gml_Script_CA_playerTalentActive", "gml_Script_CA_playerTalentUpdate",
    "gml_Script_TalentsWhiteMage", "gml_Script_TalentsUniversal",
    "gml_Script_GetTalentInfo", "gml_Script_GetTalentId", "gml_Script_ReturnTalentLevel",
    "gml_Script_ReturnTalentValue", "gml_Script_GetTalentCooldown",
    "gml_Script_GetSubTalentInfo", "gml_Script_ReturnSubTalentLevel",
    "gml_Script_LoadSkillTagsString", "gml_Script_ClearPersistSkill",
    "gml_Script_StepAbilityParentDestroyTimer", "gml_Script_PlayerUpdateTimers",
    "gml_Script_BuffAdd", "gml_Script_BuffRemove", "gml_Script_GetBuff",
    "gml_Script_RoomGoto", "gml_Script_NetworkRoomGoto", "gml_Script_NetworkRoomSetupDone",
    "gml_Script_CA_playerRoomSetupDone", "gml_Script_SetupRoomEffects",
    "gml_Script_DrawHudAbilityButtons", "gml_Script_DrawHudBuffs", "gml_Script_DrawHud",
    "gml_Script_GetPlayerTalentHudObj", "gml_Script_UiHudTalentNavigation",
    "gml_Script_DrawKeyBindSprites", "gml_Script_CheckPlayerInteraction",
    "gml_Script_InputPressed", "gml_Script_LoadAura", "gml_Script_skillsAura",
} | {
    f"gml_Script_anon_{n}_gml_Object_UI_Hud_Talent_obj_Create_0"
    for n in (1183, 2413, 10430, 11283, 11677, 12084, 12530, 13033)
}

EXPECTED_EVENTS = {
    ("White_Mage_Soul_Spurn_obj", ev)
    for ev in ("Create_0", "Step_0", "Destroy_0", "CleanUp_0", "Alarm_0")
} | {
    ("Player_Ability_Parent_obj", ev)
    for ev in ("Create_0", "Step_0", "Destroy_0", "CleanUp_0")
} | {
    ("Draw_Player_Buff_obj", ev) for ev in ("Create_0", "Destroy_0")
} | {
    ("UI_Hud_Talent_obj", ev) for ev in ("Create_0", "Step_0", "Draw_0", "Draw_64")
} | {
    ("Skill_Controller_obj", ev) for ev in ("Create_0", "Step_0")
}

SCRIPT_ROW = re.compile(
    r'X\((?P<safe>\w+),\s*HeroSiege::Scripts::(?P<const>gml_Script_\w+),\s*"(?P<label>[^"]*)",'
    r'\s*(?P<flags>[^,]+),\s*(?P<orig>[^,]+),\s*(?P<via>[^)]+)\)'
)
EVENT_ROW = re.compile(r"X\((?P<obj>\w+),\s*(?P<ev>\w+),\s*(?P<flags>[^)]+)\)")


def macro_body(source: str, header: str) -> str:
    start = source.index(header)
    lines = []
    for line in source[start:].splitlines():
        lines.append(line)
        if not line.rstrip().endswith("\\"):
            break
    return "\n".join(lines[1:])


class ToggleProbeContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_SRC.read_text(encoding="utf-8")
        start = cls.plugin.index(BLOCK_START)
        end = cls.plugin.index(BLOCK_END, start)
        cls.block = cls.plugin[start:end]
        cls.block_start = start
        cls.scripts = macro_body(cls.plugin, "#define TGPROBE_SCRIPTS(X)")
        cls.events = macro_body(cls.plugin, "#define TGPROBE_EVENTS(X)")
        cls.script_rows = [m.groupdict() for m in SCRIPT_ROW.finditer(cls.scripts)]
        cls.event_rows = [(m["obj"], m["ev"]) for m in EVENT_ROW.finditer(cls.events)]

    # ---- research build only ------------------------------------------------

    def test_block_is_inside_a_research_guard(self):
        guard = self.plugin.rfind("#ifndef FORGEPACT_RELEASE", 0, self.block_start)
        self.assertGreaterEqual(guard, 0)
        self.assertLess(self.block_start - guard, 40)

    def test_command_branch_exists_once_and_only_in_the_research_build(self):
        self.assertEqual(self.plugin.count('lc == "tgprobe"'), 1)
        self.assertIn('lc == "tgprobe"', function_body(self.plugin, "static void RunCommand("))
        self.assertNotIn('lc == "tgprobe"', strip_research_blocks(self.plugin))

    def test_probe_is_not_a_player_command(self):
        allowlist = re.search(r"kPlayerCommands\s*=\s*\{(?P<body>.*?)\};", self.plugin, re.DOTALL)
        self.assertIsNotNone(allowlist)
        self.assertNotIn("tgprobe", allowlist.group("body"))

    def test_every_mention_is_stripped_from_the_player_build(self):
        # The block, the forward declarations, the three entry notes and the
        # RunCommand branch all sit inside #ifndef FORGEPACT_RELEASE pairs.
        self.assertGreaterEqual(len(re.findall("tgprobe", self.plugin, re.IGNORECASE)), 20)
        self.assertEqual(len(re.findall("tgprobe", strip_research_blocks(self.plugin), re.IGNORECASE)), 0)

    # ---- the candidate set ----------------------------------------------------

    def test_script_rows_are_exactly_the_static_search_set(self):
        named = set(re.findall(r"\bgml_Script_\w+", self.scripts))
        self.assertEqual(named, EXPECTED_SCRIPTS)
        self.assertEqual({r["const"] for r in self.script_rows}, EXPECTED_SCRIPTS)
        self.assertEqual(len(self.script_rows), len(EXPECTED_SCRIPTS))

    def test_every_script_constant_resolves_in_the_sdk(self):
        header = (SDK_INCLUDE / "scripts.hpp").read_text(encoding="utf-8")
        for name in sorted(EXPECTED_SCRIPTS):
            self.assertRegex(header, r"std::string_view " + re.escape(name) + r" =", name)

    def test_event_rows_are_exactly_the_planned_set(self):
        self.assertEqual(set(self.event_rows), EXPECTED_EVENTS)
        self.assertEqual(len(self.event_rows), len(EXPECTED_EVENTS))

    def test_no_room_start_or_room_end_event_is_hooked(self):
        # In GameMaker Other_4 is Room Start and Other_5 is Room End. Hooking
        # the room start event has crashed the game before
        # (test_est_force_behavior.test_room_start_is_not_hooked), so the probe
        # carries neither on any object; zone change is read from the room key
        # and the Soul Spurn object's Destroy_0 / CleanUp_0 rows instead.
        for forbidden in ("Other_4", "Other_5", "RoomStart", "Room_Start"):
            self.assertNotIn(forbidden, self.block)
        self.assertFalse({ev for _, ev in self.event_rows} & {"Other_4", "Other_5"})

    def test_every_event_object_is_an_sdk_enumerator(self):
        header = (SDK_INCLUDE / "objects.hpp").read_text(encoding="utf-8")
        for obj in sorted({o for o, _ in self.event_rows}):
            self.assertRegex(header, r"\n\s+" + re.escape(obj) + r" = \d+,", obj)
        self.assertIn("HeroSiege::Objects::GameObject::OBJ", self.block)

    def test_aura_rows_are_negative_controls_only(self):
        for name in ("LoadAura", "skillsAura"):
            lines = [line for line in self.block.splitlines() if name in line]
            self.assertEqual(len(lines), 1, f"{name} appears outside its row: {lines}")
            self.assertIn("negctl", lines[0])

    def test_no_runtime_name_is_a_literal(self):
        self.assertNotRegex(self.block, r'"gml_Script_')
        # Event names are built from the SDK's object name plus the suffix.
        self.assertNotRegex(self.block, r'"gml_Object_\w')
        attach = function_body(self.plugin, "static void TgProbeAttach(")
        self.assertIn('"gml_Object_"', attach)
        self.assertIn("HeroSiege::Objects::GetObjectName(t.object)", attach)

    # ---- how rows attach ------------------------------------------------------

    def test_probe_installs_no_table_hooks(self):
        code = self.block
        for call in ("HookOneScript(", "HookOneScriptTable(", "HookRawNamedRoutine("):
            self.assertNotIn(call, code)

    def test_one_resolver_decides_every_detour(self):
        self.assertEqual(self.block.count("MmCreateHook("), 1)
        attach = function_body(self.plugin, "static void TgProbeAttach(")
        self.assertIn("MmCreateHook(", attach)
        lookup = attach.index("GetNamedRoutinePointer")
        check = attach.index("AddrIsExecutableInModule(GetModuleHandleA(nullptr)")
        hook = attach.index("MmCreateHook(")
        self.assertLess(lookup, check)
        self.assertLess(check, hook)
        for literal in ('"native"', '"via "', '"blocked', '"not found'):
            self.assertIn(literal, attach)

    def test_existing_hook_bindings(self):
        origs = set(re.findall(r"&g_Orig\w+", self.block))
        self.assertEqual(origs, {"&g_Orig_DrawHudBuffs", "&g_OrigTalentUse", "&g_OrigBuffAdd",
                                 "&g_OrigCi_CheckPlayerInteraction"})
        by_orig = {r["orig"].strip(): r["via"].strip() for r in self.script_rows if r["orig"].strip() != "nullptr"}
        self.assertEqual(by_orig, {
            "&g_Orig_DrawHudBuffs": '"Hook_DrawHudBuffs"',
            "&g_OrigTalentUse": '"HookTalentUse"',
            "&g_OrigBuffAdd": '"HookBuffAdd"',
            "&g_OrigCi_CheckPlayerInteraction": "nullptr",
        })
        vias = {r["via"].strip() for r in self.script_rows} - {"nullptr"}
        self.assertEqual(vias, {'"Hook_DrawHudBuffs"', '"HookTalentUse"', '"HookBuffAdd"'})

    def test_entry_notes_precede_the_hook_bodies_and_never_ship(self):
        shipped = strip_research_blocks(self.plugin)
        cases = (
            ("static RValue& Hook_DrawHudBuffs(", "TgProbeNoteDrawHudBuffs(S, O, argc, A);",
             "g_Orig_DrawHudBuffs(S, O, R, argc, A)"),
            ("static RValue& HookTalentUse(", "TgProbeNoteTalentUse(S, O, argc, A);", "g_BlockPuppetSkills"),
            ("static RValue& HookBuffAdd(", "TgProbeNoteBuffAdd(S, O, argc, A);", "LogBuffCall("),
        )
        for signature, note, anchor in cases:
            body = function_body(self.plugin, signature)
            self.assertIn(note, body, signature)
            self.assertLess(body.index(note), body.index(anchor), signature)
            self.assertNotIn("TgProbe", function_body(shipped, signature), signature)

    def test_room_key_and_blindness_reporting(self):
        note = function_body(self.plugin, "static void TgProbeNoteDrawHudBuffs(")
        self.assertIn("CurrentRoomKey()", note)
        show = function_body(self.plugin, "static void TgProbeShow(")
        for literal in ("n/a", "TABLE-ONLY", "CurrentRoomKey()", "hudSinceRoomChange="):
            self.assertIn(literal, show)
        hook = function_body(self.plugin, "static void TgProbeHook(")
        self.assertIn('" blocked, "', hook)

    def test_unreadable_room_key_is_never_stored(self):
        tick = function_body(self.plugin, "static void TgProbeHudRoomTick(")
        guard = tick.index("key == INT64_MIN")
        store = tick.index("g_TgHudRoomKey = key")
        self.assertLess(guard, store)

    def test_subcommands(self):
        command = function_body(self.plugin, "static void TgProbeCommand(")
        for sub in ("hook", "show", "reset", "verbose", "slots", "buffs", "abilities",
                    "vars", "snap", "diff", "room"):
            self.assertIn(f'"{sub}"', command)

    def test_installers_the_probe_attaches_around_are_unchanged(self):
        try:
            origin = subprocess.run(
                ["git", "-C", str(FORGEPACT_DIR), "show", "origin/main:plugin/ModuleMain.cpp"],
                capture_output=True, check=True,
            ).stdout.decode("utf-8").replace("\r\n", "\n")
        except (OSError, subprocess.CalledProcessError) as exc:
            self.skipTest(f"origin/main is not readable here: {exc}")
        working = self.plugin.replace("\r\n", "\n")
        for signature in ("static void InstallHeadLabelHook()", "static void InstallBuffHooks()",
                          "static void CoopRenderTick()"):
            self.assertEqual(function_body(origin, signature), function_body(working, signature), signature)


if __name__ == "__main__":
    unittest.main()
