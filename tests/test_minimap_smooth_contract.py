"""Source contract for minimap smoothing, research stage (ForgePact issue #19).

What exists today is an instrument and a pure core, and nothing a player can
reach. These assertions keep it that way until the live Phase 0 round has
filled in docs/minimap-smoothing-research.md § Results:

- `mmprobe` is research-build only and not a player command;
- no `mmsmooth` command exists in the player build yet - a toggle that reports
  ON while nothing measured backs it is this plugin's most expensive bug class;
- the core header stays free of the runtime, so the behaviour harness compiles
  exactly what ships;
- the instrument covers every candidate from the static search, including the
  positive control, and refuses an address outside Hero_Siege.exe before it
  patches anything.

Round 1 (the instrument-blindness review) adds: event detours log `self` only,
a resolved pointer that is itself code is refused, `watch` skips failed reads
and can compare a ds_list's contents, `hold` writes marker records (never
instances) from the existing research tick, and the research doc runs every
hook-free measurement before the first hook and concludes H0/H2 only on
positive evidence.

Round 2 (the round-1 review) adds: `hold` calls `instance_exists` only on the
kinds `HhResolveInstance` hands it (a string or `undefined` there is a fatal GML
dialog, not a catchable exception) and refuses every other kind without any
runtime call; its first line prints what `typeof` said instead of claiming an
instance can never look like a struct; an abort after a write restores; and
`hold ... at <row> pre|post` performs the same write from inside a detoured
named-script row - the positive control on record, field and route that H2
and H0 now both require before a hold that was never displaced may count.

Split fix (after the round-2 review) adds: H2's (c') control is retargeted to
the mode-ON `at <R1 row> post` hold only - a mode-off displacement (step 8)
ties the record to the mode-off draw only, is recorded in R10, and never
satisfies (c') - and every other refresh-cadence container from step 3 must
also be held at step 7 (mode on) and excluded before H2's (c'') is met. H0
gains (i): the tester's own by-eye reading (V) can contradict it. An `at`
hold now pins the objMinimap instance at Start and stops cleanly - restore
attempted - when a zone change replaces that instance, instead of writing
into (and later "restoring") a record that was never held. The reentrancy
flag is cleared by a scope guard so an exception inside `MmProbeHoldAbort`
cannot leave it stuck.

Split fix, round 1 (after the split-fix review) supersedes that paragraph's
(c'') rule: excluding only step-3 candidates whose `watch` gap happened to
approximate the refresh cadence let an unmeasurable store pass silently (a
ds_grid/ds_map/buffer id never changes under `watch` without `list`, a struct
container is not writable by `hold`, and step 3 never enumerated globals).
(c'') is now a complete draw-side store ledger (R11) built from a fixed
enumeration - every `objMinimap` variable, every name-filtered `Player_obj`
variable and global, each with its `gjson` kind - whose entries are
`excluded (held)`, `excluded (scalar)` or `not excluded (<reason>)`; anything
this build cannot measure or write stays `not excluded`, never excluded by
default. H0 is tightened the same way: (g) counts only a displacement with
performance mode on (step 7 or 12), (h) requires both watch runs to have
actually measured a cadence (at least two changes, no read failures), and (i)
requires V to have been recorded (re-taken at step 2 with the mode read). `MmProbeHoldStart`'s `at` pin now
checks the resolved object before calling `instance_find`, and the hold's end
line says the draw "depends on" the record, field and route rather than
"reads" it - a `post`-original write also displaces if the game copies the
record after refilling it, so a displacement alone does not prove the draw
reads that container directly.

Split fix, round 2 (after the round-1 review) closes two more uncontrolled
inputs in the ledger itself, found by auditing every input a closing verdict
consumes rather than patching the two named spots. The enumeration
(`mmprobe vars`, `gnames`) had no positive control, so an empty or partial
print - `MmProbeVars`/`GlobalNames` print a count with no branch on zero or a
short list - read as a complete, empty ledger; each enumerating command is
now checked against a name already known on that object through a *second*
builtin (`oget`/`iget`/`gjson`/`cb instance_number`), and any control that
fails makes the whole ledger `not excluded (enumeration uncontrolled:
<command>)`. The `excluded (scalar)` status also let a handle through its
gap-1 branch (a front/back ds_list pair swapping ids every frame reads as a
changing scalar); the non-negative-integer test is now hoisted over both the
`0 changes` and the gap-1 branches, so any handle-shaped value, changed or
not, is `not excluded (possible handle)` instead. Consequence, stated in the
doc: this makes H2 unreachable through the ledger on this build in *every*
session, not only this one - `minimapShowMonsters` and every other 0/1 option
flag the enumeration carries reads as a possible handle, on top of the fog
grid the round-1 rule already blocked. `undefined`/`null` leave the
excludable kinds outright (an undefined read is a `watch` failure, a null
read is unmeasured on this runner), `unsure` is a by-eye reading that
satisfies neither "displaced" nor "never displaced", and the performance mode
a hold or watch ran in is now read back through `gjson <R8 var>` when R8 is
known, rather than trusted from the tester's memory. Three probe prints say
what failed instead of printing a bare count: `MmProbeVars` and `GlobalNames`
name themselves as uncontrolled on an empty return, and `MmProbeHoldStart`'s
`at` pin separates "unknown object" from "no instance to pin" from "not an
instance kind" instead of collapsing all three into one message.
"""
import re
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from test_release_hook_contract import function_body, strip_comments, strip_research_blocks  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"
HEADER = ROOT / "plugin" / "include" / "ForgePact" / "MinimapSmoothManager.hpp"
RESEARCH_DOC = ROOT / "docs" / "minimap-smoothing-research.md"

# The "named scripts" candidate list from the static search, as the short
# names HookOneScript would take. CheckPlayerInteraction is the positive control.
NAMED_SCRIPT_TARGETS = (
    "DrawMinimap", "DrawMinimapDynamic", "MinimapRefresh", "MinimapChangeSize", "MMStamp",
    "PlayerUpdateMinimap", "playerUpdateTimer", "playerUpdateTimerLife",
    "s_MinimapPoint", "s_MinimapLine", "outline_start_minimap",
    "ZoneStateParseMinimapDataSend", "ZoneStateParseMinimapDataReceive",
    "anon@2958@gml_Object_objMinimap_Create_0", "anon@6403@gml_Object_objMinimap_Create_0",
    "anon@7771@gml_Object_objMinimap_Create_0", "anon@8167@gml_Object_objMinimap_Create_0",
    "anon@11068@gml_Object_objMinimap_Create_0",
    "UiAOptionsVideoFPSOption", "UiAOptionsVideoFPSOptionSelected", "UiUpOptionsVideoFps",
    "UiAOptionsVideoVsync", "UiAOptionsVideo", "UiAOptionsGameplay",
    "CheckPlayerInteraction",
)


def player_commands(source: str) -> str:
    match = re.search(r"kPlayerCommands\s*=\s*\{([^}]*)\}", source)
    if not match:
        raise AssertionError("kPlayerCommands block not found")
    return match.group(1)


def macro_block(source: str, name: str) -> str:
    """The text of a multi-line `#define NAME(...)` up to its last continuation."""
    start = source.index(f"#define {name}(")
    lines = []
    for line in source[start:].split("\n"):
        lines.append(line)
        if not line.rstrip().endswith("\\"):
            break
    return "\n".join(lines)


class MinimapSmoothContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN.read_text(encoding="utf-8")
        cls.shipped = strip_research_blocks(cls.plugin)
        cls.header = HEADER.read_text(encoding="utf-8")

    def test_mmprobe_is_research_build_only(self):
        self.assertIn('lc == "mmprobe"', self.plugin)
        self.assertNotIn("mmprobe", self.shipped)
        self.assertNotIn('"mmprobe"', player_commands(self.plugin))

    def test_stage_a_ships_no_player_surface(self):
        # No mmsmooth command until the adapter exists against measured
        # R1-R10. Absent from the player command set AND from everything the
        # player build compiles.
        self.assertNotIn("mmsmooth", player_commands(self.plugin))
        self.assertNotIn("mmsmooth", self.shipped)

    def test_core_declares_its_tuning_constants(self):
        for constant in ("kMinSmoothGapFrames", "kMaxSmoothedMarkers",
                         "kDefaultPeriodFrames", "kMaxPeriodFrames"):
            self.assertRegex(self.header, rf"static constexpr \w+\s+{constant}\s*=", constant)

    def test_core_is_free_of_the_runtime(self):
        code = strip_comments(self.header)
        self.assertNotIn("g_Yytk", code)
        self.assertNotIn("CallBuiltin", code)
        self.assertNotIn("#include <YYToolkit", code)

    def test_probe_targets_every_static_search_candidate_and_the_control(self):
        table = macro_block(self.plugin, "MMPROBE_SCRIPTS")
        for name in NAMED_SCRIPT_TARGETS:
            self.assertIn(f'"{name}"', table, name)
        # ...and objMinimap's raw event names (no gml_Script_ prefix). They are
        # tried although session 7 measured raw object-event names as not
        # resolving at all; a row that does resolve is itself a result.
        events = macro_block(self.plugin, "MMPROBE_EVENTS")
        for event in ("Step_0", "Draw_0", "Draw_64", "Alarm_0", "Alarm_11", "Other_4"):
            self.assertIn(f"X({event})", events, event)
        self.assertIn('"gml_Object_objMinimap_" #EVENT', self.plugin)

    def test_probe_refuses_an_address_outside_the_game_before_patching(self):
        body = function_body(self.plugin, "static void MmProbeHook(")
        guard = body.index("AddrIsExecutableInModule(GetModuleHandleA(nullptr)")
        self.assertLess(guard, body.index("MmCreateHook"))
        # Native detour, not a (blind) table swap.
        self.assertNotIn("HookOneScriptTable(", body)
        self.assertNotIn("HookRawNamedRoutine(", body)

    def test_probe_names_its_positive_control_in_show(self):
        show = function_body(self.plugin, "static void MmProbeShow(")
        self.assertIn("positive control; 0 here voids every row above", show)

    def test_watch_sampler_runs_only_in_the_research_frame_block(self):
        frame = function_body(self.plugin, "void FrameCallback(")
        self.assertRegex(frame, r"#ifndef FORGEPACT_RELEASE[^#]*CiPokeKeyTick\(\);[^#]*MmProbeWatchTick\(\);[^#]*#endif")
        self.assertNotIn("MmProbeWatchTick", self.shipped)

    # ---- Round 1 corrections (instrument-blindness review of Stage A) ----

    def test_event_detours_log_self_only(self):
        # A YYC object event is called with (self, other) only, so argc and the
        # argument array are whatever the caller left in r9 / [rsp+28h]. The
        # verbose log must not read them; the forward passes every slot through
        # unchanged, which is safe for both callee shapes.
        macro = macro_block(self.plugin, "MMPROBE_EVENT_DETOUR")
        self.assertIn('MmProbeCount(g_MmProbe_Ev_##EVENT, "objMinimap_" #EVENT, S, 0, nullptr)', macro)
        start = macro.index("MmProbeCount(")
        call = macro[start:macro.index(");", start)]
        self.assertNotIn("argc", call)
        self.assertNotIn("A)", call)
        self.assertIn("orig(S, O, R, argc, A)", macro)

    def test_script_detours_still_forward_and_log_arguments(self):
        # Negative control: the event fix did not blanket-strip argument logging.
        macro = macro_block(self.plugin, "MMPROBE_SCRIPT_DETOUR")
        start = macro.index("MmProbeCount(")
        call = macro[start:macro.index(");", start) + 1]
        self.assertIn("S, argc, A)", call)
        self.assertIn("orig(S, O, R, argc, A)", macro)

    def test_probe_refuses_a_resolved_pointer_that_is_code(self):
        # A builtin name resolves to a function address, not a CScript; reading
        # m_Functions off it would dereference instruction bytes.
        body = function_body(self.plugin, "static void MmProbeHook(")
        guard = body.index("AddrIsExecutableInModule(mainMod, p)")
        self.assertLess(guard, body.index("reinterpret_cast<CScript*>(p)"))

    def test_event_rows_are_documented_as_expected_not_found(self):
        lines = self.plugin[:self.plugin.index("#define MMPROBE_EVENTS(")].split("\n")
        preceding = "\n".join(lines[-30:])
        self.assertIn("session 7", preceding)
        self.assertIn("not found", preceding)
        self.assertNotIn("resolved at runtime by", self.plugin)
        # The old comment in this file repeated the false claim.
        self.assertNotIn("which resolve " + "without", Path(__file__).read_text(encoding="utf-8"))

    def test_watch_skips_failed_reads(self):
        # A missing instance or an undefined read used to be stringified and
        # compared, so a zone change showed up as two change frames.
        body = function_body(self.plugin, "static void MmProbeWatchTick(")
        self.assertIn("VALUE_UNDEFINED", body)
        self.assertIn("if (!readOk)", body)
        self.assertLess(body.index("if (!readOk)"), body.index("text != w.last"))

    def test_watch_has_a_list_mode(self):
        # A ds_list container is a real; comparing the id says nothing about
        # its contents.
        body = function_body(self.plugin, "static void MmProbeWatchTick(")
        self.assertIn("ds_exists", body)
        self.assertIn("ds_list_find_value", body)

    def test_hold_writes_records_not_instances(self):
        # Round 2: the decision and the write live in MmProbeHoldApply, which
        # both the frame tick and the `at` detour call.
        body = function_body(self.plugin, "static void MmProbeHoldApply(")
        guard = body.index("instance_exists")
        self.assertLess(guard, body.index("variable_struct_set"))
        self.assertLess(guard, body.index("array_set"))
        self.assertIn("readback", body)

    def test_hold_rides_the_existing_research_tick(self):
        self.assertIn("MmProbeHoldTick()", function_body(self.plugin, "static void MmProbeWatchTick("))
        self.assertNotIn("MmProbeHold", function_body(self.plugin, "void FrameCallback("))
        self.assertNotIn("MmProbeHold", self.shipped)

    def test_probe_header_no_longer_claims_read_only(self):
        self.assertNotIn("Nothing here writes game state", self.plugin)
        self.assertIn("hold", function_body(self.plugin, "static void MmProbeCommand("))

    # ---- Round 2 corrections (instrument-blindness review of round 1) ----

    def test_hold_gates_instance_exists_by_kind(self):
        # instance_exists wants a number or a reference. A freed marker slot is
        # commonly `undefined`, and a GML type error inside a builtin is the
        # runner's fatal dialog - so the kind set HhResolveInstance uses is
        # checked first, and everything else is refused with no call at all.
        body = function_body(self.plugin, "static void MmProbeHoldApply(")
        self.assertLess(body.index("VALUE_REF"), body.index("instance_exists"))
        self.assertIn('"method"', body)
        self.assertIn("refused without any runtime call", body)
        self.assertNotIn("instance_exists", function_body(self.plugin, "static void MmProbeHoldTick("))

    def test_hold_prints_typeof_and_does_not_overclaim(self):
        # Whether this runner reports an instance-backed object as "struct" is
        # unmeasured; the hold prints the answer rather than asserting it.
        body = function_body(self.plugin, "static void MmProbeHoldApply(")
        self.assertIn("typeof=", body)
        self.assertIn("at=", body)
        self.assertNotIn("it never skips the guard", self.plugin)
        doc = RESEARCH_DOC.read_text(encoding="utf-8")
        self.assertNotIn("never skips the guard", doc)
        self.assertIn("unmeasured for instance-backed objects", " ".join(doc.split()))

    def test_hold_abort_restores_after_a_write(self):
        # An abort reached after writes (a record replaced mid-hold, a throw)
        # used to leave the marker displaced.
        self.assertIn("static std::string MmProbeHoldRestore(", self.plugin)
        abort = function_body(self.plugin, "static void MmProbeHoldAbort(")
        self.assertIn("MmProbeHoldRestore(", abort)
        self.assertIn("MmProbeHoldRestore(", function_body(self.plugin, "static void MmProbeHoldFinish("))
        self.assertIn("restored=", abort)

    def test_hold_can_write_inside_a_detour(self):
        # The frame-end write is clobbered by a refill that runs before the next
        # draw, so it cannot be the positive control. The same write made
        # post-original inside the refill row can. Only script rows are write
        # points; the frame tick only counts down while one is armed.
        macro = macro_block(self.plugin, "MMPROBE_SCRIPT_DETOUR")
        self.assertIn("g_MmProbeHoldAtSlot", macro)
        self.assertIn("g_MmProbeHoldAtPost", macro)
        self.assertEqual(macro.count("MmProbeHoldApply()"), 2)
        self.assertIn("orig(S, O, R, argc, A)", macro)
        self.assertNotIn("MmProbeHoldApply", macro_block(self.plugin, "MMPROBE_EVENT_DETOUR"))
        self.assertIn("g_MmProbeHoldInApply", function_body(self.plugin, "static void MmProbeHoldApply("))
        tick = function_body(self.plugin, "static void MmProbeHoldTick(")
        self.assertIn("g_MmProbeHoldAtSlot", tick)
        self.assertIn("MmProbeHoldApply()", tick)
        start = function_body(self.plugin, "static void MmProbeHoldStart(")
        self.assertIn("hook it first", start)
        self.assertIn("fp_mmprobe_ev_", start)
        self.assertIn("g_MmProbeHoldAtSlot = nullptr",
                      function_body(self.plugin, "static std::string MmProbeHoldRestore("))
        self.assertNotIn("MmProbeHoldApply", self.shipped)

    # ---- Split fix (round-2 review of round 2) ----

    def test_hold_at_stops_when_the_minimap_instance_changes(self):
        # An `at` hold armed on one objMinimap instance used to keep writing
        # after a room change into a different instance's record, and Restore
        # would "restore" into that new record too.
        changed = function_body(self.plugin, "static bool MmProbeHoldInstanceChanged(")
        for needle in ("instance_find", "InstanceIdOf(", "minimap instance changed"):
            self.assertIn(needle, changed, needle)
        start = function_body(self.plugin, "static void MmProbeHoldStart(")
        self.assertIn("InstanceIdOf(", start)
        self.assertIn("VALUE_REF", start)
        apply_body = function_body(self.plugin, "static void MmProbeHoldApply(")
        self.assertLess(apply_body.index("MmProbeHoldInstanceChanged("), apply_body.index("MmProbeHoldElement("))
        self.assertIn("MmProbeHoldAbort(why)", apply_body)
        element = function_body(self.plugin, "static bool MmProbeHoldElement(")
        self.assertIn("h.instance", element)
        self.assertIn("instance_exists", element)
        self.assertNotIn("instanceId", function_body(self.plugin, "static bool MmProbeReadVar("))
        self.assertNotIn("MmProbeHoldInstanceChanged", self.shipped)

    def test_hold_reentrancy_guard_is_cleared_by_a_scope_guard(self):
        # If MmProbeHoldAbort threw inside the old catch(...), the standalone
        # `= false;` clear after it was skipped and every later hold write
        # would return at the guard forever.
        body = function_body(self.plugin, "static void MmProbeHoldApply(")
        self.assertRegex(body, r"~InApplyScope\(\)\s*\{\s*g_MmProbeHoldInApply = false;")
        self.assertEqual(body.count("g_MmProbeHoldInApply = false"), 1)
        self.assertEqual(body.count("g_MmProbeHoldInApply = true"), 1)
        self.assertLess(body.index("InApplyScope inApplyScope;"), body.index("apply();"))

    # ---- Round 1 (split-fix review of the split fix) ----

    def test_hold_start_checks_the_object_before_instance_find(self):
        # A mistyped object name used to hand -1 to instance_find before the
        # oi < 0 check ran (MmProbeReadVar already tested first).
        start = function_body(self.plugin, "static void MmProbeHoldStart(")
        self.assertLess(start.index("oi.ToDouble() < 0"), start.index('"instance_find"'))

    def test_hold_says_depends_on_not_reads(self):
        # A displaced marker shows the draw depends on the record, field and
        # route - not that it reads that container directly, since a copy made
        # after the refill row returns would displace too.
        self.assertNotIn("the draw reads this record, field and route", self.plugin)
        self.assertNotIn("whether the draw reads the container", self.plugin)
        finish = function_body(self.plugin, "static void MmProbeHoldFinish(")
        self.assertIn(
            "in the performance mode set during this hold, depends on this record, field and route",
            finish)
        self.assertIn("for H2 only at <refill row> post with performance mode on counts", finish)
        doc = RESEARCH_DOC.read_text(encoding="utf-8")
        collapsed = " ".join(doc.split())
        self.assertNotIn("means the draw reads exactly this record", collapsed)
        self.assertIn(
            "depends on this record, field and route (not that it reads this container directly",
            collapsed)

    # ---- Round 2 (split-fix review of round 1) ----

    def test_probe_prints_say_what_failed(self):
        # An empty or partial enumeration print must not read as a complete
        # ledger, and a mistyped object name must not be confused with a
        # missing instance.
        vars_body = function_body(self.plugin, "static void MmProbeVars(")
        self.assertIn("uncontrolled", vars_body)
        self.assertIn("built-in", vars_body)
        globals_body = function_body(self.plugin, "static void GlobalNames(")
        self.assertIn("uncontrolled", globals_body)
        start = function_body(self.plugin, "static void MmProbeHoldStart(")
        self.assertLess(start.index("unknown object "), start.index('"instance_find"'))
        self.assertLess(start.index('"instance_find"'), start.index(" instance to pin"))
        self.assertIn("not an instance kind", start)
        self.assertNotIn("MmProbeVars", self.shipped)


class MinimapSmoothResearchDocTests(unittest.TestCase):
    """The live procedure may only close the issue on positive evidence."""

    @classmethod
    def setUpClass(cls):
        cls.doc = RESEARCH_DOC.read_text(encoding="utf-8")
        cls.collapsed = " ".join(cls.doc.split())

    def heading(self, heading: str) -> int:
        # The heading line itself: the procedure quotes "## Deciding the
        # hypothesis" in running text, which must not count as the section.
        match = re.search(rf"(?m)^{re.escape(heading)}\s*$", self.doc)
        if not match:
            raise AssertionError(f"heading {heading!r} not found")
        return match.start()

    def section(self, heading: str) -> str:
        start = self.heading(heading)
        end = self.doc.find("\n## ", start + len(heading))
        return self.doc[start:] if end < 0 else self.doc[start:end]

    def test_research_doc_orders_hook_free_evidence_first(self):
        procedure = self.section("## Live procedure")
        first_hook = procedure.index("mmprobe hook")
        for hook_free in ("mmprobe vars objMinimap", "mmprobe watch", "mmprobe hold"):
            self.assertLess(procedure.index(hook_free), first_hook, hook_free)
        self.assertNotIn("If R6 equals R7", procedure)
        self.assertIn("minimapShowMonsters", procedure)
        self.assertIn("Player_obj x", procedure)

    def test_research_doc_requires_positive_evidence(self):
        deciding = self.heading("## Deciding the hypothesis")
        self.assertLess(self.heading("## Live procedure"), deciding)
        self.assertLess(deciding, self.heading("## Results"))
        section = self.section("## Deciding the hypothesis")
        for needle in ("surface_exists", "mmprobe hold", "not observed", "PLAN-DEFECT",
                       "### If nothing is observed", "AddrIsExecutableInModule"):
            self.assertIn(needle, section, needle)
        self.assertIn(
            "A script row's gap is never sufficient on its own for H2, and "
            "`mmprobe show` alone is never sufficient for H0.",
            " ".join(section.split()))

    def test_research_doc_records_session_7(self):
        self.assertNotIn("resolves at runtime under the raw name", self.collapsed)
        self.assertIn("session 7", self.collapsed)
        self.assertIn("not observed among scalar globals", self.collapsed)
        self.assertRegex(self.section("## Results"), r"(?m)^\| H \|")

    def test_research_doc_h2_and_h0_need_a_displacement_control(self):
        # A hold that was never displaced is guaranteed under every hypothesis
        # when the held field is not the drawn one. The verdicts that close the
        # issue need the same record, field and route seen to displace.
        section = self.section("## Deciding the hypothesis")
        h2 = re.search(r"(?m)^\| \*\*H2\*\*.*$", section)
        h0 = re.search(r"(?m)^\| \*\*H0\*\*.*$", section)
        self.assertIsNotNone(h2, "H2 row")
        self.assertIsNotNone(h0, "H0 row")
        for needle in ("performance mode off", "at <R1 row> post", "supporting only"):
            self.assertIn(needle, h2.group(0), needle)
        self.assertIn("R10", h0.group(0))
        collapsed = " ".join(section.split())
        self.assertIn(
            "H2 and H0 both require the same record, field and route to have displaced the marker "
            "through some write point in the same session; a hold that was never displaced through "
            "any write point proves nothing about the draw.",
            collapsed)
        self.assertIn("no displacement through any write point", collapsed)
        procedure = self.section("## Live procedure")
        self.assertGreater(procedure.index("at <R1 row> post"), procedure.index("mmprobe hook"))
        self.assertIn("by eye", procedure)

        # Split fix: a mode-off displacement ties the record to the mode-off
        # draw only, which is not H2's control; every other refresh-cadence
        # container must also be held and excluded.
        for needle in ("(c'')", "does not satisfy (c')"):
            self.assertIn(needle, h2.group(0), needle)
        self.assertNotIn("displaced by at least one of", h2.group(0))
        self.assertIn(
            "H2's (c') is met only by the `at <R1 row> post` hold with performance mode on (step 12); "
            "a displacement by the frame-tick hold with performance mode off (step 8) ties the record "
            "only to the mode-off draw, is recorded in R10, and never satisfies (c').",
            collapsed)
        self.assertIn("the draw depends on this record, field and route", collapsed)
        self.assertIn("excluded only by H2's (c'') rule", collapsed)
        self.assertNotIn("if and only if", self.collapsed)
        self.assertIn("never satisfies H2's (c')", procedure)
        self.assertNotIn("Displaced here is a positive control for step 7's field", procedure)
        hypotheses = self.section("## Hypotheses Phase 0 decides between")
        h2_hyp = re.search(r"(?m)^\| \*\*H2\*\*.*$", hypotheses)
        h0_hyp = re.search(r"(?m)^\| \*\*H0\*\*.*$", hypotheses)
        self.assertIsNotNone(h2_hyp, "H2 row (Hypotheses section)")
        self.assertIsNotNone(h0_hyp, "H0 row (Hypotheses section)")
        self.assertEqual(h2_hyp.group(0), h2.group(0))
        self.assertEqual(h0_hyp.group(0), h0.group(0))

    def test_research_doc_h0_is_contradicted_by_v(self):
        # A container refreshed every frame in both modes does not exclude a
        # surface redraw that performance mode throttles; the tester's own
        # by-eye reading (V) is the only record of that.
        section = self.section("## Deciding the hypothesis")
        h0 = re.search(r"(?m)^\| \*\*H0\*\*.*$", section)
        self.assertIsNotNone(h0, "H0 row")
        for needle in ("(i)", "R10", "not observed (V contradicts H0)", "ties R3 only to the mode-off draw"):
            self.assertIn(needle, h0.group(0), needle)
        results = self.section("## Results")
        h_row = re.search(r"(?m)^\| H \|.*$", results)
        self.assertIsNotNone(h_row, "H row")
        self.assertIn("(a)–(i)", h_row.group(0))
        self.assertRegex(results, r"(?m)^\| V \|")

    def test_research_doc_h2_exclusion_fails_closed(self):
        # (c'') is rebuilt as a fail-closed ledger: every entry is `not
        # excluded` until a positive measurement excludes it, the enumeration
        # is fixed, and a concluded H2 quotes its limit.
        section = self.section("## Deciding the hypothesis")
        h2 = re.search(r"(?m)^\| \*\*H2\*\*.*$", section)
        self.assertIsNotNone(h2, "H2 row")
        for needle in (
            "draw-side store ledger",
            "never excluded by default",
            "not observed (draw-side store not excluded: <variable> - <reason>)",
            "exclusion covers only objMinimap's variables and the Player_obj and "
            "global variables whose names contain minimap, marker, radar, icon or blip",
        ):
            self.assertIn(needle, h2.group(0), needle)
        self.assertNotIn("another container at the refresh cadence", h2.group(0))
        self.assertNotIn("whose step-5", h2.group(0))

        heading_match = re.search(r"(?m)^### The draw-side store ledger \(H2's \(c''\)\)\s*$", section)
        self.assertIsNotNone(heading_match, "ledger subsection heading")
        if_nothing = section.index("### If nothing is observed")
        self.assertLess(heading_match.start(), if_nothing)
        ledger = " ".join(section[heading_match.start():if_nothing].split())
        for literal in (
            "`excluded (held)`",
            "`excluded (scalar)`",
            "`not excluded (<reason>)`",
            "`gnames blip`",
            "`gjson <name>`",
            "minimapDiscoveredGrid",
            "non-negative integer",
            "no `overwritten` gap below 4",
            "`readFailures=0`",
            "`stuck` > 0",
            "Nothing else is enumerated",
            "The ledger may stop at the first `not excluded` entry.",
            "H2 to be `not observed` in this session",
        ):
            self.assertIn(literal, ledger, literal)

        collapsed_section = " ".join(section.split())
        self.assertIn(
            "H2's (c'') is met only by a complete draw-side store ledger in which every entry "
            "other than R3 is excluded by a positive measurement; an entry that no instrument in "
            "this build can measure or write is `not excluded`, and so is an entry that was never "
            "enumerated or never run. Otherwise H2 = `not observed (draw-side store not excluded: "
            "<variable> - <reason>)`.",
            collapsed_section)

        self.assertNotIn("another container at the refresh cadence", self.collapsed)
        self.assertNotIn("whose step-5 `watch` gap ≈ the refresh gap", self.collapsed)

        procedure = self.section("## Live procedure")
        for needle in (
            "stopping at the first `not excluded`",
            "never skipped",
            "gnames blip",
            "never excluded by default",
        ):
            self.assertIn(needle, procedure, needle)
        self.assertLess(procedure.index("draw-side store ledger"), procedure.index("mmprobe hook"))

        results = self.section("## Results")
        r11 = re.search(r"(?m)^\| R11 \|.*$", results)
        self.assertIsNotNone(r11, "R11 row")
        self.assertIn("not excluded (<reason>)", r11.group(0))
        r10 = re.search(r"(?m)^\| R10 \|.*$", results)
        self.assertIsNotNone(r10, "R10 row")
        self.assertNotIn("not held", r10.group(0))

    def test_research_doc_h0_fails_closed(self):
        # H0's three holes, same shape as H2's: a mode-off displacement, an
        # unmeasured cadence and an unrecorded V must not be able to meet it.
        section = self.section("## Deciding the hypothesis")
        h0 = re.search(r"(?m)^\| \*\*H0\*\*.*$", section)
        self.assertIsNotNone(h0, "H0 row")
        for needle in (
            "does not satisfy (g)",
            "not observed (V not recorded)",
            "at least two changes",
            "read failures=0",
            "not observed (watch: no measurable cadence on <variable>)",
            "by the frame-tick hold (step 7) or by `hold … at <R1 row> post` (step 12)",
        ):
            self.assertIn(needle, h0.group(0), needle)
        self.assertNotIn("step 7, 8 or 12", self.collapsed)
        self.assertNotIn("counts for H0's (g)", self.collapsed)
        procedure = self.section("## Live procedure")
        self.assertIn("does not satisfy H0's (g)", procedure)
        self.assertIn("H0 cannot be concluded while either is missing", procedure)

    def test_research_doc_ledger_enumeration_is_controlled(self):
        # An empty or partial enumeration print (MmProbeVars/GlobalNames print
        # a count with no branch on zero or a short list) must not read as a
        # complete ledger; each enumerating command needs its own positive
        # control through a second builtin.
        heading_match = re.search(r"(?m)^### The draw-side store ledger \(H2's \(c''\)\)\s*$", self.doc)
        self.assertIsNotNone(heading_match, "ledger subsection heading")
        if_nothing = self.doc.index("### If nothing is observed", heading_match.start())
        ledger = " ".join(self.doc[heading_match.start():if_nothing].split())
        for literal in (
            "The enumeration is controlled, or it is nothing.",
            "`not excluded (enumeration uncontrolled: <command>)`",
            "minimapCellsX",
            "`iget equippedItems`",
            "`gjson minimapShowMonsters`",
            "cb instance_number",
            "not excluded (unreadable)",
            "a built-in such as `x` is never listed",
        ):
            self.assertIn(literal, ledger, literal)

        procedure = self.section("## Live procedure")
        for needle in (
            "Enumeration control",
            "oget objMinimap minimapDiscoveredGrid",
            "iget inventory",
            "not excluded (enumeration uncontrolled: <command>)",
        ):
            self.assertIn(needle, procedure, needle)
        self.assertLess(procedure.index("Enumeration control"), procedure.index("mmprobe hook"))

        results = self.section("## Results")
        r11 = re.search(r"(?m)^\| R11 \|.*$", results)
        self.assertIsNotNone(r11, "R11 row")
        self.assertIn("enumeration uncontrolled", r11.group(0))

        instrument = self.section("## Instrument")
        self.assertIn("never listed", instrument)

    def test_research_doc_scalar_exclusion_refuses_a_possible_handle(self):
        # The gap-1 branch of `excluded (scalar)` used to accept a handle (a
        # front/back ds_list pair swapping ids every frame reads as a
        # changing scalar); the integer test is hoisted over both branches.
        heading_match = re.search(r"(?m)^### The draw-side store ledger \(H2's \(c''\)\)\s*$", self.doc)
        if_nothing = self.doc.index("### If nothing is observed", heading_match.start())
        ledger = " ".join(self.doc[heading_match.start():if_nothing].split())
        for literal in (
            "neither the value `vars`/`gjson` printed nor the watch's `first:` nor its `last:` is a non-negative integer",
            "not excluded (possible handle)",
            "`undefined` and `null` are not excludable",
            "It will in every session on this build",
            "`unsure` is not `never displaced`",
        ):
            self.assertIn(literal, ledger, literal)
        self.assertNotIn("or it printed `0 changes` and its value is not a non-negative integer", ledger)

        section = self.section("## Deciding the hypothesis")
        h0 = re.search(r"(?m)^\| \*\*H0\*\*.*$", section)
        self.assertIsNotNone(h0, "H0 row")
        for needle in ("first 64 elements only", "player record beyond the compared prefix", "`gjson <R8 var>`",
                       "not observed (watch: mode not read before <on|off> run)",
                       "each look preceded by `gjson <R8 var>` showing that look's mode",
                       "not observed (V: mode not read before <on|off> look)",
                       "a setting the game applies later than the toggle is not excluded"):
            self.assertIn(needle, h0.group(0), needle)

        procedure = self.section("## Live procedure")
        self.assertIn("not excluded (possible handle)", procedure)
        self.assertIn("`gjson <R8 var>`", procedure)
        # The mode each watch ran in is a reading (audit row A13): step 5 reads
        # the on value and step 6 the off value before its watch.
        step5 = procedure.index("5. **Container cadence, performance mode ON")
        step6 = procedure.index("6. **Container cadence, performance mode OFF")
        step7 = procedure.index("7. **Is the container live")
        for start, end, value in ((step5, step6, "R8's on value"), (step6, step7, "R8's off value")):
            body = procedure[start:end]
            self.assertIn(value, body)
            self.assertLess(body.index("`gjson <R8 var>`"), body.index("`watch`" if start == step6 else "`mmprobe watch"))
        self.assertIn("`gjson <R8 var>` before `mmprobe watch` in step 6", procedure)
        self.assertIn("*V, with the mode read:*", procedure)
        self.assertIn("which must show that look's mode", procedure)

        self.assertIn("an `int32:` or `int64:` with no minus sign, or a `real:` with no minus sign", ledger)
        h2 = re.search(r"(?m)^\| \*\*H2\*\*.*$", section)
        self.assertIn("a partial print that still contains that name is not excluded", h2.group(0))

        results = self.section("## Results")
        r10 = re.search(r"(?m)^\| R10 \|.*$", results)
        self.assertIsNotNone(r10, "R10 row")
        self.assertIn("unsure", r10.group(0))
        self.assertNotIn("not held", r10.group(0))

        hypotheses = self.section("## Hypotheses Phase 0 decides between")
        h0_hyp = re.search(r"(?m)^\| \*\*H0\*\*.*$", hypotheses)
        self.assertIsNotNone(h0_hyp, "H0 row (Hypotheses section)")
        self.assertEqual(h0_hyp.group(0), h0.group(0))

    def test_research_doc_does_not_overstate_session_7(self):
        # Session 7 measured 22 event names on three other objects; objMinimap's
        # own rows are measured at step 9.
        self.assertNotIn("objMinimap event code is unreachable by name", self.collapsed)
        results = self.section("## Results")
        self.assertIn("consistent with session 7", " ".join(results.split()))
        self.assertRegex(results, r"(?m)^\| V \|")


if __name__ == "__main__":
    unittest.main()
