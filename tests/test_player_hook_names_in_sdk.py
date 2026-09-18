"""Contract test: every plain script name the player build passes by string
literal to HookOneScript/HookOneScriptTable/GetNamedRoutinePointer (or
InstallScriptHook, hs-game-sdk's shared installer, if this plugin ever adopts
it) must be a name hs-game-sdk's own scripts.hpp still has as
``gml_Script_<name>``. A hook installed under a name the SDK no longer carries
never resolves at runtime - HookOneScript logs "not found" and moves on -
which is exactly the silent-blindness shape AGENTS.md's "Prove the Instrument"
warns about, just triggered by a game update instead of a build flag. This is
the same contract `ClosureNameContractTests` in `test_release_hook_contract.py`
already runs for `anon@N@...` closure names; this file covers the plain
(non-closure) script names those tests do not enumerate on their own.

Written after finding `HookHitReg` hooked "EnemyHitRegDamageParent", a name
that does not exist in the current SDK, and nothing caught it because it was
a diagnostic never installed by the player build (only the research build's
`InstallHook` called it). Removing it needed no change to how player-build
hooks are named, only a test that would have caught it had it ever reached
the player build.
"""
import re
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
PLUGIN_PATH = PROJECT_ROOT / "plugin" / "ModuleMain.cpp"
SDK_SCRIPTS_HEADER_PATH = PROJECT_ROOT.parent / "hs-game-sdk" / "cpp" / "include" / "hs_game_sdk" / "scripts.hpp"

SDK_CONSTANT_RE = re.compile(r'inline constexpr std::string_view (\w+) = "([^"]+)";')

# Only the calls that install a hook (or resolve a pointer) by a script's own
# name: HookOneScript(Table) and hs-game-sdk's shared InstallScriptHook take
# the SHORT name and prefix it themselves; GetNamedRoutinePointer needs the
# FULL "gml_Script_..." name already, so it is only in scope here when the
# literal already carries that prefix - an unprefixed literal passed to it is
# a builtin lookup ("method_call", "instance_activate_object", ...), not a
# script name, and out of scope for this file.
HOOK_CALL_RE = re.compile(r'\b(HookOneScript(?:Table)?|InstallScriptHook)\(\s*"([^"]+)"')
GETNAMED_LITERAL_RE = re.compile(r'\bGetNamedRoutinePointer\(\s*"(gml_Script_[^"]+)"')


def strip_research_blocks(source: str) -> str:
    """What the player build compiles, i.e. with FORGEPACT_RELEASE defined.

    Same conditional-evaluator as `test_release_hook_contract.py`'s helper of
    the same name (duplicated locally, matching this suite's existing
    convention of `test_kill_drop_contract.py` over a cross-file import):
    understands both `#ifndef FORGEPACT_RELEASE ... #endif` and the `#else`
    half of `#ifdef FORGEPACT_RELEASE ... #else ... #endif`, evaluates the
    conditional rather than pattern-matching one spelling of it, and tracks
    nesting so an unrelated inner `#if`/`#endif` cannot end an outer
    FORGEPACT_RELEASE block early.
    """
    kept = []
    stack = []
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


def strip_comments(source: str) -> str:
    """Code only. Research notes and hook-id strings name plenty of things."""
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.S)
    return re.sub(r"//[^\n]*", "", source)


def player_build_text(text: str) -> str:
    return strip_comments(strip_research_blocks(text))


def load_sdk_script_values():
    """The set of every script's full ``gml_Script_<name>`` value, parsed from
    hs-game-sdk's own header. Skips (rather than fails) when the sibling
    checkout is absent, same as `build.bat` itself needs it as
    `/I ..\\..\\hs-game-sdk\\cpp\\include`.
    """
    if not SDK_SCRIPTS_HEADER_PATH.exists():
        raise unittest.SkipTest(f"hs-game-sdk header not found at {SDK_SCRIPTS_HEADER_PATH}")
    text = SDK_SCRIPTS_HEADER_PATH.read_text(encoding="utf-8")
    values = {value for _, value in SDK_CONSTANT_RE.findall(text)}
    assert len(values) > 6000, f"parsed only {len(values)} SDK script constants; parser is broken"
    return values


def collect_hook_names(source: str):
    """[(call, literal, full_name), ...] for every plain script name `source`
    passes by string literal to HookOneScript(/HookOneScriptTable(/
    InstallScriptHook(, plus every "gml_Script_"-prefixed literal passed to
    GetNamedRoutinePointer(. `full_name` is the name hs-game-sdk's
    scripts.hpp would carry it under: the literal itself if it already has
    the "gml_Script_" prefix, that prefix prepended otherwise (mirroring how
    HookOneScript's own `SdkShortScriptName` round-trip works).
    """
    entries = []
    for match in HOOK_CALL_RE.finditer(source):
        call, literal = match.group(1), match.group(2)
        full = literal if literal.startswith("gml_Script_") else "gml_Script_" + literal
        entries.append((call, literal, full))
    for match in GETNAMED_LITERAL_RE.finditer(source):
        literal = match.group(1)
        entries.append(("GetNamedRoutinePointer", literal, literal))
    return entries


class PlayerHookNamesInSdkTests(unittest.TestCase):
    """(a)/(b)/(c)/(d) per the workorder: collect every plain script name the
    PLAYER build hooks by string literal, assert each still exists in
    hs-game-sdk's scripts.hpp, prove the scan cannot pass blind, and prove it
    both catches a stale name the player build would hook and ignores the
    same call written as research-only.
    """

    @classmethod
    def setUpClass(cls):
        cls.sdk_values = load_sdk_script_values()
        cls.plugin_text = PLUGIN_PATH.read_text(encoding="utf-8")
        cls.player_source = player_build_text(cls.plugin_text)
        cls.entries = collect_hook_names(cls.player_source)

    def test_scan_is_not_blind(self):
        # (c) Positive control: a parser that always returns nothing would
        # otherwise pass every other test in this file vacuously.
        self.assertGreater(len(self.entries), 0, "collected zero player-build hook names")
        names = {literal for _, literal, _ in self.entries}
        for expected in ("EnemyDestroyKillProc", "EnemyRaritySettings"):
            self.assertIn(expected, names, f"expected {expected!r} among {sorted(names)}")

    def test_every_player_hook_name_exists_in_the_sdk(self):
        # (b) The actual contract: a name the player build hooks by string
        # literal must be a name hs-game-sdk's scripts.hpp still has.
        missing = [(call, literal, full) for call, literal, full in self.entries
                   if full not in self.sdk_values]
        self.assertEqual([], missing,
                          "player-build hook call names hs-game-sdk's scripts.hpp does not have "
                          f"(call, literal, expected SDK name): {missing}")

    def test_checker_reports_a_stale_name_the_player_build_hooks(self):
        # (d) Negative control: current source has none of these (proven by
        # the assertion above), so this is what demonstrates "red" - the
        # hitreg hook this test suite was written to catch would have failed
        # exactly this way had it ever reached the player build.
        synthetic = (
            "static void InstallSomething()\n"
            "{\n"
            '    HookOneScript("NoSuchScriptXyz", "bp_x", (PVOID)HookX, &g_OrigX);\n'
            "}\n"
        )
        entries = collect_hook_names(player_build_text(synthetic))
        names = {literal for _, literal, _ in entries}
        self.assertIn("NoSuchScriptXyz", names)
        missing_full_names = [full for _, _, full in entries if full not in self.sdk_values]
        self.assertIn("gml_Script_NoSuchScriptXyz", missing_full_names)

    def test_checker_ignores_a_research_only_hook_of_a_stale_name(self):
        # (d) Same call, but guarded behind #ifndef FORGEPACT_RELEASE - the
        # player build never compiles it, so the checker must not report it.
        synthetic = (
            "#ifndef FORGEPACT_RELEASE\n"
            "static void InstallSomething()\n"
            "{\n"
            '    HookOneScript("NoSuchScriptXyz", "bp_x", (PVOID)HookX, &g_OrigX);\n'
            "}\n"
            "#endif\n"
        )
        entries = collect_hook_names(player_build_text(synthetic))
        self.assertEqual([], entries,
                          "a research-only (#ifndef FORGEPACT_RELEASE) hook call must not be reported")


if __name__ == "__main__":
    unittest.main()
