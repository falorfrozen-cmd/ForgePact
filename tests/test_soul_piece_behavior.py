"""Run the real ForgePact::SoulPieces decisions against fake gem rolls and uses.

Boss soul pieces (`soulpiece 1|0|status|force on|force off`, issue #197): while
the mod is on, every time the game rolls a gem drop for a dying ordinary
monster in a zone of an act, a Piece of <boss>'s Soul of that act rolls too, at
1 in D with D = floor(G x 4 / 5), G being the Gem of Incarnation's own rate
(9,000, measured in Live procedure 1, so 1 in 7,200): five quarters of the
gem's chance on the same roll. A piece is built on a key (`keys_bifrost_key`,
resolved by name) with `b` = the key's index + act/10, `c` 0 and the seed
777100 + act, so it sits in the Key tab, stacks per act and never joins the
real key; the first builds' pieces (the game's own Edible PAS Logo marked by
the same seed) stay recognised as legacy bag pieces. Its display sprite is the
Gem of Incarnation's. Right-clicking it and confirming moves the player
straight into that act's boss room (`Act_0N_Boss`), from a town or any zone of
an act, and takes one piece from the stack; it is refused off, in a room with
no act, and in a `_Boss`, `Uber_` or `Dev_` room.

The baseline class pins the mod off as the vanilla game: no drop and no transfer
for any input, and a fresh state that is off; and the act that `RoomActIndex`
answered as Live procedure 1 measured it (the answer is the act, 1.0 in
Town_01_rm and Act_01_01, 2.0 in Act_02_01). The target class pins the chance at
the measured rate and every refusal: forced drops that skip the chance but never
the eligibility, a boss's own death, a source that is not a monster, towns and
boss rooms, the use's transfer from towns and zones and its refusals, the
transfer's target and its counts, all nine acts named (Act 7 Mevius), the seed
round trip, the key piece's `b`, its identity with every signal, its survival
through a save's two-decimal round trip, the legacy bag piece beside it, the
take-one by stack count, and the display sprite's name.

The header is compiled six times. Once as written, where every scenario must
pass. Then five negative controls, each a one-line change to the header that
must turn exactly its own scenarios red while the rest stay green:
the mod-off gates removed (the baselines that pin off fail), the act read as
`RoomActIndex`'s answer plus one (the first plan's mistake, which Live
procedure 1 measured: the act baseline fails), the piece's chance replaced by
the gem's own (the chance target fails), the use's boss-room refusal removed
(the use-refusal target fails while the transfer target still passes), and the
key piece built with a whole `b` (the `plain` shape, which merges into the
real key: the three key-piece targets fail while the legacy piece, the
take-one and the sprite still pass).
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
# The key piece's `b`; the fifth negative control builds it whole (the `plain` shape).
KEY_B_LINE = "return static_cast<double>(baseIndex) + static_cast<double>(act) / kActFractionDivisor;"
KEY_B_WHOLE = "return static_cast<double>(baseIndex);"

OFF_BASELINES = ("baseline/off-never-drops", "baseline/off-never-transfers")
ACT_BASELINE = "baseline/room-act-index-is-the-act"
BASELINES = OFF_BASELINES + (ACT_BASELINE, "baseline/default-off")
CHANCE = "target/chance-five-quarters"
USE_TRANSFER = "target/use-transfers-from-towns-and-zones"
USE_REFUSED = "target/use-refused-when-off-outside-acts-and-in-boss-rooms"
KEY_B = "target/key-piece-b-is-the-base-plus-tenths"
KEY_IDENTITY = "target/key-piece-identity-needs-every-signal"
KEY_ROUND_TRIP = "target/key-piece-survives-a-two-decimal-round-trip"
KEY_PIECE = (KEY_B, KEY_IDENTITY, KEY_ROUND_TRIP)
LEGACY = "target/legacy-bag-piece-still-recognised"
TAKE_ONE = "target/stack-use-takes-one-and-answers-by-count"
SPRITE = "target/piece-sprite-is-the-gem-sprite-name"
TARGETS = (
    CHANCE,
    "target/forced-ignores-chance-not-eligibility",
    "target/boss-kill-never-drops",
    "target/not-monster-never-drops",
    "target/town-and-boss-rooms-refused",
    USE_TRANSFER,
    USE_REFUSED,
    "target/transfer-target-is-the-act-boss-room",
    "target/every-act-has-a-boss",
    "target/seed-round-trip",
    LEGACY,
) + KEY_PIECE + (TAKE_ONE, SPRITE)
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
    # One directory per process: two criteria that run this module at the same
    # time otherwise race on one soulpiece.obj (C1083 "Permission denied").
    out = ROOT / "build/soul-piece-behavior" / f"pid-{os.getpid()}"
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
    """The six builds, compiled once for both classes."""
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
        _RUNS["whole_key_b"] = _compile_and_run(
            "soulpiece-whole-key-b", _replace_once(header, KEY_B_LINE, KEY_B_WHOLE, "the key piece's b"))
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
        cls.whole_key_b = runs["whole_key_b"]

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

    def test_mod_off_never_transfers(self):
        """Off: `off` for every room, room act and piece act; the same piece on, in town, transfers."""
        self.assertScenario("baseline/off-never-transfers")

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
    """The mod on: the chance at the measured rate, the transfer and every refusal."""

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

    def test_use_transfers_from_towns_and_zones(self):
        """A piece of any act transfers from every town and every zone of an act."""
        self.assertScenario(USE_TRANSFER)

    def test_use_refused_when_off_outside_acts_and_in_boss_rooms(self):
        """In order: `off`, `not-piece`, `no-act` (boss rooms, uber realms, Satanic Zone), `boss-room` (_Boss,
        Uber_, Dev_ rooms that have an act), each beside an input that transfers."""
        self.assertScenario(USE_REFUSED)

    def test_negative_control_use_refusal_fails_without_the_boss_room_rule(self):
        self.assertOnlyThese(self.boss_room_use, (USE_REFUSED,), "a core that transfers from boss rooms")

    def test_transfer_target_is_the_act_boss_room(self):
        """Act_01_Boss .. Act_09_Boss by act, none without one; a carried-out transfer counts by act, as
        `fresh` when the boss room answered a saved state before the reset and as `cleared` when it was gone
        after it; one that was not carried out counts as failed whatever the zone-state answers were."""
        self.assertScenario("target/transfer-target-is-the-act-boss-room")

    def test_every_act_has_a_boss_including_mevius(self):
        self.assertScenario("target/every-act-has-a-boss")

    def test_seed_round_trip_per_act(self):
        """777101..777109 and back; 777001-777003 (the signature items, Miner's Helmet) are not pieces."""
        self.assertScenario("target/seed-round-trip")

    def test_legacy_bag_piece_still_recognised(self):
        """The first builds' pieces: category and class 11, `consumable_edible_pas_logo`, c 0, a piece's seed,
        recognised on their own and through the question that knows both shapes; the plain logo, a key with a
        piece's seed, another consumable, the unique repository, a fractional `b` and an unresolved base are not."""
        self.assertScenario(LEGACY)

    def test_key_piece_b_is_the_base_plus_tenths_of_the_act(self):
        """Class 12 on `keys_bifrost_key`; `b` = index + act/10 (2.1 .. 2.9 on index 2): never whole, truncating to
        the base, equal within the game's epsilon for one act and apart from every other act and the real key."""
        self.assertScenario(KEY_B)

    def test_key_piece_identity_needs_every_signal(self):
        """Class 12, c 0, `b` truncating to the base, not whole, tenths matching the seed's act: each piece
        beside an item lacking exactly one signal (the whole-b `plain` control among them)."""
        self.assertScenario(KEY_IDENTITY)

    def test_key_piece_survives_a_two_decimal_round_trip(self):
        """`string()` keeps two decimals: a reloaded piece is still its act's piece, still stacks with a fresh
        one and stays apart from the real key and the other acts; the control shows the round trip rounds."""
        self.assertScenario(KEY_ROUND_TRIP)

    def test_negative_control_key_piece_fails_with_a_whole_b(self):
        self.assertOnlyThese(self.whole_key_b, KEY_PIECE, "a core building the key piece with a whole b")

    def test_stack_use_takes_one_and_answers_by_count(self):
        """A refusal keeps the stack and answers false; a transfer from N > 1 answers false and leaves N - 1;
        the last piece answers true; a stack of three answers false, false, true."""
        self.assertScenario(TAKE_ONE)

    def test_piece_sprite_is_the_gem_sprite_name(self):
        """`Gem_Incarnation_spr` under item info key "11", its ground companion `Pickup_Gem_Incarnation_spr`."""
        self.assertScenario(SPRITE)

    def test_status_line_and_command_words(self):
        for label in COMMANDS:
            self.assertScenario(label)


if __name__ == "__main__":
    unittest.main()
