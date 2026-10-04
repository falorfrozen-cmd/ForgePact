"""Run Dungeon chest opens early's real adapter functions against a fake game.

Live 2 (2026-10-04, the parent workorder's `-b-live-2.md`) ran the player DLL
with the mod on through the panel: `total=646 creators=122 pending=122`, and
`kills=0` on every read while the owner killed. The kill consumer,
DungeonChestOnKill, counts a call only when its `self` passes the shared enemy
check (IsEnemyObject), and that check answered `false` - and cached it - for
as long as Enemy_Parent_obj's index was unset. Only InstallCreateHooks sets
it: the research build at load, the player build only for a feature that
needs the create hooks, and the owner's saved mods had none. The header's
harness (dungeon_chest_harness.cpp) never ran the enemy check, and the
Headhunter dispatch harness stubs it as always resolvable, so neither could
represent the player build's start-up state.

This test cuts the real functions out of ModuleMain.cpp (the path can be
overridden with FORGEPACT_TEST_PLUGIN_SOURCE, which is how the base tag's
copy is run) and compiles them with the real DungeonChestMod.hpp against a
fake runtime, in two binaries:

- the kill path. `BaselineTests` hold before and after the fix: the research
  build's start-up (the parent index preset) counts an enemy-`self` call, the
  player-`self` call counts nothing and is reported as `notEnemy`, nothing is
  counted while no chest is tracked, and one id counts once. `TargetTests`
  fail on the base tag and pass after it: the player build's start-up (nothing
  resolved the parent) counts an enemy-`self` call without writing
  g_EnemyParentIdx, and an ask made before the parent can be resolved is not
  kept once it can.
- the census (`CensusTests`): an `instance_number` failure refuses the total
  (`count-failed`) instead of shrinking it, each refusal says why on the status
  line (`family-unresolved`, `no-creators`, `count-failed`), the first refusal
  in a room prints one log line and repeated polls print no more, and a
  readable census still estimates.
"""
import hashlib
import os
import shutil
import subprocess
import unittest
from functools import lru_cache
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "plugin/include/ForgePact/DungeonChestMod.hpp"
HARNESS = ROOT / "tests/dungeon_chest_adapter_harness.cpp"

KILL_FUNCTIONS = (
    "static bool IsNumericInstanceRead(",
    "static int IsEnemyParentIndex(",      # the fix's on-demand resolution; absent on the base tag
    "static bool IsEnemyObject(",
    "static int CallerObjectIndex(",
    "static bool CallerIsEnemyInstance(",
    "static double InstanceIdOf(",
    "static void DungeonChestOnKill(",
)
OPTIONAL = ("static int IsEnemyParentIndex(",)
CENSUS_FUNCTIONS = (
    "static const std::vector<int>& DungeonChestCreatorObjects(",
    "static long DungeonChestEstimateTotal(",
)


def implementation(source, signature):
    start = source.rfind(signature)
    if start < 0:
        return ""
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    raise AssertionError(f"Unterminated function: {signature}")


def _source_path():
    return Path(os.environ.get("FORGEPACT_TEST_PLUGIN_SOURCE", ROOT / "plugin/ModuleMain.cpp"))


@lru_cache(maxsize=None)
def build(kind):
    """Compile the `kill` or `census` binary from the plugin source; returns its path."""
    path = _source_path()
    source = path.read_text(encoding="utf-8", errors="replace")
    signatures = KILL_FUNCTIONS if kind == "kill" else CENSUS_FUNCTIONS
    parts = []
    for signature in signatures:
        body = implementation(source, signature)
        if not body and signature not in OPTIONAL:
            raise AssertionError(f"{path} has no {signature.strip('(')}")
        parts.append(body)
    functions = "\n\n".join(p for p in parts if p)
    defines = "#define ADAPTER_KILL\n" if kind == "kill" else "#define ADAPTER_CENSUS\n"
    # The status line names refused kill calls once the consumer reports them
    # there; before that only the research probe's counter does.
    if kind == "kill" and "CountNotEnemy(" in implementation(source, "static void DungeonChestOnKill("):
        defines += "#define HAS_STATUS_NOT_ENEMY\n"
    header = "\n".join(
        line for line in HEADER.read_text(encoding="utf-8").split("\n")
        if not line.strip().startswith("#pragma once") and '#include "Common.hpp"' not in line
    )
    code = HARNESS.read_text(encoding="utf-8")
    code = defines + code.replace("// PRODUCTION_DUNGEONCHEST", header).replace("// PRODUCTION_FUNCTIONS", functions)
    tag = hashlib.sha1(str(path.resolve()).encode("utf-8")).hexdigest()[:10]
    out = ROOT / "build/dungeon-chest-adapter" / tag
    out.mkdir(parents=True, exist_ok=True)
    cpp = out / f"{kind}.cpp"
    cpp.write_text(code, encoding="utf-8")
    binary = out / (f"{kind}.exe" if os.name == "nt" else kind)
    if os.name == "nt":
        vswhere = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
        if not vswhere.is_file():
            raise unittest.SkipTest("Visual Studio C++ compiler is required for native adapter tests")
        install = subprocess.check_output(
            [str(vswhere), "-latest", "-products", "*", "-requires",
             "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"],
            text=True).strip()
        if not install:
            raise unittest.SkipTest("Visual Studio C++ toolchain not installed")
        vcvars = Path(install) / "VC/Auxiliary/Build/vcvars64.bat"
        batch = out / f"compile-{kind}.cmd"
        batch.write_text(
            f'@echo off\ncall "{vcvars}" >nul\nif errorlevel 1 exit /b 1\n'
            f'cl /nologo /std:c++20 /EHsc /O2 "{cpp}" /Fe:"{binary}" /Fo:"{out / (kind + ".obj")}"\n'
            f'exit /b %errorlevel%\n', encoding="utf-8")
        command = ["cmd", "/d", "/c", str(batch)]
    else:
        compiler = shutil.which("c++")
        if not compiler:
            raise unittest.SkipTest("A C++20 compiler is required for native adapter tests")
        command = [compiler, "-std=c++20", "-O2", str(cpp), "-o", str(binary)]
    # The compiler speaks the machine's locale; decode leniently.
    result = subprocess.run(command, cwd=out, capture_output=True, text=True, encoding="utf-8", errors="replace")
    (out / f"compile-{kind}.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode:
        raise AssertionError(result.stdout + result.stderr)
    return binary


class _Scenarios(unittest.TestCase):
    kind = "kill"

    @classmethod
    def setUpClass(cls):
        cls.binary = build(cls.kind)

    def run_scenario(self, scenario):
        result = subprocess.run([str(self.binary), scenario], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("PASS " + scenario, result.stdout)


class BaselineTests(_Scenarios):
    """Hold before and after the fix: the research build's start-up and the refusals."""

    def test_kill_research_setup(self):
        self.run_scenario("kill-research-setup")

    def test_kill_player_self(self):
        self.run_scenario("kill-player-self")

    def test_kill_not_tracking(self):
        self.run_scenario("kill-not-tracking")

    def test_kill_same_id_twice(self):
        self.run_scenario("kill-same-id-twice")


class TargetTests(_Scenarios):
    """Fail on the base tag: the player build's start-up never counted a kill."""

    def test_kill_release_setup(self):
        self.run_scenario("kill-release-setup")

    def test_enemy_check_not_poisoned(self):
        self.run_scenario("enemy-check-not-poisoned")


class CensusTests(_Scenarios):
    """The planned total's census: each refusal named, logged once per room."""
    kind = "census"

    def test_census_estimates(self):
        self.run_scenario("census-estimates")

    def test_census_count_failed(self):
        self.run_scenario("census-count-failed")

    def test_census_family_unresolved(self):
        self.run_scenario("census-family-unresolved")

    def test_census_no_creators(self):
        self.run_scenario("census-no-creators")

    def test_census_refusal_logged_once(self):
        self.run_scenario("census-refusal-logged-once")


del _Scenarios

if __name__ == "__main__":
    unittest.main()
