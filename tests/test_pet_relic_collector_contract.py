"""Contract tests for Pet Collects Relics (ForgePact issue #124).

While the pet is out it walks to relics on screen and picks them up through the
game's own loot pickup (`PickupLoot`, the companion's call shape), never one the
player already owns at 10/10. The choice itself - the maxed filter, the maxed
cache, the arbiter between the two collectors - is pinned by
test_pet_relic_collector_behavior.py against the real header. These pin how
ModuleMain.cpp wires it, on comment-stripped source: the switch, the maxed set's
source, the one travel helper both ticks share, the one collect call site with
the shape ForgePact/docs/pet-relic-collector-research.md § The mechanism
records, the destroy rule, and that the research instruments stay out of the
player build. The two rules most likely to regress carry a negative control: the
same check, run on the source with the rule broken, must fail.
"""

import re
import sys
import unittest
from pathlib import Path

TESTS_DIR = Path(__file__).resolve().parent
ROOT = TESTS_DIR.parent
REPO_ROOT = ROOT.parent
SRC_DIR = ROOT / "src"
SDK_PY_PATH = REPO_ROOT / "hs-game-sdk" / "python"
for p in (TESTS_DIR, SRC_DIR, SDK_PY_PATH):
    if str(p) not in sys.path:
        sys.path.insert(0, str(p))

import forgepact  # noqa: E402
from test_release_hook_contract import function_body, strip_comments, strip_research_blocks  # noqa: E402

PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"
HEADER = ROOT / "plugin" / "include" / "ForgePact" / "PetRelicCollectorMod.hpp"


def filtered_before_pick(tick):
    """The candidates the selector picks from are FilterRelicCandidates' output."""
    m = re.search(r"(\w+)\s*=\s*ForgePact::FilterRelicCandidates\(", tick)
    if not m:
        return False
    pick = tick.find(f"g_PetRelicSelector.Pick({m.group(1)},")
    return pick > m.start()


def destroy_only_after_true_return(collect):
    """Every instance_destroy in the collect sits after the false-return block
    (which returns) and before the closing catch: the true-return branch."""
    false_branch = collect.find("if (!result.ToBoolean())")
    if false_branch < 0:
        return False
    depth, close = 0, None
    for i in range(collect.index("{", false_branch), len(collect)):
        if collect[i] == "{":
            depth += 1
        elif collect[i] == "}":
            depth -= 1
            if depth == 0:
                close = i
                break
    if close is None or "return" not in collect[false_branch:close]:
        return False
    last_catch = collect.rfind("catch (...)")
    destroys = [m.start() for m in re.finditer(r'"instance_destroy"', collect)]
    return bool(destroys) and all(close < d < last_catch for d in destroys)


def forcerelic_resolves_like_the_tick(body):
    """`forcerelic` finds the player through HhResolveLocalPlayer and its
    `self` through HhResolveInstance, as the relic tick does, with no raw
    instance_find/GetInstanceObject path or kind gate of its own (Live 1,
    session 1: that path printed `cannot resolve player CInstance` on every
    call while `petrelic census` saw the player)."""
    local = body.find("HhResolveLocalPlayer(")
    instance = body.find("HhResolveInstance(")
    return (0 <= local < instance
            and not any(raw in body for raw in ("GetInstanceObject", "instance_find", "m_Kind"))
            and "cannot resolve player CInstance" not in body)


def forcerelic_places_on_the_ground(place, one):
    """`forcerelic <n>` and `forcerelic ids` build each relic by name
    (InitItemFromJson, the global instance as `self`, a key ending in the relic
    class) and place it with LootGroundCreateFromItem (x, y, item) at the
    player's `self`; what comes back counts as placed only once instance_exists
    and ReadGroundRelic with the requested id agree, and the command's line
    carries the ground count before and after (Live 1, session 2: 91 two-argument
    DropRelic calls printed success and placed nothing)."""
    init = one.find('"gml_Script_InitItemFromJson", g, g, { parsed, RValue(key) }')
    create = one.find('"gml_Script_LootGroundCreateFromItem", self, self, { RValue(x), RValue(y), item }')
    exists = one.find('"instance_exists", { res }')
    read = one.find("HeroSiege::Player::ReadGroundRelic(g_Yytk, inst, read)")
    return (0 <= init < create < exists < read
            and "read.relicId != id" in one
            and '"-" + std::to_string(HeroSiege::Player::kRelicItemClass)' in one
            and "via LootGroundCreateFromItem at player (%.0f, %.0f); ground items %d -> %d; ids " in place
            and "g_Yytk->GetGlobalInstance(&g);" in place
            and "ForceRelicResolvePlayer(player, self, x, y)" in place
            and "g_Orig_DropRelic" not in place + one)


def forcerelic_drop_forces(drop):
    """`forcerelic drop` calls the original DropRelic with five arguments, the
    fifth true (it skips the chance roll), and counts what each call returned."""
    return ("RValue* argv[5] = { &ax, &ay, &a2, &a3, &force };" in drop
            and "RValue force(true);" in drop
            and "RValue a2(0.0), a3(0.0);" in drop
            and "g_Orig_DropRelic(self, self, tmp, 5, argv)" in drop
            and "returned true=%d false=%d other=%d; ground items %d -> %d" in drop)


def droprelic_call_argument_counts(source):
    """The literal argument count of every direct g_Orig_DropRelic call."""
    return [int(m.group(1)) for m in
            re.finditer(r"g_Orig_DropRelic\(\s*\w+\s*,\s*\w+\s*,\s*\w+\s*,\s*(\d+)\s*,", source)]


def collect_reads_through_a_handle(collect, handle):
    """PetTravelStep hands the collect the target's instance id, a plain number
    (`RValue(targetId)`, VALUE_REAL), and the SDK's ground read refuses anything
    IsInstanceHandle does not accept (OBJECT or REF) as NoHandle. So the collect
    turns the number into its Loot_Ground_obj instance first, refuses the collect
    by name when none carries the id, and reads the relic only through that
    instance. `handle` is LootGroundHandle's body: a number is matched against
    instance_find's Loot_Ground_obj instances by `id`, and only a handle is
    returned."""
    convert = collect.find("LootGroundHandle(target, inst)")
    read = collect.find("HeroSiege::Player::ReadGroundRelic(g_Yytk, inst, read)")
    return (0 <= convert < read
            and 'PetRelicRefuse("no ground handle")' in collect[convert:read]
            and "ReadGroundRelic(g_Yytk, target" not in collect
            and "if (HeroSiege::Player::IsInstanceHandle(value)) { inst = value; return true; }" in handle
            and "value.m_Kind != VALUE_REAL" in handle
            and '"instance_find", { RValue((double)g_LootGroundObjIdx), RValue((double)i) }' in handle
            and "if (!HeroSiege::Player::IsInstanceHandle(cand)) continue;" in handle
            and '{ cand, RValue("id") }).ToDouble() == want) { inst = cand; return true; }' in handle)


def refusal_after(body, guard):
    """The first Out(...) statement after `guard`, up to its semicolon."""
    out = body.index("Out(", body.index(guard))
    return body[out: body.index(";", out)]


# Live 1 (session 1)'s ForceRelicDrop shape, which the predicate must reject.
FORCERELIC_INSTANCE_FIND_SHAPE = """
    if (!g_Orig_DropRelic) { Out("forcerelic: DropRelic not hooked yet"); return; }
    try {
        RValue oi = g_Yytk->CallBuiltin("asset_get_index", { RValue("Player_obj") });
        RValue id = g_Yytk->CallBuiltin("instance_find", { oi, RValue(0.0) });
        CInstance* self = nullptr;
        g_Yytk->GetInstanceObject((int32_t)id.ToDouble(), self);
        if (!self) { Out("forcerelic: cannot resolve player CInstance"); return; }
        for (int i = 0; i < n; i++) { try { g_Orig_DropRelic(self, self, tmp, 2, argv); } catch (...) {} }
        sprintf_s(b, "forcerelic: %d relic call(s) at player (%.0f, %.0f)", n, px.ToDouble(), py.ToDouble());
    } catch (...) { Out("forcerelic EXCEPTION"); }
"""

# Live 1 (session 2)'s ForceRelicDrop body: the player resolved, then x and y
# alone handed to DropRelic, which rolls against an absent fourth argument and
# returns false (static reading). The new predicates must reject it.
FORCERELIC_TWO_ARGUMENT_SHAPE = """
        for (int i = 0; i < n; i++) {
            RValue ax = px; RValue ay = py;
            RValue* argv[2] = { &ax, &ay };
            RValue tmp;
            try { g_Orig_DropRelic(self, self, tmp, 2, argv); } catch (...) {}
        }
        char b[128];
        sprintf_s(b, "forcerelic: %d relic call(s) at player (%.0f, %.0f)", n, px.ToDouble(), py.ToDouble());
        Out(b);
"""


class TestPetRelicCollectorContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.raw = PLUGIN.read_text(encoding="utf-8")
        cls.plugin = strip_comments(cls.raw)
        cls.shipped = strip_comments(strip_research_blocks(cls.raw))
        cls.header = strip_comments(HEADER.read_text(encoding="utf-8"))
        cls.tick = function_body(cls.plugin, "static void PetRelicCollectorTick()")
        cls.quest_tick = function_body(cls.plugin, "static void PetQuestCollectorTick()")
        cls.collect = function_body(cls.plugin, "static ForgePact::PetQuestOutcome PetRelicCollectOne(")
        cls.travel = function_body(cls.plugin, "static PetTravelResult PetTravelStep(")

    # ---- baseline: off by default, silent at launch -----------------------

    def test_defaults_has_pet_relic_pickup_off(self):
        self.assertIs(forgepact.DEFAULTS["mod_pet_relic_pickup"], False)
        self.assertIn("std::atomic<bool> m_Enabled{ false };", self.header)

    def test_build_cmds_omits_petrelic_by_default(self):
        self.assertFalse([c for c in forgepact.build_cmds(dict(forgepact.DEFAULTS)) if c.startswith("petrelic")])

    def test_tick_only_runs_while_enabled(self):
        gate = self.plugin.split("PetRelicCollectorTick();", 1)[0][-400:]
        self.assertIn("PetRelicCollectorMod::Instance().IsEnabled()", gate)

    # ---- target: what the tick picks --------------------------------------

    def test_candidates_are_filtered_by_the_maxed_set_before_pick(self):
        self.assertTrue(filtered_before_pick(self.tick))
        self.assertIn("ForgePact::FilterRelicCandidates(relics, mod.Maxed().Ids(), dropped)", self.tick)
        # Negative control: with the filter call removed the check fails.
        broken = self.tick.replace("ForgePact::FilterRelicCandidates(", "ForgePact::Unfiltered(")
        self.assertFalse(filtered_before_pick(broken))
        # The collect checks the maxed set again at arrival.
        self.assertIn("mod.Maxed().IsMaxed(read.relicId)", self.collect)

    def test_maxed_set_comes_from_the_sdk_not_the_relic_filter(self):
        self.assertIn("PetRelicReadOwned(player, owned, stopped)", self.tick)
        self.assertIn("HeroSiege::Player::MaxedRelicIdsOf(owned)", self.tick)
        # The owned read passes both scan reports and refuses an incomplete walk.
        read = function_body(self.plugin, "static bool PetRelicReadOwned(")
        self.assertIn("GetOwnedRelicLevels(g_Yytk, player, &equipped, &tab)", read)
        self.assertIn("equipped.stopped", read)
        self.assertIn("tab.stopped", read)
        self.assertIn("HhResolveLocalPlayer(player)", self.tick)
        self.assertIn("ForgePact::RelicFilterMod::Instance().TestMaxed()", self.tick)
        self.assertIn("mod.Maxed().Due(g_PetRelicFrame)", self.tick)
        for body in (self.tick, self.collect):
            self.assertNotIn("GetPlayerMaxedRelics", body)
            self.assertNotIn("MaxedForFrame", body)
        # A true return makes the cached set stale, so the next pick re-reads.
        after = self.collect[self.collect.index("if (!result.ToBoolean())"):]
        self.assertIn("mod.Maxed().MarkStale();", after)

    def test_relic_is_identified_by_the_sdk_ground_read(self):
        for body in (self.tick, self.collect):
            self.assertIn("HeroSiege::Player::ReadGroundRelic(g_Yytk, inst, read)", body)
            # No literal relic class in the tick: the SDK compares against
            # kRelicItemClass.
            self.assertIsNone(re.search(r"\b16\b", body))

    def test_collect_reads_the_relic_through_a_handle_not_the_travel_id(self):
        # What the collect is handed: the shared walk's bare id, a number.
        self.assertIn("RValue target = RValue(targetId);", self.travel)
        self.assertIn("collect(target)", self.travel)
        self.assertTrue("PetRelicCollectOne(const RValue& target)" in self.plugin)
        # The SDK read refuses a number, so a number fed straight in is
        # `no-handle` on every arrival.
        sdk = (REPO_ROOT / "hs-game-sdk" / "cpp" / "include" / "hs_game_sdk" / "player.hpp").read_text(encoding="utf-8")
        self.assertIn("return value.m_Kind == ::YYTK::VALUE_OBJECT || value.m_Kind == ::YYTK::VALUE_REF;", sdk)
        handle = function_body(self.plugin, "static bool LootGroundHandle(const RValue& value, RValue& inst)")
        self.assertTrue(collect_reads_through_a_handle(self.collect, handle))
        # The helper ships: it is not inside a research block.
        self.assertIn("static bool LootGroundHandle(const RValue& value, RValue& inst)", self.shipped)
        # forcerelic resolves its LootGroundCreateFromItem result through the same helper.
        self.assertIn("LootGroundHandle(res, inst)", self.plugin)
        # Negative controls: round 4's shape, the travel id read directly...
        broken = self.collect.replace("LootGroundHandle(target, inst)", "true", 1).replace(
            "ReadGroundRelic(g_Yytk, inst, read)", "ReadGroundRelic(g_Yytk, target, read)", 1)
        self.assertNotEqual(broken, self.collect)
        self.assertFalse(collect_reads_through_a_handle(broken, handle))
        # ...and a helper that lets only handles through refuses every id.
        broken = handle.replace(
            "if (value.m_Kind != VALUE_REAL && value.m_Kind != VALUE_INT32 && value.m_Kind != VALUE_INT64) return false;",
            "return false;", 1)
        self.assertNotEqual(broken, handle)
        self.assertFalse(collect_reads_through_a_handle(self.collect, broken))

    def test_objects_resolved_by_name(self):
        resolve = function_body(self.plugin, "static void ResolvePetRelicAssets()")
        self.assertIn('asset_get_index", { RValue(std::string("Loot_Ground_obj")) }', resolve)
        self.assertIn("HeroSiege::Objects::GameObject::Loot_Ground_obj", resolve)
        self.assertIn("ResolvePetQuestAssets();", resolve)
        quest = function_body(self.plugin, "static void ResolvePetQuestAssets()")
        self.assertIn('assetIndex("Companion_obj"', quest)
        self.assertIn("ResolvePetRelicAssets();", self.tick)

    # ---- target: one pet, one walk ----------------------------------------

    def test_both_ticks_travel_through_one_helper(self):
        self.assertEqual(self.plugin.count("static PetTravelResult PetTravelStep("), 1)
        self.assertRegex(self.quest_tick, r"PetTravelStep\(petInst, g_PetQuestTargetId, [^;]*PetQuestCollectOne, PetQuestEndTravel\)")
        self.assertRegex(self.tick, r"PetTravelStep\(petInst, g_PetRelicTargetId, [^;]*PetRelicCollectOne, PetRelicEndTravel\)")
        # The pet's x/y write lives in the helper alone.
        self.assertIn('"variable_instance_set", { petInst, RValue("x")', self.travel)
        for tick in (self.tick, self.quest_tick):
            self.assertNotIn("variable_instance_set", tick)
        # Both collect calls (arrival and timeout) go through the callback.
        self.assertEqual(self.travel.count("collect(target)"), 2)

    def test_one_pet_one_fetch(self):
        for tick, who in ((self.tick, "Relic"), (self.quest_tick, "Quest")):
            self.assertIn(f"PetFetchArbiter::Instance().MayPick(ForgePact::PetFetcher::{who})", tick)
            self.assertIn(f"PetFetchArbiter::Instance().Claim(ForgePact::PetFetcher::{who})", tick)
            # The claim follows the pick, the check precedes the walk.
            self.assertLess(tick.index("MayPick("), tick.index("instance_find\", { RValue((double)g_"
                                                               + ("LootGroundObjIdx" if who == "Relic" else "QuestObjParentIdx")))
        for end, who in (("PetRelicEndTravel", "Relic"), ("PetQuestEndTravel", "Quest")):
            body = function_body(self.plugin, f"static void {end}(")
            self.assertIn(f"PetFetchArbiter::Instance().Release(ForgePact::PetFetcher::{who})", body)
        # A collector switched off mid-walk releases the pet.
        frame = function_body(self.plugin, "FrameCallback(")
        self.assertIn("PetQuestEndTravel(ForgePact::PetQuestOutcome::Abandoned)", frame)
        self.assertIn("PetRelicEndTravel(ForgePact::PetQuestOutcome::Abandoned)", frame)

    # ---- target: the collect ----------------------------------------------

    def test_collect_has_one_call_site_and_the_measured_shape(self):
        self.assertEqual(self.plugin.count('"gml_Script_PickupLoot"'), 1)
        self.assertEqual(self.collect.count("CallGameScriptEx("), 1)
        self.assertEqual(self.plugin.count("CallGameScriptEx(result, script, item, pet, args)"), 1)
        # self = the ground item, other = the pet; argc 5 in the order read.
        self.assertIn("CInstance* item = HhResolveInstance(inst);", self.collect)
        self.assertIn("CInstance* pet = HhResolveInstance(petInst);", self.collect)
        self.assertIn("g_CompanionObjIdx", self.collect)
        self.assertIn(
            "std::vector<RValue> args{ mplr, itemStruct, RValue(true), RValue(true), PetRelicPlayerDropArg(inst) };",
            self.collect)
        self.assertIn('"variable_global_get", { RValue("mplr") }', self.collect)
        self.assertIn('"variable_instance_get", { inst, RValue("itemInstance") }', self.collect)
        drop = function_body(self.plugin, "static RValue PetRelicPlayerDropArg(")
        self.assertIn('"GetVariable"', drop)
        self.assertIn('RValue("isPlayerDrop")', drop)
        # The collect is reached only through the shared walk.
        self.assertEqual(self.plugin.count("PetRelicCollectOne"), 2)   # definition + the tick's PetTravelStep
        # Eligibility is re-read at collect time, never cached.
        self.assertIn("PetRelicItemActive(inst)", self.collect)
        self.assertIn("itemActiveMissing", self.collect)

    def test_destroy_only_inside_the_true_return_branch(self):
        self.assertTrue(destroy_only_after_true_return(self.collect))
        # Negative control: a destroy added before the call breaks the rule.
        broken = self.collect.replace(
            "RValue result;", 'g_Yytk->CallBuiltin("instance_destroy", { inst });\n        RValue result;', 1)
        self.assertNotEqual(broken, self.collect)
        self.assertFalse(destroy_only_after_true_return(broken))
        # ...and so does one in the false-return branch.
        broken = self.collect.replace(
            'PetRelicRefuse("returned false"', 'g_Yytk->CallBuiltin("instance_destroy", { inst }); PetRelicRefuse("returned false"', 1)
        self.assertFalse(destroy_only_after_true_return(broken))
        # The destroy is guarded: an item something else removed is not
        # destroyed twice, and one that stayed is counted destroy-failed.
        after = self.collect[self.collect.index("mod.collected.fetch_add(1);"):]
        self.assertLess(after.index('"instance_exists"'), after.index('"instance_destroy"'))
        self.assertLess(after.index('"instance_destroy"'), after.index("mod.destroyFailed.fetch_add(1);"))
        # A true return with no raise seen destroys nothing: both of its
        # counts come before the collect is counted and the relic destroyed.
        first_destroy = self.collect.index('"instance_destroy"')
        for why in ('"after-scan-incomplete("', '"no-raise("'):
            self.assertLess(self.collect.index(why), self.collect.index("mod.collected.fetch_add(1);"))
            self.assertLess(self.collect.index(why), first_destroy)
        # Nothing else in the relic code destroys anything.
        self.assertNotIn("instance_destroy", self.tick)
        self.assertNotIn("instance_destroy", self.travel)

    def test_route_b_is_research_only(self):
        self.assertIn('"gml_Script_PickupRelic"', self.plugin)
        self.assertNotIn('"gml_Script_PickupRelic"', self.shipped)
        self.assertNotIn("SetRoute(", self.shipped)

    # ---- the command -------------------------------------------------------

    def test_petrelic_is_a_player_command_with_one_branch(self):
        start = self.plugin.index("kPlayerCommands = {")
        self.assertIn('"petrelic"', self.plugin[start: self.plugin.index("};", start)])
        self.assertEqual(self.plugin.count('lc == "petrelic"'), 1)
        self.assertIn('lc == "petrelic"', self.shipped)

    def test_research_instruments_are_absent_from_the_player_build(self):
        instruments = ("PetRelicCensus", "Hook_PetRelicPickupLoot", "PetRelicTraceInstall", "g_PetRelicTraceOn",
                       'pv == "census"', 'pv == "stat"', 'pv.rfind("route", 0)', 'pv.rfind("trace", 0)',
                       'HookOneScript("PickupLoot"')
        for name in instruments:
            self.assertIn(name, self.plugin, f"{name} is missing from the research build")   # positive control
            self.assertNotIn(name, self.shipped, f"{name} reaches the player build")
        # The trace installs only after setup and a resolved player.
        install = function_body(self.plugin, "static void PetRelicTraceInstall()")
        self.assertLess(install.index("HhResolveLocalPlayer(player)"), install.index("HookOneScript("))
        self.assertIn("TABLE-ONLY", install)
        frame = function_body(self.plugin, "FrameCallback(")
        self.assertIn("g_PetRelicTracePending && g_Setup", frame)

    def test_petrelic_0_prints_every_counter(self):
        stat_line = self.header[self.header.index("std::string StatLine("):]
        for label in ("petrelic stat: collected=", "skipped(maxed)=", "skipped(not relic)=", "skipped(gate)=",
                      "refused=", "(last ", "true-but-nothing-raised=", "destroyed-by-plugin=",
                      "destroy-failed=", "target lost=", "travel timeouts=", "held back=", "maxed scans=",
                      "maxed ids=", "route=", "phase="):
            self.assertIn(label, stat_line)
        # Both reasoned counters print their last reason.
        self.assertIn('" (last " + LastRefusal() + ")"', stat_line)
        self.assertIn('" (last " + LastNothingRaised() + ")"', stat_line)
        self.assertLess(stat_line.index("true-but-nothing-raised="), stat_line.index("LastNothingRaised()"))
        counters = re.findall(r"std::atomic<long> (\w+)\{ 0 \};", self.header)
        self.assertGreaterEqual(len(counters), 11)
        for counter in ("collected", "trueButNothingRaised", "destroyedByPlugin", "destroyFailed"):
            self.assertIn(counter, counters)
        for counter in counters:
            self.assertIn(counter + ".load()", stat_line, f"{counter} is never printed")
        # What each one counts: a newly owned id counts as raised only at
        # after-level 1, an owned one only one level higher; every other true
        # return goes to true-but-nothing-raised with its reason.
        raised = re.search(r"const bool raised = ([^;]*);", self.collect)
        self.assertIsNotNone(raised)
        self.assertIn("levelAfter == levelBefore + 1", raised.group(1))
        self.assertIn("levelAfter == 1", raised.group(1))
        self.assertIn("nowOwned", raised.group(1))
        self.assertEqual(self.collect.count("mod.NothingRaised("), 2)
        self.assertNotIn("NothingRaised(", self.collect[self.collect.index("mod.collected.fetch_add(1);"):])
        # Negative control: the label Pet Quest Collector keeps for its own
        # count is gone from the relic stat line and the relic code, where it
        # named a counter that every working collect would have raised.
        self.assertNotIn("dispatched-but-item-remained", self.header)
        relic_section = self.raw[self.raw.index("// ---- pet collects relics (#124"):]
        self.assertNotIn("dispatched-but-item-remained", relic_section)
        self.assertNotIn("noEffect", self.header)
        self.assertIn("dispatched-but-item-remained=",
                      self.raw[:self.raw.index("// ---- pet collects relics (#124")])   # petquest keeps its own
        stats = function_body(self.shipped, "static void PetRelicCollectorStats()")
        self.assertIn("StatLine(g_PetRelicSelector.HeldBack()", stats)
        branch = self.shipped.split('lc == "petrelic"', 1)[1][:1200]
        self.assertIn("if (!enable) PetRelicCollectorStats();", branch)

    # ---- forcerelic finds the player the way the relic tick does ----------

    def forcerelic_bodies(self):
        return {name: function_body(self.plugin, signature) for name, signature in (
            ("resolve", "static bool ForceRelicResolvePlayer(RValue& player, CInstance*& self, double& x, double& y)"),
            ("one", "static std::string ForceRelicPlaceOne(int id, long long stamp, CInstance* g, CInstance* self, double x, double y)"),
            ("place", "static void ForceRelicPlace(const std::vector<int>& ids)"),
            ("default", "static void ForceRelicDrop(int n)"),
            ("drop", "static void ForceRelicDropRoute(int n)"),
            ("command", "static void ForceRelicCommand(const std::string& rest)"))}

    def test_forcerelic_resolves_the_player_like_the_relic_tick(self):
        bodies = self.forcerelic_bodies()
        body = bodies["resolve"]
        self.assertTrue(forcerelic_resolves_like_the_tick(body))
        self.assertIn("HhResolveLocalPlayer(player, &how)", body)
        self.assertIn("self = HhResolveInstance(player);", body)
        # Both routes resolve through it and take x/y from the resolved player.
        for route in ("place", "drop"):
            self.assertIn("if (!ForceRelicResolvePlayer(player, self, x, y)) return;", bodies[route])
            self.assertNotIn("instance_find", bodies[route])
        self.assertIn('"variable_instance_get", { player, RValue("x") }', body)
        # Negative control: Live 1's instance_find -> GetInstanceObject shape fails.
        self.assertFalse(forcerelic_resolves_like_the_tick(strip_comments(FORCERELIC_INSTANCE_FIND_SHAPE)))

    def test_forcerelic_refusals_name_their_stage(self):
        bodies = self.forcerelic_bodies()
        every = "".join(bodies.values())
        lines = re.findall(r'Out\(\s*(?:std::string\(\s*)?"([^"]*)"', every)
        self.assertGreaterEqual(len(lines), 6, lines)
        for line in lines:
            self.assertTrue(line.startswith("forcerelic: "), line)
        body = bodies["resolve"]
        no_player = refusal_after(body, "if (!HhResolveLocalPlayer(player, &how))")
        no_instance = refusal_after(body, "if (!self)")
        self.assertIn("forcerelic: ", no_player)
        self.assertIn("forcerelic: ", no_instance)
        self.assertNotEqual(no_player, no_instance)
        self.assertIn("how", no_player)
        self.assertLess(body.index("HhResolveLocalPlayer(player, &how)"), body.index("if (!self)"))
        # A relic not placed names its stage and what was supplied and returned,
        # the first 8 per command.
        for stage in ("json_parse", "InitItemFromJson", "LootGroundCreateFromItem", "no-instance", "not-a-relic"):
            self.assertIn('"' + stage + " (", bodies["one"], stage)
        self.assertIn("GroundRelicStageName(read.stage)", bodies["one"])
        self.assertIn("Describe(res)", bodies["one"])
        self.assertIn('Out("forcerelic: relic " + std::to_string(ids[i]) + " not placed at " + why);', bodies["place"])
        self.assertIn("if (reported++ < 8)", bodies["place"])

    def test_forcerelic_places_through_loot_ground_create_from_item(self):
        bodies = self.forcerelic_bodies()
        self.assertTrue(forcerelic_places_on_the_ground(bodies["place"], bodies["one"]))
        one = bodies["one"]
        # SpawnSignatureItem's shape: a relic tab entry's fields parsed, then
        # the game's own loader with the global instance as self.
        self.assertIn(r'"{\"b\":" + std::to_string(id) + ",\"a\":" + std::to_string(seed) + ",\"j\":0,\"c\":0}"', one)
        self.assertIn('"json_parse", g, g, { RValue(json) }', one)
        self.assertIn('const std::string key = "0-0-" + std::to_string(stamp) + "-"', one)
        # ...so the key ends `-16`: the SDK's relic class is ItemType::Relic, 16.
        sdk = REPO_ROOT / "hs-game-sdk" / "cpp" / "include" / "hs_game_sdk"
        self.assertIn("kRelicItemClass = static_cast<int>(HeroSiege::Items::ItemType::Relic);",
                      (sdk / "player.hpp").read_text(encoding="utf-8"))
        self.assertRegex((sdk / "item_type.hpp").read_text(encoding="utf-8"), r"\bRelic = 16,")
        # The default form draws n ids from the droppable range with the
        # plugin's die; `ids` places what it is given; both go through one place.
        default = bodies["default"]
        self.assertIn("std::uniform_int_distribution<int> pick(0, kForceRelicDroppableTop);", default)
        self.assertIn("pick(TyRng())", default)
        self.assertIn("ForceRelicPlace(ids);", default)
        self.assertIn("static constexpr int kForceRelicDroppableTop = 140;", self.plugin)
        self.assertIn("static constexpr int kForceRelicIdTop = 155;", self.plugin)
        self.assertIn("static constexpr int kForceRelicMax = 200;", self.plugin)
        command = bodies["command"]
        self.assertIn('form == "ids"', command)
        self.assertIn('form == "drop"', command)
        self.assertIn("id < 0 || id > kForceRelicIdTop", command)
        self.assertIn("ids.size() > (size_t)kForceRelicMax", command)
        self.assertIn("ForceRelicPlace(ids);", command)
        self.assertIn("ForceRelicDropRoute(n);", command)
        self.assertIn("ForceRelicDrop(n);", command)
        branch = self.plugin.split('lc == "forcerelic"', 1)[1][:200]
        self.assertIn("ForceRelicCommand(rest);", branch)
        # The ground count is Loot_Ground_obj's, resolved by name.
        count = function_body(self.plugin, "static int ForceRelicGroundItems()")
        self.assertIn("ResolvePetRelicAssets();", count)
        self.assertIn('"instance_number", { RValue((double)g_LootGroundObjIdx) }', count)
        # Negative control: session 2's two-argument body fails the predicate.
        shape = strip_comments(FORCERELIC_TWO_ARGUMENT_SHAPE)
        self.assertFalse(forcerelic_places_on_the_ground(shape, shape))

    def test_forcerelic_drop_route_sets_the_force_flag(self):
        drop = self.forcerelic_bodies()["drop"]
        self.assertTrue(forcerelic_drop_forces(drop))
        self.assertIn('if (!g_Orig_DropRelic) { Out("forcerelic: DropRelic not hooked yet"); return; }', drop)
        self.assertIn('"forcerelic drop: %d DropRelic call(s) with the force flag (x, y, 0, 0, true) at player', drop)
        self.assertIn("RValue ax(x), ay(y);", drop)
        # Negative control: session 2's two-argument body fails it.
        self.assertFalse(forcerelic_drop_forces(strip_comments(FORCERELIC_TWO_ARGUMENT_SHAPE)))

    def test_forcerelic_never_calls_droprelic_with_two_arguments(self):
        counts = droprelic_call_argument_counts(self.plugin)
        self.assertEqual(counts, [5])   # positive control: the drop route's call is seen
        self.assertNotIn("relic call(s) at player", self.plugin)
        # Negative control: session 2's call is seen, with its two arguments.
        self.assertEqual(droprelic_call_argument_counts(strip_comments(FORCERELIC_TWO_ARGUMENT_SHAPE)), [2])

    def test_forcerelic_stays_a_research_command(self):
        start = self.plugin.index("kPlayerCommands = {")
        self.assertNotIn('"forcerelic"', self.plugin[start: self.plugin.index("};", start)])
        self.assertIn('"forcerelic"', self.plugin)   # positive control: the command still exists


if __name__ == "__main__":
    unittest.main()
