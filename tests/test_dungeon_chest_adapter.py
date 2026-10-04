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

Live 2 of this workorder (`-c-live-2.md`) then failed two checks, and two more
pairs of classes pin them. Each pair has a kept class, which passes on the
Live 2 build's sources and after (so both binaries compile there), and a fix
class, which fails there and passes after. Run them against the tag
forgepact-issue-31-dungeon-chest-c-live2 with FORGEPACT_TEST_PLUGIN_SOURCE and
FORGEPACT_TEST_DC_HEADER pointing at that tree's ModuleMain.cpp and
DungeonChestMod.hpp.

- the latch (D15, the kill binary). Live 2 latched at 333 against a threshold
  of 321: a kill only counted, and the decision waited for the next
  once-a-second poll. `KeptTests`: a poll after every kill latches at the
  threshold. `LatchFixTests`: 25 kills through the real DungeonChestOnKill with
  no poll between them latch at the 20th, which prints the one latch line, and
  `latchedAt=20`.
- the head label (D14, a third binary, `draw`, built from the real
  DungeonChestDraw and HhDrawOutlinedWorld). Live 2 drew the label at about
  72 % of its size on single frames: it drew in whatever font the game had
  left current. `KeptDrawTests`: the label's text, place and outline, and the
  draw state put back. `LabelFixTests`: every draw in __newfont6 (or the
  `hhlabelfont` override) whatever font the frame inherits, the inherited font
  with `labelFont=inherited` when the name does not resolve, and the
  `fontSwitches=`/`guiResizes=` counters. `fontSwitches=` is only as good as
  its read of the inherited font, and draw_get_font can answer unset (a real
  "no font" state and a missing builtin look alike): with the fake answering
  unset the draws count in `inheritedUnread=` and `inheritedFont=unread`, not
  as a font that never switched, and an asset-reference answer still reads.
"""
import hashlib
import os
import shutil
import subprocess
import unittest
from functools import lru_cache
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_HEADER = ROOT / "plugin/include/ForgePact/DungeonChestMod.hpp"
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
DRAW_FUNCTIONS = (
    "static bool IsNumericInstanceRead(",   # the inherited font's read kind check
    "static void HhDrawOutlinedWorld(",
    "static void DungeonChestDraw(",
)
FUNCTIONS = {"kill": KILL_FUNCTIONS, "census": CENSUS_FUNCTIONS, "draw": DRAW_FUNCTIONS}


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


def _header_path():
    return Path(os.environ.get("FORGEPACT_TEST_DC_HEADER", DEFAULT_HEADER))


@lru_cache(maxsize=None)
def build(kind):
    """Compile the `kill`, `census` or `draw` binary from the plugin source; returns its path."""
    path = _source_path()
    header_path = _header_path()
    source = path.read_text(encoding="utf-8", errors="replace")
    signatures = FUNCTIONS[kind]
    parts = []
    for signature in signatures:
        body = implementation(source, signature)
        if not body and signature not in OPTIONAL:
            raise AssertionError(f"{path} has no {signature.strip('(')}")
        parts.append(body)
    functions = "\n\n".join(p for p in parts if p)
    defines = f"#define ADAPTER_{kind.upper()}\n"
    # The status line names refused kill calls once the consumer reports them
    # there; before that only the research probe's counter does.
    if kind == "kill" and "CountNotEnemy(" in implementation(source, "static void DungeonChestOnKill("):
        defines += "#define HAS_STATUS_NOT_ENEMY\n"
    header = "\n".join(
        line for line in header_path.read_text(encoding="utf-8").split("\n")
        if not line.strip().startswith("#pragma once") and '#include "Common.hpp"' not in line
    )
    code = HARNESS.read_text(encoding="utf-8")
    code = defines + code.replace("// PRODUCTION_DUNGEONCHEST", header).replace("// PRODUCTION_FUNCTIONS", functions)
    # Both sources name the build directory, so a run against one tree never
    # reuses another tree's binary.
    tag = hashlib.sha1((str(path.resolve()) + "|" + str(header_path.resolve())).encode("utf-8")).hexdigest()[:10]
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


class KeptTests(_Scenarios):
    """Pass on the Live 2 build's sources and after: a poll after every kill latches at the threshold."""

    def test_latch_at_poll(self):
        self.run_scenario("latch-at-poll")


class LatchFixTests(_Scenarios):
    """D15, fail on the Live 2 build's sources: the kill that reaches the threshold latches."""

    def test_latch_at_the_kill(self):
        self.run_scenario("latch-at-the-kill")


class KeptDrawTests(_Scenarios):
    """Pass on the Live 2 build's sources and after: the label's text, place, outline and restored state."""
    kind = "draw"

    def test_label_text_steady(self):
        self.run_scenario("label-text-steady")

    def test_label_state_restored(self):
        self.run_scenario("label-state-restored")


class LabelFixTests(_Scenarios):
    """D14, fail on the Live 2 build's sources: the label draws in one font every frame."""
    kind = "draw"

    def test_label_font_pinned(self):
        self.run_scenario("label-font-pinned")

    def test_label_font_override(self):
        self.run_scenario("label-font-override")

    def test_label_font_unresolved(self):
        self.run_scenario("label-font-unresolved")

    def test_label_diagnostics(self):
        self.run_scenario("label-diagnostics")

    def test_label_font_unread(self):
        self.run_scenario("label-font-unread")


del _Scenarios

if __name__ == "__main__":
    unittest.main()
