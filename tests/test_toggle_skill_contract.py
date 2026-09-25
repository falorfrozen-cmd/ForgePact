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
# forgepact-notes-cleanup.yml deletes release-notes-v*.md from main once that
# version is published (the release page keeps the text). The player-text
# checks read the notes while they exist and leave only the notes out after.
RELEASE_NOTES = FORGEPACT_DIR / "release-notes-v1.4.5.md"


def read_release_notes():
    return RELEASE_NOTES.read_text(encoding="utf-8") if RELEASE_NOTES.is_file() else None
SDK_PY_PATH = REPO_ROOT / "hs-game-sdk" / "python"
TOOLS_DIR = FORGEPACT_DIR / "tools"

if str(TESTS_DIR) not in sys.path:
    sys.path.insert(0, str(TESTS_DIR))
if str(SRC_DIR) not in sys.path:
    sys.path.insert(0, str(SRC_DIR))
if str(SDK_PY_PATH) not in sys.path:
    sys.path.insert(0, str(SDK_PY_PATH))
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

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


def collapse(text: str) -> str:
    """Whitespace-normalised text, for asserting a pinned sentence that this
    hand-wrapped doc may split across lines."""
    return " ".join(text.split())


SEPARATOR_ROW = re.compile(r"^\|[\s:|-]+\|$")


def parse_doc_table(doc: str, caption: str):
    """Rows of the markdown table immediately under a `**caption**` line.

    Each row is a list of trimmed cell strings, the id in `row[0]`. Raises
    `ValueError` if the caption is not immediately followed by a header row
    and a `|---|...|` separator row - a parser that instead scanned ahead
    for the next line starting with `|` would keep "matching" a document
    whose table structure had silently broken (AGENTS.md "Prove the
    Instrument Before Trusting a Negative Result").
    """
    doc = doc.replace("\r\n", "\n")
    marker = f"**{caption}**"
    start = doc.index(marker) + len(marker)
    lines = doc[start:].splitlines()
    idx = 0
    while idx < len(lines) and lines[idx].strip() == "":
        idx += 1
    if idx >= len(lines) or not lines[idx].strip().startswith("|"):
        raise ValueError(f"no header row found after caption {caption!r}")
    idx += 1
    if idx >= len(lines) or not SEPARATOR_ROW.match(lines[idx].strip()):
        raise ValueError(f"no separator row found after caption {caption!r}'s header")
    idx += 1
    rows = []
    while idx < len(lines) and lines[idx].strip().startswith("|"):
        cells = [c.strip() for c in lines[idx].strip().strip("|").split("|")]
        rows.append(cells)
        idx += 1
    if not rows:
        raise ValueError(f"no data rows found under caption {caption!r}")
    return rows


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
        # One other research instrument reuses the path reader, read-only:
        # restartprobe's `path` scope (issue #8, round 2). Every use outside
        # this block is inside that research block and names only the reader.
        rp_start = self.plugin.index("// ---- restartprobe: pause-menu Restart gate research")
        rp = self.plugin[rp_start:self.plugin.index("#endif // FORGEPACT_RELEASE (restartprobe)", rp_start)]
        self.assertEqual(self.plugin.count("TgProbeDeep"), self.block.count("TgProbeDeep") + rp.count("TgProbeDeep"))
        self.assertEqual(set(re.findall(r"\bTgProbeDeep\w*", rp)), {"TgProbeDeepGet"})
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

    def test_kplayercommands_contains_only_documented_player_commands(self):
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
            "satmods", "petquest",
            # ForgePact #9 Stage B, merged from main: auto-prospect's own
            # player command (test_auto_prospect_contract.py pins it). Added
            # here because this set is an exact match, so another feature
            # landing in the same table has to be named rather than ignored.
            "autoprospect",
            "toggleborder",
            # T1 (issue #11, Track A): the re-cast guard (ToggleGuardContractTests).
            "toggleguard",
            # Issue #55: the timed-skill countdown (SkillTimerShipContractTests).
            "skilltimer",
            # Another feature in the same table, named rather than ignored:
            # the read-only menu listing (test_menu_layout_contract.py).
            "menulayout",
            # And "Restart zone at any time" (issue #8,
            # test_restart_anytime_contract.py).
            "restartanytime",
            # Explicit new player command, covered by test_mining_ore_behavior.
            "miningore", "minerhelm",
            # Pack markers (map reveal's monster half since 1.4.5): marker
            # look and counters only; test_map_reveal_contract.py covers it.
            "packmarks",
            # Craft from the stash (issue #14; test_craft_mats_contract.py).
            "craftmats",
            # Gems of Incarnation's two switches and mod filter
            # (test_incarnation_gems_contract.py).
            "gemmythic", "gemmaxroll", "gemfilter",
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

    def test_draw_state_is_restored_even_when_the_marker_throws(self):
        # Re-review finding (P2): with the restore inside the same try as the
        # marker, a throwing band left deepred at that band's alpha active for
        # everything drawn after us - counted, but not contained. The marker
        # call has its own try, both restores sit AFTER it on every path, and
        # each is best-effort so a failing restore neither skips the other nor
        # escapes the loop.
        body = function_body(self.plugin, "static void ToggleIndicatorDraw(")
        marker_catch = body.index("} catch (...) { InterlockedIncrement(&g_TibDrawExc); }")
        restore_alpha = body.index('try { g_Yytk->CallBuiltin("draw_set_alpha"')
        restore_colour = body.index('try { g_Yytk->CallBuiltin("draw_set_colour"')
        self.assertLess(marker_catch, restore_alpha)
        self.assertLess(restore_alpha, restore_colour)
        for restore in (body[restore_alpha:restore_colour], body[restore_colour:]):
            self.assertIn("catch (...) {}", restore[:restore.index("\n") + 1])
        # `drawn` counts a draw that actually finished, not one that threw.
        self.assertIn("if (drew) {", body)
        self.assertLess(restore_colour, body.index("if (drew) {"))

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


# The rows sessions 6 and 9 measured, cell by cell, quoted from
# docs/toggle-skills-research.md "## Results" -> "### Toggle skill table" and
# "## Decision" -> "### After session 6" / "### After session 9". `counter`
# and `blender` are NOT here and must not be: session 6 measured no
# persistent ON instance for Counter (its toggle state is a player buff) and
# never ran Blender's ON/OFF steps. `bushido`'s sub-talent cell is the named
# constant, D-B1 - a base-form toggle, not a real `s<NN>` slot.
SHIPPED_TABLE_ROWS = [
    ("soulSpurn", 12, "White_Mage_Soul_Spurn_AOE_obj", '"isMyClient"', "Marker", '"purgatory"', 0.0),
    ("lunarOrbit", 11, "Exo_Lunar_Orbit_Crescent_Moon_obj", "nullptr", "None", "nullptr", 0.0),
    ("crematus", 13, "Plague_Doctor_Crematus_Controller_obj", "nullptr", "Marker", '"skillContamination"', 0.0),
    ("submergedKnives", 13, "Butcher_Submerged_Knives_Knifehoarder_obj", "nullptr", "None", "nullptr", 0.0),
    ("maelstromOfFrost", 11, "Prophet_Maelstrom_obj", '"isMyClient"', "TimerHeld", '"destroyTimer"', -1.0),
    ("meteorStorm", 11, "Shaman_Meteor_Storm_Controller_obj", "nullptr", "Marker", '"skillAstroHeated"', 0.0),
    ("bushido", "kToggleNoSubTalent", "Samurai_Bushido_obj", '"isMyClient"', "None", "nullptr", 0.0),
    # Session 12 (workorder forgepact-skilltimer-buff-countdown): Counter's
    # Give No Quarter form - the one `PlayerBuff` row, never resolved by name.
    ("counter", 13, "Draw_Player_Buff_obj", "nullptr", "PlayerBuff", '"buffType"', 104.0),
]
TABLE_ROW = re.compile(
    r'\{\s*"(?P<ability>\w+)",\s*(?P<sub>\d+|kToggleNoSubTalent),\s*HeroSiege::Objects::GameObject::(?P<obj>\w+),\s*'
    r'(?P<own>nullptr|"\w+"),\s*ToggleOnMark::(?P<mark>\w+),\s*(?P<field>nullptr|"\w+"),\s*'
    r'(?P<held>-?[\d.]+)\s*\}',
    re.S,
)
# kSkillTimerBuffRows' own row shape (SkillTimerMod.hpp): { "id", buffId,
# measuredFirst, "Name (Class)" } - the session-12 buff-carried table, kept
# module-level (like TABLE_ROW above) so both SkillTimerRuleContractTests'
# _forbidden_names and SkillTimerBuffContractTests below can read it without
# a second copy of the pattern.
BUFF_ROW = re.compile(r'\{\s*"(?P<ability>\w+)",\s*(?P<buffId>\d+),\s*(?P<first>[\d.]+),\s*"(?P<display>[^"]+)"\s*\}')
# Names the shipped table now owns: a production function spelling one of
# these again would be a second, drifting copy of the row set.
TABLE_ONLY_NAMES = (
    "White_Mage_Soul_Spurn_AOE_obj", '"purgatory"', '"skillContamination"', '"destroyTimer"',
    '"soulSpurn"', "kToggleIndicatorTalentId",
    "Shaman_Meteor_Storm_Controller_obj", "Samurai_Bushido_obj", '"skillAstroHeated"',
    '"meteorStorm"', '"bushido"', '"counter"',
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


class SkillTimerShipContractTests(unittest.TestCase):
    """The shipped timed-skill countdown (issue #55): `skilltimer`.

    Companion to test_toggle_skill_behavior.py's `skilltimer/*` scenarios,
    which run the read/latch/draw decisions end to end, and to
    SkillTimerProbeContractTests, which pins the research instrument that
    produced the looks and placements this class pins as the ship's own.
    """

    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_SRC.read_text(encoding="utf-8")
        cls.stripped = strip_research_blocks(cls.plugin)
        cls.header = (FORGEPACT_DIR / "plugin" / "include" / "ForgePact" / "SkillTimerMod.hpp").read_text(encoding="utf-8")
        cls.panel = (SRC_DIR / "forgepact.py").read_text(encoding="utf-8")
        cls.research_doc = (FORGEPACT_DIR / "docs" / "toggle-skills-research.md").read_text(encoding="utf-8")
        cls.release_notes = read_release_notes()
        cls.readme = (FORGEPACT_DIR / "README.md").read_text(encoding="utf-8")

    def test_skilltimer_is_a_player_command(self):
        match = re.search(
            r"static const std::unordered_set<std::string> kPlayerCommands = \{(.*?)\};",
            self.plugin, re.S)
        self.assertIsNotNone(match)
        entries = {tok.strip().strip('"') for tok in match.group(1).split(",") if tok.strip()}
        self.assertIn("skilltimer", entries)
        start = self.plugin.index('if (lc == "skilltimer")')
        end = self.plugin.index('if (lc == "toggleguard")', start)
        branch = self.plugin[start:end]
        self.assertIn('v == "stat"', branch)
        self.assertIn('v == "off" || v == "0"', branch)
        self.assertNotIn("HookOneScript(", branch)
        self.assertIn('if (lc == "skilltimer")', self.stripped)

    def test_draw_is_called_after_toggle_indicator_outside_research(self):
        body = function_body(self.plugin, "static RValue& Hook_DrawHudBuffs(")
        self.assertIn("ToggleIndicatorDraw();", body)
        self.assertIn("SkillTimerDraw();", body)
        self.assertLess(body.index("ToggleIndicatorDraw();"), body.index("SkillTimerDraw();"))
        stripped_body = function_body(self.stripped, "static RValue& Hook_DrawHudBuffs(")
        self.assertIn("SkillTimerDraw();", stripped_body)

    def test_off_is_the_first_statement(self):
        body = function_body(self.plugin, "static void SkillTimerDraw(")
        first_statement = body.strip().splitlines()[0].strip()
        self.assertEqual(
            first_statement,
            "if (g_SkillTimerStyle.load() == ForgePact::SkillTimerStyle::Off) return;")

    def test_ship_constants_equal_the_probe_defaults(self):
        # D-U12/D-U13-style pin: every look constant the ship carries as its
        # own is asserted equal to the probe's own default, so the two
        # cannot drift apart.
        ship_colour = re.search(
            r"static constexpr double kSkillTimerColourR = ([\d.]+), kSkillTimerColourG = ([\d.]+), "
            r"kSkillTimerColourB = ([\d.]+);", self.plugin)
        probe_colour = re.search(
            r"static double g_TgSpriteColourR = ([\d.]+), g_TgSpriteColourG = ([\d.]+), "
            r"g_TgSpriteColourB = ([\d.]+);", self.plugin)
        self.assertIsNotNone(ship_colour); self.assertIsNotNone(probe_colour)
        self.assertEqual(ship_colour.groups(), probe_colour.groups())

        ship_bands = re.search(r"static constexpr int kSkillTimerBands = (\d+);", self.plugin)
        probe_bands = re.search(
            r"static constexpr int kBands = (\d+);",
            function_body(self.plugin, "static void TgProbeSpriteDrawSoft("))
        self.assertIsNotNone(ship_bands); self.assertIsNotNone(probe_bands)
        self.assertEqual(ship_bands.group(1), probe_bands.group(1))

        ship_bar = re.search(
            r"static constexpr double kSkillTimerBarGap = ([\d.]+), kSkillTimerBarHeight = ([\d.]+), "
            r"kSkillTimerBarInset = ([\d.]+);", self.plugin)
        self.assertIsNotNone(ship_bar)
        probe_bar_body = function_body(self.plugin, "static void TgProbeSpriteDrawBar(")
        probe_gap_height = re.search(
            r"static constexpr double kBarGap = ([\d.]+), kBarHeight = ([\d.]+);", probe_bar_body)
        self.assertIsNotNone(probe_gap_height)
        probe_inset = re.search(r"static double g_TgSpriteBarInset = ([\d.]+);", self.plugin)
        self.assertIsNotNone(probe_inset)
        self.assertEqual(ship_bar.group(1), probe_gap_height.group(1))
        self.assertEqual(ship_bar.group(2), probe_gap_height.group(2))
        self.assertEqual(ship_bar.group(3), probe_inset.group(1))

        # Only the horizontal text offset is still the probe's *default*. The
        # vertical one is not the probe's default either (2026-09-21, owner,
        # live: `tgprobe sprite style number` on the same box and font,
        # `textoffset 0 -106` - "perfect") - it is what the owner accepted
        # with the probe, which the ship now draws exactly (D-N1); see
        # test_number_anchors_top_aligned_at_the_tuned_offset and
        # test_ship_and_probe_number_formulas_agree. The probe's own default
        # `g_TgSpriteTextOffsetDy` stays -101 for research (unchanged; see
        # docs/toggle-skills-research.md).
        ship_dx = re.search(r"static constexpr double kSkillTimerTextOffsetDx = (-?[\d.]+),", self.plugin)
        probe_text = re.search(
            r"static double g_TgSpriteTextOffsetDx = (-?[\d.]+), g_TgSpriteTextOffsetDy = (-?[\d.]+);",
            self.plugin)
        self.assertIsNotNone(ship_dx); self.assertIsNotNone(probe_text)
        self.assertEqual(ship_dx.group(1), probe_text.group(1))

    def test_ship_draw_references_no_research_symbol(self):
        for sig in ("static void SkillTimerDraw(", "static void SkillTimerDrawArc(",
                    "static void SkillTimerDrawBar(", "static void SkillTimerDrawNumber(",
                    "static void SkillTimerDrawFade(", "static void SkillTimerDrawStyle(",
                    "static void SkillTimerReadRow(", "static RValue SkillTimerColour(",
                    "static bool SkillTimerResolveRowObject(", "static int SkillTimerToggleTwin(",
                    "static std::string SkillTimerTableRowsLine(",
                    "static void SkillTimerDrawRectOutlineFraction(",
                    "static void SkillTimerStats(", "static std::string SkillTimerRowCountersLine(",
                    "static std::string SkillTimerAggregateCountersLine("):
            body = function_body(self.plugin, sig)
            self.assertNotIn("TgProbe", body, sig)
            self.assertNotIn("g_TgSprite", body, sig)

    def test_stat_is_read_only(self):
        start = self.plugin.index('if (lc == "skilltimer")')
        end = self.plugin.index('if (lc == "toggleguard")', start)
        branch = self.plugin[start:end]
        stat_start = branch.index('v == "stat"')
        stat_end = branch.index('v == "off" || v == "0"', stat_start)
        stat_branch = branch[stat_start:stat_end]
        self.assertIn("SkillTimerStats()", stat_branch)
        self.assertNotIn("g_SkillTimerStyle.store", stat_branch)

    def test_panel_default_is_off_and_startup_list_unchanged(self):
        self.assertIn("mod_skill_timer_style", forgepact.DEFAULTS)
        self.assertEqual(forgepact.DEFAULTS["mod_skill_timer_style"], "off")
        cmds = forgepact.build_cmds(dict(forgepact.DEFAULTS))
        self.assertFalse([c for c in cmds if c.startswith("skilltimer")])

    def test_build_cmds_emits_each_style(self):
        for style in ("arc", "bar", "number", "fade"):
            cfg = dict(forgepact.DEFAULTS)
            cfg["mod_skill_timer_style"] = style
            self.assertIn(f"skilltimer {style}", forgepact.build_cmds(cfg))
        # A hand-edited invalid saved value emits nothing.
        cfg = dict(forgepact.DEFAULTS)
        cfg["mod_skill_timer_style"] = "not-a-style"
        self.assertFalse([c for c in forgepact.build_cmds(cfg) if c.startswith("skilltimer")])

    def test_invalid_style_is_rejected(self):
        self.assertFalse(forgepact.skill_timer_style_valid("glow"))
        self.assertFalse(forgepact.skill_timer_style_valid(""))
        self.assertFalse(forgepact.skill_timer_style_valid(None))
        self.assertTrue(forgepact.skill_timer_style_valid("  Arc  "))
        self.assertTrue(forgepact.skill_timer_style_valid("OFF"))
        # /api/set answers 400 and saves nothing on an invalid value.
        self.assertIn('self._json({"err": "invalid skilltimer style"}, 400)', self.panel)

    def test_html_has_one_select_with_five_styles_in_order(self):
        self.assertEqual(forgepact.HTML.count("<select"), 1)
        m = re.search(
            r'<select class="style-select" id="mod_skill_timer_style">(.*?)</select>',
            forgepact.HTML, re.S)
        self.assertIsNotNone(m)
        values = re.findall(r'<option value="(\w+)">', m.group(1))
        self.assertEqual(values, ["off", "arc", "bar", "number", "fade"])

    def test_panel_sends_the_live_command(self):
        self.assertIn(
            "send_cmds([f\"skilltimer {cfg['mod_skill_timer_style']}\"], cfg)",
            self.panel,
        )

    def test_select_painted_in_both_render_paths(self):
        self.assertEqual(
            self.panel.count("document.getElementById('mod_skill_timer_style').value="), 2)
        # Never in the boolean/on-off maps: those set .checked, not .value.
        booleans_start = self.panel.index("const booleans={")
        booleans_line = self.panel[booleans_start:self.panel.index("};", booleans_start)]
        self.assertNotIn("mod_skill_timer_style", booleans_line)

    def test_research_decision_records_route_b(self):
        decision = self.research_doc[self.research_doc.index("### Decision", self.research_doc.index(
            "## Issue #55")):]
        self.assertIn("Route B, taken by the user on 2026-09-21", decision)
        self.assertIn("latched", decision)
        self.assertIn("unlatched", decision)
        self.assertIn("Mid-cast limitation", decision)
        self.assertNotIn("Not yet taken", decision)

    def test_release_notes_and_readme_name_the_control(self):
        # The contract: the panel's own visible label appears in both player-
        # facing documents so a reader can match the control to the note.
        label = "Timed skill countdown"
        self.assertIn(f'<span class="lbl" style="width:auto;flex:1">{label}', self.panel)
        if self.release_notes is not None:
            self.assertIn(label, self.release_notes)
        self.assertIn(label, self.readme)

    # ---- session 8: the countdown's own table (D-S1) ----------------------
    # The countdown reads kSkillTimerRows and nothing else; each row is one
    # the duration sweep measured (docs/toggle-skills-research.md, "Duration
    # sweep (session 8)" -> "Results", status `ship`).

    COUNTDOWN_ROW = re.compile(
        r'\{\s*"(?P<ability>[A-Za-z]+)",\s*HeroSiege::Objects::GameObject::(?P<obj>\w+),\s*'
        r'(?P<own>nullptr|"isMyClient"),\s*(?P<first>[\d.]+),\s*"(?P<display>[^"]+)"\s*\}')
    # abilityId -> (object, ownership, measuredFirst): session 8's four rows
    # plus session 10's three (Progenies, Pickup Raid, Dissipating Tornado) -
    # seven explicit rows in total.
    SHIP_SET = {
        "healingZone": ("White_Mage_Healing_Zone_obj", "nullptr", 1152.0),
        "bladeBarrier": ("Samurai_Blade_Barrier_obj", '"isMyClient"', 1296.0),
        "soulSpurn": ("White_Mage_Soul_Spurn_AOE_obj", '"isMyClient"', 144.0),
        "maelstromOfFrost": ("Prophet_Maelstrom_obj", '"isMyClient"', 4320.0),
        "progeniesOfTheGreatCataclysm": ("Bard_Progenies_Amplifier_obj", "nullptr", 2880.0),
        "pickupRaid": ("Redneck_Pickup_Truck_obj", '"isMyClient"', 576.0),
        "dissipatingTornado": ("Dissipating_Tornado_obj", "nullptr", 432.0),
    }

    def countdown_table(self):
        start = self.header.index("inline constexpr SkillTimerRow kSkillTimerRows[] = {")
        return self.header[start:self.header.index("\n};", start)]

    def countdown_rows(self):
        return [m.groupdict() for m in self.COUNTDOWN_ROW.finditer(self.countdown_table())]

    def test_countdown_iterates_the_countdown_table_not_the_toggle_table(self):
        rows = self.countdown_rows()
        self.assertEqual(
            {r["ability"]: (r["obj"], r["own"], float(r["first"])) for r in rows}, self.SHIP_SET)
        self.assertEqual(len(rows), len(self.SHIP_SET), self.countdown_table())
        self.assertIn("inline constexpr int kSkillTimerRowCount =", self.header)
        draw = function_body(self.stripped, "static void SkillTimerDraw(")
        for needle in ("r < ForgePact::kSkillTimerRowCount", "ForgePact::kSkillTimerRows[r]",
                       "g_SkillTimerTableIds.Get(r)", "g_SkillTimerRowState[r]", "g_StRow[r]"):
            self.assertIn(needle, draw, needle)
        for banned in ("kToggleSkillRowCount", "g_ToggleTableIds"):
            self.assertNotIn(banned, draw, banned)
        for decl in ("static ForgePact::SkillTimerRowState g_SkillTimerRowState[ForgePact::kSkillTimerRowCount];",
                     "static SkillTimerRowCounters g_StRow[ForgePact::kSkillTimerRowCount] = {};"):
            self.assertIn(decl, self.stripped, decl)
        self.assertIn("static void SkillTimerReadRow(const ForgePact::SkillTimerRow& row,", self.stripped)

    def test_every_countdown_row_is_measured_in_the_research_doc(self):
        doc = self.research_doc
        sweep = doc.index("### Duration sweep (session 8)")
        results = doc[doc.index("#### Results", sweep):]
        table_lines = [line for line in results.splitlines() if line.startswith("|")]
        for row in self.countdown_rows():
            first = "%.6f" % float(row["first"])
            hits = [line for line in table_lines
                    if f"`{row['obj']}`" in line and first in line
                    and len(line.split("|")) > 12 and line.split("|")[11].strip() == "ship"]
            self.assertTrue(hits, f"{row['ability']}: no `ship` Results row naming {row['obj']} with {first}")

    def test_countdown_rows_use_sdk_constants_and_no_talent_ids(self):
        objects_hpp = (SDK_INCLUDE / "objects.hpp").read_text(encoding="utf-8")
        struct = self.header[self.header.index("struct SkillTimerRow {"):]
        struct = struct[:struct.index("};")]
        self.assertNotIn("int ", struct)          # no talent id column: ids resolve at runtime (D-P1)
        self.assertNotIn("talentId", self.countdown_table())
        for row in self.countdown_rows():
            self.assertRegex(objects_hpp, rf"\b{row['obj']}\s*=\s*\d+,", row)
            # Every runtime name lives in the header's table, never again in
            # the plugin's player build.
            self.assertNotIn(f'"{row["ability"]}"', self.stripped, row)
            self.assertNotIn(row["obj"], self.stripped, row)
        # Companion skills are out (owner, 2026-09-21): no row sits under the
        # sentry parent, whose members this table's objects would be named for.
        for banned in ("Turret", "Totem", "Hydra"):
            self.assertNotIn(banned, self.countdown_table(), banned)

    def test_both_tables_resolve_in_one_walk(self):
        walk = function_body(self.plugin, "static bool ToggleTableResolveIds(")
        self.assertEqual(walk.count('"ds_map_find_first"'), 1)
        for needle in ("ForgePact::kToggleSkillRows[r].abilityId", "g_ToggleTableIds.Set(r, id)",
                       "ForgePact::kSkillTimerRows[r].abilityId", "g_SkillTimerTableIds.Set(r, id)",
                       "SkillTimerTableUnresolvedRows()"):
            self.assertIn(needle, walk, needle)
        due = function_body(self.plugin, "static bool ToggleTableResolveDue(")
        # Issue #55 follow-up (D-S4) removed the "stop once every explicit
        # row of both tables has resolved" early-out - the rule map has no
        # such stopping signal and needs rebuilding every room regardless.
        # Round 1 restores that early-out for style Off ONLY (the one case
        # where the rule map is never consulted anyway); a look selected
        # stays due purely on the once-per-room gate, plus one more walk in
        # the same room if the last one ran while Off.
        self.assertIn("ToggleTableUnresolvedRows() + SkillTimerTableUnresolvedRows() == 0) return false;", due)
        self.assertIn("g_SkillTimerStyle.load() == ForgePact::SkillTimerStyle::Off", due)
        self.assertIn("return g_ToggleResolveWalkedRuleOff;", due)
        # No second talent-map walk anywhere: the countdown's ids come from this
        # one. The one other map walk in the player build is craftmats' consume
        # check over the character's item map (issue #14), which reads no
        # talent - it is counted out here by name, so any further walk fails.
        craft_walk = function_body(self.plugin, "static int64_t CmCharacterTotal(")
        self.assertEqual(craft_walk.count('"ds_map_find_first", { map }'), 1)
        self.assertEqual(self.stripped.count('"ds_map_find_first", { map }') - craft_walk.count('"ds_map_find_first", { map }'), 1)
        self.assertNotIn("ToggleTableResolveIds", function_body(self.plugin, "static void SkillTimerDraw("))

    def test_guard_membership_is_toggle_table_only(self):
        for sig in ("static int ToggleTableRowForTalentId(", "static RValue& HookTalentUseClass("):
            body = function_body(self.plugin, sig)
            for banned in ("kSkillTimerRow", "g_SkillTimerTableIds", "SkillTimerRow"):
                self.assertNotIn(banned, body, sig + "/" + banned)
        self.assertIn("ForgePact::kToggleSkillRowCount",
                      function_body(self.plugin, "static int ToggleTableRowForTalentId("))

    def test_stat_prints_one_line_per_countdown_row(self):
        stats = function_body(self.stripped, "static void SkillTimerStats(")
        self.assertIn("r < ForgePact::kSkillTimerRowCount", stats)
        self.assertIn("SkillTimerRowCountersLine(r)", stats)
        self.assertIn("SkillTimerTableRowsLine()", stats)
        self.assertNotIn("ToggleTableRowsLine()", stats)
        row_line = function_body(self.stripped, "static std::string SkillTimerRowCountersLine(")
        self.assertIn("ForgePact::kSkillTimerRows[row].abilityId", row_line)
        ids_line = function_body(self.stripped, "static std::string SkillTimerTableRowsLine(")
        for needle in ("ForgePact::kSkillTimerRows[r].abilityId", ":talentId=", "unresolved",
                       "resolveWalks=", "unresolvedRows=", "SkillTimerTableUnresolvedRows()"):
            self.assertIn(needle, ids_line, needle)
        aggregate = function_body(self.stripped, "static std::string SkillTimerAggregateCountersLine(")
        self.assertIn("ForgePact::kSkillTimerRowCount", aggregate)
        self.assertNotIn("kToggleSkillRowCount", aggregate)

    def test_number_anchors_top_aligned_at_the_tuned_offset(self):
        # 2026-09-21 live tuning (owner, `tgprobe sprite style number` on the
        # D-U12 box in `__newfont6`): `textoffset 0 -106` - "perfect". The
        # ship now draws that same anchor (D-N1: what was judged is what
        # ships) - top alignment from the box's BOTTOM edge, not bottom
        # alignment from its TOP.
        body = function_body(self.plugin, "static void SkillTimerDrawNumber(")
        self.assertIn('"draw_set_valign", { RValue(0.0) }', body)
        self.assertIn("y + h + kSkillTimerTextOffsetDy", body)
        self.assertNotIn("kSkillTimerTextGap", self.plugin)
        dy = re.findall(r"kSkillTimerTextOffsetDy = (-?[\d.]+)", self.plugin)
        self.assertEqual(len(dy), 1, dy)
        self.assertEqual(float(dy[0]), round(float(dy[0])))   # whole pixels (guide Known Limitations 18)
        self.assertEqual(float(dy[0]), -106.0)
        # __newfont6 is still resolved by name, unresolved counted rather
        # than failing the draw - unchanged by the anchor move.
        self.assertIn('CallBuiltin("asset_get_index", { RValue(std::string("__newfont6")) })', body)
        # The bar's own gap is untouched by this change.
        bar_gap = re.search(r"static constexpr double kSkillTimerBarGap = ([\d.]+),", self.plugin)
        self.assertEqual(float(bar_gap.group(1)), 2.0)

    def test_ship_and_probe_number_formulas_agree(self):
        # D-N1: the ship draws the probe's exact formula, so what the owner
        # judged live with the probe is what ships. Only the ship's dy is
        # its own tuned value (-106); the probe's own default dy stays -101
        # for research (context "Why the probe default stays -101").
        ship_body = function_body(self.plugin, "static void SkillTimerDrawNumber(")
        probe_body = function_body(self.plugin, "static void TgProbeSpriteDrawNumber(")
        for body in (ship_body, probe_body):
            self.assertIn('"draw_set_halign", { RValue(1.0) }', body)
            self.assertIn('"draw_set_valign", { RValue(0.0) }', body)
        self.assertIn("x + w / 2.0 + kSkillTimerTextOffsetDx", ship_body)
        self.assertIn("y + h + kSkillTimerTextOffsetDy", ship_body)
        self.assertIn("x + w / 2.0 + g_TgSpriteTextOffsetDx", probe_body)
        self.assertIn("y + h + g_TgSpriteTextOffsetDy", probe_body)

        ship_dx = re.search(r"static constexpr double kSkillTimerTextOffsetDx = (-?[\d.]+), "
                             r"kSkillTimerTextOffsetDy = (-?[\d.]+);", self.plugin)
        probe_text = re.search(
            r"static double g_TgSpriteTextOffsetDx = (-?[\d.]+), g_TgSpriteTextOffsetDy = (-?[\d.]+);",
            self.plugin)
        self.assertIsNotNone(ship_dx); self.assertIsNotNone(probe_text)
        self.assertEqual(ship_dx.group(1), probe_text.group(1))   # dx: still the probe's default
        self.assertEqual(ship_dx.group(2), "-106.0")   # dy: the accepted value, not the probe's default

        probe_scale = re.search(r"static double g_TgSpriteScale = ([\d.]+);", self.plugin)
        self.assertIsNotNone(probe_scale)
        self.assertEqual(float(probe_scale.group(1)), 1.0)

        # The tuned Blade Barrier box (2026-09-21): 77x78 @ (92, 1121).
        bw, bh, bx, by = 77.0, 78.0, 92.0, 1121.0
        dx, dy = float(ship_dx.group(1)), float(ship_dx.group(2))
        tx = bx + bw / 2.0 + dx
        ty = by + bh + dy
        self.assertEqual((tx, ty), (130.5, 1093.0))


class SkillTimerRuleContractTests(unittest.TestCase):
    """Rule-based coverage of untested skills (issue #55 follow-up, D-S4).

    Companion to SkillTimerShipContractTests (the seven explicit rows) and
    test_toggle_skill_behavior.py's `rule/*` scenarios, which run the
    eligibility decision and the rule draw end to end against a controlled
    game API. This class pins the source text: the generated table matches
    hs-game-sdk, no hand-typed object name reaches the rule path, the walk
    reads the right fields by name, the deny-list names every measured
    negative, and the player text states the tier honestly.
    """

    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_SRC.read_text(encoding="utf-8")
        cls.stripped = strip_research_blocks(cls.plugin)
        cls.header = (FORGEPACT_DIR / "plugin" / "include" / "ForgePact" / "SkillTimerMod.hpp").read_text(encoding="utf-8")
        cls.names_header = (FORGEPACT_DIR / "plugin" / "include" / "ForgePact" / "SkillTimerNames.hpp").read_text(encoding="utf-8")
        # Round 1: the toggle table's own abilityIds are one of the four
        # sources player-text forbidden names are derived from - read live so
        # a row the Meteor Storm session adds to kToggleSkillRows is picked
        # up with no test edit.
        cls.toggle_header = (FORGEPACT_DIR / "plugin" / "include" / "ForgePact" / "ToggleSkillMod.hpp").read_text(
            encoding="utf-8")
        cls.panel = (SRC_DIR / "forgepact.py").read_text(encoding="utf-8")
        cls.research_doc = (FORGEPACT_DIR / "docs" / "toggle-skills-research.md").read_text(encoding="utf-8")
        cls.release_notes = read_release_notes()
        cls.readme = (FORGEPACT_DIR / "README.md").read_text(encoding="utf-8")

    DENY_LIST = {
        "submergedKnives", "crematus", "blizzard", "arrowRain", "meteorStorm",
        "defensiveShout", "berserk", "arrowTurret", "fireTotem",
        "bushido", "holyForm", "unholyForm", "melonForm",
    }
    EXPLICIT_ROWS = {
        "healingZone", "bladeBarrier", "soulSpurn", "maelstromOfFrost",
        "progeniesOfTheGreatCataclysm", "pickupRaid", "dissipatingTornado",
    }

    def test_generated_table_matches_the_sdk(self):
        import gen_skill_timer_names as gen
        rows, ambiguous = gen.build_table()
        table = {key: member for key, member in rows}
        # Hand-checked positive controls, not just trusting the generator's
        # own output: real, independently-known object names.
        self.assertEqual(table["orboffrost"].name, "Jotunn_Orb_of_Frost_obj")
        self.assertEqual(table["volcano"].name, "Pyromancer_Volcano_obj")
        self.assertEqual(table["healingzone"].name, "White_Mage_Healing_Zone_obj")
        self.assertGreaterEqual(len(rows), 700)
        # Regeneration is byte-identical to the checked-in header.
        self.assertEqual(gen.render(rows), self.names_header)

    def test_generated_table_has_no_companion_and_no_ambiguous_entry(self):
        import gen_skill_timer_names as gen
        from hs_game_sdk import GameObject, get_ancestor_indices
        rows, ambiguous = gen.build_table()
        for key, member in rows:
            ancestors = {GameObject(a).name for a in get_ancestor_indices(member.value)}
            self.assertNotIn("Player_Sentry_Parent_obj", ancestors, key)
        # The prototype run's own measured ambiguous keys stay dropped.
        for key in ("menulight", "icyground", "buckshot"):
            self.assertIn(key, ambiguous, key)
        keys = {key for key, _ in rows}
        for key in ("menulight", "icyground", "buckshot"):
            self.assertNotIn(key, keys, key)
        # A companion object itself never has a key in the shipped table.
        self.assertNotIn('"arrowturret"', self.names_header)
        self.assertNotIn('"firetotem"', self.names_header)

    def test_no_hand_typed_object_name_reaches_the_rule_path(self):
        player = strip_research_blocks(self.plugin)
        for sig in ("static bool ToggleTableResolveIds(", "static void SkillTimerDraw(",
                    "static bool SkillTimerRuleResolveObject(", "static void SkillTimerRuleReadEntry("):
            body = function_body(player, sig)
            self.assertNotIn("GameObject::", body, sig)
        # SkillTimerEnumerateHotbar's own hotbar lookup is the one legitimate
        # exception - the same shared UI_Hud_Talent_obj ToggleIndicatorFindSlot
        # already hand-names. Any OTHER enumerator here would be a hand-typed
        # skill object reaching the rule path.
        hotbar = function_body(player, "static bool SkillTimerEnumerateHotbar(")
        names = set(re.findall(r"GameObject::(\w+)", hotbar))
        self.assertEqual(names, {"UI_Hud_Talent_obj"})
        # The only other literal enumerators anywhere are the seven explicit
        # rows (SkillTimerMod.hpp) and the generated header's own table.
        rule_section = self.header[self.header.index("struct SkillTimerRuleEntry"):]
        self.assertNotIn("GameObject::", rule_section)

    def test_eligibility_reads_duration_and_cooldown_by_name_with_the_floor_constant(self):
        walk = function_body(self.plugin, "static bool ToggleTableResolveIds(")
        for needle in ('"abilityDuration"', '"abilityCooldown"', "SkillTimerRuleModel::Eligible("):
            self.assertIn(needle, walk, needle)
        eligible = function_body(self.header, "static bool Eligible(")
        for needle in ("kSkillTimerCooldownFloor", "duration > 0.0", "isExplicitRow", "denied", "readable"):
            self.assertIn(needle, eligible, needle)
        self.assertIn("inline constexpr double kSkillTimerCooldownFloor = 0.25;", self.header)

    def test_deny_list_names_every_measured_negative(self):
        block = self.header[self.header.index("kSkillTimerRuleDeny[] = {"):]
        block = block[:block.index("};")]
        for name in self.DENY_LIST:
            self.assertIn(f'"{name}"', block, name)
        self.assertEqual(len(self.DENY_LIST), 13)
        self.assertIn("inline constexpr int kSkillTimerRuleDenyCount =", self.header)

    def test_explicit_rows_win_over_the_rule(self):
        walk = function_body(self.plugin, "static bool ToggleTableResolveIds(")
        self.assertIn("SkillTimerRuleIsExplicitRow(name)", walk)
        fn = function_body(self.header, "inline bool SkillTimerRuleIsExplicitRow(")
        self.assertIn("kSkillTimerRows[i].abilityId", fn)
        # EXPLICIT_ROWS names exactly the countdown table's own abilityIds -
        # a row added to kSkillTimerRows without a matching EXPLICIT_ROWS
        # entry (or vice versa) fails here rather than passing silently.
        countdown_start = self.header.index("inline constexpr SkillTimerRow kSkillTimerRows[] = {")
        countdown_table = self.header[countdown_start:self.header.index("\n};", countdown_start)]
        header_ids = {m.group("ability") for m in SkillTimerShipContractTests.COUNTDOWN_ROW.finditer(countdown_table)}
        self.assertEqual(self.EXPLICIT_ROWS, header_ids)
        eligible = function_body(self.header, "static bool Eligible(")
        self.assertIn("if (isExplicitRow) return false;", eligible)

    def test_rule_rows_are_own_by_default_and_guard_membership_is_unchanged(self):
        read = function_body(self.plugin, "static void SkillTimerRuleReadEntry(")
        self.assertNotIn("ownershipField", read)
        self.assertIn("anyOwn = true;", read)   # D-N3: no ownership field, every instance own
        guard = function_body(self.plugin, "static int ToggleTableRowForTalentId(")
        self.assertIn("ForgePact::kToggleSkillRowCount", guard)
        self.assertNotIn("Rule", guard)   # guard membership stays toggle-table only

    def test_rule_map_is_rebuilt_once_per_room_and_bounded(self):
        due = function_body(self.plugin, "static bool ToggleTableResolveDue(")
        # Round 1: the early-out is back for style Off (the rule map is never
        # consulted while off), and the once-per-room gate for a look also
        # re-arms one more walk in the same room when the last walk ran Off.
        self.assertIn("ToggleTableUnresolvedRows() + SkillTimerTableUnresolvedRows() == 0) return false;", due)
        self.assertIn("g_SkillTimerStyle.load() == ForgePact::SkillTimerStyle::Off", due)
        self.assertIn("return g_ToggleResolveWalkedRuleOff;", due)
        walk = function_body(self.plugin, "static bool ToggleTableResolveIds(")
        self.assertIn("g_SkillTimerRuleCount = 0;", walk)
        self.assertIn("ForgePact::kSkillTimerRuleCap", walk)
        self.assertIn("g_SkillTimerRuleCapped", walk)
        self.assertIn("inline constexpr int kSkillTimerRuleCap = 64;", self.header)

    def test_stat_prints_the_rule_counters(self):
        stats = function_body(self.stripped, "static void SkillTimerStats(")
        self.assertIn("SkillTimerRuleCountersLine()", stats)
        self.assertIn("SkillTimerRuleEntryLine(", stats)
        counters = function_body(self.plugin, "static std::string SkillTimerRuleCountersLine(")
        for needle in ("ruleRows=", "ruleDrawn=", "ruleNoInstance=", "ruleUnreadable=", "ruleExpired=",
                       "ruleToggleOn=", "ruleNoObject=", "ruleDenied=", "ruleUnreadableFields=", "ruleCapped=",
                       "ruleNoName=", "ruleLatched=", "ruleUnlatched="):
            self.assertIn(needle, counters, needle)
        entry_line = function_body(self.plugin, "static std::string SkillTimerRuleEntryLine(")
        self.assertIn(":talentId=", entry_line)
        self.assertIn("object=", entry_line)
        self.assertIn("unresolved", entry_line)

    def test_rule_expectation_in_the_research_doc_matches_the_capture(self):
        doc = self.research_doc
        start = doc.index("#### Rule coverage expectation")
        section = doc[start:]
        cut = section.find("\n### ")
        if cut >= 0:
            section = section[:cut]
        entries = re.findall(
            r"abilityId=(\w+) abilityAura=\w+ abilityDuration=([\d.]+) abilityCooldown=([\d.]+)", section)
        self.assertGreaterEqual(len(entries), 146)

        import gen_skill_timer_names as gen
        rows, _ = gen.build_table()
        table = {key for key, _ in rows}

        computed = set()
        for ability_id, duration, cooldown in entries:
            if ability_id in self.EXPLICIT_ROWS or ability_id in self.DENY_LIST:
                continue
            if ability_id.lower() not in table:
                continue
            if float(duration) > 0.0 and float(cooldown) > 0.25:
                computed.add(ability_id)

        doc_selected = set(re.findall(r"\| `([a-zA-Z]+)` \| selected", section))
        self.assertEqual(computed, doc_selected)
        self.assertEqual(len(doc_selected), 17)

        # The doc's own `explicit` rows are exactly EXPLICIT_ROWS intersected
        # with the ids this capture actually saw - a row added to
        # EXPLICIT_ROWS whose id the capture never names (or vice versa)
        # fails here rather than passing silently.
        capture_ids = {ability_id for ability_id, _, _ in entries}
        doc_explicit = set(re.findall(r"\| `([a-zA-Z]+)` \| explicit", section))
        self.assertEqual(doc_explicit, self.EXPLICIT_ROWS & capture_ids)

    # ---- round 1 (owner, 2026-09-21, "Yes, no skill names"): the countdown's
    # own player text names no skill at all. Forbidden names are DERIVED,
    # never a hard-coded list - a hard-coded one goes stale the moment a new
    # row lands (the Meteor Storm session is adding rows to kToggleSkillRows).

    def _forbidden_names(self):
        # Five sources, each contributing at least one name so an emptied
        # regex can never pass silently: the seven explicit countdown rows'
        # own display names, the toggle table's own abilityIds (read live,
        # context "Name-free player text (round 1)"), the measured
        # deny-list's abilityIds, the research doc's rule-selected ids
        # (AC4's 17), and (session 12) kSkillTimerBuffRows' own display names -
        # Counter/Last Stand/Defensive Shout/Berserk were never derived from
        # anything before this table existed.
        names = set()
        countdown_start = self.header.index("inline constexpr SkillTimerRow kSkillTimerRows[] = {")
        countdown_table = self.header[countdown_start:self.header.index("\n};", countdown_start)]
        for m in SkillTimerShipContractTests.COUNTDOWN_ROW.finditer(countdown_table):
            names.add(m.group("display").split(" (")[0])

        buff_start = self.header.index("inline constexpr SkillTimerBuffRow kSkillTimerBuffRows[] = {")
        buff_table = self.header[buff_start:self.header.index("\n};", buff_start)]
        for m in BUFF_ROW.finditer(buff_table):
            names.add(m.group("display").split(" (")[0])

        toggle_start = self.toggle_header.index("inline constexpr ToggleSkillRow kToggleSkillRows[] = {")
        toggle_table = self.toggle_header[toggle_start:self.toggle_header.index("\n};", toggle_start)]
        for m in TABLE_ROW.finditer(toggle_table):
            names.add(m.group("ability"))

        names |= set(self.DENY_LIST)

        doc = self.research_doc
        start = doc.index("#### Rule coverage expectation")
        section = doc[start:]
        cut = section.find("\n### ")
        if cut >= 0:
            section = section[:cut]
        names |= set(re.findall(r"\| `([a-zA-Z]+)` \| selected", section))

        return names

    @staticmethod
    def _words(name):
        # A camelCase id or an already-spaced display name both split the
        # same way: "maelstromOfFrost" -> ["maelstrom", "of", "frost"];
        # "Healing Zone" -> ["healing", "zone"].
        return [w.lower() for w in re.findall(r"[A-Z]?[a-z0-9]+", name)]

    def _forbidden_pattern(self):
        names = self._forbidden_names()
        self.assertTrue(names)   # never an emptied regex
        alternatives = []
        for name in names:
            words = self._words(name)
            if not words:
                continue
            spaced = r"\s+".join(re.escape(w) for w in words)
            joined = "".join(re.escape(w) for w in words)
            alternatives.append(rf"(?:{spaced}|{joined})")
        return re.compile(r"\b(?:" + "|".join(alternatives) + r")\b", re.IGNORECASE)

    def _countdown_text_blocks(self):
        readme_row = next(line for line in self.readme.split("\n")
                           if line.startswith("| **Timed skill countdown**"))
        panel = self.panel[self.panel.index("Timed skill countdown<br>"):]
        panel = panel[:panel.index("</span></span>") + len("</span></span>")]
        blocks = {"README": readme_row, "panel": panel}
        if self.release_notes is not None:
            release = self.release_notes[self.release_notes.index("**Timed skill countdown.**"):]
            cut = release.find("\n- **")
            if cut >= 0:
                release = release[:cut]
            blocks["release notes"] = release
        return blocks

    def _toggle_text_blocks(self):
        # Round 1 (name-free player text): the toggle marker/guard blocks -
        # the two README rows, the two panel spans, the two release-notes
        # bullets, and the intro (everything before `## New`). D-T1's reworded
        # sub-talent clause lives in four of these seven.
        readme_marker = next(line for line in self.readme.split("\n")
                              if line.startswith("| **Mark A Running Toggle Skill**"))
        readme_guard = next(line for line in self.readme.split("\n")
                             if line.startswith("| **Stop Double Cast Re-casting A Toggle Skill**"))
        panel_marker = self.panel[self.panel.index("Mark a running toggle skill<br>"):]
        panel_marker = panel_marker[:panel_marker.index("</span></span>") + len("</span></span>")]
        panel_guard = self.panel[self.panel.index("Stop double cast re-casting a toggle skill<br>"):]
        panel_guard = panel_guard[:panel_guard.index("</span></span>") + len("</span></span>")]
        blocks = {
            "README marker": readme_marker, "README guard": readme_guard,
            "panel marker": panel_marker, "panel guard": panel_guard,
        }
        if self.release_notes is not None:
            notes_marker = self.release_notes[self.release_notes.index("- **Mark a running toggle skill"):]
            notes_marker = notes_marker[:notes_marker.index("\n- **", 5)]
            notes_guard = self.release_notes[self.release_notes.index("- **Stop double cast re-casting"):]
            notes_guard = notes_guard[:notes_guard.index("\n- **", 5)]
            intro = self.release_notes[:self.release_notes.index("## New")]
            blocks.update({"release notes marker": notes_marker, "release notes guard": notes_guard,
                           "intro": intro})
        return blocks

    def test_player_text_names_no_skill(self):
        pattern = self._forbidden_pattern()
        blocks = dict(self._countdown_text_blocks())
        blocks.update(self._toggle_text_blocks())
        for label, text in blocks.items():
            stripped = text.replace("’", "").replace("'", "")   # apostrophes removed first
            match = pattern.search(stripped)
            self.assertIsNone(match, (label, match.group(0) if match else None))

        # In-test controls: the matcher flags a synthetic string holding one
        # derived name, and passes one holding none.
        sample = sorted(self._forbidden_names())[0]
        words = self._words(sample)
        self.assertIsNotNone(pattern.search("This mentions " + " ".join(words) + " somewhere."))
        self.assertIsNone(pattern.search("This mentions nothing at all."))

    def test_toggle_texts_cover_a_toggle_on_its_own(self):
        # D-T1: the name-free clause the owner asked for, in exactly the four
        # blocks it belongs in - not the marker's own plain-cast sentences,
        # which stay true unreworded because Bushido has no plain cast.
        blocks = self._toggle_text_blocks()
        phrase = "toggle on its own"
        notes_gone = self.release_notes is None   # published notes leave main
        for label in ("README marker", "README guard", "panel guard", "release notes guard"):
            if notes_gone and label.startswith("release notes"):
                continue
            self.assertIn(phrase, " ".join(blocks[label].split()), label)
        self.assertNotIn("each with its toggle sub-talent allocated", blocks["README marker"])
        for label in ("panel marker", "release notes marker"):
            if notes_gone and label.startswith("release notes"):
                continue
            self.assertNotIn(phrase, " ".join(blocks[label].split()), label)

    def test_player_text_states_behaviour_without_overclaim(self):
        blocks = self._countdown_text_blocks()
        overclaim_skill = re.compile(r"\bany\s+(other\s+)?skill", re.IGNORECASE)
        overclaim_toggle = re.compile(r"\b(any|every|all)\s+(other\s+)?toggle\b", re.IGNORECASE)
        for label, text in blocks.items():
            low = text.lower()
            # The owner (2026-09-22): the panel says what the mod does for a
            # player, not how coverage was measured; the README and release
            # notes keep the full coverage account.
            words = ("most", "companion", "toggle") if label == "panel" else (
                "untested", "companion", "buff", "measure", "most")
            for word in words:
                self.assertIn(word, low, (label, word))
            self.assertIsNone(overclaim_skill.search(text), (label, text))
            self.assertIsNone(overclaim_toggle.search(text), (label, text))


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
        cls.research_doc = (FORGEPACT_DIR / "docs" / "toggle-skills-research.md").read_text(encoding="utf-8")
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

    def test_table_is_exactly_the_measured_rows(self):
        self.assertEqual(len(self.rows), len(SHIPPED_TABLE_ROWS), self.table)
        objects_hpp = (SDK_INCLUDE / "objects.hpp").read_text(encoding="utf-8")
        for row, want in zip(self.rows, SHIPPED_TABLE_ROWS):
            ability, sub, obj, own, mark, field, held = want
            self.assertEqual(row["ability"], ability, row)
            self.assertEqual(row["sub"], str(sub), row)
            self.assertEqual(row["obj"], obj, row)
            self.assertEqual(row["own"].strip(), own, row)
            self.assertEqual(row["mark"], mark, row)
            self.assertEqual(row["field"].strip(), field, row)
            self.assertEqual(float(row["held"]), held, row)
            self.assertRegex(objects_hpp, rf"\b{obj}\s*=\s*\d+,", row)

    def test_table_regex_parses_every_row(self):
        # An unwidened TABLE_ROW would silently drop a row (e.g. Bushido's
        # `kToggleNoSubTalent` sub-talent cell) from both self.rows and the
        # name-free test's forbidden-name set - this is the count check that
        # catches it.
        self.assertEqual(self.table.count('{ "'), len(self.rows), self.table)

    def test_blender_does_not_ship(self):
        # Blender's ON/OFF steps were never run (session 6); it stays out.
        for name in ('"blender"', "Butcher_Blender_obj"):
            self.assertNotIn(name, self.header, name)
            self.assertNotIn(name, self.stripped, name)
        # Counter's ON object is the buff instance (Draw_Player_Buff_obj,
        # session 12), never the session-6-rejected world object.
        self.assertNotIn("Shield_Lancer_Counter_World_obj", self.header)
        self.assertNotIn("Shield_Lancer_Counter_World_obj", self.stripped)

    def test_counter_toggle_row_agrees_with_the_buff_row(self):
        counter_toggle = next(r for r in self.rows if r["ability"] == "counter")
        self.assertEqual(counter_toggle["mark"], "PlayerBuff")
        self.assertIn(int(counter_toggle["sub"]), range(1, 15))

        skill_header = (FORGEPACT_DIR / "plugin" / "include" / "ForgePact" / "SkillTimerMod.hpp").read_text(
            encoding="utf-8")
        identity_field = re.search(
            r'inline constexpr const char\* kSkillTimerBuffIdentityField = "(\w+)";', skill_header)
        self.assertIsNotNone(identity_field)
        self.assertEqual(counter_toggle["field"].strip(), f'"{identity_field.group(1)}"')

        start = skill_header.index("inline constexpr SkillTimerBuffRow kSkillTimerBuffRows[] = {")
        buff_table = skill_header[start:skill_header.index("\n};", start)]
        buff_rows = {m.group("ability"): m for m in BUFF_ROW.finditer(buff_table)}
        self.assertIn("counter", buff_rows)
        self.assertEqual(float(counter_toggle["held"]), float(buff_rows["counter"].group("buffId")))

        # ToggleRowRequiresMark is unchanged - a PlayerBuff row requires its
        # mark the same as every other row (AC13/AC23).
        fn = function_body(self.header, "inline constexpr bool ToggleRowRequiresMark(")
        self.assertEqual(fn.strip(), "return row.mark != ToggleOnMark::None;")

    def test_table_carries_no_talent_id(self):
        # D-P1: ids move with every game build, so the table stores none and
        # the plugin spells none outside the research block. 224 and 134 are
        # session 9's measured ids (Meteor Storm, Bushido) - harness-only,
        # never in the shipped header.
        self.assertNotIn("talentId", self.table)
        self.assertIsNone(re.search(r"\b240\b", self.header))
        self.assertIsNone(re.search(r"\b224\b", self.header))
        self.assertIsNone(re.search(r"\b134\b", self.header))
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
        # Issue #55 follow-up (D-S4) made the walk due purely on the
        # once-per-room gate for a look - the rule map (built in the same
        # walk) has no "every row resolved" stopping signal of its own
        # (test_both_tables_resolve_in_one_walk). Round 1 restores the old
        # early-out for style Off only, since the rule map is never consulted
        # while off.
        self.assertIn("ToggleTableUnresolvedRows() + SkillTimerTableUnresolvedRows() == 0) return false;", due)
        self.assertIn("CurrentRoomKey()", due)
        self.assertIn("INT64_MIN", due)          # an unreadable room is never stored
        self.assertIn("g_ToggleResolveWalked = false;", due)
        self.assertIn("g_SkillTimerStyle.load() == ForgePact::SkillTimerStyle::Off", due)

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

    # ---- session 9: Meteor Storm and Bushido -------------------------------

    def test_every_toggle_row_is_measured_in_the_research_doc(self):
        doc = self.research_doc
        start = doc.index("### Toggle skill table")
        section = doc[start:doc.index("\n### ", start + 5)]
        lines = [line for line in section.splitlines() if line.startswith("|")]
        for row in self.rows:
            hits = [line for line in lines
                    if f"`{row['ability']}`)" in line and line.rstrip().endswith("| measured |")]
            self.assertTrue(hits, f"{row['ability']}: no `measured` Toggle skill table row")

    def test_base_form_rows_use_the_named_constant_and_every_other_row_a_real_slot(self):
        self.assertIn("inline constexpr int kToggleNoSubTalent = 0;", self.header)
        base_form = [row["ability"] for row in self.rows if row["sub"] == "kToggleNoSubTalent"]
        self.assertEqual(base_form, ["bushido"])
        for row in self.rows:
            if row["sub"] == "kToggleNoSubTalent":
                continue
            self.assertRegex(row["sub"], r"^(?:[1-9]|1[0-4])$", row)   # a real s01..s14 slot

    def test_marker_read_accepts_bool_and_positive_number(self):
        # Session 9's own claim ("the marker read already accepts bool"),
        # pinned on the source, not only on the border/meteor_storm_* scenarios
        # that run it end to end.
        truth = function_body(self.plugin, "static bool ToggleIndicatorReadTruth(")
        self.assertIn("VALUE_BOOL", truth)
        self.assertIn("v.ToDouble() > 0.0", truth)
        mark = function_body(self.plugin, "static void ToggleIndicatorCountMark(")
        self.assertIn("ToggleIndicatorReadTruth(v, marked)", mark)

    def test_toggle_count_in_player_output_follows_the_table(self):
        border = self.stripped[self.stripped.index('Out("toggleborder -> ON'):]
        border = border[:border.index(");")]
        guard = self.stripped[self.stripped.index('Out(std::string("toggleguard -> ")'):]
        guard = guard[:guard.index(");")]
        for name, text in (("toggleborder", border), ("toggleguard", guard)):
            self.assertIn("ForgePact::kToggleSkillRowCount", text, name)
            self.assertIsNone(re.search(r"\d", text), (name, text))
            self.assertNotIn("five", text.lower(), name)


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
        for key in ("refused=", "passed=", "procSeen=", "selfUnreadable=", "objUnresolved=", "baseForm=", "hook="):
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

    def test_guard_skips_the_sub_talent_read_for_a_base_form_row(self):
        # D-B1: a base-form row (Bushido) is decided WITHOUT ever calling
        # ToggleReadSubTalent - the base-form test runs first, inside the
        # same `if (refuseByCaller)` block ToggleReadSubTalent's own call
        # sits in, and short-circuits it.
        body = self.hook
        refuse = body.index("if (refuseByCaller) {")
        tail = body[refuse:]
        base = tail.index("ToggleRowIsBaseFormToggle(")
        read = tail.index("ToggleReadSubTalent(")
        self.assertLess(base, read)
        self.assertIn("if (baseForm) {", tail)
        self.assertLess(tail.index("if (baseForm) {"), read)
        self.assertIn("InterlockedIncrement(&g_TgdBaseForm);", tail)
        self.assertLess(tail.index("InterlockedIncrement(&g_TgdBaseForm);"), read)

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

# The one new line issue #55 (the timed-skill countdown) adds to
# Hook_DrawHudBuffs - directly after ToggleIndicatorDraw(), outside every
# research block, no new hook (context "Draw site, and the pins it moves").
SKILL_TIMER_DRAW_CALL_LINE = "    SkillTimerDraw();"

# The Miner's Helmet (1.4.5) draws its cosmetic pulse from the same callback,
# on the line straight after the countdown's; it is removed the same way.
MINER_HELMET_DRAW_CALL_LINE = "    ForgePact::MinerHelmet::Draw();"


def assert_hook_draw_hud_buffs_unchanged_plus_skilltimer(testcase, new_body, old_body):
    """NARROWED for issue #55, not deleted: `Hook_DrawHudBuffs` was the one
    body UNCHANGED_SINCE_T1 still pinned byte-for-byte. It now carries exactly
    one new statement - the countdown's own call, on its own line right after
    `ToggleIndicatorDraw();` - so the pin is narrowed the same way the table
    above narrows the other five: remove exactly that one line and assert
    what is left is still byte-identical to the round base."""
    lines = new_body.split("\n")
    testcase.assertEqual(lines.count(SKILL_TIMER_DRAW_CALL_LINE), 1, new_body)
    call_at = lines.index(SKILL_TIMER_DRAW_CALL_LINE)
    testcase.assertEqual(lines[call_at - 1].strip(), "ToggleIndicatorDraw();", new_body)
    testcase.assertEqual(lines.count(MINER_HELMET_DRAW_CALL_LINE), 1, new_body)
    testcase.assertEqual(lines[call_at + 1], MINER_HELMET_DRAW_CALL_LINE, new_body)
    del lines[call_at:call_at + 2]
    testcase.assertEqual("\n".join(lines), old_body)

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


class SkillTimerBuffContractTests(unittest.TestCase):
    """Buff-carried skill countdowns (issue #55, session 12): `kSkillTimerBuffRows`.

    Companion to test_toggle_skill_behavior.py's `buff/*` scenarios, which run
    the read/latch/toggle-suppression decisions end to end, and to
    ToggleSkillTableContractTests' `test_counter_toggle_row_agrees_with_the_buff_row`,
    which ties the one row shared between the two tables together. This class
    pins what only a source read can see: the buff table is disjoint from
    every other table, its own reader spells no runtime name outside the
    header, and Counter's Give No Quarter form is read at the point of use,
    never cached.
    """

    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_SRC.read_text(encoding="utf-8")
        cls.stripped = strip_research_blocks(cls.plugin)
        cls.header = (FORGEPACT_DIR / "plugin" / "include" / "ForgePact" / "SkillTimerMod.hpp").read_text(
            encoding="utf-8")
        cls.toggle_header = (FORGEPACT_DIR / "plugin" / "include" / "ForgePact" / "ToggleSkillMod.hpp").read_text(
            encoding="utf-8")
        cls.research_doc = (FORGEPACT_DIR / "docs" / "toggle-skills-research.md").read_text(encoding="utf-8")
        cls.release_notes = read_release_notes()
        cls.readme = (FORGEPACT_DIR / "README.md").read_text(encoding="utf-8")
        cls.panel = (SRC_DIR / "forgepact.py").read_text(encoding="utf-8")

        start = cls.header.index("inline constexpr SkillTimerBuffRow kSkillTimerBuffRows[] = {")
        cls.buff_table = cls.header[start:cls.header.index("\n};", start)]
        cls.buff_rows = [m.groupdict() for m in BUFF_ROW.finditer(cls.buff_table)]

        toggle_start = cls.toggle_header.index("inline constexpr ToggleSkillRow kToggleSkillRows[] = {")
        toggle_table = cls.toggle_header[toggle_start:cls.toggle_header.index("\n};", toggle_start)]
        cls.toggle_rows = [m.groupdict() for m in TABLE_ROW.finditer(toggle_table)]

    def _countdown_text_blocks(self):
        # Same three blocks SkillTimerRuleContractTests reads - duplicated
        # rather than imported across classes, the way this file already
        # duplicates small helpers (guide: match the file's own shape).
        readme_row = next(line for line in self.readme.split("\n")
                           if line.startswith("| **Timed skill countdown**"))
        panel = self.panel[self.panel.index("Timed skill countdown<br>"):]
        panel = panel[:panel.index("</span></span>") + len("</span></span>")]
        blocks = {"README": readme_row, "panel": panel}
        if self.release_notes is not None:
            release = self.release_notes[self.release_notes.index("**Timed skill countdown.**"):]
            cut = release.find("\n- **")
            if cut >= 0:
                release = release[:cut]
            blocks["release notes"] = release
        return blocks

    # ---- 1: every shipped row was measured (AC10) ---------------------------

    def test_every_buff_row_is_measured_in_the_research_doc(self):
        self.assertEqual(len(self.buff_rows), 4, self.buff_table)
        doc = self.research_doc
        section = doc[doc.index("### Buff-carried countdown (session 12)"):]
        results = section[section.index("#### Results"):section.index("#### Decision")]
        lines = [l for l in results.splitlines() if l.startswith("|")]
        for row in self.buff_rows:
            self.assertRegex(row["display"], r"^[^()]+\([^()]+\)$", row)
            found = [
                l for l in lines
                if f'`{row["ability"]}`' in l and f'| {row["buffId"]} |' in l
                and f'{float(row["first"]):.6f}' in l and "| ship |" in l
            ]
            self.assertEqual(len(found), 1, row)

    # ---- 2: the one row shared with the toggle table (AC13) -----------------

    def test_only_counter_is_in_both_tables_and_as_a_playerbuff_row(self):
        buff_ids = {r["ability"] for r in self.buff_rows}
        toggle_ids = {r["ability"] for r in self.toggle_rows}
        self.assertEqual(buff_ids & toggle_ids, {"counter"})
        playerbuff_rows = [r for r in self.toggle_rows if r["mark"] == "PlayerBuff"]
        self.assertEqual([r["ability"] for r in playerbuff_rows], ["counter"])

        deny_toggle_ids = {"bushido", "holyForm", "unholyForm", "melonForm"}
        self.assertFalse(buff_ids & deny_toggle_ids, buff_ids)
        self.assertNotIn("agility", buff_ids)
        for name in ("Turret", "Totem", "Hydra", "GameObject::"):
            self.assertNotIn(name, self.buff_table, name)

    # ---- 3: disjoint from the object rows and the rule ------------------------

    def test_buff_rows_disjoint_from_object_rows_and_never_enter_the_rule(self):
        fn = function_body(self.header, "inline bool SkillTimerRuleIsExplicitRow(")
        self.assertIn("kSkillTimerBuffRows[", fn)
        walk = function_body(self.stripped, "static bool ToggleTableResolveIds(")
        self.assertIn("SkillTimerRuleIsExplicitRow(name)", walk)
        deny_index = walk.index("SkillTimerRuleDenied(name)")
        explicit_index = walk.index("SkillTimerRuleIsExplicitRow(name)")
        self.assertLess(explicit_index, deny_index)
        buff_decl = self.header.index("inline constexpr SkillTimerBuffRow kSkillTimerBuffRows[] = {")
        explicit_fn = self.header.index("inline bool SkillTimerRuleIsExplicitRow(")
        self.assertLess(buff_decl, explicit_fn)

        twin = function_body(self.stripped, "static int SkillTimerToggleTwin(")
        row_for_talent = function_body(self.stripped, "static int ToggleTableRowForTalentId(")
        for name in ("kSkillTimerBuffRows", "SkillTimerBuffReadRow", "buffId"):
            self.assertNotIn(name, twin, name)
            self.assertNotIn(name, row_for_talent, name)

        countdown_start = self.header.index("inline constexpr SkillTimerRow kSkillTimerRows[] = {")
        countdown_table = self.header[countdown_start:self.header.index("\n};", countdown_start)]
        countdown_ids = {m.group("ability") for m in SkillTimerShipContractTests.COUNTDOWN_ROW.finditer(countdown_table)}
        buff_ids = {r["ability"] for r in self.buff_rows}
        self.assertFalse(countdown_ids & buff_ids, countdown_ids & buff_ids)

    # ---- 4: the reader spells no runtime name outside the header ------------

    def test_buff_read_uses_only_header_names_and_builtins(self):
        read = function_body(self.stripped, "static void SkillTimerBuffReadRow(")
        for literal in ('"playerBuff"', '"buffType"', '"destroyTimer"', '"host"'):
            self.assertNotIn(literal, read, literal)
        for name in ("kSkillTimerBuffArrayGlobal", "kSkillTimerBuffPlayerIndex", "kSkillTimerBuffSubIndex",
                     "kSkillTimerBuffIdentityField", "kSkillTimerField"):
            self.assertIn(name, read, name)
        builtins = set(re.findall(r'CallBuiltin\("(\w+)"', read))
        self.assertTrue(builtins)
        self.assertTrue(builtins <= {"variable_global_get", "array_get", "array_length",
                                      "instance_exists", "variable_instance_get"})
        for banned in ("CallBuiltinEx", "HhBuffAlive", "TgProbe", "MmCreateHook", "HookOneScript"):
            self.assertNotIn(banned, read, banned)
        self.assertIn("catch (...)", read)

    # ---- 5: the stat line (AC15) ---------------------------------------------

    def test_stat_prints_one_line_per_buff_row(self):
        stats = function_body(self.stripped, "static void SkillTimerStats(")
        object_loop = stats.index("SkillTimerRowCountersLine(r)")
        buff_lines = stats.index("SkillTimerBuffTableRowsLine()")
        buff_loop = stats.index("SkillTimerBuffRowCountersLine(r)")
        rule_line = stats.index("SkillTimerRuleCountersLine()")
        self.assertLess(object_loop, buff_lines)
        self.assertLess(buff_lines, buff_loop)
        self.assertLess(buff_loop, rule_line)
        counters = function_body(self.stripped, "static std::string SkillTimerBuffRowCountersLine(")
        for needle in (" drawn=", " noBuff=", " unreadable=", " identityMismatch=", " expired=",
                       " toggleOn=", " toggleUnreadable=", " unresolved=", " noSlot=", " latched=",
                       " unlatched="):
            self.assertIn(needle, counters, needle)

    # ---- 6: player text names the tier, never a skill (AC14) ----------------

    def test_player_text_says_measured_buff_skills_are_covered_without_names(self):
        old_clause = "only a buff on you are not covered"
        for label, text in self._countdown_text_blocks().items():
            normalised = " ".join(text.split())
            self.assertNotIn(old_clause, normalised, label)
            if label == "panel":
                # Short player-facing description (owner, 2026-09-22): no
                # coverage account, so no buff clause either.
                self.assertLessEqual(len(re.sub(r"<[^>]+>", "", text)), 300, text)
                continue
            low = text.lower()
            self.assertIn("buff", low, label)
            self.assertIn("covered", low, label)

        rule_tests = SkillTimerRuleContractTests()
        rule_tests.header = self.header
        rule_tests.toggle_header = self.toggle_header
        rule_tests.research_doc = self.research_doc
        forbidden = rule_tests._forbidden_names()
        for name in ("Counter", "Last Stand", "Defensive Shout", "Berserk"):
            self.assertIn(name, forbidden, name)

    # ---- 7: Counter's form is read at the point of use (D-P3/D-N-perm) ------

    def test_counter_form_is_the_sub_talent_read_at_the_point_of_use(self):
        # The not-falling guard replan 2 proposed is gone everywhere (AC23/
        # AC25 grep the header and the stripped plugin for its own name); this
        # test only pins what replaced it, never the dropped name itself.
        draw = function_body(self.stripped, "static void SkillTimerBuffDraw(")
        self.assertNotIn("frozen", draw.lower())
        counters_line = function_body(self.stripped, "static std::string SkillTimerBuffRowCountersLine(")
        self.assertNotIn("frozen", counters_line.lower())

        self.assertLess(draw.index("SkillTimerBuffReadRow("), draw.index("SkillTimerBuffToggleTwin("))
        colour_index = draw.index("draw_get_colour")
        self.assertLess(draw.index("c.toggleOn"), colour_index)
        self.assertLess(draw.index("c.toggleUnreadable"), colour_index)

        form = function_body(self.stripped, "static void ToggleIndicatorReadPlayerBuffForm(")
        self.assertEqual(form.count("ToggleReadSubTalent("), 1)
        self.assertNotIn("ToggleIndicatorModel::Decide", form)

        self.assertEqual(self.stripped.count("ToggleReadSubTalent("), 3)
        walk = function_body(self.stripped, "static bool ToggleTableResolveIds(")
        self.assertNotIn("ToggleReadSubTalent", walk)
        frame_callback = function_body(self.stripped, "void FrameCallback(")
        self.assertNotIn("ToggleReadSubTalent", frame_callback)

        read_row = function_body(self.stripped, "static ForgePact::ToggleIndicatorState ToggleIndicatorReadRow(")
        self.assertLess(read_row.index("ToggleOnMark::PlayerBuff"), read_row.index("ToggleIndicatorResolveRowObject("))

    # ---- 8: the object rows' own model is untouched (AC23) -------------------

    def test_object_row_model_is_unchanged_from_30a851c(self):
        old = git_show("30a851c:plugin/include/ForgePact/SkillTimerMod.hpp")
        if old is None:
            self.skipTest("30a851c is not readable here")
        sig = "class SkillTimerModel {"
        old_body = old[old.index(sig):old.index("\n};", old.index(sig))]
        new_body = self.header.replace("\r\n", "\n")
        new_body = new_body[new_body.index(sig):new_body.index("\n};", new_body.index(sig))]
        self.assertEqual(old_body, new_body)

    # ---- 9: one walk, both tables (T2) ----------------------------------------

    def test_buff_rows_resolve_in_the_same_walk_and_count_unresolved(self):
        walk = function_body(self.stripped, "static bool ToggleTableResolveIds(")
        self.assertIn("ForgePact::kSkillTimerBuffRows[r].abilityId", walk)
        self.assertIn("g_SkillTimerBuffTableIds.Set(r, id)", walk)
        self.assertEqual(walk.count('"ds_map_find_first"'), 1)
        unresolved = function_body(self.stripped, "static int SkillTimerTableUnresolvedRows(")
        self.assertIn("ForgePact::kSkillTimerBuffRowCount", unresolved)
        due = function_body(self.stripped, "static bool ToggleTableResolveDue(")
        self.assertIn("ToggleTableUnresolvedRows() + SkillTimerTableUnresolvedRows() == 0) return false;", due)
        draw = function_body(self.stripped, "static void SkillTimerBuffDraw(")
        self.assertIn("g_SkillTimerBuffTableIds.Get(", draw)
        self.assertIn("c.unresolved", draw)
        self.assertIn("< 0", draw)

    # ---- 10: the enable message counts both tables ----------------------------

    def test_enable_message_counts_both_tables(self):
        start = self.plugin.index('if (lc == "skilltimer")')
        end = self.plugin.index('if (lc == "toggleguard")', start)
        branch = self.plugin[start:end]
        self.assertIn("ForgePact::kSkillTimerRowCount + ForgePact::kSkillTimerBuffRowCount", branch)
        self.assertIn("timed skills", branch)
        self.assertNotRegex(branch, r"covers \d+ timed skills")


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
        self.assertIn("kTgTglRowCap", add)   # session 8: the row table's own cap
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
        # NARROWED for issue #55 (the timed-skill countdown), not deleted: see
        # assert_hook_draw_hud_buffs_unchanged_plus_skilltimer above.
        old = git_show("62a67d2:plugin/ModuleMain.cpp")
        if old is None:
            self.skipTest("git cannot read 62a67d2")
        for signature in UNCHANGED_SINCE_T1:
            new_body = function_body(self.plugin, signature)
            old_body = function_body(old, signature)
            if signature == "static RValue& Hook_DrawHudBuffs(":
                assert_hook_draw_hud_buffs_unchanged_plus_skilltimer(self, new_body, old_body)
            else:
                self.assertEqual(new_body, old_body, signature)

    def test_kplayercommands_unchanged_from_62a67d2(self):
        # NARROWED at the merge with main (2026-09-20): this phase still adds
        # no player command, but main's auto-prospect (ForgePact #9) landed in
        # the same table, so a byte-for-byte compare of the block now fails on
        # somebody else's entry. The property worth keeping is "this branch
        # added nothing here", so the compare is by SET with that one merged
        # entry named explicitly - anything else appearing still fails.
        old = git_show("62a67d2:plugin/ModuleMain.cpp")
        if old is None:
            self.skipTest("git cannot read 62a67d2")
        pattern = r"static const std::unordered_set<std::string> kPlayerCommands = \{(.*?)\};"
        as_set = lambda text: {tok.strip().strip('"') for tok in text.split(",") if tok.strip()}
        now = as_set(re.search(pattern, self.plugin, re.S).group(1))
        before = as_set(re.search(pattern, old, re.S).group(1))
        # `menulayout` is the read-only menu listing, another feature landing
        # in the same table (test_menu_layout_contract.py pins it), and
        # `restartanytime` is issue #8's (test_restart_anytime_contract.py).
        # `miningore`, `minerhelm` and `packmarks` are 1.4.5's mining slider,
        # Miner's Helmet and map pack markers (their own tests cover them), and
        # `craftmats` is issue #14's Craft from the stash
        # (test_craft_mats_contract.py), and `gemmythic`/`gemmaxroll` are the
        # Gems of Incarnation switches (test_incarnation_gems_contract.py).
        self.assertEqual(now - before, {"autoprospect", "skilltimer", "menulayout", "restartanytime",
                                        "miningore", "minerhelm", "packmarks", "craftmats",
                                        "gemmythic", "gemmaxroll", "gemfilter"})
        self.assertEqual(before - now, set())

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
        gallery_branch = sprite_gallery[sprite_gallery.index('lower == "gallery"'):sprite_gallery.index('lower == "gallery"') + 850]
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
        # Issue #55 follow-up: `style` gained an optional trailing
        # [talentId], so the style token is now the first of two, parsed
        # from `styleTok` rather than consuming the whole remainder as `v`.
        self.assertIn("TgProbeSpriteStyleFromName(Lower(styleTok), kind)", sprite)
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

    def test_alpha_arg_refuses_a_partially_parsed_or_non_finite_token(self):
        # F7, issue #55 follow-up: TgProbeSpriteParseAlphaArg fed both
        # `alpha` and `textalpha`, so routing it through the shared
        # ParseFiniteNumber fixes a typo like "0.5x" or "nan" reaching
        # draw_set_alpha for both commands at once.
        parse = function_body(self.plugin, "static bool TgProbeSpriteParseAlphaArg(")
        self.assertIn("ParseFiniteNumber(s, v)", parse)
        self.assertNotIn("std::stod(s)", parse)

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
            new_body = function_body(self.plugin, signature)
            old_body = function_body(old, signature)
            # NARROWED for issue #55, not deleted - same reasoning as
            # test_production_bodies_unchanged_from_62a67d2 above.
            if signature == "static RValue& Hook_DrawHudBuffs(":
                assert_hook_draw_hud_buffs_unchanged_plus_skilltimer(self, new_body, old_body)
            else:
                self.assertEqual(new_body, old_body, signature)
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
            new_body = function_body(self.plugin, signature)
            # NARROWED for session 8 (duration sweep), not deleted: `tgl add`
            # and `tgl list` now bound the row table by its own kTgTglRowCap
            # (DurationSweepProbeContractTests.test_tgl_row_cap_is_separate_
            # from_the_sub_walk_cap) - that one renamed bound is the only
            # change either body may carry.
            if signature in ("static void TgProbeTglAdd(", "static void TgProbeTglList("):
                new_body = new_body.replace("kTgTglRowCap", "kTgTglCap")
            self.assertEqual(new_body, function_body(old, signature), signature)
        # The `tgl` seed table itself, which carries every candidate row.
        for source in (self.plugin, old):
            self.assertIn("static const TgTglSeed kTgTglSeeds[] = {", source)
        start_new = self.plugin.index("static const TgTglSeed kTgTglSeeds[] = {")
        start_old = old.index("static const TgTglSeed kTgTglSeeds[] = {")
        self.assertEqual(self.plugin[start_new:self.plugin.index("};", start_new)].replace("\r\n", "\n"),
                         old[start_old:old.index("};", start_old)].replace("\r\n", "\n"))


# The six parents session 8's sweep enumerates (context "The instrument: sweep
# the parents, not a seed table"), in the order the sampler scans them: the
# damage parent first, the ability parent last, so an object under the sentry
# parent (itself an ability-parent child) is attributed to the sentry root.
SWEEP_ROOTS = (
    "Player_Damage_Parent_obj", "Skill_Controller_obj", "Player_Buff_Parent_obj",
    "Player_Curse_Parent_obj", "Player_Sentry_Parent_obj", "Player_Ability_Parent_obj",
)
# The class prefixes and the five non-damage roots the static table is drawn
# from (docs/toggle-skills-research.md, "#### Static candidates").
SWEEP_CLASS_PREFIXES = (
    "Amazon", "Bard", "Butcher", "Demon_Slayer", "Demonspawn", "Exo", "Illusionist", "Jotunn",
    "Marauder", "Marksman", "Necromancer", "Nomad", "Paladin", "Pirate", "Plague_Doctor", "Prophet",
    "Pyromancer", "Redneck", "Samurai", "Shaman", "Shield_Lancer", "Stormweaver", "Viking", "White_Mage",
)
SWEEP_STATIC_ROOTS = (
    "Player_Ability_Parent_obj", "Skill_Controller_obj", "Player_Sentry_Parent_obj",
    "Player_Buff_Parent_obj", "Player_Curse_Parent_obj",
)


class DurationSweepProbeContractTests(unittest.TestCase):
    """Session 8's research instrument (duration skills, phase R).

    `tgprobe sweep` reads `destroyTimer` on every descendant of six parents
    at once, one record per `object_index`, so one live session can say which
    class's timed skill carries a readable timer that spans its cast. Research
    build only; the companion `sweep/` scenarios in
    test_toggle_skill_behavior.py run its record update and sampler against a
    controlled runtime. `tgprobe talents dur` and the `tgl` row cap are the
    two companions the same build adds.
    """

    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_SRC.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.stripped = strip_research_blocks(cls.plugin)
        start = cls.plugin.index(BLOCK_START)
        cls.block = cls.plugin[start:cls.plugin.index(BLOCK_END, start)]
        cls.doc = (FORGEPACT_DIR / "docs" / "toggle-skills-research.md").read_text(
            encoding="utf-8").replace("\r\n", "\n")
        cls.sampler = function_body(cls.plugin, "static void TgProbeSweepAfterDraw()")
        cls.note = function_body(cls.plugin, "static void TgProbeSweepNote(")

    def section(self):
        start = self.doc.index("### Duration sweep (session 8): every class's timed skill")
        return self.doc[start:]

    def test_tgprobe_dispatches_sweep(self):
        body = function_body(self.plugin, "static void TgProbeCommand(const std::string& rest)")
        self.assertIn('sub == "sweep"', body)
        self.assertIn("TgProbeSweepCommand(subRest)", body)
        self.assertIn("sweep on|off|clear|show [seen|all]", body)
        command = function_body(self.plugin, "static void TgProbeSweepCommand(const std::string& rest)")
        for sub in ("on", "off", "clear", "show"):
            self.assertIn(f'sub == "{sub}"', command)
        self.assertIn("g_TgSweep.clear();", command[command.index('sub == "clear"'):])
        show = function_body(self.plugin, "static void TgProbeSweepShow(")
        for mode in ('"seen"', '"all"'):
            self.assertIn(mode, show)

    def test_sweep_roots_are_sdk_constants(self):
        start = self.plugin.index("static const HeroSiege::Objects::GameObject kTgSweepRoots[] = {")
        table = self.plugin[start:self.plugin.index("};", start)]
        roots = re.findall(r"HeroSiege::Objects::GameObject::(\w+)", table)
        self.assertEqual(tuple(roots), SWEEP_ROOTS)
        objects_hpp = (SDK_INCLUDE / "objects.hpp").read_text(encoding="utf-8")
        for root in roots:
            self.assertRegex(objects_hpp, rf"\b{root}\s*=\s*\d+,", root)
        # Resolved by name every draw, from the SDK constant's own name - no
        # literal object name and no index anywhere in the sampler.
        self.assertIn("HeroSiege::Objects::GetObjectName(kTgSweepRoots[r])", self.sampler)
        self.assertIn("TgProbeTglResolveObject(rootName, rootIdx)", self.sampler)
        for root in SWEEP_ROOTS:
            self.assertNotIn(f'"{root}"', self.block, root)
        self.assertNotRegex(self.sampler, r"RValue\(\d{3,}")

    def test_sweep_reads_object_index_timer_and_ownership_by_name(self):
        for needle in ('"instance_number"', '"instance_find"', '"object_index"', '"isMyClient"',
                       "ForgePact::kSkillTimerField", "N1ObjectIndex(oi, objIdx)",
                       "ToggleIndicatorReadTruth(mc, isMine)", "N1Numeric(tv)", "kTgSweepScanCap"):
            self.assertIn(needle, self.sampler, needle)
        # The VALUE_REF-aware predicate, never a VALUE_REAL-only kind check.
        self.assertNotIn("VALUE_REAL", self.sampler)
        for banned in ("CallBuiltinEx", "HhResolveLocalPlayer", "GetMembers(", '"playerNumber"'):
            self.assertNotIn(banned, self.sampler)
        # Per-root counts, merged by max: the sentry parent's children are the
        # ability parent's children too, so a sum would double them.
        self.assertIn("if (pr.second > o.inst) o.inst = pr.second;", self.sampler)
        self.assertIn("static constexpr long kTgSweepScanCap = 256;", self.plugin)
        show = function_body(self.plugin, "static void TgProbeSweepShow(")
        for needle in ('"object_get_name"', "HeroSiege::Objects::GetObjectName(", "NAME-MISMATCH",
                       "firstFrame", "roots=", "unresolved=", "capped=", "records=", "dropped=",
                       "indexUnreadable=", "app=", "draws=", "first=", "last=", "min=", "max=",
                       "timerUnreadable=", "maxInst=", "own=", "root=", "totalDraws="):
            self.assertIn(needle, show, needle)

    def test_sweep_off_by_default_and_gates_every_read(self):
        self.assertIn("static bool g_TgSweepOn = false;", self.plugin)
        self.assertTrue(self.sampler.strip().startswith("if (!g_TgSweepOn) return;"), self.sampler[:80])
        gate = self.sampler.index("if (!g_TgSweepOn) return;")
        self.assertLess(gate, self.sampler.index("CallBuiltin"))
        self.assertLess(gate, self.sampler.index("TgProbeTglResolveObject("))
        command = function_body(self.plugin, "static void TgProbeSweepCommand(const std::string& rest)")
        on = command[command.index('sub == "on"'):command.index('sub == "off"')]
        self.assertIn("g_TgSweepOn = true;", on)
        off = command[command.index('sub == "off"'):command.index('sub == "clear"')]
        self.assertIn("g_TgSweepOn = false;", off)
        # Hung off the existing research after-draw path, right after the
        # `tgl` sampler - never Hook_DrawHudBuffs or FrameCallback directly.
        after = function_body(self.plugin, "static void TgProbeSpurnAfterDraw()")
        self.assertLess(after.index("TgProbeTglAfterDraw();"), after.index("TgProbeSweepAfterDraw();"))
        self.assertNotIn("TgProbeSweep", function_body(self.plugin, "static RValue& Hook_DrawHudBuffs("))
        self.assertNotIn("TgProbeSweep", function_body(self.plugin, "void FrameCallback("))

    def test_sweep_unreadable_is_never_a_default(self):
        # A draw with instances but no numeric reading counts timerUnreadable
        # and returns before first/last/min/max are touched; `first` prints
        # `unreadable` until an appearance produces a number.
        unreadable = self.note.index("if (!obs->haveReading) { ++rec.timerUnreadable; return; }")
        self.assertLess(unreadable, self.note.index("rec.haveFirst = true;"))
        self.assertLess(unreadable, self.note.index("rec.last = v;"))
        show = function_body(self.plugin, "static void TgProbeSweepShow(")
        self.assertIn('rec.haveFirst ? TgProbeTglNumber(rec.first) : std::string("unreadable")', show)
        # A throw on the object_index read is counted, never taken as index 0.
        self.assertIn("++g_TgSweepIndexUnreadable", self.sampler)
        # Only numeric kinds are a timer reading; a string or bool is not.
        self.assertIn("if (N1Numeric(tv)) {", self.sampler)

    def test_sweep_is_absent_from_the_player_build(self):
        for name in ("TgProbeSweep", "TgSweep", "kTgSweep", "g_TgSweep", "sweep show"):
            self.assertIn(name, self.block, name)
            self.assertNotIn(name, self.stripped, name)
        # No new hook: the sweep only reads, from the existing draw path.
        for call in ("MmCreateHook(", "HookOneScript(", "HookOneScriptTable(", "InstallScriptHook("):
            self.assertNotIn(call, self.sampler)
            self.assertNotIn(call, self.note)
        self.assertEqual(self.block.count("MmCreateHook("), 1)

    def test_talents_dur_filter_prints_positive_durations_uncapped_at_40(self):
        body = function_body(self.plugin, "static void TgProbeTalentsCommand(")
        self.assertIn('const bool durMode = Lower(arg) == "dur";', body)
        self.assertIn("kDurShowCap = 400", body)
        self.assertIn("const long showCap = durMode ? kDurShowCap : kShowCap;", body)
        self.assertIn("if (shown < showCap) {", body)
        # The duration is read as a number on its own, never parsed out of a
        # printed field, and only a positive one is shown.
        dur = body[body.index("if (durMode) {"):]
        self.assertIn('RValue(kFields[2])', dur[:600])
        self.assertIn("N1Numeric(dv) && dv.ToDouble() > 0.0", dur[:600])
        self.assertIn("durTruncated=", body)
        self.assertIn("kShowCap = 40", body)

    def test_tgl_row_cap_is_separate_from_the_sub_walk_cap(self):
        self.assertIn("static constexpr int kTgTglCap = 16;", self.plugin)
        self.assertIn("static constexpr int kTgTglRowCap = 64;", self.plugin)
        add = function_body(self.plugin, "static void TgProbeTglAdd(")
        self.assertIn("(int)g_TgTgl.size() >= kTgTglRowCap", add)
        self.assertNotIn("kTgTglCap", add)
        self.assertIn("kTgTglRowCap", function_body(self.plugin, "static void TgProbeTglList()"))
        sub = function_body(self.plugin, "static void TgProbeTglSub()")
        self.assertIn("i < len && i < kTgTglCap", sub)
        self.assertNotIn("kTgTglRowCap", sub)

    def test_static_candidates_section_matches_the_sdk(self):
        from hs_game_sdk import GameObject, get_parent_index, NO_PARENT
        names = {o.value: o.name for o in GameObject}

        def ancestors(value):
            chain = []
            parent = get_parent_index(value)
            while parent not in (None, NO_PARENT) and parent >= 0:
                chain.append(names.get(parent, str(parent)))
                parent = get_parent_index(parent)
            return chain

        expected = [
            o for o in GameObject
            if any(o.name.startswith(prefix + "_") for prefix in SWEEP_CLASS_PREFIXES)
            and any(root in ancestors(o.value) for root in SWEEP_STATIC_ROOTS)
        ]
        section = self.section()
        static = section[section.index("#### Static candidates"):section.index("#### Instrument")]
        # Measured 2026-09-21 on this SDK; a regenerated SDK that moves the
        # count means the doc table needs regenerating too.
        self.assertEqual(len(expected), 207)
        listed = re.findall(r"^\| `(\w+)` \| (\d+) \|", static, re.M)
        self.assertEqual(sorted((n, int(i)) for n, i in listed),
                         sorted((o.name, o.value) for o in expected))
        self.assertIn("| `White_Mage_Healing_Zone_obj` | 5738 |", static)
        for heading in ("#### Static candidates", "#### Instrument", "#### Live procedure", "#### Results"):
            self.assertIn(heading, section)
        # The doc sits under Issue #55, after its own ### Decision.
        issue = self.doc.index("## Issue #55")
        self.assertLess(issue, self.doc.index("### Duration sweep (session 8)"))


class SkillTimerBuffProbeContractTests(unittest.TestCase):
    """Session 12's research instrument (buff-carried skills, phase R).

    `tgprobe buffwatch` walks global.playerBuff[1][0] the way `tgprobe buffs`
    (TgProbeBuffs) already does, one record per slot index, so a live session
    can measure which buff-carried skills draw a readable, own-attributed
    countdown that spans their cast. Research build only; the companion
    `buffwatch/` scenarios in test_toggle_skill_behavior.py run its record
    update and BuffAdd note against a controlled runtime.
    """

    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_SRC.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.stripped = strip_research_blocks(cls.plugin)
        start = cls.plugin.index(BLOCK_START)
        cls.block = cls.plugin[start:cls.plugin.index(BLOCK_END, start)]
        cls.doc = (FORGEPACT_DIR / "docs" / "toggle-skills-research.md").read_text(
            encoding="utf-8").replace("\r\n", "\n")

    def section(self):
        start = self.doc.index("### Buff-carried countdown (session 12)")
        return self.doc[start:]

    def test_tgprobe_dispatches_buffwatch(self):
        body = function_body(self.plugin, "static void TgProbeCommand(const std::string& rest)")
        self.assertIn('sub == "buffwatch"', body)
        self.assertIn("TgProbeBuffWatchCommand(subRest)", body)
        self.assertIn("buffwatch on|off|clear|show", body)
        command = function_body(self.plugin, "static void TgProbeBuffWatchCommand(const std::string& rest)")
        for sub in ("on", "off", "clear", "show"):
            self.assertIn(f'sub == "{sub}"', command)
        self.assertIn("g_TgBuffWatch.clear();", command[command.index('sub == "clear"'):])

    def test_buffwatch_symbols_are_research_only(self):
        for name in ("TgProbeBuffWatch", "TgBuffWatch", "g_TgBuffWatch", "buffwatch show",
                     "g_TgTalentUseDepth", "g_TgTalentUseClassDepth",
                     "TgProbeBuffWatchVisible", "TgProbeBuffWatchNoteText"):
            self.assertIn(name, self.block, name)
            self.assertNotIn(name, self.stripped, name)
        after_draw = function_body(self.plugin, "static void TgProbeBuffWatchAfterDraw()")
        for call in ("MmCreateHook(", "HookOneScript(", "HookOneScriptTable(", "InstallScriptHook("):
            self.assertNotIn(call, after_draw)
        self.assertEqual(self.block.count("MmCreateHook("), 1)

    def test_buffwatch_not_in_player_commands(self):
        match = re.search(r"static const std::unordered_set<std::string> kPlayerCommands = \{(.*?)\};",
                           self.plugin, re.S)
        self.assertIsNotNone(match, "kPlayerCommands not found")
        self.assertNotIn('"buffwatch"', match.group(1))
        self.assertNotIn('"tgprobe"', match.group(1))

    def test_sampler_called_next_to_sweep_no_new_hook(self):
        after = function_body(self.plugin, "static void TgProbeSpurnAfterDraw()")
        self.assertLess(after.index("TgProbeSweepAfterDraw();"), after.index("TgProbeBuffWatchAfterDraw();"))
        self.assertNotIn("TgProbeBuffWatch", function_body(self.plugin, "static RValue& Hook_DrawHudBuffs("))
        self.assertNotIn("TgProbeBuffWatch", function_body(self.plugin, "void FrameCallback("))

    def test_record_carries_the_measured_fields(self):
        rec = declaration_block(self.plugin, "struct TgBuffWatchRecord {")
        for field in ("present", "app", "draws", "firstFrame", "lastFrame", "first", "last",
                      "min", "max", "unreadable", "identityMismatch", "host", "vars",
                      "adds", "lastAddFrames", "lastAddPlayer", "inUse", "useTalent"):
            self.assertIn(field, rec, field)

    def test_buffadd_note_called_from_both_attachment_paths(self):
        note = function_body(
            self.plugin, "static void TgProbeNoteBuffAdd(CInstance* S, CInstance* O, int argc, RValue** A)")
        self.assertIn("TgProbeBuffWatchOnBuffAdd(argc, A,", note)
        detour = function_body(
            self.plugin,
            "static RValue& TgProbeDetourBody(int idx, CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)")
        self.assertIn("if (idx == kTg_BuffAdd) {", detour)
        self.assertIn("TgProbeBuffWatchOnBuffAdd(argc, A,", detour[detour.index("if (idx == kTg_BuffAdd) {"):])

    def test_talent_use_depth_kept_only_for_talentuse_and_talentuseclass(self):
        detour = function_body(
            self.plugin,
            "static RValue& TgProbeDetourBody(int idx, CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)")
        self.assertIn("if (idx == kTg_TalentUse) {", detour)
        self.assertIn("else if (idx == kTg_TalentUseClass) {", detour)
        # Every increment/decrement of the two depth counters appears exactly
        # once, inside those two branches - no other idx ever touches them.
        for stmt in ("InterlockedIncrement(&g_TgTalentUseDepth)", "InterlockedDecrement(&g_TgTalentUseDepth)",
                     "InterlockedIncrement(&g_TgTalentUseClassDepth)", "InterlockedDecrement(&g_TgTalentUseClassDepth)"):
            self.assertEqual(detour.count(stmt), 1, stmt)

    def test_doc_section_exists_with_live_procedure(self):
        section = self.section()
        for heading in ("#### Static search", "#### Instrument", "#### Live procedure", "#### Results", "#### Decision"):
            self.assertIn(heading, section)
        procedure = section[section.index("#### Live procedure"):section.index("#### Results")]
        for cmd in ("tgprobe hook TalentUse TalentUseClass BuffAdd BuffRemove DrawHudBuffs",
                    "tgprobe buffwatch on", "tgprobe buffwatch clear", "tgprobe buffwatch show",
                    "tgprobe deep find buff", "tgprobe deep get Player_obj.id"):
            self.assertIn(cmd, procedure, cmd)

    def test_buffwatch_command_usage_listed(self):
        usage = function_body(self.plugin, "static void TgProbeCommand(const std::string& rest)")
        self.assertIn("buffwatch on|off|clear|show", usage)

    # ---- Round 1 (owner-requested hardening, before the DLL is installed) --

    def test_show_prints_every_visible_shape_not_only_seen_present(self):
        # A slot whose buffType never matched its index (identityMismatch>0),
        # or an id only BuffAdd touched (adds>0) - both app==0 - are exactly
        # the shapes that would explain a failed `[104]` positive control,
        # and `show` used to skip both with a bare `rec.app <= 0` filter.
        show = function_body(self.plugin, "static void TgProbeBuffWatchShow()")
        self.assertNotIn("rec.app <= 0", show)
        self.assertIn("if (!TgProbeBuffWatchVisible(rec)) continue;", show)
        visible = function_body(self.plugin, "static bool TgProbeBuffWatchVisible(const TgBuffWatchRecord& rec)")
        self.assertIn("rec.app > 0", visible)
        self.assertIn("rec.identityMismatch > 0", visible)
        self.assertIn("rec.adds > 0", visible)
        note = function_body(self.plugin, "static std::string TgProbeBuffWatchNoteText(const TgBuffWatchRecord& rec)")
        self.assertIn("(mismatch-only)", note)
        self.assertIn("(added, never seen at this slot)", note)
        self.assertIn("TgProbeBuffWatchNoteText(rec)", show)

    def test_buffadd_note_skips_native_to_avoid_double_count(self):
        # TgProbeDetourBody's own kTg_BuffAdd branch already counts a native
        # (or table-only-native-under-HookBuffAdd) call; TgProbeNoteBuffAdd
        # runs unconditionally from inside the real, always-installed
        # HookBuffAdd, so without this guard both would fire for one call.
        note = function_body(
            self.plugin, "static void TgProbeNoteBuffAdd(CInstance* S, CInstance* O, int argc, RValue** A)")
        self.assertIn("if (g_TgRows[kTg_BuffAdd].mode != kTgNative) {", note)
        self.assertLess(note.index("g_TgRows[kTg_BuffAdd].mode != kTgNative"),
                         note.index("TgProbeBuffWatchOnBuffAdd(argc, A,"))
        # The comment claiming they never both fire is gone.
        self.assertNotIn("never both fire for one call", note)

    def test_live_procedure_has_a_second_positive_control_before_the_cast(self):
        procedure = self.section()
        procedure = procedure[procedure.index("#### Live procedure"):procedure.index("#### Results")]
        self.assertIn("tgprobe buffs", procedure)
        self.assertIn("blind", procedure)
        # The second control comes before the Counter cast control in the
        # numbered steps, not after.
        self.assertLess(procedure.index("tgprobe buffs"), procedure.index("cast Counter"))

    def test_static_search_labels_berserk_nesting_as_not_yet_observed(self):
        section = self.section()
        static_search = section[section.index("#### Static search"):section.index("#### Instrument")]
        self.assertNotIn("is never added inside a cast at all", static_search)
        self.assertIn("not observed yet", static_search)


class SkillTimerProbeContractTests(unittest.TestCase):
    """Issue #55, phase A: the research-only tick-rate readout and the
    fraction-driven countdown-look preview (`### What the probe round must
    add, and why it is one build and one session`). No player command, no
    new hook - only an extension of the existing `tgprobe talents`/
    `tgprobe sprite style` instrument, so route A can be falsified and every
    candidate look judged at rest, at any fill, with no live cast.
    """

    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_SRC.read_text(encoding="utf-8")
        cls.stripped = strip_research_blocks(cls.plugin)
        start = cls.plugin.index(BLOCK_START)
        cls.block = cls.plugin[start:cls.plugin.index(BLOCK_END, start)]
        cls.research_doc = (FORGEPACT_DIR / "docs" / "toggle-skills-research.md").read_text(encoding="utf-8")

    def test_research_doc_names_the_three_routes_the_rule_and_the_procedure(self):
        # A7: the issue-#55 section names the three candidate total sources,
        # the pre-committed decision rule, the live procedure as numbered
        # steps, and an empty results table with a status cell per probe row.
        doc = self.research_doc
        section = doc[doc.index("## Issue #55"):]
        self.assertIn("Route A", section)
        self.assertIn("Route C", section)
        self.assertIn("Route B", section)
        self.assertIn("Decision rule", section)
        procedure = section[section.index("### Live procedure"):section.index("### Results")]
        for n in ("1.", "2.", "3.", "4.", "5."):
            self.assertIn(f"\n{n}", procedure)
        results = section[section.index("### Results"):]
        for ability in ("soulSpurn", "lunarOrbit", "crematus", "counter",
                        "submergedKnives", "maelstromOfFrost", "blender"):
            self.assertIn(ability, results)
        for verdict in ("measured", "not observed", "blocked"):
            self.assertIn(verdict, results)

    def test_talents_reads_the_tick_rate_by_name_and_prints_it(self):
        body = function_body(self.plugin, "static void TgProbeTalentsCommand(")
        self.assertIn('"game_get_speed"', body)
        self.assertIn('RValue(0.0)', body[body.index('"game_get_speed"'):])
        self.assertIn("speed=", body)
        self.assertIn("fps=", body)
        # `fps` read the same way this file's own established positive
        # control does (~line 20360): GetBuiltin, not CallBuiltin.
        self.assertIn('GetBuiltin("fps", nullptr, NULL_INDEX, v)', body)
        # predictedTotal (abilityDuration x speed) sits on the same printed
        # line as abilityDuration, gated on the read succeeding.
        self.assertIn("predictedTotal=", body)
        self.assertIn("speedOk", body)

    def test_talents_speed_read_matches_the_shipped_headhunter_shape(self):
        # Same call and the same "non-positive means unreadable" floor
        # ModuleMain.cpp's Headhunter buff-duration conversion already uses
        # (game_get_speed(0.0) -> seconds-to-frames), so route A's session
        # is testing the runtime's own unit relationship, not a guess.
        body = function_body(self.plugin, "static void TgProbeTalentsCommand(")
        speed_read = body[body.index('speed = g_Yytk->CallBuiltin("game_get_speed"'):]
        self.assertIn("speedOk = speed > 0.0;", speed_read[:200])

    def test_no_new_hook_is_installed_by_this_round(self):
        # A3: this round adds no MmCreateHook/HookOneScript/
        # HookOneScriptTable/InstallScriptHook call anywhere - it only reads
        # existing builtins by name and extends the sprite-style draw probe.
        for call in ("MmCreateHook(", "HookOneScript(", "HookOneScriptTable(", "InstallScriptHook("):
            self.assertNotIn(call, function_body(self.plugin, "static void TgProbeTalentsCommand("))
            self.assertNotIn(call, function_body(self.plugin, "static void TgProbeSpriteCommand(const std::string& rest)"))
        # The block's one resolver-installed detour count is unchanged.
        self.assertEqual(self.block.count("MmCreateHook("), 1)

    def test_countdown_styles_are_dispatched_and_use_the_fraction_knob(self):
        sprite = function_body(self.plugin, "static void TgProbeSpriteCommand(const std::string& rest)")
        self.assertIn('lower == "frac"', sprite)
        from_name = function_body(self.plugin, "static bool TgProbeSpriteStyleFromName(")
        for name in ("arc", "bar", "number", "fade"):
            self.assertIn(f'lower == "{name}"', from_name)
        style = function_body(self.plugin, "static void TgProbeSpriteDrawStyle(")
        for kind in ("Arc", "Bar", "Number", "Fade"):
            self.assertIn(f"TgSpriteStyleKind::{kind}", style)
        for fn in ("TgProbeSpriteDrawArc", "TgProbeSpriteDrawBar", "TgProbeSpriteDrawNumber", "TgProbeSpriteDrawFade"):
            self.assertIn("g_TgSpriteFraction", function_body(self.plugin, f"static void {fn}("))
        # `fade` reuses the shipped-look-pinned soft draw rather than
        # duplicating its bands (UNCHANGED_PROBE_BODIES stays meaningful).
        self.assertIn("TgProbeSpriteDrawSoft(x, y, w, h, g_TgSpriteFraction)",
                       function_body(self.plugin, "static void TgProbeSpriteDrawFade("))

    def test_fraction_defaults_to_full_and_is_clamped(self):
        self.assertIn("static double g_TgSpriteFraction = 1.0;", self.plugin)
        frac_branch = function_body(self.plugin, "static void TgProbeSpriteCommand(const std::string& rest)")
        frac_branch = frac_branch[frac_branch.index('lower == "frac"'):]
        self.assertIn("if (f < 0.0) f = 0.0;", frac_branch)
        self.assertIn("if (f > 1.0) f = 1.0;", frac_branch)

    def test_new_symbols_are_research_only_names_do_not_survive_stripping(self):
        for name in ("TgProbeSpriteDrawArc", "TgProbeSpriteDrawBar", "TgProbeSpriteDrawNumber",
                     "TgProbeSpriteDrawFade", "TgProbeSpriteDrawRectOutlineFraction", "TgProbeSpriteFracText",
                     "g_TgSpriteFraction", "TgSpriteStyleKind::Arc", "TgSpriteStyleKind::Bar",
                     "TgSpriteStyleKind::Number", "TgSpriteStyleKind::Fade",
                     # issue #55 follow-up: text placement/style controls and font list
                     "g_TgSpriteTextOffsetDx", "g_TgSpriteTextOffsetDy", "g_TgSpriteTextAlpha",
                     "g_TgSpriteTextColourSet", "g_TgSpriteTextColourName", "g_TgSpriteFontName",
                     "g_TgSpriteTextFontUnresolved", "g_TgSpriteTextDrawExc",
                     "TgProbeSpriteTextColour", "TgProbeSpriteTextColourText", "TgProbeSpriteTextAlphaText",
                     "TgProbeSpriteTextOffsetText", "TgProbeSpriteFontText", "TgProbeSpriteBuiltinExists",
                     "TgProbeSpriteFontListCommand", "kTgSpriteFontFallbackNames", "kTgSpriteFontEnumCap"):
            self.assertIn(name, self.block, name)
            self.assertNotIn(name, self.stripped, name)
        # The tick-rate readout is inside the same research-only command as
        # the rest of `tgprobe talents`, so it is covered by the same guard.
        self.assertIn("TgProbeTalentsCommand", self.block)
        self.assertNotIn("TgProbeTalentsCommand", self.stripped)

    # ---- issue #55 follow-up: text placement, style controls, four fixes ---
    # (live-session capture .claude/workorders/issue-55-live-session-2026-09-20-capture.md)

    def test_number_anchors_below_the_box_not_at_its_centre(self):
        body = function_body(self.plugin, "static void TgProbeSpriteDrawNumber(")
        self.assertNotIn("y + h / 2.0", body)
        self.assertIn("y + h + g_TgSpriteTextOffsetDy", body)
        self.assertIn("x + w / 2.0 + g_TgSpriteTextOffsetDx", body)

    def test_number_saves_everything_before_its_own_inner_try_and_restores_each_alone(self):
        body = function_body(self.plugin, "static void TgProbeSpriteDrawNumber(")
        save_order = ['"draw_get_font"', '"draw_get_colour"', '"draw_get_alpha"', '"draw_get_halign"', '"draw_get_valign"']
        positions = [body.index(name) for name in save_order]
        self.assertEqual(positions, sorted(positions), "all five must be captured, in order, before anything is set")
        inner_try = body.index("try {", positions[-1])
        self.assertLess(positions[-1], inner_try, "every save must precede the inner try around the draw")
        draw_text_index = body.index('"draw_text"')
        self.assertGreater(draw_text_index, inner_try)
        for restore in (
            'try { g_Yytk->CallBuiltin("draw_set_valign", { prevValign }); } catch (...) {}',
            'try { g_Yytk->CallBuiltin("draw_set_halign", { prevHalign }); } catch (...) {}',
            'try { g_Yytk->CallBuiltin("draw_set_alpha", { prevAlpha }); } catch (...) {}',
            'try { g_Yytk->CallBuiltin("draw_set_colour", { prevColour }); } catch (...) {}',
            'try { g_Yytk->CallBuiltin("draw_set_font", { prevFont }); } catch (...) {}',
        ):
            self.assertIn(restore, body, restore)
            self.assertGreater(body.index(restore), draw_text_index, restore)

    def test_number_resolves_its_font_by_name_and_applies_it_only_when_nonnegative(self):
        body = function_body(self.plugin, "static void TgProbeSpriteDrawNumber(")
        self.assertIn('CallBuiltin("asset_get_index", { RValue(g_TgSpriteFontName) })', body)
        self.assertIn("if (f.ToDouble() < 0) f = RValue(std::stod(g_TgSpriteFontName));", body)
        self.assertIn(
            'if (f.ToDouble() >= 0) { fontApplied = true; g_Yytk->CallBuiltin("draw_set_font", { f }); }', body)
        self.assertIn("g_TgSpriteTextFontUnresolved", body)

    def test_number_only_restores_the_font_when_it_actually_applied_one(self):
        # F3, issue #55 follow-up: draw_set_font(prevFont) must not run
        # unconditionally - the default path (empty g_TgSpriteFontName)
        # never calls draw_set_font in the first place, so restoring
        # whatever draw_get_font happened to return would push a value that
        # was only ever read, never confirmed real, into the runtime's font
        # state on every draw asking for no font change.
        body = function_body(self.plugin, "static void TgProbeSpriteDrawNumber(")
        self.assertIn("bool fontApplied = false;", body)
        self.assertIn(
            'if (fontApplied) { try { g_Yytk->CallBuiltin("draw_set_font", { prevFont }); } catch (...) {} }', body)

    def test_number_marks_font_applied_before_calling_draw_set_font(self):
        # F3, issue #55 follow-up round 2 (forgepact-tgprobe-font-instrument):
        # if draw_set_font applies the font and then throws, fontApplied
        # must already be true so the restore above still runs - otherwise
        # the probe's font leaks into the game for the rest of the session.
        # Setting the flag first costs nothing on the opposite failure path
        # (the call throws before applying anything): the restore then
        # writes prevFont, the game's own font read at the top of this
        # body, which is a no-op write.
        body = function_body(self.plugin, "static void TgProbeSpriteDrawNumber(")
        self.assertIn(
            'if (f.ToDouble() >= 0) { fontApplied = true; g_Yytk->CallBuiltin("draw_set_font", { f }); }', body)
        self.assertNotIn(
            'if (f.ToDouble() >= 0) { g_Yytk->CallBuiltin("draw_set_font", { f }); fontApplied = true; }', body)

    def test_font_list_checks_each_builtin_through_callbuiltinex_and_prints_the_positive_control(self):
        body = function_body(self.plugin, "static void TgProbeSpriteFontListCommand()")
        for name in ("draw_get_font", "font_exists", "font_get_name"):
            self.assertIn(f'"{name}"', body, name)
        self.assertIn("TgProbeSpriteBuiltinExists(", body)
        exists_body = function_body(self.plugin, "static bool TgProbeSpriteBuiltinExists(")
        self.assertIn("CallBuiltinEx(", exists_body)
        self.assertIn("AurieSuccess(st)", exists_body)
        # the positive control itself: the already-proven CallBuiltin path
        # (HhDrawHeadLabels calls it every draw), and the default-font
        # sentinel handled distinctly from a real index.
        self.assertIn('CallBuiltin("draw_get_font", {})', body)
        self.assertIn("active font:", body)
        self.assertIn("default (draw_get_font=", body)
        # an empty enumeration is told apart from a missing enumerator.
        self.assertIn("enumeration not run: font_exists is not present", body)
        self.assertIn("found", body)

    def test_font_list_probes_font_get_name_only_against_a_confirmed_index(self):
        # F4, issue #55 follow-up: font_get_name (a lookup, unlike the
        # exists-check font_exists) must never be probed with the
        # unconfirmed literal 0.0 - only the active font when it is not the
        # default sentinel, else the first font_exists-confirmed index, and
        # "not probed" when neither is available.
        body = function_body(self.plugin, "static void TgProbeSpriteFontListCommand()")
        self.assertNotIn('TgProbeSpriteBuiltinExists("font_get_name", g, g, { RValue(0.0) }', body)
        self.assertIn(
            'TgProbeSpriteBuiltinExists("font_get_name", g, g, { RValue(probeIdx) }, existsRes);', body)
        self.assertIn("not probed (no confirmed font index available)", body)
        # the active font is read before font_get_name is probed at all.
        get_font_idx = body.index('CallBuiltin("draw_get_font", {})')
        probe_call = body.index('TgProbeSpriteBuiltinExists("font_get_name"')
        self.assertLess(get_font_idx, probe_call)

    def test_font_list_reads_the_active_font_only_when_draw_get_font_is_present(self):
        # F4, issue #55 follow-up round 2 (forgepact-tgprobe-font-instrument):
        # CallBuiltin returns an unset RValue (ToDouble()==0.0) for a
        # missing builtin instead of throwing, so an unguarded read would
        # fabricate activeIdx=0.0 as the font probe's own positive control.
        # Gate the read itself on hasDrawGetFont so the confirmedIdx/
        # "not probed" fallbacks run instead, and the "active font:" line
        # must then say the builtin is absent rather than that it threw.
        body = function_body(self.plugin, "static void TgProbeSpriteFontListCommand()")
        self.assertIn(
            'if (hasDrawGetFont) { try { activeIdx = g_Yytk->CallBuiltin("draw_get_font", {}).ToDouble();'
            ' activeIdxKnown = true; } catch (...) {} }', body)
        self.assertIn('if (!hasDrawGetFont) Out("  active font: draw_get_font not present");', body)
        self.assertIn('else if (!activeIdxKnown) Out("  active font: draw_get_font threw");', body)
        # the existence-check Out(...) line (the printed negative control)
        # is not itself the guard - it prints unconditionally, before the
        # real gate on the read.
        self.assertLess(body.index('Out(std::string("  draw_get_font: ")'),
                         body.index('if (hasDrawGetFont) { try {'))

    def test_font_list_disclaims_the_fallback_candidates_as_unconfirmed(self):
        # F5, issue #55 follow-up: a run where every _fnt-suffixed candidate
        # prints "unresolved" must be readable as "the guess missed," never
        # as a measured statement that the runtime has no fonts.
        body = function_body(self.plugin, "static void TgProbeSpriteFontListCommand()")
        disclaimer = body.index("none is confirmed to exist")
        first_candidate = body.index('std::string("  candidate ") + name +')
        self.assertLess(disclaimer, first_candidate)

    def test_style_parses_an_optional_trailing_talentid(self):
        body = function_body(self.plugin, "static void TgProbeSpriteCommand(const std::string& rest)")
        style_branch = body[body.index('lower == "style"'):body.index('lower == "gallery"')]
        self.assertIn("FirstToken(rest2, ignored)", style_branch)
        self.assertIn("int talentId = kToggleIndicatorTalentId;", style_branch)
        self.assertIn("g_TgSpriteTalentId = talentId;", style_branch)
        self.assertIn('" talentId=" + std::to_string(g_TgSpriteTalentId)', style_branch)
        # F6, issue #55 follow-up: parsed through the shared ParseFiniteNumber
        # and refused by name, not a bare std::stoi that silently substituted
        # kToggleIndicatorTalentId on any parse failure - including a partial
        # token like "24o", which std::stoi itself would accept as 24.
        self.assertIn("ParseFiniteNumber(talentTok, f)", style_branch)
        self.assertNotIn("std::stoi(talentTok)", style_branch)
        usage = style_branch.index("did not parse as a number")
        stored = style_branch.index("g_TgSpriteTalentId = talentId;")
        self.assertLess(usage, stored)

    def test_style_refuses_an_unparseable_talentid_naming_the_token(self):
        body = function_body(self.plugin, "static void TgProbeSpriteCommand(const std::string& rest)")
        style_branch = body[body.index('lower == "style"'):body.index('lower == "gallery"')]
        self.assertIn('talentTok + "\\" did not parse as a number)"', style_branch)
        self.assertIn('[talentId] (\\""', style_branch)
        self.assertIn("return;", style_branch[style_branch.index("did not parse as a number"):])

    def test_bar_returns_without_drawing_below_one_pixel_of_width(self):
        body = function_body(self.plugin, "static void TgProbeSpriteDrawBar(")
        # width after `barinset` trims both sides (2026-09-21 live session)
        self.assertIn("const double barWidth = usableWidth * fraction;", body)
        self.assertIn("if (barWidth < 1.0) return;", body)
        # the guard is on the drawn WIDTH, never on a bare fraction==0.0
        # equality check - a sub-pixel remainder must disappear too (D4).
        self.assertNotIn("if (fraction == 0.0)", body)
        self.assertNotIn("if (g_TgSpriteFraction == 0.0)", body)

    def test_frac_refuses_a_partially_parsed_or_non_finite_token(self):
        body = function_body(self.plugin, "static void TgProbeSpriteCommand(const std::string& rest)")
        frac_branch = body[body.index('lower == "frac"'):body.index('lower == "textoffset"')]
        self.assertIn("ParseFiniteNumber(v, f)", frac_branch)
        self.assertNotIn("std::stod(v)", frac_branch)
        # ParseFiniteNumber is the shared, already-shipped helper (the
        # custom-forge selector parser) that requires the numeric prefix to
        # cover the whole token and rejects a non-finite result.
        parse_body = function_body(self.plugin, "static bool ParseFiniteNumber(")
        self.assertIn("used == clean.size()", parse_body)
        self.assertIn("std::isfinite(out)", parse_body)

    def test_text_controls_do_not_change_any_other_candidates_draw(self):
        # D3: textoffset/textalpha/textcolour/font only ever feed
        # TgProbeSpriteDrawNumber - none of the other style bodies reference
        # any of the four new state variables.
        for fn in ("TgProbeSpriteDrawSoft(", "TgProbeSpriteDrawHalo(", "TgProbeSpriteDrawGradient(",
                   "TgProbeSpriteDrawPulse(", "TgProbeSpriteDrawArc(", "TgProbeSpriteDrawBar(",
                   "TgProbeSpriteDrawFade(", "TgProbeSpriteDrawGoldRect(", "TgProbeSpriteDrawOne("):
            body = function_body(self.plugin, f"static void {fn}")
            for name in ("g_TgSpriteTextOffsetDx", "g_TgSpriteTextAlpha", "g_TgSpriteTextColourSet", "g_TgSpriteFontName"):
                self.assertNotIn(name, body, f"{fn} must not reference {name}")

    def test_every_selection_resets_the_text_counters_alongside_draws(self):
        # F2, issue #55 follow-up: g_TgSpriteTextDrawExc/g_TgSpriteTextFontUnresolved
        # must be zeroed everywhere g_TgSpriteDraws/g_TgSpriteDrawExc already
        # are (gold/style/gallery/named-sprite selection) - otherwise a
        # second `style number` run's `off` line reports a session-cumulative
        # textDrawExc=/unresolved= next to a fresh per-run draws=/drawExc=.
        body = function_body(self.plugin, "static void TgProbeSpriteCommand(const std::string& rest)")
        draws_resets = body.count("InterlockedExchange(&g_TgSpriteDraws, 0);")
        drawexc_resets = body.count("InterlockedExchange(&g_TgSpriteDrawExc, 0);")
        text_drawexc_resets = body.count("InterlockedExchange(&g_TgSpriteTextDrawExc, 0);")
        text_unresolved_resets = body.count("InterlockedExchange(&g_TgSpriteTextFontUnresolved, 0);")
        self.assertGreaterEqual(draws_resets, 4)
        self.assertEqual(draws_resets, drawexc_resets)
        self.assertEqual(draws_resets, text_drawexc_resets)
        self.assertEqual(draws_resets, text_unresolved_resets)

    # ---- the route-A decision rule rewrite (exhaustive state tables) -------

    def _decision_rule_section(self):
        doc = self.research_doc
        return doc[doc.index("### Decision rule"):doc.index("### The look, judged live")]

    def test_table_parser_rejects_a_malformed_table(self):
        # Negative control for parse_doc_table itself: a caption followed by
        # a header-shaped line but no real `|---|` separator must not be
        # silently accepted as a table.
        synthetic = (
            "**Table X: synthetic**\n\n"
            "| id | condition | status |\n"
            "| CR1 | bogus row with no separator above it | blocked |\n"
        )
        with self.assertRaises(ValueError):
            parse_doc_table(synthetic, "Table X: synthetic")

    def test_decision_rule_tables_carry_exactly_the_declared_ids_once_each(self):
        # AC6: the five tables, located by caption, carry exactly CR1-CR3,
        # CS1-CS4, AR1-AR7, AS1-AS5 and S1-S9, each id exactly once.
        section = self._decision_rule_section()
        expected = {
            "Table 1: route C, per row": [f"CR{n}" for n in range(1, 4)],
            "Table 2: route C status": [f"CS{n}" for n in range(1, 5)],
            "Table 3: route A, per row": [f"AR{n}" for n in range(1, 8)],
            "Table 4: route A status": [f"AS{n}" for n in range(1, 6)],
            "Table 5: selection": [f"S{n}" for n in range(1, 10)],
        }
        all_ids = []
        for caption, ids in expected.items():
            rows = parse_doc_table(section, caption)
            actual_ids = [row[0] for row in rows]
            self.assertEqual(sorted(actual_ids), sorted(ids), caption)
            self.assertEqual(len(actual_ids), len(set(actual_ids)), caption)
            all_ids.extend(actual_ids)
        self.assertEqual(len(all_ids), len(set(all_ids)), "an id is reused across tables")

    def test_selection_table_covers_every_status_pair_exactly_once(self):
        # AC7: the nine rows of Table 5 are the nine ordered pairs of
        # {measured, not observed, blocked} x itself, each exactly once.
        section = self._decision_rule_section()
        rows = parse_doc_table(section, "Table 5: selection")
        statuses = ("measured", "not observed", "blocked")
        expected_pairs = sorted((c, a) for c in statuses for a in statuses)
        actual_pairs = sorted((row[1], row[2]) for row in rows)
        self.assertEqual(actual_pairs, expected_pairs)

    def test_every_status_and_selection_cell_is_from_the_declared_vocabulary(self):
        # AC8: every route-status cell is measured/not observed/blocked, and
        # every selected-route cell is route C/route A/route B/none - session
        # repeats.
        section = self._decision_rule_section()
        statuses = {"measured", "not observed", "blocked"}
        selections = {"route C", "route A", "route B", "none - session repeats"}
        for caption in ("Table 1: route C, per row", "Table 2: route C status",
                        "Table 3: route A, per row", "Table 4: route A status"):
            for row in parse_doc_table(section, caption):
                self.assertIn(row[-1], statuses, (caption, row))
        for row in parse_doc_table(section, "Table 5: selection"):
            self.assertIn(row[1], statuses, row)
            self.assertIn(row[2], statuses, row)
            self.assertIn(row[3], selections, row)

    def test_blocked_falls_through_to_none_and_route_b_is_the_double_not_observed_row(self):
        # AC9: every row with a `blocked` status cell and no `measured`
        # status cell selects `none - session repeats`; exactly one row
        # selects `route B`, and it is the row where both status cells read
        # `not observed`.
        section = self._decision_rule_section()
        rows = parse_doc_table(section, "Table 5: selection")
        route_b_pairs = []
        for row_id, c_status, a_status, selected, _record in rows:
            if "blocked" in (c_status, a_status) and "measured" not in (c_status, a_status):
                self.assertEqual(selected, "none - session repeats", row_id)
            if selected == "route B":
                route_b_pairs.append((c_status, a_status))
        self.assertEqual(route_b_pairs, [("not observed", "not observed")])

    def test_speed_is_pinned_as_a_non_input_and_absent_from_every_table_cell(self):
        # AC10: the "An unreadable speed=..." sentence is pinned verbatim
        # (whitespace-normalised, since this doc hand-wraps prose), and the
        # `speed=` token appears in no cell of any of the five tables.
        section = self._decision_rule_section()
        self.assertIn(
            "An unreadable `speed=` changes no cell in any table above.",
            collapse(section),
        )
        for caption in ("Table 1: route C, per row", "Table 2: route C status",
                        "Table 3: route A, per row", "Table 4: route A status",
                        "Table 5: selection"):
            for row in parse_doc_table(section, caption):
                for cell in row:
                    self.assertNotIn("speed=", cell, (caption, row))

    def test_live_procedure_step_2_and_results_header_carry_the_new_inputs(self):
        # AC11: step 2 names a second cast and the `appearance=` readout,
        # and the Results per-row header names the new columns the rule
        # consumes.
        doc = self.research_doc
        issue_section = doc[doc.index("## Issue #55"):]
        procedure = issue_section[issue_section.index("### Live procedure"):issue_section.index("### Results")]
        step2 = procedure[procedure.index("\n2."):procedure.index("\n3.")]
        self.assertIn("second", step2)
        self.assertIn("appearance=", step2)
        results = issue_section[issue_section.index("### Results"):]
        header_line = next(
            line for line in results.splitlines() if line.strip().startswith("| abilityId")
        )
        for token in ("first= #1", "first= #2", "ratio", "factor", "overCap=",
                      "route A row status", "route C row status"):
            self.assertIn(token, header_line, token)

    def test_ar2_and_ar3_partition_abilityDuration_and_first_at_the_zero_bound(self):
        # Regression for the instrument-blindness BLOCKING finding closed
        # this round: a row whose first draw reads the probe's own -1/0
        # sentinel (timer never started) must never enter the ratio/base/
        # factor computation. AR2 must own the <= 0 side of
        # `abilityDuration=` and AR3 the > 0 side of `first=`, so the two
        # conditions partition the number line instead of leaving a state
        # (e.g. `first=-1`, `abilityDuration>0`) that maps to no row.
        section = self._decision_rule_section()
        rows = {row[0]: row for row in parse_doc_table(section, "Table 3: route A, per row")}
        ar2_condition = collapse(rows["AR2"][1])
        self.assertIn("0", ar2_condition)
        self.assertIn("negative", ar2_condition, "AR2 must also cover a negative abilityDuration=")
        for row_id in ("AR3", "AR4"):
            condition = collapse(rows[row_id][1])
            self.assertIn(
                "greater than `0`" if row_id == "AR3" else "> 0",
                condition,
                f"{row_id} must require a positive first=, not merely a numeric one",
            )
        # The definitions block states the same bound in prose, so a reader
        # of the definitions and a reader of the table land on the same
        # partition.
        self.assertIn(
            "defined only for rows whose `abilityDuration > 0` and whose "
            "`first=` read numeric and greater than `0`",
            collapse(section),
        )

    # ---- issue #55 timer-countdown follow-up: `frac anim` -----------------
    # (workorder .claude/workorders/forgepact-tgprobe-frac-anim-plan.md)

    def test_frac_anim_parses_its_arguments_and_refuses_bad_ones(self):
        body = function_body(self.plugin, "static void TgProbeSpriteCommand(const std::string& rest)")
        frac_branch = body[body.index('lower == "frac"'):body.index('lower == "textoffset"')]
        self.assertIn("TgProbeSpriteFracAnimCommand(", frac_branch)
        self.assertLess(
            frac_branch.index("TgProbeSpriteFracAnimCommand("),
            frac_branch.index("ParseFiniteNumber(v, f)"),
            "the anim dispatch must be reached before frac's own numeric parse",
        )
        anim_body = function_body(self.plugin, "static void TgProbeSpriteFracAnimCommand(const std::string& rest)")
        self.assertIn("ParseFiniteNumber(secStr, seconds)", anim_body)
        self.assertIn("seconds <= 0.0", anim_body)
        self.assertIn('loopTok == "loop"', anim_body)
        self.assertIn("!loopTok.empty()", anim_body)

    def test_frac_anim_clock_is_resolved_by_name_and_refuses_when_unreadable(self):
        body = function_body(self.plugin, "static bool TgProbeSpriteFracAnimClockRead(")
        self.assertIn('clockName == "get_timer"', body)
        self.assertIn('TgProbeSpriteBuiltinExists("get_timer"', body)
        self.assertNotIn('CallBuiltin("get_timer"', body)
        self.assertIn('clockName == "current_time"', body)
        self.assertIn('GetBuiltin("current_time", nullptr, NULL_INDEX, v)', body)
        self.assertNotIn('CallBuiltin("current_time"', body)
        self.assertEqual(body.count("N1Numeric("), 2)
        self.assertNotIn("GetModuleHandle", body)
        self.assertNotIn("Rva", body)
        anim_body = function_body(self.plugin, "static void TgProbeSpriteFracAnimCommand(const std::string& rest)")
        self.assertIn("frac anim refused", anim_body)
        self.assertLess(
            anim_body.index("frac anim refused"),
            anim_body.index("g_TgSpriteFracAnimState = TgSpriteFracAnimState::Running;"),
        )

    def test_draw_derives_the_fraction_from_the_clock_not_from_draws(self):
        draw = function_body(self.plugin, "static void TgProbeSpriteDraw(bool fromHudLayer)")
        self.assertIn("TgProbeSpriteFracAnimTick();", draw)
        self.assertLess(
            draw.index("TgSpriteMode::Off) return;"),
            draw.index("TgProbeSpriteFracAnimTick();"),
        )
        self.assertLess(
            draw.index("TgProbeSpriteFracAnimTick();"),
            draw.index("TgProbeSpriteDrawStyle("),
        )
        tick = function_body(self.plugin, "static void TgProbeSpriteFracAnimTick()")
        self.assertTrue(
            tick.strip().startswith("if (g_TgSpriteFracAnimState == TgSpriteFracAnimState::Off) return;"),
            "the tick must return before any clock read when no animation is running",
        )
        self.assertIn("TgProbeSpriteFracAnimClockRead(", tick)
        self.assertIn("g_TgSpriteFraction = fraction;", tick)
        self.assertIn("g_TgSpriteFracAnimLoop", tick)
        self.assertIn("g_TgSpriteFraction = 0.0;", tick)   # the hold-at-zero path
        self.assertNotIn("g_TgSpriteAnimTime", tick)
        self.assertNotIn("g_RuntimeFrame", tick)
        self.assertNotIn("1.0 / 15.0", tick)

    def test_plain_frac_cancels_the_animation_only_after_it_parsed(self):
        body = function_body(self.plugin, "static void TgProbeSpriteCommand(const std::string& rest)")
        frac_branch = body[body.index('lower == "frac"'):body.index('lower == "textoffset"')]
        refusal_return = frac_branch.index("did not parse as a number")
        refusal_return = frac_branch.index("return;", refusal_return)
        cancel = frac_branch.index("g_TgSpriteFracAnimState = TgSpriteFracAnimState::Off;")
        assign = frac_branch.index("g_TgSpriteFraction = f;")
        self.assertLess(refusal_return, cancel)
        self.assertLess(cancel, assign)

    def test_off_and_frac_lines_report_the_animation_state(self):
        body = function_body(self.plugin, "static void TgProbeSpriteCommand(const std::string& rest)")
        off_branch = body[body.index('lower == "off"'):body.index('lower == "list"')]
        self.assertIn("TgProbeSpriteFracAnimText()", off_branch)
        frac_branch = body[body.index('lower == "frac"'):body.index('lower == "textoffset"')]
        self.assertIn("TgProbeSpriteFracAnimText()", frac_branch)
        text_body = function_body(self.plugin, "static std::string TgProbeSpriteFracAnimText()")
        self.assertIn('"anim=off"', text_body)
        self.assertIn("src=", text_body)
        self.assertIn("clockFail=", text_body)
        self.assertIn('"running"', text_body)
        self.assertIn('"done"', text_body)

    def test_frac_anim_elapsed_readout_is_unwrapped_in_loop_mode(self):
        # Follow-up fix: `g_TgSpriteFracAnimElapsed` must hold the total
        # elapsed time since `anim` started, even in loop mode, so it reads
        # the same as a stopwatch. Wrapping it (a sawtooth) made a short
        # loop look like the "clock stuck at 0" failure signature. The
        # wrapped value the fraction is actually derived from is kept apart
        # in `g_TgSpriteFracAnimPhase` and shown in the readout as `phase=`.
        tick = function_body(self.plugin, "static void TgProbeSpriteFracAnimTick()")
        self.assertNotIn(
            "elapsed = std::fmod(",
            tick,
            "elapsed itself must stay unwrapped; wrap a separate phase variable instead",
        )
        self.assertIn("g_TgSpriteFracAnimElapsed = elapsed;", tick)
        self.assertIn("g_TgSpriteFracAnimPhase", tick)
        text_body = function_body(self.plugin, "static std::string TgProbeSpriteFracAnimText()")
        self.assertIn("phase=", text_body)

    def test_frac_anim_symbols_are_research_only(self):
        for name in ("TgProbeSpriteFracAnimCommand", "TgProbeSpriteFracAnimClockRead",
                     "TgProbeSpriteFracAnimTick", "TgProbeSpriteFracAnimText",
                     "g_TgSpriteFracAnimState", "g_TgSpriteFracAnimLoop", "g_TgSpriteFracAnimDuration",
                     "g_TgSpriteFracAnimStart", "g_TgSpriteFracAnimClock", "g_TgSpriteFracAnimElapsed",
                     "g_TgSpriteFracAnimTicks", "g_TgSpriteFracAnimClockFail"):
            self.assertIn(name, self.block, name)
            self.assertNotIn(name, self.stripped, name)

    def test_research_doc_describes_frac_anim(self):
        doc = self.research_doc
        self.assertIn("frac anim", doc)
        self.assertIn("get_timer", doc)
        self.assertIn("current_time", doc)

    # ---- 2026-09-21 live session: `bar` and `number` sit ABOVE the icon ----

    def test_number_defaults_above_the_icon(self):
        # The tester nudged `number` to textoffset (0, -101) on the tuned
        # 77x78 Soul Spurn box; that is now the default. Still anchored to the
        # bottom edge, so `textoffset 0 2` restores the old placement.
        self.assertIn("static double g_TgSpriteTextOffsetDx = 0.0, g_TgSpriteTextOffsetDy = -101.0;", self.plugin)
        body = function_body(self.plugin, "static void TgProbeSpriteDrawNumber(")
        self.assertIn("y + h + g_TgSpriteTextOffsetDy", body)

    def test_bar_draws_above_the_box_offset_by_baroffset(self):
        self.assertIn("static double g_TgSpriteBarOffsetDx = 0.0, g_TgSpriteBarOffsetDy = 0.0;", self.plugin)
        body = function_body(self.plugin, "static void TgProbeSpriteDrawBar(")
        # anchored to the box's TOP edge, never below it
        self.assertNotIn("y + h + kBarGap", body)
        self.assertIn("by1 = y - kBarGap + g_TgSpriteBarOffsetDy", body)
        self.assertIn("by0 = by1 - kBarHeight", body)
        self.assertIn("bx0 = x + g_TgSpriteBarInset + g_TgSpriteBarOffsetDx", body)
        # the stub guard survives the move
        self.assertIn("if (barWidth < 1.0) return;", body)

    def test_baroffset_command_parses_both_values_or_changes_nothing(self):
        body = function_body(self.plugin, "static void TgProbeSpriteCommand(const std::string& rest)")
        branch = body[body.index('lower == "baroffset"'):body.index('lower == "textalpha"')]
        self.assertIn("ParseFiniteNumber(dxStr, dx) && ParseFiniteNumber(dyStr, dy)", branch)
        refusal = branch.index("tgprobe sprite baroffset: usage")
        assign = branch.index("g_TgSpriteBarOffsetDx = dx;")
        self.assertLess(refusal, assign, "a refused baroffset must return before storing anything")
        self.assertIn("TgProbeSpriteBarOffsetText()", branch)
        # reported on the `off` line and on `style bar`'s confirmation
        off_branch = body[body.index('lower == "off"'):body.index('lower == "list"')]
        self.assertIn("TgProbeSpriteBarOffsetText()", off_branch)
        self.assertIn('kind == TgSpriteStyleKind::Bar ? " " + TgProbeSpriteBarOffsetText()', body)

    def test_baroffset_feeds_bar_only(self):
        for fn in ("TgProbeSpriteDrawSoft(", "TgProbeSpriteDrawHalo(", "TgProbeSpriteDrawGradient(",
                   "TgProbeSpriteDrawPulse(", "TgProbeSpriteDrawArc(", "TgProbeSpriteDrawNumber(",
                   "TgProbeSpriteDrawFade(", "TgProbeSpriteDrawGoldRect(", "TgProbeSpriteDrawOne("):
            body = function_body(self.plugin, f"static void {fn}")
            self.assertNotIn("g_TgSpriteBarOffset", body, f"{fn} must not reference the bar offset")
        for name in ("g_TgSpriteBarOffsetDx", "g_TgSpriteBarOffsetDy", "TgProbeSpriteBarOffsetText"):
            self.assertIn(name, self.block, name)
            self.assertNotIn(name, self.stripped, name)

    def test_bar_is_inset_equally_from_both_sides(self):
        self.assertIn("static double g_TgSpriteBarInset = 4.0;", self.plugin)
        body = function_body(self.plugin, "static void TgProbeSpriteDrawBar(")
        self.assertIn("const double usableWidth = w - 2.0 * g_TgSpriteBarInset;", body)
        self.assertIn("const double barWidth = usableWidth * fraction;", body)
        self.assertNotIn("const double barWidth = w * fraction;", body)
        self.assertIn("bx0 = x + g_TgSpriteBarInset + g_TgSpriteBarOffsetDx", body)
        # the width guard comes after the inset, so an inset eating the box draws nothing
        self.assertLess(body.index("usableWidth"), body.index("if (barWidth < 1.0) return;"))

    def test_barinset_command_refuses_negative_or_unparsed_values(self):
        body = function_body(self.plugin, "static void TgProbeSpriteCommand(const std::string& rest)")
        branch = body[body.index('lower == "barinset"'):body.index('lower == "textalpha"')]
        self.assertIn("!ParseFiniteNumber(v, px) || px < 0.0", branch)
        self.assertLess(branch.index("tgprobe sprite barinset: usage"), branch.index("g_TgSpriteBarInset = px;"))
        text = function_body(self.plugin, "static std::string TgProbeSpriteBarOffsetText()")
        self.assertIn("barinset=", text)
        self.assertIn("g_TgSpriteBarInset", self.block)
        self.assertNotIn("g_TgSpriteBarInset", self.stripped)

    def test_research_doc_records_the_above_icon_placement(self):
        doc = self.research_doc
        self.assertIn("baroffset", doc)
        self.assertIn("0,-101", doc)


if __name__ == "__main__":
    unittest.main()
