#pragma once

#include "Common.hpp"

namespace ForgePact::DungeonChest {

// Dungeon chest opens early (`dungeonchest <pct>|off|status`, issue #31): in a
// key dungeon the end chest (Dungeon_Chest_obj) becomes openable once <pct> %
// of the dungeon's monsters are dead, instead of all of them.
//
// "The dungeon's monsters" is every monster the dungeon plans, spawned yet or
// not (D3, rewritten after Live procedure 1 measured 600 kills to clear a
// Pumpkin Cellar that showed 44 alive at entry): T, the planned total, is
// asked of the adapter's total source once, when the chest is first seen in
// the room, and then stays fixed. Progress is the kills counted since then,
// and the threshold is ceil(pct/100 x T), so `Chest: 50 kills to go` means
// 50 real kills and the countdown never climbs back. Rounding up means the
// threshold can equal T (95 % of 7 monsters is all 7). T is clamped to at
// least the kills counted plus the monsters alive now, so a source that
// under-counts can never make the chest open sooner than that share of what
// the room has really shown. While T is unknown (no total source, or one that
// answers 0) nothing is decided or shown, and a share is refused while there
// is no source at all.
//
// This header holds the decisions and their counters only: no game call. The
// adapter in ModuleMain.cpp supplies the room, the chest count, the chest
// instance and the alive count (a once-a-second poll), each kill (the
// EnemyDestroyKillProc hook, enemy-`self` calls only, one count per instance
// id) and three callbacks:
//   - `totalSource`: how many of the room's monsters are still to die, alive
//     or still to be made by its spawners, 0 when it cannot tell. Asked at
//     the chest's first sight, where that is the planned total, and again
//     once a second only while it answers 0; an answer given later has the
//     kills already counted added to it. Unset: every share is refused. The
//     player build's source is the estimate below (EstimatedTotal, workorder
//     token `total-route: estimate`): the creators still to spawn, counted
//     at first sight, times a mean Live procedure 1b measured.
//   - `unlock`: called once, when the threshold latches; it answers whether
//     the game's own chest can now be let open, which for this route (the
//     `instance_exists` detour, workorder token `unlock-route: builtin`)
//     means the detour is installed on the builtin's own address and the
//     room's chest instance was captured. Its answer opens the detour's view
//     for this room (`pollLatched`): from then the chest's own
//     instance_exists(Enemy_Parent_obj) poll is answered `false`
//     (AnswerPoll), and the game's chest does the rest.
//   - `chat`: put one line in the game's chat. While unset, a `chat` or
//     `both` countdown form is refused with a reason and the form stays what
//     it was; a line the callback fails to send switches the chat forms off
//     for the session (never retried every kill).
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

// ---- the planned total's estimate (`total-route: estimate`) -------------------
// Live procedure 1b (2026-10-03) found no creator variable, read by name, whose
// sum came near the kills to clear (`creator-sum` not observed), but whether a
// creator has spawned is readable: its `enemyArray` is not an array until it
// has (`creator-state`). So the planned total is the monsters alive at the
// chest's first sight plus the creators still to spawn then, times a measured
// mean, rounded up to a whole monster. The mean is run A of that session
// (Pumpkin Cellar): 619 kills to clear, 5 alive at first sight, 122 creators
// of which 5 had spawned by then, so 117 pending and (619 - 5) / 117 = 614 /
// 117, about 5.25 per pending creator. It is taken over kills, never births:
// 25 of the run's 644 births were never killed. If those 5 first-sight
// monsters were idle ones rather than packs, all 122 creators were pending and
// this estimate gives 646 for that run rather than 619; an over-count only
// raises the threshold, and the game's own rule (every monster dead) still
// opens the chest, so the mod never opens it later than the game would. The
// inputs are curated in the hub's
// hs-game-sdk/curated/dungeon_chest_measurements.json (DC19).
inline constexpr long kEstimateKills = 614;            // kills to clear less those alive at first sight: 619 - 5
inline constexpr long kEstimatePendingCreators = 117;  // creators still to spawn at first sight: 122 - 5

// alive + ceil(pending x 614 / 117); negative inputs count as 0.
inline long EstimatedTotal(long alive, long pending)
{
    const long long a = alive > 0 ? alive : 0;
    const long long p = pending > 0 ? pending : 0;
    return static_cast<long>(a + (p * kEstimateKills + kEstimatePendingCreators - 1) / kEstimatePendingCreators);
}

// What the total source saw of the room's creators when it answered, kept for
// `status`: every creator-family instance, those still to spawn, and those
// whose state could not be read (counted as spawned).
struct Census {
    long creators = 0;
    long pending = 0;
    long unreadable = 0;
};

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

inline bool InRange(int pct) { return pct >= kMinPct && pct <= kMaxPct; }

// Whether `dungeonchest <pct>` stores the share it was asked for, given the
// kill hook's state and the `instance_exists` detour's state as read after the
// command asked for them ("ok", "table-only", "failed" or "none") and whether
// a total source is supplied. Off is always stored. A share outside 50..95 is
// refused, never clamped. A share needs the kill hook on both routes ("ok"),
// the detour installed ("ok") and a total source: a failed or table-only kill
// hook counts no kill of compiled GML's, without the detour the chest never
// opens early, and without a total the share has nothing to be a share of,
// so a mode stored in any of those cases would report itself armed while
// doing nothing.
inline bool StoresMode(int pct, std::string_view hook, std::string_view unlock, bool totalAvailable)
{
    return pct == 0 || (InRange(pct) && hook == "ok" && unlock == "ok" && totalAvailable);
}

inline std::string ModeText(int pct)
{
    return pct == 0 ? std::string("off") : std::to_string(pct) + "%";
}

// The one line a refused `dungeonchest <pct>` answers: what was asked, why it
// was refused, and the mode left in place.
inline std::string RefusedLine(int asked, int kept, std::string_view hook, std::string_view unlock, bool totalAvailable)
{
    std::string why;
    if (!InRange(asked)) why = " (the share is 50..95, or off";
    else if (!totalAvailable) why = " total=unavailable (no source for the dungeon's planned total is set, so the share has nothing to be a share of";
    else if (unlock != "ok") why = " unlock=" + std::string(unlock) + " (the instance_exists detour is not installed, so the chest would not open early";
    else if (hook != "ok") why = " hook=" + std::string(hook) + " (the kill hook is not on both routes, so kills would go uncounted";
    else why = " (refused";
    return std::string("dungeonchest: refused ") + std::to_string(asked) + why
        + "; unchanged: " + ModeText(kept) + ")";
}

// ---- the decision ---------------------------------------------------------------

// The total a decision uses: the planned total, never below the kills counted
// plus the monsters alive now (a source that under-counts cannot pull the
// threshold under a share of what the room has really shown). 0 while the
// planned total is unknown.
inline long EffectiveTotal(long planned, long kills, long alive)
{
    if (planned <= 0) return 0;
    const long long seen = static_cast<long long>(kills) + static_cast<long long>(alive > 0 ? alive : 0);
    return static_cast<long>(seen > planned ? seen : planned);
}

// ceil(pct % of the total). Never above the total for a share up to 100 %.
inline long ThresholdFor(int pct, long total)
{
    if (pct <= 0 || total <= 0) return 0;
    return static_cast<long>((static_cast<long long>(pct) * total + 99) / 100);
}

// Whether the chest becomes openable now. A room whose total is unknown
// (threshold 0) never unlocks: the game's own rule stands there.
inline bool DecideUnlock(int pct, long kills, long total)
{
    if (pct <= 0) return false;
    const long threshold = ThresholdFor(pct, total);
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
    long alive0 = 0;              // Enemy_Parent_obj instances when the chest was first seen
    long total = 0;               // the planned total, fixed once known; 0 = unknown
    Census census;                // what the total source last saw of the room's creators
    long threshold = 0;           // the threshold at the last evaluation
    bool latched = false;         // reached in this room: stays reached until the room changes
    bool unlocked = false;        // the unlock action answered at the latch that the chest can open
    // The instance_exists detour's view of this room: true from the latch whose
    // unlock action answered, until the room changes. The detour tests it first.
    bool pollLatched = false;
    const void* chest = nullptr;  // the room's Dungeon_Chest_obj instance, captured at first sight; compared, never read
    long answered = 0;            // the chest's polls the detour answered `false` in this room
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
    std::atomic<long> latches{ 0 };     // rooms whose threshold latched
    std::atomic<long> unlocks{ 0 };     // latches whose unlock action answered that the chest can open
    std::atomic<long> unlockFailed{ 0 };// latches whose unlock action failed, or had none
    std::atomic<long> answered{ 0 };    // chest polls the instance_exists detour answered `false`
    std::atomic<long> totalAsks{ 0 };   // times the total source was asked
    std::atomic<long> chatLines{ 0 };   // chat lines the chat action accepted
    std::atomic<long> chatFailed{ 0 };  // chat lines it refused
};

using ChatAction = bool (*)(const std::string& line);
// Answers whether the game's own chest can now be let open (for the builtin
// route: the detour is installed and the room's chest was captured).
using UnlockAction = bool (*)();
// Answers how many of the room's monsters are still to die - alive now plus
// those its spawners have yet to make - or 0 when it cannot tell. `alive` is
// the poll's Enemy_Parent_obj count; `census` is filled with what the source
// saw of the room's creators, for `status`.
using TotalSource = long (*)(long alive, Census& census);

// ---- the head label's state (D12: a stable label) ----------------------------
// Live procedure 1b's owner report: the label "felt jerky and was blinking very
// fast as it was updating every frame". The label is drawn every frame, so
// everything that can change it is held here rather than re-derived per frame
// from inputs that move on their own: its text is written only when the count
// it shows changes (UpdateHeadLabel), and its height above the player is taken
// once, when it appears, from the player's origin and bounding box, and then
// held (PlaceHeadLabel) - the box's top follows the sprite's animation frame,
// so a label hung from it bobs while the player stands still. Its position is
// whole GUI pixels, since a fractional one is rasterised differently from
// frame to frame, and the last one placed is kept for a frame whose reads fail,
// so a missed read never blanks the label for a frame.
struct LabelSpot {
    double x = 0.0;
    double y = 0.0;
};

struct HeadLabel {
    long count = 0;          // the remaining count shown; 0 = hidden
    std::string text;        // CountdownLine(count), written when count changes
    long rewrites = 0;       // times the text changed (appeared, counted down, went)
    bool anchored = false;   // the lift is taken: from the label's first frame until it hides
    double lift = 0.0;       // room units between the player's origin and its box top, at that frame
    bool placed = false;     // `spot` holds a placed position
    LabelSpot spot;          // the last position placed, in whole GUI pixels
};

inline bool LabelShown(const HeadLabel& l) { return l.count > 0; }

struct State {
    std::atomic<int> pct{ 0 };                               // 0 = off, else 50..95
    std::atomic<int> form{ static_cast<int>(Form::Head) };   // D2: head by default
    // The research build's probe asks for the tally with the mode off: it
    // counts kills and reads the room, and never decides, unlocks or chats.
    std::atomic<bool> observe{ false };
    Tally tally;
    Counters counters;
    HeadLabel label;                    // what the head draw shows, written by UpdateHeadLabel
    ChatAction chat = nullptr;          // unset: the chat forms are refused
    UnlockAction unlock = nullptr;      // called once, when the threshold latches; unset: no latch ever unlocks
    TotalSource totalSource = nullptr;  // unset: every share is refused (`total=unavailable`)
};

// Off at load: no mod is on by default. Set only by the `dungeonchest` command.
inline State state;

inline int Pct(const State& s) { return s.pct.load(std::memory_order_relaxed); }
inline bool Active(const State& s) { return Pct(s) != 0; }
inline bool Tracking(const State& s) { return Active(s) || s.observe.load(std::memory_order_relaxed); }
inline Form CurrentForm(const State& s) { return static_cast<Form>(s.form.load(std::memory_order_relaxed)); }
inline bool ChatAvailable(const State& s) { return s.chat != nullptr; }
inline bool UnlockAvailable(const State& s) { return s.unlock != nullptr; }
inline bool TotalAvailable(const State& s) { return s.totalSource != nullptr; }
// The total this room's decision uses now (EffectiveTotal), 0 while unknown.
inline long TotalNow(const Tally& t) { return EffectiveTotal(t.total, t.kills, t.alive); }

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

// A chat line the callback could not send switches the chat forms off for the
// session: the callback is dropped (`chat=unavailable`, so `countdown chat` and
// `both` are refused from now on) and a chat form falls back to `head`, so the
// countdown still shows somewhere. Never retried every kill.
inline void DisableChat(State& s)
{
    s.chat = nullptr;
    if (FormSendsChat(CurrentForm(s))) s.form.store(static_cast<int>(Form::Head), std::memory_order_relaxed);
}

inline void SendChat(State& s, const std::string& line)
{
    if (!s.chat) return;
    if (s.chat(line)) { ++s.counters.chatLines; return; }
    ++s.counters.chatFailed;
    DisableChat(s);
}

// Recomputes the threshold and decides. Answers true once, on the evaluation
// that latched: the unlock action has then run (`tally.unlocked` says whether
// it answered that the chest can open, and opens the detour's view), and in a
// chat form the ready line has been sent - only when it did, so the chat
// never announces a chest the game keeps shut. While the total is unknown
// nothing is decided or shown.
inline bool Evaluate(State& s)
{
    Tally& t = s.tally;
    const int pct = Pct(s);
    if (pct == 0 || !t.active || t.latched) return false;
    const long total = TotalNow(t);
    t.threshold = ThresholdFor(pct, total);
    if (t.threshold <= 0) return false;
    const Form form = CurrentForm(s);
    if (DecideUnlock(pct, t.kills, total)) {
        t.latched = true;
        ++s.counters.latches;
        t.unlocked = s.unlock != nullptr && s.unlock();
        t.pollLatched = t.unlocked;
        if (t.unlocked) ++s.counters.unlocks;
        else ++s.counters.unlockFailed;
        if (t.unlocked && FormSendsChat(form) && !t.readySent) { t.readySent = true; SendChat(s, kReadyText); }
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

// The planned total, asked of the total source while it is unknown: at the
// chest's first sight, then once a poll only while the source answers 0. An
// answer is what is still to die, so the kills already counted are added.
inline void AskTotal(State& s)
{
    Tally& t = s.tally;
    if (t.total > 0 || !s.totalSource) return;
    ++s.counters.totalAsks;
    const long answer = s.totalSource(t.alive, t.census);
    if (answer > 0) t.total = answer + t.kills;
}

// The once-a-second poll: the room, how many Dungeon_Chest_obj and how many
// Enemy_Parent_obj instances it holds, and the chest instance the adapter
// resolved (null when it did not; kept from the first poll that had one). A
// room change, or the chest count dropping to 0, resets the tally, and with it
// the detour's view; an unreadable room changes nothing (an unknown room never
// compares equal to a known one). `alive` feeds the status and the total's
// clamp, never the planned total itself. Answers Evaluate's.
inline bool Poll(State& s, bool roomReadable, int64_t room, long chests, long alive, const void* chest = nullptr)
{
    if (!Tracking(s) || !roomReadable) return false;
    Tally& t = s.tally;
    if (!t.roomSet || t.room != room) { ResetTally(t); t.room = room; t.roomSet = true; }
    if (chests <= 0) {
        if (t.active) { ResetTally(t); t.room = room; t.roomSet = true; }
        return false;
    }
    t.alive = alive > 0 ? alive : 0;
    if (!t.active) { t.active = true; t.alive0 = t.alive; ++s.counters.rooms; }
    if (!t.chest) t.chest = chest;
    AskTotal(s);
    return Evaluate(s);
}

// The instance_exists detour's decision for one call: answer `false` (true
// here) only while this room's view is latched, for the room's chest as
// `self`, and for an Enemy_Parent_obj (or descendant) first argument, asked
// last through `argIsEnemy`, so every other call costs one flag test and at
// most one pointer compare before the original runs. Counts what it answered.
template <class ArgIsEnemy>
inline bool AnswerPoll(State& s, const void* self, ArgIsEnemy&& argIsEnemy)
{
    Tally& t = s.tally;
    if (!t.pollLatched) return false;
    if (self == nullptr || self != t.chest) return false;
    if (!argIsEnemy()) return false;
    ++t.answered;
    ++s.counters.answered;
    return true;
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

// The head label for this frame (D12). The count it shows is HeadText's
// window - shown only while the mode is on, the form draws above the head, a
// chest is tracked, the threshold is not reached and 0 < remaining <= 50 -
// and its text is rewritten only when that count changes, so frames with no
// kill and no threshold move draw the very same string. Hiding drops the
// anchor, so the next showing takes its lift afresh.
inline HeadLabel& UpdateHeadLabel(State& s)
{
    HeadLabel& l = s.label;
    long count = 0;
    if (Active(s) && FormShowsHead(CurrentForm(s))) {
        const Tally& t = s.tally;
        const long remaining = Remaining(t);
        if (t.active && !t.latched && remaining > 0 && remaining <= kCountdownFrom) count = remaining;
    }
    if (count != l.count) {
        l.count = count;
        l.text = count > 0 ? CountdownLine(count) : std::string();
        ++l.rewrites;
        if (count == 0) { l.anchored = false; l.placed = false; }
    }
    return l;
}

// Where the label goes this frame, in whole GUI pixels: `x`, `y` are the
// player's origin and `top` its bounding box top, in room units; the camera's
// view (`vx`, `vy`, `vw`, `vh`) and the GUI size (`gw`, `gh`) convert them, and
// `offsetPx` lifts the label above the box. The lift (y - top) is taken on the
// label's first frame and held while it shows, so only the player's position
// and the camera move it. Kept as the label's last spot.
inline LabelSpot PlaceHeadLabel(HeadLabel& l, double x, double y, double top, double vx, double vy, double vw,
                                double vh, double gw, double gh, double offsetPx)
{
    if (!l.anchored) { l.lift = y - top; l.anchored = true; }
    LabelSpot p;
    p.x = std::floor((x - vx) * gw / vw + 0.5);
    p.y = std::floor((y - l.lift - vy) * gh / vh - offsetPx + 0.5);
    l.spot = p;
    l.placed = true;
    return p;
}

// `hook` is the kill hook's state as ModuleMain records it: "ok" (both routes),
// "table-only" (compiled GML's direct calls bypass it), "failed" (not
// installed) or "none" (never asked for); `unlock` is the instance_exists
// detour's, the same words. `total` is the total the decision uses, or
// `unavailable` while it is unknown; `creators`, `pending` and `unreadable`
// are what the total source last saw of the room's creators (all, still to
// spawn, state unreadable). `latched` is the decision; `unlocked` is the
// unlock action's answer, which opens the detour's view; `answered` is what
// the detour then did to the chest's polls in this room.
inline std::string StatusLine(const State& s, const char* hook, const char* unlock)
{
    const Tally& t = s.tally;
    const long total = TotalNow(t);
    return std::string("dungeonchest: ") + ModeText(Pct(s))
        + " | kills=" + std::to_string(t.kills)
        + " total=" + (total > 0 ? std::to_string(total) : std::string("unavailable"))
        + " creators=" + std::to_string(t.census.creators)
        + " pending=" + std::to_string(t.census.pending)
        + " unreadable=" + std::to_string(t.census.unreadable)
        + " alive=" + std::to_string(t.alive)
        + " threshold=" + std::to_string(t.threshold)
        + " remaining=" + std::to_string(Remaining(t))
        + " latched=" + (t.latched ? "1" : "0")
        + " unlocked=" + (t.unlocked ? "1" : "0")
        + " unlock=" + (unlock ? unlock : "?")
        + " answered=" + std::to_string(t.answered)
        + " countdown=" + FormName(CurrentForm(s))
        + " chat=" + (ChatAvailable(s) ? "ok" : "unavailable")
        + " chatLines=" + std::to_string(s.counters.chatLines.load())
        + " hook=" + (hook ? hook : "?");
}

// The line the adapter prints on the evaluation that latched: the chest let
// open, or the unlock action that failed and left it to the game's own rule.
inline std::string UnlockedLine(const State& s)
{
    const Tally& t = s.tally;
    const std::string at = std::to_string(t.kills) + "/" + std::to_string(TotalNow(t))
        + " alive=" + std::to_string(t.alive);
    return t.unlocked ? "dungeonchest: unlocked early at " + at
                      : "dungeonchest: threshold reached at " + at
                            + " but the unlock action failed: the chest is left to the game's own rule";
}

} // namespace ForgePact::DungeonChest
