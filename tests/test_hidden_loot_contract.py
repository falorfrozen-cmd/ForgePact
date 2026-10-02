#!/usr/bin/env python3
"""Contract tests for #95 part 2's research instruments (docs/hidden-loot-research.md).

`lootspawn`, `lootsleep`, `loothide` and `lootshow` are Live 1 of workorder
forgepact-issue-95: they put thousands of filter-hidden ground items in a zone
and compare the frame thread with them awake and asleep. All four are research
build only, dispatched from `HandleLiveOneResearchCommand` beside `lootcensus`,
and reach the game only through `CallBuiltin` and the game's own scripts by
SDK name - no hook, no address. These tests read the plugin as text, in the
shape of `TestLiveOneResearchInstruments` in test_relic_filter_contract.py.
"""

import re
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_release_hook_contract import function_body, strip_comments, strip_research_blocks  # noqa: E402

PLUGIN_SRC = Path(__file__).resolve().parents[1] / "plugin" / "ModuleMain.cpp"

VERBS = ("lootspawn", "lootsleep", "loothide", "lootshow")
FUNCTIONS = ("LootSpawnCommand", "LootSleepCommand", "LootFlagCommand", "LootSpawnFirstEquipment",
             "LootGroundObject", "LootGroundCount", "LootGroundHandles", "LootFilterVerdict")
NAMES = ("kLootSpawnMaxPerCall", "kLootSpawnDropName", "kLootWalkCap", "g_LootSleepHandles")

SIGNATURES = {
    "spawn": "static void LootSpawnCommand(",
    "first": "static bool LootSpawnFirstEquipment(",
    "sleep": "static void LootSleepCommand(",
    "flag": "static void LootFlagCommand(",
    "object": "static bool LootGroundObject(",
    "count": "static long LootGroundCount(",
    "handles": "static void LootGroundHandles(",
    "verdict": "static int LootFilterVerdict(",
}

# Anything that destroys or creates an instance, or runs a game script.
DESTROY_OR_CREATE = ("instance_destroy", "instance_create", "instance_copy", "instance_change",
                     "script_execute", "ApCallScript", "CmCall", "CallGameScript", "event_perform")


class TestHiddenLootResearchInstruments(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.plugin_code = PLUGIN_SRC.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.code = strip_comments(cls.plugin_code)
        cls.player = strip_comments(strip_research_blocks(cls.plugin_code))
        cls.allowlist = re.search(r"kPlayerCommands\s*=\s*\{(?P<body>.*?)\};", cls.plugin_code, re.S).group("body")

    def body(self, which):
        return strip_comments(function_body(self.plugin_code, SIGNATURES[which]))

    def test_none_of_the_four_verbs_reaches_the_player_build(self):
        for name in VERBS + FUNCTIONS + NAMES:
            self.assertNotIn(name, self.allowlist, name)
            self.assertNotIn(name, self.player, name)
        # The dispatcher itself ships, and answers false in the player build.
        dispatch = strip_research_blocks(function_body(self.plugin_code, "static bool HandleLiveOneResearchCommand("))
        for verb in VERBS:
            self.assertNotIn(verb, dispatch)
        self.assertIn("return false;", dispatch)

    def test_each_verb_has_one_dispatch_branch(self):
        dispatch = strip_comments(function_body(self.plugin_code, "static bool HandleLiveOneResearchCommand("))
        for verb in VERBS:
            self.assertEqual(self.code.count(f'lc == "{verb}"'), 1, verb)
            self.assertIn(f'if (lc == "{verb}")', dispatch)
        self.assertIn('if (lc == "loothide") { LootFlagCommand(false); return true; }', dispatch)
        self.assertIn('if (lc == "lootshow") { LootFlagCommand(true); return true; }', dispatch)

    def test_lootspawn_never_adds_to_a_map_or_grid(self):
        spawn = self.body("spawn") + self.body("first")
        for forbidden in ("AddItemToMap", "GridAddItem", "GetItemPreferredGrid",
                          "kCmAddToMapName", "kCmPlaceName", "kCmPreferredName", "kCmRemoveFromMapName",
                          # No `o` change either: one unit per drop, the template's own count.
                          '"variable_struct_set"', 'RValue("o")'):
            self.assertNotIn(forbidden, spawn, forbidden)
        for forbidden in ('"instance_deactivate_object"', '"variable_instance_set"', '"instance_destroy"'):
            self.assertNotIn(forbidden, spawn, forbidden)

    def test_lootsleep_uses_the_two_activation_builtins(self):
        sleep = self.body("sleep")
        self.assertIn('"instance_deactivate_object"', sleep)
        self.assertIn('"instance_activate_object"', sleep)
        for forbidden in DESTROY_OR_CREATE + ('"variable_instance_set"', '"variable_struct_set"'):
            self.assertNotIn(forbidden, sleep, forbidden)
        # The walk is collected whole before any item is put to sleep: a
        # deactivation mid-walk would shift every later instance_find index.
        self.assertLess(sleep.index("LootGroundHandles("), sleep.index('"instance_deactivate_object"'))
        self.assertNotIn("instance_find", sleep)
        # Only hidden items are put to sleep, and only they are remembered.
        self.assertIn("verdict == 0", sleep)
        self.assertIn("g_LootSleepHandles.push_back(", sleep)
        self.assertIn("g_LootSleepHandles.clear();", sleep)

    def test_loothide_and_lootshow_only_write_the_filter_verdict(self):
        flag = self.body("flag")
        writes = re.findall(r'"variable_instance_set",\s*\{\s*\w+,\s*RValue\("(\w+)"\)', flag)
        self.assertEqual(writes, ["lootFilterVisible"])
        self.assertEqual(flag.count('"variable_instance_set"'), 1)
        for forbidden in DESTROY_OR_CREATE + ('"instance_deactivate_object"', '"instance_activate_object"',
                                              'RValue("visible")'):
            self.assertNotIn(forbidden, flag, forbidden)

    def test_every_object_resolves_by_sdk_name_through_asset_get_index(self):
        obj = self.body("object")
        self.assertIn('"asset_get_index"', obj)
        self.assertIn("HeroSiege::Objects::GetObjectName(HeroSiege::Objects::GameObject::Loot_Ground_obj)", obj)
        self.assertIn('"instance_number"', self.body("count"))
        handles = self.body("handles")
        self.assertIn('"instance_find"', handles)
        self.assertIn("kLootWalkCap", handles)
        spawn = self.body("spawn")
        self.assertIn("CmInstance(HeroSiege::Objects::GameObject::Player_obj,", spawn)
        self.assertIn("CmSaveInstance()", spawn)
        for which in SIGNATURES:
            # No object named by a string literal: every name comes from the SDK.
            self.assertIsNone(re.search(r'"\w+_obj"', self.body(which)), which)
        for which in ("spawn", "sleep", "flag"):
            self.assertIn("LootGroundObject(", self.body(which), which)

    def test_lootfiltervisible_is_read_only_after_variable_instance_exists(self):
        verdict = self.body("verdict")
        self.assertLess(verdict.index('"variable_instance_exists", { inst, RValue("lootFilterVisible") }'),
                        verdict.index('"variable_instance_get", { inst, RValue("lootFilterVisible") }'))
        flag = self.body("flag")
        self.assertLess(flag.index('"variable_instance_exists", { h, RValue("lootFilterVisible") }'),
                        flag.index('"variable_instance_set", { h, RValue("lootFilterVisible")'))
        # Every other reader goes through LootFilterVerdict.
        for which in ("spawn", "sleep"):
            body = self.body(which)
            self.assertIn("LootFilterVerdict(", body)
            self.assertNotIn('RValue("lootFilterVisible")', body)
        # Negative control: the check is what the order assertion measures.
        swapped = verdict.replace('"variable_instance_exists"', '"variable_instance_xx"')
        self.assertNotIn('"variable_instance_exists", { inst, RValue("lootFilterVisible") }', swapped)

    def test_the_caps_are_literals(self):
        self.assertIn("kLootSpawnMaxPerCall = 2000;", self.code)
        self.assertIn("kLootWalkCap = 8192;", self.code)
        self.assertNotIn("kLootCensusWalkCap", self.code)
        self.assertIn("kLootSpawnMaxPerCall", self.body("spawn"))
        self.assertIn("kLootWalkCap", strip_comments(function_body(self.plugin_code, "static void LootCensus()")))

    def test_lootspawn_calls_the_game_by_sdk_constants_in_x_y_item_order(self):
        self.assertIn("kLootSpawnDropName = SdkShortScriptName(HeroSiege::Scripts::gml_Script_LootGroundCreateFromItem);",
                      self.code)
        spawn = self.body("spawn")
        for name in ("kCmSaveStructName", "kCmTimestampName", "kCmFromJsonName", "kLootSpawnDropName"):
            self.assertIn(name, spawn)
        for raw in ('"gml_Script_', '"LootGroundCreateFromItem"', '"CreateItemSaveStruct"',
                    '"LootTimestamp"', '"InitItemFromJson"'):
            self.assertNotIn(raw, spawn)
        # The measured order (sigdrop/angelicdrop): x, y, item - not spawnitem's (item, x, y).
        drop = re.findall(r"ApCallScript\(kLootSpawnDropName,\s*\w+,\s*\{([^}]*)\}", spawn)
        self.assertEqual(len(drop), 1)
        self.assertRegex(drop[0], r"^\s*RValue\(x\),\s*RValue\(y\),\s*item\s*$")
        # Negative control: spawnitem's order would not match.
        self.assertIsNone(re.match(r"^\s*RValue\(x\),\s*RValue\(y\),\s*item\s*$", " item, RValue(x), RValue(y) "))

    def test_lootspawn_default_template_is_an_equipment_item(self):
        first = self.body("first")
        self.assertIn("HeroSiege::Items::ItemType::Helmet", first)
        self.assertIn("HeroSiege::Items::ItemType::Belt", first)
        self.assertIn('"ds_map_find_first"', first)
        self.assertIn("kCmMaxMapEntries", first)

    def test_every_line_starts_with_the_verb_name(self):
        spawn, sleep, flag = self.body("spawn"), self.body("sleep"), self.body("flag")
        self.assertIn('const std::string tag = "lootspawn: ";', spawn)
        self.assertIn('const std::string tag = "lootsleep: ";', sleep)
        self.assertIn('const std::string tag = show ? "lootshow: " : "loothide: ";', flag)
        for body in (spawn, sleep, flag):
            for call in re.findall(r"\bOut\(([^;]*)", body):
                self.assertTrue(call.startswith("tag + "), call)
        for field in ('"template="', '" class="', '" made="', '" hidden-now="', '" visible-now="',
                      '" return-unreadable="', '" ground="', '"refused - ', "; nothing was made"):
            self.assertIn(field, spawn)
        for field in ('"asleep="', '" skipped-visible="', '" no-filter-var="', '" errors="', '" ground-after="',
                      '"woken="', '" exist-after="', '"remembered="'):
            self.assertIn(field, sleep)
        for field in ('"set="', '" no-filter-var="', '" errors="'):
            self.assertIn(field, flag)


if __name__ == "__main__":
    unittest.main()
