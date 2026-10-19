#!/usr/bin/env python3
"""Contract tests for the crash trap (ForgePact #223), the player build's crash recorder.

plugin/include/ForgePact/CrashTrap.hpp writes what a crashing game was doing
(exception code, module and offset, thread, the hook and mod tags, a C++
exception's type) into bp_ipc\\crashtrap\\, has a MiniDumpNormal and the two
loader logs' tails taken at an unhandled crash, and the next load folds it all
into the crash report. tests/test_crash_trap_behavior.py drives it; these
tests pin the source rules a behaviour test cannot see, each with a negative
control where the check could pass vacuously:

- inside either route (the vectored handler, the unhandled filter and what
  they call): Win32 calls and fixed buffers only, no allocation, no stream,
  no lock of ours, nothing from the game's runtime, and no question to the
  loader;
- the vectored route only ever continues the search, and the filter returns
  that or the previous filter's own answer; neither handles nor resumes;
- `MiniDumpNormal` is the only dump type, the dump is taken from the
  unhandled route only, and the filter's wait for it is bounded;
- dbghelp.dll is loaded from the system directory and MiniDumpWriteDump taken
  by name, so build.bat's link line stays as it is;
- the installer runs in the player build, from IncidentMonitorStart, before
  the monitor thread, and rotates the folder before arming anything;
- nothing in it is crashwatch's (the research instrument stays out of the
  player build), and it shows no notice;
- every `catch (...)` that ends in `throw;` (an exception handed back into the
  game) writes a `rethrow: ` line first, except an explicit allowlist.
"""

import re
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_release_hook_contract import function_body, strip_comments, strip_research_blocks  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"
HEADERS = ROOT / "plugin" / "include" / "ForgePact"
TRAP = HEADERS / "CrashTrap.hpp"
BUILD_BAT = ROOT / "plugin_build" / "build.bat"


def read(path):
    return path.read_text(encoding="utf-8").replace("\r\n", "\n")


def returns(body):
    return [r.strip() for r in re.findall(r"\breturn\b([^;]*);", body)]


def vectored_only_continues(body):
    found = returns(body)
    return bool(found) and all(r == "EXCEPTION_CONTINUE_SEARCH" for r in found) \
        and "EXCEPTION_CONTINUE_EXECUTION" not in body and "EXCEPTION_EXECUTE_HANDLER" not in body


def filter_only_continues_or_defers(body):
    found = returns(body)
    return bool(found) and all(r == "EXCEPTION_CONTINUE_SEARCH" or re.fullmatch(r"previous\(info\)", r) for r in found) \
        and "EXCEPTION_CONTINUE_EXECUTION" not in body and "EXCEPTION_EXECUTE_HANDLER" not in body


# What may never run inside a route: each allocates, takes a lock the dying
# thread may hold, calls into the runtime that just broke, or asks the loader.
ROUTE_FORBIDDEN = (
    "std::ofstream", "ofstream", "fopen", "Out(", "OutRaw(", "g_OutFileLock", "lock_guard",
    "std::string", "std::to_string", "sprintf", "snprintf", "new ", "malloc", "push_back",
    "std::filesystem", "g_Yytk", "CallBuiltin", "RValue", "CInstance",
    "GetModuleFileName", "GetModuleHandle", "LoadLibrary", "GetProcAddress", "RtlPcToFileHeader",
    "K32EnumProcessModules", "MessageBox",
)

# The functions that run inside a route.
ROUTE_FUNCTIONS = {
    "FirstChance": "inline LONG CALLBACK FirstChance(PEXCEPTION_POINTERS info) noexcept",
    "Unhandled": "inline LONG WINAPI Unhandled(PEXCEPTION_POINTERS info) noexcept",
    "WriteRecord": "inline void WriteRecord(const EXCEPTION_RECORD* record, bool unhandled) noexcept",
    "AppendRecord": "inline bool AppendRecord(const char* text, int len) noexcept",
    "ReadCppType": "inline bool ReadCppType(const EXCEPTION_RECORD* record, char* out, size_t cap) noexcept",
    "FindModule": "inline const ModuleRow* FindModule(uintptr_t address) noexcept",
    "RequestDump": "inline void RequestDump(PEXCEPTION_POINTERS info) noexcept",
    "IsFatal": "inline bool IsFatal(DWORD code) noexcept",
    "Put": "inline int Put(char* buf, int pos, int cap, const char* text, size_t limit",
    "PutHex": "inline int PutHex(char* buf, int pos, int cap, unsigned long long value, int minDigits) noexcept",
    "PutDec": "inline int PutDec(char* buf, int pos, int cap, unsigned long long value) noexcept",
    "CopyText": "inline void CopyText(char* to, size_t cap, const char* from) noexcept",
}

# Every call a route function makes. A new one has to be added here, deliberately.
ROUTE_CALLS = {
    "FirstChance", "Unhandled", "WriteRecord", "AppendRecord", "ReadCppType", "FindModule", "RequestDump",
    "IsFatal", "Put", "PutHex", "PutDec", "CopyText",
    "InterlockedIncrement", "InterlockedDecrement", "InterlockedExchange", "GetCurrentThreadId",
    "SetEvent", "WaitForSingleObject", "CreateFileA", "WriteFile", "CloseHandle",
    "InHookId", "InModNow", "ModName", "load", "previous",
    "reinterpret_cast", "static_cast", "sizeof", "if", "for", "while", "do", "__except",
}


def catch_all_blocks(code):
    """(start, body) for every `catch (...) { ... }` in `code`."""
    blocks = []
    for found in re.finditer(r"catch\s*\(\s*\.\.\.\s*\)\s*\{", code):
        brace = found.end() - 1
        depth = 0
        for index in range(brace, len(code)):
            if code[index] == "{":
                depth += 1
            elif code[index] == "}":
                depth -= 1
                if depth == 0:
                    blocks.append((found.start(), code[brace + 1:index]))
                    break
    return blocks


def enclosing_function(code, at):
    """The name of the function a position sits in: the last unindented line
    before it that opens a definition."""
    name = None
    for line in re.finditer(r"^(?:static |inline |template[^\n]*\n)?[A-Za-z_][^;\n=]*?\b([A-Za-z_]\w*)\s*\([^;\n]*$",
                            code[:at], re.M):
        name = line.group(1)
    return name


def rethrows_without_a_line(code, allowlist):
    """The enclosing function of each catch-all block that ends in `throw;` and
    writes no RethrowLine first."""
    missing = []
    for start, body in catch_all_blocks(code):
        text = body.strip()
        if not text.endswith("throw;"):
            continue
        if re.search(r'RethrowLine\("[a-z]+", \w+\);\s*throw;$', text):
            continue
        name = enclosing_function(code, start)
        if name not in allowlist:
            missing.append(name)
    return missing


# Rethrows that do not reach the game: SkillTimerDrawNumber's caller is our own
# draw path, which catches and counts it.
RETHROW_ALLOWLIST = {"SkillTimerDrawNumber"}


class CrashTrapContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.trap = read(TRAP)
        cls.trap_code = strip_comments(cls.trap)
        cls.plugin = read(PLUGIN)
        cls.player = strip_comments(strip_research_blocks(cls.plugin))

    def body(self, name):
        return strip_comments(function_body(self.trap, ROUTE_FUNCTIONS[name]))

    # ---- inside the routes -----------------------------------------------------------

    def test_the_routes_use_win32_calls_and_fixed_buffers_only(self):
        for name in ROUTE_FUNCTIONS:
            body = self.body(name)
            for token in ROUTE_FORBIDDEN:
                self.assertNotIn(token, body, f"{name}: {token}")
            calls = set(re.findall(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\(", body))
            self.assertLessEqual(calls, ROUTE_CALLS, f"{name}: {calls - ROUTE_CALLS}")
        record = self.body("WriteRecord")
        self.assertIn("char line[kLineChars];", record)
        self.assertIn("char type[kTypeChars];", record)
        self.assertIn("AppendRecord(line, pos);", record)
        append = self.body("AppendRecord")
        self.assertLess(append.index("CreateFileA("), append.index("WriteFile("))
        self.assertLess(append.index("WriteFile("), append.index("CloseHandle("))
        self.assertIn("FILE_APPEND_DATA", append)
        # The module comes from the list the handler never makes.
        self.assertIn("FindModule(address)", record)
        self.assertIn("K32EnumProcessModules(", strip_comments(function_body(self.trap, "inline void RefreshModules() noexcept")))
        # Negative control: the forbidden list catches a stream write and a
        # loader call, and the call allowlist a new call.
        self.assertTrue(any(t in 'std::ofstream f(path); f << "x";' for t in ROUTE_FORBIDDEN))
        self.assertTrue(any(t in "GetModuleHandleW(nullptr)" for t in ROUTE_FORBIDDEN))
        self.assertNotIn("RtlPcToFileHeader", ROUTE_CALLS)

    def test_the_guarded_read_holds_no_cpp_object(self):
        # __try needs a function with nothing to unwind: plain data only.
        read_type = self.body("ReadCppType")
        self.assertIn("__try", read_type)
        self.assertIn("__except (EXCEPTION_EXECUTE_HANDLER)", read_type)
        self.assertIsNone(re.search(r"\bstd::", read_type))
        # The x64 layout: offsets from the image base in the fourth parameter.
        self.assertIn("record->ExceptionInformation[3]", read_type)
        self.assertIn('static_assert(sizeof(void*) == 8', self.trap_code)
        # A nested exception inside a route is not recorded again.
        self.assertIn("t_Inside", self.body("FirstChance"))
        self.assertIn("|| t_Inside) return EXCEPTION_CONTINUE_SEARCH;", self.body("FirstChance"))

    def test_the_vectored_route_only_continues_the_search(self):
        first = self.body("FirstChance")
        self.assertTrue(vectored_only_continues(first))
        # It writes for the fatal codes only, under its cap, and only
        # remembers a C++ throw.
        self.assertIn("IsFatal(record->ExceptionCode)", first)
        self.assertIn("<= kMaxFirstChanceLines", first)
        self.assertIn("record->ExceptionCode == kCppExceptionCode", first)
        cpp = first[first.index("kCppExceptionCode"):first.index("IsFatal(")]
        self.assertNotIn("WriteRecord(", cpp)
        fatal = self.body("IsFatal")
        for code in ("EXCEPTION_ACCESS_VIOLATION", "EXCEPTION_STACK_OVERFLOW", "EXCEPTION_ILLEGAL_INSTRUCTION",
                     "EXCEPTION_PRIV_INSTRUCTION", "EXCEPTION_INT_DIVIDE_BY_ZERO",
                     "EXCEPTION_ARRAY_BOUNDS_EXCEEDED", "kHeapCorruption"):
            self.assertIn(code, fatal)
        self.assertNotIn("EXCEPTION_BREAKPOINT", fatal)
        # Negative controls: a route that handles or resumes is caught.
        handles = "return EXCEPTION_EXECUTE_HANDLER;".join(first.rsplit("return EXCEPTION_CONTINUE_SEARCH;", 1))
        self.assertNotEqual(handles, first)
        self.assertFalse(vectored_only_continues(handles))
        self.assertFalse(vectored_only_continues(first + "\nreturn EXCEPTION_CONTINUE_EXECUTION;"))

    def test_the_filter_records_once_and_hands_over_to_the_previous_one(self):
        unhandled = self.body("Unhandled")
        self.assertTrue(filter_only_continues_or_defers(unhandled))
        self.assertIn("InterlockedExchange(&g_State.unhandledSeen, 1) == 0", unhandled)
        self.assertLess(unhandled.index("WriteRecord(info->ExceptionRecord, true);"), unhandled.index("RequestDump(info);"))
        self.assertLess(unhandled.index("RequestDump(info);"), unhandled.index("return previous(info);"))
        self.assertFalse(filter_only_continues_or_defers(unhandled.replace(
            "return previous(info);", "return EXCEPTION_EXECUTE_HANDLER;")))

    # ---- the dump ----------------------------------------------------------------------

    def test_minidump_normal_is_the_only_dump_type(self):
        names = set(re.findall(r"\bMiniDump\w+", self.trap_code))
        self.assertEqual(names, {"MiniDumpNormal", "MiniDumpWriteDump", "MiniDumpWriteDumpFn"})
        self.assertIn(", MiniDumpNormal,", strip_comments(function_body(self.trap, "inline bool WriteDump() noexcept")))
        # Negative control: a fuller type would be caught.
        self.assertNotEqual(set(re.findall(r"\bMiniDump\w+", self.trap_code + " MiniDumpWithFullMemory")), names)

    def test_the_dump_comes_from_the_unhandled_route_only_and_is_waited_for_a_bounded_time(self):
        self.assertIn("RequestDump(info);", self.body("Unhandled"))
        self.assertNotIn("RequestDump(", self.body("FirstChance"))
        self.assertEqual(len(re.findall(r"\bRequestDump\(", self.trap_code)), 2)   # definition + the filter's call
        request = self.body("RequestDump")
        self.assertIn("WaitForSingleObject(g_State.done, kDumpWaitMs);", request)
        self.assertNotIn("INFINITE", request)
        self.assertNotIn("MiniDumpWriteDump", request)
        self.assertRegex(self.trap_code, r"inline constexpr DWORD kDumpWaitMs = \d+;")
        # The helper thread writes it, created at install, never at the crash.
        helper = strip_comments(function_body(self.trap, "inline DWORD WINAPI Helper(LPVOID) noexcept"))
        self.assertIn("WriteDump()", helper)
        self.assertLess(helper.index("CopyLogTail("), helper.index("WriteDump()"))
        install = strip_comments(function_body(self.trap, "inline std::string Install("))
        self.assertIn("CreateThread(nullptr, 0, &Helper, nullptr, 0, nullptr)", install)
        for name in ROUTE_FUNCTIONS:
            self.assertNotIn("CreateThread", self.body(name), name)

    def test_dbghelp_comes_from_the_system_directory_and_the_link_line_is_unchanged(self):
        install = strip_comments(function_body(self.trap, "inline std::string Install("))
        self.assertIn('LoadLibraryExW(L"dbghelp.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32)', install)
        self.assertIn('GetProcAddress(dbghelp, "MiniDumpWriteDump")', install)
        self.assertEqual(len(re.findall(r"\bLoadLibrary\w*\(", self.trap_code)), 1)
        # Unavailable, it turns the dump off and says why.
        self.assertIn("dumpWhy", install)
        for source in (self.trap_code, strip_comments(self.plugin)):
            self.assertNotIn('#pragma comment(lib, "dbghelp', source)
        cl = [line for line in read(BUILD_BAT).splitlines() if line.startswith("cl ")]
        self.assertEqual(len(cl), 1)
        self.assertTrue(cl[0].endswith("/link /DLL user32.lib"), cl[0])

    # ---- install --------------------------------------------------------------------

    def test_install_rotates_first_then_arms_both_routes(self):
        install = strip_comments(function_body(self.trap, "inline std::string Install("))
        rotate = install.index("Rotate(dir,")
        for later in ("CreateFileA(s.record", "LoadLibraryExW(", "AddVectoredExceptionHandler(",
                      "SetUnhandledExceptionFilter("):
            self.assertLess(rotate, install.index(later), later)
        self.assertIn("AddVectoredExceptionHandler(1, &FirstChance)", install)
        self.assertIn("s.previous = SetUnhandledExceptionFilter(&Unhandled);", install)
        self.assertIn("g_State.installed.exchange(true)", install)
        rotate_body = strip_comments(function_body(self.trap, "inline bool Rotate("))
        self.assertLess(rotate_body.index("remove_all(prev"), rotate_body.index("rename(dir, prev"))
        for name in ('kDirName[] = "crashtrap"', 'kPrevDirName[] = "crashtrap.prev"', 'kRecordFile[] = "trap.txt"',
                     'kDumpFile[] = "crash.dmp"', 'kYytkLogFile[] = "YYToolkit.log"', 'kAurieLogFile[] = "aurie.log"'):
            self.assertIn(name, self.trap_code)

    def test_the_installer_is_in_the_player_build_before_the_monitor_thread(self):
        self.assertIn("#include <ForgePact/CrashTrap.hpp>", strip_research_blocks(self.plugin))
        start = strip_comments(function_body(self.plugin, "static void IncidentMonitorStart()"))
        self.assertIn("Out(ForgePact::CrashTrap::Install(IPC_DIR, IncidentExeDir()));", start)
        self.assertLess(start.index("CrashTrap::Install("), start.index("Monitor::Instance().Start("))
        self.assertIn(start, self.player)
        self.assertEqual(self.plugin.count("CrashTrap::Install("), 1)
        tick = strip_comments(function_body(self.plugin, "static void IncidentFrameTick()"))
        self.assertIn("ForgePact::CrashTrap::NoteGameThread(GetCurrentThreadId());", tick)
        self.assertLess(tick.index("NoteGameThread("), tick.index("if (!g_Setup) return;"))
        run = strip_comments(function_body(self.plugin, "static void IncidentMonitorRun() noexcept"))
        self.assertIn("ForgePact::CrashTrap::RefreshModules();", run)

    def test_the_next_load_folds_the_previous_session(self):
        check = strip_comments(function_body(self.plugin, "static void IncidentCrashCheck()"))
        self.assertIn("ForgePact::CrashTrap::kPrevDirName", check)
        self.assertIn("inc::ParseTrapRecord(", check)
        self.assertIn("facts.haveTrap = haveRecord;", check)
        self.assertIn("inc::CrashLine(exitCode, module, haveRecord ? inc::TrapWhere(record) : std::string())", check)
        # Deleted after a clean session, and after a crash once folded.
        clean = check[check.index("if (inc::SessionEndedCleanly(session))"):check.index("inc::TrapRecord record;")]
        self.assertIn("inc::SettleTrapPrev(trapPrev, true, false);", clean)
        self.assertGreater(check.index("inc::SettleTrapPrev(trapPrev, false, true);"),
                           check.index("if (dir.empty())"))
        bundle = strip_comments(function_body(self.plugin, "static std::string IncidentWriteBundle("))
        for name in ('"yytoolkit-tail.txt"', '"aurie-tail.txt"', '"crash.dmp"', '"trap.txt"', "inc::LogMissingText("):
            self.assertIn(name, bundle)
        # The dump moves in before report.json says whether it is there.
        self.assertLess(bundle.index('"crash.dmp"'), bundle.index('put("report.json"'))

    # Whoever sets the top-level filter last owns it, so ours being on top is
    # measured, by the status line and the monitor thread's module refresh,
    # and the filter ours replaced is kept as the bottom of its chain.
    def test_whether_ours_is_still_the_top_filter_is_measured(self):
        install = strip_comments(function_body(self.trap, "inline std::string Install("))
        self.assertIn("s.chain[0] = s.previous;", install)
        for name in ("inline std::string StatusLine()", "inline void RefreshModules()"):
            self.assertIn("CheckUnhandled();", strip_comments(function_body(self.trap, name)), name)
        check = strip_comments(function_body(self.trap, "inline void CheckUnhandled()"))
        # It only ever puts ours back, or leaves the one it found.
        self.assertEqual(re.findall(r"SetUnhandledExceptionFilter\(([^)]*)\)", check), ["&Unhandled", "found"])
        self.assertIn("count < kMaxFilterChain", check)
        self.assertIn('"filter replaced by "', check)
        # The filter walks down the chain and never past its bottom.
        unhandled = strip_comments(function_body(self.trap, "inline LONG WINAPI Unhandled("))
        self.assertIn("at >= 0 && at < kMaxFilterChain ? g_State.chain[at] : nullptr", unhandled)

    def test_incident_stat_prints_the_trap_line(self):
        command = strip_comments(function_body(self.plugin, "static void IncidentCommand(const std::string& rest)"))
        self.assertIn("Out(ForgePact::CrashTrap::StatusLine());", command)
        status = strip_comments(function_body(self.trap, "inline std::string StatusLine()"))
        self.assertIn('std::string("incident: trap ")', status)
        for part in ('"first-chance armed"', '"unhandled armed"', '"dump ready"', '"dump off - "'):
            self.assertIn(part, status)

    # ---- what the player build may carry --------------------------------------------

    def test_nothing_of_crashwatch_and_no_notice(self):
        for name in ("crashwatch", "CrashWatch"):
            self.assertFalse(name in self.trap, name)
        # Negative control: the research block of ModuleMain.cpp carries both.
        self.assertIn("crashwatch", self.plugin)
        self.assertIn("CrashWatch", self.plugin)
        for notice in ("MessageBox", "MessageBeep", "FlashWindow"):
            self.assertNotIn(notice, self.trap_code)
        # No runtime interface and no hook: only the file name aurie.log.
        for game in ("RValue", "g_Yytk", "YYTK", "CInstance", "AurieStatus", "MmCreateHook", "CallBuiltin"):
            self.assertFalse(game in self.trap_code, game)
        self.assertIsNone(re.search(r"std::(min|max)\s*\(", self.trap_code))
        # Nothing for the CRT to destroy at exit.
        self.assertIn("inline constinit State g_State{};", self.trap_code)
        self.assertIn("static_assert(std::is_trivially_destructible_v<State>", self.trap_code)

    # ---- the rethrow lines --------------------------------------------------------------

    def test_every_rethrow_into_the_game_writes_a_line_first(self):
        sources = {"ModuleMain.cpp": strip_comments(self.plugin)}
        for header in sorted(HEADERS.glob("*.hpp")):
            sources[header.name] = strip_comments(read(header))
        found = 0
        for name, code in sources.items():
            found += sum(1 for _, b in catch_all_blocks(code) if b.strip().endswith("throw;"))
            self.assertEqual(rethrows_without_a_line(code, RETHROW_ALLOWLIST), [], name)
        # Positive control: the flight charms' load scope rethrows the game's
        # own exception, and SkillTimerDrawNumber's is allowlisted.
        self.assertGreaterEqual(found, 2)
        parse = strip_comments(function_body(
            self.plugin, "static RValue& Hook_FlightParseItemToGrid(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)"))
        self.assertIn('RethrowLine("flightcharm", g_FlightParseRethrowLines);\n        throw;', parse)
        line = strip_comments(function_body(self.plugin, "static void RethrowLine(const char* mod, volatile long& written) noexcept"))
        for part in ('"rethrow: "', '" type="', "ForgePact::CrashTrap::LastCppType()", "> kRethrowLinesPerSite"):
            self.assertIn(part, line)
        self.assertIn("static constexpr long kRethrowLinesPerSite = ", self.plugin)
        self.assertIn("static void RethrowLine(", self.player)
        # Negative control: the same check on a hook that rethrows silently.
        snippet = ("static RValue& Hook_Snippet(CInstance* S)\n{\n    try {\n        return g_Orig_Snippet(S);\n"
                   "    } catch (...) {\n        Cleanup();\n        throw;\n    }\n}\n")
        self.assertEqual(rethrows_without_a_line(snippet, RETHROW_ALLOWLIST), ["Hook_Snippet"])
        lined = snippet.replace("Cleanup();\n", 'Cleanup();\n        RethrowLine("snippet", g_SnippetLines);\n')
        self.assertEqual(rethrows_without_a_line(lined, RETHROW_ALLOWLIST), [])
        self.assertEqual(rethrows_without_a_line(snippet.replace("Hook_Snippet", "SkillTimerDrawNumber"),
                                                 RETHROW_ALLOWLIST), [])


if __name__ == "__main__":
    unittest.main()
