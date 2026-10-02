"""The Bosses select (issue #44): the backend half and the panel's words.

`boss_rarity` is one of `off` / `rare` / `ancient` (default `off`), drawn as
the only control of the Mods tab's Gameplay sub-tab (`#gameplayCard`). The
backend turns it into `bossrarity <mode>`: the startup list (`build_cmds`)
carries it only when the mode is not `off`, so the all-off list stays empty,
while a live change through `/api/set` always sends it, `off` included,
because a live hook returns bosses to the game's own rarity only when told.

The baseline tests pin today's behaviour at defaults; the target tests pin the
raised modes. The live `/api/set` cases run against `PanelSandbox` (isolated
settings under its own temp dir, a server on port 0, `send_cmds` captured,
never the game's IPC).

`test_panel_hint_states_behaviour_without_overclaim` keeps the panel's words to
what the control does until a live session measured more: the Gameplay card's
text may not say `damage`, `drops` or `XP` until
`docs/boss-rarity-research.md`'s `## Live procedure 1` section records that
word as measured - a line in that section naming the word and the word
`measured`, and saying neither `not measured` nor `not observed`.
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

RESEARCH_DOC = ROOT / "docs" / "boss-rarity-research.md"

# The words that describe a measured effect, each with the pattern that finds
# it in the panel text and in the research doc.
MEASUREMENT_WORDS = {
    "damage": re.compile(r"damage", re.I),
    "drops": re.compile(r"drops?\b", re.I),
    "XP": re.compile(r"\bxp\b|experience", re.I),
}


def gameplay_card_text():
    """`#gameplayCard`'s markup in Mods.svelte: from its opening tag to the
    next Mods card, or the end of the file (it is the last card)."""
    source = panel_file("tabs/Mods.svelte")
    start = source.find('id="gameplayCard"')
    if start < 0:
        raise AssertionError("Mods.svelte has no #gameplayCard")
    start = source.rindex("<div", 0, start)
    nxt = source.find('<div class="card', start + 1)
    return source[start:nxt if nxt >= 0 else len(source)]


def measured_words(doc=None):
    """The MEASUREMENT_WORDS the research doc's Live procedure 1 marks measured."""
    if doc is None:
        if not RESEARCH_DOC.exists():
            return set()
        doc = RESEARCH_DOC.read_text(encoding="utf-8")
    match = re.search(r"^## Live procedure 1[^\n]*\n(.*?)(?=^## |\Z)", doc, re.S | re.M)
    if not match:
        return set()
    out = set()
    for line in match.group(1).splitlines():
        low = line.lower()
        if "measured" not in low or "not measured" in low or "not observed" in low:
            continue
        out.update(word for word, pattern in MEASUREMENT_WORDS.items() if pattern.search(line))
    return out


class BossRarityBaselineTests(unittest.TestCase):
    def test_baseline_default_emits_no_bossrarity(self):
        self.assertEqual(forgepact.DEFAULTS["boss_rarity"], "off")
        cmds = forgepact.build_cmds(copy.deepcopy(forgepact.DEFAULTS))
        self.assertEqual(cmds, [])
        self.assertFalse(any(c.startswith("bossrarity") for c in cmds))

    def test_baseline_off_or_missing_or_invalid_is_off(self):
        for cfg in ({}, {"boss_rarity": "off"}, {"boss_rarity": "uber"},
                    {"boss_rarity": None}, {"boss_rarity": 4}):
            with self.subTest(cfg=cfg):
                self.assertEqual(forgepact.boss_rarity_cmd(cfg), "bossrarity off")
                full = dict(copy.deepcopy(forgepact.DEFAULTS), **cfg)
                self.assertFalse(any(c.startswith("bossrarity") for c in forgepact.build_cmds(full)))

    def test_boss_rarity_is_not_a_slider_switch(self):
        self.assertNotIn("boss_rarity", forgepact.SLIDER_SWITCH_IDS)
        self.assertEqual(forgepact.BOSS_RARITY_VALUES, ("off", "rare", "ancient"))


class BossRarityTargetTests(unittest.TestCase):
    def test_target_ancient_emits_bossrarity_ancient(self):
        cfg = dict(copy.deepcopy(forgepact.DEFAULTS), boss_rarity="ancient")
        self.assertEqual(forgepact.build_cmds(cfg), ["bossrarity ancient"])
        cfg["boss_rarity"] = "rare"
        self.assertEqual(forgepact.build_cmds(cfg), ["bossrarity rare"])
        self.assertEqual(forgepact.boss_rarity_cmd({"boss_rarity": "ancient"}), "bossrarity ancient")

    def test_target_live_set_sends_and_saves_the_mode(self):
        live = LiveSandbox(self)
        for mode in ("ancient", "rare"):
            with self.subTest(mode=mode):
                code, body, sent = live.post(None, "boss_rarity", mode)
                self.assertEqual(code, 200)
                self.assertEqual(sent, [[f"bossrarity {mode}"]])
                self.assertEqual(live.saved()["boss_rarity"], mode)
                self.assertEqual(body["cfg"]["boss_rarity"], mode)

    def test_off_is_always_sent_explicitly(self):
        live = LiveSandbox(self)
        code, _, sent = live.post(None, "boss_rarity", "ancient")
        self.assertEqual((code, sent), (200, [["bossrarity ancient"]]))
        code, _, sent = live.post(None, "boss_rarity", "off")
        self.assertEqual(code, 200)
        self.assertEqual(sent, [["bossrarity off"]], "off must reach a live hook")
        self.assertEqual(live.saved()["boss_rarity"], "off")

    def test_set_without_game_saves_and_sends_nothing(self):
        live = LiveSandbox(self)
        live.sandbox.mocks[1].return_value = False   # game_running
        code, _, sent = live.post(None, "boss_rarity", "ancient")
        self.assertEqual((code, sent), (200, []))
        self.assertEqual(live.saved()["boss_rarity"], "ancient")

    def test_invalid_value_is_refused(self):
        live = LiveSandbox(self)
        before = live.sandbox.config.read_bytes()
        for bad in ("uber", "", "normal", "ancient uber", 3, None, True, ["rare"]):
            with self.subTest(value=bad):
                code, body, sent = live.post(None, "boss_rarity", bad)
                self.assertEqual(code, 400)
                self.assertEqual(body, {"err": "invalid boss rarity"})
                self.assertEqual(sent, [])
                self.assertEqual(live.sandbox.config.read_bytes(), before)


class BossRarityPanelTextTests(unittest.TestCase):
    def test_panel_hint_states_behaviour_without_overclaim(self):
        card = gameplay_card_text()
        text = re.sub(r"<[^>]+>", " ", card)
        self.assertIn('id="boss_rarity"', card)
        self.assertIn("Bosses", text)
        self.assertIn("off by default", text.lower())
        self.assertIn("left alone", text.lower(), "bosses the game made rare are left alone")
        for value in forgepact.BOSS_RARITY_VALUES:
            self.assertIn(f'<option value="{value}">', card)
        measured = measured_words()
        for word, pattern in MEASUREMENT_WORDS.items():
            if word in measured:
                continue
            with self.subTest(word=word):
                self.assertIsNone(
                    pattern.search(text),
                    f"the Gameplay card says {word!r} before Live procedure 1 measured it")

    def test_measured_words_reads_only_measured_lines(self):
        # The relaxation, with a negative control beside it: a placeholder,
        # a "not observed" line and another section never count.
        self.assertEqual(measured_words("# T\n\n## Live procedure 1\n\nNot yet run.\n"), set())
        doc = ("# T\n\n## Live procedure 1\n\n"
               "- ancient-damage: measured 2026-10-03, x1.4 damage\n"
               "- ancient-drops: drops not observed live\n"
               "- ancient-xp: XP not measured\n\n"
               "## Not verified\n\n- XP measured elsewhere\n")
        self.assertEqual(measured_words(doc), {"damage"})
        self.assertIsNone(MEASUREMENT_WORDS["drops"].search("a dropship"))


if __name__ == "__main__":
    unittest.main()
