"""Contract tests for the `gambapity` mod (ForgePact #134, phase 6).

`gambapity` guarantees Goburin's Head (the unique charm at repository type 10 /
sub 0 / base 98, key `charms_goburins_head`) from the gamba machine: it counts
the machine explosions that did not drop a head, and the explosion that brings
the count to the configured number drops exactly one head and starts the count
over. The decision core is plugin/include/ForgePact/GambaPity.hpp (exercised by
test_gamba_pity_behavior.py); this test pins the adapter in
plugin/ModuleMain.cpp on comment-stripped source:

- it is a player command - `gambapity` is in `kPlayerCommands` and dispatched
  from its own command as a standalone early return - while `gambaprobe` and
  every `Gp*`/`GambaProbe` symbol vanish from the player build;
- the phase-5 spin hook is retired: no `PickUpGoldCheck`, no `HookOneScript`
  and none of its symbols in gambapity's code;
- the two hooks are spliced into the Angelic roll's saved trampolines
  (`g_Orig_CreateDefaultParams`, `g_Orig_CreateItemNew`), and a table-only
  hook is refused with a message naming the row, never silently armed;
- the payout force is retired: neither splice writes a struct, nothing calls
  `OnPrizeRoll`, and none of the retired symbols or the `gambapity: prize
  build` line remain. The `CreateDefaultParams` splice keeps only the
  machine-self `(0, 98)` natural reset, and the `CreateItemNew` splice is a
  read-after-return head detector;
- the trigger is an end-of-frame poll from `FrameCallback` that returns at once
  while the mod is off, reads machines through `instance_find` (counting each
  unread one) and their
  sprite by name, and compares it with `Slot_Machine_01_Destroyed_spr`, a key
  of the Python SDK's `SPRITE_NAME_TO_INDEX`;
- the ground check reads `itemInstance`'s `itemType` and
  `itemDefinitionStruct` `j`/`b`; the forced drop goes through `json_parse`,
  `InitItemFromJson` and `LootGroundCreateFromItem` with the local player as
  self, a bounded retry and an `instance_exists` read-back, inside the
  own-drop scope;
- the machine self is resolved by the instance-handle rule
  (`variable_instance_get` through `N1ObjectIndex`, the masked predicate that
  accepts the flagged object-index kind this runner returns) with no raw kind
  check;
- the charm is named by its repository keys (10 / 0 / 98) and tied to the
  `kAngelicBases` `charms_goburins_head` row, and the machine by its SDK object
  name, never a hand-written index or a field guess;
- the count persists to `forgepact_gamba_pity.json` beside
  `forgepact_gem_tables.json`, written atomically (temp file then
  `std::filesystem::rename`) through the core's `CounterFileText` and read
  through its `ParseCounterFile`; an explosion's addition is saved when it is
  seen, and an older spin file prints the migration line and is rewritten,
  the load's stream closed first (Windows refuses the rename over an open
  file, Live 1); a refused rename removes its `.tmp`, and a save that lands
  clears an earlier save's refusal as well as the version error;
- `gambapity status` carries every counter (no `gold=`) and surfaces
  `g_GambaPityError` (the last load/save refusal), and every action line is
  fixed text from the core;
- the plugin-side explosion-count range equals `src/forgepact.py`'s
  `GAMBA_PITY_RANGE = (1, 200)` and `Mods.svelte`'s `min`/`max`, and a saved
  count outside it (one above 200) loads as the default, 10;
- `GambaPityFallbackDrop` is gone;
- no hex or RVA literal reaches a call;
- in the research build, `gambaprobe hook` refuses while `gambapity` holds one
  of its two spliced scripts, and `gambapity` refuses while `gambaprobe` holds
  them, each naming the holder.
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
SDK_PYTHON = ROOT.parent / "hs-game-sdk" / "python"
SDK_PLAYER = ROOT.parent / "hs-game-sdk" / "cpp" / "include" / "hs_game_sdk" / "player.hpp"
MODS_SVELTE = ROOT / "panel" / "src" / "tabs" / "Mods.svelte"

if str(TESTS_DIR) not in sys.path:
    sys.path.insert(0, str(TESTS_DIR))

from test_release_hook_contract import function_body, strip_comments, strip_research_blocks  # noqa: E402

BLOCK_START = "// gambapity (ForgePact #134 phase 6, player build): the Goburin's Head pity"
BLOCK_END = "static void RunCommand(const std::string& line)"

# The charm's repository identifier: type, sub, base.
CHARM = (10, 0, 98)
MACHINE = "Slot_Machine_01_obj"
DESTROYED_SPRITE = "Slot_Machine_01_Destroyed_spr"
# The payout force (phase 3), retired before it shipped.
RETIRED = ("GambaPityForceParams", "GambaPityForceType", "g_GambaPityForcePending", "GambaPityIsPrizeBuild",
           "GambaPityLogPrizeBuild", "OnPrizeRoll", "gambapity: prize build")
# The spin hook (phase 5), retired when the count became explosions (phase 6).
RETIRED_SPIN = ("fp_gambapity_spin", "GambaPitySpinDetour", "g_GambaPitySpinOrig", "g_GambaPitySpinNative",
                "GambaPityIsSpin", "kGambaPitySpinGold")


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

    def test_the_hooks_are_spliced_trampolines_and_nothing_is_hooked_of_its_own(self):
        install = self.body("static std::string GambaPityInstallHooks()")
        # The spin hook is retired: gambapity hooks nothing itself.
        self.assertNotIn("HookOneScript", self.code)
        self.assertNotIn("PickUpGoldCheck", self.code)
        # CreateDefaultParams and CreateItemNew are already held by the Angelic
        # roll's hooks: each is spliced into that saved trampoline, never hooked
        # a second time.
        self.assertIn("if (!g_Orig_CreateDefaultParams) InstallSignatureAngelicHooks();", install)
        self.assertIn("g_GambaPityCdpOrig = g_Orig_CreateDefaultParams;", install)
        self.assertIn("g_Orig_CreateDefaultParams = reinterpret_cast<PFUNC_YYGMLScript>(GambaPityCdpDetour);", install)
        self.assertIn("if (!g_Orig_CreateItemNew) InstallSignatureAngelicHooks();", install)
        self.assertIn("g_GambaPityItemOrig = g_Orig_CreateItemNew;", install)
        self.assertIn("g_Orig_CreateItemNew = reinterpret_cast<PFUNC_YYGMLScript>(GambaPityItemDetour);", install)
        # Negative control: the install is the one read, and the SDK still
        # names the retired script (so its absence here is a choice).
        self.assertIn("GambaPityCharmConstantsMatch()", install)
        self.assertIn("gml_Script_PickUpGoldCheck", sdk_scripts())

    def test_the_spin_hook_is_retired(self):
        for word in RETIRED_SPIN:
            self.assertNotIn(word, self.code, word + " survives in gambapity's code")
            self.assertNotIn(word, strip_comments(self.plugin), word + " survives in ModuleMain.cpp")
        for word in ("OnSpin", "kGoldPerSpin", "GoldEquivalent"):
            self.assertNotIn(word, self.header, word + " survives in GambaPity.hpp")
            self.assertNotIn(word, self.code, word + " survives in gambapity's code")

    def test_a_table_only_hook_is_refused_with_the_row_named(self):
        install = self.body("static std::string GambaPityInstallHooks()")
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
        # gambapity's own code spells out no kind check. It does go through one
        # unmasked comparison, IsNumericInstanceRead, for a machine's
        # sprite_index and an instance's id; a flagged kind there reads as
        # unread, which is counted (`unread=`) or refuses the force (`ground
        # unread`), never a silent "nothing here".
        for word in ("m_Kind", "VALUE_OBJECT", "VALUE_REF", "VALUE_REAL"):
            self.assertNotIn(word, self.code, word + ": a raw kind check spelled out in gambapity's own code")
        self.assertIn("IsNumericInstanceRead(spr)", self.body("static std::string GambaPitySpriteName("))
        self.assertIn("IsNumericInstanceRead(id)", self.body("static int64_t GambaPityIdOf("))
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

    # ---- the payout force is retired ---------------------------------------

    def test_the_payout_force_is_gone(self):
        for word in RETIRED:
            self.assertNotIn(word, self.plugin, word + " survives in ModuleMain.cpp")
            self.assertNotIn(word, self.header, word + " survives in GambaPity.hpp")
        # Neither splice writes a struct: the game's own build runs unchanged.
        for signature in ("static RValue& GambaPityCdpDetour(", "static RValue& GambaPityItemDetour("):
            splice = self.body(signature)
            self.assertNotIn("variable_struct_set", splice, signature)
            self.assertNotIn("variable_instance_set", splice, signature)
        self.assertNotIn("variable_struct_set", self.code)
        # Negative control: the splices are the ones read, not empty slices.
        self.assertIn("g_GambaPityCdpOrig(S, O, R, argc, A)", self.body("static RValue& GambaPityCdpDetour("))

    def test_the_create_default_params_splice_keeps_only_the_natural_reset(self):
        cdp = self.body("static RValue& GambaPityCdpDetour(")
        # The original runs first and always.
        self.assertLess(cdp.index("g_GambaPityCdpOrig(S, O, R, argc, A)"), cdp.index("GambaPityIsCharmBuild(argc, A)"))
        self.assertIn("g_GambaPity.Enabled() && GambaPityIsCharmBuild(argc, A) && GambaPityIsMachine(S)", cdp)
        self.assertIn("g_GambaPity.OnMachineCharmBuild(GambaPityFrame());", cdp)
        self.assertIn("Out(ForgePact::GambaPity::Pity::NaturalBuildLine());", cdp)
        self.assertNotIn("A[2]", cdp, "the third argument (the payout's) is no input")

    def test_the_create_item_new_splice_is_a_read_after_return_head_detector(self):
        item = self.body("static RValue& GambaPityItemDetour(")
        self.assertLess(item.index("g_GambaPityItemOrig(S, O, R, argc, A)"), item.index("GambaPityIsCharmItem(built)"))
        self.assertIn("g_GambaPity.OnHeadBuild(GambaPityFrame());", item)
        self.assertNotIn("A[0]", item, "the detector reads what was built, not the arguments")

    def test_the_natural_reset_keys_on_create_default_params_args_0_98(self):
        build = self.body("static bool GambaPityIsCharmBuild(")
        self.assertIn("SigNumber(*A[0], s)", build)
        self.assertIn("SigNumber(*A[1], b)", build)
        self.assertIn("kGambaPityCharmSub", build)
        self.assertIn("kGambaPityCharmBase", build)
        # The third argument is not guessed: no A[2] read here.
        self.assertNotIn("A[2]", build)

    # ---- the trigger: the end-of-frame sprite poll -------------------------

    def test_the_tick_runs_from_frame_callback_and_returns_at_once_while_off(self):
        frame = strip_comments(function_body(self.plugin, "void FrameCallback(FWFrame& FrameContext)"))
        self.assertIn("if (g_Setup) GambaPityTick();", frame)
        shipped = strip_comments(function_body(strip_research_blocks(self.plugin), "void FrameCallback(FWFrame& FrameContext)"))
        self.assertIn("if (g_Setup) GambaPityTick();", shipped, "the tick is a player-build tick")
        tick = [line.strip() for line in self.body("static void GambaPityTick()").split("\n") if line.strip()]
        tick = [line for line in tick if line != "{"]
        self.assertEqual(tick[0], "if (!g_GambaPity.Enabled() || !g_GambaPityHooked) return;", tick)
        body = "\n".join(tick)
        # An unreadable room sees and decides nothing (a sentinel never equals a room).
        self.assertLess(body.index("if (room == INT64_MIN) return;"), body.index("GambaPityWatchMachines(frame);"))
        self.assertIn("g_GambaPity.TakeDue(frame)", body)

    def test_the_machines_are_read_by_the_instance_handle_rule_and_their_sprite_by_name(self):
        watch = self.body("static void GambaPityWatchMachines(")
        self.assertIn('"instance_number"', watch)
        self.assertIn('"instance_find"', watch)
        self.assertIn("sprite == kGambaPityDestroyedSprite", watch)
        # instance_find on the machine's own object already names the machine:
        # no second identification that could drop a machine silently.
        self.assertNotIn("HhResolveInstance", watch)
        # Every skipped machine is counted (`unread=`): the instance_number
        # throw, the instances past the cap, the instance_find throw and an id,
        # sprite or position that does not read.
        self.assertEqual(watch.count("g_GambaPity.NoteMachineUnread();"), 4)
        self.assertIn('if (id < 0 || sprite == "?" || !GambaPityXY(handle, x, y)) {', watch)
        self.assertIn("g_GambaPity.SetBaseline(id, scan.heads, scan.read);", watch)
        self.assertIn('"unread (" + scan.stage + ")"', watch)
        # Round 2 (F2): a baseline that did not read is retried while the
        # machine is live, throttled by the core, and the first scan that
        # reads sets it (an unread one never replaces a read one, in the core).
        retry = watch[watch.index("case GP::Sighting::None:"):]
        self.assertIn("if (g_GambaPity.NeedsBaseline(id, frame)) {", retry)
        self.assertLess(retry.index("g_GambaPity.NeedsBaseline(id, frame)"), retry.index("GambaPityGroundHeads(x, y)"))
        self.assertLess(retry.index("GambaPityGroundHeads(x, y)"), retry.index("g_GambaPity.SetBaseline(id, scan.heads, scan.read);"))
        self.assertIn("if (scan.read) Out(GP::Pity::BaselineReadLine(id, (int)scan.heads.size()));", retry)
        self.assertEqual(watch.count("g_GambaPity.SetBaseline(id, scan.heads, scan.read);"), 2)
        self.assertNotIn("Sighting::Exploded", retry)
        needs = function_body(self.header, "bool NeedsBaseline(int64_t id, int64_t frame)")
        self.assertIn("it->second.destroyed || it->second.baselineRead", needs)
        self.assertIn("it->second.nextBaselineTry = frame + kBaselineRetryFrames;", needs)
        self.assertIn("inline constexpr int64_t kBaselineRetryFrames = 30;", self.header)
        baseline = function_body(self.header, "void SetBaseline(int64_t id, const std::vector<int64_t>& heads, bool read = true)")
        self.assertIn("(it->second.baselineRead && !read)) return;", baseline)
        sprite = self.body("static std::string GambaPitySpriteName(")
        self.assertIn('RValue("sprite_index")', sprite)
        self.assertIn("IsNumericInstanceRead(spr)", sprite)
        self.assertIn('"sprite_exists"', sprite)
        self.assertIn('"sprite_get_name"', sprite)
        # First sight takes the baseline and prints the poll's positive control.
        self.assertIn("GP::Pity::MachineSeenLine(", watch)
        self.assertIn("g_GambaPity.ExplosionLine(id, frame)", watch)

    def test_an_explosions_addition_is_saved_when_it_is_seen(self):
        watch = self.body("static void GambaPityWatchMachines(")
        exploded = watch[watch.index("case GP::Sighting::Exploded:"):]
        exploded = exploded[:exploded.index("break;")]
        self.assertIn("GambaPitySave();", exploded)
        self.assertLess(exploded.index("GambaPitySave();"), exploded.index("Out(g_GambaPity.ExplosionLine(id, frame));"))
        # The resets save too: a confirmed force, a natural head at the
        # explosion and a machine-self (0, 98) build.
        decide = self.body("static void GambaPityDecide(")
        self.assertEqual(decide.count("GambaPitySave();"), 2, "the natural and the confirmed-force resets")
        self.assertIn("GambaPitySave();", self.body("static RValue& GambaPityCdpDetour("))

    def test_the_destroyed_sprite_is_a_python_sdk_sprite_name(self):
        self.assertIn('static constexpr const char* kGambaPityDestroyedSprite = "' + DESTROYED_SPRITE + '";', self.code)
        if not SDK_PYTHON.exists():
            raise unittest.SkipTest(f"hs-game-sdk python binding not found at {SDK_PYTHON}")
        if str(SDK_PYTHON) not in sys.path:
            sys.path.insert(0, str(SDK_PYTHON))
        from hs_game_sdk import SPRITE_NAME_TO_INDEX
        self.assertGreater(len(SPRITE_NAME_TO_INDEX), 1000, "the sprite table did not load")
        self.assertIn(DESTROYED_SPRITE, SPRITE_NAME_TO_INDEX)
        self.assertIn("Slot_Machine_01_spr", SPRITE_NAME_TO_INDEX)   # control: the live sprite is there too
        # By name, never by a hand-written sprite index.
        self.assertIsNone(re.search(r"\b" + str(SPRITE_NAME_TO_INDEX[DESTROYED_SPRITE]) + r"\b", self.code))

    def test_no_gp_symbol_in_gambapitys_code(self):
        self.assertIsNone(re.search(r"\bGp[A-Z0-9_]\w*", strip_research_blocks(self.code)),
                          "gambapity's player code names a research-only Gp* symbol")

    # ---- the ground check --------------------------------------------------

    def test_the_ground_check_reads_the_ground_items_charm_identity(self):
        ground = self.body("static GambaPityGroundScan GambaPityGroundHeads(")
        self.assertIn("HeroSiege::Player::kGroundItemInstanceField", ground)
        self.assertIn("GambaPityReadCharm(item)", ground)
        self.assertIn("ForgePact::GambaPity::kGroundRadius", ground)
        # "Scanned, none" and "could not look" are told apart: `read` is set
        # only at the end, and every way the scan can fail names its stage.
        self.assertEqual(ground.count("scan.read = true;"), 1)
        self.assertLess(ground.index("scan.stage = \"the scan threw\";"), ground.index("scan.read = true;"))
        for stage in ("Loot_Ground_obj did not resolve", "a ground item did not read",
                      "a ground item near the machine did not read", "the scan threw"):
            self.assertIn('"' + stage + '"', ground)
        # Only an item read as not the charm is passed over; an unread one stops the scan.
        self.assertIn("if (charm == GambaPityCharmRead::NotCharm) continue;", ground)
        self.assertIn("HeroSiege::Objects::GameObject::Loot_Ground_obj", self.body("static int GambaPityLootObject()"))
        charm = self.body("static GambaPityCharmRead GambaPityReadCharm(")
        self.assertIn("HeroSiege::Player::kItemInstanceTypeField", charm)
        self.assertIn("HeroSiege::Player::kItemInstanceDefinitionField", charm)
        self.assertIn("if (type != (double)kGambaPityCharmType) return GambaPityCharmRead::NotCharm;", charm)
        self.assertIn('!GambaPityNumberField(def, "j", j) || !GambaPityNumberField(def, "b", b)) return GambaPityCharmRead::Unread;',
                      charm)
        self.assertIn("j == (double)kGambaPityCharmSub && b == (double)kGambaPityCharmBase", charm)
        self.assertIn("return GambaPityReadCharm(item) == GambaPityCharmRead::Charm;", self.body("static bool GambaPityIsCharmItem("))
        # The SDK names are the ones the relic reads measured (section 10.7).
        if not SDK_PLAYER.exists():
            raise unittest.SkipTest(f"hs-game-sdk header not found at {SDK_PLAYER}")
        player = SDK_PLAYER.read_text(encoding="utf-8")
        self.assertIn('kGroundItemInstanceField = "itemInstance"', player)
        self.assertIn('kItemInstanceTypeField = "itemType"', player)
        self.assertIn('kItemInstanceDefinitionField = "itemDefinitionStruct"', player)

    # ---- the decision at the point of use and the forced drop --------------

    def test_the_ground_check_runs_before_the_force_and_again_after_the_drop(self):
        decide = self.body("static void GambaPityDecide(")
        decision = "g_GambaPity.Decide(e, room, ground.heads, ground.read)"
        self.assertLess(decide.index("GambaPityGroundHeads(e.x, e.y)"), decide.index(decision))
        self.assertLess(decide.index(decision), decide.index("GambaPityDropHead("))
        # An unread scan refuses the force by name, before any drop.
        self.assertIn('GP::Pity::RefusedLine("ground unread ("', decide)
        self.assertLess(decide.index("case GP::Outcome::GroundUnread:"), decide.index("GambaPityDropHead("))
        # The after-drop check names its stage when it does not read.
        self.assertIn("GP::Pity::GroundAfterDropUnreadLine(after.stage)", decide)
        # Round 2 (F3): a below outcome whose ground did not read says so,
        # with the same stage the refusal names.
        self.assertIn("const std::string unreadStage = ground.read ? std::string(\"the machine's baseline scan\") : ground.stage;",
                      decide)
        self.assertIn('GP::Pity::RefusedLine("ground unread (" + unreadStage + ")")', decide)
        self.assertIn("Out(g_GambaPity.BelowLine(e, d.groundUnread ? unreadStage : std::string()));", decide)
        below = self.header[self.header.index("++below_;"):]
        below = below[:below.index("return d;")]
        self.assertIn("if (!groundRead || !baselineRead) {", below)
        self.assertIn("++belowGroundUnread_;", below)
        self.assertIn("d.groundUnread = true;", below)
        self.assertLess(decide.index("GambaPityDropHead("), decide.index("GP::Pity::GroundAfterDropLine("))
        self.assertEqual(decide.count("GambaPityGroundHeads("), 2)
        # The own-drop scope is around the drop.
        self.assertLess(decide.index("GambaPityOwnDropScope own;"), decide.index("GambaPityDropHead("))
        self.assertIn("GambaPityOwnDropScope() { g_GambaPity.BeginOwnDrop(); }", self.code)
        self.assertIn("~GambaPityOwnDropScope() { g_GambaPity.EndOwnDrop(); }", self.code)
        # A confirmed drop resets, a refused one keeps the counter.
        self.assertIn("g_GambaPity.ForceRefused();", decide)
        self.assertIn("g_GambaPity.ForceConfirmed(e, groundId);", decide)

    def test_a_reset_takes_only_its_explosions_standing_and_each_explosion_forces_on_its_own(self):
        # A confirmed force and a natural head take this explosion's addition
        # and every one before it; a later pending explosion's addition stays.
        confirmed = function_body(self.header, "void ForceConfirmed(const Explosion& e, int64_t groundId)")
        self.assertIn("ResetThrough(e);", confirmed)
        self.assertNotIn("count_ = 0;", confirmed)
        decide = function_body(self.header, "Decision Decide(const Explosion& e, int64_t room, const std::vector<int64_t>& ground, bool groundRead = true)")
        natural = decide[decide.index("d.outcome = Outcome::Natural;"):]
        natural = natural[:natural.index("return d;")]
        self.assertIn("ResetThrough(e);", natural)
        self.assertNotIn("OnNaturalDrop();", natural)
        # One head per explosion: each explosion forces on its own standing in
        # the count, with no gate over a pending set (the owner's rule is per
        # explosion), so at threshold 1 explosions pending together each force.
        self.assertIn("if (enabled_ && threshold_ > 0 && Position(e) >= threshold_) {", decide)
        self.assertNotIn("forcedThrough_", self.header)
        # The below line shows the explosion's own standing, the number it was
        # decided on, not the counter.
        below = function_body(self.header, "std::string BelowLine(const Explosion& e, const std::string& unreadStage = std::string()) const")
        self.assertIn("Position(e)", below)
        self.assertNotIn("count_", below)
        reset = function_body(self.header, "void ResetThrough(const Explosion& e)")
        self.assertIn("const int64_t after = added_ - e.addSeq;", reset)
        self.assertIn("clearedThrough_ = e.addSeq;", reset)
        # The machine-self (0, 98) build has no explosion: every addition goes.
        drop = function_body(self.header, "void OnNaturalDrop()")
        self.assertIn("count_ = 0;", drop)
        self.assertIn("clearedThrough_ = added_;", drop)
        self.assertIn("OnNaturalDrop();", function_body(self.header, "void OnMachineCharmBuild(int64_t frame)"))

    def test_the_forced_drop_is_the_loader_route_with_a_bounded_retry_and_a_read_back(self):
        drop = self.body("static std::string GambaPityDropHead(")
        self.assertIn("static constexpr int kGambaPityDropAttempts = 3;", self.code)
        self.assertIn("for (attempt = 1; attempt <= kGambaPityDropAttempts; ++attempt)", drop)
        order = [drop.index(s) for s in ('"json_parse"', '"gml_Script_InitItemFromJson"',
                                         '"gml_Script_LootGroundCreateFromItem"', '"instance_exists"')]
        self.assertEqual(order, sorted(order))
        self.assertIn("HhResolveLocalPlayer(playerValue)", drop)
        self.assertIn("player, player,", drop)
        self.assertIn('return "no local player";', drop)
        # The charm's numbers come from its kAngelicBases row, not new literals.
        self.assertIn("GambaPityCharmRow()", drop)
        self.assertIn("row->sub", drop)
        self.assertIn("row->b", drop)
        self.assertNotIn('\\"b\\":98', drop)
        # A throw never retries (a head may have been placed).
        self.assertIn('return "the drop threw";', drop)
        # Each attempt's key moves on by one millisecond, so keys never collide.
        self.assertIn('"0-0-" + std::to_string(ms + attempt - 1) + "-"', drop)
        self.assertLess(drop.index("const long long ms ="), drop.index("for (attempt = 1;"))
        # The refusal after the last attempt names what the call returned.
        self.assertIn('"LootGroundCreateFromItem returned no live instance (returned " + returned + ")"', drop)
        self.assertIn("returned = Describe(placed);", drop)

    def test_the_status_line_and_the_action_lines_are_the_cores_fixed_text(self):
        for name in ("MachineSeenLine", "ExplosionLine", "ForcedLine", "GroundAfterDropLine", "GroundAfterDropUnreadLine",
                     "NaturalSeenLine",
                     "BelowLine", "RefusedLine", "AbandonedLine", "NaturalBuildLine", "BaselineReadLine", "MigrationLine",
                     "VersionErrorText"):
            self.assertIn(name + "(", self.code, name + " is never printed")
            self.assertIn(name + "(", self.header)
        line = self.header[self.header.index("std::string StatusLine() const"):]
        line = line[:line.index("}")]
        for field in ("count", "threshold", "explosions", "forced", "natural", "below", "refused",
                      "abandoned", "own-head-builds", "machines", "unread", "ground-unread", "below-ground-unread"):
            self.assertIn('" ' + field + '="', line, field)
        # The count is explosions: the status line has no gold field.
        self.assertNotIn('" gold="', line)
        self.assertNotIn("gold=", self.header)
        # Every line the adapter prints itself keeps the gambapity prefix.
        printed = re.findall(r'Out\("([^"]*)', strip_research_blocks(self.code))
        self.assertTrue(printed)
        for text in printed:
            self.assertTrue(text.startswith("gambapity: "), text)

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
        self.assertIn("std::filesystem::file_size(path, ec) < 4096", load)

    def test_the_counter_file_goes_through_the_cores_text_both_ways(self):
        save = self.body("static void GambaPitySave()")
        self.assertIn("out << ForgePact::GambaPity::CounterFileText(g_GambaPity.Count());", save)
        load = self.body("static void GambaPityLoad()")
        self.assertIn("const GP::CounterFile file = GP::ParseCounterFile(text);", load)
        self.assertIn("g_GambaPity.SetCount(file.count);", load)
        # No hand parse of the count beside the core's.
        self.assertNotIn('"\\"count\\""', load)
        self.assertNotIn("std::stoi", load)
        # An older spin file: the migration line once (the load runs once), and
        # the file rewritten as version 2 at once.
        legacy = load[load.index("if (file.legacy) {"):]
        legacy = legacy[:legacy.index("}")]
        self.assertLess(legacy.index("Out(GP::Pity::MigrationLine());"), legacy.index("GambaPitySave();"))
        # Another version: the status line's error, the file left alone.
        unknown = load[load.index("} else if (file.unknown) {"):]
        unknown = unknown[:unknown.index("}", 1)]
        self.assertIn("g_GambaPityError = GP::Pity::VersionErrorText(file.version);", unknown)
        self.assertIn("g_GambaPityVersionError = true;", unknown)
        self.assertNotIn("GambaPitySave();", unknown)
        # The first save that lands replaces that file, and the version error
        # goes with it; a save that fails names itself instead, and the next
        # save that lands clears that too.
        landed = save[save.index("std::filesystem::rename(tmp, path, ec);"):]
        self.assertLess(landed.index("if (ec) {"), landed.index("if (g_GambaPityVersionError || g_GambaPitySaveError) {"))
        cleared = landed[landed.index("if (g_GambaPityVersionError || g_GambaPitySaveError) {"):]
        cleared = cleared[:cleared.index("}")]
        self.assertIn("g_GambaPityError.clear();", cleared)
        self.assertIn("g_GambaPityVersionError = false;", cleared)
        self.assertIn("g_GambaPitySaveError = false;", cleared)
        self.assertEqual(load.count("GambaPitySave();"), 1)

    def test_a_refused_save_removes_its_tmp_and_flags_itself(self):
        # Live 1 (2026-10-06): a refused rename left forgepact_gamba_pity.json.tmp
        # beside the file. The refusal removes it before it returns.
        save = self.body("static void GambaPitySave()")
        refused = save[save.index("if (ec) {"):]
        refused = refused[:refused.index("return;")]
        self.assertIn("std::filesystem::remove(tmp, removed);", refused)
        self.assertIn('g_GambaPityError = "could not save " + path.string();', refused)
        self.assertIn("g_GambaPitySaveError = true;", refused)
        caught = save[save.index("} catch (...) {"):]
        self.assertIn("g_GambaPitySaveError = true;", caught)
        self.assertIn("static bool g_GambaPitySaveError = false;", self.block)

    def test_the_load_closes_its_stream_before_the_migration_saves(self):
        # Live 1 (2026-10-06): the migration renamed the .tmp over a file the
        # load still held open, which Windows refuses, so the older file stayed.
        # The stream's scope ends (or it is closed) before GambaPitySave.
        load = self.body("static void GambaPityLoad()")
        opened = load.index("std::ifstream in(path, std::ios::binary);")
        saved = load.index("GambaPitySave();")
        self.assertLess(opened, saved)
        between = load[opened:saved]
        depth, closed = 0, False
        for ch in between:
            if ch == "{":
                depth += 1
            elif ch == "}":
                depth -= 1
                if depth < 0:
                    closed = True
                    break
        self.assertTrue(closed or "in.close();" in between,
                        "the ifstream is still in scope at the migration's GambaPitySave()")
        # The core writes exactly version 2.
        self.assertIn("inline constexpr int64_t kCounterFileVersion = 2;", self.header)
        self.assertIn("inline std::string CounterFileText(int count)", self.header)
        self.assertIn("inline CounterFile ParseCounterFile(const std::string& text)", self.header)

    # ---- the status line -----------------------------------------------------

    def test_status_names_count_and_threshold_and_surfaces_the_error(self):
        line = self.header[self.header.index("std::string StatusLine() const"):]
        self.assertIn('" count="', line)
        self.assertIn('" threshold="', line)
        self.assertIn('" explosions="', line)
        self.assertIn('" own-head-builds="', line)
        self.assertNotIn("GoldEquivalent", self.header)
        command = self.body("static void GambaPityCommand(")
        self.assertIn('if (arg.empty() || arg == "status" || arg == "stat") {', command)
        # The last load/save refusal is read beside the status line, not only
        # written (the gems: saved pattern).
        self.assertIn("g_GambaPity.StatusLine() + (g_GambaPityError.empty() ? std::string() : \" - \" + g_GambaPityError)",
                      command)

    def test_the_plugin_range_matches_python_and_the_panel(self):
        self.assertIn("static constexpr int kGambaPityMin = 1;", self.code)
        self.assertIn("static constexpr int kGambaPityMax = 200;", self.code)
        command = self.body("static void GambaPityCommand(")
        self.assertIn("count < kGambaPityMin || count > kGambaPityMax", command)
        python = FORGEPACT_PY.read_text(encoding="utf-8-sig").replace("\r\n", "\n")
        self.assertIn("GAMBA_PITY_RANGE = (1, 200)", python)
        self.assertIn("GAMBA_PITY_DEFAULT = 10", python)
        self.assertIn('"mod_gambapity": False,\n    "gambapity": 10,', python)
        svelte = MODS_SVELTE.read_text(encoding="utf-8").replace("\r\n", "\n")
        self.assertIn('id="gambapity" min="1" max="200" step="1" value="10"', svelte)

    def test_the_slider_toast_names_the_ordinal_explosion_without_a_head(self):
        from test_release_hook_contract import function_body as body
        from test_panel_performance import run_node
        panel = (ROOT / "panel" / "src" / "panel.js").read_text(encoding="utf-8").replace("\r\n", "\n")
        self.assertIn("toast('Goburin\\'s Head pity: drops at the '+gambapityOrdinal(v)+' explosion without a head'"
                      "+(on?'':' (while on)')+' - '", panel)
        ordinal = "function gambapityOrdinal(n){" + body(panel, "function gambapityOrdinal(n)") + "}"
        values = [1, 2, 3, 4, 10, 11, 12, 13, 20, 21, 22, 23, 111]
        got = run_node(ordinal, f"console.log(JSON.stringify({values}.map(gambapityOrdinal)));")
        self.assertEqual(got, ["1st", "2nd", "3rd", "4th", "10th", "11th", "12th", "13th", "20th", "21st", "22nd",
                               "23rd", "111th"])

    def test_the_slider_posts_and_toasts_a_whole_count_and_says_while_on_when_off(self):
        # PR #180 review: a typed 3.4 was posted and toasted as "3.4th" while the
        # server keeps round(3.4) = 3, and the toast promised a drop with the
        # switch off. Runs the real handler line, sliderVal and the ordinal in
        # node, typed values, switch off then on (the on run is the control).
        from test_panel_performance import run_node
        panel = (ROOT / "panel" / "src" / "panel.js").read_text(encoding="utf-8").replace("\r\n", "\n")
        self.assertIn("gpp.oninput=()=>gambapityPaint(Math.round(sliderVal(gpp)));", panel)
        handler = next((line.strip() for line in panel.splitlines() if line.strip().startswith("gpp.onchange=")), None)
        self.assertIsNotNone(handler, "panel.js has no gpp.onchange handler")
        helpers = ("function sliderVal(r){" + function_body(panel, "function sliderVal(r)") + "}\n"
                   "function gambapityOrdinal(n){" + function_body(panel, "function gambapityOrdinal(n)") + "}\n")
        stubs = ("let on=false;const posted=[],toasts=[];\n"
                 "const document={getElementById:(id)=>id==='mod_gambapity'?{checked:on}:null};\n"
                 "const j=async(url,o)=>{posted.push(JSON.parse(o.body));return {ok:'saved'}};\n"
                 "const toast=(t)=>toasts.push(t);\n"
                 "const gpp={value:'3.4',step:'any',dataset:{typed:'1',step0:'1'}};\n")
        driver = ("(async()=>{await gpp.onchange();on=true;gpp.value='12.6';await gpp.onchange();"
                  "console.log(JSON.stringify({posted,toasts}));})();")
        got = run_node(helpers + stubs + handler, driver)
        self.assertEqual(got["posted"], [{"key": "gambapity", "value": 3}, {"key": "gambapity", "value": 13}])
        self.assertEqual(got["toasts"], [
            "Goburin's Head pity: drops at the 3rd explosion without a head (while on) - saved",
            "Goburin's Head pity: drops at the 13th explosion without a head - saved",
        ])

    def test_the_readme_has_the_row_the_card_list_entry_and_the_section(self):
        # PR #180 review: README had nothing on gambapity; its Quality of Life
        # siblings each have a features-table row, a card-list mention and a section.
        readme = (ROOT / "README.md").read_text(encoding="utf-8").replace("\r\n", "\n")
        row = next((line for line in readme.splitlines() if line.startswith("| **Goburin's Head pity** |")), None)
        self.assertIsNotNone(row, "the features table has no Goburin's Head pity row")
        self.assertIn("Mods → Quality of Life, off by default", row)
        self.assertIn("(#goburins-head-pity)", row)
        self.assertIn("the timed skill countdown and Goburin's Head pity)", readme)
        self.assertRegex(readme, r"(?m)^## Goburin's Head pity$")
        section = readme.split("\n## Goburin's Head pity\n", 1)[1].split("\n## ", 1)[0]
        low = " ".join(section.split()).lower()
        for phrase in ("off by default", "from 1 to 200", "one head per explosion", "the count starts over",
                       "forgepact_gamba_pity.json", "`gambapity status`", "`gambapity off`",
                       "have not been observed"):
            self.assertIn(phrase, low)

    def _load_cfg_from(self, saved):
        if str(SDK_PYTHON) not in sys.path:
            sys.path.insert(0, str(SDK_PYTHON))
        src = str(ROOT / "src")
        if src not in sys.path:
            sys.path.insert(0, src)
        import json
        import tempfile
        from unittest import mock
        import forgepact  # noqa: E402  (only here: the other tests read source text)
        with tempfile.TemporaryDirectory() as tmp:
            config = Path(tmp) / "forgepact.json"
            config.write_text(json.dumps(saved), encoding="utf-8")
            with mock.patch.object(forgepact, "CONFIG", config):
                return forgepact.load_cfg(), forgepact

    def test_a_saved_count_outside_the_range_loads_as_the_default(self):
        # A count above the range (1000, the top of the unshipped spin count's
        # range) loads as the default, so the panel shows and sends 10.
        cfg, forgepact = self._load_cfg_from({"mod_gambapity": True, "gambapity": 1000})
        self.assertEqual(cfg["gambapity"], 10)
        self.assertEqual(forgepact.gambapity_cmd(cfg), "gambapity 10")
        self.assertTrue(cfg["mod_gambapity"])
        # Control: the top of the range itself is kept.
        cfg, forgepact = self._load_cfg_from({"mod_gambapity": True, "gambapity": 200})
        self.assertEqual(cfg["gambapity"], 200)
        self.assertEqual(forgepact.gambapity_cmd(cfg), "gambapity 200")

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
        # No file I/O in the core either: the adapter reads and writes the file.
        for word in ("fstream", "std::filesystem", "FILE*"):
            self.assertNotIn(word, self.header)
        self.assertIn("class Pity", self.header)   # control: the header is the one read

    # ---- coexistence with gambaprobe (research build) -----------------------

    def test_gambaprobe_refuses_while_gambapity_holds_the_shared_scripts(self):
        install = self.body("static void GpInstall()")
        first = install.index("GpInstallEvents(mainMod);")
        self.assertLess(install.index("GambaPityHeldScripts()"), first, "gambapity is checked after a row is installed")
        self.assertIn('"gambaprobe hook: refused - gambapity holds "', install)
        held = self.body("static std::string GambaPityHeldScripts()")
        self.assertIn('held += "CreateDefaultParams"', held)
        self.assertIn('held += (held.empty() ? "" : ", ") + std::string("CreateItemNew")', held)
        # Only the two spliced scripts: the spin hook is retired.
        self.assertNotIn("PickUpGoldCheck", held)

    def test_gambapity_refuses_while_gambaprobe_holds_the_shared_scripts(self):
        install = self.body("static std::string GambaPityInstallHooks()")
        self.assertIn('"gambaprobe holds "', install)
        self.assertIn("kGpScript_CreateDefaultParams", install)
        self.assertIn("kGpScript_CreateItemNew", install)
        # A gambaprobe holding only PickUpGoldCheck no longer blocks gambapity.
        self.assertNotIn("kGpScript_PickUpGoldCheck", install)
        self.assertNotIn("PickUpGoldCheck", install)
        # The check is research-build only: the player build never names gambaprobe.
        self.assertNotIn("kGpScript_CreateDefaultParams", self.shipped)


if __name__ == "__main__":
    unittest.main()
