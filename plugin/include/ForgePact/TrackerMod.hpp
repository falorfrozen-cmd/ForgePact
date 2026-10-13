#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

// The negative control's switch: tests/test_tracker_mod_behavior.py builds the
// harness a second time with this at 0, and that build must record the bag
// drop the shipped one holds. Nothing else defines it.
#ifndef FORGEPACT_TRACKER_CREATION_GUARD
#define FORGEPACT_TRACKER_CREATION_GUARD 1
#endif

namespace ForgePact {

// ---- HS Offline Tracker's sensor: the decision core (ForgePact #185) -------
//
// `tracker 1` turns ForgePact into the sensor HS Offline Tracker's producer
// DLL was: it counts the gold, experience and kills the game credits, records
// the Set, Satanic, Angelic, Heroic and Unholy items it drops on the ground,
// and notes room and Satanic zone changes and the character's name. All of it
// becomes protocol-v1 lines (HS-Offline-Tracker/PROTOCOL.md) in one journal
// file per game process, `bp_ipc\tracker\session-<pid>-<unix ms>.ndjson`, which
// the panel reads.
//
// The rules this header decides:
//   - the counters follow the producer's adaptive profile
//     (aurie-producer/include/hsot_aurie/producer_profile.h,
//     kCounterRoutesAdaptive; src/counter_validation.cpp): GoldLogAdd with 1
//     to 4 arguments counts the last one that decodes as a positive whole
//     number; ExperienceUpdate with exactly 2 counts argument 0, unless the
//     call answered an explicit boolean false; EnemyAddStatistics with exactly
//     1 counts one kill when argument 0 is a non-negative whole id. A real
//     must be finite, whole and at most 2^53-1; int32 and int64 are taken as
//     they are; every other kind is refused. Sums saturate rather than wrap.
//     The adapter reads the arguments before the game's own call and counts
//     after it returns, as the producer did.
//   - a ground item is decided once, at the end of its frame: off (nothing);
//     not built through CreateItemNew in this frame or the one before (held
//     as a bag drop: the creation guard Loot announcements measured on this
//     build, kept here as its own copy so it works with `lootann` off); a
//     rarity (itemInfoStruct["27"]) outside {4 Set, 6 Satanic, 7 Angelic,
//     9 Heroic, 10 Unholy}, or unread (untracked: the producer's
//     IsTrackedRareRarity set); an identity already recorded (a duplicate);
//     an item type or id that could not be read (unread). Anything else is a
//     ground_drop record.
//   - a room is recorded when its name differs from the last one; the
//     Satanic zone's code is `Satanic_<act>_<zone>` of the act room the game
//     rolled (the producer's SatanicZoneCode).
//   - the arming rule is Loot announcements': armed at launch, hooked on the
//     first frame setup is done and the local player resolves, once per
//     session (the guide's Known Limitations item 8).
//   - records are buffered and appended every kFlushFrames frames, with a
//     heartbeat every kHeartbeatMs of the frame clock. The buffer and the
//     file are capped: past either cap a record is counted and dropped, never
//     waited for. Nothing is written at process exit; what is still buffered
//     then is lost, at most kFlushFrames frames of it.
//
// It is game-independent by contract - numbers as TrackerMod::Num, item keys
// as integers, names as text, never an RValue or an instance - so
// tests/tracker_mod_harness.cpp compiles it whole. The Win32 writer below,
// TrackerJournalFile, is the one the adapter and tests/tracker_mod_exit_probe.cpp
// both use.
class TrackerMod {
public:
    // ---- numbers, the way the adapter hands them over ----------------------
    enum class NumKind : int { Real, Int32, Int64, Unsupported };
    struct Num {
        NumKind kind = NumKind::Unsupported;
        double real = 0.0;
        std::int64_t integer = 0;
    };
    static constexpr std::int64_t kMaxExactInteger = 9007199254740991LL;   // 2^53 - 1
    // GoldLogAdd's widest accepted call; the adapter decodes at most this many.
    static constexpr int kMaxCounterArguments = 4;

    static std::optional<std::int64_t> DecodeIntegral(const Num& v, bool allowZero)
    {
        switch (v.kind) {
        case NumKind::Int32:
        case NumKind::Int64:
            if (v.integer > 0 || (allowZero && v.integer == 0)) return v.integer;
            return std::nullopt;
        case NumKind::Real:
            if (!std::isfinite(v.real) || v.real < (allowZero ? 0.0 : 1.0) || v.real > (double)kMaxExactInteger
                || std::trunc(v.real) != v.real)
                return std::nullopt;
            return (std::int64_t)v.real;
        case NumKind::Unsupported:
            return std::nullopt;
        }
        return std::nullopt;
    }
    static std::optional<std::int64_t> DecodePositive(const Num& v) { return DecodeIntegral(v, false); }
    static std::optional<std::int64_t> DecodeNonNegative(const Num& v) { return DecodeIntegral(v, true); }

    // GoldLogAdd: 1 to 4 arguments; the amount is the last one that decodes
    // as a positive whole number (when two qualify the second wins, the layout
    // every 2026 build has used, per the producer).
    static std::optional<std::int64_t> CaptureGold(int argc, const Num* args)
    {
        if (!args || argc < 1 || argc > kMaxCounterArguments) return std::nullopt;
        std::optional<std::int64_t> chosen;
        for (int i = 0; i < argc; ++i)
            if (const auto candidate = DecodePositive(args[i])) chosen = candidate;
        return chosen;
    }
    // ExperienceUpdate: exactly 2 arguments, the amount is argument 0.
    static std::optional<std::int64_t> CaptureXp(int argc, const Num* args)
    {
        if (!args || argc != 2) return std::nullopt;
        return DecodePositive(args[0]);
    }
    // EnemyAddStatistics: exactly 1 argument, a non-negative whole statistic
    // id. The answer is the id; each accepted call is one kill.
    static std::optional<std::int64_t> CaptureKill(int argc, const Num* args)
    {
        if (!args || argc != 1) return std::nullopt;
        return DecodeNonNegative(args[0]);
    }

    static std::int64_t SaturatingAdd(std::int64_t total, std::int64_t delta)
    {
        constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
        if (delta <= 0) return total;
        if (total > kMax - delta) return kMax;
        return total + delta;
    }

    // After the game's own call returned. Every call while on is counted in
    // the *-calls fields, the positive control that the detour runs at all.
    void CountGold(const std::optional<std::int64_t>& amount)
    {
        if (!m_Enabled) return;
        ++m_Stats.goldCalls;
        if (!amount) return;
        m_Stats.goldSum = SaturatingAdd(m_Stats.goldSum, *amount);
        m_PendingGold = SaturatingAdd(m_PendingGold, *amount);
    }
    // `explicitFalse`: the call's result was a boolean false, the game's
    // refusal of the experience; that call is a rejection, not a gain.
    void CountXp(const std::optional<std::int64_t>& amount, bool explicitFalse)
    {
        if (!m_Enabled) return;
        ++m_Stats.xpCalls;
        if (!amount) return;
        if (explicitFalse) { ++m_Stats.xpRejected; return; }
        m_Stats.xpSum = SaturatingAdd(m_Stats.xpSum, *amount);
        m_PendingXp = SaturatingAdd(m_PendingXp, *amount);
    }
    void CountKill(const std::optional<std::int64_t>& statisticId)
    {
        if (!m_Enabled) return;
        ++m_Stats.killCalls;
        if (!statisticId) return;
        m_Stats.killsSum = SaturatingAdd(m_Stats.killsSum, 1);
        m_PendingKills = SaturatingAdd(m_PendingKills, 1);
    }

    // ---- ground drops ---------------------------------------------------------
    static constexpr int kRarityUnread = -1;
    static constexpr std::array<int, 5> kTrackedRarities = { 4, 6, 7, 9, 10 };
    static constexpr bool kCreationGuard = FORGEPACT_TRACKER_CREATION_GUARD != 0;
    static constexpr std::size_t kCreationCap = 4096;   // item keys one frame's window holds
    static constexpr std::size_t kMemoryCap = 4096;     // identities remembered, oldest forgotten first

    static constexpr bool IsTrackedRarity(int code)
    {
        for (int r : kTrackedRarities)
            if (r == code) return true;
        return false;
    }
    // The producer's RarityName: the names the Tracker's journal shows.
    static constexpr const char* RarityName(int code)
    {
        switch (code) {
        case 4: return "Set";
        case 6: return "Satanic";
        case 7: return "Angelic";
        case 9: return "Heroic";
        case 10: return "Unholy";
        default: return "";
        }
    }

    // An item struct's key, derived by the adapter the way Loot announcements
    // derives it: a struct by its object pointer, a reference by the value it
    // holds; compared, never followed.
    static constexpr int kKeyStruct = 1;
    static constexpr int kKeyReference = 2;
    struct ItemKey {
        int kind;
        std::uint64_t value;
        bool operator<(const ItemKey& o) const { return kind != o.kind ? kind < o.kind : value < o.value; }
    };

    // A CreateItemNew return (or its argument 0), noted while on.
    void NoteCreated(const ItemKey& key)
    {
        if (!m_Enabled || m_CreatedNow.count(key)) return;
        if (m_CreatedNow.size() >= kCreationCap) {
            ++m_Stats.createOverflow;
            return;
        }
        m_CreatedNow.insert(key);
        ++m_Stats.created;
    }
    bool RecentlyCreated(const ItemKey& key) const
    {
        return m_CreatedNow.count(key) != 0 || m_CreatedBefore.count(key) != 0;
    }
    // Once per tick, after its decisions.
    void AgeCreationWindow()
    {
        m_CreatedBefore.swap(m_CreatedNow);
        m_CreatedNow.clear();
    }

    // Loot announcements' identity rules: the itemType and a real
    // itemTimeStamp, else the ground instance id with the stamp. An identity
    // that could not be read is not Identifiable, so two unread items never
    // stand for each other.
    static bool IsRealStamp(const std::string& timeStamp)
    {
        return !timeStamp.empty() && timeStamp != "0" && timeStamp != "undefined";
    }
    static std::string Identity(std::int64_t instanceId, const std::string& itemType, const std::string& timeStamp)
    {
        if (IsRealStamp(timeStamp)) return "item:" + itemType + ":" + timeStamp;
        return "ground:" + std::to_string(instanceId) + ":" + timeStamp;
    }
    static bool Identifiable(bool groundIdRead, const std::string& itemType, const std::string& timeStamp)
    {
        return IsRealStamp(timeStamp) ? !itemType.empty() : groundIdRead;
    }

    // What the adapter read off one ground item. -1 is "not read" for the
    // numbers; an empty name is left out of the record.
    struct Drop {
        int rarity = kRarityUnread;
        std::int64_t itemType = -1;
        std::int64_t itemId = -1;
        std::int64_t weaponType = -1;
        std::string name;
    };
    enum class Verdict : int { Off, Recorded, HeldBagDrop, Untracked, Duplicate, Unread };
    static constexpr const char* VerdictName(Verdict v)
    {
        switch (v) {
        case Verdict::Off: return "off";
        case Verdict::Recorded: return "recorded";
        case Verdict::HeldBagDrop: return "held-bag-drop";
        case Verdict::Untracked: return "untracked";
        case Verdict::Duplicate: return "duplicate";
        case Verdict::Unread: return "unread";
        }
        return "unknown";
    }

    Verdict DecideDrop(const Drop& d, bool recentlyCreated, const std::string& identity)
    {
        if (!m_Enabled) return Verdict::Off;
        ++m_Stats.seen;
        if (kCreationGuard && !recentlyCreated) {
            ++m_Stats.heldBagDrop;
            return Verdict::HeldBagDrop;
        }
        if (!IsTrackedRarity(d.rarity)) {
            ++m_Stats.dropsUntracked;
            return Verdict::Untracked;
        }
        if (m_Seen.count(identity)) {
            ++m_Stats.dropsDuplicate;
            return Verdict::Duplicate;
        }
        if (d.itemType < 0 || d.itemId < 0) {
            ++m_Stats.dropsUnread;
            return Verdict::Unread;
        }
        std::string line = Envelope("ground_drop");
        line += ",\"event_id\":";
        AppendEscaped(line, "fp-" + std::to_string(m_ProcessId) + "-" + std::to_string(m_Sequence + 1));
        line += ",\"source\":\"game_ground_item_create\"";
        if (!m_Room.empty()) {
            line += ",\"room\":";
            AppendEscaped(line, m_Room);
        }
        line += ",\"item\":{";
        if (!d.name.empty()) {
            line += "\"name\":";
            AppendEscaped(line, d.name);
            line += ",";
        }
        line += "\"rarity\":";
        AppendEscaped(line, RarityName(d.rarity));
        line += ",\"item_type\":" + std::to_string(d.itemType);
        line += ",\"item_id\":" + std::to_string(d.itemId);
        if (d.weaponType > 0) line += ",\"weapon_type\":" + std::to_string(d.weaponType);
        line += ",\"amount\":1}";
        if (!Commit(line)) return Verdict::Recorded;   // counted in buffer-full or file-full
        ++m_Stats.dropsWritten;
        m_Seen.insert(identity);
        m_Order.push_back(identity);
        while (m_Order.size() > kMemoryCap) {
            m_Seen.erase(m_Order.front());
            m_Order.pop_front();
        }
        return Verdict::Recorded;
    }

    // ---- the room, the zone, the character -------------------------------------
    // A `room` record when the name differs from the last one noted.
    bool NoteRoom(const std::string& room)
    {
        if (!m_Enabled || room.empty() || room == m_Room) return false;
        m_Room = room;
        std::string line = Envelope("room");
        line += ",\"room\":";
        AppendEscaped(line, room);
        Commit(line);
        return true;
    }
    const std::string& Room() const { return m_Room; }

    // "Act_09_03": the act zones the game rolls; towns and sub-areas
    // ("Act_02_02_02") have other shapes.
    static bool IsActZoneRoomName(const std::string& name)
    {
        if (name.size() != 9U || name.rfind("Act_", 0U) != 0U || name[6] != '_') return false;
        for (const std::size_t i : { (std::size_t)4, (std::size_t)5, (std::size_t)7, (std::size_t)8 })
            if (name[i] < '0' || name[i] > '9') return false;
        return true;
    }
    // "Act_09_03" -> "Satanic_9_3", the spelling the Tracker's zone table uses.
    static std::string SatanicZoneCode(const std::string& roomName)
    {
        if (roomName.rfind("Act_", 0U) != 0U) return {};
        const std::size_t second = roomName.find('_', 4U);
        if (second == std::string::npos) return {};
        const std::string act = roomName.substr(4U, second - 4U);
        const std::string zone = roomName.substr(second + 1U);
        if (act.empty() || zone.empty() || act.size() > 4U || zone.size() > 4U) return {};
        for (const char c : act + zone)
            if (c < '0' || c > '9') return {};
        return "Satanic_" + std::to_string(std::stoi(act)) + "_" + std::to_string(std::stoi(zone));
    }
    // A `satanic_zone` and the `vitals` that says whether the character stands
    // in it.
    void RecordSatanicZone(const std::string& zone, const std::vector<int>& buffs, const std::vector<int>& debuffs,
                           bool here)
    {
        if (!m_Enabled || zone.empty()) return;
        std::string line = Envelope("satanic_zone");
        line += ",\"zone\":";
        AppendEscaped(line, zone);
        AppendIds(line, "buffs", buffs);
        AppendIds(line, "debuffs", debuffs);
        Commit(line);
        std::string vitals = Envelope("vitals");
        vitals += here ? ",\"satanic_here\":true" : ",\"satanic_here\":false";
        Commit(vitals);
    }
    // A `progress` record carrying the character's name only.
    void RecordProgress(const std::string& name)
    {
        if (!m_Enabled || name.empty()) return;
        std::string line = Envelope("progress");
        line += ",\"name\":";
        AppendEscaped(line, name);
        Commit(line);
    }
    // At install: ready, or blocked with a reason; `detail` is the route summary.
    void RecordBridgeStatus(bool ready, const std::string& reason, const std::string& detail)
    {
        if (!m_Enabled) return;
        std::string line = Envelope("bridge_status");
        line += ready ? ",\"state\":\"transport_ready\"" : ",\"state\":\"blocked\"";
        if (!ready) {
            line += ",\"reason\":";
            AppendEscaped(line, reason);
        }
        line += ",\"detail\":";
        AppendEscaped(line, detail);
        Commit(line);
    }

    // ---- the journal: frames, batches, caps -------------------------------------
    static constexpr std::uint64_t kFlushFrames = 30;
    static constexpr std::uint64_t kHeartbeatMs = 10000;
    static constexpr std::size_t kBufferCap = 256 * 1024;                 // bytes waiting for the next append
    static constexpr std::uint64_t kFileCap = 64ULL * 1024ULL * 1024ULL;   // bytes one session file takes

    void SetIdentity(std::uint32_t processId, const std::string& buildId)
    {
        m_ProcessId = processId;
        m_BuildId = buildId;
    }
    // The frame clock: one reading per frame, the time every record of that
    // frame carries.
    void BeginFrame(std::uint64_t nowUnixMs)
    {
        m_Now = nowUnixMs;
        if (m_HeartbeatFrom == 0) m_HeartbeatFrom = nowUnixMs;
        ++m_Frame;
    }
    // At the end of the frame. On every kFlushFrames-th: the counts since the
    // last session_delta, when any is above 0, and a heartbeat when one is
    // due. True when that frame has lines to append (TakeBatch, then the
    // writer, then NoteAppend).
    bool EndFrame()
    {
        if (!m_Enabled || m_Frame % kFlushFrames != 0) return false;
        WritePendingDelta();
        if (m_Now >= m_HeartbeatFrom && m_Now - m_HeartbeatFrom >= kHeartbeatMs) {
            m_HeartbeatFrom = m_Now;
            if (Commit(Envelope("heartbeat"))) ++m_Stats.heartbeats;
        }
        return m_BufferedLines != 0;
    }
    // The buffered lines, whole, for one append.
    std::string TakeBatch()
    {
        std::string batch;
        batch.swap(m_Buffer);
        m_BatchLines = m_BufferedLines;
        m_BatchBytes = batch.size();
        m_BufferedLines = 0;
        return batch;
    }
    // What the writer made of the last batch. A failed append is counted and
    // its lines are gone: nothing waits and nothing is retried.
    void NoteAppend(bool ok)
    {
        if (ok) {
            m_Stats.lines += (long long)m_BatchLines;
            m_FileBytes += m_BatchBytes;
        } else {
            ++m_Stats.writeErrors;
        }
        m_BatchLines = 0;
        m_BatchBytes = 0;
    }
    // Everything at once, for `tracker 0`: the counts not yet in a
    // session_delta, then the whole buffer.
    std::string Drain()
    {
        WritePendingDelta();
        return TakeBatch();
    }
    std::uint64_t Sequence() const { return m_Sequence; }
    std::size_t Buffered() const { return m_Buffer.size(); }
    std::uint64_t FileBytes() const { return m_FileBytes; }

    // ---- arming -----------------------------------------------------------------
    static constexpr unsigned long long kInstallPollFrames = 60;
    static constexpr bool LooksForPlayer(unsigned long long armedFrames)
    {
        return armedFrames % kInstallPollFrames == 0;
    }
    bool ShouldInstall(bool setupDone, bool playerResolved, bool installTried) const
    {
        return m_Enabled && setupDone && playerResolved && !installTried;
    }
    const char* InstallState(bool installTried) const
    {
        if (installTried) return "installed";
        return m_Enabled ? "waiting-for-character" : "not-armed";
    }

    bool Enabled() const { return m_Enabled; }
    // Switching off keeps the counters and the memory, and clears the
    // creation window, so nothing built while off counts as new.
    void SetEnabled(bool on)
    {
        m_Enabled = on;
        if (!on) {
            m_CreatedNow.clear();
            m_CreatedBefore.clear();
        }
    }

    // ---- routes and the stat line -----------------------------------------------
    // Each `both`, `table-only`, `none` or `not-installed`, set by the adapter.
    struct Routes {
        const char* gold = "not-installed";
        const char* xp = "not-installed";
        const char* kills = "not-installed";
        const char* drops = "not-installed";
    };
    void SetRoutes(const Routes& routes) { m_Routes = routes; }
    const Routes& GetRoutes() const { return m_Routes; }
    // Drops need two hooks; the weaker route of the two is the drops' route.
    static const char* CombineRoutes(const char* a, const char* b)
    {
        const std::string x(a ? a : "none"), y(b ? b : "none");
        if (x == "not-installed" || y == "not-installed") return "not-installed";
        if (x == "none" || y == "none") return "none";
        if (x == "table-only" || y == "table-only") return "table-only";
        return "both";
    }
    std::string RouteSummary() const
    {
        return std::string("gold=") + m_Routes.gold + " xp=" + m_Routes.xp + " kills=" + m_Routes.kills
            + " drops=" + m_Routes.drops;
    }

    struct Counters {
        long long goldCalls = 0, xpCalls = 0, killCalls = 0;   // every detour call while on
        long long goldSum = 0, xpSum = 0, killsSum = 0;        // the accepted totals
        long long xpRejected = 0;      // an explicit false answered
        long long seen = 0;            // ground items decided while on
        long long dropsWritten = 0;    // ground_drop records buffered
        long long heldBagDrop = 0;     // not built this frame or the last
        long long dropsUntracked = 0;  // a rarity outside the five, or unread
        long long dropsDuplicate = 0;  // an identity already recorded
        long long dropsUnread = 0;     // a tracked item whose type or id could not be read
        long long created = 0, createOverflow = 0;
        long long heartbeats = 0;
        long long lines = 0;           // lines the writer appended
        long long writeErrors = 0;     // appends that failed
        long long bufferFull = 0;      // records dropped at the buffer's cap
        long long fileFull = 0;        // records dropped at the file's cap
    };
    const Counters& Stats() const { return m_Stats; }

    std::string StatusLine() const { return m_Enabled ? "tracker: on" : "tracker: off"; }
    // `tracker stat`: Live procedure 1 reads these keys.
    std::string StatLine(bool installTried, const std::string& fileName) const
    {
        const auto n = [](long long v) { return std::to_string(v); };
        std::string s = StatusLine();
        s += std::string(" install=") + InstallState(installTried);
        s += " " + RouteSummary();
        s += " gold-calls=" + n(m_Stats.goldCalls) + " xp-calls=" + n(m_Stats.xpCalls) + " kill-calls=" + n(m_Stats.killCalls);
        s += " gold-sum=" + n(m_Stats.goldSum) + " xp-sum=" + n(m_Stats.xpSum) + " kills-sum=" + n(m_Stats.killsSum);
        s += " drops-written=" + n(m_Stats.dropsWritten) + " held-bag-drop=" + n(m_Stats.heldBagDrop);
        s += " drops-untracked=" + n(m_Stats.dropsUntracked) + " drops-duplicate=" + n(m_Stats.dropsDuplicate);
        s += " drops-unread=" + n(m_Stats.dropsUnread) + " xp-rejected=" + n(m_Stats.xpRejected);
        s += " created=" + n(m_Stats.created) + " create-overflow=" + n(m_Stats.createOverflow);
        s += " heartbeats=" + n(m_Stats.heartbeats);
        s += " lines=" + n(m_Stats.lines) + " write-errors=" + n(m_Stats.writeErrors);
        s += " buffer-full=" + n(m_Stats.bufferFull) + " file-full=" + n(m_Stats.fileFull);
        s += " file=" + (fileName.empty() ? std::string("none") : fileName);
        return s;
    }

    // JSON string escaping: quotes, backslashes and control characters
    // escaped; valid UTF-8 passed through; a byte that is not part of a valid
    // UTF-8 sequence becomes U+FFFD, so one bad name never makes a line the
    // panel cannot parse.
    static void AppendEscaped(std::string& out, const std::string& value)
    {
        static constexpr char kHex[] = "0123456789abcdef";
        out.push_back('"');
        const std::size_t size = value.size();
        std::size_t i = 0;
        while (i < size) {
            const unsigned char c = (unsigned char)value[i];
            if (c < 0x80U) {
                switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\b': out += "\\b"; break;
                case '\f': out += "\\f"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (c < 0x20U) {
                        out += "\\u00";
                        out.push_back(kHex[(c >> 4U) & 0x0FU]);
                        out.push_back(kHex[c & 0x0FU]);
                    } else {
                        out.push_back((char)c);
                    }
                    break;
                }
                ++i;
                continue;
            }
            const std::size_t length = Utf8SequenceLength(value, i);
            if (length == 0) {
                out += "\\ufffd";
                ++i;
                continue;
            }
            out.append(value, i, length);
            i += length;
        }
        out.push_back('"');
    }

private:
    // The length of the valid UTF-8 sequence starting at `at`, 0 when there is none.
    static std::size_t Utf8SequenceLength(const std::string& s, std::size_t at)
    {
        const auto byte = [&s](std::size_t i) { return (unsigned char)s[i]; };
        const auto cont = [&](std::size_t i, unsigned lo, unsigned hi) {
            return i < s.size() && byte(i) >= lo && byte(i) <= hi;
        };
        const unsigned c = byte(at);
        if (c >= 0xC2U && c <= 0xDFU) return cont(at + 1, 0x80U, 0xBFU) ? 2 : 0;
        if (c >= 0xE0U && c <= 0xEFU) {
            const unsigned lo = c == 0xE0U ? 0xA0U : 0x80U;
            const unsigned hi = c == 0xEDU ? 0x9FU : 0xBFU;
            return cont(at + 1, lo, hi) && cont(at + 2, 0x80U, 0xBFU) ? 3 : 0;
        }
        if (c >= 0xF0U && c <= 0xF4U) {
            const unsigned lo = c == 0xF0U ? 0x90U : 0x80U;
            const unsigned hi = c == 0xF4U ? 0x8FU : 0xBFU;
            return cont(at + 1, lo, hi) && cont(at + 2, 0x80U, 0xBFU) && cont(at + 3, 0x80U, 0xBFU) ? 4 : 0;
        }
        return 0;
    }

    static void AppendIds(std::string& line, const char* key, const std::vector<int>& ids)
    {
        line += ",\"";
        line += key;
        line += "\":[";
        bool first = true;
        for (int id : ids) {
            if (id < 0) continue;
            if (!first) line.push_back(',');
            line += std::to_string(id);
            first = false;
        }
        line.push_back(']');
    }

    // HS-Offline-Tracker's envelope (bridge-native/src/event_protocol.cpp,
    // AppendEnvelope), with the sequence the record gets if it is kept.
    std::string Envelope(const char* kind) const
    {
        std::string line = "{\"protocol\":\"hs-offline-tracker/1\",\"v\":1,\"kind\":\"";
        line += kind;
        line += "\",\"sequence\":" + std::to_string(m_Sequence + 1);
        line += ",\"emitted_at_unix_ms\":" + std::to_string(m_Now);
        line += ",\"process_id\":" + std::to_string(m_ProcessId);
        line += ",\"build_id\":";
        AppendEscaped(line, m_BuildId);
        return line;
    }

    // Closes the record and buffers it, or counts it dropped at a cap. A
    // dropped record takes no sequence number.
    bool Commit(std::string line)
    {
        line += "}\n";
        if (m_Buffer.size() + line.size() > kBufferCap) {
            ++m_Stats.bufferFull;
            return false;
        }
        if (m_FileBytes + m_Buffer.size() + line.size() > kFileCap) {
            ++m_Stats.fileFull;
            return false;
        }
        m_Buffer += line;
        ++m_BufferedLines;
        ++m_Sequence;
        return true;
    }

    // The counts since the last session_delta, when one is above 0. A delta
    // a cap refused stays pending for the next flush frame.
    void WritePendingDelta()
    {
        if (m_PendingGold <= 0 && m_PendingXp <= 0 && m_PendingKills <= 0) return;
        std::string line = Envelope("session_delta");
        line += ",\"gold\":" + std::to_string(m_PendingGold);
        line += ",\"xp\":" + std::to_string(m_PendingXp);
        line += ",\"kills\":" + std::to_string(m_PendingKills);
        line += ",\"source\":\"forgepact_tracker\"";
        if (!Commit(line)) return;
        m_PendingGold = m_PendingXp = m_PendingKills = 0;
    }

    bool m_Enabled = false;
    Counters m_Stats;
    Routes m_Routes;
    std::uint32_t m_ProcessId = 0;
    std::string m_BuildId;
    std::uint64_t m_Now = 0;
    std::uint64_t m_Frame = 0;
    std::uint64_t m_HeartbeatFrom = 0;
    std::uint64_t m_Sequence = 0;
    std::string m_Buffer;
    std::size_t m_BufferedLines = 0;
    std::size_t m_BatchLines = 0;
    std::size_t m_BatchBytes = 0;
    std::uint64_t m_FileBytes = 0;
    std::int64_t m_PendingGold = 0, m_PendingXp = 0, m_PendingKills = 0;
    std::string m_Room;
    std::set<ItemKey> m_CreatedNow;
    std::set<ItemKey> m_CreatedBefore;
    std::set<std::string> m_Seen;
    std::deque<std::string> m_Order;
};

#ifdef _WIN32
// The session file's writer: append-only, whole lines, opened with shared
// read and delete so the panel can read it and remove a finished session
// while the game still runs. It has no destructor and nothing closes it at
// process exit (the producer's writer thread is what aborted every exit,
// HS-Offline-Tracker #201); Windows closes the handle with the process.
// tests/tracker_mod_exit_probe.cpp holds one open with lines pending across
// ExitProcess.
struct TrackerJournalFile {
    void* handle = nullptr;
    char name[64] = {};
    unsigned long lastError = 0;

    // Creates `<ipcDir>\tracker` and `session-<pid>-<unix ms>.ndjson` in it,
    // once. False (and lastError) when either cannot be made.
    bool Open(const std::string& ipcDir, unsigned long processId, unsigned long long unixMs) noexcept
    {
        if (handle) return true;
        char fileName[64];
        std::snprintf(fileName, sizeof fileName, "session-%lu-%llu.ndjson", processId, unixMs);
        try {
            const std::string folder = ipcDir + "\\tracker";
            CreateDirectoryA(ipcDir.c_str(), nullptr);
            CreateDirectoryA(folder.c_str(), nullptr);
            const std::string path = folder + "\\" + fileName;
            const HANDLE h = CreateFileA(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr,
                                         OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (h == INVALID_HANDLE_VALUE) {
                lastError = GetLastError();
                return false;
            }
            handle = h;
            std::memcpy(name, fileName, sizeof name);
            name[sizeof name - 1] = '\0';
            return true;
        } catch (...) {
            return false;
        }
    }
    // One append of whole lines. False when it did not all reach the file.
    bool Append(const std::string& batch) noexcept
    {
        if (!handle) return false;
        const char* data = batch.data();
        std::size_t left = batch.size();
        while (left != 0) {
            const DWORD chunk = left > 0x40000000U ? 0x40000000U : (DWORD)left;
            DWORD written = 0;
            if (!WriteFile((HANDLE)handle, data, chunk, &written, nullptr) || written == 0) {
                lastError = GetLastError();
                return false;
            }
            data += written;
            left -= written;
        }
        return true;
    }
    bool IsOpen() const noexcept { return handle != nullptr; }
    const char* Name() const noexcept { return name[0] ? name : "none"; }
};
static_assert(std::is_trivially_destructible_v<TrackerJournalFile>,
              "the journal writer must have no destructor: nothing of the tracker runs at process exit");
#endif

} // namespace ForgePact
