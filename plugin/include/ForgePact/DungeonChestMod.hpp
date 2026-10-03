#pragma once

#include "Common.hpp"

namespace ForgePact::DungeonChest {

// Dungeon chest opens early (`dungeonchest <pct>|off|status`, issue #31): in a
// key dungeon the end chest (Dungeon_Chest_obj) becomes openable once <pct> %
// of the dungeon's monsters are dead, instead of all of them.
//
// "The dungeon's monsters" is our own definition, not the game's: the kills
// counted since the chest was first seen in the room, plus the Enemy_Parent_obj
// instances alive now. The threshold is ceil(pct/100 x that sum), re-evaluated
// once a second; rounding up means the chest never opens later than the game
// would and can equal it (95 % of 7 monsters is all 7). Kills made before the
// chest was first seen are not in the sum.
//
// This header holds the decision and its counters only: no game call. The
// adapter in ModuleMain.cpp supplies the room, the chest count and the alive
// count (a once-a-second poll), each kill (the EnemyDestroyKillProc hook,
// enemy-`self` calls only, one count per instance id) and two actions:
//   - `unlock`: what to write, and where, so the game's own chest becomes
//     openable. The route is chosen by the research session (workorder token
//     `unlock-route:`); until then the adapter's action is a no-op and the
//     decision still latches and reports `unlocked=1`.
//   - `chat`: put one line in the game's chat. Unset until a call shape is
//     proven live (`chat-route:`); while unset, a `chat` or `both` countdown
//     form is refused with a reason and the form stays what it was.
// tests/dungeon_chest_harness.cpp runs all of it against a fake dungeon.

inline constexpr int kMinPct = 50;
inline constexpr int kMaxPct = 95;
// The countdown shows only while this many kills or fewer remain.
inline constexpr long kCountdownFrom = 50;
// A chat form announces each of these the first time `remaining` reaches or
// passes below it in this dungeon; a kill that passes several at once prints
// one line with the current number.
inline constexpr int kMilestones[] = { 50, 40, 30, 20, 10, 5, 4, 3, 2, 1 };
inline constexpr int kMilestoneCount = static_cast<int>(sizeof(kMilestones) / sizeof(kMilestones[0]));
// The recent-kill set: an instance id counted once among the last this many.
inline constexpr int kRecentIds = 256;

inline constexpr char kReadyText[] = "Chest: ready to open";

// ---- the countdown's form ---------------------------------------------------

enum class Form : int { Head = 0, Chat = 1, Both = 2, None = 3 };

inline const char* FormName(Form f)
{
    switch (f) {
    case Form::Chat: return "chat";
    case Form::Both: return "both";
    case Form::None: return "none";
    default: return "head";
    }
}

inline bool ParseForm(std::string_view word, Form& out)
{
    if (word == "head") { out = Form::Head; return true; }
    if (word == "chat") { out = Form::Chat; return true; }
    if (word == "both") { out = Form::Both; return true; }
    if (word == "none") { out = Form::None; return true; }
    return false;
}

inline bool FormShowsHead(Form f) { return f == Form::Head || f == Form::Both; }
inline bool FormSendsChat(Form f) { return f == Form::Chat || f == Form::Both; }

// ---- the mode -----------------------------------------------------------------

// The command's words: `off` or `0` is off (0), like every other toggle in the
// plugin; a whole number of up to three digits is that number, which
// StoresMode then accepts or refuses. Anything else leaves `out` as it was
// and answers false.
inline bool ParsePct(std::string_view word, int& out)
{
    if (word == "off" || word == "0") { out = 0; return true; }
    if (word.empty() || word.size() > 3) return false;
    int value = 0;
    for (char c : word) {
        if (c < '0' || c > '9') return false;
        value = value * 10 + (c - '0');
    }
    out = value;
    return true;
}

// Whether `dungeonchest <pct>` stores the share it was asked for, given the
// kill hook's state as read after the command asked for it ("ok",
// "table-only", "failed" or "none"). Off is always stored. A share outside
// 50..95 is refused, never clamped. A failed kill hook refuses every share:
// a mode stored then would report itself armed while counting no kill.
inline bool StoresMode(int pct, std::string_view hook)
{
    return pct == 0 || (pct >= kMinPct && pct <= kMaxPct && hook != "failed");
}

inline std::string ModeText(int pct)
{
    return pct == 0 ? std::string("off") : std::to_string(pct) + "%";
}

// The one line a refused `dungeonchest <pct>` answers: what was asked, why it
// was refused, and the mode left in place.
inline std::string RefusedLine(int asked, int kept, std::string_view hook)
{
    const bool inRange = asked >= kMinPct && asked <= kMaxPct;
    return std::string("dungeonchest: refused ") + std::to_string(asked)
        + (inRange ? " hook=" + std::string(hook) + " (the kill hook did not install"
                   : std::string(" (the share is 50..95, or off"))
        + "; unchanged: " + ModeText(kept) + ")";
}

// ---- the decision ---------------------------------------------------------------

// ceil(pct % of every monster the tally knows: the kills since the chest was
// first seen, plus those alive now).
inline long ThresholdFor(int pct, long kills, long alive)
{
    const long long total = static_cast<long long>(kills) + static_cast<long long>(alive);
    if (pct <= 0 || total <= 0) return 0;
    return static_cast<long>((static_cast<long long>(pct) * total + 99) / 100);
}

// Whether the chest becomes openable now. A room the tally holds no monster
// for (threshold 0) never unlocks: the game's own rule stands there.
inline bool DecideUnlock(int pct, long kills, long alive)
{
    if (pct <= 0) return false;
    const long threshold = ThresholdFor(pct, kills, alive);
    if (threshold <= 0) return false;
    return kills >= threshold;
}

inline std::string CountdownLine(long remaining)
{
    return "Chest: " + std::to_string(remaining) + " kills to go";
}

// The countdown's text, or "" when nothing is shown: only while a chest is
// tracked, the threshold is not reached, and 0 < remaining <= 50.
inline std::string CountdownText(bool active, bool latched, long remaining)
{
    if (!active || latched || remaining <= 0 || remaining > kCountdownFrom) return std::string();
    return CountdownLine(remaining);
}

// ---- the per-room tally ---------------------------------------------------------

struct Tally {
    bool active = false;          // a Dungeon_Chest_obj is in the room: the tally runs
    int64_t room = 0;             // the room it belongs to (meaningful once roomSet)
    bool roomSet = false;
    long kills = 0;               // kills counted since the chest was first seen
    long alive = 0;               // Enemy_Parent_obj instances at the last poll
    long threshold = 0;           // the threshold at the last evaluation
    bool latched = false;         // reached in this room: stays reached until the room changes
    int milestonesPassed = 0;     // kMilestones[0..n) announced in chat
    bool readySent = false;       // `Chest: ready to open` sent
    int recent[kRecentIds] = {};  // the last kRecentIds counted ids, a ring
    int recentNext = 0;
    int recentCount = 0;
    std::unordered_set<int> recentIds;
};

inline long Remaining(const Tally& t)
{
    return t.threshold > t.kills ? t.threshold - t.kills : 0;
}

// Kept in both builds: `dungeonchest status` reports what was done.
struct Counters {
    std::atomic<long> kills{ 0 };       // kills counted, every room
    std::atomic<long> duplicates{ 0 };  // a second call for an id already counted
    std::atomic<long> rooms{ 0 };       // chests first seen
    std::atomic<long> unlocks{ 0 };     // rooms whose threshold latched
    std::atomic<long> chatLines{ 0 };   // chat lines the chat action accepted
    std::atomic<long> chatFailed{ 0 };  // chat lines it refused
};

using ChatAction = bool (*)(const std::string& line);
using UnlockAction = void (*)();

struct State {
    std::atomic<int> pct{ 0 };                               // 0 = off, else 50..95
    std::atomic<int> form{ static_cast<int>(Form::Head) };   // D2: head by default
    // The research build's probe asks for the tally with the mode off: it
    // counts kills and reads the room, and never decides, unlocks or chats.
    std::atomic<bool> observe{ false };
    Tally tally;
    Counters counters;
    ChatAction chat = nullptr;      // unset: the chat forms are refused
    UnlockAction unlock = nullptr;  // called once, when the threshold latches
};

// Off at load: no mod is on by default. Set only by the `dungeonchest` command.
inline State state;

inline int Pct(const State& s) { return s.pct.load(std::memory_order_relaxed); }
inline bool Active(const State& s) { return Pct(s) != 0; }
inline bool Tracking(const State& s) { return Active(s) || s.observe.load(std::memory_order_relaxed); }
inline Form CurrentForm(const State& s) { return static_cast<Form>(s.form.load(std::memory_order_relaxed)); }
inline bool ChatAvailable(const State& s) { return s.chat != nullptr; }

inline void ResetTally(Tally& t)
{
    t = Tally{};
}

// Off clears the tally, so a share switched on again starts from the next
// poll. Changing one share for another keeps it: only the threshold moves.
inline void SetMode(State& s, int pct)
{
    s.pct.store(pct, std::memory_order_relaxed);
    if (pct == 0 && !s.observe.load(std::memory_order_relaxed)) ResetTally(s.tally);
}

// `dungeonchest countdown <form>`: a chat form needs the chat action; without
// it the form is refused and stays what it was.
inline bool SetForm(State& s, Form f)
{
    if (FormSendsChat(f) && !ChatAvailable(s)) return false;
    s.form.store(static_cast<int>(f), std::memory_order_relaxed);
    return true;
}

inline std::string FormRefusedLine(Form asked, Form kept)
{
    return std::string("dungeonchest: countdown ") + FormName(asked)
        + " refused: chat route not available (unchanged: " + FormName(kept) + ")";
}

// One kill, from the kill hook, keyed by the dying enemy's instance id. Counted
// only while a chest is tracked, and once per id among the last kRecentIds; an
// id the adapter could not read (< 0) is counted, since there is nothing to
// key on. Answers whether it was counted.
inline bool CountKill(State& s, int id)
{
    Tally& t = s.tally;
    if (!Tracking(s) || !t.active) return false;
    if (id >= 0) {
        if (!t.recentIds.insert(id).second) { ++s.counters.duplicates; return false; }
        if (t.recentCount == kRecentIds) t.recentIds.erase(t.recent[t.recentNext]);
        else ++t.recentCount;
        t.recent[t.recentNext] = id;
        t.recentNext = (t.recentNext + 1) % kRecentIds;
    }
    ++t.kills;
    ++s.counters.kills;
    return true;
}

inline void SendChat(State& s, const std::string& line)
{
    if (!s.chat) return;
    if (s.chat(line)) ++s.counters.chatLines;
    else ++s.counters.chatFailed;
}

// Recomputes the threshold and decides. Answers true once, on the evaluation
// that latched: the unlock action has then run, and in a chat form the ready
// line has been sent.
inline bool Evaluate(State& s)
{
    Tally& t = s.tally;
    const int pct = Pct(s);
    if (pct == 0 || !t.active || t.latched) return false;
    t.threshold = ThresholdFor(pct, t.kills, t.alive);
    const Form form = CurrentForm(s);
    if (DecideUnlock(pct, t.kills, t.alive)) {
        t.latched = true;
        ++s.counters.unlocks;
        if (s.unlock) s.unlock();
        if (FormSendsChat(form) && !t.readySent) { t.readySent = true; SendChat(s, kReadyText); }
        return true;
    }
    const long remaining = Remaining(t);
    if (remaining > 0 && FormSendsChat(form)) {
        int passed = t.milestonesPassed;
        while (passed < kMilestoneCount && remaining <= kMilestones[passed]) ++passed;
        if (passed != t.milestonesPassed) {
            t.milestonesPassed = passed;
            SendChat(s, CountdownLine(remaining));
        }
    }
    return false;
}

// The once-a-second poll: the room, how many Dungeon_Chest_obj and how many
// Enemy_Parent_obj instances it holds. A room change, or the chest count
// dropping to 0, resets the tally; an unreadable room changes nothing (an
// unknown room never compares equal to a known one). Answers Evaluate's.
inline bool Poll(State& s, bool roomReadable, int64_t room, long chests, long alive)
{
    if (!Tracking(s) || !roomReadable) return false;
    Tally& t = s.tally;
    if (!t.roomSet || t.room != room) { ResetTally(t); t.room = room; t.roomSet = true; }
    if (chests <= 0) {
        if (t.active) { ResetTally(t); t.room = room; t.roomSet = true; }
        return false;
    }
    if (!t.active) { t.active = true; ++s.counters.rooms; }
    t.alive = alive > 0 ? alive : 0;
    return Evaluate(s);
}

// The head label's text this frame, or "" when the form or the tally shows
// nothing. Between polls `remaining` follows each kill against the last
// threshold.
inline std::string HeadText(const State& s)
{
    if (!Active(s) || !FormShowsHead(CurrentForm(s))) return std::string();
    const Tally& t = s.tally;
    return CountdownText(t.active, t.latched, Remaining(t));
}

// `hook` is the kill hook's state as ModuleMain reads it: "ok" (both routes),
// "table-only" (compiled GML's direct calls bypass it), "failed" (not
// installed) or "none" (never asked for).
inline std::string StatusLine(const State& s, const char* hook)
{
    const Tally& t = s.tally;
    return std::string("dungeonchest: ") + ModeText(Pct(s))
        + " | kills=" + std::to_string(t.kills)
        + " alive=" + std::to_string(t.alive)
        + " threshold=" + std::to_string(t.threshold)
        + " remaining=" + std::to_string(Remaining(t))
        + " unlocked=" + (t.latched ? "1" : "0")
        + " countdown=" + FormName(CurrentForm(s))
        + " chat=" + (ChatAvailable(s) ? "ok" : "unavailable")
        + " chatLines=" + std::to_string(s.counters.chatLines.load())
        + " hook=" + (hook ? hook : "?");
}

// The line the adapter prints on the evaluation that latched.
inline std::string UnlockedLine(const State& s)
{
    const Tally& t = s.tally;
    return "dungeonchest: unlocked early at " + std::to_string(t.kills) + "/" + std::to_string(t.threshold)
        + " alive=" + std::to_string(t.alive);
}

} // namespace ForgePact::DungeonChest
