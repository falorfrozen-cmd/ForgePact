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
SRC_DIR = FORGEPACT_DIR / "src"
SDK_PY_PATH = REPO_ROOT / "hs-game-sdk" / "python"

if str(TESTS_DIR) not in sys.path:
    sys.path.insert(0, str(TESTS_DIR))
if str(SRC_DIR) not in sys.path:
    sys.path.insert(0, str(SRC_DIR))
if str(SDK_PY_PATH) not in sys.path:
    sys.path.insert(0, str(SDK_PY_PATH))

from test_release_hook_contract import function_body, strip_research_blocks  # noqa: E402

import forgepact  # noqa: E402

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
    # Offsets of the SDK regenerated at hub 4539e68; a closure's name carries its
    # offset in the Create event, so these move whenever that event changes.
    for n in (1233, 2503, 10745, 11619, 12025, 12449, 12916, 13435)
}

EXPECTED_EVENTS = {
    # The event rows' positive control: the player steps every frame.
    ("Player_obj", "Step_0"),
} | {
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
        # The block, the forward declarations, the four entry notes and the
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
        # T1 (issue #11, Track A) added the re-cast guard's HookTalentUseClass:
        # once it holds TalentUseClass, the probe row counts from its entry
        # note instead of putting a second detour on the same bytes.
        origs = set(re.findall(r"&g_Orig\w+", self.block))
        self.assertEqual(origs, {"&g_Orig_DrawHudBuffs", "&g_OrigTalentUse", "&g_OrigBuffAdd",
                                 "&g_OrigCi_CheckPlayerInteraction", "&g_OrigTalentUseClass"})
        by_orig = {r["orig"].strip(): r["via"].strip() for r in self.script_rows if r["orig"].strip() != "nullptr"}
        self.assertEqual(by_orig, {
            "&g_Orig_DrawHudBuffs": '"Hook_DrawHudBuffs"',
            "&g_OrigTalentUse": '"HookTalentUse"',
            "&g_OrigBuffAdd": '"HookBuffAdd"',
            "&g_OrigCi_CheckPlayerInteraction": "nullptr",
            "&g_OrigTalentUseClass": '"HookTalentUseClass"',
        })
        vias = {r["via"].strip() for r in self.script_rows} - {"nullptr"}
        self.assertEqual(vias, {'"Hook_DrawHudBuffs"', '"HookTalentUse"', '"HookBuffAdd"',
                                '"HookTalentUseClass"'})

    def test_entry_notes_precede_the_hook_bodies_and_never_ship(self):
        shipped = strip_research_blocks(self.plugin)
        cases = (
            ("static RValue& Hook_DrawHudBuffs(", "TgProbeNoteDrawHudBuffs(S, O, argc, A);",
             "g_Orig_DrawHudBuffs(S, O, R, argc, A)"),
            ("static RValue& HookTalentUse(", "TgProbeNoteTalentUse(S, O, argc, A);", "g_BlockPuppetSkills"),
            ("static RValue& HookBuffAdd(", "TgProbeNoteBuffAdd(S, O, argc, A);", "LogBuffCall("),
            ("static RValue& HookTalentUseClass(", "TgProbeNoteTalentUseClass(S, O, argc, A);",
             "ToggleGuardMod::Instance().IsEnabled()"),
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

    def test_event_rows_have_a_positive_control(self):
        # `not found` on an event row describes the name lookup, not the
        # object; without a row that must fire, an all-`not found` event set
        # would read as a result about the game.
        self.assertIn(("Player_obj", "Step_0"), self.event_rows)
        self.assertIn("Player_obj.Step_0", function_body(self.plugin, "static void TgProbeHook("))
        self.assertNotIn("means the object has", self.block)

    def test_first_draw_after_a_zone_change_reads_the_state(self):
        # A `tgprobe show` typed after a zone change lands tens of frames late,
        # so Q6's "OFF on the first draw" read is taken inside the draw hook on
        # the draw where the room key changes, by name, and never as a count
        # when the read failed.
        tick = function_body(self.plugin, "static void TgProbeHudRoomTick(")
        change = tick.index("key != g_TgHudRoomKey")
        snap = tick.index("GameObject::White_Mage_Soul_Spurn_obj")
        self.assertLess(change, snap)
        self.assertIn("GameObject::Player_Ability_Parent_obj", tick)
        self.assertIn("zoneChange", tick)
        count = function_body(self.plugin, "static bool TgProbeCountByName(")
        self.assertIn('"asset_get_index"', count)
        self.assertIn('"instance_number"', count)
        self.assertIn("GetObjectName(obj)", count)
        show = function_body(self.plugin, "static void TgProbeShow(")
        for literal in ("firstHudSpurnInstances=", "firstHudAbilityInstances=", "zone-change",
                        "attach (not a zone change)", '"unreadable"'):
            self.assertIn(literal, show)

    def test_unreadable_room_key_is_never_stored(self):
        tick = function_body(self.plugin, "static void TgProbeHudRoomTick(")
        guard = tick.index("key == INT64_MIN")
        store = tick.index("g_TgHudRoomKey = key")
        self.assertLess(guard, store)

    def test_subcommands(self):
        command = function_body(self.plugin, "static void TgProbeCommand(")
        for sub in ("hook", "show", "reset", "verbose", "slots", "buffs", "abilities",
                    "vars", "snap", "diff", "room", "deep"):
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


class ToggleDeepReadContractTests(unittest.TestCase):
    """`tgprobe deep`: session 2's non-scalar read for Q3.

    Session 1's scalar snapshot could not see inside a container, and wrapped
    its whole enumeration loop in one error handler, so a throw on one member
    silently dropped every member after it. These tests pin the parts of the
    replacement that make its negatives worth something: every member read
    individually guarded and counted, containers of every kind walked, the
    coverage printed, a selftest fixture as the mechanics control, objects
    named through the SDK, and no hook of any kind.
    """

    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_SRC.read_text(encoding="utf-8")
        start = cls.plugin.index(BLOCK_START)
        end = cls.plugin.index(BLOCK_END, start)
        cls.block = cls.plugin[start:end]

    def test_deep_is_dispatched_and_has_every_subcommand(self):
        self.assertIn('"deep"', function_body(self.plugin, "static void TgProbeCommand("))
        command = function_body(self.plugin, "static void TgProbeDeepCommand(")
        for sub in ("snap", "diff", "flip", "find", "get", "census", "selftest"):
            self.assertIn(f'"{sub}"', command)

    def test_research_build_only(self):
        self.assertGreater(self.block.count("TgProbeDeep"), 0)
        self.assertEqual(self.plugin.count("TgProbeDeep"), self.block.count("TgProbeDeep"))
        self.assertNotIn("TgProbeDeep", strip_research_blocks(self.plugin))

    def test_walker_expands_every_container_kind_within_caps(self):
        walk = function_body(self.plugin, "static void TgProbeDeepWalk(")
        for needle in ("variable_struct_get_names", "is_struct", "array_length", "ds_map_find_first",
                       "ds_list_size", "kTgDeepMaxDepth", "kTgDeepMaxElems"):
            self.assertIn(needle, walk)
        self.assertRegex(self.block, r"kTgDeepMaxDepth\s*=\s*3")
        self.assertRegex(self.block, r"kTgDeepMaxElems\s*=\s*200")

    def test_every_member_read_is_guarded_on_its_own(self):
        # The session-1 defect: one try around the whole loop. Here the loop
        # comes first and each member gets its own handler.
        members = function_body(self.plugin, "static void TgProbeDeepReadMembers(")
        self.assertLess(members.index("for ("), members.index("try {"))
        self.assertIn("unreadable", members)

    def test_snapshot_prints_its_coverage_and_enumerates_globals_both_ways(self):
        snap = function_body(self.plugin, "static void TgProbeDeepSnap(")
        for literal in ("names=", "read=", "unreadable=", "leaves=", "truncated=", "ms=", "globalNames="):
            self.assertIn(literal, snap)
        self.assertIn("EnumInstanceMembers", snap)
        self.assertIn("variable_instance_get_names", snap)

    def test_objects_are_named_through_the_sdk(self):
        for obj in ("Player_obj", "Controller_obj", "UI_Hud_Talent_obj", "Skill_Controller_obj"):
            self.assertIn(f"GameObject::{obj}", self.block)
            self.assertNotIn(f'"{obj}"', self.block)

    def test_talent_scope_uses_the_validated_readers(self):
        self.assertIn("N1GetTalentMap(", self.block)
        self.assertIn("N1GetTalentStruct(", self.block)
        # Session-1 measurements (Soul Spurn, the chained crow talent, Healing
        # Zone as the non-toggle control), not SDK constants.
        self.assertIn("{ 240, 243, 252 }", self.block)

    def test_selftest_is_a_builtin_only_fixture_with_a_verdict(self):
        selftest = function_body(self.plugin, "static void TgProbeDeepSelfTest(")
        for needle in ("json_parse", "ds_map_create", "variable_struct_set", "ds_map_replace",
                       "ds_map_destroy", "OK leaves=", "FAIL"):
            self.assertIn(needle, selftest)

    def test_get_resolves_every_root_by_name(self):
        get = function_body(self.plugin, "static bool TgProbeDeepGet(")
        for needle in ("talent:", "'#'", "variable_global_get"):
            self.assertIn(needle, get)

    def test_census_counts_every_existing_object(self):
        census = function_body(self.plugin, "static void TgProbeDeepCensus(")
        for needle in ("object_exists", "instance_number", "object_get_name"):
            self.assertIn(needle, census)

    def test_no_float_format_and_no_new_hook(self):
        self.assertNotIn('"%f"', self.block)
        self.assertNotRegex(self.block, r"%\.\d+f")
        self.assertEqual(self.block.count("MmCreateHook("), 1)
        for call in ("HookOneScript(", "HookOneScriptTable(", "HookRawNamedRoutine(", "HookBuiltin("):
            self.assertNotIn(call, self.block)

    # ---- round 2: four gaps that could each fake a Q3 "not observed" ----

    def test_instance_handles_are_followed_one_level_and_counted(self):
        # I1: an instance handle was an opaque leaf, so a toggle living as a
        # variable on an instance that exists both ON and OFF was invisible.
        walk = function_body(self.plugin, "static void TgProbeDeepWalk(")
        self.assertIn("TgProbeDeepFollowInstance(", walk)
        # Asked only after every container kind has had its chance.
        self.assertLess(walk.index("ref ds_list "), walk.index("TgProbeDeepFollowInstance("))
        follow = function_body(self.plugin, "static bool TgProbeDeepFollowInstance(")
        # Identified by what it is (the runtime's own description, then a live
        # check), never by a kind comparison deciding whether it is read.
        for needle in ("ref instance ", "instance_exists", "variable_instance_get",
                       "walkedInstances", "followDepth", "depth + 1", "instFollowed", "instUnfollowed"):
            self.assertIn(needle, follow)
        self.assertNotIn("m_Kind", follow)
        self.assertIn("TgProbeDeepInstanceNames(", follow)
        # Each member of the followed instance is read in its own handler.
        loop = follow.index("for (")
        self.assertLess(loop, follow.index("try {", loop))
        self.assertNotIn("try {", follow[:loop].split("TgProbeDeepInstanceNames(")[-1])
        # A scope root is marked walked, so a reference back to it is not walked twice.
        self.assertIn("walkedInstances", function_body(self.plugin, "static void TgProbeDeepScopeObject("))
        self.assertIn("instRefs=", function_body(self.plugin, "static void TgProbeDeepSnap("))

    def test_leaf_budget_is_per_scope_and_truncation_is_reported_per_scope(self):
        # I2: one snapshot-wide budget let `global` (walked last) starve, and
        # its cut-off move between snapshots with one flag to show for it.
        self.assertRegex(self.block, r"kTgDeepMaxLeavesPerScope\s*=\s*250000")
        self.assertNotRegex(self.block, r"kTgDeepMaxLeaves\b")
        leaf = function_body(self.plugin, "static void TgProbeDeepLeaf(")
        self.assertIn("st.leaves >= kTgDeepMaxLeavesPerScope", leaf)
        self.assertIn("st.truncated = true", leaf)
        self.assertNotIn("out.leaves.size()", leaf)
        self.assertNotIn("out.truncated", function_body(self.plugin, "static void TgProbeDeepWalk("))
        snap = function_body(self.plugin, "static void TgProbeDeepSnap(")
        self.assertGreaterEqual(snap.count("truncated="), 2)   # every scope line, and the summary
        self.assertIn("truncatedScopes=", snap)
        self.assertIn("truncated=", function_body(self.plugin, "static void TgProbeDeepFlip("))

    def test_diff_takes_a_path_filter(self):
        # I3: 300 lines in path order put `global.` last, so the C2 line could
        # fall past the cap under HP-drain churn and read as a blind walker.
        self.assertRegex(self.plugin, r"static void TgProbeDeepDiff\(const std::string& a, const std::string& b, "
                                      r"const std::string& filter\)")
        diff = function_body(self.plugin, "static void TgProbeDeepDiff(")
        for needle in ("Lower(", "matching=", "filter="):
            self.assertIn(needle, diff)
        command = function_body(self.plugin, "static void TgProbeDeepCommand(")
        self.assertIn("TgProbeDeepDiff(a, b, filter)", command)
        self.assertIn("diff <a> <b> [substr]", command)

    def test_selftest_covers_ds_lists_and_instance_handles(self):
        # I4: no positive control reached a ds_list or a followed instance.
        selftest = function_body(self.plugin, "static void TgProbeDeepSelfTest(")
        for needle in ("ds_list_create", "ds_list_add", "ds_list_replace", "ds_list_destroy",
                       '"selftest.l[0]"', "TgProbeDeepSelfTestInstance("):
            self.assertIn(needle, selftest)
        instance = function_body(self.plugin, "static void TgProbeDeepSelfTestInstance(")
        for needle in ("GameObject::Controller_obj", "instance_find", "instFollowed", "instUnfollowed",
                       "OK followed=", "FAIL", "SKIP"):
            self.assertIn(needle, instance)

    def test_get_names_a_bad_index_instead_of_a_throw(self):
        # I6: std::stoi on a bad index surfaced as "a builtin threw", and a
        # global the snapshot enumerated could be refused by an exists gate.
        get = function_body(self.plugin, "static bool TgProbeDeepGet(")
        self.assertIn("bad index", get)
        self.assertIn("variable_global_exists=false", get)

    # ---- round 3: attribution, strings, non-struct objects ----

    def test_a_string_is_never_taken_for_a_handle(self):
        # R3: the handle checks matched "ref instance " / "ref ds_map " anywhere
        # in Describe(), and a string's description quotes its text - so a
        # game-built string(id) went to instance_exists or ds_exists.
        describes = function_body(self.plugin, "static bool TgProbeDeepDescribesRef(")
        self.assertIn('"string:"', describes)
        # Anchored: the ref text starts the description, or starts what a
        # `kind=N str=` description reports.
        for needle in ('rfind("kind=", 0)', '" str="'):
            self.assertIn(needle, describes)
        self.assertLess(describes.index('"string:"'), describes.index('rfind("kind=", 0)'))
        is_ds = function_body(self.plugin, "static bool TgProbeDeepIsDs(")
        self.assertIn("TgProbeDeepDescribesRef(", is_ds)
        self.assertNotIn(".find(describedAs)", is_ds)
        self.assertLess(is_ds.index("TgProbeDeepDescribesRef("), is_ds.index("ds_exists"))
        follow = function_body(self.plugin, "static bool TgProbeDeepFollowInstance(")
        self.assertIn('TgProbeDeepDescribesRef(handle, "ref instance ")', follow)
        self.assertNotIn('handle.find("ref instance ")', follow)
        self.assertLess(follow.index("TgProbeDeepDescribesRef("), follow.index("instance_exists"))
        self.assertNotIn("m_Kind", describes)
        self.assertNotIn("m_Kind", follow)

    def test_followed_members_have_their_own_budget_and_count(self):
        # R1: members read through a followed handle were charged to the scope
        # the handle sat in, so a scope could truncate with no attribution.
        # B1: the follow budget matches the scope budget. At 50,000 one followed
        # handle (depth 1, ~200x200 leaves) could spend it, and because it is per
        # scope a scoped retake could not recover - followTruncated=1 on player
        # or global would make a Q3 negative unobtainable for the whole session.
        self.assertRegex(self.block, r"kTgDeepMaxFollowLeavesPerScope\s*=\s*250000")
        leaf = function_body(self.plugin, "static void TgProbeDeepLeaf(")
        for needle in ("out.followDepth > 0", "st.followLeaves >= kTgDeepMaxFollowLeavesPerScope",
                       "st.followTruncated = true", "++st.followLeaves"):
            self.assertIn(needle, leaf)
        walk = function_body(self.plugin, "static void TgProbeDeepWalk(")
        self.assertIn("TgProbeDeepBudgetSpent(out, st)", walk)
        self.assertNotIn("st.truncated", walk)
        self.assertIn("TgProbeDeepBudgetSpent(out, st)", function_body(self.plugin, "static bool TgProbeDeepFollowInstance("))
        snap = function_body(self.plugin, "static void TgProbeDeepSnap(")
        for needle in ("followLeaves=", "followTruncated="):
            self.assertIn(needle, snap)
        self.assertIn("followTruncated", function_body(self.plugin, "static bool TgProbeDeepScopeTruncated("))
        # Six retained full snapshots had no way to be freed.
        command = function_body(self.plugin, "static void TgProbeDeepCommand(")
        self.assertIn('"drop"', command)
        self.assertIn("g_TgDeepSnaps.erase(", command)
        self.assertIn("drop <name>", command)

    def test_non_struct_objects_are_counted_and_never_asked_about_instances(self):
        # R2: a CInstance held as a non-struct object became an object/method
        # leaf, neither followed nor counted.
        walk = function_body(self.plugin, "static void TgProbeDeepWalk(")
        branch = walk[walk.index('"is_struct"'):walk.index("variable_struct_get_names")]
        self.assertIn("CiTryResolveMethod(v)", branch)
        self.assertIn("++st.objNonStruct", branch)
        for call in ("instance_exists", "variable_instance", "HhResolveInstance"):
            self.assertNotIn(call, branch)
        self.assertIn("objNonStruct=", function_body(self.plugin, "static void TgProbeDeepSnap("))


class ToggleIndicatorReadContractTests(unittest.TestCase):
    """The toggle-skill active indicator's P1 research control (Track B).

    Companion to test_toggle_skill_behavior.py, which runs the production
    read end to end; this class asserts on source text and placement - which
    function sits outside every research block, which call is research-only,
    and that the player command table did not move.
    """

    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_SRC.read_text(encoding="utf-8")
        cls.stripped = strip_research_blocks(cls.plugin)

    def test_production_read_sits_outside_every_research_block(self):
        # If either function were inside a research-only block, it would not
        # survive stripping to what a player build actually compiles.
        self.assertIn("static bool ToggleIndicatorResolveAoeObject(", self.stripped)
        self.assertIn("static ForgePact::ToggleIndicatorState ToggleIndicatorRead(", self.stripped)

    def test_production_read_uses_the_documented_shape(self):
        read_body = function_body(self.plugin, "static ForgePact::ToggleIndicatorState ToggleIndicatorRead(")
        resolve_body = function_body(self.plugin, "static bool ToggleIndicatorResolveAoeObject(")
        combined = resolve_body + read_body
        self.assertIn("GameObject::White_Mage_Soul_Spurn_AOE_obj", combined)
        self.assertIn('"instance_number"', combined)
        self.assertIn('"instance_find"', combined)
        self.assertIn('"isMyClient"', combined)
        self.assertIn('"purgatory"', combined)
        # P1b (replan 1): session 3 measured that Player_obj has no
        # playerNumber at all, so ownership is decided from each scanned
        # instance's own isMyClient - no local-player read, no fallback key
        # (docs/toggle-skills-research.md, "Co-op / ownership after session
        # 3: isMyClient").
        self.assertNotIn("HhResolveLocalPlayer", combined)
        self.assertNotIn('"playerNumber"', combined)
        self.assertNotIn("myHealthBar", combined)
        self.assertNotIn("CallGameScript", combined)
        # The read must stay the exact shape the research doc's ON=1 control
        # proves: two-argument CallBuiltin, global context, never the
        # self-taking CallBuiltinEx; and never a hand-resolved SDK index.
        self.assertNotIn("CallBuiltinEx", combined)
        self.assertNotIn("%f", combined)
        self.assertNotIn("5759", combined)

    def test_hook_draw_hud_buffs_calls_the_research_sampler_after_head_labels(self):
        body = function_body(self.plugin, "static RValue& Hook_DrawHudBuffs(")
        self.assertIn("HhDrawHeadLabels();", body)
        self.assertIn("TgProbeSpurnAfterDraw();", body)
        self.assertLess(body.index("HhDrawHeadLabels();"), body.index("TgProbeSpurnAfterDraw();"))
        # The research call sits in its own #ifndef FORGEPACT_RELEASE pair -
        # not merely somewhere inside a wider one, so a player build's
        # Hook_DrawHudBuffs body is unchanged apart from removing this pair.
        self.assertRegex(
            body,
            r"#ifndef FORGEPACT_RELEASE\s*\n\s*TgProbeSpurnAfterDraw\(\);\s*\n\s*#endif",
        )

    def test_research_sampler_calls_the_production_read_by_name(self):
        sampler_body = function_body(self.plugin, "static void TgProbeSpurnAfterDraw()")
        self.assertIn("ToggleIndicatorRead(", sampler_body)

    def test_tgprobe_dispatches_spurn_and_mark(self):
        dispatch_body = function_body(self.plugin, 'static void TgProbeCommand(const std::string& rest)')
        self.assertIn('sub == "spurn"', dispatch_body)
        self.assertIn('sub == "mark"', dispatch_body)

    def test_spurn_command_has_the_session_4_additions(self):
        # `spurn fields` prints the latched per-appearance snapshot, `spurn
        # as foreign` replaces `spurn as <n>` (P1b: there is no player number
        # left to override), and the bare `spurn` print carries the
        # marker-required counters and the last ownership-only transition
        # frame - session 4's S2/S3/S4/S5 controls
        # (docs/toggle-skills-research.md, "Session 4").
        spurn_cmd = function_body(self.plugin, "static void TgProbeSpurnCommand(const std::string& rest)")
        self.assertIn('"fields"', spurn_cmd)
        self.assertIn('"foreign"', spurn_cmd)
        self.assertIn("markedOn=", spurn_cmd)
        self.assertIn("lastTransitionFrame=", spurn_cmd)

    def test_latched_snapshot_fields_are_research_only(self):
        for needle in ("targetNumber", "purgatoryTimer", "destroyTimer"):
            self.assertIn(needle, self.plugin)
            self.assertNotIn(needle, self.stripped)

    def test_mark_saves_and_restores_colour_and_alpha(self):
        body = function_body(self.plugin, "static void TgProbeDrawMark()")
        get_colour = body.index("draw_get_colour")
        get_alpha = body.index("draw_get_alpha")
        first_set = body.index("draw_set_")
        self.assertLess(get_colour, first_set)
        self.assertLess(get_alpha, first_set)
        last_draw = body.rindex("draw_rectangle")
        restore_alpha = body.rindex("draw_set_alpha")
        restore_colour = body.rindex("draw_set_colour")
        self.assertGreater(restore_alpha, last_draw)
        self.assertGreater(restore_colour, last_draw)

    def test_mark_counts_draws_and_exceptions_separately(self):
        # Section 3 of the instrument-blindness review: TgProbeDrawMark used
        # to swallow every exception uncounted, so "never drew" and "drew in
        # the wrong place" printed identically. draws=/drawExc= must be
        # incremented on the two respective paths and surfaced to a tester.
        body = function_body(self.plugin, "static void TgProbeDrawMark()")
        self.assertIn("g_TgMarkDraws", body)
        self.assertIn("g_TgMarkDrawExc", body)
        self.assertIn("catch (...) { InterlockedIncrement(&g_TgMarkDrawExc); }", body)
        last_draw = body.rindex("draw_rectangle")
        draws_incremented = body.rindex("InterlockedIncrement(&g_TgMarkDraws)")
        self.assertGreater(draws_incremented, last_draw)
        mark_cmd = function_body(self.plugin, "static void TgProbeMarkCommand(const std::string& rest)")
        self.assertIn("g_TgMarkDraws", mark_cmd)
        self.assertIn("g_TgMarkDrawExc", mark_cmd)
        spurn_cmd = function_body(self.plugin, "static void TgProbeSpurnCommand(const std::string& rest)")
        self.assertIn("markDraws=", spurn_cmd)
        self.assertIn("markDrawExc=", spurn_cmd)

    def test_research_only_names_do_not_survive_stripping(self):
        self.assertNotIn("TgProbeSpurn", self.stripped)
        self.assertNotIn("TgProbeMark", self.stripped)
        self.assertNotIn("TgProbeSpurnAfterDraw", self.stripped)

    def test_kplayercommands_is_unchanged_from_7aa3c66_plus_toggleborder(self):
        # P2 (ToggleIndicatorShipContractTests below) adds `toggleborder` -
        # the one entry this set has ever gained since 7aa3c66 - so this
        # class's own P1b-era assertion (which held through session 4) is
        # updated here rather than left to go stale once the ship command
        # exists.
        match = re.search(
            r"static const std::unordered_set<std::string> kPlayerCommands = \{(.*?)\};",
            self.plugin, re.S)
        self.assertIsNotNone(match, "kPlayerCommands not found")
        entries = {tok.strip().strip('"') for tok in match.group(1).split(",") if tok.strip()}
        expected = {
            "ping", "density", "reveal", "specialrate", "dropmult",
            "stat", "statadd", "raredrop", "droprate", "dungeonkey",
            "headhunter", "hhdur", "hhmap", "hhdefault", "hhlabel", "tyrant", "beacon",
            "beaconrange", "beaconmode", "beaconwake", "beaconspawn", "beaconfarstep",
            "tyrantchance", "tyrantaffix", "hhlabelfont", "hhlabeloffset", "hhlabelmax",
            "enemyspeed", "rarity", "sigdrop", "angelicdrop", "relicfilter", "orbpickup",
            "satmods", "petquest", "toggleborder",
            # T1 (issue #11, Track A): the re-cast guard (ToggleGuardContractTests).
            "toggleguard",
        }
        self.assertEqual(entries, expected)


class ToggleIndicatorShipContractTests(unittest.TestCase):
    """The shipped indicator (P2, issue #11, Track B): `toggleborder`.

    Companion to test_toggle_skill_behavior.py (ToggleIndicatorDraw end to
    end, source of the `indicator_` PASS lines) and ToggleIndicatorReadContractTests
    (the read itself). This class pins the ship-only parts: the command is a
    real player command with no new hook, the draw call sits right after the
    head labels outside any research block, OFF is the function's very
    first statement, the slot routine names every field session 3 recorded,
    and the panel mirrors every `mod_pet_quest_pickup` site.
    """

    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_SRC.read_text(encoding="utf-8")
        cls.research_doc = (FORGEPACT_DIR / "docs" / "toggle-skills-research.md").read_text(encoding="utf-8")
        cls.panel = (SRC_DIR / "forgepact.py").read_text(encoding="utf-8")

    def test_toggleborder_is_a_player_command_with_no_new_hook(self):
        match = re.search(
            r"static const std::unordered_set<std::string> kPlayerCommands = \{(.*?)\};",
            self.plugin, re.S)
        self.assertIsNotNone(match)
        entries = {tok.strip().strip('"') for tok in match.group(1).split(",") if tok.strip()}
        self.assertIn("toggleborder", entries)
        start = self.plugin.index('if (lc == "toggleborder")')
        end = self.plugin.index('if (lc == "relicfilter")', start)
        branch = self.plugin[start:end]
        self.assertIn('v == "off" || v == "0"', branch)
        self.assertNotIn("HookOneScript(", branch)

    def test_toggleborder_dispatches_stat(self):
        # Follow-up (indicator's reviews, F): `toggleborder stat` is
        # read-only - it must not be reachable through the same branch that
        # calls g_ToggleBorderOn.store(...).
        start = self.plugin.index('if (lc == "toggleborder")')
        end = self.plugin.index('if (lc == "relicfilter")', start)
        branch = self.plugin[start:end]
        self.assertIn('v == "stat"', branch)
        stat_start = branch.index('v == "stat"')
        stat_end = branch.index('v == "off" || v == "0"', stat_start)
        stat_branch = branch[stat_start:stat_end]
        self.assertIn("ToggleBorderStats()", stat_branch)
        self.assertNotIn("g_ToggleBorderOn.store", stat_branch)

    def test_toggleborder_outputs_name_every_counter(self):
        # Both `toggleborder 0` and `toggleborder stat` share
        # ToggleBorderCountersLine(), so both outputs name every counter by
        # construction; `stat` additionally reports enabled=on|off.
        counters_line = function_body(self.plugin, "static std::string ToggleBorderCountersLine(")
        for key in ("drawn=", "on=", "off=", "unreadable=", "noHud=", "noRow0=",
                    "noTalent=", "foreign=", "drawExc="):
            self.assertIn(key, counters_line, key)
        stats_body = function_body(self.plugin, "static void ToggleBorderStats(")
        self.assertIn("ToggleBorderCountersLine()", stats_body)
        self.assertIn("enabled=", stats_body)
        start = self.plugin.index('if (lc == "toggleborder")')
        end = self.plugin.index('if (lc == "relicfilter")', start)
        branch = self.plugin[start:end]
        zero_start = branch.index('v == "off" || v == "0"')
        zero_body = branch[zero_start:branch.index("} else {", zero_start)]
        self.assertIn("ToggleBorderCountersLine()", zero_body)

    def test_no_slot_counter_removed(self):
        # Follow-up: split into noHud/noRow0/noTalent - the old identifier
        # must not survive anywhere in the plugin.
        self.assertNotIn("g_TibNoSlot", self.plugin)

    def test_draw_exception_is_counted(self):
        # Follow-up: the catch after the outline loop must not swallow the
        # exception uncounted.
        body = function_body(self.plugin, "static void ToggleIndicatorDraw(")
        catch_block = body[body.rindex("} catch (...)"):]
        self.assertIn("g_TibDrawExc", catch_block)

    def test_draw_call_follows_head_labels_outside_research(self):
        body = function_body(self.plugin, "static RValue& Hook_DrawHudBuffs(")
        self.assertIn("HhDrawHeadLabels();", body)
        self.assertIn("ToggleIndicatorDraw();", body)
        self.assertLess(body.index("HhDrawHeadLabels();"), body.index("ToggleIndicatorDraw();"))
        stripped = strip_research_blocks(self.plugin)
        self.assertIn("ToggleIndicatorDraw();", function_body(stripped, "static RValue& Hook_DrawHudBuffs("))

    def test_off_is_the_first_statement(self):
        body = function_body(self.plugin, "static void ToggleIndicatorDraw(")
        first_statement = body.strip().splitlines()[0].strip()
        self.assertEqual(first_statement, "if (!g_ToggleBorderOn.load()) return;")

    def test_frame_callback_never_calls_the_indicator(self):
        body = function_body(self.plugin, "void FrameCallback(")
        self.assertNotIn("ToggleIndicator", body)

    def test_slot_routine_names_every_recorded_field(self):
        # The `### After session 3` decision line, not the earlier prose
        # mentions of the phrase itself or the R5 results-table cell.
        after = self.research_doc[self.research_doc.index("### After session 3"):]
        m = re.search(r"^\*\*Slot geometry fields:.*$", after, re.M)
        self.assertIsNotNone(m, "Slot geometry fields: line not found")
        names = [n for n in re.findall(r"[A-Za-z_][A-Za-z0-9_]*", m.group(0))
                 if n not in ("Slot", "geometry", "fields")]
        self.assertIn("row0", names)
        self.assertIn("talentId", names)
        self.assertIn("navBboxX", names)
        self.assertIn("navBboxY", names)
        self.assertIn("navBboxWidth", names)
        self.assertIn("navBboxHeight", names)
        slot_routine = function_body(self.plugin, "static bool ToggleIndicatorFindSlot(")
        for name in names:
            self.assertIn(f'"{name}"', slot_routine, name)

    def test_draw_saves_and_restores_colour_and_alpha(self):
        body = function_body(self.plugin, "static void ToggleIndicatorDraw(")
        get_colour = body.index("draw_get_colour")
        get_alpha = body.index("draw_get_alpha")
        first_set = body.index("draw_set_")
        self.assertLess(get_colour, first_set)
        self.assertLess(get_alpha, first_set)
        last_draw = body.rindex("draw_rectangle")
        restore_alpha = body.rindex("draw_set_alpha")
        restore_colour = body.rindex("draw_set_colour")
        self.assertGreater(restore_alpha, last_draw)
        self.assertGreater(restore_colour, last_draw)

    def test_marker_required_flag_matches_state_flash_purgatory(self):
        # `## State` (forgepact-toggle-indicator-plan.md) reads `flash:
        # purgatory` (session 4, D-R2): the shipped draw must require the
        # Purgatory marker, i.e. call Decide with requireMarker=true.
        body = function_body(self.plugin, "static void ToggleIndicatorDraw(")
        self.assertIn("ToggleIndicatorModel::Decide(detail, /*requireMarker=*/true)", body)

    # ---- the panel (mirrors every mod_pet_quest_pickup site) ---------------

    def test_defaults_has_toggle_indicator_off(self):
        self.assertIn("mod_toggle_indicator", forgepact.DEFAULTS)
        self.assertFalse(forgepact.DEFAULTS["mod_toggle_indicator"])

    def test_build_cmds_omits_toggleborder_when_disabled(self):
        cfg = dict(forgepact.DEFAULTS)
        self.assertNotIn("toggleborder 1", forgepact.build_cmds(cfg))

    def test_build_cmds_emits_toggleborder_when_enabled(self):
        cfg = dict(forgepact.DEFAULTS)
        cfg["mod_toggle_indicator"] = True
        self.assertIn("toggleborder 1", forgepact.build_cmds(cfg))

    def test_html_has_the_mods_tab_control(self):
        self.assertIn('id="mod_toggle_indicator"', forgepact.HTML)

    def test_panel_sends_the_live_command(self):
        self.assertIn(
            "f\"toggleborder {1 if cfg['mod_toggle_indicator'] else 0}\"",
            self.panel,
        )


def git_show(ref_path: str):
    """`git show <ref>:<path>` in ForgePact/, LF-normalised; None if unreadable."""
    try:
        return subprocess.run(
            ["git", "-C", str(FORGEPACT_DIR), "show", ref_path],
            capture_output=True, check=True,
        ).stdout.decode("utf-8").replace("\r\n", "\n")
    except (OSError, subprocess.CalledProcessError):
        return None


class ToggleGuardContractTests(unittest.TestCase):
    """The re-cast guard (T1, issue #11, Track A): `toggleguard`.

    Companion to test_toggle_skill_behavior.py (HookTalentUseClass end to
    end, the `guard_` PASS lines). This class pins what a source read can:
    the command is a real player command that installs nothing itself; the
    one TalentUseClass install sits in FrameCallback behind the relicfilter
    gate (guide Known Limitations item 8); the hook's order - the research
    note, the off fast path, the caller by name, the guarded talent - and
    what it must never contain; the `tgprobe` row rebound onto it; the shared
    counters line in the player build; and the panel.
    """

    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_SRC.read_text(encoding="utf-8")
        cls.stripped = strip_research_blocks(cls.plugin)
        cls.header = (FORGEPACT_DIR / "plugin" / "include" / "ForgePact" / "ToggleSkillMod.hpp").read_text(encoding="utf-8")
        cls.panel = (SRC_DIR / "forgepact.py").read_text(encoding="utf-8")
        start = cls.plugin.index('if (lc == "toggleguard")')
        end = cls.plugin.index('if (lc == "toggleborder")', start)
        cls.branch = cls.plugin[start:end]
        cls.hook = function_body(cls.plugin, "static RValue& HookTalentUseClass(")

    # ---- the model (ToggleSkillMod.hpp) ---------------------------------------

    def test_model_is_a_two_valued_decision_on_three_inputs(self):
        self.assertIn("enum class ToggleGuardDecision { Pass, Refuse };", self.header)
        self.assertIn("class ToggleGuardModel", self.header)
        self.assertIn("ToggleGuardDecision Decide(bool enabled, bool callerIsDoubleCast, int talentId) const",
                      self.header)
        includes = [line.strip() for line in self.header.splitlines() if line.strip().startswith("#include")]
        self.assertEqual(includes, ['#include "Common.hpp"'])

    def test_enabled_flag_is_an_atomic_load(self):
        self.assertIn("bool IsEnabled() const { return m_Enabled.load(); }",
                      self.header[self.header.index("class ToggleGuardMod"):])

    def test_indicator_decide_is_unchanged_from_ab6fed5(self):
        old = git_show("ab6fed5:plugin/include/ForgePact/ToggleSkillMod.hpp")
        if old is None:
            self.skipTest("ab6fed5 is not readable here")
        sig = "static ToggleIndicatorState Decide("
        self.assertEqual(function_body(old, sig), function_body(self.header.replace("\r\n", "\n"), sig))

    # ---- the command ------------------------------------------------------------

    def test_toggleguard_is_a_player_command(self):
        match = re.search(r"kPlayerCommands = \{(.*?)\};", self.plugin, re.S)
        self.assertIsNotNone(match)
        self.assertIn('"toggleguard"', match.group(1))

    def test_handler_treats_zero_as_off_dispatches_stat_and_installs_nothing(self):
        self.assertIn('v == "off" || v == "0"', self.branch)
        self.assertIn('v == "stat"', self.branch)
        stat = self.branch[self.branch.index('v == "stat"'):self.branch.index('v == "off" || v == "0"')]
        self.assertIn("ToggleGuardStats()", stat)
        self.assertNotIn("SetEnabled", stat)
        self.assertNotIn("HookOneScript(", self.branch)
        self.assertIn("ToggleGuardCountersLine()", self.branch)

    def test_counters_reach_the_player_build(self):
        # `toggleguard 0` and `toggleguard stat` share the counters line, and
        # every key survives stripping.
        line = function_body(self.stripped, "static std::string ToggleGuardCountersLine(")
        for key in ("refused=", "passed=", "procSeen=", "selfUnreadable=", "objUnresolved=", "hook="):
            self.assertIn(key, line, key)
        self.assertNotIn("lastProcRet", line)   # research build only (session 5's V7)
        self.assertIn("lastProcRet=", function_body(self.plugin, "static std::string ToggleGuardCountersLine("))
        self.assertIn("ToggleGuardCountersLine()", function_body(self.stripped, "static void ToggleGuardStats("))
        state = function_body(self.stripped, "static std::string ToggleGuardHookState(")
        for literal in ('"not installed"', '"installed"', '"TABLE-ONLY"'):
            self.assertIn(literal, state)

    # ---- the install ------------------------------------------------------------

    def test_one_install_behind_the_relicfilter_gate(self):
        needle = 'HookOneScript("TalentUseClass"'
        self.assertEqual(self.plugin.count(needle), 1)
        frame = function_body(self.plugin, "void FrameCallback(")
        self.assertIn(needle, frame)
        call = frame.index(needle)
        gate = frame.rindex("if (ForgePact::ToggleGuardMod::Instance().IsPending()", 0, call)
        enclosing = frame[gate:call]
        for name in ("g_Setup", "HhResolveLocalPlayer", "(fc % 60) == 0"):
            self.assertIn(name, enclosing, name)
        self.assertIn("ClearPending()", enclosing)
        self.assertIn(needle, function_body(self.stripped, "void FrameCallback("))

    # ---- the hook ----------------------------------------------------------------

    def test_hook_order(self):
        body = self.hook
        note = body.index("TgProbeNoteTalentUseClass(S, O, argc, A);")
        guard = body.rindex("#ifndef FORGEPACT_RELEASE", 0, note)
        endif = body.index("#endif", note)
        self.assertEqual(body[guard:note].count("\n"), 1)   # the note is the pair's only line
        self.assertEqual(body[note:endif].count("\n"), 1)
        fast = body.index("if (!ForgePact::ToggleGuardMod::Instance().IsEnabled()) "
                          "return g_OrigTalentUseClass(S, O, R, argc, A);")
        object_index = body.index('"object_index"')
        obj = body.index("GameObject::Universal_Double_Cast_obj")
        talent = body.index("kToggleIndicatorTalentId")
        self.assertLess(endif, fast)
        self.assertLess(fast, object_index)
        self.assertLess(object_index, obj)
        self.assertLess(obj, talent)

    def test_hook_reads_nothing_it_must_not(self):
        body = self.hook
        for forbidden in ("GetMembers(", "CallBuiltinEx", "5318", "instance_number", "instance_find",
                          "ToggleIndicatorRead"):
            self.assertNotIn(forbidden, body, forbidden)
        self.assertIsNone(re.search(r"\b240\b", body))   # the one 240 is kToggleIndicatorTalentId

    def test_caller_and_object_are_read_with_the_value_ref_aware_predicate(self):
        # This runner returns object_index as VALUE_REF (ModuleMain.cpp's
        # N1ObjectIndex, "kind=15 str=ref object ..."). A plain-number-only
        # kind check here fails open on every live call, so both indices go
        # through the shared predicate, which masks the flag bits and accepts
        # VALUE_REF; the behavior harness's guard_on/self_object_index_kinds
        # scenarios prove it end to end.
        body = self.hook
        self.assertEqual(body.count("N1ObjectIndex("), 2)
        self.assertLess(body.index('"object_index"'), body.index("N1ObjectIndex("))
        self.assertNotIn(".m_Kind == VALUE_REAL || oi.", body)
        self.assertIsNone(re.search(r"\boi\.m_Kind\b", body))
        self.assertIsNone(re.search(r"\.ToDouble\(\)", body[:body.index("ToggleGuardModel kGuard")]))

    def test_refusal_returns_the_result_without_the_original(self):
        body = self.hook
        refuse = body.index("ToggleGuardDecision::Refuse")
        self.assertIn("return R;", body[refuse:body.index("}", refuse)])

    def test_talent_use_hook_is_unchanged_from_ab6fed5(self):
        old = git_show("ab6fed5:plugin/ModuleMain.cpp")
        if old is None:
            self.skipTest("ab6fed5 is not readable here")
        sig = "static RValue& HookTalentUse("
        self.assertEqual(function_body(old, sig), function_body(self.plugin.replace("\r\n", "\n"), sig))

    # ---- the research row -----------------------------------------------------------

    def test_probe_row_rebinds_to_the_guard_hook(self):
        rows = [m.groupdict() for m in SCRIPT_ROW.finditer(macro_body(self.plugin, "#define TGPROBE_SCRIPTS(X)"))]
        row = next(r for r in rows if r["safe"] == "TalentUseClass")
        self.assertEqual(row["orig"].strip(), "&g_OrigTalentUseClass")
        self.assertEqual(row["via"].strip(), '"HookTalentUseClass"')
        self.assertIn("kTgRet", row["flags"])

    def test_research_note_never_ships(self):
        self.assertIn("TgProbeNoteTalentUseClass", self.plugin)
        self.assertNotIn("TgProbeNoteTalentUseClass", self.stripped)

    # ---- the panel (mirrors every mod_toggle_indicator site) --------------------------

    def test_defaults_has_toggle_guard_off(self):
        self.assertIn("mod_toggle_guard", forgepact.DEFAULTS)
        self.assertIs(forgepact.DEFAULTS["mod_toggle_guard"], False)

    def test_build_cmds_omits_toggleguard_when_disabled(self):
        self.assertFalse([c for c in forgepact.build_cmds(dict(forgepact.DEFAULTS)) if "toggleguard" in c])

    def test_build_cmds_emits_toggleguard_when_enabled(self):
        cfg = dict(forgepact.DEFAULTS)
        cfg["mod_toggle_guard"] = True
        self.assertIn("toggleguard 1", forgepact.build_cmds(cfg))

    def test_html_has_the_mods_tab_control(self):
        self.assertIn('id="mod_toggle_guard"', forgepact.HTML)

    def test_panel_sends_the_live_command(self):
        self.assertIn("f\"toggleguard {1 if cfg['mod_toggle_guard'] else 0}\"", self.panel)


# The static candidate table (docs/toggle-skills-research.md, "### Other toggle
# skills: the static candidate table"): abilityId -> (the SDK objects the
# static search named as ON-object candidates, the predicted sub-talent slot).
CANDIDATE_ROWS = {
    "soulSpurn": ({"White_Mage_Soul_Spurn_AOE_obj"}, 12),
    "lunarOrbit": ({"Exo_Lunar_Orbit_obj", "Exo_Lunar_Orbit_Crescent_Moon_obj"}, 11),
    "crematus": ({"Plague_Doctor_Crematus_obj", "Plague_Doctor_Crematus_Controller_obj"}, 13),
    "counter": ({"Shield_Lancer_Counter_World_obj"}, 13),
    "submergedKnives": ({"Butcher_Submerged_Knives_obj", "Butcher_Submerged_Knives_Knifehoarder_obj"}, 13),
    "maelstromOfFrost": ({"Prophet_Maelstrom_obj", "Prophet_Maelstrom_Storm_obj", "Prophet_Maelstrom_Meteor_obj"}, 11),
    "blender": ({"Butcher_Blender_obj", "Butcher_Blender_Nanoblades_obj"}, 14),
}
SEED_ROW = re.compile(
    r'\{\s*"(?P<ability>\w+)",\s*HeroSiege::Objects::GameObject::(?P<obj>\w+),\s*(?P<talent>[^,]+),'
    r'\s*(?P<marker>[^,]+),\s*(?P<timer>[^,]+),\s*(?P<sub>\d+)\s*\}'
)
# The production bodies the research build must leave exactly as T1 shipped
# them (62a67d2): nothing outside a research block changes in phase R.
UNCHANGED_SINCE_T1 = (
    "static ForgePact::ToggleIndicatorState ToggleIndicatorRead(",
    "static bool ToggleIndicatorFindSlot(",
    "static void ToggleIndicatorDraw(",
    "static RValue& HookTalentUseClass(",
    "static RValue& Hook_DrawHudBuffs(",
    "void FrameCallback(",
)


class ToggleTableProbeContractTests(unittest.TestCase):
    """The session-6 research instrument (issue #11 generalisation, phase R).

    `tgprobe talents` enumerates every talent struct; `tgprobe tgl` is a
    runtime table of toggle-skill candidates, prefilled with the seven rows the
    static search found, each read every draw through a generalised form of
    the shipped read, with row 0 compared against the shipped read itself
    (agree=/disagree=). Companion to test_toggle_skill_behavior.py, whose
    `table/` scenarios run the pure parts. Research build only; nothing here
    is a border input, and there is no partial-draw control (D-U9).
    """

    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_SRC.read_text(encoding="utf-8")
        cls.stripped = strip_research_blocks(cls.plugin)
        start = cls.plugin.index(BLOCK_START)
        cls.block = cls.plugin[start:cls.plugin.index(BLOCK_END, start)]
        seeds_start = cls.plugin.index("static const TgTglSeed kTgTglSeeds[] = {")
        cls.seeds = cls.plugin[seeds_start:cls.plugin.index("};", seeds_start)]
        cls.seed_rows = [m.groupdict() for m in SEED_ROW.finditer(cls.seeds)]

    def test_tgprobe_dispatches_talents_and_tgl(self):
        body = function_body(self.plugin, "static void TgProbeCommand(const std::string& rest)")
        self.assertIn('sub == "talents"', body)
        self.assertIn('sub == "tgl"', body)
        self.assertIn("TgProbeTalentsCommand(subRest)", body)
        self.assertIn("TgProbeTglCommand(subRest)", body)

    def test_talents_walks_the_talent_map_and_prints_the_listed_fields(self):
        body = function_body(self.plugin, "static void TgProbeTalentsCommand(")
        for needle in ('"talentStructMap"', '"ds_map_find_first"', '"ds_map_find_next"', '"abilityId"',
                       '"abilityAura"', '"abilityDuration"', '"abilityCooldown"', '"abilityLength"',
                       '"abilityTags"', "ids=", "shown=", "N1GetTalentStruct("):
            self.assertIn(needle, body)
        # Every field read on its own: an unreadable read prints `unreadable`,
        # a missing key `absent` - never a default value.
        field = function_body(self.plugin, "static std::string TgProbeTalentsField(")
        self.assertIn('return "unreadable";', field)
        self.assertIn('return "absent";', field)

    def test_tgl_dispatches_every_subcommand(self):
        body = function_body(self.plugin, "static void TgProbeTglCommand(const std::string& rest)")
        for sub in ("on", "off", "add", "list", "clear", "slots", "fields", "sub", "timer"):
            self.assertIn(f'sub == "{sub}"', body)
        # `clear` keeps row 0 (the measured row and the agreement control).
        self.assertIn("g_TgTgl.resize(1)", body)

    def test_sampler_switch_is_off_by_default_and_gates_every_read(self):
        # Round 2: `tgprobe tgl on|off` (zero-as-off, as elsewhere); off by
        # default, and off returns before any builtin call - the gate is the
        # sampler's first statement, ahead of the room read, the seed, the
        # name resolution and both reads (instance_number lives in those).
        self.assertIn("static bool g_TgTglSamplerOn = false;", self.plugin)
        command = function_body(self.plugin, "static void TgProbeTglCommand(const std::string& rest)")
        on = command[command.index('sub == "on"'):command.index('sub == "off"')]
        self.assertIn('sub == "1"', on)
        self.assertIn("g_TgTglSamplerOn = true;", on)
        off = command[command.index('sub == "off"'):command.index('sub == "add"')]
        self.assertIn('sub == "0"', off)
        self.assertIn("g_TgTglSamplerOn = false;", off)
        sampler = function_body(self.plugin, "static void TgProbeTglAfterDraw()")
        statements = sampler.strip()
        self.assertTrue(statements.startswith("if (!g_TgTglSamplerOn) return;"), statements[:80])
        gate = sampler.index("if (!g_TgTglSamplerOn) return;")
        for call in ("TgProbeTglSeed()", "CurrentRoomKey()", "TgProbeTglResolveObject(", "TgProbeTglRead(",
                     "ToggleIndicatorRead(", "TgProbeTglSnapshot("):
            self.assertLess(gate, sampler.index(call), call)
        self.assertNotIn("CallBuiltin", sampler[:sampler.index("TgProbeTglSeed()")])
        self.assertIn("sampler=", function_body(self.plugin, "static void TgProbeTglList()"))
        # The field snapshot is throttled; the reads stay per draw.
        self.assertIn("static constexpr long kTgTglSnapshotEveryDraws = 30;", self.plugin)
        self.assertIn(">= kTgTglSnapshotEveryDraws", sampler)

    def test_fields_snapshot_reads_the_own_instance_the_read_used(self):
        # Round 2: not instance_find(obj, 0), which can be a foreign or a
        # leftover instance; the first own instance TgProbeTglRead scanned.
        read = function_body(self.plugin, "static ForgePact::ToggleIndicatorState TgProbeTglRead(")
        self.assertIn("r.ownInst = inst;", read)
        self.assertLess(read.index("++d.mine;"), read.index("r.ownInst = inst;"))
        snapshot = function_body(self.plugin, "static void TgProbeTglSnapshot(")
        self.assertIn("static void TgProbeTglSnapshot(const TgTglReadResult& r, TgTglFieldSample& out)", self.plugin)
        self.assertNotIn('"instance_find"', snapshot)
        self.assertIn("r.ownInst", snapshot)
        self.assertLess(snapshot.index("if (!r.ownFound)"), snapshot.index('"variable_instance_get_names"'))
        self.assertIn("fields: no own instance", function_body(self.plugin, "static std::string TgProbeTglFieldsText("))
        self.assertIn("TgProbeTglFieldsText(row.fieldsFirst)", function_body(self.plugin, "static void TgProbeTglFields("))

    def test_tgl_add_resolves_by_name_and_stores_nothing_when_unresolved(self):
        resolve = function_body(self.plugin, "static bool TgProbeTglResolveObject(")
        self.assertIn('"asset_get_index"', resolve)
        self.assertIn("return outObjIdx >= 0;", resolve)
        add = function_body(self.plugin, "static void TgProbeTglAdd(")
        self.assertIn("TgProbeTglResolveObject(objectName, objIdx)", add)
        self.assertIn("unresolved", add)
        self.assertIn("kTgTglCap", add)
        unresolved = add.index("unresolved")
        self.assertLess(unresolved, add.index("push_back"))
        self.assertIn("return;", add[unresolved:add.index("push_back")])
        # sdk=<index> by scanning the enumerator range, or sdk=none.
        self.assertIn("TgProbeTglSdkIndex(objectName)", add)
        self.assertIn("sdk=", add)
        scan = function_body(self.plugin, "static int TgProbeTglSdkIndex(")
        self.assertIn("HeroSiege::Objects::kObjectCount", scan)
        self.assertIn("GetObjectName", scan)

    def test_table_is_capped_at_16_and_prefilled_with_the_seven_candidate_rows(self):
        self.assertIn("static constexpr int kTgTglCap = 16;", self.plugin)
        self.assertEqual(len(self.seed_rows), 7, self.seeds)
        self.assertEqual([r["ability"] for r in self.seed_rows], list(CANDIDATE_ROWS))
        row0 = self.seed_rows[0]
        self.assertEqual(row0["obj"], "White_Mage_Soul_Spurn_AOE_obj")
        self.assertEqual(row0["talent"].strip(), "kToggleIndicatorTalentId")
        self.assertEqual(row0["marker"].strip(), '"purgatory"')
        self.assertEqual(row0["timer"].strip(), '"destroyTimer"')
        objects_hpp = (SDK_INCLUDE / "objects.hpp").read_text(encoding="utf-8")
        for row in self.seed_rows:
            candidates, sub = CANDIDATE_ROWS[row["ability"]]
            self.assertIn(row["obj"], candidates, row)
            self.assertRegex(objects_hpp, rf"\b{row['obj']}\s*=\s*\d+,", row)
            self.assertEqual(int(row["sub"]), sub, row)
        for row in self.seed_rows[1:]:
            # Every other row's talent id and marker are unknown statically.
            self.assertEqual(row["talent"].strip(), "-1", row)
            self.assertEqual(row["marker"].strip(), "nullptr", row)
            self.assertEqual(row["timer"].strip(), '"destroyTimer"', row)
        self.assertNotIn("5759", self.seeds)

    def test_generalised_read_is_parameterised_and_uses_the_shipped_shape(self):
        signature = "static ForgePact::ToggleIndicatorState TgProbeTglRead("
        start = self.plugin.index(signature)
        params = self.plugin[start:self.plugin.index(")", start)]
        for param in ("double objIdx", "const char* marker", "const char* ownership", "const char* timer"):
            self.assertIn(param, params)
        body = function_body(self.plugin, signature)
        for needle in ('"instance_number"', '"instance_find"', '"isMyClient"', "kToggleIndicatorScanCap",
                       "ToggleIndicatorReadTruth("):
            self.assertIn(needle, body)
        for banned in ("CallBuiltinEx", "HhResolveLocalPlayer", '"playerNumber"', "GetMembers("):
            self.assertNotIn(banned, body)

    def test_sampler_compares_row0_with_the_shipped_read(self):
        sampler = function_body(self.plugin, "static void TgProbeTglAfterDraw()")
        self.assertIn("ToggleIndicatorRead(", sampler)
        self.assertIn("TgProbeTglRead(", sampler)
        self.assertIn("TgProbeTglSameDetail(", sampler)
        self.assertIn("++g_TgTglAgree", sampler)
        self.assertIn("++g_TgTglDisagree", sampler)
        show = function_body(self.plugin, "static void TgProbeTglShow()")
        for needle in ("agree=", "disagree=", "state=", "n=", "mine=", "others=", "unattributed=",
                       "markedOn=", "timer=", "samples=", "transitions=", "lastTransitionFrame=",
                       "firstAfterRoomChange"):
            self.assertIn(needle, show)
        # It hangs off the existing research call in Hook_DrawHudBuffs.
        self.assertIn("TgProbeTglAfterDraw();", function_body(self.plugin, "static void TgProbeSpurnAfterDraw()"))

    def test_tgl_sub_reads_the_sub_talent_array_per_index_and_row(self):
        body = function_body(self.plugin, "static void TgProbeTglSub()")
        self.assertIn('"subTalentMap"', body)
        self.assertIn('"array_length"', body)
        self.assertIn('"array_get"', body)
        self.assertIn('"t" + std::to_string(row.talentId)', body)
        self.assertIn("for (const TgTglRow& row : g_TgTgl)", body)

    def test_tgl_timer_prints_the_discriminator_fields(self):
        line = function_body(self.plugin, "static std::string TgProbeTglTimerLine(")
        for needle in ("first=", "last=", "min=", "max=", "unreadable=", "atPredicted="):
            self.assertIn(needle, line)
        self.assertIn("TgProbeTglTimerLine(row.timerStats)", function_body(self.plugin, "static void TgProbeTglTimer()"))

    def test_no_partial_draw_control(self):
        # D-U9: no countdown and no partial border, so no `mark ring`; the
        # table instruments draw nothing at all.
        mark = function_body(self.plugin, "static void TgProbeMarkCommand(const std::string& rest)")
        self.assertNotIn('"ring"', mark)
        self.assertNotIn("mark ring", self.block)
        table = self.plugin[self.plugin.index("struct TgTglReadResult {"):
                            self.plugin.index("static void TgProbeTalentsCommand(")]
        self.assertNotIn("draw_", table)

    def test_research_only_names_do_not_survive_stripping(self):
        for name in ("TgProbeTgl", "TgProbeTalents", "TgTgl", "kTgTgl", "g_TgTalentsIdByAbility"):
            self.assertIn(name, self.block)
            self.assertNotIn(name, self.stripped)

    def test_production_bodies_unchanged_from_62a67d2(self):
        old = git_show("62a67d2:plugin/ModuleMain.cpp")
        if old is None:
            self.skipTest("git cannot read 62a67d2")
        for signature in UNCHANGED_SINCE_T1:
            self.assertEqual(function_body(self.plugin, signature), function_body(old, signature), signature)

    def test_kplayercommands_unchanged_from_62a67d2(self):
        old = git_show("62a67d2:plugin/ModuleMain.cpp")
        if old is None:
            self.skipTest("git cannot read 62a67d2")
        pattern = r"static const std::unordered_set<std::string> kPlayerCommands = \{(.*?)\};"
        self.assertEqual(re.search(pattern, self.plugin, re.S).group(1), re.search(pattern, old, re.S).group(1))


if __name__ == "__main__":
    unittest.main()
