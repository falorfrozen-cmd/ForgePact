"""The Angelic / Unholy Drops slider's pool takes potions like every other row (#229), by source.

Since 1.3.14 `BuildAngelicPool` refused every item type 18 row (`potion`) before
asking the game anything, so the Gold Inlaid Mysterious Potion, the game's one
Angelic potion, never dropped from the slider. A row now enters the pool exactly
when the game's own answers pass the three tests every row takes: the name key,
the hidden flag, and the rarity the game builds it at. `test_angelic_pool_behavior`
runs that natively; this pins the source shape without a compiler, and pins the
research build's `angelicprobe pool find|drop` instrument the live session reads.
"""
import importlib.util
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"

# One definition of "what the player build compiles", shared with the release contract.
_spec = importlib.util.spec_from_file_location(
    "_release_hook_contract", ROOT / "tests" / "test_release_hook_contract.py")
_release = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_release)
strip_research_blocks = _release.strip_research_blocks
function_body = _release.function_body
strip_comments = _release.strip_comments

#: The two type-18 rows, exactly as the table carries them (#229 edits neither).
POTION_ROWS = (
    '{ 18, 0, 5, "consumable_elixir_of_unworldly_cognition", "Elixir of Unworldly Cognition", false }',
    '{ 18, 0, 8, "consumable_gold_inlaid_mysterious_potion", "Gold Inlaid Mysterious Potion", true }',
)

BLOCK_START = "// Research instrument for docs/angelic-roll-hook-research.md"
BLOCK_END = "#endif // FORGEPACT_RELEASE (angelicprobe)"

POOL_FUNCTIONS = ("static void ApPoolFind(", "static void ApPoolDrop(", "static void ApPoolCommand(")


class AngelicPoolSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.shipped = strip_research_blocks(cls.plugin)
        cls.pool = strip_comments(function_body(cls.plugin, "static void BuildAngelicPool(bool verbose)"))
        start, end = cls.plugin.find(BLOCK_START), cls.plugin.find(BLOCK_END)
        cls.block = cls.plugin[start:end] if 0 <= start < end else ""

    # ---- the pool build: no row refused for its type ---------------------------

    def test_the_pool_build_has_no_type_rule(self):
        self.assertNotIn('"potion"', self.pool)
        self.assertIsNone(re.search(r"\bbase\.type\s*[=!<>]=?", self.pool),
                          "the pool build compares a row's type")
        self.assertIsNone(re.search(r"\btype\s*==\s*18\b", self.pool))

    def test_the_pool_build_keeps_its_three_tests(self):
        # The key, the hidden flag and the built rarity, the game's answers for every row.
        self.assertIn('"gml_Script_GetUniqueRepoStruct"', self.pool)
        self.assertIn('StructKey(info, "28")', self.pool)
        self.assertIn("key != base.key", self.pool)
        self.assertIn('RValue("40")', self.pool)
        self.assertIn('"gml_Script_InitItemFromJson"', self.pool)
        self.assertIn("rar != 7.0 && rar != 10.0", self.pool)

    def test_both_potion_rows_are_in_the_table_unchanged(self):
        start = self.plugin.index("static const AngelicBase kAngelicBases[] = {")
        table = self.plugin[start:self.plugin.index("};", start)]
        for row in POTION_ROWS:
            with self.subTest(row=row):
                self.assertEqual(table.count(row), 1)

    def test_nothing_after_the_pool_filters_by_type(self):
        # The die, the spawn and the kill hook take whatever the pool validated.
        for signature in ("static bool SpawnAngelicItem(", "static void AngelicDropOnKill("):
            with self.subTest(function=signature):
                body = strip_comments(function_body(self.plugin, signature))
                self.assertIsNone(re.search(r"\.type\s*[=!<>]=", body))
                self.assertNotIn('"potion"', body)

    def test_the_comment_no_longer_implies_potions_are_skipped(self):
        start = self.plugin.index("// ---- Angelic drops on our own die")
        comment = self.plugin[start:self.plugin.index("struct AngelicBase {", start)]
        self.assertIn("No row", comment)
        self.assertIn("type", comment)
        self.assertIn("rarity 7 or 10", comment)

    # ---- the research instrument: `angelicprobe pool find|drop` ----------------

    def test_pool_find_and_drop_are_research_build_only(self):
        self.assertTrue(self.block, "the angelicprobe research block is missing")
        for signature in POOL_FUNCTIONS:
            with self.subTest(function=signature):
                self.assertIn(signature, self.block)
                self.assertEqual(self.plugin.count(signature), 1)
                self.assertNotIn(signature.split()[-1].rstrip("("), self.shipped)
        self.assertNotIn("angelicprobe pool", self.shipped)
        command = function_body(self.plugin, "static void ApRollCommand(")
        self.assertIn('sub == "pool"', command)
        self.assertIn("ApPoolCommand(", command)

    def test_the_reply_formats_are_the_ones_the_live_procedure_reads(self):
        usage = function_body(self.plugin, "static void ApRollUsage(")
        self.assertIn('"  angelicprobe pool find <t> <s> <b> - read-only: ', usage)
        self.assertIn('"  angelicprobe pool drop <t> <s> <b> - ', usage)
        find = function_body(self.plugin, "static void ApPoolFind(")
        for token in ('"angelicprobe pool find: ["', '" key="', '" rarity="', '" hidden="',
                      '" angelic-keeps="', '" ordinary-keeps="', '" | lootListUnique [0]="',
                      '" [1]="', '" [2]="', '" [3]="', '" [4]="', '" [5]="', '" sizes="', '"refused - '):
            with self.subTest(find=token):
                self.assertIn(token, find)
        drop = function_body(self.plugin, "static void ApPoolDrop(")
        for token in ('"angelicprobe pool drop: "', '" placed"', '" (ground items "', '" -> "',
                      '" is not a validated pool candidate (pool "'):
            with self.subTest(drop=token):
                self.assertIn(token, drop)

    def test_pool_find_reads_the_lists_by_name(self):
        find = strip_comments(function_body(self.plugin, "static void ApPoolFind("))
        for step in ("SignatureController(", "kAngelicListVar", "variable_instance_exists",
                     "variable_instance_get", "array_get", "SigListHandle(", "ds_list_size", "SigEntry(",
                     "CallGameScript(HeroSiege::Scripts::gml_Script_GetUniqueRepoStruct",
                     "FcNs::PassesUniqueFilter(", "rarity == 7.0 || rarity == 10.0"):
            with self.subTest(step=step):
                self.assertIn(step, find)
        self.assertIn('kAngelicListVar = "lootListUnique"', strip_comments(self.plugin))
        self.assertIn("kApPoolLists = 6", strip_comments(self.block))
        self.assertIsNone(re.search(r"0x[0-9A-Fa-f]{4,}|Rva\b|GetModuleHandle", find))

    def test_pool_drop_places_through_the_production_spawn(self):
        drop = strip_comments(function_body(self.plugin, "static void ApPoolDrop("))
        self.assertIn("BuildAngelicPool(false)", drop)
        self.assertIn("g_AngelicPool", drop)
        self.assertIn("SpawnAngelicItem(*pick, x, y, ctx)", drop)
        self.assertIn("HhResolveLocalPlayer(", drop)
        self.assertIn("HhResolveInstance(", drop)
        self.assertIn("+ 48.0", drop)
        self.assertIn("DpCount(HeroSiege::Objects::GameObject::Loot_Ground_obj)", drop)
        self.assertNotIn("CallGameScript", drop)

    def test_neither_calls_the_guaranteed_angelic_maker_nor_touches_the_counters(self):
        # DropItemAngelic loops forever on a zone list with no candidate (angelic-drop-research.md).
        for signature in POOL_FUNCTIONS:
            body = strip_comments(function_body(self.plugin, signature))
            with self.subTest(function=signature):
                self.assertNotIn("DropItemAngelic", body.replace("DropItemAngelicChance", ""))
                for counter in ("g_AngelicDropRolls", "g_AngelicDropHits", "g_AngelicDropFails",
                                "g_AngelicDropOneIn", "g_KillsSeen"):
                    self.assertNotIn(counter, body)

    def test_both_refuse_while_a_roll_holds_the_lists(self):
        holds = strip_comments(function_body(self.plugin, "static bool ApPoolRollHoldsLists("))
        self.assertIn("g_SigRollPushed > 0", holds)
        self.assertIn("g_FlightDrop.rollPushed > 0", holds)
        for signature in ("static void ApPoolFind(", "static void ApPoolDrop("):
            body = strip_comments(function_body(self.plugin, signature))
            with self.subTest(function=signature):
                self.assertLess(body.index("ApPoolRollHoldsLists()"),
                                min(i for i in (body.find("CallGameScript"), body.find("SpawnAngelicItem")) if i >= 0))


if __name__ == "__main__":
    unittest.main()
