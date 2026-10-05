"""Contract tests for the `gambapity` mod (ForgePact #134, phase 3).

`gambapity` guarantees Goburin's Head (the unique charm at repository type 10 /
sub 0 / base 98, key `charms_goburins_head`) from the gamba machine after the
configured number of spins without it dropping. The decision core is
plugin/include/ForgePact/GambaPity.hpp (exercised by
test_gamba_pity_behavior.py); this test pins the adapter in
plugin/ModuleMain.cpp on comment-stripped source:

- it is a player command - `gambapity` is in `kPlayerCommands` and dispatched
  from its own command as a standalone early return - while `gambaprobe` and
  every `Gp*`/`GambaProbe` symbol vanish from the player build;
- the three hooks are by SDK constant through `HookOneScript` (`PickUpGoldCheck`
  is gambapity's own) or spliced into the Angelic roll's saved trampolines
  (`g_Orig_CreateDefaultParams` for the prize build, `g_Orig_CreateItemNew` for
  the placement's type), and a table-only hook is refused with a message naming
  the row, never silently armed;
- the force rewrites `CreateDefaultParams`' returned struct's `j`/`b`/`c` in
  place to the charm's 0/98/1 on the prize build (third argument truthy), and
  the item's type (10) - which the struct cannot carry - is rewritten at
  `CreateItemNew`'s entry, carried there by a one-step force-pending flag;
- the natural-drop reset keys on a machine-self `CreateDefaultParams` whose
  first two arguments are `(0, 98)`, never on a `GetUniqueRepoStruct(10, 0, 98)`
  call;
- the machine self is resolved by the instance-handle rule
  (`variable_instance_get` through `N1ObjectIndex`, the masked predicate that
  accepts the flagged object-index kind this runner returns) with no raw kind
  check;
- the charm is named by its repository keys (10 / 0 / 98) and tied to the
  `kAngelicBases` `charms_goburins_head` row, and the machine by its SDK object
  name, never a hand-written index or a field guess;
- the counter persists to `forgepact_gamba_pity.json` beside
  `forgepact_gem_tables.json`, written atomically (temp file then
  `std::filesystem::rename`);
- `gambapity status` names `count=` and `threshold=` and surfaces
  `g_GambaPityError` (the last load/save refusal), which is read, not only
  written;
- the force and the natural reset each log one action line;
- the plugin-side spin-count range equals `src/forgepact.py`'s
  `GAMBA_PITY_RANGE = (10, 1000)` and `Mods.svelte`'s `min`/`max`;
- `GambaPityFallbackDrop` is gone;
- no hex or RVA literal reaches a call;
- in the research build, `gambaprobe hook` refuses while `gambapity` holds one
  of its scripts, and `gambapity` refuses while `gambaprobe` holds them, each
  naming the holder.
"""
import re
import sys
import unittest
from pathlib import Path

TESTS_DIR = Path(__file__).resolve().parent
ROOT = TESTS_DIR.parent
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"
HEADER = ROOT / "plugin" / "include" / "ForgePact" / "GambaPity.hpp"
SDK_SCRIPTS = ROOT.parent / "hs-game-sdk" / "cpp" / "include" / "hs_game_sdk" / "scripts.hpp"
SDK_OBJECTS = ROOT.parent / "hs-game-sdk" / "cpp" / "include" / "hs_game_sdk" / "objects.hpp"
FORGEPACT_PY = ROOT / "src" / "forgepact.py"
MODS_SVELTE = ROOT / "panel" / "src" / "tabs" / "Mods.svelte"

if str(TESTS_DIR) not in sys.path:
    sys.path.insert(0, str(TESTS_DIR))

from test_release_hook_contract import function_body, strip_comments, strip_research_blocks  # noqa: E402

BLOCK_START = "// gambapity (ForgePact #134 phase 3, player build): the Goburin's Head pity"
BLOCK_END = "static void RunCommand(const std::string& line)"

# The charm's repository identifier: type, sub, base.
CHARM = (10, 0, 98)
MACHINE = "Slot_Machine_01_obj"


def without_strings(code):
    return re.sub(r'"(?:\\.|[^"\\])*"', '""', code)


def sdk_scripts():
    if not SDK_SCRIPTS.exists():
        raise unittest.SkipTest(f"hs-game-sdk header not found at {SDK_SCRIPTS}")
    constants = dict(re.findall(r'std::string_view (\w+) = "([^"]+)";', SDK_SCRIPTS.read_text(encoding="utf-8")))
    assert len(constants) > 6000, f"parsed only {len(constants)} SDK script constants; the parser is broken"
    return constants


def sdk_objects():
    if not SDK_OBJECTS.exists():
        raise unittest.SkipTest(f"hs-game-sdk header not found at {SDK_OBJECTS}")
    objects = dict(re.findall(r"^\s*(\w+) = (\d+),\s*$", SDK_OBJECTS.read_text(encoding="utf-8"), flags=re.M))
    assert len(objects) > 3000, f"parsed only {len(objects)} SDK objects; the parser is broken"
    return objects


class GambaPityContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN.read_text(encoding="utf-8").replace("\r\n", "\n")
        start = cls.plugin.index(BLOCK_START)
        cls.block = cls.plugin[start:cls.plugin.index(BLOCK_END, start)]
        cls.code = strip_comments(cls.block)
        cls.shipped = strip_comments(strip_research_blocks(cls.plugin))
        cls.header = strip_comments(HEADER.read_text(encoding="utf-8").replace("\r\n", "\n"))

    def body(self, signature):
        return strip_comments(function_body(self.plugin, signature))

    # ---- a player command ----------------------------------------------------

    def test_is_a_player_command(self):
        run = function_body(self.plugin, "static void RunCommand(const std::string& line)")
        commands = run[run.index("kPlayerCommands = {"):]
        commands = commands[:commands.index("};")]
        self.assertIn('"gambapity"', commands)
        self.assertIn('"dungeonchest"', commands)   # the set is the one read, not an empty slice
        self.assertNotIn('"gambaprobe"', commands)

    def test_gambaprobe_and_every_gp_symbol_vanish_from_the_player_build(self):
        for word in ("gambaprobe", "GambaProbe", "GambaProbe.hpp", "g_GpCore", "GpCommand", "g_GpScriptRows"):
            self.assertNotIn(word, self.shipped, word + " reaches the player build")
        self.assertIsNone(re.search(r"\bGp[A-Z0-9_]\w*", self.shipped), "a Gp* symbol reaches the player build")
        # Negative control: the strip keeps the player feature.
        self.assertIn("gambapity", self.shipped)
        self.assertIn("GambaPityCommand", self.shipped)
        self.assertIn("GambaPity.hpp", strip_research_blocks(self.plugin))

    def test_dispatched_as_a_standalone_early_return(self):
        run = function_body(self.plugin, "static void RunCommand(const std::string& line)")
        self.assertIn('if (lc == "gambapity") { GambaPityCommand(rest); return; }', run)
        self.assertEqual(run.count("GambaPityCommand"), 1)
        # The player build keeps the dispatch (it is a player command).
        shipped_run = function_body(strip_research_blocks(self.plugin), "static void RunCommand(const std::string& line)")
        self.assertIn('if (lc == "gambapity") { GambaPityCommand(rest); return; }', shipped_run)

    # ---- the hooks -----------------------------------------------------------

    def test_the_hooks_are_by_sdk_constant_or_a_spliced_trampoline(self):
        sdk = sdk_scripts()
        self.assertIn("gml_Script_PickUpGoldCheck", sdk, "not an hs-game-sdk script constant")
        install = self.body("static std::string GambaPityInstallHooks()")
        # PickUpGoldCheck is gambapity's own HookOneScript.
        self.assertIn("HookOneScript(SdkShortScriptName(HeroSiege::Scripts::gml_Script_PickUpGoldCheck), \"fp_gambapity_spin\",",
                      install)
        # CreateDefaultParams and CreateItemNew are already held by the Angelic
        # roll's hooks: each is spliced into that saved trampoline, never hooked
        # a second time.
        self.assertIn("if (!g_Orig_CreateDefaultParams) InstallSignatureAngelicHooks();", install)
        self.assertIn("g_GambaPityCdpOrig = g_Orig_CreateDefaultParams;", install)
        self.assertIn("g_Orig_CreateDefaultParams = reinterpret_cast<PFUNC_YYGMLScript>(GambaPityCdpDetour);", install)
        self.assertIn("if (!g_Orig_CreateItemNew) InstallSignatureAngelicHooks();", install)
        self.assertIn("g_GambaPityItemOrig = g_Orig_CreateItemNew;", install)
        self.assertIn("g_Orig_CreateItemNew = reinterpret_cast<PFUNC_YYGMLScript>(GambaPityItemDetour);", install)
        self.assertEqual(install.count("HookOneScript("), 1, "only the unheld script is HookOneScript'd")

    def test_a_table_only_hook_is_refused_with_the_row_named(self):
        install = self.body("static std::string GambaPityInstallHooks()")
        self.assertIn('return "PickUpGoldCheck is hooked table-only, so the game\'s own calls would pass the mod by; the mod stays off";',
                      install)
        self.assertIn('return "CreateDefaultParams is hooked table-only, so the game\'s own calls would pass the mod by; the mod stays off";',
                      install)
        self.assertIn('return "CreateItemNew is hooked table-only, so the game\'s own calls would pass the mod by; the mod stays off";',
                      install)
        # Never report armed while doing nothing: the refusal precedes `SetEnabled(true)`.
        command = self.body("static void GambaPityCommand(")
        self.assertLess(command.index("GambaPityInstallHooks()"), command.index("g_GambaPity.SetEnabled(true);"))

    # ---- who the self is -----------------------------------------------------

    def test_the_machine_self_is_by_the_instance_handle_rule_not_a_kind_check(self):
        machine = self.body("static bool GambaPityIsMachine(")
        self.assertIn("GambaPityObjectIndex(S)", machine)
        # The object-index read goes through N1ObjectIndex - the masked
        # predicate that accepts the flagged object-index kind this runner
        # returns - never IsNumericInstanceRead's unmasked comparison.
        index = self.body("static int GambaPityObjectIndex(")
        self.assertIn("variable_instance_get", index)
        self.assertIn("N1ObjectIndex", index)
        self.assertNotIn("IsNumericInstanceRead", index)
        for word in ("m_Kind", "VALUE_OBJECT", "VALUE_REF", "VALUE_REAL"):
            self.assertNotIn(word, self.code, word + ": a raw kind check in gambapity would silently disable it")
        # The machine's object is resolved by name, never a hand-written index.
        self.assertIn("HeroSiege::Objects::GetObjectName(", self.code)
        self.assertIn("HeroSiege::Objects::GameObject::Slot_Machine_01_obj", self.code)
        objects = sdk_objects()
        self.assertIn(MACHINE, objects)
        self.assertIsNone(re.search(r"\b" + objects[MACHINE] + r"\b", self.code), "a hand-written object index")

    # ---- the charm -----------------------------------------------------------

    def test_the_charm_is_named_by_its_repository_keys_and_tied_to_the_row(self):
        self.assertIn("static constexpr int kGambaPityCharmType = 10;", self.code)
        self.assertIn("static constexpr int kGambaPityCharmSub = 0;", self.code)
        self.assertIn("static constexpr int kGambaPityCharmBase = 98;", self.code)
        # The constants are tied to the kAngelicBases row by key, and the mod
        # refuses to arm if they drift from it.
        self.assertIn('if (std::string_view(a.key) == "charms_goburins_head") return &a;', self.code)
        self.assertIn("head->type == kGambaPityCharmType && head->sub == kGambaPityCharmSub && head->b == kGambaPityCharmBase",
                      self.code)
        self.assertIn("if (!GambaPityCharmConstantsMatch())", self.code)

    def test_a_spin_is_one_machine_self_pickupgoldcheck_with_a1_negative_10000(self):
        self.assertIn("static constexpr double kGambaPitySpinGold = -10000.0;", self.code)
        spin = self.body("static bool GambaPityIsSpin(")
        self.assertIn("argc > 1", spin)
        self.assertIn("A[1]", spin)
        self.assertIn("a1 == kGambaPitySpinGold", spin)

    # ---- the force -----------------------------------------------------------

    def test_the_prize_build_is_a_truthy_third_argument(self):
        build = self.body("static bool GambaPityIsPrizeBuild(")
        self.assertIn("argc <= 2", build)
        self.assertIn("A[2]", build)
        self.assertIn("ToBoolean()", build)

    def test_the_force_rewrites_the_returned_structs_jbc_in_place(self):
        force = self.body("static bool GambaPityForceParams(")
        self.assertIn('g_Yytk->CallBuiltin("variable_struct_set", { params, RValue("j"), RValue((double)kGambaPityCharmSub) });', force)
        self.assertIn('g_Yytk->CallBuiltin("variable_struct_set", { params, RValue("b"), RValue((double)kGambaPityCharmBase) });', force)
        self.assertIn('g_Yytk->CallBuiltin("variable_struct_set", { params, RValue("c"), RValue(1.0) });', force)
        cdp = self.body("static RValue& GambaPityCdpDetour(")
        self.assertIn("GambaPityIsPrizeBuild(argc, A) && g_GambaPity.OnPrizeRoll(true)", cdp)
        self.assertIn("GambaPityForceParams(r)", cdp)
        self.assertIn("g_GambaPityForcePending = true;", cdp)

    def test_the_type_is_written_at_the_placement(self):
        item = self.body("static RValue& GambaPityItemDetour(")
        self.assertIn("if (g_GambaPityForcePending)", item)
        self.assertIn("g_GambaPityForcePending = false;", item)
        self.assertIn('g_Yytk->CallBuiltin("variable_struct_set", { *A[0], RValue("itemType"), RValue((double)kGambaPityCharmType) });',
                      item)

    def test_the_natural_reset_keys_on_create_default_params_args_0_98(self):
        build = self.body("static bool GambaPityIsCharmBuild(")
        self.assertIn("SigNumber(*A[0], s)", build)
        self.assertIn("SigNumber(*A[1], b)", build)
        self.assertIn("kGambaPityCharmSub", build)
        self.assertIn("kGambaPityCharmBase", build)
        # The third argument is not guessed: no A[2] read here.
        self.assertNotIn("A[2]", build)

    def test_the_force_and_the_reset_each_log_an_action_line(self):
        cdp = self.body("static RValue& GambaPityCdpDetour(")
        self.assertIn('Out("gambapity: forced Goburin\'s Head (type "', cdp)
        self.assertIn('Out("gambapity: a natural Goburin\'s Head build reset the counter");', cdp)

    # ---- the persistent counter ---------------------------------------------

    def test_the_counter_file_matches_the_gem_tables_pattern(self):
        path = self.body("static std::filesystem::path GambaPityPath()")
        self.assertIn("ForgePact::ItemTruth::Root()", path)
        self.assertIn("truth.parent_path()", path)
        self.assertIn('L"forgepact_gamba_pity.json"', path)
        save = self.body("static void GambaPitySave()")
        self.assertIn("tmp += L\".tmp\";", save)
        self.assertIn("std::filesystem::rename(tmp, path, ec);", save)
        # Loaded on first use, never louder than a status line on failure.
        load = self.body("static void GambaPityLoad()")
        self.assertIn("if (g_GambaPityLoaded) return;", load)
        self.assertIn("g_GambaPityLoaded = true;", load)

    # ---- the status line -----------------------------------------------------

    def test_status_names_count_and_threshold_and_surfaces_the_error(self):
        line = self.header[self.header.index("std::string StatusLine() const"):]
        self.assertIn('" count="', line)
        self.assertIn('" threshold="', line)
        self.assertIn("GoldEquivalent()", line)
        command = self.body("static void GambaPityCommand(")
        self.assertIn('if (arg.empty() || arg == "status" || arg == "stat") {', command)
        # The last load/save refusal is read beside the status line, not only
        # written (the gems: saved pattern).
        self.assertIn("g_GambaPity.StatusLine() + (g_GambaPityError.empty() ? std::string() : \" - \" + g_GambaPityError)",
                      command)

    def test_the_plugin_range_matches_python_and_the_panel(self):
        self.assertIn("static constexpr int kGambaPityMin = 10;", self.code)
        self.assertIn("static constexpr int kGambaPityMax = 1000;", self.code)
        command = self.body("static void GambaPityCommand(")
        self.assertIn("count < kGambaPityMin || count > kGambaPityMax", command)
        python = FORGEPACT_PY.read_text(encoding="utf-8").replace("\r\n", "\n")
        self.assertIn("GAMBA_PITY_RANGE = (10, 1000)", python)
        svelte = MODS_SVELTE.read_text(encoding="utf-8").replace("\r\n", "\n")
        self.assertIn('id="gambapity" min="10" max="1000"', svelte)

    def test_the_fallback_drop_is_gone(self):
        self.assertNotIn("GambaPityFallbackDrop", self.code)
        self.assertNotIn("fallback drop-ourselves", self.code)

    # ---- no address reaches a call ------------------------------------------

    def test_no_hex_or_rva_literal_reaches_a_call(self):
        bare = without_strings(self.code)
        self.assertIsNone(re.search(r"\b0[xX][0-9A-Fa-f]+", bare), "a hex literal in gambapity's code")
        self.assertIsNone(re.search(r"Rva\w*", bare), "an RVA in gambapity's code")
        self.assertIsNone(re.search(r"GetModuleHandle\w*\([^)]*\)\s*\+", bare), "a call target computed from a module base")

    # ---- the decision core is game independent ------------------------------

    def test_the_decision_core_is_game_independent(self):
        for word in ("RValue", "CInstance", "g_Yytk", "YYTK", "Aurie", "CallBuiltin", "CallGameScript"):
            self.assertNotIn(word, self.header)
        self.assertIn("inline constexpr int64_t kGoldPerSpin = 10000;", self.header)

    # ---- coexistence with gambaprobe (research build) -----------------------

    def test_gambaprobe_refuses_while_gambapity_holds_the_shared_scripts(self):
        install = self.body("static void GpInstall()")
        first = install.index("GpInstallEvents(mainMod);")
        self.assertLess(install.index("GambaPityHeldScripts()"), first, "gambapity is checked after a row is installed")
        self.assertIn('"gambaprobe hook: refused - gambapity holds "', install)
        held = self.body("static std::string GambaPityHeldScripts()")
        self.assertIn('held += "CreateDefaultParams"', held)
        self.assertIn('held += (held.empty() ? "" : ", ") + std::string("CreateItemNew")', held)
        self.assertIn('held += (held.empty() ? "" : ", ") + std::string("PickUpGoldCheck")', held)

    def test_gambapity_refuses_while_gambaprobe_holds_the_shared_scripts(self):
        install = self.body("static std::string GambaPityInstallHooks()")
        self.assertIn('"gambaprobe holds "', install)
        self.assertIn("kGpScript_CreateDefaultParams", install)
        self.assertIn("kGpScript_CreateItemNew", install)
        self.assertIn("kGpScript_PickUpGoldCheck", install)
        # The check is research-build only: the player build never names gambaprobe.
        self.assertNotIn("kGpScript_CreateDefaultParams", self.shipped)


if __name__ == "__main__":
    unittest.main()
