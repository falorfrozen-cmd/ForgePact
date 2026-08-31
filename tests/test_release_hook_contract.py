import re
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
PLUGIN_PATH = PROJECT_ROOT / "plugin" / "ModuleMain.cpp"


def function_body(source: str, signature: str) -> str:
    # Some runtime functions have an early forward declaration; the final
    # occurrence is the implementation whose body defines the contract.
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


class ReleaseHookContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_PATH.read_text(encoding="utf-8")

    def test_release_initialization_has_no_eager_gameplay_hook_group(self):
        body = function_body(self.plugin, "static void InstallHook()")
        release = body.split("#ifdef FORGEPACT_RELEASE", 1)[1].split("#else", 1)[0]
        for eager in (
            "InstallCreateHooks();",
            "InstallDropMultHooks();",
            "InstallNecroBalanceHooks();",
        ):
            self.assertNotIn(eager, release)

    def test_functional_hooks_install_only_for_non_vanilla_commands(self):
        drop = function_body(self.plugin, "static void SetDropMult(")
        self.assertIn("if (n > 1) InstallDropMultHooks();", drop)

        special = function_body(self.plugin, "static void SpecialRate(")
        self.assertIn("if (n > 1) InstallCreateHooks();", special)

        command = function_body(self.plugin, "static void RunCommand(")
        density = command.split('else if (lc == "density")', 1)[1].split(
            'else if (lc == "dropstats")', 1
        )[0]
        self.assertIn("if (d > 1.0) InstallCreateHooks();", density)

        necro = function_body(self.plugin, "static void SetNecroBalance(bool enabled)")
        enabled = necro.split("if (enabled)", 1)[1].split("} else {", 1)[0]
        self.assertIn("InstallCreateHooks();", enabled)
        self.assertIn("InstallNecroBalanceHooks();", enabled)

    def test_ship_hot_path_telemetry_compiles_to_no_op(self):
        macro = re.search(
            r"#ifdef FORGEPACT_RELEASE\s*"
            r"#define BP_DIAG_INCREMENT\(counter\) \(\(void\)0\)",
            self.plugin,
        )
        self.assertIsNotNone(macro)
        self.assertIn("BP_DIAG_INCREMENT(g_cnt_##NAME);", self.plugin)
        self.assertIn("BP_DIAG_INCREMENT(g_StatSayac_##NAME);", self.plugin)
        self.assertIn("BP_DIAG_INCREMENT(g_StatAddSayac_##NAME);", self.plugin)


if __name__ == "__main__":
    unittest.main()
