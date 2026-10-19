// The crash trap (ForgePact issue #223) without a game.
//
// Compiles plugin/include/ForgePact/CrashTrap.hpp whole and drives it in this
// process and in child processes of this exe:
//
//   * in-process, with the trap installed into a work directory: a C++
//     exception thrown and caught here is remembered as this thread's type
//     and writes no line; an access violation under __try writes one
//     `route=first-chance` line with the tags set here, and this process
//     carries on; an exception code that is not fatal writes nothing; the
//     vectored route answers EXCEPTION_CONTINUE_SEARCH to everything; its
//     lines stop at kMaxFirstChanceLines.
//   * children, each with an ipc directory of its own and with Windows'
//     crash dialog off (SEM_NOGPFAULTERRORBOX): one dies of an access
//     violation, one of an unhandled C++ exception. The parent then finds a
//     `route=unhandled` line (with the type, for the second), a crash.dmp
//     that starts with the minidump signature, and the copies of the two fake
//     logs, which the child held open for writing as a logger does. A third
//     child starts in the first one's directory and ends cleanly: its install
//     must have rotated the crashed session's folder to crashtrap.prev.
//
// The records are read back through the incident monitor's own parser, so
// the writer and the next-load fold are tested against each other.
//
// Usage:  crash_trap_harness.exe <work dir>
//         crash_trap_harness.exe --child <av|cpp|clean> <ipc dir> <log dir>
//
// Each scenario prints one line, `<name> | <pass|fail> | <detail>`, and the
// process exits 0 only when every scenario passed.
// tests/test_crash_trap_behavior.py compiles and runs it.

#include <ForgePact/CrashTrap.hpp>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace trap = ForgePact::CrashTrap;
namespace inc = ForgePact::Incident;
namespace fs = std::filesystem;

namespace {

int g_Failures = 0;

void Report(const char* name, bool ok, const std::string& detail)
{
    std::printf("%s | %s | %s\n", name, ok ? "pass" : "fail", detail.c_str());
    std::fflush(stdout);
    if (!ok) ++g_Failures;
}

std::string ReadAll(const fs::path& p)
{
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

// The record's exception lines, in order.
std::vector<std::string> ExceptionLines(const fs::path& record)
{
    std::vector<std::string> lines;
    const std::string text = ReadAll(record);
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(at, end - at);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("exception 0x", 0) == 0) lines.push_back(line);
        at = end + 1;
    }
    return lines;
}

std::string ExeName()
{
    char path[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    const char* slash = std::strrchr(path, '\\');
    return slash ? slash + 1 : path;
}

// No C++ object in these two: __try needs that.
bool AccessViolationUnderTry()
{
    __try {
        int* volatile target = nullptr;
        *target = 223;
        return false;
    } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        return true;
    }
}

bool CustomCodeUnderTry(DWORD code)
{
    __try {
        RaiseException(code, 0, 0, nullptr);
        return false;
    } __except (GetExceptionCode() == code ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        return true;
    }
}

// ---- in-process scenarios ---------------------------------------------------

void CppTypeRemembered(const fs::path& record)
{
    const size_t before = ExceptionLines(record).size();
    const std::string fresh = trap::LastCppType();
    bool caught = false;
    try {
        throw std::runtime_error("harness");
    } catch (...) {
        caught = true;
    }
    const std::string after = trap::LastCppType();
    std::string elsewhere;
    std::thread other([&elsewhere] { elsewhere = trap::LastCppType(); });
    other.join();
    const size_t lines = ExceptionLines(record).size();
    const bool ok = caught && fresh == "unknown" && after.find("runtime_error") != std::string::npos
                    && elsewhere == "unknown" && lines == before;
    Report("cpp-type-remembered", ok, "before " + fresh + " | after " + after + " | another thread " + elsewhere
           + " | lines " + std::to_string(before) + " -> " + std::to_string(lines));
}

void FirstChanceLine(const fs::path& record)
{
    const size_t before = ExceptionLines(record).size();
    trap::NoteGameThread(GetCurrentThreadId());
    bool carriedOn = false;
    {
        inc::IncidentScope scope(inc::Mod::drops);
        inc::IncidentHookTag tag("harness_hook");
        carriedOn = AccessViolationUnderTry();
    }
    const std::vector<std::string> lines = ExceptionLines(record);
    std::string line = lines.size() == before + 1 ? lines.back() : std::string("(") + std::to_string(lines.size() - before) + " new lines)";
    inc::TrapRecord parsed;
    const bool read = inc::ParseTrapRecord(line + "\n", parsed);
    const bool ok = carriedOn && lines.size() == before + 1 && read && parsed.exceptionCode == "0xC0000005"
                    && parsed.module == ExeName() && parsed.offset.rfind("0x", 0) == 0 && parsed.thread == "game"
                    && parsed.threadId == GetCurrentThreadId() && parsed.inHook == "harness_hook"
                    && parsed.inMod == "drops" && !parsed.gameOriginal && parsed.cppType.empty()
                    && parsed.route == "first-chance";
    Report("first-chance-av-line", ok, std::string(carriedOn ? "carried on" : "did not carry on") + " | " + line);
}

void NonFatalQuiet(const fs::path& record)
{
    const size_t before = ExceptionLines(record).size();
    const bool handled = CustomCodeUnderTry(0x20000223);
    const bool breakpoint = CustomCodeUnderTry(static_cast<DWORD>(EXCEPTION_BREAKPOINT));
    const size_t after = ExceptionLines(record).size();
    Report("non-fatal-quiet", handled && breakpoint && after == before,
           "custom code and a breakpoint handled here, lines " + std::to_string(before) + " -> " + std::to_string(after));
}

// The vectored route asked directly: whatever the code, it continues the
// search; past the cap it writes nothing more.
void ContinueSearchOnly(const fs::path& record)
{
    EXCEPTION_RECORD r{};
    CONTEXT context{};
    EXCEPTION_POINTERS info{ &r, &context };
    const DWORD codes[] = { EXCEPTION_ACCESS_VIOLATION, trap::kCppExceptionCode, 0x20000223, EXCEPTION_STACK_OVERFLOW,
                            trap::kHeapCorruption, EXCEPTION_BREAKPOINT };
    bool always = true;
    std::string answers;
    for (const DWORD code : codes) {
        r.ExceptionCode = code;
        r.ExceptionAddress = reinterpret_cast<PVOID>(&ContinueSearchOnly);
        const LONG answer = trap::FirstChance(&info);
        always = always && answer == EXCEPTION_CONTINUE_SEARCH;
        answers += std::to_string(answer) + " ";
    }
    // A null record and a null pointer are answered too.
    always = always && trap::FirstChance(nullptr) == EXCEPTION_CONTINUE_SEARCH;
    EXCEPTION_POINTERS empty{ nullptr, &context };
    always = always && trap::FirstChance(&empty) == EXCEPTION_CONTINUE_SEARCH;
    r.ExceptionCode = EXCEPTION_ACCESS_VIOLATION;
    for (long i = 0; i < trap::kMaxFirstChanceLines + 8; ++i) trap::FirstChance(&info);
    const size_t lines = ExceptionLines(record).size();
    const bool capped = lines == static_cast<size_t>(trap::kMaxFirstChanceLines);
    const std::string status = trap::StatusLine();
    const bool stat = status.rfind("incident: trap ", 0) == 0 && status.find("first-chance armed") != std::string::npos
                      && status.find("unhandled armed") != std::string::npos;
    Report("continue-search-only", always && capped && stat,
           "answers " + answers + "| lines " + std::to_string(lines) + " of " + std::to_string(trap::kMaxFirstChanceLines)
           + " | " + status);
}

// ---- children -----------------------------------------------------------------

constexpr char kYytkText[] = "YYToolkit 4.0.1 banner\r\n[hs] the crashed session's last YYToolkit line\r\n";
constexpr char kAurieText[] = "[ArProcessAttach] Init done\r\nthe crashed session's last aurie line\r\n";

void WriteFake(const fs::path& path, const char* text)
{
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f << text;
}

[[noreturn]] void ThrowUnhandled()
{
    throw std::runtime_error("harness: nobody catches this");
}

int Child(const std::string& how, const std::string& ipcDir, const std::string& logDir)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    const std::string line = trap::Install(ipcDir, logDir);
    std::printf("%s\n", line.c_str());
    std::fflush(stdout);
    if (how == "clean") return 0;
    // The logs are open for writing when the game dies, as a logger keeps them.
    const auto open = [](const std::string& path) {
        return CreateFileA(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    };
    const HANDLE yytk = open(logDir + "\\YYToolkit.log");
    const HANDLE aurie = open(logDir + "\\aurie.log");
    if (yytk == INVALID_HANDLE_VALUE || aurie == INVALID_HANDLE_VALUE) return 0x7E1;
    trap::NoteGameThread(GetCurrentThreadId());
    inc::IncidentScope scope(inc::Mod::mapreveal);
    inc::IncidentHookTag tag("harness_child_hook");
    if (how == "cpp") ThrowUnhandled();
    int* volatile target = nullptr;
    *target = 223;
    return 0x7E2;
}

struct ChildResult {
    DWORD exitCode = 0;
    bool finished = false;
    std::string output;
};

ChildResult RunChild(const std::string& how, const fs::path& ipcDir, const fs::path& logDir)
{
    ChildResult result;
    char exe[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    const fs::path out = ipcDir.parent_path() / (ipcDir.filename().string() + "-" + how + ".out");
    std::string command = "\"" + std::string(exe) + "\" --child " + how + " \"" + ipcDir.string() + "\" \"" + logDir.string() + "\"";
    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    const HANDLE file = CreateFileA(out.string().c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = file;
    si.hStdError = file;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION pi{};
    std::vector<char> buffer(command.begin(), command.end());
    buffer.push_back('\0');
    // The child inherits this process's error mode, so no crash dialog either way.
    if (!CreateProcessA(nullptr, buffer.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
        if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
        result.output = "CreateProcess failed " + std::to_string(GetLastError());
        return result;
    }
    if (WaitForSingleObject(pi.hProcess, 90000) == WAIT_OBJECT_0) {
        result.finished = true;
        GetExitCodeProcess(pi.hProcess, &result.exitCode);
    } else {
        TerminateProcess(pi.hProcess, 0x7EE);
        WaitForSingleObject(pi.hProcess, 5000);
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    result.output = ReadAll(out);
    while (!result.output.empty() && (result.output.back() == '\n' || result.output.back() == '\r')) result.output.pop_back();
    return result;
}

std::string Hex(DWORD v)
{
    char b[16];
    std::snprintf(b, sizeof(b), "0x%08lX", static_cast<unsigned long>(v));
    return b;
}

// What a crashed child left in <ipc>\crashtrap: the record's unhandled line,
// the dump, the two log copies.
struct Left {
    bool read = false;
    inc::TrapRecord record;
    bool dump = false;
    unsigned long long dumpBytes = 0;
    bool logs = false;
    std::string detail;
};

Left Inspect(const fs::path& dir)
{
    Left left;
    const std::string text = ReadAll(dir / trap::kRecordFile);
    left.read = inc::ParseTrapRecord(text, left.record);
    const std::string dump = ReadAll(dir / trap::kDumpFile);
    left.dumpBytes = dump.size();
    left.dump = dump.size() > 4 && dump.compare(0, 4, "MDMP") == 0;
    left.logs = ReadAll(dir / trap::kYytkLogFile) == kYytkText && ReadAll(dir / trap::kAurieLogFile) == kAurieText;
    left.detail = "dump " + std::string(left.dump ? "MDMP " : "missing ") + std::to_string(left.dumpBytes)
                  + " bytes | logs " + (left.logs ? "copied" : "missing") + " | line "
                  + (ExceptionLines(dir / trap::kRecordFile).empty() ? std::string("none")
                                                                     : ExceptionLines(dir / trap::kRecordFile).back());
    return left;
}

void UnhandledChildren(const fs::path& work)
{
    const fs::path logs = work / "logs";
    std::error_code ec;
    fs::create_directories(logs, ec);
    WriteFake(logs / "YYToolkit.log", kYytkText);
    WriteFake(logs / "aurie.log", kAurieText);

    const fs::path avIpc = work / "child-av";
    const fs::path cppIpc = work / "child-cpp";
    fs::remove_all(avIpc, ec);
    fs::remove_all(cppIpc, ec);
    fs::create_directories(avIpc, ec);
    fs::create_directories(cppIpc, ec);

    const ChildResult av = RunChild("av", avIpc, logs);
    const Left avLeft = Inspect(avIpc / trap::kDirName);
    const inc::TrapRecord& a = avLeft.record;
    const bool avOk = av.finished && av.exitCode == EXCEPTION_ACCESS_VIOLATION && avLeft.read && a.route == "unhandled"
                      && a.exceptionCode == "0xC0000005" && a.module == ExeName() && a.thread == "game"
                      && a.inHook == "harness_child_hook" && a.inMod == "mapreveal" && a.cppType.empty()
                      && avLeft.dump && avLeft.logs;
    Report("unhandled-av-child", avOk, "exit " + Hex(av.exitCode) + " | " + avLeft.detail + " | child said: " + av.output);

    const ChildResult cpp = RunChild("cpp", cppIpc, logs);
    const Left cppLeft = Inspect(cppIpc / trap::kDirName);
    const inc::TrapRecord& c = cppLeft.record;
    const bool cppOk = cpp.finished && cpp.exitCode != 0 && cppLeft.read && c.route == "unhandled"
                       && c.exceptionCode == "0xE06D7363" && c.cppType.find("runtime_error") != std::string::npos
                       && c.thread == "game" && c.inMod == "mapreveal" && cppLeft.dump && cppLeft.logs;
    Report("unhandled-cpp-child", cppOk, "exit " + Hex(cpp.exitCode) + " | " + cppLeft.detail + " | child said: " + cpp.output);

    // The next session in the same directory: the crashed one moves to .prev
    // whole, and the new folder holds only its own session line.
    const ChildResult clean = RunChild("clean", avIpc, logs);
    const Left prev = Inspect(avIpc / trap::kPrevDirName);
    const std::string fresh = ReadAll(avIpc / trap::kDirName / trap::kRecordFile);
    const bool rotated = clean.finished && clean.exitCode == 0 && prev.read && prev.record.route == "unhandled" && prev.dump
                         && prev.logs && fresh.rfind("session ", 0) == 0 && ExceptionLines(avIpc / trap::kDirName / trap::kRecordFile).empty()
                         && !fs::exists(avIpc / trap::kDirName / trap::kDumpFile);
    Report("rotation", rotated, "prev: " + prev.detail + " | fresh: " + inc::FirstLine(fresh));
}

} // namespace

int main(int argc, char** argv)
{
    if (argc == 5 && std::string(argv[1]) == "--child") return Child(argv[2], argv[3], argv[4]);
    if (argc < 2) {
        std::printf("usage: crash_trap_harness <work dir>\n");
        return 2;
    }
    const fs::path work = argv[1];
    std::error_code ec;
    fs::remove_all(work / "inproc", ec);
    fs::create_directories(work / "inproc", ec);
    fs::create_directories(work / "logs", ec);
    const std::string installed = trap::Install((work / "inproc").string(), (work / "logs").string());
    const fs::path record = work / "inproc" / trap::kDirName / trap::kRecordFile;
    Report("install", installed.rfind("incident: trap ", 0) == 0 && fs::exists(record), installed);
    CppTypeRemembered(record);
    FirstChanceLine(record);
    NonFatalQuiet(record);
    ContinueSearchOnly(record);
    UnhandledChildren(work);
    std::printf("RESULT %s\n", g_Failures ? "FAIL" : "OK");
    return g_Failures ? 1 : 0;
}
