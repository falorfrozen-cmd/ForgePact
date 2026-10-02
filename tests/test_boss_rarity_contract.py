#!/usr/bin/env python3
"""Contract tests for the Bosses control (`bossrarity`, issue #44).

`bossrarity rare|ancient` makes every boss the game spawns at rarity 1 roll as
Rare (3) or Ancient (4). It runs inside the shared `EnemyRaritySettings` hook
(plugin/ModuleMain.cpp), the one the Monster Rarity sliders and Tyrant's Crown
already use, and the decision itself lives in
`plugin/include/ForgePact/BossRarityMod.hpp`, which
`test_boss_rarity_behavior.py` runs natively.

These pins hold the parts a harness cannot see: that the raise is reached only
for an instance the hook identified as a boss at the point of use, and only
through a decision that tests rarity 1 before anything is written; that the
sliders still leave every boss alone (the boss mode adds a branch, it does not
loosen theirs); that the research probes (`bossprobe`, `droptrace`) never
reach the player build; that the player build accepts `bossrarity`; and that
a hook which went in table-only says so instead of reporting itself armed
(AGENTS.md, "Prove the Instrument Before Trusting a Negative Result").
"""

import re
import unittest
from pathlib import Path

FORGEPACT_DIR = Path(__file__).resolve().parents[1]
PLUGIN_SRC = FORGEPACT_DIR / "plugin" / "ModuleMain.cpp"
HEADER = FORGEPACT_DIR / "plugin" / "include" / "ForgePact" / "BossRarityMod.hpp"


def slice_function(source, signature, next_signature):
    """Text from `signature` up to (not including) `next_signature`."""
    start = source.index(signature)
    end = source.index(next_signature, start)
    return source[start:end]


def strip_research_blocks(source):
    """What the player build compiles (FORGEPACT_RELEASE defined).

    The same nesting-aware evaluator as `test_player_hook_names_in_sdk.py`'s
    helper of that name, duplicated locally as this suite does elsewhere.
    """
    kept, stack = [], []
    for line in source.split("\n"):
        stripped = line.strip()
        if stripped.startswith("#ifdef FORGEPACT_RELEASE"):
            stack.append([True, True])
        elif stripped.startswith("#ifndef FORGEPACT_RELEASE"):
            stack.append([True, False])
        elif stripped.startswith("#if"):
            stack.append([False, True])
        elif stripped.startswith("#else") and stack:
            if stack[-1][0]:
                stack[-1][1] = not stack[-1][1]
        elif stripped.startswith("#endif") and stack:
            stack.pop()
        elif all(active for _, active in stack):
            kept.append(line)
    return "\n".join(kept)


def strip_comments(source):
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.S)
    return re.sub(r"//[^\n]*", "", source)


class TestBossRarityContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_SRC.read_text(encoding="utf-8", errors="replace")
        cls.header = HEADER.read_text(encoding="utf-8")
        cls.hook_body = slice_function(
            cls.plugin, "static RValue& Hook_EnemyRaritySettings(", "static void InstallCreateHooks();"
        )
        cls.player_build = strip_comments(strip_research_blocks(cls.plugin))

    # ---- the raise: a boss, at the point of use, rarity 1 before any write -

    def test_boss_raise_runs_only_for_a_boss_in_the_hook_guard(self):
        guard = "if (S && ForgePact::BossRarity::Active() && RarInstanceIsBoss(inst)) BossRarityRaise(inst, enemyBorn);"
        self.assertIn(guard, self.hook_body)
        self.assertEqual(self.plugin.count("BossRarityRaise(inst, enemyBorn)"), 1,
                         "the raise is reached from the hook's boss guard and nowhere else")
        # The guard runs before the original builds the boss.
        self.assertLess(self.hook_body.index(guard), self.hook_body.index("g_Orig_EnemyRaritySettings(S, O, R, argc, A)"))

        raise_body = slice_function(self.plugin, "static void BossRarityRaise(", "static bool RarInstanceIsBoss(")
        # The adapter asks the header's decision before it writes anything.
        self.assertLess(raise_body.index("BR::RaiseBoss("), raise_body.index('"variable_instance_set"'))
        self.assertIn('RValue("enemyRarity"), RValue((double)tier)', raise_body)
        self.assertIn("TyAddAffixes(inst, add)", raise_body)

        # The decision tests rarity 1 exactly; RaiseBoss consults it before
        # calling the write.
        decide = slice_function(self.header, "inline int DecideTier(", "struct Counters")
        self.assertIn("enemyRarity == 1.0", decide)
        self.assertIn("!isBoss", decide)
        self.assertIn("enemyBorn", decide)
        raise_boss = slice_function(self.header, "inline int RaiseBoss(", "inline std::string StatusLine(")
        self.assertLess(raise_boss.index("DecideTier("), raise_boss.index("write(tier"))
        self.assertLess(raise_boss.index("if (enemyBorn)"), raise_boss.index("write(tier"))
        # Same affix counts the sliders give a raised monster.
        self.assertIn("inline constexpr int kRareAffixes = 2;", self.header)
        self.assertIn("inline constexpr int kAncientAffixes = 3;", self.header)

    def test_boss_mode_is_off_by_default_and_widens_the_enemy_born_guards(self):
        self.assertIn("inline std::atomic<int> modeValue{ static_cast<int>(Mode::Off) };", self.header)
        # Enemy-born tracking runs whenever the boss mode is on, so a boss's
        # own phases and clones are judged with fresh state, not stale.
        scope = slice_function(self.plugin, "struct EnemyBornScope", "~EnemyBornScope()")
        self.assertIn("ForgePact::BossRarity::Active()", scope)
        self.assertEqual(
            self.hook_body.count("(RarityFloorActive() || TyrantActive() || ForgePact::BossRarity::Active())"), 2)
        self.assertNotIn("(RarityFloorActive() || TyrantActive())", self.hook_body)

    # ---- the sliders keep their own rule --------------------------------

    def test_sliders_still_leave_bosses_alone_while_boss_mode_is_off(self):
        # The sliders' boss check and its sibling roll are untouched, and
        # nothing the boss mode added sits between them.
        sliders = "if (S && !enemyBorn && RarityFloorActive() && RarInstanceIsBoss(inst)) {"
        roll_branch = "} else if (S && !enemyBorn && RarityFloorActive()) {"
        self.assertIn(sliders, self.hook_body)
        self.assertIn(roll_branch, self.hook_body)
        between = self.hook_body[self.hook_body.index(sliders):self.hook_body.index(roll_branch)]
        self.assertNotIn("BossRarity", between)
        self.assertNotIn("variable_instance_set", between)
        # The boss branch is gated on the mode, so with it off the hook is
        # the sliders' alone; and it comes before the sliders' check.
        self.assertLess(self.hook_body.index("ForgePact::BossRarity::Active() && RarInstanceIsBoss(inst)"),
                        self.hook_body.index(sliders))
        roll = self.hook_body[self.hook_body.index(roll_branch):self.hook_body.index("if (S && !enemyBorn && TyrantActive()) {")]
        self.assertNotIn("BossRarity", roll)
        # Tyrant's Crown block is not the boss mode's.
        tyrant = self.hook_body[self.hook_body.index("if (S && !enemyBorn && TyrantActive()) {"):]
        self.assertNotIn("BossRarity", tyrant)

    # ---- research probes stay out of the player build --------------------

    def test_probe_commands_stay_out_of_the_release_build(self):
        for name in ('lc == "bossprobe"', 'lc == "droptrace"', "BossProbeCommand", "DropTraceCommand",
                     "DropTraceNote", "g_DropTraceLeft", "PC_GetVariableGMLWrapper"):
            self.assertIn(name, self.plugin, f"{name} is missing from the research build")
            self.assertNotIn(name, self.player_build, f"{name} reaches the player build")
        # The same technique as g_RarSkippedBoss: every mention of the two
        # verbs sits after a #ifndef FORGEPACT_RELEASE with no #endif between.
        for verb in ('"bossprobe"', '"droptrace"'):
            start = 0
            while True:
                idx = self.plugin.find(verb, start)
                if idx < 0:
                    break
                guard = self.plugin.rfind("#ifndef FORGEPACT_RELEASE", 0, idx)
                endif = self.plugin.rfind("#endif", 0, idx)
                self.assertGreater(guard, endif, f"{verb} used outside #ifndef FORGEPACT_RELEASE")
                start = idx + 1

    def test_bossrarity_is_a_player_command(self):
        allowlist = re.search(r"kPlayerCommands\s*=\s*\{(?P<body>.*?)\};", self.plugin, re.S).group("body")
        self.assertIn('"bossrarity"', allowlist)
        self.assertNotIn('"bossprobe"', allowlist)
        self.assertNotIn('"droptrace"', allowlist)
        self.assertIn('if (lc == "bossrarity") { BossRarityCommand(rest); return; }', self.player_build)
        command = slice_function(self.plugin, "static void BossRarityCommand(", "static void TyrantAutoArm()")
        # rare/ancient install the shared hook; off installs nothing.
        self.assertIn("if (m != BR::Mode::Off) InstallTyrantHook();", command)
        self.assertEqual(command.count("InstallTyrantHook()"), 1)
        # Every form answers one `bossrarity:` line.
        self.assertIn('"bossrarity: usage', command)
        self.assertEqual(command.count("BR::StatusLine("), 2)
        self.assertIn('std::string("bossrarity: ")', self.header)
        self.assertIn('" raised="', self.header)
        # 0 is off, like every other toggle.
        self.assertIn('word == "off" || word == "0"', self.header)

    # ---- a blind hook says so ------------------------------------------

    def test_status_reports_a_table_only_hook(self):
        # HookOneScript answers true for a TABLE-ONLY install too, so
        # g_TyHookInstalled alone cannot tell a blind hook from a working one:
        # the install also records whether the inline detour went in.
        install = slice_function(self.plugin, "static void InstallTyrantHook()", "static const char* BossRarityHookState()")
        self.assertIn("&g_Orig_EnemyRaritySettings, &g_TyHookNative);", install)
        state = slice_function(self.plugin, "static const char* BossRarityHookState()", "static void BossRarityCommand(")
        self.assertIn('if (!g_TyHookInstalled) return "failed";', state)
        self.assertIn('return g_TyHookNative ? "ok" : "table-only";', state)
        self.assertLess(state.index("g_TyHookInstalled"), state.index("table-only"))
        command = slice_function(self.plugin, "static void BossRarityCommand(", "static void TyrantAutoArm()")
        self.assertEqual(command.count("BossRarityHookState()"), 2)
        self.assertIn('" hook="', self.header)


if __name__ == "__main__":
    unittest.main()
