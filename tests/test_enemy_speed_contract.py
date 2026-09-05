import importlib.util
import re
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
PLUGIN_PATH = PROJECT_ROOT / "plugin" / "ModuleMain.cpp"
PANEL_PATH = PROJECT_ROOT / "src" / "forgepact.py"


def function_body(source: str, signature: str) -> str:
    start = source.rfind(signature)
    if start < 0:
        raise AssertionError(f"{signature} not found")
    brace = source.find("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[brace + 1:index]
    raise AssertionError(f"unterminated body for {signature}")


def load_panel():
    spec = importlib.util.spec_from_file_location("forgepact_panel", PANEL_PATH)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class EnemySpeedPluginContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_PATH.read_text(encoding="utf-8")

    def test_command_is_available_in_release_builds(self):
        command = function_body(self.plugin, "static void RunCommand(")
        branch_at = command.find('lc == "enemyspeed"')
        self.assertGreater(branch_at, 0)
        # Walk the preprocessor blocks that enclose the branch: it must be
        # compiled into player builds, so no enclosing block may hide it
        # behind #ifndef FORGEPACT_RELEASE (or the #else of #ifdef).
        stack = []
        for match in re.finditer(r"^\s*#\s*(ifdef|ifndef|if|else|endif)\b(.*)$", command[:branch_at], re.M):
            directive, rest = match.group(1), match.group(2).strip()
            if directive in ("ifdef", "ifndef", "if"):
                stack.append([directive == "ifndef" and rest == "FORGEPACT_RELEASE",
                              directive == "ifdef" and rest == "FORGEPACT_RELEASE", False])
            elif directive == "else" and stack:
                stack[-1][2] = True
            elif directive == "endif" and stack:
                stack.pop()
        for is_ifndef_release, is_ifdef_release, in_else in stack:
            hidden = (is_ifndef_release and not in_else) or (is_ifdef_release and in_else)
            self.assertFalse(hidden, "enemyspeed branch is compiled out of release builds")
        # Player builds also filter commands through an allowlist before the
        # branch chain; a command missing there is silently refused in-game.
        allowlist = re.search(r"kPlayerCommands\s*=\s*\{(?P<body>.*?)\};", command, re.DOTALL)
        self.assertIsNotNone(allowlist)
        self.assertIn('"enemyspeed"', allowlist.group("body"))

    def test_hook_installs_only_for_a_real_multiplier(self):
        body = function_body(self.plugin, "static void EnemySpeedCmd(")
        self.assertIn("if (mult > 1.0) InstallEnemySpeedHook();", body)
        install = function_body(self.plugin, "static void InstallEnemySpeedHook()")
        self.assertIn('HookOneScript("PathFindStartPath"', install)
        release = function_body(self.plugin, "static void InstallHook()")
        self.assertNotIn("InstallEnemySpeedHook();", release)

    def test_hook_restores_base_speed_and_gates_on_chaos_tower(self):
        hook = function_body(self.plugin, "static RValue& Hook_PathFindStartPath(")
        self.assertIn("g_EnemySpeedCtOnly && !InChaosTowerCached()", hook)
        # scale before the native call, restore after it: exactly two writes
        self.assertEqual(hook.count('"moveSpeed"'), 3)
        self.assertEqual(hook.count('variable_instance_set'), 2)
        self.assertLess(hook.index("g_OrigPathFindStartPath(S, O, R, argc, A)"),
                        hook.rindex("variable_instance_set"))
        gate = function_body(self.plugin, "static bool InChaosTowerCached()")
        self.assertIn('"gml_Script_IsChaosTower"', gate)


class EnemySpeedPanelContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.panel = load_panel()

    def test_defaults_are_vanilla_and_chaos_tower_scoped(self):
        self.assertEqual(self.panel.DEFAULTS["enemy_speed"], 0)
        self.assertTrue(self.panel.DEFAULTS["enemy_speed_ct"])

    def test_startup_commands_skip_the_vanilla_setting(self):
        cfg = dict(self.panel.DEFAULTS)
        self.assertFalse(any(c.startswith("enemyspeed") for c in self.panel.build_cmds(cfg)))
        cfg["enemy_speed"] = 50
        self.assertIn("enemyspeed 1.5 ct", self.panel.build_cmds(cfg))
        cfg["enemy_speed_ct"] = False
        self.assertIn("enemyspeed 1.5 all", self.panel.build_cmds(cfg))

    def test_percent_is_clamped_to_the_panel_range(self):
        pct = self.panel.enemy_speed_pct
        self.assertEqual(pct(-20), 0)
        self.assertEqual(pct(999), 300)
        self.assertEqual(pct(52), 50)
        self.assertEqual(pct("abc"), 0)
        self.assertEqual(pct(float("nan")), 0)
        # A live reset must be explicit so an installed hook returns to vanilla.
        self.assertEqual(self.panel.enemy_speed_cmd({"enemy_speed": 0}), "enemyspeed 1 ct")


if __name__ == "__main__":
    unittest.main()
