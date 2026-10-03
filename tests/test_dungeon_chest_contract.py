"""Dungeon chest opens early (issue #31): the backend, the panel's words and
the command contract.

Two config keys, in the shape of Monster Density's `density_on` / `density`
pair: `mod_dungeon_chest` (the switch, default off) and `dungeon_chest_pct`
(the share of a key dungeon's monsters to kill before its end chest opens,
an integer 50..95, default 75). The backend turns them into
`dungeonchest <pct>` while the switch is on and `dungeonchest off` when a
live switch is turned off. The startup list (`build_cmds`) carries the line
only while the switch is on, so the all-off list stays empty. A percentage
moved while the switch is off is saved and sends nothing: 75 is only the
slider's resting position. The backend never sends `dungeonchest countdown`;
the plugin's default form applies (plan D8).

The baseline tests pin today's behaviour at defaults; the target tests pin
the switch on, a moved percentage, the switch off and refused values. The
live `/api/set` cases run against `PanelSandbox` through
`test_slider_switches.LiveSandbox` (isolated settings under its own temp dir,
a server on port 0, `send_cmds` captured, never the game's IPC).

`test_panel_text_states_behaviour_without_overclaim` keeps the panel's words
to what the control does until a live session measured more: the row's text
may not say `damage`, `drops` or `XP` until `docs/dungeon-chest-research.md`'s
`## Live procedure 2` section records that word as measured - a line in that
section naming the word and the word `measured`, and saying neither
`not measured` nor `not observed` (the mechanism of
`test_boss_rarity_panel.test_panel_hint_states_behaviour_without_overclaim`).

Page pins read `panel/src` through `panel_source.py`, never a built file.

`DungeonChestPluginContractTests` reads `plugin/ModuleMain.cpp`, which
another lane of this workorder writes: `dungeonchest` in `kPlayerCommands`
and `dungeonprobe` only inside `#ifndef FORGEPACT_RELEASE` blocks. Until that
lane's work is in the tree those two tests fail, by design; they pass once
the plugin side lands.
"""
import copy
import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import forgepact
from panel_source import panel_file
from test_slider_switches import LiveSandbox

RESEARCH_DOC = ROOT / "docs" / "dungeon-chest-research.md"
README = ROOT / "README.md"
RELEASE_NOTES = ROOT / "release-notes-v2.2.0.md"
PLUGIN_SRC = ROOT / "plugin" / "ModuleMain.cpp"

ROW_LABEL = "Dungeon chest opens early"
README_ANCHOR = "dungeon-chest-opens-early"

# The words that describe a measured effect, each with the pattern that finds
# it in the panel text and in the research doc.
MEASUREMENT_WORDS = {
    "damage": re.compile(r"damage", re.I),
    "drops": re.compile(r"drops?\b", re.I),
    "XP": re.compile(r"\bxp\b|experience", re.I),
}


def dungeon_chest_row():
    """The `.row` in Mods.svelte's #gameplayCard that holds the switch."""
    source = panel_file("tabs/Mods.svelte")
    at = source.find('id="mod_dungeon_chest"')
    if at < 0:
        raise AssertionError("Mods.svelte has no #mod_dungeon_chest")
    start = source.rindex('<div class="row"', 0, at)
    card = source.rindex('id="gameplayCard"', 0, start)
    if source.find('<div class="card', card, start) >= 0:
        raise AssertionError("#mod_dungeon_chest is not in #gameplayCard")
    end = source.find("</div>", at)
    return source[start:end + len("</div>")]


def feature_description(row):
    match = re.search(r'<span class="feature-description">(.*?)</span>', row, re.S)
    if not match:
        raise AssertionError("the dungeon chest row has no feature-description")
    return re.sub(r"\s+", " ", re.sub(r"<[^>]+>", " ", match.group(1))).strip()


def measured_words(doc=None):
    """The MEASUREMENT_WORDS the research doc's Live procedure 2 marks measured."""
    if doc is None:
        if not RESEARCH_DOC.exists():
            return set()
        doc = RESEARCH_DOC.read_text(encoding="utf-8")
    match = re.search(r"^## Live procedure 2[^\n]*\n(.*?)(?=^## |\Z)", doc, re.S | re.M)
    if not match:
        return set()
    out = set()
    for line in match.group(1).splitlines():
        low = line.lower()
        if "measured" not in low or "not measured" in low or "not observed" in low:
            continue
        out.update(word for word, pattern in MEASUREMENT_WORDS.items() if pattern.search(line))
    return out


def strip_research_blocks(source):
    """What the player build compiles (FORGEPACT_RELEASE defined).

    The same nesting-aware evaluator as `test_boss_rarity_contract.py`'s
    helper of that name, duplicated locally as this suite does elsewhere.
    """
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


def strip_comments(source):
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.S)
    return re.sub(r"//[^\n]*", "", source)


def defaults(**patch):
    return dict(copy.deepcopy(forgepact.DEFAULTS), **patch)


class DungeonChestBaselineTests(unittest.TestCase):
    def test_baseline_defaults_are_off_at_75(self):
        self.assertIs(forgepact.DEFAULTS["mod_dungeon_chest"], False)
        self.assertEqual(forgepact.DEFAULTS["dungeon_chest_pct"], 75)

    def test_baseline_default_emits_no_dungeonchest(self):
        cmds = forgepact.build_cmds(defaults())
        self.assertEqual(cmds, [])
        self.assertFalse(any(c.startswith("dungeonchest") for c in cmds))

    def test_baseline_switch_off_never_emits_whatever_the_percentage(self):
        for pct in (50, 75, 90, 95):
            with self.subTest(pct=pct):
                cmds = forgepact.build_cmds(defaults(dungeon_chest_pct=pct))
                self.assertFalse(any(c.startswith("dungeonchest") for c in cmds))

    def test_baseline_not_a_slider_switch(self):
        # Its own switch, as density_on is density's: switching it off sends
        # `dungeonchest off`, not what the slider sends at its minimum.
        self.assertNotIn("mod_dungeon_chest", forgepact.SLIDER_SWITCH_IDS)
        self.assertNotIn("dungeon_chest_pct", forgepact.SLIDER_SWITCH_IDS)

    def test_baseline_page_shows_the_switch_unchecked_and_the_range_at_75(self):
        row = dungeon_chest_row()
        switch = re.search(r'<input type="checkbox" id="mod_dungeon_chest"[^>]*>', row)
        self.assertIsNotNone(switch)
        self.assertNotIn("checked", switch.group(0))
        rng = re.search(r'<input type="range" id="dungeon_chest_pct"[^>]*>', row)
        self.assertIsNotNone(rng)
        self.assertIn('value="75"', rng.group(0))
        self.assertRegex(row, r'<span class="val off"[^>]*>off</span>')


class DungeonChestTargetTests(unittest.TestCase):
    def test_target_switch_on_emits_the_percentage(self):
        self.assertIn("dungeonchest 75", forgepact.build_cmds(defaults(mod_dungeon_chest=True)))
        cmds = forgepact.build_cmds(defaults(mod_dungeon_chest=True, dungeon_chest_pct=80))
        self.assertEqual([c for c in cmds if c.startswith("dungeonchest")], ["dungeonchest 80"])

    def test_target_build_cmds_never_sends_a_countdown_form(self):
        cmds = forgepact.build_cmds(defaults(mod_dungeon_chest=True))
        self.assertFalse(any(c.startswith("dungeonchest countdown") for c in cmds))

    def test_target_live_on_percentage_off(self):
        live = LiveSandbox(self)
        code, body, sent = live.post(None, "mod_dungeon_chest", True)
        self.assertEqual((code, sent), (200, [["dungeonchest 75"]]))
        self.assertIs(live.saved()["mod_dungeon_chest"], True)
        code, body, sent = live.post(None, "dungeon_chest_pct", 80)
        self.assertEqual((code, sent), (200, [["dungeonchest 80"]]))
        self.assertEqual(live.saved()["dungeon_chest_pct"], 80)
        self.assertEqual(body["cfg"]["dungeon_chest_pct"], 80)
        code, _, sent = live.post(None, "mod_dungeon_chest", False)
        self.assertEqual(code, 200)
        self.assertEqual(sent, [["dungeonchest off"]], "off must return the chest to the game's own rule")
        self.assertIs(live.saved()["mod_dungeon_chest"], False)
        self.assertEqual(live.saved()["dungeon_chest_pct"], 80, "off keeps the percentage")

    def test_target_percentage_while_off_is_stored_and_sends_nothing(self):
        live = LiveSandbox(self)
        code, _, sent = live.post(None, "dungeon_chest_pct", 90)
        self.assertEqual((code, sent), (200, []))
        self.assertEqual(live.saved()["dungeon_chest_pct"], 90)
        code, _, sent = live.post(None, "mod_dungeon_chest", True)
        self.assertEqual((code, sent), (200, [["dungeonchest 90"]]))

    def test_target_a_typed_value_rounds_to_an_integer(self):
        live = LiveSandbox(self)
        live.post(None, "mod_dungeon_chest", True)
        for value, stored in ((82, 82), (82.4, 82), (94.6, 95), (50.4, 50), (95.0, 95)):
            with self.subTest(value=value):
                code, _, sent = live.post(None, "dungeon_chest_pct", value)
                self.assertEqual((code, sent), (200, [[f"dungeonchest {stored}"]]))
                saved = live.saved()["dungeon_chest_pct"]
                self.assertEqual(saved, stored)
                self.assertIsInstance(saved, int)

    def test_target_out_of_range_or_not_a_number_is_refused(self):
        live = LiveSandbox(self)
        live.post(None, "mod_dungeon_chest", True)
        before = live.sandbox.config.read_bytes()
        for bad in (42, 96, 49.4, 95.6, "abc", "80", None, True, [80]):
            with self.subTest(value=bad):
                code, body, sent = live.post(None, "dungeon_chest_pct", bad)
                self.assertEqual(code, 400)
                self.assertEqual(body, {"err": "invalid dungeon chest percentage"})
                self.assertEqual(sent, [])
                self.assertEqual(live.sandbox.config.read_bytes(), before)

    def test_target_set_without_game_saves_and_sends_nothing(self):
        live = LiveSandbox(self)
        live.sandbox.mocks[1].return_value = False   # game_running
        code, _, sent = live.post(None, "mod_dungeon_chest", True)
        self.assertEqual((code, sent), (200, []))
        self.assertIs(live.saved()["mod_dungeon_chest"], True)


class DungeonChestPanelTextTests(unittest.TestCase):
    def test_the_row_is_a_switch_and_a_range_never_a_select(self):
        row = dungeon_chest_row()
        self.assertIn(ROW_LABEL, row)
        self.assertNotIn("<select", row)
        self.assertNotRegex(panel_file("tabs/Mods.svelte"), r'<select[^>]*id="[^"]*dungeon')
        self.assertIn('<label class="switch">', row)
        rng = re.search(r'<input type="range" id="dungeon_chest_pct"[^>]*>', row).group(0)
        for attr in ('min="50"', 'max="95"', 'step="5"', "aria-label=\"Share of the dungeon's monsters to kill\""):
            self.assertIn(attr, rng)
        switch = re.search(r'<input type="checkbox" id="mod_dungeon_chest"[^>]*>', row).group(0)
        self.assertIn(f'aria-label="{ROW_LABEL}"', switch)

    def test_the_value_is_typable_and_posted_on_change(self):
        js = panel_file("panel.js")
        self.assertIn("typable(dcp,document.getElementById('dcpval'))", js)
        self.assertIn("{key:'dungeon_chest_pct',value:v}", js)
        self.assertIn("{key:'mod_dungeon_chest',value:e.target.checked}", js)

    def test_panel_text_states_behaviour_without_overclaim(self):
        text = feature_description(dungeon_chest_row())
        self.assertLessEqual(len(text), 300, text)
        self.assertIn("off by default", text.lower())
        self.assertIn("50", text, "the countdown shows the last 50 kills")
        measured = measured_words()
        for word, pattern in MEASUREMENT_WORDS.items():
            if word in measured:
                continue
            with self.subTest(word=word):
                self.assertIsNone(
                    pattern.search(text),
                    f"the dungeon chest row says {word!r} before Live procedure 2 measured it")

    def test_measured_words_reads_only_measured_lines(self):
        # The relaxation, with a negative control beside it: a placeholder,
        # a "not observed" line and another section never count.
        self.assertEqual(measured_words("# T\n\n## Live procedure 2\n\nNot yet run.\n"), set())
        doc = ("# T\n\n## Live procedure 2\n\n"
               "- chest-xp: XP measured 2026-10-05\n"
               "- chest-drops: drops not observed live\n"
               "- chest-damage: damage not measured\n\n"
               "## Route\n\n- damage measured elsewhere\n")
        self.assertEqual(measured_words(doc), {"XP"})


class DungeonChestDocsTests(unittest.TestCase):
    def test_readme_row_and_details_section(self):
        readme = README.read_text(encoding="utf-8")
        row = next((line for line in readme.splitlines() if line.startswith(f"| **{ROW_LABEL}** |")), None)
        self.assertIsNotNone(row, "the mods table has no Dungeon chest opens early row")
        self.assertIn("Mods → Gameplay", row)
        self.assertIn("off by default", row.lower())
        self.assertIn(f"(#{README_ANCHOR})", row)
        self.assertRegex(readme, rf"(?m)^### {re.escape(ROW_LABEL)}\s*$")

    def test_release_notes_name_the_mod_under_new(self):
        notes = RELEASE_NOTES.read_text(encoding="utf-8")
        new = re.search(r"^## New\s*\n(.*?)(?=^## |\Z)", notes, re.S | re.M)
        self.assertIsNotNone(new)
        self.assertIn(f"**{ROW_LABEL}", new.group(1))


class DungeonChestPluginContractTests(unittest.TestCase):
    """Reads the plugin lane's file; fails until that lane's work is in."""

    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_SRC.read_text(encoding="utf-8", errors="replace")

    def test_dungeonchest_is_a_player_command(self):
        allowlist = re.search(r"kPlayerCommands\s*=\s*\{(?P<body>.*?)\};", self.plugin, re.S).group("body")
        self.assertIn('"dungeonchest"', allowlist)
        self.assertNotIn('"dungeonprobe"', allowlist)

    def test_dungeonprobe_stays_in_research_blocks(self):
        self.assertIn('"dungeonprobe"', self.plugin, "the research build has no dungeonprobe")
        self.assertNotIn('"dungeonprobe"', strip_comments(strip_research_blocks(self.plugin)))
        start = 0
        while True:
            idx = self.plugin.find('"dungeonprobe"', start)
            if idx < 0:
                break
            guard = self.plugin.rfind("#ifndef FORGEPACT_RELEASE", 0, idx)
            endif = self.plugin.rfind("#endif", 0, idx)
            self.assertGreater(guard, endif, '"dungeonprobe" used outside #ifndef FORGEPACT_RELEASE')
            start = idx + 1

    def test_dungeonprobe_names_global_resolution(self):
        # `gameCalls=` is told apart from the plugin's own global-self calls by
        # comparing against the resolved global instance; a null one would
        # count those calls as the game's. The probe has to say which it is
        # (Live procedure 1's `builtin-hook-fires` requires `global=resolved`),
        # and the line is research-only, like the rest of the probe.
        stripped = strip_research_blocks(self.plugin)
        for text in ("global=resolved", "global=unresolved"):
            self.assertIn(text, self.plugin, f"the probe never prints {text}")
            self.assertNotIn(text, stripped, f"{text} survives outside a research block")


if __name__ == "__main__":
    unittest.main()
