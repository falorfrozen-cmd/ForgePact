#pragma once

// ForgePact::ItemTruth - the game's own finished items, journaled for the Item Editor.
//
// Hero Siege saves only an item's compact definition (seeds and ids). Every
// stat line is recomputed by CreateItemNew each time the item is built, so an
// outside tool can only replay that arithmetic - and a replay drifts with every
// game update (measured 2026-09-24 on the 2026-09-16 build: 199 of 755 owned
// items matched the editor's replay line for line). ItemTruth lets the game be
// the referee instead: after the OUTERMOST CreateItemNew returns - random
// stats, runewords, special tables, sockets and the display name all done, and
// ForgePact's own Custom Forge dressing applied - ModuleMain serialises the
// finished struct and hands this module one line:
//
//   {"v":1,"src":"live","build":"pe-6aaa6779-0cad4fc8","t":<unix ms>,
//    "ts":"<itemTimeStamp>","type":<itemType>,"hash":"<itemDataHash>",
//    "def":{itemDefinitionStruct},"stats":{itemStatStruct},
//    "native":{itemStatStruct before the Custom Forge dressing, only when it differs},
//    "info":{itemInfoStruct}}
//
// This header is plain C++ on purpose (no YYToolkit types): the game thread
// does the json_stringify calls, everything here works on finished strings,
// and a background thread does all disk I/O so a loot-heavy frame never waits
// for the disk. Nothing is ever written back into the game. Nothing runs at
// all unless the Item Editor asked for it by creating
// %LOCALAPPDATA%\Hero_Siege\itemtruth\capture.request.
//
// tests/item_truth_harness.cpp exercises every function below without a game.

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ForgePact::ItemTruth {

constexpr int kSchema = 1;
// Back-pressure: beyond this many unwritten lines new ones are dropped and
// counted (status.json "dropped"), never queued without bound.
constexpr size_t kMaxQueuedLines = 20000;
// A single record is a few hundred bytes; anything this large is not an item.
constexpr size_t kMaxLineBytes = 256 * 1024;
// One journal file per session, continued in a new part past this size.
constexpr uintmax_t kRotateBytes = 16ull * 1024 * 1024;
// Per-session memory of what was already written (cleared, not grown, past it).
constexpr size_t kMaxSeen = 200000;
// status.json is rewritten at least this often while the game runs.
constexpr long long kStatusHeartbeatMs = 30000;

// ---- build identity -----------------------------------------------------------
// "pe-<link stamp>-<.text size>" from the executable's own PE headers. Both
// fields are untouched by AuriePatcher, which only appends its .aurie section
// and rewrites the entry point and SizeOfImage, so the patched and the clean
// exe of one game build share one id, and every game update changes it. The
// Item Editor computes the same id from the exe file on disk.
inline std::string BuildIdFromHeaders(const unsigned char* image, size_t size)
{
    if (!image || size < 0x40 || image[0] != 'M' || image[1] != 'Z') return {};
    uint32_t pe = 0;
    std::memcpy(&pe, image + 0x3C, sizeof pe);
    if (pe < 0x40 || size < 24 || pe > size - 24) return {};
    if (std::memcmp(image + pe, "PE\0\0", 4) != 0) return {};
    uint16_t sections = 0, optional = 0;
    uint32_t stamp = 0;
    std::memcpy(&sections, image + pe + 6, sizeof sections);
    std::memcpy(&stamp, image + pe + 8, sizeof stamp);
    std::memcpy(&optional, image + pe + 20, sizeof optional);
    const size_t table = size_t(pe) + 24 + optional;
    for (uint16_t i = 0; i < sections && i < 96; ++i) {
        const size_t at = table + size_t(i) * 40;
        if (at + 40 > size) break;
        if (std::memcmp(image + at, ".text\0\0\0", 8) != 0) continue;
        uint32_t text = 0;
        std::memcpy(&text, image + at + 8, sizeof text);
        if (!stamp || !text) return {};
        char out[32];
        std::snprintf(out, sizeof out, "pe-%08x-%08x", stamp, text);
        return out;
    }
    return {};
}

// The running game's id, read from its mapped headers.
inline std::string BuildIdOfProcess()
{
    const auto* base = reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));
    if (!base) return {};
    MEMORY_BASIC_INFORMATION region{};
    if (!VirtualQuery(base, &region, sizeof region) || region.State != MEM_COMMIT) return {};
    const size_t readable = static_cast<size_t>(
        static_cast<const unsigned char*>(region.BaseAddress) + region.RegionSize - base);
    return BuildIdFromHeaders(base, (std::min)(readable, size_t(0x1000)));
}

// ---- paths --------------------------------------------------------------------
// %LOCALAPPDATA%\Hero_Siege\itemtruth - next to the Item Editor's Vault and the
// AFK spool, independent of which game folder is running. Wide API: a user
// profile path with non-ASCII letters must still resolve.
inline std::filesystem::path Root()
{
    std::vector<wchar_t> buffer(32768);
    const DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer.data(), DWORD(buffer.size()));
    if (n == 0 || n >= buffer.size()) return {};
    return std::filesystem::path(std::wstring(buffer.data(), n)) / L"Hero_Siege" / L"itemtruth";
}

inline bool CaptureRequested(const std::filesystem::path& root)
{
    if (root.empty()) return false;
    std::error_code ec;
    return std::filesystem::is_regular_file(root / L"capture.request", ec);
}

// ---- record formatting --------------------------------------------------------
inline void AppendJsonString(std::string& out, std::string_view text)
{
    out.push_back('"');
    for (const unsigned char c : text) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char escaped[8];
                std::snprintf(escaped, sizeof escaped, "\\u%04x", unsigned(c));
                out += escaped;
            } else {
                out.push_back(char(c));
            }
        }
    }
    out.push_back('"');
}

// json_stringify output of a struct: one line, an object. A raw line break
// would split the NDJSON record, so it disqualifies the text.
inline bool IsObjectText(std::string_view text)
{
    return text.size() >= 2 && text.front() == '{' && text.back() == '}'
        && text.find('\n') == std::string_view::npos && text.find('\r') == std::string_view::npos;
}

inline bool IsDigits(std::string_view text)
{
    return !text.empty() && text.size() <= 20
        && std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; });
}

// A finite, non-negative whole number as decimal digits ("" otherwise). The
// game keeps itemTimeStamp and itemType as doubles.
inline std::string WholeNumberText(double value)
{
    if (!(value >= 0.0) || value >= 1e18 || value != static_cast<double>(static_cast<long long>(value)))
        return {};
    return std::to_string(static_cast<long long>(value));
}

struct Record {
    std::string source = "live";
    std::string request;     // evaluation request id ("" for items the game built on its own)
    std::string build;
    long long unixMs = 0;
    std::string timestamp;   // itemTimeStamp, decimal digits
    std::string type;        // itemType, decimal digits ("" -> null)
    std::string hash;        // itemDataHash, the game's own content hash
    std::string def, stats, native, info;   // json_stringify of the structs
};

// One NDJSON line, or "" when the record cannot identify an item.
inline std::string FormatRecord(const Record& r)
{
    if (!IsDigits(r.timestamp) || !IsObjectText(r.def) || !IsObjectText(r.stats)) return {};
    std::string out;
    out.reserve(160 + r.def.size() + r.stats.size() + r.native.size() + r.info.size());
    out += "{\"v\":";
    out += std::to_string(kSchema);
    out += ",\"src\":";
    AppendJsonString(out, r.source);
    if (!r.request.empty()) {
        out += ",\"req\":";
        AppendJsonString(out, r.request);
    }
    out += ",\"build\":";
    AppendJsonString(out, r.build);
    out += ",\"t\":";
    out += std::to_string(r.unixMs);
    out += ",\"ts\":";
    AppendJsonString(out, r.timestamp);
    out += ",\"type\":";
    out += IsDigits(r.type) ? r.type : std::string("null");
    out += ",\"hash\":";
    AppendJsonString(out, r.hash);
    out += ",\"def\":";
    out += r.def;
    out += ",\"stats\":";
    out += r.stats;
    if (IsObjectText(r.native) && r.native != r.stats) {
        out += ",\"native\":";
        out += r.native;
    }
    if (IsObjectText(r.info)) {
        out += ",\"info\":";
        out += r.info;
    }
    out += '}';
    return out.size() <= kMaxLineBytes ? out : std::string();
}

inline long long UnixMsNow()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

// ---- the game's own tooltip text ----------------------------------------------
// While capture is on, the first time the game draws an item's inventory tooltip
// in a session, ModuleMain records what it drew: every text row in draw order
// (the draw_text_outline(_ext) arguments, the draw colour and alignment, and the
// stat whose DrawInventoryStatsNew call drew it) and each stat call that drew a
// row. The arguments arrive as JSON fragments; strings in them are already
// escaped. Once per session one tooltip's complete stat table is recorded too -
// the game calls DrawInventoryStatsNew for every stat it knows, drawn or not.
//   {"v":1,"kind":"tooltip","build":...,"t":...,"ts":"...","hash":"...",
//    "args":[...],"rows":[{"fn":"o","s":28,"c":16777215,"ha":1,"a":[...]}],
//    "stats":[{"id":28,"h":30,"a":[...]}]}
//   {"v":1,"kind":"tooltip-table","build":...,"t":...,"stats":[...]}
// A tooltip drawn for a drawing request (below) also carries "req":"<id>".
inline bool IsRequestId(std::string_view id);
inline std::string FormatTooltipRecord(const std::string& build, long long unixMs, const std::string& timestamp,
                                       const std::string& hash, const std::string& argsJson,
                                       const std::string& rowsJson, const std::string& statsJson,
                                       const std::string& drawnBy = std::string(),
                                       const std::string& request = std::string())
{
    if (!IsDigits(timestamp) || argsJson.empty() || argsJson.front() != '[') return {};
    std::string out = "{\"v\":" + std::to_string(kSchema) + ",\"kind\":\"tooltip\",\"build\":";
    AppendJsonString(out, build);
    out += ",\"t\":" + std::to_string(unixMs) + ",\"ts\":";
    AppendJsonString(out, timestamp);
    out += ",\"hash\":";
    AppendJsonString(out, hash);
    if (!drawnBy.empty()) {
        out += ",\"by\":";
        AppendJsonString(out, drawnBy);
    }
    if (IsRequestId(request)) {
        out += ",\"req\":";
        AppendJsonString(out, request);
    }
    out += ",\"args\":" + argsJson + ",\"rows\":[" + rowsJson + "],\"stats\":[" + statsJson + "]}";
    if (out.find('\n') != std::string::npos || out.find('\r') != std::string::npos) return {};
    return out.size() <= kMaxLineBytes ? out : std::string();
}

inline std::string FormatTooltipTable(const std::string& build, long long unixMs, const std::string& statsJson)
{
    std::string out = "{\"v\":" + std::to_string(kSchema) + ",\"kind\":\"tooltip-table\",\"build\":";
    AppendJsonString(out, build);
    out += ",\"t\":" + std::to_string(unixMs) + ",\"stats\":[" + statsJson + "]}";
    if (out.find('\n') != std::string::npos || out.find('\r') != std::string::npos) return {};
    return out.size() <= kMaxLineBytes ? out : std::string();
}

// ---- evaluation requests -------------------------------------------------------
// The Item Editor asks the game to build items it has not built yet:
// <root>\requests\<id>.req, one item per line, "<item key>\t<save data json>"
// (the key as the save stores it, "<region>-<account>-<itemTimeStamp>-<type>").
// ModuleMain hands each line to the game's own save loader, never drops or
// saves the result, and journals the finished item like a live one with
// "src":"eval" and "req":"<id>". Progress lines go into the same journal, so
// the editor reads records and progress in order:
//   {"v":1,"kind":"eval","req":"<id>","build":"...","t":<ms>,
//    "total":N,"done":N,"ok":N,"failed":N,"rejected":N,"finished":true|false}
// Drawing requests, <root>\tips\<id>.req, have the same lines: the game builds
// each item the same way and draws its tooltip off screen while the player has
// a tooltip open, so the editor gets the game's own text for items the player
// never hovers. Their progress lines are "kind":"tipdraw".
constexpr size_t kMaxEvalItems = 50000;
constexpr size_t kMaxRequestBytes = 64ull * 1024 * 1024;

struct EvalItem {
    std::string key;
    std::string json;
};

// Request ids name files: 1..64 of [A-Za-z0-9_-].
inline bool IsRequestId(std::string_view id)
{
    return !id.empty() && id.size() <= 64 && std::all_of(id.begin(), id.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '-';
    });
}

// "<digits>-<digits>-<digits>-<digits>", the timestamp not zero.
inline bool IsItemKey(std::string_view key)
{
    size_t part = 0, digits = 0;
    std::string_view stamp;
    size_t stampStart = 0;
    for (size_t i = 0; i <= key.size(); ++i) {
        if (i == key.size() || key[i] == '-') {
            if (digits == 0 || digits > 20) return false;
            if (part == 2) stamp = key.substr(stampStart, digits);
            ++part;
            digits = 0;
            stampStart = i + 1;
            continue;
        }
        if (key[i] < '0' || key[i] > '9') return false;
        ++digits;
    }
    return part == 4 && stamp.find_first_not_of('0') != std::string_view::npos;
}

// Valid lines of a request, in order; malformed ones are counted, not guessed at.
inline std::vector<EvalItem> ParseRequest(std::string_view text, size_t maxItems, size_t* rejected)
{
    std::vector<EvalItem> items;
    size_t bad = 0;
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string_view::npos) end = text.size();
        std::string_view line = text.substr(start, end - start);
        start = end + 1;
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (line.empty()) continue;
        const size_t tab = line.find('\t');
        const std::string_view key = tab == std::string_view::npos ? std::string_view() : line.substr(0, tab);
        const std::string_view json = tab == std::string_view::npos ? std::string_view() : line.substr(tab + 1);
        if (items.size() >= maxItems || !IsItemKey(key) || !IsObjectText(json) || json.size() > kMaxLineBytes) {
            ++bad;
            continue;
        }
        items.push_back({ std::string(key), std::string(json) });
    }
    if (rejected) *rejected = bad;
    return items;
}

struct EvalProgress {
    std::string request, build;
    long long unixMs = 0;
    size_t total = 0, done = 0, ok = 0, failed = 0, rejected = 0;
    bool finished = false;
    bool drawing = false;   // a drawing request ("tipdraw") rather than a build check ("eval")
};

inline std::string FormatEvalProgress(const EvalProgress& p)
{
    std::string out = "{\"v\":" + std::to_string(kSchema) + ",\"kind\":\""
        + (p.drawing ? "tipdraw" : "eval") + "\",\"req\":";
    AppendJsonString(out, p.request);
    out += ",\"build\":";
    AppendJsonString(out, p.build);
    out += ",\"t\":" + std::to_string(p.unixMs) + ",\"total\":" + std::to_string(p.total)
        + ",\"done\":" + std::to_string(p.done) + ",\"ok\":" + std::to_string(p.ok)
        + ",\"failed\":" + std::to_string(p.failed) + ",\"rejected\":" + std::to_string(p.rejected)
        + ",\"finished\":" + (p.finished ? "true" : "false") + "}";
    return out;
}

// The request id a file name carries, or "" (never throws on a foreign name).
inline std::string RequestIdOf(const std::filesystem::path& path)
{
    const std::wstring stem = path.stem().wstring();
    if (!std::all_of(stem.begin(), stem.end(), [](wchar_t c) { return c > 0 && c < 128; })) return {};
    std::string id;
    id.reserve(stem.size());
    for (const wchar_t c : stem) id.push_back(static_cast<char>(c));
    return IsRequestId(id) ? id : std::string();
}

// The oldest pending request (ids start with a time stamp, so names sort by age),
// or an empty path.
inline std::filesystem::path NextRequest(const std::filesystem::path& dir)
{
    std::error_code ec;
    std::filesystem::path best;
    if (!std::filesystem::is_directory(dir, ec)) return best;
    for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        const std::filesystem::path path = it->path();
        if (path.extension() != L".req" || RequestIdOf(path).empty()) continue;
        if (best.empty() || path.filename() < best.filename()) best = path;
    }
    return best;
}

// A request that was being built when the game closed - or crashed - is set
// aside as <id>.stopped, never resumed on its own: if one of its items is what
// brought the game down, resuming would bring it down on every start. The
// editor sees the file and decides.
inline size_t AbandonWorking(const std::filesystem::path& dir)
{
    std::error_code ec;
    size_t moved = 0;
    if (!std::filesystem::is_directory(dir, ec)) return 0;
    std::vector<std::filesystem::path> working;
    for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
        if (it->path().extension() == L".working") working.push_back(it->path());
    for (const auto& path : working) {
        std::filesystem::path stopped = path;
        stopped.replace_extension(L".stopped");
        std::error_code move;
        std::filesystem::rename(path, stopped, move);
        if (!move) ++moved;
    }
    return moved;
}

// ---- per-session memory -------------------------------------------------------
// The game rebuilds the same item on many occasions. One line per distinct
// (itemTimeStamp, content hash) per session is enough; the editor keeps the rest.
class Seen {
public:
    explicit Seen(size_t cap = kMaxSeen) : cap_(cap) {}
    bool Insert(const std::string& key)
    {
        if (set_.size() >= cap_) set_.clear();
        return set_.insert(key).second;
    }
    size_t Size() const { return set_.size(); }

private:
    size_t cap_;
    std::unordered_set<std::string> set_;
};

// ---- journal ------------------------------------------------------------------
// live-<build>-<yyyymmdd-HHMMSS>-<pid>-<part>.ndjson under <root>\journal, and
// <root>\status.json for the editor's status line. The game thread only calls
// Enqueue; the writer thread owns every file handle.
//
// Lifetime: ModuleMain creates its Journal with `new` and never deletes it. A
// static Journal would be destroyed at process exit with its thread still
// joinable, which calls std::terminate. Tests call Stop() instead.
class Journal {
public:
    struct Stats {
        unsigned long long written = 0;
        unsigned long long dropped = 0;
        unsigned long long files = 0;
        size_t queued = 0;
        std::string file;
    };

    bool Start(const std::filesystem::path& root, std::string build, unsigned long pid, std::string version)
    {
        std::lock_guard<std::mutex> lock(mu_);
        if (thread_.joinable()) return true;
        std::error_code ec;
        std::filesystem::create_directories(root / L"journal", ec);
        if (ec) return false;
        root_ = root;
        build_ = build.empty() ? std::string("unknown") : std::move(build);
        version_ = std::move(version);
        pid_ = pid;
        startedMs_ = UnixMsNow();
        std::time_t now = std::time(nullptr);
        std::tm local{};
        localtime_s(&local, &now);
        char stamp[32];
        std::strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", &local);
        stamp_ = stamp;
        stop_ = false;
        statusDirty_ = true;
        thread_ = std::thread([this] { Run(); });
        return true;
    }

    // Game thread. Never touches the disk; false when the line was refused.
    bool Enqueue(std::string line)
    {
        if (line.empty() || line.size() > kMaxLineBytes) return false;
        {
            std::lock_guard<std::mutex> lock(mu_);
            if (!thread_.joinable() || stop_) return false;
            if (queue_.size() >= kMaxQueuedLines) { ++dropped_; statusDirty_ = true; return false; }
            queue_.push_back(std::move(line));
        }
        cv_.notify_one();
        return true;
    }

    Stats Snapshot()
    {
        std::lock_guard<std::mutex> lock(mu_);
        Stats s;
        s.written = written_;
        s.dropped = dropped_;
        s.files = files_;
        s.queued = queue_.size();
        s.file = fileName_;
        return s;
    }

    // Tests: block until every queued line is on disk.
    void Drain()
    {
        for (int i = 0; i < 400; ++i) {
            {
                std::lock_guard<std::mutex> lock(mu_);
                if (queue_.empty() && !writing_) return;
            }
            cv_.notify_one();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    // Tests only (see the lifetime note above).
    void Stop()
    {
        {
            std::lock_guard<std::mutex> lock(mu_);
            stop_ = true;
        }
        cv_.notify_one();
        if (thread_.joinable()) thread_.join();
    }

    // Tests: rotate after this many bytes instead of kRotateBytes.
    void SetRotateBytes(uintmax_t bytes) { std::lock_guard<std::mutex> lock(mu_); rotateBytes_ = bytes; }

private:
    void Run()
    {
        std::vector<std::string> batch;
        for (;;) {
            bool stopping = false;
            {
                std::unique_lock<std::mutex> lock(mu_);
                cv_.wait_for(lock, std::chrono::milliseconds(500), [&] { return stop_ || !queue_.empty(); });
                batch.assign(std::make_move_iterator(queue_.begin()), std::make_move_iterator(queue_.end()));
                queue_.clear();
                writing_ = !batch.empty();
                stopping = stop_;
            }
            if (!batch.empty()) WriteBatch(batch);
            batch.clear();
            WriteStatusIfDue(stopping);
            {
                std::lock_guard<std::mutex> lock(mu_);
                writing_ = false;
                if (stop_ && queue_.empty()) break;
            }
        }
        if (out_.is_open()) out_.close();
    }

    void OpenNext()
    {
        if (out_.is_open()) out_.close();
        ++part_;
        const std::string name = "live-" + build_ + "-" + stamp_ + "-" + std::to_string(pid_) + "-"
            + std::to_string(part_) + ".ndjson";
        out_.open(root_ / L"journal" / std::filesystem::path(name), std::ios::binary | std::ios::app);
        size_ = 0;
        std::lock_guard<std::mutex> lock(mu_);
        fileName_ = out_.is_open() ? name : std::string();
        if (out_.is_open()) ++files_;
        statusDirty_ = true;
    }

    void WriteBatch(const std::vector<std::string>& batch)
    {
        uintmax_t rotate = 0;
        {
            std::lock_guard<std::mutex> lock(mu_);
            rotate = rotateBytes_;
        }
        if (!out_.is_open()) OpenNext();
        unsigned long long wrote = 0;
        for (const std::string& line : batch) {
            if (!out_.is_open()) break;
            out_.write(line.data(), std::streamsize(line.size()));
            out_.put('\n');
            size_ += line.size() + 1;
            ++wrote;
            if (size_ >= rotate) { out_.flush(); OpenNext(); }
        }
        if (out_.is_open()) out_.flush();
        std::lock_guard<std::mutex> lock(mu_);
        written_ += wrote;
        dropped_ += batch.size() - wrote;
        statusDirty_ = true;
    }

    void WriteStatusIfDue(bool force)
    {
        const long long now = UnixMsNow();
        std::string body;
        {
            std::lock_guard<std::mutex> lock(mu_);
            // Changes at most every 2 s, and a heartbeat every 30 s even without
            // changes: the editor treats a status older than two minutes as "the
            // game is not running", and only then stops asking it for checks.
            const bool heartbeat = now - lastStatusMs_ >= kStatusHeartbeatMs;
            if ((!statusDirty_ && !heartbeat) || (!force && !heartbeat && now - lastStatusMs_ < 2000)) return;
            statusDirty_ = false;
            lastStatusMs_ = now;
            body = "{\"schema\":" + std::to_string(kSchema) + ",\"forgepact\":";
            AppendJsonString(body, version_);
            body += ",\"build\":";
            AppendJsonString(body, build_);
            body += ",\"pid\":" + std::to_string(pid_) + ",\"started\":" + std::to_string(startedMs_)
                + ",\"updated\":" + std::to_string(now) + ",\"written\":" + std::to_string(written_)
                + ",\"dropped\":" + std::to_string(dropped_) + ",\"file\":";
            AppendJsonString(body, fileName_);
            body += "}";
        }
        std::error_code ec;
        const std::filesystem::path target = root_ / L"status.json";
        const std::filesystem::path temp = root_ / L"status.json.tmp";
        {
            std::ofstream f(temp, std::ios::binary | std::ios::trunc);
            if (!f) return;
            f << body;
        }
        std::filesystem::rename(temp, target, ec);
        if (ec) std::filesystem::remove(temp, ec);
    }

    std::mutex mu_;
    std::condition_variable cv_;
    std::deque<std::string> queue_;
    std::thread thread_;
    bool stop_ = false;
    bool writing_ = false;
    bool statusDirty_ = false;
    std::filesystem::path root_;
    std::string build_, version_, stamp_, fileName_;
    unsigned long pid_ = 0;
    long long startedMs_ = 0, lastStatusMs_ = 0;
    unsigned long long written_ = 0, dropped_ = 0, files_ = 0;
    uintmax_t rotateBytes_ = kRotateBytes;
    // Writer thread only:
    std::ofstream out_;
    uintmax_t size_ = 0;
    unsigned part_ = 0;
};

} // namespace ForgePact::ItemTruth
