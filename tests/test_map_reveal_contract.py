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

sys.path.insert(0, str(Path(__file__).resolve().parent))
from panel_source import panel_file, panel_source


class TestMapRevealContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin_code = PLUGIN_SRC.read_text(encoding="utf-8")
        cls.panel_code = (SRC_DIR / "forgepact.py").read_text(encoding="utf-8")
        # The page itself (markup and script) is the Svelte project in panel/src.
        cls.page = panel_source()
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
        # the markers default on in the plugin, so nothing extra is needed
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
        cfg = dict(forgepact.DEFAULTS, map_reveal=False, map_reveal_packs=True, map_reveal_spawn=True)
        self.assertFalse([c for c in forgepact.build_cmds(cfg) if c.startswith("reveal")])

    # ---- the spawn pass is opt-in since 1.4.5 ------------------------------
    def test_spawn_pass_defaults_off_and_is_only_emitted_to_turn_on(self):
        # MEASURED 2026-09-22 (docs/population-performance-analysis.md): the
        # living monsters, not their birth, are what costs frame time, so the
        # markers are the default monster half and the real spawn pass is an
        # explicit second child that the plugin defaults off.
        self.assertIn("map_reveal_spawn", forgepact.DEFAULTS)
        self.assertFalse(forgepact.DEFAULTS["map_reveal_spawn"])
        cfg = dict(forgepact.DEFAULTS, map_reveal=True)
        self.assertNotIn("reveal spawn 0", forgepact.build_cmds(cfg))
        self.assertNotIn("reveal spawn 1", forgepact.build_cmds(cfg))
        cfg = dict(forgepact.DEFAULTS, map_reveal=True, map_reveal_spawn=True)
        cmds = forgepact.build_cmds(cfg)
        self.assertIn("reveal spawn 1", cmds)
        self.assertLess(cmds.index("reveal 1"), cmds.index("reveal spawn 1"))

    def test_plugin_defaults_spawn_off_and_markers_on(self):
        self.assertIn("bool m_Packs{ false };", self.header)
        self.assertIn("bool m_Marks{ true };", self.header)
        self.assertIn('v.rfind("spawn", 0) == 0', self.plugin_code)
        # `reveal packs` is the markers; the distance lie must only follow `reveal spawn`.
        idx = self.plugin_code.index('v.rfind("packs", 0) == 0')
        packs_branch = self.plugin_code[idx:self.plugin_code.index('v.rfind("spawn", 0) == 0')]
        self.assertIn("SetMarks", packs_branch)
        self.assertNotIn("InstallDistanceLieHook", packs_branch)

    def test_markers_ride_the_games_minimap_layer_after_it_drew(self):
        # The hook must let the game draw first and add markers only for the
        # monster family call - never replace or skip the game's own layer.
        idx = self.plugin_code.index("static RValue& Hook_DrawMinimapDynamic(")
        body = self.plugin_code[idx:idx + 1200]
        self.assertLess(body.index("g_Orig_DrawMinimapDynamic(S, O, R, argc, A)"), body.index("PackMarkerFamilyIsEnemy"))
        self.assertIn("markers.Draw(", body)
        self.assertIn('HookOneScript("DrawMinimapDynamic"', self.plugin_code)

    # ---- plugin surface ----------------------------------------------------
    def test_plugin_accepts_the_packs_subcommand(self):
        self.assertIn('v.rfind("packs", 0) == 0', self.plugin_code)

    def test_reveal_is_in_the_release_whitelist(self):
        # Anchored on the declaration, not the first mention: the name is
        # cited in comments elsewhere in the file, and a prose reference is
        # not the allowlist.
        block = self.plugin_code[self.plugin_code.index("kPlayerCommands = {"):][:2000]
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
        body = self.header[idx:]
        # the window may only open after a real timer value is seen
        self.assertIn("CreatorIsReady(inst)", body)
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
        self.assertIn("bool MayPopulate(const RValue& creator)", self.header)
        self.assertIn("static bool CreatorIsReady(const RValue& creator)", self.header)
        self.assertIn("enemyCreatorTimer", self.header[self.header.index("static bool CreatorIsReady"):][:600])

        hook = self.plugin_code[self.plugin_code.index("static void Hook_distance_to_object("):]
        hook = hook[: hook.index("\nstatic void InstallDistanceLieHook")]
        self.assertIn("CreatorIsReady(inst)", hook)
        self.assertIn("MayPopulate(inst)", hook)
        # The guard must precede the assignment it guards.
        self.assertLess(hook.index("CreatorIsReady(inst)"), hook.index("Result = RValue(0.0);"))
        self.assertLess(hook.index("MayPopulate(inst)"), hook.index("Result = RValue(0.0);"))

    def test_the_readiness_check_is_not_performed_twice(self):
        # MayPopulate IS `window > 0 && CreatorIsReady(creator)`, so a
        # standalone CreatorIsReady above it ran the same enemyCreatorTimer
        # read twice for every creator on an open pack window and decided
        # nothing new. The remaining occurrence is the Beacon branch's, which
        # MayPopulate does not cover. Call counts are asserted behaviourally
        # in test_map_reveal_behavior.py (ready_zone/timer_reads).
        hook = self.plugin_code[self.plugin_code.index("static void Hook_distance_to_object("):]
        hook = hook[: hook.index("\nstatic void InstallDistanceLieHook")]
        self.assertEqual(hook.count("MapRevealManager::CreatorIsReady("), 1, hook)

    def test_the_dead_ipc_poll_condition_is_gone(self):
        # g_RuntimeFrame = fc is assigned at the top of FrameCallback, before
        # the fc++ that gates the outer 30-frame test, so the inner
        # `% 6` test was always true when it was reached. The real rate is,
        # and always was, every 30 frames.
        self.assertNotIn("(g_RuntimeFrame % 6) == 0", self.plugin_code)

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
        setter = self.header[self.header.index("void SetPacks(bool on)"):]
        setter = setter[:setter.index("bool WantsPackSpawn()")]
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
        self.assertIn("if (n < 1) { m_ReadyProbeCursor = 0; return; }", body)
        # Do not cancel a pending pass just because the map loaded first.
        self.assertNotIn("if (n < 1) { m_PacksPending = false;", body)

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
    def test_both_controls_render_in_the_panel(self):
        self.assertIn('id="map_reveal"', self.page)
        self.assertIn('id="map_reveal_packs"', self.page)

    def test_child_row_is_disabled_while_parent_is_off(self):
        self.assertIn("syncRevealPacks", self.page)
        sync = panel_file("mods-sync.js")
        idx = sync.index("function syncRevealPacks")
        body = sync[idx:idx + 600]
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
        idx = self.page.index("document.getElementById('map_reveal').onchange")
        body = self.page[idx:idx + 900]
        self.assertLess(body.index("syncRevealPacks"), body.index("await j('/api/set'"))

    def test_description_explains_why_monsters_were_missing(self):
        # The honest bit: it is not a visibility flag, the packs do not exist.
        idx = self.page.index('id="map_reveal_packs_row"')
        row = self.page[idx:idx + 900]
        self.assertIn("do not exist", row)


def _function_body(source, signature):
    """The brace-matched body of the function declared by `signature`."""
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[brace + 1:index]
    raise AssertionError(f"unterminated body for {signature}")


def _strip_research_blocks(source):
    """What the player build compiles (FORGEPACT_RELEASE defined); the same
    nesting-aware evaluator as test_boss_rarity_contract.py's."""
    kept, stack = [], []
    for line in source.split("\n"):
        stripped = line.strip()
        if stripped.startswith("#ifdef FORGEPACT_RELEASE"):
            stack.append([True, True])
        elif stripped.startswith("#ifndef FORGEPACT_RELEASE"):
            stack.append([True, False])
        elif stripped.startswith("#if"):
            stack.append([False, True])
        elif stripped.startswith("#else") and stack:
            if stack[-1][0]:
                stack[-1][1] = not stack[-1][1]
        elif stripped.startswith("#endif") and stack:
            stack.pop()
        elif all(active for _, active in stack):
            kept.append(line)
    return "\n".join(kept)


def _strip_comments(source):
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.S)
    return re.sub(r"//[^\n]*", "", source)


class TestPackMarkerRetirementInstrument(unittest.TestCase):
    """Issue #181: the research build's `packmarks why|census|retire` and the
    stat line's `kinds=` field, whose exact text the live procedures match,
    and the create hook handing the created object to MarkSpawned."""

    @classmethod
    def setUpClass(cls):
        plugin = _strip_comments(PLUGIN_SRC.read_text(encoding="utf-8"))
        cls.plugin = plugin
        cls.command = _function_body(plugin, "static void PackMarksCommand(const std::string& rest)")
        cls.player_command = _strip_research_blocks(cls.command)
        cls.birth = _function_body(plugin, "static void PackMarkerBirth(")

    def _assert_in_order(self, text, fragments):
        at = 0
        for fragment in fragments:
            found = text.find(fragment, at)
            self.assertGreaterEqual(found, 0, f"{fragment!r} missing or out of order")
            at = found + len(fragment)

    def test_research_forms_exist_only_in_the_research_build(self):
        for verb in ('a1 == "why"', 'a1 == "census"', 'a1 == "retire"'):
            self.assertIn(verb, self.command)
            self.assertNotIn(verb, self.player_command)
        for text in ('"packmarks why "', '"packmarks census "', '"packmarks retire -> "', '" retire="', "Census(true)", "SetRetire("):
            self.assertIn(text, self.command)
            self.assertNotIn(text, self.player_command)

    def test_why_line_prefixes_are_exact(self):
        self._assert_in_order(self.command, [
            '"packmarks why "', '": listed="', '" marked="', '" spawned="', '" destroyed="',
            '" timergone="', '" givenup="', '" stateborn="', '" attributed="', '" age="',
            '".."', '" frame="',
        ])
        self._assert_in_order(self.command, ['"packmarks why "', '" creates: "', '"none"'])

    def test_why_names_created_objects_by_the_runtimes_object_get_name(self):
        start = self.command.index('a1 == "why"')
        why = self.command[start:self.command.index('a1 == "census"', start)]
        self.assertIn("PackMarksObjectName(", why)
        self.assertIn('"object_get_name"', _function_body(self.plugin, "static std::string PackMarksObjectName("))
        self.assertIn("TopCreates(", why)

    def test_census_line_prefixes_are_exact(self):
        self._assert_in_order(self.command, [
            '"packmarks census "', '": creators="', '" marked="', '" timer="', '" enemyArray="',
            '" lost="', '" stale="',
        ])

    def test_retire_answers_the_policy_and_accepts_both(self):
        start = self.command.index('a1 == "retire"')
        retire = self.command[start:start + 900]
        self.assertIn('"timer"', retire)
        self.assertIn('"state"', retire)
        self.assertIn('"packmarks retire -> "', retire)

    def test_stat_carries_kinds_in_both_builds(self):
        self._assert_in_order(self.player_command, ["KindCounts()", "kKindNames[", '" kinds="'])
        self.assertIn('":"', self.player_command)
        self.assertIn('","', self.player_command)

    def test_packmarks_stays_a_standalone_early_return(self):
        self.assertIn('if (lc == "packmarks") { PackMarksCommand(rest); return; }', self.plugin)

    def test_birth_passes_the_created_object(self):
        self.assertRegex(self.birth, r"const int created = \(int\)Args\[3\]\.ToDouble\(\);")
        self.assertRegex(
            self.birth,
            r"MarkSpawned\(\s*static_cast<int64_t>\(id\)\s*,\s*created\s*,\s*caller\.ObjectIndex\(\)\s*\)",
        )


class TestPackMarkerSecondInstrument(unittest.TestCase):
    """Issue #181, the second instrument (Live procedure 4 matches these lines
    exactly): `packmarks retire kind`, the new `why` fields and its `other
    creates:` line, the census getter line and fields, `packmarks census
    <kind>`, `packmarks creator <id>`, the stat line's ` unread=` in both
    builds; the room key handed to the markers; the members the create hooks
    record after the original call returns; and the research build's
    non-enemy creates, which the player build never reports."""

    @classmethod
    def setUpClass(cls):
        plugin = _strip_comments(PLUGIN_SRC.read_text(encoding="utf-8"))
        cls.plugin = plugin
        # The research helpers sit in one research-only block just above the
        # command; the region runs from that block's #ifndef to the command's
        # closing brace, so stripping it evaluates every directive in it.
        helper = plugin.index("static void PackMarksCreator(")
        start = plugin.rindex("#ifndef FORGEPACT_RELEASE", 0, helper)
        command = plugin.index("static void PackMarksCommand(const std::string& rest)")
        body = _function_body(plugin, "static void PackMarksCommand(const std::string& rest)")
        cls.region = plugin[start:plugin.index(body, command) + len(body)]
        cls.player_region = _strip_research_blocks(cls.region)
        cls.command = body
        cls.player_command = _strip_research_blocks(body)
        cls.birth = _function_body(plugin, "static void PackMarkerBirth(")
        cls.player_birth = _strip_research_blocks(cls.birth)
        cls.scope = _function_body(plugin, "struct PackMarkerBirthScope")
        cls.icd = _function_body(plugin, "static void HookICD(RValue& Result, CInstance* S, CInstance* O, int argc, RValue* Args)")
        cls.icl = _function_body(plugin, "static void HookICL(RValue& Result, CInstance* S, CInstance* O, int argc, RValue* Args)")

    def _assert_in_order(self, text, fragments):
        at = 0
        for fragment in fragments:
            found = text.find(fragment, at)
            self.assertGreaterEqual(found, 0, f"{fragment!r} missing or out of order")
            at = found + len(fragment)

    def _section(self, start, end):
        at = self.region.index(start)
        return self.region[at:self.region.index(end, at)]

    # ---- research forms, research build only --------------------------------
    def test_each_research_form_is_in_a_research_only_block(self):
        for text in (
            '"packmarks census getter: gDataProtected[177]="', '"packmarks creator "',
            '" kindborn="', '" packgone="', '" held="', '" unlinked="', '" remembered="',
            '" sameid="', '" other creates: "', '" born="', '" attributedUnborn="',
            '" spawnPack="', '" members="', '" members: "', '" near: "', '"packmarks retire -> "',
            '" protected: "', '" vars: "', '" spawners"',
        ):
            self.assertIn(text, self.region, text)
            self.assertNotIn(text, self.player_region, text)
        for verb in ('a1 == "creator"', "Census(true)", "CensusList(", "NoteOtherCreate("):
            self.assertNotIn(verb, self.player_region, verb)

    def test_stat_carries_unread_in_both_builds(self):
        self._assert_in_order(self.player_command, ['" kinds="', '" unread="', "Unread()"])
        self.assertNotIn('" retire="', self.player_command)
        self._assert_in_order(self.command, ['" kinds="', '" unread="', '" retire="'])

    def test_retire_accepts_kind(self):
        start = self.command.index('a1 == "retire"')
        retire = self.command[start:start + 1200]
        for text in ('"timer"', '"state"', '"kind"', "PM::Retire::Kind", '"packmarks retire -> "', "timer|state|kind"):
            self.assertIn(text, retire)

    def test_why_kind_line_is_exact(self):
        self._assert_in_order(self.command, [
            '"packmarks why "', '": listed="', '" marked="', '" spawned="', '" destroyed="',
            '" timergone="', '" givenup="', '" stateborn="', '" kindborn="', '" packgone="',
            '" held="', '" unlinked="', '" attributed="', '" remembered="', '" sameid="',
            '" age="', '".."', '" frame="',
        ])
        for text in ("ReasonKindBorn]", "ReasonPackGone]", "HeldNow()", ".remembered", ".sameid", ".unlinked"):
            self.assertIn(text, self.command)

    def test_why_other_creates_follow_the_creates_line(self):
        self._assert_in_order(self.command, [
            '"packmarks why "', '" creates: "', '"packmarks why "', '" other creates: "', '"none"',
        ])
        self.assertIn("TopOtherCreates(", self.command)

    def test_census_getter_line_reads_slot_177_through_both_getters_by_name(self):
        getter = _function_body(self.region, "static void PackMarksCensusGetter(")
        self._assert_in_order(getter, [
            '"packmarks census getter: gDataProtected[177]="', '" GPV="', '" wrapper="',
        ])
        self.assertIn('" -> proven"', getter)
        self.assertIn('" -> unproven"', getter)
        self.assertIn('GlobalArray("gDataProtected"', getter)
        self.assertIn("gml_Script_GPV", getter)
        self.assertIn("gml_Script_PC_GetVariableGMLWrapper", getter)
        # The key guard runs before either getter is called.
        self.assertLess(getter.index("KeyInRange("), getter.index("gml_Script_GPV"))
        self.assertLess(getter.index("KeyInRange("), getter.index("gml_Script_PC_GetVariableGMLWrapper"))
        # Proven only when both answer the same non-zero number.
        self.assertRegex(getter, r"!= 0\.0")
        # The getter line comes first in `packmarks census`.
        census = self._section('a1 == "census"', 'a1 == "retire"')
        self._assert_in_order(census, ["PackMarksCensusGetter()", "Census(true)"])

    def test_census_kind_lines_append_the_pack_state(self):
        self._assert_in_order(self.command, [
            '"packmarks census "', '": creators="', '" marked="', '" timer="', '" enemyArray="',
            '" lost="', '" stale="', '" born="', '" attributedUnborn="', '" spawnPack="',
        ])
        self.assertIn("SpawnPackTally(", self.command)

    def test_census_of_one_kind_lists_each_spawner(self):
        listing = _function_body(self.region, "static void PackMarksCensusKind(")
        self._assert_in_order(listing, [
            '"packmarks census "', '" #"', '": id="', '" x="', '" y="', '" marked="', '" born="',
            '" spawnPack="', '" enemyArray="', '" attributed="', '" members="', '"/"',
        ])
        self._assert_in_order(listing, ['"packmarks census "', '": "', '" spawners"'])
        for text in ("CensusList(", "PackText(", "kArrayTallyNames["):
            self.assertIn(text, listing)

    def test_creator_lines_are_exact(self):
        creator = _function_body(self.region, "static void PackMarksCreator(")
        self._assert_in_order(creator, [
            '"packmarks creator "', '": object="', '" kind="', '" x="', '" y="', '" exists="',
            '" attributed="',
        ])
        self._assert_in_order(creator, ['"="', '"->"', '" protected: "'])
        self._assert_in_order(creator, ['"alive="', '"/"', '" members: "'])
        self._assert_in_order(creator, ['" protected: "', '" members: "', '" near: "', '" vars: "'])
        self._assert_in_order(creator, ['"instance_nearest"', '" near: "', '"variable_instance_get_names"', '" vars: "'])
        for name in ("spawnPack", "zoneState", "destroySelf", "summoningPortal", "isWormhole", "eTyp",
                     "etherRoll", "enemySpawn", "loadAffixes", "specialType", "packType", "spawnAmount",
                     "new_enemy"):
            self.assertIn(f'"{name}"', creator, name)
        for text in ("PackMarksObjectName(", "ReadProtected(", "Members(", "CreatesOf(", "IsEnemyParentIndex()", '"none"'):
            self.assertIn(text, creator)
        # At most eight objects per members/near line, eight variables per vars line, values cut to 40.
        self.assertIn("8", creator)
        self.assertIn("40", creator)

    def test_packmarks_stays_a_standalone_early_return(self):
        self.assertIn('if (lc == "packmarks") { PackMarksCommand(rest); return; }', self.plugin)

    # ---- the room key -------------------------------------------------------
    def test_frame_callback_passes_the_room_key(self):
        self.assertRegex(
            self.plugin,
            r"marks\.OnFrame\(g_RuntimeFrame, reveal\.ZoneGeneration\(\), \[&reveal\] \{ return reveal\.HasReadableMap\(\); \}, CurrentRoomKey\(\)\)",
        )

    # ---- the birth hook -----------------------------------------------------
    def test_player_build_filters_non_enemy_creates_before_anything_else(self):
        first = self.player_birth.index("if (!IsEnemyObject(created)) return;")
        for later in ("IsCachedCreatorObject(", "InstanceIdOf(", "MarkSpawned("):
            self.assertLess(first, self.player_birth.index(later), later)
        self.assertNotIn("NoteOtherCreate(", self.player_birth)

    def test_research_build_reports_non_enemy_creates_before_the_enemy_filter_returns(self):
        other = self.birth.index("NoteOtherCreate(")
        self.assertLess(other, self.birth.index("if (!IsEnemyObject(created)) return;"))
        self.assertLess(self.birth.index("IsCachedCreatorObject("), other)
        self.assertNotIn("NoteOtherCreate(", self.player_birth)

    def test_members_are_read_from_the_result_after_the_original_call(self):
        destructor = self.scope[self.scope.index("~PackMarkerBirthScope()"):]
        self._assert_in_order(destructor, ["completed", "IsNumericInstanceRead(result)", "result.ToDouble()", "NoteMember("])
        self.assertIn("std::floor(id) == id", destructor)
        self.assertIn("PackMarkerBirth(", self.scope[:self.scope.index("~PackMarkerBirthScope()")])
        for hook, orig in ((self.icd, "g_OrigICD"), (self.icl, "g_OrigICL")):
            self.assertIn("PackMarkerBirthScope packBirth(Result, S, argc, Args, callerInfo);", hook)
            self.assertNotIn("    PackMarkerBirth(S, argc, Args, callerInfo);", hook)
            # Every route that runs the original marks the scope completed,
            # in both builds.
            for build in (hook, _strip_research_blocks(hook)):
                self.assertEqual(build.count("populationBirth.Completed();"), build.count("packBirth.Completed();"))
                self.assertGreater(build.count("packBirth.Completed();"), 0)
                self.assertEqual(build.count(f"if ({orig})"), build.count("packBirth.Completed();"))


if __name__ == "__main__":
    unittest.main()
