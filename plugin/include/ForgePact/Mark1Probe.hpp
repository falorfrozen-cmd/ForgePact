#pragma once

#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ForgePact::Mark1Probe {

// ---- mark1probe's core (ForgePact #166 phase 1, research build only) -------
//
// The Mark 1 set's full bonus casts the bar's Q, E, R and Y skills whenever the
// game would allow it, removes the movement penalty of those casts, halves the
// final movement speed and triples the jump. Five mechanisms behind that are
// not established on this build, and docs/mark1-set-research.md asks them of
// one research instrument: `mark1probe` wraps nine game scripts (the rows
// below) as pass-throughs and holds at most one lever on them at a time.
//
// This header is every decision and every line of that instrument, and
// nothing that touches the game: the parsers, the lever state and its frame
// budget, the per-call wrapper (`Probe::Around`), the displacement and scale
// arithmetic, the watch diff, the walkable verdict line and the scan points.
// It is game-independent by contract - numbers, strings and plain structs,
// never an RValue or an instance - so tests/mark1_probe_harness.cpp compiles it
// whole. The adapter in ModuleMain.cpp (symbols `M1p*`) supplies the frame
// number, whether a call's self is the local player, the trampoline, the
// member reads and writes, and the printing.
//
// With every lever off, each row's call runs the original once and its result
// is returned unchanged, with nothing read or written: the baseline.

// ---- the rows ----------------------------------------------------------------

enum class Row : int {
    CheckTalentUse, TalentUse, TalentUseSetSpeed, PlayerMovement, ReturnSpecificStat,
    StatMovementSpeed, SkillsLeap, PlayerJumpGravity, StatJumpPower,
};
inline constexpr int kRowCount = 9;

// The scripts' own names, the spelling the game and the SDK use.
inline constexpr std::string_view kRowNames[kRowCount] = {
    "CheckTalentUse", "TalentUse", "TalentUseSetSpeed", "PlayerMovement", "ReturnSpecificStat",
    "StatMovementSpeed", "skillsLeap", "playerJumpGravity", "StatJumpPower",
};

inline std::string_view RowName(Row r)
{
    const int i = static_cast<int>(r);
    return i >= 0 && i < kRowCount ? kRowNames[i] : std::string_view("?");
}

inline char Lower(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

inline std::string LowerText(std::string_view s)
{
    std::string out(s);
    for (char& c : out) c = Lower(c);
    return out;
}

inline bool SameWord(std::string_view a, std::string_view b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (Lower(a[i]) != Lower(b[i])) return false;
    return true;
}

// A row by its name, in any letter case.
inline bool ParseRow(std::string_view s, Row& out)
{
    for (int i = 0; i < kRowCount; ++i) {
        if (SameWord(s, kRowNames[i])) { out = static_cast<Row>(i); return true; }
    }
    return false;
}

// ---- scopes and targets -------------------------------------------------------
// player: the local player instance's own variables. pst: its `pSt` struct.
// profile: its `playerProfileSlot` (the adapter falls back to
// Profile_Manager_obj instance 0 when the player has none, and says which).
// input: that scope's `inputState` elements. queue: `abilityQueue`,
// `mobileTalentPressed` and `stackSkillList`, on the instance the static
// reading places each on.
enum class Scope : int { Player, Pst, Profile, Input, Queue };
inline constexpr int kScopeCount = 5;
inline constexpr std::string_view kScopeNames[kScopeCount] = { "player", "pst", "profile", "input", "queue" };

inline std::string_view ScopeName(Scope s)
{
    const int i = static_cast<int>(s);
    return i >= 0 && i < kScopeCount ? kScopeNames[i] : std::string_view("?");
}

inline bool ParseScope(std::string_view s, Scope& out)
{
    for (int i = 0; i < kScopeCount; ++i) {
        if (SameWord(s, kScopeNames[i])) { out = static_cast<Scope>(i); return true; }
    }
    return false;
}

// What a `set` or `scale` lever writes: `<scope>.<name>` or
// `<scope>.<name>[<i>]`, and `input[<i>]` for an inputState element.
struct Target {
    Scope       scope = Scope::Player;
    std::string name;
    bool        indexed = false;
    long long   index = 0;
};

inline std::string TargetText(const Target& t)
{
    if (t.scope == Scope::Input) return "input[" + std::to_string(t.index) + "]";
    std::string out = std::string(ScopeName(t.scope)) + "." + t.name;
    if (t.indexed) out += "[" + std::to_string(t.index) + "]";
    return out;
}

// ---- numbers and values ---------------------------------------------------------

inline std::string NumberText(double v)
{
    if (!std::isfinite(v)) return "?";
    char b[48];
    if (v == std::floor(v) && std::fabs(v) < 1e15) std::snprintf(b, sizeof b, "%lld", static_cast<long long>(v));
    else std::snprintf(b, sizeof b, "%.6g", v);
    return b;
}

// A whole number in [lo, hi], written in plain decimal digits (no sign, no
// fraction, no exponent).
inline bool ParseWhole(std::string_view s, long long lo, long long hi, long long& out)
{
    if (s.empty() || s.size() > 16) return false;
    long long v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
        v = v * 10 + (c - '0');
    }
    if (v < lo || v > hi) return false;
    out = v;
    return true;
}

// A plain decimal number: an optional sign, digits, an optional fraction. No
// exponent, no `inf` or `nan`, so a typo cannot become a huge or undefined
// value written into the game.
inline bool ParseNumber(std::string_view s, double& out)
{
    if (s.empty() || s.size() > 24) return false;
    size_t i = 0;
    bool negative = false;
    if (s[0] == '-' || s[0] == '+') { negative = s[0] == '-'; i = 1; }
    bool digits = false, dot = false;
    for (size_t k = i; k < s.size(); ++k) {
        const char c = s[k];
        if (c >= '0' && c <= '9') digits = true;
        else if (c == '.' && !dot) dot = true;
        else return false;
    }
    if (!digits) return false;
    const std::string_view body = s.substr(i);
    double v = 0;
    const auto r = std::from_chars(body.data(), body.data() + body.size(), v, std::chars_format::fixed);
    if (r.ec != std::errc() || r.ptr != body.data() + body.size() || !std::isfinite(v)) return false;
    out = negative ? -v : v;
    return true;
}

// A member's value as the core sees it: a number, or a bool (number 0 or 1).
// The adapter reads and writes only these two kinds; anything else is a
// member the lever leaves alone.
struct Value {
    bool   isBool = false;
    double number = 0;
};

inline std::string ValueText(const Value& v)
{
    if (v.isBool) return v.number != 0 ? "true" : "false";
    return NumberText(v.number);
}

inline bool ParseValue(std::string_view s, Value& out)
{
    if (SameWord(s, "true")) { out = Value{ true, 1 }; return true; }
    if (SameWord(s, "false")) { out = Value{ true, 0 }; return true; }
    double n = 0;
    if (!ParseNumber(s, n)) return false;
    out = Value{ false, n };
    return true;
}

// ---- the levers -------------------------------------------------------------------

inline constexpr int64_t kDefaultFrames = 600;    // ten seconds at 60 fps
inline constexpr int64_t kMaxFrames = 3600;       // one minute: every lever and watch ends on its own
inline constexpr double kMaxMult = 100;
inline constexpr long long kMaxIndex = 9999;
inline constexpr long long kMaxStatId = 999;
inline constexpr int kMaxArgs = 100;
inline constexpr int kWatchMaxLines = 400;        // one watch's printed lines; the rest are counted
inline constexpr size_t kMaxNameLength = 64;
inline constexpr size_t kMaxOutbox = 2000;        // `pos` lines waiting for the adapter to print

// set: write `value` into `target` before each local-player call of `row`,
//      and restore what was there after the original returns (or throws).
// scale: the same, multiplying the member's number by `mult`.
// move: after each local-player call of `row` that moved the player, set x
//       and y to before + mult x (after - before).
// stat: a ReturnSpecificStat call for the local player whose stat-id argument
//       is `statId` returns the original's number x `mult`; for an id with
//       its own getter row (StatGetterRow), a local-player call of that
//       getter does too. Compiled GML reaches a getter like StatMovementSpeed
//       by a direct call as well as through the dispatcher, so scaling the
//       dispatcher alone would be blind to those callers.
enum class LeverKind : int { Off, Set, Scale, Move, Stat };

// The getter row the stat dispatcher sends an id to, for the ids that have
// one among the rows (static reading: 64 movement speed, 66 jump power).
inline bool StatGetterRow(long long statId, Row& out)
{
    if (statId == 64) { out = Row::StatMovementSpeed; return true; }
    if (statId == 66) { out = Row::StatJumpPower; return true; }
    return false;
}

struct Lever {
    LeverKind kind = LeverKind::Off;
    Target    target;
    Value     value;
    double    mult = 1;
    Row       row = Row::CheckTalentUse;
    long long statId = 0;
    int64_t   frames = kDefaultFrames;
};

// One word, no spaces, so `lever=<form>` stays one token on the status line.
inline std::string LeverText(const Lever& l)
{
    const std::string frames = "frames=" + std::to_string(l.frames);
    switch (l.kind) {
    case LeverKind::Off:   return "off";
    case LeverKind::Set:   return "set(" + TargetText(l.target) + "=" + ValueText(l.value) + ",during=" + std::string(RowName(l.row)) + "," + frames + ")";
    case LeverKind::Scale: return "scale(" + TargetText(l.target) + "*" + NumberText(l.mult) + ",during=" + std::string(RowName(l.row)) + "," + frames + ")";
    case LeverKind::Move:  return "move(" + std::string(RowName(l.row)) + "*" + NumberText(l.mult) + "," + frames + ")";
    case LeverKind::Stat:  return "stat(" + std::to_string(l.statId) + "*" + NumberText(l.mult) + "," + frames + ")";
    }
    return "?";
}

// The reply to a lever form. `resolved` names the instance or struct the
// adapter resolved the target's scope to, `current` the value it read there
// now (what a restore writes back); both are only shown for set and scale.
// `replaced` is the lever this one replaced, empty when none was on.
inline std::string LeverReply(const Lever& l, const std::string& resolved, const std::string& current, const std::string& replaced)
{
    const std::string during = " during " + std::string(RowName(l.row)) + " for " + std::to_string(l.frames) + " frames";
    const std::string where = " scope=" + (resolved.empty() ? std::string("?") : resolved)
        + " restores=" + (current.empty() ? std::string("?") : current);
    std::string out;
    switch (l.kind) {
    case LeverKind::Off:   out = "mark1probe: lever off"; break;
    case LeverKind::Set:   out = "mark1probe set: " + TargetText(l.target) + " := " + ValueText(l.value) + during + where; break;
    case LeverKind::Scale: out = "mark1probe scale: " + TargetText(l.target) + " x" + NumberText(l.mult) + during + where; break;
    case LeverKind::Move:
        out = "mark1probe move: " + std::string(RowName(l.row)) + " displacement x" + NumberText(l.mult) + " for "
            + std::to_string(l.frames) + " frames";
        break;
    case LeverKind::Stat: {
        Row getter = Row::ReturnSpecificStat;
        const std::string routes = StatGetterRow(l.statId, getter)
            ? " on ReturnSpecificStat and " + std::string(RowName(getter))
            : std::string(" on ReturnSpecificStat (no getter row)");
        out = "mark1probe stat: id " + std::to_string(l.statId) + " x" + NumberText(l.mult) + routes + " for "
            + std::to_string(l.frames) + " frames";
        break;
    }
    }
    if (!replaced.empty()) out += " (replaces " + replaced + ")";
    return out;
}

// ---- the commands --------------------------------------------------------------------

enum class Form : int {
    Status, Hook, Off, Show, ShowReset, Args, Watch, Set, Scale, Move, Stat, Count, Pos, WalkHere, WalkAt, WalkScan,
};

struct Command {
    Form        form = Form::Status;
    Lever       lever;                     // Set, Scale, Move, Stat
    int64_t     frames = 0;                // Watch, Count, Pos
    int         count = 0;                 // Args: calls to print; WalkScan: points
    Scope       scope = Scope::Player;     // Watch
    bool        after = false;             // Watch: start at the row's next local-player call
    Row         afterRow = Row::CheckTalentUse;
    std::string object;                    // Count
    double      a = 0, b = 0;              // WalkHere dx dy, WalkAt x y, WalkScan deg step
};

inline const std::string& RowList()
{
    static const std::string list = [] {
        std::string s;
        for (int i = 0; i < kRowCount; ++i) {
            if (i) s += i == kRowCount - 1 ? " or " : ", ";
            s += kRowNames[i];
        }
        return s;
    }();
    return list;
}

inline bool IsNameChar(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

inline bool IsName(std::string_view s)
{
    if (s.empty() || s.size() > kMaxNameLength) return false;
    for (char c : s)
        if (!IsNameChar(c)) return false;
    return true;
}

namespace detail {

inline const char* kFormError = "a target is <scope>.<name>[<i>] or input[<i>], not ";

// `[<i>]` at the end of `s`, after `head`: the index, or false.
inline bool ParseIndexSuffix(std::string_view s, size_t open, long long& index)
{
    if (open == std::string_view::npos || s.size() < open + 3 || s.back() != ']') return false;
    return ParseWhole(s.substr(open + 1, s.size() - open - 2), 0, kMaxIndex, index);
}

inline bool ParseTarget(std::string_view s, Target& t, std::string& why)
{
    const size_t dot = s.find('.');
    if (dot == std::string_view::npos) {
        if (s.size() >= 6 && SameWord(s.substr(0, 6), "input[")) {
            long long index = 0;
            if (!ParseIndexSuffix(s, 5, index)) { why = "an index is a whole number 0..9999, not " + std::string(s); return false; }
            t = Target{ Scope::Input, "inputState", true, index };
            return true;
        }
        why = kFormError + std::string(s);
        return false;
    }
    Scope scope = Scope::Player;
    const std::string_view head = s.substr(0, dot);
    if (!ParseScope(head, scope)) { why = "unknown scope " + std::string(head) + " (player, pst, profile, input or queue)"; return false; }
    if (scope == Scope::Input) { why = kFormError + std::string(s); return false; }
    const std::string_view rest = s.substr(dot + 1);
    const size_t open = rest.find('[');
    const std::string_view name = rest.substr(0, open);
    if (!IsName(name)) { why = "a member name is 1..64 letters, digits or _, not " + std::string(rest); return false; }
    Target out{ scope, std::string(name), false, 0 };
    if (open != std::string_view::npos) {
        if (!ParseIndexSuffix(rest, open, out.index)) { why = "an index is a whole number 0..9999, not " + std::string(rest); return false; }
        out.indexed = true;
    }
    t = out;
    return true;
}

inline bool ParseFrames(std::string_view s, int64_t& out, std::string& why)
{
    long long v = 0;
    if (!ParseWhole(s, 1, kMaxFrames, v)) { why = "frames must be a whole number 1.." + std::to_string(kMaxFrames) + ", not " + std::string(s); return false; }
    out = v;
    return true;
}

inline bool ParseMult(std::string_view s, double& out, std::string& why)
{
    double v = 0;
    if (!ParseNumber(s, v) || v < -kMaxMult || v > kMaxMult) { why = "mult must be a number -100..100, not " + std::string(s); return false; }
    out = v;
    return true;
}

inline bool ParseRowArg(std::string_view s, Row& out, std::string& why)
{
    if (ParseRow(s, out)) return true;
    why = "unknown row " + std::string(s) + " (" + RowList() + ")";
    return false;
}

} // namespace detail

inline constexpr const char* kWalkUsage = "usage: walkable here <dx> <dy> | walkable at <x> <y> | walkable scan <deg> <step> <count>";

// `tok` is everything after the command word. False with `why` naming the
// first thing refused; `c` is then unspecified.
inline bool Parse(const std::vector<std::string>& tok, Command& c, std::string& why)
{
    c = Command{};
    if (tok.empty()) { c.form = Form::Status; return true; }
    const std::string head = LowerText(tok[0]);
    const size_t n = tok.size();

    if (head == "hook") {
        if (n != 1) { why = "usage: mark1probe hook"; return false; }
        c.form = Form::Hook;
        return true;
    }
    if (head == "off") {
        if (n != 1) { why = "usage: mark1probe off"; return false; }
        c.form = Form::Off;
        return true;
    }
    if (head == "show") {
        if (n == 1) { c.form = Form::Show; return true; }
        if (n == 2 && SameWord(tok[1], "reset")) { c.form = Form::ShowReset; return true; }
        why = "usage: mark1probe show [reset]";
        return false;
    }
    if (head == "args") {
        const std::string usage = "args needs a whole number 1.." + std::to_string(kMaxArgs);
        if (n != 2) { why = usage; return false; }
        long long v = 0;
        if (!ParseWhole(tok[1], 1, kMaxArgs, v)) { why = usage + ", not " + tok[1]; return false; }
        c.form = Form::Args;
        c.count = static_cast<int>(v);
        return true;
    }
    if (head == "watch") {
        const char* usage = "usage: watch <scope> <frames> [after <row>]";
        if (n != 3 && n != 5) { why = usage; return false; }
        if (!ParseScope(tok[1], c.scope)) { why = "unknown scope " + tok[1] + " (player, pst, profile, input or queue)"; return false; }
        if (!detail::ParseFrames(tok[2], c.frames, why)) return false;
        if (n == 5) {
            if (!SameWord(tok[3], "after")) { why = usage; return false; }
            if (!detail::ParseRowArg(tok[4], c.afterRow, why)) return false;
            c.after = true;
        }
        c.form = Form::Watch;
        return true;
    }
    if (head == "set" || head == "scale") {
        const bool set = head == "set";
        const std::string usage = set ? "usage: set <scope>.<name>[<i>] <value> during <row> [frames]"
                                      : "usage: scale <scope>.<name>[<i>] <mult> during <row> [frames]";
        if ((n != 5 && n != 6) || !SameWord(tok[3], "during")) { why = usage; return false; }
        Lever& l = c.lever;
        l.kind = set ? LeverKind::Set : LeverKind::Scale;
        if (!detail::ParseTarget(tok[1], l.target, why)) return false;
        if (set) {
            if (!ParseValue(tok[2], l.value)) { why = "a value is a number, true or false, not " + tok[2]; return false; }
        } else if (!detail::ParseMult(tok[2], l.mult, why)) {
            return false;
        }
        if (!detail::ParseRowArg(tok[4], l.row, why)) return false;
        if (n == 6 && !detail::ParseFrames(tok[5], l.frames, why)) return false;
        c.form = set ? Form::Set : Form::Scale;
        return true;
    }
    if (head == "move") {
        if (n != 3 && n != 4) { why = "usage: move <row> <mult> [frames]"; return false; }
        Lever& l = c.lever;
        l.kind = LeverKind::Move;
        if (!detail::ParseRowArg(tok[1], l.row, why)) return false;
        if (!detail::ParseMult(tok[2], l.mult, why)) return false;
        if (n == 4 && !detail::ParseFrames(tok[3], l.frames, why)) return false;
        c.form = Form::Move;
        return true;
    }
    if (head == "stat") {
        if (n != 3 && n != 4) { why = "usage: stat <id> <mult> [frames]"; return false; }
        Lever& l = c.lever;
        l.kind = LeverKind::Stat;
        l.row = Row::ReturnSpecificStat;
        if (!ParseWhole(tok[1], 0, kMaxStatId, l.statId)) { why = "stat id must be a whole number 0..999, not " + tok[1]; return false; }
        if (!detail::ParseMult(tok[2], l.mult, why)) return false;
        if (n == 4 && !detail::ParseFrames(tok[3], l.frames, why)) return false;
        c.form = Form::Stat;
        return true;
    }
    if (head == "count") {
        if (n != 3) { why = "usage: count <object name> <frames>"; return false; }
        if (!IsName(tok[1])) { why = "an object name is 1..64 letters, digits or _, not " + tok[1]; return false; }
        if (!detail::ParseFrames(tok[2], c.frames, why)) return false;
        c.object = tok[1];
        c.form = Form::Count;
        return true;
    }
    if (head == "pos") {
        if (n != 2) { why = "usage: pos <frames>"; return false; }
        if (!detail::ParseFrames(tok[1], c.frames, why)) return false;
        c.form = Form::Pos;
        return true;
    }
    if (head == "walkable") {
        if (n < 2) { why = kWalkUsage; return false; }
        const std::string sub = LowerText(tok[1]);
        const bool pair = sub == "here" || sub == "at";
        if (!(pair && n == 4) && !(sub == "scan" && n == 5)) { why = kWalkUsage; return false; }
        if (!ParseNumber(tok[2], c.a)) { why = "walkable needs numbers, not " + tok[2]; return false; }
        if (pair) {
            if (!ParseNumber(tok[3], c.b)) { why = "walkable needs numbers, not " + tok[3]; return false; }
            c.form = sub == "here" ? Form::WalkHere : Form::WalkAt;
            return true;
        }
        if (!ParseNumber(tok[3], c.b) || c.b < 1 || c.b > 1024) { why = "scan step must be a number 1..1024, not " + tok[3]; return false; }
        long long count = 0;
        if (!ParseWhole(tok[4], 1, 32, count)) { why = "scan count must be a whole number 1..32, not " + tok[4]; return false; }
        c.count = static_cast<int>(count);
        c.form = Form::WalkScan;
        return true;
    }
    why = "unknown form " + tok[0] + " (hook, off, show, args, watch, set, scale, move, stat, count, pos or walkable)";
    return false;
}

inline std::vector<std::string> Usage()
{
    return {
        "  mark1probe hook                                      install the nine rows as pass-throughs",
        "  mark1probe off                                       every lever and watch off (rows stay installed)",
        "  mark1probe show [reset]                              per-row calls local/all, the lever's counts, casts per talent id",
        "  mark1probe args <n>                                  the next n CheckTalentUse calls' arguments",
        "  mark1probe watch <scope> <frames> [after <row>]      changed members; scopes player pst profile input queue",
        "  mark1probe set <scope>.<name>[<i>] <v> during <row> [frames]   write before each call, restore after",
        "  mark1probe scale <scope>.<name>[<i>] <m> during <row> [frames] multiply before each call, restore after",
        "  mark1probe move <row> <mult> [frames]                scale each call's own displacement",
        "  mark1probe stat <id> <mult> [frames]                 scale one stat id's number (dispatcher, and 64/66's getter)",
        "  mark1probe count <object name> <frames>              instance_number per frame, one line per change",
        "  mark1probe pos <frames>                              the cursor, each moving call's dx dy, moves outside the rows, totals",
        "  mark1probe walkable here <dx> <dy> | at <x> <y> | scan <deg> <step> <count>",
        "  frames default 600, at most 3600; one lever at a time, a new one replaces it",
    };
}

// ---- `args` ------------------------------------------------------------------------

// `ctu #<k> f=<frame> argc=<c> a0=<v> a1=<v> a2=<v>`: the first three
// arguments as the adapter wrote them, `none` past argc.
inline std::string ArgsLine(int k, int64_t frame, int argc, const std::vector<std::string>& args)
{
    std::string out = "ctu #" + std::to_string(k) + " f=" + std::to_string(frame) + " argc=" + std::to_string(argc);
    for (size_t i = 0; i < 3; ++i) out += " a" + std::to_string(i) + "=" + (i < args.size() ? args[i] : std::string("none"));
    return out;
}

// ---- `pos` -------------------------------------------------------------------------

inline std::string PosStartLine(bool cursorRead, double x, double y)
{
    if (!cursorRead) return "pos cursor=unreadable";
    return "pos cursor x=" + NumberText(x) + " y=" + NumberText(y);
}

// ---- `walkable` ---------------------------------------------------------------------

inline constexpr int kGridCell = 16;       // global.pathGrid's cell, in pixels
inline constexpr size_t kMaxTileLayers = 4;

// Every reading below starts as unreadable, and only a read that succeeded
// overwrites it: a failed place_meeting or collision_line (unresolved object,
// no player, a builtin error) must never print as the open-floor answer.
enum class Grid : int { Free, Blocked, Unreadable };
enum class Family : int { Free, Blocked, Unreadable };
enum class WallLine : int { Clear, Crossed, Unreadable };

// A builtin's answer as the adapter received it, so the line can show what
// each verdict came from. `NotCalled`: the call was not made (no player, an
// object that did not resolve, no path grid); `Failed`: the call itself
// failed or threw; `Read`: `kind` is the RValue kind in lower case without its
// prefix (`bool`, `real`, `int32`, `int64`, `ref`, `undefined`, ...) and
// `number` its value where it has one (a bool as 0 or 1).
struct RawResult {
    enum class State : int { NotCalled, Failed, Read };
    State                 state = State::NotCalled;
    std::string           kind;
    std::optional<double> number;
};

inline bool RawNumeric(const RawResult& r)
{
    return r.kind == "real" || r.kind == "int32" || r.kind == "int64";
}

// `<kind>:<value>`, `error` for a failed call, `none` for one not made.
inline std::string RawText(const RawResult& r)
{
    if (r.state == RawResult::State::NotCalled) return "none";
    if (r.state == RawResult::State::Failed) return "error";
    if (!r.number) return r.kind + ":?";
    if (r.kind == "bool") return r.kind + (*r.number != 0 ? ":true" : ":false");
    return r.kind + ":" + NumberText(*r.number);
}

// place_meeting answers whether the mask meets the family: true or any
// non-zero number is a meeting, false or 0 is none. Not an instance answer:
// a numeric 0 is "no meeting", never instance 0. Any other kind (a ref
// included) is no answer, so the field stays unreadable.
inline Family FamilyFrom(const RawResult& r)
{
    if (r.state != RawResult::State::Read || !r.number || !(r.kind == "bool" || RawNumeric(r))) return Family::Unreadable;
    return *r.number != 0 ? Family::Blocked : Family::Free;
}

// collision_line answers an instance (an id or ref >= 0) or `noone` (-4); a
// bool is taken as it is.
inline WallLine WallLineFrom(const RawResult& r)
{
    if (r.state != RawResult::State::Read || !r.number) return WallLine::Unreadable;
    if (r.kind == "bool") return *r.number != 0 ? WallLine::Crossed : WallLine::Clear;
    if (!RawNumeric(r) && r.kind != "ref") return WallLine::Unreadable;
    return *r.number >= 0 ? WallLine::Crossed : WallLine::Clear;
}

// mp_grid_get_cell answers -1 for a blocked cell (RDM § 22.7).
inline Grid GridFrom(const RawResult& r)
{
    if (r.state != RawResult::State::Read || !r.number || !RawNumeric(r)) return Grid::Unreadable;
    return *r.number < 0 ? Grid::Blocked : Grid::Free;
}

// What a blocked family met: instance_place's answer on the same family at
// the same point, as `<object name>:<id>`, `noone`, or its raw text when it
// is not an instance answer.
inline std::string MetText(const RawResult& inst, std::string objectName)
{
    if (inst.state != RawResult::State::Read || !inst.number || !(RawNumeric(inst) || inst.kind == "ref")) return RawText(inst);
    if (*inst.number < 0) return "noone";
    for (char& ch : objectName) if (ch == ' ' || ch == ',' || ch == ':') ch = '_';
    return (objectName.empty() ? std::string("?") : objectName) + ":" + NumberText(*inst.number);
}

// One tile layer's cell; no value means the layer was found but its cell
// could not be read.
struct TileCell {
    std::string              layer;
    std::optional<long long> value;
};

// One point's readings: inside the room, the path grid's cell, the player's
// place_meeting against Collision_Parent_obj there, a collision_line from the
// player to it against Wall_Parent_obj, and up to four tile layers' cells.
// `tilesRead` says the tile layers were enumerated: read with no layers
// prints `none`, not read prints `unreadable`. The three `*Raw` answers are
// what grid, family and wall-line were read from (GridFrom, FamilyFrom,
// WallLineFrom); `met` is MetText, printed only when family reads blocked.
struct WalkPoint {
    double                x = 0, y = 0;
    bool                  roomIn = false;
    Grid                  grid = Grid::Unreadable;
    Family                family = Family::Unreadable;
    WallLine              wallLine = WallLine::Unreadable;
    bool                  tilesRead = false;
    std::vector<TileCell> tiles;
    RawResult             gridRaw, familyRaw, wallLineRaw;
    std::string           met;
};

inline std::string WalkLine(const WalkPoint& w)
{
    const char* grid = w.grid == Grid::Free ? "free" : w.grid == Grid::Blocked ? "blocked" : "unreadable";
    const char* family = w.family == Family::Free ? "free" : w.family == Family::Blocked ? "blocked" : "unreadable";
    const char* wall = w.wallLine == WallLine::Clear ? "clear" : w.wallLine == WallLine::Crossed ? "crossed" : "unreadable";
    std::string tiles;
    for (size_t i = 0; i < w.tiles.size() && i < kMaxTileLayers; ++i)
        tiles += (i ? "," : "") + w.tiles[i].layer + ":"
            + (w.tiles[i].value ? std::to_string(*w.tiles[i].value) : std::string("unreadable"));
    if (!w.tilesRead) tiles = "unreadable";
    else if (tiles.empty()) tiles = "none";
    return "walkable x=" + NumberText(w.x) + " y=" + NumberText(w.y) + " room=" + (w.roomIn ? "in" : "out")
        + " grid=" + grid + " family=" + family + " wall-line=" + wall + " tile=" + tiles
        + " grid-raw=" + RawText(w.gridRaw) + " family-raw=" + RawText(w.familyRaw) + " wall-line-raw=" + RawText(w.wallLineRaw)
        + (w.family == Family::Blocked ? " met=" + (w.met.empty() ? std::string("unreadable") : w.met) : std::string());
}

inline bool RoomContains(double x, double y, double roomWidth, double roomHeight)
{
    return x >= 0 && y >= 0 && x < roomWidth && y < roomHeight;
}

// The path grid's cell index along one axis.
inline long long GridCellOf(double v)
{
    return static_cast<long long>(std::floor(v / kGridCell));
}

// `scan`: `count` points from (px, py), `step` pixels apart, in direction
// `deg` the GameMaker way (0 is right, 90 is up, so y falls). Rounded to a
// thousandth of a pixel, so a right angle prints as a whole number.
inline std::vector<std::pair<double, double>> ScanPoints(double px, double py, double deg, double step, int count)
{
    constexpr double kPi = 3.14159265358979323846;
    const double rad = deg * kPi / 180.0;
    const auto round3 = [](double v) { return std::round(v * 1000.0) / 1000.0; };
    std::vector<std::pair<double, double>> out;
    for (int k = 1; k <= count; ++k)
        out.emplace_back(round3(px + std::cos(rad) * step * k), round3(py - std::sin(rad) * step * k));
    return out;
}

// ---- the probe ------------------------------------------------------------------------

enum class HookState : int { NotTried, Detoured, Failed, Held };

struct RowCalls {
    uint64_t local = 0;   // calls whose self is the local player
    uint64_t all = 0;     // every call
};

// The current lever's counts, zeroed when a lever is set.
struct LeverCounts {
    uint64_t applied = 0;   // set/scale: written before the original (and restored after)
    uint64_t missing = 0;   // set/scale: the member was unreadable, or a bool under scale, or the write failed
    uint64_t moved = 0;     // move: a call that moved the player, its displacement scaled
    uint64_t still = 0;     // move: a call that did not move the player, left alone
    uint64_t changedDispatch = 0;  // stat: a ReturnSpecificStat call's number scaled there
    uint64_t changedGetter = 0;    // stat: the id's own getter row's number scaled
    uint64_t getterDirect = 0;     // stat: of those, getter calls with no ReturnSpecificStat around them
    uint64_t skip = 0;             // stat: a matching call whose result was not a number
};

class Probe {
public:
    // ---- the rows' hooks (`hook`) ------------------------------------------
    // `holder` names what holds a Held row; empty means the adapter could not
    // tell, and the line says an earlier table-only hook.
    void NoteHook(Row r, HookState s, std::string holder = {})
    {
        const int i = static_cast<int>(r);
        if (i < 0 || i >= kRowCount) return;
        hook_[i] = s;
        holder_[i] = std::move(holder);
    }
    HookState HookOf(Row r) const { return hook_[static_cast<int>(r)]; }
    int Hooked() const { return CountHooks(HookState::Detoured); }
    int Held() const { return CountHooks(HookState::Held); }
    int Failed() const { return CountHooks(HookState::Failed); }

    std::string HookLine() const
    {
        return "mark1probe hook: " + std::to_string(Hooked()) + " detoured, " + std::to_string(Failed()) + " failed, "
            + std::to_string(Held()) + " held";
    }

    std::vector<std::string> HeldLines() const
    {
        std::vector<std::string> out;
        for (int i = 0; i < kRowCount; ++i) {
            if (hook_[i] != HookState::Held) continue;
            out.push_back("  " + std::string(kRowNames[i]) + " held by "
                          + (holder_[i].empty() ? std::string("an earlier hook (table-only)") : holder_[i]));
        }
        return out;
    }

    // The bare command's reply.
    std::vector<std::string> StatusLines() const
    {
        std::vector<std::string> out = {
            "mark1probe: research build",
            "mark1probe: rows=" + std::to_string(kRowCount) + " hooked=" + std::to_string(Hooked()) + " held="
                + std::to_string(Held()) + " lever=" + LeverText() + " watch=" + (WatchOn() ? "on" : "off"),
        };
        for (std::string& l : Usage()) out.push_back(std::move(l));
        return out;
    }

    // ---- the lever ----------------------------------------------------------
    // On from `frame` for the lever's frames. Returns the text of the lever it
    // replaced, empty when none was on. The lever's counts start at zero.
    std::string SetLever(const Lever& l, int64_t frame)
    {
        const std::string replaced = lever_.kind == LeverKind::Off ? std::string() : Mark1Probe::LeverText(lever_);
        lever_ = l;
        leverStart_ = frame;
        counts_ = LeverCounts{};
        return replaced;
    }
    void LeverOff() { lever_ = Lever{}; }
    const Lever& CurrentLever() const { return lever_; }
    std::string LeverText() const { return Mark1Probe::LeverText(lever_); }
    const LeverCounts& Applied() const { return counts_; }

    // On frames start .. start + frames - 1.
    bool LeverLive(int64_t frame) const
    {
        return lever_.kind != LeverKind::Off && frame >= leverStart_ && frame - leverStart_ < lever_.frames;
    }

    // `off`: every lever, watch, args, count and pos off; the rows stay
    // installed as pass-throughs and the counts stay for `show`.
    void Off()
    {
        LeverOff();
        StopWatch();
        argsLeft_ = 0;
        countOn_ = false;
        posOn_ = false;
    }

    // ---- one hooked call ----------------------------------------------------
    // The adapter's detour for `row` calls this with the trampoline as
    // `original`. `access` is what the adapter supplies around the call:
    //   bool Read(const Target&, Value&)       the member now (number or bool)
    //   bool Write(const Target&, const Value&)
    //   bool Position(double& x, double& y)     the local player's x and y
    //   bool SetPosition(double x, double y)
    //   bool StatId(double& id)                 the call's stat-id argument
    //   bool Result(double& n)                  the call's result, if a number
    //   void SetResult(double n)
    // A call whose self is not the local player runs the original and nothing
    // else. set, scale and move act on the outermost call of their row only,
    // so a row the game calls inside itself is written once. stat acts on
    // every matching ReturnSpecificStat call (an inner call may ask for a
    // different id) and on the outermost call of the id's getter row, and
    // scales a number once: a call whose original already ran a scaling
    // (the dispatcher handing id 64 to a detoured StatMovementSpeed) returns
    // that number as it is.
    template <class Access, class Original>
    void Around(Row row, int64_t frame, bool local, Access& access, Original&& original)
    {
        const int i = static_cast<int>(row);
        if (i < 0 || i >= kRowCount) { original(); return; }
        ++calls_[i].all;
        if (!local) { original(); return; }
        ++calls_[i].local;
        if (watchPending_ && watch_.after && watch_.afterRow == row) {
            watchPending_ = false;
            watchRunning_ = true;
            watchStart_ = frame;
        }

        const bool outer = depth_[i] == 0;
        const bool live = LeverLive(frame) && lever_.row == row;
        const bool writes = live && outer && (lever_.kind == LeverKind::Set || lever_.kind == LeverKind::Scale);
        const bool moves = live && outer && lever_.kind == LeverKind::Move;
        const bool statOn = LeverLive(frame) && lever_.kind == LeverKind::Stat;
        Row getter = Row::ReturnSpecificStat;
        const bool statDispatch = statOn && row == Row::ReturnSpecificStat;
        const bool statGetter = statOn && outer && StatGetterRow(lever_.statId, getter) && getter == row;
        const uint64_t scaledBefore = statScaled_;
        const bool tracing = PosRunning(frame);
        const bool outermostOfAll = depthAll_ == 0;

        const Target target = writes ? lever_.target : Target{};
        Value old{};
        bool restore = false;
        if (writes) {
            if (!access.Read(target, old) || (lever_.kind == LeverKind::Scale && old.isBool)) {
                ++counts_.missing;
            } else {
                const Value v = lever_.kind == LeverKind::Set ? lever_.value : Value{ false, old.number * lever_.mult };
                if (access.Write(target, v)) { restore = true; ++counts_.applied; }
                else ++counts_.missing;
            }
        }
        double x0 = 0, y0 = 0;
        const bool havePos = (moves || tracing) && access.Position(x0, y0);
        const double mult = lever_.mult;

        ++depth_[i];
        ++depthAll_;
        try {
            original();
        } catch (...) {
            --depth_[i];
            --depthAll_;
            if (restore) access.Write(target, old);
            throw;
        }
        --depth_[i];
        --depthAll_;
        if (restore) access.Write(target, old);

        if (havePos) {
            double x1 = 0, y1 = 0;
            if (access.Position(x1, y1)) {
                double dx = x1 - x0, dy = y1 - y0;
                const bool moved = dx != 0 || dy != 0;
                if (moves) {
                    if (moved) {
                        dx *= mult;
                        dy *= mult;
                        access.SetPosition(x0 + dx, y0 + dy);
                        ++counts_.moved;
                    } else {
                        ++counts_.still;
                    }
                }
                if (tracing && moved) {
                    Emit("pos f=" + std::to_string(frame) + " " + std::string(RowName(row)) + " dx=" + NumberText(dx)
                         + " dy=" + NumberText(dy));
                    ++posMovers_[i];
                    if (outermostOfAll) {
                        posInRows_ += std::hypot(dx, dy);
                        posRowDx_ += dx;
                        posRowDy_ += dy;
                    }
                }
            }
        }

        if (statDispatch || statGetter) {
            double id = 0;
            const bool matches = statGetter || (access.StatId(id) && id == static_cast<double>(lever_.statId));
            if (matches && statScaled_ == scaledBefore) {
                double n = 0;
                if (access.Result(n)) {
                    access.SetResult(n * lever_.mult);
                    ++statScaled_;
                    if (statGetter) {
                        ++counts_.changedGetter;
                        if (depth_[static_cast<int>(Row::ReturnSpecificStat)] == 0) ++counts_.getterDirect;
                    } else {
                        ++counts_.changedDispatch;
                    }
                } else {
                    ++counts_.skip;
                }
            }
        }
    }

    const RowCalls& Calls(Row r) const { return calls_[static_cast<int>(r)]; }

    // ---- casts (`show`) -----------------------------------------------------
    // One local-player TalentUse call, by the talent id the adapter read off
    // it (not a whole number: counted under `?`).
    void NoteCast(double talentId)
    {
        if (std::isfinite(talentId) && talentId == std::floor(talentId) && std::fabs(talentId) < 1e15)
            ++casts_[static_cast<long long>(talentId)];
        else
            ++castsOther_;
    }

    std::string CastsText() const
    {
        std::string out;
        for (const auto& [id, n] : casts_) out += (out.empty() ? "" : ",") + std::to_string(id) + ":" + std::to_string(n);
        if (castsOther_) out += (out.empty() ? "" : ",") + std::string("?:") + std::to_string(castsOther_);
        return "casts=" + (out.empty() ? std::string("none") : out);
    }

    // `show reset`: calls, casts and the lever's counts back to zero.
    void ResetCounts()
    {
        for (RowCalls& c : calls_) c = RowCalls{};
        casts_.clear();
        castsOther_ = 0;
        counts_ = LeverCounts{};
    }

    std::vector<std::string> ShowLines() const
    {
        std::vector<std::string> out;
        out.push_back("mark1probe show: lever=" + LeverText() + " applied=" + std::to_string(counts_.applied)
                      + " missing=" + std::to_string(counts_.missing) + " moved=" + std::to_string(counts_.moved)
                      + " still=" + std::to_string(counts_.still) + " " + StatCountsText());
        for (int i = 0; i < kRowCount; ++i) {
            const char* state = hook_[i] == HookState::Detoured ? "" : hook_[i] == HookState::Held ? " held"
                : hook_[i] == HookState::Failed ? " failed" : " not hooked";
            out.push_back("  " + std::string(kRowNames[i]) + " calls=" + std::to_string(calls_[i].local) + "/"
                          + std::to_string(calls_[i].all) + state);
        }
        out.push_back(CastsText());
        return out;
    }

    // ---- `args` -------------------------------------------------------------
    void StartArgs(int n) { argsLeft_ = n > 0 ? n : 0; argsTaken_ = 0; }
    bool ArgsOn() const { return argsLeft_ > 0; }
    // The next line's number (1-based), or 0 once n were taken.
    int TakeArg()
    {
        if (argsLeft_ <= 0) return 0;
        --argsLeft_;
        return ++argsTaken_;
    }

    // ---- `watch` -------------------------------------------------------------
    // From `frame` for `frames` frames, or, with `after`, from the row's next
    // local-player call. A new watch replaces the old one.
    void StartWatch(Scope scope, int64_t frames, int64_t frame, bool after, Row afterRow)
    {
        watch_ = Command{};
        watch_.form = Form::Watch;
        watch_.scope = scope;
        watch_.frames = frames;
        watch_.after = after;
        watch_.afterRow = afterRow;
        watchPending_ = after;
        watchRunning_ = !after;
        watchStart_ = frame;
        watchLines_ = 0;
        watchDropped_ = 0;
        watchHave_ = false;
        watchLast_.clear();
    }
    void StopWatch()
    {
        watchPending_ = false;
        watchRunning_ = false;
        watchLast_.clear();
    }
    bool WatchOn() const { return watchPending_ || watchRunning_; }
    bool WatchRunning(int64_t frame) const
    {
        return watchRunning_ && frame >= watchStart_ && frame - watchStart_ < watch_.frames;
    }
    Scope WatchScope() const { return watch_.scope; }

    // One snapshot of the scope's numeric and bool members (input: element
    // index -> value), each value as the adapter printed it. The first is the
    // baseline; each later one prints a line per member that changed, appeared
    // (`none->v`) or went away (`v->none`). Past kWatchMaxLines the rest are
    // counted and the watch ends, its end line last.
    std::vector<std::string> ObserveWatch(int64_t frame, const std::map<std::string, std::string>& snapshot)
    {
        std::vector<std::string> out;
        if (!WatchRunning(frame)) return out;
        if (!watchHave_) { watchLast_ = snapshot; watchHave_ = true; return out; }
        const auto line = [&](const std::string& name, const std::string& from, const std::string& to) {
            if (watchLines_ >= kWatchMaxLines) { ++watchDropped_; return; }
            ++watchLines_;
            const std::string member = watch_.scope == Scope::Input ? "input[" + name + "]" : std::string(ScopeName(watch_.scope)) + "." + name;
            out.push_back("watch f=" + std::to_string(frame) + " " + member + "=" + from + "->" + to);
        };
        for (const auto& [name, value] : snapshot) {
            const auto it = watchLast_.find(name);
            if (it == watchLast_.end()) line(name, "none", value);
            else if (it->second != value) line(name, it->second, value);
        }
        for (const auto& [name, value] : watchLast_)
            if (snapshot.find(name) == snapshot.end()) line(name, value, "none");
        watchLast_ = snapshot;
        if (watchDropped_ > 0) {
            out.push_back(WatchEndLine(frame) + " (line cap)");
            StopWatch();
        }
        return out;
    }

    // ---- `count` --------------------------------------------------------------
    void StartCount(const std::string& object, int64_t frames, int64_t frame)
    {
        countOn_ = true;
        countObject_ = object;
        countFrames_ = frames;
        countStart_ = frame;
        countHave_ = false;
        countLast_ = 0;
        countChanges_ = 0;
    }
    bool CountRunning(int64_t frame) const
    {
        return countOn_ && frame >= countStart_ && frame - countStart_ < countFrames_;
    }
    // The frame's instance_number: a line when it changed (the first reading
    // counts as a change from `none`), empty otherwise.
    std::string ObserveCount(int64_t frame, long long n)
    {
        if (!CountRunning(frame)) return {};
        if (countHave_ && n == countLast_) return {};
        const std::string from = countHave_ ? std::to_string(countLast_) : std::string("none");
        countHave_ = true;
        countLast_ = n;
        ++countChanges_;
        return "count f=" + std::to_string(frame) + " " + countObject_ + "=" + from + "->" + std::to_string(n);
    }

    // ---- `pos` ----------------------------------------------------------------
    // Two sources. Around adds up the displacement made inside the outermost
    // call of a row (`in-rows`), and the adapter samples the local player's x
    // and y once a frame through ObservePosFrame (`total`, the frame-to-frame
    // path). A frame whose step the rows' displacement since the last sample
    // does not account for is a move outside the nine rows: its residual
    // (the frame's step minus the rows' summed dx and dy) is printed and
    // added to `outside`. Without the sample, a player moved by code that is
    // not a row would read the same as a player standing still.
    void StartPos(int64_t frames, int64_t frame)
    {
        posOn_ = true;
        posFrames_ = frames;
        posStart_ = frame;
        posInRows_ = 0;
        posRowDx_ = 0;
        posRowDy_ = 0;
        posPath_ = 0;
        posOutside_ = 0;
        posOutsideFrames_ = 0;
        posSamples_ = 0;
        posUnread_ = 0;
        posHaveSample_ = false;
        for (uint64_t& m : posMovers_) m = 0;
    }
    bool PosRunning(int64_t frame) const
    {
        return posOn_ && frame >= posStart_ && frame - posStart_ < posFrames_;
    }

    // Once a frame while `pos` runs: the local player's x and y, or `read`
    // false when they could not be read (counted, never taken as a position).
    void ObservePosFrame(int64_t frame, bool read, double x, double y)
    {
        if (!PosRunning(frame)) return;
        if (!read || !std::isfinite(x) || !std::isfinite(y)) { ++posUnread_; return; }
        ++posSamples_;
        if (posHaveSample_) {
            const double fx = x - posLastX_, fy = y - posLastY_;
            posPath_ += std::hypot(fx, fy);
            const double ox = fx - posRowDx_, oy = fy - posRowDy_;
            const double outside = std::hypot(ox, oy);
            if (outside > kPosEpsilon) {
                posOutside_ += outside;
                ++posOutsideFrames_;
                Emit("pos f=" + std::to_string(frame) + " outside dx=" + NumberText(Round3(ox)) + " dy="
                     + NumberText(Round3(oy)));
            }
        }
        posHaveSample_ = true;
        posLastX_ = x;
        posLastY_ = y;
        posRowDx_ = 0;
        posRowDy_ = 0;
    }

    // Lines written inside a hooked call (`pos`), for the adapter to print.
    std::vector<std::string> TakeLines()
    {
        std::vector<std::string> out;
        out.swap(outbox_);
        return out;
    }

    // ---- the frame tick ---------------------------------------------------------
    // Ends whatever ran out of frames, and returns its end lines.
    std::vector<std::string> Tick(int64_t frame)
    {
        std::vector<std::string> out;
        if (lever_.kind != LeverKind::Off && frame - leverStart_ >= lever_.frames) {
            std::string counts;
            switch (lever_.kind) {
            case LeverKind::Set:
            case LeverKind::Scale: counts = "applied=" + std::to_string(counts_.applied); break;
            case LeverKind::Move:  counts = "moved=" + std::to_string(counts_.moved) + " still=" + std::to_string(counts_.still); break;
            case LeverKind::Stat:  counts = StatCountsText(); break;
            case LeverKind::Off:   break;
            }
            out.push_back("mark1probe: lever " + LeverText() + " ended " + counts);
            LeverOff();
        }
        if (watchRunning_ && frame - watchStart_ >= watch_.frames) {
            out.push_back(WatchEndLine(frame));
            StopWatch();
        }
        if (countOn_ && frame - countStart_ >= countFrames_) {
            out.push_back("count end " + countObject_ + " frames=" + std::to_string(frame - countStart_)
                          + " changes=" + std::to_string(countChanges_));
            countOn_ = false;
        }
        if (posOn_ && frame - posStart_ >= posFrames_) {
            std::string movers;
            for (int i = 0; i < kRowCount; ++i) {
                if (!posMovers_[i]) continue;
                movers += (movers.empty() ? "" : ",") + std::string(kRowNames[i]) + ":" + std::to_string(posMovers_[i]);
            }
            if (posOutsideFrames_) movers += (movers.empty() ? "" : ",") + std::string("outside:") + std::to_string(posOutsideFrames_);
            // Fewer than two readable samples measure no step at all: `none`,
            // never a 0 that reads as standing still.
            const bool measured = posSamples_ >= 2;
            const auto px = [](double v) { return NumberText(std::round(v * 10.0) / 10.0); };
            out.push_back("pos total=" + (measured ? px(posPath_) : std::string("none")) + " in-rows=" + px(posInRows_)
                          + " outside=" + (measured ? px(posOutside_) : std::string("none")) + " frames="
                          + std::to_string(frame - posStart_) + " samples=" + std::to_string(posSamples_)
                          + " unread=" + std::to_string(posUnread_)
                          + " movers=" + (movers.empty() ? std::string("none") : movers));
            posOn_ = false;
        }
        return out;
    }

private:
    int CountHooks(HookState s) const
    {
        int n = 0;
        for (HookState h : hook_) n += h == s ? 1 : 0;
        return n;
    }

    // stat's two routes apart, so a session can tell "never engaged where the
    // game reads the stat" (both zero, or only the dispatcher) from "engaged
    // and changed nothing". getter-direct is the getter calls no dispatcher
    // call was around: callers that bypass ReturnSpecificStat.
    std::string StatCountsText() const
    {
        return "changed-dispatch=" + std::to_string(counts_.changedDispatch) + " changed-getter="
            + std::to_string(counts_.changedGetter) + " getter-direct=" + std::to_string(counts_.getterDirect)
            + " skip=" + std::to_string(counts_.skip);
    }

    std::string WatchEndLine(int64_t frame) const
    {
        return "watch end scope=" + std::string(ScopeName(watch_.scope)) + " frames=" + std::to_string(frame - watchStart_)
            + " lines=" + std::to_string(watchLines_) + " dropped=" + std::to_string(watchDropped_);
    }

    void Emit(std::string line)
    {
        if (outbox_.size() < kMaxOutbox) outbox_.push_back(std::move(line));
    }

    HookState hook_[kRowCount]{};
    std::string holder_[kRowCount];
    RowCalls calls_[kRowCount]{};
    int depth_[kRowCount]{};
    int depthAll_ = 0;

    Lever lever_;
    int64_t leverStart_ = 0;
    LeverCounts counts_;

    std::map<long long, uint64_t> casts_;
    uint64_t castsOther_ = 0;

    int argsLeft_ = 0;
    int argsTaken_ = 0;

    Command watch_;
    bool watchPending_ = false;
    bool watchRunning_ = false;
    int64_t watchStart_ = 0;
    int watchLines_ = 0;
    uint64_t watchDropped_ = 0;
    bool watchHave_ = false;
    std::map<std::string, std::string> watchLast_;

    bool countOn_ = false;
    std::string countObject_;
    int64_t countFrames_ = 0;
    int64_t countStart_ = 0;
    bool countHave_ = false;
    long long countLast_ = 0;
    uint64_t countChanges_ = 0;

    static double Round3(double v) { return std::round(v * 1000.0) / 1000.0; }

    // A frame's residual below this is floating-point noise, not a move.
    static constexpr double kPosEpsilon = 0.01;

    uint64_t statScaled_ = 0;   // stat scalings so far, so a nested call scales once

    bool posOn_ = false;
    int64_t posFrames_ = 0;
    int64_t posStart_ = 0;
    double posInRows_ = 0;      // in-row displacement, outermost row calls only
    double posRowDx_ = 0;       // in-row dx and dy since the last frame sample
    double posRowDy_ = 0;
    double posPath_ = 0;        // frame-to-frame path length
    double posOutside_ = 0;     // summed per-frame residuals no row accounts for
    uint64_t posOutsideFrames_ = 0;
    uint64_t posSamples_ = 0;
    uint64_t posUnread_ = 0;
    bool posHaveSample_ = false;
    double posLastX_ = 0;
    double posLastY_ = 0;
    uint64_t posMovers_[kRowCount]{};

    std::vector<std::string> outbox_;
};

} // namespace ForgePact::Mark1Probe
