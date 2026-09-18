"""Contract tests for the Custom Forge hash-refresh fallback chain
(`RefreshItemHash` in `plugin/ModuleMain.cpp`).

Only `gml_Script_ItemCheckHash` has ever been observed to run: the chain
returns as soon as it succeeds, so the +method/+direct/+routine fallbacks
have never executed in a live session, `+direct`/`+routine` name a stale
`@anon@4638@` script absent from the current `hs-game-sdk` table, and
`+direct` reports success without checking that the hash actually changed.
These tests pin the source-level contract for making that chain
observable (per-route research commands) without changing what ships to
players. Source contracts rather than a compiled harness: whether
GenerateItemHash stores or returns its result is game behaviour a stub
would have to already know the answer to.

Reuses `function_body`/`strip_research_blocks`/`strip_comments` from
`test_release_hook_contract`, in the style of
`test_forgepact_notes_cleanup_workflow.py:12`. Does not import
`src/forgepact.py` (via that module's own tests) - its `panel_icons`
import fails outside a full `unittest discover` run.
"""

import re
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from test_release_hook_contract import (  # noqa: E402
    function_body,
    strip_comments,
    strip_research_blocks,
)


PROJECT_ROOT = Path(__file__).resolve().parents[1]
PLUGIN_PATH = PROJECT_ROOT / "plugin" / "ModuleMain.cpp"

# The 6630865 allowlist, confirmed against `ModuleMain.cpp` ~14703-14708.
# `test_player_commands_are_unchanged` pins this set exactly: this workorder
# adds no player command, and none should ever leave except on purpose.
EXPECTED_PLAYER_COMMANDS = {
    "ping", "density", "reveal", "specialrate", "dropmult",
    "stat", "statadd", "raredrop", "droprate", "dungeonkey",
    "headhunter", "hhdur", "hhmap", "hhdefault", "hhlabel", "tyrant",
    "beacon", "beaconrange", "beaconmode", "beaconwake", "beaconspawn",
    "beaconfarstep", "tyrantchance", "tyrantaffix", "hhlabelfont",
    "hhlabeloffset", "hhlabelmax", "enemyspeed", "rarity", "sigdrop",
    "angelicdrop", "relicfilter", "orbpickup", "satmods", "petquest",
    # The Soul Spurn/Purgatory outline (issue #11, Track B), added alongside.
    "toggleborder",
    # The Soul Spurn double-cast re-cast guard (issue #11, Track A).
    "toggleguard",
}


class ForgeHashBaselineTests(unittest.TestCase):
    """Must pass unmodified, and keep passing after the fallback chain is
    made observable."""

    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_PATH.read_text(encoding="utf-8")

    def test_hashprobe_is_research_only(self):
        self.assertIn('lc == "hashprobe"', self.plugin)
        self.assertNotIn('lc == "hashprobe"', strip_research_blocks(self.plugin))

    def test_hashprobe_is_not_a_player_command(self):
        allowlist = re.search(r"kPlayerCommands\s*=\s*\{(?P<body>.*?)\};",
                               self.plugin, re.DOTALL)
        self.assertIsNotNone(allowlist)
        self.assertNotIn("hashprobe", allowlist.group("body"))

    def test_hash_miss_counter_is_incremented_in_the_player_path(self):
        body = strip_research_blocks(
            function_body(self.plugin, "static bool TryApplyCustomForge(")
        )
        self.assertIn("InterlockedIncrement(&g_CustomForgeHashMisses)", body)

    def test_itemcheckhash_is_tried_before_the_generateitemhash_fallbacks(self):
        # ItemCheckHash is the one route proven live since v1.3.13; the chain
        # must keep trying it first, both before and after the route split.
        body = function_body(self.plugin, "static bool RefreshItemHash(")
        itemcheck_at = body.find("ItemCheck")
        fallback_at = re.search(r"GenerateItemHash|HashRouteDirect", body)
        self.assertNotEqual(itemcheck_at, -1)
        self.assertIsNotNone(fallback_at)
        self.assertLess(itemcheck_at, fallback_at.start())

    def test_player_commands_are_unchanged(self):
        allowlist = re.search(r"kPlayerCommands\s*=\s*\{(?P<body>.*?)\};",
                               self.plugin, re.DOTALL)
        self.assertIsNotNone(allowlist)
        found = set(re.findall(r'"([^"]+)"', allowlist.group("body")))
        self.assertEqual(found, EXPECTED_PLAYER_COMMANDS)


class ForgeHashTargetTests(unittest.TestCase):
    """Fail today; pass once the fallback chain is split into named routes,
    +direct is pointed at the SDK constant and checks its own result, and a
    research-only `hashprobe`/`forgehash` surface can run and report on one
    route at a time."""

    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_PATH.read_text(encoding="utf-8")

    def test_stale_anon_script_name_is_gone(self):
        self.assertNotIn("@anon@4638@", self.plugin)
        self.assertNotIn('"gml_Script_GenerateItemHash@', self.plugin)

    def test_sdk_constant_replaces_the_stale_literal(self):
        # 4645 named the pre-patch game build's closure; the current build's
        # hs-game-sdk (regenerated after "move closure names to the current
        # game's hs-game-sdk") spells it 4791.
        occurrences = self.plugin.count(
            "gml_Script_GenerateItemHash_anon_4791_s_ItemInstanceStruct_InventoryV2Funcs"
        )
        self.assertGreaterEqual(occurrences, 2)

    def test_direct_route_refuses_when_the_hash_did_not_change(self):
        body = function_body(self.plugin, "static bool HashRouteDirect(")
        self.assertIn("+direct-nohash", body)
        read_at = body.find("ReadItemHash(")
        call_at = body.find("CallGameScriptEx(")
        self.assertNotEqual(read_at, -1)
        self.assertNotEqual(call_at, -1)
        self.assertLess(read_at, call_at)
        self.assertRegex(body, r"after\s*==\s*before")

    def test_route_helpers_do_not_touch_the_shared_counters(self):
        # +routine has no HashRoute* helper of its own: it stays inlined in
        # RefreshItemHash so tests.test_release_hook_contract's
        # test_routine_fallback_validates_the_pointer_before_calling_it can
        # keep finding the AddrIsExecutableInModule guard literally inside
        # RefreshItemHash's own body (a separate HashRouteRoutine would still
        # run the guard, but that test would no longer be looking at it).
        # RefreshItemHash's own body is checked alongside the three real
        # helpers so the routine fallback's inlining doesn't get a pass on
        # this contract just because it isn't a named helper.
        for helper in (
            "static bool HashRouteItemCheck(",
            "static bool HashRouteMethod(",
            "static bool HashRouteDirect(",
            "static bool RefreshItemHash(",
        ):
            body = function_body(self.plugin, helper)
            self.assertNotIn("g_CustomForgeHashMisses", body, helper)
            self.assertNotIn("g_ForgeHashVia", body, helper)

    def test_hashprobe_can_run_a_single_route(self):
        branch = self.plugin.split('lc == "hashprobe"', 1)[1].split(
            'lc == "enemyvars"', 1
        )[0]
        self.assertIn("HashRouteDirect(", branch)
        self.assertIn("HashRouteItemCheck(", branch)
        # +routine's probe drives RefreshItemHash directly with routineOnly
        # set, rather than a HashRouteRoutine(...) call - see the comment on
        # test_route_helpers_do_not_touch_the_shared_counters above.
        self.assertIn("RefreshItemHash(", branch)
        self.assertIn("routineOnly", branch)
        self.assertIn('"hashprobe-sentinel"', branch)

    def test_forgehash_stat_reports_the_counters(self):
        body = function_body(self.plugin, "static void ForgeHashStats(")
        self.assertIn("g_CustomForgeHashMisses", body)
        self.assertIn("g_ForgeHashDirectNoHash", body)

    def test_forgehash_stat_labels_direct_nohash_as_unchanged_not_a_miss(self):
        # direct-nohash also fires when +direct correctly re-stored a hash the
        # forge pass never actually changed (nothing added this pass, or a
        # refresh that already ran once), not only when a route silently
        # failed to write - a reviewer finding on the first version of this
        # counter, which just printed the bare number next to the others.
        body = function_body(self.plugin, "static void ForgeHashStats(")
        self.assertIn("unchanged", body)
        self.assertIn("stored nothing, or hash already current", body)

    def test_hashprobe_single_route_line_leads_with_the_wrote_verdict(self):
        # Reviewer finding: after the sentinel write, itemcheck and routine
        # report success (`ok`) on ANY non-empty hash - including the
        # sentinel surviving untouched - so printing `ok`/`refused` straight
        # from the route's own return is meaningless there. The printed line
        # must lead with a verdict derived from `wrote`, and keep the route's
        # own answer visible separately, explicitly labelled.
        branch = self.plugin.split('lc == "hashprobe"', 1)[1].split(
            'lc == "enemyvars"', 1
        )[0]
        self.assertIn('(wrote ? "wrote" : "refused")', branch)
        self.assertIn('route-said=" + (ok ? "ok" : "refused")', branch)
        # The old shape - the route's own `ok` printed as the leading verdict
        # with no route-said= label at all - must be gone, not just amended
        # to sit alongside a new field.
        self.assertNotIn('": " + (ok ? "ok" : "refused") + " via "', branch)

    def test_hashprobe_restores_a_non_string_original_as_undefined_not_empty_string(self):
        # An item whose itemDataHash was never a string (pre-first-hash,
        # genuinely undefined) reads back as "" from ReadItemHash the same as
        # a real empty string would - restoring RValue(orig) after the
        # sentinel write would turn "never set" into "explicitly set to
        # empty string". The restore must use the raw RValue read before the
        # sentinel overwrite, not a string reconstructed from ReadItemHash().
        branch = self.plugin.split('lc == "hashprobe"', 1)[1].split(
            'lc == "enemyvars"', 1
        )[0]
        code = strip_comments(branch)
        raw_read_at = code.find('variable_struct_get')
        sentinel_write_at = code.find('"hashprobe-sentinel"')
        restore_at = code.rfind('variable_struct_set')
        self.assertNotEqual(raw_read_at, -1)
        self.assertNotEqual(sentinel_write_at, -1)
        self.assertLess(raw_read_at, sentinel_write_at)
        self.assertIn("origRaw", code[restore_at:])
        self.assertNotIn("RValue(orig)", code)

    def test_release_build_gains_no_hash_diagnostics(self):
        # Fails today: `lc == "forgehash"` does not exist yet.
        self.assertIn('lc == "forgehash"', self.plugin)
        self.assertIn('lc == "hashprobe"', self.plugin)
        released = strip_comments(strip_research_blocks(self.plugin))
        for marker in (
            '"forgehash"',
            "ForgeHashStats",
            "g_ForgeHashVia",
            "g_ForgeHashDirectNoHash",
            '"hashprobe-sentinel"',
        ):
            self.assertNotIn(marker, released, marker)


if __name__ == "__main__":
    unittest.main()
