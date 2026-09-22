"""Source contract for Headhunter/Tyrant's Crown joining the Angelic pool (#63).

Headhunter and Tyrant's Crown used to drop on their own standalone die
(`g_SigDropPct`/`g_SigDropAncientPct`/`g_SigDropPity`, alternating through
`g_SigDropNext`). They now join the same pool `angelicdrop`'s die picks
from - appended by `AppendSignatureCandidates`, called from
`BuildAngelicPool` - and spawn through `SpawnSignatureItem` from
`AngelicDropOnKill` on a signature pick, so every setting that changes
Liquor Holster's chance changes theirs identically. `sigdrop` survives as a
`crown | belt | off | status` test command (`g_SigDropForce`), not a rate.

`test_headhunter_dispatch.py` exercises this behaviourally, but skips
without a C++ toolchain; this file pins the same shape on the source text
(honouring `FORGEPACT_TEST_PLUGIN_SOURCE`, like the dispatch test, so it can
be checked against the pre-#63 source too) so it is checked everywhere the
suite runs.
"""
import os
import pathlib
import re
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = pathlib.Path(os.environ.get("FORGEPACT_TEST_PLUGIN_SOURCE", ROOT / "plugin" / "ModuleMain.cpp")).read_text(encoding="utf-8")
PANEL = (ROOT / "src" / "forgepact.py").read_text(encoding="utf-8")
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

    def test_build_angelic_pool_appends_signature_candidates(self):
        pool = body(self.source, "static void BuildAngelicPool(bool verbose)")
        self.assertIn("AppendSignatureCandidates(", pool)

    def test_angelic_drop_on_kill_spawns_through_spawn_signature_item(self):
        kill = body(self.source, "static void AngelicDropOnKill(CInstance* S)")
        self.assertIn("SpawnSignatureItem(", kill)

    def test_panel_hint_names_both_items(self):
        self.assertIn("Headhunter", ANGELIC_CARD_HINT, "the angelicCard hint should name Headhunter (#63)")
        self.assertIn("Tyrant's Crown", ANGELIC_CARD_HINT, "the angelicCard hint should name Tyrant's Crown (#63)")


if __name__ == "__main__":
    unittest.main()
