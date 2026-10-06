#!/usr/bin/env python3
"""Contract tests for `crashwatch` (ForgePact #173), the research-build crash instrument.

The 2026-10-04 `dropmult gold 100` crash left no dump, no event-log record and
nothing in out.txt but a missing clean-shutdown line. `crashwatch` writes what
a dead game cannot: a heartbeat, a breadcrumb before and after each call into
the DropGold and DropMonsterGold originals, and a line for each fatal
exception, each closed on disk before game code runs again; `crashwatch crash
confirm` is its positive control.

What these tests pin, each with a negative control where the check could pass
vacuously:

- none of it reaches the player build (not in `kPlayerCommands`, not in what
  `FORGEPACT_RELEASE` compiles), and the gold breadcrumb sink is defined only
  in the research build, before DropManager.hpp is included;
- one branch per verb, dispatched from `HandleLiveOneResearchCommand` the way
  `goldtrace` is;
- `crashwatch crash` without `confirm` refuses and crashes nothing;
- the exception trap only logs: every path continues the search, none handles
  or resumes, and it writes with Win32 file calls from a fixed buffer (no
  stream, no `Out(`, no `g_OutFileLock`, no allocation, no YYToolkit call);
- crashwatch installs no hook of its own;
- the file name, the heartbeat cadence and both caps are named constants.
"""

import re
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_release_hook_contract import strip_comments, strip_research_blocks  # noqa: E402

PROJECT_ROOT = Path(__file__).resolve().parents[1]
PLUGIN_SRC = PROJECT_ROOT / "plugin" / "ModuleMain.cpp"
DROP_MANAGER = PROJECT_ROOT / "plugin" / "include" / "ForgePact" / "DropManager.hpp"


def body(source, signature):
    """`signature`'s whole definition, brace-matched, from its LAST occurrence.

    The last, so a forward declaration (CrashWatchGoldCrumb has one at the top
    of the file) never stands in for the definition: several assertions below
    are negative, and a wrong span would satisfy them vacuously.
    """
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


def crash_branch_refuses_without_confirm(branch):
    """True when the `crash` branch tests for `confirm`, refuses naming it and
    returns, all before the one call that crashes."""
    call = branch.find("CrashWatchCrashOnPurpose()")
    check = branch.find('!= "confirm"')
    if call < 0 or check < 0 or check > call:
        return False
    refusal = branch[check:call]
    return "refused" in refusal and "confirm" in refusal.split('!= "confirm"', 1)[1] and "return;" in refusal


def returns_only_continue_search(trap):
    returns = re.findall(r"\breturn\b([^;]*);", trap)
    return bool(returns) and all(r.strip() == "EXCEPTION_CONTINUE_SEARCH" for r in returns) \
        and "EXCEPTION_CONTINUE_EXECUTION" not in trap and "EXCEPTION_EXECUTE_HANDLER" not in trap


# What a body that runs inside the trap may never use: each allocates, takes a
# lock the dying thread may hold, or calls into the runtime that just broke.
TRAP_FORBIDDEN = (
    "std::ofstream", "ofstream", "fopen", "Out(", "OutRaw(", "g_OutFileLock",
    "std::string", "std::to_string", "sprintf", "new ", "malloc", "push_back",
    "g_Yytk", "CallBuiltin", "GetModuleFileName", "GetModuleHandle",
)

# Every call the trap makes. A new one has to be added here, deliberately.
TRAP_CALLS = {
    "CrashWatchIsFatal", "InterlockedIncrement", "RtlPcToFileHeader", "CrashWatchPutText",
    "CrashWatchPutHex", "GetCurrentThreadId", "CrashWatchWrite", "InHookId", "InModNow",
    "ModName", "load", "reinterpret_cast", "static_cast", "sizeof", "if", "for",
}


class TestCrashWatchContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_SRC.read_text(encoding="utf-8")
        cls.code = strip_comments(cls.plugin)
        cls.player = strip_comments(strip_research_blocks(cls.plugin))
        cls.allowlist = re.search(r"kPlayerCommands\s*=\s*\{(?P<body>.*?)\};", cls.plugin, re.S).group("body")
        cls.command = strip_comments(body(cls.plugin, "static void CrashWatchCommand(const std::string& rest)"))
        cls.trap = strip_comments(body(cls.plugin, "static LONG CALLBACK CrashWatchTrap(PEXCEPTION_POINTERS info)"))
        cls.write = strip_comments(body(cls.plugin, "static bool CrashWatchWrite(const char* text, int len, bool reserved)"))
        # The instrument's own block: from its constants to the end of its command.
        start = cls.code.index("kCrashWatchFileName[]")
        end = cls.code.index("}", cls.code.index("static void CrashWatchCommand(const std::string& rest)"))
        cls.region = cls.code[start:cls.code.index("#endif", end)]

    # ---- research build only ---------------------------------------------------------

    def test_nothing_reaches_the_player_build(self):
        for name in ("crashwatch", "CrashWatch", "FP_GOLD_CRUMB_SINK"):
            # Negative control: the research build does carry each name.
            self.assertIn(name, self.code)
            self.assertNotIn(name, self.player)
            self.assertNotIn(name, self.allowlist)

    def test_the_sink_is_defined_before_drop_manager_and_only_for_research(self):
        define = self.plugin.index("#define FP_GOLD_CRUMB_SINK(")
        self.assertLess(define, self.plugin.index("#include <ForgePact/DropManager.hpp>"))
        self.assertIn("CrashWatchGoldCrumb(script, done, ordinal, mult, handed, passed)",
                      self.plugin[define:define + 200])
        # DropManager.hpp compiles its points to nothing without a sink.
        header = DROP_MANAGER.read_text(encoding="utf-8")
        self.assertIn("#ifdef FP_GOLD_CRUMB_SINK", header)
        self.assertIn("FP_GOLD_CRUMB_SINK(script, false, goldCrumb, mult, handed, passed)", header)

    def test_the_heartbeat_runs_from_the_frame_callback_research_block(self):
        frame = body(self.plugin, "void FrameCallback(FWFrame& FrameContext)")
        self.assertEqual(strip_comments(frame).count("CrashWatchTick(fc);"), 1)
        self.assertNotIn("CrashWatchTick", strip_research_blocks(frame))

    # ---- verbs -----------------------------------------------------------------------

    def test_one_branch_per_verb(self):
        self.assertEqual(self.code.count('lc == "crashwatch"'), 1)
        dispatch = strip_comments(body(self.plugin, "static bool HandleLiveOneResearchCommand("))
        self.assertIn('if (lc == "crashwatch") { CrashWatchCommand(rest); return true; }', dispatch)
        for verb in ("on", "off", "status", "crash"):
            self.assertEqual(self.command.count(f'verb == "{verb}"'), 1, verb)
        self.assertIn('Out("crashwatch: usage -> crashwatch on | off | status | crash confirm")', self.command)

    def test_status_line_and_reply_prefixes(self):
        status = strip_comments(body(self.plugin, "static std::string CrashWatchStatus()"))
        self.assertIn('std::string("crashwatch: ") + (g_CrashWatchOn.load() ? "on" : "off")', status)
        for count in ("g_CrashWatchHeartbeats", "g_CrashWatchCrumbs", "g_CrashWatchExceptions",
                      "g_CrashWatchLines", "kCrashWatchMaxLines"):
            self.assertIn(count, status)
        self.assertIn('" -> bp_ipc\\\\" + kCrashWatchFileName', status)
        # on and off both answer with the status line.
        for verb in ("on", "off"):
            start = self.command.index(f'verb == "{verb}"')
            self.assertIn("Out(CrashWatchStatus());", self.command[start:self.command.index("} else if", start)])
        tick = strip_comments(body(self.plugin, "static void CrashWatchTick(uint32_t frame)"))
        self.assertIn('CrashWatchWriteLine("hb " + CrashWatchUtcNow()', tick)
        for column in ('" room="', '" coins="', '" gold=x"', '" DropGold="', '" DropMonsterGold="'):
            self.assertIn(column, tick)
        crumb = strip_comments(body(self.plugin, "static void CrashWatchGoldCrumb("))
        self.assertIn('"gold %s enter #%ld x%d a4 %s -> %s\\n"', crumb)
        self.assertIn('"gold %s enter #%ld x%d\\n"', crumb)
        self.assertIn('"gold %s done #%ld\\n"', crumb)
        for part in ('"exception 0x"', '" at "', '"+0x"', '" thread="', '"game" : "other"',
                     '" in-hook="', '" in-mod="'):
            self.assertIn(part, self.trap)

    def test_coin_obj_is_resolved_by_sdk_name(self):
        tick = strip_comments(body(self.plugin, "static void CrashWatchTick(uint32_t frame)"))
        self.assertIn("HeroSiege::Objects::GetObjectName(HeroSiege::Objects::GameObject::Coin_obj)", tick)
        self.assertIn('"asset_get_index"', tick)
        self.assertIn('"instance_number"', tick)
        self.assertIn('coins = "?"', tick)

    # ---- the deliberate crash --------------------------------------------------------

    def test_crash_without_confirm_refuses_and_crashes_nothing(self):
        start = self.command.index('verb == "crash"')
        branch = self.command[start:self.command.index("} else {", start)]
        self.assertTrue(crash_branch_refuses_without_confirm(branch))
        # Negative control: the same branch without its refusal fails the check.
        unguarded = re.sub(r"if \(Lower\(TrimCopy\(arg\)\) != \"confirm\"\) \{.*?return;\s*\}", "", branch, flags=re.S)
        self.assertNotEqual(unguarded, branch)
        self.assertFalse(crash_branch_refuses_without_confirm(unguarded))
        # The crash has exactly one caller, that branch.
        self.assertEqual(self.code.count("CrashWatchCrashOnPurpose()"), 2)   # definition + the call

    def test_the_crash_announces_itself_and_runs_outside_every_try(self):
        crash = strip_comments(body(self.plugin, "static __declspec(noinline) void CrashWatchCrashOnPurpose()"))
        self.assertIn('"crashwatch: crashing on purpose (positive control)"', crash)
        self.assertIn("Out(announcement);", crash)
        self.assertIn("CrashWatchWrite(", crash)
        self.assertNotIn("try", crash)
        self.assertNotIn("__try", crash)
        self.assertRegex(crash, r"int\* volatile target = nullptr;\s*\*target = ")
        # It does not require `on`: nothing in it reads the switch.
        self.assertNotIn("g_CrashWatchOn", crash)

    # ---- the trap --------------------------------------------------------------------

    def test_the_trap_always_continues_the_search_and_never_handles(self):
        self.assertTrue(returns_only_continue_search(self.trap))
        # Negative control: a trap that handles is caught.
        self.assertFalse(returns_only_continue_search(
            self.trap.replace("return EXCEPTION_CONTINUE_SEARCH;\n}", "return EXCEPTION_EXECUTE_HANDLER;\n}")))
        self.assertFalse(returns_only_continue_search(
            self.trap + "\nreturn EXCEPTION_CONTINUE_EXECUTION;"))
        # First-registered, installed at `on`, removed at `off`.
        self.assertEqual(self.code.count("AddVectoredExceptionHandler("), 1)
        on = self.command[self.command.index('verb == "on"'):self.command.index('verb == "off"')]
        off = self.command[self.command.index('verb == "off"'):self.command.index('verb == "crash"')]
        self.assertIn("AddVectoredExceptionHandler(1, CrashWatchTrap)", on)
        self.assertIn("RemoveVectoredExceptionHandler(g_CrashWatchTrapHandle)", off)
        # The trap is quiet while off.
        self.assertIn("!g_CrashWatchOn.load(std::memory_order_relaxed)", self.trap)

    def test_the_trap_logs_only_the_fatal_kinds(self):
        fatal = strip_comments(body(self.plugin, "static bool CrashWatchIsFatal(DWORD code)"))
        for code in ("EXCEPTION_ACCESS_VIOLATION", "EXCEPTION_STACK_OVERFLOW", "EXCEPTION_ILLEGAL_INSTRUCTION",
                     "EXCEPTION_PRIV_INSTRUCTION", "EXCEPTION_INT_DIVIDE_BY_ZERO",
                     "EXCEPTION_ARRAY_BOUNDS_EXCEEDED", "kCrashWatchHeapCorruption"):
            self.assertIn(code, fatal)
        self.assertNotIn("EXCEPTION_BREAKPOINT", fatal)
        self.assertIn("kCrashWatchHeapCorruption = 0xC0000374", self.code)
        self.assertIn("if (!CrashWatchIsFatal(code)) return EXCEPTION_CONTINUE_SEARCH;", self.trap)

    def test_the_trap_writes_with_win32_calls_from_a_fixed_buffer(self):
        helpers = [strip_comments(body(self.plugin, sig)) for sig in (
            "static int CrashWatchPutText(char* buf, int pos, int cap, const char* text)",
            "static int CrashWatchPutHex(char* buf, int pos, int cap, unsigned long long value, int minDigits)",
            "static bool CrashWatchIsFatal(DWORD code)",
        )]
        for text in [self.trap, self.write] + helpers:
            for token in TRAP_FORBIDDEN:
                self.assertNotIn(token, text, token)
        for call in ("CreateFileA(", "WriteFile(", "CloseHandle("):
            self.assertIn(call, self.write)
        self.assertIn("char line[256];", self.trap)
        self.assertIn("CrashWatchWrite(line, pos, true);", self.trap)
        # Every call the trap makes is one of the known allocation-free ones.
        signature_end = self.trap.index("{")
        calls = set(re.findall(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\(", self.trap[signature_end:]))
        self.assertLessEqual(calls, TRAP_CALLS, calls - TRAP_CALLS)
        # Negative control: the forbidden list would catch a stream write.
        self.assertTrue(any(t in 'std::ofstream f(path); f << "x";' for t in TRAP_FORBIDDEN))

    def test_every_line_is_on_disk_before_the_writer_returns(self):
        # Open, append, close: no handle is kept between lines.
        self.assertLess(self.write.index("CreateFileA("), self.write.index("WriteFile("))
        self.assertLess(self.write.index("WriteFile("), self.write.index("CloseHandle("))
        self.assertIn("FILE_APPEND_DATA", self.write)
        self.assertNotIn("static HANDLE", self.region)
        self.assertIn("static char g_CrashWatchPath[MAX_PATH]", self.region)   # the region is the block

    # ---- named constants and caps ----------------------------------------------------

    def test_file_name_cadence_and_caps_are_named_constants(self):
        self.assertIn('kCrashWatchFileName[] = "crashwatch.txt"', self.code)
        self.assertIn("kCrashWatchHeartbeatFrames = 15", self.code)
        self.assertIn("kCrashWatchMaxLines = 50000", self.code)
        self.assertIn("kCrashWatchMaxExceptionLines = 32", self.code)
        # Each literal lives in its constant only.
        self.assertEqual(self.code.count('"crashwatch.txt"'), 1)
        for number in ("50000", "32", "15"):
            # A buffer's size (`char text[32]`) is not the cap.
            literal = rf"(?<!\[)\b{number}\b(?!\])"
            self.assertEqual(len(re.findall(literal, self.region)), 1, number)
        # ...and the code reads the names.
        tick = strip_comments(body(self.plugin, "static void CrashWatchTick(uint32_t frame)"))
        self.assertIn("% kCrashWatchHeartbeatFrames", tick)
        self.assertIn("> kCrashWatchMaxLines", self.write)
        self.assertIn("InterlockedIncrement(&g_CrashWatchOverCap)", self.write)
        self.assertIn("> kCrashWatchMaxExceptionLines", self.trap)
        path = strip_comments(body(self.plugin, "static void CrashWatchEnsurePath()"))
        self.assertIn('IPC_DIR + "\\\\" + kCrashWatchFileName', path)

    # ---- no hook of its own ----------------------------------------------------------

    def test_crashwatch_installs_no_hook(self):
        region = self.region
        for installer in ("HookOneScript", "HookBuiltin", "InstallScriptHook", "MmCreateHook",
                          "CreateHook", "InstallHooks(", "InstallDropMultHooks("):
            self.assertNotIn(installer, region)
        # Negative control: the region is the instrument, not an empty span.
        self.assertIn("AddVectoredExceptionHandler", region)
        self.assertIn("static void CrashWatchGoldCrumb(", region)


if __name__ == "__main__":
    unittest.main()
