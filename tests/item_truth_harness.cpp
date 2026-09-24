// ForgePact::ItemTruth without a game: build id parsing, record format,
// per-session memory and the journal writer. Run by tests/test_item_truth_behavior.py.
#include <ForgePact/ItemTruth.hpp>

#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace ForgePact::ItemTruth;

static int g_failed = 0;
static void Check(bool ok, const char* label)
{
    std::cout << (ok ? "PASS " : "FAIL ") << label << '\n';
    if (!ok) ++g_failed;
}

template <typename T> static void Put(std::vector<unsigned char>& b, size_t at, T v)
{
    std::memcpy(b.data() + at, &v, sizeof v);
}

// A minimal PE header: DOS stub, NT signature, file header, an empty optional
// header of the usual size and a section table.
static std::vector<unsigned char> FakeImage(uint32_t stamp, std::vector<std::pair<std::string, uint32_t>> sections)
{
    std::vector<unsigned char> b(0x1000, 0);
    b[0] = 'M'; b[1] = 'Z';
    const uint32_t pe = 0x80;
    Put<uint32_t>(b, 0x3C, pe);
    std::memcpy(b.data() + pe, "PE\0\0", 4);
    Put<uint16_t>(b, pe + 4, 0x8664);
    Put<uint16_t>(b, pe + 6, uint16_t(sections.size()));
    Put<uint32_t>(b, pe + 8, stamp);
    Put<uint16_t>(b, pe + 20, 0xF0);
    size_t at = pe + 24 + 0xF0;
    for (const auto& [name, vsize] : sections) {
        std::memcpy(b.data() + at, name.data(), (std::min)(name.size(), size_t(8)));
        Put<uint32_t>(b, at + 8, vsize);
        at += 40;
    }
    return b;
}

static std::vector<std::string> ReadLines(const fs::path& file)
{
    std::ifstream in(file, std::ios::binary);
    std::vector<std::string> lines;
    for (std::string line; std::getline(in, line);) lines.push_back(line);
    return lines;
}

static std::string ReadAll(const fs::path& file)
{
    std::ifstream in(file, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), {});
}

int main()
{
    // ---- build id ----
    {
        auto image = FakeImage(0x6aaa6779, { { ".text", 0x0cad4fc8 }, { ".rdata", 0x100 }, { ".aurie", 0x45000 } });
        Check(BuildIdFromHeaders(image.data(), image.size()) == "pe-6aaa6779-0cad4fc8",
              "build id: link stamp and .text size, the appended .aurie section ignored");
        auto clean = FakeImage(0x6aaa6779, { { ".text", 0x0cad4fc8 }, { ".rdata", 0x100 } });
        Check(BuildIdFromHeaders(clean.data(), clean.size()) == BuildIdFromHeaders(image.data(), image.size()),
              "build id: the clean and the Aurie-patched exe of one build agree");
        auto other = FakeImage(0x6a91a8b3, { { ".text", 0x0caf38b8 } });
        Check(BuildIdFromHeaders(other.data(), other.size()) == "pe-6a91a8b3-0caf38b8", "build id: another build differs");
        auto bad = image; bad[0] = 'X';
        Check(BuildIdFromHeaders(bad.data(), bad.size()).empty(), "build id: no MZ, no id");
        Check(BuildIdFromHeaders(image.data(), 0x90).empty(), "build id: truncated headers, no id");
        auto noText = FakeImage(0x6aaa6779, { { ".data", 0x10 } });
        Check(BuildIdFromHeaders(noText.data(), noText.size()).empty(), "build id: no .text section, no id");
        auto farPe = image; Put<uint32_t>(farPe, 0x3C, 0x7FFFFFF0u);
        Check(BuildIdFromHeaders(farPe.data(), farPe.size()).empty(), "build id: e_lfanew outside the buffer is refused");
        // The in-memory reader and the on-disk bytes of this very harness agree.
        wchar_t self[MAX_PATH]{};
        GetModuleFileNameW(nullptr, self, MAX_PATH);
        const std::string disk = ReadAll(self).substr(0, 4096);
        const std::string fromDisk = BuildIdFromHeaders(reinterpret_cast<const unsigned char*>(disk.data()), disk.size());
        Check(!fromDisk.empty() && fromDisk == BuildIdOfProcess(), "build id: process headers equal the exe file's");
    }

    // ---- numbers ----
    Check(WholeNumberText(212408973073.0) == "212408973073", "number: an itemTimeStamp double prints whole");
    Check(WholeNumberText(3.0) == "3", "number: an itemType");
    Check(WholeNumberText(1.5).empty() && WholeNumberText(-1.0).empty(), "number: fractions and negatives refused");
    Check(WholeNumberText(std::nan("")).empty() && WholeNumberText(1e19).empty() && WholeNumberText(INFINITY).empty(),
          "number: NaN, huge and infinite refused");

    // ---- record ----
    Record r;
    r.build = "pe-6aaa6779-0cad4fc8";
    r.unixMs = 1790000000000;
    r.timestamp = "212409236228";
    r.type = "8";
    r.hash = "d2af\"x\\";
    r.def = R"({"a":107725.0,"b":2.0,"c":0.0,"j":0.0,"o":1.0})";
    r.stats = R"({"154":4.0,"10":[51,1.0,3.0,0],"51":3.0})";
    r.info = R"({"27":1,"28":"Heavy Belt","5":"","4":""})";
    const std::string line = FormatRecord(r);
    Check(line.rfind(R"({"v":1,"src":"live","build":"pe-6aaa6779-0cad4fc8","t":1790000000000,"ts":"212409236228","type":8,)", 0) == 0,
          "record: header fields in order");
    Check(line.find(R"("hash":"d2af\"x\\")") != std::string::npos, "record: strings are JSON-escaped");
    Check(line.find(R"("stats":{"154":4.0,"10":[51,1.0,3.0,0],"51":3.0})") != std::string::npos, "record: stats verbatim");
    Check(line.find("\"native\"") == std::string::npos, "record: no native block when nothing was forged");
    Check(line.back() == '}' && line.find('\n') == std::string::npos, "record: one line");
    Record forged = r;
    forged.native = R"({"154":4.0})";
    Check(FormatRecord(forged).find(R"("native":{"154":4.0})") != std::string::npos, "record: native block when it differs");
    Record same = r;
    same.native = r.stats;
    Check(FormatRecord(same).find("\"native\"") == std::string::npos, "record: native equal to stats is left out");
    Record noType = r; noType.type.clear();
    Check(FormatRecord(noType).find("\"type\":null") != std::string::npos, "record: unknown type is null");
    Record badTs = r; badTs.timestamp = "12a";
    Check(FormatRecord(badTs).empty(), "record: without a timestamp there is nothing to match - refused");
    Record badDef = r; badDef.def = "undefined";
    Check(FormatRecord(badDef).empty(), "record: a definition that is not an object is refused");
    Record splitLine = r; splitLine.stats = "{\"1\":\n2}";
    Check(FormatRecord(splitLine).empty(), "record: a raw line break would split the journal - refused");
    Record badInfo = r; badInfo.info = "[]";
    Check(FormatRecord(badInfo).find("\"info\"") == std::string::npos, "record: info only when it is an object");
    std::string control;
    AppendJsonString(control, std::string("a\x01" "b\n", 4));
    Check(control == "\"a\\u0001b\\n\"", "json: control characters escaped");

    // ---- seen ----
    {
        Seen seen(3);
        Check(seen.Insert("1|h") && !seen.Insert("1|h"), "seen: same item content once per session");
        Check(seen.Insert("1|h2"), "seen: changed content of the same item is written again");
        seen.Insert("2|h"); seen.Insert("3|h");
        Check(seen.Size() <= 3, "seen: bounded");
    }

    // ---- journal ----
    const fs::path root = fs::temp_directory_path() / ("fp_item_truth_" + std::to_string(GetCurrentProcessId()));
    std::error_code ec;
    fs::remove_all(root, ec);
    {
        Journal journal;
        Check(!journal.Enqueue("x"), "journal: nothing is accepted before Start");
        Check(journal.Start(root, "pe-6aaa6779-0cad4fc8", 4242, "9.9.9"), "journal: starts and creates its folder");
        for (int i = 0; i < 250; ++i) journal.Enqueue("{\"n\":" + std::to_string(i) + "}");
        Check(!journal.Enqueue(std::string()), "journal: empty line refused");
        Check(!journal.Enqueue(std::string(kMaxLineBytes + 1, 'x')), "journal: oversized line refused");
        journal.Drain();
        const auto stats = journal.Snapshot();
        Check(stats.written == 250 && stats.dropped == 0 && stats.queued == 0, "journal: every line written");
        std::vector<fs::path> files;
        for (const auto& e : fs::directory_iterator(root / "journal")) files.push_back(e.path());
        Check(files.size() == 1, "journal: one file for the session");
        const std::string name = files.empty() ? std::string() : files[0].filename().string();
        Check(name.rfind("live-pe-6aaa6779-0cad4fc8-", 0) == 0 && name.find("-4242-1.ndjson") != std::string::npos,
              "journal: file named live-<build>-<start>-<pid>-<part>.ndjson");
        const auto lines = files.empty() ? std::vector<std::string>() : ReadLines(files[0]);
        Check(lines.size() == 250 && lines.front() == "{\"n\":0}" && lines.back() == "{\"n\":249}", "journal: lines in order");
        journal.Stop();
        const std::string status = ReadAll(root / "status.json");
        Check(status.find("\"forgepact\":\"9.9.9\"") != std::string::npos && status.find("\"written\":250") != std::string::npos
              && status.find("\"build\":\"pe-6aaa6779-0cad4fc8\"") != std::string::npos,
              "journal: status.json carries version, build and counters");
        Check(!fs::exists(root / "status.json.tmp"), "journal: status written through a temp file");
        Check(!journal.Enqueue("{}"), "journal: nothing is accepted after Stop");
    }
    fs::remove_all(root, ec);
    {
        Journal journal;
        journal.SetRotateBytes(64);
        journal.Start(root, "pe-1", 7, "v");
        for (int i = 0; i < 20; ++i) journal.Enqueue("{\"k\":\"" + std::string(10, char('a' + i)) + "\"}");
        journal.Drain();
        journal.Stop();
        size_t files = 0, total = 0;
        for (const auto& e : fs::directory_iterator(root / "journal")) { ++files; total += ReadLines(e.path()).size(); }
        Check(files > 1 && total == 20, "journal: rotates into numbered parts without losing a line");
        Check(journal.Snapshot().files == files, "journal: part count reported");
    }
    fs::remove_all(root, ec);

    // ---- evaluation requests ----
    {
        Record tagged = r;
        tagged.source = "eval";
        tagged.request = "1790000000000-ab12";
        const std::string evalLine = FormatRecord(tagged);
        Check(evalLine.rfind(R"({"v":1,"src":"eval","req":"1790000000000-ab12","build":)", 0) == 0,
              "eval record: tagged with its request");
        Check(IsItemKey("0-0-212409236228-8") && IsItemKey("1-2-3-14"), "key: region-account-timestamp-type");
        Check(!IsItemKey("0-0-0--1") && !IsItemKey("0-0-0-8") && !IsItemKey("0-0-12-") && !IsItemKey("a-0-1-2")
              && !IsItemKey("0-0-1-2-3") && !IsItemKey(""), "key: placeholder, zero timestamp and junk refused");
        Check(IsRequestId("1790000000000-ab12") && !IsRequestId("../x") && !IsRequestId("") && !IsRequestId(std::string(65, 'a')),
              "request id: a plain file name only");
        size_t rejected = 99;
        const std::string text =
            "0-0-212409236228-8\t{\"a\":107725,\"b\":2,\"c\":0,\"j\":0}\r\n"
            "\n"
            "0-0-0--1\t{\"a\":1}\n"
            "no tab here\n"
            "0-0-5-3\tnot json\n"
            "0-0-6-3\t{\"a\":6,\"b\":1,\"c\":1,\"j\":7}";
        const auto items = ParseRequest(text, 100, &rejected);
        Check(items.size() == 2 && rejected == 3, "request: two good lines, three refused, blank ignored");
        Check(!items.empty() && items[0].key == "0-0-212409236228-8" && items[0].json == "{\"a\":107725,\"b\":2,\"c\":0,\"j\":0}",
              "request: key and json split at the tab, CR dropped");
        Check(items.size() == 2 && items[1].key == "0-0-6-3", "request: a last line without a newline counts");
        size_t capped = 0;
        Check(ParseRequest(text, 1, &capped).size() == 1 && capped == 4, "request: beyond the cap is refused, not truncated silently");
        EvalProgress p;
        p.request = "r1"; p.build = "pe-1"; p.unixMs = 5; p.total = 10; p.done = 4; p.ok = 3; p.failed = 1; p.rejected = 2;
        Check(FormatEvalProgress(p) == R"({"v":1,"kind":"eval","req":"r1","build":"pe-1","t":5,"total":10,"done":4,"ok":3,"failed":1,"rejected":2,"finished":false})",
              "progress: one journal line");
        p.finished = true;
        Check(FormatEvalProgress(p).find("\"finished\":true") != std::string::npos, "progress: the last line says finished");

        const fs::path requests = root / "requests";
        fs::create_directories(requests, ec);
        Check(NextRequest(requests).empty(), "queue: empty folder, nothing to do");
        std::ofstream(requests / "1790000000002-b.req") << "x";
        std::ofstream(requests / "1790000000001-a.req") << "x";
        std::ofstream(requests / "notes.txt") << "x";
        std::ofstream(requests / "bad id!.req") << "x";
        Check(NextRequest(requests).filename() == "1790000000001-a.req", "queue: the oldest request first, others ignored");
        Check(RequestIdOf(requests / "1790000000001-a.req") == "1790000000001-a", "queue: id from the file name");
        std::ofstream(requests / "1790000000000-z.working") << "x";
        Check(AbandonWorking(requests) == 1 && fs::exists(requests / "1790000000000-z.stopped")
              && !fs::exists(requests / "1790000000000-z.working"),
              "queue: an unfinished check is set aside, never resumed on its own");
        Check(NextRequest(requests).filename() == "1790000000001-a.req", "queue: a set-aside check is not picked up");
        fs::remove_all(root, ec);
    }

    // ---- capture request ----
    {
        fs::create_directories(root, ec);
        Check(!CaptureRequested(root), "request: no file, no capture");
        std::ofstream(root / "capture.request") << "{}";
        Check(CaptureRequested(root), "request: the editor's file turns capture on");
        Check(!CaptureRequested(fs::path()), "request: no LOCALAPPDATA, no capture");
        fs::remove_all(root, ec);
        Check(!Root().empty() && Root().filename() == "itemtruth" && Root().parent_path().filename() == "Hero_Siege",
              "paths: %LOCALAPPDATA%\\Hero_Siege\\itemtruth");
    }

    std::cout << (g_failed ? "RESULT FAILED" : "RESULT OK") << '\n';
    return g_failed ? 1 : 0;
}
