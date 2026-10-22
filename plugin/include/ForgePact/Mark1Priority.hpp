#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace ForgePact {

// Mark 1 set, Automation's cast priority (issue #166, the 2026-10-22 playtest):
// which automated bar column the `CheckTalentUse` detour presses on each gate
// call, which entries it rests around the original, and, on the proc route,
// which column it fires straight into `TalentUseClass` once the original has
// returned. Pure: no game object, no read and no write. Mark1Automation.hpp
// asks it, with the columns it read; its tests are
// tests/test_mark1_priority_behavior.py.
//
// The press route, one press per gate call:
//   - after any landed cast the gate is left alone for the yield window (no
//     press, no mask), so a held key carries on between automated casts;
//   - then the first *cooldown* column, in Q, E, R, Y order, that is not
//     resting is pressed. A press that does not land in its own gate call
//     rests that column for the probe rest: a skill on cooldown costs one
//     press per rest;
//   - otherwise a *free* column (no cooldown) is pressed on every call until
//     it lands, which is what holding its key does. The free columns take
//     turns: the next one in Q, E, R, Y order after the free column that last
//     landed. A free column pressed for kMark1TurnPatienceFrames without a
//     landing (short of mana) rests kMark1TurnRestFrames, and the next takes
//     the turn.
//   Mod live 2 measured that pressing every column in one call serves only
//   the first (Shadow Bolt on Q 20 casts, Heavenly Fire on R none), so a call
//   still presses one column.
// The mask: on a call that presses, each mask entry (the bar's other keys,
// the right mouse button among them) that reads held (3) or pressed (4) is
// rested (1) for the original and put back after it, so a held key cannot
// keep the automated column from being cast. The lockout: on every gate call
// while the bonus is active, on both routes, each automated column's own
// entry that reads 3 or 4 is rested the same way before the press, so a skill
// under automation cannot be cast by hand.
// The proc route (static reading: the game fires an item proc straight into
// `TalentUseClass`, outside the gate): a non-toggle column whose cooldown was
// read is fired at most once per gate call, the first due in Q, E, R, Y
// order, each on its own clock: its cooldown at the player's Skill Haste plus
// its cast time at the player's Faster Cast Rate (a no-cooldown skill's
// interval is its cast time). A proc column is never pressed and never
// masked; a toggle always takes the press. The hub's
// hs_game_sdk.mark1_automation_model computes the same intervals, and
// hs-game-sdk/curated/mark1_automation_cases.json is what both are held to.

// ---- the automated columns ---------------------------------------------------

// The automated bar columns (Q, E, R, Y): columns 3 to 6 of
// `UI_Hud_Talent_obj.row0` (RDM § 8.3).
inline constexpr int kMark1FirstColumn = 3;
inline constexpr int kMark1LastColumn = 6;
inline constexpr int kMark1ColumnCount = kMark1LastColumn - kMark1FirstColumn + 1;

inline constexpr bool Mark1IsAutomatedColumn(int column)
{
    return column >= kMark1FirstColumn && column <= kMark1LastColumn;
}

// The `inputState` entry a column presses: the entry of the same index
// (measured for 3 to 6, RDM § 7.7 `slot-keys`). Anything outside the
// automated columns has no entry (-1).
inline constexpr int Mark1InputEntryForColumn(int column)
{
    return Mark1IsAutomatedColumn(column) ? column : -1;
}

// ---- constants, each with its source -----------------------------------------

// An `inputState` entry rests at 1, reads 4 on a key's press frame, 3 while it
// is held and 2 on release (docs/mark1-set-research.md, `slot-keys`).
inline constexpr double kMark1InputResting = 1.0;
inline constexpr double kMark1InputHeld = 3.0;
inline constexpr double kMark1InputPressed = 4.0;

// The value a free column is pressed with: the press frame's value, measured
// to cast (`cast-input`); 3, the held value, is the alternative Live 1's
// `free-cadence` compares (decision key `hold-value:`).
inline constexpr double kMark1FreePressValue = 4.0;

// A talent whose `abilityCooldown` reads at most this has no cooldown (RDM
// § 7.1, measured; the model's NO_COOLDOWN_SECONDS).
inline constexpr double kMark1FreeCooldownFloor = 0.25;

// After a landed cast the gate is left alone this many frames: Stats live 1's
// `hold-cadence` measured about 62 frames between the casts of a held key
// (docs/mark1-set-research.md, decision key `yield-frames:`).
inline constexpr long long kMark1YieldFrames = 61;

// How long a cooldown column rests after a press that did not land.
// Provisional (Live 1's `prio-order`).
inline constexpr long long kMark1ProbeRestFrames = 24;

// A free column pressed this long without a landing (short of mana) rests
// kMark1TurnRestFrames and the next free column takes the turn. Kept from the
// turn rule: Live 2b saw Shadow Bolt cast about every 60 frames at 144 fps,
// and the rest is 2 s at 144 fps.
inline constexpr long long kMark1TurnPatienceFrames = 90;
inline constexpr long long kMark1TurnRestFrames = 288;

// The route the automation casts by. Stats live 1 adopted the proc route
// (decision key `auto-route:`): its proc checks passed, and a toggle column
// still takes the press. The press stays the fall-back for the session when a
// proc fails.
enum class Mark1Route { Press, Proc };
inline constexpr Mark1Route kMark1AutoRoute = Mark1Route::Proc;

// The proc route's `TalentUseClass` call shape, a comma-separated template of
// its arguments in order (tokens below): Stats live 1's shape B, which cast
// both talents with `TalentUse` unmoved, with the local player's ref in place
// of the literal instance number Live 1 passed (decision key `proc-shape:`).
// An empty template refuses the proc route.
inline constexpr const char* kMark1ProcShape = "id,player,1,false,false,0,x,y";

// The base cast time, in seconds, a cast at Faster Cast Rate 0 takes. Stats
// live 1's `cast-interval` read 417 ms at Faster Cast Rate 106, 0.417 x
// (1 + 106 / 100) = 0.86 s (decision key `cast-base:`); that rate was a stale
// read, so Stats live 2 measures it again.
inline constexpr double kMark1BaseCastSeconds = 0.86;

// How often the adapter reads the player's maximum mana, Skill Haste and
// Faster Cast Rate by name, at most (the owner's proc design, Amendment 2).
inline constexpr long long kMark1StatRefreshMs = 1000;

// Skill Haste counts up to 200, 200 halving a cooldown (RDM § 7.5, measured;
// the hub's skill_stat_model).
inline constexpr double kMark1HasteCap = 200.0;
// The cast time's divisor never goes below this (the model's
// CAST_DIVISOR_FLOOR).
inline constexpr double kMark1CastDivisorFloor = 0.1;

// The mask's default entries: the bar columns other than Q, E, R and Y, on
// the reading that column c uses entry c (measured for 3 to 6 only, RDM § 7.7
// `slot-keys`), and 27 and 40, which Stats live 1's `held-entry` saw a held
// right mouse button set beside entry 2 (decision key `mask-entries:`).
inline constexpr int kMark1MaxMaskEntries = 16;
inline constexpr int kMark1HighestInputEntry = 63;
inline constexpr int kMark1DefaultMaskEntryCount = 10;
inline constexpr int kMark1DefaultMaskEntries[kMark1DefaultMaskEntryCount] = { 0, 1, 2, 7, 8, 9, 10, 11, 27, 40 };

// The research overrides' ranges (mark1probe, research build only).
inline constexpr long long kMark1MaxOverrideFrames = 600;
inline constexpr double kMark1MinBaseCastSeconds = 0.1;
inline constexpr double kMark1MaxBaseCastSeconds = 5.0;
inline constexpr int kMark1MaxProcArgs = 16;

// ---- the two kinds of column ---------------------------------------------------

enum class Mark1ColumnKind { NotEligible, Free, Cooldown };

// A free column holds a skill with no cooldown: not a toggle, and its
// talent's `abilityCooldown` readable and at most kMark1FreeCooldownFloor.
// Every other eligible column is a cooldown column, an unreadable cooldown
// included.
inline Mark1ColumnKind Mark1ClassifyColumn(bool eligible, bool toggle, bool cooldownRead, double abilityCooldown)
{
    if (!eligible) return Mark1ColumnKind::NotEligible;
    if (!toggle && cooldownRead && std::isfinite(abilityCooldown) && abilityCooldown <= kMark1FreeCooldownFloor)
        return Mark1ColumnKind::Free;
    return Mark1ColumnKind::Cooldown;
}

// An entry the mask or the lockout rests: one that read held or pressed.
// An entry that could not be read is never written, since it could not be
// put back.
inline bool Mark1EntryAsks(bool readOk, double value)
{
    return readOk && (value == kMark1InputHeld || value == kMark1InputPressed);
}

// ---- the interval (the owner's rule: cooldown plus cast time) -----------------

// The cooldown at a Skill Haste total, in seconds: 0 for a talent with no
// cooldown, otherwise abilityCooldown / (1 + min(haste, 200) / 200). A total
// so negative the rate reaches 0 never ends a cooldown in the model; here the
// rate is held at 0.01 so a clock always comes due again.
inline double Mark1ProcCooldownSeconds(double abilityCooldown, double haste)
{
    if (!std::isfinite(abilityCooldown) || !(abilityCooldown > kMark1FreeCooldownFloor)) return 0.0;
    const double h = std::isfinite(haste) ? std::min(haste, kMark1HasteCap) : 0.0;
    const double rate = std::max(1.0 + h / 200.0, 0.01);
    return abilityCooldown / rate;
}

// The cast time at a Faster Cast Rate total, in seconds: base / (1 + fcr /
// 100), the divisor never below kMark1CastDivisorFloor. Static reading of
// TalentUseSetSpeed, partly read: the shape holds, the per-talent base is not
// established (Live 1's `cast-interval`).
inline double Mark1ProcCastSeconds(double fcr, double baseCastSeconds)
{
    const double base = std::isfinite(baseCastSeconds) && baseCastSeconds > 0 ? baseCastSeconds : 0.0;
    const double f = std::isfinite(fcr) ? fcr : 0.0;
    return base / std::max(1.0 + f / 100.0, kMark1CastDivisorFloor);
}

// Milliseconds between two procs of one column.
inline double Mark1ProcIntervalMs(double abilityCooldown, double haste, double fcr, double baseCastSeconds)
{
    return 1000.0 * (Mark1ProcCooldownSeconds(abilityCooldown, haste) + Mark1ProcCastSeconds(fcr, baseCastSeconds));
}

// ---- the proc call's shape -----------------------------------------------------

// Tokens: `id` (the column's talent id), `player` (the local player instance,
// which the adapter builds as the game passes it: the runtime's own `id`
// ref, the kind `skillprobe call`'s `id:<n>` argument builds), `x` and `y`
// (the local player's position), `true`, `false`, `undefined`, and numbers. A
// literal instance number is a number, not the player.
enum class Mark1ProcToken { TalentId, Player, X, Y, True, False, Undefined, Number };

struct Mark1ProcArg {
    Mark1ProcToken token = Mark1ProcToken::Undefined;
    double number = 0;
};

struct Mark1ProcShape {
    bool ok = false;
    std::vector<Mark1ProcArg> args;   // empty: the shape is unset
    std::string bad;                  // the first token refused, by name
};

inline std::string_view Mark1Trim(std::string_view s)
{
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) s.remove_suffix(1);
    return s;
}

// A finite decimal number, the whole token.
inline bool Mark1ParseNumber(std::string_view token, double& out)
{
    if (token.empty() || token.size() > 32) return false;
    const std::string copy(token);
    char* end = nullptr;
    const double v = std::strtod(copy.c_str(), &end);
    if (end != copy.c_str() + copy.size() || !std::isfinite(v)) return false;
    out = v;
    return true;
}

// Parses a template. An empty template parses to an unset shape (ok, no
// argument); an unknown or empty token refuses the whole template, named in
// `bad`.
inline Mark1ProcShape Mark1ParseProcShape(std::string_view text)
{
    Mark1ProcShape shape;
    text = Mark1Trim(text);
    if (text.empty()) { shape.ok = true; return shape; }
    while (true) {
        const size_t comma = text.find(',');
        const std::string_view token = Mark1Trim(text.substr(0, comma));
        Mark1ProcArg arg;
        if (token == "id") arg.token = Mark1ProcToken::TalentId;
        else if (token == "player") arg.token = Mark1ProcToken::Player;
        else if (token == "x") arg.token = Mark1ProcToken::X;
        else if (token == "y") arg.token = Mark1ProcToken::Y;
        else if (token == "true") arg.token = Mark1ProcToken::True;
        else if (token == "false") arg.token = Mark1ProcToken::False;
        else if (token == "undefined") arg.token = Mark1ProcToken::Undefined;
        else if (Mark1ParseNumber(token, arg.number)) arg.token = Mark1ProcToken::Number;
        else {
            shape.args.clear();
            shape.bad = token.empty() ? std::string("(empty)") : std::string(token);
            return shape;
        }
        if ((int)shape.args.size() >= kMark1MaxProcArgs) {
            shape.args.clear();
            shape.bad = "(more than 16 arguments)";
            return shape;
        }
        shape.args.push_back(arg);
        if (comma == std::string_view::npos) break;
        text = text.substr(comma + 1);
    }
    shape.ok = true;
    return shape;
}

inline std::string Mark1ProcTokenText(const Mark1ProcArg& a)
{
    switch (a.token) {
        case Mark1ProcToken::TalentId: return "id";
        case Mark1ProcToken::Player:   return "player";
        case Mark1ProcToken::X:        return "x";
        case Mark1ProcToken::Y:        return "y";
        case Mark1ProcToken::True:     return "true";
        case Mark1ProcToken::False:    return "false";
        case Mark1ProcToken::Number: {
            char buf[40];
            std::snprintf(buf, sizeof(buf), "%.10g", a.number);
            return buf;
        }
        default:                       return "undefined";
    }
}

// The parsed shape, one token per argument, comma-separated ("unset" when
// empty).
inline std::string Mark1FormatProcShape(const std::vector<Mark1ProcArg>& args)
{
    if (args.empty()) return "unset";
    std::string out;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i) out += ",";
        out += Mark1ProcTokenText(args[i]);
    }
    return out;
}

// One argument of the proc call, as the adapter turns it into a runtime value.
// `Player` carries no value: the adapter builds the local player's ref itself
// and refuses the call when it cannot.
struct Mark1ProcValue {
    enum class Kind { Number, Bool, Undefined, Player } kind = Kind::Undefined;
    double number = 0;
    bool flag = false;
};

// Builds the call's arguments from the shape. Refuses (false, `why` set) an
// unset shape, or a talent id or position that is not a finite number: a
// refused argument is never passed to the game.
inline bool Mark1BuildProcArgs(const std::vector<Mark1ProcArg>& shape, double talentId, double x, double y,
                               std::vector<Mark1ProcValue>& out, std::string& why)
{
    out.clear();
    if (shape.empty()) { why = "shape unset"; return false; }
    for (const Mark1ProcArg& a : shape) {
        Mark1ProcValue v;
        switch (a.token) {
            case Mark1ProcToken::TalentId:
                if (!std::isfinite(talentId) || !(talentId > 0)) { why = "talent id unreadable"; out.clear(); return false; }
                v.kind = Mark1ProcValue::Kind::Number; v.number = talentId; break;
            case Mark1ProcToken::Player: v.kind = Mark1ProcValue::Kind::Player; break;
            case Mark1ProcToken::X:
                if (!std::isfinite(x)) { why = "x unreadable"; out.clear(); return false; }
                v.kind = Mark1ProcValue::Kind::Number; v.number = x; break;
            case Mark1ProcToken::Y:
                if (!std::isfinite(y)) { why = "y unreadable"; out.clear(); return false; }
                v.kind = Mark1ProcValue::Kind::Number; v.number = y; break;
            case Mark1ProcToken::True:   v.kind = Mark1ProcValue::Kind::Bool; v.flag = true; break;
            case Mark1ProcToken::False:  v.kind = Mark1ProcValue::Kind::Bool; v.flag = false; break;
            case Mark1ProcToken::Number: v.kind = Mark1ProcValue::Kind::Number; v.number = a.number; break;
            default:                     v.kind = Mark1ProcValue::Kind::Undefined; break;
        }
        out.push_back(v);
    }
    return true;
}

// ---- the mask's entries ----------------------------------------------------------

struct Mark1MaskSetting {
    bool on = true;   // off: the entries are still read (held-seen counts), never written
    int count = kMark1DefaultMaskEntryCount;
    int entry[kMark1MaxMaskEntries] = { 0, 1, 2, 7, 8, 9, 10, 11, 27, 40 };
};

// `off`, `default` or a comma-separated list of entries. An automated
// column's own entry (3 to 6) is the lockout's, never the mask's; an entry
// above kMark1HighestInputEntry, a repeat or more than 16 entries refuses
// the list.
inline bool Mark1ParseMaskSetting(std::string_view text, Mark1MaskSetting& out, std::string& why)
{
    text = Mark1Trim(text);
    Mark1MaskSetting m;
    if (text == "default") { out = m; return true; }
    if (text == "off") { m.on = false; out = m; return true; }
    m.count = 0;
    if (text.empty()) { why = "no entry"; return false; }
    while (true) {
        const size_t comma = text.find(',');
        const std::string_view token = Mark1Trim(text.substr(0, comma));
        double v = 0;
        if (!Mark1ParseNumber(token, v) || v != std::floor(v) || v < 0 || v > kMark1HighestInputEntry) {
            why = "entry " + (token.empty() ? std::string("(empty)") : std::string(token)) + " is not 0-63";
            return false;
        }
        const int e = (int)v;
        if (Mark1IsAutomatedColumn(e)) { why = "entry " + std::to_string(e) + " is an automated column's"; return false; }
        for (int k = 0; k < m.count; ++k)
            if (m.entry[k] == e) { why = "entry " + std::to_string(e) + " twice"; return false; }
        if (m.count >= kMark1MaxMaskEntries) { why = "more than 16 entries"; return false; }
        m.entry[m.count++] = e;
        if (comma == std::string_view::npos) break;
        text = text.substr(comma + 1);
    }
    out = m;
    return true;
}

// `off`, or the entries in force.
inline std::string Mark1FormatMaskSetting(const Mark1MaskSetting& m)
{
    if (!m.on) return "off";
    std::string out;
    for (int k = 0; k < m.count; ++k) {
        if (k) out += ",";
        out += std::to_string(m.entry[k]);
    }
    return out;
}

// ---- one gate call's input plan -------------------------------------------------

// Which entries a gate call reads, and what it may write to them. The
// lockout's entries (each automated column's own, `lockMask` the border's
// columns) come first, then the press, then the mask's (only on a call that
// presses). Inactive, or nothing locked and nothing pressed: no entry.
struct Mark1InputPlan {
    int lockCount = 0;
    int lockEntry[kMark1ColumnCount] = {};
    int pressCount = 0;
    int pressEntry[kMark1ColumnCount] = {};
    double pressValue[kMark1ColumnCount] = {};
    int maskCount = 0;
    int maskEntry[kMark1MaxMaskEntries] = {};
    bool maskWrites = false;   // the mask is on: an asking entry is written, not only read
};

// `pressed[i]` and `value[i]`: column kMark1FirstColumn + i is pressed with
// that value. `lockMask` bit i: column kMark1FirstColumn + i is automated (the
// border's), and with the lockout on its own entry is rested if it asks.
inline Mark1InputPlan Mark1PlanInputs(bool active, bool lockout, unsigned lockMask,
                                      const bool (&pressed)[kMark1ColumnCount],
                                      const double (&value)[kMark1ColumnCount],
                                      const Mark1MaskSetting& mask)
{
    Mark1InputPlan p;
    if (!active) return p;
    if (lockout)
        for (int i = 0; i < kMark1ColumnCount; ++i)
            if (lockMask & (1u << i)) p.lockEntry[p.lockCount++] = Mark1InputEntryForColumn(kMark1FirstColumn + i);
    for (int i = 0; i < kMark1ColumnCount; ++i) {
        if (!pressed[i]) continue;
        p.pressEntry[p.pressCount] = Mark1InputEntryForColumn(kMark1FirstColumn + i);
        p.pressValue[p.pressCount] = value[i];
        ++p.pressCount;
    }
    if (p.pressCount > 0) {
        for (int k = 0; k < mask.count && k < kMark1MaxMaskEntries; ++k) p.maskEntry[p.maskCount++] = mask.entry[k];
        p.maskWrites = mask.on;
    }
    return p;
}

// Of `count` entries read (`readOk`, `values`), which the plan rests: bit k
// for entry k that asks. Used for the lockout's and the mask's entries alike;
// the mask's only when `writes`.
inline unsigned Mark1EntriesToRest(int count, const bool* readOk, const double* values, bool writes = true)
{
    if (!writes) return 0u;
    unsigned bits = 0u;
    for (int k = 0; k < count && k < 32; ++k)
        if (Mark1EntryAsks(readOk[k], values[k])) bits |= 1u << k;
    return bits;
}

// The bar icons drawn grey, in the game's own "cannot use" tint: the border's
// columns, which are the automated ones (neither empty nor an aura), on both
// routes.
inline constexpr unsigned Mark1GreyMask(unsigned borderMask) { return borderMask; }

// ---- the stat reads --------------------------------------------------------------

// What the adapter reads by name through ReturnSpecificStat (dispatcher ids):
// maximum mana 37, Skill Haste 103, Faster Cast Rate 106.
enum class Mark1Stat : int { MaxMana = 0, Haste = 1, FasterCastRate = 2 };
inline constexpr int kMark1StatCount = 3;
inline constexpr int kMark1StatIds[kMark1StatCount] = { 37, 103, 106 };

// At most one read every kMark1StatRefreshMs, at the point of use. A failed
// read keeps the last value and counts; before any read a value is 0 (for
// haste and Faster Cast Rate, the longest interval) and shows as `-`.
class Mark1StatCache {
public:
    bool Due(long long nowMs) const
    {
        return !m_Tried || nowMs < m_LastMs || nowMs - m_LastMs >= kMark1StatRefreshMs;
    }
    void Begin(long long nowMs) { m_Tried = true; m_LastMs = nowMs; }
    void Store(Mark1Stat s, bool ok, double value)
    {
        const int i = (int)s;
        if (i < 0 || i >= kMark1StatCount) return;
        if (ok && std::isfinite(value)) { m_Have[i] = true; m_Value[i] = value; }
        else ++m_Fails;
    }
    bool Have(Mark1Stat s) const { const int i = (int)s; return i >= 0 && i < kMark1StatCount && m_Have[i]; }
    double Value(Mark1Stat s) const { return Have(s) ? m_Value[(int)s] : 0.0; }
    long Fails() const { return m_Fails.load(); }

private:
    bool m_Tried = false;
    long long m_LastMs = 0;
    bool m_Have[kMark1StatCount] = {};
    double m_Value[kMark1StatCount] = {};
    std::atomic<long> m_Fails{ 0 };
};

// ---- the settings in force (the research overrides) ------------------------------

struct Mark1PrioritySettings {
    double holdValue = kMark1FreePressValue;
    Mark1MaskSetting mask;
    long long yieldFrames = kMark1YieldFrames;
    long long restFrames = kMark1ProbeRestFrames;
    Mark1Route route = kMark1AutoRoute;
    std::vector<Mark1ProcArg> shape = Mark1ParseProcShape(kMark1ProcShape).args;
    bool lockout = true;
    double baseCastSeconds = kMark1BaseCastSeconds;
};

enum class Mark1RouteReply { Ok, ShapeUnset };

// ---- one gate call, as the core sees it ------------------------------------------

struct Mark1PriorityColumn {
    Mark1ColumnKind kind = Mark1ColumnKind::NotEligible;
    bool   toggle = false;
    bool   cooldownRead = false;
    double abilityCooldown = 0;   // seconds; meaningful when cooldownRead
};

// The millisecond clock and the stats the interval is computed from, passed
// in by the adapter. With no clock nothing is fired this call.
struct Mark1GateClock {
    bool haveMs = false;
    long long nowMs = 0;
    double haste = 0;
    double fcr = 0;
};

enum class Mark1PriorityWhy {
    NotEligible,
    Press,           // pressed this call
    Yield,           // the yield window after a landing
    Resting,         // this column rests
    OtherPressing,   // another column is pressed this call
    Proc,            // fired this call on the proc route
    ProcWait,        // a proc column whose clock has not run its interval
};

struct Mark1PriorityDecision {
    int pressColumn = 0;     // 0: none
    bool pressFree = false;  // pressed as a free column (with the hold value)
    int procColumn = 0;      // 0: none
    unsigned procMask = 0;   // bit i: column kMark1FirstColumn + i is on the proc route this call
    bool yielding = false;
    Mark1PriorityWhy why[kMark1ColumnCount] = {
        Mark1PriorityWhy::NotEligible, Mark1PriorityWhy::NotEligible,
        Mark1PriorityWhy::NotEligible, Mark1PriorityWhy::NotEligible,
    };
};

// The second stat line's counters that are the core's.
struct Mark1PriorityCounters {
    long masked = 0;     // presses that masked at least one entry
    long heldSeen = 0;   // presses during which a mask entry read 3 or 4
    long procs = 0;      // proc calls made
    long procFail = 0;   // proc calls refused or thrown
    long locked = 0;     // gate calls on which the lockout rested at least one entry
};

class Mark1PriorityCore {
public:
    // ---- the research overrides; each refuses a value out of range ----
    const Mark1PrioritySettings& Settings() const { return m_Settings; }

    bool SetHoldValue(double v)
    {
        if (v != kMark1InputHeld && v != kMark1InputPressed) return false;
        m_Settings.holdValue = v;
        return true;
    }
    void SetMask(const Mark1MaskSetting& m) { m_Settings.mask = m; }
    bool SetYieldFrames(long long f)
    {
        if (f < 0 || f > kMark1MaxOverrideFrames) return false;
        m_Settings.yieldFrames = f;
        return true;
    }
    bool SetRestFrames(long long f)
    {
        if (f < 0 || f > kMark1MaxOverrideFrames) return false;
        m_Settings.restFrames = f;
        return true;
    }
    // Proc with no shape set is refused and the press stays in force; a
    // route set again clears the session's fall-back to the press.
    Mark1RouteReply SetRoute(Mark1Route r)
    {
        if (r == Mark1Route::Proc && m_Settings.shape.empty()) { m_Settings.route = Mark1Route::Press; return Mark1RouteReply::ShapeUnset; }
        m_Settings.route = r;
        m_ProcFailed = false;
        return Mark1RouteReply::Ok;
    }
    // A parsed template; `clear` (an empty shape) also returns the route to
    // the press.
    bool SetShape(const Mark1ProcShape& s)
    {
        if (!s.ok) return false;
        m_Settings.shape = s.args;
        if (s.args.empty()) m_Settings.route = Mark1Route::Press;
        return true;
    }
    void SetLockout(bool on) { m_Settings.lockout = on; }
    bool SetBaseCastSeconds(double s)
    {
        if (!std::isfinite(s) || s < kMark1MinBaseCastSeconds || s > kMark1MaxBaseCastSeconds) return false;
        m_Settings.baseCastSeconds = s;
        return true;
    }

    // The route in force: the proc route needs a shape and no failure this
    // session.
    Mark1Route RouteInForce() const
    {
        return m_Settings.route == Mark1Route::Proc && !m_Settings.shape.empty() && !m_ProcFailed
            ? Mark1Route::Proc : Mark1Route::Press;
    }
    bool ProcFailedThisSession() const { return m_ProcFailed; }

    // A column the proc route fires: on that route, eligible, not a toggle,
    // and its cooldown read (an unreadable cooldown has no interval, so it
    // keeps the press).
    bool IsProcColumn(const Mark1PriorityColumn& c) const
    {
        return RouteInForce() == Mark1Route::Proc && c.kind != Mark1ColumnKind::NotEligible
            && !c.toggle && c.cooldownRead && std::isfinite(c.abilityCooldown);
    }

    // This call's decision. Counts yielded= for a free column whose patience
    // ran out here.
    Mark1PriorityDecision Decide(long long frame, const Mark1PriorityColumn (&cols)[kMark1ColumnCount], const Mark1GateClock& clock)
    {
        Mark1PriorityDecision d;
        bool proc[kMark1ColumnCount] = {};
        bool press[kMark1ColumnCount] = {};
        for (int i = 0; i < kMark1ColumnCount; ++i) {
            proc[i] = IsProcColumn(cols[i]);
            press[i] = !proc[i] && cols[i].kind != Mark1ColumnKind::NotEligible;
            if (proc[i]) d.procMask |= 1u << i;
        }
        m_ProcColumns.store(PopCount(d.procMask));

        // The proc route: the first due column, in Q, E, R, Y order.
        if (d.procMask && clock.haveMs)
            for (int i = 0; i < kMark1ColumnCount; ++i)
                if (proc[i] && Due(i, clock.nowMs)) { d.procColumn = kMark1FirstColumn + i; break; }
        for (int i = 0; i < kMark1ColumnCount; ++i)
            if (proc[i]) d.why[i] = d.procColumn == kMark1FirstColumn + i ? Mark1PriorityWhy::Proc : Mark1PriorityWhy::ProcWait;

        // The press route's cycle.
        if (Yielding(frame)) {
            d.yielding = true;
            for (int i = 0; i < kMark1ColumnCount; ++i) if (press[i]) d.why[i] = Mark1PriorityWhy::Yield;
            return d;
        }
        int pick = -1;
        for (int i = 0; i < kMark1ColumnCount && pick < 0; ++i)
            if (press[i] && cols[i].kind == Mark1ColumnKind::Cooldown && !Resting(i, frame)) pick = i;
        if (pick < 0) {
            pick = PickFree(frame, press, cols);
            if (pick >= 0) d.pressFree = true;
        }
        if (pick >= 0) d.pressColumn = kMark1FirstColumn + pick;
        for (int i = 0; i < kMark1ColumnCount; ++i) {
            if (!press[i]) continue;
            if (i == pick) d.why[i] = Mark1PriorityWhy::Press;
            else d.why[i] = Resting(i, frame) ? Mark1PriorityWhy::Resting : Mark1PriorityWhy::OtherPressing;
        }
        return d;
    }

    // What the gate call that wrote `column`'s press saw: whether a cast
    // landed. A landing opens the yield window and, for a free column, hands
    // the turn to the next free column; a cooldown press that did not land
    // rests that column for the probe rest (yielded=).
    void Landed(int column, long long frame, bool landed, bool free)
    {
        if (!Mark1IsAutomatedColumn(column)) return;
        const int i = column - kMark1FirstColumn;
        if (landed) {
            m_Yielding = true;
            m_LandedFrame = frame;
            m_Resting[i] = false;
            if (free) m_FreeTurn = (i + 1) % kMark1ColumnCount;
            m_RunColumn = -1;   // any landing starts the free column's patience afresh
            return;
        }
        if (!free) StartRest(i, frame, m_Settings.restFrames);
    }

    // A proc fired: its clock runs its interval from now.
    void ProcFired(int column, long long nowMs, double intervalMs)
    {
        if (!Mark1IsAutomatedColumn(column)) return;
        const int i = column - kMark1FirstColumn;
        m_HasClock[i] = true;
        m_FiredAt[i] = nowMs;
        m_NextDue[i] = nowMs + (long long)std::llround(std::isfinite(intervalMs) && intervalMs > 0 ? intervalMs : 0.0);
    }
    // A proc refused or thrown: the route returns to the press for the session.
    void ProcFailed() { m_ProcFailed = true; }

    // A column's interval with the settings in force.
    double IntervalMs(const Mark1PriorityColumn& c, const Mark1GateClock& clock) const
    {
        return Mark1ProcIntervalMs(c.abilityCooldown, clock.haste, clock.fcr, m_Settings.baseCastSeconds);
    }

    // A room (Player_obj gets a new instance id in each, so the frame state
    // of the last one means nothing) or the bonus active again: the rests,
    // the yield and the free columns' turn are forgotten. Every proc clock is
    // kept, so a cooldown skill does not proc again on entering a room before
    // its interval has run (round 0's review). The settings, the counters and
    // a proc failure this session are kept too.
    void ForgetRoom()
    {
        for (int i = 0; i < kMark1ColumnCount; ++i) {
            m_Resting[i] = false; m_RestStart[i] = 0; m_RestFrames[i] = 0;
        }
        m_Yielding = false;
        m_LandedFrame = 0;
        m_FreeTurn = 0;
        m_RunColumn = -1;
        m_RunStart = 0;
        m_ProcColumns.store(0);
    }

    // A new character: everything ForgetRoom forgets, and every proc clock.
    void Reset()
    {
        ForgetRoom();
        for (int i = 0; i < kMark1ColumnCount; ++i) { m_HasClock[i] = false; m_FiredAt[i] = 0; m_NextDue[i] = 0; }
    }

    // Inside column index i's rest. A frame counter that went backwards ends
    // the rest rather than leaving it open-ended.
    bool Resting(int i, long long frame) const
    {
        if (i < 0 || i >= kMark1ColumnCount || !m_Resting[i]) return false;
        if (frame < m_RestStart[i]) return false;
        return frame - m_RestStart[i] < m_RestFrames[i];
    }
    bool Yielding(long long frame) const
    {
        if (!m_Yielding || frame < m_LandedFrame) return false;
        return frame - m_LandedFrame < m_Settings.yieldFrames;
    }

    // ---- counters ----
    long Yielded(int i) const { return i >= 0 && i < kMark1ColumnCount ? m_Yielded[i].load() : 0; }
    int ProcColumnCount() const { return m_ProcColumns.load(); }
    void CountMasked() { ++m_Masked; }
    void CountHeldSeen() { ++m_HeldSeen; }
    void CountLocked() { ++m_Locked; }
    void CountProc() { ++m_Procs; }
    void CountProcFail() { ++m_ProcFail; }
    Mark1PriorityCounters Counters() const
    {
        Mark1PriorityCounters c;
        c.masked = m_Masked.load();
        c.heldSeen = m_HeldSeen.load();
        c.procs = m_Procs.load();
        c.procFail = m_ProcFail.load();
        c.locked = m_Locked.load();
        return c;
    }

private:
    static int PopCount(unsigned m) { int n = 0; for (; m; m &= m - 1) ++n; return n; }

    void StartRest(int i, long long frame, long long frames)
    {
        m_Resting[i] = true;
        m_RestStart[i] = frame;
        m_RestFrames[i] = frames;
        ++m_Yielded[i];
    }

    // Column index i's proc clock has run its interval (never fired: due; a
    // clock that went backwards: due).
    bool Due(int i, long long nowMs) const
    {
        if (!m_HasClock[i]) return true;
        if (nowMs < m_FiredAt[i]) return true;
        return nowMs >= m_NextDue[i];
    }

    // The free column whose turn it is, -1 when none is free to press. A
    // holder whose run lasted the patience rests and the turn moves on.
    int PickFree(long long frame, const bool (&press)[kMark1ColumnCount], const Mark1PriorityColumn (&cols)[kMark1ColumnCount])
    {
        auto candidate = [&](int i) {
            return press[i] && cols[i].kind == Mark1ColumnKind::Free && !Resting(i, frame);
        };
        if (m_RunColumn >= 0 && candidate(m_RunColumn)) {
            if (frame < m_RunStart) m_RunStart = frame;
            if (frame - m_RunStart >= kMark1TurnPatienceFrames) {
                StartRest(m_RunColumn, frame, kMark1TurnRestFrames);
                m_FreeTurn = (m_RunColumn + 1) % kMark1ColumnCount;
                m_RunColumn = -1;
            }
        }
        int holder = -1;
        if (m_RunColumn >= 0 && candidate(m_RunColumn)) holder = m_RunColumn;
        for (int k = 0; k < kMark1ColumnCount && holder < 0; ++k) {
            const int i = (m_FreeTurn + k) % kMark1ColumnCount;
            if (candidate(i)) holder = i;
        }
        if (holder != m_RunColumn) { m_RunColumn = holder; m_RunStart = frame; }
        return holder;
    }

    Mark1PrioritySettings m_Settings;
    bool m_ProcFailed = false;

    bool m_Resting[kMark1ColumnCount] = {};
    long long m_RestStart[kMark1ColumnCount] = {};
    long long m_RestFrames[kMark1ColumnCount] = {};
    bool m_Yielding = false;
    long long m_LandedFrame = 0;
    int m_FreeTurn = 0;      // where the search for the next free column starts
    int m_RunColumn = -1;    // the free column pressed until it lands; -1: none
    long long m_RunStart = 0;

    bool m_HasClock[kMark1ColumnCount] = {};
    long long m_FiredAt[kMark1ColumnCount] = {};
    long long m_NextDue[kMark1ColumnCount] = {};

    std::atomic<long> m_Yielded[kMark1ColumnCount] = {};
    std::atomic<int> m_ProcColumns{ 0 };
    std::atomic<long> m_Masked{ 0 }, m_HeldSeen{ 0 }, m_Locked{ 0 }, m_Procs{ 0 }, m_ProcFail{ 0 };
};

} // namespace ForgePact
