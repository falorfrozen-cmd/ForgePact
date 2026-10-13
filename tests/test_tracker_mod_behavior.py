"""Run the HS Offline Tracker sensor's real core against controlled calls.

`tracker 1` (ForgePact #185) counts gold, XP and kills through the
HS-Offline-Tracker producer's adaptive counter rules, records Set, Satanic,
Angelic, Heroic and Unholy ground drops that pass the same creation guard Loot
announcements uses, notes room and Satanic zone changes and the character's
name, and appends all of it as protocol-v1 lines to one journal file per game
process, 30 frames at a time. These scenarios pin what ModuleMain.cpp's adapter
takes from plugin/include/ForgePact/TrackerMod.hpp, compiled whole by
tests/tracker_mod_harness.cpp.

Baseline: with the switch off nothing is counted, decided, remembered or
written, and no install is asked for. Target: the counter rules (decoding,
each capture rule, the explicit-false XP rejection, saturation, unsupported
kinds); the drop decision order with Mythic 5 and Common 1 untracked; arming;
room changes and the zone code; every record kind's envelope, a sequence that
rises by 1 and names escaped; one append per 30 frames, a heartbeat every
10 s of the frame clock and capped buffers that count what they drop; the stat
line's keys. The negative control is the same harness built with the creation
guard compiled out (FORGEPACT_TRACKER_CREATION_GUARD=0), which must record the
bag drop the shipped build holds.

The record-kinds scenario is also written through the core's own Win32 writer
into a temporary directory, and every line of that file is parsed here as
JSON. Writes only under build/tracker-mod-behavior.
"""
import json
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

KINDS = {"bridge_status", "room", "progress", "satanic_zone", "vitals", "ground_drop", "session_delta", "heartbeat"}
ENVELOPE = ("protocol", "v", "kind", "sequence", "emitted_at_unix_ms", "process_id", "build_id")


def _compile(out, name, defines):
    header = (ROOT / "plugin/include/ForgePact/TrackerMod.hpp").read_text(encoding="utf-8")
    core = "\n".join(line for line in header.split("\n") if not line.strip().startswith("#pragma once"))
    code = (ROOT / "tests/tracker_mod_harness.cpp").read_text(encoding="utf-8")
    code = code.replace("// PRODUCTION_TRACKER_MOD", core)
    cpp = out / f"{name}.cpp"
    cpp.write_text(code, encoding="utf-8")
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
        flags = " ".join(f"/D{d}" for d in defines)
        batch = out / f"compile-{name}.cmd"
        batch.write_text(
            f'@echo off\ncall "{vcvars}" >nul\nif errorlevel 1 exit /b 1\n'
            f'cl /nologo /std:c++20 /EHsc /O2 /W4 /utf-8 {flags} /I "{ROOT / "plugin/include"}" "{cpp}" '
            f'/Fe:"{binary}" /Fo:"{out / (name + ".obj")}"\n'
            f'exit /b %errorlevel%\n', encoding="utf-8")
        command = ["cmd", "/d", "/c", str(batch)]
    else:
        compiler = shutil.which("c++")
        if not compiler:
            raise unittest.SkipTest("A C++20 compiler is required for native behavior tests")
        command = [compiler, "-std=c++20", "-O2", *(f"-D{d}" for d in defines), "-I", str(ROOT / "plugin/include"),
                   str(cpp), "-o", str(binary)]
    result = subprocess.run(command, cwd=out, capture_output=True, text=True, encoding="utf-8", errors="replace")
    (out / f"compile-{name}.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode:
        raise AssertionError(result.stdout + result.stderr)
    return binary


class TrackerModBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # One directory per process: parallel criteria runs would otherwise
        # race on one .obj.
        out = ROOT / "build/tracker-mod-behavior" / f"pid-{os.getpid()}"
        out.mkdir(parents=True, exist_ok=True)
        cls.addClassCleanup(shutil.rmtree, out, ignore_errors=True)
        journal_dir = out / "bp_ipc"
        journal_dir.mkdir()
        shipped = _compile(out, "tracker", [])
        control = _compile(out, "tracker_guard_off", ["FORGEPACT_TRACKER_CREATION_GUARD=0"])
        run = subprocess.run([str(shipped), str(journal_dir)], capture_output=True, text=True, encoding="utf-8",
                             errors="replace")
        cls.output = run.stdout
        (out / "run.log").write_text(run.stdout + run.stderr, encoding="utf-8")
        control_run = subprocess.run([str(control)], capture_output=True, text=True, encoding="utf-8", errors="replace")
        cls.control_output = control_run.stdout
        (out / "control.log").write_text(control_run.stdout + control_run.stderr, encoding="utf-8")
        cls.journal_lines = None
        for line in cls.output.split("\n"):
            if line.startswith("FILE "):
                path = Path(line[5:].strip())
                if path.is_file():
                    cls.journal_lines = path.read_bytes().decode("utf-8").split("\n")

    def line(self, label, output=None):
        for line in (output if output is not None else self.output).split("\n"):
            if line.split(" ")[1:2] == [label]:
                return line
        raise AssertionError(f"scenario {label!r} not in harness output:\n{self.output}")

    def assertScenarios(self, *labels, output=None):
        for label in labels:
            self.assertTrue(self.line(label, output).startswith("PASS "), self.line(label, output))

    def test_all_scenarios_pass(self):
        self.assertIn("RESULT OK", self.output, self.output)
        self.assertNotIn("FAIL ", self.output, self.output)

    # ---- baseline: the mod off ---------------------------------------------

    def test_baseline_off_records_nothing_and_installs_nothing(self):
        self.assertScenarios("baseline/off_counts_nothing", "baseline/off_never_installs",
                             "baseline/off_records_no_drop", "baseline/off_notes_no_room")

    # ---- the counter rules ---------------------------------------------------

    def test_decoding(self):
        self.assertScenarios("decode/real_integral_accepted", "decode/real_fraction_refused",
                             "decode/real_nan_and_infinity_refused", "decode/real_above_2p53_refused",
                             "decode/int32_and_int64_accepted", "decode/zero_is_an_id_not_a_delta",
                             "decode/negative_refused", "decode/unsupported_kinds_refused")

    def test_the_three_capture_rules(self):
        self.assertScenarios("gold/argc_1_to_4_the_last_positive_argument", "gold/argc_0_and_5_refused",
                             "gold/no_positive_argument_refused", "xp/argc_exactly_2_argument_0",
                             "kill/argc_exactly_1_a_non_negative_integral_id")

    def test_counting_rejection_and_saturation(self):
        self.assertScenarios("count/gold_sums_accepted_and_counts_every_call",
                             "count/xp_explicit_false_is_a_rejection", "count/one_kill_per_accepted_call",
                             "count/sums_saturate_instead_of_wrapping")

    # ---- the drop decision order -----------------------------------------------

    def test_the_tracked_rarities_and_the_untracked_ones(self):
        self.assertScenarios("drops/the_five_tracked_rarities_are_recorded", "drops/each_names_its_rarity",
                             "drops/mythic_5_and_common_1_are_untracked", "drops/every_other_code_is_untracked",
                             "drops/an_unread_rarity_is_untracked", "drops/the_same_item_twice_is_a_duplicate",
                             "drops/unread_type_or_id_is_not_written", "drops/ground_drop_shape",
                             "drops/weapon_type_only_above_0_and_no_empty_name",
                             "drops/identity_rules_match_loot_announcements")

    def test_the_creation_guard_holds_a_bag_drop(self):
        self.assertScenarios("control/guard_is_on_in_the_shipped_build", "drops/a_bag_drop_is_held_first",
                             "drops/created_the_frame_before_counts_two_frames_back_does_not",
                             "drops/off_clears_the_creation_window", "drops/the_key_kind_is_part_of_the_key",
                             "drops/the_creation_window_is_capped")

    def test_negative_control_without_the_guard_the_bag_drop_is_recorded(self):
        self.assertIn("RESULT OK", self.control_output, self.control_output)
        self.assertScenarios("control/guard_off_records_the_bag_drop", output=self.control_output)

    # ---- arming, the room and the zone ------------------------------------------

    def test_arming(self):
        self.assertScenarios("arming/on_without_a_character_waits", "arming/a_character_installs_once",
                             "arming/off_after_an_install_still_says_installed",
                             "arming/looks_for_the_player_every_60_frames")

    def test_room_changes_and_the_zone(self):
        self.assertScenarios("room/the_first_room_and_each_change_are_recorded", "zone/code_from_an_act_room",
                             "zone/a_satanic_zone_and_a_vitals_record")

    # ---- the journal ------------------------------------------------------------

    def test_every_record_kind_its_envelope_and_the_sequence(self):
        self.assertScenarios("journal/every_kind_carries_the_envelope", "journal/the_sequence_rises_by_one_from_1",
                             "journal/the_session_delta_names_its_source",
                             "journal/the_bridge_status_says_ready_or_blocked_with_a_reason",
                             "journal/the_heartbeat_has_no_fields", "journal/every_record_is_one_line",
                             "journal/an_empty_name_writes_nothing")

    def test_names_are_escaped(self):
        self.assertScenarios("escape/quotes_backslashes_and_controls", "escape/non_ascii_utf8_passes_through",
                             "escape/invalid_utf8_becomes_the_replacement_character")

    def test_batching_heartbeat_and_caps(self):
        self.assertScenarios("batch/one_append_per_30_frames", "batch/a_heartbeat_every_10_s_of_the_frame_clock",
                             "batch/lines_count_what_was_appended", "batch/an_empty_flush_frame_appends_nothing",
                             "batch/a_nonzero_delta_waits_for_the_flush_frame", "batch/the_flush_frame_writes_it_once",
                             "batch/no_second_delta_without_new_counts",
                             "batch/drain_writes_the_pending_delta_and_everything_buffered",
                             "cap/the_buffer_counts_and_drops_past_its_cap", "cap/a_dropped_record_takes_no_sequence",
                             "cap/the_buffer_takes_records_again_once_appended",
                             "cap/the_file_counts_and_drops_past_its_cap",
                             "cap/a_failed_append_is_counted_and_never_retried")

    def test_the_stat_line(self):
        self.assertScenarios("stat/fresh_off", "stat/the_keys_live_procedure_1_reads", "stat/route_combination",
                             "stat/verdict_names")

    # ---- the file the writer wrote, parsed as JSON --------------------------------

    @unittest.skipUnless(os.name == "nt", "the writer is Win32")
    def test_the_written_journal_parses_line_by_line(self):
        self.assertScenarios("writer/opens_the_session_file_and_appends_whole_lines")
        lines = self.journal_lines
        self.assertIsNotNone(lines, self.output)
        self.assertEqual(lines[-1], "", "the file must end with a newline")
        records = [json.loads(text) for text in lines[:-1]]
        self.assertEqual({r["kind"] for r in records}, KINDS)
        for i, record in enumerate(records, start=1):
            for key in ENVELOPE:
                self.assertIn(key, record)
            self.assertEqual(record["protocol"], "hs-offline-tracker/1")
            self.assertEqual(record["v"], 1)
            self.assertEqual(record["sequence"], i)
            self.assertEqual(record["process_id"], 4242)
            self.assertEqual(record["build_id"], "forgepact-test")
            self.assertIsInstance(record["emitted_at_unix_ms"], int)
        by_kind = {r["kind"]: r for r in records}
        self.assertEqual(by_kind["progress"]["name"], 'Ann "the" \\Brave\\ \u00dcnholy \ufffd')
        drop = by_kind["ground_drop"]
        self.assertEqual(drop["event_id"], f"fp-4242-{drop['sequence']}")
        self.assertEqual(drop["item"], {"name": 'Doom "Blade" \u2603', "rarity": "Unholy", "item_type": 3,
                                        "item_id": 412, "weapon_type": 7, "amount": 1})
        self.assertEqual(drop["room"], "Act_09_03")
        self.assertEqual(by_kind["session_delta"], {**{k: by_kind["session_delta"][k] for k in ENVELOPE},
                                                    "kind": "session_delta", "gold": 10, "xp": 20, "kills": 1,
                                                    "source": "forgepact_tracker"})
        self.assertEqual(by_kind["satanic_zone"]["buffs"], [6, 14])
        self.assertEqual(by_kind["satanic_zone"]["debuffs"], [3])
        self.assertIs(by_kind["vitals"]["satanic_here"], True)
        self.assertEqual(set(by_kind["heartbeat"]) - set(ENVELOPE), set())


if __name__ == "__main__":
    unittest.main()
