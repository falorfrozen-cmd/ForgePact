"""Source contract for the kill drops' place in the kill hook.

The angelic (`angelicdrop`) and signature (`sigdrop`: Tyrant's Crown /
Headhunter) drops used to read the dying enemy and spawn their item, with that
enemy as `self`, *after* the game's own kill proc had run - i.e. after the
game's cleanup of that enemy. Since 1.4.3 both run before the trampoline, while
the enemy is still live, and nothing after the trampoline touches it.

`test_headhunter_dispatch.py` exercises this behaviourally, but skips without a
C++ toolchain; this file pins the same order on the source text so it is
checked everywhere the suite runs.
"""
import pathlib
import re
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "plugin" / "ModuleMain.cpp").read_text(encoding="utf-8")
ORIGINAL_CALL = "g_Orig_EnemyDestroyKillProc("
DROPS = ("SignatureDropOnKill", "AngelicDropOnKill")


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


def call_end(text, start):
    """Index just past the parenthesised argument list opened at or after `start`."""
    open_paren = text.index("(", start)
    depth = 0
    for index in range(open_paren, len(text)):
        if text[index] == "(":
            depth += 1
        elif text[index] == ")":
            depth -= 1
            if depth == 0:
                return index + 1
    raise AssertionError("unterminated call")


class KillDropOrderContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.hook = strip_comments(body(SOURCE, "static RValue& Hook_EnemyDestroyKillProc("))

    def test_original_is_called_exactly_once(self):
        self.assertEqual(self.hook.count(ORIGINAL_CALL), 1, self.hook)

    def test_both_drops_run_before_the_original(self):
        original = self.hook.index(ORIGINAL_CALL)
        for drop in DROPS:
            calls = [m.start() for m in re.finditer(rf"\b{drop}\s*\(", self.hook)]
            self.assertTrue(calls, f"{drop} is no longer called from the kill hook")
            for position in calls:
                self.assertLess(position, original, f"{drop} runs after the original kill proc")

    def test_drops_are_handed_the_enemy_self(self):
        for drop in DROPS:
            self.assertRegex(self.hook, rf"\b{drop}\s*\(\s*S\s*\)")

    def test_nothing_after_the_original_names_the_enemy(self):
        after = self.hook[call_end(self.hook, self.hook.index(ORIGINAL_CALL)):]
        self.assertIsNone(re.search(r"\bS\b", after), f"S read after the original kill proc:\n{after}")

    def test_a_throwing_drop_cannot_skip_the_original(self):
        # The drops sit inside a catch-all ahead of the trampoline, so the
        # original still runs once when one of them throws.
        original = self.hook.index(ORIGINAL_CALL)
        before = self.hook[:original]
        self.assertRegex(before, r"try\s*\{[^{}]*SignatureDropOnKill\(S\);[^{}]*AngelicDropOnKill\(S\);[^{}]*\}\s*catch\s*\(\s*\.\.\.\s*\)")


if __name__ == "__main__":
    unittest.main()
