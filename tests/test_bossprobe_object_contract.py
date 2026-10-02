#!/usr/bin/env python3
"""Contract tests for `bossprobe <object index>` (issue #44, Live procedure 1b).

`bossprobe` with no argument reads every live boss. Live procedure 1b also has
to read an ordinary monster through the same instrument, so that a variable
whose rank-3/rank-1 and rank-4/rank-1 ratios match the measured rank table can
be named as the monster's damage or XP before a boss's ratio of it counts
(ForgePact/docs/boss-rarity-research.md). `bossprobe <object index>` does that:
the same control line, the same per-instance line and read rules, for every
live enemy whose `object_index` is that number, boss or not, then
`bossprobe: <k> instance(s) of <object name> among <total> enemies`.

These pins hold that the argument path exists and parses only a non-negative
whole number, that the no-argument path keeps its filter, line and summary,
and that neither form reaches the player build. Self-contained on purpose: the
helpers are duplicated here rather than imported from a sibling module, so the
plain `unittest` and the `discover -s tests` forms both load it.
"""

import re
import unittest
from pathlib import Path

FORGEPACT_DIR = Path(__file__).resolve().parents[1]
PLUGIN_SRC = FORGEPACT_DIR / "plugin" / "ModuleMain.cpp"


def slice_function(source, signature, next_signature):
    """Text from `signature` up to (not including) `next_signature`."""
    start = source.index(signature)
    end = source.index(next_signature, start)
    return source[start:end]


def strip_research_blocks(source):
    """What the player build compiles (FORGEPACT_RELEASE defined)."""
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


class TestBossProbeObjectContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_SRC.read_text(encoding="utf-8", errors="replace")
        cls.code = strip_comments(cls.plugin)
        cls.command = slice_function(cls.code, "static void BossProbeCommand(", "static void DropTraceNote(")
        cls.parse = slice_function(cls.code, "static bool BossProbeParseObjectIndex(", "static void BossProbeCommand(")
        cls.line = slice_function(cls.code, "static std::string BossProbeLine(", "static bool BossProbeParseObjectIndex(")
        cls.player_build = strip_comments(strip_research_blocks(cls.plugin))

    def test_bossprobe_takes_an_object_index(self):
        # The dispatcher hands the rest of the command line to the probe.
        self.assertIn('if (lc == "bossprobe") { BossProbeCommand(rest); return; }', self.code)
        self.assertIn("static void BossProbeCommand(const std::string& rest)", self.code)

        # Only a non-negative whole number is an object index: digits only,
        # a bounded length, nothing else. Anything else answers one usage
        # line and returns before the control line, so nothing is probed.
        self.assertIn("c < '0' || c > '9'", self.parse)
        self.assertIn("arg.empty()", self.parse)
        self.assertIn("arg.size() > 9", self.parse)
        self.assertNotIn("std::stoi", self.parse)
        usage = self.command.index('"bossprobe: usage')
        self.assertLess(self.command.index("BossProbeParseObjectIndex(arg, objIdx)"), usage)
        self.assertLess(usage, self.command.index("BossProbeGetterControl()"))
        self.assertEqual(self.command.count("BossProbeGetterControl()"), 1,
                         "both forms print the same control line, once")

        # The argument path matches an enemy's own object_index exactly, after
        # checking the read is a number (a ToDouble on `undefined` raises a
        # runner error, docs/boss-rarity-research.md), boss or not.
        self.assertIn("IsNumericInstanceRead(oi)", self.command)
        self.assertIn("(int)oi.ToDouble() != objIdx", self.command)
        # The same per-instance line and read rules as a boss's.
        self.assertEqual(self.command.count("BossProbeLine("), 2)
        self.assertIn('" instance(s) of "', self.command)
        self.assertIn('" among "', self.command)
        self.assertIn('"object_get_name"', self.command)

    def test_bossprobe_without_argument_is_unchanged(self):
        # The no-argument walk still keeps only what RarInstanceIsBoss accepts
        # and ends on the same summary.
        self.assertIn("if (!RarInstanceIsBoss(id)) continue;", self.command)
        self.assertIn('Out("bossprobe: " + std::to_string(bosses) + " boss(es) among " + std::to_string(total) + " enemies");',
                      self.command)
        self.assertIn('Out("bossprobe: EXC");', self.command)
        # The line format and its read rules, shared by both forms.
        self.assertIn('"bossprobe #" + std::to_string(n) + " " + TyInstName(id) + RarState(id) + " |"', self.line)
        self.assertIn("kBossProbeWords", self.line)
        self.assertIn("kBossProbeProvenKeys", self.line)
        self.assertIn('vars += "->not-key"', self.line)
        self.assertIn('vars += "->unread"', self.line)
        self.assertIn('std::string(proven ? "->" : "->?")', self.line)
        self.assertIn('" (no matching vars)"', self.line)
        # enemy_hp stays the one proven key; the filter words are not narrowed.
        self.assertIn('kBossProbeProvenKeys[] = { "enemy_hp" };', self.code)
        words = re.search(r"kBossProbeWords\[\]\s*=\s*\{(?P<w>[^}]*)\}", self.code).group("w")
        for w in ("hp", "health", "damage", "dmg", "exp", "slots", "chance", "dropmult", "droptable"):
            self.assertIn(f'"{w}"', words)
        # Walk cap unchanged.
        self.assertEqual(self.command.count("n < 2000"), 2)

    def test_bossprobe_stays_out_of_the_release_build(self):
        for name in ('lc == "bossprobe"', "BossProbeCommand", "BossProbeLine", "BossProbeParseObjectIndex",
                     "instance(s) of", "bossprobe: usage"):
            self.assertIn(name, self.plugin, f"{name} is missing from the research build")
            self.assertNotIn(name, self.player_build, f"{name} reaches the player build")
        allowlist = re.search(r"kPlayerCommands\s*=\s*\{(?P<body>.*?)\};", self.plugin, re.S).group("body")
        self.assertNotIn('"bossprobe"', allowlist)


if __name__ == "__main__":
    unittest.main()
