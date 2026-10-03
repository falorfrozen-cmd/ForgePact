// Behavioral regression harness for Dungeon chest opens early
// (DungeonChestMod.hpp, issue #31).
//
// The Python runner splices the REAL ForgePact::DungeonChest header in below.
// Only the game is replaced: a fake dungeon answers what ModuleMain's
// once-a-second poll reads (the room, how many Dungeon_Chest_obj and how many
// Enemy_Parent_obj instances it holds), each kill arrives as the kill hook
// hands it over (the dying enemy's instance id), and the two actions the
// adapter supplies are recorders: `unlock` counts its calls and answers that
// its write was made (or, in `unlock-failed`, that it failed), `chat` keeps
// the lines it was given. No game process is touched.
//
// Every target that says "not yet" sits beside the same tally going on to the
// latch, so a decision that never unlocks fails it instead of passing it.
#include <atomic>
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

// The adapter's two actions, as recorders.
static int unlockCalls = 0;
static std::vector<std::string> chatLines;
static bool RecordUnlock() { ++unlockCalls; return true; }
// An unlock action whose write fails (the route refused, the chest unreadable).
static bool FailUnlock() { ++unlockCalls; return false; }
static bool RecordChat(const std::string& line) { chatLines.push_back(line); return true; }
static void ResetRecorders() { unlockCalls = 0; chatLines.clear(); }

// One dungeon as the poll reads it. Every kill the game makes moves one
// monster from alive to dead; `Kill` hands it to the kill hook's path.
struct Dungeon {
    int64_t room = 216;
    long chests = 1;
    long alive = 0;
    int nextId = 100000;
};
static bool Poll(DC::State& s, const Dungeon& d) { return DC::Poll(s, true, d.room, d.chests, d.alive); }
static bool Kill(DC::State& s, Dungeon& d) {
    if (d.alive > 0) --d.alive;
    return DC::CountKill(s, d.nextId++);
}
// `n` kills, each followed by the poll, the way one kill a second plays out.
static int KillAndPoll(DC::State& s, Dungeon& d, int n) {
    int latched = 0;
    for (int i = 0; i < n; ++i) { Kill(s, d); if (Poll(s, d)) ++latched; }
    return latched;
}
static std::string Tally(const DC::State& s) {
    const DC::Tally& t = s.tally;
    return "kills=" + std::to_string(t.kills) + " alive=" + std::to_string(t.alive)
        + " threshold=" + std::to_string(t.threshold) + " remaining=" + std::to_string(DC::Remaining(t))
        + " latched=" + std::to_string(t.latched) + " unlocked=" + std::to_string(t.unlocked) + " unlockCalls=" + std::to_string(unlockCalls)
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
                && DC::StatusLine(s, "none").rfind("dungeonchest: off | ", 0) == 0,
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
            latched == 0 && !s.tally.active && s.tally.kills == 0 && unlockCalls == 0 && DC::HeadText(s).empty(),
            Tally(s));
    }

    // ---- target: 50 % of 40 latches at the 20th kill, not the 19th ---------
    {
        DC::State s;
        Arm(s, 50, false);
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
                && unlockedLine == "dungeonchest: unlocked early at 20/20 alive=20"
                && DC::StatusLine(s, "ok").find(" latched=1 unlocked=1 unlockRoute=ok ") != std::string::npos,
            "at19: " + at19 + " | at20+: " + Tally(s) + " | " + unlockedLine);
    }
    {
        // unlock-failed: the threshold latches but the unlock action's write
        // fails. The status line and the latch line say so, `unlocked` stays
        // 0, and no `ready to open` line is sent for a chest the game keeps shut.
        DC::State s;
        Arm(s, 50, true);
        s.unlock = &FailUnlock;
        DC::SetForm(s, DC::Form::Both);
        Dungeon d; d.alive = 40;
        Poll(s, d);
        const int latched = KillAndPoll(s, d, 20);
        const std::string line = DC::UnlockedLine(s);
        check("target/unlock-failed",
            latched == 1 && s.tally.latched && !s.tally.unlocked && unlockCalls == 1
                && s.counters.unlocks == 0 && s.counters.unlockFailed == 1
                && line.rfind("dungeonchest: threshold reached at 20/20 ", 0) == 0
                && line.find("unlock action failed") != std::string::npos
                && DC::StatusLine(s, "ok").find(" latched=1 unlocked=0 ") != std::string::npos
                && !chatLines.empty() && chatLines.back() != "Chest: ready to open",
            line + " | " + Tally(s) + " " + Lines());
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
        const bool wasLatched = latched == 1 && s.tally.latched;
        Dungeon next; next.room = 217; next.alive = 30;
        Poll(s, next);
        const bool reset = !s.tally.latched && s.tally.kills == 0 && s.tally.alive == 30 && s.tally.threshold == 15;
        const int again = KillAndPoll(s, next, 15);
        next.chests = 0;
        Poll(s, next);
        const bool gone = !s.tally.active && s.tally.kills == 0 && !s.tally.latched;
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
                && DC::StatusLine(s, "ok").find(" countdown=head chat=ok chatLines=0 ") != std::string::npos,
            "head=[" + head + "] lines=" + Lines());
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
        const bool status = DC::StatusLine(s, "ok").find(" chat=unavailable ") != std::string::npos;
        const bool none = DC::SetForm(s, DC::Form::None) && DC::SetForm(s, DC::Form::Head);
        check("form/chat-refused",
            !chat && !both && stays && status && none
                && line.find("countdown chat refused: chat route not available") != std::string::npos
                && line.find("unchanged: head") != std::string::npos,
            line);
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
        // kill hook on both routes and an unlock action supplied; 49, 96 and
        // `abc` are refused; a failed or table-only kill hook, or no unlock
        // action, refuses every share and never `off`. Each refusal sits
        // beside a stored pair, so a decision that refuses everything fails
        // here too.
        struct Case { const char* word; const char* hook; bool unlock; bool stored; };
        const Case cases[] = {
            { "50", "ok", true, true }, { "73", "ok", true, true }, { "95", "ok", true, true },
            { "49", "ok", true, false }, { "96", "ok", true, false }, { "abc", "ok", true, false },
            { "75", "failed", true, false }, { "75", "table-only", true, false }, { "75", "none", true, false },
            { "75", "ok", false, false },
            { "off", "failed", true, true }, { "off", "table-only", false, true }, { "0", "ok", false, true },
        };
        bool ok = true;
        std::string detail;
        for (const Case& c : cases) {
            int pct = -1;
            const bool parsed = DC::ParsePct(c.word, pct);
            const bool stored = parsed && DC::StoresMode(pct, c.hook, c.unlock);
            if (stored != c.stored) { ok = false; detail += std::string(" ") + c.word + "/" + c.hook + (c.unlock ? "/unlock" : "/no-unlock"); }
        }
        const std::string range = DC::RefusedLine(49, 0, "ok", true);
        const std::string hook = DC::RefusedLine(75, 0, "failed", true);
        const std::string tableOnly = DC::RefusedLine(75, 0, "table-only", true);
        const std::string noUnlock = DC::RefusedLine(75, 0, "ok", false);
        ok = ok && range.rfind("dungeonchest: refused 49 ", 0) == 0 && range.find("unchanged: off") != std::string::npos
            && hook.find(" hook=failed") != std::string::npos && tableOnly.find(" hook=table-only") != std::string::npos
            && noUnlock.find(" unlockRoute=unavailable") != std::string::npos && noUnlock.find("unchanged: off") != std::string::npos;
        check("command/refused", ok, range + " | " + hook + " | " + tableOnly + " | " + noUnlock
            + (detail.empty() ? "" : " wrong:" + detail));
    }
    {
        DC::State s;
        Arm(s, 50, false);
        Dungeon d; d.alive = 40;
        Poll(s, d);
        KillAndPoll(s, d, 3);
        const std::string line = DC::StatusLine(s, "ok");
        check("command/status_line",
            line == "dungeonchest: 50% | kills=3 alive=37 threshold=20 remaining=17 latched=0 unlocked=0 unlockRoute=ok countdown=head chat=unavailable chatLines=0 hook=ok",
            line);
    }

    std::cout << (failures ? "RESULT FAIL" : "RESULT OK") << "\n";
    return failures ? 1 : 0;
}
