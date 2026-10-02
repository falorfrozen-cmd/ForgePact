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
        # destroyed twice, and one that stayed is counted.
        after = self.collect[self.collect.index("mod.collected.fetch_add(1);"):]
        self.assertLess(after.index('"instance_exists"'), after.index('"instance_destroy"'))
        self.assertIn("mod.noEffect.fetch_add(1);", after)
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
                      "refused=", "(last ", "dispatched-but-item-remained=", "destroyed-by-plugin=",
                      "target lost=", "travel timeouts=", "held back=", "maxed scans=", "maxed ids=",
                      "route=", "phase="):
            self.assertIn(label, stat_line)
        counters = re.findall(r"std::atomic<long> (\w+)\{ 0 \};", self.header)
        self.assertGreaterEqual(len(counters), 10)
        for counter in counters:
            self.assertIn(counter + ".load()", stat_line, f"{counter} is never printed")
        stats = function_body(self.shipped, "static void PetRelicCollectorStats()")
        self.assertIn("StatLine(g_PetRelicSelector.HeldBack()", stats)
        branch = self.shipped.split('lc == "petrelic"', 1)[1][:1200]
        self.assertIn("if (!enable) PetRelicCollectorStats();", branch)


if __name__ == "__main__":
    unittest.main()
