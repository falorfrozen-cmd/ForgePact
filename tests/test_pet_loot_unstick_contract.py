#!/usr/bin/env python3
"""Contract tests for "Pet moves on from loot it cannot pick up" (`petunstick`, #94).

The decision itself (same target, within reach, for kPetLootStuckFrames) is
compiled and exercised by test_pet_loot_unstick_behavior.py. These tests pin
the rest, which no harness can run: the mod is off by default and sends
nothing while off, the command reaches a player build, the tick runs only
while enabled, and the tick does what docs/pet-loot-stuck-research.md says and
nothing more - it writes `itemCompanionTimer` on a ground item (only after
`variable_instance_exists` says the item carries it), drops the pet's
`lootTarget` and clears its `lootList`, and never collects, destroys or calls
anything resolved by hand; and `petunstick 0` prints the fix-1 counters
(`timer absent=`, `re-picked while held=`) a bug report needs.
"""

import re
import sys
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
FORGEPACT_DIR = REPO_ROOT / "ForgePact"
SRC_DIR = FORGEPACT_DIR / "src"
PLUGIN_SRC = FORGEPACT_DIR / "plugin" / "ModuleMain.cpp"
HEADER = FORGEPACT_DIR / "plugin" / "include" / "ForgePact" / "PetLootUnstickMod.hpp"
SDK_PY_PATH = REPO_ROOT / "hs-game-sdk" / "python"

if str(SRC_DIR) not in sys.path:
    sys.path.insert(0, str(SRC_DIR))
if str(SDK_PY_PATH) not in sys.path:
    sys.path.insert(0, str(SDK_PY_PATH))

import forgepact  # noqa: E402

sys.path.insert(0, str(Path(__file__).resolve().parent))
from panel_source import panel_file  # noqa: E402
from test_release_hook_contract import function_body, strip_comments, strip_research_blocks  # noqa: E402


# The positive control on the timer's name: the answer of a
# variable_instance_exists call on "itemCompanionTimer", kept in a variable.
TIMER_EXISTS_CALL = re.compile(
    r'(?P<var>\w+)\s*=\s*g_Yytk->CallBuiltin\(\s*"variable_instance_exists"\s*,'
    r'\s*\{[^}]*RValue\("itemCompanionTimer"\)\s*\}\s*\)\s*\.ToBoolean\(\)')
TIMER_SET_CALL = re.compile(
    r'CallBuiltin\(\s*"variable_instance_set"\s*,\s*\{[^}]*RValue\("itemCompanionTimer"\)')


def braced_block(source: str, open_brace: int) -> tuple[int, int]:
    """(start, end) of the block whose `{` is at `open_brace`, brace-matched."""
    depth = 0
    for index in range(open_brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return open_brace, index
    raise AssertionError("unterminated block")


def timer_write_is_guarded_by_exists(body: str) -> bool:
    """True iff every variable_instance_set of "itemCompanionTimer" in `body`
    (there must be at least one) comes after a variable_instance_exists call on
    the same name and sits inside the braced then-block of `if (<its answer>)`.
    A negated condition, an `else` block, code after the block closes and an
    unbraced then-statement all count as outside. Fails closed: no exists call,
    or no write, is False. Without the check, a set on a name the instance
    lacks creates a stray variable and `held back=` counts a hold that holds
    nothing.
    """
    exists = TIMER_EXISTS_CALL.search(body)
    writes = [m.start() for m in TIMER_SET_CALL.finditer(body)]
    if not exists or not writes:
        return False
    guarded = []
    for guard in re.finditer(rf"if\s*\(\s*{re.escape(exists.group('var'))}\s*\)\s*\{{", body):
        if guard.start() < exists.end():
            continue
        guarded.append(braced_block(body, guard.end() - 1))
    return all(any(start < at < end for start, end in guarded) for at in writes)


def definition_body(source: str, name: str) -> str | None:
    """The body of a `std::string <name>()` definition, or None."""
    found = re.search(rf"std::string\s+{re.escape(name)}\(\)\s*(?:const\s*)?\{{", source)
    if not found:
        return None
    start, end = braced_block(source, found.end() - 1)
    return source[start + 1:end]


class PetLootUnstickBaselineTests(unittest.TestCase):
    """Mod off: nothing is sent, nothing runs."""

    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_SRC.read_text(encoding="utf-8")
        cls.backend = (SRC_DIR / "forgepact.py").read_text(encoding="utf-8")

    def test_default_is_off(self):
        self.assertIn("mod_pet_loot_unstick", forgepact.DEFAULTS)
        self.assertIs(forgepact.DEFAULTS["mod_pet_loot_unstick"], False)

    def test_build_cmds_sends_nothing_while_off(self):
        cfg = dict(forgepact.DEFAULTS)
        self.assertFalse([c for c in forgepact.build_cmds(cfg) if c.startswith("petunstick")])

    def test_tick_only_runs_while_enabled(self):
        frame = function_body(self.plugin, "void FrameCallback(FWFrame&")
        self.assertEqual(frame.count("PetLootUnstickTick();"), 1)
        gate = frame.split("PetLootUnstickTick();", 1)[0][-300:]
        self.assertIn("PetLootUnstickMod::Instance().IsEnabled()", gate)

    def test_header_starts_disabled(self):
        header = HEADER.read_text(encoding="utf-8")
        self.assertIn("std::atomic<bool> m_Enabled{ false };", header)


class PetLootUnstickTargetTests(unittest.TestCase):
    """Mod on: the command reaches the plugin and the tick does exactly the three writes."""

    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_SRC.read_text(encoding="utf-8")
        cls.backend = (SRC_DIR / "forgepact.py").read_text(encoding="utf-8")
        cls.header = HEADER.read_text(encoding="utf-8")
        cls.tick = function_body(cls.plugin, "static void PetLootUnstickTick()")

    def test_build_cmds_sends_the_command_when_on(self):
        cfg = dict(forgepact.DEFAULTS)
        cfg["mod_pet_loot_unstick"] = True
        self.assertIn("petunstick 1", forgepact.build_cmds(cfg))

    def test_api_set_handles_the_key(self):
        # The boolean key list and its own send_cmds branch, like
        # mod_pet_quest_pickup: without the first the switch never saves,
        # without the second it saves and the running game never hears it.
        bool_keys = re.search(r'elif key in \((?P<keys>[^)]*"mod_pet_quest_pickup"[^)]*)\):', self.backend)
        self.assertIsNotNone(bool_keys)
        self.assertIn('"mod_pet_loot_unstick"', bool_keys.group("keys"))
        self.assertIn('elif key == "mod_pet_loot_unstick":', self.backend)
        branch = self.backend.split('elif key == "mod_pet_loot_unstick":', 1)[1][:200]
        self.assertIn("petunstick {1 if cfg['mod_pet_loot_unstick'] else 0}", branch)

    def test_command_exists_once_and_is_a_player_command(self):
        # A release build silently rejects an unlisted command, so the switch
        # would look on and do nothing.
        self.assertEqual(self.plugin.count('lc == "petunstick"'), 1)
        self.assertIn('lc == "petunstick"', strip_research_blocks(self.plugin))
        allowlist = re.search(r"kPlayerCommands\s*=\s*\{(?P<body>.*?)\};", self.plugin, re.DOTALL)
        self.assertIsNotNone(allowlist)
        self.assertIn('"petunstick"', allowlist.group("body"))

    def test_command_off_prints_what_the_mod_did(self):
        branch = self.plugin.split('lc == "petunstick"', 1)[1][:900]
        self.assertIn("PetLootUnstickMod::Instance().SetEnabled(", branch)
        self.assertIn("StatLine()", branch)
        self.assertIn("petunstick stat: held back=", self.header)
        self.assertIn("coins released=", self.header)
        self.assertIn("longest same-target=", self.header)

    def test_tick_writes_the_three_names_the_game_already_reads(self):
        self.assertIn('"itemCompanionTimer"', self.tick)
        self.assertIn('"lootTarget"', self.tick)
        self.assertIn('"lootList"', self.tick)
        self.assertIn('"ds_list_clear"', self.tick)
        self.assertIn("kPetLootHoldFrames", self.tick)
        self.assertIn("RValue(-4.0)", self.tick)

    def test_tick_resolves_objects_by_name(self):
        resolve = function_body(self.plugin, "static void ResolvePetLootUnstickAssets()")
        for name in ("Companion_obj", "Loot_Ground_obj", "Coin_obj"):
            self.assertIn(f'"{name}"', resolve)
            self.assertIn(f"HeroSiege::Objects::GameObject::{name}", resolve)
        self.assertIn("asset_get_index", resolve)
        self.assertIn("ResolvePetLootUnstickAssets();", self.tick)

    def test_timer_is_written_only_on_a_ground_item(self):
        # A coin has no itemCompanionTimer; writing one would leave a stray
        # variable and make the held-back count lie.
        timer_at = self.tick.index('"itemCompanionTimer"')
        before = self.tick[:timer_at]
        self.assertIn("g_PetLootGroundObjIdx", before[before.rindex("if ("):])
        self.assertIn("NoteHeldBack()", self.tick)
        self.assertIn("NoteCoinReleased()", self.tick)
        self.assertIn("g_PetLootCoinObjIdx", self.tick)

    def test_timer_write_is_gated_on_the_name_existing(self):
        # fix-1: the name itemCompanionTimer comes from a static reading, so
        # the tick asks variable_instance_exists before it writes the hold,
        # and on "no" writes nothing to the item (counted as timer absent=).
        # On code only, so a comment naming the check cannot stand in for it.
        tick = strip_comments(self.tick)
        self.assertTrue(timer_write_is_guarded_by_exists(tick), tick)
        absent_at = tick.index("PetLootNoteTimerAbsent(")
        self.assertGreater(absent_at, TIMER_EXISTS_CALL.search(tick).end())

    def test_timer_guard_check_fails_without_the_guard(self):
        # Negative controls: the same assertion, on the real tick body with
        # the guard taken away three ways, must fail.
        tick = strip_comments(self.tick)
        exists = TIMER_EXISTS_CALL.search(tick)
        self.assertIsNotNone(exists)
        var = exists.group("var")
        no_exists_call = tick[:exists.start()] + f"{var} = true" + tick[exists.end():]
        self.assertFalse(timer_write_is_guarded_by_exists(no_exists_call))
        negated = re.sub(rf"if\s*\(\s*{re.escape(var)}\s*\)", f"if (!{var})", tick)
        self.assertNotEqual(negated, tick)
        self.assertFalse(timer_write_is_guarded_by_exists(negated))
        # A second write, before the check and outside any guard.
        unguarded = (tick[:exists.start()] + 'g_Yytk->CallBuiltin("variable_instance_set", '
                     '{ target, RValue("itemCompanionTimer"), RValue(1.0) });\n' + tick[exists.start():])
        self.assertFalse(timer_write_is_guarded_by_exists(unguarded))

    def test_off_line_carries_timer_absent_and_repicked_while_held(self):
        # `petunstick 0` is what a bug report quotes. The two fix-1 counters
        # (timer absent=, re-picked while held= with its ground/coin split)
        # must reach that line, today through PetLootLocalStatSuffix()
        # appended to FullStatLine() in the command block.
        plugin = strip_comments(self.plugin)
        header = strip_comments(self.header)
        after = plugin.split('lc == "petunstick"', 1)[1]
        start, end = braced_block(after, after.index("{"))
        block = after[start:end]
        printed = re.search(r"if\s*\(\s*!\s*enable\s*\)\s*Out\((?P<arg>.*?)\);", block, re.S)
        self.assertIsNotNone(printed, block)
        # Follow every no-argument string builder the line calls, and the ones
        # those call (FullStatLine() starts with StatLine()).
        line, pending, seen = printed.group("arg"), [printed.group("arg")], set()
        while pending:
            for name in re.findall(r"(\w+)\(\)", pending.pop()):
                if name in seen:
                    continue
                seen.add(name)
                body = definition_body(plugin, name) or definition_body(header, name)
                if body is not None:
                    line += body
                    pending.append(body)
        for field in ('" timer absent="', '" re-picked while held="', '" (ground "', '" coin "',
                      '"petunstick stat: held back="'):
            self.assertIn(field, line, field)

    def test_loot_list_is_cleared_only_when_it_is_a_list_id(self):
        clear_at = self.tick.index('"ds_list_clear"')
        guard = self.tick[:clear_at][-300:]
        self.assertRegex(guard, r">=\s*0")

    def test_tick_collects_destroys_and_hooks_nothing(self):
        for forbidden in ("PickupLoot", "instance_destroy", "instance_create", "ds_list_destroy",
                          "ds_list_create", "PetQuestCollectOne", "HookOneScript", "HookBuiltin",
                          "Rva", "GetModuleHandle"):
            self.assertNotIn(forbidden, self.tick, forbidden)

    def test_every_game_call_in_the_tick_is_guarded(self):
        # The neighbouring tick's rule: a throw from the runtime ends this
        # frame's work, never the frame.
        self.assertIn("try {", self.tick)
        self.assertIn("catch (...)", self.tick)

    def test_kind_is_not_the_gate(self):
        # lootTarget is written as a real; the tick reads it with ToDouble()
        # and asks instance_exists, never a VALUE_OBJECT/VALUE_REF kind check.
        self.assertIn('"instance_exists"', self.tick)
        self.assertNotIn("VALUE_OBJECT", self.tick)
        self.assertNotIn("VALUE_REF", self.tick)

    def test_header_declares_the_constants_and_the_classes(self):
        for name in ("kPetLootStuckFrames", "kPetLootStuckRadiusPx", "kPetLootHoldFrames"):
            self.assertRegex(self.header, rf"inline constexpr \w+ {name} = ")
        self.assertIn("class PetLootStuckWatch", self.header)
        self.assertIn("class PetLootUnstickMod", self.header)
        self.assertIn("#include <ForgePact/PetLootUnstickMod.hpp>", self.plugin)


class PetLootUnstickPanelTests(unittest.TestCase):
    """The Mods-tab row, which the panel item adds after this one lands.

    Tolerant of the row being absent only because the items land in that
    order; test_mods_categories.py requires the row itself, and the gate runs
    both.
    """

    def row_span(self):
        mods = panel_file("tabs/Mods.svelte")
        if 'id="mod_pet_loot_unstick"' not in mods:
            self.skipTest("the panel row for mod_pet_loot_unstick is added by the panel item")
        row = mods[:mods.index('id="mod_pet_loot_unstick"')]
        row = row[row.rindex('<div class="row"'):]
        label = re.search(r'<span class="lbl"[^>]*>([^<]*)<br><span [^>]*>(.*?)</span></span>', row)
        self.assertIsNotNone(label, row)
        return label.group(1), re.sub(r"<[^>]+>", "", label.group(2))

    def test_panel_text_is_short_and_names_no_coverage_figure(self):
        _title, text = self.row_span()
        self.assertLessEqual(len(text), 300, text)
        self.assertNotRegex(text, r"\d+\s*(%|px|frames?)", text)
        for word in ("measured", "static reading", "itemCompanionTimer", "lootTarget", "#94"):
            self.assertNotIn(word, text, word)


if __name__ == "__main__":
    unittest.main()
