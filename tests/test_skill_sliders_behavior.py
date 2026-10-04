"""Run the real skill sliders (`skillslider`, #160) in the game's place, against controlled game API responses.

The harness (tests/skill_sliders_harness.cpp) splices in the real
ForgePact::SkillSlidersMod and calls the five hooked scripts the way the game
does: through the native detour once HookOneScript installed one, straight to
the game's function otherwise. Each scenario reports the value the game
received beside what the mod logged.

The baseline scenarios pin vanilla: nothing armed, a lever set back to 0, a
`self` that is not the player, stat 75 read outside a LoadAllModifiers or
inside someone else's, a hook that could not go in natively. The target
scenarios pin what each lever must return for the player and its double cast.
"""
import os
import re
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SDK_INCLUDE = ROOT.parent / "hs-game-sdk/cpp/include"
HEADER = ROOT / "plugin/include/ForgePact/SkillSlidersMod.hpp"


def implementation(source, signature):
    """The full text of `signature`'s definition (last occurrence wins)."""
    start = source.rfind(signature)
    if start < 0:
        raise AssertionError(f"not found: {signature}")
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    raise AssertionError(f"unterminated: {signature}")


def skill_sliders(header):
    """The real class, minus the include the harness stands in for.

    Its constructor is private (a singleton); the harness builds a fresh mod
    per scenario, so the constructor alone is made public here.
    """
    text = "\n".join(
        line for line in header.split("\n")
        if not line.strip().startswith("#pragma once")
        and '#include "Common.hpp"' not in line
    )
    private_ctor = "private:\n    SkillSlidersMod() = default;"
    if text.count(private_ctor) != 1:
        raise AssertionError("SkillSlidersMod's private default constructor was not found once")
    return text.replace(private_ctor, "public:\n    SkillSlidersMod() = default;\nprivate:")


def fresh_line(lever):
    return f"skillslider {lever} +0 hook=none first=- own=0 double=0 other=0 last-other=-"


def refusal(script, route):
    return (f"skillslider: {script} hook is {route} - the game calls it directly, "
            "so the bonus could never apply; not armed")


class SkillSlidersBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        plugin = (ROOT / "plugin/ModuleMain.cpp").read_text(encoding="utf-8").replace("\r\n", "\n")
        header = HEADER.read_text(encoding="utf-8").replace("\r\n", "\n")
        prefix = re.search(r"^static constexpr std::string_view kGmlScriptPrefix = [^\n]*$", plugin, re.M)
        if not prefix:
            raise AssertionError("kGmlScriptPrefix not found in ModuleMain.cpp")
        helpers = "\n\n".join([prefix.group(0)] + [implementation(plugin, signature) for signature in (
            "static std::string Lower(",
            "static std::string FirstToken(",
            "static consteval const char* SdkShortScriptName(",
        )])

        out = ROOT / "build/skill-sliders-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/skill_sliders_harness.cpp").read_text(encoding="utf-8")
        code = (code.replace("// PRODUCTION_HELPERS", helpers)
                    .replace("// PRODUCTION_SKILLSLIDERS", skill_sliders(header)))
        cpp = out / "skillsliders.cpp"
        cpp.write_text(code, encoding="utf-8")

        # The player build: the sliders ship, so they are tested as it compiles them.
        cls.binary = out / ("skillsliders.exe" if os.name == "nt" else "skillsliders")
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
                f'cl /nologo /std:c++20 /EHsc /O2 /DFORGEPACT_RELEASE /I "{SDK_INCLUDE}" "{cpp}" '
                f'/Fe:"{cls.binary}" /Fo:"{out / "skillsliders.obj"}"\n'
                f'exit /b %errorlevel%\n', encoding="utf-8")
            command = ["cmd", "/d", "/c", str(batch)]
        else:
            compiler = shutil.which("c++")
            if not compiler:
                raise unittest.SkipTest("A C++20 compiler is required for native behavior tests")
            command = [compiler, "-std=c++20", "-O2", "-DFORGEPACT_RELEASE", f"-I{SDK_INCLUDE}", str(cpp),
                       "-o", str(cls.binary)]

        # The compiler speaks the machine's locale; decode leniently so a
        # localized diagnostic cannot itself crash the test.
        result = subprocess.run(command, cwd=out, capture_output=True, text=True, encoding="utf-8", errors="replace")
        (out / "compile.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)

        run = subprocess.run([str(cls.binary)], capture_output=True, text=True, encoding="utf-8", errors="replace")
        cls.output = run.stdout
        (out / "run.log").write_text(run.stdout + run.stderr, encoding="utf-8")

    def scenario(self, label):
        for line in self.output.split("\n"):
            if line.startswith(f"SCENARIO {label} "):
                return line[len(f"SCENARIO {label} "):]
        raise AssertionError(f"scenario {label!r} not in harness output:\n{self.output}")

    def fields(self, label):
        return dict(piece.split("=", 1) for piece in self.scenario(label).split(" ") if "=" in piece)

    def logs(self, label):
        """The mod's own lines (the stand-in installer's are left out)."""
        prefix = f"LOG {label} :: "
        return [line[len(prefix):] for line in self.output.split("\n")
                if line.startswith(prefix) and line[len(prefix):].startswith("skillslider")]

    def status_lines(self, label):
        return [line for line in self.logs(label) if re.match(r"^skillslider \w+ \+", line)]

    def first_call_lines(self, label):
        return [line for line in self.logs(label) if ": first boosted call " in line]

    def test_harness_ran(self):
        self.assertIn("HARNESS DONE", self.output, self.output)
        self.assertNotIn("threw=", self.output, self.output)

    # ---- baseline: vanilla stays vanilla ----------------------------------

    def test_baseline_nothing_armed_every_result_reaches_its_caller_unchanged(self):
        self.assertEqual(self.fields("baseline_off"), {
            "spell": "0", "ranged": "1", "aoe": "[0,7,0,0]",
            "lam": "player:75=0,player:74=0,player:554=0", "out75": "0",
            "installs": "0", "builtins": "0",
        })

    def test_baseline_a_value_of_zero_installs_no_hook(self):
        self.assertEqual(self.fields("zero_installs_nothing"), {"spell": "0", "installs": "0", "builtins": "0"})
        self.assertEqual(self.logs("zero_installs_nothing"), [
            "skillslider projamount -> +0 (native, no hook)",
            "skillslider aoesize -> +0 (native, no hook)",
            "skillslider projspeed -> +0 (native, no hook)",
        ])

    def test_baseline_set_back_to_zero_is_a_pure_pass_through(self):
        fields = self.fields("off_after_on")
        self.assertEqual(fields["on"], "2,3,[50,7,0,0]")
        self.assertEqual(fields["onreads"], "player:75=50,player:74=0,player:554=0")
        self.assertEqual(fields["off"], "0,1,[0,7,0,0],0")
        self.assertEqual(fields["offreads"],
                         "player:75=0,player:74=0,player:554=0,merc:75=0,merc:74=0,merc:554=0")
        self.assertEqual(fields["builtinsWhileOff"], "0")
        self.assertEqual(fields["countersUnchanged"], "yes")
        self.assertEqual(fields["installs"], "5")

    def test_baseline_a_self_that_is_not_the_player_is_unchanged_and_counted_other(self):
        self.assertEqual(self.fields("amount_scope"), {
            "player": "2", "double": "2", "merc": "0", "enemy": "0", "basic": "0", "null": "0",
            "noindex": "0", "throwing": "0", "rangedplayer": "3", "rangeddouble": "3", "rangedmerc": "1",
        })
        self.assertEqual(self.status_lines("amount_scope")[0],
                         "skillslider projamount +2 hook=native first=0->2 own=2 double=2 other=7 "
                         "last-other=Mercenary_obj")

    def test_baseline_last_other_names_the_object_or_reads_unknown(self):
        lines = [line for line in self.status_lines("last_other_named") if line.startswith("skillslider projamount ")]
        self.assertEqual(lines, [
            "skillslider projamount +1 hook=native first=waiting own=0 double=0 other=2 "
            "last-other=Aztec_Sword_Skeleton_obj",
            "skillslider projamount +1 hook=native first=waiting own=0 double=0 other=3 last-other=?",
            "skillslider projamount +1 hook=native first=waiting own=0 double=0 other=5 last-other=?",
        ])

    def test_baseline_speed_outside_the_players_load_all_modifiers_is_unchanged(self):
        fields = self.fields("speed_target")
        self.assertEqual(fields["out75"], "0")
        self.assertEqual(fields["after75"], "0")
        self.assertEqual(fields["merc"], "merc:75=0,merc:74=0,merc:554=0")
        # Basic attacks run LoadAllModifiers with self=Projectile_Player_obj.
        self.assertEqual(fields["basic"], "basic:75=0,basic:74=0,basic:554=0")

    def test_baseline_a_hook_that_is_not_native_is_refused_and_leaves_the_value_zero(self):
        self.assertEqual(self.fields("table_only_refused"), {"aoe": "[0,7,0,0]", "installs": "1"})
        logs = self.logs("table_only_refused")
        self.assertEqual(logs.count(refusal("StatAOESkillSize", "TABLE-ONLY")), 2, logs)
        self.assertEqual(self.first_call_lines("table_only_refused"), [])
        self.assertIn("skillslider aoesize -> +0", logs)
        self.assertEqual(self.status_lines("table_only_refused")[1],
                         "skillslider aoesize +0 hook=StatAOESkillSize:TABLE-ONLY first=- own=0 double=0 "
                         "other=0 last-other=-")

    def test_baseline_one_helper_not_native_refuses_the_whole_lever(self):
        self.assertEqual(self.fields("partial_refused"), {"spell": "0", "ranged": "1"})
        self.assertIn(refusal("ReturnExtraProjectilesRanged", "TABLE-ONLY"), self.logs("partial_refused"))

    def test_baseline_a_script_not_found_is_refused(self):
        self.assertEqual(self.fields("not_found_refused"),
                         {"lam": "player:75=0,player:74=0,player:554=0", "out75": "0"})
        logs = self.logs("not_found_refused")
        self.assertIn(refusal("LoadAllModifiers", "FAILED"), logs)
        self.assertEqual(self.status_lines("not_found_refused")[2],
                         "skillslider projspeed +0 hook=LoadAllModifiers:FAILED first=- own=0 double=0 "
                         "other=0 last-other=-")
        self.assertEqual(logs[-1], "skillslider projspeed -> +0")

    # ---- target: each lever, for the player and its double cast ------------

    def test_target_projectile_amount_adds_on_both_helpers(self):
        fields = self.fields("amount_scope")
        self.assertEqual((fields["player"], fields["double"]), ("2", "2"))
        self.assertEqual((fields["rangedplayer"], fields["rangeddouble"]), ("3", "3"))
        # The base-6 call that rides along with a cast has self=Player_obj.
        self.assertEqual(self.fields("amount_shapes")["base6"], "8")

    def test_target_projectile_amount_on_other_result_shapes(self):
        fields = self.fields("amount_shapes")
        self.assertEqual(fields["array"], "3,9")
        self.assertEqual(fields["gamearray"], "1,9")
        self.assertEqual(fields["undefinedkind"], "5")
        self.assertEqual(fields["string"], "text:1")

    def test_target_aoe_adds_to_element_zero_of_a_copy(self):
        self.assertEqual(self.fields("aoe_target"), {
            "player": "[50,7,0,0]", "double": "[50,7,0,0]", "merc": "[0,7,0,0]",
            "built": "[0,7,0,0][0,7,0,0][0,7,0,0]", "installs": "1",
        })

    def test_target_speed_adds_to_stat_75_inside_the_players_load_all_modifiers(self):
        fields = self.fields("speed_target")
        self.assertEqual(fields["player"], "player:75=50,player:74=0,player:554=0")
        self.assertEqual(fields["double"], "double:75=50,double:74=0,double:554=0")
        self.assertEqual(self.status_lines("speed_target")[2],
                         "skillslider projspeed +50 hook=native first=0->50 own=1 double=1 other=2 "
                         "last-other=Projectile_Player_obj")

    def test_target_the_innermost_load_all_modifiers_decides(self):
        self.assertEqual(self.fields("speed_nested"), {
            "playerouter": "player:75=50,player:74=0,player:554=0,merc:75=0,merc:74=0,merc:554=0,player:75after=50",
            "mercouter": "merc:75=0,merc:74=0,merc:554=0,player:75=50,player:74=0,player:554=0,merc:75after=0",
            "out75": "0",
        })

    def test_target_speed_adds_on_top_of_what_gear_gives(self):
        self.assertEqual(self.fields("speed_adds_to_native"), {"player": "player:75=50,player:74=3,player:554=0"})

    def test_target_the_player_as_a_reference_and_as_an_instance_pointer(self):
        self.assertEqual(self.fields("self_kinds"), {
            "ref": "2", "obj": "2", "refindex": "2",
            "lamobj": "playerobj:75=40,playerobj:74=0,playerobj:554=0",
            "lamrefindex": "playerrefindex:75=40,playerrefindex:74=0,playerrefindex:554=0",
        })

    def test_target_the_first_boosted_call_is_reported_once_per_arming(self):
        self.assertEqual(self.first_call_lines("first_call_once_per_arming"), [
            "skillslider ReturnExtraSpellProjectiles: first boosted call 0 -> 2",
            "skillslider ReturnExtraSpellProjectiles: first boosted call 1 -> 4",
        ])
        self.assertEqual(self.fields("first_call_once_per_arming"), {"off": "1"})
        self.assertEqual(self.first_call_lines("first_call_aoe_speed"), [
            "skillslider StatAOESkillSize: first boosted call 0 -> 50",
            "skillslider ReturnSpecificStat: first boosted call 0 -> 50",
        ])

    def test_target_clamps(self):
        self.assertEqual(self.fields("clamps"), {
            "a": "3", "b": "2", "c": "5", "d": "0", "e": "0", "f": "0",
            "g": "[12.5,7,0,0]", "h": "[100,7,0,0]",
            "i": "0:player:75=100,player:74=0,player:554=0",
            "j": "player:75=0,player:74=0,player:554=0",
        })
        replies = [line for line in self.logs("clamps") if ": first boosted call " not in line]
        self.assertEqual(replies, [
            "skillslider projamount -> +3",
            "skillslider projamount -> +2",
            "skillslider projamount -> +5",
            "skillslider projamount -> +0",
            "skillslider projamount -> +0",
            "skillslider projamount -> +0",
            "skillslider aoesize -> +12.5",
            "skillslider aoesize -> +100",
            "skillslider projspeed -> +100",
            "skillslider projspeed -> +0",
        ])

    def test_the_ceilings_are_named_constants(self):
        header = HEADER.read_text(encoding="utf-8")
        for name, value in (("kProjAmountMax", "5.0"), ("kAoeSizeMax", "100.0"), ("kProjSpeedMax", "100.0")):
            self.assertRegex(header, rf"static constexpr double {name} = {re.escape(value)};")

    # ---- the status lines and the command's replies ------------------------

    def test_status_lines_at_a_fresh_launch(self):
        expected = [fresh_line("projamount"), fresh_line("aoesize"), fresh_line("projspeed")]
        self.assertEqual(self.logs("list_fresh"), expected + expected)
        self.assertEqual(self.fields("list_fresh"), {"builtins": "0"})

    def test_status_lines_after_arming(self):
        self.assertEqual(self.status_lines("list_after_arming"), [
            "skillslider projamount +2 hook=native first=0->2 own=1 double=0 other=1 last-other=Mercenary_obj",
            "skillslider aoesize +50 hook=native first=waiting own=0 double=0 other=0 last-other=-",
            fresh_line("projspeed"),
        ])

    def test_bad_input(self):
        self.assertEqual(self.logs("bad_input"), [
            "skillslider: unknown 'foo' (projamount, aoesize, projspeed, list)",
            "skillslider: the value must be a number",
            "skillslider: the value must be a number",
            "skillslider projamount -> +1",
            "skillslider ReturnExtraSpellProjectiles: first boosted call 0 -> 1",
        ])
        self.assertEqual(self.fields("bad_input"), {"installs": "2", "spell": "1"})


if __name__ == "__main__":
    unittest.main()
