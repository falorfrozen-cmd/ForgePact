#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ForgePact::GambaProbe {

// ---- gambaprobe's decision core (ForgePact #134, research build only) -------
//
// docs/gamba-machine-research.md asks how the gamba machine
// (Slot_Machine_01_obj) takes a spin, when it explodes, where it rolls and
// builds its prize (Goburin's Head), and which of those calls a pity mod could
// answer. ModuleMain.cpp's `gambaprobe` hooks every candidate the static
// search found - the machine's own events, the scripts its events call, and
// the RNG and instance builtins - and this header decides, per call, what is
// counted, what is logged and whether the one research lever answers it.
//
// It is game-independent by contract - row numbers, object indices, numbers
// and strings, never an instance or an RValue - so
// tests/gamba_probe_harness.cpp compiles it whole. The adapter in
// ModuleMain.cpp supplies the object index of a call's self (only when the
// probe is active, through a callback, so an idle detour reads nothing), the
// detours, the hashes of a line's text and the printing.

// ---- the rows ---------------------------------------------------------------
// Rows are numbered events first, then builtins, then the adapter's script
// rows (their names live in ModuleMain.cpp's table, as hs-game-sdk
// constants, never here).

// The machine's object events the instrument reaches through the compiled-code
// table: the adapter names each `gml_Object_<object>_<event>`. Create_0 and
// Alarm_9 are the spawn's positive control; Step_0 is where the static reading
// puts the prize. `key` is the event's name in the status line.
struct EventRow {
    std::string_view event;
    std::string_view key;
};
inline constexpr int kEventCount = 5;
inline constexpr EventRow kEvents[kEventCount] = {
    { "Create_0",  "create"  },
    { "Alarm_0",   "alarm0"  },
    { "Alarm_9",   "alarm9"  },
    { "Step_0",    "step"    },
    { "CleanUp_0", "cleanup" },
};
enum class Event : int { Create, Alarm0, Alarm9, Step, CleanUp };

// How the RNG lever answers a builtin. Value: the lever's value is the result.
// ArgumentIndex: `choose` returns one of its own arguments, so the value names
// which one (0-based); a value that names no argument of this call is not
// answered (counted out-of-range) and the game's own function runs. NotRng:
// the lever never answers the row.
enum class AnswerKind : int { NotRng, Value, ArgumentIndex };

struct BuiltinRow {
    std::string_view name;
    AnswerKind       kind;
};
inline constexpr int kBuiltinCount = 8;
inline constexpr BuiltinRow kBuiltins[kBuiltinCount] = {
    { "irandom",               AnswerKind::Value         },
    { "irandom_range",         AnswerKind::Value         },
    { "random",                AnswerKind::Value         },
    { "random_range",          AnswerKind::Value         },
    { "choose",                AnswerKind::ArgumentIndex },
    { "instance_destroy",      AnswerKind::NotRng        },
    { "instance_create_depth", AnswerKind::NotRng        },
    { "instance_create_layer", AnswerKind::NotRng        },
};
enum class Builtin : int {
    Irandom, IrandomRange, Random, RandomRange, Choose, InstanceDestroy, InstanceCreateDepth, InstanceCreateLayer,
};

inline constexpr int kFirstBuiltinRow = kEventCount;
inline constexpr int kFirstScriptRow = kEventCount + kBuiltinCount;
inline constexpr int EventRowOf(Event e) { return static_cast<int>(e); }
inline constexpr int BuiltinRowOf(Builtin b) { return kFirstBuiltinRow + static_cast<int>(b); }
inline constexpr int ScriptRowOf(int script) { return kFirstScriptRow + script; }

// How `hook` reached a row. Detoured: the probe's own inline detour (both
// routes for a script). TableOnly: the script-table swap only, blind to
// compiled GML's direct calls. Shared: another install already detours the
// function and the probe observes through that detour. Missing: nothing of
// the probe sees the row.
enum class Route : int { Missing, Detoured, TableOnly, Shared };

inline constexpr std::string_view RouteName(Route r)
{
    switch (r) {
    case Route::Missing:   return "missing";
    case Route::Detoured:  return "detoured";
    case Route::TableOnly: return "table-only";
    case Route::Shared:    return "shared";
    }
    return "?";
}

// ---- the trace budget and the lever's limits --------------------------------
// The first kTraceLinesPerRow lines per row are logged; after that the row
// only counts. A line identical to the row's previous line for the same key
// (a script's first argument, e.g. GPV's state key) is not logged and spends
// nothing, so a machine's idle Step_0 reading the same state every frame does
// not use the budget a spin needs.
inline constexpr int kTraceLinesPerRow = 40;
inline constexpr int64_t kRngMinCount = 1;
inline constexpr int64_t kRngMaxCount = 50;

// ---- the decision keys ------------------------------------------------------
// docs/gamba-machine-research.md § Decision carries one line per key, `pending`
// until Live 1, then one of these labels, backed by the named check. An open
// key (drop-route) takes the name of the script or builtin that places the
// prize instead, or `not-observed`.
inline constexpr std::string_view kPending = "pending";
struct DecisionKey {
    std::string_view key;
    std::string_view check;
    std::string_view labels[4];
    int              labelCount;
    bool             openName;
};
inline constexpr int kDecisionKeyCount = 6;
inline constexpr DecisionKey kDecisionKeys[kDecisionKeyCount] = {
    { "roll-route",     "roll-identity",   { "builtin", "script", "method", "not-observed" },             4, false },
    { "explosion-rule", "explosion-trace", { "gold", "spins", "random", "not-observed" },                 4, false },
    { "drop-route",     "prize-trace",     { "not-observed", "", "", "" },                                1, true  },
    { "counter-route",  "spin-trace",      { "spins", "explosions", "both", "not-observed" },             4, false },
    { "fallback-drop",  "fallback-drop",   { "proven", "failed", "not-run", "" },                         3, false },
    { "pity-design",    "",                { "answer-roll", "force-script", "drop-ourselves", "blocked" }, 4, false },
};

// Is `label` an answer for `key`? `pending` only until Live 1 has results;
// an open key also takes a script or builtin name (letters, digits, `_`, `@`).
inline bool DecisionLabelValid(std::string_view key, std::string_view label, bool live1Recorded)
{
    for (const DecisionKey& k : kDecisionKeys) {
        if (k.key != key) continue;
        if (label == kPending) return !live1Recorded;
        for (int i = 0; i < k.labelCount; ++i)
            if (k.labels[i] == label) return true;
        if (!k.openName || label.empty()) return false;
        for (const char c : label) {
            const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '@';
            if (!ok) return false;
        }
        return true;
    }
    return false;
}

// ---- per call -----------------------------------------------------------------
// Where one call went. Idle: the probe is neither armed nor levered, and only
// `calls` moved. Machine: self is a gamba machine. InEvent: another self, but
// inside a machine's own event (a `with` block or a struct method the event
// runs). Other: anything else.
enum class Seen : int { Idle, Machine, InEvent, Other };

struct Counters {
    uint64_t calls = 0;         // every call, armed or not
    uint64_t machineSelf = 0;   // armed or levered: self is a gamba machine
    uint64_t inEvent = 0;       // armed or levered: another self inside a machine's event
    uint64_t otherSelf = 0;     // armed or levered: everything else
    uint64_t logged = 0;        // lines written (at most kTraceLinesPerRow)
    uint64_t answered = 0;      // RNG rows: calls the lever answered
    uint64_t outOfRange = 0;    // `choose`: a lever value that named none of the call's arguments
};

// One RNG builtin call's answer. `answer` false: the game's own function runs.
struct RngDecision {
    Seen   seen = Seen::Idle;
    bool   answer = false;
    double value = 0.0;   // the result, or for ArgumentIndex the argument's index
};

// Integral numbers print without a fraction; anything else as %g.
inline std::string NumberText(double v)
{
    if (std::isfinite(v) && v == std::floor(v) && std::fabs(v) < 9.0e15) return std::to_string(static_cast<long long>(v));
    char b[48];
    std::snprintf(b, sizeof(b), "%g", v);
    return b;
}

class Probe {
public:
    // The adapter's script rows; every counter of every row starts at zero.
    void SetScriptRowCount(int scripts)
    {
        rows_.assign(static_cast<size_t>(kFirstScriptRow + (scripts > 0 ? scripts : 0)), Counters{});
        lastLine_.assign(rows_.size(), {});
    }
    int RowCount() const { return static_cast<int>(rows_.size()); }

    // Slot_Machine_01_obj's index, as the adapter resolved it by name; -1 when
    // it did not resolve, and then nothing is a machine.
    void SetMachineObject(int object) { machineObject_ = object; }
    int MachineObject() const { return machineObject_; }

    // The machine-self predicate: an object index equal to the machine's, else
    // false. Never a kind check: the adapter hands over an object index, and an
    // unknown self is -1.
    bool IsMachine(int object) const { return machineObject_ >= 0 && object == machineObject_; }

    // ---- what makes the probe do anything per call -------------------------
    // Armed (`hook`, until `off`) or the lever on: the adapter classifies calls
    // only then. Otherwise each detour only counts, and the tick returns at once.
    void SetArmed(bool on) { armed_ = on; }
    bool Armed() const { return armed_; }
    bool Active() const { return armed_ || rngOn_; }

    // `hook` again: every row's trace budget and repeat memory start over; the
    // counts continue.
    void ResetTrace()
    {
        for (Counters& c : rows_) c.logged = 0;
        for (auto& m : lastLine_) m.clear();
    }

    // One call of any row. `selfObject()` returns the object index of the
    // call's self (-1 when it is not an instance) and is called only while the
    // probe is active, so an idle call costs no read.
    template <class SelfFn>
    Seen Observe(int row, SelfFn&& selfObject, bool inMachineEvent)
    {
        if (row < 0 || row >= RowCount()) return Seen::Idle;
        Counters& c = rows_[static_cast<size_t>(row)];
        ++c.calls;
        if (!Active()) return Seen::Idle;
        int object = -1;
        try { object = selfObject(); } catch (...) { object = -1; }
        if (IsMachine(object)) { ++c.machineSelf; return Seen::Machine; }
        if (inMachineEvent) { ++c.inEvent; return Seen::InEvent; }
        ++c.otherSelf;
        return Seen::Other;
    }

    // One RNG builtin call: Observe, then the lever. Answered only for a
    // machine self, only while the lever is on and has answers left, and for
    // `choose` only with a value that names one of this call's `argc`
    // arguments. The count-th answer turns the lever off.
    template <class SelfFn>
    RngDecision DecideRng(Builtin builtin, SelfFn&& selfObject, bool inMachineEvent, int argc)
    {
        RngDecision d;
        const int b = static_cast<int>(builtin);
        if (b < 0 || b >= kBuiltinCount) return d;
        const int row = kFirstBuiltinRow + b;
        d.seen = Observe(row, selfObject, inMachineEvent);
        const AnswerKind kind = kBuiltins[b].kind;
        if (d.seen != Seen::Machine || kind == AnswerKind::NotRng || !rngOn_ || rngRemaining_ <= 0) return d;
        Counters& c = rows_[static_cast<size_t>(row)];
        if (kind == AnswerKind::ArgumentIndex) {
            const double v = rngValue_;
            if (!(v >= 0.0) || v != std::floor(v) || v >= static_cast<double>(argc)) {
                ++c.outOfRange;
                ++rngOutOfRange_;
                return d;
            }
        }
        d.answer = true;
        d.value = rngValue_;
        ++c.answered;
        ++rngAnswered_;
        if (--rngRemaining_ <= 0) {
            rngOn_ = false;
            rngFinished_ = true;
        }
        return d;
    }

    // ---- the trace budget --------------------------------------------------
    // May the row write this line? `key` identifies what the line is about
    // (a script's first argument, or 0); `text` is the line's content without
    // its call number or frame. A repeat of the row's last line for the same
    // key is refused and spends nothing; a new line spends one of the row's
    // kTraceLinesPerRow.
    bool TakeTraceLine(int row, uint64_t key, uint64_t text)
    {
        if (row < 0 || row >= RowCount()) return false;
        Counters& c = rows_[static_cast<size_t>(row)];
        auto& last = lastLine_[static_cast<size_t>(row)];
        const auto it = last.find(key);
        if (it != last.end() && it->second == text) return false;
        if (c.logged >= static_cast<uint64_t>(kTraceLinesPerRow)) return false;
        last[key] = text;
        ++c.logged;
        return true;
    }

    // ---- the lever -----------------------------------------------------------
    // `rng <value> [count]`: answer the next `count` machine-self RNG builtin
    // calls with `value`. False, and nothing changed, for a value that is not
    // finite or a count out of range.
    bool SetRng(double value, int64_t count)
    {
        if (!std::isfinite(value) || count < kRngMinCount || count > kRngMaxCount) return false;
        rngOn_ = true;
        rngFinished_ = false;
        rngValue_ = value;
        rngCount_ = count;
        rngRemaining_ = count;
        rngAnswered_ = 0;
        rngOutOfRange_ = 0;
        return true;
    }

    void RngOff()
    {
        rngOn_ = false;
        rngRemaining_ = 0;
    }

    bool RngOn() const { return rngOn_; }
    double RngValue() const { return rngValue_; }
    int64_t RngCount() const { return rngCount_; }
    int64_t RngRemaining() const { return rngRemaining_; }
    uint64_t RngAnswered() const { return rngAnswered_; }
    uint64_t RngOutOfRange() const { return rngOutOfRange_; }

    // The lever answered its last call and turned itself off. Read once: the
    // adapter prints the line, then the flag is clear.
    bool TakeRngFinished()
    {
        const bool f = rngFinished_;
        rngFinished_ = false;
        return f;
    }

    // The lever is armed but no machine RNG call has reached it: every answer
    // so far was the game's own. `status` names this state, so a lever that
    // answered nothing is never read as a lever that changed nothing.
    bool Inert() const { return rngOn_ && rngAnswered_ == 0; }

    // `off`: disarmed and the lever off. Hooks cannot be removed while the
    // game runs, so the detours stay and count calls only.
    void Off()
    {
        armed_ = false;
        RngOff();
    }

    const Counters& RowCounters(int row) const
    {
        static const Counters kNone{};
        return row >= 0 && row < RowCount() ? rows_[static_cast<size_t>(row)] : kNone;
    }

    // ---- the status text ------------------------------------------------------
    // One row's counters, every one of them.
    std::string RowText(int row) const
    {
        const Counters& c = RowCounters(row);
        std::string s = "calls=" + std::to_string(c.calls) + " machine-self=" + std::to_string(c.machineSelf)
            + " in-event=" + std::to_string(c.inEvent) + " other-self=" + std::to_string(c.otherSelf)
            + " logged=" + std::to_string(c.logged) + "/" + std::to_string(kTraceLinesPerRow);
        if (row >= kFirstBuiltinRow && row < kFirstScriptRow
            && kBuiltins[row - kFirstBuiltinRow].kind != AnswerKind::NotRng)
            s += " answered=" + std::to_string(c.answered) + " out-of-range=" + std::to_string(c.outOfRange);
        return s;
    }

    // The machine's events, by their status keys: create=.. alarm0=.. ...
    std::string EventsText() const
    {
        std::string s;
        for (int i = 0; i < kEventCount; ++i)
            s += (i ? " " : "") + std::string(kEvents[i].key) + "=" + std::to_string(RowCounters(i).calls);
        return s;
    }

    // The status line: on/off, the machine's object index, the events and
    // every counter summed over every row.
    std::string StatusLine() const
    {
        Counters t;
        for (const Counters& c : rows_) {
            t.calls += c.calls;
            t.machineSelf += c.machineSelf;
            t.inEvent += c.inEvent;
            t.otherSelf += c.otherSelf;
            t.logged += c.logged;
            t.answered += c.answered;
            t.outOfRange += c.outOfRange;
        }
        return std::string("gambaprobe: ") + (armed_ ? "on" : "off") + " machine-object=" + std::to_string(machineObject_)
            + " " + EventsText() + " | calls=" + std::to_string(t.calls) + " machine-self=" + std::to_string(t.machineSelf)
            + " in-event=" + std::to_string(t.inEvent) + " other-self=" + std::to_string(t.otherSelf)
            + " logged=" + std::to_string(t.logged) + " answered=" + std::to_string(t.answered)
            + " out-of-range=" + std::to_string(t.outOfRange);
    }

    // The lever's line: what it answered, what it holds, and INERT when it is
    // armed and nothing reached it.
    std::string RngLine() const
    {
        std::string s = "gambaprobe: rng answered " + std::to_string(rngAnswered_) + " of " + std::to_string(rngCount_)
            + " value=" + NumberText(rngValue_) + " remaining=" + std::to_string(rngOn_ ? rngRemaining_ : 0)
            + " out-of-range=" + std::to_string(rngOutOfRange_) + " lever=" + (rngOn_ ? "on" : "off");
        if (Inert())
            s += " INERT - armed, but no RNG builtin call whose self is a gamba machine has reached it; every answer"
                 " so far was the game's own";
        return s;
    }

private:
    std::vector<Counters> rows_ = std::vector<Counters>(static_cast<size_t>(kFirstScriptRow));
    std::vector<std::unordered_map<uint64_t, uint64_t>> lastLine_ =
        std::vector<std::unordered_map<uint64_t, uint64_t>>(static_cast<size_t>(kFirstScriptRow));
    int machineObject_ = -1;
    bool armed_ = false;
    bool rngOn_ = false;
    bool rngFinished_ = false;
    double rngValue_ = 0.0;
    int64_t rngCount_ = 0;
    int64_t rngRemaining_ = 0;
    uint64_t rngAnswered_ = 0;
    uint64_t rngOutOfRange_ = 0;
};

} // namespace ForgePact::GambaProbe
