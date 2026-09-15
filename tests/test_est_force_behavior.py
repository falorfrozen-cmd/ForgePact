"""Run the real eSt gate correction against a controlled game API.

Companion to est_force_harness.cpp. Special Content works by holding the
game's own eSt gate open, and the correction used to be written on every
single frame - three CallBuiltins plus one array_get per forced entry, 60
times a second, almost always to discover the value was already right.

The obvious fix is a frame throttle, and it is the wrong one.
docs/S10-special-content-notes.md establishes that the game refills the whole
array at Room Start and that the mechanic bodies read global.eSt directly at
step time, inside that same room-load sequence: a correction 15 frames late is
a correction after the read that decided whether anything spawns. That is the
"Check a Permission Where It Is Used" defect from AGENTS.md, and the symptom
would be Special Content quietly producing less, intermittently.

So the caller is gated on the room key instead - full apply on the frame the
room changes, plus a periodic safety re-poll because nobody has proven Room
Start is the only time the game writes eSt. These scenarios assert the timing,
not only the call count.
"""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


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


def declaration(source, prefix):
    """One whole single-line declaration, so its value is never restated here."""
    for line in source.split("\n"):
        if line.strip().startswith(prefix):
            return line
    raise AssertionError(f"not found: {prefix}")


class EstForceBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = (ROOT / "plugin/ModuleMain.cpp").read_text(encoding="utf-8")

        production = "\n".join([
            declaration(cls.plugin, "static const uint32_t kEstPollFrames"),
            declaration(cls.plugin, "static int64_t g_EstLastRoom"),
            implementation(cls.plugin, "static void EstForceSet("),
            implementation(cls.plugin, "static void EstForceErase("),
            implementation(cls.plugin, "static void EstForceClear()"),
            implementation(cls.plugin, "static RValue GlobalArray("),
            implementation(cls.plugin, "static void EstForceApply()"),
            implementation(cls.plugin, "static int64_t CurrentRoomKey()"),
            implementation(cls.plugin, "static void EstForceTick("),
        ])

        out = ROOT / "build/est-force-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/est_force_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_EST", production)
        cpp = out / "estforce.cpp"
        cpp.write_text(code, encoding="utf-8")

        cls.binary = out / ("estforce.exe" if os.name == "nt" else "estforce")
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
                f'cl /nologo /std:c++20 /EHsc /O2 "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "estforce.obj"}"\n'
                f'exit /b %errorlevel%\n', encoding="utf-8")
            command = ["cmd", "/d", "/c", str(batch)]
        else:
            compiler = shutil.which("c++")
            if not compiler:
                raise unittest.SkipTest("A C++20 compiler is required for native behavior tests")
            command = [compiler, "-std=c++20", "-O2", str(cpp), "-o", str(cls.binary)]

        result = subprocess.run(command, cwd=out, capture_output=True, text=True)
        (out / "compile.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)

        run = subprocess.run([str(cls.binary)], capture_output=True, text=True)
        cls.output = run.stdout
        (out / "run.log").write_text(run.stdout + run.stderr, encoding="utf-8")

    def line(self, label):
        for line in self.output.split("\n"):
            if line.split(" ")[1:2] == [label]:
                return line
        raise AssertionError(f"scenario {label!r} not in harness output:\n{self.output}")

    def assertScenario(self, label):
        self.assertTrue(self.line(label).startswith("PASS "), self.line(label))

    def test_all_scenarios_pass(self):
        self.assertIn("RESULT OK", self.output, self.output)

    def test_all_off_costs_nothing(self):
        # The helper is called unconditionally from FrameCallback, so with
        # nothing forced it must not touch the runner at all.
        for label in ("all_off/variable_global_get", "all_off/array_get", "all_off/array_set"):
            self.assertScenario(label)

    def test_the_gate_is_opened(self):
        self.assertScenario("opened/est0")
        self.assertScenario("opened/writes")

    def test_a_room_start_overwrite_is_corrected_on_the_same_frame(self):
        # Identical timing to the per-frame write it replaces: zero added
        # latency on the only overwrite anybody has observed.
        self.assertScenario("room_start/corrected_same_frame")
        self.assertScenario("room_start/est0")

    def test_an_overwrite_with_no_room_change_is_still_corrected(self):
        # The safety re-poll. Nobody has proven the game only writes eSt at
        # Room Start, and AGENTS.md says to treat an unproven negative as
        # unproven.
        self.assertScenario("no_room_change/corrected")
        self.assertScenario("no_room_change/frames_to_correct")

    def test_idle_frames_cost_a_fraction_of_what_they_did(self):
        # The harness prints both numbers, measured in the same run against
        # the pre-change shape, rather than quoting one from a comment.
        self.assertScenario("idle60/array_get_before")
        self.assertScenario("idle60/array_get_after")
        self.assertScenario("idle60/no_redundant_writes")

    def test_an_unreadable_room_key_never_suppresses_the_correction(self):
        # A sentinel that means "unknown" must not compare equal to a real
        # value, and must never be the reason nothing happens.
        self.assertScenario("unreadable_room/still_corrected")
        self.assertScenario("unreadable_room/est0")

    def test_every_forced_entry_is_corrected(self):
        for label in ("multi/writes", "multi/est0", "multi/est2"):
            self.assertScenario(label)

    def test_a_missing_global_is_survived(self):
        self.assertScenario("no_global/writes")
        self.assertScenario("no_global/array_get")

    def test_changing_what_is_forced_re_opens_the_gate_on_the_next_frame(self):
        # Toggling Special Content off and on again without leaving the room
        # used to leave the room key pointing at that room, so nothing looked
        # changed and the gate stayed vanilla (eSt[0] = 35, closed) until the
        # next re-poll. Measured here as ~250 ms of the feature being off
        # immediately after the user switched it on.
        for label in ("toggle_off_on/corrected_next_frame", "toggle_off_on/est0",
                      "toggle_erase/corrected_next_frame", "toggle_erase/est0",
                      "toggle_erase/est2_left_vanilla"):
            self.assertScenario(label)


class EstForceSourceConstraintTests(unittest.TestCase):
    """The two hard constraints from the crash history.

    Every attempt to write eSt from anywhere other than the frame callback
    crashed the game - docs/S10-special-content-notes.md records "Room
    Start'ta global eSt override -> cokme", the same from a mechanic scope,
    and zeroing ReturnSpecificStat's return. The experimental gates those
    attempts lived behind (kEstOverrideEnabled, gEstGateEnabled,
    gStatGateEnabled) are not in this plugin at all, and must not come back.
    """

    @classmethod
    def setUpClass(cls):
        cls.plugin = (ROOT / "plugin/ModuleMain.cpp").read_text(encoding="utf-8")

    def test_the_only_est_write_is_inside_est_force_apply(self):
        # The eSt array is reached through exactly two call sites: the
        # correction, and the read-only `eststat` report. Asserting on the
        # literal is what keeps a third one from appearing quietly.
        self.assertEqual(self.plugin.count('"eSt"'), 2, "a new eSt call site appeared")
        apply_body = implementation(self.plugin, "static void EstForceApply()")
        stat_body = implementation(self.plugin, "static void EstStat()")
        self.assertIn('"eSt"', apply_body)
        self.assertIn('"eSt"', stat_body)
        self.assertIn('CallBuiltin("array_set"', apply_body)
        self.assertNotIn("array_set", stat_body)

    def test_the_room_change_is_detected_by_a_read_not_a_hook(self):
        body = implementation(self.plugin, "static int64_t CurrentRoomKey()")
        self.assertIn("GetInstanceMember", body)
        self.assertIn('"room"', body)
        self.assertNotIn("HookOneScript", body)
        self.assertNotIn("HookBuiltin", body)

    def test_room_start_is_not_hooked(self):
        # The game refills eSt from Controller_obj's Room Start event. Hooking
        # it was tried and crashed the game; this is a read of the room key,
        # nothing more.
        for forbidden in ("Other_5", "RoomStart", "Room_Start"):
            self.assertNotIn(forbidden, self.plugin)

    def test_the_crashing_gate_paths_are_still_absent(self):
        # NOT "default to false": they are not in this source at all, and
        # re-introducing one is what this test exists to catch.
        for name in ("kEstOverrideEnabled", "gEstGateEnabled", "gStatGateEnabled"):
            self.assertNotIn(name, self.plugin, f"{name} must stay out of the plugin")

    def test_every_mutation_of_the_forced_set_goes_through_a_mutator(self):
        # The room gate only applies on the frame the room key CHANGES, so a
        # mutation that does not drop the remembered key leaves the gate
        # vanilla until the next re-poll - ~250 ms after the user acted. Three
        # mutators, one drop each, and no fourth way in.
        mutators = {
            "g_EstForce[": "static void EstForceSet(",
            "g_EstForce.erase(": "static void EstForceErase(",
            "g_EstForce.clear()": "static void EstForceClear()",
        }
        for token, signature in mutators.items():
            self.assertEqual(self.plugin.count(token), 1, f"{token} appears outside its mutator")
            body = implementation(self.plugin, signature)
            self.assertIn(token, body)
            self.assertIn("g_EstLastRoom = INT64_MIN;", body)

    def test_the_forced_set_has_no_reference_outside_the_known_good_inventory(self):
        # Pinning three SPELLINGS covers three spellings, not the invariant.
        # `.insert(...)`, `.emplace(...)` and `.at(k) = v` all satisfy the
        # counts above while being a fourth way in - and a fourth mutation site
        # that does not drop the remembered room key is exactly the ~250 ms
        # vanilla-gate regression the mutators exist to prevent. So the whole
        # reference set is pinned: a new one has to be justified here before
        # this passes, whatever it is spelled.
        #
        # This inventory does NOT catch the fifth spelling, and cannot: a write
        # through a non-const `for (auto& kv : g_EstForce)` adds no new
        # reference, so it stays green while changing what is forced. Measured,
        # not assumed - `kv.second = ...` inside the existing loop passed all 17
        # tests. Both loops over g_EstForce are therefore `const auto&`, which
        # turns that spelling into error C3490 at compile time. The const is
        # load-bearing; the entries below are asserted with it.
        references = []
        for line in self.plugin.split("\n"):
            code = line.split("//")[0].strip()
            if "g_EstForce" in code.replace("g_EstForceWrites", ""):
                references.append(code)
        self.assertEqual(references, [
            "static std::map<int, double> g_EstForce;",          # the declaration
            "if (g_EstForce.empty() || !g_Yytk) return;",        # EstForceApply: read
            "for (const auto& kv : g_EstForce) {",               # EstForceApply: read, const
            "g_EstForce[slot] = value;",                         # EstForceSet
            "g_EstForce.erase(slot);",                           # EstForceErase
            "g_EstForce.clear();",                               # EstForceClear
            "if (g_EstForce.empty() || !g_Yytk) return;",        # EstForceTick: all-off
            'if (g_EstForce.empty()) s += " (yok)";',            # eststat: read
            "for (const auto& kv : g_EstForce)",                 # eststat: read, const
        ])

    def test_the_tick_is_called_from_the_frame_callback(self):
        frame = implementation(self.plugin, "void FrameCallback(FWFrame& FrameContext)")
        self.assertIn("EstForceTick(fc)", frame)
        # ...and the apply itself is reached only through it, never directly
        # from a hook, a command or a room event.
        self.assertEqual(self.plugin.count("EstForceApply();"), 1)
        self.assertIn("EstForceApply();", implementation(self.plugin, "static void EstForceTick("))


if __name__ == "__main__":
    unittest.main()
