// Behavioral regression harness for Dungeon chest opens early
// (DungeonChestMod.hpp, issue #31).
//
// The Python runner splices the REAL ForgePact::DungeonChest header in below.
// Only the game is replaced: a fake dungeon answers what ModuleMain's
// once-a-second poll reads (the room, how many Dungeon_Chest_obj and how many
// Enemy_Parent_obj instances it holds, and the chest instance, here the
// address of a field), each kill arrives as the kill hook hands it over (the
// dying enemy's instance id), and the three callbacks the adapter supplies
// are recorders: `unlock` counts its calls and answers that the chest can
// open (or, in `unlock-failed`, that it cannot), `chat` keeps the lines it
// was given, and the total source answers what the scenario planned (by
// default exactly what is alive at the chest's first sight, so a dungeon
// with no spawners left behaves as the parent's tally did). The
// instance_exists detour's decision (AnswerPoll) is asked the way the detour
// asks it. No game process is touched.
//
// Every target that says "not yet" sits beside the same tally going on to the
// latch, so a decision that never unlocks fails it instead of passing it.
#include <atomic>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

// PRODUCTION_DUNGEONCHEST

namespace DC = ForgePact::DungeonChest;

static int failures = 0;
static void check(const std::string& label, bool ok, const std::string& detail = "") {
    std::cout << (ok ? "PASS " : "FAIL ") << label << (detail.empty() ? "" : " " + detail) << std::endl;
    if (!ok) ++failures;
}

// The adapter's three callbacks, as recorders.
static int unlockCalls = 0;
static std::vector<std::string> chatLines;
static int chatFailures = 0;
// What the total source answers: -1 means "exactly what is alive now", the
// dungeon whose spawners have nothing left to make.
static long sourceAnswer = -1;
static int sourceCalls = 0;
static bool RecordUnlock() { ++unlockCalls; return true; }
// An unlock action that cannot let the chest open (the detour not installed,
// the chest instance not captured).
static bool FailUnlock() { ++unlockCalls; return false; }
static bool RecordChat(const std::string& line) { chatLines.push_back(line); return true; }
// A chat call that the game refuses (the script unresolved, the call failed).
static bool FailChat(const std::string&) { ++chatFailures; return false; }
static long Source(long alive, DC::Census& census) {
    ++sourceCalls;
    census = DC::Census{};
    return sourceAnswer < 0 ? alive : sourceAnswer;
}
// The build's source as ModuleMain answers it: the census it took, run through
// the header's EstimateFromCensus. `censusCreators` creators, two of them
// still to spawn when enough are readable, `censusUnreadable` of them whose
// state could not be read; `censusFamily` false is a creator family that
// resolved no object, so the census loop never ran.
static bool censusFamily = true;
static long censusCreators = 4;
static long censusUnreadable = 0;
static long CensusSource(long alive, DC::Census& census) {
    ++sourceCalls;
    census = DC::Census{};
    if (censusFamily) {
        census.creators = censusCreators;
        census.unreadable = censusUnreadable;
        census.pending = censusCreators - censusUnreadable >= 2 ? 2 : 0;
    }
    return DC::EstimateFromCensus(censusFamily, alive, census);
}
static void ResetRecorders() {
    unlockCalls = 0; chatLines.clear(); chatFailures = 0; sourceAnswer = -1; sourceCalls = 0;
    censusFamily = true; censusCreators = 4; censusUnreadable = 0;
}

// One dungeon as the poll reads it. Every kill the game makes moves one
// monster from alive to dead; `Kill` hands it to the kill hook's path.
// `pending` monsters are still to be made by the dungeon's spawners: each
// kill lets one of them appear, the way a spawner tops a room back up.
struct Dungeon {
    int64_t room = 216;
    long chests = 1;
    long alive = 0;
    long pending = 0;
    int nextId = 100000;
    int chestInstance = 0;   // its address is the chest's `self`
};
static bool Poll(DC::State& s, const Dungeon& d) { return DC::Poll(s, true, d.room, d.chests, d.alive, &d.chestInstance); }
static bool Kill(DC::State& s, Dungeon& d) {
    if (d.alive > 0) --d.alive;
    if (d.pending > 0) { --d.pending; ++d.alive; }
    return DC::CountKill(s, d.nextId++);
}
// `n` kills, each followed by the poll, the way one kill a second plays out.
static int KillAndPoll(DC::State& s, Dungeon& d, int n) {
    int latched = 0;
    for (int i = 0; i < n; ++i) { Kill(s, d); if (Poll(s, d)) ++latched; }
    return latched;
}
// The detour's question for one call: `self`, and whether the first argument
// is Enemy_Parent_obj (or a descendant). `argReads` counts the argument reads.
static int argReads = 0;
static bool Ask(DC::State& s, const void* self, bool argIsEnemy) {
    return DC::AnswerPoll(s, self, [&] { ++argReads; return argIsEnemy; });
}
static std::string Tally(const DC::State& s) {
    const DC::Tally& t = s.tally;
    return "kills=" + std::to_string(t.kills) + " total=" + std::to_string(t.total) + " alive=" + std::to_string(t.alive)
        + " threshold=" + std::to_string(t.threshold) + " remaining=" + std::to_string(DC::Remaining(t))
        + " latched=" + std::to_string(t.latched) + " unlocked=" + std::to_string(t.unlocked)
        + " pollLatched=" + std::to_string(t.pollLatched) + " answered=" + std::to_string(t.answered)
        + " unlockCalls=" + std::to_string(unlockCalls) + " sourceCalls=" + std::to_string(sourceCalls)
        + " chat=" + std::to_string(chatLines.size());
}
static std::string Lines() {
    std::string out;
    for (const std::string& l : chatLines) out += "[" + l + "]";
    return out;
}
static void Arm(DC::State& s, int pct, bool chat) {
    ResetRecorders();
    s.unlock = &RecordUnlock;
    s.chat = chat ? &RecordChat : nullptr;
    s.totalSource = &Source;
    DC::SetMode(s, pct);
}

int main() {
    // ---- baseline: the mode off is the vanilla game ------------------------
    {
        DC::State s;
        Arm(s, 0, true);
        Dungeon d; d.alive = 40;
        int latched = 0;
        Poll(s, d);
        for (int i = 0; i < 40; ++i) { Kill(s, d); if (Poll(s, d)) ++latched; if (!DC::HeadText(s).empty()) ++latched; }
        check("baseline/off_never_latches",
            latched == 0 && !s.tally.latched && unlockCalls == 0 && chatLines.empty() && s.tally.kills == 0
                && !Ask(s, &d.chestInstance, true) && s.counters.answered == 0
                && DC::StatusLine(s, "none", "none").rfind("dungeonchest: off | ", 0) == 0,
            Tally(s));
    }
    {
        // On, but the room holds no dungeon chest: nothing is counted or shown.
        DC::State s;
        Arm(s, 50, true);
        Dungeon d; d.alive = 40; d.chests = 0;
        int latched = 0;
        for (int i = 0; i < 40; ++i) { Kill(s, d); if (Poll(s, d)) ++latched; }
        check("baseline/no_chest_no_tally",
            latched == 0 && !s.tally.active && s.tally.kills == 0 && unlockCalls == 0 && DC::HeadText(s).empty()
                && sourceCalls == 0,
            Tally(s));
    }

    // ---- target: 50 % of a planned 40 latches at the 20th kill, not the 19th
    {
        DC::State s;
        Arm(s, 50, false);
        sourceAnswer = 40;
        Dungeon d; d.alive = 40;
        Poll(s, d);
        const int early = KillAndPoll(s, d, 19);
        const bool notYet = early == 0 && !s.tally.latched && DC::Remaining(s.tally) == 1 && unlockCalls == 0;
        const std::string at19 = Tally(s);
        const int latched = KillAndPoll(s, d, 1);
        const std::string unlockedLine = DC::UnlockedLine(s);
        const int after = KillAndPoll(s, d, 5);
        check("target/latch_at_threshold",
            notYet && latched == 1 && after == 0 && s.tally.latched && s.tally.unlocked && unlockCalls == 1
                && s.tally.threshold == 20 && s.counters.unlocks == 1 && s.counters.unlockFailed == 0
                && unlockedLine == "dungeonchest: unlocked early at 20/40 alive=20"
                && DC::StatusLine(s, "ok", "ok").find(" total=40 ") != std::string::npos
                && DC::StatusLine(s, "ok", "ok").find(" latched=1 unlocked=1 unlock=ok ") != std::string::npos,
            "at19: " + at19 + " | at20+: " + Tally(s) + " | " + unlockedLine);
    }
    {
        // unlock-failed: the threshold latches but the unlock action answers
        // that the chest cannot open. The status line and the latch line say
        // so, `unlocked` stays 0, the detour's view stays shut (the chest's
        // poll is not answered), and no `ready to open` line is sent for a
        // chest the game keeps shut.
        DC::State s;
        Arm(s, 50, true);
        s.unlock = &FailUnlock;
        DC::SetForm(s, DC::Form::Both);
        Dungeon d; d.alive = 40;
        Poll(s, d);
        const int latched = KillAndPoll(s, d, 20);
        const std::string line = DC::UnlockedLine(s);
        const bool shut = !Ask(s, &d.chestInstance, true) && s.tally.answered == 0;
        check("target/unlock-failed",
            latched == 1 && s.tally.latched && !s.tally.unlocked && !s.tally.pollLatched && shut && unlockCalls == 1
                && s.counters.unlocks == 0 && s.counters.unlockFailed == 1
                && line.rfind("dungeonchest: threshold reached at 20/40 ", 0) == 0
                && line.find("unlock action failed") != std::string::npos
                && DC::StatusLine(s, "ok", "ok").find(" latched=1 unlocked=0 ") != std::string::npos
                && !chatLines.empty() && chatLines.back() != "Chest: ready to open",
            line + " | " + Tally(s) + " " + Lines());
    }

    // ---- target: the planned total, fixed at the chest's first sight -------
    {
        // total-planned: Live procedure 1's Pumpkin Cellar - 44 alive at entry,
        // 600 to clear, the spawners topping the room back up after every
        // kill. T = 600 at 50 % latches at the 300th kill, not the 299th; the
        // countdown appears at kill 250 (`50 kills to go`) and only ever
        // counts down; the total source is asked once.
        DC::State s;
        Arm(s, 50, false);
        sourceAnswer = 600;
        Dungeon d; d.alive = 44; d.pending = 556;
        Poll(s, d);
        const bool fixed = s.tally.total == 600 && s.tally.alive0 == 44 && s.tally.threshold == 300 && DC::HeadText(s).empty();
        long last = DC::Remaining(s.tally);
        bool monotone = true;
        std::string at249, at250;
        int early = 0;
        for (int k = 1; k <= 299; ++k) {
            Kill(s, d);
            if (Poll(s, d)) ++early;
            const long r = DC::Remaining(s.tally);
            if (r > last) monotone = false;
            last = r;
            if (k == 249) at249 = DC::HeadText(s);
            if (k == 250) at250 = DC::HeadText(s);
        }
        const bool notYet = early == 0 && !s.tally.latched && DC::Remaining(s.tally) == 1 && unlockCalls == 0;
        const std::string at299 = Tally(s);
        const int latched = KillAndPoll(s, d, 1);
        const std::string line = DC::UnlockedLine(s);
        const int unlocks = unlockCalls, asks = sourceCalls;
        // The clamp: a source that under-counts (10 answered, 40 alive) is
        // raised to the kills counted plus those alive, so 50 % is 20, not 5.
        DC::State c;
        Arm(c, 50, false);
        sourceAnswer = 10;
        Dungeon small; small.alive = 40;
        Poll(c, small);
        const int clampEarly = KillAndPoll(c, small, 19);
        const int clampLatch = KillAndPoll(c, small, 1);
        // The player build's estimate (`total-route: estimate`) gives back Live
        // procedure 1b's run A from its own inputs - 5 alive and 117 creators
        // still to spawn at first sight, 619 to clear - and rounds up.
        const bool estimate = DC::EstimatedTotal(5, 117) == 619 && DC::EstimatedTotal(5, 122) == 646
            && DC::EstimatedTotal(44, 0) == 44 && DC::EstimatedTotal(0, 1) == 6 && DC::EstimatedTotal(-3, -1) == 0;
        check("target/total-planned",
            fixed && monotone && notYet && latched == 1 && s.tally.latched && unlocks == 1
                && s.tally.threshold == 300 && asks == 1
                && at249.empty() && at250 == "Chest: 50 kills to go"
                && line == "dungeonchest: unlocked early at 300/600 alive=44"
                && clampEarly == 0 && clampLatch == 1 && c.tally.threshold == 20 && estimate,
            "at299: " + at299 + " | at300: " + Tally(s) + " | at249=[" + at249 + "] at250=[" + at250 + "] " + line
                + " | clamp: " + Tally(c) + " | estimate(5,117)=" + std::to_string(DC::EstimatedTotal(5, 117)));
    }
    {
        // total-unknown: with no total source the share is refused with
        // `total=unavailable`; with a source that answers 0 the tally never
        // latches and shows nothing however many die, and the source is asked
        // again every poll. Once it answers (10 still to die after 30 kills),
        // the total is those 10 plus the 30 already counted, fixed, and the
        // chest latches.
        const std::string refused = DC::RefusedLine(75, 0, "ok", "ok", false);
        DC::State s;
        Arm(s, 50, false);
        sourceAnswer = 0;
        Dungeon d; d.alive = 40;
        Poll(s, d);
        int latched = 0, shown = 0;
        for (int i = 0; i < 30; ++i) { Kill(s, d); if (Poll(s, d)) ++latched; if (!DC::HeadText(s).empty()) ++shown; }
        const bool silent = latched == 0 && shown == 0 && !s.tally.latched && unlockCalls == 0 && s.tally.threshold == 0
            && sourceCalls == 31 && DC::StatusLine(s, "ok", "ok").find(" total=unavailable ") != std::string::npos;
        const std::string before = Tally(s);
        sourceAnswer = 10;
        const int answered = Poll(s, d) ? 1 : 0;
        const int asks = sourceCalls;
        Poll(s, d);
        const int asksAfter = sourceCalls;
        const std::string after = Tally(s);
        DC::State none;
        Arm(none, 50, false);
        none.totalSource = nullptr;
        DC::State blind;
        Arm(blind, 50, false);
        blind.totalSource = &CensusSource;
        censusUnreadable = 4;
        Dungeon b; b.alive = 5;
        Poll(blind, b);
        const std::string blindLine = DC::StatusLine(blind, "ok", "ok");
        check("target/total-unknown",
            refused.find(" total=unavailable") != std::string::npos && refused.find("unchanged: off") != std::string::npos
                && silent && answered == 1 && s.tally.total == 40 && s.tally.latched && asks == 32 && asksAfter == 32
                && !DC::TotalAvailable(none) && DC::StatusLine(none, "ok", "ok").find(" total=unavailable ") != std::string::npos
                && blind.tally.total == 0 && blindLine.find(" total=unavailable(unreadable=4/4) ") != std::string::npos,
            refused + " | before: " + before + " | after: " + after + " | blind: " + blindLine);
    }
    {
        // total-census-refused (owner, D13): the build's estimate refuses a
        // census with any creator unreadable - 1 of 4 as well as 4 of 4 - and
        // one whose family resolved nothing or that found no creators. The
        // control beside it: the same census with every creator readable
        // estimates 20 alive + 2 pending x 614/117 = 31 and latches at 16.
        const DC::Census oneOf4{4, 2, 1}, allOf4{4, 0, 4}, readable{4, 2, 0}, empty{};
        const bool predicate = DC::EstimateFromCensus(true, 20, oneOf4) == 0
            && DC::EstimateFromCensus(true, 20, allOf4) == 0
            && DC::EstimateFromCensus(true, 20, empty) == 0
            && DC::EstimateFromCensus(false, 20, readable) == 0
            && DC::EstimateFromCensus(true, 20, readable) == 31;
        DC::State one;
        Arm(one, 50, false);
        one.totalSource = &CensusSource;
        censusUnreadable = 1;
        Dungeon d1; d1.alive = 20; d1.pending = 11;
        Poll(one, d1);
        const int oneLatched = KillAndPoll(one, d1, 31);
        const std::string oneLine = DC::StatusLine(one, "ok", "ok");
        DC::State lost;
        Arm(lost, 50, false);
        lost.totalSource = &CensusSource;
        censusFamily = false;
        Dungeon d2; d2.alive = 20;
        Poll(lost, d2);
        const std::string lostLine = DC::StatusLine(lost, "ok", "ok");
        DC::State ok;
        Arm(ok, 50, false);
        ok.totalSource = &CensusSource;
        Dungeon d3; d3.alive = 20; d3.pending = 11;
        Poll(ok, d3);
        const int okEarly = KillAndPoll(ok, d3, 15);
        const int okLatch = KillAndPoll(ok, d3, 1);
        check("target/total-census-refused",
            predicate && oneLatched == 0 && !one.tally.latched && one.tally.total == 0 && DC::HeadText(one).empty()
                && oneLine.find(" total=unavailable(unreadable=1/4) creators=4 ") != std::string::npos
                && !lost.tally.latched && lostLine.find(" total=unavailable creators=0 ") != std::string::npos
                && okEarly == 0 && okLatch == 1 && ok.tally.total == 31 && ok.tally.threshold == 16 && ok.tally.latched,
            "1/4: " + oneLine + " | family unresolved: " + lostLine + " | readable: " + Tally(ok));
    }

    // ---- target: the instance_exists detour's decision ----------------------
    {
        // poll-answered-while-latched: once the room's tally latches and the
        // unlock action answers, the chest's own instance_exists poll with an
        // Enemy_Parent_obj argument is answered `false`, each call counted.
        DC::State s;
        Arm(s, 50, false);
        Dungeon d; d.alive = 40;
        Poll(s, d);
        const bool before = !Ask(s, &d.chestInstance, true);
        KillAndPoll(s, d, 20);
        const bool first = Ask(s, &d.chestInstance, true);
        const bool second = Ask(s, &d.chestInstance, true);
        check("target/poll-answered-while-latched",
            before && s.tally.latched && s.tally.pollLatched && first && second
                && s.tally.answered == 2 && s.counters.answered == 2
                && DC::StatusLine(s, "ok", "ok").find(" unlock=ok answered=2 ") != std::string::npos,
            Tally(s));
    }
    {
        // poll-untouched-otherwise: every other call gets the original's
        // answer and is not counted - before the latch, another `self`, an
        // argument that is not an enemy, a latch whose unlock action failed,
        // and the next room - while the chest's own enemy poll, after the
        // same latch, is answered (the positive control beside them). Another
        // `self` costs no argument read.
        DC::State s;
        Arm(s, 50, false);
        Dungeon d; d.alive = 40;
        int other = 0;
        Poll(s, d);
        const bool notLatched = !Ask(s, &d.chestInstance, true);
        KillAndPoll(s, d, 20);
        argReads = 0;
        const bool otherSelf = !Ask(s, &other, true) && argReads == 0;
        const bool nullSelf = !Ask(s, nullptr, true) && argReads == 0;
        const bool otherArg = !Ask(s, &d.chestInstance, false);
        const long untouched = s.tally.answered;
        const bool chestPoll = Ask(s, &d.chestInstance, true);
        Dungeon next; next.room = 217; next.alive = 30;
        Poll(s, next);
        const bool nextRoom = !Ask(s, &d.chestInstance, true) && !Ask(s, &next.chestInstance, true) && s.tally.answered == 0;
        DC::State f;
        Arm(f, 50, false);
        f.unlock = &FailUnlock;
        Dungeon fd; fd.alive = 40;
        Poll(f, fd);
        KillAndPoll(f, fd, 20);
        const bool failed = f.tally.latched && !Ask(f, &fd.chestInstance, true) && f.counters.answered == 0;
        check("target/poll-untouched-otherwise",
            notLatched && otherSelf && nullSelf && otherArg && untouched == 0 && chestPoll
                && s.counters.answered == 1 && nextRoom && failed,
            Tally(s) + " | failed unlock: " + Tally(f));
    }

    // ---- target: the countdown's text --------------------------------------
    {
        DC::State s;
        Arm(s, 50, false);
        Dungeon d; d.alive = 120;   // threshold 60
        Poll(s, d);
        const bool above = DC::HeadText(s).empty() && DC::Remaining(s.tally) == 60;
        KillAndPoll(s, d, 10);
        const std::string at50 = DC::HeadText(s);
        KillAndPoll(s, d, 49);
        const std::string at1 = DC::HeadText(s);
        KillAndPoll(s, d, 1);
        const std::string at0 = DC::HeadText(s);
        check("target/countdown_text",
            above && at50 == "Chest: 50 kills to go" && at1 == "Chest: 1 kills to go" && at0.empty() && s.tally.latched,
            "at50=[" + at50 + "] at1=[" + at1 + "] at0=[" + at0 + "] " + Tally(s));
    }

    // ---- target: a kill reported twice for one id counts once --------------
    {
        DC::State s;
        Arm(s, 50, false);
        Dungeon d; d.alive = 40;
        Poll(s, d);
        KillAndPoll(s, d, 18);
        const int id = d.nextId;
        Kill(s, d);                                   // the 19th
        const bool second = DC::CountKill(s, id);     // the same enemy, a second trigger
        Poll(s, d);
        const bool once = !second && s.tally.kills == 19 && !s.tally.latched && s.counters.duplicates == 1;
        const std::string at19 = Tally(s);
        const int latched = KillAndPoll(s, d, 1);     // the 20th distinct enemy
        check("target/kill_once_per_id",
            once && latched == 1 && s.tally.kills == 20 && unlockCalls == 1,
            "after duplicate: " + at19 + " | " + Tally(s));
    }

    // ---- target: a room change, or the chest gone, resets the tally --------
    {
        DC::State s;
        Arm(s, 50, false);
        Dungeon d; d.alive = 10;
        Poll(s, d);
        const int latched = KillAndPoll(s, d, 5);
        const bool wasLatched = latched == 1 && s.tally.latched && s.tally.pollLatched;
        Dungeon next; next.room = 217; next.alive = 30;
        Poll(s, next);
        const bool reset = !s.tally.latched && !s.tally.pollLatched && s.tally.kills == 0 && s.tally.alive == 30
            && s.tally.total == 30 && s.tally.threshold == 15 && s.tally.chest == &next.chestInstance;
        const int again = KillAndPoll(s, next, 15);
        next.chests = 0;
        Poll(s, next);
        const bool gone = !s.tally.active && s.tally.kills == 0 && !s.tally.latched && s.tally.chest == nullptr;
        check("target/room_change_resets",
            wasLatched && reset && again == 1 && gone && unlockCalls == 2,
            Tally(s));
    }

    // ---- countdown forms ----------------------------------------------------
    {
        // countdown-head: the default form draws the text and sends no chat line.
        DC::State s;
        Arm(s, 50, true);
        Dungeon d; d.alive = 40;
        Poll(s, d);
        KillAndPoll(s, d, 5);
        const std::string head = DC::HeadText(s);
        check("form/countdown-head",
            DC::CurrentForm(s) == DC::Form::Head && head == "Chest: 15 kills to go" && chatLines.empty()
                && DC::StatusLine(s, "ok", "ok").find(" countdown=head chat=ok chatLines=0 ") != std::string::npos,
            "head=[" + head + "] lines=" + Lines());
    }
    {
        // countdown-head-stable (D12, the owner's Live procedure 1b report of a
        // label that blinked and jerked): over frames with no kill, polls
        // included, the label is present on every frame with the very same
        // text, and it is placed on the same whole pixel while the player
        // stands still - though the sprite's box top moves with its animation
        // frame and the camera sits on a fractional position, which a label
        // hung from the box top each frame follows (the negative control
        // beside it). A kill changes the text exactly once, and the label goes
        // when the form draws nothing above the head.
        DC::State s;
        Arm(s, 50, false);
        Dungeon d; d.alive = 40;
        Poll(s, d);
        KillAndPoll(s, d, 5);                     // remaining 15
        const double x = 812.0, y = 640.0, vx = 100.25, vy = 220.5, vw = 1280.0, vh = 720.0, gw = 1920.0, gh = 1080.0;
        const double tops[] = { 600.0, 598.0, 601.0, 597.0 };   // the box top as the idle animation cycles
        DC::HeadLabel& label = DC::UpdateHeadLabel(s);
        const std::string first = label.text;
        const long rewrites0 = label.rewrites;
        const DC::LabelSpot spot0 = DC::PlaceHeadLabel(label, x, y, tops[0], vx, vy, vw, vh, gw, gh, 150.0);
        bool same = DC::LabelShown(label) && first == "Chest: 15 kills to go";
        bool whole = spot0.x == std::floor(spot0.x) && spot0.y == std::floor(spot0.y);
        bool naiveMoved = false;
        double naive0 = 0.0;
        for (int frame = 1; frame <= 240; ++frame) {
            if (frame % 60 == 0) { d.alive = frame % 120 == 0 ? 35 : 33; Poll(s, d); }   // the once-a-second poll
            DC::HeadLabel& l = DC::UpdateHeadLabel(s);
            const double top = tops[frame % 4];
            const DC::LabelSpot p = DC::PlaceHeadLabel(l, x, y, top, vx, vy, vw, vh, gw, gh, 150.0);
            same = same && DC::LabelShown(l) && l.text == first && p.x == spot0.x && p.y == spot0.y;
            const double naive = (top - vy) * gh / vh - 150.0;
            if (frame == 1) naive0 = naive; else if (naive != naive0) naiveMoved = true;
        }
        same = same && DC::UpdateHeadLabel(s).rewrites == rewrites0;
        Kill(s, d);                               // remaining 14, between polls
        std::string after;
        for (int frame = 0; frame < 30; ++frame) after = DC::UpdateHeadLabel(s).text;
        const long rewrites1 = DC::UpdateHeadLabel(s).rewrites;
        DC::SetForm(s, DC::Form::None);
        const bool gone = !DC::LabelShown(DC::UpdateHeadLabel(s)) && !s.label.anchored;
        check("form/countdown-head-stable",
            same && whole && naiveMoved && after == "Chest: 14 kills to go" && rewrites1 == rewrites0 + 1 && gone,
            "first=[" + first + "] after=[" + after + "] rewrites " + std::to_string(rewrites0) + "->"
                + std::to_string(rewrites1) + " spot=" + std::to_string(spot0.x) + "," + std::to_string(spot0.y)
                + (naiveMoved ? " (a box-top label moved)" : " (control: a box-top label did not move)"));
    }
    {
        // countdown-none: neither the head text nor a chat line.
        DC::State s;
        Arm(s, 50, true);
        const bool set = DC::SetForm(s, DC::Form::None);
        Dungeon d; d.alive = 40;
        Poll(s, d);
        KillAndPoll(s, d, 5);
        check("form/countdown-none",
            set && DC::CurrentForm(s) == DC::Form::None && DC::HeadText(s).empty() && chatLines.empty(),
            "lines=" + Lines());
    }
    {
        // chat-refused: with no chat action, `chat` and `both` are refused and
        // the form stays `head`; `none` and `head` are still accepted.
        DC::State s;
        Arm(s, 50, false);
        const bool chat = DC::SetForm(s, DC::Form::Chat);
        const bool both = DC::SetForm(s, DC::Form::Both);
        const bool stays = DC::CurrentForm(s) == DC::Form::Head;
        const std::string line = DC::FormRefusedLine(DC::Form::Chat, DC::CurrentForm(s));
        const bool status = DC::StatusLine(s, "ok", "ok").find(" chat=unavailable ") != std::string::npos;
        const bool none = DC::SetForm(s, DC::Form::None) && DC::SetForm(s, DC::Form::Head);
        check("form/chat-refused",
            !chat && !both && stays && status && none
                && line.find("countdown chat refused: chat route not available") != std::string::npos
                && line.find("unchanged: head") != std::string::npos,
            line);
    }
    {
        // chat-failed: the chat call fails on its first line. It is counted
        // (`chatFailed`), the chat forms switch off for the session
        // (`chat=unavailable`, a later `countdown chat` refused), the form
        // falls back to `head` so the countdown still shows, and the failing
        // call is never made again, however many milestones follow.
        DC::State s;
        Arm(s, 50, false);
        s.chat = &FailChat;
        const bool set = DC::SetForm(s, DC::Form::Chat);
        Dungeon d; d.alive = 40;
        Poll(s, d);
        KillAndPoll(s, d, 15);
        const std::string head = DC::HeadText(s);
        check("form/chat-failed",
            set && chatFailures == 1 && s.counters.chatFailed == 1 && s.counters.chatLines == 0
                && !DC::ChatAvailable(s) && DC::CurrentForm(s) == DC::Form::Head && head == "Chest: 5 kills to go"
                && !DC::SetForm(s, DC::Form::Both)
                && DC::StatusLine(s, "ok", "ok").find(" countdown=head chat=unavailable ") != std::string::npos,
            "head=[" + head + "] failures=" + std::to_string(chatFailures));
    }

    // ---- target: the chat form's milestones ---------------------------------
    {
        // chat-milestones: 40 alive at 50 % starts at 20 remaining (past 50, 40,
        // 30 and 20 at once: one line), then 10, 5, 4, 3, 2, 1, and the ready
        // line at the latch - each once.
        DC::State s;
        Arm(s, 50, true);
        const bool set = DC::SetForm(s, DC::Form::Chat);
        Dungeon d; d.alive = 40;
        Poll(s, d);
        KillAndPoll(s, d, 25);
        const std::vector<std::string> want = {
            "Chest: 20 kills to go", "Chest: 10 kills to go", "Chest: 5 kills to go", "Chest: 4 kills to go",
            "Chest: 3 kills to go", "Chest: 2 kills to go", "Chest: 1 kills to go", "Chest: ready to open" };
        check("target/chat-milestones",
            set && chatLines == want && DC::HeadText(s).empty() && s.counters.chatLines == 8 && unlockCalls == 1,
            Lines());
    }
    {
        // chat-milestones, a double kill: two kills before one poll pass 5 and
        // 4 together and send one line, with the current number.
        DC::State s;
        Arm(s, 50, true);
        DC::SetForm(s, DC::Form::Both);
        Dungeon d; d.alive = 40;
        Poll(s, d);
        KillAndPoll(s, d, 14);           // remaining 6
        chatLines.clear();
        Kill(s, d); Kill(s, d);          // remaining 4, one poll
        Poll(s, d);
        const std::vector<std::string> pair = chatLines;
        const std::string head = DC::HeadText(s);
        KillAndPoll(s, d, 4);
        check("target/chat-milestones-double-kill",
            pair == std::vector<std::string>{ "Chest: 4 kills to go" } && head == "Chest: 4 kills to go"
                && chatLines.back() == "Chest: ready to open" && chatLines.size() == 5 && unlockCalls == 1,
            Lines());
    }

    // ---- the command's words -------------------------------------------------
    {
        DC::Form f = DC::Form::Head;
        bool ok = DC::ParseForm("chat", f) && f == DC::Form::Chat;
        ok = ok && DC::ParseForm("both", f) && f == DC::Form::Both;
        ok = ok && DC::ParseForm("none", f) && f == DC::Form::None;
        ok = ok && DC::ParseForm("head", f) && f == DC::Form::Head;
        ok = ok && !DC::ParseForm("loud", f) && f == DC::Form::Head;
        int pct = 75;
        ok = ok && DC::ParsePct("off", pct) && pct == 0 && DC::ParsePct("0", pct) && pct == 0;
        ok = ok && DC::ParsePct("73", pct) && pct == 73;
        pct = 75;
        ok = ok && !DC::ParsePct("abc", pct) && !DC::ParsePct("", pct) && !DC::ParsePct("7.5", pct) && pct == 75;
        check("command/words_parsed", ok);
    }
    {
        // What DungeonChestCommand stores: 50, 73 and 95 are stored with the
        // kill hook on both routes, the instance_exists detour installed and a
        // total source supplied; 49, 96 and `abc` are refused; a failed,
        // table-only or never-asked kill hook or detour, or no total source,
        // refuses every share and never `off`. Each refusal sits beside a
        // stored triple, so a decision that refuses everything fails here too.
        struct Case { const char* word; const char* hook; const char* unlock; bool total; bool stored; };
        const Case cases[] = {
            { "50", "ok", "ok", true, true }, { "73", "ok", "ok", true, true }, { "95", "ok", "ok", true, true },
            { "49", "ok", "ok", true, false }, { "96", "ok", "ok", true, false }, { "abc", "ok", "ok", true, false },
            { "75", "failed", "ok", true, false }, { "75", "table-only", "ok", true, false }, { "75", "none", "ok", true, false },
            { "75", "ok", "failed", true, false }, { "75", "ok", "table-only", true, false }, { "75", "ok", "none", true, false },
            { "75", "ok", "ok", false, false },
            { "off", "failed", "none", false, true }, { "off", "table-only", "failed", true, true }, { "0", "ok", "ok", false, true },
        };
        bool ok = true;
        std::string detail;
        for (const Case& c : cases) {
            int pct = -1;
            const bool parsed = DC::ParsePct(c.word, pct);
            const bool stored = parsed && DC::StoresMode(pct, c.hook, c.unlock, c.total);
            if (stored != c.stored) {
                ok = false;
                detail += std::string(" ") + c.word + "/hook=" + c.hook + "/unlock=" + c.unlock + (c.total ? "/total" : "/no-total");
            }
        }
        const std::string range = DC::RefusedLine(49, 0, "ok", "ok", true);
        const std::string hook = DC::RefusedLine(75, 0, "failed", "ok", true);
        const std::string tableOnly = DC::RefusedLine(75, 0, "table-only", "ok", true);
        const std::string noUnlock = DC::RefusedLine(75, 0, "ok", "failed", true);
        const std::string noTotal = DC::RefusedLine(75, 0, "ok", "ok", false);
        ok = ok && range.rfind("dungeonchest: refused 49 ", 0) == 0 && range.find("unchanged: off") != std::string::npos
            && hook.find(" hook=failed") != std::string::npos && tableOnly.find(" hook=table-only") != std::string::npos
            && noUnlock.find(" unlock=failed") != std::string::npos && noUnlock.find("unchanged: off") != std::string::npos
            && noTotal.find(" total=unavailable") != std::string::npos;
        check("command/refused", ok, range + " | " + hook + " | " + tableOnly + " | " + noUnlock + " | " + noTotal
            + (detail.empty() ? "" : " wrong:" + detail));
    }
    {
        DC::State s;
        Arm(s, 50, false);
        Dungeon d; d.alive = 40;
        Poll(s, d);
        KillAndPoll(s, d, 3);
        const std::string line = DC::StatusLine(s, "ok", "ok");
        check("command/status_line",
            line == "dungeonchest: 50% | kills=3 notEnemy=0 total=40 creators=0 pending=0 unreadable=0 alive=37 threshold=20 remaining=17 latched=0 unlocked=0 unlock=ok answered=0 countdown=head chat=unavailable chatLines=0 hook=ok",
            line);
    }

    std::cout << (failures ? "RESULT FAIL" : "RESULT OK") << "\n";
    return failures ? 1 : 0;
}
