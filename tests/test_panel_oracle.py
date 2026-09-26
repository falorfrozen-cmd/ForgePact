#!/usr/bin/env python3
"""The panel's browser suites, run from the Python suite.

`panel/` is a Svelte + Vite project; what proves a build of it behaves like
the page it replaced lives in `panel/tests/`:

- `npm run oracle:replay` replays `panel/tests/behaviour-oracle.json` (every
  control's POST bodies and plugin commands, recorded from the old page)
  against the current build and fails on any difference, and with it
  `panel/tests/behaviour-oracle-gems.json` (the Gems of Incarnation controls,
  recorded from origin/main's last pre-port page, named by its `sourceRev`)
  and `panel/tests/behaviour-oracle-primeevil.json` (the Prime Evil Parts
  slider, recorded from origin/main's 1.4.7 page at 841c2db). Because the
  backend now sends the Prime Evil reset line in every full key reset, the
  legacy recording and the Gems supplement are replayed through
  `insertAddedKeys` (panel/tests/lib/oracle-relocate.mjs), which inserts
  exactly that line; the files themselves are never edited, and the tests
  below check that the inserted line is what the merged backend sends;
- `npm run e2e` runs the checks ported from the old agent-browser harnesses
  (saves, failures, filters, keyboard, install and launch paths);
- `npm run e2e:gems` checks the Gems of Incarnation controls' place, defaults,
  Enabled mods entries and mod filter list;
- `npm run e2e:polish` checks the owner's polish pass: the Mining Ore
  Multiplier label, one card per Mods mod, the theme on Setup, the plugin
  warning icons and their tooltips, the Modifiers separators, the helmet's
  accent, and an idle slider's note as a tooltip that moves no row.

All four drive the installed Edge headless through playwright-core against
`tests/panel_sandbox_server.py`, which needs a built `panel/dist/`. Each test
here skips, naming what is missing, when `node`/`npm`, Edge, the installed
dev dependencies or the build are absent - build first with
`npm --prefix panel ci` and `npm --prefix panel run build`. Together they
take a couple of minutes.
"""
import json
import os
import re
import shutil
import subprocess
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
EDGE_PATHS = (
    Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Microsoft" / "Edge" / "Application" / "msedge.exe",
    Path(os.environ.get("ProgramFiles", r"C:\Program Files")) / "Microsoft" / "Edge" / "Application" / "msedge.exe",
)


def _missing():
    """What the browser suites need and this machine lacks, or None."""
    if not shutil.which("node"):
        return "node is not on PATH"
    if not shutil.which("npm"):
        return "npm is not on PATH"
    if not any(p.is_file() for p in EDGE_PATHS):
        return "Microsoft Edge is not installed (playwright-core drives it through channel 'msedge')"
    if not (PANEL / "node_modules" / "playwright-core").is_dir():
        return "panel dev dependencies are not installed: run npm --prefix panel ci"
    if not (PANEL / "dist" / "index.html").is_file():
        return "panel/dist/index.html is missing: run npm --prefix panel run build"
    return None


def _npm(script):
    result = subprocess.run([shutil.which("npm"), "--prefix", str(PANEL), "run", script],
                            capture_output=True, text=True, encoding="utf-8", errors="replace",
                            timeout=900)
    return result.returncode, result.stdout + result.stderr


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


class PanelBrowserSuiteTests(unittest.TestCase):
    def setUp(self):
        missing = _missing()
        if missing:
            self.skipTest(missing)

    def test_behaviour_oracle_replays_with_no_mismatch(self):
        code, out = _npm("oracle:replay")
        lines = [l for l in out.splitlines() if l.startswith("oracle: ")]
        self.assertEqual(code, 0, out[-4000:])
        self.assertTrue(lines and re.fullmatch(r"oracle: \d+ steps, 0 mismatches", lines[-1]), out[-4000:])

    def test_e2e_suite_passes(self):
        code, out = _npm("e2e")
        lines = [l for l in out.splitlines() if l.startswith("e2e: ")]
        self.assertEqual(code, 0, out[-4000:])
        self.assertTrue(lines and re.fullmatch(r"e2e: (\d+)/\1 checks passed", lines[-1]), out[-4000:])

    def test_gems_e2e_suite_passes(self):
        code, out = _npm("e2e:gems")
        lines = [l for l in out.splitlines() if l.startswith("e2e-gems: ")]
        self.assertEqual(code, 0, out[-4000:])
        self.assertTrue(lines and re.fullmatch(r"e2e-gems: (\d+)/\1 checks passed", lines[-1]), out[-4000:])

    def test_polish_e2e_suite_passes(self):
        code, out = _npm("e2e:polish")
        lines = [l for l in out.splitlines() if l.startswith("e2e-polish: ")]
        self.assertEqual(code, 0, out[-4000:])
        self.assertTrue(lines and re.fullmatch(r"e2e-polish: (\d+)/\1 checks passed", lines[-1]), out[-4000:])


if __name__ == "__main__":
    unittest.main()
