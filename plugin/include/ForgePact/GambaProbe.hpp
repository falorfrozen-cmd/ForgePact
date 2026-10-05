#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
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
// The last four are Live 1's removal candidates (replan 1): a machine
// created by `spawn` was cleaned up in the next step with no instance_destroy
// call from GML, so the runner paths that end or swap an instance are rows too.
inline constexpr int kBuiltinCount = 15;
inline constexpr BuiltinRow kBuiltins[kBuiltinCount] = {
    { "irandom",               AnswerKind::Value         },
    { "irandom_range",         AnswerKind::Value         },
    { "random",                AnswerKind::Value         },
    { "random_range",          AnswerKind::Value         },
    { "choose",                AnswerKind::ArgumentIndex },
    { "instance_destroy",      AnswerKind::NotRng        },
    { "instance_create_depth", AnswerKind::NotRng        },
    { "instance_create_layer", AnswerKind::NotRng        },
    { "instance_change",            AnswerKind::NotRng   },
    { "layer_destroy_instances",    AnswerKind::NotRng   },
    { "instance_deactivate_object", AnswerKind::NotRng   },
    { "room_goto",                  AnswerKind::NotRng   },
    // The extension functions Alarm_9 and sCP call by name (builtin
    // convention) to read and write the machine's protected store.
    { "GetVariable",              AnswerKind::NotRng      },
    { "SetVariable",              AnswerKind::NotRng      },
    { "SetVariableToUndefined",   AnswerKind::NotRng      },
};
enum class Builtin : int {
    Irandom, IrandomRange, Random, RandomRange, Choose, InstanceDestroy, InstanceCreateDepth, InstanceCreateLayer,
    InstanceChange, LayerDestroyInstances, InstanceDeactivateObject, RoomGoto,
    GetVariable, SetVariable, SetVariableToUndefined,
};

// The rows whose first argument can name what they act on: a call another
// self makes still concerns a machine when that argument names one
// (`machine-arg=`, ArgNamesMachine). instance_change's argument is the object
// its self becomes, so there it counts a call that turns something into a
// machine.
inline constexpr bool BuiltinChecksArgument(Builtin b)
{
    return b == Builtin::InstanceDestroy || b == Builtin::InstanceChange || b == Builtin::InstanceDeactivateObject;
}

inline constexpr int kFirstBuiltinRow = kEventCount;
inline constexpr int kFirstScriptRow = kEventCount + kBuiltinCount;
inline constexpr int EventRowOf(Event e) { return static_cast<int>(e); }
inline constexpr int BuiltinRowOf(Builtin b) { return kFirstBuiltinRow + static_cast<int>(b); }
inline constexpr int ScriptRowOf(int script) { return kFirstScriptRow + script; }

// How `hook` reached a row. Detoured: the probe's own inline detour (both
// routes for a script). DetouredUnder: another ForgePact hook holds the
// script's table entry table-only, so its saved original is still the game's
// function, and the probe detoured that function itself - the holder's body
// and compiled GML's direct calls both reach the probe. TableOnly: the
// script-table swap only, blind to compiled GML's direct calls. Shared:
// another install already detours the function and the probe observes
// through that detour. Missing: nothing of the probe sees the row.
enum class Route : int { Missing, Detoured, DetouredUnder, TableOnly, Shared };

inline constexpr std::string_view RouteName(Route r)
{
    switch (r) {
    case Route::Missing:       return "missing";
    case Route::Detoured:      return "detoured";
    case Route::DetouredUnder: return "detoured-under";
    case Route::TableOnly:     return "table-only";
    case Route::Shared:        return "shared";
    }
    return "?";
}

// ---- the by-name route (replan 1) ------------------------------------------------
// Live 1 counted no InitPV, SPV or FPV call while the machine's Create_0 and
// CleanUp_0 ran to their end, and the local reading says compiled GML calls
// those scripts by name through the runtime's functions array, not through
// the script's own function. So at the first `hook`, before any script row
// is installed, the adapter looks each row's short name and its
// `gml_Script_<short>` name up (GetNamedRoutineIndex, then
// GetNamedRoutinePointer) and this decides what the row's by-name route is:
//   same      no name reaches a routine other than the row's own function;
//   detoured  a name reaches another routine of the game, which the adapter
//             detours as a second attachment feeding the row's counters;
//   shared    that routine is reached from several rows: detoured once, as
//             its own `byname-shared` row;
//   missing   the `gml_Script_` name does not resolve, or a name's routine is
//             nothing the probe can detour (no pointer, not game code, or
//             its detour failed).
// The runner numbers scripts from kScriptIndexBase: an index below it names
// an entry of the functions array.
enum class ByName : int { NotRead, Same, Detoured, Shared, Missing };
inline constexpr int kScriptIndexBase = 100000;

inline constexpr std::string_view ByNameWord(ByName b)
{
    switch (b) {
    case ByName::NotRead:  return "not-read";
    case ByName::Same:     return "same";
    case ByName::Detoured: return "detoured";
    case ByName::Shared:   return "shared";
    case ByName::Missing:  return "missing";
    }
    return "?";
}

// One name's lookup, as the adapter read it. `routine` and
// `routineIsGameCode` matter only for a functions-array index.
struct NameLookup {
    bool      resolved = false;          // GetNamedRoutineIndex answered an index >= 0
    int       index = -1;
    uintptr_t routine = 0;               // a functions-array index: the routine GetNamedRoutinePointer returned
    bool      routineIsGameCode = false; // ... executable inside Hero_Siege.exe
};

inline bool NamesScript(const NameLookup& n) { return n.resolved && n.index >= kScriptIndexBase; }
inline bool NamesRoutine(const NameLookup& n) { return n.resolved && n.index >= 0 && n.index < kScriptIndexBase; }

// The routine a name reaches besides the row's own function, or 0.
inline uintptr_t OtherRoutine(const NameLookup& n, uintptr_t rowFunction)
{
    return NamesRoutine(n) && n.routine && n.routine != rowFunction ? n.routine : 0;
}

// The row's by-name route from its two lookups, before sharing is known.
inline ByName ClassifyByName(const NameLookup& shortName, const NameLookup& fullName, uintptr_t rowFunction)
{
    if (!fullName.resolved || fullName.index < 0) return ByName::Missing;
    bool other = false;
    for (const NameLookup* n : { &shortName, &fullName }) {
        if (NamesRoutine(*n) && !n->routine) return ByName::Missing;
        if (!OtherRoutine(*n, rowFunction)) continue;
        if (!n->routineIsGameCode) return ByName::Missing;
        other = true;
    }
    return other ? ByName::Detoured : ByName::Same;
}

// A detoured row whose routine other rows reach too is shared.
inline ByName WithSharing(ByName b, int rowsOnRoutine)
{
    return b == ByName::Detoured && rowsOnRoutine > 1 ? ByName::Shared : b;
}

inline std::string IndexText(const NameLookup& n) { return n.resolved ? std::to_string(n.index) : std::string("none"); }

// What a script row's hook and status lines end with:
// `idx=<short>/<gml_Script_> byname=<word>`.
inline std::string ByNameText(ByName b, const NameLookup& shortName, const NameLookup& fullName)
{
    return "idx=" + IndexText(shortName) + "/" + IndexText(fullName) + " byname=" + std::string(ByNameWord(b));
}

// ---- the trace budget and the lever's limits --------------------------------
// Each line carries a key - what it is about: a script's first argument
// (GPV's state key), a builtin's argument text, an event's instance id - and
// one key writes at most kTraceLinesPerKey lines a window, so one call shape a
// machine repeats every frame (an idle irandom, a timer key whose value moves)
// spends only its own lines; the lines a key was refused are counted as
// key-capped. The row's own cap, kTraceLinesPerRow, is there only to bound a
// row whose keys never repeat: it covers kTraceKeysPerRow full keys, so the
// protected store's moving keys (InitPV sets up 28) cannot spend GPV's or
// SPV's row during a spin's animation and leave the keys written at the
// spin's end undescribed. A row past it only counts, and its status line says
// BUDGET SPENT. A line identical to the previous line for the same key is not
// logged and costs nothing, but is counted (`repeats=`) and named until the
// key's next line carries it (TakeTraceLine). The budget is per window: `gambaprobe trace`,
// `hook` again and every `spawn` start it over, so the live procedure re-arms
// it right before each spin it measures.
inline constexpr int kTraceLinesPerKey = 8;
inline constexpr int kTraceKeysPerRow = 64;
inline constexpr int kTraceLinesPerRow = 512;
static_assert(kTraceLinesPerRow == kTraceLinesPerKey * kTraceKeysPerRow, "a row's budget covers kTraceKeysPerRow full keys");
// How many keys holding unlogged repeats one row's `status` names.
inline constexpr int kPendingRepeatKeysShown = 8;
inline constexpr int64_t kRngMinCount = 1;
inline constexpr int64_t kRngMaxCount = 50;

// ---- the caller walk (replan 1) ---------------------------------------------------
// Live 1 could not say who removes a spawned machine. At CleanUp_0 and
// Alarm_9 (and the window's first Create_0), before the original runs, the
// adapter prints the instance's state and its return-address stack, each
// frame named by FrameText. kCallerWalksPerRow walks per row per window
// (Create_0: kCreateWalksPerWindow); the count carries the rest
// (`walks-skipped=`).
inline constexpr int kCallerWalksPerRow = 4;
inline constexpr int kCreateWalksPerWindow = 1;
inline constexpr int kCallerWalkFrames = 24;

inline constexpr int CallerWalkBudget(Event e)
{
    return e == Event::Create ? kCreateWalksPerWindow
        : (e == Event::Alarm9 || e == Event::CleanUp) ? kCallerWalksPerRow : 0;
}

// One compiled-code table row the walk can name a frame by: its function and
// its `gml_` name. The adapter copies the rows whose function is game code,
// once per `hook`, and sorts them (SortCodeRows).
struct CodeRow {
    uintptr_t   function = 0;
    std::string name;
};

// A module's image, [base, end); `name` as a frame prints it.
struct CodeModule {
    uintptr_t   base = 0;
    uintptr_t   end = 0;
    std::string name;
    bool Contains(uintptr_t a) const { return base != 0 && a >= base && a < end; }
};

inline void SortCodeRows(std::vector<CodeRow>& rows)
{
    std::sort(rows.begin(), rows.end(), [](const CodeRow& a, const CodeRow& b) { return a.function < b.function; });
}

// The row with the greatest function address not above `frame`, or null.
inline const CodeRow* NearestRow(const std::vector<CodeRow>& sorted, uintptr_t frame)
{
    const auto it = std::upper_bound(sorted.begin(), sorted.end(), frame,
                                     [](uintptr_t a, const CodeRow& r) { return a < r.function; });
    return it == sorted.begin() ? nullptr : &*(it - 1);
}

inline std::string HexText(uintptr_t v)
{
    char b[24];
    std::snprintf(b, sizeof(b), "0x%llx", static_cast<unsigned long long>(v));
    return b;
}

// One frame of a walk. Inside the game's image the frame is named by the
// nearest row below it (`gml:<row>+0x<off>`) - unless `functionStart`, the
// start of the function holding the frame as the image's unwind table says
// (0 when it does not say), is another function, which makes it runner code
// above a row: `exe+0x<off>`, with the nearest row named after it. Inside
// this plugin `forgepact+0x<off>`, inside another module `<module>+0x<off>`,
// else `?`.
inline std::string FrameText(int k, uintptr_t frame, uintptr_t functionStart, const std::vector<CodeRow>& sorted,
                             const CodeModule& game, const CodeModule& plugin, const CodeModule& other)
{
    const std::string head = "  #" + std::to_string(k) + " ";
    if (game.Contains(frame)) {
        const CodeRow* row = NearestRow(sorted, frame);
        if (row && (functionStart == 0 || functionStart == row->function))
            return head + "gml:" + row->name + "+" + HexText(frame - row->function);
        std::string s = head + "exe+" + HexText(frame - game.base);
        if (row) s += " (runner code; nearest row below gml:" + row->name + "+" + HexText(frame - row->function) + ")";
        return s;
    }
    if (plugin.Contains(frame)) return head + "forgepact+" + HexText(frame - plugin.base);
    if (other.Contains(frame) && !other.name.empty()) return head + other.name + "+" + HexText(frame - other.base);
    return head + "?";
}

// ---- what an instance builtin's first argument names -----------------------------
// As the adapter read it: an instance (with its object_index), an object
// index (and whether that object is an ancestor of the machine's), or `all`.
enum class ArgKind : int { None, Instance, Object, All };
struct ArgTarget {
    ArgKind kind = ArgKind::None;
    int     object = -1;
    bool    machineAncestor = false;
};

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
    uint64_t logged = 0;        // lines written this window (at most kTraceLinesPerRow)
    uint64_t keyCapped = 0;     // new lines refused because their key had written kTraceLinesPerKey
    uint64_t repeats = 0;       // this window: lines identical to their key's last line, folded into it, not logged
    uint64_t answered = 0;      // RNG rows: calls the lever answered
    uint64_t outOfRange = 0;    // `choose`: a lever value that named none of the call's arguments
    uint64_t passed = 0;        // RNG rows: machine-self calls the armed lever let through (not its target)
    uint64_t machineArg = 0;    // BuiltinChecksArgument rows: another self's call whose first argument names a machine
    uint64_t walked = 0;        // walk rows: caller walks printed this window (at most CallerWalkBudget)
    uint64_t walkSkipped = 0;   // walk rows: walks the window's budget refused
};

// A builtin's row by its name (`irandom`, `choose`, ...); false for a name
// the probe does not hook.
inline bool BuiltinByName(std::string_view name, Builtin& out)
{
    for (int i = 0; i < kBuiltinCount; ++i)
        if (kBuiltins[i].name == name) {
            out = static_cast<Builtin>(i);
            return true;
        }
    return false;
}

// A call's argument text as the lever compares it: trimmed, every run of
// whitespace one space. The adapter's text (` a0=real:1.000000 a1=real:100.000000`) and what the
// operator types after `args` (copied from a trace line) meet here.
inline std::string ArgsKey(std::string_view text)
{
    std::string s;
    bool space = false;
    for (const char ch : text) {
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') { space = !s.empty(); continue; }
        if (space) s += ' ';
        space = false;
        s += ch;
    }
    return s;
}

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

    // `trace`, `hook` again or `spawn`: every row's trace budget, its keys'
    // budgets, its repeat memory and its caller walks start over; the counts
    // continue.
    void ResetTrace()
    {
        for (Counters& c : rows_) {
            c.logged = 0;
            c.repeats = 0;
            c.walked = 0;
        }
        for (auto& m : lastLine_) m.clear();
        takenRepeats_ = 0;
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

    // ---- the by-argument rule ----------------------------------------------
    // Does a call's first argument name a machine? An instance whose
    // object_index is the machine's, the machine's object or one of its
    // ancestors (instance_destroy(<object>) ends every instance of it and of
    // its children), or `all`. Nothing when the machine did not resolve.
    bool ArgNamesMachine(const ArgTarget& a) const
    {
        if (machineObject_ < 0) return false;
        switch (a.kind) {
        case ArgKind::Instance: return a.object == machineObject_;
        case ArgKind::Object:   return a.object == machineObject_ || a.machineAncestor;
        case ArgKind::All:      return true;
        case ArgKind::None:     break;
        }
        return false;
    }

    // A BuiltinChecksArgument row's call Observe did not find to be a
    // machine's own, whose first argument names one: counted as
    // `machine-arg=` and to be logged. An idle probe or a machine-self call
    // (already counted and logged as one) is not.
    bool NoteMachineArg(int row, Seen seen, bool argNamesMachine)
    {
        if (row < 0 || row >= RowCount() || seen == Seen::Idle || seen == Seen::Machine || !argNamesMachine) return false;
        ++rows_[static_cast<size_t>(row)].machineArg;
        return true;
    }

    // ---- the caller walk ---------------------------------------------------
    // May this machine event print its caller walk? Within the window's
    // CallerWalkBudget, else the refusal is counted.
    bool TakeCallerWalk(Event e)
    {
        const int row = EventRowOf(e);
        if (row < 0 || row >= RowCount()) return false;
        const int budget = CallerWalkBudget(e);
        if (budget <= 0) return false;
        Counters& c = rows_[static_cast<size_t>(row)];
        if (c.walked >= static_cast<uint64_t>(budget)) {
            ++c.walkSkipped;
            return false;
        }
        ++c.walked;
        return true;
    }

    // One RNG builtin call: Observe, then the lever. Answered only for a
    // machine self, only while the lever is on and has answers left, only
    // for the lever's target builtin and, when the lever names argument
    // text, only for a call whose `argsText()` matches it, and for `choose`
    // only with a value that names one of this call's `argc` arguments. Any
    // other machine-self RNG call the armed lever sees passes through
    // untouched and is counted as passed. `argsText()` is called only for a
    // machine-self call of the target with an argument filter set. The
    // count-th answer turns the lever off.
    template <class SelfFn, class ArgsFn>
    RngDecision DecideRng(Builtin builtin, SelfFn&& selfObject, bool inMachineEvent, int argc, ArgsFn&& argsText)
    {
        RngDecision d;
        const int b = static_cast<int>(builtin);
        if (b < 0 || b >= kBuiltinCount) return d;
        const int row = kFirstBuiltinRow + b;
        d.seen = Observe(row, selfObject, inMachineEvent);
        const AnswerKind kind = kBuiltins[b].kind;
        if (d.seen != Seen::Machine || kind == AnswerKind::NotRng || !rngOn_ || rngRemaining_ <= 0) return d;
        Counters& c = rows_[static_cast<size_t>(row)];
        // Aimed: another builtin's call, or the target's with other
        // arguments, is not the call the lever was armed for.
        if (builtin != rngTarget_) {
            ++c.passed;
            ++rngPassedBuiltin_;
            return d;
        }
        if (!rngArgs_.empty()) {
            std::string text;
            try { text = ArgsKey(argsText()); } catch (...) { text.clear(); }
            if (text != rngArgs_) {
                ++c.passed;
                ++rngPassedArgs_;
                return d;
            }
        }
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
    // (a script's first argument, a builtin's argument text, an event's
    // instance id); `text` is the line's content without its call number or
    // frame; `keyText` is the key as the operator reads it (a builtin's
    // argument text), kept for `status`. A repeat of the key's last line is
    // refused and spends nothing, but it is never silent: the row counts it
    // as `repeats=`, the key holds it until its next line, which carries it
    // (TakenRepeats), and until then `status` names the key and how many
    // calls it folded. Without that, a later call with the earlier call's
    // arguments and result - the prize roll after a same-shape reel roll -
    // would vanish and the earlier call read as the decider. A new line
    // spends one of the row's kTraceLinesPerRow and one of the key's
    // kTraceLinesPerKey, and a key that has none left is refused and counted
    // key-capped.
    bool TakeTraceLine(int row, uint64_t key, uint64_t text, std::string_view keyText = {})
    {
        takenRepeats_ = 0;
        if (row < 0 || row >= RowCount()) return false;
        Counters& c = rows_[static_cast<size_t>(row)];
        auto& last = lastLine_[static_cast<size_t>(row)];
        const auto it = last.find(key);
        if (it != last.end() && it->second.text == text) {
            ++c.repeats;
            ++it->second.repeats;
            return false;
        }
        if (c.logged >= static_cast<uint64_t>(kTraceLinesPerRow)) return false;
        if (it != last.end() && it->second.lines >= kTraceLinesPerKey) {
            ++c.keyCapped;
            return false;
        }
        KeyLine& k = last[key];
        if (k.lines == 0) k.keyText = ArgsKey(keyText);
        takenRepeats_ = k.repeats;
        k.repeats = 0;
        k.text = text;
        ++k.lines;
        ++c.logged;
        return true;
    }

    // The repeats of its key's previous line that the line TakeTraceLine just
    // took folded in: the adapter prints them on that line. Zero after any
    // refused line.
    uint64_t TakenRepeats() const { return takenRepeats_; }

    // The keys of a row holding repeats no later line has carried yet, as
    // `+<n> "<key>"`, so a call that only repeated an earlier line is named
    // at `status` even when its key never logged again this window.
    std::string PendingRepeatsText(int row) const
    {
        if (row < 0 || row >= RowCount()) return {};
        std::vector<std::pair<std::string, uint64_t>> held;
        for (const auto& [key, k] : lastLine_[static_cast<size_t>(row)])
            if (k.repeats > 0) held.emplace_back(k.keyText, k.repeats);
        std::sort(held.begin(), held.end());
        std::string s;
        for (size_t i = 0; i < held.size() && i < static_cast<size_t>(kPendingRepeatKeysShown); ++i)
            s += (i ? ", +" : "+") + std::to_string(held[i].second) + " \"" + held[i].first + "\"";
        if (held.size() > static_cast<size_t>(kPendingRepeatKeysShown))
            s += ", and " + std::to_string(held.size() - static_cast<size_t>(kPendingRepeatKeysShown)) + " more key(s)";
        return s;
    }

    // The row has written its kTraceLinesPerRow lines this window: it only
    // counts until the budget starts over.
    bool BudgetSpent(int row) const
    {
        return RowCounters(row).logged >= static_cast<uint64_t>(kTraceLinesPerRow);
    }

    // ---- the lever -----------------------------------------------------------
    // `rng <builtin> <value> [count] [args <text>]`: answer the next `count`
    // machine-self calls of one RNG builtin - with `args`, only those whose
    // argument text is `args` (ArgsKey on both sides) - with `value`. Aimed,
    // because the machine makes RNG calls the prize does not depend on (a
    // reel roll each spin, possibly an idle call every frame), and an unaimed
    // lever hands its answer to the first of them. False, and nothing
    // changed, for a builtin that is not an RNG row, a value that is not
    // finite or a count out of range.
    bool SetRng(Builtin target, double value, int64_t count, std::string_view args = {})
    {
        const int b = static_cast<int>(target);
        if (b < 0 || b >= kBuiltinCount || kBuiltins[b].kind == AnswerKind::NotRng) return false;
        if (!std::isfinite(value) || count < kRngMinCount || count > kRngMaxCount) return false;
        rngOn_ = true;
        rngFinished_ = false;
        rngTarget_ = target;
        rngArgs_ = ArgsKey(args);
        rngValue_ = value;
        rngCount_ = count;
        rngRemaining_ = count;
        rngAnswered_ = 0;
        rngOutOfRange_ = 0;
        rngPassedBuiltin_ = 0;
        rngPassedArgs_ = 0;
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
    Builtin RngTarget() const { return rngTarget_; }
    const std::string& RngArgs() const { return rngArgs_; }
    // Machine-self RNG calls the armed lever let through: another builtin's,
    // and the target's with other argument text.
    uint64_t RngPassedBuiltin() const { return rngPassedBuiltin_; }
    uint64_t RngPassedArgs() const { return rngPassedArgs_; }

    // The lever answered its last call and turned itself off. Read once: the
    // adapter prints the line, then the flag is clear.
    bool TakeRngFinished()
    {
        const bool f = rngFinished_;
        rngFinished_ = false;
        return f;
    }

    // The lever is armed but no machine call of its target has reached it:
    // every answer so far was the game's own. `status` names this state, and
    // how many other machine-self RNG calls passed by, so a lever that
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
    // One row's counters, every one of them, and BUDGET SPENT once the row
    // only counts.
    std::string RowText(int row) const
    {
        const Counters& c = RowCounters(row);
        std::string s = "calls=" + std::to_string(c.calls) + " machine-self=" + std::to_string(c.machineSelf)
            + " in-event=" + std::to_string(c.inEvent) + " other-self=" + std::to_string(c.otherSelf)
            + " logged=" + std::to_string(c.logged) + "/" + std::to_string(kTraceLinesPerRow)
            + " key-capped=" + std::to_string(c.keyCapped) + " repeats=" + std::to_string(c.repeats);
        if (row >= kFirstBuiltinRow && row < kFirstScriptRow
            && kBuiltins[row - kFirstBuiltinRow].kind != AnswerKind::NotRng)
            s += " answered=" + std::to_string(c.answered) + " out-of-range=" + std::to_string(c.outOfRange)
                + " passed=" + std::to_string(c.passed);
        if (row >= kFirstBuiltinRow && row < kFirstScriptRow
            && BuiltinChecksArgument(static_cast<Builtin>(row - kFirstBuiltinRow)))
            s += " machine-arg=" + std::to_string(c.machineArg);
        if (row >= 0 && row < kEventCount && CallerWalkBudget(static_cast<Event>(row)) > 0)
            s += " caller-walks=" + std::to_string(c.walked) + "/" + std::to_string(CallerWalkBudget(static_cast<Event>(row)))
                + " walks-skipped=" + std::to_string(c.walkSkipped);
        const std::string held = PendingRepeatsText(row);
        if (!held.empty()) s += " unlogged-repeats: " + held;
        if (BudgetSpent(row))
            s += " BUDGET SPENT - counted, not described; `gambaprobe trace` starts it over";
        return s;
    }

    // How many rows have spent their budget this window.
    int SpentRows() const
    {
        int n = 0;
        for (int row = 0; row < RowCount(); ++row) if (BudgetSpent(row)) ++n;
        return n;
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
            t.keyCapped += c.keyCapped;
            t.repeats += c.repeats;
            t.answered += c.answered;
            t.outOfRange += c.outOfRange;
            t.passed += c.passed;
            t.machineArg += c.machineArg;
            t.walked += c.walked;
            t.walkSkipped += c.walkSkipped;
        }
        return std::string("gambaprobe: ") + (armed_ ? "on" : "off") + " machine-object=" + std::to_string(machineObject_)
            + " " + EventsText() + " | calls=" + std::to_string(t.calls) + " machine-self=" + std::to_string(t.machineSelf)
            + " in-event=" + std::to_string(t.inEvent) + " other-self=" + std::to_string(t.otherSelf)
            + " logged=" + std::to_string(t.logged) + " key-capped=" + std::to_string(t.keyCapped)
            + " repeats=" + std::to_string(t.repeats) + " spent-rows=" + std::to_string(SpentRows()) + " answered=" + std::to_string(t.answered)
            + " out-of-range=" + std::to_string(t.outOfRange) + " passed=" + std::to_string(t.passed)
            + " machine-arg=" + std::to_string(t.machineArg) + " caller-walks=" + std::to_string(t.walked)
            + " walks-skipped=" + std::to_string(t.walkSkipped);
    }

    // What the lever is aimed at: `irandom` or `irandom args="a0=real:100.000000"`
    // (the argument text as a trace line prints it, kind prefix and all).
    std::string RngTargetText() const
    {
        std::string s(kBuiltins[static_cast<int>(rngTarget_)].name);
        if (!rngArgs_.empty()) s += " args=\"" + rngArgs_ + "\"";
        return s;
    }

    // The lever's line: what it is aimed at, what it answered, what it let
    // through, and INERT when it is armed and its target never reached it.
    std::string RngLine() const
    {
        std::string s = "gambaprobe: rng answered " + std::to_string(rngAnswered_) + " of " + std::to_string(rngCount_)
            + " target=" + RngTargetText() + " value=" + NumberText(rngValue_)
            + " remaining=" + std::to_string(rngOn_ ? rngRemaining_ : 0) + " out-of-range=" + std::to_string(rngOutOfRange_)
            + " passed=" + std::to_string(rngPassedBuiltin_ + rngPassedArgs_) + " (other builtin "
            + std::to_string(rngPassedBuiltin_) + ", other args " + std::to_string(rngPassedArgs_) + ") lever="
            + (rngOn_ ? "on" : "off");
        if (Inert())
            s += " INERT - armed, but no machine-self " + RngTargetText() + " call has reached it; every answer so far"
                 " was the game's own, and " + std::to_string(rngPassedBuiltin_ + rngPassedArgs_)
                 + " other machine-self RNG call(s) passed through untouched";
        return s;
    }

private:
    // One key's memory this window: its last line, the lines it wrote, the
    // repeats of its last line no later line has carried yet, and the key as
    // the operator reads it.
    struct KeyLine {
        uint64_t    text = 0;
        int         lines = 0;
        uint64_t    repeats = 0;
        std::string keyText;
    };
    std::vector<Counters> rows_ = std::vector<Counters>(static_cast<size_t>(kFirstScriptRow));
    std::vector<std::unordered_map<uint64_t, KeyLine>> lastLine_ =
        std::vector<std::unordered_map<uint64_t, KeyLine>>(static_cast<size_t>(kFirstScriptRow));
    uint64_t takenRepeats_ = 0;
    int machineObject_ = -1;
    bool armed_ = false;
    bool rngOn_ = false;
    bool rngFinished_ = false;
    Builtin rngTarget_ = Builtin::Irandom;
    std::string rngArgs_;   // empty: any argument text
    double rngValue_ = 0.0;
    int64_t rngCount_ = 0;
    int64_t rngRemaining_ = 0;
    uint64_t rngAnswered_ = 0;
    uint64_t rngOutOfRange_ = 0;
    uint64_t rngPassedBuiltin_ = 0;
    uint64_t rngPassedArgs_ = 0;
};

// ---- the explosion watch (phase 4) ------------------------------------------------
// The owner reports that a machine explodes after roughly 10-14 spins, can
// no longer be used afterwards, and that the explosion is the only time
// Goburin's Head drops. Nothing has observed the explosion yet, and the
// local reading could not place it, so the watch looks for it from two
// sides: each frame the adapter hands over every live machine's id and
// sprite name, and a machine whose sprite changes or that the refresh no
// longer finds opens a window. While the window is open, every build-row
// call and every instance create/destroy call is a window line whatever its
// self, and each CreateItemNew is followed by what it built. The adapter
// also keeps the last kWatchRingSize build-row calls, for any self, because
// a build in the transition's own step runs before the end-of-frame poll
// that notices the transition (EVENT_FRAME is the end of the frame): a new
// window first replays the ring's last kWindowLookBackFrames frames.
//
// A window has its own line cap, kWindowLineCap, apart from the trace
// budget (kTraceLinesPerRow / kTraceLinesPerKey), and counts what it
// dropped. Outside a window the trace behaves exactly as it did before.
inline constexpr int kWatchRingSize = 64;
inline constexpr int kWindowLookBackFrames = 2;
inline constexpr int kWindowSpanDefault = 300;
inline constexpr int kWindowSpanMax = 3600;
inline constexpr int kWindowLineCap = 400;

// The builtin rows a window logs for any self, besides the build rows.
inline constexpr bool BuiltinInWindow(Builtin b)
{
    return b == Builtin::InstanceCreateLayer || b == Builtin::InstanceCreateDepth || b == Builtin::InstanceDestroy;
}

// `gambaprobe window [frames]`: nothing or a non-positive count is the
// default span, more than kWindowSpanMax is capped there.
inline int WindowSpan(long long requested)
{
    if (requested <= 0) return kWindowSpanDefault;
    return requested > kWindowSpanMax ? kWindowSpanMax : static_cast<int>(requested);
}

enum class WindowReason : int { Sprite, Gone, Command };

inline constexpr std::string_view WindowReasonWord(WindowReason r)
{
    switch (r) {
    case WindowReason::Sprite:  return "sprite";
    case WindowReason::Gone:    return "gone";
    case WindowReason::Command: return "command";
    }
    return "?";
}

// One machine as the refresh found it: its id and its sprite's name (`?`
// when the value was not a sprite).
struct MachineSight {
    long long   id = -1;
    std::string sprite;
};

// One build-row call as the adapter formatted it: the row's label, the
// self's text, argc and the arguments as GpScriptArgs prints them (each with
// its leading space). `inWindow` is set by Push.
struct RingCall {
    std::string row;
    std::string self;
    int         argc = 0;
    std::string args;
    int64_t     frame = 0;
    bool        inWindow = false;
};

// ---- the fixed lines ----------------------------------------------------------------
inline std::string FrameTail(int64_t frame) { return " frame=" + std::to_string(frame); }
inline std::string IdText(long long id) { return id < 0 ? std::string("-") : std::to_string(id); }

inline std::string MachineFirstLine(long long id, std::string_view sprite, int64_t frame)
{
    return "gambaprobe machine id=" + std::to_string(id) + " sprite=" + std::string(sprite) + FrameTail(frame);
}
inline std::string MachineChangeLine(long long id, std::string_view from, std::string_view to, int64_t frame)
{
    return "gambaprobe machine id=" + std::to_string(id) + " sprite " + std::string(from) + " -> " + std::string(to) + FrameTail(frame);
}
inline std::string MachineGoneLine(long long id, int64_t frame)
{
    return "gambaprobe machine id=" + std::to_string(id) + " gone" + FrameTail(frame);
}
inline std::string WindowOpenLine(WindowReason r, long long id, int64_t frame, int replayed, int span)
{
    return "gambaprobe window open reason=" + std::string(WindowReasonWord(r)) + " id=" + IdText(id) + FrameTail(frame)
        + " replayed=" + std::to_string(replayed) + " span=" + std::to_string(span);
}
// A transition or a command while a window is open moves its end out.
inline std::string WindowExtendedLine(WindowReason r, long long id, int64_t end, int64_t frame)
{
    return "gambaprobe window extended reason=" + std::string(WindowReasonWord(r)) + " id=" + IdText(id) + " end="
        + std::to_string(end) + FrameTail(frame);
}
inline std::string WindowCallLine(std::string_view row, std::string_view self, int argc, std::string_view args, int64_t frame)
{
    return "gambaprobe window " + std::string(row) + " self=" + std::string(self) + " argc=" + std::to_string(argc)
        + std::string(args) + FrameTail(frame);
}
inline std::string WindowReplayLine(std::string_view row, std::string_view self, int argc, std::string_view args, int64_t frame)
{
    return "gambaprobe window replay " + std::string(row) + " self=" + std::string(self) + " argc=" + std::to_string(argc)
        + std::string(args) + FrameTail(frame);
}
// What a CreateItemNew built; each value as the adapter read it, `?` when
// it could not (OptionalNumberText).
inline std::string WindowBuiltLine(std::string_view itemType, std::string_view j, std::string_view b, std::string_view c,
                                   std::string_view rarity, std::string_view name, std::string_view self, int64_t frame)
{
    return "gambaprobe window built itemType=" + std::string(itemType) + " j=" + std::string(j) + " b=" + std::string(b)
        + " c=" + std::string(c) + " rarity=" + std::string(rarity) + " name=" + std::string(name) + " self=" + std::string(self)
        + FrameTail(frame);
}
inline std::string WindowClosedLine(uint64_t lines, uint64_t dropped, int64_t frame)
{
    return "gambaprobe window closed lines=" + std::to_string(lines) + " dropped=" + std::to_string(dropped) + FrameTail(frame);
}
inline std::string OptionalNumberText(bool read, double v) { return read ? NumberText(v) : std::string("?"); }

class Watch {
public:
    // The per-frame refresh's machines. A machine seen for the first time
    // prints its first-sight line; one whose sprite differs from the last
    // name it showed prints a change line and opens (or extends) a window;
    // one the record holds that `seen` lacks prints a gone line, opens a
    // window and is forgotten. A gone line can also mean the instance was
    // deactivated or left the room: the operator judges.
    std::vector<std::string> Poll(int64_t frame, const std::vector<MachineSight>& seen)
    {
        std::vector<std::string> out;
        for (const MachineSight& m : seen) {
            auto it = std::find_if(machines_.begin(), machines_.end(), [&m](const MachineSight& k) { return k.id == m.id; });
            if (it == machines_.end()) {
                machines_.push_back(m);
                ++machinesSeen_;
                out.push_back(MachineFirstLine(m.id, m.sprite, frame));
                continue;
            }
            if (it->sprite == m.sprite) continue;
            const std::string from = it->sprite;
            it->sprite = m.sprite;
            ++transitions_;
            out.push_back(MachineChangeLine(m.id, from, m.sprite, frame));
            for (std::string& line : Open(WindowReason::Sprite, m.id, frame, kWindowSpanDefault)) out.push_back(std::move(line));
        }
        for (size_t i = 0; i < machines_.size();) {
            const long long id = machines_[i].id;
            const bool found = std::any_of(seen.begin(), seen.end(), [id](const MachineSight& m) { return m.id == id; });
            if (found) { ++i; continue; }
            machines_.erase(machines_.begin() + static_cast<std::ptrdiff_t>(i));
            ++transitions_;
            out.push_back(MachineGoneLine(id, frame));
            for (std::string& line : Open(WindowReason::Gone, id, frame, kWindowSpanDefault)) out.push_back(std::move(line));
        }
        return out;
    }

    // Opens a window of `span` frames at `frame`: its open line, then the
    // ring's calls from the last kWindowLookBackFrames frames that no window
    // counted yet, oldest first, within the cap. While one is open, its end
    // moves out to frame + span instead and only the extended line prints.
    std::vector<std::string> Open(WindowReason reason, long long id, int64_t frame, int span)
    {
        std::vector<std::string> out;
        if (span <= 0) span = kWindowSpanDefault;
        if (span > kWindowSpanMax) span = kWindowSpanMax;
        if (open_) {
            if (frame + span > end_) end_ = frame + span;   // never std::max: windows.h's macro breaks it in the plugin
            out.push_back(WindowExtendedLine(reason, id, end_, frame));
            return out;
        }
        open_ = true;
        end_ = frame + span;
        lines_ = 0;
        dropped_ = 0;
        ++windows_;
        std::vector<std::string> replay;
        for (RingCall& c : ring_) {
            if (c.inWindow || c.frame < frame - kWindowLookBackFrames || c.frame > frame) continue;
            c.inWindow = true;
            if (!TakeLine()) continue;
            replay.push_back(WindowReplayLine(c.row, c.self, c.argc, c.args, c.frame));
        }
        out.push_back(WindowOpenLine(reason, id, frame, static_cast<int>(replay.size()), span));
        for (std::string& line : replay) out.push_back(std::move(line));
        return out;
    }

    // One build-row call, any self: kept in the ring (the oldest goes past
    // kWatchRingSize), marked when an open window already covers it.
    void Push(RingCall call)
    {
        call.inWindow = InWindow(call.frame);
        ring_.push_back(std::move(call));
        if (ring_.size() > static_cast<size_t>(kWatchRingSize)) ring_.erase(ring_.begin());
    }

    // Is a call at `frame` inside the open window? Its frames are
    // [open, open + span).
    bool InWindow(int64_t frame) const { return open_ && frame < end_; }

    // May a window line for a call at `frame` print? Only inside the window
    // and within its cap; past the cap it is counted as dropped.
    bool TakeWindowLine(int64_t frame)
    {
        if (!InWindow(frame)) return false;
        return TakeLine();
    }

    // The end-of-frame tick: a window whose span has run closes, once, with
    // its line; otherwise nothing.
    std::string Tick(int64_t frame)
    {
        if (!open_ || frame < end_) return {};
        return Close(frame);
    }

    // `gambaprobe off`: an open window closes with its line and the machine
    // record is forgotten; the counts stay.
    std::string Off(int64_t frame)
    {
        machines_.clear();
        return open_ ? Close(frame) : std::string();
    }

    bool WindowOpen() const { return open_; }
    int64_t WindowEnd() const { return end_; }
    uint64_t WindowLines() const { return lines_; }
    uint64_t WindowDropped() const { return dropped_; }
    uint64_t MachinesSeen() const { return machinesSeen_; }
    uint64_t Transitions() const { return transitions_; }
    uint64_t Windows() const { return windows_; }
    int RingSize() const { return static_cast<int>(ring_.size()); }

    // `gambaprobe status`'s watch line.
    std::string StatusLine() const
    {
        return "gambaprobe watch: machines-seen=" + std::to_string(machinesSeen_) + " transitions=" + std::to_string(transitions_)
            + " windows=" + std::to_string(windows_) + " window=" + (open_ ? "open" : "closed") + " ring=" + std::to_string(ring_.size());
    }

private:
    bool TakeLine()
    {
        if (lines_ >= static_cast<uint64_t>(kWindowLineCap)) {
            ++dropped_;
            return false;
        }
        ++lines_;
        return true;
    }

    std::string Close(int64_t frame)
    {
        open_ = false;
        return WindowClosedLine(lines_, dropped_, frame);
    }

    std::vector<MachineSight> machines_;
    std::vector<RingCall> ring_;
    bool open_ = false;
    int64_t end_ = 0;
    uint64_t lines_ = 0;
    uint64_t dropped_ = 0;
    uint64_t machinesSeen_ = 0;
    uint64_t transitions_ = 0;
    uint64_t windows_ = 0;
};

} // namespace ForgePact::GambaProbe
