"""Contract tests for auto-prospect on insert (ForgePact issue #9, Stage B).

The human chose auto-prospect on insert over a bigger prospect grid on
2026-09-18 (docs/prospect-window-research.md, § Decision gate). Its decision
core, plugin/include/ForgePact/AutoProspectMod.hpp, is pinned behaviourally by
test_auto_prospect_behavior.py. This file pins its shape: the core names no
runtime interface, so the harness compiles it whole and the adapter in
ModuleMain.cpp stays the only code that touches the game.

The adapter, the player command and the panel toggle wait on the Phase 1 live
measurement (research doc, § Stage B Phase 1 live procedure) that ForgePact can
invoke the Prospect handler at all; their tests land with them.
"""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "plugin" / "include" / "ForgePact" / "AutoProspectMod.hpp"


class AutoProspectContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.header = HEADER.read_text(encoding="utf-8")

    def test_core_header_is_game_independent(self):
        for forbidden in ("g_Yytk", "CallBuiltin", "Out(", "RValue"):
            self.assertNotIn(forbidden, self.header)
        includes = [l.strip() for l in self.header.split("\n") if l.strip().startswith("#include")]
        self.assertEqual(includes, ['#include "Common.hpp"'])
        # Off by default, and the hook-side entry point never decides to invoke.
        self.assertIn("std::atomic<bool> m_Enabled{ false };", self.header)
        self.assertIn("void OnInsert(int64_t nodeId, bool isProspectGrid, bool invoking)", self.header)


if __name__ == "__main__":
    unittest.main()
