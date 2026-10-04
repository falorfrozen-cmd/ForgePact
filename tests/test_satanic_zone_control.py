#!/usr/bin/env python3
"""Satanic Zone control (issue #157): the `satzone` command and the two
World-tab switches.

The zone is a writable protected value (ForgePact #155/#156; the research
doc's "Live 3"): `Controller_obj.satanicZone` is the key, `GPV`/`SPV` are the
game's own read/write scripts, and the game re-rolls on its own, so a pin is
re-asserted on the satmods poll. What this pins:

- the command is a player command (in `kPlayerCommands`, reachable after the
  research blocks are stripped), a standalone early return, and off by
  default (pin -1, follow/everywhere false);
- the everywhere force sits in `HookLoadSatanicZone`'s body and reads the
  atomic; its hook install is deferred to the frame tick and gated on a player
  existing (char select must not get hooks - the restartanytime rule);
- the pin tick runs from `SatanicPollTick` before its bail-out, and the zone
  is read and written through `gml_Script_GPV`/`gml_Script_SPV` only - never a
  `variable_instance_set` on `satanicZone` (that would write the key, not the
  protected value);
- the panel carries both switches with their ids, their boot state and their
  handlers, `build_cmds` sends the two commands only while set, and the
  defaults fixture already carries the keys (test_enabled_mods_panel).
"""

import re
import sys
import unittest
from pathlib import Path

TESTS = Path(__file__).resolve().parent
ROOT = TESTS.parent
sys.path.insert(0, str(TESTS))

from test_release_hook_contract import function_body, strip_comments, strip_research_blocks  # noqa: E402


class SatanicZoneControlPluginTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = (ROOT / "plugin" / "ModuleMain.cpp").read_text(encoding="utf-8", errors="replace")
        cls.tick = function_body(cls.plugin, "static void SatanicPollTick()")
        cls.hook = function_body(cls.plugin, "static RValue& HookLoadSatanicZone(")
        cls.pin_tick = function_body(cls.plugin, "static void SatZoneTick()")
        cls.cmd = function_body(cls.plugin, "static void SatZoneCmd(")
        cls.reader = function_body(cls.plugin, "static bool SatZoneReadValue(")
        cls.writer = function_body(cls.plugin, "static bool SatZoneWriteValue(")

    def test_command_is_a_player_command(self):
        allowlist = re.search(r"kPlayerCommands\s*=\s*\{(?P<body>.*?)\};", self.plugin, re.DOTALL)
        self.assertIsNotNone(allowlist)
        self.assertIn('"satzone"', allowlist.group("body"))
        # A release build silently rejects an unlisted command.
        self.assertIn('"satmods", "satzone"', self.plugin)
        self.assertEqual(strip_comments(self.plugin).count('lc == "satzone"'), 1)
        release = strip_research_blocks(strip_comments(self.plugin))
        self.assertIn('lc == "satzone"', release)

    def test_command_is_a_standalone_early_return(self):
        # Not one more `else if`: that chain is at MSVC's nesting limit
        # (C1061), the restartanytime/toggleguard rule.
        self.assertIn('if (lc == "satzone") { SatZoneCmd(rest); return; }', self.plugin)
        self.assertNotIn('else if (lc == "satzone")', self.plugin)

    def test_defaults_are_off(self):
        for decl in ("static std::atomic<int>  g_SatZonePin{ -1 };",
                     "static std::atomic<bool> g_SatZoneFollow{ false };",
                     "static std::atomic<bool> g_SatEverywhere{ false };"):
            self.assertIn(decl, self.plugin)

    def test_everywhere_force_is_in_the_hook_body(self):
        body = strip_comments(self.hook)
        self.assertIn("if (g_SatEverywhere.load()) r = RValue(true);", body)

    def test_hook_install_is_deferred_to_the_tick(self):
        # The tick attempts the install, gated on a player existing; the
        # command itself never installs a hook (char select must not get one).
        tick = strip_comments(self.pin_tick)
        self.assertIn("g_SatEverywhere.load() && !g_OrigLoadSatanicZone", tick)
        self.assertIn("SatZonePlayerExists()", tick)
        self.assertIn("EnsureSatanicZoneHook();", tick)
        self.assertNotIn("EnsureSatanicZoneHook", strip_comments(self.cmd))

    def test_pin_tick_runs_before_the_poll_bailout(self):
        body = strip_comments(self.tick)
        self.assertIn("SatZoneTick();", body)
        self.assertLess(body.index("SatZoneTick();"), body.index("g_SatDisabledBuffs.empty()"))

    def test_zone_is_read_and_written_by_name_only(self):
        read = strip_comments(self.reader)
        write = strip_comments(self.writer)
        self.assertIn('"gml_Script_GPV"', read)
        self.assertIn('"gml_Script_SPV"', write)
        # The key itself must never be overwritten: writing `satanicZone`
        # would replace the protected-value handle, not the value.
        self.assertNotIn('variable_instance_set", { controller, RValue("satanicZone")',
                         strip_comments(self.plugin))

    def test_player_resolution_uses_the_proven_pair(self):
        # instance_find(Player_obj) hands back a VALUE_REF on this runner
        # (measured 2026-09-10); a hand-rolled ToDouble() + GetInstanceObject
        # path silently resolved no player in the frame tick (live
        # 2026-10-04: refused=30 "player instance unreadable" while the
        # command path's pcall worked). Both bodies must use the proven pair,
        # which resolves the reference through the engine's own
        # @@GetInstance@@, and must not call GetInstanceObject on it.
        for body in (strip_comments(self.reader), strip_comments(self.writer)):
            self.assertIn("HhResolveLocalPlayer(player)", body)
            self.assertIn("HhResolveInstance(player)", body)
            self.assertNotIn("GetInstanceObject", body)

    def test_pin_refuses_non_act_rooms(self):
        # Towns and sub-areas are not zones: the shape gate is the tracker's
        # IsActZoneRoomName precedent.
        body = strip_comments(self.plugin)
        self.assertIn("static bool SatZoneIsZoneRoomName(", body)
        self.assertIn('name.size() != 9U', body)
        self.assertIn("is not an act zone", body)


class SatanicZoneControlPanelTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.forgepact = (ROOT / "src" / "forgepact.py").read_text(encoding="utf-8", errors="replace")
        cls.world = (ROOT / "panel" / "src" / "tabs" / "World.svelte").read_text(encoding="utf-8", errors="replace")
        cls.panel = (ROOT / "panel" / "src" / "panel.js").read_text(encoding="utf-8", errors="replace")
        cls.mods = (ROOT / "panel" / "src" / "enabled-mods.js").read_text(encoding="utf-8", errors="replace")
        cls.readme = (ROOT / "README.md").read_text(encoding="utf-8", errors="replace")

    def test_defaults_are_off(self):
        for key in ("satanic_follow", "satanic_everywhere"):
            self.assertIn(f'"{key}": False,', self.forgepact)

    def test_build_cmds_send_only_while_set(self):
        cmds = self.forgepact[self.forgepact.index("def build_cmds"):]
        cmds = cmds[:cmds.index("\ndef ", 10)]
        self.assertIn('if cfg.get("satanic_follow", False):', cmds)
        self.assertIn('out.append("satzone follow 1")', cmds)
        self.assertIn('if cfg.get("satanic_everywhere", False):', cmds)
        self.assertIn('out.append("satzone everywhere 1")', cmds)

    def test_api_set_handlers_post_the_toggled_value(self):
        for key in ("satanic_follow", "satanic_everywhere"):
            self.assertIn(f"key:'{key}',value:e.target.checked", self.panel)
            self.assertIn(f"send_cmds([f\"satzone {'follow' if key == 'satanic_follow' else 'everywhere'} "
                          f"{{1 if cfg['{key}'] else 0}}\"], cfg)", self.forgepact)
        self.assertEqual(self.panel.count("getElementById('satanic_follow').onchange"), 1)
        self.assertEqual(self.panel.count("getElementById('satanic_everywhere').onchange"), 1)

    def test_boot_restores_both_switches(self):
        for key, val in (("satanic_follow", "szfval"), ("satanic_everywhere", "szeval")):
            self.assertIn(f"getElementById('{key}').checked", self.panel)
            self.assertIn(f"getElementById('{val}').textContent", self.panel)

    def test_world_tab_carries_the_two_rows(self):
        for ident in ("satanic_follow", "satanic_everywhere", "szfval", "szeval"):
            self.assertIn(f'id="{ident}"', self.world)
        self.assertIn("Keep the zone you are in satanic", self.world)
        self.assertIn("Every zone counts as satanic", self.world)

    def test_switches_are_enabled_mods_entries(self):
        # Off by default and each sends its line while on, so the Enabled
        # mods list shows them like any other mod: the pools' exclusion
        # (all-on by default) does not apply to the switches (#157).
        body = re.search(r"BOOLEAN_MODS\s*=\s*\[(?P<body>.*?)\];", self.mods, re.DOTALL).group("body")
        for key in ("satanic_follow", "satanic_everywhere"):
            self.assertIn(f"'{key}'", body)

    def test_readme_documents_the_command(self):
        self.assertIn("satzone", self.readme)
