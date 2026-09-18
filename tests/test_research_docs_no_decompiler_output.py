#!/usr/bin/env python3
"""Every ForgePact research doc stays free of decompiler output.

The hub's AGENTS.md "Legal: Decompiled Output Never Reaches Any Origin" allows
reading the game in Ghidra/IDA locally, but never lets the output reach a
tracked file: no decompiler-named functions or data, no disassembly, no byte
signatures, no transcribed script bodies. docs/S10-special-content-notes.md,
docs/dungeon-key-research.md and docs/angelic-drop-research.md once carried all
of those and were rewritten in our own words; this module keeps every doc
under docs/ from drifting back.

Two tiers:

* Universal - every docs/*.md, current and future: decompiler tokens,
  IDA-style sub_ names, disassembly lines, hex byte signatures. These have no
  innocent reading in a research note.
* Strict - the docs named in STRICT_DOCS, which were rewritten to the stricter
  standard: also no code addresses, no RVAs, no register names, no per-event
  byte sizes, no offset listings, no pseudo-code call forms and no numbered
  steps that follow a routine call by call. Other docs still carry
  interoperability addresses (hooked-script RVAs, measured function sizes)
  on purpose; add a doc here when it is rewritten to this standard.

Every banned token below is spelled in pieces, so this file never carries one
literally, and every category has a positive control (a known-bad sample the
matcher flags) beside a negative control (ordinary prose it must not flag), so
a matcher that silently stops matching fails here instead of passing every doc.
"""

import re
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
FORGEPACT_DIR = REPO_ROOT / "ForgePact"
DOCS_DIR = FORGEPACT_DIR / "docs"

# The seven tokens tests/test_prospect_window_contract.py uses on its branch,
# plus a Ghidra data-label prefix and the decompiler's pointer-cast call form.
DECOMPILER_TOKENS = (
    "FUN" "_14",
    "undefined" "8",
    "__fast" "call",
    "gml_push" "glb",
    "@@This" "@@",
    "uStack" "_",
    "param" "_1",
    "_DAT" "_1",
    "(code " "**)",
)

UNIVERSAL = {
    "decompiler token": re.compile("|".join(re.escape(t) for t in DECOMPILER_TOKENS)),
    "IDA sub_ name": re.compile(r"\bsub_[0-9a-fA-F]{5,}\b"),
    "disassembly": re.compile(
        r"(?i)\b(?:mov|movabs|movzx|lea|xorps|set(?:le|ge|l|g|e|ne)|test|cmp|je|jne|jz|jnz|jmp|call|nop)"
        r"\s+(?:(?:byte|word|dword|qword)\s+ptr\s+)?"
        r"(?:\[|0x[0-9a-f]|sub_|(?:[re]?[abcd]x|[re]?[sd]i|[re]?[sb]p|r(?:[89]|1[0-5])[dwb]?"
        r"|[abcd][lh]|[sd]il|xmm\d{1,2})\b)"
    ),
    # Four or more space-separated byte pairs, at least one with an A-F digit,
    # so a plain list of numbers ("12 25 15 14") is not a signature.
    "byte signature": re.compile(r"\b(?:[0-9A-F]{2} ){3,}[0-9A-F]{2}\b"),
}

STRICT = {
    "offset listing": re.compile(r"(?m)^\s*\+\d{2,5}\s|\(\+\d{3,5}[,)]|\+\d{3,5}\s+`?0x"),
    "code address": re.compile(r"\b0x[0-9a-fA-F]{5,}"),
    "RVA": re.compile(r"(?i)\brva\b"),
    "register name": re.compile(r"\b(?:rcx|rdx|rax|rbx|rsp|rbp|rdi|rsi|r8d|r9d|r9b|xmm\d|dil|sil)\b"),
    "event byte size": re.compile(
        r"\b(?:Create|Step|Alarm|Draw|Other)_\d+\s*=\s*\d+\s?B\b|\(\d{3,5} ?(?:B|bayt|bytes)[,)]"
        r"|uzunluk \d+|\b\d{1,2}\.\d{3} bayt\b"
    ),
    "pseudo-code": re.compile(
        r"\b(?:GPV|SPV)\(|event_inherited\(|alarm\[0\]\s*=|SetVariableToUndefined|(?i:\bif \(global)"
    ),
    "numbered call step": re.compile(r"(?m)^\s*\d+\.\s+[A-Za-z_][\w.\[\]]*\("),
}

STRICT_DOCS = (
    "S10-special-content-notes.md",
    "dungeon-key-research.md",
    "angelic-drop-research.md",
)

# One known-bad sample per category, built in pieces.
POSITIVE_SAMPLES = {
    "decompiler token": "the call goes through " + "FUN" "_1400" + "12345",
    "IDA sub_ name": "see " + "sub" "_" + "14ABCDEF0",
    "disassembly": "m" "ov " + "r" "cx, 1",
    "byte signature": "48 8B " + "C4 55 AA",
    "offset listing": "  +" + "123 " + "load the flag",
    "code address": "at 0x" + "14ABCDE",
    "RVA": "the R" "VA of it",
    "register name": "passed in r" "dx here",
    "event byte size": "Step" "_0 = 4" "12 B",
    "pseudo-code": "G" "PV(self, 3)",
    "numbered call step": "1. Some" "Script(x)",
}

NEGATIVE_SAMPLE = (
    "Call the hook, then test the gate: the counts were 12 25 15 14.\n"
    "Set the flag to 1 and let the game move on (a silent test, a daily call).\n"
    "3. Step the room and read the counter; +10 slots, 1.5 seconds.\n"
    "Stat 35 closes the gate, and 0xAF is a short value.\n"
)


def _hits(patterns, text):
    """Yield (category, line number, line) for every banned shape in text."""
    for name, rx in patterns.items():
        for m in rx.finditer(text):
            if name == "byte signature" and not re.search("[A-F]", m.group(0)):
                continue
            line_no = text.count("\n", 0, m.start()) + 1
            line = text.splitlines()[line_no - 1] if text.splitlines() else ""
            yield name, line_no, line.strip()


def _docs():
    return sorted(DOCS_DIR.glob("*.md"))


class ResearchDocsNoDecompilerOutputTests(unittest.TestCase):
    def test_glob_finds_every_research_doc(self):
        names = [p.name for p in _docs()]
        self.assertGreaterEqual(len(names), 10, names)
        for strict in STRICT_DOCS:
            self.assertIn(strict, names)

    def test_every_doc_is_free_of_universal_decompiler_shapes(self):
        failures = []
        for doc in _docs():
            text = doc.read_text(encoding="utf-8")
            for name, line_no, line in _hits(UNIVERSAL, text):
                failures.append(f"docs/{doc.name}:{line_no}: {name}: {line[:160]}")
        self.assertEqual(failures, [], "decompiler output in a research doc:\n" + "\n".join(failures))

    def test_rewritten_docs_meet_the_strict_standard(self):
        failures = []
        for name in STRICT_DOCS:
            text = (DOCS_DIR / name).read_text(encoding="utf-8")
            for cat, line_no, line in _hits(STRICT, text):
                failures.append(f"docs/{name}:{line_no}: {cat}: {line[:160]}")
        self.assertEqual(failures, [], "strict-tier shape in a rewritten doc:\n" + "\n".join(failures))

    def test_rewritten_docs_state_their_posture(self):
        # The pet-quest docs' convention: the doc says what it carries.
        for name in ("S10-special-content-notes.md", "dungeon-key-research.md"):
            self.assertIn("no decompiled script text", (DOCS_DIR / name).read_text(encoding="utf-8"), name)

    def test_positive_control_every_category_flags_its_sample(self):
        patterns = dict(UNIVERSAL, **STRICT)
        self.assertEqual(set(POSITIVE_SAMPLES), set(patterns))
        for name, sample in POSITIVE_SAMPLES.items():
            with self.subTest(category=name):
                found = {cat for cat, _, _ in _hits({name: patterns[name]}, sample)}
                self.assertEqual(found, {name}, sample)

    def test_negative_control_ordinary_prose_is_not_flagged(self):
        patterns = dict(UNIVERSAL, **STRICT)
        self.assertEqual(list(_hits(patterns, NEGATIVE_SAMPLE)), [])

    def test_negative_control_plain_number_list_is_not_a_byte_signature(self):
        self.assertEqual(list(_hits(UNIVERSAL, "measured 12 25 15 14 and 10 20 30 40")), [])

    def test_this_file_carries_no_token_literally(self):
        own = Path(__file__).read_text(encoding="utf-8")
        for token in DECOMPILER_TOKENS:
            self.assertNotIn(token, own)


if __name__ == "__main__":
    unittest.main()
