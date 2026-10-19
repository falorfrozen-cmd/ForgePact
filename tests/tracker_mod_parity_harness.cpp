// Parity harness for the HS Offline Tracker sensor (ForgePact #185): the
// plugin's half of tests/test_tracker_parity.py.
//
// The Python runner injects the REAL ForgePact::TrackerMod header below, as
// tests/tracker_mod_harness.cpp's runner does, and passes two paths: the
// shared script (tests/fixtures/tracker/parity-script.txt) and a directory.
// The harness plays the script through the core and writes the session file
// through the core's own Win32 writer, TrackerJournalFile, under
// <directory>\tracker\. It then prints what the sensor itself knows, for the
// test to hold the panel's ledger against:
//
//   FILE <path>                    the session file
//   STAT <tracker stat line>       gold-sum, xp-sum, kills-sum, the drop counts
//                                  and the zone's (zone=, zones-written=)
//   RECORDED <rarity>\t<name>      each drop the core recorded, in order
//   ROOM <name>                    each room the core recorded, in order
//   RESULT OK | RESULT FAIL <why>
//
// No game is touched: a counter call is the arguments the adapter would have
// read (TrackerMod::Num), a ground item is what the adapter would have read
// off it, and a room is a name.
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// PRODUCTION_TRACKER_MOD

using ForgePact::TrackerMod;
using Num = TrackerMod::Num;
using NumKind = TrackerMod::NumKind;
using Verdict = TrackerMod::Verdict;

#ifdef _WIN32
namespace {

std::vector<std::string> Words(const std::string& line)
{
    std::istringstream in(line);
    std::vector<std::string> out;
    std::string w;
    while (in >> w) out.push_back(w);
    return out;
}

// What follows the first `skip` words of a line, as written (a name with spaces).
std::string Rest(const std::string& line, std::size_t skip)
{
    std::size_t at = 0;
    for (std::size_t i = 0; i < skip; ++i) {
        at = line.find_first_not_of(" \t", at);
        if (at == std::string::npos) return {};
        at = line.find_first_of(" \t", at);
        if (at == std::string::npos) return {};
    }
    at = line.find_first_not_of(" \t", at);
    if (at == std::string::npos) return {};
    std::string rest = line.substr(at);
    while (!rest.empty() && (rest.back() == ' ' || rest.back() == '\t' || rest.back() == '\r')) rest.pop_back();
    return rest;
}

Num Value(const std::string& w)
{
    if (w.empty() || w == "x") return { NumKind::Unsupported, 0.0, 0 };
    const std::string v = w.substr(1);
    switch (w[0]) {
    case 'r': return { NumKind::Real, std::stod(v), 0 };
    case 'i': return { NumKind::Int32, 0.0, std::stoll(v) };
    case 'l': return { NumKind::Int64, 0.0, std::stoll(v) };
    default: return { NumKind::Unsupported, 0.0, 0 };
    }
}

std::vector<int> Ids(const std::string& w)
{
    std::vector<int> ids;
    if (w == "-") return ids;
    std::istringstream in(w);
    std::string part;
    while (std::getline(in, part, ',')) ids.push_back(std::stoi(part));
    return ids;
}

// A reason token, as `zone-why=` prints it, back to the core's enum.
TrackerMod::ZoneWhy Why(const std::string& token)
{
    for (int i = 0; i <= (int)TrackerMod::ZoneWhy::NotActZone; ++i) {
        const auto why = (TrackerMod::ZoneWhy)i;
        if (token == TrackerMod::ZoneWhyName(why)) return why;
    }
    throw std::runtime_error("unknown zone reason: " + token);
}

struct Player {
    TrackerMod core;
    ForgePact::TrackerJournalFile file;
    std::string dir;
    std::uint32_t pid = 0;
    bool appendFailed = false;
    bool openFailed = false;

    void Append(const std::string& batch)
    {
        if (batch.empty()) return;
        const bool ok = file.Append(batch);
        if (!ok) appendFailed = true;
        core.NoteAppend(ok);
    }
    void Frame(std::uint64_t ms)
    {
        core.BeginFrame(ms);
        if (!file.IsOpen() && !file.Open(dir, pid, ms)) openFailed = true;
    }
    void End()
    {
        if (core.EndFrame()) Append(core.TakeBatch());
        core.AgeCreationWindow();
    }
};

std::string Play(Player& p, const std::string& line)
{
    const auto w = Words(line);
    if (w.empty() || w[0][0] == '#') return {};
    const std::string& op = w[0];
    std::vector<Num> args;
    const auto counterArgs = [&](std::size_t from) {
        const int argc = std::stoi(w.at(from));
        for (std::size_t i = from + 1; i < w.size(); ++i) args.push_back(Value(w[i]));
        if ((int)args.size() != argc) throw std::runtime_error("argc does not match the values: " + line);
        return argc;
    };
    if (op == "identity") {
        p.pid = (std::uint32_t)std::stoul(w.at(1));
        p.core.SetIdentity(p.pid, w.at(2));
    } else if (op == "on") {
        p.core.SetEnabled(true);
        TrackerMod::Routes routes;
        routes.gold = routes.xp = routes.kills = routes.drops = "both";
        p.core.SetRoutes(routes);
    } else if (op == "frame") {
        p.Frame(std::stoull(w.at(1)));
    } else if (op == "end") {
        p.End();
    } else if (op == "frames") {
        const int n = std::stoi(w.at(1));
        const std::uint64_t first = std::stoull(w.at(2)), step = std::stoull(w.at(3));
        for (int i = 0; i < n; ++i) {
            p.Frame(first + step * (std::uint64_t)i);
            p.End();
        }
    } else if (op == "status") {
        p.core.RecordBridgeStatus(true, "", p.core.RouteSummary());
    } else if (op == "room") {
        if (p.core.NoteRoom(w.at(1))) std::cout << "ROOM " << w.at(1) << std::endl;
    } else if (op == "progress") {
        p.core.RecordProgress(Rest(line, 1));
    } else if (op == "zone") {
        p.core.RecordSatanicZone(TrackerMod::SatanicZoneCode(w.at(1)), Ids(w.at(2)), Ids(w.at(3)), w.at(4) == "1");
    } else if (op == "zoneread") {
        p.core.ZoneAnswered(w.at(1), Ids(w.at(2)), Ids(w.at(3)));
    } else if (op == "zonemiss") {
        p.core.ZoneUnanswered(Why(w.at(1)));
    } else if (op == "gold") {
        const int argc = counterArgs(1);
        p.core.CountGold(TrackerMod::CaptureGold(argc, args.data()));
    } else if (op == "xp" || op == "xpfalse") {
        const int argc = counterArgs(1);
        p.core.CountXp(TrackerMod::CaptureXp(argc, args.data()), op == "xpfalse");
    } else if (op == "kill") {
        const int argc = counterArgs(1);
        p.core.CountKill(TrackerMod::CaptureKill(argc, args.data()));
    } else if (op == "create") {
        p.core.NoteCreated({ TrackerMod::kKeyStruct, std::stoull(w.at(1)) }, true);   // the script plays an installed session
    } else if (op == "drop") {
        const TrackerMod::ItemKey key{ TrackerMod::kKeyStruct, std::stoull(w.at(1)) };
        TrackerMod::Drop d;
        d.rarity = std::stoi(w.at(2));
        d.itemType = std::stoll(w.at(3));
        d.itemId = std::stoll(w.at(4));
        d.weaponType = std::stoll(w.at(5));
        d.name = Rest(line, 7);
        const std::string identity = TrackerMod::Identity((std::int64_t)key.value, w.at(3), w.at(6));
        const Verdict v = p.core.DecideDrop(d, p.core.RecentlyCreated(key), identity);
        if (v == Verdict::Recorded)
            std::cout << "RECORDED " << TrackerMod::RarityName(d.rarity) << "\t" << d.name << std::endl;
    } else if (op == "drain") {
        p.Append(p.core.Drain());
    } else {
        return "unknown operation: " + line;
    }
    return {};
}

} // namespace
#endif

int main(int argc, char** argv)
{
#ifndef _WIN32
    (void)argc;
    (void)argv;
    std::cout << "RESULT FAIL the session writer is Win32 only" << std::endl;
    return 1;
#else
    if (argc < 3) {
        std::cout << "RESULT FAIL usage: <script> <directory>" << std::endl;
        return 2;
    }
    std::ifstream script(argv[1]);
    if (!script) {
        std::cout << "RESULT FAIL cannot read " << argv[1] << std::endl;
        return 2;
    }
    Player p;
    p.dir = argv[2];
    std::string line, error;
    try {
        while (error.empty() && std::getline(script, line)) error = Play(p, line);
    } catch (const std::exception& e) {
        error = std::string("bad line (") + e.what() + "): " + line;
    }
    if (error.empty() && p.openFailed) error = "the session file could not be opened";
    if (error.empty() && p.appendFailed) error = "an append failed";
    if (error.empty() && p.core.Buffered() != 0) error = "records left in the buffer after the drain";
    std::cout << "FILE " << p.dir << "\\tracker\\" << p.file.Name() << std::endl;
    std::cout << "STAT " << p.core.StatLine(true, p.file.Name()) << std::endl;
    std::cout << (error.empty() ? std::string("RESULT OK") : "RESULT FAIL " + error) << std::endl;
    return error.empty() ? 0 : 1;
#endif
}
