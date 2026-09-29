#!/usr/bin/env python3
"""Contract tests for ForgePact Relic Drop Pool Filter Mod & Mods Tab."""

import re
import sys
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
FORGEPACT_DIR = REPO_ROOT / "ForgePact"
SRC_DIR = FORGEPACT_DIR / "src"
PLUGIN_SRC = FORGEPACT_DIR / "plugin" / "ModuleMain.cpp"
FORGEPACT_INCLUDE_DIR = FORGEPACT_DIR / "plugin" / "include" / "ForgePact"

if str(SRC_DIR) not in sys.path:
    sys.path.insert(0, str(SRC_DIR))

import forgepact

sys.path.insert(0, str(Path(__file__).resolve().parent))
from panel_source import panel_file, panel_source  # noqa: E402
from test_release_hook_contract import strip_comments, strip_research_blocks  # noqa: E402

# The page (markup and script) is the Svelte project in panel/src.
PANEL_PAGE = panel_source()


def body(source, signature):
    """The whole of `signature`'s definition, brace-matched.

    The fixed-length windows elsewhere in this file are fine for short bodies,
    but a function that grows a paragraph of comment silently slides its own
    code out of the window and the assertion starts measuring the comment.
    Same helper the two behaviour-harness runners use - including their
    `rfind`, which takes the LAST occurrence. A forward declaration would make
    `index` return the declaration's span instead, and the assertions built on
    this include negative ones (`assertNotIn`), which a wrong span satisfies
    vacuously rather than failing.
    """
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


class TestRelicFilterContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin_code = PLUGIN_SRC.read_text(encoding="utf-8")
        cls.panel_code = (SRC_DIR / "forgepact.py").read_text(encoding="utf-8")
        cls.relic_filter_header = (FORGEPACT_INCLUDE_DIR / "RelicFilterMod.hpp").read_text(encoding="utf-8")

    def test_defaults_has_relic_filter(self):
        self.assertIn("mod_filter_max_relics", forgepact.DEFAULTS)
        self.assertFalse(forgepact.DEFAULTS["mod_filter_max_relics"])

    def test_build_cmds_emits_relicfilter_when_enabled(self):
        cfg = dict(forgepact.DEFAULTS)
        cfg["mod_filter_max_relics"] = True
        cmds = forgepact.build_cmds(cfg)
        self.assertIn("relicfilter 1", cmds)

    def test_build_cmds_omits_relicfilter_when_disabled(self):
        cfg = dict(forgepact.DEFAULTS)
        cfg["mod_filter_max_relics"] = False
        cmds = forgepact.build_cmds(cfg)
        self.assertNotIn("relicfilter 1", cmds)

    def test_html_contains_mods_tab_and_control(self):
        self.assertIn('data-tab="mods"', PANEL_PAGE)
        self.assertIn('id="mod_filter_max_relics"', PANEL_PAGE)
        self.assertIn("Remove owned relics from drop pool", PANEL_PAGE)
        self.assertIn("10 out of 10", PANEL_PAGE)

    def test_panel_imports_hs_game_sdk(self):
        self.assertIn("from hs_game_sdk import", self.panel_code)

    def test_plugin_includes_hs_game_sdk(self):
        self.assertIn("#include <hs_game_sdk/hs_game_sdk.hpp>", self.plugin_code)

    def test_plugin_handles_relicfilter_command(self):
        self.assertIn('lc == "relicfilter"', self.plugin_code)
        # Enabled-state moved into ForgePact::RelicFilterMod (2026-09 class split).
        self.assertIn("std::atomic<bool> m_Enabled{ false };", self.relic_filter_header)
        self.assertIn("RelicFilterMod::Instance().SetEnabled(", self.plugin_code)

    def test_plugin_implements_relic_filtering(self):
        # #125: the lever is GetRelicQuest, the pick's own draw-again check.
        self.assertIn("static RValue& Hook_GetRelicQuest(", self.plugin_code)
        self.assertIn("MaxedForFrame(", self.plugin_code)
        self.assertIn("GetPlayerMaxedRelics", self.relic_filter_header)

    def test_player_resolution_converts_numeric_ids_to_instances(self):
        # Superseded upstream (origin v1.3.16, the Headhunter dispatch fix):
        # HhSteal's fallback used to convert HhResolveLocalPlayer's result with
        # `p.ToInstance()`, which only handles VALUE_OBJECT and silently drops a
        # VALUE_REF - the same class of bug this project's own fix (VALUE_REF
        # support in HhUsableInstance/HhResolveLocalPlayer) addressed elsewhere.
        # HhResolveInstance() replaces it: it accepts VALUE_REF/VALUE_OBJECT/the
        # numeric kinds and verifies the resolved instance is the one asked for.
        self.assertIn("static CInstance* HhResolveInstance(", self.plugin_code)
        self.assertIn("if (HhResolveLocalPlayer(p, nullptr)) player = HhResolveInstance(p);", self.plugin_code)
        # Find the definition, not merely the first mention: the pet quest
        # collector forward-declares this so it can call it from a tick
        # defined earlier in the file, and a forward declaration has no body
        # to inspect.
        resolver = ""
        for part in self.plugin_code.split("static CInstance* HhResolveInstance(")[1:]:
            if part.lstrip().startswith("const RValue& value)\n{"):
                resolver = part[:1200]
                break
        self.assertTrue(resolver, "no definition of HhResolveInstance found (only declarations)")
        self.assertIn("VALUE_REF", resolver)
        self.assertNotIn('GetInstanceObject((int32_t)p.ToDouble(), player)', self.plugin_code)

    def test_release_build_requires_fresh_staged_plugin(self):
        build_code = (FORGEPACT_DIR / "build_release.py").read_text(encoding="utf-8")
        self.assertIn('if not ship.is_file():', build_code)
        self.assertIn('ship.read_bytes() != packaged.read_bytes()', build_code)

    def test_orb_pickup_mod_emits_ten_x_command(self):
        cfg = dict(forgepact.DEFAULTS)
        cfg["mod_orb_pickup_radius"] = True
        self.assertIn("orbpickup 10", forgepact.build_cmds(cfg))
        self.assertIn('id="mod_orb_pickup_radius"', PANEL_PAGE)

    def test_release_setup_is_delayed_past_character_selection(self):
        self.assertIn("if (!g_Setup && fc > 300)", self.plugin_code)

    def test_relic_filter_hook_is_deferred_until_a_player_exists(self):
        # Sending relicfilter must only ARM the mod; hooking DropRelic while the
        # character screen runs stalls the runner for about a minute.
        # Migrated into ForgePact::RelicFilterMod (2026-09 class split): the
        # pending flag is now m_Pending, exposed via IsPending()/ClearPending().
        self.assertIn("m_Pending", self.relic_filter_header)
        armed = body(self.plugin_code, 'if (lc == "relicfilter")')
        self.assertNotIn("HookOneScript(", armed)
        self.assertIn("RelicFilterMod::Instance().SetEnabled(enable, g_Orig_GetRelicQuest != nullptr);", armed)
        # ...and the frame callback installs it once a player resolves.
        self.assertIn(
            "if (ForgePact::RelicFilterMod::Instance().IsPending() && g_Setup",
            self.plugin_code,
        )
        self.assertIn("RelicFilterMod::Instance().ClearPending();", self.plugin_code)

    def test_relicfilter_has_exactly_one_command_branch(self):
        # A second, unreachable branch used to install a different hook set.
        self.assertEqual(self.plugin_code.count('lc == "relicfilter"'), 1)

    def test_orb_pickup_is_driven_from_the_frame_callback(self):
        # MEASURED twice: the globes are reached neither through
        # distance_to_object (0 matches) nor by hooking their step scripts
        # (installed, ran 0 times).  The pull is therefore driven from the frame
        # callback, which is known to run, by enumerating globe instances.
        self.assertIn("static void OrbPickupTick(uint32_t frame)", self.plugin_code)
        self.assertIn("OrbPickupTick(fc);", self.plugin_code)
        # Since 1.3.20 the enumeration is throttled to one scan every
        # kOrbScanFrames frames while the pull still runs every frame, so the
        # instance walk lives in OrbScan and the tick drives it.
        scan = body(self.plugin_code, "static void OrbScan()")
        self.assertIn("instance_number", scan)
        self.assertIn("instance_find", scan)
        tick = body(self.plugin_code, "static void OrbPickupTick(uint32_t frame)")
        self.assertIn("OrbScan();", tick)
        self.assertIn("PullOneGlobe", tick)

    def test_orb_pickup_no_longer_relies_on_interception(self):
        # Both dead interception points must be gone, not left behind as noise.
        self.assertNotIn("Hook_ExpGlobeStep", self.plugin_code)
        self.assertNotIn('HookBuiltin("distance_to_object", "fp_orb', self.plugin_code)

    def test_orb_pickup_tick_is_bounded(self):
        # A runaway globe count must not be able to cost a frame.
        self.assertIn("budget", body(self.plugin_code, "static void OrbScan()"))

    def test_orb_pickup_reads_player_position_once_per_frame(self):
        # Not once per globe per step.
        self.assertIn("g_PlayerPosValid", self.plugin_code)
        self.assertNotIn("HhResolveLocalPlayer", body(self.plugin_code, "static void PullOneGlobe("))

    def test_orb_pickup_counts_why_it_did_nothing(self):
        # "pulled 0 globes" has several different causes; each is counted so one
        # more session distinguishes them without another guess.
        for counter in ("g_OrbGlobesSeen", "g_OrbNoPlayer", "g_OrbOutOfReach", "g_OrbNearestPx"):
            self.assertIn(counter, self.plugin_code)
        self.assertIn("static void OrbPickupStats()", self.plugin_code)
        # Turning the mod off must report them, not just the pulled count.
        self.assertIn("OrbPickupStats();", self.plugin_code)

    def test_player_resolution_accepts_instance_references(self):
        # MEASURED 2026-09-10: instance_find returns VALUE_REF (kind 15) on this
        # runner, not a number.  Accepting only the numeric kinds made the
        # fallback always fail, which silently disabled every feature gated on
        # the local player (orbpickup logged seen=176993, noplayer=176993).
        self.assertIn("VALUE_REF", self.plugin_code)
        resolve = self.plugin_code.split("static bool HhResolveLocalPlayer(RValue& out, std::string* how)")[-1][:2500]
        self.assertIn("HhUsableInstance(inst)", resolve)
        self.assertIn("HhUsableInstance(p)", resolve)

    def test_usable_instance_is_proved_by_reading_a_variable(self):
        # A kind tag alone is not proof; the probe does what the callers do.
        probe = self.plugin_code.split("static bool HhUsableInstance", 1)[1][:600]
        self.assertIn("variable_instance_get", probe)

    def test_orb_stat_reports_how_the_player_resolved(self):
        self.assertIn("g_OrbPlayerHow", self.plugin_code)
        self.assertIn("player via %s", self.plugin_code)

    def test_stall_watchdog_records_io_and_memory(self):
        # Where the thread is blocked says nothing about why; disk and free RAM
        # across the stall separate machine-wide thrashing from a GPU wait.
        self.assertIn("GetProcessIoCounters", self.plugin_code)
        self.assertIn("GlobalMemoryStatusEx", self.plugin_code)
        self.assertIn("free RAM", self.plugin_code)

    def test_stall_watchdog_is_present_in_the_research_build(self):
        # Names the culprit when frames stop arriving. The player build does
        # not carry it - see test_release_hook_contract.py's
        # test_stall_watchdog_never_reaches_the_player_build.
        self.assertIn("static void StallWatchdogLoop()", self.plugin_code)
        self.assertIn("StartStallWatchdog();", self.plugin_code)
        watchdog = self.plugin_code.split("static void StallWatchdogLoop()", 1)[1]
        watchdog = watchdog[:watchdog.index("static void StartStallWatchdog()")]
        # The frame thread must be resumed before anything that can allocate,
        # or the watchdog deadlocks on a lock the stalled thread is holding.
        self.assertLess(watchdog.index("ResumeThread"), watchdog.index("ms - frame thread at"))

    @staticmethod
    def _tab_of(html, control_id_attr):
        # Which tab-card a control lives in: the nearest data-tab="..." that
        # precedes it. Each control id must appear exactly once in the HTML
        # for this to be unambiguous.
        pos = html.index(control_id_attr)
        tab_start = html.rfind('data-tab="', 0, pos)
        assert tab_start != -1, f"no data-tab before {control_id_attr!r}"
        return html[tab_start + len('data-tab="'): html.index('"', tab_start + len('data-tab="'))]

    def test_world_mods_relocated_to_mods_tab(self):
        # User request 2026-09-10: Map Reveal, Headhunter, Tyrant's Crown and
        # Beacon move out of the World tab and into the Mods tab.
        html = PANEL_PAGE
        for control_id_attr, must_not_be in (
            ('id="map_reveal"', "world"),
            ('id="headhunter"', "world"),
            ('id="tyrant"', "world"),
            ('id="beacon"', "world"),
        ):
            tab = self._tab_of(html, control_id_attr)
            self.assertNotEqual(tab, must_not_be, f"{control_id_attr} still in the {tab!r} tab")
            self.assertEqual(tab, "mods", f"{control_id_attr} landed in {tab!r}, expected 'mods'")

    def test_map_reveal_joins_the_quality_of_life_section(self):
        # Map Reveal has no dedicated card of its own any more; it is a row in
        # the "Quality of Life" card (formerly "Gameplay Mods") alongside the
        # relic filter and orb pickup. `test_mods_categories.py` covers the
        # card boundaries and assignment rule in full; this just keeps the
        # one fact this module already depended on.
        html = PANEL_PAGE
        qol_start = html.index('id="qolCard"')
        qol_end = html.index('id="itemsCard"', qol_start + 1)
        self.assertIn('id="map_reveal"', html[qol_start:qol_end])

    def test_headhunter_tyrant_beacon_are_in_an_items_section(self):
        # ...and Headhunter/Tyrant's Crown/Beacon get their own "Items" card,
        # distinct from "Quality of Life" (they are mechanics tied to items
        # forged in the Item Editor, not standalone plugin toggles).
        html = PANEL_PAGE
        mods = panel_file("tabs/Mods.svelte")
        # The Items panel carries no heading of its own since the owner's
        # polish pass (its sub-tab names it), so the section starts at its id.
        items_start = mods.index('id="itemsCard"')
        self.assertGreater(items_start, mods.index('id="qolCard"'), "Items section should follow Quality of Life")
        # The Items section runs to the end of the Mods tab's markup.
        items_section = mods[items_start:]
        for control_id_attr in ('id="headhunter"', 'id="tyrant"', 'id="beacon"'):
            self.assertIn(control_id_attr, items_section)
        qol_start = html.index('id="qolCard"')
        # And NOT inside the Quality of Life card itself.
        qol_end = html.index('id="itemsCard"', qol_start + 1)
        for control_id_attr in ('id="headhunter"', 'id="tyrant"', 'id="beacon"'):
            self.assertNotIn(control_id_attr, html[qol_start:qol_end])

    def test_world_tab_no_longer_references_relocated_controls(self):
        # The four dedicated cards that used to hold these controls in the
        # World tab are gone outright: none of them kept a standalone <h2> of
        # their own - they became row labels inside the Items/Quality of Life
        # cards instead. (The plain mechanic names, e.g. "Tyrant's Crown", can
        # still appear in OTHER cards' prose - Monster Rarity cross-references
        # it - so this checks the specific old/new headings, not bare names.)
        html = PANEL_PAGE
        for old_heading in (
            "<h2>&#128506; Map Reveal</h2>",
            "<h2>&#129686; Headhunter</h2>",
            "<h2>&#128081; Tyrant's Crown</h2>",
            "<h2>&#128293; Beacon</h2>",
        ):
            self.assertNotIn(old_heading, html)
        mods_first_card = html.index('id="qolCard"')
        for row_label in ("Headhunter buffs on rare kills",
                          "Tyrant's Crown: more rares, richer rares",
                          "Beacon: every monster hunts you"):
            idx = html.find(row_label)
            self.assertGreaterEqual(idx, mods_first_card,
                f"{row_label!r} still appears before the Mods tab cards")


class TestRelicFilterArmScanLine(unittest.TestCase):
    """#93: one line when the filter arms, naming what the scan found.

    The filter's only other report is inside `Hook_DropRelic`, at a relic
    roll, and relics roll only in Satanic zones, so neither a live gate nor a
    player's log could say what the scan saw. `test_relic_filter_behavior.py`
    runs the line; this pins where it is emitted from.
    """

    @classmethod
    def setUpClass(cls):
        cls.plugin_code = PLUGIN_SRC.read_text(encoding="utf-8")
        cls.header = (FORGEPACT_INCLUDE_DIR / "RelicFilterMod.hpp").read_text(encoding="utf-8")
        cls.report = body(cls.plugin_code, "static void RelicFilterReportArmScan(")
        cls.frame = body(cls.plugin_code, "void FrameCallback(FWFrame& FrameContext)")

    def test_the_line_names_the_count_and_ids_of_the_scans_own_set(self):
        report = strip_comments(self.report)
        self.assertIn('"relicfilter: scan found "', report)
        self.assertIn('" maxed relics (ids "', report)
        self.assertIn('"none"', report)
        # The set it counts is the one GetPlayerMaxedRelics just filled, not a
        # cached or separately computed one.
        scan = re.search(r"const bool scanRan = rf\.GetPlayerMaxedRelics\((\w+), &\w+, &\w+\);", report)
        self.assertIsNotNone(scan, report)
        self.assertIn(f"std::vector<int> ids({scan.group(1)}.begin(), {scan.group(1)}.end());", report)
        self.assertIn("std::sort(ids.begin(), ids.end());", report)
        self.assertIn("std::to_string(ids.size())", report)

    def test_a_scan_that_did_not_run_says_so(self):
        report = strip_comments(self.report)
        self.assertRegex(report, r'if \(!scanRan\) \{\s*Out\("relicfilter: scan did not run \(no player yet\)"\);\s*return;')

    def test_it_is_emitted_from_the_arm_path_once_per_arm(self):
        frame = strip_comments(self.frame)
        call = frame.index("RelicFilterReportArmScan();")
        guard = frame.rfind("if (ForgePact::RelicFilterMod::Instance().IsArmScanDue()", 0, call)
        self.assertGreaterEqual(guard, 0, "the report is not guarded by the arm flag")
        condition = frame[guard:frame.index("{", guard)]
        self.assertIn("g_Setup", condition)
        self.assertIn("(fc % 60) == 0", condition)
        # A pending install goes first, so the line follows the hook install.
        self.assertIn("!ForgePact::RelicFilterMod::Instance().IsPending()", condition)
        self.assertLess(frame.index("if (ForgePact::RelicFilterMod::Instance().IsPending() && g_Setup"), guard)
        self.assertIn("HhResolveLocalPlayer(", frame[guard:call])
        # The report itself clears the flag, so one arm is one line.
        self.assertIn("rf.ClearArmScanDue();", strip_comments(self.report))
        # One call site in the whole plugin, and never from the roll itself.
        code = strip_comments(self.plugin_code)
        self.assertEqual(len(re.findall(r"(?<!void )RelicFilterReportArmScan\(\);", code)), 1)
        self.assertNotIn("RelicFilterReportArmScan", body(self.plugin_code, "static RValue& Hook_DropRelic("))
        self.assertNotIn("RelicFilterReportArmScan", body(self.plugin_code, "static RValue& Hook_GetRelicQuest("))

    def test_arming_makes_the_report_due_and_disarming_cancels_it(self):
        enable = body(self.header, "void SetEnabled(bool enabled, bool alreadyHooked)")
        self.assertIn("m_ArmScanDue.store(enabled);", enable)
        self.assertIn("std::atomic<bool> m_ArmScanDue{ false };", self.header)

    def test_the_line_ships_in_the_player_build(self):
        player = strip_comments(strip_research_blocks(self.plugin_code))
        self.assertIn('"relicfilter: scan found "', player)
        self.assertIn('"relicfilter: equipped slots "', player)
        self.assertIn('"relicfilter: relic tab "', player)
        self.assertIn("RelicFilterReportArmScan();", body(player, "void FrameCallback(FWFrame& FrameContext)"))

    # The SDK's equipped-slot read (hs_game_sdk/player.hpp) fills an
    # EquippedSlotScanReport only when a caller passes one, and a `scan found 0`
    # alone cannot say whether the slots were read. The arm line asks for it.

    def test_the_arm_scan_asks_for_the_equipped_slot_report(self):
        report = strip_comments(self.report)
        declared = re.search(r"HeroSiege::Player::EquippedSlotScanReport (\w+);", report)
        self.assertIsNotNone(declared, report)
        name = declared.group(1)
        self.assertRegex(report, rf"const bool scanRan = rf\.GetPlayerMaxedRelics\(\w+, &{name}, &\w+\);")
        line = f'Out("relicfilter: equipped slots " + HeroSiege::Player::FormatEquippedSlotScanReport({name}));'
        self.assertIn(line, report)
        # Right after the `scan found` line, and never for a scan that did not run.
        self.assertLess(report.index('"relicfilter: scan found "'), report.index(line))
        self.assertLess(report.index("if (!scanRan)"), report.index(line))
        self.assertEqual(report.count("relicfilter: equipped slots"), 1)

    # #125: the relic tab (Controller_obj.inventoryData[key].inventoryRelicGrid)
    # gets its own SDK report, on the line after the equipped slots'.

    def test_the_arm_scan_asks_for_the_relic_tab_report(self):
        report = strip_comments(self.report)
        declared = re.search(r"HeroSiege::Player::RelicTabScanReport (\w+);", report)
        self.assertIsNotNone(declared, report)
        name = declared.group(1)
        self.assertRegex(report, rf"const bool scanRan = rf\.GetPlayerMaxedRelics\(\w+, &\w+, &{name}\);")
        line = f'Out("relicfilter: relic tab " + HeroSiege::Player::FormatRelicTabScanReport({name}));'
        self.assertIn(line, report)
        self.assertLess(report.index('"relicfilter: equipped slots "'), report.index(line))
        self.assertLess(report.index("if (!scanRan)"), report.index(line))
        self.assertEqual(report.count("relicfilter: relic tab"), 1)

    def test_the_filter_forwards_the_report_to_the_sdk_scan(self):
        header = strip_comments(self.header)
        get = body(header, "bool GetPlayerMaxedRelics(")
        signature = header[header.index("bool GetPlayerMaxedRelics("):header.index("{", header.index("bool GetPlayerMaxedRelics("))]
        self.assertIn("HeroSiege::Player::EquippedSlotScanReport* equippedReport = nullptr", signature)
        self.assertIn("HeroSiege::Player::RelicTabScanReport* tabReport = nullptr", signature)
        self.assertIn("HeroSiege::Player::GetMaxedRelicIds(g_Yytk, player, equippedReport, tabReport)", get)
        # The roll itself asks for no report: its scan runs at every relic roll.
        for hook_name in ("static RValue& Hook_DropRelic(", "static RValue& Hook_GetRelicQuest("):
            hook = strip_comments(body(self.plugin_code, hook_name))
            self.assertNotIn("EquippedSlotScanReport", hook)
            self.assertNotIn("RelicTabScanReport", hook)
        cached = strip_comments(body(header, "const std::unordered_set<int>& MaxedForFrame("))
        self.assertIn("GetPlayerMaxedRelics(m_CacheSet)", cached)


class TestRelicFilterLever(unittest.TestCase):
    """#125: the filter answers GetRelicQuest, the relic pick's own draw-again check.

    ForgePact 2.0.1 wrote each maxed relic's `droprate.base` around DropRelic
    and logged "holding back", but no relic pick reads that field (hub
    docs/models/relic-pick-spec.md). `test_relic_filter_behavior.py` runs the
    lever; this pins its shape in the source.
    """

    @classmethod
    def setUpClass(cls):
        cls.plugin_code = PLUGIN_SRC.read_text(encoding="utf-8")
        cls.header = (FORGEPACT_INCLUDE_DIR / "RelicFilterMod.hpp").read_text(encoding="utf-8")
        cls.hook = strip_comments(body(cls.plugin_code, "static RValue& Hook_GetRelicQuest("))
        cls.drop = strip_comments(body(cls.plugin_code, "static RValue& Hook_DropRelic("))
        cls.frame = strip_comments(body(cls.plugin_code, "void FrameCallback(FWFrame& FrameContext)"))

    def test_nothing_writes_the_drop_table_any_more(self):
        for code in (self.hook, self.drop):
            self.assertNotIn("droprate", code)
            self.assertNotIn("variable_struct_set", code)
            self.assertNotIn("1e18", code)

    def test_the_game_answers_first_and_the_decision_is_the_pure_one(self):
        self.assertLess(self.hook.index("g_Orig_GetRelicQuest(S, O, R, argc, A)"), self.hook.index("QuestAnswer("))
        self.assertIn("HeroSiege::RewardScope::Active()", self.hook)
        self.assertIn("rf.MaxedForFrame(g_RuntimeFrame, scanRan)", self.hook)
        self.assertIn("ForgePact::RelicFilterMod::AnyRelicLeft(", self.hook)
        self.assertIn("rf.IsQuestCached(", self.hook)
        self.assertIn("res = RValue(true);", self.hook)

    def test_the_install_needs_the_native_detour(self):
        self.assertIn('HookOneScript("GetRelicQuest", "bp_grelicq", (PVOID)Hook_GetRelicQuest, &g_Orig_GetRelicQuest, &native);',
                      self.frame)
        self.assertIn("g_GetRelicQuestNative = native;", self.frame)
        self.assertNotIn('HookOneScript("DropRelic"', self.frame)
        state = strip_comments(body(self.plugin_code, "static std::string RelicFilterHookState("))
        self.assertIn("table-only", state)
        self.assertIn("if (!g_GetRelicQuestNative)", state)

    def test_status_ships_and_the_research_instruments_do_not(self):
        player = strip_comments(strip_research_blocks(self.plugin_code))
        branch = body(player, 'if (lc == "relicfilter")')
        self.assertIn("RelicFilterStatus();", branch)
        self.assertNotIn("RelicFilterTestMaxed", branch)
        self.assertNotIn("RelicFilterGround", branch)
        self.assertNotIn("static void RelicFilterTestMaxed(", player)
        self.assertNotIn("static void RelicFilterGround(", player)


class TestLiveOneResearchInstruments(unittest.TestCase):
    """`lootcensus` (#95 part 1, #77) and `goldtrace` (#77): research build only.

    Both are read-only instruments for Live 1. Neither may reach the player
    build: not in `kPlayerCommands`, and not in what the player build compiles.
    """

    @classmethod
    def setUpClass(cls):
        cls.plugin_code = PLUGIN_SRC.read_text(encoding="utf-8")
        cls.player = strip_comments(strip_research_blocks(cls.plugin_code))
        cls.allowlist = re.search(r"kPlayerCommands\s*=\s*\{(?P<body>.*?)\};", cls.plugin_code, re.S).group("body")

    def test_each_command_has_one_branch(self):
        self.assertEqual(self.plugin_code.count('lc == "lootcensus"'), 1)
        self.assertEqual(self.plugin_code.count('lc == "goldtrace"'), 1)

    def test_neither_reaches_the_player_build(self):
        for name in ("lootcensus", "goldtrace", "LootCensus", "GoldTrace"):
            self.assertNotIn(name, self.allowlist)
            self.assertNotIn(name, self.player)

    def test_lootcensus_resolves_both_objects_by_sdk_name(self):
        census = strip_comments(body(self.plugin_code, "static void LootCensus()"))
        for obj in ("Loot_Ground_obj", "Coin_obj"):
            self.assertIn(f"HeroSiege::Objects::GetObjectName(HeroSiege::Objects::GameObject::{obj})", census)
        self.assertIn('"asset_get_index"', census)
        self.assertIn('"instance_number"', census)
        self.assertIn('"instance_find"', census)
        self.assertIn("kLootCensusWalkCap = 2048", self.plugin_code)

    def test_lootcensus_reads_only(self):
        census = strip_comments(body(self.plugin_code, "static void LootCensus()"))
        # lootFilterVisible is the game's own variable: checked before it is read.
        self.assertLess(census.index('RValue("lootFilterVisible")'),
                        census.index('"variable_instance_get", { inst, RValue("lootFilterVisible")'))
        self.assertIn('"variable_instance_exists", { inst, RValue("lootFilterVisible") }', census)
        for write in ("variable_instance_set", "variable_struct_set", "instance_destroy", "instance_create"):
            self.assertNotIn(write, census)
        for field in ("ground=", "hidden=", "invisible=", "coins=", "walked="):
            self.assertIn(field, census)

    def test_goldtrace_is_fed_from_logdrop_for_the_two_gold_scripts_only(self):
        logdrop = body(self.plugin_code, "static void LogDrop(const char* fn, RValue& res, int argc, RValue** A)")
        self.assertIn("GoldTraceAppend(fn, argc, A);", logdrop)
        self.assertNotIn("GoldTraceAppend", strip_research_blocks(logdrop))
        append = strip_comments(body(self.plugin_code, "static void GoldTraceAppend("))
        self.assertIn('"DropGold"', append)
        self.assertIn('"DropMonsterGold"', append)
        self.assertIn("kGoldTraceMaxLines = 400", self.plugin_code)
        self.assertIn("kGoldTraceMaxLines", append)
        self.assertIn("goldtrace.txt", self.plugin_code)
        self.assertIn('"%.6g"', append)


if __name__ == "__main__":
    unittest.main()
