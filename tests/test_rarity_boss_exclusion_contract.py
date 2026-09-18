#!/usr/bin/env python3
"""Contract tests for excluding bosses from the Monster Rarity sliders' roll.

Player report on v1.4.1: with 20% rare and 20% ancient set, an Anubis boss
went from ~500k to ~4.5M HP. `EnemyRaritySettings` (plugin/ModuleMain.cpp)
runs from `Enemy_Parent_obj`'s own Alarm_4 for every enemy - boss or not -
and only checked `enemyRarity == 1` before rolling; nothing checked whether
the instance was a boss that already has its own scripted HP/affix setup.

The fix identifies a boss the same way the Pet Quest Collector already
identifies its own object family (`CiInstanceIsQuestObject`): by GameMaker's
real object ancestry, read through `HeroSiege::Objects::IsDescendantOf`
against `hs-game-sdk`'s own parent-index table, not by an HP threshold or a
name substring (AGENTS.md, "Identify a thing by what it is"). These tests pin
that the identity check is the one hs-game-sdk's own hierarchy backs
(`test_boss_signal_is_the_sdks_own_object_hierarchy`), that it runs before
the die is rolled at the point of use in the hook
(`test_boss_check_runs_before_the_roll_and_is_the_hook_guard`), that an
ordinary rarity-1 monster is untouched by it
(`test_baseline_normal_monster_is_still_eligible_for_the_roll`), and that
Tyrant's Crown - a separate mechanic sharing the same hook - is untouched
(out of scope for this fix).
"""

import sys
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
FORGEPACT_DIR = REPO_ROOT / "ForgePact"
SRC_DIR = FORGEPACT_DIR / "src"
PLUGIN_SRC = FORGEPACT_DIR / "plugin" / "ModuleMain.cpp"
SDK_PY_PATH = REPO_ROOT / "hs-game-sdk" / "python"

if str(SRC_DIR) not in sys.path:
    sys.path.insert(0, str(SRC_DIR))
if str(SDK_PY_PATH) not in sys.path:
    sys.path.insert(0, str(SDK_PY_PATH))

import forgepact  # noqa: E402


def slice_function(source, signature, next_signature):
    """Text of `signature`'s definition up to (not including) `next_signature`.

    Simple substring slicing (same approach test_pet_quest_collector_contract.py
    uses) rather than brace-matching: both boundary strings are top-level
    declarations in this file, so the slice between them is exactly one
    function's body plus nothing else.
    """
    start = source.index(signature)
    end = source.index(next_signature, start)
    return source[start:end]


class TestRarityBossExclusionContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin_code = PLUGIN_SRC.read_text(encoding="utf-8")
        cls.panel_code = (SRC_DIR / "forgepact.py").read_text(encoding="utf-8")
        cls.hook_body = slice_function(
            cls.plugin_code,
            "static RValue& Hook_EnemyRaritySettings(",
            "static void InstallCreateHooks();",
        )

    # ---- the boss signal itself: what it is, not a threshold or a name ---

    def test_boss_signal_is_the_sdks_own_object_hierarchy(self):
        # hs-game-sdk's own OBJECT_PARENT_INDEX table (regenerated from the
        # game's real object hierarchy, not hand-maintained) confirms Anubis
        # is a descendant of Enemy_Child_Boss_obj through the same chain
        # every other named boss in the SDK's table uses, and that an
        # ordinary monster the game itself raises to rarity 4 on its own
        # (Scorching_Legion_obj, see the comment above g_RarRarePct) is not.
        from hs_game_sdk import GameObject, is_descendant_of

        self.assertTrue(
            is_descendant_of(
                GameObject.Anubis_obj.value, GameObject.Enemy_Child_Boss_obj.value
            ),
            "Anubis_obj is no longer a descendant of Enemy_Child_Boss_obj in "
            "hs-game-sdk - the boss signal this fix relies on has moved; "
            "re-derive it rather than assuming this test is stale.",
        )
        self.assertFalse(
            is_descendant_of(
                GameObject.Scorching_Legion_obj.value,
                GameObject.Enemy_Child_Boss_obj.value,
            )
        )

    def test_plugin_boss_check_uses_the_sdk_descendant_relation(self):
        self.assertIn("static bool RarInstanceIsBoss(", self.plugin_code)
        rar_is_boss = slice_function(
            self.plugin_code,
            "static bool RarInstanceIsBoss(",
            "static RValue& Hook_EnemyRaritySettings(",
        )
        self.assertIn("object_index", rar_is_boss)
        self.assertIn("HeroSiege::Objects::IsDescendantOf", rar_is_boss)
        self.assertIn(
            "HeroSiege::Objects::GameObject::Enemy_Child_Boss_obj", rar_is_boss
        )
        # Not an HP threshold and not a name substring (AGENTS.md).
        for banned in ("ToDouble() >", "ToDouble() <", ".find(\"Boss\"", "strstr"):
            self.assertNotIn(banned, rar_is_boss)

    # ---- target: the boss check gates the roll, at the point of use ------

    def test_boss_check_runs_before_the_roll_and_is_the_hook_guard(self):
        self.assertIn(
            "if (S && !enemyBorn && RarityFloorActive() && RarInstanceIsBoss(inst)) {",
            self.hook_body,
        )
        boss_check_pos = self.hook_body.index("RarInstanceIsBoss(inst)")
        roll_pos = self.hook_body.index("uniform_real_distribution<double>(0.0, 100.0)(TyRng())")
        self.assertLess(
            boss_check_pos,
            roll_pos,
            "the boss check must run before the rarity die is rolled",
        )
        # The roll itself stays reachable only through the sibling branch
        # that runs when the boss check did NOT match.
        else_branch = self.hook_body[
            self.hook_body.index("} else if (S && !enemyBorn && RarityFloorActive()) {")
        :]
        self.assertLess(
            else_branch.index("if (rar == 1.0) {"),
            else_branch.index("uniform_real_distribution"),
        )

    def test_baseline_normal_monster_is_still_eligible_for_the_roll(self):
        # The tier assignment itself - what a non-boss, rarity-1 monster gets
        # raised to - is untouched: still exclusive Ancient(4)/Rare(3), still
        # topped up to the game's own affix counts.
        self.assertIn("if (roll < g_RarAncientPct) tier = 4;", self.hook_body)
        self.assertIn(
            "else if (roll < g_RarAncientPct + g_RarRarePct) tier = 3;", self.hook_body
        )
        self.assertIn("const int want = (tier == 4) ? 3 : 2;", self.hook_body)

    def test_boss_skipped_counter_stays_out_of_the_release_build(self):
        # Declaration and increment both sit behind #ifndef FORGEPACT_RELEASE,
        # and the report line only appends the count under the same guard.
        occurrences = []
        start = 0
        while True:
            idx = self.plugin_code.find("g_RarSkippedBoss", start)
            if idx < 0:
                break
            occurrences.append(idx)
            start = idx + 1
        self.assertGreaterEqual(len(occurrences), 3)  # declare, increment, report
        for idx in occurrences:
            guard_idx = self.plugin_code.rfind("#ifndef FORGEPACT_RELEASE", 0, idx)
            endif_idx = self.plugin_code.rfind("#endif", 0, idx)
            self.assertGreater(
                guard_idx,
                -1,
                "g_RarSkippedBoss used with no preceding #ifndef FORGEPACT_RELEASE",
            )
            self.assertGreater(
                guard_idx,
                endif_idx,
                "g_RarSkippedBoss used outside its #ifndef FORGEPACT_RELEASE guard",
            )

    def test_tyrant_crown_block_is_out_of_scope_and_unchanged(self):
        # This fix is scoped to the Monster Rarity sliders (the player
        # report and the panel card both name that feature). Tyrant's Crown
        # shares the hook but is a separate mechanic; it keeps its own
        # unconditional roll.
        tyrant_block = self.hook_body[self.hook_body.index("if (S && !enemyBorn && TyrantActive()) {"):]
        self.assertNotIn("RarInstanceIsBoss", tyrant_block)

    # ---- panel text ---------------------------------------------------

    def test_panel_hint_says_bosses_are_left_alone(self):
        idx = forgepact.HTML.index("Monster Rarity</h2>")
        hint = forgepact.HTML[idx: idx + 1200]
        self.assertIn("bosses", hint)
        self.assertIn("left alone", hint)


if __name__ == "__main__":
    unittest.main()
