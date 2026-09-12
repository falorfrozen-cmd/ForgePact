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
        self.assertIn('data-tab="mods"', forgepact.HTML)
        self.assertIn('id="mod_filter_max_relics"', forgepact.HTML)
        self.assertIn("Remove owned relics from drop pool", forgepact.HTML)
        self.assertIn("10 out of 10", forgepact.HTML)

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
        self.assertIn("Hook_DropRelic", self.plugin_code)
        self.assertIn("GetPlayerMaxedRelics", self.plugin_code)

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
        self.assertIn('id="mod_orb_pickup_radius"', forgepact.HTML)

    def test_release_setup_is_delayed_past_character_selection(self):
        self.assertIn("if (!g_Setup && fc > 300)", self.plugin_code)

    def test_relic_filter_hook_is_deferred_until_a_player_exists(self):
        # Sending relicfilter must only ARM the mod; hooking DropRelic while the
        # character screen runs stalls the runner for about a minute.
        # Migrated into ForgePact::RelicFilterMod (2026-09 class split): the
        # pending flag is now m_Pending, exposed via IsPending()/ClearPending().
        self.assertIn("m_Pending", self.relic_filter_header)
        armed = self.plugin_code.split('if (lc == "relicfilter")', 1)[1][:800]
        self.assertNotIn("HookOneScript(\"DropRelic\"", armed)
        self.assertIn("RelicFilterMod::Instance().SetEnabled(enable, g_Orig_DropRelic != nullptr);", armed)
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
        self.assertIn("static void OrbPickupTick()", self.plugin_code)
        self.assertIn("OrbPickupTick();", self.plugin_code)
        tick = self.plugin_code.split("static void OrbPickupTick()", 1)[1][:900]
        self.assertIn("instance_number", tick)
        self.assertIn("instance_find", tick)

    def test_orb_pickup_no_longer_relies_on_interception(self):
        # Both dead interception points must be gone, not left behind as noise.
        self.assertNotIn("Hook_ExpGlobeStep", self.plugin_code)
        self.assertNotIn('HookBuiltin("distance_to_object", "fp_orb', self.plugin_code)

    def test_orb_pickup_tick_is_bounded(self):
        # A runaway globe count must not be able to cost a frame.
        tick = self.plugin_code.split("static void OrbPickupTick()", 1)[1][:900]
        self.assertIn("budget", tick)

    def test_orb_pickup_reads_player_position_once_per_frame(self):
        # Not once per globe per step.
        self.assertIn("g_PlayerPosValid", self.plugin_code)
        pull = self.plugin_code.split("static void PullOneGlobe", 1)[1][:1200]
        self.assertNotIn("HhResolveLocalPlayer", pull)

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

    def test_stall_watchdog_is_present_in_every_build(self):
        # Names the culprit when frames stop arriving; must not be dev-only.
        self.assertIn("static void StallWatchdogLoop()", self.plugin_code)
        self.assertIn("StartStallWatchdog();", self.plugin_code)
        watchdog = self.plugin_code.split("static void StallWatchdogLoop()", 1)[1]
        watchdog = watchdog[:watchdog.index("static void StartStallWatchdog()")]
        self.assertNotIn("#ifndef FORGEPACT_RELEASE", watchdog)
        # The frame thread must be resumed before anything that can allocate,
        # or the watchdog deadlocks on a lock the stalled thread is holding.
        self.assertLess(watchdog.index("ResumeThread"), watchdog.index("ms - frame thread at"))

    def test_unknown_runner_cache_is_removed(self):
        panel_code = (SRC_DIR / "forgepact.py").read_text(encoding="utf-8")
        self.assertIn("if cache.exists():", panel_code)
        self.assertIn("cache.unlink()", panel_code)

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
        html = forgepact.HTML
        for control_id_attr, must_not_be in (
            ('id="map_reveal"', "world"),
            ('id="headhunter"', "world"),
            ('id="tyrant"', "world"),
            ('id="beacon"', "world"),
        ):
            tab = self._tab_of(html, control_id_attr)
            self.assertNotEqual(tab, must_not_be, f"{control_id_attr} still in the {tab!r} tab")
            self.assertEqual(tab, "mods", f"{control_id_attr} landed in {tab!r}, expected 'mods'")

    def test_map_reveal_joins_the_gameplay_mods_section(self):
        # Map Reveal has no dedicated card of its own any more; it is a row in
        # the same "Gameplay Mods" card as the relic filter and orb pickup.
        html = forgepact.HTML
        gm_start = html.index("Gameplay Mods")
        gm_end = html.index('<div class="card tab-card" data-tab="mods">', gm_start + 1)
        self.assertIn('id="map_reveal"', html[gm_start:gm_end])

    def test_headhunter_tyrant_beacon_are_in_an_items_section(self):
        # ...and Headhunter/Tyrant's Crown/Beacon get their own "Items" card,
        # distinct from "Gameplay Mods" (they are mechanics tied to items
        # forged in the Item Editor, not standalone plugin toggles).
        html = forgepact.HTML
        items_start = html.index("Items</h2>")
        gm_start = html.index("Gameplay Mods")
        self.assertGreater(items_start, gm_start, "Items section should follow Gameplay Mods")
        items_section = html[items_start:]
        for control_id_attr in ('id="headhunter"', 'id="tyrant"', 'id="beacon"'):
            self.assertIn(control_id_attr, items_section[:items_section.index("</script>")])
        # And NOT inside the Gameplay Mods card itself.
        gm_end = html.index('<div class="card tab-card" data-tab="mods">', gm_start + 1)
        for control_id_attr in ('id="headhunter"', 'id="tyrant"', 'id="beacon"'):
            self.assertNotIn(control_id_attr, html[gm_start:gm_end])

    def test_world_tab_no_longer_references_relocated_controls(self):
        # The four dedicated cards that used to hold these controls in the
        # World tab are gone outright: none of them kept a standalone <h2> of
        # their own - they became row labels inside the Items/Gameplay Mods
        # cards instead. (The plain mechanic names, e.g. "Tyrant's Crown", can
        # still appear in OTHER cards' prose - Monster Rarity cross-references
        # it - so this checks the specific old/new headings, not bare names.)
        html = forgepact.HTML
        for old_heading in (
            "<h2>&#128506; Map Reveal</h2>",
            "<h2>&#129686; Headhunter</h2>",
            "<h2>&#128081; Tyrant's Crown</h2>",
            "<h2>&#128293; Beacon</h2>",
        ):
            self.assertNotIn(old_heading, html)
        mods_first_card = html.index('<div class="card tab-card" data-tab="mods">')
        for row_label in ("Headhunter buffs on rare kills",
                          "Tyrant's Crown: more rares, richer rares",
                          "Beacon: every monster hunts you"):
            idx = html.find(row_label)
            self.assertGreaterEqual(idx, mods_first_card,
                f"{row_label!r} still appears before the Mods tab cards")


if __name__ == "__main__":
    unittest.main()
