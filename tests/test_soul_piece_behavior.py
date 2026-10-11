"""Run the real ForgePact::SoulPieces decisions against fake gem rolls and uses.

Boss soul pieces (`soulpiece 1|0|status|force on|force off`, issue #197): while
the mod is on, every time the game rolls a gem drop for a dying ordinary
monster in a zone of an act, a Piece of <boss>'s Soul of that act rolls too, at
1 in D with D = floor(G x 4 / 5), G being the Gem of Incarnation's own rate
(9,000, measured in Live procedure 1, so 1 in 7,200): five quarters of the
gem's chance on the same roll. A piece is the game's own Edible PAS Logo marked
by the seed 777100 + act. Right-clicking it in the backpack and confirming
opens a portal beside the player to that act's boss room (`Act_0N_Boss`), from
a town or any zone of an act; it is refused off, in a room with no act, and in
a `_Boss`, `Uber_` or `Dev_` room.

The baseline class pins the mod off as the vanilla game: no drop and no portal
for any input, and a fresh state that is off; and the act that `RoomActIndex`
answered as Live procedure 1 measured it (the answer is the act, 1.0 in
Town_01_rm and Act_01_01, 2.0 in Act_02_01). The target class pins the chance at
the measured rate and every refusal: forced drops that skip the chance but never
the eligibility, a boss's own death, a source that is not a monster, towns and
boss rooms, the use's portal in towns and zones and its refusals, the portal's
target and place, all nine acts named (Act 7 Mevius), the base, and the seed
round trip.

The header is compiled five times. Once as written, where every scenario must
pass. Then four negative controls, each a one-line change to the header that
must turn exactly its own scenarios red while the rest stay green:
the mod-off gates removed (the baselines that pin off fail), the act read as
`RoomActIndex`'s answer plus one (the first plan's mistake, which Live
procedure 1 measured: the act baseline fails), the piece's chance replaced by
the gem's own (the chance target fails), and the use's boss-room refusal
removed (the use-refusal target fails while the portal target still passes).
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

# The two mod-off gates; the first negative control removes both.
OFF_GATES = (
    "if (!modOn) return DropVerdict::Off;",
    "if (!modOn) return UseVerdict::Off;",
)
# The act from `RoomActIndex`'s answer; the second negative control adds one.
ACT_LINE = "return static_cast<int>(answer);"
ACT_PLUS_ONE = "return static_cast<int>(answer) + 1;"
# The piece's chance; the third negative control swaps in the gem's own.
CHANCE_LINE = "return roll < 1.0 / static_cast<double>(d) ? DropVerdict::Hit : DropVerdict::Miss;"
GEM_CHANCE = "return roll < 1.0 / gemRate ? DropVerdict::Hit : DropVerdict::Miss;"
# The use's boss-room refusal; the fourth negative control removes it.
USE_BOSS_ROOM_GATE = "if (IsBossRoom(room)) return UseVerdict::BossRoom;"

OFF_BASELINES = ("baseline/off-never-drops", "baseline/off-never-opens-a-portal")
ACT_BASELINE = "baseline/room-act-index-is-the-act"
BASELINES = OFF_BASELINES + (ACT_BASELINE, "baseline/default-off")
CHANCE = "target/chance-five-quarters"
USE_PORTAL = "target/use-opens-a-portal-in-towns-and-zones"
USE_REFUSED = "target/use-refused-when-off-outside-acts-and-in-boss-rooms"
TARGETS = (
    CHANCE,
    "target/forced-ignores-chance-not-eligibility",
    "target/boss-kill-never-drops",
    "target/not-monster-never-drops",
    "target/town-and-boss-rooms-refused",
    USE_PORTAL,
    USE_REFUSED,
    "target/portal-target-is-the-act-boss-room",
    "target/every-act-has-a-boss",
    "target/seed-round-trip",
    "target/base-is-the-edible-pas-logo",
)
COMMANDS = ("command/status-line", "command/words-parsed")
ALL = BASELINES + TARGETS + COMMANDS
# The scenarios that also ask with the mod off (the forced drop's off case and
# the use refusals' off cases) fail in the build without the off gates; every
# other scenario asks with the mod on alone and must still pass there.
ASKS_OFF = OFF_BASELINES + ("target/forced-ignores-chance-not-eligibility", USE_REFUSED)


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


def _replace_once(header, old, new, what):
    assert header.count(old) == 1, f"{what} moved; update this module's copy of it: {old}"
    return header.replace(old, new)


_RUNS = {}


def _runs():
    """The five builds, compiled once for both classes."""
    if not _RUNS:
        header = HEADER.read_text(encoding="utf-8")
        _RUNS["real"] = _compile_and_run("soulpiece", header)
        ungated = header
        for gate in OFF_GATES:
            ungated = _replace_once(ungated, gate, "", "a mod-off gate")
        _RUNS["ungated"] = _compile_and_run("soulpiece-off-gates-removed", ungated)
        _RUNS["act_plus_one"] = _compile_and_run(
            "soulpiece-act-plus-one", _replace_once(header, ACT_LINE, ACT_PLUS_ONE, "the act line"))
        _RUNS["gem_chance"] = _compile_and_run(
            "soulpiece-gem-chance", _replace_once(header, CHANCE_LINE, GEM_CHANCE, "the chance line"))
        _RUNS["boss_room_use"] = _compile_and_run(
            "soulpiece-use-in-boss-rooms", _replace_once(header, USE_BOSS_ROOM_GATE, "", "the use's boss-room refusal"))
    return _RUNS


class _Scenarios(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        runs = _runs()
        cls.output = runs["real"]
        cls.ungated = runs["ungated"]
        cls.act_plus_one = runs["act_plus_one"]
        cls.gem_chance = runs["gem_chance"]
        cls.boss_room_use = runs["boss_room_use"]

    def assertScenario(self, label, output=None):
        line = _line(self.output if output is None else output, label)
        self.assertTrue(line.startswith("PASS "), line)

    def assertOnlyThese(self, output, failing, build):
        """In a negative-control build, exactly `failing` fail and every other scenario passes."""
        for label in ALL:
            line = _line(output, label)
            if label in failing:
                self.assertTrue(line.startswith("FAIL "), f"{label} passed against {build}: {line}")
            else:
                self.assertTrue(line.startswith("PASS "), f"{label} failed against {build}, which should not reach it: {line}")


class SoulPieceBaselineTests(_Scenarios):
    """The mod off is the vanilla game, and the act is `RoomActIndex`'s answer."""

    def test_all_scenarios_pass(self):
        self.assertIn("RESULT OK", self.output, self.output)

    def test_mod_off_never_drops(self):
        """Off: `off` for every room, act, source, rate and roll, forced or not; the same input on drops."""
        self.assertScenario("baseline/off-never-drops")

    def test_mod_off_never_opens_a_portal(self):
        """Off: `off` for every room, room act and piece act; the same piece on, in town, opens a portal."""
        self.assertScenario("baseline/off-never-opens-a-portal")

    def test_room_act_index_answer_is_the_act(self):
        """1.0 in Town_01_rm and Act_01_01 is act 1, 2.0 in Act_02_01 act 2 (Live procedure 1); undefined,
        0, 10, 1.5, negative and NaN are no act."""
        self.assertScenario(ACT_BASELINE)

    def test_fresh_state_is_off(self):
        """A fresh state is off and unforced, and its status line starts `soulpiece: off force=off`."""
        self.assertScenario("baseline/default-off")

    def test_negative_control_baselines_fail_without_the_off_gates(self):
        self.assertOnlyThese(self.ungated, ASKS_OFF, "a core with no mod-off gate")

    def test_negative_control_act_baseline_fails_at_the_answer_plus_one(self):
        self.assertOnlyThese(self.act_plus_one, (ACT_BASELINE,), "a core reading the act as the answer plus one")


class SoulPieceTargetTests(_Scenarios):
    """The mod on: the chance at the measured rate, the portal and every refusal."""

    def test_chance_is_five_quarters_of_the_gem_rate(self):
        """D = floor(G x 4 / 5): 7,200 for the measured 9,000, 8,000 for 10,000, 8,800 for 11,000, at least 1,
        0 for no rate; on one grid of 72,000 rolls the piece hits 10 times where the gem hits 8, and twice the
        rolls give twice the pieces."""
        self.assertScenario(CHANCE)

    def test_negative_control_chance_fails_at_the_gem_own_rate(self):
        self.assertOnlyThese(self.gem_chance, (CHANCE,), "a core rolling at the gem's own chance")

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

    def test_use_opens_a_portal_in_towns_and_zones(self):
        """A piece of any act opens a portal from every town and every zone of an act."""
        self.assertScenario(USE_PORTAL)

    def test_use_refused_when_off_outside_acts_and_in_boss_rooms(self):
        """In order: `off`, `not-piece`, `no-act` (boss rooms, uber realms, Satanic Zone), `boss-room` (_Boss,
        Uber_, Dev_ rooms that have an act), each beside an input that opens a portal."""
        self.assertScenario(USE_REFUSED)

    def test_negative_control_use_refusal_fails_without_the_boss_room_rule(self):
        self.assertOnlyThese(self.boss_room_use, (USE_REFUSED,), "a core that opens portals in boss rooms")

    def test_portal_target_is_the_act_boss_room(self):
        """Act_01_Boss .. Act_09_Boss by act; 96 px to the right, mirrored left past the room's width."""
        self.assertScenario("target/portal-target-is-the-act-boss-room")

    def test_every_act_has_a_boss_including_mevius(self):
        self.assertScenario("target/every-act-has-a-boss")

    def test_seed_round_trip_per_act(self):
        """777101..777109 and back; 777001-777003 (the signature items, Miner's Helmet) are not pieces."""
        self.assertScenario("target/seed-round-trip")

    def test_base_is_the_edible_pas_logo(self):
        """Category and class 11, `consumable_edible_pas_logo`; a piece is that base, c 0, with a piece's seed."""
        self.assertScenario("target/base-is-the-edible-pas-logo")

    def test_status_line_and_command_words(self):
        for label in COMMANDS:
            self.assertScenario(label)


if __name__ == "__main__":
    unittest.main()
