import ast
import re
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
PANEL_PATH = PROJECT_ROOT / "src" / "forgepact.py"
PLUGIN_PATH = PROJECT_ROOT / "plugin" / "ModuleMain.cpp"


def _python_dict(source: str, name: str) -> dict:
    tree = ast.parse(source)
    for node in tree.body:
        if not isinstance(node, ast.Assign):
            continue
        if any(isinstance(target, ast.Name) and target.id == name for target in node.targets):
            if not isinstance(node.value, ast.Dict):
                raise AssertionError(f"{name} is not a dictionary literal")
            values = {}
            for key, value in zip(node.value.keys, node.value.values):
                if not isinstance(key, ast.Constant) or not isinstance(key.value, str):
                    continue
                try:
                    values[key.value] = ast.literal_eval(value)
                except (TypeError, ValueError):
                    # DEFAULTS also contains names and comprehensions; this helper
                    # only needs statically literal contract entries.
                    continue
            return values
    raise AssertionError(f"{name} was not found in {PANEL_PATH}")


def _cpp_number(source: str, name: str) -> float:
    match = re.search(
        rf"\b{re.escape(name)}\b\s*=\s*"
        rf"(?P<value>[-+]?(?:\d+(?:\.\d*)?|\.\d+))(?:[fF])?\s*;",
        source,
    )
    if not match:
        raise AssertionError(f"numeric C++ manifest entry {name} was not found")
    return float(match.group("value"))


class NecromancerN1ContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.panel = PANEL_PATH.read_text(encoding="utf-8-sig")
        cls.plugin = PLUGIN_PATH.read_text(encoding="utf-8")

    def n1(self, suffix: str) -> float:
        return _cpp_number(self.plugin, "kN1" + suffix)

    def test_panel_manifest_is_off_by_default_and_wires_necrobal_both_ways(self):
        defaults = _python_dict(self.panel, "DEFAULTS")
        self.assertIs(defaults["necro_balance"], False)

        # Apply-all/launch and the live toggle must use the same reversible command.
        self.assertGreaterEqual(self.panel.count('f"necrobal {1 if'), 2)
        self.assertIn("cfg.get('necro_balance', False)", self.panel)
        self.assertIn("cfg['necro_balance']", self.panel)

        for fragment in (
            'class="card necro-card tab-card"',
            "Necromancer Balance Patch",
            'id="necro_balance"',
            'id="necrobalval"',
            "Damage slope 8 &rarr; 9.40",
            "Damage slope 7.25 &rarr; 8.51",
            "Life slope 55 &rarr; 68",
            "Duration 25 &rarr; 35",
            "cooldown 70 &rarr; 30",
            "Amplify duration 5 &rarr; 10",
            "N1 does not add a corpse fallback",
            "document.getElementById('necro_balance').onchange",
        ):
            self.assertIn(fragment, self.panel)

    def test_plugin_manifest_is_reversible_and_available_in_player_builds(self):
        expected = {
            "WarriorValue1Vanilla": 8.0,
            "WarriorValue1Balanced": 9.40,
            "MageValue1Vanilla": 7.25,
            "MageValue1Balanced": 8.51,
            "MageLifeValue2Vanilla": 55.0,
            "MageLifeValue2Balanced": 68.0,
            "AmplifyDurationVanilla": 5.0,
            "AmplifyDurationBalanced": 10.0,
            "FrenzyDurationVanilla": 25.0,
            "FrenzyDurationBalanced": 35.0,
            "FrenzyCooldownVanilla": 70.0,
            "FrenzyCooldownBalanced": 30.0,
            "FrenzyStartingValue1Vanilla": 8.0,
            "FrenzyStartingValue1Balanced": 8.0,
            "FrenzyValue1Vanilla": 2.0,
            "FrenzyValue1Balanced": 1.0,
            "FrenzyStartingValue2Vanilla": 4.0,
            "FrenzyStartingValue2Balanced": 4.0,
            "FrenzyValue2Vanilla": 1.0,
            "FrenzyValue2Balanced": 1.0,
            "MageMaxSummonsBalanced": 2.0,
            "SpiritMaxSummonsBalanced": 1.0,
            "WarriorPlayerRangeVanilla": 48.0,
            "WarriorPlayerRangeBalanced": 64.0,
        }
        for suffix, value in expected.items():
            name = "kN1" + suffix
            self.assertEqual(self.n1(suffix), value, name)
            self.assertGreaterEqual(
                self.plugin.count(name),
                2,
                f"{name} is declared but is not consumed by the N1 implementation",
            )

        player_commands = re.search(
            r"kPlayerCommands\s*=\s*\{(?P<body>.*?)\};", self.plugin, re.DOTALL
        )
        self.assertIsNotNone(player_commands)
        self.assertIn('"necrobal"', player_commands.group("body"))
        self.assertRegex(self.plugin, r'(?:if|else\s+if)\s*\(\s*lc\s*==\s*"necrobal"\s*\)')
        self.assertRegex(self.plugin, r"static\s+void\s+InstallNecroBalanceHooks\s*\([^;]*\)\s*\{")
        self.assertRegex(self.plugin, r"static\s+void\s+SetNecroBalance\s*\([^;]*\)\s*\{")
        self.assertRegex(self.plugin, r"static\s+void\s+NecroBalanceStatus\s*\([^;]*\)\s*\{")

        # These are semantic hook anchors, not an assertion about YYTK internals.
        self.assertIn("PopulateTalentStructMapNecromancer", self.plugin)
        self.assertIn("LoadSummonStats", self.plugin)
        self.assertIn("NecroBalancePostCreatedInstance(objIdx, Result)", self.plugin)
        self.assertRegex(
            self.plugin,
            r"g_NecroBalanceEnabled\s*\{\s*false\s*\}",
            "N1 must remain opt-in in the runtime as well as the panel",
        )

    def test_runtime_contract_is_fail_closed_and_restore_is_retryable(self):
        # The map handle must be type/lifetime checked before any ds_map call,
        # and booleans must not pass the numeric-value semantic gate.
        self.assertIn(
            'CallBuiltin("ds_exists", { map, RValue(1.0) })',
            self.plugin,
        )
        talent_map = re.search(
            r"static\s+bool\s+N1GetTalentMap\s*\([^)]*\)\s*\{(?P<body>.*?)\n\}",
            self.plugin,
            re.DOTALL,
        )
        self.assertIsNotNone(talent_map)
        self.assertNotIn("N1Numeric(map)", talent_map.group("body"))

        # Asset/object indices may be VALUE_REF in current runners.  They get a
        # dedicated converter, while talent scalar values remain strictly numeric.
        object_index = re.search(
            r"static\s+bool\s+N1ObjectIndex\s*\([^)]*\)\s*\{(?P<body>.*?)\n\}",
            self.plugin,
            re.DOTALL,
        )
        self.assertIsNotNone(object_index)
        self.assertIn("VALUE_REF", object_index.group("body"))
        self.assertIn("value.ToDouble()", object_index.group("body"))
        numeric = re.search(
            r"static\s+bool\s+N1Numeric\s*\([^)]*\)\s*\{(?P<body>.*?)\n\}",
            self.plugin,
            re.DOTALL,
        )
        self.assertIsNotNone(numeric)
        self.assertNotIn("VALUE_BOOL", numeric.group("body"))
        self.assertNotIn("VALUE_REF", numeric.group("body"))

        resolver = re.search(
            r"static\s+void\s+N1ResolveWarriorObject\s*\([^)]*\)\s*"
            r"\{(?P<body>.*?)\n\}",
            self.plugin,
            re.DOTALL,
        )
        self.assertIsNotNone(resolver)
        resolver_body = resolver.group("body")
        self.assertIn('"Summon_Skeleton_Warrior_obj"', resolver_body)
        self.assertIn('CallBuiltin("object_get_name"', resolver_body)
        self.assertIn("resolvedName.ToString() != kExactWarriorObjectName", resolver_body)

        for function_name in (
            "N1AuditWarriorRanges",
            "N1PatchWarriorRange",
            "HookLoadSummonStatsN1",
        ):
            function = re.search(
                rf"static\s+[^\n]+\s+{function_name}\s*\([^)]*\)\s*"
                rf"\{{(?P<body>.*?)\n\}}",
                self.plugin,
                re.DOTALL,
            )
            self.assertIsNotNone(function, function_name)
            self.assertIn("N1ObjectIndex(objectIndex", function.group("body"), function_name)

        for state in (
            "g_NecroBalanceEnabled",
            "g_NecroBalanceOwned",
            "g_NecroRestorePending",
            "g_NecroRangeIntegrity",
            "g_NecroPostCreateHooksInstalled",
        ):
            self.assertRegex(self.plugin, rf"std::atomic<bool>\s+{state}\s*\{{")

        fields = re.search(
            r"kN1TalentFields\[\]\s*=\s*\{(?P<body>.*?)\n\};",
            self.plugin,
            re.DOTALL,
        )
        self.assertIsNotNone(fields)
        self.assertEqual(
            len(re.findall(r'^\s*\{\s*\d+\s*,\s*"', fields.group("body"), re.MULTILINE)),
            12,
        )
        self.assertIn('N1ReadStructNumber(talent, "abilityId"', self.plugin)

        restore = re.search(
            r"static\s+bool\s+N1RestoreTalentMapOwned\s*\([^)]*\)\s*"
            r"\{(?P<body>.*?)\n\}\n\n// Pure read-only",
            self.plugin,
            re.DOTALL,
        )
        self.assertIsNotNone(restore)
        restore_body = restore.group("body")
        self.assertIn("for (const auto& spec : kN1TalentFields)", restore_body)
        self.assertIn("failures++", restore_body)
        self.assertIn("N1NearlyEqual(current, spec.balanced)", restore_body)
        self.assertIn("N1WriteStructNumber(talent, spec, spec.vanilla", restore_body)
        self.assertIn("return failures == 0", restore_body)

        post_create = re.search(
            r"static\s+void\s+NecroBalancePostCreatedInstance\s*\([^)]*\)\s*"
            r"\{(?P<body>.*?)\n\}\n\nstatic RValue& HookPopulate",
            self.plugin,
            re.DOTALL,
        )
        self.assertIsNotNone(post_create)
        failure = post_create.group("body").split("if (!N1PatchWarriorRange", 1)[1]
        self.assertLess(failure.index("g_NecroBalanceEnabled.store(false)"),
                        failure.index("g_NecroRestorePending.store(true)"))
        self.assertIn("g_NecroBalanceOwned.store(true)", failure)
        self.assertIn("N1RestoreOwnedState(restoreDetail)", failure)

        # Future summons are safe only if both GameMaker creation builtins feed
        # the authoritative post-create check. ON must refuse partial readiness.
        self.assertIn("depthReady && layerReady", self.plugin)
        self.assertIn("g_NecroPostCreateHooksInstalled.store", self.plugin)
        self.assertIn("const bool postCreateReady", self.plugin)
        self.assertIn("!g_NecroBalanceHookInstalled || !postCreateReady", self.plugin)
        self.assertIn("postcreate_hooks=", self.plugin)

        # ON is published only after the independent map+range verification;
        # OFF always retries cleanup while ownership/pending state remains.
        self.assertIn("g_NecroBalanceEnabled.store(true); // publish LAST", self.plugin)
        self.assertIn("const bool pending = g_NecroRestorePending.load()", self.plugin)
        self.assertIn("const bool restored = N1RestoreOwnedState(detail)", self.plugin)
        self.assertIn('"necrobal: verify="', self.plugin)
        self.assertIn("restore_pending=", self.plugin)

        # Commands queued while the game is closed must not be consumed before
        # the one-time hook setup has run; fc still increments before setup.
        self.assertRegex(
            self.plugin,
            r"if\s*\(\s*\(\(fc\+\+\)\s*%\s*12\)\s*==\s*0\s*&&\s*g_Setup\s*\)",
        )

    def test_warrior_and_mage_damage_slopes_gain_17_45_percent_together(self):
        warrior_count, mage_count = 3.0, 2.0
        warrior_interval, mage_interval = 1.10, 0.90

        old_warrior = warrior_count * self.n1("WarriorValue1Vanilla") / warrior_interval
        new_warrior = warrior_count * self.n1("WarriorValue1Balanced") / warrior_interval
        old_mage = mage_count * self.n1("MageValue1Vanilla") / mage_interval
        new_mage = mage_count * self.n1("MageValue1Balanced") / mage_interval

        self.assertAlmostEqual(new_warrior / old_warrior - 1.0, 0.175, places=9)
        self.assertAlmostEqual(new_mage / old_mage - 1.0, 0.1737931034, places=9)
        combined_gain = (new_warrior + new_mage) / (old_warrior + old_mage) - 1.0
        self.assertAlmostEqual(combined_gain, 0.1744873502, places=9)
        self.assertAlmostEqual(combined_gain * 100.0, 17.45, places=2)

        # This was the balancing target: match Raven+Ent's marginal coefficient/sec.
        raven_and_ent = 3.0 * 10.0 / 1.50 + 3.0 * 9.0 / 1.10
        self.assertAlmostEqual(new_warrior + new_mage, raven_and_ent, places=2)

    def test_frenzy_35_over_30_is_full_uptime_and_rank20_factor_is_1_28(self):
        rank = 20.0

        def long_run_factor(profile: str) -> tuple[float, float, float]:
            duration = self.n1("FrenzyDuration" + profile)
            cooldown = self.n1("FrenzyCooldown" + profile)
            active_bonus = (
                self.n1("FrenzyStartingValue1" + profile)
                + rank * self.n1("FrenzyValue1" + profile)
            ) / 100.0
            uptime = min(1.0, duration / cooldown)
            return 1.0 + active_bonus * uptime, uptime, active_bonus

        old_factor, old_uptime, old_active = long_run_factor("Vanilla")
        new_factor, new_uptime, new_active = long_run_factor("Balanced")

        self.assertAlmostEqual(old_uptime, 25.0 / 70.0, places=12)
        self.assertAlmostEqual(old_active, 0.48, places=12)
        self.assertAlmostEqual(old_factor, 1.1714285714, places=10)
        self.assertEqual(new_uptime, 1.0)
        self.assertAlmostEqual(new_active, 0.28, places=12)
        self.assertAlmostEqual(new_factor, 1.28, places=12)
        self.assertAlmostEqual(new_factor / old_factor - 1.0, 0.0926829268, places=10)

    def test_mage_life_slope_and_rank20_total_match_the_n1_manifest(self):
        old_slope = self.n1("MageLifeValue2Vanilla")
        new_slope = self.n1("MageLifeValue2Balanced")
        self.assertAlmostEqual(new_slope / old_slope - 1.0, 0.2363636364, places=10)

        # StartingValue2 remains 245; N1 changes only Value2 (the per-rank slope).
        rank, starting_life = 20.0, 245.0
        old_rank20 = starting_life + rank * old_slope
        new_rank20 = starting_life + rank * new_slope
        self.assertEqual(old_rank20, 1345.0)
        self.assertEqual(new_rank20, 1605.0)
        self.assertAlmostEqual(new_rank20 / old_rank20 - 1.0, 0.1933085502, places=10)


if __name__ == "__main__":
    unittest.main()
