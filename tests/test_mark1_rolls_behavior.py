"""Run the real ForgePact::Mark1Rolls core against controlled seeds and stat calls.

The four Mark 1 pieces carry the owner's "Bulwark SS" stats (the owner,
2026-10-22, replacing the 2026-10-13 decision "no item stats"). A piece's roll
lives in its definition seed `a`, as a flight charm's does: one band per
piece from 1,000,000,000, mixed radix in table order; the legacy seeds
777500..777503 stay pieces at the bottom roll. The set bonus answers the
stat dispatcher's ids at 2, 3 and 4 pieces worn.

Baseline: the legacy seeds are still pieces, every natural seed is not, and
the bonus answers nothing below two pieces, switched off or not installed.
Target: the codec round-trips every corner and every seed of each band, the
dressed stats per piece, the thresholds and their answers, and the core's
keys, dispatcher ids and set lines against the hub's curated
hs-game-sdk/curated/item_stat_keys.json. The adapter in ModuleMain.cpp
(dressing, drops, the dispatcher consumer) is pinned by
test_mark1_set_contract.py.
"""
import json
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HUB = ROOT.parent
HEADER = ROOT / "plugin/include/ForgePact/Mark1Rolls.hpp"
CURATED = HUB / "hs-game-sdk/curated/item_stat_keys.json"

# What each item stat key the pieces write is (the semantics file's names).
PIECE_KEYS = {
    154: "Defense", 29: "Enhanced Defense", 173: "to All Resistances", 201: "to All Skills",
    20: "Sockets", 48: "to Vitality", 52: "to Life", 170: "Faster Hit Recovery",
    59: "Life After each Kill", 51: "to All Attributes",
}

# The owner's per-piece table (2026-10-22), as the context's "The pieces' own stats" gives it.
OWNER_ROLLS = {
    "helmet": [(154, 60, 80), (29, 120, 180), (173, 10, 30), (201, 2, 3), (20, 1, 2)],
    "body": [(154, 210, 240), (29, 220, 280), (52, 300, 400), (201, 3, 4), (20, 3, 4)],
    "boots": [(154, 72, 90), (29, 150, 200), (59, 15, 25), (173, 20, 30)],
    "gloves": [(154, 46, 68), (29, 75, 100), (201, 2, 3)],
}
OWNER_FIXED = {"helmet": [(48, 35)], "body": [(170, 125)], "boots": [], "gloves": [(51, 20)]}
OWNER_BANDS = {
    "helmet": (1000000000, 107604, 777500), "body": (1001000000, 763964, 777501),
    "boots": (1002000000, 117249, 777502), "gloves": (1003000000, 1196, 777503),
}

# The set bonus (the owner, 2026-10-22): threshold, how it applies, dispatcher id, value, words.
OWNER_BONUS = [
    (2, "dispatcher", 92, "500", "+500 Defense"),
    (3, "dispatcher", 161, "35", "+35% to All Resistances"),
    (3, "dispatcher", 38, "25", "Replenish Life 25%"),
    (4, "automation", 0, "1", "+1 to Automation"),
    (4, "dispatcher", 2, "5", "+5 to All Skills"),
    (4, "dispatcher", 36, "750", "+750 to Life"),
    (4, "dispatcher", 97, "20", "Physical Damage Taken Reduced by 20%"),
    (4, "halfwalk", 64, "-50", "-50% Movement Speed"),
]
# The getter each answered id reaches (the dispatcher's case map, static reading).
BONUS_GETTERS = {
    92: "StatDefense", 161: "StatAllResistances", 38: "StatLifeReplenish", 2: "StatAllSkills",
    36: "StatMaxLife", 97: "StatPhysicalDamageReduction", 64: "StatMovementSpeed",
}


class Mark1RollsBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        header = HEADER.read_text(encoding="utf-8")
        cls.header = header
        core = "\n".join(line for line in header.split("\n") if not line.strip().startswith("#pragma once"))
        out = ROOT / "build/mark1-rolls-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/mark1_rolls_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_MARK1ROLLS", core)
        cpp = out / "mark1rolls.cpp"
        cpp.write_text(code, encoding="utf-8")
        cls.binary = out / ("mark1rolls.exe" if os.name == "nt" else "mark1rolls")
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
            batch = out / "compile.cmd"
            batch.write_text(
                f'@echo off\ncall "{vcvars}" >nul\nif errorlevel 1 exit /b 1\n'
                f'cl /nologo /std:c++20 /EHsc /O2 /W4 /I "{ROOT / "plugin/include"}" "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "mark1rolls.obj"}"\n'
                f'exit /b %errorlevel%\n', encoding="utf-8")
            command = ["cmd", "/d", "/c", str(batch)]
        else:
            compiler = shutil.which("c++")
            if not compiler:
                raise unittest.SkipTest("A C++20 compiler is required for native behavior tests")
            command = [compiler, "-std=c++20", "-O2", "-I", str(ROOT / "plugin/include"), str(cpp), "-o", str(cls.binary)]
        # The compiler speaks the machine's locale; decode leniently so a
        # localized diagnostic cannot itself crash the test.
        result = subprocess.run(command, cwd=out, capture_output=True, text=True, encoding="utf-8", errors="replace")
        (out / "compile.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        run = subprocess.run([str(cls.binary)], capture_output=True, text=True, encoding="utf-8", errors="replace")
        cls.output = run.stdout
        (out / "run.log").write_text(run.stdout + run.stderr, encoding="utf-8")

    def line(self, label):
        for line in self.output.split("\n"):
            if line.split(" ")[1:2] == [label]:
                return line
        raise AssertionError(f"scenario {label!r} not in harness output:\n{self.output}")

    def assertScenario(self, label):
        self.assertTrue(self.line(label).startswith("PASS "), self.line(label))

    def rows(self, tag):
        return [line.split(" ")[1:] for line in self.output.split("\n") if line.startswith(tag + " ")]

    def bonus_lines(self):
        out = []
        for line in self.output.split("\n"):
            if not line.startswith("BONUSLINE "):
                continue
            head, label, words = line.split("|")
            threshold, applied, ident, value, fmt, style = head.split(" ")[1:]
            out.append({"threshold": int(threshold), "applied": applied, "id": int(ident), "value": value,
                        "format": int(fmt), "style": int(style), "label": label, "words": words})
        return out

    def curated(self):
        return json.loads(CURATED.read_text(encoding="utf-8"))

    def test_all_scenarios_pass(self):
        self.assertIn("RESULT OK", self.output, self.output)

    # ---- baseline: what already exists stays what it is ----------------------
    def test_rolls_baseline_legacy_seeds_are_pieces_at_the_bottom_roll(self):
        self.assertScenario("baseline/legacy_seeds_decode_to_their_piece")
        self.assertScenario("baseline/legacy_seeds_roll_the_bottom_of_every_range")
        self.assertScenario("baseline/legacy_helmet_has_the_lower_socket_count")
        self.assertScenario("baseline/legacy_body_has_the_lower_socket_count")
        self.assertScenario("baseline/a_band_seed_is_not_legacy")

    def test_rolls_baseline_natural_seeds_are_not_pieces(self):
        self.assertScenario("baseline/natural_seeds_are_not_pieces")
        self.assertScenario("baseline/bands_sit_above_every_natural_seed_measured")
        self.assertScenario("baseline/bands_fit_a_32_bit_signed_integer")

    # ---- target: the codec and the dressed stats ------------------------------
    def test_rolls_target_codec_round_trips_every_corner(self):
        self.assertScenario("target/codec_round_trips_every_corner")
        self.assertScenario("target/codec_bounds_are_the_band_ends")
        self.assertScenario("target/codec_round_trips_every_seed")
        self.assertScenario("target/codec_refuses_a_value_outside_its_range")
        self.assertScenario("target/codec_first_stat_is_the_major_digit")
        self.assertScenario("target/uniform_draw_spans_the_band")
        # The band layout is a save format once released: a static_assert pins
        # it in the header, and this pins the header's numbers.
        bands = {piece: (int(lo), int(total), int(legacy)) for piece, lo, total, legacy in self.rows("BAND")}
        self.assertEqual(bands, OWNER_BANDS)
        self.assertIn("static_assert(BandsExact()", self.header)

    def test_rolls_target_stats_for_each_piece(self):
        for label in ("helmet_top", "helmet_low", "body_top", "body_mid", "boots_top_no_sockets_nothing_fixed",
                      "boots_low", "gloves_top", "gloves_low", "no_movement_speed_on_any_piece"):
            self.assertScenario("target/stats_" + label)
        rolls = {}
        for piece, key, lo, hi in self.rows("ROLL"):
            rolls.setdefault(piece, []).append((int(key), int(lo), int(hi)))
        self.assertEqual(rolls, OWNER_ROLLS)
        fixed = {piece: [] for piece in OWNER_FIXED}
        for piece, key, value in self.rows("FIXED"):
            fixed[piece].append((int(key), int(value)))
        self.assertEqual(fixed, OWNER_FIXED)

    def test_rolls_target_keys_match_the_curated_file(self):
        curated = self.curated()
        keys = curated["keys"]
        used = {int(k) for _, k, _, _ in self.rows("ROLL")} | {int(k) for _, k, _ in self.rows("FIXED")}
        self.assertEqual(used, set(PIECE_KEYS))
        for key in sorted(used):
            self.assertIn(str(key), keys, f"item stat key {key} missing from {CURATED.name}")
            self.assertEqual(keys[str(key)]["name"], PIECE_KEYS[key])
        # The header's named key constants say the same.
        for key, name in ((154, "kKeyDefense"), (29, "kKeyEnhancedDefense"), (173, "kKeyAllResistances"),
                          (201, "kKeyAllSkills"), (20, "kKeySockets"), (48, "kKeyVitality"), (52, "kKeyLife"),
                          (170, "kKeyFasterHitRecovery"), (59, "kKeyLifeAfterEachKill"), (51, "kKeyAllAttributes")):
            self.assertRegex(self.header, rf"inline constexpr int {name} = {key};")
        # Each answered dispatcher id reaches the getter the curated file names.
        ids = curated["dispatcher_ids"]
        for line in self.bonus_lines():
            if line["applied"] == "automation":
                continue
            self.assertEqual(ids[str(line["id"])]["getter"], BONUS_GETTERS[line["id"]], line)
        for element in (162, 163, 164, 165, 167):
            self.assertIn(str(element), ids)
        # Each drawn bonus line's label, format and style are a set line the game drew.
        drawn = {(s["label"], s["format"], s["style"]) for s in curated["set_lines"]}
        for line in self.bonus_lines():
            if line["applied"] == "automation":
                continue
            self.assertIn((line["label"], line["format"], line["style"]), drawn, line)

    # ---- the set bonus ----------------------------------------------------------
    def test_bonus_baseline_nothing_below_two_pieces_or_switched_off(self):
        self.assertScenario("bonus_baseline/nothing_below_two_or_switched_off_or_not_installed")
        self.assertScenario("bonus_baseline/every_line_grey_below_two_or_switched_off")
        self.assertScenario("bonus_baseline/not_active_below_two_or_switched_off")
        self.assertScenario("bonus_baseline/other_ids_untouched_even_with_four")

    def test_bonus_target_thresholds_and_answers(self):
        for label in ("answers_at_each_threshold", "each_call_from_its_own_original", "non_finite_original_left_alone",
                      "wants_the_six_answered_ids", "allres_route_dispatcher_answers_161_only", "lit_lines_by_count",
                      "words_are_value_and_label"):
            self.assertScenario("bonus_target/" + label)
        got = [(l["threshold"], l["applied"], l["id"], l["value"], l["words"]) for l in self.bonus_lines()]
        self.assertEqual(got, OWNER_BONUS)
        self.assertIn("ROUTE allres-route: dispatcher", self.output)


if __name__ == "__main__":
    unittest.main()
