"""Both builds detach from YYToolkit's console (#58, #44).

YYToolkit opens its "YYToolkit Log" console inside the game's own process, and
that console becomes the game's standard output, so GameMaker writes every
runtime warning and error to it synchronously. Underground Garden's zone
generation (entered from Misty Swamp) emits thousands of `tilemap_get()`
warnings, and writing them to the console froze the load for 30-70 seconds.
MEASURED 2026-09-23: the game's main thread sat in WriteFile for the whole
freeze, called from the game's own code, and the console buffer held nothing
but that warning. The unmodded game has no console, so there those writes fail
at once. Hiding the console window did not stop the writes; detaching does.

The player build has detached since #58; the research build kept its console
until 2026-10-02, when a research session (ForgePact #44, Live 1) froze in a
console flood of the runner's own `YYError` lines from `timer_system_update`
(observed by the owner in the console; none of it reached `out.txt`, so no
ForgePact print was involved). Nothing the research build prints needs the
console: every `Out()` line goes to `out.txt` first, and YYToolkit's own lines
go to `YYToolkit.log`. So the detach is no longer gated on FORGEPACT_RELEASE,
and the last two tests here pin that over the unstripped source.
"""
import unittest
from pathlib import Path

from test_release_hook_contract import function_body, strip_comments, strip_research_blocks

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"


def research_build_text(source: str) -> str:
    """What `build.bat dev` compiles, i.e. with FORGEPACT_RELEASE undefined.

    The mirror of `strip_research_blocks`: the `#ifdef FORGEPACT_RELEASE` half
    is dropped and the `#ifndef` / `#else` half kept. Other conditionals pass
    through untouched, with nesting tracked the same way.
    """
    kept = []
    stack = []
    for line in source.split("\n"):
        stripped = line.strip()
        if stripped.startswith("#ifdef FORGEPACT_RELEASE"):
            stack.append([True, False])
        elif stripped.startswith("#ifndef FORGEPACT_RELEASE"):
            stack.append([True, True])
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


def release_gated_lines(source: str, needle: str) -> list:
    """(line number, gated) for every code line containing `needle`.

    Walks the preprocessor lines and records, for each match, whether any
    enclosing conditional is about FORGEPACT_RELEASE (either spelling, either
    branch). Comments are dropped per line first, so prose naming the
    function never counts as a call.
    """
    hits = []
    stack = []
    in_block_comment = False
    for number, line in enumerate(source.split("\n"), start=1):
        code = line
        if in_block_comment:
            end = code.find("*/")
            if end < 0:
                continue
            code = code[end + 2:]
            in_block_comment = False
        code = strip_comments(code)
        if "/*" in code:
            code = code[:code.find("/*")]
            in_block_comment = True
        stripped = code.strip()
        if stripped.startswith("#if"):
            stack.append("FORGEPACT_RELEASE" in stripped)
        elif stripped.startswith("#endif") and stack:
            stack.pop()
        elif needle in code:
            hits.append((number, any(stack)))
    return hits


class ConsoleDetachContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = PLUGIN.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.player = strip_research_blocks(cls.source)
        cls.research = research_build_text(cls.source)

    def test_the_player_build_detaches_instead_of_hiding(self):
        body = strip_comments(function_body(self.player, "static void DetachConsole()"))
        self.assertIn("FreeConsole()", body)
        self.assertNotIn("ShowWindow", body)

    def test_module_initialize_detaches_in_the_player_build(self):
        init = strip_comments(function_body(self.player, "EXPORTED AurieStatus ModuleInitialize("))
        self.assertIn("DetachConsole();", init)
        self.assertNotIn("KonsoluGizle", self.player)

    def test_the_research_build_detaches_too(self):
        body = strip_comments(function_body(self.research, "static void DetachConsole()"))
        self.assertIn("FreeConsole()", body)
        init = strip_comments(function_body(self.research, "EXPORTED AurieStatus ModuleInitialize("))
        self.assertIn("DetachConsole();", init)

    def test_detach_is_not_release_gated(self):
        for needle in ("static void DetachConsole()", "DetachConsole();"):
            hits = release_gated_lines(self.source, needle)
            self.assertTrue(hits, f"{needle} not found in ModuleMain.cpp")
            gated = [number for number, is_gated in hits if is_gated]
            self.assertEqual(gated, [], f"{needle} sits inside a FORGEPACT_RELEASE block at line(s) {gated}")


if __name__ == "__main__":
    unittest.main()
