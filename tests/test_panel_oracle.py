#!/usr/bin/env python3
"""The panel's behaviour-oracle recordings, checked with no browser.

`panel/tests/behaviour-oracle.json` (every control's POST bodies and plugin
commands, recorded from the old page), `behaviour-oracle-gems.json` (the Gems
of Incarnation controls, from origin/main's last pre-port page) and
`behaviour-oracle-primeevil.json` (the Prime Evil Parts slider, from
origin/main's 1.4.7 page at 841c2db) are what `npm run oracle:replay` holds
the current build to. These tests pin where each was recorded from, that
their negative and positive controls are in them, and that the Prime Evil
line `insertAddedKeys` adds at replay is exactly what the merged backend
sends.

The browser suites themselves each run from a module of their own, so that
`tools/run_tests_parallel.py` can run them side by side:
`test_panel_oracle_replay.py` (`npm run oracle:replay`), `test_panel_e2e.py`,
`test_panel_e2e_gems.py`, `test_panel_e2e_polish.py`,
`test_panel_e2e_motion.py` and `test_panel_e2e_finish.py`, sharing their skip
conditions and npm helper from `panel_browser.py`; `test_panel_perf.py`
(`npm run e2e:perf`) runs last and alone.
"""
import json
import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PANEL = ROOT / "panel"
ORACLE = PANEL / "tests" / "behaviour-oracle.json"
SUPPLEMENT = PANEL / "tests" / "behaviour-oracle-gems.json"
DERIVED = PANEL / "tests" / "behaviour-oracle-derived.json"
PRIMEEVIL = PANEL / "tests" / "behaviour-oracle-primeevil.json"
#: origin/main's merge of PR #84: its last legacy page, with the Gems controls.
GEMS_SOURCE_REV = "f1e2f57edd60ffbed7ae82b7df087f0ca6b3da95"
#: origin/main at 1.4.7 plus "uber bosses drop none": its legacy page, with the Prime Evil Parts slider.
PRIMEEVIL_SOURCE_REV = "841c2db654b374400b47d25790b87c47a58d8461"
PRIMEEVIL_RANGE = 'input[type=range][data-sec="keys"][data-key="primeevil"]'


class BehaviourOracleFileTests(unittest.TestCase):
    """The recording itself: no browser needed."""

    def test_oracle_was_recorded_from_the_old_page_and_covers_every_control(self):
        oracle = json.loads(ORACLE.read_text(encoding="utf-8"))
        self.assertEqual(oracle["recordedFrom"], "legacy")
        self.assertGreaterEqual(len(oracle["steps"]), 120)
        for step in oracle["steps"]:
            self.assertIn("posts", step)
            self.assertIn("cmds", step)
        # Every kind of control the walk knows was exercised, with live commands.
        actions = {s["action"] for s in oracle["steps"]}
        for action in ("click", "max", "min", "increment", "decrement", "type", "select"):
            self.assertIn(action, actions)
        self.assertTrue(any(s["cmds"] for s in oracle["steps"]), "no step recorded a plugin command")
        # Negative control: a disabled control is recorded as such, not as a click.
        self.assertTrue(any(s["action"] == "skipped-disabled" for s in oracle["steps"]))

    def test_gems_supplement_was_recorded_from_mains_legacy_page(self):
        supplement = json.loads(SUPPLEMENT.read_text(encoding="utf-8"))
        # Provenance: the legacy page of a named tree, never this build.
        self.assertEqual(supplement["recordedFrom"], "legacy")
        self.assertEqual(supplement["sourceRev"], GEMS_SOURCE_REV)
        self.assertEqual(supplement["controls"], ["#mod_gem_mythic", "#mod_gem_maxroll", "#gemfilter_toggle"])
        steps = supplement["steps"]
        saves = [s for s in steps if s["control"].endswith('[data-gf="save"]')]
        # Negative control: saving with nothing ticked is refused in the page,
        # so the first save posts nothing and sends nothing.
        self.assertEqual((saves[0]["posts"], saves[0]["cmds"]), ([], []))
        # Positive control: the same button, once two mods are ticked, posts
        # them and the backend sends the filter.
        self.assertEqual(saves[1]["posts"], [{"url": "/api/set", "body": {"key": "gem_filter", "value": [68, 284]}}])
        self.assertEqual(saves[1]["cmds"], ["gemfilter 68,284"])
        self.assertTrue(any(s["cmds"] for s in steps if s["control"] == "#mod_gem_mythic"))
        # The derived oracle takes the gems' Turn off steps from it.
        derived = json.loads(DERIVED.read_text(encoding="utf-8"))
        self.assertEqual(derived["supplementFrom"], "tests/behaviour-oracle-gems.json")

    def test_primeevil_supplement_was_recorded_from_mains_legacy_page(self):
        supplement = json.loads(PRIMEEVIL.read_text(encoding="utf-8"))
        # Provenance: main's own 1.4.7 page, never this build.
        self.assertEqual(supplement["recordedFrom"], "legacy")
        self.assertEqual(supplement["sourceRev"], PRIMEEVIL_SOURCE_REV)
        self.assertEqual(supplement["controls"], [PRIMEEVIL_RANGE])
        steps = supplement["steps"]
        self.assertEqual([s["control"] for s in (steps[0], steps[-1])], ["tab:loot", "#applyall"])
        self.assertEqual([s["action"] for s in steps if s["control"] == PRIMEEVIL_RANGE],
                         ["max", "min", "increment", "decrement", "type"])
        # The max step: one POST, and the full key reset with the key at x100
        # exactly where the backend's KEYS puts it, between colosfrag and ruby.
        top = next(s for s in steps if s["control"] == PRIMEEVIL_RANGE and s["action"] == "max")
        self.assertEqual(top["posts"], [{"url": "/api/set", "body": {"section": "keys", "key": "primeevil", "value": 100}}])
        i = top["cmds"].index("droprate group primeevil 100")
        self.assertEqual(top["cmds"][i - 1:i + 2],
                         ["droprate group colosfrag 1", "droprate group primeevil 100", "droprate group ruby 1"])
        # Negative control: at x1 (the min step) the page sends the key at its default.
        low = next(s for s in steps if s["control"] == PRIMEEVIL_RANGE and s["action"] == "min")
        self.assertIn("droprate group primeevil 1", low["cmds"])
        self.assertNotIn("droprate group primeevil 100", low["cmds"])
        # The derived oracle takes the slider's switch steps from it; the Gems
        # file stays its `supplementFrom`.
        derived = json.loads(DERIVED.read_text(encoding="utf-8"))
        self.assertEqual(derived["keySupplementFrom"], "tests/behaviour-oracle-primeevil.json")
        self.assertEqual(derived["supplementFrom"], "tests/behaviour-oracle-gems.json")
        self.assertIn("#sw_keys_primeevil", derived["controls"])

    def test_legacy_key_resets_equal_the_merged_backend_plus_the_primeevil_line(self):
        # What insertAddedKeys adds at replay is exactly what the merged
        # backend sends: every full key reset the legacy page recorded equals
        # build_key_cmds of that step's key values plus primeevil at x1, with
        # the one line inserted after colosfrag and nothing else moved.
        sys.path.insert(0, str(ROOT / "src"))
        import forgepact  # noqa: E402  (only here: the other tests need no hs_game_sdk)
        oracle = json.loads(ORACLE.read_text(encoding="utf-8"))
        checked = 0
        for step in oracle["steps"]:
            cmds = step["cmds"]
            if "dungeonkey del 12" not in cmds:
                continue
            cfg = {m.group(1): int(m.group(2)) for c in cmds
                   for m in [re.fullmatch(r"droprate group (\w+) (\d+)", c)] if m}
            self.assertNotIn("primeevil", cfg, step["step"])
            k = next(j for j, c in enumerate(cmds) if c.startswith("droprate group colosfrag "))
            self.assertEqual(forgepact.build_key_cmds(dict(cfg, primeevil=1), include_resets=True),
                             cmds[:k + 1] + ["droprate group primeevil 1"] + cmds[k + 1:], step["step"])
            checked += 1
        self.assertEqual(checked, 70)


if __name__ == "__main__":
    unittest.main()
