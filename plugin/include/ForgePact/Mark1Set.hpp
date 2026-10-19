#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace ForgePact::Mark1Set {

// ---- Mark 1: the set's decision core (ForgePact #166, phase 2) ----------------
//
// Four built-in Heroic pieces: helmet, body armor, boots and gloves. While all
// four are worn, each in its own slot, and the "Mark 1 set" switch is on, the
// set bonus is active: Mark1Automation.hpp casts the bar's Q, E, R and Y,
// Mark1Jump.hpp scales the jump and Mark1Movement.hpp halves the walk.
//
// This header decides; it never touches the game. It holds:
//   - the piece table and recognition. A piece is saved as its base item of
//     its own class with a fixed seed `a` in ForgePact's built-in band; the
//     seed layout is a save format after release.
//   - the dressing's constants (name, rarity, mechanic, affix rows). The
//     adapter writes them through a recognition-only CustomForgeEntry, as the
//     flight charms' are.
//   - the full-set read's verdict, from the four worn slots the adapter reads,
//     refreshed at most once every kRefreshFrames at the point of use (inside
//     the CheckTalentUse detour) and on `mark1set stat`.
//   - the install state: armed by `mark1set bonus 1`, installed once a
//     character exists (the guide's Known Limitations item 8, as jumpscenery).
//   - the `mark1set` command's forms and its `stat` line, whose automation,
//     jump, art and movement fields the other cores' counters fill.
//   - the pieces' art rule: armed by the session's first switch-on, and when
//     a dressing may write our icon and the ground consumer may go in.
//
// It is game-independent by contract - numbers and strings, never an instance
// or an RValue - so tests/mark1_set_harness.cpp compiles it whole. The adapter
// in ModuleMain.cpp resolves every script and global by name and hands the
// worn slots in through ReadWornFn, as MinerHelmetMod.hpp's ReadWorn reads the
// helmet. The worn gear is global.equippedItems[global.mplr][0][slot], as
// fingerprint strings (the hub's docs/RUNTIME_DATA_MODELS.md, "Equipment Slots
// Layout").

// ---- the four pieces ----------------------------------------------------------

enum class Piece : int { Helmet, Body, Boots, Gloves };
inline constexpr int kPieceCount = 4;

struct PieceRow {
    Piece            piece;
    std::string_view key;       // `mark1probe spawn`'s word
    std::string_view name;      // the display name dressing writes
    int              itemType;  // the class, HeroSiege::Items::ItemType
    int              slot;      // the worn slot, equippedItems[mplr][0][slot]
    long long        seed;      // the definition's `a`; one per piece, no rolls
    int              baseB;     // the base item's `b` a new piece is built on
};

// Classes: Helmet 0, Body 1, Boots 2, Gloves 4 (hs-game-sdk's item_type.hpp;
// the behavior test pins them against it). The helmet's base is the one the
// signature helmets use; the other three are provisional until Live 1's
// base-check measures them. `b` is not part of recognition, so changing a base
// later does not orphan a piece already made.
inline constexpr PieceRow kPieces[kPieceCount] = {
    { Piece::Helmet, "helmet", "Mark 1 Helmet",     0, 0, 777500, 7 },
    { Piece::Body,   "body",   "Mark 1 Body Armor", 1, 1, 777501, 0 },
    { Piece::Boots,  "boots",  "Mark 1 Boots",      2, 2, 777502, 0 },
    { Piece::Gloves, "gloves", "Mark 1 Gloves",     4, 3, 777503, 0 },
};

// ForgePact's built-in seed band. 777500..777503 sits clear of the ranges in
// use when the table was written (777001-777003, 777100-777109,
// 777200-777343, 777400-777490, and 779000 up).
inline constexpr long long kBuiltInSeedLo = 777000;
inline constexpr long long kBuiltInSeedHi = 777999;

inline constexpr bool SeedsDistinctInBand()
{
    for (int i = 0; i < kPieceCount; ++i) {
        if (kPieces[i].seed < kBuiltInSeedLo || kPieces[i].seed > kBuiltInSeedHi) return false;
        if (kPieces[i].slot != i || static_cast<int>(kPieces[i].piece) != i) return false;
        for (int k = i + 1; k < kPieceCount; ++k)
            if (kPieces[i].seed == kPieces[k].seed) return false;
    }
    return true;
}
static_assert(SeedsDistinctInBand(), "the four seeds are distinct and inside 777000..777999, the table in slot order");

// ---- the dressing --------------------------------------------------------------

inline constexpr int kRarity = 9;                       // Heroic (itemInfoStruct "27")
inline constexpr std::string_view kMechanic = "mark1";  // fp_mechanic
// The gold affix rows: the first reads exactly "Mark 1", the rest the full-set
// bonus in player words. No stats, no tier and no lore are written.
inline constexpr int kAffixRowCount = 3;
inline constexpr std::string_view kAffixRows[kAffixRowCount] = {
    "Mark 1",
    "Full set: Q, E, R and Y cast themselves",
    "Full set: half walk speed, triple jump",
};

inline const PieceRow& RowOf(Piece p)
{
    return kPieces[static_cast<int>(p)];
}

inline bool PieceFromKey(std::string_view key, Piece& out)
{
    for (const PieceRow& r : kPieces) {
        if (r.key != key) continue;
        out = r.piece;
        return true;
    }
    return false;
}

// A whole number a real holds exactly (|v| <= 2^53 - 1).
inline bool AsWhole(double v, long long& out)
{
    if (!std::isfinite(v) || std::floor(v) != v || std::fabs(v) > 9007199254740991.0) return false;
    out = static_cast<long long>(v);
    return true;
}

// Recognition: the item's `itemType` is a piece's class, its definition's `a`
// is that piece's seed and its `j` is 0 (not a unique). `b` and `c` are read
// by the adapter and deliberately ignored here.
inline bool IsPiece(double itemType, double a, double /*b*/, double /*c*/, double j, Piece& out)
{
    long long tt = 0, aa = 0, jj = 0;
    if (!AsWhole(itemType, tt) || !AsWhole(a, aa) || !AsWhole(j, jj) || jj != 0) return false;
    for (const PieceRow& r : kPieces) {
        if (r.itemType != tt || r.seed != aa) continue;
        out = r.piece;
        return true;
    }
    return false;
}

// ---- the full-set read ----------------------------------------------------------

// One worn slot as the adapter read it: empty (no fingerprint), an item with
// its class and definition fields, or unreadable (the fingerprint or the item
// behind it could not be resolved).
enum class SlotRead : int { Empty, Item, Unreadable };

struct WornSlot {
    SlotRead read = SlotRead::Unreadable;
    double   itemType = 0;
    double   a = 0;
    double   b = 0;
    double   c = 0;
    double   j = 0;
};

// Worn slots 0..3 (helmet, body, boots, gloves), indexed by slot.
using WornSlots = std::array<WornSlot, kPieceCount>;

struct SetRead {
    int  matched = 0;     // slots holding their own piece
    int  unreadable = 0;  // slots that could not be read
    bool full = false;
};

// Full = each slot holds its own piece. A piece in another piece's slot does
// not count, and an unreadable slot is not full.
inline SetRead Evaluate(const WornSlots& slots)
{
    SetRead r;
    for (int i = 0; i < kPieceCount; ++i) {
        const WornSlot& s = slots[static_cast<size_t>(i)];
        if (s.read == SlotRead::Unreadable) {
            ++r.unreadable;
            continue;
        }
        Piece p = Piece::Helmet;
        if (s.read == SlotRead::Item && IsPiece(s.itemType, s.a, s.b, s.c, s.j, p) && RowOf(p).slot == i) ++r.matched;
    }
    r.full = r.matched == kPieceCount && r.unreadable == 0;
    return r;
}

// The ` set=` field: full, partial(<n>) or none.
inline std::string SetText(const SetRead& r)
{
    if (r.full) return "full";
    if (r.matched == 0) return "none";
    return "partial(" + std::to_string(r.matched) + ")";
}

// ---- the `mark1set stat` line -------------------------------------------------------

inline constexpr int kColumnCount = 4;  // bar columns 3..6: Q, E, R, Y

// One automated column's counters, filled from Mark1Automation.hpp's. The four
// failures are never folded into a skip: `presses=0` has to say whether a read
// or a write failed, or every column was skipped.
struct ColumnCounts {
    uint64_t presses = 0;
    uint64_t skipEmpty = 0;           // no skill on the column
    uint64_t skipAura = 0;            // an aura, never automated
    uint64_t skipToggleOn = 0;        // a toggle already on
    uint64_t skipSettle = 0;          // a toggle inside its settle window
    uint64_t skipNoState = 0;         // a toggle whose state could not be read
    uint64_t unreadable = 0;          // the bar column itself could not be read
    uint64_t originalUnreadable = 0;  // a press planned, the entry's value unreadable: nothing pressed
    uint64_t pressWriteFailed = 0;    // the press could not be written
    uint64_t restoreFailed = 0;       // the original value could not be put back
};

// The jump's counters and its natural maximum M, filled from Mark1Jump.hpp's.
// The refusals on a successful read (rejected*) are per candidate; a failed
// read (unread*) is counted by what was being read, never as the game's answer.
struct JumpCounts {
    uint64_t    granted = 0;
    uint64_t    backedOff = 0;
    uint64_t    refusedLanding = 0;
    uint64_t    refusedGate = 0;
    uint64_t    refusedCursor = 0;
    double      max = 175;             // M
    std::string maxSource = "start";   // cap, reach or start
    uint64_t    refusedUnreadable = 0; // vanilla: a read the decision needed failed
    uint64_t    jumps = 0;
    uint64_t    scaled = 0;            // skillsLeap calls whose displacement was rewritten
    uint64_t    clamped = 0;
    uint64_t    answered = 0;
    uint64_t    excluded = 0;
    uint64_t    rejectedRoom = 0;
    uint64_t    rejectedFamily = 0;
    uint64_t    rejectedGrid = 0;
    uint64_t    rejectedGate = 0;
    uint64_t    unreadTakeoff = 0;
    uint64_t    unreadCursor = 0;
    uint64_t    unreadRoom = 0;
    uint64_t    unreadFamily = 0;
    uint64_t    unreadGrid = 0;
    uint64_t    unreadGate = 0;
};

// A movement lever's count, or `unavailable` when its route is `none`.
struct LeverCount {
    bool     available = false;
    uint64_t count = 0;
};

// Every field of the line. A default StatFields prints the fresh session's
// line; Bonus::FillStat fills the set's part and the adapter the rest.
struct StatFields {
    bool         bonus = false;
    bool         active = false;
    std::string  install = "not-armed";
    SetRead      set;
    uint64_t     unreadable = 0;
    uint64_t     reads = 0;
    ColumnCounts columns[kColumnCount];
    uint64_t     gateCalls = 0;   // the gate's entries for the local player while the bonus was active
    uint64_t     gateOther = 0;   // its entries with the switch on whose self was not the local player
    uint64_t     threw = 0;       // calls whose original threw
    JumpCounts   jump;
    int          iconsLoaded = 0;
    int          iconsEmbedded = 0;
    bool         artArmed = false;           // the art rule's sticky flag (Bonus::ArtArmed)
    uint64_t     ground = 0;                 // ground items given our icon
    std::string  groundRoute = "none";       // the shared LootGroundInit detour's route
    LeverCount   speed;
    LeverCount   penalty;
};

// A number as the line prints it: whole numbers bare, others to one decimal.
inline std::string NumberText(double v)
{
    if (!std::isfinite(v)) return "?";
    long long whole = 0;
    if (AsWhole(v, whole)) return std::to_string(whole);
    const long long tenths = std::llround(v * 10.0);
    if (tenths % 10 == 0) return std::to_string(tenths / 10);
    const long long magnitude = tenths < 0 ? -tenths : tenths;
    return std::string(tenths < 0 ? "-" : "") + std::to_string(magnitude / 10) + "." + std::to_string(magnitude % 10);
}

inline std::string ColumnsText(const ColumnCounts (&c)[kColumnCount], uint64_t ColumnCounts::*field)
{
    std::string s;
    for (int i = 0; i < kColumnCount; ++i) {
        if (i) s += "/";
        s += std::to_string(c[i].*field);
    }
    return s;
}

inline std::string LeverText(const LeverCount& l)
{
    return l.available ? std::to_string(l.count) : std::string("unavailable");
}

inline std::string JoinCounts(std::initializer_list<uint64_t> counts)
{
    std::string s;
    for (uint64_t c : counts) {
        if (!s.empty()) s += "/";
        s += std::to_string(c);
    }
    return s;
}

// `mark1set stat`. The line begins `mark1set: bonus=`; per-column counters are
// Q/E/R/Y joined by `/`. A field added to a group goes after that group's
// existing fields, so a check that reads one by name keeps finding it.
inline std::string StatLine(const StatFields& f)
{
    std::string s = "mark1set: bonus=" + std::string(f.bonus ? "on" : "off");
    s += " active=" + std::string(f.active ? "yes" : "no");
    s += " install=" + f.install;
    s += " set=" + SetText(f.set);
    s += " unreadable=" + std::to_string(f.unreadable);
    s += " reads=" + std::to_string(f.reads);
    s += " | automation presses=" + ColumnsText(f.columns, &ColumnCounts::presses);
    s += " skip-empty=" + ColumnsText(f.columns, &ColumnCounts::skipEmpty);
    s += " skip-aura=" + ColumnsText(f.columns, &ColumnCounts::skipAura);
    s += " skip-toggle-on=" + ColumnsText(f.columns, &ColumnCounts::skipToggleOn);
    s += " skip-settle=" + ColumnsText(f.columns, &ColumnCounts::skipSettle);
    s += " skip-no-state=" + ColumnsText(f.columns, &ColumnCounts::skipNoState);
    s += " col-unreadable=" + ColumnsText(f.columns, &ColumnCounts::unreadable);
    s += " orig-unreadable=" + ColumnsText(f.columns, &ColumnCounts::originalUnreadable);
    s += " write-failed=" + ColumnsText(f.columns, &ColumnCounts::pressWriteFailed);
    s += " restore-failed=" + ColumnsText(f.columns, &ColumnCounts::restoreFailed);
    s += " gate-calls=" + std::to_string(f.gateCalls);
    s += " gate-other=" + std::to_string(f.gateOther);
    s += " threw=" + std::to_string(f.threw);
    const JumpCounts& j = f.jump;
    s += " | jump granted=" + std::to_string(j.granted);
    s += " backed-off=" + std::to_string(j.backedOff);
    s += " refused-landing=" + std::to_string(j.refusedLanding);
    s += " refused-gate=" + std::to_string(j.refusedGate);
    s += " refused-cursor=" + std::to_string(j.refusedCursor);
    s += " max=" + NumberText(j.max) + "(" + j.maxSource + ")";
    s += " refused-unreadable=" + std::to_string(j.refusedUnreadable);
    s += " jumps=" + std::to_string(j.jumps);
    s += " scaled=" + std::to_string(j.scaled);
    s += " clamped=" + std::to_string(j.clamped);
    s += " answered=" + std::to_string(j.answered);
    s += " excluded=" + std::to_string(j.excluded);
    // Candidates refused by a successful read: room/family/grid/gate.
    s += " rejected=" + JoinCounts({ j.rejectedRoom, j.rejectedFamily, j.rejectedGrid, j.rejectedGate });
    // Failed reads: takeoff/cursor/room/family/grid/gate.
    s += " unread=" + JoinCounts({ j.unreadTakeoff, j.unreadCursor, j.unreadRoom, j.unreadFamily, j.unreadGrid, j.unreadGate });
    s += " | icons=" + std::to_string(f.iconsLoaded) + "/" + std::to_string(f.iconsEmbedded);
    s += " art=" + std::string(f.artArmed ? "armed" : "off");
    s += " ground=" + std::to_string(f.ground);
    s += " ground-route=" + f.groundRoute;
    s += " | speed=" + LeverText(f.speed) + " penalty=" + LeverText(f.penalty);
    return s;
}

// ---- the pieces' own art: the rule the adapter applies --------------------------
//
// The bag icon and the ground consumer go together, and both follow the switch:
// the art is armed for the rest of the session by the first `mark1set bonus 1`
// (Bonus keeps the flag; turning the switch off does not clear it). Until then
// a piece shows its base item's own bag and ground sprites. Our runtime sprite
// under "11" draws Error_spr on the ground unless the ground consumer sets the
// ground item's sprite_index (RDM § 23.7), so the icon is written only where
// that consumer can follow it.

// The shared LootGroundInit detour's route, as hidden loot's RouteName() names
// it. `none` (not in yet) and `both` allow the art; anything else is a route
// known to be other than both, which compiled GML's direct calls pass by.
inline bool GroundRouteAllowsArt(std::string_view route)
{
    return route == "none" || route == "both";
}

// A dressing writes our bag icon under "11" for a piece only while the art is
// armed, its bag icon and its ground copy both loaded, and the ground route
// allows it.
inline bool ArtWritesIcon(bool armed, bool bagLoaded, bool groundLoaded, std::string_view groundRoute)
{
    return armed && bagLoaded && groundLoaded && GroundRouteAllowsArt(groundRoute);
}

// Whether the ground consumer still waits to be installed: armed, an icon
// embedded and no try yet. The tick checks this before any game call, so an
// unarmed session reads nothing for it.
inline bool ArtGroundInstallPending(bool armed, int iconsEmbedded, bool tried)
{
    return armed && iconsEmbedded > 0 && !tried;
}

// Whether the adapter installs the ground consumer now: pending, setup done
// and the local player resolved (never at character select, the guide's Known
// Limitations item 8). The switch is not asked: once armed, it is wanted
// whatever the switch now says.
inline bool ArtWantsGroundInstall(bool armed, int iconsEmbedded, bool tried, bool setupDone, bool playerResolved)
{
    return ArtGroundInstallPending(armed, iconsEmbedded, tried) && setupDone && playerResolved;
}

// ---- the bonus: the switch, the install and the refreshed read -------------------

class Bonus {
public:
    // The four worn slots, read through GetOnlinePlayerItemOwner and
    // GetItemFromFingerprint. False when the read failed as a whole (no local
    // player, no readable equippedItems): counted, and not full.
    using ReadWornFn = std::function<bool(WornSlots& slots)>;

    // The set is read at most once in this many frames at the point of use.
    static constexpr unsigned long long kRefreshFrames = 30;

    // The `mark1set bonus 1|0` switch. Turning it off ends the bonus at once
    // and forgets the last read, so turning it on again reads at once. Turning
    // it on also arms the pieces' art for the rest of the session.
    void SetEnabled(bool on)
    {
        if (on) artArmed_ = true;
        if (on == enabled_) return;
        enabled_ = on;
        hasRead_ = false;
        full_ = false;
    }
    bool Enabled() const { return enabled_; }

    // The art rule's sticky flag: set by the first switch-on, never cleared.
    bool ArtArmed() const { return artArmed_; }

    // ---- the install: armed by the switch, hooked once a character exists ----
    // The guide's Known Limitations item 8: a hook installed at character
    // select stalls the runner. `mark1set bonus 1` with no character only arms;
    // the adapter's tick (which returns at once unless TickNeeded) looks for the
    // local player on the frames LooksForPlayer picks, counted from the first
    // armed frame, and installs once; `mark1set bonus 1` with a character
    // loaded tries at once, again after a refusal. A refused install turns the
    // switch off.
    static constexpr unsigned long long kInstallPollFrames = 60;
    static constexpr bool LooksForPlayer(unsigned long long armedFrames)
    {
        return armedFrames % kInstallPollFrames == 0;
    }
    // Whether the adapter's tick has anything to do.
    bool TickNeeded() const { return enabled_ && !installed_ && !tickTried_; }
    // Whether the adapter installs now: the switch on, the hooks not in yet,
    // setup done, the local player resolved, and, for the tick, no try of its
    // own yet this session.
    bool ShouldInstall(bool setupDone, bool playerResolved, bool fromTick) const
    {
        return enabled_ && !installed_ && !(fromTick && tickTried_) && setupDone && playerResolved;
    }
    // What the adapter's install returned: every hook in, or refused (the
    // adapter prints why, once).
    void NoteInstall(bool ok, bool fromTick)
    {
        if (fromTick) tickTried_ = true;
        if (ok) {
            installed_ = true;
            refused_ = false;
            return;
        }
        refused_ = true;
        SetEnabled(false);
    }
    bool Installed() const { return installed_; }
    // The ` install=` field: installed once the hooks are in, refused after a
    // refused install, waiting-for-character while on without them, not-armed
    // otherwise.
    std::string_view InstallStateName() const
    {
        if (installed_) return "installed";
        if (refused_) return "refused";
        return enabled_ ? "waiting-for-character" : "not-armed";
    }

    // ---- the point of use ----
    // Is the bonus active now? Called by the CheckTalentUse detour for the
    // local player. With the switch off nothing is read; otherwise the set is
    // read when no read is held, when kRefreshFrames have passed since the last
    // one, or when the frame counter went back.
    bool Active(unsigned long long frame, const ReadWornFn& read)
    {
        if (!enabled_) return false;
        if (!hasRead_ || frame < lastFrame_ || frame - lastFrame_ >= kRefreshFrames) ReadNow(frame, read);
        return full_;
    }

    // Read the set now: `mark1set stat`, and Active when the period is up. A
    // read with the switch off fills the line's set= but never activates.
    void ReadNow(unsigned long long frame, const ReadWornFn& read)
    {
        ++reads_;
        WornSlots slots{};
        SetRead r;
        if (!read || !read(slots)) {
            r.unreadable = kPieceCount;
        } else {
            r = Evaluate(slots);
        }
        if (r.unreadable > 0) ++unreadable_;
        last_ = r;
        full_ = r.full;
        lastFrame_ = frame;
        hasRead_ = enabled_;
    }

    // The bonus as the last read left it, without reading.
    bool ActiveCached() const { return enabled_ && full_; }
    const SetRead& LastRead() const { return last_; }
    uint64_t Reads() const { return reads_; }
    uint64_t Unreadable() const { return unreadable_; }

    // The set's part of the stat line.
    void FillStat(StatFields& f) const
    {
        f.bonus = enabled_;
        f.active = ActiveCached();
        f.install = std::string(InstallStateName());
        f.set = last_;
        f.unreadable = unreadable_;
        f.reads = reads_;
        f.artArmed = artArmed_;
    }

private:
    bool               enabled_ = false;
    bool               artArmed_ = false;
    bool               installed_ = false;
    bool               refused_ = false;
    bool               tickTried_ = false;
    bool               hasRead_ = false;
    bool               full_ = false;
    unsigned long long lastFrame_ = 0;
    SetRead            last_;
    uint64_t           reads_ = 0;
    uint64_t           unreadable_ = 0;
};

// ---- the `mark1set` command ----------------------------------------------------------

inline constexpr std::string_view kUsage = "usage: mark1set bonus 1|0 | mark1set stat";

enum class CommandForm : int { Stat, Bonus };

struct Command {
    CommandForm form = CommandForm::Stat;
    bool        on = false;
};

// The words after `mark1set`. Bare or `stat` prints the stat line;
// `bonus 1|0` sets the switch.
inline bool ParseCommand(const std::vector<std::string>& args, Command& out, std::string& why)
{
    why.clear();
    if (args.empty() || (args.size() == 1 && args[0] == "stat")) {
        out = Command{};
        return true;
    }
    if (args.size() == 2 && args[0] == "bonus" && (args[1] == "1" || args[1] == "0")) {
        out.form = CommandForm::Bonus;
        out.on = args[1] == "1";
        return true;
    }
    why = "mark1set: " + std::string(kUsage);
    return false;
}

// The reply to `mark1set bonus 1|0`, after the adapter has tried its install.
inline std::string BonusReply(const Bonus& b)
{
    return "mark1set: bonus=" + std::string(b.Enabled() ? "on" : "off") + " install=" + std::string(b.InstallStateName());
}

} // namespace ForgePact::Mark1Set
