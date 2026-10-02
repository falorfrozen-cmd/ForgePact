"""Source contract for Headhunter/Tyrant's Crown and the Angelic pool (#63, then #74).

Headhunter and Tyrant's Crown used to drop on their own standalone die
(`g_SigDropPct`/`g_SigDropAncientPct`/`g_SigDropPity`, alternating through
`g_SigDropNext`). #63 removed that die and put them into the pool
`angelicdrop`'s die picks from (`AppendSignatureCandidates`, called from
`BuildAngelicPool`, and a `SpawnSignatureItem` dispatch in
`AngelicDropOnKill`). #74 (owner-directed, 2026-10-02) took them out of that
pool again and moved them onto the game's own Angelic roll, switch-gated
(`test_angelic_hit_contract.py`), so the three #63 assertions below are now
their #74 inverses. The standalone die stays gone, and `sigdrop` survives as
a `crown | belt | off | status` test command (`g_SigDropForce`), not a rate.

`test_headhunter_dispatch.py` exercises this behaviourally, but skips
without a C++ toolchain; this file pins the same shape on the source text
(honouring `FORGEPACT_TEST_PLUGIN_SOURCE`, like the dispatch test, so it can
be checked against an older source too) so it is checked everywhere the
suite runs.
"""
import os
import pathlib
import re
import sys
import unittest

# Importable as `tests.test_signature_drop_contract` too, not only from discovery.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from panel_source import panel_file  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = pathlib.Path(os.environ.get("FORGEPACT_TEST_PLUGIN_SOURCE", ROOT / "plugin" / "ModuleMain.cpp")).read_text(encoding="utf-8")
# The Loot tab's markup (panel/src/tabs/Loot.svelte, or FORGEPACT_TEST_PANEL_SRC).
PANEL = panel_file("tabs/Loot.svelte")
# Scoped to the Angelic/Unholy card's own hint, not the whole panel - both names also
# appear, unrelated, in the Custom Forge Headhunter/Tyrant's Crown mechanic rows.
_ANGELIC_CARD = re.search(r'id="angelicCard".*?id="angelicnote"', PANEL, re.S)
ANGELIC_CARD_HINT = _ANGELIC_CARD.group(0) if _ANGELIC_CARD else ""

# The standalone signature die this change removes.
REMOVED_IDENTIFIERS = (
    "kSigDropAngelicPct",
    "g_SigDropPct",
    "g_SigDropAncientPct",
    "g_SigDropPity",
    "g_SigDropSinceLast",
    "g_SigDropNext",
)


def strip_comments(source):
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.S)
    return re.sub(r"//[^\n]*", "", source)


def body(source, signature):
    """Brace-matched definition; `rfind` so a forward declaration is skipped."""
    start = source.rfind(signature)
    if start < 0:
        raise AssertionError(f"not found: {signature}")
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    raise AssertionError(f"unterminated: {signature}")


class SignatureDropPoolContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = strip_comments(SOURCE)

    def test_the_standalone_die_is_gone(self):
        for identifier in REMOVED_IDENTIFIERS:
            self.assertNotIn(identifier, self.source, f"{identifier} should have been removed by #63")

    def test_build_angelic_pool_appends_no_signature_candidates(self):
        # #74 inverse of #63's "appends signature candidates".
        pool = body(self.source, "static void BuildAngelicPool(bool verbose)")
        self.assertNotIn("AppendSignatureCandidates", pool)
        self.assertNotIn("(signature)", pool)
        self.assertNotIn("AppendSignatureCandidates", self.source)

    def test_angelic_drop_on_kill_never_spawns_a_signature_item(self):
        # #74 inverse of #63's "spawns through SpawnSignatureItem".
        kill = body(self.source, "static void AngelicDropOnKill(CInstance* S)")
        self.assertNotIn("SpawnSignatureItem(", kill)
        self.assertIn("SpawnAngelicItem(", kill)

    def test_panel_hint_names_neither_item(self):
        # #74 inverse of #63's "hint names both": the slider no longer drops them.
        self.assertTrue(ANGELIC_CARD_HINT, "the angelicCard hint was not found")
        self.assertNotIn("Headhunter", ANGELIC_CARD_HINT, "the angelicCard hint should no longer name Headhunter (#74)")
        self.assertNotIn("Tyrant", ANGELIC_CARD_HINT, "the angelicCard hint should no longer name Tyrant's Crown (#74)")

    def test_sigdrop_stays_a_test_command(self):
        self.assertIn("g_SigDropForce", self.source)
        self.assertIn('lc == "sigdrop"', self.source)


if __name__ == "__main__":
    unittest.main()
