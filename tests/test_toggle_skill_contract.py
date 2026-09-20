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


def declaration_block(source: str, anchor: str) -> str:
    """One whole class/struct/enum declaration: `anchor` up to its closing `};`.

    Phase S pins the header's existing declarations against an older commit
    while the file around them grows, so a whole-file compare no longer says
    anything useful.
    """
    source = source.replace("\r\n", "\n")
    start = source.index(anchor)
    if not anchor.endswith("{"):
        return source[start:source.index(";", start) + 1]
    depth = 0
    for index in range(source.index("{", start), len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    raise AssertionError(f"unterminated declaration: {anchor}")


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
        # If the production read were inside a research-only block, it would
        # not survive stripping to what a player build actually compiles.
        # NARROWED after phase S's review: the production read is the
        # ROW-taking one, which the shipped draw calls; row 0's two aliases
        # (ToggleIndicatorResolveAoeObject/ToggleIndicatorRead) lost their last
        # shipped caller when the draw started walking the table, so they are
        # now research-only - carried in a player build they would be a
        # function with no caller, not a production read.
        self.assertIn("static bool ToggleIndicatorResolveRowObject(", self.stripped)
        self.assertIn("static ForgePact::ToggleIndicatorState ToggleIndicatorReadRow(", self.stripped)
        self.assertIn("static void ToggleIndicatorCountMark(", self.stripped)
        self.assertIn("static void ToggleIndicatorDraw(", self.stripped)
        self.assertNotIn("static bool ToggleIndicatorResolveAoeObject(", self.stripped)
        self.assertNotIn("static ForgePact::ToggleIndicatorState ToggleIndicatorRead(", self.stripped)
        # And nothing a player build compiles calls either alias - the reason
        # they can be research-only at all.
        for alias in ("ToggleIndicatorResolveAoeObject(", "ToggleIndicatorRead(&"):
            self.assertNotIn(alias, self.stripped, alias)

    def test_production_read_uses_the_documented_shape(self):
        # NARROWED in phase S (issue #11, the five-row table): the runtime
        # names this read used to spell - the AOE object, `isMyClient`,
        # `purgatory` - now live in ToggleSkillMod.hpp's table and reach the
        # read as row fields, so they are pinned on row 0 of the table instead
        # of inside the function. What is still pinned here is the SHAPE the
        # research doc's ON=1 control proved, which has not changed.
        read_body = function_body(self.plugin, "static ForgePact::ToggleIndicatorState ToggleIndicatorReadRow(")
        mark_body = function_body(self.plugin, "static void ToggleIndicatorCountMark(")
        resolve_body = function_body(self.plugin, "static bool ToggleIndicatorResolveRowObject(")
        combined = resolve_body + read_body + mark_body
        self.assertIn("GetObjectName(row.onObject)", resolve_body)
        self.assertIn('"instance_number"', combined)
        self.assertIn('"instance_find"', combined)
        self.assertIn("row.ownershipField", combined)
        self.assertIn("row.markField", combined)
        header = (FORGEPACT_DIR / "plugin" / "include" / "ForgePact" / "ToggleSkillMod.hpp").read_text(
            encoding="utf-8")
        row0 = header[header.index('{ "soulSpurn"'):header.index('{ "lunarOrbit"')]
        self.assertIn("GameObject::White_Mage_Soul_Spurn_AOE_obj", row0)
        self.assertIn('"isMyClient"', row0)
        self.assertIn('"purgatory"', row0)
        # Row 0's own aliases still exist and still go through the row read,
        # so the frozen research sampler measures exactly the shipped read.
        self.assertIn("ForgePact::kToggleSkillRows[0]",
                      function_body(self.plugin, "static bool ToggleIndicatorResolveAoeObject("))
        self.assertIn("ForgePact::kToggleSkillRows[0]",
                      function_body(self.plugin, "static ForgePact::ToggleIndicatorState ToggleIndicatorRead("))
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
        body = function_body(self.plugin, "static void TgProbeDrawMark(bool fromHudLayer)")
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
        body = function_body(self.plugin, "static void TgProbeDrawMark(bool fromHudLayer)")
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

    def test_mark_only_draws_at_the_active_layer(self):
        # R round 4: `tgprobe mark` shares the layer setting with
        # `tgprobe sprite`; only the after-draw call site matching the
        # active layer actually draws.
        body = function_body(self.plugin, "static void TgProbeDrawMark(bool fromHudLayer)")
        gate = body.index("if (fromHudLayer != g_TgProbeLayerHud) return;")
        self.assertLess(gate, body.index("draw_get_colour"))
        mark_cmd = function_body(self.plugin, "static void TgProbeMarkCommand(const std::string& rest)")
        self.assertIn("TgProbeLayerName()", mark_cmd)

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
        # NARROWED in phase S: the band draw itself moved into
        # ToggleIndicatorDrawMarker, so the save/restore pair now brackets the
        # CALL to it rather than a loop written inline. The property is the
        # same one - nothing the marker sets outlives the draw.
        body = function_body(self.plugin, "static void ToggleIndicatorDraw(")
        marker = function_body(self.plugin, "static void ToggleIndicatorDrawMarker(")
        call = body.index("ToggleIndicatorDrawMarker(")
        self.assertLess(body.index("draw_get_colour"), call)
        self.assertLess(body.index("draw_get_alpha"), call)
        self.assertGreater(body.rindex("draw_set_alpha"), call)
        self.assertGreater(body.rindex("draw_set_colour"), call)
        # Everything that touches the draw state is inside the marker or the
        # restore - the draw body itself sets nothing before the save.
        self.assertGreater(body.index("draw_set_"), body.index("draw_get_alpha"))
        self.assertIn("draw_rectangle", marker)

    def test_marker_required_flag_matches_state_flash_purgatory(self):
        # RE-PINNED in phase S to the per-row rule. `## State` read `flash:
        # purgatory` (session 4, D-R2) and Soul Spurn is still a marker row, so
        # it still requires its marker; the draw now asks the table, which
        # answers true for every marker or timer row and false only for a
        # `none-needed` one (D-P5).
        body = function_body(self.plugin, "static void ToggleIndicatorDraw(")
        self.assertIn("ToggleIndicatorModel::Decide(detail, ForgePact::ToggleRowRequiresMark(row))", body)
        header = (FORGEPACT_DIR / "plugin" / "include" / "ForgePact" / "ToggleSkillMod.hpp").read_text(
            encoding="utf-8")
        self.assertIn("return row.mark != ToggleOnMark::None;",
                      function_body(header, "inline constexpr bool ToggleRowRequiresMark("))

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


# The five rows session 6 measured, cell by cell, quoted from
# docs/toggle-skills-research.md "## Results" -> "### Toggle skill table" and
# "## Decision" -> "### After session 6". `counter` and `blender` are NOT here
# and must not be: session 6 measured no persistent ON instance for Counter
# (its toggle state is a player buff) and never ran Blender's ON/OFF steps.
SHIPPED_TABLE_ROWS = [
    ("soulSpurn", 12, "White_Mage_Soul_Spurn_AOE_obj", '"isMyClient"', "Marker", '"purgatory"'),
    ("lunarOrbit", 11, "Exo_Lunar_Orbit_Crescent_Moon_obj", "nullptr", "None", "nullptr"),
    ("crematus", 13, "Plague_Doctor_Crematus_Controller_obj", "nullptr", "Marker", '"skillContamination"'),
    ("submergedKnives", 13, "Butcher_Submerged_Knives_Knifehoarder_obj", "nullptr", "None", "nullptr"),
    ("maelstromOfFrost", 11, "Prophet_Maelstrom_obj", '"isMyClient"', "TimerHeld", '"destroyTimer"'),
]
TABLE_ROW = re.compile(
    r'\{\s*"(?P<ability>\w+)",\s*(?P<sub>\d+),\s*HeroSiege::Objects::GameObject::(?P<obj>\w+),\s*'
    r'(?P<own>nullptr|"\w+"),\s*ToggleOnMark::(?P<mark>\w+),\s*(?P<field>nullptr|"\w+"),\s*'
    r'(?P<held>-?[\d.]+)\s*\}',
    re.S,
)
# Names the shipped table now owns: a production function spelling one of
# these again would be a second, drifting copy of the row set.
TABLE_ONLY_NAMES = (
    "White_Mage_Soul_Spurn_AOE_obj", '"purgatory"', '"skillContamination"', '"destroyTimer"',
    '"soulSpurn"', "kToggleIndicatorTalentId",
)
# Nothing in the marker's draw path may reach for a partial or animated shape
# (D-U9 rejected every countdown; D-U13 chose a static banded outline).
BANNED_DRAW_NAMES = (
    "Fraction", "TimerTotal", "timerTotal", "Denominator", "draw_line", "draw_arc",
    "draw_primitive", "draw_sprite", "draw_ellipse", "draw_rectangle_colour",
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


class ToggleSkillTableContractTests(unittest.TestCase):
    """The shipped five-row table, the D-U13 marker and the id resolver (S).

    Companion to test_toggle_skill_behavior.py's `border/*` and `table/*`
    scenarios, which run the decisions; this class pins what only a source
    read can see: that the rows are exactly session 6's measured five, that
    every runtime name lives in the header's table and nowhere else, that the
    marker and its box are the ones the AUTHOR judged - pinned by equality
    with the research probe's own constants rather than by a second copy of
    the numbers - and that the talent ids are resolved at the frame boundary,
    never in a draw or a hook.
    """

    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_SRC.read_text(encoding="utf-8")
        cls.stripped = strip_research_blocks(cls.plugin)
        cls.header = (FORGEPACT_DIR / "plugin" / "include" / "ForgePact" / "ToggleSkillMod.hpp").read_text(
            encoding="utf-8")
        start = cls.header.index("inline constexpr ToggleSkillRow kToggleSkillRows[] = {")
        cls.table = cls.header[start:cls.header.index("\n};", start)]
        cls.rows = [m.groupdict() for m in TABLE_ROW.finditer(cls.table)]
        cls.draw_path = "".join(
            function_body(cls.stripped, sig) for sig in (
                "static void ToggleIndicatorDraw(",
                "static void ToggleIndicatorDrawMarker(",
                "static bool ToggleIndicatorFindSlot(",
                "static void ToggleIndicatorMarkerBox(",
            )
        )

    # ---- S1: the table is session 6's five rows, cell by cell ---------------

    def test_header_includes_only_common(self):
        includes = [line.strip() for line in self.header.splitlines() if line.strip().startswith("#include")]
        self.assertEqual(includes, ['#include "Common.hpp"'])

    def test_table_is_exactly_the_five_measured_rows(self):
        self.assertEqual(len(self.rows), len(SHIPPED_TABLE_ROWS), self.table)
        objects_hpp = (SDK_INCLUDE / "objects.hpp").read_text(encoding="utf-8")
        for row, want in zip(self.rows, SHIPPED_TABLE_ROWS):
            ability, sub, obj, own, mark, field = want
            self.assertEqual(row["ability"], ability, row)
            self.assertEqual(int(row["sub"]), sub, row)
            self.assertEqual(row["obj"], obj, row)
            self.assertEqual(row["own"].strip(), own, row)
            self.assertEqual(row["mark"], mark, row)
            self.assertEqual(row["field"].strip(), field, row)
            self.assertRegex(objects_hpp, rf"\b{obj}\s*=\s*\d+,", row)
        # The one row with a held value: session 6 measured Maelstrom's
        # destroyTimer sitting at exactly -1.000000 while the toggle is on.
        self.assertEqual(float(self.rows[-1]["held"]), -1.0)
        for row in self.rows[:-1]:
            self.assertEqual(float(row["held"]), 0.0, row)

    def test_counter_and_blender_do_not_ship(self):
        for name in ('"counter"', '"blender"', "Shield_Lancer_Counter_World_obj", "Butcher_Blender_obj"):
            self.assertNotIn(name, self.header, name)
            self.assertNotIn(name, self.stripped, name)

    def test_table_carries_no_talent_id(self):
        # D-P1: ids move with every game build, so the table stores none and
        # the plugin spells none outside the research block.
        self.assertNotIn("talentId", self.table)
        self.assertIsNone(re.search(r"\b240\b", self.header))
        self.assertNotIn("kToggleIndicatorTalentId", self.stripped)

    # ---- S2: every runtime name lives in the table -------------------------

    def test_row_names_appear_in_the_header_only(self):
        for name in TABLE_ONLY_NAMES:
            self.assertNotIn(name, self.stripped, name)
        for name in ("White_Mage_Soul_Spurn_AOE_obj", '"purgatory"', '"skillContamination"',
                     '"destroyTimer"', '"soulSpurn"'):
            self.assertIn(name, self.header, name)

    def test_no_partial_or_animated_draw_anywhere(self):
        for name in BANNED_DRAW_NAMES:
            self.assertNotIn(name, self.header, name)
            self.assertNotIn(name, self.draw_path, name)

    # ---- S3: the marker is the look the author judged ----------------------

    def test_marker_band_count_equals_the_probes(self):
        soft = function_body(self.plugin, "static void TgProbeSpriteDrawSoft(")
        probe_bands = int(re.search(r"static constexpr int kBands = (\d+);", soft).group(1))
        shipped = int(re.search(r"static constexpr int kToggleMarkerBands = (\d+);", self.plugin).group(1))
        self.assertEqual(shipped, probe_bands)

    def test_marker_colour_equals_the_probes_deepred_preset(self):
        presets = self.plugin[self.plugin.index("static const TgColourPreset kTgColourPresets[] = {"):]
        preset = re.search(r'\{\s*"deepred",\s*([\d.]+),\s*([\d.]+),\s*([\d.]+)\s*\}', presets)
        self.assertIsNotNone(preset)
        shipped = tuple(
            re.search(rf"static constexpr double kToggleMarker{c} = ([\d.]+);", self.plugin).group(1)
            for c in "RGB"
        )
        self.assertEqual(shipped, preset.groups())

    def test_marker_draws_nested_bands_with_a_linear_alpha_ramp(self):
        body = function_body(self.plugin, "static void ToggleIndicatorDrawMarker(")
        self.assertEqual(body.count('"make_colour_rgb"'), 1)
        self.assertIn("for (int i = 0; i < kToggleMarkerBands; ++i)", body)
        self.assertIn("const double t = (double)i / (double)(kToggleMarkerBands - 1);", body)
        self.assertIn('g_Yytk->CallBuiltin("draw_set_alpha", { RValue(1.0 - t) });', body)
        self.assertIn("RValue(x - i), RValue(y - i), RValue(x + w + i), RValue(y + h + i)", body)
        # No scale factor, no frame or time term, no alpha floor but 0.0.
        for banned in ("g_RuntimeFrame", "sin(", "Scale", "scale", "alphaMin", "floor"):
            self.assertNotIn(banned, body, banned)

    def test_marker_never_calls_a_research_function(self):
        for sig in ("static void ToggleIndicatorDrawMarker(", "static void ToggleIndicatorDraw(",
                    "static void ToggleIndicatorMarkerBox(", "static bool ToggleIndicatorFindSlot("):
            self.assertNotIn("TgProbe", function_body(self.plugin, sig), sig)

    # ---- S4: the box is derived, never hardcoded ---------------------------

    def test_box_offset_equals_the_probes_and_reproduces_du12(self):
        pairs = (("kToggleMarkerBoxDX", "kTgTunedBoxDX"), ("kToggleMarkerBoxDY", "kTgTunedBoxDY"),
                 ("kToggleMarkerBoxDW", "kTgTunedBoxDW"), ("kToggleMarkerBoxDH", "kTgTunedBoxDH"))
        values = {}
        for shipped_name, probe_name in pairs:
            shipped = re.search(rf"static constexpr double {shipped_name} = (-?[\d.]+);", self.plugin)
            probe = re.search(rf"static constexpr double {probe_name} = (-?[\d.]+);", self.plugin)
            self.assertIsNotNone(shipped, shipped_name)
            self.assertIsNotNone(probe, probe_name)
            self.assertEqual(shipped.group(1), probe.group(1), shipped_name)
            values[shipped_name] = float(shipped.group(1))
        body = function_body(self.plugin, "static void ToggleIndicatorMarkerBox(")
        self.assertIn("outX = std::round(bboxX + kToggleMarkerBoxDX);", body)
        self.assertIn("outY = std::round(bboxY + kToggleMarkerBoxDY);", body)
        self.assertIn("outW = std::round(bboxW + kToggleMarkerBoxDW);", body)
        self.assertIn("outH = std::round(bboxH + kToggleMarkerBoxDH);", body)
        # D-U12's worked example, in Python, as a check on the constants above
        # rather than a duplicate of the C++.
        derived = (
            round(385.700006 + values["kToggleMarkerBoxDX"]),
            round(1711.000000 + values["kToggleMarkerBoxDY"]),
            round(124.700000 + values["kToggleMarkerBoxDW"]),
            round(139.200000 + values["kToggleMarkerBoxDH"]),
        )
        self.assertEqual(derived, (388, 1711, 120, 126))
        # ...and none of those four numbers is written down in the plugin's
        # own slot or draw functions, which is the point of deriving them.
        for literal in ("388", "1711", "120", "126"):
            self.assertIsNone(re.search(rf"\b{literal}\b", self.draw_path), literal)

    def test_slot_lookup_takes_the_row_id_and_returns_the_derived_box(self):
        body = function_body(self.plugin, "static bool ToggleIndicatorFindSlot(")
        self.assertIn("static bool ToggleIndicatorFindSlot(int talentId,", self.plugin)
        self.assertIn("(int)tid.ToDouble() != talentId", body)
        self.assertIn("ToggleIndicatorMarkerBox(bx, by, bw, bh, outX, outY, outW, outH);", body)

    # ---- S6: the ids are resolved by name, at the frame boundary -----------

    def test_resolver_walks_the_talent_map_by_ability_id(self):
        body = function_body(self.plugin, "static bool ToggleTableResolveIds(")
        for needle in ("N1GetTalentMap(", '"ds_map_find_first"', '"ds_map_find_next"', '"abilityId"',
                       "ForgePact::kToggleSkillRows[r].abilityId", "g_ToggleTableIds.Set(r, id)"):
            self.assertIn(needle, body, needle)
        self.assertIn("if (id >= 0 && N1GetTalentStruct(", body)   # a negative key is never stored
        self.assertIn("g_ToggleResolveWalks", body)

    def test_resolver_runs_only_from_frame_callback_and_is_bounded(self):
        frame = function_body(self.plugin, "void FrameCallback(")
        self.assertIn("ToggleTableResolveIds()", frame)
        gate = frame[frame.index("if (g_Setup && (fc % 60) == 0 && ToggleTableResolveDue())"):]
        self.assertIn("ToggleTableResolveIds();", gate[:200])
        self.assertIn("ToggleTableResolveIds()", self.stripped)
        # Never in a draw or in the hook: a map walk there would be the
        # frame-boundary/point-of-use mistake in reverse.
        for sig in ("static void ToggleIndicatorDraw(", "static bool ToggleIndicatorFindSlot(",
                    "static RValue& HookTalentUseClass("):
            self.assertNotIn("ToggleTableResolveIds", function_body(self.plugin, sig), sig)
        due = function_body(self.plugin, "static bool ToggleTableResolveDue(")
        self.assertIn("if (ToggleTableUnresolvedRows() == 0) return false;", due)
        self.assertIn("CurrentRoomKey()", due)
        self.assertIn("INT64_MIN", due)          # an unreadable room is never stored
        self.assertIn("g_ToggleResolveWalked = false;", due)
        self.assertIn("return !g_ToggleResolveWalked;", due)

    def test_both_stat_outputs_report_every_row(self):
        line = function_body(self.stripped, "static std::string ToggleTableRowsLine(")
        for needle in ("abilityId", ":talentId=", "unresolved", "resolveWalks=", "unresolvedRows="):
            self.assertIn(needle, line, needle)
        for sig in ("static void ToggleBorderStats(", "static void ToggleGuardStats("):
            self.assertIn("ToggleTableRowsLine()", function_body(self.stripped, sig), sig)

    def test_unresolved_rows_are_skipped_and_counted_in_the_draw(self):
        body = function_body(self.plugin, "static void ToggleIndicatorDraw(")
        skip = body[body.index("if (talentId < 0) {"):body.index("ToggleIndicatorReadRow(")]
        self.assertIn("InterlockedIncrement(&g_TibUnresolved);", skip)
        self.assertIn("continue;", skip)
        self.assertNotIn("CallBuiltin", skip)
        self.assertIn("unresolved=", function_body(self.stripped, "static std::string ToggleBorderCountersLine("))

    def test_enable_messages_name_the_covered_count_not_one_skill(self):
        # Re-review follow-up: both confirmations predate the table and named
        # Soul Spurn, so a player on Exo or Prophet read the whole feature as
        # a White Mage one. Each now says how many toggle skills are covered,
        # taken from the table rather than restated, and points at the `stat`
        # output for which.
        border = self.stripped[self.stripped.index('Out("toggleborder -> ON'):]
        border = border[:border.index(");")]
        guard = self.stripped[self.stripped.index('Out(std::string("toggleguard -> ")'):]
        guard = guard[:guard.index(");")]
        for name, text in (("toggleborder", border), ("toggleguard", guard)):
            self.assertIn("ForgePact::kToggleSkillRowCount", text, name)
            self.assertIn("stat", text, name)
            for skill in ("Soul Spurn", "Purgatory"):
                self.assertNotIn(skill, text, name + "/" + skill)

    def test_border_stat_reports_per_row_counters(self):
        # Phase S review follow-up: every counter in ToggleBorderCountersLine
        # is a sum over the five rows, so on its own it cannot say which row
        # was ON, and a player with nothing toggled reads `off=` at five times
        # the draw count. Each row gets its own line, named by its `abilityId`.
        row_line = function_body(self.stripped, "static std::string ToggleBorderRowCountersLine(")
        self.assertIn("ForgePact::kToggleSkillRows[row].abilityId", row_line)
        for key in ("drawn=", "on=", "off=", "unreadable=", "unresolved=", "noSlot="):
            self.assertIn(key, row_line, key)
        stats = function_body(self.stripped, "static void ToggleBorderStats(")
        self.assertIn("ToggleBorderRowCountersLine(r)", stats)
        self.assertIn("r < ForgePact::kToggleSkillRowCount", stats)
        # The summed line says so, rather than letting a reader take it for a
        # per-draw total - in the shared counters line, so `toggleborder 0`
        # carries it too.
        self.assertIn("summed over", function_body(
            self.stripped, "static std::string ToggleBorderCountersLine("))
        # Every outcome the draw decides is charged to the row that produced
        # it, including the slot lookup that failed.
        draw = function_body(self.stripped, "static void ToggleIndicatorDraw(")
        for counter in ("g_TibRow[r].unresolved", "g_TibRow[r].unreadable", "g_TibRow[r].off",
                        "g_TibRow[r].on", "g_TibRow[r].noSlot", "g_TibRow[r].drawn"):
            self.assertIn("InterlockedIncrement(&" + counter + ")", draw, counter)


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
        # NARROWED in phase S: the guarded talent is no longer a literal
        # constant but the shipped table's membership test, which is what now
        # follows the caller identification.
        talent = body.index("ToggleTableRowForTalentId(")
        self.assertLess(endif, fast)
        self.assertLess(fast, object_index)
        self.assertLess(object_index, obj)
        self.assertLess(obj, talent)
        # D-P3: the sub-talent is read only after membership is settled.
        self.assertLess(talent, body.index("ToggleReadSubTalent("))

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
        # NARROWED in phase S: the cut point was the kGuard construction,
        # which now follows the talent read (a legitimate `.ToDouble()`). The
        # region this test is about is the caller identification, which ends
        # where procSeen is counted.
        caller_region = body[:body.index("if (callerIsDoubleCast) InterlockedIncrement(&g_TgdProcSeen);")]
        self.assertIsNone(re.search(r"\.ToDouble\(\)", caller_region))

    def test_sub_talent_is_read_at_the_call_after_the_membership_test(self):
        # D-P3, and AGENTS.md's "Check a Permission Where It Is Used": the
        # permission is read HERE, with the talent the call named, after the
        # caller check and after membership - never from anything cached at a
        # frame boundary, which would answer for the previous frame.
        body = self.hook
        caller = body.index("GameObject::Universal_Double_Cast_obj")
        member = body.index("ToggleTableRowForTalentId(")
        sub = body.index("ToggleReadSubTalent(")
        self.assertLess(caller, member)
        self.assertLess(member, sub)
        self.assertNotIn('"subTalentMap"', body[:member])
        read = function_body(self.plugin, "static ToggleSubTalentState ToggleReadSubTalent(")
        for needle in ('"variable_global_exists"', '"variable_global_get"', '"subTalentMap"',
                       '"array_length"', '"array_get"', "ForgePact::kToggleSubTalentMapIndex",
                       '"t" + std::to_string(talentId)', '"s" + std::to_string(slot)'):
            self.assertIn(needle, read, needle)
        self.assertIn("map.m_Kind != VALUE_ARRAY", read)
        self.assertIn("return level.ToDouble() > 0.0 ? ToggleSubTalentState::Allocated", read)
        # The measured index lives in the header as a named constant whose
        # value is `## State`'s `subidx:`.
        self.assertIn("inline constexpr int kToggleSubTalentMapIndex = 1;", self.header)

    def test_refusal_is_gated_on_the_sub_talent_and_fails_open(self):
        body = self.hook
        refuse = body.index("if (refuseByCaller) {")
        tail = body[refuse:]
        self.assertIn("if (sub == ToggleSubTalentState::Allocated) {", tail)
        self.assertLess(tail.index("ToggleSubTalentState::Allocated"), tail.index("g_TgdRefused"))
        self.assertIn("if (sub == ToggleSubTalentState::NotAllocated) InterlockedIncrement(&g_TgdSubOff);", tail)
        self.assertIn("else InterlockedIncrement(&g_TgdSubUnreadable);", tail)
        # Nothing about the toggle's own state is consulted (D-N1 still).
        for forbidden in ("instance_number", "instance_find", "ToggleIndicatorRead"):
            self.assertNotIn(forbidden, function_body(self.plugin, "static ToggleSubTalentState ToggleReadSubTalent("))

    def test_guard_stat_reports_the_new_counters(self):
        line = function_body(self.stripped, "static std::string ToggleGuardCountersLine(")
        for key in ("subOff=", "subUnreadable=", "subIndex="):
            self.assertIn(key, line, key)
        # `subIndex=` must be able to say "no index answered" rather than
        # printing a number that never answered anything.
        self.assertIn('std::string("none")', line)

    def test_sub_talent_index_is_selected_not_assumed(self):
        # Phase S review follow-up. Session 6 measured index 1 on one
        # character on one build; the read starts there and then looks for the
        # index whose `t<talentId>` struct is actually present, which is the
        # positive signal the research probe walks the same array with. A
        # fixed index that turned out to be a character slot would leave the
        # guard inert with only a `subUnreadable=` counter as the symptom.
        read = function_body(self.plugin, "static ToggleSubTalentState ToggleReadSubTalent(")
        self.assertIn("ForgePact::kToggleSubTalentMapIndex", read)
        # The measured index is attempt 0; every other index follows it.
        self.assertIn("attempt == 0 ? ForgePact::kToggleSubTalentMapIndex : attempt - 1", read)
        self.assertIn("ForgePact::kToggleSubTalentScanCap", read)
        self.assertIn("inline constexpr int kToggleSubTalentScanCap = 16;", self.header)
        # The struct's presence is what selects the index, and the index that
        # answered is recorded for the stat line - before the level read, so a
        # present-but-unreadable struct still names where it was found.
        select = read[read.index("attempt == 0 ?"):]
        self.assertLess(select.index('"t" + std::to_string(talentId)'),
                        select.index("g_TgdSubIndex.store(index)"))
        self.assertLess(select.index("g_TgdSubIndex.store(index)"),
                        select.index('"s" + std::to_string(slot)'))
        self.assertIn("static std::atomic<int> g_TgdSubIndex{ -1 };", self.plugin)

    def test_one_bad_sub_talent_entry_costs_one_index_not_the_scan(self):
        # Re-review follow-up: with only the function-level catch, a junk
        # entry in front of the index that carries `t<talentId>` ended the
        # walk and the guard went inert again - `subIndex=none`, every proc
        # passed, `subUnreadable=` climbing. Each attempt is judged on its own.
        read = function_body(self.plugin, "static ToggleSubTalentState ToggleReadSubTalent(")
        attempt = read[read.index("for (int attempt"):]
        # The entry's kind is checked before it is read, accepting both kinds
        # this runner hands a struct back as (the recurring kind-gate bug).
        self.assertIn("entry.m_Kind != VALUE_OBJECT && entry.m_Kind != VALUE_REF", attempt)
        self.assertLess(attempt.index("entry.m_Kind != VALUE_OBJECT"),
                        attempt.index('"t" + std::to_string(talentId)'))
        # ...and the attempt's own catch continues the walk instead of ending
        # it, which the function-level catch (still there, for the reads
        # before the loop) cannot do.
        self.assertIn("catch (...) { continue; }", attempt)
        self.assertIn("catch (...) { return ToggleSubTalentState::Unreadable; }", read)

    def test_refusal_returns_the_result_without_the_original(self):
        # NARROWED in phase S: the Refuse decision is now taken in one place
        # (the membership test) and acted on in another (after D-P3's
        # sub-talent gate), so the branch that actually refuses is the one to
        # pin - it must still return the untouched result, never the original.
        body = self.hook
        refuse = body.index("if (sub == ToggleSubTalentState::Allocated) {")
        branch = body[refuse:body.index("}", refuse)]
        self.assertIn("return R;", branch)
        self.assertNotIn("g_OrigTalentUseClass", branch)

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
# The production bodies the research build had to leave exactly as T1 shipped
# them (62a67d2): nothing outside a research block changed in phase R.
#
# NARROWED in phase S (issue #11, the five-row table), not deleted: phase S
# ships the table itself, so five of the six entries are now code it changes
# on purpose, and pinning them to T1 would only assert that the phase did not
# happen. Each dropped entry is covered by its own contract test instead:
#   - ToggleIndicatorRead        -> row 0's alias, pinned to delegate to
#                                   ToggleIndicatorReadRow (the read shape is
#                                   pinned in ToggleIndicatorReadContractTests)
#   - ToggleIndicatorFindSlot    -> takes the row's resolved talent id and
#                                   returns D-U12's derived box (S4's test)
#   - ToggleIndicatorDraw        -> loops the shipped rows and draws D-U13's
#                                   marker (S3's and S5's tests)
#   - HookTalentUseClass         -> membership on the table plus D-P3's
#                                   sub-talent gate (ToggleGuardContractTests)
#   - FrameCallback              -> gains the once-a-second id resolver (S6)
# Hook_DrawHudBuffs is the one body phase S genuinely leaves alone, so it is
# the one this tuple still holds.
UNCHANGED_SINCE_T1 = (
    "static RValue& Hook_DrawHudBuffs(",
)

# The research block phase S must not touch at all: the sprite look probe the
# author judged the marker against, and the whole `tgprobe tgl` instrument.
# The shipped marker is pinned EQUAL to the probe's own constants rather than
# calling into it, so these staying byte-identical is what makes that pin mean
# something.
UNCHANGED_PROBE_BODIES = (
    "static void TgProbeSpriteDrawSoft(",
    "static void TgProbeSpriteTunedBox(",
    "static bool TgProbeSpriteBaseBox(",
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

    # ---- Sprite look probe (R round 3, issue #11): `tgprobe sprite ...` ----
    # A research-only probe that draws a *named* sprite, or today's shipped
    # gold rectangles, over a talent's hotbar slot so the tester can judge a
    # candidate look by eye. Never a shipped draw input (D-U9 extends: no
    # shipped draw change comes out of this probe either).

    def test_sprite_command_dispatches_off_gold_list_and_a_named_sprite(self):
        command = function_body(self.plugin, "static void TgProbeCommand(const std::string& rest)")
        self.assertIn('sub == "sprite"', command)
        self.assertIn("TgProbeSpriteCommand(subRest)", command)
        sprite = function_body(self.plugin, "static void TgProbeSpriteCommand(const std::string& rest)")
        for needle in ('lower == "off"', 'lower == "gold"', 'lower == "list"'):
            self.assertIn(needle, sprite)
        # A bare talentId argument after the sprite name, defaulting to Soul
        # Spurn's talent id when omitted.
        self.assertIn("FirstToken(subRest, t1)", sprite)
        self.assertIn("kToggleIndicatorTalentId", sprite)

    def test_sprite_resolves_by_name_and_stores_nothing_when_unresolved(self):
        resolve = function_body(self.plugin, "static bool TgProbeSpriteResolve(")
        self.assertIn('"asset_get_index"', resolve)
        self.assertIn("return outIdx >= 0;", resolve)
        sprite = function_body(self.plugin, "static void TgProbeSpriteCommand(const std::string& rest)")
        self.assertIn("TgProbeSpriteResolve(first, idx)", sprite)
        self.assertIn("unresolved", sprite)
        # The only place a resolved sprite is actually stored/armed is
        # `g_TgSpriteIdx = idx;`, further down than the unresolved return.
        unresolved = sprite.index("unresolved")
        stored = sprite.index("g_TgSpriteIdx = idx;")
        self.assertLess(unresolved, stored)
        self.assertIn("return;", sprite[unresolved:stored])

    def test_sprite_list_names_the_round_brief_candidates_with_resolved_index(self):
        listing = function_body(self.plugin, "static void TgProbeSpriteList()")
        self.assertIn("TgProbeSpriteResolve(", listing)
        self.assertIn("unresolved", listing)
        self.assertIn("kTgSpriteCandidates", listing)
        for name in (
            "Talent_Aura_Frame_spr", "Talent_Frame_Indicator_spr", "Ability_Indicator_Border_spr",
            "Ability_Indicator_spr", "Ability_Indicator_White_spr", "Sub_Talent_Big_Border_spr",
            "Skill_Frames_spr",
        ):
            self.assertIn(f'"{name}"', self.plugin)

    def test_sprite_draws_from_the_probe_path_only_and_animates(self):
        draw = function_body(self.plugin, "static void TgProbeSpriteDraw(bool fromHudLayer)")
        self.assertIn("TgProbeSpriteDrawOne(", draw)
        self.assertIn('"draw_get_colour"', draw)
        self.assertIn('"draw_get_alpha"', draw)
        self.assertIn("InterlockedIncrement(&g_TgSpriteDraws)", draw)
        self.assertIn("InterlockedIncrement(&g_TgSpriteDrawExc)", draw)
        one = function_body(self.plugin, "static void TgProbeSpriteDrawOne(")
        self.assertIn('"draw_sprite_ext"', one)
        self.assertIn('"sprite_get_number"', one)
        self.assertIn("g_TgSpriteAnimTime", one)
        # A shared, read-only time base - never mutated per sprite/cell, so a
        # gallery's several sprites animate in lockstep rather than the
        # counter compounding once per cell per draw.
        self.assertNotIn("g_TgSpriteAnimTime +=", one)
        self.assertIn("g_TgSpriteAnimTime += 1.0 / 15.0;", draw)
        # One call site: TgProbeSpriteDrawOne's whole-sprite draw.
        # TgProbeSpriteDrawQuad draws via draw_sprite_part_ext instead
        # (round 10's corrected per-quadrant crop, not a whole-sprite copy).
        self.assertEqual(self.plugin.count('"draw_sprite_ext"'), 1)
        self.assertEqual(self.plugin.count('"draw_sprite_part_ext"'), 1)
        # Hung off the existing research after-draw path (buffs) and the new
        # hud-layer hook - never Hook_DrawHudBuffs or FrameCallback directly,
        # both of which stay byte-identical (UNCHANGED_SINCE_T1).
        self.assertIn("TgProbeSpriteDraw(/*fromHudLayer=*/false);",
                       function_body(self.plugin, "static void TgProbeSpurnAfterDraw()"))
        self.assertIn("TgProbeSpriteDraw(/*fromHudLayer=*/true);",
                       function_body(self.plugin, "static RValue& TgProbeDetourBody("))
        self.assertNotIn("TgProbeSpriteDraw", function_body(self.plugin, "static RValue& Hook_DrawHudBuffs("))
        self.assertNotIn("TgProbeSpriteDraw", function_body(self.plugin, "void FrameCallback("))

    def test_sprite_off_reports_draws_and_exceptions(self):
        sprite = function_body(self.plugin, "static void TgProbeSpriteCommand(const std::string& rest)")
        off = sprite[sprite.index('lower == "off"'):sprite.index('lower == "list"')]
        self.assertIn("g_TgSpriteMode = TgSpriteMode::Off;", off)
        self.assertIn("draws=", off)
        self.assertIn("drawExc=", off)

    def test_sprite_research_only_names_do_not_survive_stripping(self):
        for name in ("TgProbeSpriteResolve", "TgProbeSpriteFindSlot", "TgProbeSpriteDraw",
                     "TgProbeSpriteDrawOne", "TgProbeSpriteDrawGoldRect", "TgProbeSpriteDrawGallery",
                     "TgProbeSpriteGalleryLegend", "TgProbeSpriteGuiSize",
                     "TgProbeSpriteHudRowAttached", "TgProbeSpriteList",
                     "TgProbeSpriteCommand", "TgSpriteMode", "g_TgSpriteMode", "kTgSpriteCandidates"):
            self.assertIn(name, self.block)
            self.assertNotIn(name, self.stripped)

    # ---- Sprite look probe round 4 (2026-09-20): gallery, centre, layer ----
    # The live session found two named candidates drew (draws=/drawExc=0)
    # but were not visible - occluded by the hotbar button's own art, painted
    # after the `buffs`-layer draw. `gallery`/`centre` move the judgement off
    # the hotbar entirely; `layer hud` moves the draw itself to after the
    # outermost `DrawHud` call, on top of the button art.

    def test_sprite_dispatches_gallery_centre_and_layer(self):
        sprite = function_body(self.plugin, "static void TgProbeSpriteCommand(const std::string& rest)")
        for needle in ('lower == "gallery"', 'lower == "layer"'):
            self.assertIn(needle, sprite)
        self.assertIn('arg2Lower == "centre"', sprite)
        self.assertIn('arg2Lower == "center"', sprite)
        self.assertIn("TgSpriteMode::Centre", sprite)
        self.assertIn("TgSpriteMode::Gallery", sprite)
        self.assertIn('v == "hud"', sprite)
        self.assertIn('v == "buffs"', sprite)

    def test_gallery_draws_every_candidate_plus_gold_and_prints_a_legend(self):
        gallery = function_body(self.plugin, "static void TgProbeSpriteDrawGallery()")
        self.assertIn("for (const char* name : kTgSpriteCandidates)", gallery)
        self.assertIn("TgProbeSpriteDrawOne(", gallery)
        self.assertIn("TgProbeSpriteDrawGoldRect(", gallery)
        # Round 6: no per-cell label draw any more - it displaced the icon in
        # the tester's session (cause not diagnosed). No draw_text anywhere
        # in the gallery's own draw path; the legend is log-only.
        self.assertNotIn('"draw_text"', gallery)
        self.assertNotIn("TgProbeSpriteDrawGalleryLabel", self.plugin)
        legend = function_body(self.plugin, "static void TgProbeSpriteGalleryLegend()")
        self.assertIn("for (const char* name : kTgSpriteCandidates)", legend)
        self.assertIn("gold (positive control)", legend)
        self.assertNotIn('"draw_', legend)   # log-only: Out(), never a draw call
        sprite_gallery = function_body(self.plugin, "static void TgProbeSpriteCommand(const std::string& rest)")
        gallery_branch = sprite_gallery[sprite_gallery.index('lower == "gallery"'):sprite_gallery.index('lower == "gallery"') + 700]
        self.assertIn("TgProbeSpriteGalleryLegend()", gallery_branch)

    def test_hud_layer_is_a_separate_draw_site_reusing_the_one_resolver(self):
        # No new hook for the hud layer: it reuses the DrawHud candidate row
        # every other row already funnels through (TgProbeAttach/
        # MmCreateHook, called from exactly one place - test_probe_installs_
        # no_table_hooks and test_one_resolver_decides_every_detour pin that
        # the block calls HookOneScript/HookOneScriptTable/HookRawNamedRoutine
        # nowhere and MmCreateHook exactly once).
        for call in ("HookOneScript(", "HookOneScriptTable(", "HookRawNamedRoutine("):
            self.assertNotIn(call, self.block)
        body = function_body(self.plugin, "static RValue& TgProbeDetourBody(")
        drawhud_branch = body[body.index("idx == kTg_DrawHud"):]
        self.assertIn("TgProbeDrawMark(/*fromHudLayer=*/true);", drawhud_branch)
        self.assertIn("TgProbeSpriteDraw(/*fromHudLayer=*/true);", drawhud_branch)
        # After the row's own trampoline call, not before - draws after
        # DrawHud's whole body (including the nested DrawHudBuffs call).
        self.assertLess(body.index("t.tramp ? t.tramp("), body.index("idx == kTg_DrawHud"))
        # `layer hud` only flips the flag these draws check; it does not
        # attach anything itself - the tester runs `tgprobe hook` for that,
        # same as any other candidate row.
        layer_branch = function_body(self.plugin, "static void TgProbeSpriteCommand(const std::string& rest)")
        layer_branch = layer_branch[layer_branch.index('lower == "layer"'):]
        self.assertIn("TgProbeSpriteHudRowAttached()", layer_branch)
        attached = function_body(self.plugin, "static bool TgProbeSpriteHudRowAttached()")
        self.assertIn("kTg_DrawHud", attached)
        self.assertIn("kTgNative", attached)

    # ---- Sprite look probe round 5 (2026-09-20): scale ("surround") ----
    # The live session found the button's own art paints over anything drawn
    # inside the icon's bounds, at both layers; only the gold outline's own
    # navBbox (bigger than the icon) reads. `scale` inflates a candidate the
    # same way, centred on the slot.

    def test_scale_dispatches_defaults_to_one_and_clamps_by_hand(self):
        sprite = function_body(self.plugin, "static void TgProbeSpriteCommand(const std::string& rest)")
        self.assertIn('lower == "scale"', sprite)
        self.assertIn("static double g_TgSpriteScale = 1.0;", self.plugin)
        scale_branch = sprite[sprite.index('lower == "scale"'):sprite.index('lower == "gold"')]
        # Bare argument reports the current value without changing it.
        self.assertIn("if (!v.empty())", scale_branch)
        # Clamped by hand (if-based idiom), never std::max/std::min - the
        # C2589 build break this same file guards against elsewhere
        # (test_no_bare_std_max_or_std_min, tests/test_release_hook_contract.py).
        self.assertIn("if (scale < 0.25) scale = 0.25;", scale_branch)
        self.assertIn("if (scale > 4.0) scale = 4.0;", scale_branch)
        self.assertNotRegex(scale_branch, r"\bstd::max\(")
        self.assertNotRegex(scale_branch, r"\bstd::min\(")

    def test_scale_reaches_the_draw_call_via_the_shared_scaled_box(self):
        box = function_body(self.plugin, "static bool TgProbeSpriteScaledSlotBox(")
        # Round 10: the box read now goes through TgProbeSpriteBaseBox (which
        # itself reads TgProbeSpriteFindSlot and applies the tuned/bbox
        # offset), not TgProbeSpriteFindSlot directly.
        self.assertIn("TgProbeSpriteBaseBox(talentId, x, y, w, h)", box)
        self.assertIn("w * g_TgSpriteScale", box)
        self.assertIn("h * g_TgSpriteScale", box)
        # Centred on the slot's own centre, not its top-left corner.
        self.assertIn("cx - sw / 2.0", box)
        self.assertIn("cy - sh / 2.0", box)
        draw = function_body(self.plugin, "static void TgProbeSpriteDraw(bool fromHudLayer)")
        self.assertIn("TgProbeSpriteScaledSlotBox(g_TgSpriteTalentId, x, y, w, h)", draw)
        self.assertNotIn("TgProbeSpriteFindSlot(g_TgSpriteTalentId", draw)

    def test_scale_applies_to_named_and_gold_not_centre_gallery_or_mark(self):
        draw = function_body(self.plugin, "static void TgProbeSpriteDraw(bool fromHudLayer)")
        # The Gold/Named branch is the only caller of the scaled box; Centre
        # and Gallery keep their own fixed-size boxes untouched by scale.
        else_branch = draw[draw.index("} else {"):]
        self.assertIn("TgProbeSpriteScaledSlotBox(", else_branch)
        centre_branch = draw[draw.index("TgSpriteMode::Centre"):draw.index("} else {")]
        self.assertNotIn("g_TgSpriteScale", centre_branch)
        gallery = function_body(self.plugin, "static void TgProbeSpriteDrawGallery()")
        self.assertNotIn("g_TgSpriteScale", gallery)
        # `tgprobe mark` already takes explicit geometry - untouched by scale.
        mark = function_body(self.plugin, "static void TgProbeDrawMark(bool fromHudLayer)")
        self.assertNotIn("g_TgSpriteScale", mark)
        # gold's own command handler reports the scaled box in its
        # confirmation line, same as a named sprite's.
        sprite = function_body(self.plugin, "static void TgProbeSpriteCommand(const std::string& rest)")
        gold_branch = sprite[sprite.index('lower == "gold"'):sprite.index('lower == "gallery"')]
        self.assertIn("TgProbeSpriteScaledSlotBox(g_TgSpriteTalentId, bx, by, bw, bh)", gold_branch)
        self.assertIn("scale=", gold_branch)
        # box= itself is now formatted by the shared TgProbeSpriteBoxText
        # helper (round 10), not inlined here.
        self.assertIn("TgProbeSpriteBoxText(boxFound, bw, bh, bx, by)", gold_branch)

    def test_scale_names_do_not_survive_stripping(self):
        for name in ("TgProbeSpriteScaledSlotBox", "g_TgSpriteScale"):
            self.assertIn(name, self.block)
            self.assertNotIn(name, self.stripped)

    # ---- Sprite look probe round 6 (2026-09-20): procedural styles ----
    # Drawn by us, not a game sprite - a soft alternative to the flat gold
    # rectangle, requested after the tester found it crude. Every style
    # draws into the same scaled slot box a named sprite/gold uses.

    def test_style_dispatches_all_four_names_and_rejects_others(self):
        sprite = function_body(self.plugin, "static void TgProbeSpriteCommand(const std::string& rest)")
        self.assertIn('lower == "style"', sprite)
        self.assertIn("TgProbeSpriteStyleFromName(v, kind)", sprite)
        from_name = function_body(self.plugin, "static bool TgProbeSpriteStyleFromName(")
        for name in ("soft", "halo", "gradient", "pulse"):
            self.assertIn(f'lower == "{name}"', from_name)
        style_name = function_body(self.plugin, "static const char* TgProbeSpriteStyleName(")
        for name in ("soft", "halo", "gradient", "pulse"):
            self.assertIn(f'return "{name}";', style_name)
        # `style list` names are also surfaced from `sprite list`.
        listing = function_body(self.plugin, "static void TgProbeSpriteList()")
        for name in ("soft", "halo", "gradient", "pulse"):
            self.assertIn(name, listing)

    def test_style_draws_into_the_scaled_box_and_reports_scale(self):
        style_branch_source = function_body(self.plugin, "static void TgProbeSpriteCommand(const std::string& rest)")
        style_branch = style_branch_source[style_branch_source.index('lower == "style"'):
                                            style_branch_source.index('lower == "gallery"')]
        self.assertIn("TgProbeSpriteScaledSlotBox(g_TgSpriteTalentId, bx, by, bw, bh)", style_branch)
        self.assertIn("scale=", style_branch)
        # box= itself is now formatted by the shared TgProbeSpriteBoxText
        # helper (round 10), not inlined here.
        self.assertIn("TgProbeSpriteBoxText(boxFound, bw, bh, bx, by)", style_branch)
        self.assertIn("TgSpriteMode::Style", style_branch)
        draw = function_body(self.plugin, "static void TgProbeSpriteDraw(bool fromHudLayer)")
        else_branch = draw[draw.index("} else {"):]
        self.assertIn("TgProbeSpriteDrawStyle(g_TgSpriteStyleKind, x, y, w, h)", else_branch)
        # Style shares the Named/Gold branch's scaled box, not its own read.
        self.assertEqual(else_branch.count("TgProbeSpriteScaledSlotBox("), 1)

    def test_style_draw_functions_save_and_restore_and_count_exceptions(self):
        # The four style draws themselves don't save/restore colour/alpha -
        # TgProbeSpriteDraw does, once, around whichever branch actually
        # drew (Named/Gold/Style/Gallery/Centre alike), and its own catch is
        # what counts drawExc - same shape as every other mode already uses.
        draw = function_body(self.plugin, "static void TgProbeSpriteDraw(bool fromHudLayer)")
        self.assertIn('"draw_get_colour"', draw)
        self.assertIn('"draw_get_alpha"', draw)
        self.assertIn("catch (...) { InterlockedIncrement(&g_TgSpriteDrawExc); }", draw)
        soft = function_body(self.plugin, "static void TgProbeSpriteDrawSoft(")
        self.assertIn('"draw_rectangle"', soft)
        self.assertIn("TgProbeSpriteFadeAlpha(1.0, t)", soft)   # alpha ramps down outward (round 9: via the shared fade helper)
        halo = function_body(self.plugin, "static void TgProbeSpriteDrawHalo(")
        self.assertIn('"draw_ellipse_colour"', halo)
        gradient = function_body(self.plugin, "static void TgProbeSpriteDrawGradient(")
        self.assertIn('"draw_rectangle_colour"', gradient)
        pulse = function_body(self.plugin, "static void TgProbeSpriteDrawPulse(")
        self.assertIn("TgProbeSpriteDrawSoft(x, y, w, h, TgProbeSpritePulseFactor())", pulse)
        pulse_factor = function_body(self.plugin, "static double TgProbeSpritePulseFactor()")
        self.assertIn("std::sin(", pulse_factor)
        self.assertIn("kTgPulsePeriodFrames", pulse_factor)
        # The period is named in the pulse confirmation line, not left silent.
        style_branch_source = function_body(self.plugin, "static void TgProbeSpriteCommand(const std::string& rest)")
        style_branch = style_branch_source[style_branch_source.index('lower == "style"'):
                                            style_branch_source.index('lower == "gallery"')]
        self.assertIn("period=", style_branch)
        self.assertIn("TgSpriteStyleKind::Pulse", style_branch)

    def test_style_names_do_not_survive_stripping(self):
        for name in ("TgSpriteStyleKind", "g_TgSpriteStyleKind", "TgProbeSpriteStyleName",
                     "TgProbeSpriteStyleFromName", "TgProbeSpritePulseFactor", "TgProbeSpriteDrawSoft",
                     "TgProbeSpriteDrawHalo", "TgProbeSpriteDrawGradient", "TgProbeSpriteDrawPulse",
                     "TgProbeSpriteDrawStyle", "kTgPulsePeriodFrames"):
            self.assertIn(name, self.block)
            self.assertNotIn(name, self.stripped)

    # ---- Sprite look probe round 8 (2026-09-20): colour control ----
    # D-U11: the shipped marker will be red ("that's how aura is indicated
    # as working" in the game's own HUD), superseding D-U1's gold. The
    # shared setting still defaults to gold so nothing existing changes
    # silently until a tester asks for red.

    def test_colour_dispatches_name_and_triple_forms(self):
        sprite = function_body(self.plugin, "static void TgProbeSpriteCommand(const std::string& rest)")
        self.assertIn('lower == "colour" || lower == "color"', sprite)
        colour_branch = sprite[sprite.index('lower == "colour"'):sprite.index('lower == "layer"')]
        self.assertIn("TgProbeSpriteColourFromPreset(Lower(first2)", colour_branch)
        self.assertIn("std::stod(first2)", colour_branch)
        self.assertIn("std::stod(gStr)", colour_branch)
        self.assertIn("std::stod(bStr)", colour_branch)
        # Bare `colour` (no argument) reports without changing anything.
        self.assertIn("if (first2.empty())", colour_branch)

    def test_colour_presets_include_gold_and_a_family_of_reds(self):
        presets = self.plugin[self.plugin.index("static const TgColourPreset kTgColourPresets[] = {"):
                               self.plugin.index("};", self.plugin.index("static const TgColourPreset kTgColourPresets[] = {"))]
        self.assertIn('"gold"', presets)
        # A crimson family, not pure 255,0,0 (the author's own steer: "a
        # nicer shade, similar to what talent aura frame uses") - a default
        # red plus a brighter and a deeper neighbour either side of it.
        self.assertIn('"red"', presets)
        self.assertIn('"brightred"', presets)
        self.assertIn('"deepred"', presets)
        self.assertNotIn("255.0, 0.0, 0.0", presets)
        # `sprite list` iterates the preset table itself (not a copy-pasted
        # name list), so every preset it ever gains is listed automatically;
        # it prints each one's rgb triple too, not only its name, so a
        # tester's choice is recordable as a number.
        listing = function_body(self.plugin, "static void TgProbeSpriteList()")
        self.assertIn("for (const TgColourPreset& p : kTgColourPresets)", listing)
        self.assertIn("p.name", listing)
        self.assertIn("p.r", listing)
        self.assertIn("p.g", listing)
        self.assertIn("p.b", listing)

    def test_colour_default_is_gold(self):
        self.assertIn('static double g_TgSpriteColourR = 255.0, g_TgSpriteColourG = 215.0, g_TgSpriteColourB = 0.0;', self.plugin)
        self.assertIn('static std::string g_TgSpriteColourName = "gold";', self.plugin)

    def test_colour_clamps_and_rejects_unparseable_without_storing(self):
        sprite = function_body(self.plugin, "static void TgProbeSpriteCommand(const std::string& rest)")
        colour_branch = sprite[sprite.index('lower == "colour"'):sprite.index('lower == "layer"')]
        # Clamp by hand (0..255), not std::max/std::min - this file's own
        # C2589 lesson (test_no_bare_std_max_or_std_min).
        for lo, hi in (("if (r < 0.0) r = 0.0;", "if (r > 255.0) r = 255.0;"),
                       ("if (g < 0.0) g = 0.0;", "if (g > 255.0) g = 255.0;"),
                       ("if (b < 0.0) b = 0.0;", "if (b > 255.0) b = 255.0;")):
            self.assertIn(lo, colour_branch)
            self.assertIn(hi, colour_branch)
        self.assertNotRegex(colour_branch, r"\bstd::max\(")
        self.assertNotRegex(colour_branch, r"\bstd::min\(")
        # An unparseable triple takes the same "usage" path as an empty one
        # and never reaches the g_TgSpriteColourR/G/B assignment.
        self.assertIn("if (!parsed) {", colour_branch)
        not_parsed = colour_branch.index("if (!parsed) {")
        stored = colour_branch.index("g_TgSpriteColourR = r;")
        self.assertLess(not_parsed, stored)
        self.assertIn("return;", colour_branch[not_parsed:stored])

    def test_colour_reaches_every_style_gold_and_mark(self):
        for signature, needle in (
            ("static void TgProbeSpriteDrawGoldRect(", "TgProbeSpriteActiveColour()"),
            ("static void TgProbeSpriteDrawSoft(", "TgProbeSpriteActiveColour()"),
            ("static void TgProbeSpriteDrawHalo(", "TgProbeSpriteActiveColour()"),
            ("static void TgProbeSpriteDrawGradient(", "TgProbeSpriteActiveColour()"),
            ("static void TgProbeDrawMark(bool fromHudLayer)", "TgProbeSpriteActiveColour()"),
        ):
            body = function_body(self.plugin, signature)
            self.assertIn(needle, body, signature)
        # None of the five hardcodes a gold/red literal triple any more -
        # they all go through the one shared colour instead.
        for signature in ("static void TgProbeSpriteDrawGoldRect(", "static void TgProbeSpriteDrawSoft(",
                           "static void TgProbeSpriteDrawGradient(", "static void TgProbeDrawMark(bool fromHudLayer)"):
            body = function_body(self.plugin, signature)
            self.assertNotIn("RValue(215.0)", body, signature)
        active = function_body(self.plugin, "static RValue TgProbeSpriteActiveColour()")
        self.assertIn("g_TgSpriteColourR", active)
        self.assertIn("g_TgSpriteColourG", active)
        self.assertIn("g_TgSpriteColourB", active)
        self.assertIn('"make_colour_rgb"', active)
        # Printed beside scale=/layer= in gold's and style's confirmation
        # lines, and in mark's - the full rgb triple (TgProbeSpriteColourText),
        # not only the preset's name, so whatever the tester settles on is
        # quotable straight into the doc even if it is a raw r g b value.
        sprite = function_body(self.plugin, "static void TgProbeSpriteCommand(const std::string& rest)")
        gold_branch = sprite[sprite.index('lower == "gold"'):sprite.index('lower == "style"')]
        self.assertIn("colour=\" + TgProbeSpriteColourText()", gold_branch)
        style_branch = sprite[sprite.index('lower == "style"'):sprite.index('lower == "gallery"')]
        self.assertIn("colour=\" + TgProbeSpriteColourText()", style_branch)
        mark_cmd = function_body(self.plugin, "static void TgProbeMarkCommand(const std::string& rest)")
        self.assertIn("colour=\" + TgProbeSpriteColourText()", mark_cmd)
        text = function_body(self.plugin, "static std::string TgProbeSpriteColourText()")
        self.assertIn("g_TgSpriteColourName", text)
        self.assertIn("g_TgSpriteColourR", text)

    def test_colour_names_do_not_survive_stripping(self):
        for name in ("TgColourPreset", "kTgColourPresets", "TgProbeSpriteColourFromPreset",
                     "TgProbeSpriteActiveColour", "TgProbeSpriteColourText",
                     "g_TgSpriteColourR", "g_TgSpriteColourName"):
            self.assertIn(name, self.block)
            self.assertNotIn(name, self.stripped)

    # ---- Sprite look probe round 9 (2026-09-20): quad mirroring, alpha ----
    # `quad` is the author's own word "inside out" - four mirrored copies of
    # a named sprite, not a whole-sprite flip. `alpha` is the floor/ceiling
    # `soft`/`gradient` fade between, replacing 0 as the floor.

    def test_quad_dispatches_on_off_and_defaults_off(self):
        self.assertIn("static bool g_TgSpriteQuad = false;", self.plugin)
        sprite = function_body(self.plugin, "static void TgProbeSpriteCommand(const std::string& rest)")
        self.assertIn('lower == "quad"', sprite)
        quad_branch = sprite[sprite.index('lower == "quad"'):sprite.index('lower == "alpha"')]
        self.assertIn('v == "on" || v == "1"', quad_branch)
        self.assertIn('v == "off" || v == "0" || v.empty()', quad_branch)

    def test_quad_reaches_named_and_centre_not_gold_style_or_gallery(self):
        draw = function_body(self.plugin, "static void TgProbeSpriteDraw(bool fromHudLayer)")
        self.assertIn("if (g_TgSpriteQuad) TgProbeSpriteDrawQuad(g_TgSpriteIdx, bx, by, box, box);", draw)
        self.assertIn("if (g_TgSpriteQuad) TgProbeSpriteDrawQuad(g_TgSpriteIdx, x, y, w, h);", draw)
        gallery = function_body(self.plugin, "static void TgProbeSpriteDrawGallery()")
        self.assertNotIn("g_TgSpriteQuad", gallery)
        gold = function_body(self.plugin, "static void TgProbeSpriteDrawGoldRect(")
        self.assertNotIn("g_TgSpriteQuad", gold)

    def test_quad_draws_four_mirrored_source_quadrants_on_whole_pixels(self):
        # Round 10: round 9's "four whole-sprite copies" construction was
        # rejected on sight. The corrected version crops each of the
        # SOURCE sprite's own four quadrants (draw_sprite_part_ext) and
        # rotates each 180 degrees in place into the matching destination
        # quadrant, so the sprite's inner edges become its outer edges and
        # the assembled result stays a square.
        quad = function_body(self.plugin, "static void TgProbeSpriteDrawQuad(")
        self.assertEqual(quad.count('"draw_sprite_part_ext"'), 1)   # one call site, looped over 4 tiles
        self.assertNotIn('"draw_sprite_ext"', quad)
        self.assertIn(
            "struct TgQuadTile { double srcLeft, srcTop, srcW, srcH, dstX, dstY, dstW, dstH; };", quad)
        self.assertIn("for (const TgQuadTile& t : tiles)", quad)
        # Whole-pixel geometry (D-U11): halves rounded on both the source
        # sprite and destination box axes, not a bare divide.
        self.assertIn("const double srcHalfW = std::round(sw / 2.0);", quad)
        self.assertIn("const double srcHalfH = std::round(sh / 2.0);", quad)
        self.assertIn("const double dstHalfW = std::round(w / 2.0);", quad)
        self.assertIn("const double dstHalfH = std::round(h / 2.0);", quad)
        # The two tiles on each axis still sum to the sprite's/box's own
        # width/height exactly, even when a pixel rounding leaves a
        # remainder - the right/bottom quadrant absorbs it.
        self.assertIn("const double srcRightW = sw - srcHalfW;", quad)
        self.assertIn("const double srcBottomH = sh - srcHalfH;", quad)
        self.assertIn("const double dstRightW = w - dstHalfW;", quad)
        self.assertIn("const double dstBottomH = h - dstHalfH;", quad)
        # Uniform anchor-at-own-bottom-right, both scales negative, for all
        # four tiles alike - a 180-degree rotation of each source quadrant
        # in place, not round 9's four different per-tile sign combinations.
        self.assertIn("const double xscale = t.srcW > 0 ? -(t.dstW / t.srcW) : -1.0;", quad)
        self.assertIn("const double yscale = t.srcH > 0 ? -(t.dstH / t.srcH) : -1.0;", quad)
        self.assertIn(
            "RValue(t.dstX + t.dstW), RValue(t.dstY + t.dstH), RValue(xscale), RValue(yscale)", quad)

    def test_quad_printed_in_gold_named_and_off_confirmation_lines(self):
        sprite = function_body(self.plugin, "static void TgProbeSpriteCommand(const std::string& rest)")
        off_branch = sprite[sprite.index('lower == "off"'):sprite.index('lower == "list"')]
        self.assertIn("TgProbeSpriteQuadText()", off_branch)
        gold_branch = sprite[sprite.index('lower == "gold"'):sprite.index('lower == "style"')]
        self.assertIn("TgProbeSpriteQuadText()", gold_branch)
        self.assertIn("TgProbeSpriteQuadText()", sprite[sprite.rindex("boxText"):])

    def test_alpha_dispatches_min_and_optional_max(self):
        sprite = function_body(self.plugin, "static void TgProbeSpriteCommand(const std::string& rest)")
        self.assertIn('lower == "alpha"', sprite)
        alpha_branch = sprite[sprite.index('lower == "alpha"'):sprite.index('lower == "gold"')]
        self.assertIn("TgProbeSpriteParseAlphaArg(minStr, minFraction)", alpha_branch)
        self.assertIn("TgProbeSpriteParseAlphaArg(maxStr, maxFraction)", alpha_branch)
        # Bare `alpha` (no argument) reports without changing anything.
        self.assertIn("if (minStr.empty())", alpha_branch)
        # Omitting max resets the override back to "use the style's own
        # default", rather than carrying a stale previous max forward.
        self.assertIn("g_TgSpriteAlphaMaxOverride = maxStr.empty() ? -1.0 : maxFraction;", alpha_branch)

    def test_alpha_accepts_0_255_or_0_1_and_clamps_by_hand(self):
        parse = function_body(self.plugin, "static bool TgProbeSpriteParseAlphaArg(")
        self.assertIn("if (v > 1.0) v = v / 255.0;", parse)
        self.assertIn("if (v < 0.0) v = 0.0;", parse)
        self.assertIn("if (v > 1.0) v = 1.0;", parse)
        self.assertNotRegex(parse, r"\bstd::max\(")
        self.assertNotRegex(parse, r"\bstd::min\(")
        # An unparseable value is rejected and nothing is stored - the usage
        # line comes before any assignment to the shared alpha state.
        sprite = function_body(self.plugin, "static void TgProbeSpriteCommand(const std::string& rest)")
        alpha_branch = sprite[sprite.index('lower == "alpha"'):sprite.index('lower == "gold"')]
        usage = alpha_branch.index("tgprobe sprite alpha: usage")
        stored = alpha_branch.index("g_TgSpriteAlphaMin = minFraction;")
        self.assertLess(usage, stored)

    def test_alpha_reaches_soft_and_gradient_defaults_reproduce_todays_look(self):
        soft = function_body(self.plugin, "static void TgProbeSpriteDrawSoft(")
        gradient = function_body(self.plugin, "static void TgProbeSpriteDrawGradient(")
        self.assertIn("TgProbeSpriteFadeAlpha(1.0, t)", soft)
        self.assertIn("TgProbeSpriteFadeAlpha(0.5, t)", gradient)
        fade = function_body(self.plugin, "static double TgProbeSpriteFadeAlpha(")
        self.assertIn("g_TgSpriteAlphaMin", fade)
        self.assertIn("g_TgSpriteAlphaMaxOverride", fade)
        # Defaults (min=0.0, override=-1.0) reproduce each style's own
        # original ceiling exactly - the styleDefaultMax argument, unchanged.
        self.assertIn("static double g_TgSpriteAlphaMin = 0.0;", self.plugin)
        self.assertIn("static double g_TgSpriteAlphaMaxOverride = -1.0;", self.plugin)
        # halo/pulse are untouched by the new floor/ceiling (pulse reuses
        # soft's own fade, already covered above).
        halo = function_body(self.plugin, "static void TgProbeSpriteDrawHalo(")
        self.assertNotIn("TgProbeSpriteFadeAlpha", halo)

    def test_quad_and_alpha_names_do_not_survive_stripping(self):
        for name in ("g_TgSpriteQuad", "TgProbeSpriteDrawQuad", "TgQuadTile", "TgProbeSpriteQuadText",
                     "g_TgSpriteAlphaMin", "g_TgSpriteAlphaMaxOverride", "TgProbeSpriteFadeAlpha",
                     "TgProbeSpriteParseAlphaArg", "TgProbeSpriteAlphaText",
                     "TgProbeSpriteTunedBox", "TgProbeSpriteBaseBox", "TgSpriteBoxKind",
                     "g_TgSpriteBoxKind", "TgProbeSpriteBoxKindName", "TgProbeSpriteBoxText",
                     "kTgTunedBoxDX", "kTgTunedBoxDY", "kTgTunedBoxDW", "kTgTunedBoxDH"):
            self.assertIn(name, self.block)
            self.assertNotIn(name, self.stripped)

    def test_tuned_box_derives_du12_geometry_from_bbox_and_rounds(self):
        # D-U12's worked example (round 9, restated round 10): Soul Spurn's
        # navBbox at the moment the author accepted the marker geometry was
        # `385.700006, 1711.000000, 124.700000 x 139.200000`; the accepted
        # box was `388, 1711, 120 x 126` (whole pixels). Pin the constant
        # offset and the round-to-whole-pixels step that derive one from
        # the other, rather than a hardcoded box.
        self.assertIn("static constexpr double kTgTunedBoxDX = 2.3;", self.plugin)
        self.assertIn("static constexpr double kTgTunedBoxDY = 0.0;", self.plugin)
        self.assertIn("static constexpr double kTgTunedBoxDW = -4.7;", self.plugin)
        self.assertIn("static constexpr double kTgTunedBoxDH = -13.2;", self.plugin)
        tuned = function_body(self.plugin, "static void TgProbeSpriteTunedBox(")
        self.assertIn("outX = std::round(bboxX + kTgTunedBoxDX);", tuned)
        self.assertIn("outY = std::round(bboxY + kTgTunedBoxDY);", tuned)
        self.assertIn("outW = std::round(bboxW + kTgTunedBoxDW);", tuned)
        self.assertIn("outH = std::round(bboxH + kTgTunedBoxDH);", tuned)
        # The worked example itself, in Python, as a check on the constants
        # above rather than a duplicate of the C++: 385.700006 + 2.3 rounds
        # to 388, not the offset landing short/long of the accepted box.
        bbox_x, bbox_y, bbox_w, bbox_h = 385.700006, 1711.000000, 124.700000, 139.200000
        out_x = round(bbox_x + 2.3)
        out_y = round(bbox_y + 0.0)
        out_w = round(bbox_w - 4.7)
        out_h = round(bbox_h - 13.2)
        self.assertEqual((out_x, out_y, out_w, out_h), (388, 1711, 120, 126))

    def test_base_box_kind_defaults_tuned_and_bbox_skips_the_offset(self):
        self.assertIn("static TgSpriteBoxKind g_TgSpriteBoxKind = TgSpriteBoxKind::Tuned;", self.plugin)
        base = function_body(self.plugin, "static bool TgProbeSpriteBaseBox(")
        self.assertIn("if (g_TgSpriteBoxKind == TgSpriteBoxKind::Bbox) {", base)
        # Bbox kind rounds the raw navBbox directly, without the offset.
        self.assertIn("outX = std::round(bx); outY = std::round(by); outW = std::round(bw); outH = std::round(bh);",
                       base)
        # Tuned kind (the default) runs the derived-box offset instead.
        self.assertIn("TgProbeSpriteTunedBox(bx, by, bw, bh, outX, outY, outW, outH);", base)
        scaled = function_body(self.plugin, "static bool TgProbeSpriteScaledSlotBox(")
        self.assertIn("TgProbeSpriteBaseBox(talentId, x, y, w, h)", scaled)

    def test_box_dispatches_tuned_and_bbox_and_reports_active_kind(self):
        sprite = function_body(self.plugin, "static void TgProbeSpriteCommand(const std::string& rest)")
        self.assertIn('lower == "box"', sprite)
        box_branch = sprite[sprite.index('lower == "box"'):sprite.index('lower == "quad"')]
        self.assertIn('v == "tuned"', box_branch)
        self.assertIn("g_TgSpriteBoxKind = TgSpriteBoxKind::Tuned;", box_branch)
        self.assertIn('v == "bbox"', box_branch)
        self.assertIn("g_TgSpriteBoxKind = TgSpriteBoxKind::Bbox;", box_branch)
        self.assertIn("TgProbeSpriteBoxText(boxFound, bw, bh, bx, by)", box_branch)
        # Every draw path that goes through TgProbeSpriteScaledSlotBox
        # reports via the same TgProbeSpriteBoxText helper, so gold/style/
        # named all show the same kind and box as `box` itself.
        gold_branch = sprite[sprite.index('lower == "gold"'):sprite.index('lower == "style"')]
        self.assertIn("TgProbeSpriteBoxText(boxFound, bw, bh, bx, by)", gold_branch)
        style_branch = sprite[sprite.index('lower == "style"'):sprite.index('lower == "gallery"')]
        self.assertIn("TgProbeSpriteBoxText(boxFound, bw, bh, bx, by)", style_branch)
        self.assertIn("TgProbeSpriteBoxText(boxFound, bw, bh, bx, by)", sprite[sprite.rindex("boxText = "):])

    def test_shipped_draw_functions_unchanged_from_round_base(self):
        # Round base for R round 3 (context "R Round 3 - sprite look probe"):
        # hub eaaaf20, ForgePact 7169440. Stronger than the 62a67d2 pin above
        # - it also covers everything phase R has touched since T1.
        old = git_show("7169440:plugin/ModuleMain.cpp")
        if old is None:
            self.skipTest("git cannot read 7169440")
        for signature in UNCHANGED_SINCE_T1:
            self.assertEqual(function_body(self.plugin, signature), function_body(old, signature), signature)
        old_hpp = git_show("7169440:plugin/include/ForgePact/ToggleSkillMod.hpp")
        if old_hpp is None:
            self.skipTest("git cannot read 7169440")
        new_hpp = (FORGEPACT_DIR / "plugin" / "include" / "ForgePact" / "ToggleSkillMod.hpp").read_text(
            encoding="utf-8").replace("\r\n", "\n")
        # NARROWED in phase S: the whole-file equality is replaced by
        # per-declaration equality, because phase S adds the shipped table to
        # this header - and the table is the ONLY addition. Every decision the
        # header already made is still byte for byte what it was.
        for anchor in ("enum class ToggleIndicatorState", "struct ToggleIndicatorReadDetail {",
                       "class ToggleIndicatorModel {", "enum class ToggleGuardDecision",
                       "class ToggleGuardModel {", "class ToggleGuardMod {"):
            self.assertEqual(declaration_block(new_hpp, anchor), declaration_block(old_hpp, anchor), anchor)

    def test_probe_bodies_unchanged_from_the_s_round_base(self):
        # The shipped marker's band count and colour are pinned EQUAL to the
        # sprite look probe's (S3/S4), so the probe drifting would silently
        # move the shipped look. What this pins is every sprite-probe and
        # `tgl` BODY plus the `tgl` seeds table, byte for byte - not the
        # research block as a whole, which phase S did move a declaration
        # into (row 0's two aliases, whose only callers are in here).
        old = git_show("2f40223:plugin/ModuleMain.cpp")
        if old is None:
            self.skipTest("git cannot read 2f40223")
        signatures = list(UNCHANGED_PROBE_BODIES)
        signatures += sorted({
            m.group(0) for m in re.finditer(r"^static [\w:<>&* ]*?\bTgProbeTgl\w*\(", self.plugin, re.M)
        })
        self.assertGreater(len(signatures), len(UNCHANGED_PROBE_BODIES))
        for signature in signatures:
            self.assertEqual(function_body(self.plugin, signature), function_body(old, signature), signature)
        # The `tgl` seed table itself, which carries every candidate row.
        for source in (self.plugin, old):
            self.assertIn("static const TgTglSeed kTgTglSeeds[] = {", source)
        start_new = self.plugin.index("static const TgTglSeed kTgTglSeeds[] = {")
        start_old = old.index("static const TgTglSeed kTgTglSeeds[] = {")
        self.assertEqual(self.plugin[start_new:self.plugin.index("};", start_new)].replace("\r\n", "\n"),
                         old[start_old:old.index("};", start_old)].replace("\r\n", "\n"))


if __name__ == "__main__":
    unittest.main()
