#!/usr/bin/env python3
"""Contract tests for the map-reveal mod and its monster sub-toggle.

Background (ForgePact/docs/map-reveal-research.md, measured live 2026-09-11):
the fog clear alone already reveals every *static* icon, so the mod's second
half exists for monsters only - and monsters were missing because most packs
are never created until the player walks near their spawner, not because a
draw flag was off. The pack pass is therefore a real gameplay/perf change
riding on a cosmetic toggle, which is why it gets its own checkbox and why
these tests pin the parent/child wiring in both directions.
"""

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


class TestMapRevealContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin_code = PLUGIN_SRC.read_text(encoding="utf-8")
        cls.panel_code = (SRC_DIR / "forgepact.py").read_text(encoding="utf-8")
        cls.header = (FORGEPACT_INCLUDE_DIR / "MapRevealManager.hpp").read_text(encoding="utf-8")

    # ---- defaults ----------------------------------------------------------
    def test_defaults_carry_both_keys(self):
        self.assertIn("map_reveal", forgepact.DEFAULTS)
        self.assertIn("map_reveal_packs", forgepact.DEFAULTS)

    def test_parent_defaults_off_and_child_defaults_on(self):
        # The parent stays opt-in; the child defaults on so that turning the
        # mod on does what its name says, without a second click.
        self.assertFalse(forgepact.DEFAULTS["map_reveal"])
        self.assertTrue(forgepact.DEFAULTS["map_reveal_packs"])

    # ---- build_cmds --------------------------------------------------------
    def test_build_cmds_emits_nothing_when_parent_off(self):
        cfg = dict(forgepact.DEFAULTS, map_reveal=False)
        self.assertFalse([c for c in forgepact.build_cmds(cfg) if c.startswith("reveal")])

    def test_build_cmds_emits_reveal_when_on(self):
        cfg = dict(forgepact.DEFAULTS, map_reveal=True, map_reveal_packs=True)
        cmds = forgepact.build_cmds(cfg)
        self.assertIn("reveal 1", cmds)
        # packs default on in the plugin, so nothing extra is needed
        self.assertNotIn("reveal packs 1", cmds)

    def test_build_cmds_turns_packs_off_explicitly(self):
        cfg = dict(forgepact.DEFAULTS, map_reveal=True, map_reveal_packs=False)
        cmds = forgepact.build_cmds(cfg)
        self.assertIn("reveal 1", cmds)
        self.assertIn("reveal packs 0", cmds)
        self.assertLess(cmds.index("reveal 1"), cmds.index("reveal packs 0"))

    def test_packs_never_emitted_while_parent_is_off(self):
        # The child is meaningless on its own; emitting it would turn the
        # plugin flag on for a mod the player has switched off.
        cfg = dict(forgepact.DEFAULTS, map_reveal=False, map_reveal_packs=True)
        self.assertFalse([c for c in forgepact.build_cmds(cfg) if c.startswith("reveal")])

    # ---- plugin surface ----------------------------------------------------
    def test_plugin_accepts_the_packs_subcommand(self):
        self.assertIn('v.rfind("packs", 0) == 0', self.plugin_code)

    def test_reveal_is_in_the_release_whitelist(self):
        block = self.plugin_code[self.plugin_code.index("kPlayerCommands"):][:2000]
        self.assertIn('"reveal"', block)

    def test_stat_is_research_only(self):
        # `reveal stat` must sit behind the release guard, like orbpickup's.
        idx = self.plugin_code.index('lc == "reveal"')
        block = self.plugin_code[idx:idx + 2500]
        guard = block.index("#ifndef FORGEPACT_RELEASE")
        stat = block.index('v == "stat"')
        self.assertLess(guard, stat)

    # ---- the mechanism itself ---------------------------------------------
    def test_pack_pass_is_a_bounded_window_not_a_permanent_hook(self):
        # The spawn lie rides a hot builtin; it must switch itself off.
        self.assertIn("kSpawnWindowFrames", self.header)
        self.assertIn("m_SpawnWindow", self.header)
        self.assertIn("WantsPackSpawn", self.header)

    def test_hot_path_reads_one_relaxed_atomic(self):
        self.assertIn("std::atomic<int> m_SpawnWindow", self.header)
        self.assertIn("m_SpawnWindow.load(std::memory_order_relaxed)", self.header)

    def test_distance_hook_serves_both_callers_without_beacon_side_effects(self):
        # Map-reveal must not switch on the Beacon's hunt behaviour (map-sized
        # aggroRange / skipped leash) just to populate a zone.
        self.assertIn("InstallDistanceLieHook", self.plugin_code)
        idx = self.plugin_code.index("static void InstallDistanceLieHook")
        body = self.plugin_code[idx:idx + 400]
        self.assertNotIn("PathFindScanTick", body)
        self.assertNotIn("PathFindLeashCheck", body)

    def test_spawn_window_waits_for_creator_readiness(self):
        # REGRESSION (measured 2026-09-11): opening the window straight from
        # the zone-identity change caught the creators mid-initialisation and
        # left them inert - enemyCreatorTimer undefined, no packs then and no
        # packs even when walked onto afterwards, i.e. emptier than vanilla.
        # Readiness must be asked about, never waited out with a fixed delay.
        self.assertIn("TryOpenSpawnWindow", self.header)
        self.assertIn("enemyCreatorTimer", self.header)
        self.assertIn("m_PacksPending", self.header)
        idx = self.header.index("void TryOpenSpawnWindow")
        body = self.header[idx:idx + 1800]
        # the window may only open after a real timer value is seen
        self.assertIn("VALUE_REAL", body)
        self.assertIn("if (!ready) return;", body)
        self.assertLess(body.index("if (!ready) return;"), body.index("m_SpawnWindow.store"))

    def test_window_does_not_survive_a_zone_transition(self):
        # REPORTED 2026-09-12 (origin's review of PR #2, issue 2): neither
        # ResetIdentity() nor the new-zone branch cleared an already-open
        # window, so walking to another zone mid-window left WantsPackSpawn()
        # true while the next zone was still loading - the readiness gate
        # bypassed, and the inert-creator damage above reachable by a second
        # route. Losing the map, changing identity, or changing room all have
        # to shut the window.
        self.assertIn("void CloseSpawnWindow()", self.header)
        closer = self.header[self.header.index("void CloseSpawnWindow()"):][:400]
        self.assertIn("m_SpawnWindow.store(0", closer)
        self.assertIn("m_PacksPending = false", closer)

        reset = self.header[self.header.index("void ResetIdentity()"):][:400]
        self.assertIn("CloseSpawnWindow();", reset)

        # The new-zone branch must drop the previous zone's window before
        # arming the next one.
        tick = self.header[self.header.index("void Tick()"):]
        tick = tick[: tick.index("void TryOpenSpawnWindow")]
        self.assertIn("CloseSpawnWindow();", tick)
        self.assertLess(tick.index("CloseSpawnWindow();"), tick.index("m_PacksPending = true"))

    def test_authorization_is_checked_where_the_distance_is_changed(self):
        # REPORTED 2026-09-12, second round: OnFrame runs at EVENT_FRAME,
        # which this YYToolkit dispatches from HkPresent - the END of the
        # frame - while the creators consume the permission during their step
        # events, earlier in the same frame. A window invalidated at Present
        # is already too late for the first call in a new zone, and no amount
        # of extra identity tracking at Present can fix that ordering.
        #
        # So the authorization asks the creator in hand, at the point its
        # distance would be changed. Behaviour is covered by
        # test_map_reveal_behavior.py, which calls the real hook before the
        # next OnFrame; this pins the structure.
        self.assertIn("bool MayPopulate(const RValue& creator) const", self.header)
        self.assertIn("static bool CreatorIsReady(const RValue& creator)", self.header)
        self.assertIn("enemyCreatorTimer", self.header[self.header.index("static bool CreatorIsReady"):][:600])

        hook = self.plugin_code[self.plugin_code.index("static void Hook_distance_to_object("):]
        hook = hook[: hook.index("\nstatic void InstallDistanceLieHook")]
        self.assertIn("CreatorIsReady(inst)", hook)
        self.assertIn("MayPopulate(inst)", hook)
        # The guard must precede the assignment it guards.
        self.assertLess(hook.index("CreatorIsReady(inst)"), hook.index("Result = RValue(0.0);"))
        self.assertLess(hook.index("MayPopulate(inst)"), hook.index("Result = RValue(0.0);"))

    def test_window_identity_is_full_and_never_unknown(self):
        # Two related gaps reported with the above: the per-frame check
        # compared only the room key, so a replaced or removed minimap with an
        # unchanged room key kept the window; and TryOpenSpawnWindow stored
        # INT64_MIN when the room was unreadable, so every later failed read
        # compared equal to it and the window was never invalidated.
        frame = self.header[self.header.index("void OnFrame("):]
        frame = frame[: frame.index("private:")]
        self.assertIn("!WindowIdentityValid()", frame)
        self.assertLess(frame.index("!WindowIdentityValid()"),
                        frame.index("m_SpawnWindow.store(w - 1"))
        self.assertLess(frame.index("!WindowIdentityValid()"), frame.index("% 20"))

        # Identity is room + minimap instance + grid, and a failed read is a
        # failure rather than a sentinel value that can compare equal.
        valid = self.header[self.header.index("bool WindowIdentityValid()"):][:500]
        for field in ("m_WindowRoom", "m_WindowInstance", "m_WindowGrid"):
            self.assertIn(field, valid)
        self.assertIn("if (!ReadIdentity(room, inst, grid)) return false;", valid)

        # A window is never opened against an identity that could not be read.
        opener = self.header[self.header.index("void TryOpenSpawnWindow"):][:2600]
        self.assertIn("if (!ReadIdentity(room, minimap, grid)) return;", opener)
        self.assertLess(opener.index("ReadIdentity(room, minimap, grid)"),
                        opener.index("m_SpawnWindow.store(kSpawnWindowFrames"))

    def test_enabling_packs_applies_to_the_current_zone(self):
        # REPORTED 2026-09-12 (issue 3): SetPacks(true) only flipped the flag.
        # Tick() returns early while the zone identity is unchanged, so the
        # zone the player was standing in never got armed - the checkbox said
        # it applied live and nothing happened until the next zone change or a
        # reveal off/on cycle.
        setter = self.header[self.header.index("void SetPacks(bool on)"):][:900]
        self.assertIn("m_PacksPending = true", setter)
        # It must ARM the readiness-gated pass, not open the window directly -
        # skipping the gate would reintroduce the inert-creator bug.
        self.assertNotIn("m_SpawnWindow.store(kSpawnWindowFrames", setter)
        # Only on an off->on edge, and only while the parent is on.
        self.assertIn("!was", setter)
        self.assertIn("m_Enabled", setter)

    def test_zone_with_no_creators_is_left_alone(self):
        idx = self.header.index("void TryOpenSpawnWindow")
        body = self.header[idx:idx + 1800]
        self.assertIn("nothing to populate here", body)

    def test_pending_state_gives_up_rather_than_polling_forever(self):
        self.assertIn("kPendingGiveUpTicks", self.header)

    def test_reveal_does_not_spawn_monsters_itself(self):
        # The game's own creator logic must do the spawning, so pack
        # composition and rarity stay vanilla.
        self.assertNotIn("instance_create", self.header)

    # ---- guardrails from the 2026-09-10 decisions --------------------------
    def test_never_unlocks_waypoints(self):
        # Measured: revealing a waypoint icon leaves waypointActive false.
        # Nothing here may start writing it.
        self.assertNotIn("UnlockWaypoint", self.header)
        self.assertNotIn("UnlockWaypoint", self.plugin_code)
        self.assertNotIn("waypointActive", self.header)

    def test_never_writes_the_players_own_minimap_options(self):
        for opt in ("minimapShowMonsters", "minimapShowEnvironment"):
            self.assertNotIn(opt, self.header)

    def test_no_per_object_discovery_sweep(self):
        # isDiscovered was measured NOT to gate minimap icons; a sweep over it
        # would be cost with no effect. The header is allowed to *explain*
        # that in a comment - what must not exist is a write in real code.
        self.assertNotIn("isDiscovered", self._code_only(self.header))

    @staticmethod
    def _code_only(text):
        return "\n".join(
            line for line in text.splitlines() if not line.lstrip().startswith("//")
        )

    # ---- panel -------------------------------------------------------------
    def test_both_controls_render_in_the_gameplay_mods_section(self):
        self.assertIn('id="map_reveal"', self.panel_code)
        self.assertIn('id="map_reveal_packs"', self.panel_code)

    def test_child_row_is_disabled_while_parent_is_off(self):
        self.assertIn("syncRevealPacks", self.panel_code)
        idx = self.panel_code.index("function syncRevealPacks")
        body = self.panel_code[idx:idx + 600]
        self.assertIn("box.disabled=!parentOn", body)

    def test_turning_the_parent_on_restates_the_child(self):
        # Otherwise a player who turned packs off would get them back silently
        # after toggling the parent.
        idx = self.panel_code.index('elif key == "map_reveal":')
        body = self.panel_code[idx:idx + 700]
        self.assertIn("reveal packs", body)

    def test_child_has_its_own_live_push(self):
        self.assertIn('elif key == "map_reveal_packs":', self.panel_code)

    def test_row_repaints_before_awaiting_the_post(self):
        # Caught live 2026-09-11: with the panel server gone the fetch throws,
        # so anything after `await` never runs - the child row stayed enabled
        # and read "on" beneath a switched-off parent.
        idx = self.panel_code.index("document.getElementById('map_reveal').onchange")
        body = self.panel_code[idx:idx + 900]
        self.assertLess(body.index("syncRevealPacks"), body.index("await j('/api/set'"))

    def test_description_explains_why_monsters_were_missing(self):
        # The honest bit: it is not a visibility flag, the packs do not exist.
        idx = self.panel_code.index('id="map_reveal_packs_row"')
        row = self.panel_code[idx:idx + 900]
        self.assertIn("do not exist", row)


if __name__ == "__main__":
    unittest.main()
