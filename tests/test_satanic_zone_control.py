#!/usr/bin/env python3
"""Satanic Zone control (issue #157): the `satzone` command and the
World-tab switch.

The zone is a writable protected value (ForgePact #155/#156; the research
doc's "Live 3"): `Controller_obj.satanicZone` is the key, `GPV`/`SPV` are the
game's own read/write scripts, and the game re-rolls on its own, so a pin is
re-asserted on the satmods poll. What this pins:

- the command is a player command (in `kPlayerCommands`, reachable after the
  research blocks are stripped), a standalone early return, and off by
  default (pin -1, everywhere false);
- the follow mode ("Keep the zone you are in satanic") is gone everywhere:
  no atomic, no sub-command, no stat field, no setting, no panel row, and a
  saved `satanic_follow` is retired on load (removed before it shipped:
  Every zone counts as satanic covers it, the owner, 2026-10-08);
- the everywhere force sits in `HookLoadSatanicZone`'s body and reads the
  atomic; its hook install is deferred to the frame tick and gated on a player
  existing (char select must not get hooks - the restartanytime rule);
- the pin tick runs from `SatanicPollTick` before its bail-out, and the zone
  is read and written through `gml_Script_GPV`/`gml_Script_SPV` only - never a
  `variable_instance_set` on `satanicZone` (that would write the key, not the
  protected value);
- the panel carries the everywhere switch with its id, its boot state and its
  handler, `build_cmds` sends its command only while set, and the defaults
  fixture already carries the key (test_enabled_mods_panel).
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
                     "static std::atomic<bool> g_SatEverywhere{ false };"):
            self.assertIn(decl, self.plugin)

    def test_follow_mode_is_gone(self):
        # "Keep the zone you are in satanic" was removed (the owner,
        # 2026-10-08): no atomic, no sub-command (`satzone follow ...` falls
        # to the usage line), no `follow=` in the stat line, no follow in the
        # usage text, and the tick's early return reads only the pin.
        self.assertNotIn("g_SatZoneFollow", self.plugin)
        cmd = strip_comments(self.cmd)
        self.assertNotIn('sub == "follow"', cmd)
        self.assertNotIn("follow", cmd)
        tick = strip_comments(self.pin_tick)
        self.assertNotIn("follow", tick)
        self.assertIn("if (pin < 0) return;", tick)
        self.assertIn("satzone pin here|<index> | satzone everywhere 0|1 | satzone off | satzone stat", cmd)

    def test_pin_and_everywhere_are_kept(self):
        # Negative control for the removal: the pin, `off` and everywhere
        # sub-commands still exist.
        cmd = strip_comments(self.cmd)
        for sub in ('sub == "pin"', 'sub == "off"', 'sub == "everywhere"'):
            self.assertIn(sub, cmd)
        self.assertIn("g_SatZonePin.store(-1);", cmd)
        self.assertIn("SatZoneWriteValue(key, target)", strip_comments(self.pin_tick))

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
        self.assertIn('"satanic_everywhere": False,', self.forgepact)
        self.assertNotIn('"satanic_follow": False,', self.forgepact)

    def test_build_cmds_send_only_while_set(self):
        cmds = self.forgepact[self.forgepact.index("def build_cmds"):]
        cmds = cmds[:cmds.index("\ndef ", 10)]
        self.assertNotIn("satanic_follow", cmds)
        self.assertNotIn("satzone follow", cmds)
        self.assertIn('if cfg.get("satanic_everywhere", False):', cmds)
        self.assertIn('out.append("satzone everywhere 1")', cmds)

    def test_api_set_handlers_post_the_toggled_value(self):
        self.assertIn("key:'satanic_everywhere',value:e.target.checked", self.panel)
        self.assertIn("send_cmds([f\"satzone everywhere {1 if cfg['satanic_everywhere'] else 0}\"], cfg)",
                      self.forgepact)
        self.assertEqual(self.panel.count("getElementById('satanic_everywhere').onchange"), 1)
        # The removed follow switch has no handler, no /api/set boolean entry
        # and no live send.
        self.assertNotIn("satanic_follow", self.panel)
        self.assertNotIn("satzone follow", self.forgepact)
        api_set = self.forgepact[self.forgepact.index('if u.path == "/api/set":'):]
        api_set = api_set[:api_set.index('elif u.path == "/api/setexe":')]
        self.assertNotIn('"satanic_follow"', api_set)

    def test_boot_restores_the_switch(self):
        self.assertIn("getElementById('satanic_everywhere').checked", self.panel)
        self.assertIn("getElementById('szeval').textContent", self.panel)
        self.assertNotIn("szfval", self.panel)

    def test_world_tab_carries_the_one_row(self):
        for ident in ("satanic_everywhere", "szeval"):
            self.assertIn(f'id="{ident}"', self.world)
        self.assertIn("Every zone counts as satanic", self.world)
        for gone in ('id="satanic_follow"', 'id="szfval"', "Keep the zone you are in satanic"):
            self.assertNotIn(gone, self.world)

    def test_switches_are_enabled_mods_entries(self):
        # Off by default and sends its line while on, so the Enabled mods
        # list shows it like any other mod: the pools' exclusion (all-on by
        # default) does not apply to the switch (#157).
        body = re.search(r"BOOLEAN_MODS\s*=\s*\[(?P<body>.*?)\];", self.mods, re.DOTALL).group("body")
        self.assertIn("'satanic_everywhere'", body)
        self.assertNotIn("'satanic_follow'", body)

    def test_readme_documents_the_command(self):
        self.assertIn("satzone", self.readme)


class SatanicFollowRetiredTests(unittest.TestCase):
    """A saved `satanic_follow` (the removed "Keep the zone you are in
    satanic" switch, never in a release) loads, launches and sends nothing.
    The owner's own forgepact.json carries the key, so a file that still has
    it must not bring it back: `load_cfg` drops it, `build_cmds` emits no
    follow line, `/api/set` with it changes nothing and sends nothing, and the
    next save writes the file without it. Every zone counts as satanic, in
    the same file, is kept (the negative control)."""

    def _load_cfg_from(self, saved):
        from test_satanic_panel import forgepact  # noqa: E402  (imports the backend)
        import json
        import tempfile
        from unittest import mock
        with tempfile.TemporaryDirectory() as tmp:
            config = Path(tmp) / "forgepact.json"
            config.write_text(json.dumps(saved), encoding="utf-8")
            with mock.patch.object(forgepact, "CONFIG", config):
                cfg = forgepact.load_cfg()
                forgepact.save_cfg(cfg)
                written = json.loads(config.read_text(encoding="utf-8"))
            return cfg, written, forgepact

    def test_a_saved_satanic_follow_is_retired_on_load(self):
        cfg, written, forgepact = self._load_cfg_from({"satanic_follow": True, "satanic_everywhere": True})
        self.assertNotIn("satanic_follow", cfg)
        self.assertNotIn("satanic_follow", written)
        self.assertIs(cfg["satanic_everywhere"], True)
        cmds = forgepact.build_cmds(cfg)
        self.assertIn("satzone everywhere 1", cmds)
        self.assertEqual([c for c in cmds if "follow" in c], [])

    def test_retired_key_is_not_a_default(self):
        from test_satanic_panel import forgepact  # noqa: E402
        self.assertNotIn("satanic_follow", forgepact.DEFAULTS)
        self.assertIs(forgepact.DEFAULTS["satanic_everywhere"], False)

    def test_api_set_with_the_retired_key_changes_and_sends_nothing(self):
        import json
        from http.client import HTTPConnection
        from unittest import mock
        from test_satanic_panel import PanelSandbox, forgepact  # noqa: E402
        with PanelSandbox() as sandbox, mock.patch.object(forgepact, "game_running", return_value=True):
            send = sandbox.mocks[3]
            connection = HTTPConnection("127.0.0.1", sandbox.port, timeout=5)
            try:
                connection.request("POST", "/api/set", json.dumps({"key": "satanic_follow", "value": True}),
                                   {"Content-Type": "application/json"})
                response = connection.getresponse()
                response.read()
            finally:
                connection.close()
            saved = json.loads(sandbox.config.read_text(encoding="utf-8"))
            self.assertNotIn("satanic_follow", saved)
            sent = [line for call in send.call_args_list for line in call.args[0]]
            self.assertEqual([line for line in sent if "follow" in line], [])
            # Control: the kept switch still posts and sends its line.
            connection = HTTPConnection("127.0.0.1", sandbox.port, timeout=5)
            try:
                connection.request("POST", "/api/set", json.dumps({"key": "satanic_everywhere", "value": True}),
                                   {"Content-Type": "application/json"})
                connection.getresponse().read()
            finally:
                connection.close()
            sent = [line for call in send.call_args_list for line in call.args[0]]
            self.assertIn("satzone everywhere 1", sent)
