"""Contract tests for the skill bar and talent tree research (toolkit #147).

A session driving the game through `hs-drive` is to read the skill bar, cast a
skill from it, bind a skill to a slot, and allocate and reset talent points.
Which routine each of those runs through, and which store it changes first,
is measured in one research launch (docs/skill-actions-research.md) before
any player command or hub tool is written against it. This stage ships two
things for that launch, and these tests pin both:

- `menulayout` (a player command) lists the nine talent objects too, and
  follows the skill bar's row with one `  slot=` row per bar element. It stays
  a reader: the slot rows are read only through four read builtins.
- `skillprobe` (research build only): one table of every candidate script and
  every SDK closure of the nine objects, each native-detoured by one
  MmCreateHook behind AddrIsExecutableInModule, a function another install
  already detours reported `held` and never hooked twice, one confirm-gated
  by-name `call` that reuses craftprobe's `other:` parser, argument parser and
  dispatcher, and three hook-free readers (`state`, `keys`, `slots`).

The first live session found every closure that fired on the owner's bind,
allocation and reset logging `argc=0`: the buttons run named `UiA*`
activation scripts wired through `UiSetActivationFunc`, and the key per slot
is read through getters the bar draws with, none of which was a row. Replan 3
adds those twenty-two scripts (60 rows to 82), and the three Decision lines
that session settled are pinned as no longer `pending`.

The research doc's headings and its twelve decision keys are pinned so the
player verbs and the hub tools that follow are never written against a key
the document lacks. `pending` is allowed until the phase-0 launch has run.
"""
import re
import sys
import unittest
from pathlib import Path

TESTS_DIR = Path(__file__).resolve().parent
ROOT = TESTS_DIR.parent
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"
DOC = ROOT / "docs" / "skill-actions-research.md"
SDK_SCRIPTS = ROOT.parent / "hs-game-sdk" / "cpp" / "include" / "hs_game_sdk" / "scripts.hpp"

if str(TESTS_DIR) not in sys.path:
    sys.path.insert(0, str(TESTS_DIR))

from test_menu_layout_contract import PLANNED_OBJECTS, TALENT_OBJECTS, doc_section  # noqa: E402
from test_release_hook_contract import function_body, strip_comments, strip_research_blocks  # noqa: E402

BLOCK_START = "// ---- skillprobe: the skill bar and talent tree phase 0 instrument (toolkit #147)"
BLOCK_END = "#endif // FORGEPACT_RELEASE (skillprobe)"

# docs/skill-actions-research.md § Static search: the nine talent objects.
NINE_OBJECTS = (
    "UI_Hud_Talent_obj", "UI_Talent_Screen_obj", "UI_Talent_Button_obj",
    "UI_Button_Talent_Player_obj", "UI_Button_Subtalent_obj", "UI_Talent_Screen_Allocate_obj",
    "UI_Sub_Talents_obj", "UI_Talent_Node_Tree_Parent_obj", "UI_Button_Sub_Skill_obj",
)

# The candidate scripts, the control last. The six constructor-style ones are
# spelled as the SDK spells them, with no gml_Script_ prefix.
CANDIDATE_SCRIPTS = (
    "CheckTalentUse", "TalentUse", "TalentUseClass", "TalentRequirement", "TalentRequirementFunc",
    "GetTalentCooldown", "GetTalentInfo", "ReturnTalentLevel", "GetTalentLevelReq", "ReturnSubTalentLevel",
    "GetSubTalentInfo", "UiActivateTalents", "UiActivateHudTalentButtonFuncs", "UiHudTalentNavigationFunc",
    "UiTalentNavigationFunc", "UiHideTalentsWithNoBind", "UiSetActivationFunc", "NetworkSendTalentUpdate",
    "NetworkSendClientAllTalents", "NetworkSendClientTalentUse", "CA_playerTalentUpdate",
    "CA_playerExtraTalentUpdate", "CA_playerTalentActive", "ClearPersistSkill", "LoadControls",
    "SaveControlsFunc", "GetPlayerTalentHudObj", "GetPlayerProfileObj", "PlayerManaUpdate", "UiCreate",
    "UiSetFocus", "ReportClient", "CheckPlayerInteraction",
)
UNPREFIXED = ("TalentRequirementFunc", "UiActivateTalents", "UiActivateHudTalentButtonFuncs",
              "UiHudTalentNavigationFunc", "UiTalentNavigationFunc", "SaveControlsFunc")

# Replan 3 additions: the activation scripts the talent screen's and the bar's
# buttons are wired to, then the key getters and the keyboard controls' load
# and save. Every one a gml_Script_ constant.
ACTIVATION_SCRIPTS = (
    "UiATalentScreenTalent", "UiATalentScreenAssign", "UiATalentChange", "UiAActivateSkillSubPoint",
    "UiAActivateSkillSpecialization", "UiATalentScreenResetTalents", "UiAResetSubSkillPoints",
    "UiATalentsPlayer", "UiAOpenTalents", "UiAActiveTalentSelect", "UiAContextTalents",
    "UiATalentScreenTalentLoadout", "UiSetTalentSelectTopRowEnabled", "UiHudTalentNavigation",
)
KEY_GETTERS = ("GetSpecificKeyBind", "GetSpecificKBKeyBind", "GetSpecificGPKeyBind",
               "GetPlayerInputBindings", "GetControlName")
KEY_SCRIPTS = KEY_GETTERS + ("DrawKeyBindSprites", "LoadKeyboardControls", "SaveControls")
REPLAN3_SCRIPTS = ACTIVATION_SCRIPTS + KEY_SCRIPTS

# The Decision lines the first live session settled (castByNameRoute
# reproduced; the slot and talent-id rules measured).
SETTLED_KEYS = ("castByNameRoute", "slotRule", "talentIdRule")

DOC_HEADINGS = ("## Static search", "## Static readings", "## Instrument",
                "## Live procedure", "## Results", "## Decision")
DECISION_KEYS = (
    "slotRule", "castKeyRule", "castProof", "castByNameRoute", "bindWriteRule", "bindRoute",
    "talentScreenOpenRoute", "allocRoute", "subAllocRoute", "resetRoute", "pointsReader", "talentIdRule",
)

# The builtins the slot rows may read through, and nothing else.
SLOT_READS = {"variable_instance_get", "array_length", "array_get", "variable_struct_get"}

# What a hook-free reader must never mention: an installer, a write, an event,
# a create or destroy, or a game script reached some other way than the one
# ReturnTalentLevel read `state` makes through craftprobe's dispatcher.
READER_FORBIDDEN = (
    "MmCreateHook", "HookOneScript", "HookBuiltin", "SpInstall", "CallGameScriptEx", "CallBuiltinEx",
    '"script_execute"', "event_perform", "instance_create", "instance_destroy", '"variable_instance_set"',
    '"variable_struct_set"', '"array_set"', "Rva",
)
READER_FUNCTIONS = (
    "static void SpState(", "static void SpKeys(", "static void SpKeysOf(", "static void SpSlots(",
    "static std::string SpValueText(", "static bool SpPathValue(", "static bool SpHud(",
    "static std::string SpAbilityOf(", "static std::string SpSubText(", "static std::string SpLevelText(",
    "static void SpPointCandidates(",
)


def sdk_constants():
    if not SDK_SCRIPTS.exists():
        raise unittest.SkipTest(f"hs-game-sdk header not found at {SDK_SCRIPTS}")
    constants = dict(re.findall(r'std::string_view (\w+) = "([^"]+)";', SDK_SCRIPTS.read_text(encoding="utf-8")))
    assert len(constants) > 6000, f"parsed only {len(constants)} SDK script constants; the parser is broken"
    return constants


class SkillProbeContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.block = cls.plugin[cls.plugin.index(BLOCK_START):cls.plugin.index(BLOCK_END)]
        cls.code = strip_comments(cls.block)
        cls.shipped = strip_comments(strip_research_blocks(cls.plugin))
        table = cls.plugin[cls.plugin.index("#define SKILLPROBE_TARGETS(X)"):]
        table = table[:table.index("#define SP_DEFINE_DETOUR")]
        cls.rows = re.findall(r'X\((\w+),\s*"([^"]+)",\s*(\w+)\)', table)

    def body(self, signature):
        return strip_comments(function_body(self.plugin, signature))

    # ---- research build only --------------------------------------------------

    def test_every_sp_symbol_is_absent_from_the_player_build(self):
        symbols = set(re.findall(r"\b(?:g_|k)?Sp[A-Z]\w*", self.code)) | set(re.findall(r"\bSKILLPROBE_\w+", self.code))
        # A scan that finds nothing would pass vacuously.
        for expected in ("SpCommand", "SpInstall", "SpCall", "SpState", "g_SpTargets", "kSpTargetCount",
                         "SKILLPROBE_TARGETS", "SKILLPROBE_DETOUR"):
            self.assertIn(expected, symbols)
        # Negative control: the strip keeps player code, so an absence below
        # is the strip working, not an empty string.
        self.assertIn("kPlayerCommands", self.shipped)
        self.assertIn("static bool HandleSkillProbeCommand(", self.shipped)
        for symbol in sorted(symbols):
            self.assertIsNone(re.search(r"\b" + re.escape(symbol) + r"\b", self.shipped), symbol + " reaches the player build")
        self.assertNotIn('"skillprobe"', self.shipped)
        self.assertNotIn("skillprobe", self.shipped)

    def test_not_a_player_command(self):
        run = function_body(self.plugin, "static void RunCommand(const std::string& line)")
        commands = run[run.index("kPlayerCommands = {"):]
        commands = commands[:commands.index("};")]
        self.assertIn('"menulayout"', commands)   # the set is the one read, not an empty slice
        self.assertNotIn('"skillprobe"', commands)

    def test_dispatched_from_its_own_handler_as_a_standalone_early_return(self):
        run = function_body(self.plugin, "static void RunCommand(const std::string& line)")
        self.assertIn("    if (HandleRestartProbeCommand(lc, rest)) return;\n    if (HandleSkillProbeCommand(lc, rest)) return;\n", run)
        handler = function_body(self.plugin, "static bool HandleSkillProbeCommand(")
        guard = handler.index("#ifndef FORGEPACT_RELEASE")
        self.assertLess(guard, handler.index('if (lc == "skillprobe") { SpCommand(rest); return true; }'))
        self.assertIn("return false;", handler)
        self.assertEqual(self.plugin.count('lc == "skillprobe"'), 1)
        # In the player build the handler answers false and names nothing.
        shipped_handler = function_body(strip_research_blocks(self.plugin), "static bool HandleSkillProbeCommand(")
        self.assertNotIn("SpCommand", shipped_handler)

    def test_frame_callback_never_mentions_it(self):
        frame = function_body(self.plugin, "void FrameCallback(")
        for word in ("skillprobe", "SpInstall", "SpCommand", "g_SpTargets"):
            self.assertNotIn(word, frame)

    # ---- the row table --------------------------------------------------------

    def test_table_names_every_candidate_script_through_its_sdk_constant(self):
        sdk = sdk_constants()
        rows = {label: constant for _, label, constant in self.rows}
        self.assertEqual(len(CANDIDATE_SCRIPTS), 33)
        for name in CANDIDATE_SCRIPTS:
            self.assertIn(name, rows, name + " is not a skillprobe row")
            constant = rows[name]
            self.assertEqual(constant, name if name in UNPREFIXED else "gml_Script_" + name, name)
            self.assertIn(constant, sdk, constant + " is not an hs-game-sdk constant")
        # The runtime name is the constant's own value, never retyped.
        self.assertIn('HeroSiege::Scripts::CONSTANT.data(), "fp_sp_" #SAFE', self.plugin)
        for name in CANDIDATE_SCRIPTS:
            self.assertNotIn('"gml_Script_' + name + '"', self.code)
        # The control stays the table's last row.
        self.assertEqual(self.rows[-1][2], "gml_Script_CheckPlayerInteraction")
        # KeyboardMouseInput is deliberately not a row (a per-frame dispatcher).
        self.assertNotIn("KeyboardMouseInput", self.code)

    def test_table_names_every_activation_script_and_key_getter(self):
        sdk = sdk_constants()
        rows = {label: constant for _, label, constant in self.rows}
        self.assertEqual(len(REPLAN3_SCRIPTS), 22)
        self.assertEqual(len(set(REPLAN3_SCRIPTS) | set(CANDIDATE_SCRIPTS)), 33 + 22)
        for name in REPLAN3_SCRIPTS:
            self.assertIn(name, rows, name + " is not a skillprobe row")
            self.assertEqual(rows[name], "gml_Script_" + name, name)
            self.assertIn(rows[name], sdk, rows[name] + " is not an hs-game-sdk constant")
            self.assertNotIn('"gml_Script_' + name + '"', self.code)
        # Negative control: the out-of-scope mercenary activation scripts are not rows.
        self.assertFalse(any("Mercenary" in label for label in rows))

    def test_skillprobe_table_covers_every_sdk_closure_of_its_objects(self):
        sdk = sdk_constants()
        table = {constant for _, _, constant in self.rows}
        expected = [c for c, v in sdk.items()
                    if any("@gml_Object_" + obj + "_Create_0" in v for obj in NINE_OBJECTS)]
        # A scan that finds nothing would pass vacuously: the nine objects carry 27.
        self.assertEqual(len(expected), 27, "SDK closure scan found an unexpected count - the regex is blind or the SDK moved")
        missing = [c for c in expected if c not in table]
        self.assertEqual(missing, [], "SDK closures missing from SKILLPROBE_TARGETS: " + ", ".join(missing))
        # Negative control: an object outside the nine has no closure row.
        self.assertFalse(any("UI_Craft_obj" in c or "UI_Stash_obj" in c for c in table))
        # 33 scripts + 22 activation scripts and key getters + 27 closures, one row each.
        self.assertEqual(len(self.rows), 82)
        self.assertEqual(len({c for _, _, c in self.rows}), 82)
        self.assertEqual(len({s for s, _, _ in self.rows}), 82)
        self.assertEqual(len({label.lower() for _, label, _ in self.rows}), 82)   # SpFindRow matches either case

    # ---- how a row attaches ---------------------------------------------------

    def test_each_row_attaches_by_one_mmcreatehook_behind_the_executable_check(self):
        install = self.body("static void SpInstall(")
        self.assertEqual(self.code.count("MmCreateHook("), 1)
        self.assertIn("MmCreateHook(", install)
        hook = install.index("MmCreateHook(")
        self.assertLess(install.index("AddrIsExecutableInModule(mainMod, src)"), hook)
        self.assertLess(install.index("SpResolveName(t, name, p, st)"), hook)
        # Resolved by name only.
        resolve = self.body("static bool SpResolveName(")
        self.assertIn("GetNamedRoutinePointer(name.c_str(), &p)", resolve)
        self.assertEqual(self.code.count("GetNamedRoutinePointer("), 2)   # the SDK name, then with the prefix
        # The detour calls the trampoline, never re-enters the table.
        detour = self.plugin[self.plugin.index("#define SKILLPROBE_DETOUR"):self.plugin.index("#define SKILLPROBE_TARGETS")]
        self.assertIn("g_SpOrig_##SAFE(S, O, R, argc, A)", detour)
        self.assertIn("*t.origSlot = reinterpret_cast<PFUNC_YYGMLScript>(tramp);", install)
        for word in ("HookOneScriptTable", "HookOneScript(", "Rva", "HookBuiltin("):
            self.assertNotIn(word, self.code, word)
        self.assertIsNone(re.search(r"GetModuleHandleA\(nullptr\)\s*\+", self.code))

    def test_a_row_another_install_detours_is_held_and_never_hooked_twice(self):
        install = self.body("static void SpInstall(")
        holder = self.body("static std::string SpHolder(")
        for record in ("g_CpTargets", "g_TgRows", "g_PpTargets", "g_RpRows", "g_CiNatTargets", "MkHolds(name)", "CmHolds(name)"):
            self.assertIn(record, holder, record)
        # Asked before anything is resolved or hooked, and a held row stops there.
        asked = install.index("SpHolder(t.runtimeName, holderCalls)")
        self.assertLess(asked, install.index("SpResolveName("))
        held = install[asked:install.index("SpResolveName(")]
        self.assertIn("++held;", held)
        self.assertIn("continue;", held)
        self.assertIn('" held by "', held)
        # A ForgePact hook that took the table entry inline holds the row too.
        self.assertIn("SpKnownOriginal(name, hook)", install)
        self.assertEqual(install.count("++held;"), 2)
        # The answer line is the one the live control reads.
        self.assertIn('" detoured, " + std::to_string(failed) + " failed, " + std::to_string(held)', install)
        self.assertIn('+ " held"', install)
        # `show` reads a held row's count from its holder, never prints a 0 for a row it cannot read.
        self.assertIn("t.heldCalls", self.body("static bool SpCallsOf("))
        self.assertIn('" calls=n/a ("', self.body("static void SpShow("))

    # ---- the controls ------------------------------------------------------------

    def test_show_proves_skillprobes_own_detours_with_a_second_control(self):
        # In the launch shared with the stash research craftprobe already holds
        # CheckPlayerInteraction, and its count then proves craftprobe's
        # detours. A row skillprobe detours itself - CheckTalentUse, measured
        # running once per frame - is the control for this table's own
        # detour bodies, trampolines and counters.
        own = self.body("static bool SpIsOwnControl(")
        self.assertIn("gml_Script_CheckTalentUse", own)
        self.assertIn(("CheckTalentUse", "CheckTalentUse", "gml_Script_CheckTalentUse"), self.rows)
        show = self.body("static void SpShow(")
        self.assertIn("SpIsOwnControl(t)", show)
        self.assertIn('"  own-detour control "', show)
        # Not detoured here, it says the table's own counts are unproven.
        own_line = show[show.index('"  own-detour control "'):]
        self.assertIn("INSTRUMENT-BLIND", own_line[:own_line.index("continue;")])
        # A held CheckPlayerInteraction says whose detours its count proves.
        self.assertIn("not skillprobe's", show)
        # Negative control: `arm all` still leaves CheckPlayerInteraction alone.
        self.assertIn("SpIsControl(t)", self.body("static void SpArm("))

    def test_a_row_with_no_count_is_never_silent(self):
        show = self.body("static void SpShow(")
        unknown = show[show.index("if (!known)"):]
        unknown = unknown[:unknown.index("continue;")]
        self.assertIn('" calls=n/a ("', unknown)
        # Printed for every such row, not only for the control or under `show all`.
        self.assertIsNone(re.search(r"\ball\b", unknown), unknown)
        self.assertNotIn("pass == 0 ||", show)

    def test_a_row_held_inline_names_the_instrument_that_can_count_it(self):
        # toggleguard's HookTalentUseClass installs both routes, so the function
        # holds a trampoline nothing else can detour and no counter; tgprobe's
        # entry note in that hook's body is what sees those calls.
        hint = self.body("static std::string SpCountHint(")
        for part in ("g_TgRows", "viaHook", "`tgprobe hook ", "`skillprobe hook ", "`tgprobe verbose on`"):
            self.assertIn(part, hint)
        self.assertIn('"TalentUseClass", kTgArgs | kTgRet, &g_OrigTalentUseClass, "HookTalentUseClass")', self.plugin)
        self.assertIn("SpCountHint(t)", self.body("static void SpInstall("))
        self.assertIn("SpCountHint(*row)", self.body("static void SpArm("))
        self.assertIn("SpCountHint(t)", self.body("static void SpShow("))
        # The old wording promised a log nobody had turned on.
        self.assertNotIn("its holder's own log does", self.code)

    def test_the_instruments_own_calls_are_not_counted(self):
        detour = self.plugin[self.plugin.index("#define SKILLPROBE_DETOUR"):self.plugin.index("#define SKILLPROBE_TARGETS")]
        self.assertLess(detour.index("if (g_SpOwnCall)"), detour.index("InterlockedIncrement(&g_SpCalls_##SAFE)"))

    def test_the_bare_command_prints_the_build_marker(self):
        usage = self.body("static void SpUsage(")
        self.assertIn('"skillprobe: rows=" + std::to_string(kSpTargetCount) + " hooked=" + std::to_string(hooked) + " held="', usage)
        command = self.body("static void SpCommand(")
        self.assertIn("if (tok.empty()) { SpUsage(); return; }", command)

    # ---- the one write ---------------------------------------------------------

    def test_call_requires_confirm_and_reuses_craftprobes_parser_and_dispatcher(self):
        call = self.body("static void SpCall(")
        gate = call.index('Lower(tok.back()) != "confirm"')
        dispatch = call.index("CpDispatchScript(script, inst, other, args, res, st)")
        for step in ("SpFindRow(tok[1])", "runtime.find('@')", "SpResolveName(*t, name, p, rs)", "MpResolve(",
                     '"instance_exists", { handle }', "HhResolveInstance(handle)",
                     'CpResolveOther("skillprobe call", tok[argsFrom], other)', "SpResolveArg(tok[i], inst, v)"):
            self.assertLess(gate, call.index(step), step)
            self.assertLess(call.index(step), dispatch, step)
        # One parser for other: and one dispatcher, both craftprobe's.
        self.assertNotIn("static bool SpResolveOther(", self.plugin)
        self.assertNotIn('std::string("other:").size()', self.code)
        self.assertEqual(self.code.count("CpResolveOther("), 1)
        self.assertEqual(self.code.count("CpDispatchScript("), 2)   # `call`, and `state`'s ReturnTalentLevel read
        self.assertIn("CpResolveArg(\"skillprobe call\", a, inst, v)", self.body("static bool SpResolveArg("))
        for word in ('"script_execute"', "CallBuiltinEx", "CallGameScriptEx", "ApCallScript("):
            self.assertNotIn(word, self.code, word)
        # Every refusal says nothing was called and returns, before the dispatch.
        before = call[:dispatch]
        refusals = [m.start() for m in re.finditer(r"refused - ", before)]
        self.assertGreaterEqual(len(refusals), 8)
        for at in refusals:
            segment = before[at:before.index("return;", at)]
            self.assertIn("nothing was called", segment, segment)

    def test_call_has_no_argument_kind_craftprobe_lacks(self):
        # SpResolveArg only delegates: every kind `skillprobe call` accepts is
        # one CpResolveArg parses, so a replay shape logged here can be
        # supplied to `craftprobe call` in the same words, and back.
        resolve = self.body("static bool SpResolveArg(")
        statements = [s.strip() for s in resolve.split(";") if s.strip()]
        self.assertEqual(statements, ['return CpResolveArg("skillprobe call", a, inst, v)'])
        self.assertIsNone(re.search(r'"\w+:"', resolve))
        # Negative control: the kinds live in craftprobe's parser, `id:` and `obj:` among them.
        craft = self.body("static bool CpResolveArg(")
        for kind in ('"id:"', '"obj:"', '"fp:"', '"kept:"', '"path:"'):
            self.assertIn(kind, craft, kind)

    def test_a_closure_row_is_refused_naming_the_route_that_works(self):
        call = self.body("static void SpCall(")
        closure = call.index("runtime.find('@')")
        refusal = call[closure:call.index("return;", closure)]
        self.assertIn("craftprobe methods", refusal)
        self.assertIn("craftprobe callm", refusal)
        self.assertIn("inst <member>", refusal)
        self.assertLess(closure, call.index("CpDispatchScript("))

    def test_a_profile_getter_is_never_invoked(self):
        call = self.body("static void SpCall(")
        at = call.index("gml_Script_GetPlayerProfileObj")
        self.assertLess(at, call.index("CpDispatchScript("))
        self.assertIn("return;", call[at:call.index("SpResolveName(")])

    def test_the_state_read_runs_only_return_talent_level_as_its_own_call(self):
        level = self.body("static std::string SpLevelText(")
        self.assertIn("gml_Script_ReturnTalentLevel", level)
        self.assertLess(level.index("g_SpOwnCall = true;"), level.index("CpDispatchScript("))
        self.assertLess(level.index("CpDispatchScript("), level.index("g_SpOwnCall = false;"))
        # A detour neither logs that call nor spends its budget on it.
        self.assertIn("if (g_SpOwnCall) return false;", self.body("static bool SpObserve("))
        # `nolevel` skips it.
        state = self.body("static void SpState(")
        self.assertIn('l == "nolevel"', state)
        self.assertIn("if (levels) for (int id : bar)", state)

    # ---- the hook-free readers ------------------------------------------------

    def test_state_keys_and_slots_install_no_hook_and_write_nothing(self):
        for signature in READER_FUNCTIONS:
            body = self.body(signature)
            for word in READER_FORBIDDEN:
                self.assertNotIn(word, body, f"{signature} mentions {word}")
        command = self.body("static void SpCommand(")
        for sub in ('"state") { SpState(tok)', '"keys") { SpKeys(tok)', '"slots") { SpSlots(tok)'):
            self.assertIn(sub, command)

    def test_keys_points_each_key_getter_at_its_armed_calls(self):
        # No member held a key code per slot in the first live session; the bar
        # reads its keys through getters, so `keys` says how to read them.
        keys = self.body("static void SpKeys(")
        self.assertIn("kSpKeyGetters", keys)
        self.assertIn("arm it and read the HUD's own calls", keys)
        self.assertIn("`skillprobe arm ", keys)
        getters = self.plugin[self.plugin.index("static constexpr std::string_view kSpKeyGetters[] = {"):]
        getters = getters[:getters.index("};")]
        listed = re.findall(r"HeroSiege::Scripts::(\w+)", getters)
        self.assertEqual(listed, ["gml_Script_" + g for g in KEY_GETTERS])
        # Each getter is a row, so the hint names something `arm` accepts.
        constants = {constant for _, _, constant in self.rows}
        for constant in listed:
            self.assertIn(constant, constants)

    def test_every_state_read_has_its_own_try_and_prints_unreadable(self):
        state = self.body("static void SpState(")
        self.assertGreaterEqual(state.count("try"), 6)
        self.assertGreaterEqual(state.count("unreadable"), 8)
        for part in ('"  global.mySkills="', '"  hud.playerSlot.bind_skill="', '"  bind "', '"  sub="', '"  level="',
                     'SpPointCandidates("player", player)'):
            self.assertIn(part, state)
        self.assertIn("unreadable", self.body("static std::string SpSubText("))
        self.assertIn("unreadable", self.body("static std::string SpAbilityOf("))


class MenuLayoutSlotRows(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN.read_text(encoding="utf-8").replace("\r\n", "\n")

    def body(self, signature):
        return strip_comments(function_body(self.plugin, signature))

    def test_the_nine_talent_objects_are_listed(self):
        self.assertEqual(TALENT_OBJECTS, set(NINE_OBJECTS))
        self.assertEqual(len(PLANNED_OBJECTS), 38)
        table = self.plugin[self.plugin.index("static const HeroSiege::Objects::GameObject kMenuLayoutObjects[] = {"):]
        table = table[:table.index("};")]
        for name in NINE_OBJECTS:
            self.assertIn("HeroSiege::Objects::GameObject::" + name + ",", table)

    def test_the_bar_row_is_followed_by_its_slot_rows(self):
        command = self.body("static void MenuLayoutCommand(")
        self.assertIn("HeroSiege::Objects::GameObject::UI_Hud_Talent_obj", command)
        row = command.index("Out(MenuLayoutRow(inst, sc));")
        slots = command.index('if (hudIdx >= 0 && MenuLayoutRead(inst, "object_index") == hudIdx) MenuLayoutSlotRows(inst, sc);')
        self.assertLess(row, slots)
        rows = self.body("static void MenuLayoutSlotRows(")
        for part in ('"  slot="', '" talent="', '" gui="', '" win="', '"row0", "row1"'):
            self.assertIn(part, rows)
        # A missing array and an empty one each say so, never nothing.
        self.assertIn('",* absent"', rows)
        self.assertIn('",* empty"', rows)

    def test_slot_rows_read_only_through_four_builtins(self):
        rows = self.body("static void MenuLayoutSlotRows(")
        field = self.body("static std::string MenuLayoutSlotField(")
        used = set(re.findall(r'CallBuiltin\("(\w+)"', rows + field))
        # Negative control: the scan sees the reads that are there.
        self.assertIn("variable_struct_get", used)
        self.assertIn("array_get", used)
        self.assertLessEqual(used, SLOT_READS)
        for word in ("CallBuiltinEx", "script_execute", "variable_instance_set", "variable_struct_set", "array_set"):
            self.assertNotIn(word, rows + field)


class SkillActionsResearchDoc(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.doc = DOC.read_text(encoding="utf-8").replace("\r\n", "\n")

    def test_headings_in_order(self):
        at = [self.doc.index("\n" + h + "\n") for h in DOC_HEADINGS]
        self.assertEqual(at, sorted(at))

    def test_one_line_per_decision_key(self):
        decision = doc_section(self.doc, "## Decision")
        for key in DECISION_KEYS:
            lines = re.findall(r"(?m)^" + key + r": (.+)$", decision)
            self.assertEqual(len(lines), 1, key)
            # `pending` is allowed until the phase-0 launch has measured it.
            self.assertTrue(lines[0].strip(), key)

    def test_no_other_decision_key(self):
        decision = doc_section(self.doc, "## Decision")
        keys = re.findall(r"(?m)^(\w+): ", decision)
        self.assertEqual(sorted(keys), sorted(DECISION_KEYS))
        self.assertEqual(len(DECISION_KEYS), 12)

    def test_settled_decision_lines_are_not_pending(self):
        decision = doc_section(self.doc, "## Decision")
        for key in SETTLED_KEYS:
            line = re.search(r"(?m)^" + key + r": (.+)$", decision)
            self.assertIsNotNone(line, key)
            self.assertNotIn("pending", line.group(1), key)
        # Negative control: a key live 1 did not settle may still be pending.
        self.assertIsNotNone(re.search(r"(?m)^castKeyRule: ", decision))

    def test_candidate_table_is_documented(self):
        static = doc_section(self.doc, "## Static search")
        for name in NINE_OBJECTS + CANDIDATE_SCRIPTS + REPLAN3_SCRIPTS:
            self.assertIn(f"`{name}`", static, name)

    def test_live_procedure_carries_the_recording_rule_and_the_checks_block(self):
        procedure = doc_section(self.doc, "## Live procedure")
        for literal in ("shape not reproduced", "not-run (instrument", "## Checks"):
            self.assertIn(literal, procedure, literal)

    def test_the_control_proves_skillprobes_own_detours(self):
        for heading in ("## Instrument", "## Live procedure"):
            section = doc_section(self.doc, heading)
            self.assertIn("own-detour control", section, heading)
            self.assertIn("`CheckTalentUse`", section, heading)
            self.assertIn("`tgprobe hook TalentUseClass`", section, heading)
            self.assertIn("`tgprobe verbose on`", section, heading)


if __name__ == "__main__":
    unittest.main()
