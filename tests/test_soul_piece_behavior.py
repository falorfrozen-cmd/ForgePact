"""Run the real ForgePact::SoulPieces decisions against fake gem rolls and uses.

Boss soul pieces (`soulpiece 1|0|status|force on|force off`, issue #197): while
the mod is on, every time the game rolls a gem drop for a dying ordinary
monster in a zone of an act, a Piece of <boss>'s Soul of that act rolls too, at
1 in D with D = floor(G x 4 / 5), G being the Gem of Incarnation's own rate,
five quarters of the gem's chance on the same roll. Right-clicking a piece in
the backpack outside town summons that act's boss. A piece is one of the
game's own keys marked by the seed 777100 + act, and while the mod is on two
pieces of different acts, or a piece and an ordinary key, are kept from
stacking.

The baseline class pins the mod off as the vanilla game: no drop for any input,
no summon, no stack veto, and a fresh state that is off. The target class pins
the chance and every refusal: forced drops that skip the chance but never the
eligibility, a boss's own death, a source that is not a monster, towns and boss
rooms, the use refused off and in town, all nine acts named (Act 7 Mevius), the
veto only while on, and the seed round trip.

The header is compiled three times. Once as written, where every scenario must
pass. Once with its three mod-off gates removed (the drop's, the use's and the
veto's), where every baseline must fail: the negative control that shows the
baselines measure the off switch rather than an input that happens to refuse.
And once with the piece's chance replaced by the gem's own, where the chance
target must fail and every baseline still pass.
"""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "plugin/include/ForgePact/SoulPieceMod.hpp"
HARNESS = ROOT / "tests/soul_piece_harness.cpp"
# vcvars64 plus one cl /O2 takes a few seconds on an idle machine, but beside
# other load a comparable harness compile was measured at about 2 minutes
# (ForgePact #165), so the compile gets a wide bound.
COMPILE_TIMEOUT_SECONDS = 600
# The harness asserts nothing about elapsed time and finishes well under a
# second; this bound only catches a hang.
RUN_TIMEOUT_SECONDS = 30

# The three mod-off gates; the first negative control removes all of them.
OFF_GATES = (
    "if (!modOn) return DropVerdict::Off;",
    "if (!modOn) return UseVerdict::Off;",
    "if (!modOn) return false;",
)
# The piece's chance; the second negative control swaps in the gem's own.
CHANCE_LINE = "return roll < 1.0 / static_cast<double>(d) ? DropVerdict::Hit : DropVerdict::Miss;"
GEM_CHANCE = "return roll < 1.0 / gemRate ? DropVerdict::Hit : DropVerdict::Miss;"

BASELINES = (
    "baseline/off-never-drops",
    "baseline/off-never-summons",
    "baseline/off-never-vetoes",
    "baseline/default-off",
)
CHANCE = "target/chance-five-quarters"
TARGETS = (
    CHANCE,
    "target/forced-ignores-chance-not-eligibility",
    "target/boss-kill-never-drops",
    "target/not-monster-never-drops",
    "target/town-and-boss-rooms-refused",
    "target/use-refused-in-town-and-off",
    "target/every-act-has-a-boss",
    "target/stack-veto-only-while-on",
    "target/seed-round-trip",
)
# The targets that run with the mod on only; the ones that also check an off
# input (forced, use, stack veto) fail in the build without the off gates.
ON_ONLY = (
    CHANCE,
    "target/boss-kill-never-drops",
    "target/not-monster-never-drops",
    "target/town-and-boss-rooms-refused",
    "target/every-act-has-a-boss",
    "target/seed-round-trip",
)
COMMANDS = ("command/status-line", "command/words-parsed")


def _compile_and_run(name, header_text):
    klass = "\n".join(
        line for line in header_text.split("\n")
        if not line.strip().startswith("#pragma once") and '#include "Common.hpp"' not in line
    )
    out = ROOT / "build/soul-piece-behavior"
    out.mkdir(parents=True, exist_ok=True)
    cpp = out / f"{name}.cpp"
    cpp.write_text(HARNESS.read_text(encoding="utf-8").replace("// PRODUCTION_SOULPIECES", klass), encoding="utf-8")
    binary = out / (f"{name}.exe" if os.name == "nt" else name)
    if os.name == "nt":
        vswhere = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
        if not vswhere.is_file():
            raise unittest.SkipTest("Visual Studio C++ compiler is required for native behavior tests")
        install = subprocess.check_output(
            [str(vswhere), "-latest", "-products", "*", "-requires",
             "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"],
            text=True).strip()
        if not install:
            raise unittest.SkipTest("Visual Studio C++ toolchain not installed")
        vcvars = Path(install) / "VC/Auxiliary/Build/vcvars64.bat"
        batch = out / f"compile-{name}.cmd"
        batch.write_text(
            f'@echo off\ncall "{vcvars}" >nul\nif errorlevel 1 exit /b 1\n'
            f'cl /nologo /std:c++20 /EHsc /O2 "{cpp}" /Fe:"{binary}" /Fo:"{out / (name + ".obj")}"\n'
            f'exit /b %errorlevel%\n', encoding="utf-8")
        command = ["cmd", "/d", "/c", str(batch)]
    else:
        compiler = shutil.which("c++")
        if not compiler:
            raise unittest.SkipTest("A C++20 compiler is required for native behavior tests")
        command = [compiler, "-std=c++20", "-O2", str(cpp), "-o", str(binary)]
    # The compiler speaks the machine's locale; decode leniently.
    result = subprocess.run(command, cwd=out, capture_output=True, text=True, encoding="utf-8", errors="replace",
                            timeout=COMPILE_TIMEOUT_SECONDS)
    (out / f"compile-{name}.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode:
        raise AssertionError(result.stdout + result.stderr)
    run = subprocess.run([str(binary)], capture_output=True, text=True, encoding="utf-8", errors="replace",
                         timeout=RUN_TIMEOUT_SECONDS)
    (out / f"{name}.log").write_text(run.stdout + run.stderr, encoding="utf-8")
    return run.stdout


def _line(output, label):
    for line in output.split("\n"):
        if line.split(" ")[1:2] == [label]:
            return line
    raise AssertionError(f"scenario {label!r} not in harness output:\n{output}")


_RUNS = {}


def _runs():
    """The three builds, compiled once for both classes."""
    if not _RUNS:
        header = HEADER.read_text(encoding="utf-8")
        _RUNS["real"] = _compile_and_run("soulpiece", header)
        ungated = header
        for gate in OFF_GATES:
            assert header.count(gate) == 1, f"a mod-off gate moved; update OFF_GATES: {gate}"
            ungated = ungated.replace(gate, "")
        _RUNS["ungated"] = _compile_and_run("soulpiece-off-gates-removed", ungated)
        assert header.count(CHANCE_LINE) == 1, "the chance line moved; update CHANCE_LINE"
        _RUNS["gem_chance"] = _compile_and_run("soulpiece-gem-chance", header.replace(CHANCE_LINE, GEM_CHANCE))
    return _RUNS


class _Scenarios(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        runs = _runs()
        cls.output = runs["real"]
        cls.ungated = runs["ungated"]
        cls.gem_chance = runs["gem_chance"]

    def assertScenario(self, label, output=None):
        line = _line(self.output if output is None else output, label)
        self.assertTrue(line.startswith("PASS "), line)


class SoulPieceBaselineTests(_Scenarios):
    """The mod off is the vanilla game."""

    def test_all_scenarios_pass(self):
        self.assertIn("RESULT OK", self.output, self.output)

    def test_mod_off_never_drops(self):
        """Off: `off` for every room, act, source, rate and roll, forced or not."""
        self.assertScenario("baseline/off-never-drops")

    def test_mod_off_never_summons(self):
        self.assertScenario("baseline/off-never-summons")

    def test_mod_off_never_vetoes_a_stack(self):
        self.assertScenario("baseline/off-never-vetoes")

    def test_fresh_state_is_off(self):
        """A fresh state is off and unforced, and its status line starts `soulpiece: off force=off`."""
        self.assertScenario("baseline/default-off")

    def test_negative_control_baselines_fail_without_the_off_gates(self):
        for label in BASELINES[:3]:
            line = _line(self.ungated, label)
            self.assertTrue(line.startswith("FAIL "),
                            f"{label} passed against a core with no mod-off gate: {line}")
        # Only the gates were removed: every target that asks with the mod on alone still passes there.
        for label in ON_ONLY:
            self.assertScenario(label, self.ungated)


class SoulPieceTargetTests(_Scenarios):
    """The mod on: the chance and every refusal."""

    def test_chance_is_five_quarters_of_the_gem_rate(self):
        """D = floor(G x 4 / 5): 8,000 for 10,000, 7,200 for 9,000, 8,800 for 11,000, at least 1, 0 for no rate;
        on one grid of rolls the piece hits 10 times where the gem hits 8, and twice the rolls give twice the pieces."""
        self.assertScenario(CHANCE)

    def test_negative_control_chance_fails_at_the_gem_own_rate(self):
        line = _line(self.gem_chance, CHANCE)
        self.assertTrue(line.startswith("FAIL "), f"{CHANCE} passed at the gem's own chance: {line}")
        for label in BASELINES + tuple(x for x in TARGETS if x != CHANCE):
            self.assertScenario(label, self.gem_chance)

    def test_forced_drop_ignores_chance_not_eligibility(self):
        """`force on` hits at any roll and rate, but town, boss room, no act, not a monster, a boss kill and off stand."""
        self.assertScenario("target/forced-ignores-chance-not-eligibility")

    def test_boss_kill_never_drops(self):
        self.assertScenario("target/boss-kill-never-drops")

    def test_non_monster_source_never_drops(self):
        self.assertScenario("target/not-monster-never-drops")

    def test_town_and_boss_rooms_refused(self):
        """Town_ is `town`; _Boss, Uber_ and Dev_ rooms are `boss-room`; no act is `no-act`; zones, dungeons and rifts drop."""
        self.assertScenario("target/town-and-boss-rooms-refused")

    def test_use_refused_in_town_and_when_off(self):
        """Off and Town_ refuse; a non-piece is `not-piece`; any other room, a boss room included, summons."""
        self.assertScenario("target/use-refused-in-town-and-off")

    def test_every_act_has_a_boss_including_mevius(self):
        self.assertScenario("target/every-act-has-a-boss")

    def test_stack_veto_only_while_on(self):
        self.assertScenario("target/stack-veto-only-while-on")

    def test_seed_round_trip_per_act(self):
        """777101..777109 and back; 777001-777003 (the signature items, Miner's Helmet) are not pieces."""
        self.assertScenario("target/seed-round-trip")

    def test_status_line_and_command_words(self):
        for label in COMMANDS:
            self.assertScenario(label)


if __name__ == "__main__":
    unittest.main()
