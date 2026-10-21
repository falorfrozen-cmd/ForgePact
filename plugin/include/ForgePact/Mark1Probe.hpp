#pragma once

#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <ForgePact/Mark1Drops.hpp>
#include <ForgePact/Mark1Set.hpp>

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
// is returned unchanged, with nothing written: the baseline. The two stat
// rows' own answers are noted (ReturnSpecificStat's id argument and result,
// a getter row's result), so `statpin` can pin the last one; nothing else is
// read.
//
// Phase 2's walk research (the Mark 1 set's workorder, Live procedure 1) adds
// eleven builtin rows the walk may move the player through, `place_meeting`
// being their control; `statlog`, which prints every local answer for one
// stat id; `statpin`, which answers each one with the last answer before it
// started; `hold`, a `set` restored once, when it ends; the `effect`, `pst`
// array and `builtin` watch scopes; `pos`'s modal step and frame time; and
// `base`, a scratch item of one class and base, never placed.
//
// The set itself (phase 2's set adapter) adds `spawn`: the Mark 1 pieces, built
// from Mark1Set.hpp's table through the game's loader and placed at the
// player, so the live session has pieces to wear.
//
// The drops (phase 3, Mark1Drops.hpp) add three forms for their live session:
// `heroic <n>` calls the Heroic roll by name and tallies what it placed, the
// pieces among it by their own fields; `pool [<k>]` reads one sub-list the
// way the drops do and prints what the roll keeps of it and each piece's
// stand-in; `copies <k>` sets the copies each piece's stand-in is pushed with.
// The kill route (the ordinary unique roll) adds two: `unique <n>` calls
// DropUniqueItems by name the same way, and `pool kill` reads lootListUnique[0..5]
// the way the kill route does and prints each piece's kill stand-in and its
// count on each of [0..4]; `copies <k>` applies to both rolls. Mod live
// procedure 6 adds `killgate [0|1]` (the `Killgate` class below): a lever that
// opens LoadDrops' type-45 gate once per kill, inside the game's own call, and
// counts what that gate gets with the lever off. Mod live procedure 7 adds
// `draws` (the `Draws` class below): a watch on DropItem's own unique draw that
// classes every build by the scope it was built in and records each call's
// LoadSatanicDropTier answer; `draws push <c>`, a lever that puts the Mark 1
// kill stand-ins on all six lists for the length of each DropItem call and
// counts the draw's picks of them; and `pool draw`, the candidates the draw
// keeps per class.

// ---- the rows ----------------------------------------------------------------

// The nine script rows first (ModuleMain.cpp hooks each through
// HookOneScript), then the builtin rows (HookBuiltin), in this order.
enum class Row : int {
    CheckTalentUse, TalentUse, TalentUseSetSpeed, PlayerMovement, ReturnSpecificStat,
    StatMovementSpeed, SkillsLeap, PlayerJumpGravity, StatJumpPower,
    PlaceMeeting, MpLinearStep, MpLinearStepObject, MpPotentialStep, MpPotentialStepObject,
    MoveTowardsPoint, MotionSet, MotionAdd, MoveContactSolid, MoveOutsideSolid, MoveAndCollide,
};
inline constexpr int kScriptRowCount = 9;
inline constexpr int kBuiltinRowCount = 11;
inline constexpr int kRowCount = kScriptRowCount + kBuiltinRowCount;

// The scripts' own names, the spelling the game and the SDK use, then the
// builtins' runtime names. place_meeting is the builtin rows' control: the
// walk calls it with the player as self (hub RDM § 19.6, measured).
inline constexpr std::string_view kRowNames[kRowCount] = {
    "CheckTalentUse", "TalentUse", "TalentUseSetSpeed", "PlayerMovement", "ReturnSpecificStat",
    "StatMovementSpeed", "skillsLeap", "playerJumpGravity", "StatJumpPower",
    "place_meeting", "mp_linear_step", "mp_linear_step_object", "mp_potential_step", "mp_potential_step_object",
    "move_towards_point", "motion_set", "motion_add", "move_contact_solid", "move_outside_solid", "move_and_collide",
};

inline bool IsBuiltinRow(Row r)
{
    const int i = static_cast<int>(r);
    return i >= kScriptRowCount && i < kRowCount;
}

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
// reading places each on. effect: the player's `playerEffect` array, element
// by element. builtin: the player's own built-in variables (kBuiltinWatch and
// `alarm[0..11]`). `pst` is read element by element when `pSt` is an array,
// which phase 1's Live 3 measured (54 entries).
enum class Scope : int { Player, Pst, Profile, Input, Queue, Effect, Builtin };
inline constexpr int kScopeCount = 7;
inline constexpr std::string_view kScopeNames[kScopeCount] = { "player", "pst", "profile", "input", "queue", "effect", "builtin" };
inline constexpr const char* kScopeList = "player, pst, profile, input, queue, effect or builtin";

// The `builtin` scope's members, then alarm[0] .. alarm[kAlarmCount - 1].
inline constexpr std::string_view kBuiltinWatch[] = { "image_speed", "image_index", "sprite_index", "speed", "hspeed", "vspeed" };
inline constexpr int kAlarmCount = 12;

// The member a whole-array target names: `input[<i>]`, `effect[<i>]` and
// `pst[<i>]` (pSt read as the player's array).
inline constexpr std::string_view kInputMember = "inputState";
inline constexpr std::string_view kEffectMember = "playerEffect";
inline constexpr std::string_view kPstMember = "pSt";

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

// What a `set`, `scale` or `hold` lever writes: `<scope>.<name>` or
// `<scope>.<name>[<i>]`, and `input[<i>]`, `effect[<i>]` or `pst[<i>]` for an
// element of the scope's own array.
struct Target {
    Scope       scope = Scope::Player;
    std::string name;
    bool        indexed = false;
    long long   index = 0;
};

// `pst[<i>]`: an element of the player's pSt array, not a member of a pSt struct.
inline bool PstArrayTarget(const Target& t)
{
    return t.scope == Scope::Pst && t.indexed && t.name == kPstMember;
}

inline std::string TargetText(const Target& t)
{
    if (t.scope == Scope::Input) return "input[" + std::to_string(t.index) + "]";
    if (t.scope == Scope::Effect) return "effect[" + std::to_string(t.index) + "]";
    if (PstArrayTarget(t)) return "pst[" + std::to_string(t.index) + "]";
    std::string out = std::string(ScopeName(t.scope)) + "." + t.name;
    if (t.indexed) out += "[" + std::to_string(t.index) + "]";
    return out;
}

// A watched member as a `watch` line names it, which is how a target names
// it too: `input[<i>]` and `effect[<i>]` from an element index, `pst[<i>]`
// from a `[<i>]` key (pSt read as an array), `<scope>.<name>` otherwise.
inline std::string WatchMember(Scope scope, const std::string& name)
{
    if (scope == Scope::Input || scope == Scope::Effect) return std::string(ScopeName(scope)) + "[" + name + "]";
    if (!name.empty() && name[0] == '[') return std::string(ScopeName(scope)) + name;
    return std::string(ScopeName(scope)) + "." + name;
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
inline constexpr size_t kMaxOutbox = 2000;        // `pos` and `statlog` lines waiting for the adapter to print
inline constexpr int kStatlogMaxLines = 1200;     // one statlog's printed lines; the rest are counted
inline constexpr int kScanMaxCount = 64;          // `walkable scan` points
inline constexpr long long kMaxItemClass = 19;    // hs_game_sdk/item_type.hpp's highest class
inline constexpr long long kMaxBase = 9999;
inline constexpr long long kMaxHeroicCalls = 20;  // `heroic <n>`: calls of the Heroic roll per command
inline constexpr long long kMaxUniqueCalls = 20;  // `unique <n>`: calls of the ordinary unique roll per command

// set: write `value` into `target` before each local-player call of `row`,
//      and restore what was there after the original returns (or throws).
// scale: the same, multiplying the member's number by `mult`.
// hold: write `value` (or, with `holdMult`, the value read when the lever
//       started x `mult`) before each local-player call of `row`, and restore
//       the value read when it started once, when the lever ends (its frames
//       run out, `off`, or another lever replaces it), not after each call.
// statpin: a call `stat` would scale returns `value`, the last local answer
//       for the id before the lever started, in place of its own.
// move: after each local-player call of `row` that moved the player, set x
//       and y to before + mult x (after - before).
// stat: a ReturnSpecificStat call for the local player whose stat-id argument
//       is `statId` returns the original's number x `mult`; for an id with
//       its own getter row (StatGetterRow), a local-player call of that
//       getter does too. Compiled GML reaches a getter like StatMovementSpeed
//       by a direct call as well as through the dispatcher, so scaling the
//       dispatcher alone would be blind to those callers.
enum class LeverKind : int { Off, Set, Scale, Move, Stat, Hold, Pin };

// The getter row the stat dispatcher sends an id to, for the ids that have
// one among the rows (static reading: 64 movement speed, 66 jump power).
inline bool StatGetterRow(long long statId, Row& out)
{
    if (statId == 64) { out = Row::StatMovementSpeed; return true; }
    if (statId == 66) { out = Row::StatJumpPower; return true; }
    return false;
}

// The other way round: the stat id a getter row answers.
inline bool GetterStatId(Row r, long long& out)
{
    if (r == Row::StatMovementSpeed) { out = 64; return true; }
    if (r == Row::StatJumpPower) { out = 66; return true; }
    return false;
}

struct Lever {
    LeverKind kind = LeverKind::Off;
    Target    target;
    Value     value;              // set, hold; statpin: the pinned answer
    double    mult = 1;           // scale, move, stat; hold with holdMult
    bool      holdMult = false;   // hold: `x<mult>` of the value read when it started
    Row       row = Row::CheckTalentUse;
    long long statId = 0;
    int64_t   frames = kDefaultFrames;
};

inline std::string StatRoutes(long long statId)
{
    Row getter = Row::ReturnSpecificStat;
    return StatGetterRow(statId, getter) ? " on ReturnSpecificStat and " + std::string(RowName(getter))
                                         : std::string(" on ReturnSpecificStat (no getter row)");
}

// One word, no spaces, so `lever=<form>` stays one token on the status line.
inline std::string LeverText(const Lever& l)
{
    const std::string frames = "frames=" + std::to_string(l.frames);
    const std::string during = ",during=" + std::string(RowName(l.row)) + "," + frames + ")";
    switch (l.kind) {
    case LeverKind::Off:   return "off";
    case LeverKind::Set:   return "set(" + TargetText(l.target) + "=" + ValueText(l.value) + during;
    case LeverKind::Scale: return "scale(" + TargetText(l.target) + "*" + NumberText(l.mult) + during;
    case LeverKind::Move:  return "move(" + std::string(RowName(l.row)) + "*" + NumberText(l.mult) + "," + frames + ")";
    case LeverKind::Stat:  return "stat(" + std::to_string(l.statId) + "*" + NumberText(l.mult) + "," + frames + ")";
    case LeverKind::Hold:
        return "hold(" + TargetText(l.target) + (l.holdMult ? "*" + NumberText(l.mult) : "=" + ValueText(l.value)) + during;
    case LeverKind::Pin:   return "statpin(" + std::to_string(l.statId) + "=" + ValueText(l.value) + "," + frames + ")";
    }
    return "?";
}

// The reply to a lever form. `resolved` names the instance or struct the
// adapter resolved the target's scope to, `current` the value it read there
// now (what a restore writes back); both are only shown for set, scale and
// hold. `replaced` is the lever this one replaced, empty when none was on.
// `from` is the frame the lever starts on, printed as `from f=<frame>`.
// statpin's `current` names where its pinned answer came from.
inline std::string LeverReply(const Lever& l, int64_t from, const std::string& resolved, const std::string& current,
                              const std::string& replaced)
{
    const std::string window = " for " + std::to_string(l.frames) + " frames from f=" + std::to_string(from);
    const std::string during = " during " + std::string(RowName(l.row)) + window;
    const std::string where = " scope=" + (resolved.empty() ? std::string("?") : resolved)
        + " restores=" + (current.empty() ? std::string("?") : current);
    std::string out;
    switch (l.kind) {
    case LeverKind::Off:   out = "mark1probe: lever off"; break;
    case LeverKind::Set:   out = "mark1probe set: " + TargetText(l.target) + " := " + ValueText(l.value) + during + where; break;
    case LeverKind::Scale: out = "mark1probe scale: " + TargetText(l.target) + " x" + NumberText(l.mult) + during + where; break;
    case LeverKind::Move:
        out = "mark1probe move: " + std::string(RowName(l.row)) + " displacement x" + NumberText(l.mult) + window;
        break;
    case LeverKind::Stat:
        out = "mark1probe stat: id " + std::to_string(l.statId) + " x" + NumberText(l.mult) + StatRoutes(l.statId) + window;
        break;
    case LeverKind::Hold:
        out = "mark1probe hold: " + TargetText(l.target) + " := " + (l.holdMult ? "x" + NumberText(l.mult) : ValueText(l.value))
            + during + where + " (once, when the lever ends)";
        break;
    case LeverKind::Pin:
        out = "mark1probe statpin: id " + std::to_string(l.statId) + " pinned=" + ValueText(l.value)
            + (current.empty() ? std::string() : " (" + current + ")") + StatRoutes(l.statId) + window;
        break;
    }
    if (!replaced.empty()) out += " (replaces " + replaced + ")";
    return out;
}

// ---- the commands --------------------------------------------------------------------

enum class Form : int {
    Status, Hook, Off, Show, ShowReset, Args, Watch, Set, Scale, Move, Stat, Count, Pos, WalkHere, WalkAt, WalkScan,
    Statlog, Statpin, Hold, Base, Spawn, Heroic, Pool, Copies, Unique, PoolKill, Killgate, KillgateSet,
    Draws, DrawsSet, DrawsPush, PoolDraw,
};

// `draws push <c> [ss|below]`'s side: which of `pool draw`'s two bests each piece's stand-in
// is. No side word is `SsFirst`, the rule as first built (kDrawLeverPrefersSs below: SS, the
// other side as a named fallback); `ss` and `below` take only that side, with no fallback, so
// a class with no candidate there pushes nothing. `below` is the keep control: a below-SS
// entry passes the draw's own SS check whenever the draw keeps a pushed entry at all.
enum class DrawSide : int { SsFirst, Ss, Below };

inline const char* DrawSideText(DrawSide s)
{
    switch (s) {
    case DrawSide::Ss: return "ss";
    case DrawSide::Below: return "below";
    case DrawSide::SsFirst: break;
    }
    return "ss-first";
}

struct Command {
    Form        form = Form::Status;
    Lever       lever;                     // Set, Scale, Move, Stat, Hold, Statpin
    int64_t     frames = 0;                // Watch, Count, Pos, Statlog
    int         count = 0;                 // Args: calls to print; WalkScan: points; Heroic, Unique: calls; Pool:
                                           // the sub-list (Mark1Drops' default when not given); Copies: the copies;
                                           // KillgateSet: the lever, 0 or 1; DrawsSet: the watch,
                                           // 0 or 1; DrawsPush: the copies, 0 for off
    Scope       scope = Scope::Player;     // Watch
    bool        after = false;             // Watch: start at the row's next local-player call
    Row         afterRow = Row::CheckTalentUse;
    std::string object;                    // Count
    double      a = 0, b = 0;              // WalkHere dx dy, WalkAt x y, WalkScan deg step
    long long   statId = 0;                // Statlog
    long long   itemClass = 0;             // Base: the class (hs_game_sdk ItemType) and the base b
    long long   base = 0;
    std::string piece;                     // Spawn: helmet, body, boots, gloves or set
    DrawSide    side = DrawSide::SsFirst;  // DrawsPush: the stand-ins' side, SsFirst with no side word
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

inline const char* kFormError = "a target is <scope>.<name>[<i>], input[<i>], effect[<i>] or pst[<i>], not ";

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
        // An element of the scope's own array: input[<i>], effect[<i>], pst[<i>].
        const struct { std::string_view head; Scope scope; std::string_view member; } kArrays[] = {
            { "input[", Scope::Input, kInputMember }, { "effect[", Scope::Effect, kEffectMember }, { "pst[", Scope::Pst, kPstMember },
        };
        for (const auto& a : kArrays) {
            if (s.size() < a.head.size() || !SameWord(s.substr(0, a.head.size()), a.head)) continue;
            long long index = 0;
            if (!ParseIndexSuffix(s, a.head.size() - 1, index)) { why = "an index is a whole number 0..9999, not " + std::string(s); return false; }
            t = Target{ a.scope, std::string(a.member), true, index };
            return true;
        }
        why = kFormError + std::string(s);
        return false;
    }
    Scope scope = Scope::Player;
    const std::string_view head = s.substr(0, dot);
    if (!ParseScope(head, scope)) { why = "unknown scope " + std::string(head) + " (" + kScopeList + ")"; return false; }
    if (scope == Scope::Input || scope == Scope::Effect) { why = kFormError + std::string(s); return false; }
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
inline constexpr const char* kSpawnUsage = "usage: spawn <helmet|body|boots|gloves|set>";

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
        if (!ParseScope(tok[1], c.scope)) { why = "unknown scope " + tok[1] + " (" + kScopeList + ")"; return false; }
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
    if (head == "hold") {
        const char* usage = "usage: hold <scope>.<name>[<i>] <value|x<mult>> during <row> [frames]";
        if ((n != 5 && n != 6) || !SameWord(tok[3], "during")) { why = usage; return false; }
        Lever& l = c.lever;
        l.kind = LeverKind::Hold;
        if (!detail::ParseTarget(tok[1], l.target, why)) return false;
        const std::string& v = tok[2];
        if (v.size() > 1 && Lower(v[0]) == 'x') {
            if (!detail::ParseMult(std::string_view(v).substr(1), l.mult, why)) return false;
            l.holdMult = true;
        } else if (!ParseValue(v, l.value)) {
            why = "a hold value is a number, true, false or x<mult>, not " + v;
            return false;
        }
        if (!detail::ParseRowArg(tok[4], l.row, why)) return false;
        if (n == 6 && !detail::ParseFrames(tok[5], l.frames, why)) return false;
        c.form = Form::Hold;
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
    if (head == "statpin") {
        if (n != 2 && n != 3) { why = "usage: statpin <id> [frames]"; return false; }
        Lever& l = c.lever;
        l.kind = LeverKind::Pin;
        l.row = Row::ReturnSpecificStat;
        if (!ParseWhole(tok[1], 0, kMaxStatId, l.statId)) { why = "stat id must be a whole number 0..999, not " + tok[1]; return false; }
        if (n == 3 && !detail::ParseFrames(tok[2], l.frames, why)) return false;
        c.form = Form::Statpin;
        return true;
    }
    if (head == "statlog") {
        if (n != 3) { why = "usage: statlog <id> <frames>"; return false; }
        if (!ParseWhole(tok[1], 0, kMaxStatId, c.statId)) { why = "stat id must be a whole number 0..999, not " + tok[1]; return false; }
        if (!detail::ParseFrames(tok[2], c.frames, why)) return false;
        c.form = Form::Statlog;
        return true;
    }
    if (head == "base") {
        if (n != 3) { why = "usage: base <class 0-19> <b>"; return false; }
        if (!ParseWhole(tok[1], 0, kMaxItemClass, c.itemClass)) { why = "class must be a whole number 0..19, not " + tok[1]; return false; }
        if (!ParseWhole(tok[2], 0, kMaxBase, c.base)) { why = "b must be a whole number 0..9999, not " + tok[2]; return false; }
        c.form = Form::Base;
        return true;
    }
    if (head == "spawn") {
        if (n != 2) { why = kSpawnUsage; return false; }
        const std::string word = LowerText(tok[1]);
        Mark1Set::Piece piece = Mark1Set::Piece::Helmet;
        if (word != "set" && !Mark1Set::PieceFromKey(word, piece)) { why = std::string(kSpawnUsage) + ", not " + tok[1]; return false; }
        c.piece = word;
        c.form = Form::Spawn;
        return true;
    }
    if (head == "heroic") {
        const std::string usage = "usage: heroic <n>, n a whole number 1.." + std::to_string(kMaxHeroicCalls);
        if (n != 2) { why = usage; return false; }
        long long v = 0;
        if (!ParseWhole(tok[1], 1, kMaxHeroicCalls, v)) { why = usage + ", not " + tok[1]; return false; }
        c.count = static_cast<int>(v);
        c.form = Form::Heroic;
        return true;
    }
    if (head == "unique") {
        const std::string usage = "usage: unique <n>, n a whole number 1.." + std::to_string(kMaxUniqueCalls);
        if (n != 2) { why = usage; return false; }
        long long v = 0;
        if (!ParseWhole(tok[1], 1, kMaxUniqueCalls, v)) { why = usage + ", not " + tok[1]; return false; }
        c.count = static_cast<int>(v);
        c.form = Form::Unique;
        return true;
    }
    if (head == "pool") {
        const std::string usage = "usage: pool [<k>|kill|draw], k a sub-list 0.." + std::to_string(Mark1Drops::kListCount - 1);
        if (n != 1 && n != 2) { why = usage; return false; }
        if (n == 2 && SameWord(tok[1], "kill")) { c.form = Form::PoolKill; return true; }
        if (n == 2 && SameWord(tok[1], "draw")) { c.form = Form::PoolDraw; return true; }
        long long v = Mark1Drops::kHeroicDefaultListIndex;
        if (n == 2 && !ParseWhole(tok[1], 0, Mark1Drops::kListCount - 1, v)) { why = usage + ", not " + tok[1]; return false; }
        c.count = static_cast<int>(v);
        c.form = Form::Pool;
        return true;
    }
    if (head == "copies") {
        const std::string usage = "usage: copies <k>, k a whole number 1.." + std::to_string(Mark1Drops::kMaxCopies);
        if (n != 2) { why = usage; return false; }
        long long v = 0;
        if (!ParseWhole(tok[1], 1, Mark1Drops::kMaxCopies, v)) { why = usage + ", not " + tok[1]; return false; }
        c.count = static_cast<int>(v);
        c.form = Form::Copies;
        return true;
    }
    if (head == "killgate") {
        const std::string usage = "usage: killgate [0|1]";
        if (n == 1) { c.form = Form::Killgate; return true; }
        if (n != 2) { why = usage; return false; }
        long long v = 0;
        if (!ParseWhole(tok[1], 0, 1, v)) { why = usage + ", not " + tok[1]; return false; }
        c.count = static_cast<int>(v);
        c.form = Form::KillgateSet;
        return true;
    }
    if (head == "draws") {
        const char* usage = "usage: draws [on|off|push <c> [ss|below]|push off]";
        if (n == 1) { c.form = Form::Draws; return true; }
        const std::string word = LowerText(tok[1]);
        if (n == 2 && (word == "on" || word == "off")) {
            c.count = word == "on" ? 1 : 0;
            c.form = Form::DrawsSet;
            return true;
        }
        if ((n == 3 || n == 4) && word == "push") {
            c.form = Form::DrawsPush;
            if (n == 3 && SameWord(tok[2], "off")) { c.count = 0; return true; }
            const std::string pushUsage = "usage: draws push <c> [ss|below]|off, c a whole number 1.." + std::to_string(Mark1Drops::kMaxCopies);
            long long v = 0;
            if (!ParseWhole(tok[2], 1, Mark1Drops::kMaxCopies, v)) { why = pushUsage + ", not " + tok[2]; return false; }
            c.count = static_cast<int>(v);
            if (n == 4) {
                const std::string side = LowerText(tok[3]);
                if (side == "ss") c.side = DrawSide::Ss;
                else if (side == "below") c.side = DrawSide::Below;
                else { why = pushUsage + ", the side ss or below, not " + tok[3]; return false; }
            }
            return true;
        }
        why = usage;
        return false;
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
        if (!ParseWhole(tok[4], 1, kScanMaxCount, count)) { why = "scan count must be a whole number 1..64, not " + tok[4]; return false; }
        c.count = static_cast<int>(count);
        c.form = Form::WalkScan;
        return true;
    }
    why = "unknown form " + tok[0]
        + " (hook, off, show, args, watch, set, scale, hold, move, stat, statpin, statlog, count, pos, base, spawn, heroic,"
          " unique, pool, copies, killgate, draws or walkable)";
    return false;
}

inline std::vector<std::string> Usage()
{
    return {
        "  mark1probe hook                                      install the script and builtin rows as pass-throughs",
        "  mark1probe off                                       every lever, watch and statlog off (rows stay installed)",
        "  mark1probe show [reset]                              per-row calls local/all, the lever's counts, casts per talent id",
        "  mark1probe args <n>                                  the next n CheckTalentUse calls' arguments",
        "  mark1probe watch <scope> <frames> [after <row>]      changed members; scopes player pst profile input queue effect builtin",
        "  mark1probe set <scope>.<name>[<i>] <v> during <row> [frames]   write before each call, restore after",
        "  mark1probe scale <scope>.<name>[<i>] <m> during <row> [frames] multiply before each call, restore after",
        "  mark1probe hold <scope>.<name>[<i>] <v|x<m>> during <row> [frames] write before each call, restore once at the end",
        "  mark1probe move <row> <mult> [frames]                scale each call's own displacement",
        "  mark1probe stat <id> <mult> [frames]                 scale one stat id's number (dispatcher, and 64/66's getter)",
        "  mark1probe statpin <id> [frames]                     answer the id with its last local answer before the lever",
        "  mark1probe statlog <id> <frames>                     each local answer for the id, the rows around it, min and max",
        "  mark1probe count <object name> <frames>              instance_number per frame, one line per change",
        "  mark1probe pos <frames>                              the cursor, each moving call's dx dy, moves outside the rows, mode, ms",
        "  mark1probe base <class> <b>                          build a scratch item of that class and base (never placed)",
        "  mark1probe spawn <helmet|body|boots|gloves|set>      build Mark 1 pieces through the loader and place them at the player",
        "  mark1probe heroic <n>                                call DropItemHeroic n times (1..20) at the player; rarities and pieces",
        "  mark1probe unique <n>                                call DropUniqueItems n times (1..20) at the player; rarities and pieces",
        "  mark1probe pool [<k>]                                lootListUnique[k] (5 by default) as the Heroic roll keeps it, each piece's stand-in",
        "  mark1probe pool kill                                 lootListUnique[0..4] as the unique roll keeps them, each piece's kill stand-in",
        "  mark1probe copies <k>                                each piece's stand-in pushed k times (1..400) per roll, Heroic and unique",
        "  mark1probe killgate [0|1]                            open LoadDrops' type-45 gate once per kill inside the game's call; alone, its counts",
        "  mark1probe draws [on|off]                            watch DropItem's own unique draw: each build's scope, the call's tier; alone, its counts",
        "  mark1probe draws push <c> [ss|below]|off             push each Mark 1 kill stand-in c times (1..400) onto lootListUnique[0..5] per DropItem call; ss or below: that side's only",
        "  mark1probe pool draw                                 per class (helmet, body, boots, gloves, elf) the candidates DropItem's draw keeps",
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

// ---- `base` ------------------------------------------------------------------------
// A scratch item of one class and base, built by the game's own loader the
// way charmprobe's scratch charm is (json_parse, then InitItemFromJson with
// the global instance as self; hs-game-sdk/curated/item_info.json `loader`)
// and never placed: the collector takes a build nothing keeps. Its seed is
// charmprobe's scratch seed, outside every range a ForgePact item dresses,
// so the Mark 1 pieces' own seeds never reach this build.
inline constexpr long long kBaseScratchSeed = 779000;

// The loader's key: `0-0-<ms>-<class>`, the class last (as `angelicdrop` and
// charmprobe's `0-0-<ms>-10` build theirs).
inline std::string BaseKey(long long ms, long long itemClass)
{
    return "0-0-" + std::to_string(ms) + "-" + std::to_string(itemClass);
}

// The definition: the signature items' shape, no unique (`j` 0), base `b`.
inline std::string BaseJson(long long b)
{
    return "{\"w\":1,\"a\":" + std::to_string(kBaseScratchSeed) + ",\"j\":0,\"b\":" + std::to_string(b) + ",\"c\":0,\"o\":1}";
}

// What the build gave. `built` is a struct with an itemInfoStruct and a
// non-empty name (info key 28); otherwise `why` says which part was missing.
// rarity is info key 27, the display sprite info key 11 (item_info.json's
// info_keys, measured), and `spriteExists` sprite_exists' answer on it.
struct BaseResult {
    long long             itemClass = 0;
    long long             base = 0;
    bool                  built = false;
    std::string           why;
    std::string           name;
    std::optional<double> rarity;
    std::optional<double> sprite;
    std::optional<bool>   spriteExists;
};

inline std::string BaseLine(const BaseResult& r)
{
    std::string out = "mark1probe base: class=" + std::to_string(r.itemClass) + " b=" + std::to_string(r.base)
        + " built=" + (r.built ? "yes" : "no");
    if (!r.why.empty()) out += " why=" + r.why;
    out += " rarity=" + (r.rarity ? NumberText(*r.rarity) : std::string("none"));
    out += " sprite=" + (r.sprite ? NumberText(*r.sprite) : std::string("none"));
    if (r.spriteExists) out += std::string(" sprite-exists=") + (*r.spriteExists ? "yes" : "no");
    out += " name=" + (r.name.empty() ? std::string("none") : "\"" + r.name + "\"");
    return out;
}

// ---- `spawn` -----------------------------------------------------------------------
// The Mark 1 pieces, built the way the boss soul pieces are (json_parse, then
// InitItemFromJson with the global instance as self, which keeps the seed;
// then LootGroundCreateFromItem at the player), each from its row of
// Mark1Set.hpp's table: its class in the loader's key, its seed `a` and base
// `b` in the definition. The game's own CreateItemNew dresses it on the way,
// as it dresses a piece loaded from a save; `dressed=` reads whether it did.

// The pieces one `spawn` word places: one, or all four in slot order.
inline std::vector<Mark1Set::Piece> SpawnPieces(std::string_view word)
{
    if (word == "set") return { Mark1Set::Piece::Helmet, Mark1Set::Piece::Body, Mark1Set::Piece::Boots, Mark1Set::Piece::Gloves };
    Mark1Set::Piece p = Mark1Set::Piece::Helmet;
    if (Mark1Set::PieceFromKey(word, p)) return { p };
    return {};
}

// A piece's definition: the signature items' shape, no unique (`j` 0).
inline std::string PieceJson(long long a, long long b)
{
    return "{\"w\":1,\"a\":" + std::to_string(a) + ",\"j\":0,\"b\":" + std::to_string(b) + ",\"c\":0,\"o\":1}";
}

// What one piece's spawn gave. t, b and a are read back from the built item
// (its itemType and its definition), `dressed` is whether it carries
// fp_mechanic = mark1, `tier` is its info key 32 (5 SS once dressed; `?` when
// unread), and `ground` is what LootGroundCreateFromItem returned; with no
// ground, `why` says which step refused.
struct SpawnResult {
    std::string                piece;
    bool                       built = false;
    std::optional<double>      t, b, a;
    bool                       dressed = false;
    std::optional<double>      tier;
    std::optional<std::string> ground;
    std::string                why;
};

inline std::string SpawnLine(const SpawnResult& r)
{
    auto num = [](const std::optional<double>& v) { return v ? NumberText(*v) : std::string("?"); };
    std::string out = "mark1probe spawn: piece=" + r.piece;
    if (!r.built) return out + " built=no why=" + (r.why.empty() ? std::string("?") : r.why);
    out += " t=" + num(r.t) + " b=" + num(r.b) + " a=" + num(r.a) + " dressed=" + (r.dressed ? "yes" : "no")
        + " tier=" + num(r.tier);
    if (r.ground) return out + " ground=" + *r.ground;
    return out + " ground=none why=" + (r.why.empty() ? std::string("?") : r.why);
}

// ---- `heroic` ----------------------------------------------------------------------
// DropItemHeroic called by name with the local player as self and its position
// as the first two arguments (charmprobe heroic's measured call shape), n times;
// what each call placed is read off the newest ground item. A piece is
// recognised by its own fields (the adapter's Mark1PieceOf, Mark1Set.hpp's
// IsPiece), never by its name.

// What one call gave: nothing placed (`why` says what the adapter saw), or the
// placed item's class, base, c, rarity (info key 27), tier (info key 32; `?`
// when unread), name (info key 28) and whether it is a piece.
struct HeroicCall {
    bool                   placed = false;
    std::string            why;
    std::optional<double>  t, b, c, rarity, tier;
    std::string            name;
    bool                   isPiece = false;
    Mark1Set::Piece        piece = Mark1Set::Piece::Helmet;
};

// `unique <n>` (the kill route) calls DropUniqueItems the same way and prints the
// same lines under its own form's name: `mark1probe unique #<i>: ...` and
// `mark1probe unique: calls=...`.
inline std::string CallLine(std::string_view form, int k, const HeroicCall& r)
{
    auto num = [](const std::optional<double>& v) { return v ? NumberText(*v) : std::string("?"); };
    std::string out = "mark1probe " + std::string(form) + " #" + std::to_string(k) + ": ";
    if (!r.placed) return out + "nothing placed (" + (r.why.empty() ? std::string("?") : r.why) + ")";
    out += "placed t=" + num(r.t) + " b=" + num(r.b) + " c=" + num(r.c) + " rarity=" + num(r.rarity)
        + " tier=" + num(r.tier) + " name=" + (r.name.empty() ? std::string("none") : "\"" + r.name + "\"");
    out += " piece=" + (r.isPiece ? std::string(Mark1Set::RowOf(r.piece).key) : std::string("none"));
    return out;
}

inline std::string HeroicCallLine(int k, const HeroicCall& r) { return CallLine("heroic", k, r); }
inline std::string UniqueCallLine(int k, const HeroicCall& r) { return CallLine("unique", k, r); }

// The summary after the calls: `calls=`, `placed=`, the rarity histogram
// (`rarity=<r>:<count>,...`, rising, `-` when none was read), the placed items
// whose rarity could not be read, and the pieces among what was placed. Under
// the form's own name (`heroic` by default, `unique` for the kill route's).
class HeroicTally {
public:
    HeroicTally() = default;
    explicit HeroicTally(std::string_view form) : form_(form) {}
    void Add(const HeroicCall& r)
    {
        ++calls_;
        if (!r.placed) return;
        ++placed_;
        long long whole = 0;
        if (r.rarity && Mark1Set::AsWhole(*r.rarity, whole)) ++rarities_[whole];
        else ++rarityUnread_;
        if (r.isPiece) ++pieces_[static_cast<size_t>(r.piece)];
    }
    int Calls() const { return calls_; }
    int Placed() const { return placed_; }
    int Pieces(Mark1Set::Piece p) const { return pieces_[static_cast<size_t>(p)]; }
    std::string Line() const
    {
        std::string rarity;
        for (const auto& [r, count] : rarities_) rarity += (rarity.empty() ? "" : ",") + std::to_string(r) + ":" + std::to_string(count);
        std::string pieces;
        for (const Mark1Set::PieceRow& row : Mark1Set::kPieces)
            pieces += (pieces.empty() ? "" : ",") + std::string(row.key) + ":" + std::to_string(pieces_[static_cast<size_t>(row.piece)]);
        return "mark1probe " + form_ + ": calls=" + std::to_string(calls_) + " placed=" + std::to_string(placed_) + " rarity="
            + (rarity.empty() ? std::string("-") : rarity) + " rarity-unread=" + std::to_string(rarityUnread_) + " pieces=" + pieces
            + " self=player";
    }

private:
    std::string form_ = "heroic";
    int calls_ = 0;
    int placed_ = 0;
    int rarityUnread_ = 0;
    std::map<long long, int> rarities_;
    std::array<int, Mark1Set::kPieceCount> pieces_{};
};

// ---- `pool` ------------------------------------------------------------------------
// One sub-list read the way the drops read it (Mark1Drops.hpp's ListEntry, and
// each distinct triple's repository verdict once, by name, outside a roll):
// its length, what the Heroic roll keeps of it (`droppable=`, T_H, the entries
// whose triple it keeps, repeats counted), by rarity, the entries it draws past
// (refused rarities, hidden, unread, not a triple), and per piece class the
// core's stand-in and its class's kept triples and entries.
struct PoolSummary {
    int                     index = Mark1Drops::kHeroicDefaultListIndex;
    int                     length = 0;
    int                     droppable = 0;
    int                     hidden = 0;
    int                     unread = 0;   // entries whose triple's verdict could not be read
    int                     other = 0;    // entries that are not a triple of whole numbers
    std::map<int, int>      kept;         // rarity -> entries kept
    std::map<int, int>      refused;      // rarity -> entries drawn past (7, 10)
    Mark1Drops::Resolution  resolution;
};

inline PoolSummary Pool(int index, const std::vector<Mark1Drops::ListEntry>& list, const Mark1Drops::VerdictFn& verdict)
{
    PoolSummary s;
    s.index = index;
    s.length = static_cast<int>(list.size());
    struct Seen { bool read = false; int rarity = 0; bool hidden = false; };
    std::map<Mark1Drops::Triple, Seen> verdicts;
    // Each distinct triple is asked once; the stand-in rule below asks the same memo.
    auto ask = [&](const Mark1Drops::Triple& k, int& rarity, bool& hidden) {
        auto it = verdicts.find(k);
        if (it == verdicts.end()) {
            Seen v;
            v.read = verdict && verdict(k, v.rarity, v.hidden);
            it = verdicts.emplace(k, v).first;
        }
        rarity = it->second.rarity;
        hidden = it->second.hidden;
        return it->second.read;
    };
    for (const Mark1Drops::ListEntry& e : list) {
        Mark1Drops::Triple k;
        if (!e.triple || !Mark1Set::AsWhole(e.t, k.t) || !Mark1Set::AsWhole(e.sub, k.sub) || !Mark1Set::AsWhole(e.b, k.b)) {
            ++s.other;
            continue;
        }
        int rarity = 0;
        bool hidden = false;
        if (!ask(k, rarity, hidden)) { ++s.unread; continue; }
        if (hidden) { ++s.hidden; continue; }
        if (!Mark1Drops::HeroicKeeps(rarity, hidden)) { ++s.refused[rarity]; continue; }
        ++s.droppable;
        ++s.kept[rarity];
    }
    s.resolution = Mark1Drops::ResolveStandIns(index, list, ask);
    return s;
}

inline std::vector<std::string> PoolLines(const PoolSummary& s)
{
    auto histogram = [](const std::map<int, int>& m) {
        std::string out;
        for (const auto& [r, count] : m) out += (out.empty() ? "" : ",") + std::to_string(r) + ":" + std::to_string(count);
        return out.empty() ? std::string("-") : out;
    };
    std::vector<std::string> lines;
    lines.push_back("mark1probe pool: list=" + std::to_string(s.index) + " length=" + std::to_string(s.length) + " droppable="
                    + std::to_string(s.droppable) + " kept=" + histogram(s.kept) + " refused=" + histogram(s.refused) + " hidden="
                    + std::to_string(s.hidden) + " unread=" + std::to_string(s.unread) + " other=" + std::to_string(s.other));
    for (const Mark1Set::PieceRow& row : Mark1Set::kPieces) {
        const size_t i = static_cast<size_t>(row.piece);
        lines.push_back("mark1probe pool: " + std::string(row.key) + " class=" + std::to_string(row.itemType) + " standin="
                        + Mark1Drops::StandInText(s.resolution.standIns[i]) + " kept-triples="
                        + std::to_string(s.resolution.kept[i].keptTriples) + " kept-entries="
                        + std::to_string(s.resolution.kept[i].keptEntries));
    }
    return lines;
}

// ---- `pool kill` -------------------------------------------------------------------
// The six sub-lists read the way the kill route reads them (Mark1Drops.hpp's
// KillLists, each distinct triple's verdict once, by name, outside a roll), then
// the core's kill stand-in rule: per piece class the chosen stand-in, its count
// on each of [0..4], and how many of the class's kept triples have one count on
// all five (`candidates=`). The first line gives [0..4]'s lengths, the share's
// denominators.
struct PoolKillSummary {
    Mark1Drops::KillResolution resolution;
};

inline PoolKillSummary PoolKill(const Mark1Drops::KillLists& lists, const Mark1Drops::VerdictFn& verdict)
{
    PoolKillSummary s;
    std::map<Mark1Drops::Triple, std::pair<bool, std::pair<int, bool>>> memo;
    auto ask = [&](const Mark1Drops::Triple& k, int& rarity, bool& hidden) {
        auto it = memo.find(k);
        if (it == memo.end()) {
            int r = 0;
            bool h = false;
            const bool read = verdict && verdict(k, r, h);
            it = memo.emplace(k, std::make_pair(read, std::make_pair(r, h))).first;
        }
        rarity = it->second.second.first;
        hidden = it->second.second.second;
        return it->second.first;
    };
    s.resolution = Mark1Drops::ResolveKillStandIns(lists, ask);
    return s;
}

inline std::vector<std::string> PoolKillLines(const PoolKillSummary& s)
{
    const Mark1Drops::KillResolution& r = s.resolution;
    std::string lengths;
    for (int k = 0; k < Mark1Drops::kKillListCount; ++k)
        lengths += (k ? "," : "") + std::to_string(r.lengths[static_cast<size_t>(Mark1Drops::kKillLists[k])]);
    std::vector<std::string> lines;
    lines.push_back("mark1probe pool: kill lengths=" + lengths + " unread=" + std::to_string(r.unread));
    for (const Mark1Set::PieceRow& row : Mark1Set::kPieces) {
        const size_t i = static_cast<size_t>(row.piece);
        const Mark1Drops::StandIn& standIn = r.standIns[i];
        std::string counts = "-";
        if (standIn.resolved) {
            counts.clear();
            for (int k = 0; k < Mark1Drops::kKillListCount; ++k) counts += (k ? "," : "") + std::to_string(r.counts[i][static_cast<size_t>(k)]);
        }
        lines.push_back("mark1probe pool: " + std::string(row.key) + " class=" + std::to_string(row.itemType) + " kill-standin="
                        + Mark1Drops::StandInText(standIn) + " counts=" + counts + " candidates=" + std::to_string(r.qualifying[i])
                        + " kept-triples=" + std::to_string(r.kept[i].keptTriples));
    }
    return lines;
}

// ---- `copies` ----------------------------------------------------------------------

inline std::string CopiesLine(int copies)
{
    return "mark1probe copies: each piece's stand-in is pushed " + std::to_string(copies)
        + " time(s) per roll, Heroic and unique (the player build pushes " + std::to_string(Mark1Drops::kPlayerCopies) + ")";
}

// ---- `killgate` (Mod live procedure 6) ----------------------------------------------
//
// The owner's route (a), 2026-10-20: a lever that opens LoadDrops' type-45 ("a unique
// item") gate on kills, so that a kill runs the game's own DropUniqueItems call from
// LoadDrops and the live session can read the kill route's direct call. It exists only
// in the research build, like everything in this header, and it is off until `killgate 1`.
//
// It works only inside the game's own LoadDrops call (the adapter's LoadDrops detour,
// around the trampoline), in one of two forms, at most once per chances array:
//
//   - raise: the game itself calls LoadDrops with type 45 on an array not handled yet;
//     chances[45] is opened for that call and put back after it;
//   - re-enter: the first game call on an array not handled yet, of any other type,
//     runs the game's call untouched, then calls the trampoline once more at type 45
//     with chances[45] opened, and puts the value back.
//
// One DropItem passes one array to all its LoadDrops calls (RDM § 13.1), so this is at
// most one opened type-45 roll per kill. What calls DropUniqueItems is LoadDrops' own
// code, and the draw, the filter and the placement are the game's: the lever never
// calls DropUniqueItems and never rolls a die. The opened value goes back on every way
// out, a throw included, and a call nested inside an opened one passes untouched.
//
// With the lever off every call passes through with nothing written and nothing
// re-entered; the counters still move, so `c45-nonzero=` and `game45=` answer how often
// an unlevered kill reaches type 45 at all.

inline constexpr int kKillgateType = 45;                  // LoadDrops' "a unique item" (RDM § 13.1)
inline constexpr double kKillgateOpenChance = 100000.0;   // above the gate's die (about 100 outcomes, RDM § 13.1),
                                                          // the value `dungeonkey typemap` opens a type with

// How the LoadDrops detour went in, as the lever reads it: `detoured` is the only one
// the lever works on, since a table-only hook misses DropItem's direct call.
enum class KillgateHook : int { None, Detoured, TableOnly, Refused };

inline const char* KillgateHookText(KillgateHook h)
{
    switch (h) {
    case KillgateHook::Detoured: return "detoured";
    case KillgateHook::TableOnly: return "table-only";
    case KillgateHook::Refused: return "refused";
    case KillgateHook::None: break;
    }
    return "none";
}

// The route from what the adapter knows: whether its own install was tried and failed,
// whether a LoadDrops hook exists, and whether that hook's install put the inline detour in.
inline KillgateHook KillgateHookOf(bool installFailed, bool installed, bool native)
{
    if (installed) return native ? KillgateHook::Detoured : KillgateHook::TableOnly;
    return installFailed ? KillgateHook::Refused : KillgateHook::None;
}

struct KillgateCounts {
    uint64_t loaddrops = 0;      // every call the detour sees
    uint64_t arrays = 0;         // distinct chances arrays
    uint64_t c45Nonzero = 0;     // of those, the ones whose vanilla chances[45] is not 0
    double   c45Max = 0;         // the largest vanilla chances[45] read
    uint64_t game45 = 0;         // the game's own type-45 calls
    uint64_t raised = 0;         // game type-45 calls run with the chance opened
    uint64_t reentered = 0;      // type-45 re-entries
    uint64_t uniqueDuring = 0;   // the shared DropUniqueItems detour's calls inside opened calls
    uint64_t threw = 0;          // opened calls that threw
};

class Killgate {
public:
    bool On() const { return on_; }
    const KillgateCounts& Counts() const { return counts_; }

    // `killgate 0|1`. Refused, with nothing changed, unless the hook is the inline detour.
    std::string Set(bool on, KillgateHook hook)
    {
        if (hook != KillgateHook::Detoured)
            return std::string("mark1probe killgate: refused - the LoadDrops hook is ") + KillgateHookText(hook)
                + ", and only the inline detour sees DropItem's direct call; the lever stays "
                + (on_ ? "on" : "off") + ", nothing changed";
        on_ = on;
        return on ? "mark1probe killgate: lever on - the first LoadDrops call of each kill opens type 45 once, inside the game's own call"
                  : "mark1probe killgate: lever off - every LoadDrops call is the game's own (still counted)";
    }

    std::string StatLine(KillgateHook hook) const
    {
        const KillgateCounts& c = counts_;
        return std::string("mark1probe killgate: lever=") + (on_ ? "on" : "off") + " hook=" + KillgateHookText(hook)
            + " loaddrops=" + std::to_string(c.loaddrops) + " arrays=" + std::to_string(c.arrays)
            + " c45-nonzero=" + std::to_string(c.c45Nonzero) + " c45-max=" + NumberText(c.c45Max)
            + " game45=" + std::to_string(c.game45) + " raised=" + std::to_string(c.raised)
            + " reentered=" + std::to_string(c.reentered) + " unique-during=" + std::to_string(c.uniqueDuring)
            + " threw=" + std::to_string(c.threw);
    }

    // One LoadDrops call the detour sees. `type` is its drop type; `hasArray` and `arrayId`
    // name its chances array (the identity of the array value, within `frame`). The ops:
    //   bool ReadChance(double&)  chances[45] as it is now
    //   bool WriteChance(double)  chances[45] := v
    //   void CallGame()           the game's own call, its own arguments, through the trampoline
    //   void CallReentry()        the trampoline once more, the same arguments, type 45
    //   uint64_t UniqueCalls()    the shared DropUniqueItems detour's `kill-calls=`
    // CallGame runs exactly once on every path; a throw from it is passed on.
    template <class Ops>
    void Around(int type, bool hasArray, uint64_t arrayId, int64_t frame, Ops& ops)
    {
        ++counts_.loaddrops;
        if (depth_ > 0) { ops.CallGame(); return; }   // inside an opened call: never a second opening
        bool fresh = false;
        bool readable = false;
        double vanilla = 0;
        if (hasArray) {
            if (frame != frame_) { seen_.clear(); frame_ = frame; }
            fresh = true;
            for (uint64_t id : seen_) fresh = fresh && id != arrayId;
            if (fresh) {
                seen_.push_back(arrayId);
                ++counts_.arrays;
                readable = ops.ReadChance(vanilla) && std::isfinite(vanilla);
                if (readable && vanilla != 0) {
                    ++counts_.c45Nonzero;
                    if (vanilla > counts_.c45Max) counts_.c45Max = vanilla;
                }
            }
        }
        if (type == kKillgateType) ++counts_.game45;
        if (!on_ || !fresh || !readable) { ops.CallGame(); return; }

        if (type == kKillgateType) {
            // raise: the game's own type-45 call, with the chance opened for its length.
            if (!ops.WriteChance(kKillgateOpenChance)) {
                ops.WriteChance(vanilla);   // an opening not confirmed is put back before the game's call
                ops.CallGame();
                return;
            }
            ++counts_.raised;
            Opened<Ops> opened(*this, ops, vanilla);
            PassingThrow passing(counts_.threw);   // the game's own throw goes on, uncaught here
            ops.CallGame();
            return;
        }

        // re-enter: the game's call untouched first, then type 45 once with the chance opened.
        ops.CallGame();
        if (!ops.WriteChance(kKillgateOpenChance)) { ops.WriteChance(vanilla); return; }
        ++counts_.reentered;
        Opened<Ops> opened(*this, ops, vanilla);
        try {
            ops.CallReentry();
        } catch (...) {
            ++counts_.threw;
        }
    }

private:
    // An opened call's scope: the nesting, `unique-during=`'s start, and the vanilla value
    // put back on every way out.
    template <class Ops>
    struct Opened {
        Killgate& gate;
        Ops& ops;
        double vanilla;
        uint64_t uniqueAtStart;
        Opened(Killgate& g, Ops& o, double v) : gate(g), ops(o), vanilla(v), uniqueAtStart(o.UniqueCalls()) { ++gate.depth_; }
        ~Opened()
        {
            const uint64_t now = ops.UniqueCalls();
            if (now > uniqueAtStart) gate.counts_.uniqueDuring += now - uniqueAtStart;
            ops.WriteChance(vanilla);
            --gate.depth_;
        }
        Opened(const Opened&) = delete;
        Opened& operator=(const Opened&) = delete;
    };

    // Counts `threw=` for an exception leaving its scope without catching it, so the game's
    // own call's throw reaches the adapter's detour, which writes the rethrow line, unchanged.
    struct PassingThrow {
        uint64_t& threw;
        int unwinding;
        explicit PassingThrow(uint64_t& t) : threw(t), unwinding(std::uncaught_exceptions()) {}
        ~PassingThrow() { if (std::uncaught_exceptions() > unwinding) ++threw; }
        PassingThrow(const PassingThrow&) = delete;
        PassingThrow& operator=(const PassingThrow&) = delete;
    };

    bool on_ = false;
    int depth_ = 0;
    int64_t frame_ = -1;
    std::vector<uint64_t> seen_;   // the arrays handled this frame
    KillgateCounts counts_;
};

// ---- `draws` (Mod live procedure 7) --------------------------------------------------
//
// A static reading (hub RDM § 13.12) has DropItem draw a unique itself: from
// Controller_obj.lootListUnique[k], k being what LoadSatanicDropTier answered in the same
// call, with one GetUniqueRepoStruct read of the entry and one CreateDefaultParams call
// before the build. Whether a kill's unique really comes from there is not established,
// and this watch is the instrument that asks it. Research build only, like everything in
// this header, and off until `draws on`.
//
// While it is on, every item the Custom Forge's final pass sees built is counted
// (`builds=`, `normal=` for the normal repository's, c 0) and every unique (c 1) is
// classed by the innermost scope it was built in: `draw` inside a DropItem call and
// outside every nested roll, `dropunique` inside the shared DropUniqueItems detour,
// `angelic` inside #74's Angelic roll, `heroic` inside Mark 1's Heroic hook, `outside`
// with none of them in progress. Each unique gets one `built` line (200 a session): its
// scope, the tier its DropItem call drew (`k=`), its triple's count on each of the six
// lists (`lists=`, the lever's entries left out) and the repository reads its scope made
// since the scope's last build (`reads=`). `on-k=` and `off-k=` count the `draw` builds
// of a game triple listed on list k at least once, or not at all.
//
// `draws push <c>` (the lever): for the length of each outermost DropItem call, c copies
// of each Mark 1 kill stand-in on each of lootListUnique[0..5]. A `draw`-scope
// CreateDefaultParams call whose pair is a pushed stand-in's is a pick (`picks=`); one built
// is a `pick-builds=`. A pick is typed against every triple the scope read since its last
// typed pick or build, not only the latest read: whether the draw keeps the first entry it
// accepts or weighs several and keeps an earlier one is not established, and typing by the
// latest read alone would leave every pick of a weighing draw untyped (picks, pick-builds,
// on-k and off-k all flat). When the pair names triples of more than one class, the pick
// counts at once only if all of them were pushed; otherwise the build settles it by its
// class. Nothing is rewritten, the
// adapter takes the copies off again on every way out (the tail rule, `anomalies=`
// otherwise), and the lever refuses while Mark 1 drops or Pocket Elf is switched on,
// since both push onto the same lists. Three counters say whether the lists the draw read
// carried the push at all: `push-held=` the outermost calls whose push read back on every
// list, `push-skipped=` those that were to push and did not (whatever the reason: a list
// unread, no stand-in, nothing planned, a push taken off again, a throw the adapter
// swallowed; the adapter notes it, the core counts it), and `pushed-reads=` the `draw`-scope
// repository reads of a triple this call pushed. `draws push <c> ss|below` picks the
// stand-ins from that side only (`push-side=`), the below side being the keep control.

inline constexpr int kDrawListCount = Mark1Drops::kListCount;   // lootListUnique[0..5]
inline constexpr int kDrawTierCount = 6;                         // LoadSatanicDropTier answers 0..5 (static reading)
inline constexpr int kDrawBuiltLineCap = 200;                    // `built` lines a session; the counters go on
inline constexpr size_t kDrawReadWindowCap = 2048;               // distinct triples a scope remembers between picks

enum class DrawScope : int { Draw, DropUnique, Angelic, Heroic, Outside };
inline constexpr int kDrawScopeCount = 5;

inline const char* DrawScopeText(DrawScope s)
{
    switch (s) {
    case DrawScope::Draw: return "draw";
    case DrawScope::DropUnique: return "dropunique";
    case DrawScope::Angelic: return "angelic";
    case DrawScope::Heroic: return "heroic";
    case DrawScope::Outside: break;
    }
    return "outside";
}

// DropItem's argument 7 (counting from 0), the list its second draw reads: absent or
// undefined, a live ds_list, or anything else.
enum class DrawArg7 : int { Undefined, List, Other };

// What the adapter's installs left, as the lines name it: DropItem's route (`detoured`,
// `TABLE-ONLY`, `not-installed` or `held-by-<row>`), LoadSatanicDropTier's one detour and
// ReturnRandomSatanic's count-only hook (`detoured`, `TABLE-ONLY`, `not-found`, or
// `not-installed` before the first `draws on`).
struct DrawRoutes {
    std::string dropitem = "not-installed";
    std::string sattier = "not-installed";
    std::string retsat = "not-installed";
};

inline constexpr const char* kDrawRouteDetoured = "detoured";

// One item the forge's final pass saw built: its class (`itemType`), its definition's b
// and c, and its info keys 27 (rarity), 32 (tier) and 28 (name); unset when unread.
struct DrawBuilt {
    std::optional<double> t, b, c, rarity, tier;
    std::string name;
};

struct DrawCounts {
    uint64_t calls = 0;                                // DropItem calls
    int      argcMin = 0, argcMax = 0;                 // over those calls
    std::array<uint64_t, 3> arg7{};                    // by DrawArg7
    std::array<uint64_t, kDrawTierCount> tiers{};      // LoadSatanicDropTier's answers
    uint64_t tierUnread = 0;                           // answers that were not a whole 0..5
    uint64_t reads = 0;                                // GetUniqueRepoStruct reads
    uint64_t builds = 0;                               // every item built
    uint64_t normal = 0;                               // of those, c 0
    std::array<uint64_t, kDrawScopeCount> scopes{};    // the uniques (c 1), by DrawScope
    uint64_t onK = 0, offK = 0;
    uint64_t retsatCalls = 0;
    uint64_t picks = 0, pickBuilds = 0;
    uint64_t pushHeld = 0;                             // outermost calls whose push read back Held
    uint64_t pushSkipped = 0;                          // outermost calls that were to push and held none
    uint64_t pushedReads = 0;                          // `draw`-scope reads of a triple the call pushed
    uint64_t anomalies = 0;
};

// What the adapter reads for one build before NoteBuilt: whether it is a unique (c 1),
// and the triple its scope's latest typed pick names when the build's class and b agree
// with one of the triples that pick matched (then `lists=` is that triple's counts).
struct DrawBuildQuery {
    bool unique = false;
    bool haveTriple = false;
    Mark1Drops::Triple triple;
};

using DrawListCounts = std::array<int, kDrawListCount>;

// The lever's first plan: c copies of each Mark 1 kill stand-in `pool kill` resolves, each
// onto every one of [0..5]. Copies below 1 push nothing, above 400 push 400. That rule is
// DropUniqueItems' (rarity below 7, field 40 clear), so it can pick an entry the draw always
// sends back (field 41 set, or a rarity its other branch takes): the lever's plan is the
// draw-candidate overload after `pool draw` below, and this one stays only for the kill
// route's own form.
inline std::vector<Mark1Drops::PushEntry> DrawPushPlan(const Mark1Drops::KillResolution& r, int copies)
{
    return Mark1Drops::PlanKillPush(r, copies, std::array<bool, Mark1Set::kPieceCount>{}).entries;
}

// How a push went: nothing planned or a list unreadable (`None`), on every list and read
// back (`Held`), or taken off again because an add threw (`Threw`) or a list's tail did not
// show it (`NotHeld`).
enum class DrawPushResult : int { None, Held, Threw, NotHeld };

// The lever's entries for one call: the plan, the entries of each plan row that actually
// went onto each list (fewer after an add that threw), and each list's size before.
// The ops, the adapter's on the six lists by name:
//   int  Length(int k)                    the list's size, -1 when it cannot be read
//   bool Add(int k, const Triple&)        one fresh entry at the end; false when it threw
//   bool TailHolds(int k, int before, const std::vector<PushEntry>&, const std::vector<int>& on, int& len)
//                                         the list ends with exactly these entries; len its size
//   int  DeleteLast(int k, int len, int n) n entries off the end, then the size (-1 on a throw)
class DrawPushBook {
public:
    int Pushed() const { return pushed_; }
    const std::vector<Mark1Drops::PushEntry>& Entries() const { return entries_; }
    // List k's size before this call's entries went on, -1 with none on.
    int Before(int k) const { return k >= 0 && k < kDrawListCount ? before_[static_cast<size_t>(k)] : -1; }

    // A push that comes off again appends Remove's lines to `lines`.
    template <class Ops>
    DrawPushResult Push(const std::vector<Mark1Drops::PushEntry>& plan, Ops& ops, std::vector<std::string>& lines)
    {
        Clear();
        if (plan.empty()) return DrawPushResult::None;
        for (int k = 0; k < kDrawListCount; ++k) {
            before_[static_cast<size_t>(k)] = ops.Length(k);
            if (before_[static_cast<size_t>(k)] < 0) { Clear(); return DrawPushResult::None; }
        }
        entries_ = plan;
        for (std::vector<int>& on : on_) on.assign(plan.size(), 0);
        bool threw = false;
        for (int k = 0; k < kDrawListCount && !threw; ++k) {
            for (size_t e = 0; e < plan.size() && !threw; ++e) {
                for (int n = 0; n < plan[e].copies && !threw; ++n) {
                    if (!ops.Add(k, plan[e].triple)) { threw = true; break; }
                    ++on_[static_cast<size_t>(k)][e];
                    ++pushed_;
                }
            }
        }
        bool held = !threw;
        for (int k = 0; k < kDrawListCount && held; ++k) {
            int len = -1;
            held = ops.TailHolds(k, before_[static_cast<size_t>(k)], entries_, on_[static_cast<size_t>(k)], len);
        }
        if (held) return DrawPushResult::Held;
        for (std::string& l : Remove(ops)) lines.push_back(std::move(l));
        return threw ? DrawPushResult::Threw : DrawPushResult::NotHeld;
    }

    // Takes this call's entries off every list they went onto, only while that list's tail
    // still holds exactly them; otherwise the list is left as found. One line per list left
    // as found (the adapter counts each as an anomaly).
    template <class Ops>
    std::vector<std::string> Remove(Ops& ops)
    {
        std::vector<std::string> lines;
        for (int k = 0; k < kDrawListCount && pushed_ > 0; ++k) {
            int perList = 0;
            for (int n : on_[static_cast<size_t>(k)]) perList += n;
            if (perList <= 0) continue;
            const int before = before_[static_cast<size_t>(k)];
            int len = -1;
            const bool tail = ops.TailHolds(k, before, entries_, on_[static_cast<size_t>(k)], len);
            if (tail) {
                len = ops.DeleteLast(k, len, perList);
                if (len == before) continue;
            }
            lines.push_back("mark1probe draws push: lootListUnique[" + std::to_string(k) + "] "
                            + (tail ? "kept entries after removal - size " + std::to_string(len) + ", expected " + std::to_string(before)
                                    : "changed during the call, left as found (size " + std::to_string(before) + " + "
                                      + std::to_string(perList) + " pushed, now " + std::to_string(len) + ")"));
        }
        Clear();
        return lines;
    }

private:
    void Clear()
    {
        entries_.clear();
        for (std::vector<int>& on : on_) on.clear();
        before_.fill(-1);
        pushed_ = 0;
    }

    std::vector<Mark1Drops::PushEntry> entries_;
    std::array<std::vector<int>, kDrawListCount> on_{};
    std::array<int, kDrawListCount> before_{ -1, -1, -1, -1, -1, -1 };
    int pushed_ = 0;
};

// The line for a push that came off again, once per change (the adapter logs it once).
inline std::string DrawPushRefusedLine(DrawPushResult r)
{
    return r == DrawPushResult::Threw
        ? std::string("mark1probe draws push: an add threw - taken off again; this call carries no stand-in")
        : std::string("mark1probe draws push: not visible on every list's tail - taken off again; calls carry no stand-in until a push reads back");
}

class Draws {
public:
    bool On() const { return on_; }
    int Push() const { return push_; }
    DrawSide Side() const { return side_; }
    const DrawCounts& Counts() const { return counts_; }

    // `draws on|off`. On needs DropItem's route to be the inline detour: only that one sees
    // the game's compiled calls, and a watch that cannot see them would count zeros.
    std::string SetOn(bool on, const DrawRoutes& r)
    {
        if (on && r.dropitem != kDrawRouteDetoured)
            return "mark1probe draws: refused - dropitem=" + r.dropitem + " sattier=" + r.sattier + " retsat=" + r.retsat
                + " (only DropManager's inline DropItem detour sees the game's calls, and a probe holding its slot is named);"
                  " the watch stays " + std::string(on_ ? "on" : "off");
        on_ = on;
        if (on) return "mark1probe draws: watch=on" + RoutesText(r);
        return "mark1probe draws: watch=off" + RoutesText(r) + " - counting and the push stopped; the hooks stay";
    }

    // `draws push <c> [ss|below]|off`; c 0 is off. A push on is refused while either feature
    // that pushes onto the same lists is switched on. The side (SsFirst with no side word) is
    // the one the stand-ins are picked from; the reply names it only when one was asked.
    std::string SetPush(int copies, bool mark1On, bool elfOn, DrawSide side = DrawSide::SsFirst)
    {
        if (copies > 0 && (mark1On || elfOn))
            return std::string("mark1probe draws push: refused - ")
                + (mark1On ? "Mark 1 drops are switched on (their push shares these lists)" : "Pocket Elf is switched on (its push shares these lists)")
                + "; the lever stays " + PushText();
        if (copies > Mark1Drops::kMaxCopies) copies = Mark1Drops::kMaxCopies;
        push_ = copies < 1 ? 0 : copies;
        side_ = push_ == 0 ? DrawSide::SsFirst : side;
        if (push_ == 0) return "mark1probe draws push: off - nothing is pushed";
        std::string out = "mark1probe draws push: c=" + std::to_string(push_) + " - while the watch is on, each DropItem call pushes "
            + std::to_string(push_) + " copies of each Mark 1 kill stand-in onto each of lootListUnique[0..5]; nothing is rewritten";
        if (side_ == DrawSide::Ss) out += "; side=ss: SS stand-ins only, no fallback";
        if (side_ == DrawSide::Below) out += "; side=below: below-SS stand-ins only, no fallback";
        return out;
    }

    std::string StatusLine(const DrawRoutes& r) const
    {
        const DrawCounts& c = counts_;
        auto n = [](uint64_t v) { return std::to_string(v); };
        std::string tiers;
        for (int k = 0; k < kDrawTierCount; ++k) tiers += (k ? "," : "") + n(c.tiers[static_cast<size_t>(k)]);
        std::string out = std::string("mark1probe draws: watch=") + (on_ ? "on" : "off") + RoutesText(r) + " calls=" + n(c.calls)
            + " argc=" + (c.calls ? std::to_string(c.argcMin) + ".." + std::to_string(c.argcMax) : std::string("-"))
            + " arg7=undefined:" + n(c.arg7[0]) + ",list:" + n(c.arg7[1]) + ",other:" + n(c.arg7[2]) + " tiers=" + tiers
            + " tier-unread=" + n(c.tierUnread) + " reads=" + n(c.reads) + " builds=" + n(c.builds) + " normal=" + n(c.normal);
        for (int s = 0; s < kDrawScopeCount; ++s)
            out += std::string(" ") + DrawScopeText(static_cast<DrawScope>(s)) + "=" + n(c.scopes[static_cast<size_t>(s)]);
        out += " on-k=" + n(c.onK) + " off-k=" + n(c.offK) + " retsat-calls=" + n(c.retsatCalls) + " push=" + PushText()
            + " push-side=" + (push_ > 0 ? std::string(DrawSideText(side_)) : std::string("-")) + " picks=" + n(c.picks)
            + " pick-builds=" + n(c.pickBuilds) + " push-held=" + n(c.pushHeld) + " push-skipped=" + n(c.pushSkipped)
            + " pushed-reads=" + n(c.pushedReads) + " anomalies=" + n(c.anomalies);
        return out;
    }

    // One DropItem call, before the game's: counted only while the watch is on, and then a
    // `draw` scope until EndCall. True when it was counted; the adapter calls EndCall then.
    bool BeginCall(int argc, DrawArg7 arg7)
    {
        if (!on_) return false;
        if (counts_.calls == 0 || argc < counts_.argcMin) counts_.argcMin = argc;
        if (counts_.calls == 0 || argc > counts_.argcMax) counts_.argcMax = argc;
        ++counts_.calls;
        ++counts_.arg7[static_cast<size_t>(arg7)];
        frames_.push_back(Frame{ DrawScope::Draw });
        return true;
    }

    // Whether this call pushes: the watch and the lever on, neither pushing feature switched
    // on, and the outermost DropItem call (a nested one rides on the outer one's entries).
    bool ShouldPush(bool mark1On, bool elfOn) const
    {
        return on_ && push_ > 0 && !mark1On && !elfOn && DrawFrames() == 1 && pushed_.empty();
    }

    // The entries the adapter's push put on, read back Held: the picks are typed against them,
    // and the call is one `push-held=`. Only the outermost call pushes; a nested one, which
    // rides on the outer one's entries, is not noted.
    void NotePushed(const std::vector<Mark1Drops::PushEntry>& entries)
    {
        if (!on_ || DrawFrames() != 1) return;
        pushed_.clear();
        for (const Mark1Drops::PushEntry& e : entries) pushed_.push_back(e.triple);
        if (!pushed_.empty()) ++counts_.pushHeld;
    }
    // A call ShouldPush let push and no Held push followed, the adapter's note whatever the
    // reason (a list unread, no stand-ins, DrawPushResult None, NotHeld or Threw, a throw it
    // swallowed): one `push-skipped=`.
    void NotePushSkipped()
    {
        if (on_) ++counts_.pushSkipped;
    }
    void NoteAnomaly() { ++counts_.anomalies; }
    bool Pushing() const { return !pushed_.empty(); }

    // The end of a call BeginCall counted: its scope closes (and any nested one left open
    // with it), and the outermost call's pushed entries are gone.
    void EndCall()
    {
        while (!frames_.empty()) {
            const bool draw = frames_.back().kind == DrawScope::Draw;
            frames_.pop_back();
            if (draw) break;
        }
        if (DrawFrames() == 0) pushed_.clear();
    }

    // A nested roll's scope (the shared DropUniqueItems detour, #74's Angelic roll, Mark 1's
    // Heroic hook), only while the watch is on. True when entered; LeaveRoll closes it.
    bool EnterRoll(DrawScope s)
    {
        if (!on_ || s == DrawScope::Draw || s == DrawScope::Outside) return false;
        frames_.push_back(Frame{ s });
        return true;
    }
    void LeaveRoll()
    {
        if (!frames_.empty() && frames_.back().kind != DrawScope::Draw) frames_.pop_back();
    }

    DrawScope Current() const { return frames_.empty() ? DrawScope::Outside : frames_.back().kind; }

    // LoadSatanicDropTier's answer: counted, and the innermost DropItem call's k.
    void NoteTier(bool numeric, double v)
    {
        if (!on_) return;
        long long k = -1;
        const bool whole = numeric && Mark1Set::AsWhole(v, k) && k >= 0 && k < kDrawTierCount;
        if (whole) ++counts_.tiers[static_cast<size_t>(k)];
        else ++counts_.tierUnread;
        for (auto it = frames_.rbegin(); it != frames_.rend(); ++it) {
            if (it->kind != DrawScope::Draw) continue;
            it->k = whole ? static_cast<int>(k) : -1;
            break;
        }
    }

    // A GetUniqueRepoStruct read of (type, sub, b): counted, the current scope's latest, and one
    // of the triples its next pick is typed against; in `draw`, a read of a triple the current
    // call pushed is a `pushed-reads=`.
    void NoteRead(bool numeric, double t, double sub, double b)
    {
        if (!on_) return;
        ++counts_.reads;
        Frame& f = Top();
        ++f.reads;
        f.haveRead = numeric && Mark1Set::AsWhole(t, f.read.t) && Mark1Set::AsWhole(sub, f.read.sub) && Mark1Set::AsWhole(b, f.read.b);
        if (!f.haveRead) return;
        RememberRead(f, f.read);
        if (f.kind == DrawScope::Draw && IsPushed(f.read)) ++counts_.pushedReads;
    }

    // A CreateDefaultParams call of (sub, b): the current scope's pick, typed when any triple
    // the scope read since its last typed pick or build names the same pair (the newest such
    // read first). A draw that weighs several entries and keeps one it read earlier is typed
    // the same as one that keeps the last entry it read. In `draw`, a typed pick counts in
    // `picks=` now when every triple it matched was pushed; when the pair also names a triple
    // that was not, the build settles it by its class (NoteBuilt).
    void NotePick(bool numeric, double sub, double b)
    {
        if (!on_) return;
        Frame& f = Top();
        f.havePick = false;
        f.pickPushed = false;
        f.pickMatches.clear();
        long long s = 0, bb = 0;
        if (numeric && Mark1Set::AsWhole(sub, s) && Mark1Set::AsWhole(b, bb)) {
            for (auto it = f.window.rbegin(); it != f.window.rend(); ++it)
                if (it->sub == s && it->b == bb) f.pickMatches.push_back(*it);
        }
        if (f.pickMatches.empty()) return;   // untyped: the reads stay for the next pick
        f.havePick = true;
        f.pick = f.pickMatches.front();
        f.window.clear();                    // the next pick is typed against the reads after this one
        if (f.kind != DrawScope::Draw) return;
        bool allPushed = true;
        for (const Mark1Drops::Triple& m : f.pickMatches) allPushed = allPushed && IsPushed(m);
        if (!allPushed) return;
        f.pickPushed = true;
        ++counts_.picks;
    }

    // What to read for one build: whether it is a unique, and the triple its scope picked, the
    // one of the pick's matches whose class and b are the build's.
    DrawBuildQuery Query(const DrawBuilt& item) const
    {
        DrawBuildQuery q;
        long long c = -1, t = -1, b = -1;
        q.unique = item.c && Mark1Set::AsWhole(*item.c, c) && c == 1;
        const Frame& f = TopConst();
        if (!q.unique || !f.havePick || !item.t || !item.b || !Mark1Set::AsWhole(*item.t, t) || !Mark1Set::AsWhole(*item.b, b))
            return q;
        for (const Mark1Drops::Triple& m : f.pickMatches) {
            if (m.t != t || m.b != b) continue;
            q.haveTriple = true;
            q.triple = m;
            break;
        }
        return q;
    }

    // One item built, with the triple's counts when Query named one and the adapter read
    // them. Returns the `built` line for a unique while under the cap, else "".
    std::string NoteBuilt(const DrawBuilt& item, const std::optional<DrawListCounts>& lists)
    {
        if (!on_) return {};
        ++counts_.builds;
        long long c = -1;
        const bool whole = item.c && Mark1Set::AsWhole(*item.c, c);
        if (whole && c == 0) ++counts_.normal;
        if (!whole || c != 1) return {};
        const DrawBuildQuery q = Query(item);
        const DrawScope scope = Current();
        ++counts_.scopes[static_cast<size_t>(scope)];
        Frame& f = Top();
        const int k = DrawK();
        if (scope == DrawScope::Draw && q.haveTriple) {
            if (IsPushed(q.triple)) {
                if (!f.pickPushed) ++counts_.picks;   // a pick its pair left open, settled by the build's class
                ++counts_.pickBuilds;
            } else if (lists && k >= 0) {
                ++((*lists)[static_cast<size_t>(k)] >= 1 ? counts_.onK : counts_.offK);
            }
        }
        std::string line;
        if (lines_ < kDrawBuiltLineCap) {
            ++lines_;
            auto num = [](const std::optional<double>& v) { return v ? NumberText(*v) : std::string("?"); };
            std::string counts = "-";
            if (q.haveTriple && lists) {
                counts.clear();
                for (int i = 0; i < kDrawListCount; ++i) counts += (i ? "," : "") + std::to_string((*lists)[static_cast<size_t>(i)]);
            }
            line = "mark1probe draws built: \"" + (item.name.empty() ? std::string("?") : item.name) + "\" t=" + num(item.t)
                + " b=" + num(item.b) + " rarity=" + num(item.rarity) + " tier=" + num(item.tier) + " scope=" + DrawScopeText(scope)
                + " k=" + (k >= 0 ? std::to_string(k) : std::string("-")) + " lists=" + counts + " reads=" + std::to_string(f.reads);
        }
        f.reads = 0;
        f.havePick = false;
        f.pickPushed = false;
        f.pickMatches.clear();
        f.window.clear();
        return line;
    }

    void NoteRetsat()
    {
        if (on_) ++counts_.retsatCalls;
    }

private:
    struct Frame {
        DrawScope          kind = DrawScope::Outside;
        int                k = -1;          // Draw: the tier LoadSatanicDropTier answered in this call
        int                reads = 0;       // reads in this scope since its last build
        bool               haveRead = false;
        Mark1Drops::Triple read;            // the latest read
        std::vector<Mark1Drops::Triple> window;        // distinct reads since the last typed pick or build, oldest first
        bool               havePick = false;
        Mark1Drops::Triple pick;            // the latest typed pick: its newest matching read
        std::vector<Mark1Drops::Triple> pickMatches;   // every read in the window the pick's pair named, newest first
        bool               pickPushed = false;          // counted in `picks=` at the pick: every match was pushed
    };

    bool IsPushed(const Mark1Drops::Triple& x) const
    {
        for (const Mark1Drops::Triple& p : pushed_)
            if (p == x) return true;
        return false;
    }

    // A read joins its scope's window once, as the newest; past the cap the oldest drops out.
    static void RememberRead(Frame& f, const Mark1Drops::Triple& x)
    {
        for (auto it = f.window.begin(); it != f.window.end(); ++it) {
            if (!(*it == x)) continue;
            f.window.erase(it);
            break;
        }
        f.window.push_back(x);
        if (f.window.size() > kDrawReadWindowCap) f.window.erase(f.window.begin());
    }

    static std::string RoutesText(const DrawRoutes& r)
    {
        return " dropitem=" + r.dropitem + " sattier=" + r.sattier + " retsat=" + r.retsat;
    }
    std::string PushText() const { return push_ > 0 ? std::to_string(push_) : std::string("off"); }
    int DrawFrames() const
    {
        int n = 0;
        for (const Frame& f : frames_) n += f.kind == DrawScope::Draw ? 1 : 0;
        return n;
    }
    // The innermost DropItem call's tier, -1 with none in progress or none read.
    int DrawK() const
    {
        for (auto it = frames_.rbegin(); it != frames_.rend(); ++it)
            if (it->kind == DrawScope::Draw) return it->k;
        return -1;
    }
    Frame& Top() { return frames_.empty() ? outside_ : frames_.back(); }
    const Frame& TopConst() const { return frames_.empty() ? outside_ : frames_.back(); }

    bool on_ = false;
    int push_ = 0;
    DrawSide side_ = DrawSide::SsFirst;
    int lines_ = 0;
    std::vector<Frame> frames_;
    Frame outside_;
    std::vector<Mark1Drops::Triple> pushed_;
    DrawCounts counts_;
};

// ---- `pool draw` ---------------------------------------------------------------------
//
// The candidates DropItem's draw keeps, per class (helmet 0, body 1, boots 2, gloves 4, and
// Pocket Elf's class 10): the distinct triples of the class on any of [0..5] whose
// definition reads rarity 6 (the draw's Satanic branch) with fields 40 and 41 clear (a set
// one sends the draw back, static reading), each read by name outside a roll. `ss=` counts
// the tier-5 ones. Per tier side (SS, and below SS) the best is the smallest largest count
// over the six lists (0 everywhere first), then the lowest (sub, b): the stand-in rule's
// order. Writes nothing.

inline constexpr int kDrawRarity = 6;     // the Satanic rarity the draw keeps (static reading)
inline constexpr int kDrawSsTier = 5;     // info key 32's SS tier
inline constexpr int kElfItemType = 10;   // Pocket Elf's class (FlightCharms.hpp)

// One triple's repository verdict as the draw reads it: rarity (27), tier (32), and fields
// 40 and 41. False when it could not be read.
struct DrawVerdict {
    int  rarity = 0;
    int  tier = -1;
    bool f40 = false;
    bool f41 = false;
};
using DrawVerdictFn = std::function<bool(const Mark1Drops::Triple& key, DrawVerdict& out)>;

struct DrawCandidate {
    Mark1Drops::Triple triple;
    int                rarity = 0;
    int                tier = -1;
    DrawListCounts     counts{};
    bool               f40 = false;   // as read; a candidate has both clear
    bool               f41 = false;
};

struct PoolDrawClass {
    std::string                  key;
    int                          itemType = 0;
    int                          candidates = 0;
    int                          ss = 0;
    std::optional<DrawCandidate> bestSs, bestOther;
};

// The five classes, in the line's order: the Mark 1 pieces' own rows, then Pocket Elf.
inline std::vector<std::pair<std::string, int>> PoolDrawClasses()
{
    std::vector<std::pair<std::string, int>> out;
    for (const Mark1Set::PieceRow& row : Mark1Set::kPieces) out.emplace_back(std::string(row.key), row.itemType);
    out.emplace_back("elf", kElfItemType);
    return out;
}

// Whether `a` is the better stand-in: the smaller largest count, then the lower (sub, b).
inline bool DrawBetter(const DrawCandidate& a, const DrawCandidate& b)
{
    int ma = 0, mb = 0;
    for (int n : a.counts) ma = n > ma ? n : ma;
    for (int n : b.counts) mb = n > mb ? n : mb;
    if (ma != mb) return ma < mb;
    if (a.triple.sub != b.triple.sub) return a.triple.sub < b.triple.sub;
    return a.triple.b < b.triple.b;
}

inline std::vector<PoolDrawClass> PoolDraw(const Mark1Drops::KillLists& lists, const DrawVerdictFn& verdict)
{
    const std::vector<std::pair<std::string, int>> classes = PoolDrawClasses();
    std::map<Mark1Drops::Triple, DrawListCounts> counts;   // ordered by (type, sub, b)
    for (int k = 0; k < kDrawListCount; ++k) {
        for (const Mark1Drops::ListEntry& e : lists[static_cast<size_t>(k)]) {
            Mark1Drops::Triple key;
            if (!e.triple || !Mark1Set::AsWhole(e.t, key.t) || !Mark1Set::AsWhole(e.sub, key.sub) || !Mark1Set::AsWhole(e.b, key.b)) continue;
            bool wanted = false;
            for (const auto& c : classes) wanted = wanted || c.second == key.t;
            if (wanted) ++counts[key][static_cast<size_t>(k)];
        }
    }
    std::vector<PoolDrawClass> out;
    for (const auto& [name, type] : classes) {
        PoolDrawClass pc;
        pc.key = name;
        pc.itemType = type;
        out.push_back(pc);
    }
    for (const auto& [key, c] : counts) {
        DrawVerdict v;
        if (!verdict || !verdict(key, v)) continue;
        if (v.rarity != kDrawRarity || v.f40 || v.f41) continue;
        for (PoolDrawClass& pc : out) {
            if (pc.itemType != key.t) continue;
            ++pc.candidates;
            const DrawCandidate cand{ key, v.rarity, v.tier, c, v.f40, v.f41 };
            const bool ss = v.tier == kDrawSsTier;
            if (ss) ++pc.ss;
            std::optional<DrawCandidate>& best = ss ? pc.bestSs : pc.bestOther;
            if (!best || DrawBetter(cand, *best)) best = cand;
        }
    }
    return out;
}

// `best-ss=<sub>:<b>@<rarity>/<tier>x<n0>,...,<n5>`, or none.
inline std::string DrawCandidateText(const std::optional<DrawCandidate>& c)
{
    if (!c) return "none";
    std::string counts;
    for (int k = 0; k < kDrawListCount; ++k) counts += (k ? "," : "") + std::to_string(c->counts[static_cast<size_t>(k)]);
    return std::to_string(c->triple.sub) + ":" + std::to_string(c->triple.b) + "@" + std::to_string(c->rarity) + "/"
        + std::to_string(c->tier) + "x" + counts;
}

inline std::vector<std::string> PoolDrawLines(const std::vector<PoolDrawClass>& classes)
{
    std::vector<std::string> lines;
    for (const PoolDrawClass& pc : classes)
        lines.push_back("mark1probe pool draw: " + pc.key + " class=" + std::to_string(pc.itemType) + " candidates="
                        + std::to_string(pc.candidates) + " ss=" + std::to_string(pc.ss) + " best-ss=" + DrawCandidateText(pc.bestSs)
                        + " best-other=" + DrawCandidateText(pc.bestOther));
    return lines;
}

// ---- the lever's stand-ins -----------------------------------------------------------
//
// `draws push` pushes, per Mark 1 piece, an entry the draw itself keeps as read: a `pool
// draw` candidate of the piece's class (rarity 6, fields 40 and 41 clear). `pool kill`'s
// stand-in is DropUniqueItems' choice and can be one the draw always sends back, so a low or
// zero pick count would then be about our entry, not about the draw. The side is the one
// Mark 1's kill draw is to use, SS first (the draw's one more test on an SS entry included,
// so the lever meets what the move would meet), and the other side only when the class has
// no SS candidate; the reply names the side. A class with no candidate pushes nothing and
// is named as refused. Each pushed stand-in prints its rarity, tier, fields 40 and 41 and
// its counts on [0..5]. That is the rule with no side word (DrawSide::SsFirst); `ss` or
// `below` takes that side's best only, and a class with none there is refused, never
// filled from the other side.

inline constexpr bool kDrawLeverPrefersSs = true;   // Mark 1's kill-draw default: SS first

struct DrawLeverStandIn {
    Mark1Set::Piece              piece = Mark1Set::Piece::Helmet;
    std::string                  key;
    int                          itemType = 0;
    std::optional<DrawCandidate> pick;
    bool                         ss = false;         // picked on the SS side
    bool                         fallback = false;   // the preferred side had no candidate
    DrawSide                     side = DrawSide::SsFirst;   // the side asked
};
using DrawLeverStandIns = std::array<DrawLeverStandIn, Mark1Set::kPieceCount>;

// Why the draw would not keep `c` as read: "" when it would, else the failing field.
inline std::string DrawLeverRefusal(const DrawCandidate& c)
{
    if (c.rarity != kDrawRarity) return "rarity " + std::to_string(c.rarity);
    if (c.f40) return "field 40 set";
    if (c.f41) return "field 41 set";
    return {};
}

// Each piece's stand-in from `pool draw`'s classes. With no side: the preferred side's best,
// else the other side's, else none. With `ss` or `below`: that side's best, else none.
inline DrawLeverStandIns DrawLeverPick(const std::vector<PoolDrawClass>& classes, DrawSide side = DrawSide::SsFirst)
{
    DrawLeverStandIns out{};
    for (int i = 0; i < Mark1Set::kPieceCount; ++i) {
        const Mark1Set::PieceRow& row = Mark1Set::kPieces[i];
        DrawLeverStandIn& s = out[static_cast<size_t>(i)];
        s.piece = row.piece;
        s.key = std::string(row.key);
        s.itemType = row.itemType;
        s.side = side;
        for (const PoolDrawClass& pc : classes) {
            if (pc.itemType != row.itemType) continue;
            if (side != DrawSide::SsFirst) {
                const std::optional<DrawCandidate>& only = side == DrawSide::Ss ? pc.bestSs : pc.bestOther;
                if (only) {
                    s.pick = only;
                    s.ss = side == DrawSide::Ss;
                }
                break;
            }
            const std::optional<DrawCandidate>& first = kDrawLeverPrefersSs ? pc.bestSs : pc.bestOther;
            const std::optional<DrawCandidate>& second = kDrawLeverPrefersSs ? pc.bestOther : pc.bestSs;
            if (first) {
                s.pick = first;
                s.ss = kDrawLeverPrefersSs;
            } else if (second) {
                s.pick = second;
                s.ss = !kDrawLeverPrefersSs;
                s.fallback = true;
            }
            break;
        }
    }
    return out;
}

// The lever's plan: c copies of each piece's draw stand-in, each onto every one of [0..5];
// a stand-in the draw would not keep is never planned. `n` is the stand-in's largest count
// over the six lists. Copies below 1 push nothing, above 400 push 400.
inline std::vector<Mark1Drops::PushEntry> DrawPushPlan(const DrawLeverStandIns& s, int copies)
{
    std::vector<Mark1Drops::PushEntry> plan;
    if (copies < 1) return plan;
    if (copies > Mark1Drops::kMaxCopies) copies = Mark1Drops::kMaxCopies;
    for (const DrawLeverStandIn& e : s) {
        if (!e.pick || !DrawLeverRefusal(*e.pick).empty()) continue;
        int most = 0;
        for (int n : e.pick->counts) most = n > most ? n : most;
        plan.push_back(Mark1Drops::PushEntry{ e.piece, e.pick->triple, most, copies });
    }
    return plan;
}

// One piece in the lever's reply: `<key>=<sub>:<b> rarity=<r> tier=<t> f40=<0|1> f41=<0|1>
// lists=<n0>,...,<n5> side=<ss|below-ss>[ (fallback)]`, or `<key>=refused (...)` naming why.
inline std::string DrawLeverStandInText(const DrawLeverStandIn& s)
{
    if (!s.pick && s.side != DrawSide::SsFirst)
        return s.key + "=refused (class " + std::to_string(s.itemType) + " has no "
            + (s.side == DrawSide::Ss ? "SS" : "below-SS") + " draw candidate on lootListUnique[0..5]; side="
            + DrawSideText(s.side) + " takes no fallback)";
    if (!s.pick)
        return s.key + "=refused (class " + std::to_string(s.itemType)
            + " has no draw candidate: no rarity-6 entry with fields 40 and 41 clear on lootListUnique[0..5])";
    const DrawCandidate& c = *s.pick;
    const std::string pair = std::to_string(c.triple.sub) + ":" + std::to_string(c.triple.b);
    const std::string why = DrawLeverRefusal(c);
    if (!why.empty()) return s.key + "=refused (" + pair + ": " + why + ")";
    std::string counts;
    for (int k = 0; k < kDrawListCount; ++k) counts += (k ? "," : "") + std::to_string(c.counts[static_cast<size_t>(k)]);
    return s.key + "=" + pair + " rarity=" + std::to_string(c.rarity) + " tier=" + std::to_string(c.tier) + " f40=" + (c.f40 ? "1" : "0")
        + " f41=" + (c.f41 ? "1" : "0") + " lists=" + counts + " side=" + (s.ss ? "ss" : "below-ss") + (s.fallback ? " (fallback)" : "");
}

// The reply `draws push <c> [ss|below]` prints after the core's, naming what each call will
// push and from which side.
inline std::string DrawLeverLine(const DrawLeverStandIns& s, bool watchOn, DrawSide side = DrawSide::SsFirst)
{
    std::string out = std::string("mark1probe draws push: stand-ins (pool draw's candidates, ")
        + (side == DrawSide::Ss      ? std::string("SS only, side=ss, no fallback)")
           : side == DrawSide::Below ? std::string("below SS only, side=below, no fallback)")
                                     : std::string(kDrawLeverPrefersSs ? "SS first" : "below SS first") + ", the side Mark 1's kill draw is to use)");
    for (size_t i = 0; i < s.size(); ++i) out += (i ? " | " : " ") + DrawLeverStandInText(s[i]);
    if (!watchOn) out += " (the watch is off: `draws on` to push)";
    return out;
}

// The lever's stand-ins, resolved again when any of [0..5] changed (another array or
// another length), as the kill route's cache does, or when another side is asked.
class DrawLeverCache {
public:
    bool Stale(const Mark1Drops::KillListKeys& keys, DrawSide side = DrawSide::SsFirst) const
    {
        if (!have_ || side != side_) return true;
        for (size_t k = 0; k < keys.size(); ++k)
            if (keys[k].id != keys_[k].id || keys[k].length != keys_[k].length) return true;
        return false;
    }
    void Store(const Mark1Drops::KillListKeys& keys, const DrawLeverStandIns& s, DrawSide side = DrawSide::SsFirst)
    {
        have_ = true;
        keys_ = keys;
        standIns_ = s;
        side_ = side;
    }
    const DrawLeverStandIns* Find() const { return have_ ? &standIns_ : nullptr; }

private:
    bool                         have_ = false;
    Mark1Drops::KillListKeys     keys_{};
    DrawLeverStandIns            standIns_{};
    DrawSide                     side_ = DrawSide::SsFirst;
};

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

// NotFound: a builtin row whose name the runtime does not know. Listed, never
// counted as failed: the walk cannot be moving the player through it.
enum class HookState : int { NotTried, Detoured, Failed, Held, NotFound };

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

// The answers `statlog` has seen.
struct StatlogCounts {
    uint64_t calls = 0;      // dispatcher calls for the id
    uint64_t direct = 0;     // getter-row calls with no dispatcher around them
    int      lines = 0;
    uint64_t dropped = 0;
    bool     have = false;   // a number was seen: min and max hold it
    double   min = 0, max = 0;
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
    int NotFound() const { return CountHooks(HookState::NotFound); }

    std::string HookLine() const
    {
        return "mark1probe hook: " + std::to_string(Hooked()) + " detoured, " + std::to_string(Failed()) + " failed, "
            + std::to_string(Held()) + " held, " + std::to_string(NotFound()) + " not-found";
    }

    // The builtin names the runtime does not know, one line, or none.
    std::vector<std::string> NotFoundLines() const
    {
        std::string names;
        for (int i = 0; i < kRowCount; ++i)
            if (hook_[i] == HookState::NotFound) names += (names.empty() ? "" : ", ") + std::string(kRowNames[i]);
        if (names.empty()) return {};
        return { "  not-found: " + names + " (no builtin by that name on this runtime; not counted as failed)" };
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
                + std::to_string(Held()) + " lever=" + LeverText() + " watch=" + (WatchOn() ? "on" : "off")
                + " statlog=" + (statlogOn_ ? "on" : "off"),
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

    int64_t LeverStart() const { return leverStart_; }

    // On frames start .. start + frames - 1.
    bool LeverLive(int64_t frame) const
    {
        return lever_.kind != LeverKind::Off && frame >= leverStart_ && frame - leverStart_ < lever_.frames;
    }

    // `hold`: SetLever, plus the value the adapter read at the target now
    // (`startRead` false when it could not), which the lever's end writes
    // back once. With nothing read there is nothing to restore, so the lever
    // writes nothing at all (each call counts `missing`); neither does `x<m>`
    // of a bool. The caller restores an earlier hold first (RestoreHold with
    // force), so `start` is never the earlier hold's own value.
    std::string StartHold(const Lever& l, int64_t frame, bool startRead, const Value& start)
    {
        const std::string replaced = SetLever(l, frame);
        holdTarget_ = l.target;
        holdStart_ = start;
        holdReady_ = startRead && !(l.holdMult && start.isBool);
        holdValue_ = l.holdMult ? Value{ false, start.number * l.mult } : l.value;
        holdDirty_ = false;
        return replaced;
    }

    // Whether a hold wrote and its restore is still owed.
    bool HoldOwed() const { return holdDirty_; }

    // Writes the value a hold read when it started back, once, when the hold
    // is no longer live at `frame` (it ran out, `off` turned it off, or
    // another lever replaced it), or at once with `force` (the adapter, just
    // before a new lever starts). The line says what was written back, or ""
    // when nothing was owed.
    template <class Access>
    std::string RestoreHold(Access& access, int64_t frame, bool force)
    {
        if (!holdDirty_) return {};
        if (!force && lever_.kind == LeverKind::Hold && LeverLive(frame)) return {};
        holdDirty_ = false;
        const bool ok = access.Write(holdTarget_, holdStart_);
        return "mark1probe hold: restored " + TargetText(holdTarget_) + " := " + ValueText(holdStart_)
            + (ok ? std::string() : std::string(" (the write did not land)"));
    }

    // `statpin`: the last local answer for the id before now, from either
    // route (the dispatcher with that id, or the id's getter row), and the
    // frame it was seen on. False when none was seen since the rows went in.
    bool LastStat(long long statId, double& value, int64_t& frame) const
    {
        const auto it = lastStat_.find(statId);
        if (it == lastStat_.end()) return false;
        value = it->second.first;
        frame = it->second.second;
        return true;
    }

    // `off`: every lever, watch, statlog, args, count and pos off; the rows
    // stay installed as pass-throughs and the counts stay for `show`. A
    // hold's restore stays owed until the adapter's RestoreHold.
    void Off()
    {
        LeverOff();
        StopWatch();
        statlogOn_ = false;
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
    //   std::string ResultKind()                the result's kind, for one that is not
    //   void SetResult(double n)
    // A call whose self is not the local player runs the original and nothing
    // else. set, scale, hold and move act on the outermost call of their row
    // only, so a row the game calls inside itself is written once. stat and
    // statpin act on every matching ReturnSpecificStat call (an inner call
    // may ask for a different id) and on the outermost call of the id's
    // getter row, and change a number once: a call whose original already
    // ran one (the dispatcher handing id 64 to a detoured StatMovementSpeed)
    // returns that number as it is. Whatever the lever, a local
    // ReturnSpecificStat call's id and answer and an outermost getter-row
    // call's answer are noted (statpin's pin, statlog's lines), before any
    // lever changes the answer.
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
        const bool holds = live && outer && lever_.kind == LeverKind::Hold;
        const bool moves = live && outer && lever_.kind == LeverKind::Move;
        const bool statOn = LeverLive(frame) && (lever_.kind == LeverKind::Stat || lever_.kind == LeverKind::Pin);
        long long getterId = 0;
        const bool isDispatch = row == Row::ReturnSpecificStat;
        const bool isGetter = outer && GetterStatId(row, getterId);
        const bool statDispatch = statOn && isDispatch;
        const bool statGetter = statOn && isGetter && getterId == lever_.statId;
        const bool getterDirect = isGetter && depth_[static_cast<int>(Row::ReturnSpecificStat)] == 0;
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
        if (holds) {
            // No restore after the call: the value read when the hold
            // started goes back once, through RestoreHold.
            if (holdReady_ && access.Write(lever_.target, holdValue_)) { holdDirty_ = true; ++counts_.applied; }
            else ++counts_.missing;
        }
        double x0 = 0, y0 = 0;
        const bool havePos = (moves || tracing) && access.Position(x0, y0);
        const double mult = lever_.mult;

        ++depth_[i];
        ++depthAll_;
        stack_.push_back(row);
        // A throw from the original unwinds the depth, the stack and a set or
        // scale write through this guard, and reaches the adapter untouched
        // (the adapter writes the rethrow line, test_crash_trap_contract.py).
        struct Unwind {
            Probe& self;
            int i;
            Access& access;
            const Target& target;
            const Value& old;
            bool restore;
            bool returned = false;
            ~Unwind()
            {
                if (returned) return;
                --self.depth_[i];
                --self.depthAll_;
                self.stack_.pop_back();
                if (restore) { try { access.Write(target, old); } catch (...) {} }
            }
        } unwind{ *this, i, access, target, old, restore };
        original();
        unwind.returned = true;
        --depth_[i];
        --depthAll_;
        stack_.pop_back();
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

        if (!isDispatch && !isGetter) return;
        // The call's stat id: the getter row's own, or the dispatcher's argument.
        double id = static_cast<double>(getterId);
        const bool haveId = isGetter || access.StatId(id);
        if (!haveId) return;
        double n = 0;
        const bool number = access.Result(n);
        if (number) lastStat_[static_cast<long long>(id)] = { n, frame };
        if (StatlogRunning(frame) && id == static_cast<double>(statlogId_) && (isDispatch || getterDirect))
            NoteStatlog(frame, row, number, n, number ? std::string() : access.ResultKind(), isDispatch);

        const bool matches = statGetter || (statDispatch && id == static_cast<double>(lever_.statId));
        if (matches && statScaled_ == scaledBefore) {
            if (number) {
                access.SetResult(lever_.kind == LeverKind::Stat ? n * lever_.mult : lever_.value.number);
                ++statScaled_;
                if (statGetter) {
                    ++counts_.changedGetter;
                    if (getterDirect) ++counts_.getterDirect;
                } else {
                    ++counts_.changedDispatch;
                }
            } else {
                ++counts_.skip;
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
                : hook_[i] == HookState::Failed ? " failed" : hook_[i] == HookState::NotFound ? " not-found" : " not hooked";
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

    // ---- `statlog` -----------------------------------------------------------
    // From `frame` for `frames` frames: one line per local ReturnSpecificStat
    // call for the id, and per local call of the id's getter row that no
    // dispatcher call is around (compiled GML reaches StatMovementSpeed
    // directly too). A new statlog replaces the old one.
    void StartStatlog(long long statId, int64_t frames, int64_t frame)
    {
        statlogOn_ = true;
        statlogId_ = statId;
        statlogFrames_ = frames;
        statlogStart_ = frame;
        statlog_ = StatlogCounts{};
    }
    bool StatlogRunning(int64_t frame) const
    {
        return statlogOn_ && frame >= statlogStart_ && frame - statlogStart_ < statlogFrames_;
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

    // One snapshot of the scope's numeric and bool members (input and effect:
    // element index -> value; pst as an array: `[<i>]` -> value), each value
    // as the adapter printed it. The first is the baseline; each later one
    // prints a line per member that changed, appeared (`none->v`) or went away
    // (`v->none`), named the way a `set` or `hold` target is written. Past
    // kWatchMaxLines the rest are counted and the watch ends, its end line last.
    std::vector<std::string> ObserveWatch(int64_t frame, const std::map<std::string, std::string>& snapshot)
    {
        std::vector<std::string> out;
        if (!WatchRunning(frame)) return out;
        if (!watchHave_) { watchLast_ = snapshot; watchHave_ = true; return out; }
        const auto line = [&](const std::string& name, const std::string& from, const std::string& to) {
            if (watchLines_ >= kWatchMaxLines) { ++watchDropped_; return; }
            ++watchLines_;
            out.push_back("watch f=" + std::to_string(frame) + " " + WatchMember(watch_.scope, name) + "=" + from + "->" + to);
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
    // not a row would read the same as a player standing still. The samples
    // need no row hooked. They also give the end line's `mode=` (the most
    // frequent non-zero frame-to-frame step, to 0.001 px, the walk's steady
    // speed) and `ms=` (the mean wall-clock time between samples, so two walks
    // can be told apart by frame time before their steps are compared).
    void StartPos(int64_t frames, int64_t frame)
    {
        posSteps_.clear();
        posClocks_ = 0;
        posClockFirst_ = 0;
        posClockLast_ = 0;
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
    // false when they could not be read (counted, never taken as a position),
    // and the wall clock in milliseconds (`ms`; not finite when unread).
    void ObservePosFrame(int64_t frame, bool read, double x, double y, double ms = std::nan(""))
    {
        if (!PosRunning(frame)) return;
        if (std::isfinite(ms)) {
            if (posClocks_ == 0) posClockFirst_ = ms;
            posClockLast_ = ms;
            ++posClocks_;
        }
        if (!read || !std::isfinite(x) || !std::isfinite(y)) { ++posUnread_; return; }
        ++posSamples_;
        if (posHaveSample_) {
            const double fx = x - posLastX_, fy = y - posLastY_;
            const long long step = std::llround(std::hypot(fx, fy) * 1000.0);
            if (step != 0) ++posSteps_[step];
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
            case LeverKind::Hold:  counts = "applied=" + std::to_string(counts_.applied) + " missing=" + std::to_string(counts_.missing); break;
            case LeverKind::Move:  counts = "moved=" + std::to_string(counts_.moved) + " still=" + std::to_string(counts_.still); break;
            case LeverKind::Stat:
            case LeverKind::Pin:   counts = StatCountsText(); break;
            case LeverKind::Off:   break;
            }
            out.push_back("mark1probe: lever " + LeverText() + " ended " + counts);
            LeverOff();
        }
        if (statlogOn_ && frame - statlogStart_ >= statlogFrames_) {
            const StatlogCounts& s = statlog_;
            out.push_back("statlog end id=" + std::to_string(statlogId_) + " frames=" + std::to_string(frame - statlogStart_)
                          + " calls=" + std::to_string(s.calls) + " direct=" + std::to_string(s.direct)
                          + " min=" + (s.have ? NumberText(s.min) : std::string("none"))
                          + " max=" + (s.have ? NumberText(s.max) : std::string("none"))
                          + " lines=" + std::to_string(s.lines) + " dropped=" + std::to_string(s.dropped));
            statlogOn_ = false;
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
            // The modal step: the most frequent non-zero one, the smaller on a tie.
            long long modeKey = 0;
            uint64_t modeCount = 0;
            for (const auto& [key, n] : posSteps_)
                if (n > modeCount) { modeKey = key; modeCount = n; }
            const std::string mode = modeCount ? NumberText(static_cast<double>(modeKey) / 1000.0) : std::string("none");
            const std::string ms = posClocks_ >= 2
                ? NumberText(std::round((posClockLast_ - posClockFirst_) / static_cast<double>(posClocks_ - 1) * 100.0) / 100.0)
                : std::string("none");
            out.push_back("pos total=" + (measured ? px(posPath_) : std::string("none")) + " in-rows=" + px(posInRows_)
                          + " outside=" + (measured ? px(posOutside_) : std::string("none")) + " mode=" + mode + " ms=" + ms
                          + " frames="
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

    // The rows the call is inside, innermost first, each once: `in=` of a
    // statlog line.
    std::string StackText() const
    {
        std::string out;
        bool seen[kRowCount]{};
        for (auto it = stack_.rbegin(); it != stack_.rend(); ++it) {
            const int r = static_cast<int>(*it);
            if (seen[r]) continue;
            seen[r] = true;
            out += (out.empty() ? "" : ",") + std::string(kRowNames[r]);
        }
        return out.empty() ? std::string("none") : out;
    }

    // `statlog f=<frame> id=<id> <row>=<answer> in=<rows>`: the answer the
    // call's original gave (a number, or its kind), before any lever.
    void NoteStatlog(int64_t frame, Row row, bool number, double n, const std::string& kind, bool dispatch)
    {
        StatlogCounts& s = statlog_;
        ++(dispatch ? s.calls : s.direct);
        if (number) {
            if (!s.have || n < s.min) s.min = n;
            if (!s.have || n > s.max) s.max = n;
            s.have = true;
        }
        if (s.lines >= kStatlogMaxLines) { ++s.dropped; return; }
        ++s.lines;
        Emit("statlog f=" + std::to_string(frame) + " id=" + std::to_string(statlogId_) + " " + std::string(RowName(row)) + "="
             + (number ? NumberText(n) : (kind.empty() ? std::string("?") : kind)) + " in=" + StackText());
    }

    HookState hook_[kRowCount]{};
    std::string holder_[kRowCount];
    RowCalls calls_[kRowCount]{};
    int depth_[kRowCount]{};
    int depthAll_ = 0;
    std::vector<Row> stack_;

    Lever lever_;
    int64_t leverStart_ = 0;
    LeverCounts counts_;

    // hold's restore, apart from lever_ so it outlives the lever's end.
    Target holdTarget_;
    Value holdStart_;
    Value holdValue_;
    bool holdReady_ = false;   // the start was read: the lever writes, and can restore
    bool holdDirty_ = false;   // a write landed and its restore is owed

    // statpin's pins: the last local answer per stat id, and its frame.
    std::map<long long, std::pair<double, int64_t>> lastStat_;

    bool statlogOn_ = false;
    long long statlogId_ = 0;
    int64_t statlogFrames_ = 0;
    int64_t statlogStart_ = 0;
    StatlogCounts statlog_;

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
    std::map<long long, uint64_t> posSteps_;   // frame-to-frame steps in 0.001 px -> frames
    uint64_t posClocks_ = 0;
    double posClockFirst_ = 0;
    double posClockLast_ = 0;

    std::vector<std::string> outbox_;
};

} // namespace ForgePact::Mark1Probe
